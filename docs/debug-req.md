# AQL 调试系统需求文档

## 概述

AQL 采用分层调试系统，通过编译时宏控制和运行时标志位，实现从生产版本的零开销到调试版本的全功能支持。

## 设计原则

1. **编译时优化**: 生产版本完全无调试代码开销
2. **分离关注点**: 不同工具有不同的调试需求  
3. **格式统一**: 结构化输出有统一的漂亮格式
4. **运行时控制**: 支持细粒度的调试选项组合

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

## 命令行参数

### aql (生产版本)
```bash
aql script.aql                    # 纯执行，无任何调试输出
```

### aqlm (字节码工具)
```bash
aqlm script.by                    # 基础执行
aqlm -v script.by                 # 全部调试信息 (vd + vt + vast + vl + vb)
aqlm -vt script.by                # 仅执行跟踪
aqlm -vd script.by                # 仅详细调试信息
aqlm -vb script.by                # 仅字节码输出
```

### aqld (调试版本)
```bash
aqld script.aql                   # 基础执行
aqld -v script.aql                # 全部调试信息 (vl + vast + vb + vt + vd)
aqld -vl script.aql               # 仅词法分析输出
aqld -vast script.aql             # 仅AST输出  
aqld -vb script.aql               # 仅字节码输出
aqld -vt script.aql               # 仅执行跟踪
aqld -vd script.aql               # 仅详细调试信息
aqld -vb -vt script.aql           # 组合：字节码 + 执行跟踪
```


## 预期输出效果

### 1. 基本指令执行

```
PC=0 [main] CLOSURE BEFORE: A=[R0=?], Bx=0
PC=0 [main] CLOSURE AFTER:  A=[R0=func@0x1234], Bx=0

PC=1 [main] LOADI BEFORE:   A=[R1=?], sBx=5
PC=1 [main] LOADI AFTER:    A=[R1=5], sBx=5

PC=2 [main] LOADI BEFORE:   A=[R2=?], sBx=3
PC=2 [main] LOADI AFTER:    A=[R2=3], sBx=3
```

### 2. 函数调用完整流程

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

### 3. 复杂函数调用示例

```
PC=4 [main] MOVE BEFORE:    A=[R3=?], B=[R0=15]
PC=4 [main] MOVE AFTER:     A=[R3=15], B=[R0=15]

PC=5 [main] CLOSURE BEFORE: A=[R4=?], Bx=0
PC=5 [main] CLOSURE AFTER:  A=[R4=func@0x5678], Bx=0

PC=6 [main] LOADI BEFORE:   A=[R5=?], sBx=1
PC=6 [main] LOADI AFTER:    A=[R5=1], sBx=1

PC=7 [main] LOADI BEFORE:   A=[R6=?], sBx=1
PC=7 [main] LOADI AFTER:    A=[R6=1], sBx=1

PC=8 [main] CALL BEFORE:    A=[R4=func@0x5678], B=[R5=1], C=[R6=1]
--- CALL: func=func@0x5678, args=2, rets=1 ---
  Args: R0=1, R1=1
--- ENTER: func=func@0x5678 ---

PC=0 [func@0x5678] ADD BEFORE: A=[R2=?], B=[R0=1], C=[R1=1]
PC=0 [func@0x5678] ADD AFTER:  A=[R2=2], B=[R0=1], C=[R1=1]

PC=2 [func@0x5678] RETURN1 BEFORE: A=[R2=2]
--- RETURN: func=func@0x5678, rets=1 ---
  Return: R2=2
--- EXIT: func=func@0x5678 ---

PC=8 [main] CALL AFTER:     A=[R4=2], B=[R5=1], C=[R6=1]

PC=9 [main] ADD BEFORE:     A=[R7=?], B=[R3=15], C=[R4=2]
PC=9 [main] ADD AFTER:      A=[R7=17], B=[R3=15], C=[R4=2]

PC=11 [main] RETURN1 BEFORE: A=[R7=17]
--- RETURN: func=main, rets=1 ---
  Return: R7=17
--- EXIT: func=main ---
```

### 4. 不同指令格式的显示

#### iABC格式（如ADD, MUL, SUB）
```
PC=0 [main] ADD BEFORE: A=[R2=?], B=[R0=5], C=[R1=3]
PC=0 [main] ADD AFTER:  A=[R2=8], B=[R0=5], C=[R1=3]
```

#### iABx格式（如LOADI, CLOSURE）
```
PC=1 [main] LOADI BEFORE: A=[R1=?], sBx=5
PC=1 [main] LOADI AFTER:  A=[R1=5], sBx=5
```

#### iAsBx格式（如JMP, FORLOOP）
```
PC=5 [main] JMP BEFORE: sJ=2
PC=5 [main] JMP AFTER:  sJ=2
```

#### iAx格式（如EXTRAARG）
```
PC=10 [main] EXTRAARG BEFORE: Ax=1000
PC=10 [main] EXTRAARG AFTER:  Ax=1000
```

### 5. 参数值显示格式规范

#### 寄存器参数 (R)
- **整数寄存器**：`A=[R0=42]`
- **浮点寄存器**：`B=[R1=3.14]`
- **字符串寄存器**：`C=[R2="hello"]`
- **函数寄存器**：`A=[R3=func@0x1234]`
- **未初始化**：`A=[R4=?]`
- **nil值**：`A=[R5=nil]`

#### 常量参数 (K)
- **整数常量**：`B=[K0=100]`
- **浮点常量**：`C=[K1=3.14159]`
- **字符串常量**：`B=[K2="world"]`
- **函数常量**：`C=[K3=func@0x5678]`

#### 立即数参数
- **有符号立即数**：`sBx=5`, `sC=-10`
- **大立即数**：`Ax=1000`
- **跳转偏移**：`sJ=2`

### 6. 函数名显示规范

#### 主函数
```
PC=0 [main] LOADI BEFORE: A=[R1=?], sBx=5
```

#### 命名函数（如果可获取）
```
PC=0 [multiply] MUL BEFORE: A=[R2=?], B=[R0=5], C=[R1=3]
```

#### 匿名函数（使用地址）
```
PC=0 [func@0x7f8990406010] MUL BEFORE: A=[R2=?], B=[R0=5], C=[R1=3]
```

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

### 编译标志和运行时标志的关系

#### 编译时标志（宏定义）
- **`AQL_PRODUCTION_BUILD`**：生产版本，所有调试代码编译为空操作
- **`AQL_DEBUG_BUILD`**：调试版本，包含所有调试功能
- **注意**：只需要两个编译标志，简化架构

#### 运行时调试标志（命令行参数）
- **`-v`**：全部调试信息（相当于 -vd + -vt + -vast + -vl + -vb）
- **`-vd`**：详细调试信息
- **`-vt`**：执行跟踪
- **`-vast`**：AST输出
- **`-vl`**：词法分析
- **`-vb`**：字节码输出

#### 宏定义层次关系
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

#### 实现要求
- **生产版本 (aql)**：`-DAQL_PRODUCTION_BUILD`，所有调试宏编译为空操作
- **调试版本 (aqlm/aqld)**：`-DAQL_DEBUG_BUILD`，根据运行时标志控制输出

#### 运行时标志检查机制

##### 全局状态变量
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

##### 宏定义示例
```c
// 生产版本：所有调试代码编译为空
#ifdef AQL_PRODUCTION_BUILD
    #define aql_info_vt(...)        ((void)0)
    #define aql_info_vd(...)        ((void)0)
    #define aql_info_vast(...)      ((void)0)
    #define AQL_INFO_VT_BEFORE()         ((void)0)
    #define AQL_INFO_VT_AFTER()          ((void)0)
    #define AQL_INFO_VT_LOADI_BEFORE()   ((void)0)
    #define AQL_INFO_VT_ADD_BEFORE()     ((void)0)
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

#### 性能优化要求
- **生产版本**：所有调试宏编译为空操作，零开销
- **调试版本**：运行时检查，避免不必要的计算
- **指令特定宏**：避免通用宏的复杂逻辑判断

#### 指令特定宏使用场景

##### 使用指令特定宏的优势
- **性能优化**：每个指令只包含必要的调试逻辑
- **代码清晰**：避免复杂的switch-case判断
- **维护简单**：新增指令只需添加对应的宏
- **编译优化**：编译器可以更好地优化每个宏

##### 使用示例
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

##### 指令特定宏定义要求
- **命名规范**：`AQL_INFO_VT_{指令名}_{阶段}()`
- **参数传递**：通过全局变量或参数传递指令信息
- **逻辑简化**：每个宏只处理一种指令的调试逻辑
- **性能优先**：避免不必要的计算和内存分配

## 调试信息分层显示

### Level 0: 生产版本 (aql)
- **无调试输出**
- **仅错误信息**
- **编译时零开销**

### Level 1: VM工具 (aqlm)
- **基础执行信息**
- **指令级跟踪 (-vt)**
- **详细调试信息 (-vd)**
- **字节码显示 (-vb)**

### Level 2: 完整调试 (aqld)
- **词法分析 (-vl)**
- **语法分析 (-vast)**
- **字节码生成 (-vb)**
- **执行跟踪 (-vt)**
- **详细调试 (-vd)**
- **内部调试信息**

## 实现要求

### 指令跟踪宏要求

#### 通用指令跟踪宏
- **AQL_INFO_VT_BEFORE()**：通用指令执行前状态显示
- **AQL_INFO_VT_AFTER()**：通用指令执行后状态显示
- **格式**：`PC=编号 [函数名] 指令名 BEFORE/AFTER: 参数列表`

#### 指令特定调试宏
- **AQL_INFO_VT_LOADI_BEFORE()**：LOADI指令特定的BEFORE逻辑
- **AQL_INFO_VT_ADD_BEFORE()**：ADD指令特定的BEFORE逻辑
- **AQL_INFO_VT_CALL_BEFORE()**：CALL指令特定的BEFORE逻辑
- **AQL_INFO_VT_JMP_BEFORE()**：JMP指令特定的BEFORE逻辑
- **优势**：每个指令可以有自己的调试逻辑，避免通用宏的复杂性

#### 宏定义层次
```
通用宏：
├── AQL_INFO_VT_BEFORE() - 通用BEFORE逻辑
└── AQL_INFO_VT_AFTER() - 通用AFTER逻辑

指令特定宏：
├── AQL_INFO_VT_LOADI_BEFORE() - LOADI特定逻辑
├── AQL_INFO_VT_ADD_BEFORE() - ADD特定逻辑
├── AQL_INFO_VT_CALL_BEFORE() - CALL特定逻辑
└── AQL_INFO_VT_JMP_BEFORE() - JMP特定逻辑
```

### 值显示宏要求

#### debug_print_value() 宏要求
- **功能**：智能显示TValue的值
- **类型识别**：自动识别整数、浮点、字符串、函数、nil等类型
- **字符串处理**：长字符串自动截断显示
- **特殊值处理**：未初始化显示`=?`，nil显示`=nil`
- **函数显示**：显示为`=func@地址`

### BEFORE/AFTER 区别说明

#### BEFORE阶段显示要求
- **目的**：显示指令执行前的状态
- **寄存器值**：显示`=?`表示未知或未初始化
- **常量值**：显示实际常量值（K参数）
- **立即数**：显示实际立即数值
- **格式**：`PC=0 [main] LOADI BEFORE: A=[R1=?], sBx=5`
- **宏调用**：`AQL_INFO_VT_BEFORE()` 或 `AQL_INFO_VT_LOADI_BEFORE()`

#### AFTER阶段显示要求
- **目的**：显示指令执行后的状态
- **寄存器值**：显示执行后的实际值
- **常量值**：显示实际常量值（K参数）
- **立即数**：显示实际立即数值
- **格式**：`PC=0 [main] LOADI AFTER: A=[R1=5], sBx=5`
- **宏调用**：`AQL_INFO_VT_AFTER()` 或 `AQL_INFO_VT_LOADI_AFTER()`

#### 关键区别
- **BEFORE**：目标寄存器显示`=?`，其他参数显示实际值
- **AFTER**：所有参数显示执行后的实际值
- **对比效果**：可以清楚看到指令执行前后的状态变化

### 指令格式和含义说明

#### 指令格式识别要求
- **iABC格式**：A、B、C三个参数，用于算术运算、比较等
- **iABx格式**：A参数 + Bx/sBx参数，用于加载、闭包创建等
- **iAsBx格式**：A参数 + sBx/sJ参数，用于跳转、循环控制等
- **iAx格式**：Ax参数，用于大立即数、额外参数等

#### 指令含义显示要求
- **算术指令**：显示运算类型和操作数
- **加载指令**：显示加载源和目标
- **跳转指令**：显示跳转条件和目标
- **函数指令**：显示函数调用和参数传递
- **控制指令**：显示控制流变化

#### 参数类型识别要求
- **寄存器参数**：自动识别R0-R255
- **常量参数**：自动识别K0-K255
- **立即数参数**：根据指令格式自动识别sBx、sC、Ax等
- **类型显示**：自动识别并显示整数、浮点、字符串、函数等类型

## 总结

### 核心设计原则

1. **编译时优化**：生产版本(aql)完全无调试代码，编译时零开销
2. **分层显示**：不同版本提供不同层次的调试信息
3. **智能解析**：指令参数根据类型自动解析显示
4. **格式统一**：所有调试输出都有清晰的结构和格式
5. **性能优先**：使用宏定义方式，避免运行时开销

### 版本定位

- **aql**：生产版本，高效执行，零调试开销
- **aqlm**：VM工具，验证指令正确性，基础调试功能
- **aqld**：开发版本，完整调试支持，全功能调试

### 关键特性

- **指令级跟踪**：BEFORE/AFTER状态对比
- **智能值显示**：自动识别R/K/立即数，字符串截断
- **函数调用跟踪**：完整的调用链和参数传递
- **分层控制**：细粒度的调试选项组合
- **零开销设计**：生产版本完全无调试代码