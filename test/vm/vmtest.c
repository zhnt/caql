/*
** VM 字节码测试工具
** 类似于 make rtest，但专门测试字节码执行
** 加载 bytecode/ 目录下的 .by 文件，执行并对比 .expected 结果
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#ifdef SIMPLIFIED_BUILD
// 简化构建，只包含必要的定义
#include <stdint.h>

// 直接包含 aopcodes.h 以保持一致性
#include "../../src/aconf.h"
#include "../../src/alimits.h"
#include "../../src/aopcodes.h"

// 简化的宏定义 (类型定义已在 aconf.h 中)
#ifndef cast
#define cast(t, exp) ((t)(exp))
#endif
#ifndef cast_int
#define cast_int(i) cast(int, (i))
#endif

#else
// 完整构建，包含所有头文件
#include "../../src/aql.h"
#include "../../src/aopcodes.h"
#include "../../src/aobject.h"
#include "../../src/astate.h"
#include "../../src/afunc.h"

// 声明 avm_core.c 中的新函数
extern void aqlV_execute2(aql_State *L, struct CallInfo *ci);
#endif

/*
** 测试统计
*/
typedef struct {
    int total;
    int passed;
    int failed;
    int skipped;
} TestStats;

static TestStats g_stats = {0, 0, 0, 0};

/*
** 字节码文件头结构
*/
typedef struct {
    uint32_t magic;      // "AQLB" = 0x414C5142
    uint32_t version;    // 版本号
    uint32_t code_size;  // 指令数量
    uint32_t const_size; // 常量数量
    uint32_t stack_size; // 栈大小
} BytecodeHeader;

/*
** 简单的文本字节码解析器 (内嵌在 vmtest.c 中)
*/
static int load_bytecode_text_file(const char *filename, Instruction **code, int *code_size,
                                  void **constants, int *const_size, int *stack_size) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("❌ 无法打开文件: %s\n", filename);
        return 0;
    }
    
    static Instruction instructions[1000];
    int instruction_count = 0;
    *stack_size = 10;  // 默认栈大小
    
    char line[256];
    int in_code_section = 0;
    int in_constants_section = 0;
    
    // 简单的常量存储
    static char string_constants[10][256];
    int string_count = 0;
    
    while (fgets(line, sizeof(line), f)) {
        // 移除换行符
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // 跳过空行和注释
        char *trimmed = line;
        while (*trimmed && isspace(*trimmed)) trimmed++;
        if (!*trimmed || *trimmed == '#') continue;
        
        // 处理段标记
        if (strcmp(trimmed, ".constants") == 0) {
            in_constants_section = 1;
            in_code_section = 0;
            continue;
        } else if (strcmp(trimmed, ".code") == 0) {
            in_code_section = 1;
            in_constants_section = 0;
            continue;
        } else if (strcmp(trimmed, ".end") == 0) {
            in_code_section = 0;
            in_constants_section = 0;
            continue;
        }
        
        // 解析栈大小配置
        if (strstr(trimmed, "栈大小:") || strstr(trimmed, "stack size:")) {
            char *ptr = trimmed;
            while (*ptr && !isdigit(*ptr)) ptr++;
            if (*ptr) {
                *stack_size = atoi(ptr);
            }
            continue;
        }
        
        // 解析常量定义
        if (in_constants_section) {
            char const_name[32], const_type[32], const_value[256];
            int args = sscanf(trimmed, "%s %s %[^\n]", const_name, const_type, const_value);
            if (args >= 3 && strcmp(const_type, "STRING") == 0) {
                // 去掉引号
                if (const_value[0] == '"' && const_value[strlen(const_value)-1] == '"') {
                    const_value[strlen(const_value)-1] = '\0';
                    strcpy(string_constants[string_count], const_value + 1);
                    string_count++;
                }
            }
            continue;
        }
        
        // 只在代码段中解析指令
        if (!in_code_section) continue;
        
        // 简单的指令解析
        char opcode[32], arg1[32], arg2[32], arg3[32];
        int args = sscanf(trimmed, "%s %s %s %s", opcode, arg1, arg2, arg3);
        if (args < 1) continue;
        
        // 解析操作码和参数
        OpCode op = OP_MOVE;  // 默认值 (使用 aopcodes.h 中的第一个操作码)
        int a = 0, b = 0, c = 0;
        
        if (strcmp(opcode, "LOADI") == 0 && args >= 3) {
            op = OP_LOADI;
            a = (arg1[0] == 'R') ? atoi(arg1 + 1) : 0;
            b = atoi(arg2);
            c = 0;
        } else if (strcmp(opcode, "ADD") == 0 && args >= 4) {
            op = OP_ADD;
            a = (arg1[0] == 'R') ? atoi(arg1 + 1) : 0;
            b = (arg2[0] == 'R') ? atoi(arg2 + 1) : 0;
            c = (arg3[0] == 'R') ? atoi(arg3 + 1) : 0;
        } else if (strcmp(opcode, "SUB") == 0 && args >= 4) {
            op = OP_SUB;
            a = (arg1[0] == 'R') ? atoi(arg1 + 1) : 0;
            b = (arg2[0] == 'R') ? atoi(arg2 + 1) : 0;
            c = (arg3[0] == 'R') ? atoi(arg3 + 1) : 0;
        } else if (strcmp(opcode, "MUL") == 0 && args >= 4) {
            op = OP_MUL;
            a = (arg1[0] == 'R') ? atoi(arg1 + 1) : 0;
            b = (arg2[0] == 'R') ? atoi(arg2 + 1) : 0;
            c = (arg3[0] == 'R') ? atoi(arg3 + 1) : 0;
        } else if (strcmp(opcode, "DIV") == 0 && args >= 4) {
            op = OP_DIV;
            a = (arg1[0] == 'R') ? atoi(arg1 + 1) : 0;
            b = (arg2[0] == 'R') ? atoi(arg2 + 1) : 0;
            c = (arg3[0] == 'R') ? atoi(arg3 + 1) : 0;
        } else if (strcmp(opcode, "RET_ONE") == 0 && args >= 2) {
            op = OP_RET_ONE;
            a = (arg1[0] == 'R') ? atoi(arg1 + 1) : 0;
            b = 0;
            c = 0;
        } else if (strcmp(opcode, "LOADTRUE") == 0 && args >= 2) {
            op = OP_LOADTRUE;
            a = (arg1[0] == 'R') ? atoi(arg1 + 1) : 0;
            b = 0;
            c = 0;
        } else if (strcmp(opcode, "LOADFALSE") == 0 && args >= 2) {
            op = OP_LOADFALSE;
            a = (arg1[0] == 'R') ? atoi(arg1 + 1) : 0;
            b = 0;
            c = 0;
        } else if (strcmp(opcode, "TEST") == 0 && args >= 3) {
            op = OP_TEST;
            a = (arg1[0] == 'R') ? atoi(arg1 + 1) : 0;
            b = atoi(arg2);
            c = 0;
        } else if (strcmp(opcode, "JMP") == 0 && args >= 3) {
            op = OP_JMP;
            a = atoi(arg1);
            b = atoi(arg2);  // 跳转偏移
            c = 0;
        } else if (strcmp(opcode, "LOADK") == 0 && args >= 3) {
            op = OP_LOADK;
            a = (arg1[0] == 'R') ? atoi(arg1 + 1) : 0;
            b = (arg2[0] == 'K') ? atoi(arg2 + 1) : 0;
            c = 0;
            
            // 验证常量索引
            if (b >= string_count) {
                printf("⚠️  常量索引超出范围: K%d (只有 %d 个常量)\n", b, string_count);
            }
        } else {
            printf("⚠️  未知指令: %s\n", opcode);
            continue;
        }
        
        // 创建指令
        instructions[instruction_count] = CREATE_ABC(op, a, b, c);
        instruction_count++;
        
        if (instruction_count >= 1000) {
            printf("❌ 指令数量超出限制\n");
            fclose(f);
            return 0;
        }
    }
    
    fclose(f);
    
    // 返回结果
    *code = instructions;
    *code_size = instruction_count;
    *constants = (string_count > 0) ? (void*)string_constants : NULL;
    *const_size = string_count;
    
    printf("📝 解析完成: %d 条指令, %d 个常量, 栈大小: %d\n", 
           instruction_count, string_count, *stack_size);
    
    // 显示常量表
    if (string_count > 0) {
        printf("📋 常量表:\n");
        for (int i = 0; i < string_count; i++) {
            printf("  K%d: \"%s\"\n", i, string_constants[i]);
        }
    }
    
    return 1;
}

/*
** 加载字节码文件 (二进制格式)
*/
static int load_bytecode_file(const char *filename, Instruction **code, int *code_size,
                             void **constants, int *const_size, int *stack_size) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        printf("❌ 无法打开文件: %s\n", filename);
        return 0;
    }
    
    BytecodeHeader header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        printf("❌ 读取文件头失败: %s\n", filename);
        fclose(f);
        return 0;
    }
    
    // 检查魔数
    if (header.magic != 0x414C5142) {  // "AQLB"
        printf("❌ 无效的字节码文件: %s (magic=0x%08X)\n", filename, header.magic);
        fclose(f);
        return 0;
    }
    
    // 读取字节码
    *code_size = header.code_size;
    *code = malloc(sizeof(Instruction) * header.code_size);
    if (fread(*code, sizeof(Instruction), header.code_size, f) != header.code_size) {
        printf("❌ 读取字节码失败: %s\n", filename);
        free(*code);
        fclose(f);
        return 0;
    }
    
    // 读取常量表 (简化版本)
    *const_size = header.const_size;
    if (header.const_size > 0) {
        *constants = malloc(header.const_size * 16);  // 简化分配
        // 跳过文件中的常量数据
        fseek(f, header.const_size * 16, SEEK_CUR);  // 假设每个常量16字节
    } else {
        *constants = NULL;
    }
    
    *stack_size = header.stack_size;
    
    fclose(f);
    return 1;
}

/*
** 读取期望结果文件
*/
static char* read_expected_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    
    fclose(f);
    return content;
}

/*
** 执行字节码 (简化版本，避免复杂依赖)
*/
static int execute_bytecode_simple(Instruction *code, int code_size, int stack_size, 
                                  char *output, size_t output_size) {
#ifdef SIMPLIFIED_BUILD
    // 简化版本：只做字节码分析
    char analysis[1024] = "";
    char temp[256];
    
    for (int i = 0; i < code_size && i < 10; i++) {  // 最多分析前10条指令
        OpCode op = GET_OPCODE(code[i]);
        int a = GETARG_A(code[i]);
        int b = GETARG_B(code[i]);
        int c = GETARG_C(code[i]);
        
        snprintf(temp, sizeof(temp), "[%d] OP_%d A=%d B=%d C=%d\n", i, op, a, b, c);
        strncat(analysis, temp, sizeof(analysis) - strlen(analysis) - 1);
    }
    
    // 简化版本：显示字节码但不执行
    snprintf(output, output_size, "简化版本，无法调用 aqlV_execute2 (缺少完整的 AQL 运行时)");
    return 1;
#else
    // 完整版本：实际执行字节码
    aql_State *L = aql_newstate(NULL, NULL);
    if (!L) {
        snprintf(output, output_size, "执行结果:\n执行状态: 失败 (无法创建 AQL 状态)");
        return 0;
    }
    
    // 创建函数原型
    Proto *p = aqlF_newproto(L);
    p->code = code;
    p->sizecode = code_size;
    p->maxstacksize = stack_size;
    p->numparams = 0;
    p->is_vararg = 0;
    
    // 创建闭包
    LClosure *cl = aqlF_newLclosure(L, 0);
    cl->p = p;
    
    // 设置函数到栈顶
    setclLvalue2s(L, L->top.p, cl);
    L->top.p++;
    
    // 创建调用信息
    struct CallInfo *ci = &L->base_ci;
    ci->func.p = L->top.p - 1;
    ci->top.p = L->top.p + stack_size;
    ci->u.l.savedpc = code;
    ci->callstatus = 0;
    
    // 执行字节码
    printf("🔧 调用 aqlV_execute2...\n");
    aqlV_execute2(L, ci);
    
    // 获取返回值
    if (L->top.p > ci->func.p + 1) {
        TValue *result = s2v(ci->func.p + 1);
        
        if (ttisinteger(result)) {
            snprintf(output, output_size, "%lld", ivalue(result));
        } else if (ttisfloat(result)) {
            snprintf(output, output_size, "%.6g", fltvalue(result));
        } else if (ttisstring(result)) {
            snprintf(output, output_size, "%s", svalue(result));
        } else if (ttisboolean(result)) {
            snprintf(output, output_size, "%s", bvalue(result) ? "true" : "false");
        } else if (ttisnil(result)) {
            snprintf(output, output_size, "nil");
        } else {
            snprintf(output, output_size, "(未知类型)");
        }
    } else {
        snprintf(output, output_size, "(无返回值)");
    }
    
    aql_close(L);
    return 1;
#endif
}

/*
** 运行单个测试
*/
static void run_single_test(const char *test_name, const char *bytecode_dir) {
    char by_file[512], bin_file[512], expected_file[512];
    snprintf(by_file, sizeof(by_file), "%s/%s.by", bytecode_dir, test_name);
    snprintf(bin_file, sizeof(bin_file), "%s/%s.bin", bytecode_dir, test_name);
    snprintf(expected_file, sizeof(expected_file), "%s/%s.expected", bytecode_dir, test_name);
    
    printf("🧪 测试: %s\n", test_name);
    g_stats.total++;
    
    // 检查文件是否存在
    if (access(by_file, F_OK) != 0) {
        printf("⚠️  跳过: 字节码文件不存在 %s\n", by_file);
        g_stats.skipped++;
        return;
    }
    
    // 直接解析文本格式的 .by 文件
    
    // 加载字节码
    Instruction *code;
    void *constants;
    int code_size, const_size, stack_size;
    
    if (!load_bytecode_text_file(by_file, &code, &code_size, &constants, &const_size, &stack_size)) {
        printf("❌ 加载字节码失败\n");
        g_stats.failed++;
        return;
    }
    
    // 执行字节码
    char actual_output[4096];
    if (!execute_bytecode_simple(code, code_size, stack_size, actual_output, sizeof(actual_output))) {
        printf("❌ 执行字节码失败\n");
        free(code);
        if (constants) free(constants);
        g_stats.failed++;
        return;
    }
    
    // 读取期望结果
    char *expected_output = read_expected_file(expected_file);
    if (!expected_output) {
        printf("⚠️  无期望结果文件，显示实际输出:\n%s\n", actual_output);
        g_stats.skipped++;
    } else {
        // 对比结果
        if (strcmp(actual_output, expected_output) == 0) {
            printf("✅ 通过\n");
            g_stats.passed++;
        } else {
            printf("❌ 失败\n");
            printf("期望输出:\n%s\n", expected_output);
            printf("实际输出:\n%s\n", actual_output);
            g_stats.failed++;
        }
        free(expected_output);
    }
    
    free(code);
    if (constants) free(constants);
}

/*
** 扫描并运行所有测试
*/
static void run_all_tests(const char *bytecode_dir) {
    DIR *dir = opendir(bytecode_dir);
    if (!dir) {
        printf("❌ 无法打开目录: %s\n", bytecode_dir);
        return;
    }
    
    printf("🚀 开始 VM 字节码测试...\n");
    printf("📁 测试目录: %s\n\n", bytecode_dir);
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".by")) {
            // 提取测试名称 (去掉 .by 扩展名)
            char test_name[256];
            strncpy(test_name, entry->d_name, sizeof(test_name));
            char *dot = strrchr(test_name, '.');
            if (dot) *dot = '\0';
            
            run_single_test(test_name, bytecode_dir);
            printf("\n");
        }
    }
    
    closedir(dir);
}

/*
** 显示帮助信息
*/
static void show_help() {
    printf("VM 字节码测试工具\n");
    printf("\n用法:\n");
    printf("  vmtest [选项] [测试目录]\n");
    printf("\n选项:\n");
    printf("  -h, --help     显示此帮助\n");
    printf("  -v, --verbose  详细输出\n");
    printf("  -t <name>      运行指定测试\n");
    printf("\n示例:\n");
    printf("  vmtest                    # 运行所有测试\n");
    printf("  vmtest test/vm/bytecode   # 指定测试目录\n");
    printf("  vmtest -t arithmetic      # 运行指定测试\n");
    printf("\n文件格式:\n");
    printf("  *.by       - 字节码文件\n");
    printf("  *.expected - 期望输出文件\n");
}

/*
** 主函数
*/
int main(int argc, char *argv[]) {
    const char *bytecode_dir = "test/vm/bytecode";
    const char *single_test = NULL;
    int verbose = 0;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            single_test = argv[++i];
        } else if (argv[i][0] != '-') {
            bytecode_dir = argv[i];
        }
    }
    
    if (single_test) {
        run_single_test(single_test, bytecode_dir);
    } else {
        run_all_tests(bytecode_dir);
    }
    
    // 显示统计结果
    printf("📊 测试结果统计:\n");
    printf("🔢 总计: %d\n", g_stats.total);
    printf("✅ 通过: %d\n", g_stats.passed);
    printf("❌ 失败: %d\n", g_stats.failed);
    printf("⚠️  跳过: %d\n", g_stats.skipped);
    
    if (g_stats.failed > 0) {
        printf("\n💡 提示: 失败的测试可能需要在 avm_core.c 中实现相应的字节码支持\n");
        return 1;
    } else if (g_stats.passed > 0) {
        printf("\n🎉 所有测试通过！\n");
        return 0;
    } else {
        printf("\n⚠️  没有找到可执行的测试\n");
        return 1;
    }
}
