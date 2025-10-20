/*
** $Id: aopcodes.c $
** Opcodes for AQL virtual machine
** See Copyright Notice in aql.h
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "aopcodes.h"

/*
** 获取指令格式描述
*/
static const char* get_instruction_format(OpCode op) {
    switch (op) {
        case OP_LOADI:
        case OP_LOADF:
            return "LOADI R<A>, <sBx>";
        case OP_LOADK:
            return "LOADK R<A>, K<Bx>";
        case OP_LOADKX:
            return "LOADKX R<A>";
        case OP_MOVE:
            return "MOVE R<A>, R<B>";
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
            return "ADD R<A>, R<B>, R<C>";
        case OP_ADDI:
            return "ADDI R<A>, R<B>, <sC>";
        case OP_MULK:
            return "MULK R<A>, R<B>, K<C>";
        case OP_LT:
        case OP_LE:
        case OP_EQ:
            return "LT R<A>, R<B>, R<C>";
        case OP_LTI:
        case OP_LEI:
        case OP_EQI:
            return "LTI R<A>, <sB>, <k>";
        case OP_JMP:
            return "JMP <sJ>";
        case OP_CALL:
            return "CALL R<A>, <B>, <C>";
        case OP_RETURN:
        case OP_RETURN1:
        case OP_RETURN0:
            return "RETURN R<A>, <B>, <C>";
        case OP_MMBIN:
            return "MMBIN R<A>, R<B>, <C>";
        case OP_MMBINI:
            return "MMBINI R<A>, <sB>, <C>, <k>";
        case OP_MMBINK:
            return "MMBINK R<A>, K<B>, <C>";
        default:
            return "Unknown instruction format";
    }
}

/*
** 通用指令解析函数 - 根据操作码名称和参数解析指令
** 返回值：1=成功，0=失败
*/
int aql_parse_instruction(const char *opcode, const char *arg1, 
                         const char *arg2, const char *arg3, 
                         const char *arg4, int args, Instruction *result) {
    OpCode op = NUM_OPCODES; // 无效值
    
    // 查找操作码
    for (int i = 0; i < NUM_OPCODES; i++) {
        if (strcmp(aql_opnames[i], opcode) == 0) {
            op = (OpCode)i;
            break;
        }
    }
    
    if (op >= NUM_OPCODES) {
        return 0; // 未找到操作码
    }
    
    // 解析参数
    int a = 0, b = 0, c = 0, k = 0;
    
    // 解析A参数（寄存器）
    if (args >= 2 && arg1 && arg1[0] == 'R') {
        a = atoi(arg1 + 1);
    }
    
    // 根据操作码类型解析其他参数
    enum OpMode mode = getOpMode(op);
    
    switch (op) {
        // 特殊处理的指令
        case OP_TEST:
            if (args >= 3) {
                k = atoi(arg2);
                *result = CREATE_ABCk(op, a, b, c, k);
                return 1;
            }
            break;
            
        case OP_JMP:
            if (args >= 2) {
                int offset = atoi(arg1);
                *result = CREATE_sJ(op, offset, 0);
                return 1;
            }
            break;
            
        case OP_LOADI:
        case OP_LOADF:
            if (args >= 3) {
                int sbx = atoi(arg2);
                *result = CREATE_AsBx(op, a, sbx);
                return 1;
            }
            break;
            
        case OP_LOADK:
            if (args >= 3) {
                int bx;
                if (arg2[0] == 'K') {
                    // 常量索引：K1 -> bx=1
                    bx = atoi(arg2 + 1);  // 跳过'K'字符
                } else {
                    // 直接数字索引
                    bx = atoi(arg2);
                }
                *result = CREATE_ABx(op, a, bx);
                return 1;
            }
            break;
            
        // MMBIN 系列指令
        case OP_MMBIN:
            if (args >= 4) {
                b = (arg2[0] == 'R') ? atoi(arg2 + 1) : atoi(arg2);
                c = atoi(arg3);  // TM_* 编号
                *result = CREATE_ABC(op, a, b, c);
                return 1;
            }
            break;
            
        case OP_MMBINI:
            if (args >= 5) {  // A sB C k 需要5个参数
                int sb = atoi(arg2);  // 有符号立即数
                c = atoi(arg3);       // TM_* 编号
                k = atoi(arg4);       // flip标志位
                // sB参数编码到B字段
                b = int2sC(sb);
                *result = CREATE_ABCk(op, a, b, c, k);
                return 1;
            }
            break;
            
        case OP_MMBINK:
            if (args >= 4) {
                b = atoi(arg2);  // 常量索引
                c = atoi(arg3);  // TM_* 编号
                *result = CREATE_ABCk(op, a, b, c, 1);
                return 1;
            }
            break;
            
        // LEI系列指令 - A sB k 格式 (Lua兼容)
        case OP_LEI:
        case OP_LTI:
        case OP_GTI:
        case OP_GEI:
        case OP_EQI:
            if (args >= 3) {  // Lua格式：LEI A sB k (3个参数)
                int sb = atoi(arg2);  // 有符号立即数
                k = atoi(arg3);       // 条件标志位
                // sB参数需要编码到B字段，使用int2sC进行有符号编码
                b = int2sC(sb);
                *result = CREATE_ABCk(op, a, b, 0, k);
                return 1;
            }
            break;
            
        // ADDI系列指令 - A B sC 格式
        case OP_ADDI:
            if (args >= 4) {
                b = (arg2[0] == 'R') ? atoi(arg2 + 1) : atoi(arg2);
                int sc = atoi(arg3);  // 有符号立即数
                // sC参数需要编码到C字段，使用int2sC进行有符号编码
                c = int2sC(sc);
                *result = CREATE_ABC(op, a, b, c);
                return 1;
            }
            break;
            
        // MULK系列指令 - A B C 格式 (与常量相乘)
        case OP_MULK:
            if (args >= 4) {
                b = (arg2[0] == 'R') ? atoi(arg2 + 1) : atoi(arg2);
                c = atoi(arg3);  // 常量索引
                *result = CREATE_ABC(op, a, b, c);
                return 1;
            }
            break;
            
        // FORPREP 和 FORLOOP 指令 - A Bx 格式 (iABx)
        case OP_FORPREP:
        case OP_FORLOOP:
            if (args >= 3) {
                int bx = atoi(arg2);  // 跳转偏移量
                *result = CREATE_ABx(op, a, bx);
                return 1;
            }
            break;
            
        // AQL 容器指令特殊处理
        case OP_NEWOBJECT:
            if (args >= 4) {
                int container_type = atoi(arg2);  // 容器类型：0=ARRAY, 1=SLICE, 2=DICT, 3=VECTOR
                int size_or_capacity = atoi(arg3); // 大小或容量
                *result = CREATE_ABC(op, a, container_type, size_or_capacity);
                return 1;
            }
            break;
            
        case OP_GETPROP:
        case OP_SETPROP:
            // GETPROP R0, R1, R2  -> R0 = R1[R2] 或 R0 = R1.prop[R2]
            // SETPROP R0, R1, R2  -> R0[R1] = R2 或 R0.prop[R1] = R2
            if (args >= 4) {
                b = (arg2[0] == 'R') ? atoi(arg2 + 1) : atoi(arg2);
                c = (arg3[0] == 'R') ? atoi(arg3 + 1) : atoi(arg3);
                *result = CREATE_ABC(op, a, b, c);
                return 1;
            }
            break;
            
        case OP_INVOKE:
            // INVOKE R0, R1, method_id  -> R0 = R1:method(...)
            if (args >= 4) {
                b = (arg2[0] == 'R') ? atoi(arg2 + 1) : atoi(arg2);
                int method_id = atoi(arg3);  // 方法ID：0=append, 1=length, etc.
                *result = CREATE_ABC(op, a, b, method_id);
                return 1;
            }
            break;
            
        case OP_CALL:
            // CALL R0, nargs, nresults  -> 调用R0，nargs个参数，期望nresults个返回值
            if (args >= 4) {
                b = atoi(arg2);  // 参数数量+1 (Lua风格)
                c = atoi(arg3);  // 返回值数量+1 (Lua风格)
                *result = CREATE_ABC(op, a, b, c);
                return 1;
            }
            break;
            
        case OP_TAILCALL:
            // TAILCALL R0, nargs, nresults  -> 尾调用R0
            if (args >= 3) {
                b = atoi(arg2);  // 参数数量+1
                c = atoi(arg3);  // 返回值数量+1
                *result = CREATE_ABC(op, a, b, c);
                return 1;
            }
            break;
            
        case OP_CLOSURE:
            // CLOSURE R0, function_index  -> 创建函数闭包
            if (args >= 3) {
                b = atoi(arg2);  // 函数索引
                *result = CREATE_ABx(op, a, b);
                return 1;
            }
            break;
            
        case OP_RETURN:
            // RETURN R0, nvalues, k  -> 返回多个值
            if (args >= 3) {
                b = atoi(arg2);  // 返回值数量+1 (Lua风格)
                c = (args >= 4) ? atoi(arg3) : 0;  // k标志 (可选)
                if (args >= 4 && c != 0) {
                    // 如果有k位，使用CREATE_ABCk
                    *result = CREATE_ABCk(op, a, b, 0, c);
                } else {
                    // 没有k位，使用CREATE_ABC
                    *result = CREATE_ABC(op, a, b, c);
                }
                return 1;
            }
            break;
            
        case OP_GETTABUP:
            // GETTABUP R0, upvalue_index, key_index  -> 从upvalue表获取值
            if (args >= 4) {
                b = atoi(arg2);  // upvalue索引
                if (arg3[0] == 'K') {
                    // 常量访问：K0 -> c=0, k=1
                    c = atoi(arg3 + 1);  // 跳过'K'字符
                    *result = CREATE_ABCk(op, a, b, c, 1);  // 设置k=1标志
                } else {
                    // 寄存器访问：R0 -> c=0, k=0  
                    c = atoi(arg3 + 1);  // 跳过'R'字符
                    *result = CREATE_ABCk(op, a, b, c, 0);  // 设置k=0标志
                }
                return 1;
            }
            break;
            
        case OP_SETTABUP:
            // SETTABUP upvalue_index, key_index, value  -> 向upvalue表设置值
            if (args >= 4) {
                if (arg2[0] == 'K') {
                    // key是常量：K0 -> b=0, k位在key
                    b = atoi(arg2 + 1);  // 跳过'K'字符
                } else {
                    // key是寄存器：R0 -> b=0
                    b = atoi(arg2 + 1);  // 跳过'R'字符
                }
                
                if (arg3[0] == 'R') {
                    // value是寄存器：R2 -> c=2
                    c = atoi(arg3 + 1);  // 跳过'R'字符
                } else if (arg3[0] == 'K') {
                    // value是常量：K2 -> c=2, 需要设置常量标志
                    c = atoi(arg3 + 1) | 0x100;  // 设置常量标志位
                } else {
                    c = atoi(arg3);
                }
                
                *result = CREATE_ABC(op, a, b, c);
                return 1;
            }
            break;
            
        case OP_GETFIELD:
            // GETFIELD R0, table_reg, key_index  -> 从表获取字段
            if (args >= 4) {
                b = atoi(arg2);  // 表寄存器
                c = atoi(arg3);  // 常量表中的key索引
                *result = CREATE_ABC(op, a, b, c);
                return 1;
            }
            break;
            
        case OP_GETUPVAL:
            // GETUPVAL R0, upvalue_index  -> 获取upvalue
            if (args >= 3) {
                b = atoi(arg2);  // upvalue索引
                *result = CREATE_ABC(op, a, b, 0);
                return 1;
            }
            break;
            
        case OP_SETUPVAL:
            // SETUPVAL R0, upvalue_index  -> 设置upvalue
            if (args >= 3) {
                b = atoi(arg2);  // upvalue索引
                *result = CREATE_ABC(op, a, b, 0);
                return 1;
            }
            break;
            
        // 标准ABC格式指令
        default:
            switch (mode) {
                case iABC:
                    if (args >= 3 && arg2) {
                        b = (arg2[0] == 'R') ? atoi(arg2 + 1) : atoi(arg2);
                    }
                    if (args >= 4 && arg3) {
                        c = (arg3[0] == 'R') ? atoi(arg3 + 1) : atoi(arg3);
                    }
                    *result = CREATE_ABC(op, a, b, c);
                    return 1;
                    
                case iABx:
                    if (args >= 3) {
                        int bx = atoi(arg2);
                        *result = CREATE_ABx(op, a, bx);
                        return 1;
                    }
      break;
                    
                case iAsBx:
                    if (args >= 3) {
                        int sbx = atoi(arg2);
                        *result = CREATE_AsBx(op, a, sbx);
                        return 1;
                    }
      break;
                    
                case iAx:
                    if (args >= 2) {
                        int ax = atoi(arg1);
                        *result = CREATE_Ax(op, ax);
                        return 1;
                    }
      break;
    }
      break;
    }
    
    // 如果没有找到对应的指令处理，输出错误信息
    printf("Error: Unsupported instruction '%s' or invalid arguments\n", opcode);
    printf("  Instruction: %s\n", opcode);
    printf("  Arguments: ");
    if (arg1) printf("'%s' ", arg1);
    if (arg2) printf("'%s' ", arg2);
    if (arg3) printf("'%s' ", arg3);
    if (arg4) printf("'%s' ", arg4);
    printf("\n");
    printf("  Expected format: %s\n", get_instruction_format(op));
    
    return 0; // 解析失败
} 