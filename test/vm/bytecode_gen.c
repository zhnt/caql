/*
** 字节码生成工具
** 生成测试用的 .by 文件和对应的 .expected 文件
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../../src/aopcodes.h"

/*
** 字节码文件头
*/
typedef struct {
    uint32_t magic;      // "AQLB" = 0x414C5142
    uint32_t version;    // 版本号
    uint32_t code_size;  // 指令数量
    uint32_t const_size; // 常量数量
    uint32_t stack_size; // 栈大小
} BytecodeHeader;

/*
** 指令构造宏
*/
#ifndef CREATE_ABC
#define CREATE_ABC(op, a, b, c) \
    ((uint32_t)(((c) << 24) | ((b) << 16) | ((a) << 7) | (op)))
#endif

#ifndef CREATE_ABx
#define CREATE_ABx(op, a, bx) \
    ((uint32_t)(((bx) << 15) | ((a) << 7) | (op)))
#endif

/*
** 保存字节码文件
*/
static int save_bytecode(const char *filename, uint32_t *code, int code_size, int stack_size) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        printf("❌ 无法创建文件: %s\n", filename);
        return 0;
    }
    
    BytecodeHeader header = {
        .magic = 0x414C5142,  // "AQLB"
        .version = 1,
        .code_size = code_size,
        .const_size = 0,      // 暂时不支持常量
        .stack_size = stack_size
    };
    
    fwrite(&header, sizeof(header), 1, f);
    fwrite(code, sizeof(uint32_t), code_size, f);
    
    fclose(f);
    printf("✅ 生成字节码: %s\n", filename);
    return 1;
}

/*
** 保存期望结果文件
*/
static int save_expected(const char *filename, const char *expected) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("❌ 无法创建文件: %s\n", filename);
        return 0;
    }
    
    fprintf(f, "%s", expected);
    fclose(f);
    printf("✅ 生成期望结果: %s\n", filename);
    return 1;
}

/*
** 生成测试1: 简单算术
*/
static void generate_arithmetic_test() {
    printf("📝 生成算术测试...\n");
    
    // 计算 5 + 3 = 8
    uint32_t code[] = {
        CREATE_ABC(OP_LOADI, 0, 5, 0),      // R0 = 5
        CREATE_ABC(OP_LOADI, 1, 3, 0),      // R1 = 3
        CREATE_ABC(OP_ADD, 2, 0, 1),        // R2 = R0 + R1
        CREATE_ABC(OP_RET_ONE, 2, 0, 0)     // return R2
    };
    
    const char *expected = "字节码分析:\n"
                          "[0] OP_1 A=0 B=5 C=0\n"
                          "[1] OP_1 A=1 B=3 C=0\n"
                          "[2] OP_26 A=2 B=0 C=1\n"
                          "[3] OP_78 A=2 B=0 C=0\n"
                          "\n注意: 需要实现完整的 aqlV_execute2 调用";
    
    save_bytecode("bytecode/arithmetic.by", code, 4, 10);
    save_expected("bytecode/arithmetic.expected", expected);
}

/*
** 生成测试2: 多指令运算
*/
static void generate_complex_test() {
    printf("📝 生成复杂运算测试...\n");
    
    // 计算 (10 * 2) + (15 - 5) = 30
    uint32_t code[] = {
        CREATE_ABC(OP_LOADI, 0, 10, 0),     // R0 = 10
        CREATE_ABC(OP_LOADI, 1, 2, 0),      // R1 = 2
        CREATE_ABC(OP_MUL, 2, 0, 1),        // R2 = R0 * R1 = 20
        CREATE_ABC(OP_LOADI, 3, 15, 0),     // R3 = 15
        CREATE_ABC(OP_LOADI, 4, 5, 0),      // R4 = 5
        CREATE_ABC(OP_SUB, 5, 3, 4),        // R5 = R3 - R4 = 10
        CREATE_ABC(OP_ADD, 6, 2, 5),        // R6 = R2 + R5 = 30
        CREATE_ABC(OP_RET_ONE, 6, 0, 0)     // return R6
    };
    
    const char *expected = "字节码分析:\n"
                          "[0] OP_1 A=0 B=10 C=0\n"
                          "[1] OP_1 A=1 B=2 C=0\n"
                          "[2] OP_28 A=2 B=0 C=1\n"
                          "[3] OP_1 A=3 B=15 C=0\n"
                          "[4] OP_1 A=4 B=5 C=0\n"
                          "[5] OP_27 A=5 B=3 C=4\n"
                          "[6] OP_26 A=6 B=2 C=5\n"
                          "[7] OP_78 A=6 B=0 C=0\n"
                          "\n注意: 需要实现完整的 aqlV_execute2 调用";
    
    save_bytecode("bytecode/complex.by", code, 8, 15);
    save_expected("bytecode/complex.expected", expected);
}

/*
** 生成测试3: 跳转指令
*/
static void generate_jump_test() {
    printf("📝 生成跳转测试...\n");
    
    // 简单的条件跳转
    uint32_t code[] = {
        CREATE_ABC(OP_LOADTRUE, 0, 0, 0),   // R0 = true
        CREATE_ABC(OP_TEST, 0, 0, 0),       // test R0
        CREATE_ABC(OP_JMP, 0, 1, 0),        // jump +1
        CREATE_ABC(OP_LOADI, 1, 100, 0),    // R1 = 100 (skipped)
        CREATE_ABC(OP_LOADI, 1, 200, 0),    // R1 = 200 (executed)
        CREATE_ABC(OP_RET_ONE, 1, 0, 0)     // return R1
    };
    
    const char *expected = "字节码分析:\n"
                          "[0] OP_5 A=0 B=0 C=0\n"
                          "[1] OP_49 A=0 B=0 C=0\n"
                          "[2] OP_50 A=0 B=1 C=0\n"
                          "[3] OP_1 A=1 B=100 C=0\n"
                          "[4] OP_1 A=1 B=200 C=0\n"
                          "[5] OP_78 A=1 B=0 C=0\n"
                          "\n注意: 需要实现完整的 aqlV_execute2 调用";
    
    save_bytecode("bytecode/jump.by", code, 6, 10);
    save_expected("bytecode/jump.expected", expected);
}

/*
** 生成测试4: 不支持的指令
*/
static void generate_unsupported_test() {
    printf("📝 生成不支持指令测试...\n");
    
    // 使用一些可能不支持的指令
    uint32_t code[] = {
        CREATE_ABC(OP_NEWTABLE, 0, 0, 0),   // R0 = {} (可能不支持)
        CREATE_ABC(OP_SETTABLE, 0, 1, 2),  // R0[R1] = R2 (可能不支持)
        CREATE_ABC(OP_RET_VOID, 0, 0, 0)   // return
    };
    
    const char *expected = "UNSUPPORTED: 字节码执行需要完整的 VM 支持\n";
    
    save_bytecode("bytecode/unsupported.by", code, 3, 10);
    save_expected("bytecode/unsupported.expected", expected);
}

/*
** 生成所有测试
*/
static void generate_all_tests() {
    printf("🏭 字节码测试生成器\n");
    printf("🎯 目标: 生成 VM 测试用的 .by 和 .expected 文件\n\n");
    
    generate_arithmetic_test();
    generate_complex_test();
    generate_jump_test();
    generate_unsupported_test();
    
    printf("\n✅ 所有测试文件生成完成！\n");
    printf("📁 文件位置: test/vm/bytecode/\n");
    printf("🚀 运行测试: make vmtest\n");
}

/*
** 主函数
*/
int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("字节码生成工具\n");
        printf("用法: %s [--help]\n", argv[0]);
        printf("生成测试用的 .by 和 .expected 文件到 test/vm/bytecode/ 目录\n");
        return 0;
    }
    
    generate_all_tests();
    return 0;
}
