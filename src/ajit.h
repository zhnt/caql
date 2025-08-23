/*
** $Id: ajit.h $
** Just-In-Time Compiler for AQL
** Based on LuaJIT patterns for tracing JIT compilation
** See Copyright Notice in aql.h
*/

#ifndef ajit_h
#define ajit_h

#include "aconf.h"
#include "aobject.h"
#include "aopcodes.h"
#include "astate.h"

/* JIT Error Codes */
#define JIT_ERROR_NONE              0
#define JIT_ERROR_INVALID_INPUT    -1
#define JIT_ERROR_OUT_OF_MEMORY    -2
#define JIT_ERROR_COMPILATION      -3
#define JIT_ERROR_BACKEND_UNAVAILABLE -4
#define JIT_ERROR_OPTIMIZATION_FAILED -5
#define JIT_ERROR_CODE_TOO_LARGE      -6
#define JIT_ERROR_TIMEOUT             -7
#define JIT_ERROR_INITIALIZATION      -8
#define JIT_ERROR_INTERNAL            -9

/*
** JIT Error Information
*/
typedef struct {
    int code;                /* Error code */
    const char *message;     /* Error message */
    const char *function;    /* Function where error occurred */
    const char *file;        /* Source file */
    int line;                /* Line number */
    void *context;           /* Additional context data */
} JIT_Error;

/*
** JIT Compilation Backend Types
*/
typedef enum {
  JIT_BACKEND_NONE = 0,
  JIT_BACKEND_NATIVE,    /* Direct machine code generation */
  JIT_BACKEND_LLVM,      /* LLVM IR generation */
  JIT_BACKEND_CRANELIFT, /* Cranelift code generator */
  JIT_BACKEND_LIGHTNING, /* GNU Lightning */
  JIT_BACKEND_DYNASM     /* DynASM macro assembler */
} JIT_Backend;

/*
** JIT Compilation Levels
*/
typedef enum {
  JIT_LEVEL_NONE = 0,
  JIT_LEVEL_BASIC,       /* Basic compilation, no optimizations */
  JIT_LEVEL_OPTIMIZED,   /* Standard optimizations */
  JIT_LEVEL_AGGRESSIVE,  /* Aggressive optimizations */
  JIT_LEVEL_ADAPTIVE     /* Adaptive optimization based on profiling */
} JIT_Level;

/*
** JIT Hotspot Detection
*/
typedef struct {
  int call_count;        /* Number of function calls */
  int loop_count;        /* Number of loop iterations */
  int bytecode_size;     /* Size of bytecode */
  double execution_time; /* Total execution time in ms */
  double avg_time_per_call; /* Average time per call */
  aql_byte is_hot;       /* Marked as hot path */
  aql_byte is_compiled;  /* Already JIT compiled */
} JIT_HotspotInfo;

/*
** JIT Hotspot Detection Configuration
*/
typedef struct {
  double call_weight;      /* Call count weight (0.0-1.0) */
  double time_weight;      /* Execution time weight (0.0-1.0) */  
  double size_weight;      /* Code size weight (0.0-1.0) */
  double loop_weight;      /* Loop complexity weight (0.0-1.0) */
  double threshold;        /* Hotspot threshold score */
  int min_calls;           /* Minimum calls before evaluation */
  double max_avg_time;     /* Maximum average time threshold (ms) */
  int max_bytecode_size;   /* Maximum bytecode size for JIT */
} JIT_HotspotConfig;

/*
** JIT Performance Monitor
*/
typedef struct {
  uint64_t compilation_count;     /* Total compilations */
  uint64_t execution_count;       /* Total JIT executions */
  uint64_t cache_hits;            /* Cache hit count */
  uint64_t cache_misses;          /* Cache miss count */
  double total_compile_time;      /* Total compilation time */
  double total_execution_time;    /* Total JIT execution time */
  double avg_compile_time;        /* Average compilation time */
  double avg_execution_time;      /* Average execution time */
  double cache_hit_rate;          /* Cache hit rate percentage */
  size_t peak_memory_usage;       /* Peak memory usage */
  size_t current_memory_usage;    /* Current memory usage */
  double interpreter_time;        /* Time spent in interpreter */
  double jit_overhead_ratio;      /* JIT overhead vs execution time */
} JIT_PerfMonitor;

/*
** JIT State (per AQL state)
*/
typedef struct JIT_State {
  int enabled;               /* JIT enabled flag */
  JIT_Backend backend;       /* Active backend */
  struct {
    JIT_Backend backend;        /* Compilation backend */
    JIT_Level default_level;    /* Default optimization level */
    int hotspot_threshold;      /* Call count threshold for compilation */
    int max_inline_size;        /* Maximum function size for inlining */
    int max_unroll_iterations;  /* Maximum loop unroll iterations */
    size_t max_code_cache_size; /* Maximum code cache size */
    aql_byte enable_profiling;  /* Enable profiling */
    aql_byte enable_tracing;    /* Enable execution tracing */
    aql_byte aggressive_inline; /* Aggressive function inlining */
    aql_byte vectorize_loops;   /* Enable loop vectorization */
    JIT_HotspotConfig hotspot;  /* Hotspot detection configuration */
  } config;         /* Configuration */
  struct {
    int functions_compiled;     /* Number of functions compiled */
    int functions_executed;     /* Number of JIT functions executed */
    int optimizations_applied;  /* Number of optimizations applied */
    double total_compile_time;  /* Total compilation time */
    double total_execution_time; /* Total JIT execution time */
    size_t code_cache_size;     /* Size of code cache */
    size_t memory_overhead;     /* JIT memory overhead */
    double speedup_ratio;       /* Speedup vs interpreter */
  } stats;           /* Statistics */
  struct JIT_Cache *cache[256];     /* JIT cache (simple hash table) */
  void *code_allocator;      /* Code memory allocator */
  size_t total_code_size;    /* Total generated code size */
  JIT_Error last_error;      /* Last error information */
  JIT_PerfMonitor perf_monitor; /* Performance monitoring */
  
  /* LRU cache management */
  struct JIT_Cache *lru_head; /* Most recently used */
  struct JIT_Cache *lru_tail; /* Least recently used */
  int cache_count;           /* Current number of cached entries */
  int max_cache_entries;     /* Maximum cache entries */
} JIT_State;

/*
** JIT Compilation Context
*/
typedef struct JIT_Context {
  aql_State *L;              /* AQL state */
  Proto *proto;              /* Function prototype being compiled */
  JIT_Backend backend;       /* Compilation backend */
  JIT_Level level;           /* Optimization level */
  void *code_buffer;         /* Generated machine code */
  size_t code_size;          /* Size of generated code */
  void *metadata;            /* Backend-specific metadata */
  JIT_HotspotInfo *hotspot;  /* Hotspot information */
  
  /* Compilation statistics */
  double compile_time;       /* Time spent compiling */
  int optimization_count;    /* Number of optimizations applied */
  size_t memory_used;        /* Memory used for compilation */
} JIT_Context;

/*
** JIT Function Entry Point
*/
typedef void (*JIT_Function)(aql_State *L, CallInfo *ci);

/* Forward declarations */
typedef struct JIT_Config {
  JIT_Backend backend;        /* Compilation backend */
  JIT_Level default_level;    /* Default optimization level */
  int hotspot_threshold;      /* Call count threshold for compilation */
  int max_inline_size;        /* Maximum function size for inlining */
  int max_unroll_iterations;  /* Maximum loop unroll iterations */
  size_t max_code_cache_size; /* Maximum code cache size */
  aql_byte enable_profiling;  /* Enable profiling */
  aql_byte enable_tracing;    /* Enable execution tracing */
  aql_byte aggressive_inline; /* Aggressive function inlining */
  aql_byte vectorize_loops;   /* Enable loop vectorization */
} JIT_Config;

typedef struct JIT_Stats {
  int functions_compiled;     /* Number of functions compiled */
  int functions_executed;     /* Number of JIT functions executed */
  int optimizations_applied;  /* Number of optimizations applied */
  double total_compile_time;  /* Total compilation time */
  double total_execution_time; /* Total JIT execution time */
  size_t code_cache_size;     /* Size of code cache */
  size_t memory_overhead;     /* JIT memory overhead */
  double speedup_ratio;       /* Speedup vs interpreter */
} JIT_Stats;

/* Forward declarations */
typedef struct JIT_Cache JIT_Cache;

/*
** JIT Cache Entry with LRU support
*/
typedef struct JIT_Cache {
  Proto *proto;               /* Function prototype */
  JIT_Function compiled_func; /* Compiled function */
  void *code_buffer;          /* Machine code buffer */
  size_t code_size;           /* Size of machine code */
  JIT_HotspotInfo hotspot;    /* Hotspot information */
  double last_access_time;    /* Last access timestamp */
  uint64_t access_count;      /* Total access count */
  struct JIT_Cache *next;     /* Next in hash chain */
  
  /* LRU list pointers */
  struct JIT_Cache *lru_prev; /* Previous in LRU list */
  struct JIT_Cache *lru_next; /* Next in LRU list */
} JIT_Cache;

/* JIT Constants */
#define JIT_CACHE_BUCKETS       256
#define JIT_MIN_HOTSPOT_CALLS   10
#define JIT_MAX_INLINE_DEPTH    3
#define JIT_MAX_LOOP_UNROLL     8
#define JIT_CODE_CACHE_SIZE     (16 * 1024 * 1024)
#define JIT_COMPILATION_TIMEOUT 5000

/* JIT Compiler Interface */
AQL_API int aqlJIT_init(aql_State *L, JIT_Backend backend);
AQL_API void aqlJIT_close(aql_State *L);
AQL_API JIT_Context *aqlJIT_create_context(aql_State *L, Proto *proto);
AQL_API void aqlJIT_destroy_context(JIT_Context *ctx);

/* Hotspot Detection */
AQL_API void aqlJIT_profile_function(aql_State *L, Proto *proto);
AQL_API int aqlJIT_is_hot(const JIT_HotspotInfo *info);
AQL_API void aqlJIT_update_hotspot(JIT_HotspotInfo *info, double execution_time);
AQL_API double aqlJIT_calculate_hotspot_score(const JIT_HotspotInfo *info, const JIT_HotspotConfig *config);
AQL_API void aqlJIT_set_hotspot_config(aql_State *L, const JIT_HotspotConfig *config);
AQL_API void aqlJIT_get_hotspot_config(aql_State *L, JIT_HotspotConfig *config);

/* Error Handling */
AQL_API const char *aqlJIT_get_error_message(int error_code);
AQL_API void aqlJIT_set_error(JIT_Error *error, int code, const char *message, 
                              const char *function, const char *file, int line);
AQL_API void aqlJIT_clear_error(JIT_Error *error);

/* Error handling macros */
#define JIT_OK() ((JIT_Error){JIT_ERROR_NONE, NULL, __func__, __FILE__, __LINE__, NULL})
#define JIT_ERR(code, msg) ((JIT_Error){code, msg, __func__, __FILE__, __LINE__, NULL})
#define JIT_CHECK(condition, code, msg) \
    do { \
        if (!(condition)) { \
            return JIT_ERR(code, msg); \
        } \
    } while (0)

/* Compilation */
AQL_API JIT_Function aqlJIT_compile_function(JIT_Context *ctx);
AQL_API int aqlJIT_compile_bytecode(JIT_Context *ctx, Instruction *code, int ncode);

/* Cache Management */
AQL_API JIT_Cache *aqlJIT_cache_lookup(aql_State *L, Proto *proto);
AQL_API void aqlJIT_cache_insert(aql_State *L, Proto *proto, JIT_Function func, void *code, size_t size);
AQL_API void aqlJIT_cache_clear(aql_State *L);
AQL_API void aqlJIT_cache_touch(aql_State *L, Proto *proto);
AQL_API void aqlJIT_cache_evict_lru(aql_State *L, size_t target_size);
AQL_API void aqlJIT_cache_set_max_entries(aql_State *L, int max_entries);

/* Statistics */
AQL_API void aqlJIT_get_stats(aql_State *L, JIT_Stats *stats);
AQL_API void aqlJIT_print_stats(aql_State *L);

/* Performance Monitoring */
AQL_API void aqlJIT_get_performance_report(aql_State *L, JIT_PerfMonitor *monitor);
AQL_API void aqlJIT_reset_performance_monitor(aql_State *L);
AQL_API void aqlJIT_print_performance_report(aql_State *L);
AQL_API void aqlJIT_update_memory_usage(aql_State *L, size_t delta, int is_allocation);

/* VM Integration */
AQL_API int aqlJIT_should_compile(aql_State *L, Proto *proto);
AQL_API void aqlJIT_trigger_compilation(aql_State *L, Proto *proto);

/* Memory Management */
AQL_API void *aqlJIT_alloc_code(size_t size);
AQL_API void aqlJIT_free_code(void *ptr, size_t size);

/* Native Backend */
#if defined(AQL_JIT_NATIVE)
AQL_API JIT_Function aqlJIT_native_compile(JIT_Context *ctx);
#endif

/* LLVM Backend */
#if defined(AQL_JIT_LLVM)
AQL_API int aqlJIT_llvm_init(aql_State *L);
AQL_API JIT_Function aqlJIT_llvm_compile(JIT_Context *ctx);
#endif

#endif /* ajit_h */