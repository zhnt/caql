/*
** $Id: ajit.c $
** Just-In-Time Compiler implementation for AQL
** Based on LuaJIT patterns for tracing JIT compilation
** See Copyright Notice in aql.h
*/

#include "ajit.h"
#include "amem.h"
#include "adebug_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Platform-specific includes */
#ifdef _WIN32
    #include <windows.h>
    #define WIN32_LEAN_AND_MEAN
#else
    #include <sys/mman.h>
    #include <unistd.h>
    #include <pthread.h>
#endif

#if AQL_USE_JIT

/*
** Forward declarations for internal functions
*/
static double get_high_precision_time(void);
static void update_cache_stats(aql_State *L, int is_hit);
static void update_compile_stats(aql_State *L, double compile_time);
static void set_jit_error(aql_State *L, int code, const char *message);

/*
** JIT State Management
*/
static JIT_State *g_jit_state = NULL;

/*
** Hotspot Detection and Profiling
*/

/* Calculate hotspot score based on multiple dimensions */
static double calculate_hotspot_score_internal(const JIT_HotspotInfo *info, 
                                              const JIT_HotspotConfig *config) {
    if (!info || !config) return 0.0;
    
    double score = 0.0;
    
    /* Call frequency score (normalized to 0-100) */
    double call_score = (double)info->call_count / config->min_calls * 100.0;
    if (call_score > 100.0) call_score = 100.0;
    
    /* Execution efficiency score (inverse of average time) */
    double time_score = 0.0;
    if (info->avg_time_per_call > 0) {
        time_score = (config->max_avg_time / info->avg_time_per_call) * 100.0;
        if (time_score > 100.0) time_score = 100.0;
    }
    
    /* Code size score (inverse of bytecode size) */
    double size_score = 0.0;
    if (info->bytecode_size > 0) {
        size_score = ((double)config->max_bytecode_size / info->bytecode_size) * 100.0;
        if (size_score > 100.0) size_score = 100.0;
    }
    
    /* Loop complexity score */
    double loop_score = (double)info->loop_count * 10.0;
    if (loop_score > 100.0) loop_score = 100.0;
    
    /* Weighted final score */
    score = call_score * config->call_weight +
            time_score * config->time_weight +
            size_score * config->size_weight +
            loop_score * config->loop_weight;
    
    return score;
}

static void update_hotspot_info(JIT_HotspotInfo *info, double execution_time) {
    if (!info) return;
    
    info->execution_time += execution_time;
    if (info->call_count > 0) {
        info->avg_time_per_call = info->execution_time / info->call_count;
    }
    
    /* Simple fallback for backward compatibility */
    if (info->call_count >= JIT_MIN_HOTSPOT_CALLS ||
        info->avg_time_per_call > 1.0 /* 1ms threshold */) {
        info->is_hot = 1;
    }
}

/*
** JIT Initialization
*/
int aqlJIT_init(aql_State *L, JIT_Backend backend) {
    if (!L) {
        return JIT_ERROR_INVALID_INPUT;
    }
    
    JIT_State *js = (JIT_State *)aqlM_malloc(L, sizeof(JIT_State));
    if (!js) {
        return JIT_ERROR_OUT_OF_MEMORY;
    }
    
    memset(js, 0, sizeof(JIT_State));
    js->backend = backend;
    js->enabled = 1;
    js->config.default_level = JIT_LEVEL_OPTIMIZED;
    js->config.hotspot_threshold = JIT_MIN_HOTSPOT_CALLS;
    js->config.max_code_cache_size = JIT_CODE_CACHE_SIZE;
    
    /* Initialize hotspot detection configuration */
    js->config.hotspot.call_weight = 0.4;
    js->config.hotspot.time_weight = 0.3;
    js->config.hotspot.size_weight = 0.2;
    js->config.hotspot.loop_weight = 0.1;
    js->config.hotspot.threshold = 60.0;
    js->config.hotspot.min_calls = JIT_MIN_HOTSPOT_CALLS;
    js->config.hotspot.max_avg_time = 10.0; /* 10ms max average time */
    js->config.hotspot.max_bytecode_size = 1000; /* Max 1000 bytes */
    
    /* Initialize hash table for JIT cache */
    for (int i = 0; i < JIT_CACHE_BUCKETS; i++) {
        js->cache[i] = NULL;
    }
    
    /* Clear error state */
    aqlJIT_clear_error(&js->last_error);
    
    /* Initialize performance monitor */
    memset(&js->perf_monitor, 0, sizeof(JIT_PerfMonitor));
    
    /* Initialize LRU cache management */
    js->lru_head = NULL;
    js->lru_tail = NULL;
    js->cache_count = 0;
    js->max_cache_entries = 100; /* Default max entries */
    
    L->jit_state = js;
    g_jit_state = js;
    
    AQL_DEBUG(1, "JIT initialized with backend %d", backend);
    return JIT_ERROR_NONE;
}

void aqlJIT_close(aql_State *L) {
    if (!L || !L->jit_state) return;
    
    JIT_State *js = L->jit_state;
    
    /* Clear JIT cache */
    aqlJIT_cache_clear(L);
    
    aqlM_free(L, js, sizeof(JIT_State));
    L->jit_state = NULL;
    
    if (g_jit_state == js) {
        g_jit_state = NULL;
    }
    
    AQL_DEBUG(1, "JIT closed");
}

/*
** JIT Context Management
*/
JIT_Context *aqlJIT_create_context(aql_State *L, Proto *proto) {
    if (!L || !proto) {
        if (L) set_jit_error(L, JIT_ERROR_INVALID_INPUT, "Invalid proto parameter");
        return NULL;
    }
    
    if (!L->jit_state) {
        set_jit_error(L, JIT_ERROR_INITIALIZATION, "JIT not initialized");
        return NULL;
    }
    
    JIT_Context *ctx = (JIT_Context *)aqlM_malloc(L, sizeof(JIT_Context));
    if (!ctx) {
        set_jit_error(L, JIT_ERROR_OUT_OF_MEMORY, "Failed to allocate JIT context");
        return NULL;
    }
    
    memset(ctx, 0, sizeof(JIT_Context));
    ctx->L = L;
    ctx->proto = proto;
    ctx->backend = L->jit_state->config.backend;
    ctx->level = L->jit_state->config.default_level;
    
    /* Initialize hotspot info */
    ctx->hotspot = (JIT_HotspotInfo *)aqlM_malloc(L, sizeof(JIT_HotspotInfo));
    if (ctx->hotspot) {
        memset(ctx->hotspot, 0, sizeof(JIT_HotspotInfo));
    }
    
    AQL_DEBUG(2, "Created JIT context for function %p", proto);
    return ctx;
}

void aqlJIT_destroy_context(JIT_Context *ctx) {
    if (!ctx) return;
    
    if (ctx->code_buffer) {
        aqlJIT_free_code(ctx->code_buffer, ctx->code_size);
    }
    
    if (ctx->hotspot) {
        aqlM_free(ctx->L, ctx->hotspot, sizeof(JIT_HotspotInfo));
    }
    
    if (ctx->metadata) {
        aqlM_free(ctx->L, ctx->metadata, sizeof(void*)); /* Size unknown, use pointer size */
    }
    
    aqlM_free(ctx->L, ctx, sizeof(JIT_Context));
    AQL_DEBUG(2, "Destroyed JIT context");
}

/*
** Hotspot Detection
*/
void aqlJIT_profile_function(aql_State *L, Proto *proto) {
    if (!L || !L->jit_state || !proto) return;
    
    JIT_Cache *cache = aqlJIT_cache_lookup(L, proto);
    if (cache) {
        cache->hotspot.call_count++;
        update_hotspot_info(&cache->hotspot, 0.0 /* placeholder */);
    }
}

int aqlJIT_is_hot(const JIT_HotspotInfo *info) {
    if (!info) return 0;
    return info->is_hot;
}

void aqlJIT_update_hotspot(JIT_HotspotInfo *info, double execution_time) {
    update_hotspot_info(info, execution_time);
}

/*
** JIT Compilation Core
*/
JIT_Function aqlJIT_compile_function(JIT_Context *ctx) {
    if (!ctx || !ctx->proto) return NULL;
    
    AQL_PROFILE_START("jit_compile");
    AQL_DEBUG(1, "Compiling function %s", ctx->proto->source ? ctx->proto->source : "unknown");
    
    double start_time = get_high_precision_time();
    
    /* Check if already compiled */
    JIT_Cache *cache = aqlJIT_cache_lookup(ctx->L, ctx->proto);
    if (cache && cache->compiled_func) {
        AQL_PROFILE_END("jit_compile");
        return cache->compiled_func;
    }
    
    /* Check hotspot threshold */
    if (!aqlJIT_is_hot(ctx->hotspot)) {
        AQL_DEBUG(2, "Function not hot enough for compilation");
        AQL_PROFILE_END("jit_compile");
        return NULL;
    }
    
    /* Perform compilation */
    JIT_Function func = NULL;
    
    switch (ctx->backend) {
        case JIT_BACKEND_NATIVE:
            func = aqlJIT_native_compile(ctx);
            break;
        case JIT_BACKEND_LLVM:
            #if defined(AQL_JIT_LLVM)
            func = aqlJIT_llvm_compile(ctx);
            #endif
            break;
        default:
            AQL_DEBUG(1, "Unsupported JIT backend %d", ctx->backend);
            break;
    }
    
    if (func) {
        /* Cache the compiled function */
        aqlJIT_cache_insert(ctx->L, ctx->proto, func, ctx->code_buffer, ctx->code_size);
        ctx->L->jit_state->stats.functions_compiled++;
        
        /* Update performance statistics */
        double compile_time = get_high_precision_time() - start_time;
        update_compile_stats(ctx->L, compile_time);
        
        AQL_DEBUG(1, "Successfully compiled function in %.3fms", compile_time * 1000.0);
    } else {
        set_jit_error(ctx->L, JIT_ERROR_COMPILATION, "Compilation failed");
    }
    
    AQL_PROFILE_END("jit_compile");
    return func;
}

/*
** LRU Cache Management Functions
*/

/* Remove a cache entry from LRU list */
static void lru_remove(JIT_State *js, JIT_Cache *cache) {
    if (!js || !cache) return;
    
    if (cache->lru_prev) {
        cache->lru_prev->lru_next = cache->lru_next;
    } else {
        js->lru_head = cache->lru_next;
    }
    
    if (cache->lru_next) {
        cache->lru_next->lru_prev = cache->lru_prev;
    } else {
        js->lru_tail = cache->lru_prev;
    }
    
    cache->lru_prev = cache->lru_next = NULL;
}

/* Add cache entry to front of LRU list (most recently used) */
static void lru_add_front(JIT_State *js, JIT_Cache *cache) {
    if (!js || !cache) return;
    
    cache->lru_prev = NULL;
    cache->lru_next = js->lru_head;
    
    if (js->lru_head) {
        js->lru_head->lru_prev = cache;
    } else {
        js->lru_tail = cache;
    }
    
    js->lru_head = cache;
}

/* Move cache entry to front of LRU list */
static void lru_move_to_front(JIT_State *js, JIT_Cache *cache) {
    if (!js || !cache || js->lru_head == cache) return;
    
    lru_remove(js, cache);
    lru_add_front(js, cache);
}

/* Get least recently used cache entry */
__attribute__((unused))
static JIT_Cache *lru_get_tail(JIT_State *js) {
    return js ? js->lru_tail : NULL;
}

/*
** JIT Cache Management
*/
static unsigned int hash_proto(Proto *proto) {
    return (unsigned int)((uintptr_t)proto >> 4);
}

JIT_Cache *aqlJIT_cache_lookup(aql_State *L, Proto *proto) {
    if (!L || !proto || !L->jit_state) return NULL;
    
    JIT_State *js = L->jit_state;
    unsigned int bucket = hash_proto(proto) % JIT_CACHE_BUCKETS;
    
    JIT_Cache *cache = js->cache[bucket];
    while (cache) {
        if (cache->proto == proto) {
            cache->last_access_time = get_high_precision_time();
            cache->access_count++;
            
            /* Move to front of LRU list */
            lru_move_to_front(js, cache);
            
            update_cache_stats(L, 1); /* Cache hit */
            return cache;
        }
        cache = cache->next;
    }
    
    update_cache_stats(L, 0); /* Cache miss */
    return NULL;
}

void aqlJIT_cache_insert(aql_State *L, Proto *proto, JIT_Function func, void *code, size_t size) {
    if (!L || !proto) return;
    
    JIT_State *js = L->jit_state;
    
    /* Check if we need to evict entries first */
    if (js->cache_count >= js->max_cache_entries) {
        aqlJIT_cache_evict_lru(L, js->max_cache_entries - 1);
    }
    
    unsigned int bucket = hash_proto(proto) % JIT_CACHE_BUCKETS;
    
    JIT_Cache *cache = (JIT_Cache *)aqlM_malloc(L, sizeof(JIT_Cache));
    if (!cache) {
        set_jit_error(L, JIT_ERROR_OUT_OF_MEMORY, "Failed to allocate cache entry");
        return;
    }
    
    memset(cache, 0, sizeof(JIT_Cache));
    cache->proto = proto;
    cache->compiled_func = func;
    cache->code_buffer = code;
    cache->code_size = size;
    cache->last_access_time = get_high_precision_time();
    cache->access_count = 1;
    cache->next = js->cache[bucket];
    
    /* Add to hash table */
    js->cache[bucket] = cache;
    
    /* Add to front of LRU list */
    lru_add_front(js, cache);
    
    js->cache_count++;
    js->stats.code_cache_size += size;
    
    AQL_DEBUG(3, "Inserted cache entry: proto=%p, size=%zu, total_entries=%d", 
              proto, size, js->cache_count);
}

void aqlJIT_cache_clear(aql_State *L) {
    if (!L || !L->jit_state) return;
    
    JIT_State *js = L->jit_state;
    
    for (int i = 0; i < JIT_CACHE_BUCKETS; i++) {
        JIT_Cache *cache = js->cache[i];
        while (cache) {
            JIT_Cache *next = cache->next;
            if (cache->code_buffer) {
                aqlJIT_free_code(cache->code_buffer, cache->code_size);
                js->stats.code_cache_size -= cache->code_size;
            }
            aqlM_free(L, cache, sizeof(JIT_Cache));
            cache = next;
        }
        js->cache[i] = NULL;
    }
    
    /* Reset LRU list */
    js->lru_head = NULL;
    js->lru_tail = NULL;
    js->cache_count = 0;
    
    AQL_DEBUG(2, "JIT cache cleared");
}

void aqlJIT_cache_gc(aql_State *L) {
    if (!L || !L->jit_state) return;
    
    JIT_State *js = L->jit_state;
    
    /* Simple LRU eviction - remove old entries */
    double current_time = (double)clock() / CLOCKS_PER_SEC;
    
    for (int i = 0; i < JIT_CACHE_BUCKETS; i++) {
        JIT_Cache *cache = js->cache[i];
        JIT_Cache *prev = NULL;
        
        while (cache) {
            if (current_time - cache->last_access_time > 60.0 /* 60 seconds */) {
                JIT_Cache *next = cache->next;
                if (cache->code_buffer) {
                    aqlJIT_free_code(cache->code_buffer, cache->code_size);
                    js->stats.code_cache_size -= cache->code_size;
                }
                aqlM_free(L, cache, sizeof(JIT_Cache));
                
                if (prev) {
                    prev->next = next;
                } else {
                    js->cache[i] = next;
                }
                cache = next;
            } else {
                prev = cache;
                cache = cache->next;
            }
        }
    }
}

/*
** JIT Statistics and Profiling
*/
void aqlJIT_get_stats(aql_State *L, JIT_Stats *stats) {
    if (!L || !stats || !L->jit_state) return;
    
    /* Copy stats manually to handle structure differences */
    stats->functions_compiled = L->jit_state->stats.functions_compiled;
    stats->functions_executed = L->jit_state->stats.functions_executed;
    stats->optimizations_applied = L->jit_state->stats.optimizations_applied;
    stats->total_compile_time = L->jit_state->stats.total_compile_time;
    stats->total_execution_time = L->jit_state->stats.total_execution_time;
    stats->code_cache_size = L->jit_state->stats.code_cache_size;
    stats->memory_overhead = L->jit_state->stats.memory_overhead;
    stats->speedup_ratio = L->jit_state->stats.speedup_ratio;
}

void aqlJIT_reset_stats(aql_State *L) {
    if (!L || !L->jit_state) return;
    
    memset(&L->jit_state->stats, 0, sizeof(JIT_Stats));
}

void aqlJIT_print_stats(aql_State *L) {
    if (!L || !L->jit_state) return;
    
    printf("=== JIT Statistics ===\n");
    printf("Functions compiled: %d\n", L->jit_state->stats.functions_compiled);
    printf("Functions executed: %d\n", L->jit_state->stats.functions_executed);
    printf("Optimizations applied: %d\n", L->jit_state->stats.optimizations_applied);
    printf("Total compile time: %.3fms\n", L->jit_state->stats.total_compile_time * 1000);
    printf("Total execution time: %.3fms\n", L->jit_state->stats.total_execution_time * 1000);
    printf("Code cache size: %zu bytes\n", L->jit_state->stats.code_cache_size);
    printf("Memory overhead: %zu bytes\n", L->jit_state->stats.memory_overhead);
    printf("Speedup ratio: %.2fx\n", L->jit_state->stats.speedup_ratio > 0 ? L->jit_state->stats.speedup_ratio : 0.0);
}

/*
** VM Integration
*/
int aqlJIT_should_compile(aql_State *L, Proto *proto) {
    if (!L || !proto || !L->jit_state) return 0;
    
    JIT_Cache *cache = aqlJIT_cache_lookup(L, proto);
    if (!cache) return 0;
    
    return cache->hotspot.is_hot;
}

void aqlJIT_trigger_compilation(aql_State *L, Proto *proto) {
    if (!L || !proto || !L->jit_state) return;
    
    JIT_Context *ctx = aqlJIT_create_context(L, proto);
    if (ctx) {
        JIT_Function func = aqlJIT_compile_function(ctx);
        if (func) {
            AQL_DEBUG(1, "Triggered compilation for function %p", proto);
        }
        aqlJIT_destroy_context(ctx);
    }
}

/*
** JIT Memory Management (Cross-platform)
*/
AQL_API void *aqlJIT_alloc_code(size_t size) {
    if (size == 0) return NULL;
    
#ifdef _WIN32
    /* Windows: Use VirtualAlloc for executable memory */
    void *ptr = VirtualAlloc(NULL, size, 
                            MEM_COMMIT | MEM_RESERVE, 
                            PAGE_EXECUTE_READWRITE);
    return ptr;
#else
    /* Unix-like: Use mmap for executable memory */
    size_t page_size = getpagesize();
    size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);
    
    void *ptr = mmap(NULL, aligned_size, 
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (ptr == MAP_FAILED) return NULL;
    
    /* Update memory usage statistics if we have a global state */
    if (g_jit_state) {
        aqlJIT_update_memory_usage((aql_State *)g_jit_state, aligned_size, 1);
    }
    
    return ptr;
#endif
}

AQL_API void aqlJIT_free_code(void *ptr, size_t size) {
    if (!ptr) return;
    
#ifdef _WIN32
    /* Windows: Use VirtualFree */
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    /* Unix-like: Use munmap */
    size_t page_size = getpagesize();
    size_t aligned_size = (size + page_size - 1) & ~(page_size - 1);
    
    /* Update memory usage statistics */
    if (g_jit_state) {
        aqlJIT_update_memory_usage((aql_State *)g_jit_state, aligned_size, 0);
    }
    
    munmap(ptr, aligned_size);
#endif
}

void aqlJIT_make_executable(void *ptr, size_t size) {
    /* Already handled in alloc_code */
    (void)ptr;
    (void)size;
}

void aqlJIT_make_writable(void *ptr, size_t size) {
    /* Already handled in alloc_code */
    (void)ptr;
    (void)size;
}

/*
** Advanced Native Backend Implementation using new codegen system
*/
#if defined(AQL_JIT_NATIVE)
#include "acodegen.h"

AQL_API JIT_Function aqlJIT_native_compile(JIT_Context *ctx) {
    if (!ctx || !ctx->proto) return NULL;
    
    AQL_DEBUG(2, "Starting advanced native compilation for function %p", ctx->proto);
    
    /* Detect target architecture */
    CodegenArch arch = ARCH_X86_64;  /* Default to x86-64 */
    #if defined(__aarch64__) || defined(_M_ARM64)
    arch = ARCH_ARM64;
    #elif defined(__x86_64__) || defined(_M_X64)
    arch = ARCH_X86_64;
    #endif
    
    /* Create codegen context */
    CodegenContext *codegen_ctx = aqlCodegen_create_context(arch, ctx->proto);
    if (!codegen_ctx) {
        set_jit_error(ctx->L, JIT_ERROR_OUT_OF_MEMORY, "Failed to create codegen context");
        return NULL;
    }
    
    /* Set optimization level based on JIT level */
    switch (ctx->level) {
        case JIT_LEVEL_BASIC:
            codegen_ctx->opt_config.optimization_level = 0;
            codegen_ctx->opt_config.enable_constant_folding = false;
            codegen_ctx->opt_config.enable_dead_code_elimination = false;
            break;
        case JIT_LEVEL_OPTIMIZED:
            codegen_ctx->opt_config.optimization_level = 2;
            break;
        case JIT_LEVEL_AGGRESSIVE:
            codegen_ctx->opt_config.optimization_level = 3;
            codegen_ctx->opt_config.enable_register_coalescing = true;
            codegen_ctx->opt_config.enable_peephole_optimization = true;
            break;
        default:
            codegen_ctx->opt_config.optimization_level = 1;
            break;
    }
    
    /* Perform compilation */
    int result = aqlCodegen_compile_bytecode(codegen_ctx);
    if (result != 0) {
        set_jit_error(ctx->L, JIT_ERROR_COMPILATION, "Bytecode compilation failed");
        aqlCodegen_destroy_context(codegen_ctx);
        return NULL;
    }
    
    /* Allocate executable memory for the generated code */
    void *code = aqlJIT_alloc_code(codegen_ctx->code_size);
    if (!code) {
        set_jit_error(ctx->L, JIT_ERROR_OUT_OF_MEMORY, "Failed to allocate executable memory");
        aqlCodegen_destroy_context(codegen_ctx);
        return NULL;
    }
    
    /* Copy generated code to executable memory */
    memcpy(code, codegen_ctx->code_buffer, codegen_ctx->code_size);
    
    /* Update JIT context with compilation results */
    ctx->code_buffer = code;
    ctx->code_size = codegen_ctx->code_size;
    ctx->compile_time = codegen_ctx->stats.generation_time;
    ctx->optimization_count = codegen_ctx->stats.optimizations_applied;
    ctx->memory_used = codegen_ctx->stats.memory_used;
    
    AQL_DEBUG(1, "Advanced native compilation successful: %zu bytes, %d optimizations, %.3fms", 
              codegen_ctx->code_size, codegen_ctx->stats.optimizations_applied,
              codegen_ctx->stats.generation_time * 1000.0);
    
    /* Cleanup codegen context */
    aqlCodegen_destroy_context(codegen_ctx);
    
    return (JIT_Function)code;
}
#endif

/*
** LLVM Backend (placeholder)
*/
#if defined(AQL_JIT_LLVM)
int aqlJIT_llvm_init(aql_State *L) {
    return 0; /* Placeholder */
}

void aqlJIT_llvm_shutdown(aql_State *L) {
    /* Placeholder */
}

JIT_Function aqlJIT_llvm_compile(JIT_Context *ctx) {
    return NULL; /* Placeholder */
}
#endif

/*
** High-precision timing for performance monitoring
*/
static double get_high_precision_time(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / frequency.QuadPart;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return ts.tv_sec + ts.tv_nsec / 1e9;
    }
    /* Fallback to standard time */
    return (double)clock() / CLOCKS_PER_SEC;
#endif
}

/*
** Performance Monitoring Implementation
*/
AQL_API void aqlJIT_get_performance_report(aql_State *L, JIT_PerfMonitor *monitor) {
    if (!L || !L->jit_state || !monitor) return;
    
    JIT_PerfMonitor *perf = &L->jit_state->perf_monitor;
    *monitor = *perf;
    
    /* Calculate derived metrics */
    if (perf->compilation_count > 0) {
        monitor->avg_compile_time = perf->total_compile_time / perf->compilation_count;
    }
    
    if (perf->execution_count > 0) {
        monitor->avg_execution_time = perf->total_execution_time / perf->execution_count;
    }
    
    uint64_t total_cache_ops = perf->cache_hits + perf->cache_misses;
    if (total_cache_ops > 0) {
        monitor->cache_hit_rate = (double)perf->cache_hits / total_cache_ops * 100.0;
    }
    
    if (perf->total_execution_time > 0) {
        monitor->jit_overhead_ratio = perf->total_compile_time / perf->total_execution_time;
    }
}

AQL_API void aqlJIT_reset_performance_monitor(aql_State *L) {
    if (!L || !L->jit_state) return;
    
    memset(&L->jit_state->perf_monitor, 0, sizeof(JIT_PerfMonitor));
    AQL_DEBUG(2, "Performance monitor reset");
}

AQL_API void aqlJIT_print_performance_report(aql_State *L) {
    if (!L || !L->jit_state) return;
    
    JIT_PerfMonitor monitor;
    aqlJIT_get_performance_report(L, &monitor);
    
    printf("\n=== JIT Performance Report ===\n");
    printf("Compilation Statistics:\n");
    printf("  Total compilations: %llu\n", (unsigned long long)monitor.compilation_count);
    printf("  Total compile time: %.3fms\n", monitor.total_compile_time * 1000.0);
    printf("  Average compile time: %.3fms\n", monitor.avg_compile_time * 1000.0);
    
    printf("\nExecution Statistics:\n");
    printf("  Total JIT executions: %llu\n", (unsigned long long)monitor.execution_count);
    printf("  Total execution time: %.3fms\n", monitor.total_execution_time * 1000.0);
    printf("  Average execution time: %.3fμs\n", monitor.avg_execution_time * 1000000.0);
    
    printf("\nCache Statistics:\n");
    printf("  Cache hits: %llu\n", (unsigned long long)monitor.cache_hits);
    printf("  Cache misses: %llu\n", (unsigned long long)monitor.cache_misses);
    printf("  Cache hit rate: %.2f%%\n", monitor.cache_hit_rate);
    
    printf("\nMemory Usage:\n");
    printf("  Current memory: %zu bytes\n", monitor.current_memory_usage);
    printf("  Peak memory: %zu bytes\n", monitor.peak_memory_usage);
    
    printf("\nPerformance Metrics:\n");
    printf("  JIT overhead ratio: %.3fx\n", monitor.jit_overhead_ratio);
    printf("  Interpreter time: %.3fms\n", monitor.interpreter_time * 1000.0);
    
    if (monitor.interpreter_time > 0 && monitor.total_execution_time > 0) {
        double speedup = monitor.interpreter_time / monitor.total_execution_time;
        printf("  Speedup vs interpreter: %.2fx\n", speedup);
    }
    printf("===========================\n");
}

AQL_API void aqlJIT_update_memory_usage(aql_State *L, size_t delta, int is_allocation) {
    if (!L || !L->jit_state) return;
    
    JIT_PerfMonitor *perf = &L->jit_state->perf_monitor;
    
    if (is_allocation) {
        perf->current_memory_usage += delta;
        if (perf->current_memory_usage > perf->peak_memory_usage) {
            perf->peak_memory_usage = perf->current_memory_usage;
        }
    } else {
        if (perf->current_memory_usage >= delta) {
            perf->current_memory_usage -= delta;
        } else {
            perf->current_memory_usage = 0;
        }
    }
}

/* Update cache statistics */
static void update_cache_stats(aql_State *L, int is_hit) {
    if (!L || !L->jit_state) return;
    
    JIT_PerfMonitor *perf = &L->jit_state->perf_monitor;
    if (is_hit) {
        perf->cache_hits++;
    } else {
        perf->cache_misses++;
    }
}

/* Update compilation statistics */
static void update_compile_stats(aql_State *L, double compile_time) {
    if (!L || !L->jit_state) return;
    
    JIT_PerfMonitor *perf = &L->jit_state->perf_monitor;
    perf->compilation_count++;
    perf->total_compile_time += compile_time;
}

/* Update execution statistics */
__attribute__((unused))
static void update_execution_stats(aql_State *L, double execution_time) {
    if (!L || !L->jit_state) return;
    
    JIT_PerfMonitor *perf = &L->jit_state->perf_monitor;
    perf->execution_count++;
    perf->total_execution_time += execution_time;
}

/*
** Error Handling Implementation
*/
static const char* jit_error_messages[] = {
    "No error",                    /* JIT_ERROR_NONE (0) */
    "Invalid input parameter",     /* JIT_ERROR_INVALID_INPUT (-1) */
    "Out of memory",               /* JIT_ERROR_OUT_OF_MEMORY (-2) */
    "Compilation failed",          /* JIT_ERROR_COMPILATION (-3) */
    "JIT backend unavailable",     /* JIT_ERROR_BACKEND_UNAVAILABLE (-4) */
    "Optimization failed",         /* JIT_ERROR_OPTIMIZATION_FAILED (-5) */
    "Code too large for JIT",      /* JIT_ERROR_CODE_TOO_LARGE (-6) */
    "Compilation timeout",         /* JIT_ERROR_TIMEOUT (-7) */
    "Initialization failed",       /* JIT_ERROR_INITIALIZATION (-8) */
    "Internal error"               /* JIT_ERROR_INTERNAL (-9) */
};

AQL_API const char *aqlJIT_get_error_message(int error_code) {
    int index = -error_code;  /* Convert negative error code to positive index */
    if (index >= 0 && index < (int)(sizeof(jit_error_messages) / sizeof(jit_error_messages[0]))) {
        return jit_error_messages[index] ? jit_error_messages[index] : "Unknown error";
    }
    return "Invalid error code";
}

AQL_API void aqlJIT_set_error(JIT_Error *error, int code, const char *message, 
                              const char *function, const char *file, int line) {
    if (!error) return;
    
    error->code = code;
    error->message = message ? message : aqlJIT_get_error_message(code);
    error->function = function;
    error->file = file;
    error->line = line;
    error->context = NULL;
}

AQL_API void aqlJIT_clear_error(JIT_Error *error) {
    if (!error) return;
    
    memset(error, 0, sizeof(JIT_Error));
}

/* Set error in JIT state */
static void set_jit_error(aql_State *L, int code, const char *message) {
    if (!L || !L->jit_state) return;
    
    aqlJIT_set_error(&L->jit_state->last_error, code, message, 
                     __func__, __FILE__, __LINE__);
    
    AQL_DEBUG(1, "JIT Error [%d]: %s", code, message ? message : aqlJIT_get_error_message(code));
}

/* Get last error from JIT state */
__attribute__((unused))
static JIT_Error get_last_jit_error(aql_State *L) {
    if (!L || !L->jit_state) {
        return JIT_ERR(JIT_ERROR_INVALID_INPUT, "Invalid state");
    }
    
    return L->jit_state->last_error;
}

/*
** Enhanced Hotspot Detection API
*/
AQL_API double aqlJIT_calculate_hotspot_score(const JIT_HotspotInfo *info, 
                                              const JIT_HotspotConfig *config) {
    return calculate_hotspot_score_internal(info, config);
}

AQL_API void aqlJIT_set_hotspot_config(aql_State *L, const JIT_HotspotConfig *config) {
    if (!L || !L->jit_state || !config) return;
    
    L->jit_state->config.hotspot = *config;
    AQL_DEBUG(2, "Updated hotspot configuration: threshold=%.1f", config->threshold);
}

AQL_API void aqlJIT_get_hotspot_config(aql_State *L, JIT_HotspotConfig *config) {
    if (!L || !L->jit_state || !config) return;
    
    *config = L->jit_state->config.hotspot;
}

/* Enhanced hotspot detection using the new algorithm */
int aqlJIT_is_hot_enhanced(aql_State *L, const JIT_HotspotInfo *info) {
    if (!L || !L->jit_state || !info) return 0;
    
    /* Check minimum requirements first */
    if (info->call_count < L->jit_state->config.hotspot.min_calls) {
        return 0;
    }
    
    /* Check size limits */
    if (info->bytecode_size > L->jit_state->config.hotspot.max_bytecode_size) {
        AQL_DEBUG(3, "Function too large for JIT: %d bytes", info->bytecode_size);
        return 0;
    }
    
    /* Calculate hotspot score */
    double score = calculate_hotspot_score_internal(info, &L->jit_state->config.hotspot);
    int is_hot = score >= L->jit_state->config.hotspot.threshold;
    
    AQL_DEBUG(3, "Hotspot score: %.2f (threshold: %.2f) -> %s", 
              score, L->jit_state->config.hotspot.threshold, 
              is_hot ? "HOT" : "COLD");
    
    return is_hot;
}

/*
** Advanced Cache Management API
*/
AQL_API void aqlJIT_cache_touch(aql_State *L, Proto *proto) {
    if (!L || !proto) return;
    
    JIT_Cache *cache = aqlJIT_cache_lookup(L, proto);
    if (cache) {
        /* Lookup already handles LRU update */
        AQL_DEBUG(3, "Cache touched: proto=%p, access_count=%llu", 
                  proto, (unsigned long long)cache->access_count);
    }
}

AQL_API void aqlJIT_cache_evict_lru(aql_State *L, size_t target_size) {
    if (!L || !L->jit_state) return;
    
    JIT_State *js = L->jit_state;
    
    while (js->cache_count > (int)target_size && js->lru_tail) {
        JIT_Cache *lru_entry = js->lru_tail;
        
        /* Remove from LRU list */
        lru_remove(js, lru_entry);
        
        /* Find and remove from hash table */
        unsigned int bucket = hash_proto(lru_entry->proto) % JIT_CACHE_BUCKETS;
        JIT_Cache **cache_ptr = &js->cache[bucket];
        
        while (*cache_ptr) {
            if (*cache_ptr == lru_entry) {
                *cache_ptr = lru_entry->next;
                break;
            }
            cache_ptr = &(*cache_ptr)->next;
        }
        
        /* Free resources */
        if (lru_entry->code_buffer) {
            aqlJIT_free_code(lru_entry->code_buffer, lru_entry->code_size);
            js->stats.code_cache_size -= lru_entry->code_size;
        }
        
        AQL_DEBUG(3, "Evicted LRU cache entry: proto=%p, access_count=%llu", 
                  lru_entry->proto, (unsigned long long)lru_entry->access_count);
        
        aqlM_free(L, lru_entry, sizeof(JIT_Cache));
        js->cache_count--;
    }
}

AQL_API void aqlJIT_cache_set_max_entries(aql_State *L, int max_entries) {
    if (!L || !L->jit_state || max_entries < 1) return;
    
    JIT_State *js = L->jit_state;
    int old_max = js->max_cache_entries;
    js->max_cache_entries = max_entries;
    
    /* If reducing size, evict excess entries */
    if (max_entries < js->cache_count) {
        aqlJIT_cache_evict_lru(L, max_entries);
    }
    
    AQL_DEBUG(2, "Cache max entries changed: %d -> %d, current: %d", 
              old_max, max_entries, js->cache_count);
}

/*
** Fallback implementations for non-JIT builds
*/
#else /* !AQL_USE_JIT */

/* All functions are no-ops in non-JIT builds */

#endif /* AQL_USE_JIT */