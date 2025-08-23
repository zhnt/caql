/*
** 简单的字符串测试 - 用于调试
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
    if (nsize == 0) {
        free(ptr);
        return NULL;
    } else {
        return realloc(ptr, nsize);
    }
}

int main() {
    printf("开始简单字符串测试...\n");
    
    /* 创建AQL状态 */
    printf("创建AQL状态...\n");
    aql_State *L = aql_newstate(test_alloc, NULL);
    if (!L) {
        printf("❌ 无法创建AQL状态\n");
        return 1;
    }
    printf("✅ AQL状态创建成功\n");
    
    /* 初始化字符串表 */
    printf("初始化字符串表...\n");
    aqlStr_init(L);
    printf("✅ 字符串表初始化成功\n");
    
    /* 测试创建短字符串 */
    printf("创建短字符串...\n");
    TString *str = aqlStr_newlstr(L, "Hello", 5);
    if (!str) {
        printf("❌ 无法创建字符串\n");
        aql_close(L);
        return 1;
    }
    printf("✅ 字符串创建成功\n");
    
    /* 测试字符串长度 */
    size_t len = aqlS_len(str);
    printf("字符串长度: %zu\n", len);
    
    /* 测试字符串数据 */
    const char *data = aqlS_data(str);
    printf("字符串内容: %s\n", data);
    
    printf("清理资源...\n");
    aql_close(L);
    
    printf("✅ 简单字符串测试完成\n");
    return 0;
}