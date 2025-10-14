/*
** 字节码文本解析器
** 将文本格式的 .by 文件解析为二进制字节码
** 支持类似汇编语言的语法
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "../../src/aopcodes.h"

/*
** 最大限制
*/
#define MAX_INSTRUCTIONS 1000
#define MAX_CONSTANTS 100
#define MAX_LINE_LENGTH 256
#define MAX_LABEL_LENGTH 64

/*
** 操作码映射表
*/
typedef struct {
    const char *name;
    OpCode opcode;
    int format;  // 0=ABC, 1=ABx, 2=AsBx, 3=Ax
} OpcodeInfo;

static const OpcodeInfo opcode_table[] = {
    // 基础指令
    {"LOADI",    OP_LOADI,    0},  // ABC
    {"LOADK",    OP_LOADK,    1},  // ABx
    {"LOADTRUE", OP_LOADTRUE, 0},  // ABC
    {"LOADFALSE",OP_LOADFALSE,0},  // ABC
    {"LOADNIL",  OP_LOADNIL,  0},  // ABC
    
    // 算术指令
    {"ADD",      OP_ADD,      0},  // ABC
    {"SUB",      OP_SUB,      0},  // ABC
    {"MUL",      OP_MUL,      0},  // ABC
    {"DIV",      OP_DIV,      0},  // ABC
    {"MOD",      OP_MOD,      0},  // ABC
    {"POW",      OP_POW,      0},  // ABC
    {"UNM",      OP_UNM,      0},  // ABC
    
    // 比较指令
    {"LT",       OP_LT,       0},  // ABC
    {"LE",       OP_LE,       0},  // ABC
    {"EQ",       OP_EQ,       0},  // ABC
    {"NE",       OP_NE,       0},  // ABC
    {"GT",       OP_GT,       0},  // ABC
    {"GE",       OP_GE,       0},  // ABC
    
    // 控制流指令
    {"JMP",      OP_JMP,      2},  // AsBx
    {"TEST",     OP_TEST,     0},  // ABC
    {"CALL",     OP_CALL,     0},  // ABC
    {"RET",      OP_RET,      0},  // ABC
    {"RET_ONE",  OP_RET_ONE,  0},  // ABC
    {"RET_VOID", OP_RET_VOID, 0},  // ABC
    
    {NULL, 0, 0}  // 结束标记
};

/*
** 常量类型
*/
typedef enum {
    CONST_NIL,
    CONST_BOOL,
    CONST_INT,
    CONST_FLOAT,
    CONST_STRING
} ConstantType;

/*
** 常量值
*/
typedef struct {
    ConstantType type;
    union {
        int b;           // bool
        int64_t i;       // int
        double f;        // float
        char *s;         // string
    } value;
} Constant;

/*
** 解析状态
*/
typedef struct {
    Instruction instructions[MAX_INSTRUCTIONS];
    int instruction_count;
    
    Constant constants[MAX_CONSTANTS];
    int constant_count;
    
    int stack_size;
    
    char error_msg[256];
    int line_number;
    
    // 解析模式
    enum {
        PARSE_MODE_HEADER,
        PARSE_MODE_CONSTANTS,
        PARSE_MODE_CODE
    } parse_mode;
} ParseState;

/*
** 查找操作码
*/
static const OpcodeInfo* find_opcode(const char *name) {
    for (int i = 0; opcode_table[i].name; i++) {
        if (strcmp(opcode_table[i].name, name) == 0) {
            return &opcode_table[i];
        }
    }
    return NULL;
}

/*
** 解析寄存器 (R0, R1, ...)
*/
static int parse_register(const char *str) {
    if (str[0] != 'R') return -1;
    return atoi(str + 1);
}

/*
** 解析常量 (K0, K1, ...)
*/
static int parse_constant(const char *str) {
    if (str[0] != 'K') return -1;
    return atoi(str + 1);
}

/*
** 解析整数
*/
static int parse_integer(const char *str) {
    return atoi(str);
}

/*
** 跳过空白字符
*/
static char* skip_whitespace(char *str) {
    while (*str && isspace(*str)) str++;
    return str;
}

/*
** 获取下一个token
*/
static char* get_next_token(char **str) {
    *str = skip_whitespace(*str);
    if (!**str) return NULL;
    
    char *start = *str;
    
    // 跳过非空白字符
    while (**str && !isspace(**str) && **str != ',' && **str != '#') {
        (*str)++;
    }
    
    // 如果遇到逗号，跳过它
    if (**str == ',') {
        **str = '\0';
        (*str)++;
    } else if (**str) {
        **str = '\0';
        (*str)++;
    }
    
    return start;
}

/*
** 解析指令行
*/
static int parse_instruction_line(ParseState *state, char *line) {
    // 跳过注释
    char *comment = strchr(line, '#');
    if (comment) *comment = '\0';
    
    // 跳过空行
    line = skip_whitespace(line);
    if (!*line) return 1;  // 空行，成功
    
    // 解析操作码
    char *ptr = line;
    char *opcode_name = get_next_token(&ptr);
    if (!opcode_name) return 1;  // 空行
    
    const OpcodeInfo *opinfo = find_opcode(opcode_name);
    if (!opinfo) {
        snprintf(state->error_msg, sizeof(state->error_msg), 
                "未知操作码: %s", opcode_name);
        return 0;
    }
    
    // 解析参数
    int a = 0, b = 0, c = 0;
    char *arg1 = get_next_token(&ptr);
    char *arg2 = get_next_token(&ptr);
    char *arg3 = get_next_token(&ptr);
    
    switch (opinfo->format) {
        case 0: {  // ABC格式
            if (!arg1) {
                snprintf(state->error_msg, sizeof(state->error_msg), 
                        "%s 指令缺少参数", opcode_name);
                return 0;
            }
            
            a = parse_register(arg1);
            if (a < 0) {
                snprintf(state->error_msg, sizeof(state->error_msg), 
                        "无效的寄存器: %s", arg1);
                return 0;
            }
            
            if (arg2) {
                if (arg2[0] == 'R') {
                    b = parse_register(arg2);
                } else if (arg2[0] == 'K') {
                    b = parse_constant(arg2);
                } else {
                    b = parse_integer(arg2);
                }
            }
            
            if (arg3) {
                if (arg3[0] == 'R') {
                    c = parse_register(arg3);
                } else if (arg3[0] == 'K') {
                    c = parse_constant(arg3);
                } else {
                    c = parse_integer(arg3);
                }
            }
            
            state->instructions[state->instruction_count] = CREATE_ABC(opinfo->opcode, a, b, c);
            break;
        }
        
        case 1: {  // ABx格式
            if (!arg1 || !arg2) {
                snprintf(state->error_msg, sizeof(state->error_msg), 
                        "%s 指令参数不足", opcode_name);
                return 0;
            }
            
            a = parse_register(arg1);
            if (a < 0) {
                snprintf(state->error_msg, sizeof(state->error_msg), 
                        "无效的寄存器: %s", arg1);
                return 0;
            }
            
            if (arg2[0] == 'K') {
                b = parse_constant(arg2);
            } else {
                b = parse_integer(arg2);
            }
            
            state->instructions[state->instruction_count] = CREATE_ABx(opinfo->opcode, a, b);
            break;
        }
        
        case 2: {  // AsBx格式 (跳转)
            if (!arg1 || !arg2) {
                snprintf(state->error_msg, sizeof(state->error_msg), 
                        "%s 指令参数不足", opcode_name);
                return 0;
            }
            
            a = parse_register(arg1);
            if (a < 0) a = parse_integer(arg1);  // 可能是立即数
            
            b = parse_integer(arg2);  // 跳转偏移
            
            state->instructions[state->instruction_count] = CREATE_sBx(opinfo->opcode, a, b);
            break;
        }
        
        default:
            snprintf(state->error_msg, sizeof(state->error_msg), 
                    "不支持的指令格式: %s", opcode_name);
            return 0;
    }
    
    state->instruction_count++;
    return 1;
}

/*
** 解析常量定义行 (K0 STRING "Hello")
*/
static int parse_constant_line(ParseState *state, char *line) {
    char *ptr = line;
    char *const_name = get_next_token(&ptr);
    char *const_type = get_next_token(&ptr);
    char *const_value = get_next_token(&ptr);
    
    if (!const_name || !const_type || !const_value) {
        snprintf(state->error_msg, sizeof(state->error_msg), 
                "常量定义格式错误: %s", line);
        return 0;
    }
    
    // 解析常量索引
    if (const_name[0] != 'K') {
        snprintf(state->error_msg, sizeof(state->error_msg), 
                "常量名格式错误: %s", const_name);
        return 0;
    }
    
    int const_idx = atoi(const_name + 1);
    if (const_idx >= MAX_CONSTANTS) {
        snprintf(state->error_msg, sizeof(state->error_msg), 
                "常量索引超出范围: %d", const_idx);
        return 0;
    }
    
    // 解析常量类型和值
    if (strcmp(const_type, "STRING") == 0) {
        // 去掉引号
        if (const_value[0] == '"' && const_value[strlen(const_value)-1] == '"') {
            const_value[strlen(const_value)-1] = '\0';
            const_value++;
        }
        
        state->constants[const_idx].type = CONST_STRING;
        state->constants[const_idx].value.s = strdup(const_value);
        
        if (const_idx >= state->constant_count) {
            state->constant_count = const_idx + 1;
        }
    } else if (strcmp(const_type, "INT") == 0) {
        state->constants[const_idx].type = CONST_INT;
        state->constants[const_idx].value.i = atoll(const_value);
        
        if (const_idx >= state->constant_count) {
            state->constant_count = const_idx + 1;
        }
    } else if (strcmp(const_type, "FLOAT") == 0) {
        state->constants[const_idx].type = CONST_FLOAT;
        state->constants[const_idx].value.f = atof(const_value);
        
        if (const_idx >= state->constant_count) {
            state->constant_count = const_idx + 1;
        }
    } else if (strcmp(const_type, "BOOL") == 0) {
        state->constants[const_idx].type = CONST_BOOL;
        state->constants[const_idx].value.b = (strcmp(const_value, "true") == 0);
        
        if (const_idx >= state->constant_count) {
            state->constant_count = const_idx + 1;
        }
    } else {
        snprintf(state->error_msg, sizeof(state->error_msg), 
                "不支持的常量类型: %s", const_type);
        return 0;
    }
    
    return 1;
}

/*
** 解析配置行 (# 栈大小: 10)
*/
static int parse_config_line(ParseState *state, char *line) {
    if (strstr(line, "栈大小:") || strstr(line, "stack size:")) {
        char *ptr = line;
        while (*ptr && !isdigit(*ptr)) ptr++;
        if (*ptr) {
            state->stack_size = atoi(ptr);
        }
        return 1;
    }
    
    return 1;
}

/*
** 解析字节码文本文件
*/
static int parse_bytecode_text(const char *filename, ParseState *state) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        snprintf(state->error_msg, sizeof(state->error_msg), 
                "无法打开文件: %s", filename);
        return 0;
    }
    
    // 初始化状态
    state->instruction_count = 0;
    state->constant_count = 0;
    state->stack_size = 10;  // 默认栈大小
    state->line_number = 0;
    state->parse_mode = PARSE_MODE_HEADER;
    
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), f)) {
        state->line_number++;
        
        // 移除换行符
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // 跳过空行
        char *trimmed = skip_whitespace(line);
        if (!*trimmed) continue;
        
        // 处理指令
        if (strcmp(trimmed, ".constants") == 0) {
            state->parse_mode = PARSE_MODE_CONSTANTS;
            continue;
        } else if (strcmp(trimmed, ".code") == 0) {
            state->parse_mode = PARSE_MODE_CODE;
            continue;
        } else if (strcmp(trimmed, ".end") == 0) {
            state->parse_mode = PARSE_MODE_HEADER;
            continue;
        }
        
        // 处理配置行
        if (*trimmed == '#') {
            if (!parse_config_line(state, trimmed)) {
                fclose(f);
                return 0;
            }
            continue;
        }
        
        // 根据解析模式处理行
        switch (state->parse_mode) {
            case PARSE_MODE_HEADER:
                // 头部模式，跳过
                break;
                
            case PARSE_MODE_CONSTANTS:
                // 常量定义模式
                if (!parse_constant_line(state, line)) {
                    fclose(f);
                    return 0;
                }
                break;
                
            case PARSE_MODE_CODE:
                // 指令模式
                if (!parse_instruction_line(state, line)) {
                    fclose(f);
                    return 0;
                }
                break;
        }
    }
    
    fclose(f);
    return 1;
}

/*
** 保存二进制字节码
*/
static int save_binary_bytecode(const char *filename, ParseState *state) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        snprintf(state->error_msg, sizeof(state->error_msg), 
                "无法创建文件: %s", filename);
        return 0;
    }
    
    // 写入文件头
    uint32_t magic = 0x414C5142;  // "AQLB"
    uint32_t version = 1;
    uint32_t code_size = state->instruction_count;
    uint32_t const_size = state->constant_count;
    uint32_t stack_size = state->stack_size;
    
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(&code_size, sizeof(code_size), 1, f);
    fwrite(&const_size, sizeof(const_size), 1, f);
    fwrite(&stack_size, sizeof(stack_size), 1, f);
    
    // 写入指令
    fwrite(state->instructions, sizeof(Instruction), state->instruction_count, f);
    
    // 写入常量表 (暂时为空)
    // TODO: 实现常量表写入
    
    fclose(f);
    return 1;
}

/*
** 显示解析结果
*/
static void show_parse_result(ParseState *state) {
    printf("解析结果:\n");
    printf("  指令数量: %d\n", state->instruction_count);
    printf("  常量数量: %d\n", state->constant_count);
    printf("  栈大小: %d\n", state->stack_size);
    printf("\n指令列表:\n");
    
    for (int i = 0; i < state->instruction_count; i++) {
        Instruction inst = state->instructions[i];
        OpCode op = GET_OPCODE(inst);
        int a = GETARG_A(inst);
        int b = GETARG_B(inst);
        int c = GETARG_C(inst);
        
        printf("  [%d] OP_%d A=%d B=%d C=%d\n", i, op, a, b, c);
    }
}

/*
** 主函数
*/
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("字节码文本解析器\n");
        printf("用法: %s <input.by> [output.bin]\n", argv[0]);
        printf("\n功能:\n");
        printf("  - 解析文本格式的字节码文件\n");
        printf("  - 生成二进制字节码文件\n");
        printf("  - 支持类似汇编语言的语法\n");
        printf("\n示例:\n");
        printf("  %s arithmetic.by arithmetic.bin\n", argv[0]);
        return 1;
    }
    
    const char *input_file = argv[1];
    const char *output_file = argc > 2 ? argv[2] : "output.bin";
    
    ParseState state;
    
    printf("🔧 解析字节码文本: %s\n", input_file);
    
    if (!parse_bytecode_text(input_file, &state)) {
        printf("❌ 解析失败 (行 %d): %s\n", state.line_number, state.error_msg);
        return 1;
    }
    
    printf("✅ 解析成功\n");
    show_parse_result(&state);
    
    printf("\n💾 保存二进制文件: %s\n", output_file);
    
    if (!save_binary_bytecode(output_file, &state)) {
        printf("❌ 保存失败: %s\n", state.error_msg);
        return 1;
    }
    
    printf("✅ 保存成功\n");
    printf("🎯 可以使用 vmtest 加载此文件进行测试\n");
    
    return 0;
}
