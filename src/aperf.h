/*
** $Id: aperf.h $
** AQL 统一性能监控头文件
** 极简设计，零开销，支持typeinter2 + JIT集成
**
** 使用示例：
**   PERF_INC(total_requests);
**   PERF_ADD(memory_kb, 1024);
**   aql_perf_report(L, "JIT");
**
** 编译控制：
**   -DAQL_ENABLE_PERF=0  # 生产环境，零开销
**   -DAQL_ENABLE_PERF=1  # 开发环境，全功能
*/

#ifndef AQL_PERF_H
#define AQL_PERF_H

#include "aql.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 核心数据结构
 * ============================================================================ */

/* 统一性能监控中心 (仅1KB内存) */
typedef struct AQL_PerfMonitor {
    uint64_t total_requests;      /* 总处理请求 */
    uint64_t cache_hits;         /* 缓存命中率 */
    uint64_t memory_allocs;      /* 内存分配计数 */
    uint64_t jit_compilations;   /* JIT编译次数 */
    uint64_t type_inference_ns;  /* 类型推断耗时(ns) */
    uint64_t jit_execution_ns;   /* JIT执行耗时(ns) */
    uint64_t error_count;        /* 错误计数 */
    uint64_t memory_kb;          /* 内存使用(KB) */
    
    uint8_t  pool_fragmentation; /* 内存碎片率(0-100) */
    uint8_t  type_stability;     /* 类型稳定性(0-100) */
} AQL_PerfMonitor;

/* 高精度时间统计 */
typedef struct AQL_TimeStats {
    uint64_t total_ns;      /* 总耗时(ns) */
    uint64_t min_ns;        /* 最小耗时(ns) */
    uint64_t max_ns;        /* 最大耗时(ns) */
    uint32_t count;         /* 统计次数 */
    uint64_t last_ns;       /* 上次耗时(ns) */
} AQL_TimeStats;

/* 运行时配置结构 */
typedef struct AQL_PerfConfig {
    bool enable_time_stats;     /* 是否启用时间统计 */
    bool enable_memory_stats;   /* 是否启用内存统计 */
    bool enable_jit_stats;      /* 是否启用JIT统计 */
    bool enable_type_stats;     /* 是否启用类型统计 */
    uint32_t report_interval;   /* 报告间隔(秒) */
    uint32_t max_memory_kb;     /* 内存使用阈值 */
    uint8_t  log_level;         /* 日志级别(0-3) */
} AQL_PerfConfig;

/* ============================================================================
 * 预定义配置
 * ============================================================================ */

static const AQL_PerfConfig AQL_PERF_PRODUCTION = {
    0, 0, 0, 0, 0, 1024, 0
};

static const AQL_PerfConfig AQL_PERF_DEVELOPMENT = {
    1, 1, 1, 1, 30, 256, 2
};

static const AQL_PerfConfig AQL_PERF_DEBUG = {
    1, 1, 1, 1, 10, 512, 3
};

/* ============================================================================
 * 条件编译控制宏
 * ============================================================================ */

#ifndef AQL_ENABLE_PERF
#define AQL_ENABLE_PERF 0  /* 默认关闭 */
#endif

#if AQL_ENABLE_PERF
    /* 启用监控 */
    #define AQL_PERF_ENABLED 1
    #define PERF_INC(field) (aql_perf_get(NULL)->field++)
    #define PERF_ADD(field, val) (aql_perf_get(NULL)->field += (val))
    #define PERF_SET(field, val) (aql_perf_get(NULL)->field = (val))
#else
    /* 禁用监控 - 零开销 */
    #define AQL_PERF_ENABLED 0
    #define PERF_INC(field) ((void)0)
    #define PERF_ADD(field, val) ((void)0)
    #define PERF_SET(field, val) ((void)0)
#endif

/* ============================================================================
 * API 声明
 * ============================================================================ */

/* 初始化性能监控 */
AQL_API void aql_perf_init(aql_State *L);

/* 重置所有监控数据 */
AQL_API void aql_perf_reset(aql_State *L);

/* 获取监控数据 */
AQL_API AQL_PerfMonitor* aql_perf_get(aql_State *L);

/* 启用/禁用性能监控 */
AQL_API void aql_perf_enable(bool enable);

/* 配置性能监控 */
AQL_API void aql_perf_configure(aql_State *L, const AQL_PerfConfig *config);

/* 获取当前配置 */
AQL_API AQL_PerfConfig* aql_perf_get_config(aql_State *L);

/* 时间统计辅助函数 */
AQL_API uint64_t aql_perf_get_time_ns(void);
AQL_API void aql_perf_time_start(aql_State *L, const char *operation);
AQL_API void aql_perf_time_end(aql_State *L, const char *operation);

/* 生成性能报告 */
AQL_API void aql_perf_report(aql_State *L, const char *component);

/* 验证配置有效性 */
AQL_API bool aql_perf_validate_config(const AQL_PerfConfig *config);

/* ============================================================================
 * 内联辅助函数
 * ============================================================================ */


#ifdef __cplusplus
}
#endif

#endif /* AQL_PERF_H */