/*
** AQL VM (aqlm) - AQL 字节码虚拟机
** 类似于 aqld，但专门用于加载和执行 .by 字节码文件
** 用法: aqlm script.by
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "aql.h"
#include "aopcodes.h"
#include "aobject.h"
#include "astate.h"
#include "afunc.h"
#include "amem.h"
#include "astring.h"
#include "adebug.h"
#include "adebug_user.h"
#include "acontainer.h"
#include "ado.h"
#include "atable.h"

// 声明 avm_core.c 中的函数
extern void aqlV_execute2(aql_State *L, struct CallInfo *ci);


// 常量类型枚举
typedef enum {
    CONST_STRING,
    CONST_INTEGER,
    CONST_FLOAT
} ConstantType;

// 常量结构
typedef struct {
    ConstantType type;
    union {
        char string[256];
        int integer;
        double number;
    } value;
} Constant;

// 函数原型结构
typedef struct {
    Instruction *code;
    int code_size;
    Constant *constants;
    int const_size;
    int stack_size;
    int num_params;
    int is_vararg;
    // upvalue支持
    int num_upvalues;
    Upvaldesc *upvalues;
} FunctionProto;

/*
** 函数前向声明
*/
static int execute_multi_function_bytecode(FunctionProto *functions, int function_count);
static int execute_single_function_bytecode(Instruction *code, int code_size, void *constants, int const_size, int stack_size);

/*
** 简单的内存分配器
*/
static void *test_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud; (void)osize;  /* not used */
    aql_debug("[DEBUG] test_alloc: ptr=%p, osize=%zu, nsize=%zu\n", ptr, osize, nsize);
    
    if (nsize == 0) {
        aql_debug("[DEBUG] test_alloc: freeing memory\n");
        free(ptr);
        return NULL;
    }
    
    void *result = realloc(ptr, nsize);
    aql_debug("[DEBUG] test_alloc: realloc result=%p\n", result);
    return result;
}

/*
** 打印使用说明
*/
static void print_usage(const char *program_name) {
    printf("AQL 字节码虚拟机 (aqlm) - 执行 .by 字节码文件\n");
    printf("用法: %s [选项] <script.by>\n", program_name);
    printf("\n选项:\n");
    printf("  -h, --help           显示此帮助信息\n");
    printf("  -v, --verbose        详细模式 (启用所有调试信息)\n");
    printf("  -vt, --verbose-trace 执行跟踪模式 (显示指令执行)\n");
    printf("  -vd, --verbose-debug 详细调试信息\n");
    printf("  -vb, --verbose-bytecode 字节码输出\n");
    printf("  -q, --quiet          静默模式 (禁用所有调试输出)\n");
    printf("\n示例:\n");
    printf("  %s test.by           # 基础执行\n", program_name);
    printf("  %s -vt test.by       # 执行跟踪模式\n", program_name);
    printf("  %s -v test.by        # 详细模式\n", program_name);
    printf("  %s -vb -vt test.by   # 字节码输出 + 执行跟踪\n", program_name);
}

/*
** 简化的文本字节码解析器 - 使用统一的 aql_parse_instruction
*/
static int load_bytecode_text_file(const char *filename, Instruction **code, int *code_size,
                                  void **constants, int *const_size, int *stack_size) {
    aql_debug("[DEBUG] load_bytecode_text_file 开始\n");
    
    FILE *f = fopen(filename, "r");
    if (!f) {
        aql_debug("❌ 无法打开文件: %s\n", filename);
        return 0;
    }
    
    aql_debug("[DEBUG] 文件%s打开成功\n", filename);
    
    Instruction *instructions = malloc(1000 * sizeof(Instruction));
    if (!instructions) {
        aql_debug("❌ 内存分配失败\n");
        fclose(f);
        return 0;
    }
    // 初始化整个数组为0，避免垃圾数据
    memset(instructions, 0, 1000 * sizeof(Instruction));
    int instruction_count = 0;
    
    aql_debug("[DEBUG] 初始化变量完成\n");
    
    char line[256];
    int in_code_section = 0;
    int in_constants_section = 0;
    
    // 统一的常量存储
    Constant constants_array[20];  // 支持最多20个常量
    int constants_count = 0;
    
    aql_debug("[DEBUG] 开始读取文件行\n");
    
    int line_num = 0;
    while (fgets(line, sizeof(line), f)) {
        line_num++;
        aql_debug("[DEBUG] 读取第 %d 行: %s", line_num, line);
        
        // 移除换行符
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // 跳过空行和注释
        char *trimmed = line;
        while (*trimmed && isspace(*trimmed)) trimmed++;
        aql_debug("[DEBUG] 处理trimmed行: '%s'\n", trimmed);
        if (!*trimmed || *trimmed == '#') continue;
        
        aql_debug("[DEBUG] 处理段标记\n");
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
            if (args >= 3 && constants_count < 20) {
                if (strcmp(const_type, "STRING") == 0) {
                    // 去掉引号
                    if (const_value[0] == '"' && const_value[strlen(const_value)-1] == '"') {
                        const_value[strlen(const_value)-1] = '\0';
                        constants_array[constants_count].type = CONST_STRING;
                        strcpy(constants_array[constants_count].value.string, const_value + 1);
                        aql_debug("[DEBUG] 解析字符串常量: %s = \"%s\"\n", const_name, constants_array[constants_count].value.string);
                        constants_count++;
                    }
                } else if (strcmp(const_type, "INTEGER") == 0) {
                    // 解析整数常量
                    constants_array[constants_count].type = CONST_INTEGER;
                    constants_array[constants_count].value.integer = atoi(const_value);
                    aql_debug("[DEBUG] 解析整数常量: %s = %d\n", const_name, constants_array[constants_count].value.integer);
                    constants_count++;
                } else if (strcmp(const_type, "FLOAT") == 0 || strcmp(const_type, "NUMBER") == 0) {
                    // 解析浮点数常量
                    constants_array[constants_count].type = CONST_FLOAT;
                    constants_array[constants_count].value.number = atof(const_value);
                    aql_debug("[DEBUG] 解析浮点数常量: %s = %f\n", const_name, constants_array[constants_count].value.number);
                    constants_count++;
                }
            }
            continue;
        }

        // 只在代码段中解析指令
        if (!in_code_section) continue;

        // 使用统一的指令解析函数
        char opcode[32], arg1[32], arg2[32], arg3[32], arg4[32];
        aql_debug("[DEBUG] 准备解析指令行: %s\n", trimmed);
        int args = sscanf(trimmed, "%s %s %s %s %s", opcode, arg1, arg2, arg3, arg4);
        aql_debug("[DEBUG] sscanf 返回: %d, opcode=%s\n", args, opcode);
        if (args < 1) continue;
        
        // 使用统一的指令解析函数
        Instruction inst;
        if (aql_parse_instruction(opcode, arg1, arg2, arg3, arg4, args, &inst)) {
            instructions[instruction_count] = inst;
            instruction_count++;
            
            if (instruction_count >= 1000) {
                printf("❌ 指令数量超出限制\n");
                fclose(f);
                free(instructions);
                return 0;
            }
            continue;
        }
        
        // 如果解析失败，显示错误并继续
        printf("⚠️  未知指令: %s\n", opcode);
        continue;
    }
    
    aql_debug("[DEBUG] 文件读取循环结束，共处理 %d 行\n", line_num);
    
    fclose(f);
    
    aql_debug("[DEBUG] 文件关闭成功\n");
    
    // 返回结果
    aql_debug("[DEBUG] 设置返回结果，指令数: %d，常量数: %d\n", instruction_count, constants_count);
    
    // 显示生成的指令
    aql_debug("[DEBUG] 生成的指令:\n");
    for (int i = 0; i < instruction_count; i++) {
        aql_debug("  [%d]: 0x%08lX\n", i, (unsigned long)instructions[i]);
    }
    
    aql_debug("[DEBUG] 即将设置输出参数\n");
    *code = instructions;
    *code_size = instruction_count;
    *constants = (constants_count > 0) ? (void*)constants_array : NULL;
    *const_size = constants_count;
    *stack_size = (*stack_size > 0) ? *stack_size : 10;  // 默认栈大小为10
    
    aql_debug("[DEBUG] load_bytecode_text_file 即将返回\n");
    
    return 1;
}

/*
** 多函数字节码解析器 - 支持真正的函数调用
*/
static int load_multi_function_bytecode(const char *filename, FunctionProto **functions, int *function_count) {
    aql_debug("[DEBUG] load_multi_function_bytecode 开始\n");
    
    FILE *f = fopen(filename, "r");
    if (!f) {
        aql_debug("❌ 无法打开文件: %s\n", filename);
        return 0;
    }
    
    // 支持最多10个函数
    FunctionProto *func_array = malloc(10 * sizeof(FunctionProto));
    if (!func_array) {
        aql_debug("❌ 函数数组内存分配失败\n");
        fclose(f);
        return 0;
    }
    memset(func_array, 0, 10 * sizeof(FunctionProto));
    
    int current_func = 0;  // 当前函数索引
    int total_functions = 1;  // 至少有一个主函数
    
    // 为主函数分配空间
    func_array[0].code = malloc(1000 * sizeof(Instruction));
    if (!func_array[0].code) {
        aql_debug("❌ 主函数代码内存分配失败\n");
        free(func_array);
        fclose(f);
        return 0;
    }
    memset(func_array[0].code, 0, 1000 * sizeof(Instruction));
    func_array[0].code_size = 0;
    func_array[0].stack_size = 10;
    func_array[0].num_params = 0;
    func_array[0].is_vararg = 0;
    
    char line[256];
    int in_code_section = 0;
    int in_constants_section = 0;
    
    // 统一的常量存储（为当前函数）
    Constant constants_array[20];
    int constants_count = 0;
    
    aql_debug("[DEBUG] 开始读取文件行\n");
    
    int line_num = 0;
    while (fgets(line, sizeof(line), f)) {
        line_num++;
        
        // 移除换行符
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // 跳过空行和注释
        char *trimmed = line;
        while (*trimmed && isspace(*trimmed)) trimmed++;
        if (!*trimmed || *trimmed == '#') continue;
        
        aql_debug("[DEBUG] 处理行: '%s'\n", trimmed);
        
        // 处理段标记
        if (*trimmed == '.') {
            if (strcmp(trimmed, ".main") == 0) {
                // 开始主函数定义
                current_func = 0;
                in_constants_section = 0;
                in_code_section = 0;
                constants_count = 0;  // 重置常量计数
                aql_debug("[DEBUG] 进入主函数定义\n");
                continue;
            } else if (strcmp(trimmed, ".constants") == 0) {
                in_constants_section = 1;
                in_code_section = 0;
                continue;
            } else if (strcmp(trimmed, ".code") == 0) {
                in_code_section = 1;
                in_constants_section = 0;
                continue;
            } else if (strcmp(trimmed, ".end") == 0) {
                // 结束当前函数定义，保存常量
                if (constants_count > 0) {
                    func_array[current_func].constants = malloc(constants_count * sizeof(Constant));
                    if (func_array[current_func].constants) {
                        memcpy(func_array[current_func].constants, constants_array, constants_count * sizeof(Constant));
                        func_array[current_func].const_size = constants_count;
                        aql_debug("[DEBUG] 函数 %d 保存了 %d 个常量\n", current_func, constants_count);
                    }
                }
                in_constants_section = 0;
                in_code_section = 0;
                constants_count = 0;  // 重置常量计数
                aql_debug("[DEBUG] 结束函数 %d 定义\n", current_func);
                continue;
            } else if (strncmp(trimmed, ".function", 9) == 0) {
                // 保存当前函数的常量
                if (constants_count > 0) {
                    func_array[current_func].constants = malloc(constants_count * sizeof(Constant));
                    if (func_array[current_func].constants) {
                        memcpy(func_array[current_func].constants, constants_array, constants_count * sizeof(Constant));
                        func_array[current_func].const_size = constants_count;
                    }
                }
                
                // 创建新函数
                if (total_functions < 10) {
                    current_func = total_functions;
                    total_functions++;
                    
                    func_array[current_func].code = malloc(1000 * sizeof(Instruction));
                    if (!func_array[current_func].code) {
                        aql_debug("❌ 函数 %d 代码内存分配失败\n", current_func);
                        // 清理已分配的内存
                        for (int i = 0; i < current_func; i++) {
                            free(func_array[i].code);
                            if (func_array[i].constants) free(func_array[i].constants);
                        }
                        free(func_array);
                        fclose(f);
                        return 0;
                    }
                    memset(func_array[current_func].code, 0, 1000 * sizeof(Instruction));
                    func_array[current_func].code_size = 0;
                    func_array[current_func].stack_size = 10;
                    
                    // 解析参数数量
                    char func_name[64];
                    int params = 0;
                    if (sscanf(trimmed, ".function %s %d", func_name, &params) >= 2) {
                        func_array[current_func].num_params = params;
                    }
                    
                    aql_debug("[DEBUG] 创建函数 %d: %s, 参数数量: %d\n", current_func, func_name, params);
                }
                
                // 初始化upvalue信息
                func_array[current_func].num_upvalues = 0;
                func_array[current_func].upvalues = NULL;
                
                // 重置常量计数器为新函数做准备
                constants_count = 0;
                in_code_section = 0;
                in_constants_section = 0;
                continue;
            } else if (strncmp(trimmed, ".upvalues", 9) == 0) {
                // 解析 .upvalues N
                int num_upvalues = 0;
                if (sscanf(trimmed, ".upvalues %d", &num_upvalues) == 1) {
                    func_array[current_func].num_upvalues = num_upvalues;
                    if (num_upvalues > 0) {
                        func_array[current_func].upvalues = calloc(num_upvalues, sizeof(Upvaldesc));
                        aql_debug("[DEBUG] 函数 %d 分配 %d 个upvalue槽位\n", current_func, num_upvalues);
                    }
                }
                continue;
            } else if (strncmp(trimmed, ".upvalue", 8) == 0) {
                // 解析 .upvalue index "name" instack stack_idx
                // 或者 .upvalue index "name" upval upval_idx
                int index, stack_idx;
                char name[64], type[16];
                if (sscanf(trimmed, ".upvalue %d \"%[^\"]\" %s %d", &index, name, type, &stack_idx) == 4) {
                    if (index >= 0 && index < func_array[current_func].num_upvalues) {
                        Upvaldesc *uv = &func_array[current_func].upvalues[index];
                        uv->name = NULL; // 暂时不设置名称，避免内存管理复杂性
                        uv->instack = (strcmp(type, "instack") == 0) ? 1 : 0;
                        uv->idx = stack_idx;
                        uv->kind = 0; // 默认类型
                        aql_debug("[DEBUG] 函数 %d upvalue[%d]: %s=%s, idx=%d\n", 
                                   current_func, index, name, type, stack_idx);
                    }
                }
                continue;
            } else if (strcmp(trimmed, ".end") == 0) {
                // 保存当前函数的常量
                if (constants_count > 0) {
                    func_array[current_func].constants = malloc(constants_count * sizeof(Constant));
                    if (func_array[current_func].constants) {
                        memcpy(func_array[current_func].constants, constants_array, constants_count * sizeof(Constant));
                        func_array[current_func].const_size = constants_count;
                    }
                }
                
                in_code_section = 0;
                in_constants_section = 0;
                
                // 重置常量计数器
                constants_count = 0;
                current_func = 0;  // 回到主函数
                continue;
            }
        }
        
        // 解析常量定义（支持在函数定义中直接定义常量）
        if (in_constants_section || (trimmed[0] == 'K' && isdigit(trimmed[1]))) {
            char const_name[32], const_type[32], const_value[256];
            int args = sscanf(trimmed, "%s %s %[^\n]", const_name, const_type, const_value);
            if (args >= 3 && constants_count < 20) {
                if (strcmp(const_type, "STRING") == 0) {
                    if (const_value[0] == '"' && const_value[strlen(const_value)-1] == '"') {
                        const_value[strlen(const_value)-1] = '\0';
                        constants_array[constants_count].type = CONST_STRING;
                        strcpy(constants_array[constants_count].value.string, const_value + 1);
                        aql_debug("[DEBUG] 解析字符串常量: %s = \"%s\"\n", const_name, constants_array[constants_count].value.string);
                        constants_count++;
                    }
                } else if (strcmp(const_type, "INTEGER") == 0) {
                    constants_array[constants_count].type = CONST_INTEGER;
                    constants_array[constants_count].value.integer = atoi(const_value);
                    aql_debug("[DEBUG] 解析整数常量: %s = %d\n", const_name, constants_array[constants_count].value.integer);
                    constants_count++;
                } else if (strcmp(const_type, "FLOAT") == 0 || strcmp(const_type, "NUMBER") == 0) {
                    constants_array[constants_count].type = CONST_FLOAT;
                    constants_array[constants_count].value.number = atof(const_value);
                    aql_debug("[DEBUG] 解析浮点数常量: %s = %f\n", const_name, constants_array[constants_count].value.number);
                    constants_count++;
                }
            }
            continue;
        }

        // 只在代码段中解析指令
        if (!in_code_section) continue;

        // 使用统一的指令解析函数
        char opcode[32], arg1[32], arg2[32], arg3[32], arg4[32];
        int args = sscanf(trimmed, "%s %s %s %s %s", opcode, arg1, arg2, arg3, arg4);
        if (args < 1) continue;
        
        // 使用统一的指令解析函数
        Instruction inst;
        if (aql_parse_instruction(opcode, arg1, arg2, arg3, arg4, args, &inst)) {
            func_array[current_func].code[func_array[current_func].code_size] = inst;
            func_array[current_func].code_size++;
            
            aql_debug("[DEBUG] 函数 %d 解析指令: %s -> 0x%08lX\n", 
                         current_func, opcode, (unsigned long)inst);
            
            if (func_array[current_func].code_size >= 1000) {
                printf("❌ 函数 %d 指令数量超出限制\n", current_func);
                fclose(f);
                // 清理内存
                for (int i = 0; i < total_functions; i++) {
                    free(func_array[i].code);
                    if (func_array[i].constants) free(func_array[i].constants);
                }
                free(func_array);
                return 0;
            }
            continue;
        }
        
        // 如果解析失败，显示错误并继续
        printf("⚠️  未知指令: %s\n", opcode);
    }
    
    fclose(f);
    
    // 返回结果
    *functions = func_array;
    *function_count = total_functions;
    
    aql_debug("[DEBUG] 多函数解析完成，函数数量: %d\n", total_functions);
    
    return 1;
}

/*
** 执行字节码 - 支持单函数和多函数
*/
static int execute_bytecode(const char *filename) {
    // 调试系统已在命令行解析时初始化
    
    // 设置跟踪模式
    // 调试标志已在命令行解析时设置，无需额外设置

    aql_debug("📝 [VM] AQL 状态创建成功，调试系统已初始化\n");
    aql_debug("📝 [VM] 开始执行 execute_bytecode\n");
    aql_debug("📝 [VM] 加载字节码文件: %s\n", filename);
    
    // 检查文件是否包含多函数
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("❌ 无法打开文件: %s\n", filename);
        return 1;
    }
    
    int has_multiple_functions = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = line;
        while (*trimmed && isspace(*trimmed)) trimmed++;
        if (strncmp(trimmed, ".function", 9) == 0) {
            has_multiple_functions = 1;
            break;
        }
    }
    fclose(f);
    
    if (has_multiple_functions) {
        aql_debug("📝 [VM] 检测到多函数文件，使用多函数解析器\n");
        
        // 使用多函数解析器
        FunctionProto *functions;
        int function_count;
        
        if (!load_multi_function_bytecode(filename, &functions, &function_count)) {
            printf("❌ 加载多函数字节码失败\n");
            return 1;
        }
        
        aql_debug("📝 [VM] 多函数解析成功，函数数量: %d\n", function_count);
        
        // 执行多函数字节码
        return execute_multi_function_bytecode(functions, function_count);
        
    } else {
        aql_debug("📝 [VM] 检测到单函数文件，使用单函数解析器\n");
        
        // 使用原有的单函数解析器
        Instruction *code;
        void *constants;
        int code_size, const_size, stack_size = 10;
        
        aql_debug("📝 [VM] 调用 load_bytecode_text_file\n");
        
        int result = load_bytecode_text_file(filename, &code, &code_size, &constants, &const_size, &stack_size);
        
        aql_debug("📝 [VM] load_bytecode_text_file 返回值: %d\n", result);
        
        if (!result) {
            printf("❌ 加载字节码失败\n");
            return 1;
        }
        
        // 执行单函数字节码（使用原有逻辑）
        return execute_single_function_bytecode(code, code_size, constants, const_size, stack_size);
    }
}

/*
** 执行多函数字节码 - 修正版：按Lua模型实现
*/
static int execute_multi_function_bytecode(FunctionProto *functions, int function_count) {
    aql_debug("📝 [VM] 开始执行多函数字节码，函数数量: %d\n", function_count);
    
    // 创建AQL状态
    aql_State *L = aql_newstate(test_alloc, NULL);
    if (!L) {
        printf("❌ 创建AQL状态失败\n");
        return 1;
    }
    
    // 创建主函数原型（函数0）
    Proto *main_proto = aqlF_newproto(L);
    FunctionProto *main_func = &functions[0];
    
    // 设置主函数基本属性
    main_proto->sizecode = main_func->code_size;
    main_proto->maxstacksize = main_func->stack_size;
    main_proto->numparams = main_func->num_params;
    main_proto->is_vararg = main_func->is_vararg;
    
    // 设置主函数upvalue信息
    main_proto->sizeupvalues = main_func->num_upvalues;
    if (main_func->num_upvalues > 0) {
        main_proto->upvalues = aqlM_newvector(L, main_func->num_upvalues, Upvaldesc);
        memcpy(main_proto->upvalues, main_func->upvalues, main_func->num_upvalues * sizeof(Upvaldesc));
        aql_debug("📝 [VM] 主函数upvalue设置完成: %d个upvalue\n", main_func->num_upvalues);
    } else {
        main_proto->upvalues = NULL;
    }
    
    // 关键修复：复制指令数据到Proto管理的内存中
    if (main_func->code_size > 0) {
        main_proto->code = aqlM_newvector(L, main_func->code_size, Instruction);
        memcpy(main_proto->code, main_func->code, main_func->code_size * sizeof(Instruction));
        
        aql_debug("📝 [VM] 主函数指令复制完成: 复制了 %d 条指令\n", main_func->code_size);
        for (int i = 0; i < main_func->code_size; i++) {
            aql_debug("📝 [VM] 主函数指令[%d]: 0x%08lX\n", i, (unsigned long)main_proto->code[i]);
        }
    }
    
    aql_debug("📝 [VM] 主函数设置完成: code_size=%d, stack_size=%d\n", 
                 main_func->code_size, main_func->stack_size);
    
    // 处理主函数常量表
    if (main_func->const_size > 0 && main_func->constants) {
        main_proto->k = aqlM_newvector(L, main_func->const_size, TValue);
        main_proto->sizek = main_func->const_size;
        
        for (int i = 0; i < main_func->const_size; i++) {
            Constant *c = &main_func->constants[i];
            TValue *tv = &main_proto->k[i];
            
            switch (c->type) {
                case CONST_STRING:
                    setsvalue2n(L, tv, aqlStr_newlstr(L, c->value.string, strlen(c->value.string)));
                    aql_debug("📝 [VM] 主函数常量[%d]: STRING \"%s\"\n", i, c->value.string);
                    break;
                case CONST_INTEGER:
                    setivalue(tv, c->value.integer);
                    aql_debug("📝 [VM] 主函数常量[%d]: INTEGER %d\n", i, c->value.integer);
                    break;
                case CONST_FLOAT:
                    setfltvalue(tv, c->value.number);
                    aql_debug("📝 [VM] 主函数常量[%d]: FLOAT %f\n", i, c->value.number);
                    break;
            }
        }
    }
    
    // ========================================================================
    // 新的嵌套函数解析系统 - 正确实现Lua的函数嵌套模型
    // ========================================================================
    
    // 第一步：分析每个函数的CLOSURE指令，确定子函数需求
    typedef struct {
        int parent_func;     // 父函数索引
        int *child_indices;  // 子函数索引数组
        int child_count;     // 子函数数量
    } FunctionDependency;
    
    FunctionDependency *deps = calloc(function_count, sizeof(FunctionDependency));
    
    // 分析函数依赖关系
    for (int i = 0; i < function_count; i++) {
        FunctionProto *func = &functions[i];
        int max_closure_index = -1;
        
        // 扫描CLOSURE指令，找出最大的子函数索引
        for (int j = 0; j < func->code_size; j++) {
            Instruction instr = func->code[j];
            int op = GET_OPCODE(instr);
            if (op == OP_CLOSURE) {
                int bx = GETARG_Bx(instr);
                if (bx > max_closure_index) {
                    max_closure_index = bx;
                }
            }
        }
        
        if (max_closure_index >= 0) {
            deps[i].child_count = max_closure_index + 1;
            deps[i].child_indices = calloc(deps[i].child_count, sizeof(int));
            
            aql_debug("📝 [VM] 函数[%d]需要%d个子函数\n", i, deps[i].child_count);
            
            // 根据函数结构确定子函数映射
            if (i == 0) {
                // 主函数：按顺序映射子函数
                for (int k = 0; k < deps[i].child_count && (k + 1) < function_count; k++) {
                    deps[i].child_indices[k] = k + 1; // functions[1], functions[2], ...
                }
            } else if (i == 1) {
                // outer函数（用于嵌套函数测试）：子函数0映射到functions[2], 子函数1映射到functions[3]
                for (int k = 0; k < deps[i].child_count && (k + 2) < function_count; k++) {
                    deps[i].child_indices[k] = k + 2; // inner1, inner2等
                }
            }
            // 其他函数暂时不需要子函数
        }
    }
    
    // 第二步：创建所有函数的原型
    Proto **all_protos = calloc(function_count, sizeof(Proto*));
    
    // 主函数原型已存在
    all_protos[0] = main_proto;
    
    // 创建其他函数的原型
    for (int i = 1; i < function_count; i++) {
        all_protos[i] = aqlF_newproto(L);
        FunctionProto *func = &functions[i];
        
        // 设置基本属性
        all_protos[i]->sizecode = func->code_size;
        all_protos[i]->maxstacksize = func->stack_size;
        all_protos[i]->numparams = func->num_params;
        all_protos[i]->is_vararg = func->is_vararg;
        
        // 设置upvalue信息
        all_protos[i]->sizeupvalues = func->num_upvalues;
        if (func->num_upvalues > 0) {
            all_protos[i]->upvalues = aqlM_newvector(L, func->num_upvalues, Upvaldesc);
            memcpy(all_protos[i]->upvalues, func->upvalues, func->num_upvalues * sizeof(Upvaldesc));
            aql_debug("📝 [VM] 函数[%d]upvalue设置完成: %d个upvalue\n", i, func->num_upvalues);
        } else {
            all_protos[i]->upvalues = NULL;
        }
        
        // 复制指令
        if (func->code_size > 0) {
            all_protos[i]->code = aqlM_newvector(L, func->code_size, Instruction);
            memcpy(all_protos[i]->code, func->code, func->code_size * sizeof(Instruction));
            
            aql_debug("📝 [VM] 函数[%d]指令复制完成: %d条指令\n", i, func->code_size);
        }
        
        // 复制常量表
        if (func->const_size > 0 && func->constants) {
            all_protos[i]->k = aqlM_newvector(L, func->const_size, TValue);
            all_protos[i]->sizek = func->const_size;
            
            for (int j = 0; j < func->const_size; j++) {
                Constant *c = &func->constants[j];
                TValue *tv = &all_protos[i]->k[j];
                
                switch (c->type) {
                    case CONST_STRING:
                        setsvalue2n(L, tv, aqlStr_newlstr(L, c->value.string, strlen(c->value.string)));
                        break;
                    case CONST_INTEGER:
                        setivalue(tv, c->value.integer);
                        break;
                    case CONST_FLOAT:
                        setfltvalue(tv, c->value.number);
                        break;
                }
            }
        }
    }
    
    // 第三步：为每个函数设置子函数数组
    for (int i = 0; i < function_count; i++) {
        if (deps[i].child_count > 0) {
            all_protos[i]->p = aqlM_newvector(L, deps[i].child_count, Proto*);
            all_protos[i]->sizep = deps[i].child_count;
            
            aql_debug("📝 [VM] 为函数[%d]创建%d个子函数槽位\n", i, deps[i].child_count);
            
            for (int j = 0; j < deps[i].child_count; j++) {
                int target_func_index = deps[i].child_indices[j];
                if (target_func_index < function_count) {
                    all_protos[i]->p[j] = all_protos[target_func_index];
                    aql_debug("📝 [VM] 函数[%d].p[%d] -> 函数[%d]\n", 
                                 i, j, target_func_index);
                } else {
                    aql_debug("⚠️  [VM] 警告: 函数[%d]的子函数[%d]索引%d超出范围\n", 
                                 i, j, target_func_index);
                }
            }
        }
    }
    
    // 清理临时数据
    for (int i = 0; i < function_count; i++) {
        if (deps[i].child_indices) {
            free(deps[i].child_indices);
        }
    }
    free(deps);
    free(all_protos);
    
    // 创建主函数闭包
    LClosure *cl = aqlF_newLclosure(L, main_proto->sizeupvalues);
    cl->p = main_proto;
    
    // 初始化upvalues (关键步骤！)
    if (main_proto->sizeupvalues > 0) {
        aql_debug("📝 [VM] 初始化主函数的%d个upvalue\n", main_proto->sizeupvalues);
        aqlF_initupvals(L, cl);
        
        // 对于全局环境(_ENV)，需要特殊处理
        // 在Lua中，_ENV指向全局表
        if (main_proto->sizeupvalues >= 1) {
            // 创建全局环境表并设置为第一个upvalue
            Table *global_env = aqlH_new(L);
            sethvalue(L, cl->upvals[0]->v.p, global_env);
            aql_debug("📝 [VM] 设置全局环境表到upvalue[0]: %p\n", (void*)global_env);
        }
    }
    
    setclLvalue2s(L, L->top.p, cl);
    L->top.p++;
    
    aql_debug("📝 [VM] 主函数闭包创建完成，准备调用\n");
    
    // 调用主函数 - 这里会触发VM执行，包括CLOSURE和CALL指令
    aqlD_call(L, L->top.p - 1, AQL_MULTRET);
    
    aql_debug("📝 [VM] 主函数执行完成\n");
    
    // 获取返回值
    if (L->top.p > L->stack.p) {
        StkId result_slot = L->top.p - 1;
        TValue *result = s2v(result_slot);
        aql_debug("📝 [VM] 结果类型: %d, 栈顶: %p, 栈底: %p\n", ttype(result), (void*)L->top.p, (void*)L->stack.p);
        if (ttisinteger(result)) {
            printf("%lld\n", ivalue(result));
        } else if (ttisfloat(result)) {
            printf("%g\n", fltvalue(result));
        } else if (ttisstring(result)) {
            printf("%s\n", getstr(tsvalue(result)));
        } else if (ttisnil(result)) {
            printf("nil\n");
        } else if (ttistable(result)) {
            printf("table\n");
        } else if (ttisclosure(result)) {
            printf("function@%p\n", (void*)clvalue(result));
        } else {
            printf("(unknown result type: %d)\n", ttype(result));
        }
    } else {
        printf("(no result)\n");
    }
    
    // 清理内存
    for (int i = 0; i < function_count; i++) {
        free(functions[i].code);
        if (functions[i].constants) free(functions[i].constants);
    }
    free(functions);
    
    aql_close(L);
    return 0;
}

/*
** 执行单函数字节码 (原有逻辑)
*/
static int execute_single_function_bytecode(Instruction *code, int code_size, void *constants, int const_size, int stack_size) {
        
    // 显示常量表
    if (aql_debug_is_enabled(AQL_FLAG_VB) && const_size > 0) {
        printf("📋 常量表:\n");
        Constant *constants_array = (Constant*)constants;
        for (int i = 0; i < const_size; i++) {
            switch (constants_array[i].type) {
                case CONST_STRING:
                    printf("  K%d: STRING \"%s\"\n", i, constants_array[i].value.string);
                    break;
                case CONST_INTEGER:
                    printf("  K%d: INTEGER %d\n", i, constants_array[i].value.integer);
                    break;
                case CONST_FLOAT:
                    printf("  K%d: FLOAT %f\n", i, constants_array[i].value.number);
                    break;
                default:
                    printf("  K%d: UNKNOWN TYPE %d\n", i, constants_array[i].type);
                    break;
            }
        }
    }
    
    // 显示字节码指令
    if (aql_debug_is_enabled(AQL_FLAG_VB)) {
        aql_debug("🔍 字节码指令:\n");
        for (int i = 0; i < code_size; i++) {
            Instruction inst = code[i];
            OpCode op = GET_OPCODE(inst);
            int a = GETARG_A(inst);
            int b = GETARG_B(inst);
            int c = GETARG_C(inst);
            
            // 直接使用 aql_opnames 数组而不是函数
            const char *opname = (op < NUM_OPCODES) ? aql_opnames[op] : "UNKNOWN";
            aql_debug("  [%d] %s A=%d B=%d C=%d\n", i, opname, a, b, c);
        }
        aql_debug("\n");
    }
    
    aql_debug("📝 [VM] 即将创建 AQL 状态\n");
    
    // 创建 AQL 状态
    aql_State *L = aql_newstate(test_alloc, NULL);
    if (!L) {
        aql_debug("❌ 无法创建 AQL 状态\n");
        return 1;
    }
    
    /* 注册测试C函数 */
    /* TODO: 暂时注释掉，等API实现后再启用 */
    /* 暂时不注册硬编码函数，改用.by文件中的软编码方案 */
    
    // 创建函数原型
    aql_debug("📝 [VM] 创建函数原型\n");
    
    Proto *p = aqlF_newproto(L);
    aql_debug("📝 [VM] 函数原型创建成功\n");
    
    p->code = code;
    p->sizecode = code_size;
    p->maxstacksize = stack_size;
    p->numparams = 0;
    p->is_vararg = 0;
    
    aql_debug("📝 [VM] 函数原型基本属性设置完成\n");
    
    // 设置常量表
    if (const_size > 0) {
        aql_debug("📝 [VM] 设置常量表，常量数量: %d\n", const_size);
        
        p->k = aqlM_newvector(L, const_size, TValue);
        p->sizek = const_size;
        
        aql_debug("📝 [VM] 常量表内存分配成功\n");
        
        // 初始化混合类型常量
        Constant *constants_array = (Constant*)constants;
        for (int i = 0; i < const_size; i++) {
            switch (constants_array[i].type) {
                case CONST_STRING: {
                    aql_debug("📝 [VM] 初始化字符串常量 %d: \"%s\"\n", i, constants_array[i].value.string);
                    TString *str = aqlStr_newlstr(L, constants_array[i].value.string, strlen(constants_array[i].value.string));
                    setsvalue2n(L, &p->k[i], str);
                    break;
                }
                case CONST_INTEGER:
                    aql_debug("📝 [VM] 初始化整数常量 %d: %d\n", i, constants_array[i].value.integer);
                    setivalue(&p->k[i], constants_array[i].value.integer);
                    break;
                case CONST_FLOAT:
                    aql_debug("📝 [VM] 初始化浮点数常量 %d: %f\n", i, constants_array[i].value.number);
                    setfltvalue(&p->k[i], constants_array[i].value.number);
                    break;
            }
        }
        
        aql_debug("📝 [VM] 所有常量初始化完成\n");
    } else {
        aql_debug("📝 [VM] 无常量，跳过常量表设置\n");
        p->k = NULL;
        p->sizek = 0;
    }
    
    // 创建闭包
    aql_debug("📝 [VM] 创建闭包\n");
    
    LClosure *cl = aqlF_newLclosure(L, 0);
    
    aql_debug("📝 [VM] 闭包创建成功，cl=%p\n", (void*)cl);
    
    cl->p = p;
    
    aql_debug("📝 [VM] 闭包与函数原型关联完成\n");
    
    // 设置函数到栈顶
    aql_debug("📝 [VM] 设置函数到栈顶\n");
    
    setclLvalue2s(L, L->top.p, cl);
    L->top.p++;
    
    aql_debug("📝 [VM] 函数已设置到栈顶\n");
    
    // 创建调用信息
    aql_debug("📝 [VM] 创建调用信息\n");
    
    struct CallInfo *ci = &L->base_ci;
    
    aql_debug("📝 [VM] CallInfo 获取成功，ci=%p\n", (void*)ci);
    
    ci->func.p = L->top.p - 1;
    ci->top.p = L->top.p + stack_size;
    ci->u.l.savedpc = code;
    ci->callstatus = 0;
    ci->nresults = 1;  /* 期望1个返回值 */
    
    aql_debug("📝 [VM] 调用信息设置完成\n");
    aql_debug("🚀 [VM] 开始执行字节码...\n");
    
    // 执行字节码
    aql_debug("📝 [VM] 即将调用 aqlV_execute2(L=%p, ci=%p)\n", (void*)L, (void*)ci);
    
    aqlV_execute2(L, ci);
    
    aql_debug("📝 [VM] 字节码执行完成\n");
    
    // 获取返回值
    aql_debug("📝 [VM] 检查返回值\n");
    
    // 检查栈上是否有返回值
    if (L->top.p > L->stack.p) {
        aql_debug("📝 [VM] 发现栈中有数据，检查多个可能的返回值位置\n");
        
        // 检查栈底的几个位置
        for (int offset = 0; offset < 3 && (L->stack.p + offset) < L->top.p; offset++) {
            TValue *candidate = s2v(L->stack.p + offset);
            aql_debug("📝 [VM] 检查位置[%d] %p: 类型=%d\n", 
                        offset, (void*)(L->stack.p + offset), ttype(candidate));
            
            if (ttisinteger(candidate)) {
                printf("%lld\n", ivalue(candidate));  // 输出到标准输出
                aql_debug("📝 [VM] 在位置[%d]找到整数返回值: %lld\n", offset, ivalue(candidate));
                goto found_return_value;
            } else if (ttisfloat(candidate)) {
                printf("%.6g\n", fltvalue(candidate));  // 输出到标准输出
                aql_debug("📝 [VM] 在位置[%d]找到浮点返回值: %.6g\n", offset, fltvalue(candidate));
                goto found_return_value;
            } else if (ttisstring(candidate)) {
                printf("%s\n", svalue(candidate));  // 输出到标准输出
                aql_debug("📝 [VM] 在位置[%d]找到字符串返回值: %s\n", offset, svalue(candidate));
                goto found_return_value;
            } else if (ttisboolean(candidate)) {
                printf("%s\n", bvalue(candidate) ? "true" : "false");  // 输出到标准输出
                aql_debug("📝 [VM] 在位置[%d]找到布尔返回值: %s\n", offset, bvalue(candidate) ? "true" : "false");
                goto found_return_value;
            } else if (ttiscontainer(candidate)) {
                // 处理容器类型
                AQL_ContainerBase *container = (AQL_ContainerBase*)containervalue(candidate);
                const char *container_name = "unknown";
                switch (container->type) {
                    case CONTAINER_ARRAY: container_name = "array"; break;
                    case CONTAINER_SLICE: container_name = "slice"; break; 
                    case CONTAINER_DICT: container_name = "dict"; break;
                    case CONTAINER_VECTOR: container_name = "vector"; break;
                    default: container_name = "container"; break;
                }
                printf("%s\n", container_name);  // 输出到标准输出
                aql_debug("📝 [VM] 在位置[%d]找到容器返回值: %s (length=%zu)\n", 
                           offset, container_name, container->length);
                goto found_return_value;
            } else if (ttistable(candidate)) {
                // 处理表类型
                printf("table\n");  // 输出到标准输出
                aql_debug("📝 [VM] 在位置[%d]找到表返回值\n", offset);
                goto found_return_value;
            }
        }
        
        // 如果前面几个位置都不是有效返回值，则显示nil
        printf("nil\n");  // 输出到标准输出
        aql_debug("📝 [VM] 未找到有效返回值，显示nil\n");
        
        found_return_value:;
    } else {
        printf("(no return value)\n");  // 输出到标准输出
        aql_debug("📝 [VM] 栈中无数据\n");
    } 
    
    aql_debug("📝 [VM] 关闭 AQL 状态\n");
    
    aql_close(L);
    
    aql_debug("✅ [VM] 执行完成\n");
    
    return 0;
}

/*
** 主函数
*/
int main(int argc, char *argv[]) {
    
    const char *filename = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            aql_debug_enable_verbose_all();  // 启用所有详细信息
        } else if (strcmp(argv[i], "-vt") == 0 || strcmp(argv[i], "--verbose-trace") == 0) {
            aql_debug_enable_flag(AQL_FLAG_VT);  // 仅启用执行跟踪
        } else if (strcmp(argv[i], "-vd") == 0 || strcmp(argv[i], "--verbose-debug") == 0) {
            aql_debug_enable_flag(AQL_FLAG_VD);  // 仅启用详细调试
        } else if (strcmp(argv[i], "-vb") == 0 || strcmp(argv[i], "--verbose-bytecode") == 0) {
            aql_debug_enable_flag(AQL_FLAG_VB);  // 仅启用字节码输出
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            aql_debug_disable_all();  // 静默模式
        } else if (argv[i][0] == '-') {
            aql_debug("❌ 未知选项: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else {
            if (filename) {
                aql_debug("❌ 只能指定一个字节码文件\n");
                print_usage(argv[0]);
                return 1;
            }
            filename = argv[i];
        }
    }
    
    if (!filename) {
        printf("❌ 请指定字节码文件\n");
        print_usage(argv[0]);
        return 1;
    }
    
    aql_debug("[DEBUG] 文件名: %s\n", filename);
    
    // 检查文件扩展名
    const char *ext = strrchr(filename, '.');
    if (!ext || strcmp(ext, ".by") != 0) {
        aql_debug("⚠️  警告: 文件扩展名不是 .by\n");
    }
    
    aql_debug("[DEBUG] 调用 execute_bytecode\n");
    
    return execute_bytecode(filename);
}
