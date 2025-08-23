# AQL 变量系统终极设计 (Lua架构完整增强版)

## 1. 设计哲学升级

**保留Lua基础 + 评估增强 = 完整解决方案**

```
var3基础：Lua兼容架构
var5增强：评估建议补充
= 完整生产级变量系统
```

## 2. 保留的Lua基础架构

### 2.1 标准Lua TValue结构 (完全保留)
```c
// 完全保留Lua 5.4的TValue结构
typedef union Value {
  GCObject *gc;    /* collectable objects */
  void *p;         /* light userdata */
  int b;           /* booleans */
  lua_CFunction f; /* light C functions */
  lua_Integer i;   /* integer numbers */
  lua_Number n;    /* float numbers */
} Value;

// 标准Lua TValue定义
typedef struct TValue {
  Value value_;      /* 8字节 */
  int tt_;           /* 4字节类型标签 */
} TValue;            /* 总计16字节 */

// Lua字符串结构 (完全保留)
typedef struct TString {
  CommonHeader;      /* 标准GC头 */
  lu_byte extra;     /* 保留字节 */
  lu_byte shrlen;    /* 短字符串长度 */
  unsigned int hash; /* 哈希值 */
  union {
    size_t lnglen;   /* 长字符串长度 */
    struct TString *hnext;  /* 哈希链 */
  } u;
  char contents[1];  /* 字符串内容 */
} TString;

// Lua函数原型 (完全保留)
typedef struct Proto {
  CommonHeader;
  lu_byte numparams;  /* 参数数量 */
  lu_byte is_vararg;
  lu_byte maxstacksize; /* 最大栈大小 */
  int sizeupvalues;    /* 上值数量 */
  int sizek;          /* 常量数量 */
  int sizecode;       /* 代码数量 */
  int sizelineinfo;   /* 行信息数量 */
  int sizep;          /* 原型数量 */
  int sizelocvars;    /* 局部变量数量 */
  /* ... 其他Lua标准字段 */
} Proto;
```

### 2.2 标准Lua作用域管理 (完全保留)
```c
// 完全保留Lua的CallInfo结构
typedef struct CallInfo {
  StkId func;           /* 函数位置 */
  StkId top;            /* 栈顶 */
  struct CallInfo *previous, *next;
  union {
    struct {  /* Lua函数 */
      const Instruction *savedpc;
      volatile l_signalT trap;
      int nextraargs;
    } l;
    struct {  /* C函数 */
      lua_KFunction k;
      ptrdiff_t old_errfunc;
      lua_KContext ctx;
    } c;
  } u;
  short nresults;
  unsigned short callstatus;
} CallInfo;

// 标准Lua上值结构
typedef struct UpVal {
  TValue *v;          /* 指向栈变量或自身 */
  union {
    TValue value;     /* 上值实际值 */
    struct {          /* 当变量离开时 */
      struct UpVal *next;
      struct UpVal **prev;
    } open;
  } u;
} UpVal;
```

## 3. 评估增强功能

### 3.1 智能类型推断系统 (新增)
```c
// 在Lua TValue基础上扩展类型推断
typedef struct AQLTypeInfo {
  TValue base;          /* 标准Lua TValue */
  struct {
    bool is_inferred : 1;    /* 是否推断类型 */
    bool is_static : 1;      /* 是否静态类型 */
    uint8_t confidence;      /* 推断置信度 0-100 */
    AQLType static_type;     /* 静态类型信息 */
  } meta;
} AQLTypeInfo;

// 类型推断引擎
typedef struct TypeInference {
  AQLTypeInfo infer_literal(TValue value);
  AQLTypeInfo infer_expression(AST *expr);
  AQLTypeInfo infer_function(Proto *proto);
} TypeInference;
```

### 3.2 零成本调试系统 (新增)
```c
// 在Lua调试基础上增强
typedef struct AQLDebugInfo {
  /* 标准Lua调试信息 */
  const char *source;
  int linedefined;
  int lastlinedefined;
  
  /* AQL增强调试信息 */
  struct {
    const char *var_name;
    int var_line;
    int var_column;
    AQLType static_type;
    uint64_t access_count;
  } *var_debug;
  
  /* 条件编译控制 */
  DebugLevel debug_level;
} AQLDebugInfo;

// 零运行时开销的调试宏
#ifdef AQL_DEBUG_VARS
  #define AQL_DEBUG_VAR(name, line) \
    do { \
      if (G(L)->debug_level > 0) \
        debug_var_access(L, name, line); \
    } while(0)
#else
  #define AQL_DEBUG_VAR(name, line) ((void)0)
#endif
```

### 3.3 智能降级机制 (新增)
```c
// 在Lua模式基础上增强降级策略
typedef enum FallbackLevel {
  FALLBACK_NONE,        /* 正常执行 */
  FALLBACK_JIT,         /* JIT回退到脚本 */
  FALLBACK_AOT,         /* AOT回退到JIT */
  FALLBACK_SCRIPT       /* 最终回退 */
} FallbackLevel;

typedef struct FallbackHandler {
  FallbackLevel try_compile_aot(Proto *f);
  FallbackLevel try_compile_jit(Proto *f);
  void log_fallback(FallbackLevel level, const char *reason);
} FallbackHandler;
```

## 4. 增强型三模式实现

### 4.1 脚本模式 (Lua标准 + 调试增强)
```c
// 标准Lua执行 + 可选调试信息
void execute_script_mode(lua_State *L, Proto *f) {
  /* 标准Lua执行逻辑 */
  luaV_execute(L);
  
  /* AQL增强调试 */
  if (AQL_DEBUG_VARS) {
    AQL_DEBUG_VAR(f->locvars[0].varname, f->linedefined);
  }
}
```

### 4.2 JIT模式 (LuaJIT优化 + 类型推断)
```c
// 基于LuaJIT的寄存器分配 + 类型推断
typedef struct JITVar {
  TValue base;          /* 标准Lua TValue */
  struct {
    uint8_t reg_idx;     /* 寄存器索引 */
    uint8_t spill_slot;  /* 溢出槽 */
    uint16_t type_hint;  /* 类型提示 */
  } jit_meta;
} JITVar;

void compile_jit_mode(lua_State *L, Proto *f) {
  /* 标准LuaJIT编译 */
  luaJIT_compile(L, f);
  
  /* AQL类型推断增强 */
  AQLTypeInfo type_info = infer_jit_types(f);
  apply_type_optimization(type_info);
}
```

### 4.3 AOT模式 (静态编译 + 逃逸分析)
```c
// 基于LLVM的AOT编译
typedef struct AOTVar {
  /* 编译期元数据，运行时不存在 */
  struct {
    int stack_offset;    /* 栈偏移或寄存器 */
    EscapeLevel escape;  /* 逃逸级别 */
    bool is_global;      /* 是否全局 */
  } aot_meta;
} AOTVar;

AOTResult compile_aot_mode(lua_State *L, Proto *f) {
  /* 逃逸分析 */
  EscapeLevel escape = analyze_escape(f);
  
  /* LLVM代码生成 */
  LLVMValueRef code = generate_llvm_code(f, escape);
  
  /* 验证并返回 */
  return validate_aot_compilation(code);
}
```

## 5. 完整生命周期管理

### 5.1 标准Lua GC + AQL增强
```c
// 保留标准Lua GC，增加AQL特定处理
void aql_gc_step(lua_State *L) {
  /* 标准Lua GC步骤 */
  luaC_step(L);
  
  /* AQL增强：调试信息清理 */
  if (AQL_DEBUG_VARS) {
    cleanup_debug_info(L);
  }
  
  /* AQL增强：类型信息清理 */
  cleanup_type_info(L);
}
```

### 5.2 完整内存模型
```c
// 标准Lua内存布局 + AQL扩展
/*
┌─────────────────────────────────────────┐
│              Lua标准内存布局              │
│ ┌─────────┐ ┌─────────┐ ┌─────────────┐ │
│ │ Eden    │ │Survivor │ │  Old Gen    │ │
│ │ (Lua)   │ │ (Lua)   │ │   (Lua)     │ │
│ └─────────┘ └─────────┘ └─────────────┘ │
│ ┌─────────────────────────────────────┐ │
│ │  AQL调试信息 (条件编译时存在)       │ │
│ │  AQL类型信息 (调试时存在)           │ │
│ └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
*/
```

## 6. 完整测试与验证

### 6.1 保留Lua测试 + AQL增强
```c
// 标准Lua回归测试 (100%保留)
run_lua_regression_tests();

// AQL增强测试
run_aql_type_inference_tests();
run_aql_debug_system_tests();
run_aql_fallback_tests();

// 性能对比测试
benchmark_lua_vs_aql();
```

### 6.2 生产验证标准
```c
// 完整验证清单
typedef struct ValidationSuite {
  bool lua_compatibility_passed;
  bool type_inference_accuracy > 95;
  bool debug_zero_cost_confirmed;
  bool fallback_mechanism_works;
  bool memory_leak_detected;
  bool performance_baseline_met;
} ValidationSuite;
```

## 7. 完整API兼容性

### 7.1 标准Lua API (100%保留)
```c
// 完全保留的Lua C API
lua_State *luaL_newstate(void);
void lua_close(lua_State *L);
int luaL_loadfile(lua_State *L, const char *filename);
int lua_pcall(lua_State *L, int nargs, int nresults, int errfunc);

/* 所有标准Lua函数完全保留 */
```

### 7.2 AQL增强API (新增)
```c
// AQL增强接口
AQLState *aql_newstate(void);
void aql_set_debug_level(AQLState *L, DebugLevel level);
AOTResult aql_compile_mode(AQLState *L, const char *mode);
int aql_get_type_info(AQLState *L, int index, AQLTypeInfo *info);
```

## 8. 完整部署配置

### 8.1 生产环境配置
```c
// 完整生产配置
typedef struct AQLProductionConfig {
  /* 标准Lua配置 */
  size_t gc_pause;
  size_t gc_stepmul;
  
  /* AQL增强配置 */
  DebugLevel debug_level;
  bool enable_aot;
  bool enable_jit;
  int jit_threshold;
  int max_complexity;
  
  /* 监控配置 */
  bool enable_perf_monitor;
  bool enable_memory_debug;
} AQLProductionConfig;

// 一键生产配置
#define AQL_PRODUCTION_DEFAULT { \
  .gc_pause = 200, \
  .gc_stepmul = 200, \
  .debug_level = DEBUG_NONE, \
  .enable_aot = true, \
  .enable_jit = true, \
  .jit_threshold = 1000, \
  .max_complexity = 100, \
  .enable_perf_monitor = false, \
  .enable_memory_debug = false \
}
```

## 9. 总结：完整增强版

**var5-design.md = var3基础 + 评估增强**

### 保留的Lua核心
- ✅ 100% Lua 5.4架构保留
- ✅ 完整TValue结构
- ✅ 标准作用域管理
- ✅ 原生GC系统
- ✅ 完整C API

### 新增的评估增强
- ✅ 智能类型推断系统
- ✅ 零成本调试架构
- ✅ 智能降级机制
- ✅ 完整测试验证
- ✅ 生产级配置

**这就是完整的变量系统：在Lua坚实基础上，通过评估建议实现生产就绪的终极设计！**