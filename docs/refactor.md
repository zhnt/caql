# AQL 重构规划设计

## 🎯 总体目标

工欲善其事，必先利其器。本重构旨在：
1. 打造强大的调试工具，让字节码成为调试程序的最核心利器
2. 与 Lua 保持最大兼容性，便于学习和迁移
3. 为未来扩展（类、对象、JIT、协程）留出清晰的架构空间
4. 建立完善的 AST 和字节码输出系统

## 1. **调试工具重构 (aqld)**

### **目标**：打造与 `luac -l` 同等强大的字节码调试工具

### **基本用法**：
```bash
aqld -vb script.aql             # 只输出字节码 (类似 luac -l)
aqld -v script.aql              # 详细模式 (词法+ AST +字节码 + 执行跟踪)
aqld -vast script.aql           # 只输出 AST
aqld -vt script.aql             # 输出执行跟踪
aqld -vl script.aql             # 输出词流
aqld -compare lua_file.lua      # 与 Lua 字节码对比
aqld -vd                        # 详细日志输出 等于print_debug激活+ -v模式
```

### **输出格式设计**：
```
=== AQL BYTECODE ANALYSIS ===
📁 File: test.aql
📊 Stats: 15 instructions, 3 constants, 2 functions

🔤 Lexical Analysis (Token Stream):
   0: FUNCTION     value=function (line 3, col 9)
   1: IDENTIFIER   value=get42 (line 3, col 17)
   2: LBRACE       value={ (line 3, col 21)
   3: RETURN       value=return (line 4, col 11)
   4: INTEGER      value=42 (line 4, col 18)
   5: RBRACE       value=} (line 5, col 2)
   ...

🏗️  AST Structure:
├── FunctionDef: main
│   ├── FunctionDef: get42
│   │   └── ReturnStmt: 42
│   └── CallExpr: print
│       ├── StringLit: "value:"
│       └── CallExpr: get42

📦 Constants Pool:
  K[0] = "get42"     (string)
  K[1] = "value:"    (string)

🔧 Functions:
  F[0] = main <test.aql:1,6> (9 instructions)
  F[1] = get42 <test.aql:2,4> (3 instructions)

📝 Bytecode - main function:
  PC   OPCODE      A    B    C    Description
  ---  ------      -    -    -    -----------
  0    GETTABUP    0    0    0    ; R[0] = _ENV["get42"]
  1    CLOSURE     1    0    -    ; R[1] = closure(F[0])
  2    SETTABUP    0    0    1    ; _ENV["get42"] = R[1]
  3    GETTABUP    0    0    1    ; R[0] = _ENV["print"] 
  4    LOADK       1    1    -    ; R[1] = K[1] ("value:")
  5    GETTABUP    2    0    0    ; R[2] = _ENV["get42"]
  6    CALL        2    1    0    ; R[2] = get42() (all results)
  7    CALL        0    0    1    ; print(R[1], R[2]...) 
  8    RETURN      0    1    -    ; return

📝 Bytecode - get42 function:
  PC   OPCODE      A    B    C    Description
  ---  ------      -    -    -    -----------
  0    LOADI       0    42   -    ; R[0] = 42
  1    RETURN1     0    -    -    ; return R[0]
  2    RETURN0     -    -    -    ; return (end)

🎯 Register Usage Analysis:
  main: R[0-2] used, max stack = 3
  get42: R[0] used, max stack = 1

🔍 Execution Trace (aqld -vt):
  [ENTER] main function
  [PC=0] GETTABUP R[0] = _ENV["get42"] -> nil
  [PC=1] CLOSURE R[1] = <function get42>
  [PC=2] SETTABUP _ENV["get42"] = R[1]
  [PC=3] GETTABUP R[0] = _ENV["print"] -> <builtin print>
  [PC=4] LOADK R[1] = "value:"
  [PC=5] GETTABUP R[2] = _ENV["get42"] -> <function get42>
  [PC=6] CALL R[2] = get42()
    [ENTER] get42 function
    [PC=0] LOADI R[0] = 42
    [PC=1] RETURN1 return R[0] (42)
    [EXIT] get42 -> 42
  [PC=7] CALL print(R[1], R[2]) -> "value:" 42
  [PC=8] RETURN
  [EXIT] main

✅ Lua Compatibility: 95% (missing: VARARG support)
```

## 2. **AST 输出重构**

### **设计原则**：
- 采用标准的树形结构显示
- 支持多种输出格式：文本、JSON、DOT (Graphviz)
- 包含位置信息、类型信息、作用域信息

### **AST 节点设计**：
```c
typedef enum {
  AST_PROGRAM,
  AST_FUNCTION_DEF,
  AST_RETURN_STMT,
  AST_IF_STMT,
  AST_FOR_STMT,
  AST_WHILE_STMT,
  AST_CALL_EXPR,
  AST_BINARY_EXPR,
  AST_UNARY_EXPR,
  AST_IDENTIFIER,
  AST_LITERAL,
  // AQL 扩展
  AST_CLASS_DEF,
  AST_METHOD_DEF,
  AST_PROPERTY_ACCESS,
  AST_RANGE_EXPR,
  AST_CONTAINER_EXPR
} ASTNodeType;

typedef struct ASTNode {
  ASTNodeType type;
  SourceLocation loc;
  struct ASTNode **children;
  int child_count;
  union {
    struct { TString *name; } identifier;
    struct { TValue value; } literal;
    struct { BinOpr op; } binary;
    // ...
  } data;
} ASTNode;
```

### **AST 输出格式**：
```json
{
  "type": "Program",
  "location": { "line": 1, "column": 1, "file": "test.aql" },
  "children": [
    {
      "type": "FunctionDef",
      "name": "get42",
      "location": { "line": 2, "column": 1 },
      "parameters": [],
      "body": {
        "type": "ReturnStmt",
        "value": {
          "type": "IntegerLiteral",
          "value": 42,
          "location": { "line": 3, "column": 12 }
        }
      }
    }
  ]
}
```

## 3. **字节码 Lua 兼容性设计**

### **兼容性目标**：
- **核心指令 100% 兼容**：MOVE, LOADK, GETTABUP, SETTABUP, CALL, RETURN 等
- **扩展指令清晰标识**：从 100 开始，避免与未来 Lua 指令冲突
- **指令编码格式完全一致**：ABC, ABx, AsBx, Ax 格式

### **指令映射表**：
```c
// Lua 兼容指令 (0-46) - 与 Lua 5.4 完全一致
OP_MOVE = 0,      // 完全兼容
OP_LOADI = 1,     // 完全兼容  
OP_LOADF = 2,     // 完全兼容
OP_LOADK = 3,     // 完全兼容
OP_LOADKX = 4,    // 完全兼容
OP_LOADFALSE = 5, // 完全兼容
OP_LFALSESKIP = 6,// 完全兼容
OP_LOADTRUE = 7,  // 完全兼容
OP_LOADNIL = 8,   // 完全兼容
OP_GETUPVAL = 9,  // 完全兼容
OP_SETUPVAL = 10, // 完全兼容
OP_GETTABUP = 11, // 完全兼容
OP_GETTABLE = 12, // 完全兼容
OP_GETI = 13,     // 完全兼容
OP_GETFIELD = 14, // 完全兼容
OP_SETTABUP = 15, // 完全兼容
OP_SETTABLE = 16, // 完全兼容
OP_SETI = 17,     // 完全兼容
OP_SETFIELD = 18, // 完全兼容
OP_NEWTABLE = 19, // 完全兼容
OP_SELF = 20,     // 完全兼容
OP_ADDI = 21,     // 完全兼容
OP_ADDK = 22,     // 完全兼容
OP_SUBK = 23,     // 完全兼容
OP_MULK = 24,     // 完全兼容
OP_MODK = 25,     // 完全兼容
OP_POWK = 26,     // 完全兼容
OP_DIVK = 27,     // 完全兼容
OP_IDIVK = 28,    // 完全兼容
OP_BANDK = 29,    // 完全兼容
OP_BORK = 30,     // 完全兼容
OP_BXORK = 31,    // 完全兼容
OP_SHRI = 32,     // 完全兼容
OP_SHLI = 33,     // 完全兼容
OP_ADD = 34,      // 完全兼容
OP_SUB = 35,      // 完全兼容
OP_MUL = 36,      // 完全兼容
OP_MOD = 37,      // 完全兼容
OP_POW = 38,      // 完全兼容
OP_DIV = 39,      // 完全兼容
OP_IDIV = 40,     // 完全兼容
OP_BAND = 41,     // 完全兼容
OP_BOR = 42,      // 完全兼容
OP_BXOR = 43,     // 完全兼容
OP_SHL = 44,      // 完全兼容
OP_SHR = 45,      // 完全兼容
OP_MMBIN = 46,    // 完全兼容
OP_MMBINI = 47,   // 完全兼容
OP_MMBINK = 48,   // 完全兼容
OP_UNM = 49,      // 完全兼容
OP_BNOT = 50,     // 完全兼容
OP_NOT = 51,      // 完全兼容
OP_LEN = 52,      // 完全兼容
OP_CONCAT = 53,   // 完全兼容
OP_CLOSE = 54,    // 完全兼容
OP_TBC = 55,      // 完全兼容
OP_JMP = 56,      // 完全兼容
OP_EQ = 57,       // 完全兼容
OP_LT = 58,       // 完全兼容
OP_LE = 59,       // 完全兼容
OP_EQK = 60,      // 完全兼容
OP_EQI = 61,      // 完全兼容
OP_LTI = 62,      // 完全兼容
OP_LEI = 63,      // 完全兼容
OP_GTI = 64,      // 完全兼容
OP_GEI = 65,      // 完全兼容
OP_TEST = 66,     // 完全兼容
OP_TESTSET = 67,  // 完全兼容
OP_CALL = 68,     // 完全兼容
OP_TAILCALL = 69, // 完全兼容
OP_RETURN = 70,   // 完全兼容
OP_RETURN0 = 71,  // 完全兼容
OP_RETURN1 = 72,  // 完全兼容
OP_FORLOOP = 73,  // 完全兼容
OP_FORPREP = 74,  // 完全兼容
OP_TFORPREP = 75, // 完全兼容
OP_TFORCALL = 76, // 完全兼容
OP_TFORLOOP = 77, // 完全兼容
OP_SETLIST = 78,  // 完全兼容
OP_CLOSURE = 79,  // 完全兼容
OP_VARARG = 80,   // 完全兼容
OP_VARARGPREP = 81, // 完全兼容
OP_EXTRAARG = 82, // 完全兼容

// AQL 扩展指令 (100+) - 避免与未来 Lua 指令冲突
OP_RANGE = 100,       // range(start, stop, step)
OP_CONTAINER = 101,   // 容器操作
OP_CLASS_NEW = 102,   // 类实例化
OP_METHOD_CALL = 103, // 方法调用
OP_PROPERTY_GET = 104,// 属性访问
OP_PROPERTY_SET = 105,// 属性设置
OP_JIT_COMPILE = 106, // JIT 编译标记
OP_SOW_YIELD = 107,   // SOW 协程让出
OP_SOW_RESUME = 108,  // SOW 协程恢复
OP_ASYNC_CALL = 109,  // 异步函数调用
OP_AWAIT = 110,       // await 操作
```

## 4. **acode.c 重构设计**

### **目标**：与 Lua 的 lcode.c 保持最大兼容性

### **重构策略**：
```c
// 1. 完全采用 Lua 的函数签名和逻辑
void aqlK_exp2nextreg(FuncState *fs, expdesc *e);  // = luaK_exp2nextreg
int aqlK_exp2anyreg(FuncState *fs, expdesc *e);    // = luaK_exp2anyreg
void aqlK_setreturns(FuncState *fs, expdesc *e, int nresults); // = luaK_setreturns
void aqlK_setoneret(FuncState *fs, expdesc *e);    // = luaK_setoneret
void aqlK_dischargevars(FuncState *fs, expdesc *e); // = luaK_dischargevars
void aqlK_exp2val(FuncState *fs, expdesc *e);      // = luaK_exp2val

// 2. 扩展函数使用 aqlK_ext_ 前缀
void aqlK_ext_range(FuncState *fs, expdesc *e, expdesc *start, expdesc *stop, expdesc *step);
void aqlK_ext_class_new(FuncState *fs, expdesc *e, int class_id);
void aqlK_ext_method_call(FuncState *fs, expdesc *obj, expdesc *method, expdesc *args);
void aqlK_ext_container_literal(FuncState *fs, expdesc *e, expdesc *elements, int count);

// 3. 寄存器管理完全兼容 Lua
#define MAXREGS     255  // 与 Lua 一致
#define NO_REG      MAXREGS  // 与 Lua 一致

// 4. 表达式类型完全兼容 Lua
typedef enum {
  VVOID = 0,    // 与 Lua 一致
  VNIL,         // 与 Lua 一致
  VTRUE,        // 与 Lua 一致
  VFALSE,       // 与 Lua 一致
  VK,           // 与 Lua 一致
  VKFLT,        // 与 Lua 一致
  VKINT,        // 与 Lua 一致
  VKSTR,        // 与 Lua 一致
  VNONRELOC,    // 与 Lua 一致
  VLOCAL,       // 与 Lua 一致
  VUPVAL,       // 与 Lua 一致
  VINDEXED,     // 与 Lua 一致
  VINDEXUP,     // 与 Lua 一致
  VINDEXI,      // 与 Lua 一致
  VINDEXSTR,    // 与 Lua 一致
  VJMP,         // 与 Lua 一致
  VRELOC,       // 与 Lua 一致
  VCALL,        // 与 Lua 一致
  VVARARG,      // 与 Lua 一致
  // AQL 扩展
  VRANGE,       // range 表达式
  VCLASS,       // 类表达式
  VMETHOD,      // 方法表达式
} expkind;
```

## 5. **avm.c 重构设计**

### **目标**：虚拟机核心与 Lua 保持一致，扩展功能模块化

### **架构设计**：
```c
// 核心执行循环 - 与 Lua lvm.c 保持一致
void aqlV_execute(lua_State *L) {
  // 完全采用 Lua 的 VM 循环结构
  // 只在扩展指令处添加 AQL 特有逻辑
  
  vmfetch(); // 与 Lua 一致的指令获取
  
  switch (GET_OPCODE(i)) {
    // Lua 兼容指令 (0-82) - 完全采用 Lua 的实现
    case OP_MOVE: case OP_LOADI: case OP_LOADF: /* ... */
      // 直接使用 Lua 的指令实现
      
    // AQL 扩展指令 (100+)
    case OP_RANGE:
      aqlV_ext_range(L, i);
      break;
    case OP_CLASS_NEW:
      aqlV_ext_class_new(L, i);
      break;
    // ...
  }
}

// 扩展功能模块化
typedef struct AQLExtensions {
  // 容器支持
  ContainerOps *container_ops;
  // 类系统支持  
  ClassSystem *class_system;
  // JIT 支持
  JITCompiler *jit_compiler;
  // 协程支持
  CoroutineScheduler *coroutine_scheduler;
} AQLExtensions;

// 扩展指令处理器
typedef int (*ExtInstructionHandler)(lua_State *L, Instruction i);
ExtInstructionHandler ext_handlers[NUM_EXT_OPCODES];

// 注册扩展指令处理器
void aqlV_register_ext_handler(OpCode op, ExtInstructionHandler handler) {
  if (op >= 100) {
    ext_handlers[op - 100] = handler;
  }
}
```

## 6. **aparser.c 重构设计**

### **目标**：解析器与 Lua lparser.c 保持核心兼容，语法糖层次化

### **分层设计**：
```c
// 第一层：Lua 兼容层 (完全兼容 Lua 语法)
static void lua_compat_expr(LexState *ls, expdesc *v);
static void lua_compat_stat(LexState *ls);
static void lua_compat_funcargs(LexState *ls, expdesc *f);
static void lua_compat_explist(LexState *ls, expdesc *v);
static void lua_compat_suffixedexp(LexState *ls, expdesc *v);

// 第二层：AQL 语法糖层
static void aql_sugar_class_def(LexState *ls);           // class 语法
static void aql_sugar_range_expr(LexState *ls, expdesc *v); // range 语法
static void aql_sugar_container_literal(LexState *ls, expdesc *v); // 容器字面量
static void aql_sugar_method_call(LexState *ls, expdesc *v); // 方法调用语法糖
static void aql_sugar_property_access(LexState *ls, expdesc *v); // 属性访问

// 第三层：未来扩展层
static void aql_future_pattern_match(LexState *ls);     // 模式匹配
static void aql_future_async_await(LexState *ls);       // async/await
static void aql_future_sow_syntax(LexState *ls);        // SOW 语法
static void aql_future_macro_expansion(LexState *ls);   // 宏展开

// 统一入口 - 根据 token 类型分发到不同层
static void aql_expr(LexState *ls, expdesc *v) {
  switch (ls->t.token) {
    // Lua 兼容语法
    case TK_NAME: case TK_STRING: case TK_INT: /* ... */
      lua_compat_expr(ls, v);
      break;
      
    // AQL 语法糖
    case TK_CLASS:
      aql_sugar_class_def(ls);
      break;
    case TK_RANGE:
      aql_sugar_range_expr(ls, v);
      break;
      
    // 未来扩展语法
    case TK_ASYNC:
      aql_future_async_await(ls);
      break;
      
    default:
      lua_compat_expr(ls, v);
  }
}
```

## 7. **扩展架构设计**

### **类和对象系统**：
```c
// aclass.h - 类系统核心
typedef struct AQLClass {
  GCObject *next;           // GC 链表
  lu_byte tt;               // 类型标记
  TString *name;            // 类名
  struct AQLClass *superclass; // 父类
  Table *methods;           // 方法表
  Table *properties;        // 属性表
  Table *static_methods;    // 静态方法
  Table *static_properties; // 静态属性
  int instance_size;        // 实例大小
  lu_byte flags;            // 类标志 (abstract, final, etc.)
} AQLClass;

typedef struct AQLObject {
  GCObject *next;           // GC 链表
  lu_byte tt;               // 类型标记
  AQLClass *class;          // 所属类
  Table *properties;        // 实例属性
} AQLObject;

// 字节码扩展
OP_CLASS_DEF = 102,       // 定义类
OP_CLASS_NEW = 103,       // 创建实例  
OP_METHOD_CALL = 104,     // 方法调用
OP_SUPER_CALL = 105,      // 超类调用
OP_PROPERTY_GET = 106,    // 属性获取
OP_PROPERTY_SET = 107,    // 属性设置
OP_INSTANCEOF = 108,      // instanceof 检查
```

### **JIT 编译器接口**：
```c
// ajit.h - JIT 编译器接口
typedef struct AQLJITCompiler {
  const char *name;                    // JIT 名称 (LLVM, Custom, etc.)
  int (*can_compile)(Proto *p);        // 是否可以编译
  void* (*compile)(Proto *p);          // 编译函数
  int (*execute)(void *compiled_code, lua_State *L); // 执行编译后代码
  void (*cleanup)(void *compiled_code); // 清理资源
} AQLJITCompiler;

// 热点检测
typedef struct HotSpot {
  Proto *proto;             // 函数原型
  int call_count;           // 调用次数
  int compile_threshold;    // 编译阈值
  void *compiled_code;      // 编译后代码
  AQLJITCompiler *compiler; // 使用的编译器
} HotSpot;

// JIT 管理器
typedef struct JITManager {
  HotSpot *hotspots;        // 热点数组
  int hotspot_count;        // 热点数量
  AQLJITCompiler **compilers; // 可用编译器
  int compiler_count;       // 编译器数量
  int global_threshold;     // 全局编译阈值
} JITManager;
```

### **协程系统**：
```c
// acoroutine.h - 协程系统
typedef enum {
  CO_RUNNING,               // 运行中
  CO_SUSPENDED,             // 挂起
  CO_DEAD,                  // 已结束
  CO_ERROR                  // 错误状态
} CoroutineStatus;

typedef struct AQLCoroutine {
  GCObject *next;           // GC 链表
  lu_byte tt;               // 类型标记
  lua_State *L;             // Lua 状态
  CoroutineStatus status;   // 协程状态
  TValue *yield_values;     // yield 的值
  int yield_count;          // yield 值数量
  int priority;             // 优先级
  lu_mem memory_limit;      // 内存限制
} AQLCoroutine;

// 协程调度器
typedef struct CoroutineScheduler {
  AQLCoroutine **coroutines; // 协程数组
  int active_count;          // 活跃协程数
  int max_coroutines;        // 最大协程数
  int current_index;         // 当前调度索引
  int (*schedule)(CoroutineScheduler *sched); // 调度函数
  void (*on_yield)(AQLCoroutine *co);   // yield 回调
  void (*on_resume)(AQLCoroutine *co);  // resume 回调
} CoroutineScheduler;
```

## 8. **重构实施计划**

### **第一阶段：调试工具和兼容性基础 (2-3周)**
1. **重构 `aqld` 调试工具**
   - 实现 `-vb`, `-v`, `-vast`, `-vt`, `-vl`, `-vd` 参数
   - 建立字节码格式化输出系统
   - 添加与 `luac -l` 的对比功能

2. **完善字节码输出格式**
   - 统一字节码描述格式
   - 添加寄存器使用分析
   - 实现执行跟踪功能

3. **建立 AST 输出系统**
   - 设计 AST 节点结构
   - 实现树形文本输出
   - 支持 JSON 格式导出

4. **修复当前的寄存器分配问题**
   - 分析并修复 `explist` 中的寄存器分配
   - 确保与 Lua 的行为一致

### **第二阶段：核心兼容性 (3-4周)**
1. **重构 `acode.c` 与 Lua 完全兼容**
   - 采用 Lua 的函数签名和实现
   - 确保寄存器管理策略一致
   - 建立扩展函数框架

2. **重构 `avm.c` 核心执行循环**
   - 采用 Lua 的 VM 循环结构
   - 实现扩展指令分发机制
   - 保持核心指令 100% 兼容

3. **重构 `aparser.c` 分层架构**
   - 建立 Lua 兼容层
   - 实现 AQL 语法糖层
   - 预留未来扩展接口

4. **建立扩展指令框架**
   - 定义扩展指令编号 (100+)
   - 实现扩展指令注册机制
   - 建立模块化扩展架构

### **第三阶段：扩展功能 (4-5周)**
1. **实现类和对象系统**
   - 设计类和对象的内存布局
   - 实现继承和多态机制
   - 添加相关字节码指令

2. **集成 JIT 编译器接口**
   - 设计 JIT 编译器抽象接口
   - 实现热点检测机制
   - 支持多种 JIT 后端

3. **实现 SOW 协程系统**
   - 设计轻量级协程调度器
   - 实现协程生命周期管理
   - 添加协程相关语法

4. **完善容器和 range 支持**
   - 优化现有 range 实现
   - 添加更多容器类型支持
   - 实现容器字面量语法

### **第四阶段：优化和测试 (2-3周)**
1. **性能优化和内存管理**
   - 优化关键路径性能
   - 完善 GC 集成
   - 内存使用优化

2. **全面的兼容性测试**
   - 运行 Lua 官方测试套件
   - 字节码输出对比测试
   - 边界情况测试

3. **基准测试和性能对比**
   - 建立性能基准测试
   - 与 Lua 性能对比
   - 回归测试防护

4. **文档和示例完善**
   - 编写用户文档
   - 提供示例代码
   - API 参考文档

## 9. **文件组织结构**

```
src/
├── core/                   # Lua 兼容核心
│   ├── acode.c            # 代码生成 (兼容 lcode.c)
│   ├── acode.h            # 代码生成头文件
│   ├── avm.c              # 虚拟机 (兼容 lvm.c)
│   ├── avm.h              # 虚拟机头文件
│   ├── aparser.c          # 解析器 (兼容 lparser.c)
│   ├── aparser.h          # 解析器头文件
│   ├── aobject.c          # 对象系统 (兼容 lobject.c)
│   ├── aobject.h          # 对象系统头文件
│   ├── alex.c             # 词法分析器 (兼容 llex.c)
│   ├── alex.h             # 词法分析器头文件
│   ├── ado.c              # 执行器 (兼容 ldo.c)
│   ├── ado.h              # 执行器头文件
│   ├── agc.c              # 垃圾回收 (兼容 lgc.c)
│   ├── agc.h              # 垃圾回收头文件
│   ├── amem.c             # 内存管理 (兼容 lmem.c)
│   ├── amem.h             # 内存管理头文件
│   ├── astring.c          # 字符串 (兼容 lstring.c)
│   ├── astring.h          # 字符串头文件
│   ├── atable.c           # 表 (兼容 ltable.c)
│   ├── atable.h           # 表头文件
│   ├── astate.c           # 状态管理 (兼容 lstate.c)
│   ├── astate.h           # 状态管理头文件
│   ├── afunc.c            # 函数 (兼容 lfunc.c)
│   ├── afunc.h            # 函数头文件
│   └── aopcodes.h         # 操作码定义 (兼容 lopcodes.h)
├── extensions/             # AQL 扩展
│   ├── aclass.c           # 类系统实现
│   ├── aclass.h           # 类系统头文件
│   ├── arange.c           # Range 支持
│   ├── arange.h           # Range 头文件
│   ├── acontainer.c       # 容器支持
│   ├── acontainer.h       # 容器头文件
│   ├── asow.c             # SOW 协程实现
│   ├── asow.h             # SOW 协程头文件
│   ├── aasync.c           # 异步支持
│   ├── aasync.h           # 异步头文件
│   └── aext_common.h      # 扩展通用定义
├── jit/                   # JIT 编译器
│   ├── ajit.c             # JIT 接口实现
│   ├── ajit.h             # JIT 接口头文件
│   ├── ajit_hotspot.c     # 热点检测
│   ├── ajit_hotspot.h     # 热点检测头文件
│   └── backends/          # JIT 后端
│       ├── ajit_llvm.c    # LLVM 后端
│       ├── ajit_custom.c  # 自定义后端
│       └── ajit_backend.h # 后端接口
├── tools/                 # 调试工具
│   ├── aqld.c             # 调试器主程序
│   ├── aast.c             # AST 输出工具
│   ├── aast.h             # AST 输出头文件
│   ├── abytecode.c        # 字节码输出工具
│   ├── abytecode.h        # 字节码输出头文件
│   ├── atrace.c           # 执行跟踪工具
│   ├── atrace.h           # 执行跟踪头文件
│   ├── acompare.c         # Lua 兼容性对比
│   ├── acompare.h         # 兼容性对比头文件
│   └── atools_common.h    # 工具通用定义
├── tests/                 # 测试套件
│   ├── lua_compat/        # Lua 兼容性测试
│   ├── extensions/        # 扩展功能测试
│   ├── benchmarks/        # 性能基准测试
│   ├── regression/        # 回归测试
│   └── unit/              # 单元测试
└── docs/                  # 文档
    ├── refactor.md        # 重构规划 (本文档)
    ├── lua_compat.md      # Lua 兼容性文档
    ├── extensions.md      # 扩展功能文档
    ├── jit.md             # JIT 编译器文档
    ├── sow.md             # SOW 协程文档
    └── api_reference.md   # API 参考文档
```

## 10. **兼容性保证机制**

### **自动化测试**：
```bash
# Lua 兼容性测试
make test-lua-compat

# 字节码对比测试  
make test-bytecode-compat

# 性能回归测试
make test-performance

# 扩展功能测试
make test-extensions

# 全面测试
make test-all
```

### **持续集成**：
- 每次提交自动运行 Lua 官方测试套件
- 字节码输出与 `luac -l` 对比验证
- 性能基准测试防止回归
- 内存泄漏检测
- 静态代码分析

### **兼容性指标**：
- **字节码兼容性**: 100% (核心指令完全一致)
- **语法兼容性**: 100% (所有 Lua 语法都支持)
- **API 兼容性**: 95% (核心 API 完全兼容，少数扩展)
- **性能兼容性**: 90%+ (性能不低于 Lua 90%)

## 11. **开发工具和调试支持**

### **开发者工具**：
```bash
# 开发模式编译
make dev                    # 启用所有调试信息

# 代码质量检查
make lint                   # 静态代码分析
make format                 # 代码格式化
make check-compat          # 兼容性检查

# 性能分析
make profile               # 性能分析版本
make benchmark             # 运行基准测试

# 文档生成
make docs                  # 生成 API 文档
make examples              # 生成示例代码
```

### **调试支持**：
- **GDB 集成**: 提供 `.gdbinit` 配置
- **Valgrind 支持**: 内存检查和性能分析
- **AddressSanitizer**: 内存错误检测
- **代码覆盖率**: 测试覆盖率报告

## 12. **总结**

这个重构规划确保了：

1. **与 Lua 的最大兼容性** - 核心功能完全兼容，便于学习和迁移
2. **清晰的扩展架构** - 新功能不影响核心稳定性，模块化设计
3. **强大的调试工具** - 便于开发和问题诊断，提高开发效率
4. **未来扩展能力** - 为类、JIT、SOW 等预留清晰的架构空间
5. **渐进式重构** - 可以分阶段实施，降低风险，确保稳定性
6. **完善的测试体系** - 自动化测试保证质量，防止回归
7. **优秀的开发体验** - 丰富的工具支持，清晰的文档

通过这个规划，AQL 将成为一个既保持 Lua 兼容性，又具有现代语言特性的强大脚本语言。
