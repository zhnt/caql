/*
** $Id: aoptimizer.c $
** Advanced Code Optimization Passes for AQL JIT
** Implements constant folding, dead code elimination, and peephole optimization
** See Copyright Notice in aql.h
*/

#include "acodegen.h"
#include "amem.h"
#include "adebug.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/*
** Optimization statistics
*/
typedef struct {
    int constants_folded;
    int dead_instructions_eliminated;
    int redundant_moves_eliminated;
    int branches_optimized;
    int arithmetic_simplified;
} OptimizationStats;

/*
** Check if a value is a compile-time constant
*/
static bool is_constant_value(CodegenContext *ctx, int reg) {
    if (reg < 0 || reg >= ctx->num_virtual_regs) return false;
    return ctx->virtual_regs[reg].is_constant;
}

/*
** Get constant value for a register
*/
static TValue get_constant_value(CodegenContext *ctx, int reg) {
    if (reg < 0 || reg >= ctx->num_virtual_regs) {
        TValue nil_val;
        setnilvalue(&nil_val);
        return nil_val;
    }
    return ctx->virtual_regs[reg].constant_val;
}

/*
** Set constant value for a register
*/
static void set_constant_value(CodegenContext *ctx, int reg, const TValue *val) {
    if (reg >= 0 && reg < ctx->num_virtual_regs) {
        ctx->virtual_regs[reg].is_constant = true;
        ctx->virtual_regs[reg].constant_val = *val;
    }
}

/*
** Constant Folding Optimization Pass
*/
AQL_API void aqlCodegen_optimize_constant_folding(CodegenContext *ctx) {
    if (!ctx || !ctx->opt_config.enable_constant_folding) return;
    
    OptimizationStats stats = {0};
    
    AQL_DEBUG(2, "Starting constant folding optimization");
    
    for (int pc = 0; pc < ctx->bytecode_count; pc++) {
        Instruction *inst = &ctx->bytecode[pc];
        OpCode op = GET_OPCODE(*inst);
        int a = GETARG_A(*inst);
        int b = GETARG_B(*inst);
        int c = GETARG_C(*inst);
        
        switch (op) {
            case OP_LOADI: {
                /* Load immediate integer */
                int value = GETARG_sBx(*inst);
                TValue const_val;
                setivalue(&const_val, value);
                set_constant_value(ctx, a, &const_val);
                stats.constants_folded++;
                break;
            }
            
            case OP_LOADK: {
                /* Load constant from constant table */
                if (b < ctx->proto->sizek) {
                    set_constant_value(ctx, a, &ctx->proto->k[b]);
                    stats.constants_folded++;
                }
                break;
            }
            
            case OP_ADD: {
                /* Constant folding for addition */
                if (is_constant_value(ctx, b) && is_constant_value(ctx, c)) {
                    TValue val_b = get_constant_value(ctx, b);
                    TValue val_c = get_constant_value(ctx, c);
                    
                    if (ttisinteger(&val_b) && ttisinteger(&val_c)) {
                        /* Integer addition */
                        aql_Integer result = ivalue(&val_b) + ivalue(&val_c);
                        TValue result_val;
                        setivalue(&result_val, result);
                        set_constant_value(ctx, a, &result_val);
                        
                        /* Replace with LOADI instruction */
                        *inst = CREATE_ABx(OP_LOADI, a, result);
                        stats.constants_folded++;
                        
                        AQL_DEBUG(3, "Folded ADD: %lld + %lld = %lld", 
                                 (long long)ivalue(&val_b), 
                                 (long long)ivalue(&val_c), 
                                 (long long)result);
                    } else if (ttisnumber(&val_b) && ttisnumber(&val_c)) {
                        /* Float addition */
                        aql_Number result = fltvalue(&val_b) + fltvalue(&val_c);
                        TValue result_val;
                        setfltvalue(&result_val, result);
                        set_constant_value(ctx, a, &result_val);
                        stats.constants_folded++;
                    }
                }
                break;
            }
            
            case OP_SUB: {
                /* Constant folding for subtraction */
                if (is_constant_value(ctx, b) && is_constant_value(ctx, c)) {
                    TValue val_b = get_constant_value(ctx, b);
                    TValue val_c = get_constant_value(ctx, c);
                    
                    if (ttisinteger(&val_b) && ttisinteger(&val_c)) {
                        aql_Integer result = ivalue(&val_b) - ivalue(&val_c);
                        TValue result_val;
                        setivalue(&result_val, result);
                        set_constant_value(ctx, a, &result_val);
                        
                        *inst = CREATE_ABx(OP_LOADI, a, result);
                        stats.constants_folded++;
                    }
                }
                break;
            }
            
            case OP_MUL: {
                /* Constant folding for multiplication */
                if (is_constant_value(ctx, b) && is_constant_value(ctx, c)) {
                    TValue val_b = get_constant_value(ctx, b);
                    TValue val_c = get_constant_value(ctx, c);
                    
                    if (ttisinteger(&val_b) && ttisinteger(&val_c)) {
                        aql_Integer result = ivalue(&val_b) * ivalue(&val_c);
                        TValue result_val;
                        setivalue(&result_val, result);
                        set_constant_value(ctx, a, &result_val);
                        
                        *inst = CREATE_ABx(OP_LOADI, a, result);
                        stats.constants_folded++;
                    }
                }
                
                /* Special cases: multiply by 0 or 1 */
                if (is_constant_value(ctx, c)) {
                    TValue val_c = get_constant_value(ctx, c);
                    if (ttisinteger(&val_c)) {
                        aql_Integer c_val = ivalue(&val_c);
                        if (c_val == 0) {
                            /* x * 0 = 0 */
                            *inst = CREATE_ABx(OP_LOADI, a, 0);
                            stats.arithmetic_simplified++;
                        } else if (c_val == 1) {
                            /* x * 1 = x */
                            *inst = CREATE_ABC(OP_MOVE, a, b, 0);
                            stats.arithmetic_simplified++;
                        }
                    }
                }
                break;
            }
            
            case OP_DIV: {
                /* Constant folding for division */
                if (is_constant_value(ctx, b) && is_constant_value(ctx, c)) {
                    TValue val_b = get_constant_value(ctx, b);
                    TValue val_c = get_constant_value(ctx, c);
                    
                    if (ttisnumber(&val_b) && ttisnumber(&val_c)) {
                        aql_Number c_val = fltvalue(&val_c);
                        if (c_val != 0.0) {  /* Avoid division by zero */
                            aql_Number result = fltvalue(&val_b) / c_val;
                            TValue result_val;
                            setfltvalue(&result_val, result);
                            set_constant_value(ctx, a, &result_val);
                            stats.constants_folded++;
                        }
                    }
                }
                
                /* Special case: divide by 1 */
                if (is_constant_value(ctx, c)) {
                    TValue val_c = get_constant_value(ctx, c);
                    if (ttisnumber(&val_c) && fltvalue(&val_c) == 1.0) {
                        /* x / 1 = x */
                        *inst = CREATE_ABC(OP_MOVE, a, b, 0);
                        stats.arithmetic_simplified++;
                    }
                }
                break;
            }
            
            default:
                /* Clear constant flag for non-constant-producing instructions */
                if (testAMode(op)) {
                    ctx->virtual_regs[a].is_constant = false;
                }
                break;
        }
    }
    
    ctx->stats.optimizations_applied += stats.constants_folded + stats.arithmetic_simplified;
    
    AQL_DEBUG(2, "Constant folding complete: %d constants folded, %d arithmetic simplified", 
              stats.constants_folded, stats.arithmetic_simplified);
}

/*
** Dead Code Elimination Pass
*/
AQL_API void aqlCodegen_optimize_dead_code_elimination(CodegenContext *ctx) {
    if (!ctx || !ctx->opt_config.enable_dead_code_elimination) return;
    
    OptimizationStats stats = {0};
    bool *is_used = (bool *)aqlM_malloc((aql_State*)ctx, ctx->num_virtual_regs * sizeof(bool));
    if (!is_used) return;
    
    AQL_DEBUG(2, "Starting dead code elimination");
    
    /* Initialize usage tracking */
    memset(is_used, false, ctx->num_virtual_regs * sizeof(bool));
    
    /* Mark all used registers (backward pass) */
    for (int pc = ctx->bytecode_count - 1; pc >= 0; pc--) {
        Instruction inst = ctx->bytecode[pc];
        OpCode op = GET_OPCODE(inst);
        int a = GETARG_A(inst);
        int b = GETARG_B(inst);
        int c = GETARG_C(inst);
        
        /* Mark registers used by this instruction */
        if (!ISK(b) && b < ctx->num_virtual_regs) {
            is_used[b] = true;
        }
        if (!ISK(c) && c < ctx->num_virtual_regs) {
            is_used[c] = true;
        }
        
        /* Check if instruction result is used */
        if (testAMode(op) && a < ctx->num_virtual_regs && !is_used[a]) {
            /* Result is not used - mark instruction as dead */
            if (op != OP_CALL && op != OP_TAILCALL && op != OP_RET) {
                /* Don't eliminate instructions with side effects */
                ctx->bytecode[pc] = CREATE_ABC(OP_MOVE, 0, 0, 0);  /* Replace with NOP */
                stats.dead_instructions_eliminated++;
                
                AQL_DEBUG(3, "Eliminated dead instruction at PC %d: %s", 
                         pc, aql_opnames[op]);
            }
        }
        
        /* Mark register defined by this instruction as potentially unused */
        if (testAMode(op) && a < ctx->num_virtual_regs) {
            is_used[a] = false;  /* Will be set to true if used later */
        }
    }
    
    ctx->stats.optimizations_applied += stats.dead_instructions_eliminated;
    
    aqlM_free((aql_State*)ctx, is_used, ctx->num_virtual_regs * sizeof(bool));
    
    AQL_DEBUG(2, "Dead code elimination complete: %d instructions eliminated", 
              stats.dead_instructions_eliminated);
}

/*
** Peephole Optimization Pass
*/
AQL_API void aqlCodegen_optimize_peephole(CodegenContext *ctx) {
    if (!ctx || !ctx->opt_config.enable_peephole_optimization) return;
    
    OptimizationStats stats = {0};
    
    AQL_DEBUG(2, "Starting peephole optimization");
    
    for (int pc = 0; pc < ctx->bytecode_count - 1; pc++) {
        Instruction *inst1 = &ctx->bytecode[pc];
        Instruction *inst2 = &ctx->bytecode[pc + 1];
        
        OpCode op1 = GET_OPCODE(*inst1);
        OpCode op2 = GET_OPCODE(*inst2);
        
        /* Pattern 1: MOVE followed by MOVE of same register */
        if (op1 == OP_MOVE && op2 == OP_MOVE) {
            int a1 = GETARG_A(*inst1), b1 = GETARG_B(*inst1);
            int a2 = GETARG_A(*inst2), b2 = GETARG_B(*inst2);
            
            if (a1 == b2) {
                /* MOVE R(a1), R(b1); MOVE R(a2), R(a1) => MOVE R(a2), R(b1) */
                *inst2 = CREATE_ABC(OP_MOVE, a2, b1, 0);
                *inst1 = CREATE_ABC(OP_MOVE, 0, 0, 0);  /* NOP */
                stats.redundant_moves_eliminated++;
                
                AQL_DEBUG(3, "Eliminated redundant MOVE sequence at PC %d-%d", pc, pc+1);
            }
        }
        
        /* Pattern 2: Load constant followed by arithmetic with constant */
        if (op1 == OP_LOADI && (op2 == OP_ADD || op2 == OP_SUB || op2 == OP_MUL)) {
            int a1 = GETARG_A(*inst1);
            int b2 = GETARG_B(*inst2);
            
            if (a1 == b2) {
                /* Can potentially use immediate arithmetic instructions */
                int imm_val = GETARG_sBx(*inst1);
                if (imm_val >= -128 && imm_val <= 127) {  /* Fits in 8-bit immediate */
                    OpCode new_op = OP_ADDI;  /* Default to ADDI */
                    if (op2 == OP_SUB) new_op = OP_SUBI;
                    else if (op2 == OP_MUL) new_op = OP_MULI;
                    
                    int a2 = GETARG_A(*inst2);
                    int c2 = GETARG_C(*inst2);
                    
                    *inst2 = CREATE_ABC(new_op, a2, c2, imm_val & 0xFF);
                    *inst1 = CREATE_ABC(OP_MOVE, 0, 0, 0);  /* NOP */
                    stats.arithmetic_simplified++;
                    
                    AQL_DEBUG(3, "Combined constant load with arithmetic at PC %d-%d", pc, pc+1);
                }
            }
        }
        
        /* Pattern 3: Redundant branches */
        if (op1 == OP_JMP && op2 == OP_JMP) {
            /* Two consecutive unconditional jumps - second is unreachable */
            *inst2 = CREATE_ABC(OP_MOVE, 0, 0, 0);  /* NOP */
            stats.branches_optimized++;
            
            AQL_DEBUG(3, "Eliminated unreachable jump at PC %d", pc+1);
        }
        
        /* Pattern 4: Branch followed by its target (branch elimination) */
        if ((op1 == OP_EQ || op1 == OP_LT || op1 == OP_LE) && op2 == OP_JMP) {
            int jump_offset = GETARG_sBx(*inst2);
            if (jump_offset == 1) {  /* Jump to next instruction */
                /* Conditional branch that jumps to next instruction - eliminate both */
                *inst1 = CREATE_ABC(OP_MOVE, 0, 0, 0);  /* NOP */
                *inst2 = CREATE_ABC(OP_MOVE, 0, 0, 0);  /* NOP */
                stats.branches_optimized++;
                
                AQL_DEBUG(3, "Eliminated redundant branch sequence at PC %d-%d", pc, pc+1);
            }
        }
    }
    
    ctx->stats.optimizations_applied += stats.redundant_moves_eliminated + 
                                       stats.arithmetic_simplified + 
                                       stats.branches_optimized;
    
    AQL_DEBUG(2, "Peephole optimization complete: %d moves, %d arithmetic, %d branches optimized", 
              stats.redundant_moves_eliminated, stats.arithmetic_simplified, stats.branches_optimized);
}

/*
** Register Coalescing Optimization
*/
AQL_API void aqlCodegen_optimize_register_coalescing(CodegenContext *ctx) {
    if (!ctx || !ctx->opt_config.enable_register_coalescing) return;
    
    OptimizationStats stats = {0};
    
    AQL_DEBUG(2, "Starting register coalescing optimization");
    
    /* Look for MOVE instructions that can be eliminated by register coalescing */
    for (int pc = 0; pc < ctx->bytecode_count; pc++) {
        Instruction *inst = &ctx->bytecode[pc];
        OpCode op = GET_OPCODE(*inst);
        
        if (op == OP_MOVE) {
            int dst = GETARG_A(*inst);
            int src = GETARG_B(*inst);
            
            /* Check if we can coalesce these registers */
            if (dst < ctx->num_virtual_regs && src < ctx->num_virtual_regs) {
                VirtualRegister *dst_reg = &ctx->virtual_regs[dst];
                VirtualRegister *src_reg = &ctx->virtual_regs[src];
                
                /* Simple coalescing: if both are not spilled and have non-overlapping live ranges */
                if (dst_reg->physical_reg != -1 && src_reg->physical_reg != -1) {
                    if (dst_reg->def_point > src_reg->last_use) {
                        /* Non-overlapping - can coalesce */
                        dst_reg->physical_reg = src_reg->physical_reg;
                        *inst = CREATE_ABC(OP_MOVE, 0, 0, 0);  /* NOP */
                        stats.redundant_moves_eliminated++;
                        
                        AQL_DEBUG(3, "Coalesced registers %d and %d at PC %d", dst, src, pc);
                    }
                }
            }
        }
    }
    
    ctx->stats.optimizations_applied += stats.redundant_moves_eliminated;
    
    AQL_DEBUG(2, "Register coalescing complete: %d moves eliminated", 
              stats.redundant_moves_eliminated);
}

/*
** Run all optimization passes
*/
AQL_API void aqlCodegen_optimize_all(CodegenContext *ctx) {
    if (!ctx) return;
    
    AQL_DEBUG(1, "Running optimization passes (level %d)", ctx->opt_config.optimization_level);
    
    double start_time = (double)clock() / CLOCKS_PER_SEC;
    int initial_optimizations = ctx->stats.optimizations_applied;
    
    /* Run optimization passes in order */
    if (ctx->opt_config.optimization_level >= 1) {
        aqlCodegen_optimize_constant_folding(ctx);
        aqlCodegen_optimize_peephole(ctx);
    }
    
    if (ctx->opt_config.optimization_level >= 2) {
        aqlCodegen_optimize_dead_code_elimination(ctx);
        aqlCodegen_optimize_register_coalescing(ctx);
    }
    
    if (ctx->opt_config.optimization_level >= 3) {
        /* Additional aggressive optimizations */
        aqlCodegen_optimize_constant_folding(ctx);  /* Run again after DCE */
        aqlCodegen_optimize_peephole(ctx);          /* Run again after coalescing */
    }
    
    double end_time = (double)clock() / CLOCKS_PER_SEC;
    int total_optimizations = ctx->stats.optimizations_applied - initial_optimizations;
    
    AQL_DEBUG(1, "Optimization complete: %d optimizations applied in %.3fms", 
              total_optimizations, (end_time - start_time) * 1000.0);
}