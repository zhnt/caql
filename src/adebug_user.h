/*
** $Id: adebug_user.h $
** AQL User-Friendly Debug System
** 
** Purpose: Beautiful debug output for users and education
** - Command-line debug flags (-v, -vt, -va, -vb, -ve, -vr)
** - Formatted output like minic compiler
** - Token, AST, bytecode, and execution visualization
** - Runtime-controlled debug categories
** - Educational and demonstration features
** 
** See Copyright Notice in aql.h
*/

#ifndef adebug_user_h
#define adebug_user_h

#include "aconf.h"
#include "aobject.h"

/*
** AQL Unified Debug System
** 
** Combines compile-time optimization with runtime flexibility:
** - Zero-cost debugging in release builds (compile-time)
** - Fine-grained runtime control in debug builds (runtime flags)
** - Beautiful formatted output inspired by Lua and minic
** - Command-line controlled debug categories
*/

/* Debug categories for fine-grained control */
typedef enum {
    AQL_DEBUG_NONE    = 0x00,    /* No debug output */
    AQL_DEBUG_LEX     = 0x01,    /* Lexical analysis (-vt) */
    AQL_DEBUG_PARSE   = 0x02,    /* Abstract syntax tree (-va) */
    AQL_DEBUG_CODE    = 0x04,    /* Bytecode instructions (-vb) */
    AQL_DEBUG_VM      = 0x08,    /* VM execution trace (-ve) */
    AQL_DEBUG_REG     = 0x10,    /* Register values (-vr) */
    AQL_DEBUG_MEM     = 0x20,    /* Memory management (-vm) */
    AQL_DEBUG_GC      = 0x40,    /* Garbage collection (-vg) */
    AQL_DEBUG_REPL    = 0x80,    /* REPL operations (-vd) */
    AQL_DEBUG_ALL     = 0xFF     /* All debug info (-v) */
} AQL_DebugFlags;

/* Global debug state */
extern int aql_debug_flags;
extern int aql_debug_enabled;

/* Early exit flags for -s* parameters */
extern int aql_stop_after_lex;
extern int aql_stop_after_parse;
extern int aql_stop_after_compile;

/*
** High-performance debug macros
** - Compile to nothing in release builds
** - Zero overhead when disabled at runtime
** - Rich information when enabled
*/

#ifdef AQL_DEBUG_BUILD

/* Main debug macro with category checking */
#define AQL_DEBUG(category, ...) \
    do { \
        if (aql_debug_enabled && (aql_debug_flags & (category))) { \
            aqlD_printf(__VA_ARGS__); \
        } \
    } while(0)

/* Category-specific debug macros */
#define AQL_DEBUG_LEX_PRINT(...)     AQL_DEBUG(AQL_DEBUG_LEX, __VA_ARGS__)
#define AQL_DEBUG_PARSE_PRINT(...)   AQL_DEBUG(AQL_DEBUG_PARSE, __VA_ARGS__)
#define AQL_DEBUG_CODE_PRINT(...)    AQL_DEBUG(AQL_DEBUG_CODE, __VA_ARGS__)
#define AQL_DEBUG_VM_PRINT(...)      AQL_DEBUG(AQL_DEBUG_VM, __VA_ARGS__)
#define AQL_DEBUG_REG_PRINT(...)     AQL_DEBUG(AQL_DEBUG_REG, __VA_ARGS__)
#define AQL_DEBUG_MEM_PRINT(...)     AQL_DEBUG(AQL_DEBUG_MEM, __VA_ARGS__)
#define AQL_DEBUG_GC_PRINT(...)      AQL_DEBUG(AQL_DEBUG_GC, __VA_ARGS__)
#define AQL_DEBUG_REPL_PRINT(...)    AQL_DEBUG(AQL_DEBUG_REPL, __VA_ARGS__)

/* Conditional execution macros */
#define AQL_IF_DEBUG(category, code) \
    do { \
        if (aql_debug_enabled && (aql_debug_flags & (category))) { \
            code; \
        } \
    } while(0)

/* Performance profiling macros */
#define AQL_PROFILE_START(name) \
    do { \
        if (aql_debug_enabled) { \
            aqlD_profile_start(name); \
        } \
    } while(0)

#define AQL_PROFILE_END(name) \
    do { \
        if (aql_debug_enabled) { \
            aqlD_profile_end(name); \
        } \
    } while(0)

#else /* Release build - everything compiles to nothing */

#define AQL_DEBUG(category, ...)     ((void)0)
#define AQL_DEBUG_LEX_PRINT(...)     ((void)0)
#define AQL_DEBUG_PARSE_PRINT(...)   ((void)0)
#define AQL_DEBUG_CODE_PRINT(...)    ((void)0)
#define AQL_DEBUG_VM_PRINT(...)      ((void)0)
#define AQL_DEBUG_REG_PRINT(...)     ((void)0)
#define AQL_DEBUG_MEM_PRINT(...)     ((void)0)
#define AQL_DEBUG_GC_PRINT(...)      ((void)0)
#define AQL_DEBUG_REPL_PRINT(...)    ((void)0)
#define AQL_IF_DEBUG(category, code) ((void)0)
#define AQL_PROFILE_START(name)      ((void)0)
#define AQL_PROFILE_END(name)        ((void)0)

#endif

/*
** Data structures for debug information
*/

/* Token information for lexical analysis */
typedef struct {
    int type;
    const char *name;
    const char *value;
    int line;
    int column;
} AQL_TokenInfo;

/* AST node information */
typedef struct {
    const char *type;
    const char *value;
    int line;
    int children_count;
} AQL_ASTInfo;

/* Bytecode instruction information */
typedef struct {
    int pc;                  /* Program counter */
    const char *opname;      /* Instruction name (e.g., "ADD", "LOADK") */
    int opcode;              /* OpCode value */
    int a, b, c;             /* ABC format arguments */
    int bx, sbx;             /* Bx/sBx format arguments */
    const char *format;      /* Instruction format ("ABC", "ABx", "AsBx", "Ax") */
    const char *description; /* Human-readable description */
} AQL_InstrInfo;

/* VM execution state */
typedef struct {
    int pc;
    const char *opname;
    const char *description;
    TValue *registers;
    int reg_count;
} AQL_VMState;

/* Register state information for detailed debugging */
typedef struct {
    int reg_id;              /* Register number */
    const char *type_name;   /* Type name (e.g., "integer", "string") */
    const char *value_str;   /* String representation of value */
    int changed;             /* 1 if register changed in this instruction */
} AQL_RegisterInfo;

/* Performance profiling */
typedef struct {
    const char *name;
    double start_time;
    double total_time;
    int call_count;
} AQL_ProfileEntry;

/*
** Debug system API
*/

/* Initialization and control */
void aqlD_init_debug(void);
void aqlD_set_debug_flags(int flags);
int aqlD_get_debug_flags(void);
void aqlD_enable_debug(int enable);
const char *aqlD_flags_to_string(int flags);

/* Formatted output functions */
void aqlD_printf(const char *fmt, ...);
void aqlD_print_header(const char *title);
void aqlD_print_separator(void);

/* Lexical analysis debug */
void aqlD_print_tokens_header(void);
void aqlD_print_token(int index, AQL_TokenInfo *token);
void aqlD_print_tokens_footer(int total_tokens);

/* Parse debug (AST) */
void aqlD_print_ast_header(void);
void aqlD_print_ast_node(AQL_ASTInfo *node, int depth);
void aqlD_print_ast_footer(int total_nodes);

/* Bytecode debug */
void aqlD_print_bytecode_header(void);
void aqlD_print_constants_pool(TValue *constants, int count);
void aqlD_print_instruction_header(void);
void aqlD_print_instruction(AQL_InstrInfo *instr);
void aqlD_print_bytecode_footer(int total_instructions);

/* VM execution debug */
void aqlD_print_execution_header(void);
void aqlD_print_vm_state(AQL_VMState *state);
void aqlD_print_registers(TValue *registers, int count);
void aqlD_print_execution_footer(TValue *result);

/* Register state debug */
void aqlD_print_register_header(void);
void aqlD_print_register_state(TValue *registers, int count, int *changed_regs, int changed_count);
void aqlD_print_register_changes(TValue *old_regs, TValue *new_regs, int count);

/* Memory and GC debug */
void aqlD_print_memory_stats(size_t allocated, size_t freed, size_t peak);
void aqlD_print_gc_stats(int collections, double gc_time);

/* REPL debug */
void aqlD_print_repl_input(const char *input);
void aqlD_print_repl_result(TValue *result);

/* Performance profiling */
void aqlD_profile_start(const char *name);
void aqlD_profile_end(const char *name);
void aqlD_print_profile_report(void);

/* Utility functions */
const char *aqlD_token_name(int token_type);
const char *aqlD_opcode_name(int opcode);
const char *aqlD_value_type_name(TValue *value);
void aqlD_format_value(TValue *value, char *buffer, size_t size);
double aqlD_get_time(void);

/* Integration helpers */
void aqlD_dump_vm_state(aql_State *L);
void aqlD_dump_stack(aql_State *L);
void aqlD_dump_globals(aql_State *L);

#endif /* adebug_user_h */
