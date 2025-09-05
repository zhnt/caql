/*
** $Id: adebug_unified.c $
** AQL Unified Debug System Implementation
** See Copyright Notice in aql.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "adebug_user.h"
#include "aopcodes.h"
#include "alex.h"

/* Global debug state */
int aql_debug_flags = AQL_DEBUG_NONE;
int aql_debug_enabled = 0;

/* Performance profiling state */
static AQL_ProfileEntry *profile_entries = NULL;
static int profile_count = 0;
static int profile_capacity = 0;

/*
** Debug system initialization and control
*/
void aqlD_init_debug(void) {
    aql_debug_enabled = 0;
    aql_debug_flags = AQL_DEBUG_NONE;
    profile_count = 0;
}

void aqlD_set_debug_flags(int flags) {
    aql_debug_flags = flags;
    aql_debug_enabled = (flags != AQL_DEBUG_NONE);
}

int aqlD_get_debug_flags(void) {
    return aql_debug_flags;
}

void aqlD_enable_debug(int enable) {
    aql_debug_enabled = enable;
}

const char *aqlD_flags_to_string(int flags) {
    static char buffer[256];
    buffer[0] = '\0';
    
    if (flags == AQL_DEBUG_NONE) return "none";
    if (flags == AQL_DEBUG_ALL) return "all";
    
    if (flags & AQL_DEBUG_LEX) strcat(buffer, "lex ");
    if (flags & AQL_DEBUG_PARSE) strcat(buffer, "parse ");
    if (flags & AQL_DEBUG_CODE) strcat(buffer, "code ");
    if (flags & AQL_DEBUG_VM) strcat(buffer, "vm ");
    if (flags & AQL_DEBUG_REG) strcat(buffer, "reg ");
    if (flags & AQL_DEBUG_MEM) strcat(buffer, "mem ");
    if (flags & AQL_DEBUG_GC) strcat(buffer, "gc ");
    if (flags & AQL_DEBUG_REPL) strcat(buffer, "repl ");
    
    return buffer;
}

/*
** Formatted debug output
*/
void aqlD_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

void aqlD_print_header(const char *title) {
    aqlD_printf("\n🔍 === %s ===\n", title);
}

void aqlD_print_separator(void) {
    aqlD_printf("  ---   ------       -        -        -       \n");
}

/*
** Lexical analysis debug output
*/
void aqlD_print_tokens_header(void) {
    aqlD_print_header("LEXICAL ANALYSIS (Tokens)");
}

void aqlD_print_token(int index, AQL_TokenInfo *token) {
    if (token->value && strlen(token->value) > 0) {
        aqlD_printf("  %2d: %-12s value=%s (line %d, col %d)\n", 
                   index, token->name, token->value, token->line, token->column);
    } else {
        aqlD_printf("  %2d: %-12s (line %d, col %d)\n", 
                   index, token->name, token->line, token->column);
    }
}

void aqlD_print_tokens_footer(int total_tokens) {
    aqlD_printf("\n📊 Total tokens: %d\n\n", total_tokens);
}

/*
** AST debug output
*/
void aqlD_print_ast_header(void) {
    aqlD_print_header("ABSTRACT SYNTAX TREE (AST)");
}

void aqlD_print_ast_node(AQL_ASTInfo *node, int depth) {
    int i;
    for (i = 0; i < depth * 2; i++) {
        aqlD_printf(" ");
    }
    
    if (node->value) {
        aqlD_printf("%s: %s", node->type, node->value);
    } else {
        aqlD_printf("%s", node->type);
    }
    
    if (node->children_count > 0) {
        aqlD_printf(" (%d children)", node->children_count);
    }
    aqlD_printf("\n");
}

void aqlD_print_ast_footer(int total_nodes) {
    aqlD_printf("\n📊 AST Statistics:\n");
    aqlD_printf("  Total nodes: %d\n\n", total_nodes);
}

/*
** Bytecode debug output
*/
void aqlD_print_bytecode_header(void) {
    aqlD_print_header("BYTECODE INSTRUCTIONS");
}

void aqlD_print_constants_pool(TValue *constants, int count) {
    int i;
    aqlD_printf("📦 Constants Pool:\n");
    for (i = 0; i < count; i++) {
        char buffer[64];
        aqlD_format_value(&constants[i], buffer, sizeof(buffer));
        aqlD_printf("  CONST[%d] = %s\n", i, buffer);
    }
    aqlD_printf("\n");
}

void aqlD_print_instruction_header(void) {
    aqlD_printf("📝 Instructions:\n");
    aqlD_printf("  PC    OPCODE       A        B        C       \n");
    aqlD_print_separator();
}

void aqlD_print_instruction(AQL_InstrInfo *instr) {
    aqlD_printf("  %-4d  %-12s %-8d %-8d %-8d", 
               instr->pc, instr->opname, instr->a, instr->b, instr->c);
    if (instr->description) {
        aqlD_printf("  # %s", instr->description);
    }
    aqlD_printf("\n");
}

void aqlD_print_bytecode_footer(int total_instructions) {
    aqlD_printf("\n📊 Total instructions: %d\n\n", total_instructions);
}

/*
** VM execution debug output
*/
void aqlD_print_execution_header(void) {
    aqlD_print_header("EXECUTION TRACE");
    aqlD_printf("\n");
}

void aqlD_print_vm_state(AQL_VMState *state) {
    aqlD_printf("📍 PC=%d: %s\n", state->pc, state->description);
}

void aqlD_print_registers(TValue *registers, int count) {
    int i;
    aqlD_printf("   Registers: ");
    for (i = 0; i < count && i < 8; i++) {  /* Limit to first 8 registers */
        char buffer[32];
        aqlD_format_value(&registers[i], buffer, sizeof(buffer));
        aqlD_printf("R[%d]=%s ", i, buffer);
    }
    if (count > 8) {
        aqlD_printf("... (%d more)", count - 8);
    }
    aqlD_printf("\n\n");
}

void aqlD_print_execution_footer(TValue *result) {
    char buffer[64];
    aqlD_format_value(result, buffer, sizeof(buffer));
    aqlD_printf("✅ Execution complete! Final result: %s\n\n", buffer);
}

/*
** Register state debug output
*/
void aqlD_print_register_header(void) {
    aqlD_print_header("REGISTER STATE");
}

void aqlD_print_register_state(TValue *registers, int count, int *changed_regs, int changed_count) {
    int i;
    
    aqlD_printf("📋 Register State (%d registers):\n", count);
    aqlD_printf("  REG   TYPE        VALUE                    STATUS\n");
    aqlD_print_separator();
    
    for (i = 0; i < count; i++) {
        char value_buffer[64];
        const char *type_name;
        const char *status = "";
        
        /* Determine if this register changed */
        int is_changed = 0;
        for (int j = 0; j < changed_count; j++) {
            if (changed_regs[j] == i) {
                is_changed = 1;
                break;
            }
        }
        
        /* Get type name */
        switch (rawtt(&registers[i])) {
            case AQL_TNIL: type_name = "nil"; break;
            case AQL_TBOOLEAN: type_name = "boolean"; break;
            case AQL_TNUMBER: 
                if (ttisinteger(&registers[i])) {
                    type_name = "integer";
                } else {
                    type_name = "number";
                }
                break;
            case AQL_TSTRING: type_name = "string"; break;
            case AQL_TARRAY: type_name = "array"; break;
            case AQL_TSLICE: type_name = "slice"; break;
            case AQL_TDICT: type_name = "dict"; break;
            case AQL_TVECTOR: type_name = "vector"; break;
            default: type_name = "unknown"; break;
        }
        
        /* Format value */
        aqlD_format_value(&registers[i], value_buffer, sizeof(value_buffer));
        
        /* Set status */
        if (is_changed) {
            status = "🔄 CHANGED";
        }
        
        aqlD_printf("  R[%2d] %-10s %-24s %s\n", i, type_name, value_buffer, status);
    }
    
    aqlD_printf("\n");
}

void aqlD_print_register_changes(TValue *old_regs, TValue *new_regs, int count) {
    int changes_found = 0;
    int i;
    
    for (i = 0; i < count; i++) {
        /* Compare old and new values */
        int changed = 0;
        
        if (rawtt(&old_regs[i]) != rawtt(&new_regs[i])) {
            changed = 1;
        } else {
            switch (rawtt(&new_regs[i])) {
                case AQL_TNIL:
                    /* nil values are always equal */
                    break;
                case AQL_TBOOLEAN:
                    if (bvalue(&old_regs[i]) != bvalue(&new_regs[i])) {
                        changed = 1;
                    }
                    break;
                case AQL_TNUMBER:
                    if (ttisinteger(&new_regs[i])) {
                        if (!ttisinteger(&old_regs[i]) || ivalue(&old_regs[i]) != ivalue(&new_regs[i])) {
                            changed = 1;
                        }
                    } else {
                        if (ttisinteger(&old_regs[i]) || fltvalue(&old_regs[i]) != fltvalue(&new_regs[i])) {
                            changed = 1;
                        }
                    }
                    break;
                default:
                    /* For complex types, assume changed if pointers differ */
                    if (gcvalue(&old_regs[i]) != gcvalue(&new_regs[i])) {
                        changed = 1;
                    }
                    break;
            }
        }
        
        if (changed) {
            if (!changes_found) {
                aqlD_printf("🔄 Register Changes:\n");
                changes_found = 1;
            }
            
            char old_buffer[32], new_buffer[32];
            aqlD_format_value(&old_regs[i], old_buffer, sizeof(old_buffer));
            aqlD_format_value(&new_regs[i], new_buffer, sizeof(new_buffer));
            
            aqlD_printf("   R[%d]: %s → %s\n", i, old_buffer, new_buffer);
        }
    }
    
    if (changes_found) {
        aqlD_printf("\n");
    }
}

/*
** Memory and GC debug
*/
void aqlD_print_memory_stats(size_t allocated, size_t freed, size_t peak) {
    aqlD_printf("💾 Memory Statistics:\n");
    aqlD_printf("  Allocated: %zu bytes\n", allocated);
    aqlD_printf("  Freed: %zu bytes\n", freed);
    aqlD_printf("  Peak usage: %zu bytes\n", peak);
    aqlD_printf("  Current usage: %zu bytes\n\n", allocated - freed);
}

void aqlD_print_gc_stats(int collections, double gc_time) {
    aqlD_printf("🗑️  Garbage Collection Statistics:\n");
    aqlD_printf("  Collections: %d\n", collections);
    aqlD_printf("  Total GC time: %.3f ms\n", gc_time * 1000.0);
    if (collections > 0) {
        aqlD_printf("  Average GC time: %.3f ms\n", (gc_time * 1000.0) / collections);
    }
    aqlD_printf("\n");
}

/*
** REPL debug
*/
void aqlD_print_repl_input(const char *input) {
    aqlD_printf("📝 REPL Input: %s\n", input);
}

void aqlD_print_repl_result(TValue *result) {
    char buffer[64];
    aqlD_format_value(result, buffer, sizeof(buffer));
    aqlD_printf("📤 REPL Result: %s\n", buffer);
}

/*
** Performance profiling
*/
double aqlD_get_time(void) {
    return (double)clock() / CLOCKS_PER_SEC;
}

void aqlD_profile_start(const char *name) {
    /* Find or create profile entry */
    int i;
    for (i = 0; i < profile_count; i++) {
        if (strcmp(profile_entries[i].name, name) == 0) {
            profile_entries[i].start_time = aqlD_get_time();
            return;
        }
    }
    
    /* Create new entry */
    if (profile_count >= profile_capacity) {
        profile_capacity = profile_capacity ? profile_capacity * 2 : 16;
        profile_entries = realloc(profile_entries, 
                                profile_capacity * sizeof(AQL_ProfileEntry));
    }
    
    profile_entries[profile_count].name = name;
    profile_entries[profile_count].start_time = aqlD_get_time();
    profile_entries[profile_count].total_time = 0.0;
    profile_entries[profile_count].call_count = 0;
    profile_count++;
}

void aqlD_profile_end(const char *name) {
    double end_time = aqlD_get_time();
    int i;
    
    for (i = 0; i < profile_count; i++) {
        if (strcmp(profile_entries[i].name, name) == 0) {
            profile_entries[i].total_time += end_time - profile_entries[i].start_time;
            profile_entries[i].call_count++;
            return;
        }
    }
}

void aqlD_print_profile_report(void) {
    int i;
    if (profile_count == 0) {
        aqlD_printf("⏱️  No profiling data available\n\n");
        return;
    }
    
    aqlD_printf("⏱️  Performance Profile:\n");
    aqlD_printf("  %-20s %8s %8s %10s\n", "Function", "Calls", "Total(ms)", "Avg(ms)");
    aqlD_printf("  %-20s %8s %8s %10s\n", "--------", "-----", "--------", "-------");
    
    for (i = 0; i < profile_count; i++) {
        double avg_time = profile_entries[i].call_count > 0 ? 
                         profile_entries[i].total_time / profile_entries[i].call_count : 0.0;
        aqlD_printf("  %-20s %8d %8.3f %10.3f\n", 
                   profile_entries[i].name,
                   profile_entries[i].call_count,
                   profile_entries[i].total_time * 1000.0,
                   avg_time * 1000.0);
    }
    aqlD_printf("\n");
}

/*
** Utility functions
*/
const char *aqlD_token_name(int token_type) {
    switch (token_type) {
        case TK_INT: return "NUMBER";
        case TK_FLT: return "FLOAT";
        case TK_NAME: return "IDENTIFIER";
        case TK_STRING: return "STRING";
        case TK_PLUS: return "PLUS";
        case TK_MINUS: return "MINUS";
        case TK_MUL: return "MULTIPLY";
        case TK_DIV: return "DIVIDE";
        case TK_MOD: return "MODULO";
        case TK_POW: return "POWER";
        case TK_ASSIGN: return "ASSIGN";
        case TK_EQ: return "EQUAL";
        case TK_NE: return "NOT_EQUAL";
        case TK_LT: return "LESS_THAN";
        case TK_LE: return "LESS_EQUAL";
        case TK_GT: return "GREATER_THAN";
        case TK_GE: return "GREATER_EQUAL";
        case TK_AND: return "AND";
        case TK_OR: return "OR";
        case TK_NOT: return "NOT";
        case TK_IF: return "IF";
        case TK_ELSE: return "ELSE";
        case TK_ELIF: return "ELIF";
        case TK_RETURN: return "RETURN";
        case TK_LET: return "LET";
        case TK_EOS: return "EOF";
        case ';': return "SEMICOLON";
        case '(': return "LPAREN";
        case ')': return "RPAREN";
        case '{': return "LBRACE";
        case '}': return "RBRACE";
        case '[': return "LBRACKET";
        case ']': return "RBRACKET";
        case ',': return "COMMA";
        default: return "UNKNOWN";
    }
}

const char *aqlD_opcode_name(int opcode) {
    switch (opcode) {
        case OP_MOVE: return "MOVE";
        case OP_LOADI: return "LOADI";
        case OP_LOADF: return "LOADF";
        case OP_LOADK: return "LOADK";
        case OP_LOADKX: return "LOADKX";
        case OP_LOADFALSE: return "LOADFALSE";
        case OP_LOADTRUE: return "LOADTRUE";
        case OP_LOADNIL: return "LOADNIL";
        case OP_GETUPVAL: return "GETUPVAL";
        case OP_SETUPVAL: return "SETUPVAL";
        case OP_GETTABUP: return "GETTABUP";
        case OP_SETTABUP: return "SETTABUP";
        case OP_CLOSE: return "CLOSE";
        case OP_TBC: return "TBC";
        case OP_CONCAT: return "CONCAT";
        case OP_EXTRAARG: return "EXTRAARG";
        case OP_ADD: return "ADD";
        case OP_ADDK: return "ADDK";
        case OP_ADDI: return "ADDI";
        case OP_SUB: return "SUB";
        case OP_SUBK: return "SUBK";
        case OP_SUBI: return "SUBI";
        case OP_MUL: return "MUL";
        case OP_MULK: return "MULK";
        case OP_MULI: return "MULI";
        case OP_DIV: return "DIV";
        case OP_DIVK: return "DIVK";
        case OP_DIVI: return "DIVI";
        case OP_MOD: return "MOD";
        case OP_POW: return "POW";
        case OP_UNM: return "UNM";
        case OP_LEN: return "LEN";
        case OP_BAND: return "BAND";
        case OP_BOR: return "BOR";
        case OP_BXOR: return "BXOR";
        case OP_SHL: return "SHL";
        case OP_SHR: return "SHR";
        case OP_BNOT: return "BNOT";
        case OP_NOT: return "NOT";
        case OP_EQ: return "EQ";
        case OP_LT: return "LT";
        case OP_LE: return "LE";
        case OP_TEST: return "TEST";
        case OP_TESTSET: return "TESTSET";
        case OP_JMP: return "JMP";
        case OP_CALL: return "CALL";
        case OP_TAILCALL: return "TAILCALL";
        case OP_RET: return "RET";
        case OP_RET_VOID: return "RET_VOID";
        case OP_RET_ONE: return "RET_ONE";
        case OP_FORLOOP: return "FORLOOP";
        case OP_FORPREP: return "FORPREP";
        case OP_CLOSURE: return "CLOSURE";
        case OP_VARARG: return "VARARG";
        case OP_INVOKE: return "INVOKE";
        case OP_YIELD: return "YIELD";
        default: return "UNKNOWN";
    }
}

const char *aqlD_value_type_name(TValue *value) {
    if (!value) return "nil";
    
    switch (ttype(value)) {
        case AQL_TNIL: return "nil";
        case AQL_TBOOLEAN: return "boolean";
        case AQL_TNUMBER: return "number";
        case AQL_TSTRING: return "string";
        case AQL_TTABLE: return "table";
        case AQL_TFUNCTION: return "function";
        case AQL_TUSERDATA: return "userdata";
        case AQL_TTHREAD: return "thread";
        default: return "unknown";
    }
}

void aqlD_format_value(TValue *value, char *buffer, size_t size) {
    if (!value) {
        snprintf(buffer, size, "nil");
        return;
    }
    
    switch (ttype(value)) {
        case AQL_TNIL:
            snprintf(buffer, size, "nil");
            break;
        case AQL_TBOOLEAN:
            snprintf(buffer, size, "%s", bvalue(value) ? "true" : "false");
            break;
        case AQL_TNUMBER:
            if (ttisinteger(value)) {
                snprintf(buffer, size, "%lld", ivalue(value));
            } else {
                snprintf(buffer, size, "%.2f", fltvalue(value));
            }
            break;
        case AQL_TSTRING:
            snprintf(buffer, size, "\"%s\"", svalue(value));
            break;
        default:
            snprintf(buffer, size, "<%s>", aqlD_value_type_name(value));
            break;
    }
}

/*
** Integration helpers
*/
void aqlD_dump_vm_state(aql_State *L) {
    aqlD_printf("🔍 VM State Dump:\n");
    aqlD_printf("  Stack top: %p\n", (void*)L->top);
    aqlD_printf("  Stack base: %p\n", (void*)L->stack);
    aqlD_printf("  Call info: %p\n", (void*)L->ci);
    aqlD_printf("\n");
}

void aqlD_dump_stack(aql_State *L) {
    StkId p;
    int i = 0;
    aqlD_printf("📚 Stack Dump:\n");
    for (p = L->stack; p < L->top; p++, i++) {
        char buffer[64];
        aqlD_format_value(s2v(p), buffer, sizeof(buffer));
        aqlD_printf("  [%d] %s\n", i, buffer);
    }
    aqlD_printf("\n");
}

void aqlD_dump_globals(aql_State *L) {
    aqlD_printf("🌍 Globals Dump:\n");
    aqlD_printf("  (Global variable dumping not implemented yet)\n");
    aqlD_printf("\n");
}
