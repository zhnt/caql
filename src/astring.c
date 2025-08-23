/*
** $Id: astring.c $
** String table (keeps all strings handled by AQL)
** See Copyright Notice in aql.h
*/

#include "astring.h"
#include "amem.h"
#include "agc.h"
#include "astate.h"
#include "azio.h"
#include "alimits.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>

/* 前向声明和宏定义 */
#ifndef AQLAI_BUFFERSIZE
#define AQLAI_BUFFERSIZE 8192
#endif

#ifndef AQLAI_MAXALIGN
#define AQLAI_MAXALIGN double u; void *s; long l; aql_Number n;
#endif

/* 字符串哈希函数 */
static unsigned int aqlS_hash_string(const char *str, size_t l) {
    unsigned int hash = 5381;
    for (size_t i = 0; i < l; i++) {
        hash = ((hash << 5) + hash) + cast_byte(str[i]);
    }
    return hash;
}

/* 模运算宏 */
#define lmod(s,size) \
    (check_exp((size&(size-1))==0, (cast_int((s) & ((size)-1)))))

/* 字符串对象创建函数 */
static TString *createstrobj(aql_State *L, size_t l, int tag, unsigned int h) {
    TString *ts;
    GCObject *o;
    size_t totalsize;
    
    /* 计算字符串对象总大小 */
    totalsize = sizeof(TString) + ((l + 1) * sizeof(char));
    o = aqlC_newobj(L, tag, totalsize);
    ts = gco2ts(o);
    ts->hash = h;
    ts->extra = 0;
    if (tag == AQL_VSHRSTR) {
        ts->shrlen = cast_byte(l);
    } else {
        ts->shrlen = 0xFF;
        ts->u.lnglen = l;
    }
    return ts;
}

/* 内存不足错误 */
static l_noret aqlM_toobig(aql_State *L) {
    aqlG_runerror(L, "memory allocation error: block too big");
    /* 这里不应该到达，但为了消除警告 */
    exit(EXIT_FAILURE);
}

/* ============================================================================
 * 字符串哈希和比较函数
 * ============================================================================ */

/*
** 计算长字符串的哈希值 (使用djb2算法的变种)
*/
unsigned int aqlS_hashlongstr(TString *ts) {
    const char *str = getstr(ts);
    size_t len = tsslen(ts);
    unsigned int hash = 5381;
    
    /* 对于长字符串，只哈希头部、中部和尾部 */
    if (len <= 32) {
        /* 短于32字节，全部哈希 */
        for (size_t i = 0; i < len; i++) {
            hash = ((hash << 5) + hash) + cast_byte(str[i]);
        }
    } else {
        /* 长字符串采样哈希 */
        size_t step = len / 16;  /* 步长 */
        for (size_t i = 0; i < len; i += step) {
            hash = ((hash << 5) + hash) + cast_byte(str[i]);
        }
        /* 确保包含首尾字符 */
        hash = ((hash << 5) + hash) + cast_byte(str[0]);
        hash = ((hash << 5) + hash) + cast_byte(str[len-1]);
    }
    
    return hash;
}

/*
** 比较两个长字符串是否相等
*/
int aqlS_eqlngstr(TString *a, TString *b) {
    size_t len = tsslen(a);
    aql_assert(a->shrlen == 0xFF && b->shrlen == 0xFF);  /* 都是长字符串 */
    
    /* 长度不同，直接返回不相等 */
    if (len != tsslen(b)) return 0;
    
    /* 哈希值不同，直接返回不相等 (如果都已计算) */
    if (a->hash != 0 && b->hash != 0 && a->hash != b->hash) return 0;
    
    /* 逐字节比较 */
    return (memcmp(getstr(a), getstr(b), len) == 0);
}

/*
** 比较两个字符串是否相等 (短字符串或长字符串)
*/
int aqlS_eqstr(TString *a, TString *b) {
    if (a == b) return 1;  /* 相同对象 */
    
    /* 检查是否都是短字符串或都是长字符串 */
    int a_is_short = (a->shrlen != 0xFF);
    int b_is_short = (b->shrlen != 0xFF);
    
    if (a_is_short != b_is_short) return 0;  /* 类型不同 */
    
    if (a_is_short) {
        /* 短字符串已经内部化，指针相等即内容相等 */
        return (a == b);
    } else {
        /* 长字符串需要内容比较 */
        return aqlS_eqlngstr(a, b);
    }
}

/* ============================================================================
 * 字符串创建和管理
 * ============================================================================ */

/*
** 创建长字符串对象 (不进行内部化)
*/
TString *aqlStr_createlngstrobj(aql_State *L, size_t l) {
    TString *ts;
    GCObject *o;
    
    if (l >= (MAX_SIZE - sizeof(TString))/sizeof(char))
        aqlM_toobig(L);
    
    o = aqlC_newobj(L, AQL_VLNGSTR, sizeof(TString) + (l + 1) * sizeof(char));
    ts = gco2ts(o);
    ts->shrlen = 0xFF;  /* 标记为长字符串 */
    ts->u.lnglen = l;
    ts->hash = 0;  /* 延迟计算哈希值 */
    ts->extra = 0;
    getstr(ts)[l] = '\0';  /* 确保以null结尾 */
    
    return ts;
}

/*
** 从字符串池中移除字符串
*/
void aqlStr_remove(aql_State *L, TString *ts) {
    stringtable *tb = &G(L)->strt;
    TString **list = &tb->hash[lmod(ts->hash, tb->size)];
    
    while (*list != ts)  /* 查找字符串 */
        list = &(*list)->u.hnext;
    
    *list = (*list)->u.hnext;  /* 从链表中移除 */
    tb->nuse--;
}

/*
** 调整字符串表大小
*/
void aqlStr_resize(aql_State *L, int newsize) {
    stringtable *tb = &G(L)->strt;
    TString **oldhash = tb->hash;
    int oldsize = tb->size;
    
    /* 分配新的哈希表 */
    tb->hash = (TString**)aqlM_malloc(L, newsize * sizeof(TString*));
    if (!tb->hash) {
        tb->hash = oldhash;  /* 恢复原状态 */
        return;
    }
    
    /* 初始化新表 */
    for (int i = 0; i < newsize; i++) {
        tb->hash[i] = NULL;
    }
    tb->size = newsize;
    
    /* 重新哈希所有字符串 */
    if (oldhash) {
        for (int i = 0; i < oldsize; i++) {
            TString *p = oldhash[i];
            while (p) {
                TString *hnext = p->u.hnext;
                unsigned int h = lmod(p->hash, newsize);
                p->u.hnext = tb->hash[h];
                tb->hash[h] = p;
                p = hnext;
            }
        }
        /* 释放旧表 */
        aqlM_freemem(L, oldhash, oldsize * sizeof(TString*));
    }
}

/*
** 清除字符串缓存
*/
void aqlStr_clearcache(global_State *g) {
    for (int i = 0; i < STRCACHE_N; i++) {
        for (int j = 0; j < STRCACHE_M; j++) {
            g->strcache[i][j] = NULL;
        }
    }
}

/*
** 初始化字符串表
*/
void aqlStr_init(aql_State *L) {
    global_State *g = G(L);
    int i, j;
    
    aqlStr_resize(L, MINSTRTABSIZE);
    
    /* 清除字符串缓存 */
    for (i = 0; i < STRCACHE_N; i++) {
        for (j = 0; j < STRCACHE_M; j++) {
            g->strcache[i][j] = NULL;
        }
    }
}

/*
** 内部化字符串 (仅用于短字符串)
*/
static TString *internshrstr(aql_State *L, const char *str, size_t l) {
    TString *ts;
    global_State *g = G(L);
    stringtable *tb = &g->strt;
    unsigned int h = aqlS_hash_string(str, l);
    TString **list = &tb->hash[lmod(h, tb->size)];
    
    aql_assert(l <= AQLAI_MAXSHORTLEN);
    
    /* 在字符串表中查找 */
    for (ts = *list; ts != NULL; ts = ts->u.hnext) {
        if (l == tsslen(ts) && (memcmp(str, getstr(ts), l * sizeof(char)) == 0)) {
            /* 找到相同字符串，检查是否需要重新激活 */
            if (isdead(g, ts))
                changewhite(ts);  /* 重新激活 */
            return ts;
        }
    }
    
    /* 检查是否需要扩展字符串表 */
    if (tb->nuse >= tb->size && tb->size <= MAX_INT/2) {
        aqlStr_resize(L, tb->size * 2);
        list = &tb->hash[lmod(h, tb->size)];  /* 重新计算位置 */
    }
    
    /* 创建新的短字符串 */
    ts = createstrobj(L, l, AQL_VSHRSTR, h);
    memcpy(getstr(ts), str, l * sizeof(char));
    getstr(ts)[l] = '\0';
    ts->u.hnext = *list;
    *list = ts;
    tb->nuse++;
    
    return ts;
}

/*
** 创建新字符串 (带长度)
*/
TString *aqlStr_newlstr(aql_State *L, const char *str, size_t l) {
    if (l <= AQLAI_MAXSHORTLEN)  /* 短字符串？ */
        return internshrstr(L, str, l);
    else {
        TString *ts;
        if (l >= (MAX_SIZE - sizeof(TString))/sizeof(char))
            aqlM_toobig(L);
        ts = aqlStr_createlngstrobj(L, l);
        memcpy(getstr(ts), str, l * sizeof(char));
        return ts;
    }
}

/*
** 创建新字符串 (以null结尾)
*/
TString *aqlStr_new(aql_State *L, const char *str) {
    return aqlStr_newlstr(L, str, strlen(str));
}

/* ============================================================================
 * 字符串工具函数
 * ============================================================================ */

/*
** 获取字符串数据指针
*/
const char *aqlS_data(TString *ts) {
    return getstr(ts);
}

/*
** 获取字符串长度
*/
size_t aqlS_len(TString *ts) {
    return tsslen(ts);
}

/*
** 字符串拼接
*/
TString *aqlStr_concat(aql_State *L, TString *a, TString *b) {
    size_t la = tsslen(a);
    size_t lb = tsslen(b);
    size_t total = la + lb;
    
    if (total < la || total > MAX_SIZE)  /* 溢出检查 */
        aqlM_toobig(L);
    
    if (total == 0)
        return aqlStr_newlstr(L, "", 0);
    
    /* 直接分配内存进行拼接 */
    char *buffer = (char *)aqlM_malloc(L, (total + 1) * sizeof(char));
    if (!buffer) aqlM_toobig(L);
    
    memcpy(buffer, getstr(a), la);
    memcpy(buffer + la, getstr(b), lb);
    buffer[total] = '\0';
    
    TString *result = aqlStr_newlstr(L, buffer, total);
    aqlM_freemem(L, buffer, (total + 1) * sizeof(char));
    
    return result;
}

/*
** 字符串子串
*/
TString *aqlStr_sub(aql_State *L, TString *str, size_t start, size_t end) {
    size_t len = tsslen(str);
    const char *data = getstr(str);
    
    /* 边界检查 */
    if (start > len) start = len;
    if (end > len) end = len;
    if (start > end) start = end;
    
    size_t sublen = end - start;
    if (sublen == 0)
        return aqlStr_newlstr(L, "", 0);
    
    return aqlStr_newlstr(L, data + start, sublen);
}

/* ============================================================================
 * 字符串格式化
 * ============================================================================ */

/*
** 格式化字符串 (变参版本)
*/
TString *aqlS_formatf(aql_State *L, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    TString *result = aqlS_formatv(L, fmt, args);
    va_end(args);
    return result;
}

/*
** 格式化字符串 (va_list版本) - 简化实现
*/
TString *aqlS_formatv(aql_State *L, const char *fmt, va_list args) {
    char buffer[BUFVFS];  /* 使用固定大小缓冲区 */
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    
    if (len < 0) {
        /* 格式化错误 */
        return aqlStr_newlstr(L, "format error", 12);
    }
    
    if (len >= (int)sizeof(buffer)) {
        /* 缓冲区不够，截断 */
        len = sizeof(buffer) - 1;
        buffer[len] = '\0';
    }
    
    return aqlStr_newlstr(L, buffer, (size_t)len);
}

/* ============================================================================
 * 字符串搜索和匹配
 * ============================================================================ */

/*
** 在字符串中查找子串 (返回位置，-1表示未找到)
*/
int aqlStr_find(TString *str, TString *pattern, size_t start) {
    size_t str_len = tsslen(str);
    size_t pat_len = tsslen(pattern);
    const char *str_data = getstr(str);
    const char *pat_data = getstr(pattern);
    
    if (start >= str_len) return -1;
    if (pat_len == 0) return (int)start;
    if (pat_len > str_len - start) return -1;
    
    /* 简单的暴力搜索算法 */
    for (size_t i = start; i <= str_len - pat_len; i++) {
        if (memcmp(str_data + i, pat_data, pat_len) == 0) {
            return (int)i;
        }
    }
    
    return -1;
}

/*
** 从后向前查找子串
*/
int aqlStr_findlast(TString *str, TString *pattern) {
    size_t str_len = tsslen(str);
    size_t pat_len = tsslen(pattern);
    const char *str_data = getstr(str);
    const char *pat_data = getstr(pattern);
    
    if (pat_len == 0) return (int)str_len;
    if (pat_len > str_len) return -1;
    
    /* 从后向前搜索 */
    for (int i = (int)(str_len - pat_len); i >= 0; i--) {
        if (memcmp(str_data + i, pat_data, pat_len) == 0) {
            return i;
        }
    }
    
    return -1;
}

/*
** 字符串替换 (count = -1 表示替换所有) - 简化实现
*/
TString *aqlS_replace(aql_State *L, TString *str, TString *old, TString *new, int count) {
    size_t str_len = tsslen(str);
    size_t old_len = tsslen(old);
    size_t new_len = tsslen(new);
    const char *str_data = getstr(str);
    const char *old_data = getstr(old);
    const char *new_data = getstr(new);
    
    if (old_len == 0) return str;  /* 无法替换空字符串 */
    
    /* 估计结果大小并分配缓冲区 */
    size_t max_result_len = str_len + (new_len > old_len ? (new_len - old_len) * 10 : 0);
    char *buffer = (char *)aqlM_malloc(L, max_result_len + 1);
    if (!buffer) aqlM_toobig(L);
    
    size_t pos = 0;
    size_t result_pos = 0;
    int replaced = 0;
    
    while (pos < str_len && (count == -1 || replaced < count)) {
        /* 查找下一个匹配位置 */
        int found = -1;
        if (pos <= str_len - old_len) {
            for (size_t i = pos; i <= str_len - old_len; i++) {
                if (memcmp(str_data + i, old_data, old_len) == 0) {
                    found = (int)i;
                    break;
                }
            }
        }
        
        if (found == -1) {
            /* 没有找到更多匹配，复制剩余部分 */
            size_t remaining = str_len - pos;
            memcpy(buffer + result_pos, str_data + pos, remaining);
            result_pos += remaining;
            break;
        } else {
            /* 复制匹配前的部分 */
            size_t before_len = found - pos;
            memcpy(buffer + result_pos, str_data + pos, before_len);
            result_pos += before_len;
            
            /* 复制替换字符串 */
            memcpy(buffer + result_pos, new_data, new_len);
            result_pos += new_len;
            
            pos = found + old_len;
            replaced++;
        }
    }
    
    /* 复制剩余部分 */
    if (pos < str_len) {
        size_t remaining = str_len - pos;
        memcpy(buffer + result_pos, str_data + pos, remaining);
        result_pos += remaining;
    }
    
    buffer[result_pos] = '\0';
    TString *result = aqlStr_newlstr(L, buffer, result_pos);
    aqlM_freemem(L, buffer, max_result_len + 1);
    
    return result;
}

/* ============================================================================
 * 字符串大小写转换
 * ============================================================================ */

/*
** 转换为大写
*/
TString *aqlS_upper(aql_State *L, TString *str) {
    size_t len = tsslen(str);
    const char *data = getstr(str);
    
    char *buffer = (char *)aqlM_malloc(L, len + 1);
    if (!buffer) aqlM_toobig(L);
    
    for (size_t i = 0; i < len; i++) {
        buffer[i] = toupper(cast_uchar(data[i]));
    }
    buffer[len] = '\0';
    
    TString *result = aqlStr_newlstr(L, buffer, len);
    aqlM_freemem(L, buffer, len + 1);
    
    return result;
}

/*
** 转换为小写
*/
TString *aqlS_lower(aql_State *L, TString *str) {
    size_t len = tsslen(str);
    const char *data = getstr(str);
    
    char *buffer = (char *)aqlM_malloc(L, len + 1);
    if (!buffer) aqlM_toobig(L);
    
    for (size_t i = 0; i < len; i++) {
        buffer[i] = tolower(cast_uchar(data[i]));
    }
    buffer[len] = '\0';
    
    TString *result = aqlStr_newlstr(L, buffer, len);
    aqlM_freemem(L, buffer, len + 1);
    
    return result;
}

/* ============================================================================
 * 用户数据分配 (兼容性函数)
 * ============================================================================ */

/*
** 创建用户数据对象
*/
Udata *aqlS_newudata(aql_State *L, size_t s, int nuvalue) {
    Udata *u;
    int i;
    GCObject *o;
    
    if (s > MAX_SIZE - udatamemoffset(nuvalue))
        aqlM_toobig(L);
    
    o = aqlC_newobj(L, AQL_TUSERDATA, sizeudata(nuvalue, s));
    u = gco2u(o);
    u->len = s;
    u->nuvalue = nuvalue;
    u->metatable = NULL;
    
    for (i = 0; i < nuvalue; i++)
        setnilvalue(&u->uv[i].uv);
    
    return u;
}