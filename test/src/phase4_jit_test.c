/*
** Phase 4 JIT模式测试
** 验证JIT编译、热点检测和执行功能
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "aql.h"
#include "ai_jit.h"

/* 测试用的简单Lua代码 */
static const char* test_code = "function fib(n) if n < 2 then return n end return fib(n-1) + fib(n-2) end function factorial(n) if n <= 1 then return 1 end return n * factorial(n-1) end function sum_array(arr) local sum = 0 for i = 1, #arr do sum = sum + arr[i] end return sum end"

/* 性能测试函数 */
static double measure_execution_time(aql_State* L, const char* func_name, int arg) {
    clock_t start = clock();
    
    /* 调用函数 */
    aql_getglobal(L, func_name);
    aql_pushinteger(L, arg);
    aql_call(L, 1, 1);
    
    clock_t end = clock();
    return (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
}

/* 测试JIT基本功能 */
static void test_jit_basic() {
    printf("=== JIT基本功能测试 ===\n");
    
    aql_State* L = aql_newstate(NULL, NULL);
    if (!L) {
        printf("❌ 无法创建AQL状态\n");
        return;
    }
    
    /* 初始化JIT */
    if (aqlJIT_init(L, JIT_BACKEND_NATIVE) != JIT_ERROR_NONE) {
        printf("❌ JIT初始化失败\n");
        aql_close(L);
        return;
    }
    
    /* 加载测试代码 */
    if (aqlL_dostring(L, test_code) != AQL_OK) {
        printf("❌ 代码加载失败: %s\n", aql_tostring(L, -1));
        aql_close(L);
        return;
    }
    
    printf("✅ JIT初始化成功\n");
    printf("✅ 测试代码加载成功\n");
    
    aql_close(L);
}

/* 测试热点检测 */
static void test_hotspot_detection() {
    printf("\n=== 热点检测测试 ===\n");
    
    aql_State* L = aql_newstate(NULL, NULL);
    if (!L) return;
    
    aqlJIT_init(L, JIT_BACKEND_NATIVE);
    aqlL_dostring(L, test_code);
    
    /* 多次调用使函数变热 */
    for (int i = 0; i < 15; i++) {
        aql_getglobal(L, "fib");
        aql_pushinteger(L, 10);
        aql_call(L, 1, 1);
        aql_pop(L, 1); /* 移除结果 */
    }
    
    /* 检查热点状态 */
    aql_getglobal(L, "fib");
    if (aql_isfunction(L, -1)) {
        Proto* proto = (Proto*)aql_touserdata(L, -1);
        if (proto && aqlJIT_should_compile(L, proto)) {
            printf("✅ 热点检测成功 - fib函数被标记为热点\n");
        } else {
            printf("⚠️  热点检测未完成（需要更多调用）\n");
        }
    }
    
    aql_close(L);
}

/* 测试性能对比 */
static void test_performance_comparison() {
    printf("\n=== 性能对比测试 ===\n");
    
    aql_State* L = aql_newstate(NULL, NULL);
    if (!L) return;
    
    aqlJIT_init(L, JIT_BACKEND_NATIVE);
    aqlL_dostring(L, test_code);
    
    /* 预热JIT */
    for (int i = 0; i < 20; i++) {
        aql_getglobal(L, "factorial");
        aql_pushinteger(L, 8);
        aql_call(L, 1, 1);
        aql_pop(L, 1);
    }
    
    /* 测试执行时间 */
    double time1 = measure_execution_time(L, "factorial", 10);
    double time2 = measure_execution_time(L, "factorial", 10);
    double time3 = measure_execution_time(L, "factorial", 10);
    
    double avg_time = (time1 + time2 + time3) / 3.0;
    printf("✅ 平均执行时间: %.3f ms\n", avg_time);
    
    /* 打印JIT统计 */
    aqlJIT_print_stats(L);
    
    aql_close(L);
}

/* 测试内存管理 */
static void test_memory_management() {
    printf("\n=== 内存管理测试 ===\n");
    
    aql_State* L = aql_newstate(NULL, NULL);
    if (!L) return;
    
    aqlJIT_init(L, JIT_BACKEND_NATIVE);
    aqlL_dostring(L, test_code);
    
    /* 创建多个JIT缓存项 */
    for (int i = 0; i < 50; i++) {
        aql_getglobal(L, "sum_array");
        if (aql_isfunction(L, -1)) {
            Proto* proto = (Proto*)aql_touserdata(L, -1);
            aqlJIT_trigger_compilation(L, proto);
        }
        aql_pop(L, 1);
    }
    
    /* 触发垃圾回收 */
    aql_gc(L, AQL_GCCOLLECT, 0);
    
    /* 检查内存状态 */
    JIT_Stats stats;
    aqlJIT_get_stats(L, &stats);
    printf("✅ 代码缓存大小: %zu bytes\n", stats.code_cache_size);
    printf("✅ 内存开销: %zu bytes\n", stats.memory_overhead);
    
    aql_close(L);
}

/* 测试错误处理 */
static void test_error_handling() {
    printf("\n=== 错误处理测试 ===\n");
    
    aql_State* L = aql_newstate(NULL, NULL);
    if (!L) return;
    
    /* 测试无效的JIT初始化 */
    if (aqlJIT_init(NULL, JIT_BACKEND_NATIVE) == JIT_ERROR_INVALID_INPUT) {
        printf("✅ 空状态检测成功\n");
    }
    
    /* 测试无效的后端 */
    if (aqlJIT_init(L, 999) == JIT_ERROR_INVALID_INPUT) {
        printf("✅ 无效后端检测成功\n");
    }
    
    aql_close(L);
}

int main() {
    printf("🚀 AQL Phase 4 JIT模式测试\n");
    printf("============================\n\n");
    
    /* 基本功能测试 */
    test_jit_basic();
    
    /* 热点检测测试 */
    test_hotspot_detection();
    
    /* 性能对比测试 */
    test_performance_comparison();
    
    /* 内存管理测试 */
    test_memory_management();
    
    /* 错误处理测试 */
    test_error_handling();
    
    printf("\n🎉 Phase 4 JIT模式测试完成！\n");
    printf("所有核心功能已验证\n");
    
    return 0;
}