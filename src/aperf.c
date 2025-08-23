/*
** $Id: aperf.c $
** AQL 统一性能监控实现
** 极简设计，零开销，支持typeinter2 + JIT集成
**
** 编译配置：
**   -DAQL_ENABLE_PERF=0  # 生产环境，零开销
**   -DAQL_ENABLE_PERF=1  # 开发环境，全功能
*/

#include "aperf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 定义max宏 */
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#if defined(_WIN32)
#include <windows.h>
static uint64_t get_time_ns(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((double)counter.QuadPart * 1000000000.0 / (double)freq.QuadPart);
}
#elif defined(__APPLE__)
#include <mach/mach_time.h>
static uint64_t get_time_ns(void) {
    static mach_timebase_info_data_t info;
    static bool initialized = false;
    if (!initialized) {
        mach_timebase_info(&info);
        initialized = true;
    }
    return mach_absolute_time() * info.numer / info.denom;
}
#else
#include <time.h>
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}
#endif

/* ============================================================================
 * 全局状态管理
 * ============================================================================ */

static AQL_PerfMonitor g_perf_monitor = {0};
static AQL_PerfConfig g_perf_config = {0};
static bool g_perf_initialized = false;

/* 操作时间统计 */
#define MAX_OPERATIONS 64

typedef struct OperationTime {
    char name[32];
    AQL_TimeStats stats;
} OperationTime;

static OperationTime g_operations[MAX_OPERATIONS];
static int g_operation_count = 0;

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

static OperationTime* find_operation(const char *name) {
    for (int i = 0; i < g_operation_count; i++) {
        if (strcmp(g_operations[i].name, name) == 0) {
            return &g_operations[i];
        }
    }
    return NULL;
}

static OperationTime* add_operation(const char *name) {
    if (g_operation_count >= MAX_OPERATIONS) {
        return NULL;
    }
    
    OperationTime *op = &g_operations[g_operation_count++];
    strncpy(op->name, name, sizeof(op->name) - 1);
    op->name[sizeof(op->name) - 1] = '\0';
    memset(&op->stats, 0, sizeof(AQL_TimeStats));
    return op;
}

static void update_time_stats(AQL_TimeStats *stats, uint64_t elapsed_ns) {
    stats->total_ns += elapsed_ns;
    stats->last_ns = elapsed_ns;
    stats->count++;
    
    if (stats->count == 1) {
        stats->min_ns = elapsed_ns;
        stats->max_ns = elapsed_ns;
    } else {
        if (elapsed_ns < stats->min_ns) stats->min_ns = elapsed_ns;
        if (elapsed_ns > stats->max_ns) stats->max_ns = elapsed_ns;
    }
}

/* ============================================================================
 * API 实现
 * ============================================================================ */

/* 初始化性能监控 */
AQL_API void aql_perf_init(aql_State *L) {
    if (g_perf_initialized) return;
    
    memset(&g_perf_monitor, 0, sizeof(AQL_PerfMonitor));
    g_perf_config = AQL_PERF_PRODUCTION;
    g_perf_initialized = true;
}

/* 重置所有监控数据 */
AQL_API void aql_perf_reset(aql_State *L) {
    memset(&g_perf_monitor, 0, sizeof(AQL_PerfMonitor));
    memset(g_operations, 0, sizeof(g_operations));
    g_operation_count = 0;
}

/* 获取监控数据 */
AQL_API AQL_PerfMonitor* aql_perf_get(aql_State *L) {
    (void)L; /* 参数未使用 */
    return &g_perf_monitor;
}

/* 启用/禁用性能监控 */
AQL_API void aql_perf_enable(bool enable) {
    /* 全局启用状态，影响后续实例 */
    /* 注意：需要在aql_State创建前设置 */
    (void)enable;
}

/* 配置性能监控 */
AQL_API void aql_perf_configure(aql_State *L, const AQL_PerfConfig *config) {
    (void)L; /* 参数未使用 */
    if (!config) return;
    
    if (aql_perf_validate_config(config)) {
        g_perf_config = *config;
    }
}

/* 获取当前配置 */
AQL_API AQL_PerfConfig* aql_perf_get_config(aql_State *L) {
    (void)L; /* 参数未使用 */
    return &g_perf_config;
}

/* 获取当前时间（纳秒） */
AQL_API uint64_t aql_perf_get_time_ns(void) {
    return get_time_ns();
}

/* 开始时间统计 */
AQL_API void aql_perf_time_start(aql_State *L, const char *operation) {
    (void)L; /* 参数未使用 */
    if (!operation) return;
    
    OperationTime *op = find_operation(operation);
    if (!op) {
        op = add_operation(operation);
    }
}

/* 结束时间统计 */
AQL_API void aql_perf_time_end(aql_State *L, const char *operation) {
    (void)L; /* 参数未使用 */
    if (!operation) return;
    
    OperationTime *op = find_operation(operation);
    if (op) {
        /* 这里简化处理，实际应该存储开始时间 */
        /* 由于简化设计，此函数主要用于演示 */
    }
}

/* 生成性能报告 */
AQL_API void aql_perf_report(aql_State *L, const char *component) {
    (void)L; /* 参数未使用 */
    
    AQL_PerfMonitor *p = &g_perf_monitor;
    
    printf("=== AQL %s 性能报告 ===\n", component ? component : "System");
    printf("总请求: %llu\n", (unsigned long long)p->total_requests);
    printf("缓存命中: %.1f%%\n", 
           p->total_requests > 0 ? (p->cache_hits * 100.0 / p->total_requests) : 0.0);
    printf("内存使用: %lluKB\n", (unsigned long long)p->memory_kb);
    printf("内存分配: %llu\n", (unsigned long long)p->memory_allocs);
    printf("JIT编译: %llu\n", (unsigned long long)p->jit_compilations);
    printf("类型推断耗时: %.2fms\n", p->type_inference_ns / 1000000.0);
    printf("JIT执行耗时: %.2fms\n", p->jit_execution_ns / 1000000.0);
    printf("错误计数: %llu\n", (unsigned long long)p->error_count);
    printf("内存碎片: %d%%\n", p->pool_fragmentation);
    printf("类型稳定性: %d%%\n", p->type_stability);
    
    /* 操作时间统计 */
    if (g_operation_count > 0) {
        printf("\n--- 操作时间统计 ---\n");
        for (int i = 0; i < g_operation_count; i++) {
            OperationTime *op = &g_operations[i];
            printf("%s: avg=%.2fμs, min=%.2fμs, max=%.2fμs, count=%u\n",
                   op->name,
                   op->stats.count > 0 ? op->stats.total_ns / 1000.0 / op->stats.count : 0.0,
                   op->stats.min_ns / 1000.0,
                   op->stats.max_ns / 1000.0,
                   op->stats.count);
        }
    }
    
    printf("===================\n");
}

/* 验证配置有效性 */
AQL_API bool aql_perf_validate_config(const AQL_PerfConfig *config) {
    if (!config) return false;
    
    if (config->log_level > 3) return false;
    if (config->report_interval > 3600) return false; /* 最大1小时 */
    if (config->max_memory_kb > 1024 * 1024) return false; /* 最大1GB */
    
    return true;
}

/* ============================================================================
 * 简化宏实现
 * ============================================================================ */

#if AQL_ENABLE_PERF

/* 简化的时间统计宏实现 */
#define PERF_TIME_START(L, op) \
    do { \
        if (AQL_ENABLE_PERF) { \
            aql_perf_time_start(L, op); \
        } \
    } while (0)

#define PERF_TIME_END(L, op) \
    do { \
        if (AQL_ENABLE_PERF) { \
            aql_perf_time_end(L, op); \
        } \
    } while (0)

#else

#define PERF_TIME_START(L, op) ((void)0)
#define PERF_TIME_END(L, op) ((void)0)

#endif

/* ============================================================================
 * 初始化钩子
 * ============================================================================ */

/* 在aql_newstate中调用 */
void aql_perf_global_init(aql_State *L) {
    aql_perf_init(L);
}

/* 在aql_close中调用 */
void aql_perf_global_cleanup(aql_State *L) {
    (void)L;
    /* 全局状态无需清理 */
}