# AQL架构设计文档

## 项目定位

**AQL (AI Query Language)** 是一门面向AI时代的原生编程语言。

**当前阶段目标**: 复现Lua核心功能 + 关键系统重构
- 基于Lua 5.4的成熟VM架构
- 重构核心系统: table→容器系统, GC改良, class系统
- 为后续AI原生特性提供高性能运行时基础

**长期愿景**: 真正的AI原生编程语言
- AI语法糖和意图驱动编程
- 工作流编排和人机协作
- 分布式AI任务执行和优化

## 1. 核心功能需求分析

### 1.1 核心功能需求
- **编译器功能**: 词法分析、语法分析、语义分析、字节码生成
- **虚拟机功能**: 字节码执行、内存管理、函数调用、异常处理
- **AI原生功能**: 意图解析、工作流编排、数据科学库、人机协作
- **嵌入功能**: C/Python API、跨语言调用、资源管理

### 1.2 约束条件
- **性能约束**: 解释性能接近Lua，JIT性能接近V8，数据处理比Pandas快2-10倍
- **内存约束**: GC暂停<1ms，内存碎片<5%，支持大内存应用
- **平台约束**: 支持Linux/macOS/Windows，x86_64/ARM64架构
- **兼容约束**: C11标准，GCC/Clang/MSVC编译器支持

### 1.3 关键非功能需求
- **可维护性**: 模块化设计，清晰的接口边界，完整的测试覆盖
- **可扩展性**: 插件式架构，支持新指令、新类型、新优化
- **可移植性**: 硬件抽象层，操作系统适配层
- **可调试性**: 丰富的诊断信息，可视化工具支持

## 2. 架构概览图

### 2.1 整体分层架构图
```
┌─────────────────────────────────────────┐
│           AQL语法层                      │
│  (语法糖、AI工作流、eDSL)                │
├─────────────────────────────────────────┤
│           编译器层                       │
│  (alex、aparser、acode、adump)           │
├─────────────────────────────────────────┤
│           运行时层                       │
│  (avm、agc、amem、atable)                │
├─────────────────────────────────────────┤
│           平台抽象层                     │
│  (aapi、aauxlib、aloadlib)               │
└─────────────────────────────────────────┘
```

### 2.2 高级组件架构图
```
    用户代码(AQL)
         │
    ┌────▼────┐
    │Compiler │────► 字节码文件(.aqlc)
    │ (alex+  │
    │aparser+ │
    │ acode)  │
    └────┬────┘
         │
    ┌────▼────┐    ┌─────────┐
    │   VM    │◄──►│   GC    │
    │ (avm+   │    │ (agc)   │
    │  ado)   │    └─────────┘
    └────┬────┘
         │
    ┌────▼────┐    ┌─────────┐
    │Runtime  │◄──►│AI Native│
    │(astate+ │    │(ai_*)   │
    │ aauxlib)│    └─────────┘
    └────┬────┘
         │
    ┌────▼────┐
    │ C/Py API│
    │ (aapi)  │
    └─────────┘
```

### 2.3 内存管理架构图
```
┌──────────────────────────────────────┐
│              Heap (amem)             │
│ ┌─────────┐ ┌──────────┐ ┌─────────┐ │
│ │Young Gen│ │ Old Gen  │ │Large Obj│ │
│ │(Eden+   │ │          │ │ Space   │ │
│ │Survivor)│ │          │ │         │ │
│ └─────────┘ └──────────┘ └─────────┘ │
└──────────────────────────────────────┘
         │              │
    ┌────▼────┐    ┌────▼────┐
    │ Mark&   │    │ Compact │
    │ Sweep   │    │   GC    │
    │ (agc)   │    │ (agc)   │
    └─────────┘    └─────────┘

Stack Memory (astate):
┌─────────────┐
│ Call Stack  │ ← 函数调用栈(CallInfo)
├─────────────┤
│ Local Vars  │ ← 局部变量
├─────────────┤  
│ Temp Values │ ← 临时值(TValue)
└─────────────┘
```

### 2.4 嵌入场景架构图
```
┌─────────────┐    ┌─────────────┐
│Python程序   │    │   C程序     │
│             │    │             │
│ import aql  │    │#include aql │
└──────┬──────┘    └──────┬──────┘
       │                  │
       ▼                  ▼
┌─────────────────────────────────┐
│         AQL Runtime             │
│  ┌─────────┐    ┌─────────────┐ │
│  │avm Core │    │ amem+agc    │ │
│  │(avm+ado)│    │             │ │
│  └─────────┘    └─────────────┘ │
│  ┌─────────────────────────────┐ │
│  │        aapi Interface       │ │
│  └─────────────────────────────┘ │
└─────────────────────────────────┘
```

### 2.5 编译场景架构图
```
AQL源码 ──► alex ──► aparser ──► acode ──► adump ──► 字节码
   │          │         │          │         │         │
   ▼          ▼         ▼          ▼         ▼         ▼
Token流   AST树    符号表    优化AST   指令序列   .aqlc文件
             │                       │
             ▼                       ▼
        AI语法糖解析            AI指令生成
       (ai_sugar)             (ai_workflow)
```

### 2.6 JIT场景架构图
```
字节码执行 ──► 热点检测 ──► JIT编译 ──► 机器码执行
(avm解释)    (性能计数)    (ai_jit)    (原生性能)
    │            │           │           │
    ▼            ▼           ▼           ▼
解释执行     计数器热点   LLVM后端    接近C性能
    │                        │
    └──── 性能分析 ◄─────────┘
       (adebug支持)
```

## 3. 关键架构设计

### 3.1 技术路线选型和决策

#### 3.1.1 VM架构选择
- **选择**: 寄存器VM (借鉴Lua设计)
- **理由**: 
  - 指令数量更少，执行效率更高
  - 更好的局部性，缓存友好
  - 已被Lua验证的成熟架构
- **实现**: `avm.c/h`核心执行引擎

#### 3.1.2 GC算法选择
- **选择**: 标记清除 + 分代 + 并发 (借鉴V8)
- **理由**:
  - 分代假设适用于大多数程序
  - 并发GC减少暂停时间
  - 支持大内存应用
- **实现**: `agc.c/h`垃圾回收器

#### 3.1.3 JIT后端选择
- **选择**: LLVM后端 (Phase 3)
- **理由**:
  - 成熟的优化基础设施
  - 跨平台支持良好
  - 丰富的优化Pass
- **实现**: `ai_jit.c/h`即时编译器

#### 3.1.4 SIMD实现策略
- **选择**: 编译器自动向量化 + 手工优化内核
- **理由**:
  - 自动向量化覆盖常见场景
  - 手工优化保证关键路径性能
  - 硬件适配灵活
- **实现**: `ai_simd.c/h`向量化引擎

#### 3.1.5 AI语法实现
- **选择**: 编译时转换为标准字节码
- **理由**:
  - 运行时开销为零
  - 便于优化和调试
  - 与现有VM无缝集成
- **实现**: `ai_sugar.c/h`语法糖转换器

### 3.2 关键数据结构设计

#### 3.2.1 指令格式设计
```c
// 32位固定长度指令，完全兼容Lua 5.4设计
typedef unsigned int Instruction;

// 四种指令格式
// iABC: OP(6) A(8) B(9) C(9)
// iABx: OP(6) A(8) Bx(18) 
// iAsBx: OP(6) A(8) sBx(18)
// iAx: OP(6) Ax(26)

#define SIZE_OP    6
#define SIZE_A     8  
#define SIZE_B     9
#define SIZE_C     9
#define SIZE_Bx    (SIZE_B + SIZE_C)
#define SIZE_Ax    (SIZE_A + SIZE_Bx)

// 位置定义
#define POS_OP     0
#define POS_A      (POS_OP + SIZE_OP)
#define POS_C      (POS_A + SIZE_A)
#define POS_B      (POS_C + SIZE_C)
#define POS_Bx     POS_C
#define POS_Ax     POS_A

// AQL指令集布局 (总计64条，Phase 1完整实现)
enum AQLOpCode {
    // 基础指令组 (0-15): 加载存储
    OP_MOVE = 0, OP_LOADI, OP_LOADF, OP_LOADK, OP_LOADKX,
    OP_LOADFALSE, OP_LOADTRUE, OP_LOADNIL, OP_GETUPVAL, OP_SETUPVAL,
    OP_GETTABUP, OP_SETTABUP, OP_CLOSE, OP_TBC, OP_CONCAT, OP_EXTRAARG,
    
    // 算术运算组 (16-31): K/I优化版本
    OP_ADD = 16, OP_ADDK, OP_ADDI, OP_SUB, OP_SUBK, OP_SUBI,
    OP_MUL, OP_MULK, OP_MULI, OP_DIV, OP_DIVK, OP_DIVI,
    OP_MOD, OP_POW, OP_UNM, OP_LEN,
    
    // 位运算组 (32-39): 精简版本
    OP_BAND = 32, OP_BOR, OP_BXOR, OP_SHL, OP_SHR,
    OP_BNOT, OP_NOT, OP_SHRI,
    
    // 比较控制组 (40-47): 优化比较
    OP_JMP = 40, OP_EQ, OP_LT, OP_LE, OP_TEST, OP_TESTSET,
    OP_EQI, OP_LTI,
    
    // 函数调用组 (48-55): 返回优化
    OP_CALL = 48, OP_TAILCALL, OP_RET, OP_RET_VOID, OP_RET_ONE,
    OP_FORLOOP, OP_FORPREP, OP_CLOSURE,
    
    // AQL容器组 (56-59): 统一容器操作
    OP_NEWOBJECT = 56, OP_GETPROP, OP_SETPROP, OP_INVOKE,
    
    // AQL扩展组 (60-63): 特有功能
    OP_YIELD = 60, OP_RESUME, OP_BUILTIN, OP_VARARG
};
```

#### 3.2.2 对象表示设计
```c
// 统一对象头
#define CommonHeader \
  GCObject *next; lu_byte tt; lu_byte marked

// 统一值类型
typedef struct TValue {
  TValuefields;
} TValue;

// 字符串对象
typedef struct TString {
  CommonHeader;
  lu_byte extra;           // 用户数据
  lu_byte shrlen;          // 短字符串长度
  unsigned int hash;       // 哈希值
  union {
    size_t lnglen;         // 长字符串长度
    struct TString *hnext; // 哈希链
  } u;
} TString;

// AQL容器对象设计 (替代Lua unified table)

// 固定数组对象
typedef struct Array {
  CommonHeader;
  DataType dtype;          // 元素类型
  size_t length;           // 固定长度(编译时确定)
  TValue data[];           // 内联数据，紧凑布局
} Array;

// 动态切片对象  
typedef struct Slice {
  CommonHeader;
  DataType dtype;          // 元素类型
  size_t length;           // 当前长度
  size_t capacity;         // 容量
  TValue *data;            // 指向数据
} Slice;

// 字典对象
typedef struct Dict {
  CommonHeader;
  DataType key_type;       // 键类型
  DataType value_type;     // 值类型
  size_t length;           // 当前元素数
  size_t capacity;         // 容量
  size_t mask;             // 哈希掩码 (capacity - 1)
  DictEntry *entries;      // 哈希表条目
} Dict;

// 字典条目
typedef struct DictEntry {
  TValue key;              // 键
  TValue value;            // 值
  uint32_t hash;           // 缓存的哈希值
  uint32_t flags;          // 状态标志 (空槽/已删除/占用)
} DictEntry;
```

#### 3.2.3 栈帧设计
```c
// 调用信息 (基于Lua CallInfo设计)
typedef struct CallInfo {
  StkIdRel func;           // 函数位置
  StkIdRel top;            // 栈顶
  struct CallInfo *previous, *next;  // 调用链
  union {
    struct {  // AQL函数调用
      const Instruction *savedpc;    // 保存PC
      volatile a_signalT trap;       // 调试跟踪
      int nextraargs;                // 变参数量
    } a;
    struct {  // C函数调用
      aql_KFunction k;     // 继续函数
      ptrdiff_t old_errfunc;
      aql_KContext ctx;    // 上下文
    } c;
  } u;
  union {
    int funcidx;           // 函数索引
    int nyield;            // yield值数量
    int nres;              // 返回值数量
    struct {               // 信息传递
      unsigned short ftransfer;
      unsigned short ntransfer;
    } transferinfo;
  } u2;
  short nresults;          // 期望返回值
  unsigned short callstatus;
} CallInfo;

/*
栈帧布局 (基于Lua设计，支持AQL容器类型):
┌─────────────┐ ← ci->top
│ 临时计算区域 │  (寄存器/局部变量)
├─────────────┤
│ 局部变量n   │  (统一TValue表示，可能内联小容器)
├─────────────┤
│ 局部变量1   │  (TValue统一接口)
├─────────────┤ ← ci->func + 1 + numparams
│ 变参区域    │  (可变参数，如果有)
├─────────────┤
│ 参数n       │  (支持容器类型，智能优化传递)
├─────────────┤
│ 参数1       │  (统一TValue表示)
├─────────────┤ ← ci->func + 1
│ 函数对象    │  (Function/Closure)
└─────────────┘ ← ci->func

容器传递策略:
- array<T,N>: 小数组可能内联，大数组按引用
- slice<T>: 总是按引用(slice头)
- dict<K,V>: 总是按引用
- vector<T,N>: SIMD对齐，可能内联或引用
*/
```

#### 3.2.4 向量对象 (SIMD优化)
```c
// 向量对象 (SIMD优化)
typedef struct Vector {
  CommonHeader;
  DataType dtype;          // 元素类型
  size_t length;           // 向量长度 (编译时常量)
  size_t simd_width;       // SIMD宽度 (8, 16, 32等)
  void *data;              // SIMD对齐的数据
} Vector;

// 数据科学对象 (简化版)
typedef struct Series {
  CommonHeader;
  DataType dtype;          // 数据类型
  size_t length;           // 长度
  size_t capacity;         // 容量
  void *data;              // 数据指针(SIMD对齐)
  bool simd_aligned;       // SIMD对齐标志
  Dict *metadata;          // 元数据字典 (string -> any)
} Series;
```

### 3.3 关键算法设计

#### 3.3.1 寄存器分配算法
```c
// 基于图着色的寄存器分配
typedef struct RegAllocator {
  int *interference_graph;  // 干涉图
  int *color_map;          // 颜色映射
  int num_registers;       // 寄存器数
  int num_vars;           // 变量数
} RegAllocator;

// 分配策略: 图着色 + 线性扫描优化
void allocate_registers(RegAllocator *alloc) {
  build_interference_graph(alloc);
  color_graph(alloc);
  handle_spills(alloc);
}
```

#### 3.3.2 垃圾回收算法
```c
// 三色标记算法
void gc_mark_phase(global_State *g) {
  // 1. 标记根集合
  mark_root_set(g);
  
  // 2. 三色标记传播
  while (g->gray) {
    GCObject *o = g->gray;
    mark_object_gray_to_black(g, o);
  }
  
  // 3. 处理弱引用
  clear_weak_references(g);
}

// 并发清理
void gc_sweep_phase(global_State *g) {
  GCObject **p = &g->allgc;
  while (*p) {
    GCObject *curr = *p;
    if (iswhite(curr)) {
      *p = curr->next;
      free_object(g, curr);
    } else {
      reset_color(curr);
      p = &curr->next;
    }
  }
}
```

#### 3.3.3 热点检测算法
```c
// 基于计数器的热点检测
typedef struct HotspotDetector {
  int *call_counts;        // 调用计数
  int *loop_counts;        // 循环计数
  int threshold;           // 热点阈值
  bool *hot_functions;     // 热点函数标记
} HotspotDetector;

// 简单计数策略
bool is_hotspot(HotspotDetector *detector, int func_id) {
  return detector->call_counts[func_id] > detector->threshold;
}
```

#### 3.3.4 向量化算法
```c
// SIMD模式匹配
typedef enum {
  SIMD_ADD,     // 向量加法
  SIMD_MUL,     // 向量乘法
  SIMD_REDUCE,  // 归约操作
  SIMD_WINDOW   // 窗口函数
} SIMDPattern;

// 自动向量化
bool can_vectorize(Instruction *code, int len) {
  // 数据依赖分析
  if (has_data_dependency(code, len)) return false;
  
  // 内存访问模式分析  
  if (!is_sequential_access(code, len)) return false;
  
  // 成本收益分析
  if (vectorization_cost(len) > threshold) return false;
  
  return true;
}
```

## 4. 组件列表

### 4.1 核心组件清单

| 组件名 | 源文件 | 头文件 | 功能描述 |
|--------|--------|--------|----------|
| **编译器组件** | | | |
| 词法分析器 | `alex.c` | `alex.h` | Token识别和生成 |
| 语法分析器 | `aparser.c` | `aparser.h` | AST构建 |
| 语义分析器 | `asemantic.c` | `asemantic.h` | 类型检查和作用域 |
| 代码生成器 | `acode.c` | `acode.h` | 字节码生成 |
| 字节码输出 | `adump.c` | `adump.h` | 序列化字节码 |
| 字节码加载 | `aundump.c` | `aundump.h` | 反序列化字节码 |
| **虚拟机组件** | | | |
| VM核心 | `avm.c` | `avm.h` | 指令执行循环 |
| 执行控制 | `ado.c` | `ado.h` | 函数调用、异常处理 |
| 操作码定义 | `aopcodes.c` | `aopcodes.h` | 指令集定义 |
| **内存管理** | | | |
| 内存分配器 | `amem.c` | `amem.h` | 基础内存管理 |
| 垃圾回收器 | `agc.c` | `agc.h` | GC算法实现 |
| **对象系统** | | | |
| 对象模型 | `aobject.c` | `aobject.h` | 统一对象表示 |
| 字符串管理 | `astring.c` | `astring.h` | 字符串内部化 |
| 数组容器 | `aarray.c` | `aarray.h` | 固定数组实现 |
| 切片容器 | `aslice.c` | `aslice.h` | 动态切片实现 |
| 字典容器 | `adict.c` | `adict.h` | 哈希字典实现 |
| 向量容器 | `avector.c` | `avector.h` | SIMD向量实现 |
| 函数对象 | `afunc.c` | `afunc.h` | 函数和闭包 |
| **运行时支持** | | | |
| 全局状态 | `astate.c` | `astate.h` | VM状态管理 |
| 元方法 | `atm.c` | `atm.h` | 元编程支持 |
| 调试支持 | `adebug.c` | `adebug.h` | 调试信息 |
| 输入流 | `azio.c` | `azio.h` | 字节码读取 |
| **AI扩展** | | | |
| SIMD优化 | `ai_simd.c` | `ai_simd.h` | 向量化计算 |
| JIT编译器 | `ai_jit.c` | `ai_jit.h` | 即时编译(Phase 3) |
| **API组件** | | | |
| C API | `aapi.c` | `aapi.h` | 嵌入接口 |
| 辅助库 | `aauxlib.c` | `aauxlib.h` | 便利函数 |
| **标准库** | | | |
| 基础库 | `abaselib.c` | - | 基础函数 |
| 字符串库 | `astrlib.c` | - | 字符串操作 |
| 数组库 | `aarraylib.c` | - | 数组操作 |
| 切片库 | `aslicelib.c` | - | 切片操作 |
| 字典库 | `adictlib.c` | - | 字典操作 |
| 向量库 | `avectorlib.c` | - | 向量操作 |
| 数学库 | `amathlib.c` | - | 数学函数 |
| IO库 | `aiolib.c` | - | 输入输出 |
| 协程库 | `acorolib.c` | - | 协程支持 |
| UTF8库 | `autf8lib.c` | - | UTF8处理 |
| 调试库 | `adblib.c` | - | 调试函数 |
| 动态加载 | `aloadlib.c` | - | 模块加载 |

### 4.2 文件组织结构
```
src/
├── Makefile
├── aql.h              # 主头文件
├── aqlconf.h          # 配置头文件  
├── aqllib.h           # 库函数头文件
├── aql.c              # 主程序入口
├── aqlc.c             # 编译器主程序
├── aprefix.h          # 前缀定义
├── ainit.c            # 初始化
│
├── # 编译器
├── alex.c/h           # 词法分析
├── aparser.c/h        # 语法分析
├── acode.c/h          # 代码生成
├── adump.c/h          # 字节码输出
├── aundump.c/h        # 字节码加载
│
├── # 虚拟机
├── avm.c/h            # 虚拟机核心
├── ado.c/h            # 执行控制
├── aopcodes.c/h       # 操作码
│
├── # 内存管理
├── amem.c/h           # 内存分配
├── agc.c/h            # 垃圾回收
│
├── # 对象系统
├── aobject.c/h        # 对象模型
├── astring.c/h        # 字符串
├── aarray.c/h         # 数组
├── aslice.c/h         # 切片
├── adict.c/h          # 字典
├── avector.c/h        # 向量
├── afunc.c/h          # 函数
│
├── # 运行时
├── astate.c/h         # 全局状态
├── atm.c/h            # 元方法
├── adebug.c/h         # 调试
├── azio.c/h           # 输入流
├── actype.c/h         # 字符类型
│
├── # 性能优化
├── ai_simd.c/h        # SIMD
├── ai_jit.c/h         # JIT(Phase 3)
│
├── # API
├── aapi.c/h           # C API
├── aauxlib.c/h        # 辅助库
│
└── # 标准库
├── abaselib.c         # 基础库
├── astrlib.c          # 字符串库
├── aarraylib.c        # 数组库
├── aslicelib.c        # 切片库
├── adictlib.c         # 字典库
├── avectorlib.c       # 向量库
├── amathlib.c         # 数学库
├── aiolib.c           # IO库
├── acorolib.c         # 协程库
├── autf8lib.c         # UTF8库
├── adblib.c           # 调试库
└── aloadlib.c         # 动态加载
```

## 5. 组件模型

### 5.1 组件层次结构
```
性能优化组件 (为AI特性预备):
├── SIMD (ai_simd) - 向量化计算，AI数据处理基础
└── JIT (ai_jit) - 即时编译(Phase 3)，AI热路径优化

核心层组件:
├── Compiler (编译器)
│   ├── Lexer (alex) - 词法分析
│   ├── Parser (aparser) - 语法分析
│   ├── CodeGen (acode) - 代码生成
│   ├── Dump (adump) - 字节码输出
│   └── Undump (aundump) - 字节码加载
├── VM (虚拟机)
│   ├── Executor (avm) - 执行器
│   ├── Controller (ado) - 执行控制
│   └── Opcodes (aopcodes) - 操作码
├── Memory (内存管理)
│   ├── Allocator (amem) - 内存分配
│   └── GC (agc) - 垃圾回收
├── Objects (对象系统)
│   ├── Object (aobject) - 对象模型
│   ├── String (astring) - 字符串
│   ├── Array (aarray) - 固定数组
│   ├── Slice (aslice) - 动态切片
│   ├── Dict (adict) - 字典
│   ├── Vector (avector) - SIMD向量
│   └── Function (afunc) - 函数
└── Runtime (运行时)
    ├── State (astate) - 全局状态
    ├── MetaMethod (atm) - 元方法
    ├── Debug (adebug) - 调试
    └── IO (azio) - 输入输出

平台层组件:
├── C API (aapi) - C接口
├── AuxLib (aauxlib) - 辅助库
└── StdLib (*lib) - 标准库
```

### 5.2 组件依赖图 (正确的自底向上架构)
```
编译器层 (源码→字节码转换):
┌─────────────────────────────────────────────────┐
│ alex ──► aparser ──► acode ──► adump            │
└─────────────────────┬─────────────────────────────┘
                      ▼ (生成字节码)
API层 (对外接口):
┌─────────────────────────────────────────────────┐
│ aapi ◄──► aauxlib ──► 标准库(*lib)              │
└─────────────────────┬─────────────────────────────┘
                      ▼ (调用)
运行时层 (高级服务):
┌─────────────────────────────────────────────────┐
│ astate ◄──► atm ──► adebug                      │
│    ▲         │                                  │
│    │         ▼                                  │
│ astring ◄─ aarray/aslice/adict/avector ◄─ afunc │
└───────────────────┬─────────────────────────────┘
                    ▼ (使用)
虚拟机层 (执行引擎):
┌─────────────────────────────────────────────────┐
│ aundump ──► avm ──► ado                         │
│     ▲               │                           │
│     │               ▼                           │
│   azio           agc ◄──► amem                  │
└───────────────────┬─────────────────────────────┘
                    ▼ (依赖)
对象系统层 (数据表示):
┌─────────────────────────────────────────────────┐
│ aobject ◄──► TValue ◄──► CommonHeader           │
│    ▲         │                                  │
│    │         ▼                                  │
│ TString   Array/Slice/Dict/Vector/Function      │
└───────────────────┬─────────────────────────────┘
                    ▼ (基于)
基础定义层 (核心数据结构):
┌─────────────────────────────────────────────────┐
│ aopcodes (指令定义) ◄──► aconf (配置)            │
│    ▲                          │                 │
│    │                          ▼                 │
│ OpCode枚举  ◄───────► 基础类型定义               │
└─────────────────────────────────────────────────┘

横向性能优化 (为各层提供加速):
┌───────────┐    ┌─────────────┐
│ ai_simd   │    │  ai_jit     │
│(数据处理) │    │(热路径优化) │
└───────────┘    └─────────────┘

依赖关系说明:
• 编译器层 → API层 → 运行时层 → VM层 → 对象层 → 基础层
• 下层为上层提供基础设施和服务
• 基础层定义核心数据结构和操作码
• 性能优化层横向为所有层提供加速
```

### 5.3 组件接口关系 (按正确的依赖层次)

#### 5.3.1 核心接口 (上层使用下层)
- **Compiler → API**: 
  - 字节码生成完成后加载到VM (`aql_loadbuffer`)
  - 编译错误报告 (`aql_syntax_error`)

- **API → Runtime**: 
  - 状态管理 (`aql_newstate`, `aql_close`)
  - 栈操作 (`aql_push*`, `aql_to*`, `aql_checkstack`)
  - 函数调用 (`aql_call`, `aql_pcall`)
  - 错误处理 (`aql_error`, `aql_throw`)

- **Runtime → VM**: 
  - 字节码执行请求 (`aql_execute`)
  - 状态查询 (`aql_getstate`)
  - 调试支持 (`aql_sethook`)

- **VM → Objects**: 
  - 对象分配 (`aql_newobject`)
  - 类型检查 (`aql_isobject`, `aql_typeof`)
  - 容器操作 (`aql_arrayget`, `aql_sliceappend`, `aql_dictset`)

- **Objects → Base**: 
  - 基础类型定义 (`TValue`, `CommonHeader`)
  - 操作码使用 (`OpCode`, `Instruction`)
  - 配置参数 (`aql_config.h`)

- **VM → Memory/GC** (跨层服务): 
  - 内存分配 (`aql_realloc`)
  - 垃圾回收 (`gc_mark`, `gc_barrier`)
  - 内存统计 (`aql_gc_count`)

#### 5.3.2 性能优化接口 (横向加速服务)
- **VM/Runtime → SIMD**:
  - 向量化请求 (`simd_execute`)
  - SIMD对齐分配请求 (`simd_aligned_alloc`)
  - 向量化内存操作 (`simd_memcpy`, `simd_memset`)

- **VM/Runtime → JIT**:
  - 热点检测请求 (`jit_should_compile`)
  - JIT编译请求 (`jit_compile`)
  - 编译代码执行 (`jit_execute`)

#### 5.3.3 对外API接口 (上层调用下层)
- **用户代码 → C API**:
  - 语言嵌入 (`aql_newstate`, `aql_close`)
  - 脚本执行 (`aql_loadfile`, `aql_pcall`)
  - 数据交换 (`aql_push*`, `aql_to*`)
  - 错误处理 (`aql_error`, `aql_gettop`)

- **C API → AuxLib → StdLib**:
  - AuxLib提供便利函数封装
  - StdLib提供标准库实现
  - 都基于底层C API构建

#### 5.3.4 依赖层次总结 (修正版)
```
用户代码 (AQL源文件)
    ↓ (编译)
Compiler Layer (alex → aparser → acode → adump)
    ↓ (生成字节码)
API Layer (aapi / aauxlib / stdlib) ←→ SIMD/JIT (横向优化)
    ↓ (调用)
Runtime Layer (astate / atm / adebug / 容器)
    ↓ (使用)
VM Layer (avm / ado / agc / amem)
    ↓ (依赖)
Object Layer (aobject / TValue / 容器实现)
    ↓ (基于)
Base Layer (aopcodes / aconf / 基础类型)

数据流向:
源码 → 字节码 → VM执行 → 对象操作 → 基础数据结构
```

## 6. 运行时模型

### 6.1 内存模型

#### 6.1.1 堆内存布局
```
┌─────────────────────────────────────────┐
│                 Heap                    │
│ ┌─────────┐ ┌─────────┐ ┌─────────────┐ │
│ │ Eden    │ │Survivor │ │  Old Gen    │ │
│ │ Space   │ │  Space  │ │             │ │
│ │(新对象) │ │(幸存者) │ │ (长寿对象)  │ │
│ └─────────┘ └─────────┘ └─────────────┘ │
│ ┌─────────────────────────────────────┐ │
│ │          Large Object Space         │ │
│ │          (大对象直接分配)           │ │
│ └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘

Stack Memory (astate):
┌─────────────┐
│ Call Stack  │ ← 函数调用栈(CallInfo链表)
├─────────────┤
│ Value Stack │ ← 值栈(TValue数组)
├─────────────┤  
│ Upvalues    │ ← 闭包变量(UpVal链表)
└─────────────┘

SIMD Memory Extensions:
┌─────────────┐
│ Vector Data │ ← SIMD对齐的向量数据
├─────────────┤
│ Series Data │ ← 数据科学对象
└─────────────┘
```

#### 6.1.2 对象生命周期
```
对象创建 → Eden分配 → 首次GC → Survivor
    ↓
多次GC存活 → 晋升Old Gen → 最终回收
    ↓
大对象 → 直接分配Large Object Space
```

### 6.2 GC模型

#### 6.2.1 GC触发条件
- Eden空间分配失败
- 堆内存使用超过阈值
- 显式调用 `aql_gc()`
- 长时间未执行GC

#### 6.2.2 GC执行流程
```
1. 标记阶段 (Mark Phase):
   - 从根集合开始(全局变量、栈、寄存器)
   - 三色标记传播(白→灰→黑)
   - 处理弱引用表

2. 清理阶段 (Sweep Phase):
   - 回收白色(未标记)对象
   - 执行finalizer
   - 更新空闲链表

3. 压缩阶段 (Compact Phase - 可选):
   - 整理内存碎片
   - 更新对象引用
   - 提高内存局部性

4. 晋升阶段 (Promotion):
   - 长寿对象晋升到老年代
   - 调整分代阈值
```

#### 6.2.3 并发GC支持
```c
// 并发标记
void concurrent_mark_phase(global_State *g) {
  while (g->gray && !should_yield()) {
    GCObject *o = g->gray;
    mark_object_incremental(g, o);
  }
  
  if (g->gray) {
    // 需要让步给用户程序
    schedule_gc_continuation(g);
  }
}

// 写屏障确保并发安全
void gc_barrier(aql_State *L, GCObject *p, GCObject *v) {
  global_State *g = G(L);
  if (isblack(p) && iswhite(v)) {
    if (g->gcstate == GCSpropagate)
      mark_object_gray(g, v);  // 重新标记为灰色
    else
      makewhite(g, p);         // 将父对象重新标记为白色
  }
}
```

### 6.3 数据流设计

#### 6.3.1 编译时数据流
```
源码文件(.aql)
    │
    ▼ alex词法分析
Token流
    │
    ▼ aparser语法分析  
AST树
    │
    ▼ acode代码生成
字节码指令序列
    │
    ▼ adump序列化
字节码文件(.aqlc)

AI语法扩展流:
AI语法糖 → ai_sugar解析 → 标准AST → 正常编译流程
```

#### 6.3.2 运行时数据流
```
字节码文件(.aqlc)
    │
    ▼ aundump反序列化
内存中字节码
    │
    ▼ avm执行引擎
指令解码执行
    │
    ▼ 寄存器/栈操作
内存读写访问
    │
    ▼ 函数调用/返回
结果产生

性能优化流:
热点检测 → JIT编译 → 机器码执行 → 性能提升
```

#### 6.3.3 向量化数据流
```
数组/向量数据
    │
    ▼ SIMD检测
向量化候选
    │
    ▼ ai_simd优化
SIMD指令生成
    │
    ▼ 硬件执行
向量化计算
    │
    ▼ 结果收集
优化输出
```

#### 6.3.4 嵌入调用数据流
```
宿主语言(Python/C)
    │
    ▼ aapi接口调用
参数类型转换
    │
    ▼ aql_State状态机
VM执行AQL代码
    │
    ▼ 结果转换
返回宿主语言

双向数据流:
宿主 ←→ AQL API ←→ VM ←→ SIMD优化 ←→ 标准库
```

### 6.4 错误处理模型

#### 6.4.1 异常传播机制
```c
// 长跳转异常处理 (类似Lua)
struct aql_longjmp {
  struct aql_longjmp *previous;
  jmp_buf b;
  volatile int status;  // 错误状态
};

// 异常抛出
void aql_throw(aql_State *L, int errcode) {
  global_State *g = G(L);
  if (L->errorJmp) {
    L->errorJmp->status = errcode;
    longjmp(L->errorJmp->b, 1);
  } else {
    // 未处理异常，调用panic函数
    if (g->panic)
      g->panic(L);
    abort();
  }
}

// 保护调用
int aql_pcall(aql_State *L, int nargs, int nresults, int errfunc) {
  struct aql_longjmp lj;
  lj.previous = L->errorJmp;
  L->errorJmp = &lj;
  
  if (setjmp(lj.b) == 0) {
    aql_call(L, nargs, nresults);
    L->errorJmp = lj.previous;
    return AQL_OK;
  } else {
    // 异常被捕获
    L->errorJmp = lj.previous;
    return lj.status;
  }
}
```

#### 6.4.2 SIMD错误处理扩展
```c
// SIMD专用错误类型
typedef enum {
  AQL_SIMD_ALIGNMENT_ERROR = 100,  // 对齐错误
  AQL_SIMD_SIZE_MISMATCH,          // 尺寸不匹配
  AQL_SIMD_UNSUPPORTED_OP,         // 不支持的操作
  AQL_SIMD_HARDWARE_LIMIT          // 硬件限制
} SIMDErrorCode;

// SIMD操作错误恢复
int simd_operation_with_fallback(aql_State *L, SIMDOperation *op) {
  int status = aql_pcall(L, op->nargs, op->nresults, 0);
  if (status == AQL_OK) {
    return AQL_OK;  // SIMD成功
  } else if (status == AQL_SIMD_ALIGNMENT_ERROR || 
             status == AQL_SIMD_UNSUPPORTED_OP) {
    // 回退到标量实现
    return execute_scalar_fallback(L, op);
  } else {
    return status;  // 其他错误直接返回
  }
}
```

## 7. 总结

### 7.1 核心设计成果

#### 7.1.1 指令集设计成果
- **完整的64条指令**: 涵盖Lua核心功能 + AQL扩展特性
- **性能优化完备**: K/I版本优化提供1.5-2.0x性能提升
- **容器系统革新**: 4条统一指令替代Lua的10条表操作
- **AI扩展就绪**: 通过BUILTIN/INVOKE为AI特性提供扩展接口

#### 7.1.2 架构优势
- **简化而不简陋**: 比Lua减少18条指令，但功能更强大
- **类型安全**: array/slice/dict替代模糊的table概念
- **扩展灵活**: 软件层扩展机制，易于添加新功能
- **向前兼容**: 为Phase 2的AI特性预留充足空间

#### 7.1.3 实现策略
- **渐进式开发**: Phase 1专注核心，Phase 2添加AI特性
- **借鉴成熟设计**: 基于Lua 5.4的成功架构
- **现代化改进**: 容器系统、函数返回优化、批量操作

AQL架构设计采用了经过验证的技术路线，同时针对AI时代的需求进行了创新：

### 7.1 核心优势
- **成熟基础**: 借鉴Lua的寄存器VM和V8的GC设计
- **现代容器**: 完全抛弃Lua unified table，采用类型明确的array/slice/dict/vector
- **AI原生定位**: 为AI时代设计，当前阶段专注核心基础重构
- **高性能**: 分层执行模式，从解释到JIT到AOT，SIMD友好的容器设计
- **易嵌入**: 清晰的API设计，支持C/Python集成

### 7.2 创新特性
- **类型安全容器**: array<T,N>/slice<T>/dict<K,V>/vector<T,N>提供编译时类型检查
- **SIMD优化**: vector<T,N>原生SIMD支持，自动向量化
- **内联优化**: 小对象栈内联，减少堆分配和指针跳转
- **智能传递**: 容器类型的透明优化传递策略
- **数据科学**: 高性能Series/DataFrame，基于专门设计的容器

### 7.3 实施路径 (AI原生语言的分阶段实现)
- **Phase 1**: 复现Lua核心 + 关键重构 (table→容器系统, GC改良, class系统)
- **Phase 2**: AI原生特性 (语法糖, 工作流, 意图驱动编程)
- **Phase 3**: 高级AI优化 (JIT, 分布式, 企业特性)

### 7.4 容器系统革命性改进

AQL完全抛弃了Lua的unified table设计，采用现代类型明确的容器系统：

**相比Lua table的优势:**
- **类型安全**: 编译时类型检查，避免运行时类型错误
- **性能可预测**: 每种容器都有明确的性能特征，无隐含开销
- **SIMD友好**: vector<T,N>原生支持向量化，array<T,N>内存连续
- **内存高效**: 专门优化的布局，比Lua table减少30-50%内存使用
- **AI就绪**: 专为AI工作负载设计，当前专注基础架构重构

**新容器映射关系:**
```
Lua table用法              →  AQL替代方案
{1,2,3,4,5}                →  array<int,5> 或 slice<int>
{a=1, b=2}                 →  dict<string,int> 或 struct
模块对象                    →  真正的module系统
元表行为                    →  trait/interface系统
```

这种设计使AQL在保持简洁性的同时，获得了现代语言的类型安全和极致性能，为AI时代的编程需求奠定了坚实的技术基础。当前阶段专注于核心系统重构，为后续AI原生特性提供高性能、可扩展的运行时基础。