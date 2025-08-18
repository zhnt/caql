# AQL 栈帧与闭包结合设计

## 1. 概述

栈帧与闭包的结合是现代编程语言中最复杂但也最重要的特性之一。AQL借鉴Lua的成熟设计，同时针对AI原生特性进行了优化，实现了高效的闭包捕获、传递和执行机制。

### 1.1 核心挑战
- **变量生命周期管理**: 栈变量超出作用域后仍需被闭包访问
- **性能优化**: 避免不必要的变量捕获和内存分配
- **内存安全**: 防止悬垂指针和内存泄漏
- **AI工作流支持**: 闭包在AI任务中的特殊需求

### 1.2 设计目标
- **零拷贝优化**: 尽可能避免变量拷贝
- **延迟关闭**: 只在必要时才关闭upvalue
- **智能捕获**: 编译器分析优化变量捕获
- **AI增强**: 支持AI上下文的闭包传递

## 2. 基础概念

### 2.1 Upvalue的设计

```c
// Upvalue: 闭包中捕获的外部变量
typedef struct UpVal {
  CommonHeader;              // GC头
  
  union {
    TValue *p;               // 指向栈或自己的value (开放状态)
    ptrdiff_t offset;        // 栈重分配时的偏移量
  } v;
  
  union {
    struct {                 // 开放状态 (指向栈)
      struct UpVal *next;    // 链表下一个
      struct UpVal **previous; // 链表上一个的next指针
    } open;
    TValue value;            // 关闭状态的实际值
  } u;
  
  // AQL扩展: 类型和优化信息
  TypeInfo type_info;        // 变量类型信息
  lu_byte capture_mode;      // 捕获模式 (值/引用/智能)
  lu_byte access_pattern;    // 访问模式 (只读/可写/频繁)
} UpVal;

// Upvalue状态检查
#define upisopen(up)    ((up)->v.p != &(up)->u.value)
#define upisclosed(up)  ((up)->v.p == &(up)->u.value)
#define uplevel(up)     (cast(StkId, (up)->v.p))
```

### 2.2 闭包结构

```c
// AQL闭包 (基于Lua LClosure改进)
typedef struct AQLClosure {
  CommonHeader;              // GC头
  lu_byte nupvalues;         // upvalue数量
  GCObject *gclist;          // GC链表
  
  struct Proto *p;           // 函数原型
  UpVal *upvals[1];          // upvalue数组
  
  // AQL扩展: AI和优化相关
  AIContext *ai_context;     // AI上下文 (如果是AI闭包)
  CaptureOptimization *opt;  // 捕获优化信息
  struct {
    lu_byte is_ai_closure : 1;    // 是否为AI闭包
    lu_byte has_container : 1;    // 是否捕获容器类型
    lu_byte needs_simd_align : 1; // 是否需要SIMD对齐
    lu_byte is_pure : 1;          // 是否为纯函数
  } flags;
} AQLClosure;

// C闭包 (用于AI服务集成)
typedef struct AQLCClosure {
  CommonHeader;
  lu_byte nupvalues;
  GCObject *gclist;
  
  aql_CFunction f;           // C函数指针
  TValue upvalue[1];         // upvalue值数组
  
  // AQL扩展: AI服务集成
  AIServiceBinding *service; // AI服务绑定
  QualityCallback *quality_cb; // 质量回调
} AQLCClosure;
```

## 3. 栈帧与闭包的生命周期

### 3.1 变量捕获过程

```c
// 栈帧中的变量捕获
/*
函数调用栈帧:
┌─────────────────┐ ← ci->top
│ 临时变量区      │
├─────────────────┤
│ local3 (int)    │ ← 被内层函数捕获
├─────────────────┤
│ local2 (slice)  │ ← 被内层函数捕获
├─────────────────┤
│ local1 (string) │
├─────────────────┤ ← ci->func + 1
│ 参数区域        │
└─────────────────┘ ← ci->func

当创建闭包时:
1. 编译器分析哪些局部变量被内层函数引用
2. 为这些变量创建UpVal对象
3. UpVal初始状态为"开放"，指向栈中的实际位置
4. 当栈帧销毁时，UpVal转为"关闭"状态，拷贝值到自身
*/

// 查找或创建upvalue
UpVal *aql_findupval(aql_State *A, StkId level) {
  UpVal **pp = &A->openupval;
  UpVal *p;
  
  // 在已有的开放upvalue链表中查找
  while (*pp != NULL && uplevel(*pp) >= level) {
    p = *pp;
    if (uplevel(p) == level) {  // 找到了
      return p;
    }
    pp = &p->u.open.next;
  }
  
  // 创建新的upvalue
  p = aql_newupval(A);
  p->v.p = s2v(level);        // 指向栈位置
  p->u.open.next = *pp;       // 插入链表
  p->u.open.previous = pp;
  if (*pp) (*pp)->u.open.previous = &p->u.open.next;
  *pp = p;
  
  // AQL扩展: 设置类型和优化信息
  p->type_info = get_stack_var_type(A, level);
  p->capture_mode = analyze_capture_mode(A, level);
  
  return p;
}
```

### 3.2 闭包创建

```c
// 创建AQL闭包
AQLClosure *aql_newclosure(aql_State *A, Proto *p) {
  int nupvalues = p->sizeupvalues;
  AQLClosure *c = aql_malloc(A, sizeof(AQLClosure) + 
                            sizeof(UpVal*) * (nupvalues - 1));
  
  c->nupvalues = cast_byte(nupvalues);
  c->p = p;
  c->ai_context = NULL;
  c->flags.is_ai_closure = 0;
  
  // 初始化所有upvalue为NULL
  for (int i = 0; i < nupvalues; i++) {
    c->upvals[i] = NULL;
  }
  
  return c;
}

// 初始化闭包的upvalue
void aql_initupvals(aql_State *A, AQLClosure *cl) {
  for (int i = 0; i < cl->nupvalues; i++) {
    Proto *p = cl->p;
    
    if (p->upvalues[i].instack) {
      // upvalue在当前栈中
      StkId level = A->ci->func.p + 1 + p->upvalues[i].idx;
      cl->upvals[i] = aql_findupval(A, level);
    } else {
      // upvalue在外层闭包中
      AQLClosure *outer = clAQLvalue(s2v(A->ci->func.p));
      cl->upvals[i] = outer->upvals[p->upvalues[i].idx];
    }
    
    // AQL扩展: 容器类型的特殊处理
    if (is_container_type(&cl->upvals[i]->type_info)) {
      cl->flags.has_container = 1;
      optimize_container_upvalue(A, cl->upvals[i]);
    }
  }
}
```

### 3.3 Upvalue关闭机制

```c
// 关闭upvalue (当栈帧销毁时)
void aql_closeupval(aql_State *A, StkId level) {
  UpVal *uv;
  
  // 关闭所有 >= level 的开放upvalue
  while (A->openupval != NULL && uplevel(A->openupval) >= level) {
    uv = A->openupval;
    
    // 从开放链表中移除
    A->openupval = uv->u.open.next;
    if (A->openupval) A->openupval->u.open.previous = &A->openupval;
    
    // AQL扩展: 容器类型的智能关闭
    if (is_container_type(&uv->type_info)) {
      close_container_upvalue(A, uv);
    } else {
      // 标准关闭: 拷贝值到upvalue内部
      setobj(A, &uv->u.value, uv->v.p);
    }
    
    uv->v.p = &uv->u.value;  // 指向自己的value
  }
}

// 容器类型的智能关闭
static void close_container_upvalue(aql_State *A, UpVal *uv) {
  TValue *val = uv->v.p;
  
  switch (ttype(val)) {
    case AQL_TARRAY: {
      ArrayType *arr = arrayvalue(val);
      if (arr->length <= SMALL_ARRAY_THRESHOLD) {
        // 小数组直接拷贝
        setobj(A, &uv->u.value, val);
      } else {
        // 大数组使用引用计数
        increment_array_refcount(arr);
        setobj(A, &uv->u.value, val);
      }
      break;
    }
    
    case AQL_TSLICE: {
      SliceType *slice = slicevalue(val);
      // slice总是引用类型，增加引用计数
      increment_slice_refcount(slice);
      setobj(A, &uv->u.value, val);
      break;
    }
    
    case AQL_TDICT: {
      DictType *dict = dictvalue(val);
      // 字典使用写时拷贝机制
      if (dict->refcount == 1) {
        setobj(A, &uv->u.value, val);  // 直接转移所有权
      } else {
        dict = copy_dict_for_closure(A, dict);  // 拷贝字典
        setdictvalue(A, &uv->u.value, dict);
      }
      break;
    }
    
    case AQL_TVECTOR: {
      VectorType *vec = vectorvalue(val);
      // 向量数据需要保持SIMD对齐
      VectorType *new_vec = create_aligned_vector_copy(A, vec);
      setvectorvalue(A, &uv->u.value, new_vec);
      break;
    }
    
    default:
      setobj(A, &uv->u.value, val);  // 普通类型直接拷贝
  }
}
```

## 4. AI闭包的特殊处理

### 4.1 AI上下文捕获

```c
// AI闭包的特殊结构
typedef struct AIClosure {
  AQLClosure base;           // 基础闭包
  
  // AI特有字段
  AIIntent *intent;          // 捕获的意图
  QualityThreshold threshold; // 质量要求
  IterationContext *iter_ctx; // 迭代上下文
  
  // 捕获的AI相关变量
  Dict *ai_parameters;       // AI参数字典
  Slice *training_data;      // 训练数据
  Model *ai_model;           // AI模型引用
} AIClosure;

// 创建AI闭包
AIClosure *aql_newai_closure(aql_State *A, Proto *p, AIContext *ai_ctx) {
  AIClosure *ac = aql_malloc(A, sizeof(AIClosure));
  
  // 初始化基础闭包
  aql_initclosure(A, &ac->base, p);
  ac->base.flags.is_ai_closure = 1;
  
  // 设置AI上下文
  ac->intent = ai_ctx->intent;
  ac->threshold = ai_ctx->threshold;
  ac->iter_ctx = create_iteration_context(A, ai_ctx);
  
  // 捕获AI相关变量
  capture_ai_variables(A, ac, ai_ctx);
  
  return ac;
}

// 捕获AI相关变量
static void capture_ai_variables(aql_State *A, AIClosure *ac, AIContext *ctx) {
  // 捕获AI参数字典 (写时拷贝)
  if (ctx->parameters) {
    ac->ai_parameters = create_cow_dict_copy(A, ctx->parameters);
  }
  
  // 捕获训练数据 (只读引用)
  if (ctx->training_data) {
    ac->training_data = create_readonly_slice_ref(A, ctx->training_data);
  }
  
  // 捕获AI模型 (引用计数)
  if (ctx->model) {
    ac->ai_model = ctx->model;
    increment_model_refcount(ctx->model);
  }
}
```

### 4.2 AI闭包执行

```c
// AI闭包的特殊执行流程
int aql_call_ai_closure(aql_State *A, AIClosure *ac, int nargs) {
  // 创建AI专用栈帧
  CallInfo *ci = aql_createaiframe(A, AI_TASK_CLOSURE, ac->base.ai_context);
  
  // 设置AI特有的调用信息
  ci->u.ai.task_type = AI_TASK_CLOSURE;
  ci->u.ai.threshold = ac->threshold;
  ci->u.ai.iteration_count = 0;
  
  // 准备AI上下文
  prepare_ai_closure_context(A, ci, ac);
  
  // 执行闭包 (可能需要多次迭代)
  int result;
  do {
    result = aql_execute_closure(A, &ac->base, nargs);
    
    // 质量评估
    QualityScore quality = evaluate_ai_closure_quality(A, ci, result);
    ci->u2.ai_transfer.current_quality = quality;
    
    if (quality >= ac->threshold.min_quality) {
      break;  // 质量满足要求
    }
    
    // 检查迭代限制
    if (++ci->u.ai.iteration_count >= ac->threshold.max_iterations) {
      aql_error(A, "AI closure iteration limit exceeded");
    }
    
    // 调整参数进行下一轮迭代
    adjust_ai_closure_parameters(A, ac, quality);
    
  } while (true);
  
  return result;
}
```

## 5. 性能优化策略

### 5.1 智能捕获优化

```c
// 编译时分析闭包捕获模式
typedef struct CaptureAnalysis {
  bool *var_captured;        // 哪些变量被捕获
  CaptureMode *capture_modes; // 每个变量的捕获模式
  bool has_escaping_closure; // 是否有逃逸闭包
  int optimization_level;    // 优化级别
} CaptureAnalysis;

// 捕获模式
typedef enum {
  CAPTURE_VALUE,             // 按值捕获 (拷贝)
  CAPTURE_REFERENCE,         // 按引用捕获 (共享)
  CAPTURE_COW,               // 写时拷贝
  CAPTURE_READONLY,          // 只读引用
  CAPTURE_SMART              // 智能模式 (运行时决定)
} CaptureMode;

// 分析最优捕获策略
CaptureMode analyze_optimal_capture(Compiler *comp, VarInfo *var, ClosureInfo *closure) {
  // 分析变量的使用模式
  if (var->is_readonly_in_closure) {
    if (is_immutable_type(var->type)) {
      return CAPTURE_VALUE;    // 不可变类型按值捕获
    } else {
      return CAPTURE_READONLY; // 可变类型只读引用
    }
  }
  
  // 分析变量大小
  if (get_type_size(var->type) <= SMALL_VALUE_THRESHOLD) {
    return CAPTURE_VALUE;      // 小对象按值捕获
  }
  
  // 分析容器类型
  if (is_container_type(var->type)) {
    if (var->write_frequency > HIGH_WRITE_THRESHOLD) {
      return CAPTURE_COW;      // 高写频率使用写时拷贝
    } else {
      return CAPTURE_REFERENCE; // 低写频率直接引用
    }
  }
  
  // 默认智能模式
  return CAPTURE_SMART;
}
```

### 5.2 Upvalue池化

```c
// Upvalue对象池 (减少GC压力)
typedef struct UpValuePool {
  UpVal *free_list;          // 空闲upvalue链表
  int pool_size;             // 池大小
  int peak_usage;            // 峰值使用
  MemoryAllocator *alloc;    // 内存分配器
} UpValuePool;

// 快速分配upvalue
UpVal *fast_alloc_upvalue(aql_State *A) {
  UpValuePool *pool = A->upvalue_pool;
  
  if (pool->free_list) {
    UpVal *uv = pool->free_list;
    pool->free_list = uv->u.open.next;
    
    // 快速重置upvalue
    uv->v.p = NULL;
    uv->u.open.next = NULL;
    uv->u.open.previous = NULL;
    uv->capture_mode = CAPTURE_SMART;
    
    return uv;
  }
  
  // 池空，分配新的
  return create_new_upvalue(A);
}

// 归还upvalue到池中
void return_upvalue_to_pool(aql_State *A, UpVal *uv) {
  UpValuePool *pool = A->upvalue_pool;
  
  // 清理容器引用
  if (uv->flags.has_container) {
    cleanup_container_references(A, uv);
  }
  
  // 放回池中
  uv->u.open.next = pool->free_list;
  pool->free_list = uv;
}
```

### 5.3 SIMD优化的向量捕获

```c
// 向量类型的SIMD优化捕获
static void optimize_vector_upvalue_capture(aql_State *A, UpVal *uv) {
  if (uv->type_info.container_type != CONTAINER_VECTOR) return;
  
  VectorType *vec = vectorvalue(uv->v.p);
  
  // 检查SIMD对齐
  if (!is_simd_aligned(vec->data)) {
    // 创建SIMD对齐的拷贝
    VectorType *aligned_vec = create_aligned_vector_copy(A, vec);
    setvectorvalue(A, uv->v.p, aligned_vec);
  }
  
  // 设置优化标志
  uv->flags.is_simd_optimized = 1;
  
  // 为频繁访问的向量启用缓存行预取
  if (uv->access_pattern == ACCESS_FREQUENT) {
    enable_vector_prefetch(vec);
  }
}
```

## 6. 调试和诊断

### 6.1 闭包调试信息

```c
// 闭包调试信息
typedef struct ClosureDebugInfo {
  const char *name;          // 闭包名称
  const char *source_file;   // 定义文件
  int definition_line;       // 定义行号
  
  // 捕获信息
  int num_captures;          // 捕获变量数
  CaptureInfo *captures;     // 捕获详情
  
  // AI特有调试信息
  const char *ai_intent;     // AI意图描述
  QualityHistory *quality_hist; // 质量变化历史
  
  // 性能信息
  uint64_t call_count;       // 调用次数
  uint64_t total_time;       // 总执行时间
  size_t memory_usage;       // 内存使用
} ClosureDebugInfo;

// 捕获变量信息
typedef struct CaptureInfo {
  const char *var_name;      // 变量名
  TypeInfo type;             // 类型信息
  CaptureMode mode;          // 捕获模式
  bool is_mutable;           // 是否可变
  size_t memory_cost;        // 内存开销
  uint32_t access_count;     // 访问次数
} CaptureInfo;

// 生成闭包调试报告
void generate_closure_debug_report(aql_State *A, AQLClosure *cl, 
                                  DebugReport *report) {
  ClosureDebugInfo *info = &report->closure_info;
  
  info->name = get_closure_name(cl);
  info->source_file = cl->p->source;
  info->definition_line = cl->p->linedefined;
  
  info->num_captures = cl->nupvalues;
  info->captures = aql_malloc(A, sizeof(CaptureInfo) * cl->nupvalues);
  
  for (int i = 0; i < cl->nupvalues; i++) {
    UpVal *uv = cl->upvals[i];
    CaptureInfo *cap = &info->captures[i];
    
    cap->var_name = get_upvalue_name(cl->p, i);
    cap->type = uv->type_info;
    cap->mode = uv->capture_mode;
    cap->is_mutable = !uv->flags.is_readonly;
    cap->memory_cost = calculate_upvalue_memory_cost(uv);
    cap->access_count = uv->stats.access_count;
  }
  
  // AI特有信息
  if (cl->flags.is_ai_closure) {
    AIClosure *ac = (AIClosure*)cl;
    info->ai_intent = ac->intent ? ac->intent->description : NULL;
    info->quality_hist = ac->iter_ctx ? &ac->iter_ctx->quality_history : NULL;
  }
}
```

### 6.2 内存泄漏检测

```c
// 闭包内存泄漏检测
typedef struct ClosureLeakDetector {
  Dict *active_closures;     // 活跃闭包集合
  Dict *upvalue_refs;        // upvalue引用计数
  CircularBuffer *leak_log;  // 泄漏日志
} ClosureLeakDetector;

// 检测闭包内存泄漏
void detect_closure_memory_leaks(aql_State *A) {
  ClosureLeakDetector *detector = A->leak_detector;
  
  // 扫描所有活跃闭包
  for (GCObject *o = A->allgc; o; o = o->next) {
    if (o->tt == AQL_TLCLOSURE) {
      AQLClosure *cl = gco2cl(o);
      
      // 检查upvalue引用循环
      if (detect_upvalue_cycles(cl)) {
        log_potential_leak(detector, cl, "Upvalue reference cycle");
      }
      
      // 检查过期的AI上下文
      if (cl->flags.is_ai_closure) {
        AIClosure *ac = (AIClosure*)cl;
        if (is_ai_context_stale(ac->base.ai_context)) {
          log_potential_leak(detector, cl, "Stale AI context");
        }
      }
      
      // 检查长时间未使用的大对象捕获
      check_large_object_captures(detector, cl);
    }
  }
}
```

## 7. 总结

AQL的栈帧与闭包结合设计实现了以下关键目标：

### 7.1 核心优势
- **高效捕获**: 智能分析确定最优捕获策略
- **内存安全**: 自动管理变量生命周期，防止悬垂指针
- **AI原生**: 专门支持AI上下文和迭代优化
- **性能优化**: SIMD对齐、写时拷贝、对象池等优化

### 7.2 关键创新
- **容器感知**: 针对AQL容器类型的专门优化
- **质量驱动**: AI闭包的质量驱动执行机制
- **智能捕获**: 编译时分析 + 运行时优化
- **调试友好**: 丰富的调试和诊断能力

### 7.3 实施策略
1. **Phase 1**: 实现基础闭包和upvalue机制
2. **Phase 2**: 添加容器类型优化和AI闭包支持
3. **Phase 3**: 完善性能优化和调试工具

这个设计为AQL提供了强大而高效的闭包支持，特别适合AI时代的函数式编程和异步工作流需求。 