/*
** $Id: avm_debug.c $
** AQL VM Debug Functions
** VM特定的调试功能实现
** See Copyright Notice in aql.h
*/

#define avm_debug_c
#define AQL_CORE

#include <stdio.h>
#include <string.h>

#include "aql.h"
#include "aopcodes.h"
#include "avm.h"
#include "adebug.h"

/* ===================================================================
** VM特定调试函数实现
** =================================================================== */

#ifdef AQL_DEBUG_BUILD

/* 全局变量用于存储当前指令信息 */
static aql_State *current_L = NULL;
static CallInfo *current_ci = NULL;
static Instruction current_i = 0;
static const Instruction *current_pc = NULL;
static LClosure *current_cl = NULL;
static StkId current_base = NULL;
static const char *current_func_name = NULL;

/* 设置当前指令上下文 */
void aql_vt_set_context(aql_State *L, CallInfo *ci, Instruction i, 
                       const Instruction *pc, LClosure *cl, 
                       StkId base, const char *func_name) {
    current_L = L;
    current_ci = ci;
    current_i = i;
    current_pc = pc;
    current_cl = cl;
    current_base = base;
    current_func_name = func_name;
}

/* 获取函数名称 */
static const char *get_function_name(void) {
    if (current_func_name) {
        return current_func_name;
    }
    if (current_cl && current_cl->p && current_cl->p->source) {
        return getstr(current_cl->p->source);
    }
    return "main";
}

/* 智能值显示函数 */
static void aql_debug_print_value(TValue *val) {
    if (!val) {
        printf("=?");
        return;
    }
    
    switch (ttypetag(val)) {
        case AQL_VNIL:
            printf("=nil");
            break;
        case AQL_VFALSE:
            printf("=false");
            break;
        case AQL_VTRUE:
            printf("=true");
            break;
        case AQL_VNUMINT:
            printf("=%lld", (long long)ivalue(val));
            break;
        case AQL_VNUMFLT:
            printf("=%.2f", fltvalue(val));
            break;
        case AQL_VSHRSTR:
        case AQL_VLNGSTR: {
            const char *str = getstr(tsvalue(val));
            size_t len = tsslen(tsvalue(val));
            if (len <= 20) {
                printf("=\"%s\"", str);
            } else {
                printf("=\"%.17s...\"", str);
            }
            break;
        }
        case AQL_VLCL:
            printf("=func@%p", (void*)clvalue(val));
            break;
        default:
            printf("=type%d", ttype(val));
            break;
    }
}

/* 通用加载指令调试函数 (LOADI, LOADF, LOADK, LOADKX, LOADFALSE, LOADTRUE, LOADNIL) */
void aql_vt_load_before(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s BEFORE: A=[R%d=?]", 
           pc_num, get_function_name(), op_name, a);
    
    /* 根据指令类型显示额外参数 */
    if (strcmp(op_name, "LOADI") == 0 || strcmp(op_name, "LOADF") == 0) {
        int sbx = GETARG_sBx(current_i);
        printf(", sBx=%d", sbx);
    } else if (strcmp(op_name, "LOADK") == 0) {
        int bx = GETARG_Bx(current_i);
        printf(", Bx=%d", bx);
    } else if (strcmp(op_name, "LOADNIL") == 0) {
        int b = GETARG_B(current_i);
        printf(", B=%d", b);
    }
    printf("\n");
}

void aql_vt_load_after(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s AFTER:  A=[R%d", pc_num, get_function_name(), op_name, a);
    
    
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("]");
    
    /* 根据指令类型显示额外参数 */
    if (strcmp(op_name, "LOADI") == 0 || strcmp(op_name, "LOADF") == 0) {
        int sbx = GETARG_sBx(current_i);
        printf(", sBx=%d", sbx);
    } else if (strcmp(op_name, "LOADK") == 0) {
        int bx = GETARG_Bx(current_i);
        printf(", Bx=%d", bx);
    } else if (strcmp(op_name, "LOADNIL") == 0) {
        int b = GETARG_B(current_i);
        printf(", B=%d", b);
    }
    printf("\n");
}

/* LOADI指令调试函数 */
void aql_vt_loadi_before(void) {
    aql_vt_load_before("LOADI");
}

void aql_vt_loadi_after(void) {
    aql_vt_load_after("LOADI");
}

/* 通用算术指令调试函数 (ADD, SUB, MUL, DIV, MOD, POW, IDIV, BAND, BOR, BXOR, SHL, SHR) */
void aql_vt_arith_before(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int b = GETARG_B(current_i);
    int c = GETARG_C(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s BEFORE: A=[R%d=?], B=[R%d", pc_num, get_function_name(), op_name, a, b);
    if (current_base + b < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + b));
    } else {
        printf("=?");
    }
    printf("], C=[R%d", c);
    if (current_base + c < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + c));
    } else {
        printf("=?");
    }
    printf("]\n");
}

void aql_vt_arith_after(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int b = GETARG_B(current_i);
    int c = GETARG_C(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s AFTER:  A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("], B=[R%d", b);
    if (current_base + b < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + b));
    } else {
        printf("=?");
    }
    printf("], C=[R%d", c);
    if (current_base + c < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + c));
    } else {
        printf("=?");
    }
    printf("]\n");
}

/* ADD指令调试函数 */
void aql_vt_add_before(void) {
    aql_vt_arith_before("ADD");
}

void aql_vt_add_after(void) {
    aql_vt_arith_after("ADD");
}

/* MUL指令调试函数 */
void aql_vt_mul_before(void) {
    aql_vt_arith_before("MUL");
}

void aql_vt_mul_after(void) {
    aql_vt_arith_after("MUL");
}

/* SUB指令调试函数 */
void aql_vt_sub_before(void) {
    aql_vt_arith_before("SUB");
}

void aql_vt_sub_after(void) {
    aql_vt_arith_after("SUB");
}

/* DIV指令调试函数 */
void aql_vt_div_before(void) {
    aql_vt_arith_before("DIV");
}

void aql_vt_div_after(void) {
    aql_vt_arith_after("DIV");
}

/* 通用控制流指令调试函数 (CALL, TAILCALL, RETURN, RETURN0, RETURN1) */
void aql_vt_control_before(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s BEFORE: A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("]");
    
    /* 根据指令类型显示额外参数 */
    if (strcmp(op_name, "CALL") == 0 || strcmp(op_name, "TAILCALL") == 0 || strcmp(op_name, "RETURN") == 0) {
        int b = GETARG_B(current_i);
        int c = GETARG_C(current_i);
        printf(", B=[R%d", b);
        if (current_base + b < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + b));
        } else {
            printf("=?");
        }
        printf("], C=[R%d", c);
        if (current_base + c < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + c));
        } else {
            printf("=?");
        }
        printf("]");
    }
    printf("\n");
}

void aql_vt_control_after(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s AFTER:  A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("]");
    
    /* 根据指令类型显示额外参数 */
    if (strcmp(op_name, "CALL") == 0 || strcmp(op_name, "TAILCALL") == 0 || strcmp(op_name, "RETURN") == 0) {
        int b = GETARG_B(current_i);
        int c = GETARG_C(current_i);
        printf(", B=[R%d", b);
        if (current_base + b < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + b));
        } else {
            printf("=?");
        }
        printf("], C=[R%d", c);
        if (current_base + c < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + c));
        } else {
            printf("=?");
        }
        printf("]");
    }
    printf("\n");
}

/* CALL指令调试函数 */
void aql_vt_call_before(void) {
    aql_vt_control_before("CALL");
}

void aql_vt_call_after(void) {
    aql_vt_control_after("CALL");
}

/* 通用跳转指令调试函数 (JMP) */
void aql_vt_jump_before(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int sbx = GETARG_sBx(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s BEFORE: A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("], sBx=%d\n", sbx);
}

void aql_vt_jump_after(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int sbx = GETARG_sBx(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s AFTER:  A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("], sBx=%d\n", sbx);
}

/* JMP指令调试函数 */
void aql_vt_jmp_before(void) {
    aql_vt_jump_before("JMP");
}

void aql_vt_jmp_after(void) {
    aql_vt_jump_after("JMP");
}

/* 通用对象创建指令调试函数 (CLOSURE, NEWTABLE) */
void aql_vt_object_before(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s BEFORE: A=[R%d=?]", pc_num, get_function_name(), op_name, a);
    
    /* 根据指令类型显示额外参数 */
    if (strcmp(op_name, "CLOSURE") == 0) {
        int bx = GETARG_Bx(current_i);
        printf(", Bx=%d", bx);
    } else if (strcmp(op_name, "NEWTABLE") == 0) {
        int b = GETARG_B(current_i);
        int c = GETARG_C(current_i);
        printf(", B=%d, C=%d", b, c);
    }
    printf("\n");
}

void aql_vt_object_after(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s AFTER:  A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("]");
    
    /* 根据指令类型显示额外参数 */
    if (strcmp(op_name, "CLOSURE") == 0) {
        int bx = GETARG_Bx(current_i);
        printf(", Bx=%d", bx);
    } else if (strcmp(op_name, "NEWTABLE") == 0) {
        int b = GETARG_B(current_i);
        int c = GETARG_C(current_i);
        printf(", B=%d, C=%d", b, c);
    }
    printf("\n");
}

/* CLOSURE指令调试函数 */
void aql_vt_closure_before(void) {
    aql_vt_object_before("CLOSURE");
}

void aql_vt_closure_after(void) {
    aql_vt_object_after("CLOSURE");
}

/* RETURN指令调试函数 */
void aql_vt_return_before(void) {
    aql_vt_control_before("RETURN");
}

void aql_vt_return_after(void) {
    aql_vt_control_after("RETURN");
}

/* 通用比较指令调试函数 (EQ, LT, LE, EQK, EQI, LTI, LEI, GTI, GEI) */
void aql_vt_compare_before(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s BEFORE: A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("]");
    
    /* 根据指令类型显示额外参数 */
    if (strcmp(op_name, "EQ") == 0 || strcmp(op_name, "LT") == 0 || strcmp(op_name, "LE") == 0) {
        int b = GETARG_B(current_i);
        int k = GETARG_k(current_i);
        printf(", B=[R%d", b);
        if (current_base + b < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + b));
        } else {
            printf("=?");
        }
        printf("], k=%d", k);
    } else if (strcmp(op_name, "EQK") == 0) {
        int b = GETARG_B(current_i);
        int k = GETARG_k(current_i);
        printf(", Bx=%d, k=%d", b, k);
    } else if (strcmp(op_name, "EQI") == 0 || strcmp(op_name, "LTI") == 0 || 
               strcmp(op_name, "LEI") == 0 || strcmp(op_name, "GTI") == 0 || 
               strcmp(op_name, "GEI") == 0) {
        int sb = GETARG_sB(current_i);
        int k = GETARG_k(current_i);
        printf(", sB=%d, k=%d", sb, k);
    }
    printf("\n");
}

void aql_vt_compare_after(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s AFTER:  A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("]");
    
    /* 根据指令类型显示额外参数 */
    if (strcmp(op_name, "EQ") == 0 || strcmp(op_name, "LT") == 0 || strcmp(op_name, "LE") == 0) {
        int b = GETARG_B(current_i);
        int k = GETARG_k(current_i);
        printf(", B=[R%d", b);
        if (current_base + b < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + b));
        } else {
            printf("=?");
        }
        printf("], k=%d", k);
    } else if (strcmp(op_name, "EQK") == 0) {
        int b = GETARG_B(current_i);
        int k = GETARG_k(current_i);
        printf(", Bx=%d, k=%d", b, k);
    } else if (strcmp(op_name, "EQI") == 0 || strcmp(op_name, "LTI") == 0 || 
               strcmp(op_name, "LEI") == 0 || strcmp(op_name, "GTI") == 0 || 
               strcmp(op_name, "GEI") == 0) {
        int sb = GETARG_sB(current_i);
        int k = GETARG_k(current_i);
        printf(", sB=%d, k=%d", sb, k);
    }
    printf("\n");
}

/* 通用测试指令调试函数 (TEST, TESTSET) */
void aql_vt_test_before(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s BEFORE: A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("]");
    
    if (strcmp(op_name, "TEST") == 0) {
        int k = GETARG_k(current_i);
        printf(", k=%d", k);
    } else if (strcmp(op_name, "TESTSET") == 0) {
        int b = GETARG_B(current_i);
        int k = GETARG_k(current_i);
        printf(", B=[R%d", b);
        if (current_base + b < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + b));
        } else {
            printf("=?");
        }
        printf("], k=%d", k);
    }
    printf("\n");
}

void aql_vt_test_after(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s AFTER:  A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("]");
    
    if (strcmp(op_name, "TEST") == 0) {
        int k = GETARG_k(current_i);
        printf(", k=%d", k);
    } else if (strcmp(op_name, "TESTSET") == 0) {
        int b = GETARG_B(current_i);
        int k = GETARG_k(current_i);
        printf(", B=[R%d", b);
        if (current_base + b < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + b));
        } else {
            printf("=?");
        }
        printf("], k=%d", k);
    }
    printf("\n");
}

/* 通用for循环指令调试函数 (FORLOOP, FORPREP) */
void aql_vt_for_before(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int bx = GETARG_Bx(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s BEFORE: A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("], Bx=%d", bx);
    
    /* 显示循环相关的寄存器 */
    if (strcmp(op_name, "FORPREP") == 0) {
        /* FORPREP: 显示初始值、限制值、步长 */
        printf(" (init=");
        if (current_base + a < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + a));
        } else {
            printf("?");
        }
        printf(", limit=");
        if (current_base + a + 1 < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + a + 1));
        } else {
            printf("?");
        }
        printf(", step=");
        if (current_base + a + 2 < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + a + 2));
        } else {
            printf("?");
        }
        printf(")");
    } else if (strcmp(op_name, "FORLOOP") == 0) {
        /* FORLOOP: 显示循环控制变量 */
        printf(" (control=");
        if (current_base + a + 3 < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + a + 3));
        } else {
            printf("?");
        }
        printf(")");
    }
    printf("\n");
}

void aql_vt_for_after(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int bx = GETARG_Bx(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s AFTER:  A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("], Bx=%d", bx);
    
    /* 显示循环相关的寄存器 */
    if (strcmp(op_name, "FORPREP") == 0) {
        /* FORPREP: 显示初始值、限制值、步长 */
        printf(" (init=");
        if (current_base + a < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + a));
        } else {
            printf("?");
        }
        printf(", limit=");
        if (current_base + a + 1 < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + a + 1));
        } else {
            printf("?");
        }
        printf(", step=");
        if (current_base + a + 2 < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + a + 2));
        } else {
            printf("?");
        }
        printf(")");
    } else if (strcmp(op_name, "FORLOOP") == 0) {
        /* FORLOOP: 显示循环控制变量 */
        printf(" (control=");
        if (current_base + a + 3 < current_ci->top.p) {
            aql_debug_print_value(s2v(current_base + a + 3));
        } else {
            printf("?");
        }
        printf(")");
    }
    printf("\n");
}

/* 通用表操作指令调试函数 (GETTABLE, SETTABLE, GETI, SETI, GETFIELD, SETFIELD) */
void aql_vt_table_before(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int b = GETARG_B(current_i);
    int c = GETARG_C(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s BEFORE: A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("], B=[R%d", b);
    if (current_base + b < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + b));
    } else {
        printf("=?");
    }
    printf("], C=[R%d", c);
    if (current_base + c < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + c));
    } else {
        printf("=?");
    }
    printf("]\n");
}

void aql_vt_table_after(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int b = GETARG_B(current_i);
    int c = GETARG_C(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s AFTER:  A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("], B=[R%d", b);
    if (current_base + b < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + b));
    } else {
        printf("=?");
    }
    printf("], C=[R%d", c);
    if (current_base + c < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + c));
    } else {
        printf("=?");
    }
    printf("]\n");
}

/* 通用一元操作指令调试函数 (UNM, BNOT, NOT, LEN) */
void aql_vt_unary_before(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int b = GETARG_B(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s BEFORE: A=[R%d=?], B=[R%d", pc_num, get_function_name(), op_name, a, b);
    if (current_base + b < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + b));
    } else {
        printf("=?");
    }
    printf("]\n");
}

void aql_vt_unary_after(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int b = GETARG_B(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s AFTER:  A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("], B=[R%d", b);
    if (current_base + b < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + b));
    } else {
        printf("=?");
    }
    printf("]\n");
}

/* 通用常量算术指令调试函数 (ADDK, SUBK, MULK, DIVK, MODK, POWK, IDIVK) */
void aql_vt_arithk_before(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int b = GETARG_B(current_i);
    int c = GETARG_C(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s BEFORE: A=[R%d=?], B=[R%d", pc_num, get_function_name(), op_name, a, b);
    if (current_base + b < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + b));
    } else {
        printf("=?");
    }
    printf("], C=[K%d", c);
    if (current_cl && current_cl->p && current_cl->p->k && c < current_cl->p->sizek) {
        aql_debug_print_value(&current_cl->p->k[c]);
    } else {
        printf("=?");
    }
    printf("]\n");
}

void aql_vt_arithk_after(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int b = GETARG_B(current_i);
    int c = GETARG_C(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s AFTER:  A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("], B=[R%d", b);
    if (current_base + b < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + b));
    } else {
        printf("=?");
    }
    printf("], C=[K%d", c);
    if (current_cl && current_cl->p && current_cl->p->k && c < current_cl->p->sizek) {
        aql_debug_print_value(&current_cl->p->k[c]);
    } else {
        printf("=?");
    }
    printf("]\n");
}

/* 通用立即数算术指令调试函数 (ADDI, SUBI, MULI, DIVI) */
void aql_vt_arithi_before(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int b = GETARG_B(current_i);
    int sc = GETARG_sC(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s BEFORE: A=[R%d=?], B=[R%d", pc_num, get_function_name(), op_name, a, b);
    if (current_base + b < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + b));
    } else {
        printf("=?");
    }
    printf("], sC=%d\n", sc);
}

void aql_vt_arithi_after(const char *op_name) {
    if (!current_L || !aql_debug_is_enabled(AQL_FLAG_VT)) return;
    
    int a = GETARG_A(current_i);
    int b = GETARG_B(current_i);
    int sc = GETARG_sC(current_i);
    int pc_num = (int)(current_pc - current_cl->p->code - 1);
    
    printf("PC=%d [%s] %s AFTER:  A=[R%d", pc_num, get_function_name(), op_name, a);
    if (current_base + a < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + a));
    } else {
        printf("=?");
    }
    printf("], B=[R%d", b);
    if (current_base + b < current_ci->top.p) {
        aql_debug_print_value(s2v(current_base + b));
    } else {
        printf("=?");
    }
    printf("], sC=%d\n", sc);
}

#endif /* AQL_DEBUG_BUILD */
