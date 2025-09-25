# AQL函数系统设计文档 v0.2 - 基于现有基础设施的融合方案

## 1. 现有基础设施分析

### 1.1 已实现的核心组件 ✅

#### **数据结构**（已完整实现）
```c
// src/aobject.h - 完整的Lua兼容结构
typedef struct Proto {
  CommonHeader;
  aql_byte numparams;      /* 固定参数数量 */
  aql_byte is_vararg;      /* 变参支持 */
  aql_byte maxstacksize;   /* 栈大小 */
  int sizeupvalues;        /* upvalue数量 */
  int sizek;               /* 常量数量 */
  int sizecode;            /* 指令数量 */
  TValue *k;               /* 常量数组 */
  Instruction *code;       /* 字节码 */
  struct Proto **p;        /* 嵌套函数 */
  Upvaldesc *upvalues;     /* upvalue描述 */
  // ... 调试信息
} Proto;

typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;         /* 函数原型 */
  UpVal *upvals[1];        /* upvalue数组 */
} LClosure;

typedef struct UpVal {
  CommonHeader;
  union {
    TValue *p;             /* 指向栈或自身 */
    ptrdiff_t offset;      /* 重分配时使用 */
  } v;
  union {
    struct {               /* 开放状态 */
      struct UpVal *next;
      struct UpVal **previous;
    } open;
    TValue value;          /* 关闭状态值 */
  } u;
} UpVal;
```

#### **函数管理**（已实现）
```c
// src/afunc.c - 完整的闭包管理
LClosure *aqlF_newLclosure(aql_State *L, int nupvals);
CClosure *aqlF_newCclosure(aql_State *L, int nupvals);
void aqlF_initupvals(aql_State *L, LClosure *cl);
UpVal *aqlF_findupval(aql_State *L, StkId level);
void aqlF_closeupval(aql_State *L, StkId level);
```

#### **VM指令**（已定义）
```c
// src/aopcodes.h - 函数相关指令已定义
OP_CALL,        /* A B C   函数调用 */
OP_TAILCALL,    /* A B     尾调用 */
OP_RET,         /* A B     返回多值 */
OP_RET_VOID,    /* A       返回空 */
OP_RET_ONE,     /* A       返回单值 */
OP_CLOSURE,     /* A Bx    闭包创建 */
```

#### **解析器支持**（部分实现）
```c
// src/aparser.c - 函数调用解析已实现
if (ls->t.token == TK_LPAREN) {
  // 函数调用语法已支持：name(args...)
  aqlK_codeABC(ls->fs, OP_CALL, v->u.info, nargs + 1, 2);
}
```

### 1.2 缺失的关键组件 ❌

#### **函数定义语法**
- ❌ `function name() { ... }` 语法解析
- ❌ 匿名函数 `function() { ... }`
- ❌ 参数列表解析
- ❌ 函数体编译

#### **VM指令实现**
- ❌ `OP_CLOSURE` 指令实现（标记为TODO）
- ❌ `OP_CALL` 完整实现（当前简化版）
- ❌ `OP_RET*` 系列指令实现

#### **编译器集成**
- ❌ 函数原型创建
- ❌ Upvalue分析和生成
- ❌ 嵌套函数支持

## 2. 融合设计方案

### 2.1 设计原则

1. **最大化复用**：充分利用现有的Proto、LClosure、UpVal结构
2. **渐进增强**：在现有基础上逐步添加缺失功能
3. **保持兼容**：不破坏现有的builtin函数调用机制
4. **简化实现**：相比完整Lua实现，简化不必要的复杂性

### 2.2 实现路线图

#### **阶段1：基础函数定义** 🎯
**目标**：支持最简单的函数定义和调用

**新增语法**：
```aql
function add(a, b) {
    return a + b
}
```

**实现要点**：
1. **扩展词法分析器**（`src/alex.c`）
   - `function` 关键字已存在
   - `return` 关键字需要添加

2. **扩展解析器**（`src/aparser.c`）
   ```c
   // 在statement()中添加
   case TK_FUNCTION: {
     funcstat(ls, line);
     break;
   }
   
   // 新增函数
   static void funcstat(LexState *ls, int line) {
     // function name(params) { body }
     expdesc v, b;
     aqlX_next(ls);  // skip 'function'
     
     // 解析函数名
     singlevar(ls, &v);
     
     // 解析函数体
     body(ls, &b, 0, line);
     
     // 存储函数到变量
     aqlK_storevar(ls->fs, &v, &b);
   }
   ```

3. **实现VM指令**（`src/avm.c`）
   ```c
   case OP_CLOSURE: {
     Proto *p = cl->p->p[GETARG_Bx(i)];
     LClosure *ncl = aqlF_newLclosure(L, p->sizeupvalues);
     ncl->p = p;
     setclLvalue2s(L, ra, ncl);
     
     // 初始化upvalue（复用现有函数）
     aqlF_initupvals(L, ncl);
     break;
   }
   
   case OP_RET_VOID: {
     return 1;  // 返回到调用者
   }
   ```

#### **阶段2：函数调用完善** 🎯
**目标**：完善现有的函数调用机制

**实现要点**：
1. **修复`aqlD_precall`**（`src/ado.c`）
   ```c
   AQL_API int aqlD_precall(aql_State *L, StkId func, int nResults) {
     TValue *f = s2v(func);
     
     if (ttisLclosure(f)) {
       // AQL函数调用
       LClosure *cl = clLvalue(f);
       Proto *p = cl->p;
       
       // 设置调用帧
       CallInfo *ci = /* 获取新调用帧 */;
       ci->func = func;
       ci->u.l.savedpc = p->code;
       
       return 0;  // 需要VM执行
     } else {
       // C函数调用（现有逻辑）
       return 1;  // 已处理
     }
   }
   ```

2. **完善`OP_CALL`实现**
   ```c
   case OP_CALL: {
     int b = GETARG_B(i);
     int nargs = b ? b - 1 : cast_int(L->top - ra) - 1;
     int nresults = GETARG_C(i) - 1;
     
     if (aqlD_precall(L, ra, nresults) == 0) {
       // AQL函数，需要VM执行
       continue;  // 重新进入VM循环
     }
     // C函数已在precall中处理
     break;
   }
   ```

#### **阶段3：Upvalue和闭包** 🎯
**目标**：支持闭包和变量捕获

**新增语法**：
```aql
function make_counter(start) {
    let count = start
    return function() {
        count = count + 1
        return count
    }
}
```

**实现要点**：
1. **Upvalue分析**（扩展`src/aparser.c`）
   - 复用现有的`searchupvalue`、`newupvalue`函数
   - 在变量查找时标记upvalue

2. **闭包创建**（完善`OP_CLOSURE`）
   - 复用现有的`aqlF_findupval`函数
   - 正确链接开放upvalue

#### **阶段4：高级特性** 🎯
**目标**：匿名函数、变参、尾调用优化

### 2.3 关键融合点

#### **2.3.1 与现有builtin系统融合**
```c
// 保持现有的builtin调用不变
if (v->k == VBUILTIN) {
  // 现有逻辑：OP_BUILTIN
  aqlK_codeABC(ls->fs, OP_BUILTIN, result_reg, v->u.info, nargs);
} else if (v->k == VFUNCTION) {
  // 新增：用户定义函数
  aqlK_codeABC(ls->fs, OP_CALL, v->u.info, nargs + 1, 2);
}
```

#### **2.3.2 与现有类型系统融合**
```c
// 在aobject.h中，函数类型已定义
#define AQL_TFUNCTION    6
#define ttisfunction(o)  checktype(o, AQL_TFUNCTION)

// 无需修改，直接复用
```

#### **2.3.3 与现有REPL系统融合**
```c
// src/arepl.c - 函数定义检测
if (strncmp(input, "function ", 9) == 0) {
  // 函数定义，不添加return
  return 1;
}
```

## 3. 具体实现计划

### 3.1 第一步：最小可用函数（1-2天）

**目标**：实现最基础的函数定义和调用
```aql
function test() {
    print("Hello from function!")
}
test()
```

**修改文件**：
- `src/alex.c`: 添加`return`关键字
- `src/aparser.c`: 添加`funcstat`和`body`函数
- `src/avm.c`: 实现`OP_CLOSURE`和`OP_RET_VOID`
- `src/ado.c`: 修复`aqlD_precall`

### 3.2 第二步：参数和返回值（2-3天）

**目标**：支持参数传递和返回值
```aql
function add(a, b) {
    return a + b
}
let result = add(3, 4)
```

**修改文件**：
- `src/aparser.c`: 实现参数解析
- `src/avm.c`: 完善`OP_CALL`和`OP_RET`
- `src/acode.c`: 参数寄存器分配

### 3.3 第三步：闭包支持（3-4天）

**目标**：支持变量捕获和闭包
```aql
function make_adder(x) {
    return function(y) {
        return x + y
    }
}
```

**修改文件**：
- `src/aparser.c`: Upvalue分析
- `src/avm.c`: 完善`OP_CLOSURE`的upvalue处理
- `src/afunc.c`: 可能需要微调upvalue管理

## 4. 测试策略

### 4.1 单元测试
```aql
// test/regression/functions/basic_function.aql
function hello() {
    print("Hello, World!")
}
hello()

// test/regression/functions/function_params.aql  
function add(a, b) {
    return a + b
}
assert(add(2, 3) == 5)

// test/regression/functions/closure_basic.aql
function make_counter() {
    let count = 0
    return function() {
        count = count + 1
        return count
    }
}
let counter = make_counter()
assert(counter() == 1)
assert(counter() == 2)
```

### 4.2 集成测试
- 与现有控制流语句集成
- 与现有变量系统集成  
- 与现有容器类型集成

## 5. 总结

### 5.1 优势
1. **基础设施完备**：90%的核心数据结构已实现
2. **架构成熟**：基于Lua的成熟设计
3. **渐进实现**：可以分阶段交付功能
4. **风险可控**：不会破坏现有功能

### 5.2 工作量评估
- **阶段1**：2-3天（基础函数定义）
- **阶段2**：3-4天（参数和返回值）
- **阶段3**：4-5天（闭包支持）
- **总计**：约2周完成核心功能

### 5.3 关键决策
1. **复用现有结构**：不重新设计Proto/LClosure/UpVal
2. **保持兼容性**：builtin函数调用机制不变
3. **简化实现**：暂不支持协程、元表等高级特性
4. **分阶段交付**：每个阶段都有可测试的功能

**下一步**：开始阶段1的实现，从最简单的无参数函数开始。
