# AQL 调试和诊断系统设计

## 1. 概述

AQL调试系统采用分层设计，提供从源码到机器码的全流程调试能力。借鉴minic的简洁命令行接口，为开发者提供易用且强大的调试工具。

## 2. 命令行接口设计

### 2.1 主命令接口

```bash
aql - AQL Compiler and Runtime

USAGE:
  aql [COMMAND] [OPTIONS] [FILE]

COMMANDS:
  compile    Compile AQL source code
  run        Run AQL program  
  debug      Interactive debugger
  profile    Performance analysis
  reflect    Reflection tools
  test       Run tests

GLOBAL OPTIONS:
  -h, --help      Show this help
  -v, --version   Show version
  --verbose       Verbose output
```

### 2.2 编译诊断命令

```bash
# 编译时诊断 (借鉴minic设计)
aql compile [OPTIONS] [FILE]

OPTIONS:
  -h, --help     Show this help
  -v             Show all compilation stages (equivalent to -vl -va -vb -vo -vm)
  -vl            Show tokens from lexical analysis  
  -va            Show abstract syntax tree (AST)
  -vb            Show bytecode instructions
  -vo            Show optimization passes
  -vm            Show machine code (JIT/AOT)
  -vd            Show debug information
  
  --ast-format   AST output format: tree, json, dot
  --bc-format    Bytecode format: text, hex, binary
  --target       Target: interpret, jit, aot, llvm
  --opt-level    Optimization level: 0, 1, 2, 3
  --debug-info   Include debug information
  --no-color     Disable colored output

EXAMPLES:
  aql compile -v example.aql                    # 显示所有编译阶段
  aql compile -vl -va example.aql              # 只显示词法和语法分析
  aql compile --ast-format=json example.aql    # 输出JSON格式AST
  aql compile --target=llvm -vo example.aql    # LLVM编译并显示优化
```

### 2.3 运行时调试命令

```bash
# 运行时调试
aql run [OPTIONS] [FILE] [ARGS...]

OPTIONS:
  -h, --help     Show this help
  -d, --debug    Enable interactive debugger
  -t, --trace    Show execution trace
  -r, --regs     Show register values during execution
  -m, --memory   Show memory usage
  -p, --profile  Enable profiling
  -w, --watch    Watch variable: -w var1,var2
  
  --break        Set breakpoint: --break file:line
  --step         Step execution mode
  --verbose      Verbose execution info

EXAMPLES:
  aql run -d example.aql                       # 交互式调试
  aql run -t -r example.aql                   # 显示执行跟踪和寄存器
  aql run --break example.aql:10 example.aql  # 在第10行设置断点
  aql run -w data,result example.aql          # 监视变量
```

## 3. 编译流程诊断

### 3.1 词法分析诊断

```bash
# 词法分析输出
$ aql compile -vl fibonacci.aql

=== LEXICAL ANALYSIS ===
位置    | 类型        | 值          | 属性
--------|-------------|-------------|----------
1:1     | FUNCTION    | function    | keyword
1:10    | IDENTIFIER  | fibonacci   | 
1:19    | LPAREN      | (           | delimiter
1:20    | IDENTIFIER  | n           | 
1:21    | COLON       | :           | operator
1:23    | TYPE        | int         | builtin_type
1:27    | RPAREN      | )           | delimiter
1:29    | ARROW       | ->          | operator
1:32    | TYPE        | int         | builtin_type

Tokens: 24, Errors: 0, Time: 0.8ms
```

### 3.2 语法树可视化

```bash
# AST树形显示
$ aql compile -va fibonacci.aql

=== ABSTRACT SYNTAX TREE ===
FunctionDecl: fibonacci (1:1-5:1)
├── Parameters:
│   └── Parameter: n (type: int)
├── ReturnType: int
└── Body: BlockStmt (2:1-5:1)
    └── IfStmt (2:3-4:3)
        ├── Condition: BinaryExpr (<=) (2:6-2:11)
        │   ├── Left: Identifier(n) (2:6)
        │   └── Right: IntLiteral(1) (2:10)
        ├── ThenBranch: ReturnStmt (2:13-2:20)
        │   └── Identifier(n) (2:20)
        └── ElseBranch: ReturnStmt (3:3-3:38)
            └── BinaryExpr (+) (3:10-3:38)
                ├── Left: CallExpr (3:10-3:24)
                │   ├── Callee: Identifier(fibonacci)
                │   └── Args: [BinaryExpr(-)]
                └── Right: CallExpr (3:27-3:38)

Nodes: 15, Depth: 6, Time: 1.2ms

# JSON格式AST (供工具使用)
$ aql compile -va --ast-format=json fibonacci.aql > fibonacci.ast.json
```

### 3.3 字节码分析

```bash
# 字节码反汇编
$ aql compile -vb fibonacci.aql

=== BYTECODE DISASSEMBLY ===
Function: fibonacci (locals: 8, upvalues: 0)

地址 | 指令      | 操作数     | 注释
-----|-----------|-----------|------------------
0000 | GETPARAM  | A=0       | R0 = n (parameter)
0001 | LOADK     | A=1 Bx=0  | R1 = 1 (constant)
0002 | LE        | A=0 B=0 C=1| if R0 <= R1 then skip
0003 | JMP       | sBx=2     | goto 0006
0004 | MOVE      | A=2 B=0   | R2 = R0 (return n)
0005 | RETURN    | A=2 B=2   | return R2
0006 | LOADK     | A=3 Bx=0  | R3 = 1
0007 | SUB       | A=4 B=0 C=3| R4 = R0 - R3 (n-1)
0008 | CALL      | A=5 B=4 C=2| R5 = fibonacci(R4)
0009 | LOADK     | A=6 Bx=1  | R6 = 2
000A | SUB       | A=7 B=0 C=6| R7 = R0 - R6 (n-2)
000B | CALL      | A=8 B=7 C=2| R8 = fibonacci(R7)
000C | ADD       | A=9 B=5 C=8| R9 = R5 + R8
000D | RETURN    | A=9 B=2   | return R9

Instructions: 14, Constants: 2, Size: 56 bytes
```

### 3.4 优化过程展示

```bash
# 优化过程显示
$ aql compile -vo --opt-level=2 fibonacci.aql

=== OPTIMIZATION PASSES ===
Pass 1: Dead Code Elimination
  - Removed 0 dead instructions
  
Pass 2: Constant Folding  
  - Folded 2 constant expressions
  - Before: LOADK R1, 1; LOADK R2, 1; ADD R3, R1, R2
  - After:  LOADK R3, 2
  
Pass 3: Register Allocation
  - Reduced register usage: 9 -> 6
  - Eliminated redundant moves: 3
  
Pass 4: Jump Optimization
  - Optimized 1 conditional jump
  - Eliminated 1 unnecessary jump
  
Pass 5: Function Inlining (skipped - recursive function)

Total: 6 optimizations applied, 12% size reduction
```

## 4. 交互式调试器

### 4.1 调试器命令

```bash
# 启动调试器
$ aql debug fibonacci.aql

AQL Debugger v1.0 - Type 'help' for commands
Loading: fibonacci.aql...
Ready.

(aqldb) help
Debugger Commands:
  break, b <location>    Set breakpoint (file:line or function)
  run, r [args]          Start/restart program
  continue, c            Continue execution
  step, s                Step into
  next, n                Step over
  finish, f              Step out
  
  print, p <expr>        Print expression value
  info <what>            Show information (vars, regs, stack, bt)
  watch <var>            Watch variable changes
  unwatch <var>          Remove watch
  
  disasm [addr]          Show disassembly
  memory [addr] [len]    Show memory contents
  registers              Show register state
  
  list, l [file:line]    Show source code
  quit, q                Exit debugger

(aqldb) break fibonacci:3
Breakpoint 1 set at fibonacci:3

(aqldb) run 10
Starting program with args: [10]

Breakpoint 1 hit: fibonacci:3
3:    return fibonacci(n-1) + fibonacci(n-2);

(aqldb) print n
n = 10 (int)

(aqldb) info regs
Register State:
  R0 = 10    (n - parameter)
  R1 = 1     (constant)  
  R2 = 0     (uninitialized)
  R3 = 0     (uninitialized)

(aqldb) watch n
Watch added: n

(aqldb) step
Executing: SUB R4, R0, R3  # R4 = n - 1
R4 = 9

(aqldb) continue
Watch: n changed 10 -> 9 at fibonacci:3
Breakpoint 1 hit: fibonacci:3

(aqldb) info bt
Call Stack:
  #0  fibonacci(n=9) at fibonacci.aql:3
  #1  fibonacci(n=10) at fibonacci.aql:3  
  #2  main() at fibonacci.aql:8

(aqldb) quit
```

## 5. 性能分析工具

### 5.1 性能分析命令

```bash
# 性能分析
aql profile [OPTIONS] [FILE]

OPTIONS:
  --mode=MODE        Profiling mode: cpu, memory, cache, ai
  --output=FORMAT    Output format: text, json, html
  --duration=SEC     Profile duration (for server apps)
  --threshold=PCT    Show functions above threshold
  --sampling=FREQ    Sampling frequency (Hz)

EXAMPLES:
  aql profile --mode=cpu example.aql           # CPU性能分析
  aql profile --mode=memory --output=html      # 内存分析HTML报告
  aql profile --mode=ai neural_net.aql        # AI工作负载分析
```

### 5.2 CPU性能分析输出

```bash
$ aql profile --mode=cpu fibonacci.aql 10

=== CPU PROFILING REPORT ===
Total runtime: 89.2ms
Sample count: 8920 (100Hz sampling)

Hot Functions:
函数名         | 调用次数  | 总时间   | 平均时间  | 百分比 | 指令数
---------------|----------|----------|----------|-------|--------
fibonacci      | 177      | 85.1ms   | 481μs    | 95.4% | 2478
main           | 1        | 89.2ms   | 89.2ms   | 4.6%  | 12

Hot Instructions:
指令           | 执行次数  | 时间占比  | 优化建议
---------------|----------|----------|------------------
CALL           | 176      | 52.3%    | ⚠️  递归调用开销大，考虑尾递归优化
SUB            | 354      | 18.7%    | ✅ 已优化
ADD            | 88       | 12.1%    | ✅ 已优化
RETURN         | 177      | 8.9%     | ✅ 已优化

JIT Statistics:
函数           | 编译次数  | 编译时间  | 优化级别 | 加速比
---------------|----------|----------|----------|--------
fibonacci      | 2        | 2.3ms    | O2→O3    | 15.2x
main           | 1        | 0.4ms    | O1       | 3.1x

Recommendations:
🔧 fibonacci函数递归深度过大，建议使用迭代实现
🚀 可考虑启用尾递归优化: --opt-tail-recursion
📊 90%时间花在递归调用上，符合算法特征
```

### 5.3 内存分析

```bash
$ aql profile --mode=memory data_processing.aql

=== MEMORY PROFILING REPORT ===
Peak memory: 12.4MB
Current usage: 8.7MB
GC cycles: 15

Memory Timeline:
时间(ms) | 堆使用  | GC事件    | 说明
---------|---------|----------|------------------
0        | 0.1MB   | -        | 程序启动
150      | 2.3MB   | -        | 数据加载
280      | 5.8MB   | Minor GC | 清理临时对象
420      | 12.4MB  | -        | 峰值使用
450      | 8.1MB   | Major GC | 大量回收
600      | 8.7MB   | -        | 稳定状态

Allocation Hotspots:
类型            | 分配次数  | 总大小    | 平均大小 | 位置
----------------|----------|-----------|----------|------------------
slice<float>    | 1247     | 6.2MB     | 5.1KB    | process_data:23
dict<str,int>   | 89       | 1.8MB     | 20.7KB   | load_config:45
string          | 5623     | 1.1MB     | 200B     | 多处
vector<float,8> | 890      | 28KB      | 32B      | vectorize:67

GC Performance:
类型     | 次数 | 总时间  | 平均时间 | 暂停时间
---------|------|---------|----------|----------
Minor GC | 12   | 3.2ms   | 267μs    | < 1ms
Major GC | 3    | 8.1ms   | 2.7ms    | < 1ms
Total    | 15   | 11.3ms  | 753μs    | < 1ms ✅

Memory Leaks: None detected ✅
```

## 6. AI专用调试

### 6.1 AI工作负载分析

```bash
# AI专用分析
$ aql profile --mode=ai neural_network.aql

=== AI WORKLOAD ANALYSIS ===
Model: Neural Network Classifier
Input shape: [batch=32, features=784]
Output shape: [batch=32, classes=10]

Layer Performance:
层名称        | 输入形状      | 输出形状      | 时间(ms) | GPU利用率
-------------|---------------|---------------|----------|----------
input        | [32, 784]     | [32, 784]     | 0.1      | 0%
dense_1      | [32, 784]     | [32, 128]     | 12.3     | 87%
activation_1 | [32, 128]     | [32, 128]     | 0.8      | 45%
dropout_1    | [32, 128]     | [32, 128]     | 0.2      | 12%
dense_2      | [32, 128]     | [32, 10]      | 2.1      | 78%
softmax      | [32, 10]      | [32, 10]      | 0.3      | 23%

Vectorization Analysis:
函数                | 向量化率 | SIMD指令 | 加速比 | 瓶颈
--------------------|----------|----------|-------|--------
matrix_multiply     | 95%      | AVX2     | 7.2x  | 内存带宽
activation_relu     | 100%     | AVX2     | 8.1x  | -
batch_normalize     | 78%      | SSE4.1   | 3.4x  | 分支预测

Recommendations:
🚀 dense_1层是性能瓶颈，占总时间76%
🔧 考虑批量大小优化：32 → 64 (提升GPU利用率)
⚡ activation_1可以与dense_1融合减少内存访问
📊 整体GPU利用率49%，有优化空间
```

### 6.2 数据流可视化

```bash
# 数据流跟踪
$ aql debug --ai-trace data_pipeline.aql

=== DATA FLOW TRACE ===
Pipeline: Market Data Analysis

输入数据: market_data.csv [rows=10000, cols=5]
↓ (load_csv)
DataFrame: [10000×5] float64, 390KB
│ columns: [open, high, low, close, volume]
│ memory: heap allocated
↓ (calculate_sma)  
Rolling Mean: [9980×1] float64, 78KB
│ window: 20 periods
│ vectorized: ✅ AVX2 (4x speedup)
↓ (calculate_volatility)
Volatility: [9970×1] float64, 78KB  
│ window: 10 periods
│ algorithm: rolling_std
│ vectorized: ✅ AVX2 (6x speedup)
↓ (risk_assessment)
Risk Scores: [9970×3] float64, 234KB
│ features: [trend, volatility, momentum]
│ model: logistic_regression
│ inference_time: 2.3ms
↓ (generate_signals)
Signals: [9970×1] int8, 10KB
│ classes: [buy=1, hold=0, sell=-1]
│ compression: 95% space saving

总处理时间: 45ms
内存峰值: 1.2MB
向量化覆盖率: 87%
```

## 7. 反射调试工具

### 7.1 反射信息查看

```bash
# 反射工具
aql reflect [OPTIONS] [FILE]

OPTIONS:
  --type=TYPE        Show type information
  --function=FUNC    Show function signature  
  --struct=STRUCT    Show struct layout
  --module=MODULE    Show module exports
  --annotations      Show annotation info

EXAMPLES:
  aql reflect --type=slice example.aql        # 显示slice类型信息
  aql reflect --function=process_data          # 显示函数签名
  aql reflect --struct=Person                 # 显示结构体布局
```

### 7.2 反射信息输出

```bash
$ aql reflect --struct=Person example.aql

=== STRUCT REFLECTION ===
Type: Person
Size: 24 bytes
Alignment: 8 bytes
Location: example.aql:15:1

Fields:
名称     | 类型      | 偏移  | 大小 | 对齐 | 注解
---------|-----------|-------|------|------|----------
name     | string    | 0     | 8    | 8    | @validate("required")
age      | int       | 8     | 8    | 8    | @range(0, 150)
email    | string    | 16    | 8    | 8    | @validate("email")

Methods:
名称      | 签名                          | 位置
----------|-------------------------------|----------
new       | (string, int, string) -> Person | example.aql:19
greet     | (self) -> string              | example.aql:23
validate  | (self) -> bool                | example.aql:27

Memory Layout:
[0-7   ] name     (string pointer)
[8-15  ] age      (64-bit integer)  
[16-23 ] email    (string pointer)

Annotations:
@Serializable
@JsonMapping(snake_case=true)
```

## 8. IDE集成

### 8.1 语言服务器功能

```json
{
  "capabilities": {
    "textDocumentSync": "incremental",
    "completionProvider": true,
    "hoverProvider": true,
    "definitionProvider": true,
    "referencesProvider": true,
    "documentFormattingProvider": true,
    "diagnosticsProvider": true,
    "codeActionProvider": true,
    
    "aql": {
      "bytecodeVisualization": true,
      "performanceHints": true,
      "vectorizationAnalysis": true,
      "aiDebugging": true,
      "reflectionInfo": true
    }
  }
}
```

### 8.2 VS Code调试配置

```json
{
  "type": "aql",
  "request": "launch", 
  "name": "Debug AQL Program",
  "program": "${workspaceFolder}/main.aql",
  "args": ["--input", "data.csv"],
  "stopOnEntry": false,
  "console": "integratedTerminal",
  "aql": {
    "showBytecode": true,
    "enableProfiling": true,
    "aiVisualization": true,
    "optimizationLevel": 2
  }
}
```

## 9. 实现优先级

### 9.1 MVP版本 (v0.1)
- ✅ 基础命令行接口 (`-vl`, `-va`, `-vb`)
- ✅ 字节码反汇编器
- ✅ 简单交互式调试器 (断点、单步、变量查看)
- ✅ 基础性能分析 (函数热点)

### 9.2 稳定版本 (v1.0) 
- ✅ 完整调试器功能 (调用栈、内存查看、监视点)
- ✅ 优化过程可视化
- ✅ AI工作负载分析
- ✅ LSP集成

### 9.3 增强版本 (v2.0+)
- ⚠️ 高级可视化 (数据流图、调用图)
- ⚠️ 远程调试支持
- ⚠️ 分布式系统调试
- ⚠️ 性能优化建议引擎

## 10. 总结

AQL调试系统的核心设计理念：

1. **简洁易用**: 借鉴minic的命令行设计，一键查看所有信息
2. **分层诊断**: 从词法到机器码的完整可视化
3. **AI特化**: 专门的AI工作负载调试功能  
4. **性能导向**: 实时性能分析和优化建议
5. **开发友好**: 强大的IDE集成和反射支持

通过这套调试系统，AQL将为开发者提供业界领先的调试体验，特别是在AI应用开发领域！ 