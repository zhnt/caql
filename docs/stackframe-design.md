# AQL 栈帧设计文档

## 1. 概述

AQL的栈帧设计严格借鉴Lua 5.4的成熟架构，专注于核心的函数调用机制。采用双栈设计：值栈存储数据，调用栈管理函数调用层次。

### 1.1 设计目标
- **高性能**: 接近C的函数调用性能
- **类型安全**: 配合AQL的强类型系统
- **内存高效**: 紧凑的内存布局，支持大规模并发
- **调试友好**: 丰富的调试信息支持

### 1.2 核心改进
相比Lua的设计，AQL栈帧的主要改进：
- **容器类型优化**: 专门支持array/slice/dict/vector的高效传递
- **SIMD对齐**: 向量数据的内存对齐优化
- **类型信息**: 更丰富的类型元数据支持

## 2. 核心数据结构

### 2.1 AQL状态机 (aql_State)

```c
// AQL虚拟机状态 (基于lua_State设计)
struct aql_State {
  CommonHeader;              // GC头
  lu_byte status;            // 线程状态
  lu_byte allowhook;         // 允许调试钩子
  unsigned short nci;        // CallInfo数量
  
  // 栈管理 (与Lua完全一致)
  StkIdRel top;              // 栈顶指针
  global_State *a_G;         // 全局状态
  CallInfo *ci;              // 当前调用信息
  StkIdRel stack_last;       // 栈结束位置
  StkIdRel stack;            // 栈基址
  
  // 闭包和上值 (与Lua一致)
  UpVal *openupval;          // 开放上值链表
  StkIdRel tbclist;          // 待关闭变量
  
  // 错误处理 (与Lua一致)
  struct aql_longjmp *errorJmp; // 错误恢复点
  CallInfo base_ci;          // 基础调用信息
  
  // 调试和性能 (与Lua一致)
  volatile aql_Hook hook;    // 调试钩子
  ptrdiff_t errfunc;         // 错误处理函数
  a_uint32 nCcalls;          // C调用嵌套数
};
```

### 2.2 调用信息 (CallInfo)

```c
// 函数调用信息 (基于Lua CallInfo但扩展容器支持)
struct CallInfo {
  StkIdRel func;             // 函数在栈中的位置
  StkIdRel top;              // 该函数的栈顶
  struct CallInfo *previous, *next; // 调用链表
  
  union {
    struct {  // AQL函数专用
      const Instruction *savedpc;    // 保存的程序计数器
      volatile a_signalT trap;       // 调试跟踪
      int nextraargs;                // 变参数量
    } a;
    struct {  // C函数专用
      aql_KFunction k;               // 继续函数
      ptrdiff_t old_errfunc;
      aql_KContext ctx;              // 上下文
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
  
  short nresults;          // 期望返回值数量
  unsigned short callstatus; // 调用状态标志
};
```

### 2.3 统一值表示系统 (TValue)

```c
// AQL统一值类型 (基于Lua TValue扩展)
#define TValuefields  Value value_; lu_byte tt_

typedef struct TValue {
  TValuefields;
} TValue;

// 值的联合体 (扩展Lua的Value)
typedef union Value {
  // 基础类型 (完全兼容Lua)
  struct GCObject *gc;    // 可回收对象
  void *p;                // 轻量用户数据
  aql_CFunction f;        // 轻量C函数
  aql_Integer i;          // 整数
  aql_Number n;           // 浮点数
  lu_byte ub;             // 字节值
  
  // AQL扩展: 容器类型快速访问
  struct {
    void *ptr;            // 容器数据指针
    size_t length;        // 长度 (用于快速访问)
    lu_byte container_type; // 容器类型标识
  } container;
  
  // AQL扩展: SIMD向量快速访问
  struct {
    void *data;           // SIMD对齐的数据
    lu_byte simd_width;   // SIMD宽度
    lu_byte element_type; // 元素类型
  } vector;
} Value;

// 栈中的值 (保持Lua StackValue设计但扩展)
typedef union StackValue {
  TValue val;              // 标准TValue
  
  struct {                 // TBC (to-be-closed) 变量
    TValuefields;
    unsigned short delta;  // 距离下一个TBC的偏移
  } tbclist;
  
  // AQL扩展: 容器内联优化
  struct {
    TValuefields;
    union {
      // 小数组内联 (避免堆分配)
      struct {
        size_t length;
        TValue data[INLINE_ARRAY_SIZE]; // 通常是4-8个元素
      } inline_array;
      
      // 小向量内联 (SIMD友好)
      struct {
        size_t length;
        lu_byte simd_width;
        union {
          float f32[8];   // AVX 256位
          double f64[4];  // AVX 256位
          int32_t i32[8]; // AVX 256位
          int64_t i64[4]; // AVX 256位
        } simd_data;
      } inline_vector;
    } inline_container;
  } container_value;
} StackValue;

typedef StackValue *StkId;
```

### 2.4 类型标签系统扩展

```c
// AQL类型标签 (扩展Lua的类型系统)
// 基础类型 (完全兼容Lua)
#define AQL_TNIL        0
#define AQL_TBOOLEAN    1
#define AQL_TLIGHTUSERDATA 2
#define AQL_TNUMBER     3
#define AQL_TSTRING     4
#define AQL_TTABLE      5  // 保留兼容性，但不推荐使用
#define AQL_TFUNCTION   6
#define AQL_TUSERDATA   7
#define AQL_TTHREAD     8

// AQL扩展类型
#define AQL_TARRAY      9   // array<T,N>
#define AQL_TSLICE      10  // slice<T>
#define AQL_TDICT       11  // dict<K,V>
#define AQL_TVECTOR     12  // vector<T,N>
#define AQL_TSTRUCT     13  // struct
#define AQL_TCLASS      14  // class
#define AQL_TINTERFACE  15  // interface

// 类型检查宏 (扩展Lua的类型检查)
#define ttisnil(o)        checktag((o), AQL_TNIL)
#define ttisboolean(o)    checktag((o), AQL_TBOOLEAN)
#define ttisnumber(o)     checktag((o), AQL_TNUMBER)
#define ttisstring(o)     checktag((o), AQL_TSTRING)
#define ttisfunction(o)   checktag((o), AQL_TFUNCTION)

// AQL容器类型检查
#define ttisarray(o)      checktag((o), AQL_TARRAY)
#define ttisslice(o)      checktag((o), AQL_TSLICE)
#define ttisdict(o)       checktag((o), AQL_TDICT)
#define ttisvector(o)     checktag((o), AQL_TVECTOR)

// 容器类型判断
#define ttiscontainer(o)  (ttype(o) >= AQL_TARRAY && ttype(o) <= AQL_TVECTOR)

// 内联优化判断
#define can_inline_array(arr)   ((arr)->length <= INLINE_ARRAY_SIZE)
#define can_inline_vector(vec)  ((vec)->length <= MAX_INLINE_VECTOR_SIZE)
```

## 3. 栈帧布局设计

### 3.1 函数调用栈帧布局

```c
/*
AQL函数调用的栈帧布局 (基于Lua设计):

高地址  ┌─────────────────┐ ← ci->top (函数栈顶)
       │ 临时计算区域     │   (寄存器/局部变量)
       ├─────────────────┤
       │ 局部变量n       │   (可能是容器类型)
       ├─────────────────┤
       │ 局部变量1       │   (TValue表示)
       ├─────────────────┤ ← ci->func + 1 + numparams
       │ 变参区域        │   (可变参数，如果有)
       ├─────────────────┤
       │ 参数n           │   (支持容器类型传递)
       ├─────────────────┤
       │ 参数1           │   (统一TValue表示)
       ├─────────────────┤ ← ci->func + 1
       │ 函数对象        │   (Function/Closure)
低地址  └─────────────────┘ ← ci->func

容器传递策略:
- array<T,N>: 小数组可能内联，大数组按引用
- slice<T>: 总是按引用(slice头)
- dict<K,V>: 总是按引用
- vector<T,N>: SIMD对齐，可能内联或引用
*/
```

### 3.2 栈帧内存布局优化

```c
/*
栈帧布局 (统一TValue + 优化策略):

高地址  ┌─────────────────┐ ← ci->top
       │ TValue: 临时值  │   (统一TValue表示)
       ├─────────────────┤
       │ TValue: local_n │   (可能内联小容器)
       ├─────────────────┤
       │ TValue: local_1 │   (统一接口，性能优化)
       ├─────────────────┤ ← ci->func + 1 + numparams
       │ TValue: arg_n   │   (大容器: GC对象指针)
       ├─────────────────┤   (小容器: 内联数据)
       │ TValue: arg_1   │   (向量: SIMD对齐)
       ├─────────────────┤ ← ci->func + 1
       │ TValue: function│   (Function/Closure对象)
低地址  └─────────────────┘ ← ci->func

优化策略:
1. 统一TValue接口 - 保持编程模型简单
2. 内联小对象 - array<int,4>, vector<float,8>等直接存储在栈中
3. 引用大对象 - 大数组、字典等存储GC对象指针
4. SIMD对齐 - 向量数据保证内存对齐
*/

// 栈值的智能设置 (内联 vs 引用)
static void setstack_smartvalue(aql_State *A, StkId slot, TValue *val) {
  switch (ttype(val)) {
    case AQL_TARRAY: {
      ArrayType *arr = arrayvalue(val);
      if (can_inline_array(arr) && fits_in_stackslot(arr)) {
        // 小数组内联到栈中
        settt_(s2v(slot), AQL_TARRAY | INLINE_FLAG);
        inline_array_to_stack(slot, arr);
      } else {
        // 大数组使用标准GC对象
        setobj(A, s2v(slot), val);
      }
      break;
    }
    
    case AQL_TVECTOR: {
      VectorType *vec = vectorvalue(val);
      if (can_inline_vector(vec)) {
        // 小向量内联，保证SIMD对齐
        settt_(s2v(slot), AQL_TVECTOR | INLINE_FLAG);
        inline_vector_to_stack(slot, vec);
      } else {
        setobj(A, s2v(slot), val);
      }
      break;
    }
    
    case AQL_TSLICE:
    case AQL_TDICT: {
      // slice和dict总是引用类型
      setobj(A, s2v(slot), val);
      break;
    }
    
    default: {
      // 其他类型使用标准设置
      setobj(A, s2v(slot), val);
      break;
    }
  }
}
```

## 4. 栈帧操作接口

### 4.1 基础栈帧操作

```c
// 栈帧创建和管理 (基于Lua接口)
CallInfo *aql_createframe(aql_State *A, StkId func, int nresults);
void aql_destroyframe(aql_State *A, CallInfo *ci);
void aql_pushframe(aql_State *A, CallInfo *ci);
CallInfo *aql_popframe(aql_State *A);

// 栈空间管理 (与Lua一致)
int aql_checkstack(aql_State *A, int n);
int aql_growstack(aql_State *A, int n);
void aql_shrinkstack(aql_State *A);

// 统一TValue栈操作 (完全兼容Lua接口)
void aql_pushvalue(aql_State *A, const TValue *val);      // 通用push
void aql_pushnil(aql_State *A);                           // push nil
void aql_pushboolean(aql_State *A, int b);                // push boolean
void aql_pushinteger(aql_State *A, aql_Integer n);        // push integer
void aql_pushnumber(aql_State *A, aql_Number n);          // push number
void aql_pushstring(aql_State *A, const char *s);         // push string

// 容器类型的智能push (统一TValue + 优化)
void aql_pusharray(aql_State *A, ArrayType *arr);         // 智能内联/引用
void aql_pushslice(aql_State *A, SliceType *slice);       // 引用类型
void aql_pushdict(aql_State *A, DictType *dict);          // 引用类型
void aql_pushvector(aql_State *A, VectorType *vec);       // 智能SIMD对齐

// 统一TValue栈访问 (与Lua兼容)
TValue *aql_index2value(aql_State *A, int idx);           // 获取TValue指针
int aql_type(aql_State *A, int idx);                      // 获取类型标签
bool aql_isnil(aql_State *A, int idx);                    // 类型检查
bool aql_isboolean(aql_State *A, int idx);
bool aql_isnumber(aql_State *A, int idx);
bool aql_isstring(aql_State *A, int idx);

// 容器类型检查和访问
bool aql_isarray(aql_State *A, int idx);
bool aql_isslice(aql_State *A, int idx);
bool aql_isdict(aql_State *A, int idx);
bool aql_isvector(aql_State *A, int idx);

ArrayType *aql_toarray(aql_State *A, int idx);            // 支持内联/引用
SliceType *aql_toslice(aql_State *A, int idx);
DictType *aql_todict(aql_State *A, int idx);
VectorType *aql_tovector(aql_State *A, int idx);          // 支持SIMD对齐

// 栈顶操作 (完全兼容Lua)
int aql_gettop(aql_State *A);                             // 获取栈顶位置
void aql_settop(aql_State *A, int idx);                   // 设置栈顶位置
void aql_pop(aql_State *A, int n);                        // 弹出n个值
```

## 5. 函数调用流程

### 5.1 标准函数调用 (基于Lua设计)

```c
// 函数调用准备 (基于luaD_precall)
CallInfo *aql_precall(aql_State *A, StkId func, int nresults) {
retry:
  switch (ttype(s2v(func))) {
    case AQL_TCCLOSURE:    // C闭包
    case AQL_TLCFUNCTION:  // 轻量C函数
      return precall_C(A, func, nresults);
      
    case AQL_TLCLOSURE: {  // AQL函数
      LClosure *cl = clLvalue(s2v(func));
      Proto *p = cl->p;
      int nargs = cast_int(A->top.p - func) - 1;
      int fsize = p->maxstacksize;
      
      // 检查栈空间
      checkstackGC(A, fsize, func);
      
      // 创建新栈帧 (与Lua完全一致)
      CallInfo *ci = prepCallInfo(A, func, nresults, 0, func + 1 + fsize);
      ci->u.a.savedpc = p->code;
      
      return ci;
    }
    
    default:
      aql_error(A, "attempt to call a non-function value");
  }
}

// 函数调用后处理 (基于luaD_poscall)
void aql_poscall(aql_State *A, CallInfo *ci, int nres) {
  StkId func = ci->func.p;
  StkId res = func;
  int wanted = ci->nresults;
  
  // 移动返回值到正确位置
  moveresults(A, res, nres, wanted);
  
  // 恢复栈顶
  A->top.p = res + wanted;
  
  // 弹出调用信息
  A->ci = ci->previous;
}
```

### 5.2 容器参数优化

```c
// 容器参数准备 (AQL特有)
static void prepare_container_args(aql_State *A, CallInfo *ci, Proto *p, int nargs) {
  // 在函数调用前优化容器类型参数的传递
  for (int i = 0; i < min(nargs, p->numparams); i++) {
    StkId arg = ci->func.p + 1 + i;
    TValue *val = s2v(arg);
    
    // 根据容器类型进行优化
    if (ttiscontainer(val)) {
      optimize_container_arg(A, arg, val);
    }
  }
}

// 单个容器参数优化
static void optimize_container_arg(aql_State *A, StkId arg, TValue *val) {
  switch (ttype(val)) {
    case AQL_TARRAY: {
      ArrayType *arr = arrayvalue(val);
      if (can_inline_array(arr)) {
        // 小数组内联到栈中，减少指针跳转
        setstack_smartvalue(A, arg, val);
      }
      break;
    }
    
    case AQL_TVECTOR: {
      VectorType *vec = vectorvalue(val);
      // 确保向量数据SIMD对齐
      if (!is_simd_aligned(vec->data)) {
        realign_vector_data(A, vec);
      }
      break;
    }
    
    // slice和dict保持引用传递
    default:
      break;
  }
}
```

## 6. 性能优化

### 6.1 栈帧内存优化

```c
// 栈帧内存池 (减少malloc/free开销)
typedef struct StackFramePool {
  CallInfo *free_list;     // 空闲CallInfo链表
  int pool_size;           // 池大小
  int peak_usage;          // 峰值使用量
} StackFramePool;

// 快速栈帧分配
CallInfo *fast_alloc_callinfo(aql_State *A) {
  StackFramePool *pool = A->frame_pool;
  
  if (pool->free_list) {
    CallInfo *ci = pool->free_list;
    pool->free_list = ci->next;
    memset(ci, 0, sizeof(CallInfo));  // 快速清零
    return ci;
  }
  
  // 池空，分配新的
  return (CallInfo *)aql_realloc(A, NULL, 0, sizeof(CallInfo));
}

// 快速栈帧释放
void fast_free_callinfo(aql_State *A, CallInfo *ci) {
  StackFramePool *pool = A->frame_pool;
  
  // 放回池中
  ci->next = pool->free_list;
  pool->free_list = ci;
}
```

### 6.2 智能栈收缩

```c
// 智能栈收缩 (基于Lua策略但优化)
void smart_stack_shrink(aql_State *A) {
  int current_usage = stackinuse(A);
  int total_size = stacksize(A);
  
  // 只有当使用率低于25%且总大小超过基础大小时才收缩
  if (current_usage * 4 < total_size && total_size > BASIC_STACK_SIZE) {
    int new_size = max(current_usage * 2, BASIC_STACK_SIZE);
    aql_reallocstack(A, new_size);
  }
}
```

## 7. 调试支持

### 7.1 栈跟踪生成

```c
// 栈跟踪生成 (基于Lua但增强)
void aql_generate_stacktrace(aql_State *A, StackTrace *trace) {
  trace->frame_count = 0;
  
  for (CallInfo *ci = A->ci; ci != NULL; ci = ci->previous) {
    StackFrameInfo *frame = &trace->frames[trace->frame_count++];
    
    frame->func_ptr = ci->func.p;
    frame->call_type = ci->callstatus;
    frame->debug_info = get_debug_info(A, ci);
    
    if (trace->frame_count >= MAX_STACK_FRAMES) break;
  }
}
```

## 8. 与现有组件的集成

### 8.1 与GC的集成

```c
// GC栈扫描 (基于Lua但支持容器)
void aql_gc_mark_stack(global_State *g, aql_State *A) {
  // 扫描值栈
  for (StkId p = A->stack.p; p < A->top.p; p++) {
    markvalue(g, s2v(p));
    
    // 处理内联容器的特殊标记
    if (s2v(p)->tt_ & INLINE_FLAG) {
      mark_inline_container_contents(g, p);
    }
  }
  
  // 扫描CallInfo链
  for (CallInfo *ci = A->ci; ci != NULL; ci = ci->previous) {
    markobject(g, ci);
  }
}
```

## 9. 总结

AQL的栈帧设计严格遵循Lua的成熟架构，同时针对容器类型进行了最小化的扩展：

### 9.1 核心特点
- **Lua兼容**: 保持与Lua栈帧的完全兼容性
- **容器优化**: 最小化扩展支持AQL的容器类型
- **统一TValue**: 保持简洁的编程模型
- **性能优化**: 智能内联和SIMD对齐

### 9.2 实施路径
1. **Phase 1**: 实现基础栈帧，完全兼容Lua
2. **Phase 2**: 添加容器类型的内联优化
3. **Phase 3**: 完善调试和性能监控

这个设计为AQL提供了稳固可靠的函数调用基础设施，为后续的AI扩展预留了空间。 