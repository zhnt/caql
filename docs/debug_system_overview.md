# AQL 调试系统综述

## 概述

AQL提供了一套完整的调试系统，支持从词法分析到虚拟机执行的全过程跟踪。调试系统设计参考了`minic`编译器的输出风格，提供结构化、易读的调试信息。

## 调试级别

### 基础调试标志

| 标志 | 功能 | 描述 |
|------|------|------|
| `-v` | 全部调试 | 启用所有调试输出 |
| `-vt` | 词法分析 | 显示Token序列和词法分析过程 |
| `-va` | AST分析 | 显示抽象语法树构建过程 |
| `-vb` | 字节码生成 | 显示字节码指令和常量池 |
| `-ve` | 虚拟机执行 | 显示VM执行跟踪和寄存器状态 |
| `-vr` | 寄存器状态 | 详细的寄存器分配和状态变化 |

## 调试输出格式

### 1. 词法分析 (`-vt`)

```
🔍 === LEXICAL ANALYSIS (Tokens) ===
   1: TK_LET       value=let (line 1, col 1)
   2: TK_NAME      value=sum (line 1, col 5)
   3: TK_ASSIGN    value== (line 1, col 9)
   4: TK_INT       value=0 (line 1, col 11)
   5: TK_FOR       value=for (line 2, col 1)
   6: TK_NAME      value=i (line 2, col 5)
```

**特点**：
- 清晰的Token索引和类型
- 包含源码位置信息（行号、列号）
- 显示Token的实际值

### 2. 字节码生成 (`-vb`)

```
⚙️  === BYTECODE INSTRUCTIONS ===
📦 Constants Pool:
  CONST[0] = "sum"
  CONST[1] = 0
  CONST[2] = 100
  CONST[3] = 10

📝 Instructions:
  PC    OPCODE       A        B        C       
  ---   ------       -        -        -       
  0     SETTABUP     0        128      129       # SETTABUP 0 128 129
  1     LOADI        0        0        128       # R(0) := 1
  2     LOADI        2        1        128       # R(2) := 3
  3     MOVE         1        2        0         # MOVE 1 2 0
  4     LOADI        4        0        128       # R(4) := 1
  5     MOVE         2        4        0         # MOVE 2 4 0
  6     FORPREP      0        10       128       # FORPREP 0 10 128

📊 Total instructions: 32
```

**特点**：
- 完整的常量池显示
- 统一的A, B, C表格格式（简化显示）
- 每条指令的详细注释
- 指令统计信息

**指令格式说明**：
AQL使用多种指令格式（iABC, iABx, iAsBx, iAx），但调试输出统一显示为A, B, C格式：
- **iABC格式**：`MOVE 1 2 0` → A=1, B=2, C=0
- **iAsBx格式**：`LOADI 0 0 128` → A=0, sBx=1 (显示为B=0, C=128)
- **iABx格式**：`LOADK 0 5 0` → A=0, Bx=5 (显示为B=5, C=0)
- **iAx格式**：`EXTRAARG 25 0 0` → Ax=25 (显示为A=25, B=0, C=0)

### 3. 虚拟机执行 (`-ve`)

```
🚀 === EXECUTION TRACE ===

📍 PC=0: SETTABUP 0 128 129
  Before: 📊 Registers: R0=nil R1=nil R2=nil R3=nil 
  After:  📊 Registers: R0=nil R1=nil R2=nil R3=nil 

📍 PC=1: LOADI 0 0 128  
  Before: 📊 Registers: R0=nil R1=nil R2=nil R3=nil 
  After:  📊 Registers: R0=1 R1=nil R2=nil R3=nil 

📍 PC=17: MUL 19 3 130
  Before: 📊 Registers: R0=1 R1=2 R2=1 R3=1 R19=nil 
  After:  📊 Registers: R0=1 R1=2 R2=1 R3=1 R19=100 

📍 PC=18: MUL 20 10 131
  Before: 📊 Registers: R0=1 R1=2 R2=1 R3=1 R10=1 R19=100 R20=nil 
  After:  📊 Registers: R0=1 R1=2 R2=1 R3=1 R10=1 R19=100 R20=10 

📍 PC=19: ADD 21 19 20
  Before: 📊 Registers: R19=100 R20=10 R21=nil 
  After:  📊 Registers: R19=100 R20=10 R21=110 
```

**特点**：
- 每条指令执行前后的寄存器状态对比
- 清晰的PC（程序计数器）跟踪
- 寄存器值的实时变化显示
- 支持复杂表达式的中间结果跟踪

## AQL指令集架构

### 指令格式详解

AQL虚拟机使用32位指令，支持4种基本格式：

```
指令位布局 (32位):
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
 |     C(8)      |      B(8)     |k|     A(8)     |   Op(7)     |  ← iABC/iABCk
 |           Bx(17)              |k|     A(8)     |   Op(7)     |  ← iABx  
 |          sBx(17)              |k|     A(8)     |   Op(7)     |  ← iAsBx
 |                   Ax(25)                       |   Op(7)     |  ← iAx
```

#### 格式类型

| 格式 | 参数 | 用途 | 示例指令 |
|------|------|------|----------|
| **iABC** | A, B, C | 三操作数运算 | `ADD`, `MUL`, `MOVE` |
| **iABx** | A, Bx | 常量加载 | `LOADK` (Bx=常量索引) |
| **iAsBx** | A, sBx | 立即数/跳转 | `LOADI` (sBx=立即数), `FORPREP` (sBx=跳转偏移) |
| **iAx** | Ax | 扩展参数 | `EXTRAARG` (Ax=扩展值) |

#### 调试输出映射

调试系统将所有格式统一显示为A, B, C列：

```c
// iABC: 直接映射
MOVE 1 2 0        → A=1, B=2, C=0

// iAsBx: sBx分解为B, C显示  
LOADI 0 0 128     → A=0, sBx=1 (内部: B=0, C=128)

// iABx: Bx显示在B列
LOADK 0 5 0       → A=0, Bx=5 (显示: B=5, C=0)

// iAx: Ax显示在A列
EXTRAARG 25 0 0   → Ax=25 (显示: A=25, B=0, C=0)
```

### 关键指令类型

#### 1. 算术指令 (iABC)
```
MUL 19 3 130      # R(19) := R(3) * K(130-BITRK)
ADD 21 19 20      # R(21) := R(19) + R(20)
```

#### 2. 加载指令 (iAsBx)
```
LOADI 0 0 128     # R(0) := 1 (sBx=1, 显示为B=0,C=128)
```

#### 3. 控制流指令 (iAsBx)
```
FORPREP 0 10 128  # 循环准备，sBx=跳转偏移
FORLOOP 0 10 128  # 循环回跳，sBx=跳转偏移
```

## 调试系统架构

### 核心组件

#### 1. 调试标志管理 (`src/adebug_user.h`)

```c
// 调试标志定义
#define AQL_DEBUG_LEX    0x01    // 词法分析
#define AQL_DEBUG_PARSE  0x02    // 语法分析  
#define AQL_DEBUG_CODE   0x04    // 字节码生成
#define AQL_DEBUG_VM     0x08    // 虚拟机执行
#define AQL_DEBUG_REG    0x10    // 寄存器状态
#define AQL_DEBUG_ALL    0xFF    // 全部调试

// VM状态结构
typedef struct {
    int pc;                    // 程序计数器
    const char* opname;        // 操作码名称
    const char* description;   // 指令描述
} AQL_VMState;
```

#### 2. 格式化输出函数 (`src/adebug_user.c`)

```c
// 词法分析调试
void aqlD_print_tokens_header(void);
void aqlD_print_token(int index, AQL_TokenInfo *token);
void aqlD_print_tokens_footer(void);

// 字节码调试
void aqlD_print_bytecode_header(void);
void aqlD_print_constants_pool(TValue *constants, int count);
void aqlD_print_instruction(AQL_InstrInfo *instr);
void aqlD_print_bytecode_footer(void);

// 虚拟机执行调试
void aqlD_print_execution_header(void);
void aqlD_print_vm_state(AQL_VMState *state);
void aqlD_print_registers(TValue *registers, int count);
void aqlD_print_execution_footer(TValue *result);
```

#### 3. 集成点

**词法分析器集成** (`src/alex.c`):
```c
#ifdef AQL_DEBUG_BUILD
if (aql_debug_enabled && (aql_debug_flags & AQL_DEBUG_LEX)) {
    aqlD_print_token(token_index++, &token_info);
}
#endif
```

**字节码生成集成** (`src/acode.c`):
```c
#ifdef AQL_DEBUG_BUILD
if (aql_debug_enabled && (aql_debug_flags & AQL_DEBUG_CODE)) {
    aqlD_print_instruction(&instr_info);
}
#endif
```

**虚拟机执行集成** (`src/avm.c`):
```c
#ifdef AQL_DEBUG_BUILD
if (trace_execution) {
    aqlD_print_vm_state(&vm_state);
    // 执行前寄存器状态
    printf("  Before: 📊 Registers: ");
    for (int j = 0; j < reg_count && j < 8; j++) {
        printf("R%d=%s ", j, format_value(&registers[j]));
    }
    printf("\n");
}
#endif

// 执行指令...

#ifdef AQL_DEBUG_BUILD  
if (trace_execution) {
    // 执行后寄存器状态
    printf("  After:  📊 Registers: ");
    for (int j = 0; j < reg_count && j < 8; j++) {
        printf("R%d=%s ", j, format_value(&registers[j]));
    }
    printf("\n\n");
}
#endif
```

## 实际应用案例

### 案例1：寄存器分配冲突调试

**问题代码**：
```aql
let val = i * 100 + j * 10 + k
```

**调试输出揭示问题**：
```
📍 PC=17: MUL 18 3 130        # R(18) := i * 100
📍 PC=18: MUL 18 10 131       # R(18) := j * 10  ← 覆盖了前面的结果！
📍 PC=19: ADD 17 18 18        # R(17) := R(18) + R(18)  ← 错误计算
```

**修复后的正确输出**：
```
📍 PC=17: MUL 19 3 130        # R(19) := i * 100
📍 PC=18: MUL 20 10 131       # R(20) := j * 10
📍 PC=19: ADD 21 19 20        # R(21) := R(19) + R(20)  ← 正确计算
```

### 案例2：嵌套循环变量作用域调试

**问题代码**：
```aql
for i = 2, 5, 1 {
    for j in range(i, i * 2, 1) {
        print(i * 10 + j)
    }
}
```

**调试输出显示问题**：
```
[DEBUG] forstat_range_to_numeric: start allocated to register 7
[DEBUG] forstat_range_to_numeric: stop allocated to register 7  ← 冲突！
```

**修复后的正确输出**：
```
[DEBUG] forstat_range_to_numeric: moved start from R7 to R8
[DEBUG] forstat_range_to_numeric: start allocated to register 8
[DEBUG] forstat_range_to_numeric: moved stop from R7 to R9  
[DEBUG] forstat_range_to_numeric: stop allocated to register 9
```

## 调试最佳实践

### 1. 渐进式调试

```bash
# 1. 先检查词法分析
./bin/aqld -vt program.aql

# 2. 再检查字节码生成
./bin/aqld -vb program.aql  

# 3. 最后检查执行过程
./bin/aqld -ve program.aql

# 4. 全面调试
./bin/aqld -v program.aql
```

### 2. 问题定位策略

**寄存器冲突问题**：
- 使用`-ve`查看寄存器状态变化
- 关注连续指令间的寄存器重用
- 检查`codebinexpval`调试输出

**循环控制问题**：
- 使用`-vb`查看`FORPREP`和`FORLOOP`指令
- 检查循环变量的寄存器分配
- 验证跳转地址的正确性

**变量作用域问题**：
- 使用`-vt`确认变量名解析
- 使用`-vb`检查`GETTABUP`/`SETTABUP`指令
- 使用`-ve`跟踪变量值的传递

### 3. 性能调试

**指令效率分析**：
```bash
# 统计指令类型分布
./bin/aqld -vb program.aql | grep -E "^\s*[0-9]+" | \
    awk '{print $2}' | sort | uniq -c | sort -nr
```

**寄存器使用分析**：
```bash
# 查看最高使用的寄存器编号
./bin/aqld -ve program.aql | grep -o "R[0-9]\+" | \
    sed 's/R//' | sort -n | tail -1
```

## 扩展功能

### 1. 条件调试

```c
// 只在特定PC范围内调试
#define DEBUG_PC_START 10
#define DEBUG_PC_END   20

if (pc >= DEBUG_PC_START && pc <= DEBUG_PC_END) {
    aqlD_print_vm_state(&vm_state);
}
```

### 2. 选择性寄存器跟踪

```c
// 只跟踪特定寄存器
int watch_registers[] = {0, 3, 18, 19, 20};
int watch_count = sizeof(watch_registers) / sizeof(int);

for (int i = 0; i < watch_count; i++) {
    int reg = watch_registers[i];
    printf("R%d=%s ", reg, format_value(&registers[reg]));
}
```

### 3. 调试输出重定向

```bash
# 将调试输出保存到文件
./bin/aqld -v program.aql 2> debug.log

# 只保留执行跟踪
./bin/aqld -ve program.aql 2>&1 | grep "📍\|📊" > execution.trace
```

## 总结

AQL的调试系统提供了：

1. **全面覆盖**：从词法分析到虚拟机执行的完整调试链
2. **结构化输出**：清晰的格式和丰富的视觉元素
3. **实时跟踪**：寄存器状态的即时变化显示
4. **问题定位**：快速识别寄存器冲突、作用域问题等
5. **性能分析**：指令效率和资源使用统计

这套调试系统在修复复杂的寄存器分配问题和嵌套循环bug中发挥了关键作用，大大提升了AQL编译器的开发和维护效率。
