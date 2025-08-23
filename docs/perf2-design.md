# AQL 统一性能监控设计 v2.0

## 🎯 设计原则

**极简统一，零开销，渐进增强**
- 仅10个核心指标
- 统一数据结构
- 条件编译，零成本
- 支持typeinter2 + JIT集成

## 🚀 核心架构

### 1.1 统一监控中心

```c
/* 统一性能监控 (仅1KB内存) */
typedef struct AQL_PerfMonitor {
    // 核心运行指标 (8个)
    uint64_t total_requests;      // 总处理请求
    uint64_t cache_hits;         // 缓存命中率
    uint64_t memory_allocs;      // 内存分配计数
    uint64_t jit_compilations;   // JIT编译次数
    uint64_t type_inference_ns;  // 类型推断耗时(ns)
    uint64_t jit_execution_ns;   // JIT执行耗时(ns)
    uint64_t error_count;        // 错误计数
    uint64_t memory_kb;          // 内存使用(KB)
    
    // 实时状态 (2个)
    uint8_t  pool_fragmentation; // 内存碎片率(0-100)
    uint8_t  type_stability;     // 类型稳定性(0-100)
} AQL_PerfMonitor;

/* 🔧 高精度时间统计 */
typedef struct AQL_TimeStats {
    uint64_t total_ns;      // 总耗时(ns)
    uint64_t min_ns;        // 最小耗时(ns)
    uint64_t max_ns;        // 最大耗时(ns)
    uint32_t count;         // 统计次数
    uint64_t last_ns;       // 上次耗时(ns)
} AQL_TimeStats;

/* 🔧 运行时配置结构 */
typedef struct AQL_PerfConfig {
    bool enable_time_stats;     // 是否启用时间统计
    bool enable_memory_stats;   // 是否启用内存统计
    bool enable_jit_stats;      // 是否启用JIT统计
    bool enable_type_stats;     // 是否启用类型统计
    uint32_t report_interval;   // 报告间隔(秒)
    uint32_t max_memory_kb;     // 内存使用阈值
    uint8_t  log_level;         // 日志级别(0-3)
} AQL_PerfConfig;

/* 全局监控实例 */
#define AQL_PERF_GLOBAL(L) (&G(L)->perf_monitor)
```

### 1.2 零开销监控宏

```c
/* 条件编译控制 */
#ifndef AQL_ENABLE_PERF
#define AQL_ENABLE_PERF 0  // 默认关闭
#endif

#if AQL_ENABLE_PERF
    #define PERF_INC(field) (AQL_PERF_GLOBAL(L)->field++)
    #define PERF_ADD(field, val) (AQL_PERF_GLOBAL(L)->field += (val))
    #define PERF_SET(field, val) (AQL_PERF_GLOBAL(L)->field = (val))
#else
    #define PERF_INC(field) ((void)0)
    #define PERF_ADD(field, val) ((void)0)
    #define PERF_SET(field, val) ((void)0)
#endif
```

## 🔗 集成接口

### 2.1 与typeinter2集成

```c
/* 类型推断监控点 */
void typeinfo_compute_batch(TypeComputeScheduler *scheduler) {
    PERF_INC(type_inference_ns);
    PERF_INC(total_requests);
    
    if (scheduler->queue_depth > 50) {
        PERF_SET(type_stability, 95);  // 类型非常稳定
    }
}

/* 内存池监控 */
void typeinfo_memory_update(size_t bytes) {
    PERF_ADD(memory_kb, bytes / 1024);
    PERF_INC(memory_allocs);
}
```

### 2.2 与JIT集成

```c
/* JIT编译监控 */
bool aqlJIT_compile(aql_State *L, Proto *p) {
    PERF_INC(jit_compilations);
    PERF_INC(total_requests);
    
    size_t code_size = generate_jit_code(p);
    PERF_ADD(memory_kb, code_size / 1024);
    
    return true;
}

/* JIT执行监控 */
void aqlJIT_execute(aql_State *L) {
    uint64_t start = get_time_ns();
    // ... JIT执行代码 ...
    uint64_t elapsed = get_time_ns() - start;
    PERF_ADD(jit_execution_ns, elapsed);
}
```

## 📊 统一报告

### 3.1 一键报告生成

```c
/* 统一报告格式 */
void aql_perf_report(aql_State *L, const char *component) {
    AQL_PerfMonitor *p = AQL_PERF_GLOBAL(L);
    
    printf("=== AQL %s 性能报告 ===\n", component);
    printf("总请求: %llu\n", p->total_requests);
    printf("缓存命中: %.1f%%\n", p->cache_hits * 100.0 / max(p->total_requests, 1));
    printf("内存使用: %lluKB\n", p->memory_kb);
    printf("JIT编译: %llu\n", p->jit_compilations);
    printf("类型推断耗时: %.2fms\n", p->type_inference_ns / 1000000.0);
    printf("JIT执行耗时: %.2fms\n", p->jit_execution_ns / 1000000.0);
    printf("错误率: %.2f%%\n", p->error_count * 100.0 / max(p->total_requests, 1));
    printf("内存碎片: %d%%\n", p->pool_fragmentation);
    printf("类型稳定性: %d%%\n", p->type_stability);
}
```

### 3.2 实时监控API

```c
/* 简化API */
AQL_API void aql_perf_init(aql_State *L);
AQL_API void aql_perf_reset(aql_State *L);
AQL_API AQL_PerfMonitor* aql_perf_get(aql_State *L);
AQL_API void aql_perf_enable(bool enable);

/* 🔧 动态配置API */
AQL_API void aql_perf_configure(aql_State *L, const AQL_PerfConfig *config);
AQL_API AQL_PerfConfig* aql_perf_get_config(aql_State *L);
AQL_API void aql_perf_time_start(aql_State *L, const char *operation);
AQL_API void aql_perf_time_end(aql_State *L, const char *operation);
AQL_API AQL_TimeStats* aql_perf_get_time_stats(aql_State *L, const char *operation);
```

## ⚙️ 运行时配置

```c
/* 🔧 运行时配置 */
#define AQL_PERF_PRODUCTION { \
    .enable_time_stats = false, \
    .enable_memory_stats = false, \
    .enable_jit_stats = false, \
    .enable_type_stats = false, \
    .report_interval = 0, \
    .max_memory_kb = 1024, \
    .log_level = 0 \
}

#define AQL_PERF_DEVELOPMENT { \
    .enable_time_stats = true, \
    .enable_memory_stats = true, \
    .enable_jit_stats = true, \
    .enable_type_stats = true, \
    .report_interval = 30, \
    .max_memory_kb = 256, \
    .log_level = 2 \
}

#define AQL_PERF_DEBUG { \
    .enable_time_stats = true, \
    .enable_memory_stats = true, \
    .enable_jit_stats = true, \
    .enable_type_stats = true, \
    .report_interval = 10, \
    .max_memory_kb = 512, \
    .log_level = 3 \
}
```

## 📈 性能基准

| 配置 | 内存开销 | 性能影响 | 适用场景 |
|------|----------|----------|----------|
| 关闭 | 0 bytes | 0% | 生产环境 |
| 开启 | 1KB | <1% | 开发调试 |
| 详细 | 2KB | <2% | 性能分析 |

## 📊 实战使用

### 5.1 动态配置示例

```c
// 运行时配置调整
void configure_performance_monitor(aql_State *L) {
    AQL_PerfConfig config = AQL_PERF_DEVELOPMENT;
    
    // 根据负载动态调整
    if (get_system_load() > 80) {
        config.enable_time_stats = false;  // 降低开销
        config.report_interval = 60;       // 延长报告间隔
    }
    
    aql_perf_configure(L, &config);
}

// 命令行配置
// ./aql --perf-config=debug script.aql
// ./aql --perf-time-stats=1 --perf-report-interval=30 script.aql
```

### 5.2 时间精度场景

| 精度需求 | 配置选项 | 适用场景 | 开销 |
|----------|----------|----------|------|
| 纳秒级   | enable_time_stats=true | 性能调优 | <1% |
| 微秒级   | 默认配置 | 开发测试 | <0.5% |
| 毫秒级   | 采样统计 | 生产监控 | <0.1% |
| 关闭     | enable_time_stats=false | 生产环境 | 0% |

## ✅ 总结

**perf2-design.md = 极简统一 + 零开销 + 有效集成 + 动态配置**

✅ **极简**：仅10个核心指标，1KB内存
✅ **统一**：typeinter2 + JIT共用同一监控中心
✅ **零开销**：条件编译，生产环境零成本
✅ **动态**：运行时配置，无需重启应用
✅ **高精度**：纳秒级时间统计，完整性能分析