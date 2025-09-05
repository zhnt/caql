/*
** $Id: adebug_internal.h $
** AQL Internal Development Debug System
** 
** Purpose: Internal debugging for AQL developers
** - Compile-time debug levels (AQL_DEBUG_LEVEL)
** - Performance profiling and memory tracking
** - Variable and stack frame tracing
** - Low-level VM state inspection
** - Zero-cost in release builds
** 
** See Copyright Notice in aql.h
*/

#ifndef adebug_internal_h
#define adebug_internal_h

#include "aconf.h"
#include "aobject.h"

/*
** Zero-cost debugging system
** - Debug code is completely removed in release builds
** - No runtime overhead when debugging is disabled
** - Rich debugging information when enabled
*/

/* Debug control flags */
#define AQL_DEBUG_ENABLED 1
#define AQL_TRACE_ENABLED 1
#define AQL_PROFILE_ENABLED 1

/* Compile-time debug level selection */
#ifndef AQL_DEBUG_LEVEL
#define AQL_DEBUG_LEVEL 0  /* 0=off, 1=basic, 2=verbose, 3=trace */
#endif

/* Debug macros that compile to nothing in release builds */
#if AQL_DEBUG_LEVEL > 0

#define AQL_DEBUG(level, fmt, ...) \
    do { \
        if (level <= AQL_DEBUG_LEVEL) { \
            aqlD_debug(__FILE__, __LINE__, level, fmt, ##__VA_ARGS__); \
        } \
    } while (0)

#define AQL_TRACE(fmt, ...) \
    do { \
        if (AQL_DEBUG_LEVEL >= 2) { \
            aqlD_trace(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__); \
        } \
    } while (0)

#define AQL_PROFILE_START(name) \
    do { \
        if (AQL_DEBUG_LEVEL >= 1) { \
            aqlD_internal_profile_start(name); \
        } \
    } while (0)

#define AQL_PROFILE_END(name) \
    do { \
        if (AQL_DEBUG_LEVEL >= 1) { \
            aqlD_internal_profile_end(name); \
        } \
    } while (0)

#define AQL_ASSERT(condition) \
    do { \
        if (AQL_DEBUG_LEVEL >= 1 && !(condition)) { \
            aqlD_assert_failed(__FILE__, __LINE__, #condition); \
        } \
    } while (0)

#else /* AQL_DEBUG_LEVEL == 0 */

#define AQL_DEBUG(level, fmt, ...)    ((void)0)
#define AQL_TRACE(fmt, ...)           ((void)0)
#define AQL_PROFILE_START(name)       ((void)0)
#define AQL_PROFILE_END(name)         ((void)0)
#define AQL_ASSERT(condition)         ((void)0)

#endif

/* Debug information structure */
typedef struct DebugInfo {
    const char *file;
    int line;
    const char *function;
    const char *message;
    int level;
    double timestamp;
} DebugInfo;

/* Variable tracking */
typedef struct VarDebug {
    TString *name;
    TValue value;
    const char *type;
    int line;
    int assignment_count;
} VarDebug;

/* Stack frame tracking */
typedef struct StackFrame {
    TString *function_name;
    int line;
    int level;
    struct StackFrame *parent;
} StackFrame;

/* Execution trace */
typedef struct ExecutionTrace {
    const char *operation;
    int line;
    double timestamp;
    const char *details;
} ExecutionTrace;

/* Performance profiling */
typedef struct ProfileEntry {
    const char *name;
    double start_time;
    double duration;
    int call_count;
} ProfileEntry;

/* Debug state */
typedef struct DebugState {
    int enabled;
    int level;
    int trace_enabled;
    int profile_enabled;
    
    /* Debug data storage */
    DebugInfo *debug_buffer;
    int debug_count;
    int debug_capacity;
    
    /* Variable tracking */
    VarDebug *var_buffer;
    int var_count;
    int var_capacity;
    
    /* Stack trace */
    StackFrame *current_frame;
    
    /* Performance profiling */
    ProfileEntry *profile_buffer;
    int profile_count;
    int profile_capacity;
    
    /* Execution trace */
    ExecutionTrace *trace_buffer;
    int trace_count;
    int trace_capacity;
} DebugState;

/* Debug API functions */
void aqlD_init(DebugState *ds);
void aqlD_cleanup(DebugState *ds);

/* Debug output functions */
void aqlD_debug(const char *file, int line, int level, const char *fmt, ...);
void aqlD_trace(const char *file, int line, const char *function, const char *fmt, ...);
void aqlD_assert_failed(const char *file, int line, const char *condition);

/* Variable tracking */
void aqlD_track_variable(const char *name, TValue *value, const char *type, int line);
void aqlD_track_assignment(const char *name, TValue *old_value, TValue *new_value, int line);

/* Stack tracing */
void aqlD_push_frame(const char *function_name, int line);
void aqlD_pop_frame(void);
void aqlD_print_stack_trace(void);

/* Performance profiling */
void aqlD_internal_profile_start(const char *name);
void aqlD_internal_profile_end(const char *name);
void aqlD_print_profile(void);

/* Execution tracing */
void aqlD_trace_operation(const char *operation, int line, const char *details);
void aqlD_print_trace(void);

/* Memory debugging */
void aqlD_track_allocation(void *ptr, size_t size, const char *file, int line);
void aqlD_track_free(void *ptr, const char *file, int line);
void aqlD_print_memory_leaks(void);

/* Conditional compilation helpers */
int aqlD_is_enabled(void);
int aqlD_get_level(void);
void aqlD_set_level(int level);

/* Debug dump functions */
void aqlD_dump_state(DebugState *ds);
void aqlD_dump_variables(DebugState *ds);
void aqlD_dump_stack(DebugState *ds);
void aqlD_dump_profile(DebugState *ds);

/* Integration with AQL VM */
void aqlD_vm_state(aql_State *L);
void aqlD_vm_stack(aql_State *L);
void aqlD_vm_globals(aql_State *L);

#endif /* adebug_internal_h */