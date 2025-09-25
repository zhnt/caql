# AQL函数系统设计文档 v0.1

## 1. 概述

本文档基于对Lua 5.4.8函数系统的深入分析，为AQL设计一个完整的函数系统，包括函数定义、调用、闭包和upvalue管理。

## 2. Lua函数系统分析

### 2.1 核心数据结构

#### 2.1.1 Proto（函数原型）
```c
typedef struct Proto {
  CommonHeader;
  lu_byte numparams;      /* 固定参数数量 */
  lu_byte is_vararg;      /* 是否为变参函数 */
  lu_byte maxstacksize;   /* 函数需要的最大栈大小 */
  int sizeupvalues;       /* upvalue数组大小 */
  int sizek;              /* 常量数组大小 */
  int sizecode;           /* 指令数组大小 */
  TValue *k;              /* 常量数组 */
  Instruction *code;      /* 字节码指令 */
  struct Proto **p;       /* 嵌套函数原型 */
  Upvaldesc *upvalues;    /* upvalue描述信息 */
  LocVar *locvars;        /* 局部变量信息（调试用） */
  TString *source;        /* 源文件名 */
} Proto;
```

#### 2.1.2 Closure（闭包）
```c
// Lua闭包
typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;        /* 函数原型 */
  UpVal *upvals[1];       /* upvalue数组 */
} LClosure;

// C闭包
typedef struct CClosure {
  ClosureHeader;
  lua_CFunction f;        /* C函数指针 */
  TValue upvalue[1];      /* upvalue数组 */
} CClosure;
```

#### 2.1.3 UpVal（上值）
```c
typedef struct UpVal {
  CommonHeader;
  union {
    TValue *p;            /* 指向栈或自身value */
    ptrdiff_t offset;     /* 栈重分配时使用 */
  } v;
  union {
    struct {              /* 开放状态 */
      struct UpVal *next;
      struct UpVal **previous;
    } open;
    TValue value;         /* 关闭状态的值 */
  } u;
} UpVal;
```

### 2.2 关键机制

#### 2.2.1 函数调用流程
1. **luaD_precall**: 准备函数调用，区分C函数和Lua函数
2. **luaV_execute**: 执行Lua函数字节码
3. **luaD_poscall**: 处理函数返回，移动结果到正确位置

#### 2.2.2 Upvalue管理
1. **开放upvalue**: 指向活跃栈位置，多个闭包可共享
2. **关闭upvalue**: 复制值到upvalue内部，独立存储
3. **upvalue链表**: 按栈位置排序的全局链表管理

#### 2.2.3 闭包创建
1. **编译时**: 分析变量引用，生成upvalue描述
2. **运行时**: 创建闭包，查找或创建upvalue
3. **优化**: 原型缓存机制减少闭包创建开销

## 3. AQL函数系统设计

### 3.1 设计原则

1. **简化优先**: 相比Lua，AQL函数系统更简洁
2. **性能考虑**: 保留Lua的核心优化机制
3. **类型安全**: 结合AQL的类型系统
4. **渐进实现**: 分阶段实现，先基础后高级

### 3.2 语法设计

#### 3.2.1 函数定义语法
```aql
// 基础函数定义
function add(a, b) {
    return a + b
}

// 匿名函数
let multiply = function(x, y) {
    return x * y
}

// 箭头函数（简化语法）
let square = (x) => x * x

// 变参函数
function sum(...args) {
    let total = 0
    for arg in args {
        total = total + arg
    }
    return total
}
```

#### 3.2.2 函数调用语法
```aql
// 普通调用
let result = add(3, 4)

// 方法调用（如果支持对象）
obj.method(arg1, arg2)

// 链式调用
getValue().process().format()
```

### 3.3 核心数据结构

#### 3.3.1 AqlProto（函数原型）
```c
typedef struct AqlProto {
  AqlGCHeader;
  aql_byte numparams;       /* 固定参数数量 */
  aql_byte is_vararg;       /* 是否为变参函数 */
  aql_byte maxstacksize;    /* 最大栈大小 */
  int sizeupvalues;         /* upvalue数量 */
  int sizek;                /* 常量数量 */
  int sizecode;             /* 指令数量 */
  TValue *k;                /* 常量数组 */
  Instruction *code;        /* 字节码 */
  struct AqlProto **p;      /* 嵌套函数 */
  AqlUpvaldesc *upvalues;   /* upvalue描述 */
  AqlString *name;          /* 函数名（调试用） */
  AqlString *source;        /* 源文件名 */
  int linedefined;          /* 定义行号 */
} AqlProto;
```

#### 3.3.2 AqlClosure（闭包）
```c
typedef struct AqlLClosure {
  AqlGCHeader;
  aql_byte nupvalues;       /* upvalue数量 */
  struct AqlProto *p;       /* 函数原型 */
  AqlUpVal *upvals[1];      /* upvalue数组 */
} AqlLClosure;

typedef struct AqlCClosure {
  AqlGCHeader;
  aql_byte nupvalues;       /* upvalue数量 */
  aql_CFunction f;          /* C函数指针 */
  TValue upvalue[1];        /* upvalue数组 */
} AqlCClosure;
```

#### 3.3.3 AqlUpVal（上值）
```c
typedef struct AqlUpVal {
  AqlGCHeader;
  TValue *v;                /* 指向值的位置 */
  union {
    TValue value;           /* 关闭时的值 */
    struct {                /* 开放时的链表 */
      struct AqlUpVal *next;
      int level;            /* 栈层级 */
    } open;
  } u;
} AqlUpVal;
```

### 3.4 编译器扩展

#### 3.4.1 语法分析
```c
// 在aparser.c中添加
static void funcstat(LexState *ls, int line) {
  // function name(params) { body }
  expdesc v, b;
  aqlX_next(ls);  // skip 'function'
  
  // 解析函数名
  int ismethod = funcname(ls, &v);
  
  // 解析函数体
  body(ls, &b, ismethod, line);
  
  // 存储函数
  aqlK_storevar(ls->fs, &v, &b);
}

static void body(LexState *ls, expdesc *e, int ismethod, int line) {
  // (params) { statements }
  FuncState new_fs;
  BlockCnt bl;
  
  new_fs.f = addprototype(ls);
  new_fs.f->linedefined = line;
  open_func(ls, &new_fs, &bl);
  
  checknext(ls, '(');
  if (ismethod) {
    new_localvarliteral(ls, "self");
    adjustlocalvars(ls, 1);
  }
  parlist(ls);
  checknext(ls, ')');
  checknext(ls, '{');
  statlist(ls);
  checknext(ls, '}');
  
  new_fs.f->lastlinedefined = ls->linenumber;
  codeclosure(ls, e);
  close_func(ls);
}
```

#### 3.4.2 代码生成
```c
// 在acode.c中添加
void aqlK_closure(FuncState *fs, expdesc *e) {
  AqlProto *f = fs->f;
  int oldsize = f->sizep;
  
  // 分配新的原型
  luaM_growvector(fs->ls->L, f->p, fs->np, f->sizep, 
                  AqlProto *, MAXARG_Bx, "functions");
  
  // 生成CLOSURE指令
  e->u.info = aqlK_codeABx(fs, OP_CLOSURE, 0, fs->np);
  e->k = VRELOCABLE;
  
  // 处理upvalue
  for (int i = 0; i < f->p[fs->np]->sizeupvalues; i++) {
    // 生成GETUPVAL或MOVE指令
  }
  
  fs->np++;
}
```

### 3.5 虚拟机扩展

#### 3.5.1 新增指令
```c
// 在avm.h中添加
typedef enum {
  // ... 现有指令 ...
  OP_CLOSURE,     /* A Bx    R(A) := closure(KPROTO[Bx]) */
  OP_CALL,        /* A B C   R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1)) */
  OP_TAILCALL,    /* A B C   return R(A)(R(A+1), ... ,R(A+B-1)) */
  OP_RETURN,      /* A B     return R(A), ... ,R(A+B-2) */
  OP_GETUPVAL,    /* A B     R(A) := UpValue[B] */
  OP_SETUPVAL,    /* A B     UpValue[B] := R(A) */
} OpCode;
```

#### 3.5.2 指令实现
```c
// 在avm.c中添加
case OP_CLOSURE: {
  AqlProto *p = cl->p->p[GETARG_Bx(i)];
  AqlLClosure *ncl = aqlF_newLclosure(L, p->sizeupvalues);
  ncl->p = p;
  setclLvalue2s(L, ra, ncl);
  
  // 初始化upvalue
  for (int j = 0; j < p->sizeupvalues; j++) {
    if (p->upvalues[j].instack) {
      ncl->upvals[j] = aqlF_findupval(L, base + p->upvalues[j].idx);
    } else {
      ncl->upvals[j] = cl->upvals[p->upvalues[j].idx];
    }
  }
  break;
}

case OP_CALL: {
  int b = GETARG_B(i);
  int nargs = b ? b - 1 : cast_int(L->top - ra) - 1;
  int nresults = GETARG_C(i) - 1;
  
  if (aqlD_precall(L, ra, nresults)) {
    // Lua函数，继续执行
    continue;
  } else {
    // C函数，已完成调用
    if (nresults >= 0) L->top = ci->top;
  }
  break;
}

case OP_RETURN: {
  int b = GETARG_B(i);
  if (b != 0) L->top = ra + b - 1;
  if (aqlD_poscall(L, ci, cast_int(L->top - ra))) {
    // 返回到C代码
    return;
  }
  // 继续执行Lua代码
  continue;
}
```

### 3.6 运行时系统

#### 3.6.1 函数调用管理
```c
// 在ado.c中实现
int aqlD_precall(aql_State *L, StkId func, int nresults) {
  switch (ttype(func)) {
    case AQL_TLCL: {  // Lua闭包
      AqlLClosure *cl = clLvalue(func);
      AqlProto *p = cl->p;
      int n = cast_int(L->top - func) - 1;  // 参数数量
      
      // 检查栈空间
      aqlD_checkstack(L, p->maxstacksize);
      
      // 设置调用信息
      CallInfo *ci = aqlE_extendCI(L);
      ci->func = func;
      ci->base = func + 1;
      ci->top = ci->base + p->maxstacksize;
      ci->savedpc = p->code;
      ci->nresults = nresults;
      
      // 补齐参数
      for (; n < p->numparams; n++) {
        setnilvalue(L->top++);
      }
      
      L->ci = ci;
      return 1;  // Lua函数
    }
    
    case AQL_TCCL: {  // C闭包
      AqlCClosure *cl = clCvalue(func);
      int n = (*cl->f)(L);
      aqlD_poscall(L, L->ci, n);
      return 0;  // C函数
    }
    
    default:
      aqlG_typeerror(L, func, "call");
  }
}
```

#### 3.6.2 Upvalue管理
```c
// 在afunc.c中实现
AqlUpVal *aqlF_findupval(aql_State *L, StkId level) {
  AqlUpVal **pp = &L->openupval;
  AqlUpVal *p;
  
  // 在开放upvalue链表中查找
  while ((p = *pp) != NULL && p->u.open.level >= level) {
    if (p->u.open.level == level) {
      return p;  // 找到现有的
    }
    pp = &p->u.open.next;
  }
  
  // 创建新的upvalue
  p = aqlM_new(L, AqlUpVal);
  p->v = level;
  p->u.open.level = level;
  p->u.open.next = *pp;
  *pp = p;
  
  return p;
}

void aqlF_closeupval(aql_State *L, StkId level) {
  AqlUpVal *uv;
  while (L->openupval != NULL && 
         (uv = L->openupval)->u.open.level >= level) {
    L->openupval = uv->u.open.next;
    
    // 关闭upvalue：复制值
    setobj(L, &uv->u.value, uv->v);
    uv->v = &uv->u.value;
  }
}
```

### 3.7 实现阶段

#### 第一阶段：基础函数
- [ ] 函数定义语法解析
- [ ] 基础Proto和Closure结构
- [ ] 简单函数调用（无upvalue）
- [ ] return语句

#### 第二阶段：闭包支持
- [ ] Upvalue分析和生成
- [ ] 闭包创建和管理
- [ ] 嵌套函数支持

#### 第三阶段：高级特性
- [ ] 变参函数
- [ ] 尾调用优化
- [ ] 匿名函数和箭头函数
- [ ] 函数作为一等公民

#### 第四阶段：优化和调试
- [ ] 调用栈优化
- [ ] 调试信息支持
- [ ] 性能分析和优化

### 3.8 测试计划

#### 3.8.1 单元测试
```aql
// 基础函数调用
function test_basic_call() {
    function add(a, b) {
        return a + b
    }
    assert(add(2, 3) == 5)
}

// 闭包测试
function test_closure() {
    function make_counter(start) {
        let count = start
        return function() {
            count = count + 1
            return count
        }
    }
    
    let counter = make_counter(10)
    assert(counter() == 11)
    assert(counter() == 12)
}

// 递归测试
function test_recursion() {
    function factorial(n) {
        if n <= 1 {
            return 1
        } else {
            return n * factorial(n - 1)
        }
    }
    assert(factorial(5) == 120)
}
```

#### 3.8.2 集成测试
- 与现有AQL语言特性的集成
- 性能基准测试
- 内存泄漏检测

## 4. 总结

本设计文档为AQL函数系统提供了完整的架构方案，基于Lua的成熟设计，结合AQL的特点进行了适配和简化。实现将分阶段进行，确保每个阶段都有可测试的功能。

关键创新点：
1. **简化的upvalue管理**：相比Lua更直接的实现
2. **类型感知的函数调用**：结合AQL类型系统
3. **渐进式实现**：分阶段交付可用功能
4. **现代语法支持**：箭头函数等现代特性

下一步将开始第一阶段的实现工作。
