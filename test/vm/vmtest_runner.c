/*
** VM 字节码测试运行器
** 类似于 make rtest，但专门测试字节码执行
** 调用 aqlm 执行 .by 文件，并对比 .expected 结果
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>

/*
** 测试统计
*/
typedef struct {
    int total;
    int passed;
    int failed;
    int skipped;
} TestStats;

/*
** 颜色输出宏
*/
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"

/*
** 显示帮助信息
*/
static void show_help(void) {
    printf("用法: vmtest <bytecode_dir> <aqlm_path> [选项]\n");
    printf("选项:\n");
    printf("  -h, --help     显示此帮助信息\n");
    printf("  -v, --verbose  显示详细输出\n");
    printf("  -t <test>      只运行指定的测试\n");
    printf("\n");
    printf("示例:\n");
    printf("  vmtest test/vm/bytecode_text bin/aqlm\n");
    printf("  vmtest test/vm/bytecode_text bin/aqlm -v\n");
    printf("  vmtest test/vm/bytecode_text bin/aqlm -t simple\n");
}

/*
** 读取文件内容
*/
static char* read_file(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }
    
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);
    
    // 去掉末尾的换行符
    while (size > 0 && (content[size-1] == '\n' || content[size-1] == '\r')) {
        content[--size] = '\0';
    }
    
    return content;
}

/*
** 执行aqlm并获取输出
*/
static char* run_aqlm(const char *aqlm_path, const char *bytecode_file) {
    char command[1024];
    snprintf(command, sizeof(command), "%s %s 2>/dev/null", aqlm_path, bytecode_file);
    
    FILE *pipe = popen(command, "r");
    if (!pipe) return NULL;
    
    char buffer[1024];
    char *result = malloc(1024);
    result[0] = '\0';
    
    while (fgets(buffer, sizeof(buffer), pipe)) {
        strcat(result, buffer);
    }
    
    int status = pclose(pipe);
    if (status != 0) {
        free(result);
        return NULL;
    }
    
    // 去掉末尾的换行符
    size_t len = strlen(result);
    while (len > 0 && (result[len-1] == '\n' || result[len-1] == '\r')) {
        result[--len] = '\0';
    }
    
    return result;
}

/*
** 递归搜索目录中的测试文件
*/
static void find_tests_recursive(const char *dir_path, char tests[][256], int *test_count, int max_tests) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *test_count < max_tests) {
        if (entry->d_name[0] == '.') continue;  // 跳过隐藏文件
        
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // 递归搜索子目录
                find_tests_recursive(full_path, tests, test_count, max_tests);
            } else if (strstr(entry->d_name, ".by") && 
                      entry->d_name[strlen(entry->d_name) - 3] == '.') {
                // 找到.by文件，保存相对路径和测试名
                char *dot = strrchr(entry->d_name, '.');
                if (dot) {
                    *dot = '\0';  // 去掉.by后缀
                    snprintf(tests[*test_count], 256, "%s/%s", dir_path, entry->d_name);
                    (*test_count)++;
                    *dot = '.';  // 恢复文件名
                }
            }
        }
    }
    closedir(dir);
}

/*
** 运行单个测试（支持相对路径）
*/
static int run_test_with_path(const char *test_path, const char *aqlm_path, int verbose) {
    char bytecode_file[512];
    char expected_file[512];
    
    snprintf(bytecode_file, sizeof(bytecode_file), "%s.by", test_path);
    snprintf(expected_file, sizeof(expected_file), "%s.expected", test_path);
    
    // 提取测试名称用于显示
    const char *test_name = strrchr(test_path, '/');
    test_name = test_name ? test_name + 1 : test_path;
    
    if (verbose) {
        printf("🧪 测试: %s\n", test_name);
        printf("  文件: %s\n", bytecode_file);
    }
    
    // 检查文件是否存在
    if (access(bytecode_file, R_OK) != 0) {
        if (verbose) {
            printf("  ❌ 字节码文件不存在: %s\n", bytecode_file);
        }
        return -1; // 跳过
    }
    
    if (access(expected_file, R_OK) != 0) {
        if (verbose) {
            printf("  ⚠️  期望结果文件不存在: %s\n", expected_file);
        }
        return -1; // 跳过
    }
    
    // 执行字节码
    char *actual = run_aqlm(aqlm_path, bytecode_file);
    if (!actual) {
        if (verbose) {
            printf("  ❌ 执行失败\n");
        }
        return 0; // 失败
    }
    
    // 读取期望结果
    char *expected = read_file(expected_file);
    if (!expected) {
        if (verbose) {
            printf("  ❌ 无法读取期望结果文件\n");
        }
        free(actual);
        return 0; // 失败
    }
    
    // 比较结果
    int success = (strcmp(actual, expected) == 0);
    
    if (verbose) {
        if (success) {
            printf("  ✅ 通过\n");
        } else {
            printf("  ❌ 失败\n");
            printf("    期望: '%s'\n", expected);
            printf("    实际: '%s'\n", actual);
        }
        printf("\n");
    } else {
        if (success) {
            printf(COLOR_GREEN "✅ %s" COLOR_RESET "\n", test_name);
        } else {
            printf(COLOR_RED "❌ %s" COLOR_RESET "\n", test_name);
            printf("    期望: '%s', 实际: '%s'\n", expected, actual);
        }
    }
    
    free(actual);
    free(expected);
    return success ? 1 : 0;
}

/*
** 主函数
*/
int main(int argc, char *argv[]) {
    if (argc < 3) {
        show_help();
        return 1;
    }
    
    const char *bytecode_dir = argv[1];
    const char *aqlm_path = argv[2];
    const char *single_test = NULL;
    int verbose = 0;
    
    // 解析命令行参数
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            single_test = argv[++i];
        }
    }
    
    printf("🚀 开始 VM 字节码测试...\n");
    printf("📁 测试目录: %s\n", bytecode_dir);
    printf("🔧 使用VM: %s\n", aqlm_path);
    printf("\n");
    
    TestStats stats = {0, 0, 0, 0};
    
    if (single_test) {
        // 运行单个测试
        int result = run_test_with_path(single_test, aqlm_path, verbose);
        if (result > 0) {
            stats.passed = 1;
            stats.total = 1;
        } else if (result == 0) {
            stats.failed = 1;
            stats.total = 1;
        } else {
            stats.skipped = 1;
        }
    } else {
        // 递归搜索所有测试
        char tests[1000][256];  // 最多支持1000个测试
        int test_count = 0;
        
        find_tests_recursive(bytecode_dir, tests, &test_count, 1000);
        
        printf("📋 找到 %d 个测试文件\n\n", test_count);
        
        for (int i = 0; i < test_count; i++) {
            int result = run_test_with_path(tests[i], aqlm_path, verbose);
            if (result > 0) {
                stats.passed++;
                stats.total++;
            } else if (result == 0) {
                stats.failed++;
                stats.total++;
            } else {
                stats.skipped++;
            }
        }
    }
    
    // 显示统计结果
    printf("========================================\n");
    printf("📊 测试结果统计:\n");
    printf("   总计: %d\n", stats.total);
    printf("   " COLOR_GREEN "通过: %d" COLOR_RESET "\n", stats.passed);
    printf("   " COLOR_RED "失败: %d" COLOR_RESET "\n", stats.failed);
    if (stats.skipped > 0) {
        printf("   " COLOR_YELLOW "跳过: %d" COLOR_RESET "\n", stats.skipped);
    }
    printf("========================================\n");
    
    if (stats.failed > 0) {
        printf(COLOR_RED "❌ 有测试失败!" COLOR_RESET "\n");
        return 1;
    } else if (stats.total > 0) {
        printf(COLOR_GREEN "✅ 所有测试通过!" COLOR_RESET "\n");
        return 0;
    } else {
        printf(COLOR_YELLOW "⚠️  没有找到可执行的测试!" COLOR_RESET "\n");
        return 0;
    }
}
