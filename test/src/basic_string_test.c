/*
** 基础字符串测试 - 逐步验证功能
*/

#include "src/aql.h"
#include "src/astring.h"
#include "src/astate.h"
#include "src/amem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
    printf("🧪 AQL 基础字符串测试\n");
    printf("========================================\n");
    
    /* 测试1: 创建状态和字符串 */
    printf("\n=== 测试1: 字符串创建 ===\n");
    aql_State *L = aql_newstate(test_alloc, NULL);
    assert(L != NULL);
    printf("✅ AQL状态创建成功\n");
    
    aqlStr_init(L);
    printf("✅ 字符串表初始化成功\n");
    
    TString *str1 = aqlStr_newlstr(L, "Hello", 5);
    assert(str1 != NULL);
    assert(aqlS_len(str1) == 5);
    assert(strcmp(aqlS_data(str1), "Hello") == 0);
    printf("✅ 短字符串创建和访问成功\n");
    
    TString *str2 = aqlStr_new(L, "World");
    assert(str2 != NULL);
    assert(aqlS_len(str2) == 5);
    assert(strcmp(aqlS_data(str2), "World") == 0);
    printf("✅ null结尾字符串创建成功\n");
    
    /* 测试2: 字符串比较 */
    printf("\n=== 测试2: 字符串比较 ===\n");
    TString *str3 = aqlStr_new(L, "Hello");
    assert(aqlS_eqstr(str1, str3) == 1);
    printf("✅ 相同内容字符串比较成功\n");
    
    assert(aqlS_eqstr(str1, str2) == 0);
    printf("✅ 不同内容字符串比较成功\n");
    
    /* 测试3: 字符串拼接 */
    printf("\n=== 测试3: 字符串拼接 ===\n");
    TString *space = aqlStr_new(L, " ");
    TString *hello_space = aqlStr_concat(L, str1, space);
    assert(hello_space != NULL);
    assert(aqlS_len(hello_space) == 6);
    assert(strcmp(aqlS_data(hello_space), "Hello ") == 0);
    printf("✅ 字符串拼接成功\n");
    
    TString *hello_world = aqlStr_concat(L, hello_space, str2);
    assert(hello_world != NULL);
    assert(aqlS_len(hello_world) == 11);
    assert(strcmp(aqlS_data(hello_world), "Hello World") == 0);
    printf("✅ 多次字符串拼接成功\n");
    
    /* 测试4: 字符串子串 */
    printf("\n=== 测试4: 字符串子串 ===\n");
    TString *sub1 = aqlStr_sub(L, hello_world, 0, 5);
    assert(strcmp(aqlS_data(sub1), "Hello") == 0);
    printf("✅ 子串提取成功\n");
    
    TString *sub2 = aqlStr_sub(L, hello_world, 6, 11);
    assert(strcmp(aqlS_data(sub2), "World") == 0);
    printf("✅ 后半部分子串提取成功\n");
    
    /* 测试5: 字符串格式化 */
    printf("\n=== 测试5: 字符串格式化 ===\n");
    TString *formatted = aqlS_formatf(L, "Number: %d", 42);
    assert(formatted != NULL);
    printf("格式化结果: %s\n", aqlS_data(formatted));
    printf("✅ 字符串格式化成功\n");
    
    /* 测试6: 字符串搜索 */
    printf("\n=== 测试6: 字符串搜索 ===\n");
    TString *pattern = aqlStr_new(L, "World");
    int pos = aqlStr_find(hello_world, pattern, 0);
    assert(pos == 6);
    printf("✅ 字符串搜索成功，位置: %d\n", pos);
    
    /* 测试7: 大小写转换 */
    printf("\n=== 测试7: 大小写转换 ===\n");
    TString *upper = aqlS_upper(L, hello_world);
    assert(strcmp(aqlS_data(upper), "HELLO WORLD") == 0);
    printf("✅ 转大写成功: %s\n", aqlS_data(upper));
    
    TString *lower = aqlS_lower(L, hello_world);
    assert(strcmp(aqlS_data(lower), "hello world") == 0);
    printf("✅ 转小写成功: %s\n", aqlS_data(lower));
    
    /* 清理 */
    printf("\n=== 清理资源 ===\n");
    aql_close(L);
    printf("✅ 资源清理完成\n");
    
    printf("\n🎉 所有基础字符串测试通过！\n");
    printf("✨ 功能验证:\n");
    printf("   - 字符串创建和访问 ✅\n");
    printf("   - 字符串比较 ✅\n");
    printf("   - 字符串拼接 ✅\n");
    printf("   - 字符串子串 ✅\n");
    printf("   - 字符串格式化 ✅\n");
    printf("   - 字符串搜索 ✅\n");
    printf("   - 大小写转换 ✅\n");
    
    return 0;
}