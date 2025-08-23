/*
** 调试字符串测试 - 逐步测试每个函数
*/

#include "src/aql.h"
#include "src/astring.h"
#include "src/astate.h"
#include "src/amem.h"
#include <stdio.h>
#include <stdlib.h>

/* 测试分配器 */
static void *test_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud; (void)osize;
    printf("分配器调用: ptr=%p, osize=%zu, nsize=%zu\n", ptr, osize, nsize);
    if (nsize == 0) {
        free(ptr);
        return NULL;
    } else {
        return realloc(ptr, nsize);
    }
}

int main() {
    printf("开始调试字符串测试...\n");
    
    /* 创建AQL状态 */
    printf("步骤1: 创建AQL状态...\n");
    aql_State *L = aql_newstate(test_alloc, NULL);
    if (!L) {
        printf("❌ 无法创建AQL状态\n");
        return 1;
    }
    printf("✅ AQL状态创建成功: %p\n", (void*)L);
    
    /* 检查全局状态 */
    printf("步骤2: 检查全局状态...\n");
    global_State *g = G(L);
    if (!g) {
        printf("❌ 无法获取全局状态\n");
        aql_close(L);
        return 1;
    }
    printf("✅ 全局状态获取成功: %p\n", (void*)g);
    
    /* 检查字符串表 */
    printf("步骤3: 检查字符串表...\n");
    stringtable *tb = &g->strt;
    printf("字符串表地址: %p\n", (void*)tb);
    printf("字符串表大小: %d\n", tb->size);
    printf("字符串表使用: %d\n", tb->nuse);
    
    /* 手动初始化字符串表（避免调用aqlStr_resize） */
    printf("步骤4: 手动初始化字符串表...\n");
    if (tb->size == 0) {
        tb->size = 128;  /* MINSTRTABSIZE */
        tb->hash = (TString**)aqlM_malloc(L, tb->size * sizeof(TString*));
        if (!tb->hash) {
            printf("❌ 无法分配字符串表内存\n");
            aql_close(L);
            return 1;
        }
        for (int i = 0; i < tb->size; i++) {
            tb->hash[i] = NULL;
        }
        tb->nuse = 0;
        printf("✅ 字符串表手动初始化成功\n");
    }
    
    /* 清除字符串缓存 */
    printf("步骤5: 清除字符串缓存...\n");
    for (int i = 0; i < STRCACHE_N; i++) {
        for (int j = 0; j < STRCACHE_M; j++) {
            g->strcache[i][j] = NULL;
        }
    }
    printf("✅ 字符串缓存清除成功\n");
    
    printf("步骤6: 清理资源...\n");
    aql_close(L);
    
    printf("✅ 调试测试完成\n");
    return 0;
}