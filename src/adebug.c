/*
** $Id: adebug.c $
** Zero-cost debugging system implementation for AQL
** See Copyright Notice in aql.h
*/

#include "adebug.h"
#include "astate.h"
#include "aobject.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
** Static debug state (global for simplicity)
*/
static DebugState g_debug_state = {0};

/*
** Initialize debugging system
*/
void aqlD_init(DebugState *ds) {
    if (ds == NULL) ds = &g_debug_state;
    
    memset(ds, 0, sizeof(DebugState));
    ds->enabled = AQL_DEBUG_LEVEL > 0;
    ds->level = AQL_DEBUG_LEVEL;
    ds->trace_enabled = AQL_DEBUG_LEVEL >= 2;
    ds->profile_enabled = AQL_DEBUG_LEVEL >= 1;
    
    /* Initialize buffers */
    ds->debug_capacity = 1000;
    ds->debug_buffer = (DebugInfo *)malloc(ds->debug_capacity * sizeof(DebugInfo));
    
    ds->var_capacity = 500;
    ds->var_buffer = (VarDebug *)malloc(ds->var_capacity * sizeof(VarDebug));
    
    ds->profile_capacity = 100;
    ds->profile_buffer = (ProfileEntry *)malloc(ds->profile_capacity * sizeof(ProfileEntry));
    
    ds->trace_capacity = 10000;
    ds->trace_buffer = (ExecutionTrace *)malloc(ds->trace_capacity * sizeof(ExecutionTrace));
    
    AQL_DEBUG(1, "Debug system initialized at level %d", AQL_DEBUG_LEVEL);
}

/*
** Cleanup debugging system
*/
void aqlD_cleanup(DebugState *ds) {
    if (ds == NULL) ds = &g_debug_state;
    
    free(ds->debug_buffer);
    free(ds->var_buffer);
    free(ds->profile_buffer);
    free(ds->trace_buffer);
    
    memset(ds, 0, sizeof(DebugState));
}

/*
** Get current timestamp
*/
static double get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/*
** Debug output function
*/
void aqlD_debug(const char *file, int line, int level, const char *fmt, ...) {
    if (!aqlD_is_enabled() || level > aqlD_get_level()) return;
    
    va_list args;
    va_start(args, fmt);
    
    /* Print timestamp and location */
    double timestamp = get_timestamp();
    fprintf(stderr, "[%.6f] %s:%d: ", timestamp, file ? file : "?", line);
    
    /* Print message */
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    
    va_end(args);
    
    /* Store in debug buffer */
    if (g_debug_state.debug_count < g_debug_state.debug_capacity) {
        DebugInfo *info = &g_debug_state.debug_buffer[g_debug_state.debug_count++];
        info->file = file;
        info->line = line;
        info->level = level;
        info->timestamp = timestamp;
    }
}

/*
** Trace function
*/
void aqlD_trace(const char *file, int line, const char *function, const char *fmt, ...) {
    if (!aqlD_is_enabled() || !g_debug_state.trace_enabled) return;
    
    va_list args;
    va_start(args, fmt);
    
    double timestamp = get_timestamp();
    fprintf(stderr, "[%.6f] TRACE %s:%d (%s): ", timestamp, file ? file : "?", line, function ? function : "?");
    
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    
    va_end(args);
    
    /* Store in trace buffer */
    if (g_debug_state.trace_count < g_debug_state.trace_capacity) {
        ExecutionTrace *trace = &g_debug_state.trace_buffer[g_debug_state.trace_count++];
        trace->line = line;
        trace->timestamp = timestamp;
    }
}

/*
** Assertion failure handler
*/
void aqlD_assert_failed(const char *file, int line, const char *condition) {
    fprintf(stderr, "ASSERTION FAILED at %s:%d: %s\n", file ? file : "?", line, condition ? condition : "unknown");
    /* In debug mode, we might want to break here */
    abort(); /* For now, just abort */
}

/*
** Variable tracking
*/
void aqlD_track_variable(const char *name, TValue *value, const char *type, int line) {
    if (!aqlD_is_enabled()) return;
    
    if (g_debug_state.var_count < g_debug_state.var_capacity) {
        VarDebug *var = &g_debug_state.var_buffer[g_debug_state.var_count++];
        var->name = (TString *)name; /* Simplified */
        var->value = value ? *value : (TValue){0};
        var->type = type;
        var->line = line;
        var->assignment_count = 1;
    }
    
    AQL_TRACE("Variable %s tracked at line %d", name ? name : "?", line);
}

/*
** Assignment tracking
*/
void aqlD_track_assignment(const char *name, TValue *old_value, TValue *new_value, int line) {
    if (!aqlD_is_enabled()) return;
    
    AQL_DEBUG(2, "Assignment: %s = %p (line %d)", name ? name : "?", (void *)new_value, line);
    aqlD_track_variable(name, new_value, "assignment", line);
}

/*
** Stack frame handling
*/
void aqlD_push_frame(const char *function_name, int line) {
    if (!aqlD_is_enabled()) return;
    
    StackFrame *frame = (StackFrame *)malloc(sizeof(StackFrame));
    if (frame) {
        frame->function_name = (TString *)function_name;
        frame->line = line;
        frame->level = g_debug_state.current_frame ? g_debug_state.current_frame->level + 1 : 0;
        frame->parent = g_debug_state.current_frame;
        g_debug_state.current_frame = frame;
    }
    
    AQL_TRACE("Entering function %s at line %d", function_name ? function_name : "?", line);
}

void aqlD_pop_frame(void) {
    if (!aqlD_is_enabled()) return;
    
    if (g_debug_state.current_frame) {
        StackFrame *frame = g_debug_state.current_frame;
        AQL_TRACE("Exiting function %s", frame->function_name ? frame->function_name : "?");
        g_debug_state.current_frame = frame->parent;
        free(frame);
    }
}

/*
** Performance profiling
*/
void aqlD_profile_start(const char *name) {
    if (!aqlD_is_enabled() || !g_debug_state.profile_enabled) return;
    
    /* Find existing entry or create new one */
    ProfileEntry *entry = NULL;
    for (int i = 0; i < g_debug_state.profile_count; i++) {
        if (g_debug_state.profile_buffer[i].name && strcmp(g_debug_state.profile_buffer[i].name, name) == 0) {
            entry = &g_debug_state.profile_buffer[i];
            break;
        }
    }
    
    if (!entry && g_debug_state.profile_count < g_debug_state.profile_capacity) {
        entry = &g_debug_state.profile_buffer[g_debug_state.profile_count++];
        entry->name = name;
        entry->call_count = 0;
        entry->duration = 0.0;
    }
    
    if (entry) {
        entry->start_time = get_timestamp();
        entry->call_count++;
    }
}

void aqlD_profile_end(const char *name) {
    if (!aqlD_is_enabled() || !g_debug_state.profile_enabled) return;
    
    for (int i = 0; i < g_debug_state.profile_count; i++) {
        if (g_debug_state.profile_buffer[i].name && strcmp(g_debug_state.profile_buffer[i].name, name) == 0) {
            double end_time = get_timestamp();
            g_debug_state.profile_buffer[i].duration += (end_time - g_debug_state.profile_buffer[i].start_time);
            break;
        }
    }
}

/*
** Execution tracing
*/
void aqlD_trace_operation(const char *operation, int line, const char *details) {
    if (!aqlD_is_enabled() || !g_debug_state.trace_enabled) return;
    
    if (g_debug_state.trace_count < g_debug_state.trace_capacity) {
        ExecutionTrace *trace = &g_debug_state.trace_buffer[g_debug_state.trace_count++];
        trace->operation = operation;
        trace->line = line;
        trace->timestamp = get_timestamp();
        trace->details = details;
    }
}

/*
** Debug state queries
*/
int aqlD_is_enabled(void) {
    return g_debug_state.enabled;
}

int aqlD_get_level(void) {
    return g_debug_state.level;
}

void aqlD_set_level(int level) {
    g_debug_state.level = level;
    g_debug_state.enabled = level > 0;
    g_debug_state.trace_enabled = level >= 2;
    g_debug_state.profile_enabled = level >= 1;
}

/*
** Print debug information
*/
void aqlD_print_profile(void) {
    if (!aqlD_is_enabled()) return;
    
    printf("\n=== Performance Profile ===\n");
    for (int i = 0; i < g_debug_state.profile_count; i++) {
        ProfileEntry *entry = &g_debug_state.profile_buffer[i];
        printf("%-20s: %8.3fms (%d calls)\n", 
               entry->name ? entry->name : "?", 
               entry->duration * 1000.0, 
               entry->call_count);
    }
}

/*
** Helper function to safely get string data from TString
*/
static const char *get_tstring_data(TString *ts) {
    if (ts == NULL) return "?";
    /* For now, return a placeholder since TString structure access needs proper implementation */
    return "<function>";
}

void aqlD_print_stack_trace(void) {
    if (!aqlD_is_enabled()) return;
    
    printf("\n=== Stack Trace ===\n");
    StackFrame *frame = g_debug_state.current_frame;
    while (frame != NULL) {
        printf("%*s%s:%d\n", frame->level * 2, "", 
               get_tstring_data(frame->function_name), 
               frame->line);
        frame = frame->parent;
    }
}

/*
** Integration with AQL VM
*/
void aqlD_vm_state(aql_State *L) {
    if (!aqlD_is_enabled()) return;
    
    AQL_DEBUG(1, "VM State: top=%d, base=%d", 
              L ? (int)(L->top - L->base) : 0,
              L ? (int)(L->bottom - L->base) : 0);
}

void aqlD_vm_stack(aql_State *L) {
    if (!aqlD_is_enabled()) return;
    
    AQL_DEBUG(2, "VM Stack dump:");
    /* Implementation would iterate through the stack */
}

void aqlD_vm_globals(aql_State *L) {
    if (!aqlD_is_enabled()) return;
    
    AQL_DEBUG(2, "VM Globals dump:");
    /* Implementation would iterate through globals */
}