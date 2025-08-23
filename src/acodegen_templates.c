/*
** $Id: acodegen_templates.c $
** Instruction Templates for AQL JIT Code Generation
** Maps AQL bytecode instructions to native machine code patterns
** See Copyright Notice in aql.h
*/

#include "acodegen.h"
#include <string.h>

/*
** x86-64 Register Encoding
*/
#define REG_RAX  0
#define REG_RCX  1
#define REG_RDX  2
#define REG_RBX  3
#define REG_RSP  4
#define REG_RBP  5
#define REG_RSI  6
#define REG_RDI  7
#define REG_R8   8
#define REG_R9   9
#define REG_R10  10
#define REG_R11  11
#define REG_R12  12
#define REG_R13  13
#define REG_R14  14
#define REG_R15  15

/*
** x86-64 Instruction Prefixes and Opcodes
*/
#define REX_W    0x48  /* 64-bit operand size */
#define MOV_REG  0x89  /* MOV r/m64, r64 */
#define MOV_IMM  0xB8  /* MOV r64, imm64 */
#define ADD_REG  0x01  /* ADD r/m64, r64 */
#define SUB_REG  0x29  /* SUB r/m64, r64 */
#define MUL_REG  0xF7  /* MUL r/m64 (with /4) */
#define CMP_REG  0x39  /* CMP r/m64, r64 */
#define JE_REL   0x74  /* JE rel8 */
#define JNE_REL  0x75  /* JNE rel8 */
#define JMP_REL  0xEB  /* JMP rel8 */
#define CALL_REL 0xE8  /* CALL rel32 */
#define RET      0xC3  /* RET */

/*
** ARM64 Instruction Encoding
*/
#define ARM64_ADD_IMM    0x91000000  /* ADD Xd, Xn, #imm */
#define ARM64_SUB_IMM    0xD1000000  /* SUB Xd, Xn, #imm */
#define ARM64_MOV_REG    0xAA0003E0  /* MOV Xd, Xn */
#define ARM64_MOV_IMM    0xD2800000  /* MOV Xd, #imm */
#define ARM64_LDR_IMM    0xF9400000  /* LDR Xd, [Xn, #imm] */
#define ARM64_STR_IMM    0xF9000000  /* STR Xd, [Xn, #imm] */
#define ARM64_B_COND     0x54000000  /* B.cond label */
#define ARM64_B          0x14000000  /* B label */
#define ARM64_BL         0x94000000  /* BL label */
#define ARM64_RET        0xD65F03C0  /* RET */

/*
** Instruction Template Database
** Maps each AQL opcode to native code templates for different architectures
*/
const InstructionTemplate aql_instruction_templates[NUM_OPCODES] = {

    /* OP_MOVE: R(A) := R(B) */
    [OP_MOVE] = {
        .type = TEMPLATE_LOAD_REG,
        .aql_opcode = OP_MOVE,
        .x86_64 = {
            .encoding = {REX_W, MOV_REG, 0xC0},  /* mov %src, %dst (ModR/M to be filled) */
            .length = 3,
            .has_modrm = true,
            .has_sib = false,
            .has_displacement = false,
            .has_immediate = false
        },
        .arm64 = {
            .encoding = ARM64_MOV_REG,  /* mov xd, xn */
            .has_immediate = false,
            .immediate_bits = 0
        },
        .hints = {
            .can_eliminate = true,      /* Can eliminate if src == dst */
            .is_commutative = false,
            .affects_flags = false,
            .can_fold_constants = false
        }
    },

    /* OP_LOADI: R(A) := sBx */
    [OP_LOADI] = {
        .type = TEMPLATE_LOAD_CONST,
        .aql_opcode = OP_LOADI,
        .x86_64 = {
            .encoding = {REX_W, MOV_IMM},  /* mov $imm, %reg */
            .length = 2,
            .has_modrm = false,
            .has_sib = false,
            .has_displacement = false,
            .has_immediate = true
        },
        .arm64 = {
            .encoding = ARM64_MOV_IMM,
            .has_immediate = true,
            .immediate_bits = 16
        },
        .hints = {
            .can_eliminate = false,
            .is_commutative = false,
            .affects_flags = false,
            .can_fold_constants = true
        }
    },

    /* OP_ADD: R(A) := R(B) + R(C) */
    [OP_ADD] = {
        .type = TEMPLATE_BINARY_OP,
        .aql_opcode = OP_ADD,
        .x86_64 = {
            .encoding = {REX_W, ADD_REG, 0xC0},  /* add %src, %dst */
            .length = 3,
            .has_modrm = true,
            .has_sib = false,
            .has_displacement = false,
            .has_immediate = false
        },
        .arm64 = {
            .encoding = ARM64_ADD_IMM,  /* add xd, xn, xm */
            .has_immediate = false,
            .immediate_bits = 0
        },
        .hints = {
            .can_eliminate = false,
            .is_commutative = true,
            .affects_flags = true,
            .can_fold_constants = true
        }
    },

    /* OP_SUB: R(A) := R(B) - R(C) */
    [OP_SUB] = {
        .type = TEMPLATE_BINARY_OP,
        .aql_opcode = OP_SUB,
        .x86_64 = {
            .encoding = {REX_W, SUB_REG, 0xC0},  /* sub %src, %dst */
            .length = 3,
            .has_modrm = true,
            .has_sib = false,
            .has_displacement = false,
            .has_immediate = false
        },
        .arm64 = {
            .encoding = ARM64_SUB_IMM,  /* sub xd, xn, xm */
            .has_immediate = false,
            .immediate_bits = 0
        },
        .hints = {
            .can_eliminate = false,
            .is_commutative = false,
            .affects_flags = true,
            .can_fold_constants = true
        }
    },

    /* OP_LOADK: R(A) := K(Bx) */
    [OP_LOADK] = {
        .type = TEMPLATE_LOAD_CONST,
        .aql_opcode = OP_LOADK,
        .x86_64 = {
            .encoding = {REX_W, MOV_IMM},  /* mov $const, %reg */
            .length = 2,
            .has_modrm = false,
            .has_sib = false,
            .has_displacement = false,
            .has_immediate = true
        },
        .arm64 = {
            .encoding = ARM64_LDR_IMM,  /* ldr xd, =constant */
            .has_immediate = true,
            .immediate_bits = 12
        },
        .hints = {
            .can_eliminate = false,
            .is_commutative = false,
            .affects_flags = false,
            .can_fold_constants = true
        }
    },

    /* OP_RET: return R(A), ..., R(A+B-2) */
    [OP_RET] = {
        .type = TEMPLATE_RETURN,
        .aql_opcode = OP_RET,
        .x86_64 = {
            .encoding = {RET},  /* ret */
            .length = 1,
            .has_modrm = false,
            .has_sib = false,
            .has_displacement = false,
            .has_immediate = false
        },
        .arm64 = {
            .encoding = ARM64_RET,  /* ret */
            .has_immediate = false,
            .immediate_bits = 0
        },
        .hints = {
            .can_eliminate = false,
            .is_commutative = false,
            .affects_flags = false,
            .can_fold_constants = false
        }
    },

    /* OP_JMP: pc += sBx; if (A) close upvalues >= R(A-1) */
    [OP_JMP] = {
        .type = TEMPLATE_JUMP,
        .aql_opcode = OP_JMP,
        .x86_64 = {
            .encoding = {JMP_REL, 0x00},  /* jmp rel8/rel32 */
            .length = 2,
            .has_modrm = false,
            .has_sib = false,
            .has_displacement = true,
            .has_immediate = false
        },
        .arm64 = {
            .encoding = ARM64_B,  /* b label */
            .has_immediate = true,
            .immediate_bits = 26
        },
        .hints = {
            .can_eliminate = false,
            .is_commutative = false,
            .affects_flags = false,
            .can_fold_constants = false
        }
    },

    /* OP_EQ: if ((R(A) == R(B)) ~= k) then pc++ */
    [OP_EQ] = {
        .type = TEMPLATE_COMPARE,
        .aql_opcode = OP_EQ,
        .x86_64 = {
            .encoding = {REX_W, CMP_REG, 0xC0, JE_REL, 0x00},  /* cmp %a, %b; je rel8 */
            .length = 5,
            .has_modrm = true,
            .has_sib = false,
            .has_displacement = true,
            .has_immediate = false
        },
        .arm64 = {
            .encoding = ARM64_B_COND | 0x0,  /* b.eq label (condition = 0) */
            .has_immediate = true,
            .immediate_bits = 19
        },
        .hints = {
            .can_eliminate = false,
            .is_commutative = true,
            .affects_flags = true,
            .can_fold_constants = true
        }
    },

    /* OP_CALL: R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1)) */
    [OP_CALL] = {
        .type = TEMPLATE_CALL,
        .aql_opcode = OP_CALL,
        .x86_64 = {
            .encoding = {CALL_REL, 0x00, 0x00, 0x00, 0x00},  /* call rel32 */
            .length = 5,
            .has_modrm = false,
            .has_sib = false,
            .has_displacement = true,
            .has_immediate = false
        },
        .arm64 = {
            .encoding = ARM64_BL,  /* bl label */
            .has_immediate = true,
            .immediate_bits = 26
        },
        .hints = {
            .can_eliminate = false,
            .is_commutative = false,
            .affects_flags = true,
            .can_fold_constants = false
        }
    }

    /* 其他指令模板... */
    /* 注意：这里只展示了核心指令，实际实现需要覆盖所有 NUM_OPCODES 个指令 */
};

/*
** Architecture-specific register information
*/
static const char *x86_64_reg_names[] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"
};

static const char *arm64_reg_names[] = {
    "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
    "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
    "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
    "x24", "x25", "x26", "x27", "x28", "x29", "x30", "sp"
};

/*
** Get architecture name
*/
AQL_API const char *aqlCodegen_arch_name(CodegenArch arch) {
    switch (arch) {
        case ARCH_X86_64:  return "x86-64";
        case ARCH_ARM64:   return "ARM64";
        case ARCH_RISCV64: return "RISC-V 64";
        case ARCH_WASM32:  return "WebAssembly";
        default:           return "Unknown";
    }
}

/*
** Get register count for architecture and type
*/
AQL_API int aqlCodegen_get_register_count(CodegenArch arch, RegisterType type) {
    switch (arch) {
        case ARCH_X86_64:
            switch (type) {
                case REG_TYPE_GENERAL: return 16;  /* RAX-R15 */
                case REG_TYPE_FLOAT:   return 16;  /* XMM0-XMM15 */
                case REG_TYPE_VECTOR:  return 16;  /* YMM0-YMM15 */
                case REG_TYPE_SPECIAL: return 4;   /* RSP, RBP, RIP, RFLAGS */
                default: return 0;
            }
        case ARCH_ARM64:
            switch (type) {
                case REG_TYPE_GENERAL: return 31;  /* X0-X30 */
                case REG_TYPE_FLOAT:   return 32;  /* V0-V31 */
                case REG_TYPE_VECTOR:  return 32;  /* V0-V31 (same as float) */
                case REG_TYPE_SPECIAL: return 2;   /* SP, PC */
                default: return 0;
            }
        default:
            return 0;
    }
}

/*
** Estimate generated code size for a function
*/
AQL_API size_t aqlCodegen_estimate_code_size(Proto *proto) {
    if (!proto) return 0;
    
    /* Conservative estimate: 8 bytes per bytecode instruction on average */
    size_t base_size = proto->sizecode * 8;
    
    /* Add overhead for prologue/epilogue */
    size_t overhead = 64;
    
    /* Add space for constants and jump tables */
    size_t constants_size = proto->sizek * 16;
    
    return base_size + overhead + constants_size;
}

/*
** Helper function to encode ModR/M byte for x86-64
*/
__attribute__((unused))
static inline unsigned char encode_modrm(int mod, int reg, int rm) {
    return ((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7);
}

/*
** Helper function to encode ARM64 register field
*/
__attribute__((unused))
static inline uint32_t encode_arm64_reg(uint32_t instruction, int reg_field, int reg_num) {
    return instruction | ((reg_num & 0x1F) << reg_field);
}

/*
** Get instruction template for AQL opcode
*/
AQL_API const InstructionTemplate *aqlCodegen_get_template(OpCode op) {
    if (op >= 0 && op < NUM_OPCODES) {
        return &aql_instruction_templates[op];
    }
    return NULL;
}