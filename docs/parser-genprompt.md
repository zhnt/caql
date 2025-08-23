# AQL解析器代码生成提示词

## 核心要求
基于parser-design-v6.md，完全复刻Lua 5.4解析器架构，但使用AQL现有代码库的类型和方法。

## 类型兼容性要求
- 使用aql_Integer代替lua_Integer
- 使用aql_Number代替lua_Number
- 使用aql_byte代替lu_byte
- 使用TString代替TString（保持不变）
- 使用TValue代替TValue（保持不变）
- 所有结构体前缀改为A（如AExpDesc、AFuncState、ABlockCnt）

## 代码结构要求

### aparser.h
1. 包含完整的类型定义，与现有AQL代码库100%兼容
2. 所有Lua原生函数名改为aqlY_前缀
3. 保持Lua的递归下降解析器架构
4. 支持AQL语法：let/const、:=、{}、elif、switch、match

### aparser.c
1. 完全按照Lua 5.4的lparser.c实现，但适配AQL语法
2. 保持以下核心函数签名：
   - AClosure *aqlY_parser(aql_State*, AZio*, AMbuffer*, ADyndata*, const char*, int)
   - static void aqlY_mainfunc(aql_State*, ALexState*, AMbuffer*)
   - static void aqlY_statlist(AFuncState*)
   - static void aqlY_statement(AFuncState*)
   - static void aqlY_expr(ALexState*, AExpDesc*)
   - static int aqlY_subexpr(ALexState*, AExpDesc*, int)

3. 语法适配：
   - if/elif/else使用{}而不是then/end
   - while使用{}而不是do/end
   - for循环使用AQL语法：for i=1,10 {} 和 for x in items {}
   - let/const变量声明
   - 支持?:三元运算

## 具体实现要求

### 1. 表达式解析
- 保持Lua的优先级表结构
- 支持AExpKind枚举（AEXP_KINT, AEXP_KFLT等）
- 三元运算?:的优先级处理

### 2. 作用域管理
- 使用aqlY_enterblock()/aqlY_leaveblock()
- 保持BlockCnt链式结构
- 寄存器分配与Lua相同

### 3. 构造器支持
- 数组构造：[1,2,3]
- 字典构造：{key:value}
- 向量构造：<1.0,2.0,3.0>
- 切片构造：[1:5]

### 4. 错误处理
- 保持Lua的错误处理风格
- 提供AQL特定的错误提示（如"使用{}代替then"）

## 代码风格
- 使用AQL命名约定（aqlY_*, A*结构体）
- 保持C99标准
- 避免C++特性
- 紧凑的结构体设计

## 验证标准
- 所有类型与现有AQL代码库兼容
- 无编译警告
- 支持示例代码的正确解析
- 内存布局与Lua解析器一致

生成完整的aparser.h和aparser.c代码，保持Lua解析器的高效性和正确性，同时完全适配AQL语法要求。