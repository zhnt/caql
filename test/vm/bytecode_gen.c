/*
** Text bytecode generator for VM smoke tests.
** The current VM runners execute .by files as text assembly, so this
** generator intentionally emits text fixtures instead of binary blobs.
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int ensure_dir(const char *path) {
    if (mkdir(path, 0777) == 0 || errno == EEXIST) {
        return 1;
    }
    printf("❌ 无法创建目录: %s\n", path);
    return 0;
}

static int write_file(const char *filename, const char *content) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("❌ 无法创建文件: %s\n", filename);
        return 0;
    }
    fputs(content, f);
    fclose(f);
    return 1;
}

static int write_test(const char *stem, const char *bytecode, const char *expected) {
    char by_path[256];
    char expected_path[256];

    snprintf(by_path, sizeof(by_path), "bytecode/generated/%s.by", stem);
    snprintf(expected_path, sizeof(expected_path), "bytecode/generated/%s.expected", stem);

    if (!write_file(by_path, bytecode)) {
        return 0;
    }
    if (!write_file(expected_path, expected)) {
        return 0;
    }

    printf("✅ 生成: %s\n", by_path);
    printf("✅ 生成: %s\n", expected_path);
    return 1;
}

static void generate_arithmetic_test(void) {
    const char *bytecode =
        "# arithmetic.by\n"
        ".code\n"
        "LOADI   R0, 5\n"
        "LOADI   R1, 3\n"
        "ADD     R2, R0, R1\n"
        "MMBIN   R0, R1, 6\n"
        "RETURN1 R2\n"
        ".end\n";

    write_test("arithmetic", bytecode, "8\n");
}

static void generate_complex_test(void) {
    const char *bytecode =
        "# complex.by\n"
        ".code\n"
        "LOADI   R0, 10\n"
        "LOADI   R1, 2\n"
        "MUL     R2, R0, R1\n"
        "MMBIN   R0, R1, 8\n"
        "LOADI   R3, 15\n"
        "LOADI   R4, 5\n"
        "SUB     R5, R3, R4\n"
        "MMBIN   R3, R4, 7\n"
        "ADD     R6, R2, R5\n"
        "MMBIN   R2, R5, 6\n"
        "RETURN1 R6\n"
        ".end\n";

    write_test("complex", bytecode, "30\n");
}

static void generate_jump_test(void) {
    const char *bytecode =
        "# jump_offset.by\n"
        ".code\n"
        "LOADFALSE R0\n"
        "TEST      R0, 0\n"
        "JMP       3         # inline comment should be ignored\n"
        "LOADI     R1, 200\n"
        "RETURN1   R1\n"
        "JMP       2         # inline comment should be ignored\n"
        "LOADI     R1, 100\n"
        "RETURN1   R1\n"
        ".end\n";

    write_test("jump_offset", bytecode, "100\n");
}

static void generate_mmbin_passthrough_test(void) {
    const char *bytecode =
        "# mmbin_passthrough.by\n"
        ".code\n"
        "LOADI   R0, 5\n"
        "LOADI   R1, 3\n"
        "ADD     R2, R0, R1\n"
        "MMBIN   R0, R1, 6\n"
        "RETURN1 R2\n"
        ".end\n";

    write_test("mmbin_passthrough", bytecode, "8\n");
}

static void generate_return0_test(void) {
    const char *bytecode =
        "# return0_legacy.by\n"
        ".code\n"
        "RET_VOID\n"
        ".end\n";

    write_test("return0_legacy", bytecode, "nil\n");
}

int main(void) {
    printf("🏭 文本字节码测试生成器\n");

    if (!ensure_dir("bytecode")) {
        return 1;
    }
    if (!ensure_dir("bytecode/generated")) {
        return 1;
    }

    generate_arithmetic_test();
    generate_complex_test();
    generate_jump_test();
    generate_mmbin_passthrough_test();
    generate_return0_test();

    printf("✅ 文本字节码测试生成完成\n");
    return 0;
}
