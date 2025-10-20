# AQL 调试系统设计文档

## 概述

基于 `debug-req.md` 的需求规范，本文档详细描述AQL调试系统的技术设计和实现架构。

## 设计原则

1. **编译时优化**: 生产版本完全无调试代码开销
2. **分离关注点**: 不同工具有不同的调试需求  
3. **格式统一**: 结构化输出有统一的漂亮格式
4. **运行时控制**: 支持细粒度的调试选项组合
5. **性能优先**: 使用宏定义方式，避免运行时开销

## 三个版本定位

### aql - 生产版本
- **目标**：高效执行，零调试开销
- **编译标志**：`-DAQL_PRODUCTION_BUILD`
- **特点**：
  - 完全无调试代码，编译时全部优化掉
  - 仅保留错误信息输出
  - 性能最优，体积最小
  - 使用宏定义方式，编译时零开销

### aqlm - 字节码执行工具
- **目标**：辅助调试验证avm_core.c和指令正确性
- **编译标志**：`-DAQL_DEBUG_BUILD`（与aqld相同）
- **特点**：
  - 专门运行.by字节码文件
  - 基础调试功能，验证VM执行
  - 支持指令级跟踪和寄存器状态显示
  - 帮助开发者验证指令实现的正确性
- **注意**：与aqld使用相同的编译标志，区别在于功能定位

### aqld - 完整调试版本
- **目标**：全功能调试支持，开发调试用
- **编译标志**：`-DAQL_DEBUG_BUILD`
- **特点**：
  - 完整的调试功能支持
  - 词法分析、语法分析、字节码生成、执行跟踪
  - 详细的内部调试信息
  - 开发阶段使用

## 调试输出分类

### 基础输出
- `aql_error(...)` - 错误信息，所有版本都有
- `aql_info(...)` - 基础信息，非生产版本有

### 格式化输出 (带统一格式)
- `aql_info_vl` - 词法分析输出 (Lexer tokens)
- `aql_info_vast` - 抽象语法树输出 (AST)  
- `aql_info_vb` - 字节码输出 (Bytecode)
- `aql_info_vt` - 执行跟踪输出 (Trace)
- `aql_info_vd` - 详细调试信息 (Debug)

### 内部调试
- `aql_debug(...)` - 内部调试信息，仅调试构建版本
- **注意**：vd输出通常使用`aql_debug`宏，提供详细的内部状态信息

## 编译标志和运行时标志的关系

### 编译时标志（宏定义）
- **`AQL_PRODUCTION_BUILD`**：生产版本，所有调试代码编译为空操作
- **`AQL_DEBUG_BUILD`**：调试版本，包含所有调试功能
- **注意**：只需要两个编译标志，简化架构

### 运行时调试标志（命令行参数）
- **`-v`**：全部调试信息（相当于 -vd + -vt + -vast + -vl + -vb）
- **`-vd`**：详细调试信息
- **`-vt`**：执行跟踪
- **`-vast`**：AST输出
- **`-vl`**：词法分析
- **`-vb`**：字节码输出

### 宏定义层次关系
```
编译时检查：
├── 如果没有 AQL_DEBUG_BUILD → 所有调试宏定义为 ((void)0)
└── 如果有 AQL_DEBUG_BUILD → 再检查运行时标志
    ├── 如果运行时没有 -v/-vt → aql_info_vt 为空
    ├── 如果运行时没有 -v/-vd → aql_info_vd 为空
    ├── 如果运行时没有 -v/-vast → aql_info_vast 为空
    ├── 如果运行时没有 -v/-vl → aql_info_vl 为空
    └── 如果运行时没有 -v/-vb → aql_info_vb 为空
```

## 文件组织架构

### 核心调试文件

#### adebug.h - 通用调试接口
**职责**：定义所有通用的调试宏和接口
**内容**：
- 调试标志位定义 (`AQL_FLAG_VD`, `AQL_FLAG_VT` 等)
- 通用调试宏定义 (`aql_debug`, `aql_info`, `aql_info_vt` 等)
- 编译时宏系统 (`#ifdef AQL_PRODUCTION_BUILD`)
- 运行时标志检查函数声明
- 格式化输出函数声明

#### adebug.c - 通用调试实现
**职责**：实现通用的调试功能
**内容**：
- 全局调试状态变量
- 运行时标志检查函数实现
- 基础输出函数实现 (`aql_output_error`, `aql_output_info` 等)
- 格式化输出函数实现
- 调试标志设置函数

#### avm.h - VM特定调试接口
**职责**：定义VM执行相关的调试宏
**内容**：
- 指令特定调试宏声明 (`AQL_INFO_VT_LOADI_BEFORE` 等)
- VM调试相关的辅助函数声明
- 寄存器值显示函数声明
- 指令参数解析函数声明

#### avm_core.c - VM调试实现
**职责**：实现VM执行中的调试逻辑
**内容**：
- 指令特定调试宏实现
- VM执行循环中的调试调用
- 寄存器状态显示逻辑
- 指令参数解析和显示逻辑

### 文件职责分工

#### 通用调试功能 (adebug.h/c)
```c
// 通用调试宏
#define aql_debug(...)          // 内部调试信息
#define aql_info(...)           // 基础信息输出
#define aql_info_vt(...)        // VT跟踪信息
#define aql_info_vd(...)        // 详细调试信息
#define aql_info_vast(...)      // AST输出
#define aql_info_vl(...)        // 词法分析输出
#define aql_info_vb(...)        // 字节码输出

// 通用指令跟踪宏
#define AQL_INFO_VT_BEFORE()    // 通用BEFORE逻辑
#define AQL_INFO_VT_AFTER()     // 通用AFTER逻辑
```

#### VM特定调试功能 (avm.h + avm_core.c)
```c
// 指令特定调试宏
#define AQL_INFO_VT_LOADI_BEFORE()  // LOADI特定BEFORE逻辑
#define AQL_INFO_VT_LOADI_AFTER()   // LOADI特定AFTER逻辑
#define AQL_INFO_VT_ADD_BEFORE()    // ADD特定BEFORE逻辑
#define AQL_INFO_VT_ADD_AFTER()     // ADD特定AFTER逻辑
#define AQL_INFO_VT_CALL_BEFORE()   // CALL特定BEFORE逻辑
#define AQL_INFO_VT_CALL_AFTER()    // CALL特定AFTER逻辑
```

## 实现架构

### 运行时标志检查机制

#### 全局状态变量
```c
// 全局调试标志状态
static int aql_debug_flags = 0;

// 运行时标志检查函数
int aql_debug_is_enabled(AQL_NewDebugFlags flag) {
    return (aql_debug_flags & flag) != 0;
}

// 设置调试标志
void aql_debug_set_flags(int flags) {
    aql_debug_flags = flags;
}
```

#### 宏定义实现
```c
// 生产版本：所有调试代码编译为空
#ifdef AQL_PRODUCTION_BUILD
    #define aql_debug(...)          ((void)0)
    #define aql_info_vt(...)        ((void)0)
    #define aql_info_vd(...)        ((void)0)
    #define aql_info_vast(...)      ((void)0)
    #define AQL_INFO_VT_BEFORE()    ((void)0)
    #define AQL_INFO_VT_AFTER()     ((void)0)
    #define AQL_INFO_VT_LOADI_BEFORE() ((void)0)
    #define AQL_INFO_VT_ADD_BEFORE()   ((void)0)
#endif

// 调试版本：根据运行时标志控制
#ifdef AQL_DEBUG_BUILD
    #define aql_info_vt(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    
    #define AQL_INFO_VT_BEFORE() \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                /* 通用BEFORE逻辑 */ \
            } \
        } while(0)
    
    #define AQL_INFO_VT_LOADI_BEFORE() \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                /* LOADI特定BEFORE逻辑 */ \
            } \
        } while(0)
#endif
```

### 指令特定调试宏设计

#### 命名规范
- **通用宏**：`AQL_INFO_VT_BEFORE()`, `AQL_INFO_VT_AFTER()`
- **指令特定宏**：`AQL_INFO_VT_{指令名}_{阶段}()`
- **示例**：
  - `AQL_INFO_VT_LOADI_BEFORE()`
  - `AQL_INFO_VT_ADD_AFTER()`
  - `AQL_INFO_VT_CALL_BEFORE()`

#### 使用场景
```c
// 在VM执行循环中
switch (GET_OPCODE(i)) {
    case OP_LOADI:
        AQL_INFO_VT_LOADI_BEFORE();  // LOADI特定的BEFORE逻辑
        // 执行LOADI指令
        AQL_INFO_VT_LOADI_AFTER();   // LOADI特定的AFTER逻辑
        break;
        
    case OP_ADD:
        AQL_INFO_VT_ADD_BEFORE();    // ADD特定的BEFORE逻辑
        // 执行ADD指令
        AQL_INFO_VT_ADD_AFTER();     // ADD特定的AFTER逻辑
        break;
        
    case OP_CALL:
        AQL_INFO_VT_CALL_BEFORE();   // CALL特定的BEFORE逻辑
        // 执行CALL指令
        AQL_INFO_VT_CALL_AFTER();    // CALL特定的AFTER逻辑
        break;
}
```

#### 优势
- **性能优化**：每个指令只包含必要的调试逻辑
- **代码清晰**：避免复杂的switch-case判断
- **维护简单**：新增指令只需添加对应的宏
- **编译优化**：编译器可以更好地优化每个宏

## 指令输出格式规范

### 指令参数自动解析规则

#### 寄存器参数 (R)
- **格式要求**：`R[编号=值]`
- **类型识别**：自动识别整数、浮点、字符串、函数、nil等类型
- **显示规范**：
  - 整数：`A=[R0=42]`
  - 浮点：`B=[R1=3.14]`
  - 字符串：`C=[R2="hello"]`
  - 函数：`A=[R3=func@0x1234]`
  - 未初始化：`A=[R4=?]`
  - nil值：`A=[R5=nil]`

#### 常量参数 (K)
- **格式要求**：`K[索引=值]`
- **类型识别**：自动识别常量表中的值类型
- **显示规范**：
  - 整数常量：`B=[K0=100]`
  - 字符串常量：`C=[K1="world"]`
  - 函数常量：`B=[K2=func@0x5678]`

#### 立即数参数
- **格式要求**：直接显示数值
- **类型识别**：根据指令格式自动识别
- **显示规范**：
  - 有符号立即数：`sBx=5`, `sC=-10`
  - 大立即数：`Ax=1000`
  - 跳转偏移：`sJ=2`

### 字符串显示规则

#### 短字符串 (≤20字符)
- **显示要求**：直接显示完整字符串
- **格式**：`"hello world"`

#### 长字符串 (>20字符)
- **显示要求**：前17字符 + "..."
- **格式**：`"this is a very long string..."`

#### 特殊字符处理
- **换行符**：显示为`\n`
- **制表符**：显示为`\t`
- **引号**：显示为`\"`

### 指令格式分类显示要求

#### iABC格式指令 (如ADD, MUL, SUB)
- **参数显示**：A、B、C三个参数
- **格式要求**：`A=[R2=?], B=[R0=5], C=[R1=3]`

#### iABx格式指令 (如LOADI, CLOSURE)
- **参数显示**：A参数 + Bx/sBx参数
- **格式要求**：`A=[R1=?], sBx=5`

#### iAsBx格式指令 (如JMP, FORLOOP)
- **参数显示**：A参数 + sBx/sJ参数
- **格式要求**：`sJ=2`

#### iAx格式指令 (如EXTRAARG)
- **参数显示**：Ax参数
- **格式要求**：`Ax=1000`

## 预期输出效果

### 基本指令执行
```
PC=0 [main] CLOSURE BEFORE: A=[R0=?], Bx=0
PC=0 [main] CLOSURE AFTER:  A=[R0=func@0x1234], Bx=0

PC=1 [main] LOADI BEFORE:   A=[R1=?], sBx=5
PC=1 [main] LOADI AFTER:    A=[R1=5], sBx=5

PC=2 [main] LOADI BEFORE:   A=[R2=?], sBx=3
PC=2 [main] LOADI AFTER:    A=[R2=3], sBx=3
```

### 函数调用完整流程
```
PC=3 [main] CALL BEFORE:    A=[R0=func@0x1234], B=[R1=5], C=[R2=3]
--- CALL: func=func@0x1234, args=2, rets=1 ---
  Args: R0=5, R1=3
--- ENTER: func=func@0x1234 ---

PC=0 [func@0x1234] MUL BEFORE: A=[R2=?], B=[R0=5], C=[R1=3]
PC=0 [func@0x1234] MUL AFTER:  A=[R2=15], B=[R0=5], C=[R1=3]

PC=2 [func@0x1234] RETURN1 BEFORE: A=[R2=15]
--- RETURN: func=func@0x1234, rets=1 ---
  Return: R2=15
--- EXIT: func=func@0x1234 ---

PC=3 [main] CALL AFTER:     A=[R0=15], B=[R1=5], C=[R2=3]
```

## 性能优化策略

### 生产版本优化
- **编译时优化**：所有调试宏编译为空操作 `((void)0)`
- **零开销**：完全避免调试逻辑的执行
- **体积最小**：不包含任何调试代码

### 调试版本优化
- **运行时检查**：避免不必要的计算
- **指令特定宏**：避免复杂的通用逻辑判断
- **条件编译**：根据编译标志选择不同的实现

### 关键特性
- **指令级跟踪**：BEFORE/AFTER状态对比
- **智能值显示**：自动识别R/K/立即数，字符串截断
- **函数调用跟踪**：完整的调用链和参数传递
- **分层控制**：细粒度的调试选项组合
- **零开销设计**：生产版本完全无调试代码

## 总结

AQL调试系统采用分层、模块化的设计：

1. **编译时优化**：生产版本零开销
2. **运行时控制**：调试版本灵活控制
3. **文件分离**：通用功能与VM特定功能分离
4. **宏定义系统**：高效的调试信息输出
5. **指令特定优化**：每个指令有专门的调试逻辑

这个设计确保了调试系统的性能、可维护性和扩展性。
