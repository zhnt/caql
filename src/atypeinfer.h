/*
** $Id: atypeinfer.h $
** AQL 类型推断系统 - 基于typeinter2-design.md
** 特性：延迟计算 + 内存池 + 批量处理 + perf2集成
**
** 核心设计原则：
** - 高性能：内存池管理，批量处理
** - 错误恢复：冲突解决，失败回退
** - JIT集成：智能触发，类型特化
**
** 使用示例：
**   TypeInferContext *ctx = aqlT_create_context(L);
**   TypeInfo *types = aqlT_infer_function(ctx, proto);
**   if (aqlT_should_jit_compile(L, proto, types)) { ... }
*/

#ifndef AQL_TYPEINFER_H
#define AQL_TYPEINFER_H

#include "aql.h"
#include "aobject.h"
#include "aperf.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 核心类型定义
 * ============================================================================ */

/* AQL类型枚举 - 扩展版本 */
typedef enum AQL_Type {
    /* 基础类型 (0-7, 3 bits) */
    AQL_TYPE_NIL = 0,
    AQL_TYPE_BOOLEAN,
    AQL_TYPE_INTEGER,
    AQL_TYPE_FLOAT,
    AQL_TYPE_STRING,
    AQL_TYPE_FUNCTION,
    AQL_TYPE_USERDATA,
    AQL_TYPE_ANY,           /* 动态类型 */
    
    /* 容器类型 (8-15) */
    AQL_TYPE_ARRAY = 8,
    AQL_TYPE_SLICE,
    AQL_TYPE_DICT,
    AQL_TYPE_VECTOR,
    
    /* 复合类型 (16-31) */
    AQL_TYPE_TUPLE = 16,
    AQL_TYPE_UNION,
    AQL_TYPE_UNKNOWN = 31   /* 推断失败 */
} AQL_Type;

/* 类型推断状态 */
typedef enum TypeInferState {
    TYPE_STATE_UNKNOWN = 0,
    TYPE_STATE_PENDING,      /* 延迟计算中 */
    TYPE_STATE_COMPUTED,     /* 已计算 */
    TYPE_STATE_INVALID       /* 计算失败 */
} TypeInferState;

/* 类型冲突解决策略 */
typedef enum TypeConflictResolution {
    CONFLICT_RESOLUTION_UNION = 0,      /* 类型联合 */
    CONFLICT_RESOLUTION_PROMOTION,      /* 类型提升 */
    CONFLICT_RESOLUTION_ERROR,          /* 报告错误 */
    CONFLICT_RESOLUTION_DYNAMIC         /* 降级到动态类型 */
} TypeConflictResolution;

/* 推断失败回退级别 */
typedef enum FallbackLevel {
    FALLBACK_NONE = 0,
    FALLBACK_TO_KNOWN,      /* 使用已知类型 */
    FALLBACK_TO_ANY,        /* 降级到动态类型 */
    FALLBACK_TO_RUNTIME,    /* 运行时类型检查 */
    FALLBACK_TO_ERROR       /* 报告错误 */
} FallbackLevel;

/* ============================================================================
 * 核心数据结构
 * ============================================================================ */

/* 类型信息结构 - 32字节优化版本 */
typedef struct TypeInfo {
    AQL_Type inferred_type;     /* 推断出的类型 (4 bytes) */
    AQL_Type actual_type;       /* 运行时实际类型 (4 bytes) */
    
    double confidence;          /* 推断置信度 0-100% (8 bytes) */
    uint32_t usage_count;       /* 使用次数统计 (4 bytes) */
    uint32_t mutation_count;    /* 修改次数统计 (4 bytes) */
    
    TypeInferState state;       /* 推断状态 (4 bytes) */
    uint32_t flags;             /* 状态标志位 (4 bytes) */
} TypeInfo;

/* 类型约束结构 */
typedef struct TypeConstraint {
    AQL_Type required_type;     /* 要求的类型 */
    AQL_Type alternative_type;  /* 备选类型 */
    double weight;              /* 约束权重 */
    struct TypeConstraint *next;
} TypeConstraint;

/* 类型冲突信息 */
typedef struct TypeConflict {
    AQL_Type type1;
    AQL_Type type2;
    TypeConflictResolution resolution;
    AQL_Type resolved_type;
    const char *error_message;
    int line_number;
} TypeConflict;

/* 推断失败回退信息 */
typedef struct InferenceFallback {
    FallbackLevel level;
    AQL_Type fallback_type;
    const char *reason;
    void (*recovery_fn)(aql_State *, int);
} InferenceFallback;

/* ============================================================================
 * 内存池管理 - 基于typeinter2-design.md
 * ============================================================================ */

#define TYPEINFO_POOL_SIZE 1024
#define TYPEINFO_BATCH_ALLOC 32

/* TypeInfo内存池 */
typedef struct TypeInfoPool {
    TypeInfo *pool[TYPEINFO_POOL_SIZE];
    uint32_t free_list[TYPEINFO_POOL_SIZE];
    uint32_t free_count;
    uint32_t next_batch;
    bool initialized;
} TypeInfoPool;

/* 延迟计算结构 */
typedef struct DeferredTypeInfo {
    TypeInfo *base;
    TypeInferState state;
    uint32_t dependencies[8];  /* 依赖的其他类型 */
    uint32_t dep_count;
    void (*compute_fn)(struct DeferredTypeInfo *);
    void *context;
} DeferredTypeInfo;

/* 延迟计算调度器 */
typedef struct TypeComputeScheduler {
    DeferredTypeInfo *pending_queue[256];
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t batch_size;
    uint64_t compute_budget;
} TypeComputeScheduler;

/* 批量更新系统 */
typedef struct TypeUpdateBatch {
    TypeInfo **updates;
    uint32_t capacity;
    uint32_t count;
    bool dirty;
} TypeUpdateBatch;

/* ============================================================================
 * 前向分析状态 - 基于typeinter2-design.md
 * ============================================================================ */

/* 前向分析状态 */
typedef struct ForwardAnalysisState {
    TypeInfo *locals[256];      /* 局部变量类型 */
    TypeInfo *stack[64];        /* 操作数栈类型 */
    uint32_t local_count;
    uint32_t stack_top;
    TypeConstraint *constraints;
    uint32_t analysis_depth;
} ForwardAnalysisState;

/* 类型推断上下文 */
typedef struct TypeInferContext {
    aql_State *L;                       /* AQL状态 */
    TypeInfoPool *pool;                 /* 内存池 */
    TypeComputeScheduler *scheduler;    /* 延迟计算调度器 */
    TypeUpdateBatch *batch;             /* 批量更新 */
    ForwardAnalysisState *forward;      /* 前向分析状态 */
    
    /* 配置参数 */
    double confidence_threshold;        /* 置信度阈值 */
    uint32_t max_analysis_depth;       /* 最大分析深度 */
    uint32_t complexity_budget;        /* 复杂度预算 */
    
    /* 统计信息 */
    uint32_t inference_requests;       /* 推断请求数 */
    uint32_t cache_hits;               /* 缓存命中数 */
    uint32_t fallback_count;           /* 回退次数 */
} TypeInferContext;

/* ============================================================================
 * 性能监控集成 - 基于perf2-design.md
 * ============================================================================ */

/* 性能监控宏 - 与perf2集成 */
#define TYPEINFER_PERF_START(L) \
    uint64_t _ti_start = 0; \
    if (AQL_PERF_ENABLED()) _ti_start = get_time_nanoseconds()

#define TYPEINFER_PERF_END(L, success) do { \
    if (AQL_PERF_ENABLED()) { \
        uint64_t _ti_duration = get_time_nanoseconds() - _ti_start; \
        PERF_ADD(type_inference_ns, _ti_duration); \
        PERF_INC(total_requests); \
        if (success) PERF_INC(cache_hits); \
    } \
} while(0)

#define TYPEINFER_PERF_CACHE_HIT(L) \
    if (AQL_PERF_ENABLED()) PERF_INC(cache_hits)

#define TYPEINFER_PERF_FALLBACK(L, reason) \
    if (AQL_PERF_ENABLED()) PERF_INC(error_count)

/* ============================================================================
 * API 声明
 * ============================================================================ */

/* 上下文管理 */
AQL_API TypeInferContext* aqlT_create_context(aql_State *L);
AQL_API void aqlT_destroy_context(TypeInferContext *ctx);
AQL_API void aqlT_reset_context(TypeInferContext *ctx);

/* 内存池管理 */
AQL_API TypeInfo* aqlT_alloc_typeinfo(TypeInferContext *ctx);
AQL_API void aqlT_free_typeinfo(TypeInferContext *ctx, TypeInfo *info);
AQL_API void aqlT_reset_pool(TypeInferContext *ctx);

/* 核心推断接口 */
AQL_API int aqlT_infer_types(aql_State *L, Proto *p);
AQL_API TypeInfo* aqlT_infer_function(TypeInferContext *ctx, Proto *p);
AQL_API AQL_Type aqlT_infer_expression(TypeInferContext *ctx, int pc);
AQL_API AQL_Type aqlT_infer_binary_op(AQL_Type left, AQL_Type right, int op);
AQL_API AQL_Type aqlT_infer_literal(TValue *value);

/* 前向分析 */
AQL_API void aqlT_forward_analysis(TypeInferContext *ctx, Proto *p);
AQL_API void aqlT_analyze_instruction(TypeInferContext *ctx, Instruction inst, int pc);

/* 延迟计算 */
AQL_API void aqlT_defer_computation(TypeInferContext *ctx, TypeInfo *info, 
                                   void (*compute_fn)(DeferredTypeInfo *));
AQL_API void aqlT_compute_deferred_batch(TypeInferContext *ctx);

/* 批量更新 */
AQL_API void aqlT_batch_update(TypeInferContext *ctx, TypeInfo *info, AQL_Type new_type);
AQL_API void aqlT_flush_batch(TypeInferContext *ctx);

/* 错误处理 */
AQL_API AQL_Type aqlT_resolve_conflict(TypeConflict *conflict);
AQL_API InferenceFallback aqlT_handle_failure(TypeInferContext *ctx, int pc, const char *reason);

/* JIT集成接口 */
AQL_API bool aqlT_should_jit_compile(aql_State *L, Proto *p, TypeInfo *types);
AQL_API void aqlT_prepare_jit_types(TypeInferContext *ctx, Proto *p, TypeInfo **types);
AQL_API double aqlT_compute_type_stability(TypeInfo *types, int count);

/* 工具函数 */
AQL_API const char* aqlT_type_name(AQL_Type type);
AQL_API bool aqlT_is_compatible(AQL_Type a, AQL_Type b);
AQL_API AQL_Type aqlT_promote_types(AQL_Type a, AQL_Type b);
AQL_API uint64_t get_time_nanoseconds(void);

/* 调试和诊断 */
AQL_API void aqlT_print_typeinfer_info(TypeInfo *info);
AQL_API void aqlT_print_context_stats(TypeInferContext *ctx);
AQL_API void aqlT_validate_context(TypeInferContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* AQL_TYPEINFER_H */