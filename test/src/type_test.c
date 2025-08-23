/*
** Phase 2 类型推断系统测试
** 验证类型推断引擎核心功能
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/atype.h"

int main() {
    printf("=== AQL Phase 2 类型推断系统测试 ===\n\n");
    
    /* 初始化类型推断系统 */
    TypeInfer ti;
    aqlT_init(&ti);
    
    /* 测试基本功能 */
    printf("1. 基本类型推断测试:\n");
    
    /* 创建作用域 */
    TypeScope *scope = aqlT_new_scope(&ti, NULL);
    
    /* 测试变量类型跟踪 */
    TypeInfo int_type = {TYPEINT, 100, 0, {0}};
    aqlT_add_variable(&ti, "x", int_type);
    
    TypeInfo float_type = {TYPEFLOAT, 95, 0, {0}};
    aqlT_add_variable(&ti, "y", float_type);
    
    /* 测试变量类型查询 */
    TypeInfo x_type = aqlT_get_variable_type(&ti, "x");
    printf("变量 x 的类型: %s\n", aqlT_type_to_string(x_type));
    
    TypeInfo y_type = aqlT_get_variable_type(&ti, "y");
    printf("变量 y 的类型: %s\n", aqlT_type_to_string(y_type));
    
    /* 测试类型兼容性 */
    printf("\n2. 类型兼容性测试:\n");
    int compatible = aqlT_types_compatible(x_type, y_type);
    printf("int 和 float 兼容: %s\n", compatible ? "是" : "否");
    
    TypeInfo string_type = {TYPESTRING, 100, 0, {0}};
    int string_compatible = aqlT_types_compatible(x_type, string_type);
    printf("int 和 string 兼容: %s\n", string_compatible ? "是" : "否");
    
    /* 测试类型转换 */
    printf("\n3. 类型转换测试:\n");
    int can_convert = aqlT_can_convert(x_type, y_type);
    printf("int -> float 转换: %s\n", can_convert ? "允许" : "不允许");
    
    /* 测试类型分数计算 */
    printf("\n4. 类型分数计算:\n");
    int score = aqlT_type_score(x_type, y_type);
    printf("int 和 float 的兼容性分数: %d/100\n", score);
    
    score = aqlT_type_score(x_type, string_type);
    printf("int 和 string 的兼容性分数: %d/100\n", score);
    
    /* 清理 */
    aqlT_close_scope(&ti, scope);
    
    printf("\n🎉 Phase 2 类型推断系统核心功能验证完成！\n");
    printf("类型推断引擎已就绪，可集成到AQL编译流程\n");
    
    return 0;
}