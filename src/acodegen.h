/*
** $Id: acodegen.h $
** Advanced Machine Code Generator for AQL JIT
** Multi-architecture native code generation system
** See Copyright Notice in aql.h
*/

#ifndef acodegen_h
#define acodegen_h

#include "aconf.h"
#include "aobject.h"
#include "aopcodes.h"
#include "ajit.h"
#include <stdbool.h>

/*
** Target Architecture Support
*/
typedef enum {
    ARCH_X86_64,
    ARCH_ARM64,
    ARCH_RISCV64,
    ARCH_WASM32
} CodegenArch;

/*
** Register Types and Allocation
*/
typedef enum {
    REG_TYPE_GENERAL,    /* General purpose registers */
    REG_TYPE_FLOAT,      /* Floating point registers */
    REG_TYPE_VECTOR,     /* SIMD/Vector registers */
    REG_TYPE_SPECIAL     /* Special purpose registers */
} RegisterType;

typedef struct {
    int id;              /* Physical register ID */
    RegisterType type;   /* Register type */
    bool is_allocated;   /* Currently allocated */
    int virtual_reg;     /* Virtual register mapped to this physical register */
    int last_use;        /* Last instruction that used this register */
    bool is_dirty;       /* Needs to be spilled */
} PhysicalRegister;

typedef struct {
    int id;              /* Virtual register ID */
    int physical_reg;    /* Physical register (-1 if spilled) */
    int spill_slot;      /* Stack spill slot (-1 if not spilled) */
    int def_point;       /* Instruction where defined */
    int last_use;        /* Last instruction that uses this register */
    bool is_constant;    /* Contains a constant value */
    TValue constant_val; /* Constant value if is_constant */
} VirtualRegister;

/*
** Code Generation Context
*/
typedef struct CodegenContext {
    /* Target architecture */
    CodegenArch arch;
    
    /* Input bytecode */
    Instruction *bytecode;
    int bytecode_count;
    Proto *proto;
    
    /* Register allocation */
    PhysicalRegister *physical_regs;
    int num_physical_regs;
    VirtualRegister *virtual_regs;
    int num_virtual_regs;
    int next_virtual_reg;
    
    /* Code generation */
    unsigned char *code_buffer;
    size_t code_size;
    size_t code_capacity;
    
    /* Stack frame management */
    int stack_size;
    int max_stack_size;
    int spill_slots_used;
    
    /* Jump targets and labels */
    void *labels;  /* Array of label structures */
    int num_labels;
    
    /* Optimization context */
    struct {
        bool enable_constant_folding;
        bool enable_dead_code_elimination;
        bool enable_register_coalescing;
        bool enable_peephole_optimization;
        int optimization_level;  /* 0-3 */
    } opt_config;
    
    /* Statistics */
    struct {
        int instructions_generated;
        int optimizations_applied;
        double generation_time;
        size_t memory_used;
    } stats;
    
} CodegenContext;

/*
** Instruction Template System
*/
typedef enum {
    TEMPLATE_LOAD_CONST,     /* Load constant to register */
    TEMPLATE_LOAD_REG,       /* Move between registers */
    TEMPLATE_BINARY_OP,      /* Binary arithmetic operation */
    TEMPLATE_UNARY_OP,       /* Unary operation */
    TEMPLATE_MEMORY_LOAD,    /* Load from memory */
    TEMPLATE_MEMORY_STORE,   /* Store to memory */
    TEMPLATE_BRANCH,         /* Conditional branch */
    TEMPLATE_JUMP,           /* Unconditional jump */
    TEMPLATE_CALL,           /* Function call */
    TEMPLATE_RETURN,         /* Function return */
    TEMPLATE_COMPARE,        /* Comparison operation */
    TEMPLATE_CONVERT         /* Type conversion */
} TemplateType;

typedef struct InstructionTemplate {
    TemplateType type;
    OpCode aql_opcode;       /* Corresponding AQL opcode */
    
    /* x86-64 encoding */
    struct {
        unsigned char encoding[16];  /* Machine code bytes */
        int length;                  /* Actual length */
        bool has_modrm;             /* Has ModR/M byte */
        bool has_sib;               /* Has SIB byte */
        bool has_displacement;      /* Has displacement */
        bool has_immediate;         /* Has immediate value */
    } x86_64;
    
    /* ARM64 encoding */
    struct {
        uint32_t encoding;          /* 32-bit instruction */
        bool has_immediate;         /* Has immediate field */
        int immediate_bits;         /* Number of immediate bits */
    } arm64;
    
    /* Optimization hints */
    struct {
        bool can_eliminate;         /* Can be eliminated if result unused */
        bool is_commutative;        /* Operands can be swapped */
        bool affects_flags;         /* Modifies CPU flags */
        bool can_fold_constants;    /* Can fold constant operands */
    } hints;
    
} InstructionTemplate;

/*
** Code Generation API
*/

/* Context Management */
AQL_API CodegenContext *aqlCodegen_create_context(CodegenArch arch, Proto *proto);
AQL_API void aqlCodegen_destroy_context(CodegenContext *ctx);

/* Register Allocation */
AQL_API int aqlCodegen_alloc_virtual_reg(CodegenContext *ctx);
AQL_API int aqlCodegen_alloc_physical_reg(CodegenContext *ctx, RegisterType type);
AQL_API void aqlCodegen_free_physical_reg(CodegenContext *ctx, int reg_id);
AQL_API int aqlCodegen_get_physical_reg(CodegenContext *ctx, int virtual_reg);
AQL_API void aqlCodegen_spill_register(CodegenContext *ctx, int virtual_reg);

/* Code Generation */
AQL_API int aqlCodegen_compile_bytecode(CodegenContext *ctx);
AQL_API void aqlCodegen_emit_instruction(CodegenContext *ctx, const InstructionTemplate *tmpl, ...);
AQL_API void aqlCodegen_emit_label(CodegenContext *ctx, int label_id);
AQL_API void aqlCodegen_emit_jump(CodegenContext *ctx, int target_label);

/* Optimization Passes */
AQL_API void aqlCodegen_optimize_constant_folding(CodegenContext *ctx);
AQL_API void aqlCodegen_optimize_dead_code_elimination(CodegenContext *ctx);
AQL_API void aqlCodegen_optimize_register_coalescing(CodegenContext *ctx);
AQL_API void aqlCodegen_optimize_peephole(CodegenContext *ctx);

/* Architecture-specific backends */
AQL_API int aqlCodegen_x86_64_compile(CodegenContext *ctx);
AQL_API int aqlCodegen_arm64_compile(CodegenContext *ctx);

/* Instruction Templates */
extern const InstructionTemplate aql_instruction_templates[NUM_OPCODES];

/* Utility functions */
AQL_API const char *aqlCodegen_arch_name(CodegenArch arch);
AQL_API int aqlCodegen_get_register_count(CodegenArch arch, RegisterType type);
AQL_API size_t aqlCodegen_estimate_code_size(Proto *proto);

#endif /* acodegen_h */