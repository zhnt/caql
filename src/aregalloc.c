/*
** $Id: aregalloc.c $
** Advanced Register Allocation for AQL JIT
** Implements Linear Scan and Graph Coloring algorithms
** See Copyright Notice in aql.h
*/

#include "acodegen.h"
#include "amem.h"
#include "adebug_internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
** Live Interval for Linear Scan Register Allocation
*/
typedef struct LiveInterval {
    int virtual_reg;        /* Virtual register ID */
    int start;              /* First definition/use */
    int end;                /* Last use */
    int physical_reg;       /* Assigned physical register (-1 if spilled) */
    int spill_slot;         /* Spill slot if spilled */
    bool is_spilled;        /* True if spilled to memory */
    struct LiveInterval *next; /* Next interval in sorted list */
} LiveInterval;

/*
** Register allocation context
*/
typedef struct RegAllocContext {
    CodegenContext *codegen_ctx;
    LiveInterval *intervals;     /* List of live intervals */
    int *active_regs;           /* Currently active registers */
    int num_active;             /* Number of active registers */
    int next_spill_slot;        /* Next available spill slot */
    
    /* Architecture-specific register sets */
    bool *general_regs_free;    /* Free general purpose registers */
    bool *float_regs_free;      /* Free floating point registers */
    int num_general_regs;       /* Total general registers */
    int num_float_regs;         /* Total float registers */
    
    /* Statistics */
    int spills_generated;       /* Number of spills */
    int moves_eliminated;       /* Coalesced moves */
    
} RegAllocContext;

/*
** Initialize register allocator
*/
static RegAllocContext *regalloc_init(CodegenContext *ctx) {
    RegAllocContext *ra_ctx = (RegAllocContext *)aqlM_malloc(
        (aql_State*)ctx, sizeof(RegAllocContext));
    if (!ra_ctx) return NULL;
    
    memset(ra_ctx, 0, sizeof(RegAllocContext));
    ra_ctx->codegen_ctx = ctx;
    
    /* Initialize architecture-specific register counts */
    ra_ctx->num_general_regs = aqlCodegen_get_register_count(ctx->arch, REG_TYPE_GENERAL);
    ra_ctx->num_float_regs = aqlCodegen_get_register_count(ctx->arch, REG_TYPE_FLOAT);
    
    /* Allocate register availability arrays */
    ra_ctx->general_regs_free = (bool *)aqlM_malloc(
        (aql_State*)ctx, ra_ctx->num_general_regs * sizeof(bool));
    ra_ctx->float_regs_free = (bool *)aqlM_malloc(
        (aql_State*)ctx, ra_ctx->num_float_regs * sizeof(bool));
    
    if (!ra_ctx->general_regs_free || !ra_ctx->float_regs_free) {
        return NULL;
    }
    
    /* Mark all registers as initially free */
    memset(ra_ctx->general_regs_free, true, ra_ctx->num_general_regs * sizeof(bool));
    memset(ra_ctx->float_regs_free, true, ra_ctx->num_float_regs * sizeof(bool));
    
    /* Reserve special registers (stack pointer, frame pointer, etc.) */
    if (ctx->arch == ARCH_X86_64) {
        ra_ctx->general_regs_free[4] = false;   /* Stack pointer (RSP) */
        ra_ctx->general_regs_free[5] = false;   /* Frame pointer (RBP) */
    } else if (ctx->arch == ARCH_ARM64) {
        ra_ctx->general_regs_free[31] = false;  /* Stack pointer (SP) */
        ra_ctx->general_regs_free[30] = false;  /* Link register (LR) */
        ra_ctx->general_regs_free[29] = false;  /* Frame pointer (FP) */
    }
    
    return ra_ctx;
}

/*
** Cleanup register allocator
*/
static void regalloc_cleanup(RegAllocContext *ra_ctx) {
    if (!ra_ctx) return;
    
    /* Free live intervals */
    LiveInterval *interval = ra_ctx->intervals;
    while (interval) {
        LiveInterval *next = interval->next;
        aqlM_free((aql_State*)ra_ctx->codegen_ctx, interval, sizeof(LiveInterval));
        interval = next;
    }
    
    /* Free register arrays */
    if (ra_ctx->general_regs_free) {
        aqlM_free((aql_State*)ra_ctx->codegen_ctx, ra_ctx->general_regs_free, ra_ctx->num_general_regs * sizeof(bool));
    }
    if (ra_ctx->float_regs_free) {
        aqlM_free((aql_State*)ra_ctx->codegen_ctx, ra_ctx->float_regs_free, ra_ctx->num_float_regs * sizeof(bool));
    }
    
    aqlM_free((aql_State*)ra_ctx->codegen_ctx, ra_ctx, sizeof(RegAllocContext));
}

/*
** Compute live intervals for all virtual registers
*/
static void compute_live_intervals(RegAllocContext *ra_ctx) {
    CodegenContext *ctx = ra_ctx->codegen_ctx;
    
    /* Initialize virtual register usage tracking */
    int *first_def = (int *)aqlM_malloc((aql_State*)ctx, 
        ctx->num_virtual_regs * sizeof(int));
    int *last_use = (int *)aqlM_malloc((aql_State*)ctx, 
        ctx->num_virtual_regs * sizeof(int));
    
    if (!first_def || !last_use) return;
    
    /* Initialize to invalid positions */
    for (int i = 0; i < ctx->num_virtual_regs; i++) {
        first_def[i] = -1;
        last_use[i] = -1;
    }
    
    /* Scan bytecode to find definitions and uses */
    for (int pc = 0; pc < ctx->bytecode_count; pc++) {
        Instruction inst = ctx->bytecode[pc];
        OpCode op = GET_OPCODE(inst);
        int a = GETARG_A(inst);
        int b = GETARG_B(inst);
        int c = GETARG_C(inst);
        
        /* Record definition of register A (most instructions define A) */
        if (testAMode(op) && a < ctx->num_virtual_regs) {
            if (first_def[a] == -1) {
                first_def[a] = pc;
            }
            last_use[a] = pc;
        }
        
        /* Record uses of registers B and C */
        if (b < ctx->num_virtual_regs && !ISK(b)) {
            last_use[b] = pc;
            if (first_def[b] == -1) {
                first_def[b] = pc;  /* Use before def - parameter or upvalue */
            }
        }
        
        if (c < ctx->num_virtual_regs && !ISK(c)) {
            last_use[c] = pc;
            if (first_def[c] == -1) {
                first_def[c] = pc;  /* Use before def - parameter or upvalue */
            }
        }
    }
    
    /* Create live intervals for each virtual register */
    for (int i = 0; i < ctx->num_virtual_regs; i++) {
        if (first_def[i] != -1 && last_use[i] != -1) {
            LiveInterval *interval = (LiveInterval *)aqlM_malloc(
                (aql_State*)ctx, sizeof(LiveInterval));
            if (interval) {
                interval->virtual_reg = i;
                interval->start = first_def[i];
                interval->end = last_use[i];
                interval->physical_reg = -1;
                interval->spill_slot = -1;
                interval->is_spilled = false;
                interval->next = ra_ctx->intervals;
                ra_ctx->intervals = interval;
            }
        }
    }
    
    /* Cleanup */
    aqlM_free((aql_State*)ctx, first_def, ctx->num_virtual_regs * sizeof(int));
    aqlM_free((aql_State*)ctx, last_use, ctx->num_virtual_regs * sizeof(int));
}

/*
** Sort live intervals by start position (for linear scan)
*/
static void sort_intervals_by_start(RegAllocContext *ra_ctx) {
    /* Simple insertion sort - good enough for small functions */
    LiveInterval *sorted = NULL;
    LiveInterval *current = ra_ctx->intervals;
    
    while (current) {
        LiveInterval *next = current->next;
        
        /* Insert current into sorted list */
        if (!sorted || current->start < sorted->start) {
            current->next = sorted;
            sorted = current;
        } else {
            LiveInterval *pos = sorted;
            while (pos->next && pos->next->start <= current->start) {
                pos = pos->next;
            }
            current->next = pos->next;
            pos->next = current;
        }
        
        current = next;
    }
    
    ra_ctx->intervals = sorted;
}

/*
** Find a free register of the specified type
*/
static int find_free_register(RegAllocContext *ra_ctx, RegisterType type) {
    bool *regs_free;
    int num_regs;
    
    switch (type) {
        case REG_TYPE_GENERAL:
            regs_free = ra_ctx->general_regs_free;
            num_regs = ra_ctx->num_general_regs;
            break;
        case REG_TYPE_FLOAT:
            regs_free = ra_ctx->float_regs_free;
            num_regs = ra_ctx->num_float_regs;
            break;
        default:
            return -1;
    }
    
    for (int i = 0; i < num_regs; i++) {
        if (regs_free[i]) {
            regs_free[i] = false;  /* Mark as allocated */
            return i;
        }
    }
    
    return -1;  /* No free register */
}

/*
** Free a register
*/
static void free_register(RegAllocContext *ra_ctx, int reg_id, RegisterType type) {
    bool *regs_free;
    int num_regs;
    
    switch (type) {
        case REG_TYPE_GENERAL:
            regs_free = ra_ctx->general_regs_free;
            num_regs = ra_ctx->num_general_regs;
            break;
        case REG_TYPE_FLOAT:
            regs_free = ra_ctx->float_regs_free;
            num_regs = ra_ctx->num_float_regs;
            break;
        default:
            return;
    }
    
    if (reg_id >= 0 && reg_id < num_regs) {
        regs_free[reg_id] = true;
    }
}

/*
** Spill a virtual register to memory
*/
static void spill_register(RegAllocContext *ra_ctx, LiveInterval *interval) {
    interval->is_spilled = true;
    interval->spill_slot = ra_ctx->next_spill_slot++;
    interval->physical_reg = -1;
    
    /* Update codegen context */
    if (interval->spill_slot >= ra_ctx->codegen_ctx->spill_slots_used) {
        ra_ctx->codegen_ctx->spill_slots_used = interval->spill_slot + 1;
    }
    
    ra_ctx->spills_generated++;
    
    AQL_DEBUG(3, "Spilled virtual register %d to slot %d", 
              interval->virtual_reg, interval->spill_slot);
}

/*
** Linear Scan Register Allocation Algorithm
*/
static void linear_scan_allocation(RegAllocContext *ra_ctx) {
    LiveInterval **active = (LiveInterval **)aqlM_malloc(
        (aql_State*)ra_ctx->codegen_ctx, 
        ra_ctx->num_general_regs * sizeof(LiveInterval*));
    if (!active) return;
    
    int num_active = 0;
    
    /* Process intervals in order of start position */
    for (LiveInterval *interval = ra_ctx->intervals; interval; interval = interval->next) {
        
        /* Expire old intervals */
        for (int i = 0; i < num_active; i++) {
            if (active[i] && active[i]->end < interval->start) {
                /* Free the register */
                free_register(ra_ctx, active[i]->physical_reg, REG_TYPE_GENERAL);
                active[i] = NULL;
                
                /* Compact active array */
                for (int j = i; j < num_active - 1; j++) {
                    active[j] = active[j + 1];
                }
                num_active--;
                i--;  /* Recheck this position */
            }
        }
        
        /* Try to allocate a register */
        int reg = find_free_register(ra_ctx, REG_TYPE_GENERAL);
        if (reg != -1) {
            /* Successful allocation */
            interval->physical_reg = reg;
            active[num_active++] = interval;
            
            AQL_DEBUG(3, "Allocated virtual register %d to physical register %d", 
                      interval->virtual_reg, reg);
        } else {
            /* No free register - spill */
            spill_register(ra_ctx, interval);
        }
    }
    
    aqlM_free((aql_State*)ra_ctx->codegen_ctx, active, ra_ctx->num_general_regs * sizeof(LiveInterval*));
}

/*
** Main register allocation entry point
*/
AQL_API int aqlCodegen_alloc_registers(CodegenContext *ctx) {
    if (!ctx) return -1;
    
    RegAllocContext *ra_ctx = regalloc_init(ctx);
    if (!ra_ctx) return -1;
    
    AQL_DEBUG(2, "Starting register allocation for %d virtual registers", 
              ctx->num_virtual_regs);
    
    /* Compute live intervals */
    compute_live_intervals(ra_ctx);
    
    /* Sort intervals by start position */
    sort_intervals_by_start(ra_ctx);
    
    /* Perform linear scan allocation */
    linear_scan_allocation(ra_ctx);
    
    /* Update virtual register mappings in codegen context */
    for (LiveInterval *interval = ra_ctx->intervals; interval; interval = interval->next) {
        int vr = interval->virtual_reg;
        if (vr < ctx->num_virtual_regs) {
            ctx->virtual_regs[vr].physical_reg = interval->physical_reg;
            ctx->virtual_regs[vr].spill_slot = interval->spill_slot;
        }
    }
    
    /* Update statistics */
    ctx->spill_slots_used = ra_ctx->next_spill_slot;
    
    AQL_DEBUG(2, "Register allocation complete: %d spills, %d moves eliminated", 
              ra_ctx->spills_generated, ra_ctx->moves_eliminated);
    
    int result = ra_ctx->spills_generated;
    regalloc_cleanup(ra_ctx);
    
    return result;  /* Return number of spills generated */
}

/*
** Get physical register for virtual register
*/
AQL_API int aqlCodegen_get_physical_reg(CodegenContext *ctx, int virtual_reg) {
    if (!ctx || virtual_reg < 0 || virtual_reg >= ctx->num_virtual_regs) {
        return -1;
    }
    
    return ctx->virtual_regs[virtual_reg].physical_reg;
}

/*
** Check if virtual register is spilled
*/
AQL_API bool aqlCodegen_is_spilled(CodegenContext *ctx, int virtual_reg) {
    if (!ctx || virtual_reg < 0 || virtual_reg >= ctx->num_virtual_regs) {
        return false;
    }
    
    return ctx->virtual_regs[virtual_reg].physical_reg == -1;
}

/*
** Get spill slot for virtual register
*/
AQL_API int aqlCodegen_get_spill_slot(CodegenContext *ctx, int virtual_reg) {
    if (!ctx || virtual_reg < 0 || virtual_reg >= ctx->num_virtual_regs) {
        return -1;
    }
    
    return ctx->virtual_regs[virtual_reg].spill_slot;
}