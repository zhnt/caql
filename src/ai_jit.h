/*
** $Id: ai_jit.h $
** Just-In-Time Compiler for AQL
** See Copyright Notice in aql.h
*/

#ifndef ai_jit_h
#define ai_jit_h

#include "aconf.h"
#include "aobject.h"
#include "aopcodes.h"
#include "astate.h"

#if AQL_USE_JIT

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
  aql_Byte is_hot;       /* Marked as hot path */
  aql_Byte is_compiled;  /* Already JIT compiled */
} JIT_HotspotInfo;

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

/*
** JIT Compiler Interface
*/
AQL_API int aqlJIT_init(aql_State *L, JIT_Backend backend);
AQL_API void aqlJIT_close(aql_State *L);
AQL_API JIT_Context *aqlJIT_create_context(aql_State *L, Proto *proto);
AQL_API void aqlJIT_destroy_context(JIT_Context *ctx);

/*
** Hotspot Detection and Profiling
*/
AQL_API void aqlJIT_profile_function(aql_State *L, Proto *proto);
AQL_API int aqlJIT_is_hot(const JIT_HotspotInfo *info);
AQL_API void aqlJIT_update_hotspot(JIT_HotspotInfo *info, double execution_time);
AQL_API void aqlJIT_reset_profiling(aql_State *L);

/*
** JIT Compilation
*/
AQL_API JIT_Function aqlJIT_compile_function(JIT_Context *ctx);
AQL_API int aqlJIT_compile_bytecode(JIT_Context *ctx, Instruction *code, int ncode);
AQL_API void aqlJIT_invalidate_function(aql_State *L, Proto *proto);
AQL_API void aqlJIT_recompile_function(aql_State *L, Proto *proto, JIT_Level level);

/*
** JIT Optimizations
*/

/* Instruction-level optimizations */
AQL_API int aqlJIT_optimize_constants(JIT_Context *ctx);
AQL_API int aqlJIT_optimize_arithmetic(JIT_Context *ctx);
AQL_API int aqlJIT_optimize_comparisons(JIT_Context *ctx);
AQL_API int aqlJIT_optimize_branches(JIT_Context *ctx);
AQL_API int aqlJIT_optimize_calls(JIT_Context *ctx);

/* Control flow optimizations */
AQL_API int aqlJIT_optimize_loops(JIT_Context *ctx);
AQL_API int aqlJIT_optimize_jumps(JIT_Context *ctx);
AQL_API int aqlJIT_eliminate_dead_code(JIT_Context *ctx);

/* Data flow optimizations */
AQL_API int aqlJIT_optimize_register_allocation(JIT_Context *ctx);
AQL_API int aqlJIT_optimize_memory_access(JIT_Context *ctx);
AQL_API int aqlJIT_optimize_stack_operations(JIT_Context *ctx);

/* High-level optimizations */
AQL_API int aqlJIT_inline_functions(JIT_Context *ctx);
AQL_API int aqlJIT_unroll_loops(JIT_Context *ctx);
AQL_API int aqlJIT_vectorize_operations(JIT_Context *ctx);

/*
** JIT Code Generation
*/

/* Basic code generation */
AQL_API void aqlJIT_emit_prologue(JIT_Context *ctx);
AQL_API void aqlJIT_emit_epilogue(JIT_Context *ctx);
AQL_API void aqlJIT_emit_instruction(JIT_Context *ctx, Instruction instr);

/* Specific instruction emission */
AQL_API void aqlJIT_emit_move(JIT_Context *ctx, int dst, int src);
AQL_API void aqlJIT_emit_loadk(JIT_Context *ctx, int dst, int k);
AQL_API void aqlJIT_emit_arithmetic(JIT_Context *ctx, OpCode op, int a, int b, int c);
AQL_API void aqlJIT_emit_comparison(JIT_Context *ctx, OpCode op, int a, int b, int c);
AQL_API void aqlJIT_emit_jump(JIT_Context *ctx, int target);
AQL_API void aqlJIT_emit_call(JIT_Context *ctx, int func, int nargs, int nrets);
AQL_API void aqlJIT_emit_return(JIT_Context *ctx, int first, int nrets);

/* Container operations */
AQL_API void aqlJIT_emit_newobject(JIT_Context *ctx, int dst, int type);
AQL_API void aqlJIT_emit_getprop(JIT_Context *ctx, int dst, int obj, int key);
AQL_API void aqlJIT_emit_setprop(JIT_Context *ctx, int obj, int key, int val);
AQL_API void aqlJIT_emit_invoke(JIT_Context *ctx, int obj, int method, int nargs);

/*
** JIT Runtime Support
*/
AQL_API void aqlJIT_execute_function(aql_State *L, JIT_Function func, CallInfo *ci);
AQL_API void aqlJIT_handle_exception(aql_State *L, JIT_Context *ctx);
AQL_API void aqlJIT_garbage_collect(aql_State *L, JIT_Context *ctx);

/*
** JIT Memory Management
*/
AQL_API void *aqlJIT_alloc_code(size_t size);
AQL_API void aqlJIT_free_code(void *ptr, size_t size);
AQL_API void aqlJIT_make_executable(void *ptr, size_t size);
AQL_API void aqlJIT_make_writable(void *ptr, size_t size);

/*
** JIT Cache Management
*/
typedef struct JIT_Cache {
  Proto *proto;               /* Function prototype */
  JIT_Function compiled_func; /* Compiled function */
  void *code_buffer;          /* Machine code buffer */
  size_t code_size;           /* Size of machine code */
  JIT_HotspotInfo hotspot;    /* Hotspot information */
  double last_access_time;    /* Last access timestamp */
  struct JIT_Cache *next;     /* Next in hash chain */
} JIT_Cache;

AQL_API JIT_Cache *aqlJIT_cache_lookup(aql_State *L, Proto *proto);
AQL_API void aqlJIT_cache_insert(aql_State *L, Proto *proto, JIT_Function func, void *code, size_t size);
AQL_API void aqlJIT_cache_remove(aql_State *L, Proto *proto);
AQL_API void aqlJIT_cache_clear(aql_State *L);
AQL_API void aqlJIT_cache_gc(aql_State *L);

/*
** JIT Statistics and Profiling
*/
typedef struct {
  int functions_compiled;     /* Number of functions compiled */
  int functions_executed;     /* Number of JIT functions executed */
  int optimizations_applied;  /* Number of optimizations applied */
  double total_compile_time;  /* Total compilation time */
  double total_execution_time; /* Total JIT execution time */
  size_t code_cache_size;     /* Size of code cache */
  size_t memory_overhead;     /* JIT memory overhead */
  double speedup_ratio;       /* Speedup vs interpreter */
} JIT_Stats;

AQL_API void aqlJIT_get_stats(aql_State *L, JIT_Stats *stats);
AQL_API void aqlJIT_reset_stats(aql_State *L);
AQL_API void aqlJIT_print_stats(aql_State *L);

/*
** JIT Configuration
*/
typedef struct {
  JIT_Backend backend;        /* Compilation backend */
  JIT_Level default_level;    /* Default optimization level */
  int hotspot_threshold;      /* Call count threshold for compilation */
  int max_inline_size;        /* Maximum function size for inlining */
  int max_unroll_iterations;  /* Maximum loop unroll iterations */
  size_t max_code_cache_size; /* Maximum code cache size */
  aql_Byte enable_profiling;  /* Enable profiling */
  aql_Byte enable_tracing;    /* Enable execution tracing */
  aql_Byte aggressive_inline; /* Aggressive function inlining */
  aql_Byte vectorize_loops;   /* Enable loop vectorization */
} JIT_Config;

AQL_API void aqlJIT_get_config(aql_State *L, JIT_Config *config);
AQL_API void aqlJIT_set_config(aql_State *L, const JIT_Config *config);
AQL_API void aqlJIT_load_config_file(aql_State *L, const char *filename);

/*
** JIT Debugging and Analysis
*/
AQL_API void aqlJIT_dump_bytecode(const Proto *proto);
AQL_API void aqlJIT_dump_machine_code(const JIT_Context *ctx);
AQL_API void aqlJIT_dump_optimizations(const JIT_Context *ctx);
AQL_API void aqlJIT_trace_execution(aql_State *L, const char *function_name);

/*
** JIT Error Handling
*/
typedef enum {
  JIT_ERROR_NONE = 0,
  JIT_ERROR_BACKEND_NOT_AVAILABLE,
  JIT_ERROR_COMPILATION_FAILED,
  JIT_ERROR_OUT_OF_MEMORY,
  JIT_ERROR_INVALID_BYTECODE,
  JIT_ERROR_EXECUTION_FAILED,
  JIT_ERROR_OPTIMIZATION_FAILED
} JIT_Error;

AQL_API JIT_Error aqlJIT_get_last_error(aql_State *L);
AQL_API const char *aqlJIT_error_string(JIT_Error error);
AQL_API void aqlJIT_set_error_handler(aql_State *L, void (*handler)(JIT_Error));

/*
** JIT Backend-Specific Interfaces
*/

#if defined(AQL_JIT_LLVM)
/* LLVM Backend */
AQL_API int aqlJIT_llvm_init(aql_State *L);
AQL_API void aqlJIT_llvm_shutdown(aql_State *L);
AQL_API JIT_Function aqlJIT_llvm_compile(JIT_Context *ctx);
AQL_API void aqlJIT_llvm_optimize(JIT_Context *ctx, int level);
#endif

#if defined(AQL_JIT_NATIVE)
/* Native Backend (platform-specific assembly) */
AQL_API int aqlJIT_native_init(aql_State *L);
AQL_API JIT_Function aqlJIT_native_compile(JIT_Context *ctx);
AQL_API void aqlJIT_native_emit_x86_64(JIT_Context *ctx, const char *assembly);
AQL_API void aqlJIT_native_emit_aarch64(JIT_Context *ctx, const char *assembly);
#endif

/*
** JIT Integration with VM
*/
AQL_API void aqlJIT_vm_execute(aql_State *L, CallInfo *ci);
AQL_API int aqlJIT_should_compile(aql_State *L, Proto *proto);
AQL_API void aqlJIT_trigger_compilation(aql_State *L, Proto *proto);

/*
** JIT Constants and Limits
*/
#define JIT_MIN_HOTSPOT_CALLS       10    /* Minimum calls to be considered hot */
#define JIT_MAX_INLINE_DEPTH        3     /* Maximum inlining depth */
#define JIT_MAX_LOOP_UNROLL         8     /* Maximum loop unroll factor */
#define JIT_CODE_CACHE_SIZE         (16 * 1024 * 1024) /* 16MB default cache */
#define JIT_COMPILATION_TIMEOUT     5000  /* 5 second compilation timeout */

/*
** JIT Debugging Support
*/
#if defined(AQL_DEBUG_JIT)
AQL_API void aqlJIT_debug_function(const char *name, JIT_Function func);
AQL_API void aqlJIT_trace_compilation(JIT_Context *ctx, const char *phase);
AQL_API void aqlJIT_verify_code(JIT_Context *ctx);
#else
#define aqlJIT_debug_function(name, func) ((void)0)
#define aqlJIT_trace_compilation(ctx, phase) ((void)0)
#define aqlJIT_verify_code(ctx) ((void)0)
#endif

/*
** Fallback for non-JIT builds
*/
#else /* !AQL_USE_JIT */

typedef struct { int dummy; } JIT_Context;
typedef struct { int dummy; } JIT_HotspotInfo;
typedef struct { int dummy; } JIT_Stats;
typedef struct { int dummy; } JIT_Config;
typedef struct { int dummy; } JIT_Cache;
typedef void (*JIT_Function)(void);
typedef int JIT_Backend;
typedef int JIT_Level;
typedef int JIT_Error;

/* Provide no-op implementations */
#define aqlJIT_init(L, backend) 0
#define aqlJIT_close(L) ((void)0)
#define aqlJIT_profile_function(L, proto) ((void)0)
#define aqlJIT_is_hot(info) 0
#define aqlJIT_compile_function(ctx) NULL
#define aqlJIT_should_compile(L, proto) 0

#endif /* AQL_USE_JIT */

#endif /* ai_jit_h */ 