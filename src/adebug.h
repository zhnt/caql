#ifndef ADEBUG_H
#define ADEBUG_H

#include <stdio.h>
#include <stdarg.h>

/* 调试标志位定义 */
typedef enum {
    AQL_FLAG_VD   = 0x01,  /* 详细调试 (-vd) */
    AQL_FLAG_VT   = 0x02,  /* 执行跟踪 (-vt) */
    AQL_FLAG_VAST = 0x04,  /* AST输出 (-vast) */
    AQL_FLAG_VL   = 0x08,  /* 词法分析 (-vl) */
    AQL_FLAG_VB   = 0x10,  /* 字节码输出 (-vb) */
    AQL_FLAG_V    = 0x1F   /* 全部详细信息 (-v) */
} AQL_NewDebugFlags;

/* 格式化输出类型 */
typedef enum {
    AQL_FORMAT_TOKENS,    /* 词法分析输出 */
    AQL_FORMAT_AST,       /* AST树形输出 */
    AQL_FORMAT_BYTECODE,  /* 字节码表格输出 */
    AQL_FORMAT_TRACE      /* 执行跟踪输出 */
} AQL_FormatType;

/* 运行时控制函数声明 */
void aql_debug_enable_flag(AQL_NewDebugFlags flag);
void aql_debug_enable_verbose_all(void);
void aql_debug_disable_all(void);
int aql_debug_is_enabled(AQL_NewDebugFlags flag);
void aql_debug_set_flags(int flags);

/* 格式化输出函数声明 */
void aql_format_begin_impl(AQL_FormatType type, const char *title);
void aql_format_end_impl(AQL_FormatType type);
void aql_format_tree_node_impl(int level, const char *format, ...);
void aql_format_table_row_impl(const char *col1, const char *col2, const char *col3, const char *col4, const char *col5, const char *col6);
void aql_format_item_impl(const char *format, ...);

/* 基础输出函数声明 */
void aql_output_error(const char *format, ...);
void aql_output_info(const char *format, ...);
void aql_output_debug(const char *format, ...);
void aql_output_if_enabled(AQL_NewDebugFlags flag, const char *format, ...);
void aql_output_ast_if_enabled(int level, const char *format, ...);

/* 编译时宏系统 */
#ifdef AQL_PRODUCTION_BUILD
    /* 生产版本：完全无调试代码 */
    #define aql_error(...)          aql_output_error(__VA_ARGS__)
    #define aql_info(...)           ((void)0)
    #define aql_info_vd(...)        ((void)0)
    #define aql_info_vt(...)        ((void)0)
    #define aql_info_vast(...)      ((void)0)
    #define aql_info_vl(...)        ((void)0)
    #define aql_info_vb(...)        ((void)0)
    #define aql_debug(...)          ((void)0)
    
    /* 通用指令跟踪宏 */
    #define AQL_INFO_VT_BEFORE()    ((void)0)
    #define AQL_INFO_VT_AFTER()     ((void)0)
    
    /* 格式化输出函数在生产版本中也是空操作 */
    #define aql_format_begin(...)   ((void)0)
    #define aql_format_end(...)     ((void)0)
    #define aql_format_tree_node(...) ((void)0)
    #define aql_format_table_row(...) ((void)0)
    #define aql_format_item(...)    ((void)0)

#elif defined(AQL_DEBUG_BUILD)
    /* 调试版本：根据运行时标志控制 */
    #define aql_error(...)          aql_output_error(__VA_ARGS__)
    #define aql_info(...)           aql_output_info(__VA_ARGS__)
    #define aql_info_vd(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VD)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vt(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vast(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VAST)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vl(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VL)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vb(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VB)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_debug(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VD)) { \
                aql_output_debug(__VA_ARGS__); \
            } \
        } while(0)
    
    /* 通用指令跟踪宏 */
    #define AQL_INFO_VT_BEFORE() \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                /* 通用BEFORE逻辑将在avm_core.c中实现 */ \
            } \
        } while(0)
    #define AQL_INFO_VT_AFTER() \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                /* 通用AFTER逻辑将在avm_core.c中实现 */ \
            } \
        } while(0)
    
    /* 格式化输出 - 有条件编译 */
    #define aql_format_begin(type, title) aql_format_begin_impl(type, title)
    #define aql_format_end(type)    aql_format_end_impl(type)
    #define aql_format_table_row(...) aql_format_table_row_impl(__VA_ARGS__)
    #define aql_format_item(...)    aql_format_item_impl(__VA_ARGS__)
    #define aql_format_tree_node(level, ...) aql_format_tree_node_impl(level, __VA_ARGS__)

#else
    /* 默认构建 - 与DEBUG_BUILD相同 */
    #define aql_error(...)          aql_output_error(__VA_ARGS__)
    #define aql_info(...)           aql_output_info(__VA_ARGS__)
    #define aql_info_vd(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VD)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vt(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vast(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VAST)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vl(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VL)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vb(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VB)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_debug(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VD)) { \
                aql_output_debug(__VA_ARGS__); \
            } \
        } while(0)
    
    /* 通用指令跟踪宏 */
    #define AQL_INFO_VT_BEFORE() \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                /* 通用BEFORE逻辑将在avm_core.c中实现 */ \
            } \
        } while(0)
    #define AQL_INFO_VT_AFTER() \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                /* 通用AFTER逻辑将在avm_core.c中实现 */ \
            } \
        } while(0)
    
    /* 格式化输出 */
    #define aql_format_begin(type, title) aql_format_begin_impl(type, title)
    #define aql_format_end(type)    aql_format_end_impl(type)
    #define aql_format_table_row(...) aql_format_table_row_impl(__VA_ARGS__)
    #define aql_format_item(...)    aql_format_item_impl(__VA_ARGS__)
    #define aql_format_tree_node(level, ...) aql_format_tree_node_impl(level, __VA_ARGS__)
#endif

/* 便利宏 - 格式化输出的开始/结束 */
#define aql_info_vl_begin()     aql_format_begin(AQL_FORMAT_TOKENS, "词法分析结果")
#define aql_info_vl_end()       aql_format_end(AQL_FORMAT_TOKENS)
#define aql_info_vast_begin()   aql_format_begin(AQL_FORMAT_AST, "抽象语法树")
#define aql_info_vast_end()     aql_format_end(AQL_FORMAT_AST)
#define aql_info_vb_begin()     aql_format_begin(AQL_FORMAT_BYTECODE, "字节码")
#define aql_info_vb_end()       aql_format_end(AQL_FORMAT_BYTECODE)

/* VT调试宏 - 指令执行跟踪 */
#if defined(AQL_DEBUG_BUILD) || defined(AQL_VM_BUILD)
    /* 调试版本：完整的VT跟踪功能 */
    #define AQL_VT_BEFORE() \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                OpCode op = GET_OPCODE(i); \
                int pc_num = (int)(pc - cl->p->code - 1); \
                printf("PC=%d [%s] %s BEFORE: ", pc_num, func_name, aql_opnames[op]); \
                /* 显示指令参数 */ \
                switch (op) { \
                    case OP_LOADI: { \
                        int a = GETARG_A(i), sbx = GETARG_sBx(i); \
                        printf("A=[R%d=?], sBx=%d", a, sbx); \
                        break; \
                    } \
                    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: { \
                        int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_C(i); \
                        printf("A=[R%d=?], B=[R%d], C=[R%d]", a, b, c); \
                        break; \
                    } \
                    case OP_ADDI: { \
                        int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_sC(i); \
                        printf("A=[R%d=?], B=[R%d], sC=%d", a, b, c); \
                        break; \
                    } \
                    case OP_LTI: { \
                        int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_C(i); \
                        printf("A=[R%d=?], B=[R%d], C=%d", a, b, c); \
                        break; \
                    } \
                    case OP_JMP: { \
                        int sj = GETARG_sJ(i); \
                        printf("sJ=%d", sj); \
                        break; \
                    } \
                    case OP_RETURN1: { \
                        int a = GETARG_A(i); \
                        printf("A=[R%d]", a); \
                        break; \
                    } \
                    default: \
                        printf("args..."); \
                        break; \
                } \
                printf("\n"); \
            } \
        } while(0)

    #define AQL_VT_AFTER() \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                OpCode op = GET_OPCODE(i); \
                int pc_num = (int)(pc - cl->p->code - 1); \
                printf("PC=%d [%s] %s AFTER:  ", pc_num, func_name, aql_opnames[op]); \
                /* 显示指令执行后的结果 */ \
                switch (op) { \
                    case OP_LOADI: { \
                        int a = GETARG_A(i), sbx = GETARG_sBx(i); \
                        TValue *val = DEBUG_GET_REG_VALUE(L, cl, base, a); \
                        printf("A=[R%d", a); \
                        if (val) { debug_print_value(val); } else { printf("=?"); } \
                        printf("], sBx=%d", sbx); \
                        break; \
                    } \
                    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: { \
                        int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_C(i); \
                        TValue *val_a = DEBUG_GET_REG_VALUE(L, cl, base, a); \
                        TValue *val_b = DEBUG_GET_REG_VALUE(L, cl, base, b); \
                        TValue *val_c = DEBUG_GET_REG_VALUE(L, cl, base, c); \
                        printf("A=[R%d", a); \
                        if (val_a) { debug_print_value(val_a); } else { printf("=?"); } \
                        printf("], B=[R%d", b); \
                        if (val_b) { debug_print_value(val_b); } else { printf("=?"); } \
                        printf("], C=[R%d", c); \
                        if (val_c) { debug_print_value(val_c); } else { printf("=?"); } \
                        printf("]"); \
                        break; \
                    } \
                    case OP_ADDI: { \
                        int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_sC(i); \
                        TValue *val_a = DEBUG_GET_REG_VALUE(L, cl, base, a); \
                        TValue *val_b = DEBUG_GET_REG_VALUE(L, cl, base, b); \
                        printf("A=[R%d", a); \
                        if (val_a) { debug_print_value(val_a); } else { printf("=?"); } \
                        printf("], B=[R%d", b); \
                        if (val_b) { debug_print_value(val_b); } else { printf("=?"); } \
                        printf("], sC=%d", c); \
                        break; \
                    } \
                    case OP_LTI: { \
                        int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_C(i); \
                        TValue *val_a = DEBUG_GET_REG_VALUE(L, cl, base, a); \
                        TValue *val_b = DEBUG_GET_REG_VALUE(L, cl, base, b); \
                        printf("A=[R%d", a); \
                        if (val_a) { debug_print_value(val_a); } else { printf("=?"); } \
                        printf("], B=[R%d", b); \
                        if (val_b) { debug_print_value(val_b); } else { printf("=?"); } \
                        printf("], C=%d", c); \
                        break; \
                    } \
                    case OP_JMP: { \
                        int sj = GETARG_sJ(i); \
                        printf("sJ=%d", sj); \
                        break; \
                    } \
                    case OP_RETURN1: { \
                        int a = GETARG_A(i); \
                        TValue *val = DEBUG_GET_REG_VALUE(L, cl, base, a); \
                        printf("A=[R%d", a); \
                        if (val) { debug_print_value(val); } else { printf("=?"); } \
                        printf("]"); \
                        break; \
                    } \
                    default: \
                        printf("result..."); \
                        break; \
                } \
                printf("\n"); \
            } \
        } while(0)

    /* ===== 指令特定的调试输出系统 ===== */
    
    /* 1. 通用的值显示函数 - 使用宏避免类型依赖问题 */
    #define debug_print_value(val) \
        do { \
            if (!(val)) { printf("=?"); break; } \
                if (ttisinteger(val)) printf("=%lld", (long long)ivalue(val)); \
                else if (ttisnumber(val)) printf("=%.2f", fltvalue(val)); \
                else if (ttisstring(val)) printf("=\"%s\"", getstr(tsvalue(val))); \
            else if (ttisLclosure(val)) printf("=func@%p", (void*)clvalue(val)); \
                else if (ttisnil(val)) printf("=nil"); \
                else printf("=type%d", ttype(val)); \
        } while(0)
    
    /* 2. 参数获取宏 */
    #define DEBUG_GET_REG_VALUE(L, cl, base, reg) \
        ((reg) < (cl)->p->maxstacksize && (base) + (reg) >= (L)->stack.p && (base) + (reg) < (L)->top.p ? \
         s2v((base) + (reg)) : NULL)
    
    #define DEBUG_GET_CONST_VALUE(cl, idx) \
        ((idx) < (cl)->p->sizek ? &(cl)->p->k[idx] : NULL)
    
    #define DEBUG_GET_UPVAL_VALUE(cl, idx) \
        ((idx) < (cl)->nupvalues && (cl)->upvals[idx] ? \
         (cl)->upvals[idx]->v.p : NULL)
    
    /* 3. 指令分组宏 - 使用现有的调试标志机制 */
    #define DEBUG_UPVAL_AB(L, ci, i, pc, cl, base, func_name, opname) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                int a = GETARG_A(i), b = GETARG_B(i); \
                printf("PC=%d [%s] %s R%d := UpValue[%d]", \
                       (int)(pc - cl->p->code), func_name, opname, a, b); \
                TValue *val = DEBUG_GET_UPVAL_VALUE(cl, b); \
                if (val) { printf(" ("); debug_print_value(val); printf(")"); } \
                printf("\n"); \
            } \
        } while(0)
    
    #define DEBUG_SETUPVAL_AB(L, ci, i, pc, cl, base, func_name) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                int a = GETARG_A(i), b = GETARG_B(i); \
                printf("PC=%d [%s] SETUPVAL UpValue[%d] := R%d", \
                       (int)(pc - cl->p->code), func_name, b, a); \
                TValue *val = DEBUG_GET_REG_VALUE(L, cl, base, a); \
                if (val) { printf(" ("); debug_print_value(val); printf(")"); } \
                printf("\n"); \
            } \
        } while(0)
    
    #define DEBUG_ARITH_ABC(L, ci, i, pc, cl, base, func_name, opname) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_C(i); \
                printf("PC=%d [%s] %s R%d := R%d %s R%d", \
                       (int)(pc - cl->p->code), func_name, opname, a, b, \
                       get_arith_op_symbol(GET_OPCODE(i)), c); \
                TValue *val_b = DEBUG_GET_REG_VALUE(L, cl, base, b); \
                TValue *val_c = DEBUG_GET_REG_VALUE(L, cl, base, c); \
                if (val_b) { printf(" ("); debug_print_value(val_b); printf(")"); } \
                if (val_c) { printf(" ("); debug_print_value(val_c); printf(")"); } \
                printf("\n"); \
            } \
        } while(0)
    
    #define DEBUG_TABUP_ABC(L, ci, i, pc, cl, base, func_name, opname) \
        do { \
            int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_C(i); \
            printf("PC=%d [%s] %s R%d := UpValue[%d][K%d]", \
                   (int)(pc - cl->p->code), func_name, opname, a, b, c); \
            TValue *upval = DEBUG_GET_UPVAL_VALUE(cl, b); \
            TValue *key = DEBUG_GET_CONST_VALUE(cl, c); \
            if (upval && ttistable(upval)) printf(" (table@%p)", (void*)hvalue(upval)); \
            if (key && ttisstring(key)) printf(" (key=\"%s\")", getstr(tsvalue(key))); \
            printf("\n"); \
        } while(0)
    
    #define DEBUG_SETTABUP_ABC(L, ci, i, pc, cl, base, func_name) \
        do { \
            int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_C(i); \
            printf("PC=%d [%s] SETTABUP UpValue[%d][K%d] := R%d", \
                   (int)(pc - cl->p->code), func_name, a, b, c); \
            TValue *key = DEBUG_GET_CONST_VALUE(cl, b); \
            TValue *value = DEBUG_GET_REG_VALUE(L, cl, base, c); \
            if (key && ttisstring(key)) printf(" (key=\"%s\")", getstr(tsvalue(key))); \
            if (value) { printf(" ("); debug_print_value(value); printf(")"); } \
            printf("\n"); \
        } while(0)
    
    /* 4. 具体指令宏 - 在调试构建块内提供完整实现 */
    #define DEBUG_GETUPVAL(L, ci, i, pc, cl, base, func_name) \
        DEBUG_UPVAL_AB(L, ci, i, pc, cl, base, func_name, "GETUPVAL")
    
    #define DEBUG_SETUPVAL(L, ci, i, pc, cl, base, func_name) \
        DEBUG_SETUPVAL_AB(L, ci, i, pc, cl, base, func_name)
    
    #define DEBUG_GETTABUP(L, ci, i, pc, cl, base, func_name) \
        DEBUG_TABUP_ABC(L, ci, i, pc, cl, base, func_name, "GETTABUP")
    
    #define DEBUG_SETTABUP(L, ci, i, pc, cl, base, func_name) \
        DEBUG_SETTABUP_ABC(L, ci, i, pc, cl, base, func_name)
    
    #define DEBUG_ADD(L, ci, i, pc, cl, base, func_name) \
        DEBUG_ARITH_ABC(L, ci, i, pc, cl, base, func_name, "ADD")
    
    #define DEBUG_SUB(L, ci, i, pc, cl, base, func_name) \
        DEBUG_ARITH_ABC(L, ci, i, pc, cl, base, func_name, "SUB")
    
    #define DEBUG_MUL(L, ci, i, pc, cl, base, func_name) \
        DEBUG_ARITH_ABC(L, ci, i, pc, cl, base, func_name, "MUL")
    
    #define DEBUG_DIV(L, ci, i, pc, cl, base, func_name) \
        DEBUG_ARITH_ABC(L, ci, i, pc, cl, base, func_name, "DIV")
    
    #define DEBUG_ADDI(L, ci, i, pc, cl, base, func_name) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_sC(i); \
                printf("PC=%d [%s] ADDI R%d := R%d + %d", \
                       (int)(pc - cl->p->code), func_name, a, b, c); \
                TValue *val = DEBUG_GET_REG_VALUE(L, cl, base, b); \
                if (val) { printf(" ("); debug_print_value(val); printf(")"); } \
                printf("\n"); \
            } \
        } while(0)
    
    #define DEBUG_LOADI(L, ci, i, pc, cl, base, func_name) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                int a = GETARG_A(i), sbx = GETARG_sBx(i); \
                printf("PC=%d [%s] LOADI R%d := %d", \
                       (int)(pc - cl->p->code), func_name, a, sbx); \
                TValue *val = DEBUG_GET_REG_VALUE(L, cl, base, a); \
                if (val) { printf(" ("); debug_print_value(val); printf(")"); } \
                printf("\n"); \
            } \
        } while(0)
    
    #define DEBUG_LT(L, ci, i, pc, cl, base, func_name) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_C(i); \
                printf("PC=%d [%s] LT R%d := R%d < R%d", \
                       (int)(pc - cl->p->code), func_name, a, b, c); \
                TValue *val_b = DEBUG_GET_REG_VALUE(L, cl, base, b); \
                TValue *val_c = DEBUG_GET_REG_VALUE(L, cl, base, c); \
                if (val_b) { printf(" ("); debug_print_value(val_b); printf(")"); } \
                if (val_c) { printf(" ("); debug_print_value(val_c); printf(")"); } \
                printf("\n"); \
            } \
        } while(0)
    
#define DEBUG_JMP(L, ci, i, pc, cl, base, func_name) \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            int sj = GETARG_sJ(i); \
            printf("PC=%d [%s] JMP PC+%d", \
                   (int)(pc - cl->p->code), func_name, sj); \
            printf("\n"); \
        } \
    } while(0)

#define DEBUG_MULK(L, ci, i, pc, cl, base, func_name) \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_C(i); \
            printf("PC=%d [%s] MULK R%d := R%d * K%d", \
                   (int)(pc - cl->p->code), func_name, a, b, c); \
            TValue *val_b = DEBUG_GET_REG_VALUE(L, cl, base, b); \
            TValue *val_c = DEBUG_GET_CONST_VALUE(cl, c); \
            if (val_b) { printf(" ("); debug_print_value(val_b); printf(")"); } \
            if (val_c) { printf(" ("); debug_print_value(val_c); printf(")"); } \
            printf("\n"); \
        } \
    } while(0)

#define DEBUG_FORPREP(L, ci, i, pc, cl, base, func_name) \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            int a = GETARG_A(i), bx = GETARG_Bx(i); \
            printf("PC=%d [%s] FORPREP R%d, %d", \
                   (int)(pc - cl->p->code), func_name, a, bx); \
            printf("\n"); \
        } \
    } while(0)

#define DEBUG_FORLOOP(L, ci, i, pc, cl, base, func_name) \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            int a = GETARG_A(i), bx = GETARG_Bx(i); \
            printf("PC=%d [%s] FORLOOP R%d, %d", \
                   (int)(pc - cl->p->code), func_name, a, bx); \
            printf("\n"); \
        } \
    } while(0)
    
    #define DEBUG_LTI(L, ci, i, pc, cl, base, func_name) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_C(i); \
                printf("PC=%d [%s] LTI R%d := %d", \
                       (int)(pc - cl->p->code), func_name, a, b); \
                TValue *val = DEBUG_GET_REG_VALUE(L, cl, base, a); \
                if (val) { printf(" ("); debug_print_value(val); printf(")"); } \
                printf("\n"); \
            } \
        } while(0)
    
    #define DEBUG_CLOSURE(L, ci, i, pc, cl, base, func_name) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                int a = GETARG_A(i), bx = GETARG_Bx(i); \
                printf("PC=%d [%s] CLOSURE R%d := closure(KPROTO[%d])", \
                       (int)(pc - cl->p->code), func_name, a, bx); \
                printf("\n"); \
            } \
        } while(0)
    
    #define DEBUG_CALL(L, ci, i, pc, cl, base, func_name) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_C(i); \
                printf("PC=%d [%s] CALL R%d := R%d(R%d...R%d)", \
                       (int)(pc - cl->p->code), func_name, a, a, a+1, a+b-1); \
                TValue *func = DEBUG_GET_REG_VALUE(L, cl, base, a); \
                if (func) { printf(" ("); debug_print_value(func); printf(")"); } \
                printf("\n"); \
            } \
        } while(0)

    #define DEBUG_RETURN(L, ci, i, pc, cl, base, func_name) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                int a = GETARG_A(i), b = GETARG_B(i), c = GETARG_C(i); \
                printf("PC=%d [%s] RETURN R%d...R%d (n=%d)", \
                       (int)(pc - cl->p->code), func_name, a, a+b-2, b-1); \
                if (TESTARG_k(i)) printf(" [k=1, close upvalues]"); \
                printf("\n"); \
            } \
        } while(0)
    
    /* 辅助宏：获取算术操作符符号 */
    #define get_arith_op_symbol(op) \
        ((op) == OP_ADD ? "+" : \
         (op) == OP_SUB ? "-" : \
         (op) == OP_MUL ? "*" : \
         (op) == OP_DIV ? "/" : \
         (op) == OP_MOD ? "%" : \
         (op) == OP_POW ? "^" : \
         (op) == OP_IDIV ? "//" : \
         (op) == OP_BAND ? "&" : \
         (op) == OP_BOR ? "|" : \
         (op) == OP_BXOR ? "~" : \
         (op) == OP_SHL ? "<<" : \
         (op) == OP_SHR ? ">>" : "?")
    
    /* 废弃的通用宏已移除 - 使用新的指令特定调试宏 */

    #define print_instruction_trace_asbx(L, ci, i, pc, cl, base, func_name, is_before) \
        do { \
            int a = GETARG_A(i); \
            int sbx = GETARG_sBx(i); \
            int pc_num = (int)(pc - cl->p->code); \
            OpCode op = GET_OPCODE(i); \
            printf("PC=%d [%s] <%s,op=%d> %s", pc_num, func_name, aql_opnames[op], op, \
                   is_before ? "BEFORE" : "AFTER"); \
            printf("  A=[R%d", a); \
            if (a < cl->p->maxstacksize && (base + a) >= L->stack.p) { \
                TValue *val = s2v(base + a); \
                if (ttisinteger(val)) printf("=%lld", (long long)ivalue(val)); \
                else if (ttisnumber(val)) printf("=%.2f", fltvalue(val)); \
                else if (ttisstring(val)) printf("=\"%s\"", getstr(tsvalue(val))); \
                else if (ttisLclosure(val)) printf("=func@%p", (void*)clvalue(val)); \
                else if (ttisnil(val)) printf("=nil"); \
                else printf("=type%d", ttype(val)); \
            } else { \
                printf("=?"); \
            } \
            printf("]  sBx=%d\n", sbx); \
        } while(0)

    #define print_instruction_trace_ax(L, ci, i, pc, cl, base, func_name, is_before) \
        do { \
            int a = GETARG_A(i); \
            int x = GETARG_Ax(i); \
            int pc_num = (int)(pc - cl->p->code); \
            OpCode op = GET_OPCODE(i); \
            printf("PC=%d [%s] <%s,op=%d> %s", pc_num, func_name, aql_opnames[op], op, \
                   is_before ? "BEFORE" : "AFTER"); \
            printf("  A=[R%d", a); \
            if (a < cl->p->maxstacksize && (base + a) >= L->stack.p) { \
                TValue *val = s2v(base + a); \
                if (ttisinteger(val)) printf("=%lld", (long long)ivalue(val)); \
                else if (ttisnumber(val)) printf("=%.2f", fltvalue(val)); \
                else if (ttisstring(val)) printf("=\"%s\"", getstr(tsvalue(val))); \
                else if (ttisLclosure(val)) printf("=func@%p", (void*)clvalue(val)); \
                else if (ttisnil(val)) printf("=nil"); \
                else printf("=type%d", ttype(val)); \
            } else { \
                printf("=?"); \
            } \
            printf("]  Ax=%d\n", x); \
        } while(0)


#endif

#endif /* ADEBUG_H */
