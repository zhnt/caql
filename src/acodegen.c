/*
** $Id: acodegen.c $
** Advanced Machine Code Generator for AQL JIT
** Core implementation of multi-architecture code generation
** See Copyright Notice in aql.h
*/

#include "acodegen.h"
#include "amem.h"
#include "adebug_internal.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <stdarg.h>

/*
** Label structure for jump targets
*/
typedef struct {
    int bytecode_pc;     /* Bytecode PC */
    size_t code_offset;  /* Offset in generated code */
} CodegenLabel;

/*
** Forward declarations for architecture-specific functions
*/
static int emit_x86_64_instruction(CodegenContext *ctx, const InstructionTemplate *tmpl, 
                                  int dst_reg, int src1_reg, int src2_reg, int64_t immediate);
static int emit_arm64_instruction(CodegenContext *ctx, const InstructionTemplate *tmpl,
                                 int dst_reg, int src1_reg, int src2_reg, int64_t immediate);

/*
** External function declarations
*/
extern const InstructionTemplate *aqlCodegen_get_template(OpCode op);
extern int aqlCodegen_alloc_registers(CodegenContext *ctx);
extern void aqlCodegen_optimize_all(CodegenContext *ctx);

/*
** Create a new code generation context
*/
AQL_API CodegenContext *aqlCodegen_create_context(CodegenArch arch, Proto *proto) {
    if (!proto) return NULL;
    
    CodegenContext *ctx = (CodegenContext *)aqlM_malloc(
        (aql_State*)proto, sizeof(CodegenContext));
    if (!ctx) return NULL;
    
    memset(ctx, 0, sizeof(CodegenContext));
    
    /* Initialize basic context */
    ctx->arch = arch;
    ctx->proto = proto;
    ctx->bytecode = proto->code;
    ctx->bytecode_count = proto->sizecode;
    
    /* Estimate code size and allocate buffer */
    size_t estimated_size = aqlCodegen_estimate_code_size(proto);
    ctx->code_capacity = estimated_size * 2;  /* 2x safety margin */
    ctx->code_buffer = (unsigned char *)aqlM_malloc(
        (aql_State*)proto, ctx->code_capacity);
    if (!ctx->code_buffer) {
        aqlM_free((aql_State*)proto, ctx, sizeof(CodegenContext));
        return NULL;
    }
    
    /* Initialize virtual registers */
    ctx->num_virtual_regs = proto->maxstacksize + proto->sizeupvalues + 16;  /* Extra for temps */
    ctx->virtual_regs = (VirtualRegister *)aqlM_malloc(
        (aql_State*)proto, ctx->num_virtual_regs * sizeof(VirtualRegister));
    if (!ctx->virtual_regs) {
        aqlM_free((aql_State*)proto, ctx->code_buffer, ctx->code_capacity);
        aqlM_free((aql_State*)proto, ctx, sizeof(CodegenContext));
        return NULL;
    }
    
    /* Initialize virtual register array */
    for (int i = 0; i < ctx->num_virtual_regs; i++) {
        ctx->virtual_regs[i].id = i;
        ctx->virtual_regs[i].physical_reg = -1;
        ctx->virtual_regs[i].spill_slot = -1;
        ctx->virtual_regs[i].def_point = -1;
        ctx->virtual_regs[i].last_use = -1;
        ctx->virtual_regs[i].is_constant = false;
        setnilvalue(&ctx->virtual_regs[i].constant_val);
    }
    
    /* Initialize physical registers */
    ctx->num_physical_regs = aqlCodegen_get_register_count(arch, REG_TYPE_GENERAL);
    ctx->physical_regs = (PhysicalRegister *)aqlM_malloc(
        (aql_State*)proto, ctx->num_physical_regs * sizeof(PhysicalRegister));
    if (!ctx->physical_regs) {
        aqlM_free((aql_State*)proto, ctx->virtual_regs, ctx->num_virtual_regs * sizeof(VirtualRegister));
        aqlM_free((aql_State*)proto, ctx->code_buffer, ctx->code_capacity);
        aqlM_free((aql_State*)proto, ctx, sizeof(CodegenContext));
        return NULL;
    }
    
    /* Initialize physical register array */
    for (int i = 0; i < ctx->num_physical_regs; i++) {
        ctx->physical_regs[i].id = i;
        ctx->physical_regs[i].type = REG_TYPE_GENERAL;
        ctx->physical_regs[i].is_allocated = false;
        ctx->physical_regs[i].virtual_reg = -1;
        ctx->physical_regs[i].last_use = -1;
        ctx->physical_regs[i].is_dirty = false;
    }
    
    /* Initialize labels array */
    ctx->num_labels = ctx->bytecode_count;  /* One label per potential jump target */
    ctx->labels = aqlM_malloc(
        (aql_State*)proto, ctx->num_labels * sizeof(CodegenLabel));
    if (!ctx->labels) {
        aqlM_free((aql_State*)proto, ctx->physical_regs, ctx->num_physical_regs * sizeof(PhysicalRegister));
        aqlM_free((aql_State*)proto, ctx->virtual_regs, ctx->num_virtual_regs * sizeof(VirtualRegister));
        aqlM_free((aql_State*)proto, ctx->code_buffer, ctx->code_capacity);
        aqlM_free((aql_State*)proto, ctx, sizeof(CodegenContext));
        return NULL;
    }
    
    /* Initialize labels */
    CodegenLabel *labels = (CodegenLabel *)ctx->labels;
    for (int i = 0; i < ctx->num_labels; i++) {
        labels[i].bytecode_pc = i;
        labels[i].code_offset = 0;
    }
    
    /* Set default optimization configuration */
    ctx->opt_config.enable_constant_folding = true;
    ctx->opt_config.enable_dead_code_elimination = true;
    ctx->opt_config.enable_register_coalescing = true;
    ctx->opt_config.enable_peephole_optimization = true;
    ctx->opt_config.optimization_level = 2;  /* Moderate optimization */
    
    /* Initialize stack frame */
    ctx->stack_size = proto->maxstacksize * sizeof(TValue);
    ctx->max_stack_size = ctx->stack_size + 256;  /* Extra space for spills */
    
    AQL_DEBUG(2, "Created codegen context: arch=%s, %d virtual regs, %d physical regs", 
              aqlCodegen_arch_name(arch), ctx->num_virtual_regs, ctx->num_physical_regs);
    
    return ctx;
}

/*
** Destroy code generation context
*/
AQL_API void aqlCodegen_destroy_context(CodegenContext *ctx) {
    if (!ctx) return;
    
    if (ctx->labels) {
        aqlM_free((aql_State*)ctx->proto, ctx->labels, ctx->num_labels * sizeof(CodegenLabel));
    }
    if (ctx->physical_regs) {
        aqlM_free((aql_State*)ctx->proto, ctx->physical_regs, ctx->num_physical_regs * sizeof(PhysicalRegister));
    }
    if (ctx->virtual_regs) {
        aqlM_free((aql_State*)ctx->proto, ctx->virtual_regs, ctx->num_virtual_regs * sizeof(VirtualRegister));
    }
    if (ctx->code_buffer) {
        aqlM_free((aql_State*)ctx->proto, ctx->code_buffer, ctx->code_capacity);
    }
    
    aqlM_free((aql_State*)ctx->proto, ctx, sizeof(CodegenContext));
    
    AQL_DEBUG(2, "Destroyed codegen context");
}

/*
** Ensure code buffer has enough space
*/
static int ensure_code_space(CodegenContext *ctx, size_t needed) {
    if (ctx->code_size + needed > ctx->code_capacity) {
        size_t new_capacity = ctx->code_capacity * 2;
        if (new_capacity < ctx->code_size + needed) {
            new_capacity = ctx->code_size + needed + 1024;
        }
        
        unsigned char *new_buffer = (unsigned char *)aqlM_realloc(
            (aql_State*)ctx->proto, ctx->code_buffer, ctx->code_capacity, new_capacity);
        if (!new_buffer) return -1;
        
        ctx->code_buffer = new_buffer;
        ctx->code_capacity = new_capacity;
        
        AQL_DEBUG(3, "Expanded code buffer to %zu bytes", new_capacity);
    }
    
    return 0;
}

/*
** Emit raw bytes to code buffer
*/
static int emit_bytes(CodegenContext *ctx, const void *bytes, size_t count) {
    if (ensure_code_space(ctx, count) != 0) return -1;
    
    memcpy(ctx->code_buffer + ctx->code_size, bytes, count);
    ctx->code_size += count;
    
    return 0;
}

/*
** Emit a single byte
*/
static int emit_byte(CodegenContext *ctx, unsigned char byte) {
    return emit_bytes(ctx, &byte, 1);
}

/*
** Emit a 32-bit integer (little endian)
*/
static int emit_int32(CodegenContext *ctx, int32_t value) {
    unsigned char bytes[4];
    bytes[0] = value & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    bytes[2] = (value >> 16) & 0xFF;
    bytes[3] = (value >> 24) & 0xFF;
    return emit_bytes(ctx, bytes, 4);
}

/*
** Emit a 64-bit integer (little endian)
*/
static int emit_int64(CodegenContext *ctx, int64_t value) {
    unsigned char bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (value >> (i * 8)) & 0xFF;
    }
    return emit_bytes(ctx, bytes, 8);
}

/*
** x86-64 specific instruction emission
*/
static int emit_x86_64_instruction(CodegenContext *ctx, const InstructionTemplate *tmpl,
                                  int dst_reg, int src1_reg, int src2_reg, int64_t immediate) {
    if (!tmpl) return -1;
    
    /* Access x86-64 template data directly */
    const unsigned char *encoding = tmpl->x86_64.encoding;
    int length = tmpl->x86_64.length;
    bool has_modrm = tmpl->x86_64.has_modrm;
    bool has_immediate = tmpl->x86_64.has_immediate;
    bool has_displacement = tmpl->x86_64.has_displacement;
    
    /* Emit base instruction bytes */
    for (int i = 0; i < length; i++) {
        if (emit_byte(ctx, encoding[i]) != 0) return -1;
    }
    
    /* Handle ModR/M byte if present */
    if (has_modrm) {
        unsigned char modrm = 0xC0;  /* Register-to-register mode */
        if (dst_reg >= 0) modrm |= (dst_reg & 7) << 3;  /* REG field */
        if (src1_reg >= 0) modrm |= (src1_reg & 7);     /* R/M field */
        
        if (emit_byte(ctx, modrm) != 0) return -1;
    }
    
    /* Handle immediate values */
    if (has_immediate) {
        if (immediate >= -128 && immediate <= 127) {
            /* 8-bit immediate */
            if (emit_byte(ctx, (unsigned char)immediate) != 0) return -1;
        } else if (immediate >= -2147483648LL && immediate <= 2147483647LL) {
            /* 32-bit immediate */
            if (emit_int32(ctx, (int32_t)immediate) != 0) return -1;
        } else {
            /* 64-bit immediate */
            if (emit_int64(ctx, immediate) != 0) return -1;
        }
    }
    
    /* Handle displacement for jumps */
    if (has_displacement) {
        /* For now, emit placeholder - will be patched later */
        if (emit_int32(ctx, 0) != 0) return -1;
    }
    
    ctx->stats.instructions_generated++;
    return 0;
}

/*
** ARM64 specific instruction emission
*/
static int emit_arm64_instruction(CodegenContext *ctx, const InstructionTemplate *tmpl,
                                 int dst_reg, int src1_reg, int src2_reg, int64_t immediate) {
    if (!tmpl) return -1;
    
    /* Access ARM64 template data directly */
    uint32_t instruction = tmpl->arm64.encoding;
    bool has_immediate = tmpl->arm64.has_immediate;
    int immediate_bits = tmpl->arm64.immediate_bits;
    
    /* Encode register fields */
    if (dst_reg >= 0) instruction |= (dst_reg & 0x1F);           /* Rd field (bits 0-4) */
    if (src1_reg >= 0) instruction |= (src1_reg & 0x1F) << 5;    /* Rn field (bits 5-9) */
    if (src2_reg >= 0) instruction |= (src2_reg & 0x1F) << 16;   /* Rm field (bits 16-20) */
    
    /* Handle immediate values */
    if (has_immediate) {
        int64_t max_imm = (1LL << immediate_bits) - 1;
        
        if (immediate >= 0 && immediate <= max_imm) {
            instruction |= (immediate & max_imm) << 10;  /* Immediate field */
        } else {
            AQL_DEBUG(1, "ARM64 immediate value %lld out of range for %d bits", 
                     (long long)immediate, immediate_bits);
            return -1;
        }
    }
    
    /* Emit 32-bit instruction (little endian) */
    if (emit_int32(ctx, (int32_t)instruction) != 0) return -1;
    
    ctx->stats.instructions_generated++;
    return 0;
}

/*
** Emit an instruction using template
*/
AQL_API void aqlCodegen_emit_instruction(CodegenContext *ctx, const InstructionTemplate *tmpl, ...) {
    if (!ctx || !tmpl) return;
    
    va_list args;
    va_start(args, tmpl);
    
    int dst_reg = va_arg(args, int);
    int src1_reg = va_arg(args, int);
    int src2_reg = va_arg(args, int);
    int64_t immediate = va_arg(args, int64_t);
    
    va_end(args);
    
    /* Convert virtual registers to physical registers */
    if (dst_reg >= 0) dst_reg = aqlCodegen_get_physical_reg(ctx, dst_reg);
    if (src1_reg >= 0) src1_reg = aqlCodegen_get_physical_reg(ctx, src1_reg);
    if (src2_reg >= 0) src2_reg = aqlCodegen_get_physical_reg(ctx, src2_reg);
    
    /* Emit architecture-specific instruction */
    int result = -1;
    switch (ctx->arch) {
        case ARCH_X86_64:
            result = emit_x86_64_instruction(ctx, tmpl, dst_reg, src1_reg, src2_reg, immediate);
            break;
        case ARCH_ARM64:
            result = emit_arm64_instruction(ctx, tmpl, dst_reg, src1_reg, src2_reg, immediate);
            break;
        default:
            AQL_DEBUG(1, "Unsupported architecture: %d", ctx->arch);
            break;
    }
    
    if (result != 0) {
        AQL_DEBUG(1, "Failed to emit instruction for opcode %d", tmpl->aql_opcode);
    }
}

/*
** Generate prologue code
*/
static int generate_prologue(CodegenContext *ctx) {
    AQL_DEBUG(3, "Generating function prologue");
    
    if (ctx->arch == ARCH_X86_64) {
        /* Standard x86-64 function prologue */
        /* push %rbp */
        if (emit_byte(ctx, 0x55) != 0) return -1;
        
        /* mov %rsp, %rbp */
        if (emit_bytes(ctx, "\x48\x89\xe5", 3) != 0) return -1;
        
        /* sub $stack_size, %rsp */
        if (ctx->max_stack_size > 0) {
            if (emit_bytes(ctx, "\x48\x81\xec", 3) != 0) return -1;
            if (emit_int32(ctx, ctx->max_stack_size) != 0) return -1;
        }
        
    } else if (ctx->arch == ARCH_ARM64) {
        /* Standard ARM64 function prologue */
        /* stp x29, x30, [sp, #-16]! */
        if (emit_int32(ctx, 0xa9bf7bfd) != 0) return -1;
        
        /* mov x29, sp */
        if (emit_int32(ctx, 0x910003fd) != 0) return -1;
        
        /* sub sp, sp, #stack_size */
        if (ctx->max_stack_size > 0) {
            uint32_t sub_inst = 0xd10003ff | ((ctx->max_stack_size & 0xfff) << 10);
            if (emit_int32(ctx, sub_inst) != 0) return -1;
        }
    }
    
    return 0;
}

/*
** Generate epilogue code
*/
static int generate_epilogue(CodegenContext *ctx) {
    AQL_DEBUG(3, "Generating function epilogue");
    
    if (ctx->arch == ARCH_X86_64) {
        /* Standard x86-64 function epilogue */
        /* mov %rbp, %rsp */
        if (emit_bytes(ctx, "\x48\x89\xec", 3) != 0) return -1;
        
        /* pop %rbp */
        if (emit_byte(ctx, 0x5d) != 0) return -1;
        
        /* ret */
        if (emit_byte(ctx, 0xc3) != 0) return -1;
        
    } else if (ctx->arch == ARCH_ARM64) {
        /* Standard ARM64 function epilogue */
        /* mov sp, x29 */
        if (emit_int32(ctx, 0x910003bf) != 0) return -1;
        
        /* ldp x29, x30, [sp], #16 */
        if (emit_int32(ctx, 0xa8c17bfd) != 0) return -1;
        
        /* ret */
        if (emit_int32(ctx, 0xd65f03c0) != 0) return -1;
    }
    
    return 0;
}

/*
** Main bytecode compilation function
*/
AQL_API int aqlCodegen_compile_bytecode(CodegenContext *ctx) {
    if (!ctx) return -1;
    
    AQL_DEBUG(1, "Starting bytecode compilation for %d instructions", ctx->bytecode_count);
    
    double start_time = (double)clock() / CLOCKS_PER_SEC;
    
    /* Run optimization passes first */
    aqlCodegen_optimize_all(ctx);
    
    /* Perform register allocation */
    int spills = aqlCodegen_alloc_registers(ctx);
    if (spills < 0) {
        AQL_DEBUG(1, "Register allocation failed");
        return -1;
    }
    
    AQL_DEBUG(2, "Register allocation complete: %d spills generated", spills);
    
    /* Generate function prologue */
    if (generate_prologue(ctx) != 0) {
        AQL_DEBUG(1, "Failed to generate prologue");
        return -1;
    }
    
    /* Compile each bytecode instruction */
    for (int pc = 0; pc < ctx->bytecode_count; pc++) {
        Instruction inst = ctx->bytecode[pc];
        OpCode op = GET_OPCODE(inst);
        
        /* Skip NOPs created by optimization */
        if (op == OP_MOVE && GETARG_A(inst) == 0 && GETARG_B(inst) == 0) {
            continue;
        }
        
        /* Record label position for jump targets */
        CodegenLabel *labels = (CodegenLabel *)ctx->labels;
        labels[pc].code_offset = ctx->code_size;
        
        /* Get instruction template */
        const InstructionTemplate *tmpl = aqlCodegen_get_template(op);
        if (!tmpl) {
            AQL_DEBUG(1, "No template found for opcode %d (%s)", op, aql_opnames[op]);
            continue;
        }
        
        /* Extract instruction arguments */
        int a = GETARG_A(inst);
        int b = GETARG_B(inst);
        int c = GETARG_C(inst);
        int bx = GETARG_Bx(inst);
        int sbx = GETARG_sBx(inst);
        
        /* Emit instruction based on template type */
        switch (tmpl->type) {
            case TEMPLATE_LOAD_CONST:
                if (op == OP_LOADI) {
                    aqlCodegen_emit_instruction(ctx, tmpl, a, -1, -1, sbx);
                } else if (op == OP_LOADK) {
                    /* Load constant from constant table */
                    if (bx < ctx->proto->sizek) {
                        TValue *k = &ctx->proto->k[bx];
                        int64_t value = 0;
                        if (ttisinteger(k)) {
                            value = ivalue(k);
                        } else if (ttisnumber(k)) {
                            value = (int64_t)fltvalue(k);
                        }
                        aqlCodegen_emit_instruction(ctx, tmpl, a, -1, -1, value);
                    }
                }
                break;
                
            case TEMPLATE_LOAD_REG:
                aqlCodegen_emit_instruction(ctx, tmpl, a, b, -1, 0);
                break;
                
            case TEMPLATE_BINARY_OP:
                aqlCodegen_emit_instruction(ctx, tmpl, a, b, c, 0);
                break;
                
            case TEMPLATE_UNARY_OP:
                aqlCodegen_emit_instruction(ctx, tmpl, a, b, -1, 0);
                break;
                
            case TEMPLATE_RETURN:
                /* For return instructions, just generate the return */
                aqlCodegen_emit_instruction(ctx, tmpl, a, b, -1, 0);
                /* Generate epilogue and return */
                if (generate_epilogue(ctx) != 0) {
                    AQL_DEBUG(1, "Failed to generate epilogue");
                    return -1;
                }
                goto compilation_complete;
                
            case TEMPLATE_JUMP:
            case TEMPLATE_BRANCH:
            case TEMPLATE_CALL:
                /* These require special handling for address resolution */
                aqlCodegen_emit_instruction(ctx, tmpl, a, b, c, 0);
                break;
                
            default:
                AQL_DEBUG(2, "Unhandled template type %d for opcode %s", 
                         tmpl->type, aql_opnames[op]);
                break;
        }
        
        CodegenLabel *debug_labels = (CodegenLabel *)ctx->labels;
        AQL_DEBUG(3, "Compiled PC %d: %s (A=%d, B=%d, C=%d) -> %zu bytes", 
                 pc, aql_opnames[op], a, b, c, ctx->code_size - debug_labels[pc].code_offset);
    }
    
compilation_complete:
    
    /* Ensure we have a return instruction */
    if (ctx->code_size == 0 || ctx->code_buffer[ctx->code_size - 1] != 0xc3) {
        if (generate_epilogue(ctx) != 0) {
            AQL_DEBUG(1, "Failed to generate final epilogue");
            return -1;
        }
    }
    
    double end_time = (double)clock() / CLOCKS_PER_SEC;
    ctx->stats.generation_time = end_time - start_time;
    ctx->stats.memory_used = ctx->code_size;
    
    AQL_DEBUG(1, "Compilation complete: %zu bytes generated in %.3fms, %d instructions, %d optimizations", 
              ctx->code_size, ctx->stats.generation_time * 1000.0, 
              ctx->stats.instructions_generated, ctx->stats.optimizations_applied);
    
    return 0;
}

/*
** Virtual register allocation
*/
AQL_API int aqlCodegen_alloc_virtual_reg(CodegenContext *ctx) {
    if (!ctx) return -1;
    
    if (ctx->next_virtual_reg >= ctx->num_virtual_regs) {
        AQL_DEBUG(1, "Out of virtual registers");
        return -1;
    }
    
    int reg_id = ctx->next_virtual_reg++;
    ctx->virtual_regs[reg_id].id = reg_id;
    
    return reg_id;
}

/*
** Physical register allocation
*/
AQL_API int aqlCodegen_alloc_physical_reg(CodegenContext *ctx, RegisterType type) {
    if (!ctx) return -1;
    
    for (int i = 0; i < ctx->num_physical_regs; i++) {
        if (!ctx->physical_regs[i].is_allocated && ctx->physical_regs[i].type == type) {
            ctx->physical_regs[i].is_allocated = true;
            return i;
        }
    }
    
    return -1;  /* No free register */
}

/*
** Free physical register
*/
AQL_API void aqlCodegen_free_physical_reg(CodegenContext *ctx, int reg_id) {
    if (!ctx || reg_id < 0 || reg_id >= ctx->num_physical_regs) return;
    
    ctx->physical_regs[reg_id].is_allocated = false;
    ctx->physical_regs[reg_id].virtual_reg = -1;
    ctx->physical_regs[reg_id].is_dirty = false;
}

/*
** Architecture-specific compilation backends
*/
AQL_API int aqlCodegen_x86_64_compile(CodegenContext *ctx) {
    if (!ctx || ctx->arch != ARCH_X86_64) return -1;
    
    AQL_DEBUG(2, "Using x86-64 compilation backend");
    return aqlCodegen_compile_bytecode(ctx);
}

AQL_API int aqlCodegen_arm64_compile(CodegenContext *ctx) {
    if (!ctx || ctx->arch != ARCH_ARM64) return -1;
    
    AQL_DEBUG(2, "Using ARM64 compilation backend");
    return aqlCodegen_compile_bytecode(ctx);
}