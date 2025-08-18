# AQL 设计文档

## 1. 虚拟机架构设计

### 1.1 整体架构
```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   源代码解析    │ -> │   字节码生成    │ -> │   虚拟机执行    │
│   Parser        │    │   CodeGen       │    │   VM Engine     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                │                       │
                                ▼                       ▼
                       ┌─────────────────┐    ┌─────────────────┐
                       │   JIT编译器     │    │   内存管理器    │
                       │   JIT Compiler  │    │   Memory Mgmt   │
                       └─────────────────┘    └─────────────────┘
                                │                       │
                                ▼                       ▼
                       ┌─────────────────┐    ┌─────────────────┐
                       │   LLVM编译器    │    │   垃圾回收器    │
                       │   LLVM Compiler │    │   GC System     │
                       └─────────────────┘    └─────────────────┘
```

### 1.2 寄存器虚拟机设计
```c
// 虚拟机状态
typedef struct {
    Value* registers;      // 寄存器数组
    Value* stack;          // 操作数栈
    int sp;               // 栈指针
    int pc;               // 程序计数器
    CallFrame* frames;    // 调用栈
    int frame_count;      // 当前帧数
    Upvalue* upvalues;    // 上值数组
    Table* globals;       // 全局变量表
} VM;

// 值类型
typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_FUNCTION,
    VAL_CLOSURE,
    VAL_COROUTINE,
    VAL_PROMISE,
    VAL_ARRAY,
    VAL_DICT,
    VAL_SLICE,
    VAL_STRUCT,
    VAL_CLASS,
    VAL_INTERFACE
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        int64_t integer;
        double floating;
        String* string;
        Function* function;
        Closure* closure;
        Coroutine* coroutine;
        Promise* promise;
        Array* array;
        Dict* dict;
        Slice* slice;
        Struct* struct_val;
        Class* class_val;
        Interface* interface_val;
    } as;
} Value;
```

## 2. 指令集设计

### 2.1 基础指令集
```c
typedef enum {
    // 常量加载指令
    OP_CONSTANT,      // 加载常量到寄存器
    OP_NIL,           // 加载nil
    OP_TRUE,          // 加载true
    OP_FALSE,         // 加载false
    OP_INT,           // 加载整数
    OP_FLOAT,         // 加载浮点数
    
    // 变量访问指令
    OP_GET_LOCAL,     // 获取局部变量
    OP_SET_LOCAL,     // 设置局部变量
    OP_GET_GLOBAL,    // 获取全局变量
    OP_SET_GLOBAL,    // 设置全局变量
    OP_GET_UPVALUE,   // 获取上值
    OP_SET_UPVALUE,   // 设置上值
    
    // 算术运算指令
    OP_ADD,           // 加法
    OP_SUBTRACT,      // 减法
    OP_MULTIPLY,      // 乘法
    OP_DIVIDE,        // 除法
    OP_MODULO,        // 取模
    OP_POWER,         // 幂运算
    OP_NEGATE,        // 负号
    OP_NOT,           // 逻辑非
    
    // 比较运算指令
    OP_EQUAL,         // 等于
    OP_LESS,          // 小于
    OP_LESS_EQUAL,    // 小于等于
    OP_GREATER,       // 大于
    OP_GREATER_EQUAL, // 大于等于
    
    // 控制流指令
    OP_JUMP,          // 无条件跳转
    OP_JUMP_IF_FALSE, // 条件跳转
    OP_LOOP,          // 循环跳转
    OP_CALL,          // 函数调用
    OP_RETURN,        // 返回
    OP_CLOSURE,       // 创建闭包
    OP_CLOSE_UPVALUE, // 关闭上值
    
    // 表操作指令
    OP_GET_PROPERTY,  // 获取属性
    OP_SET_PROPERTY,  // 设置属性
    OP_GET_INDEX,     // 获取索引
    OP_SET_INDEX,     // 设置索引
    
    // 类型操作指令
    OP_TYPE_CHECK,    // 类型检查
    OP_TYPE_CAST,     // 类型转换
    OP_INSTANCE_OF,   // 实例检查
    
    // 异常处理指令
    OP_TRY,           // 开始try块
    OP_CATCH,         // 开始catch块
    OP_THROW,         // 抛出异常
    
    // 协程指令
    OP_YIELD,         // 协程让出
    OP_RESUME,        // 协程恢复
    
    // 异步指令
    OP_AWAIT,         // 等待异步结果
    OP_PROMISE_NEW,   // 创建Promise
    OP_PROMISE_RESOLVE, // 解析Promise
    OP_PROMISE_REJECT,  // 拒绝Promise
    
    // 模块指令
    OP_IMPORT,        // 导入模块
    OP_EXPORT,        // 导出模块
    
    // 注解指令
    OP_ANNOTATION,    // 处理注解
    OP_DEFER,         // 延迟执行
    
    // AI服务指令
    OP_AI_CALL,       // AI服务调用
    OP_PIPELINE,      // 管道操作
    
    // 函数式编程指令
    OP_LAMBDA,        // 创建lambda
    OP_COMPOSE,       // 函数组合
    OP_CURRY,         // 柯里化
    
    // 内存管理指令
    OP_ALLOC,         // 分配内存
    OP_FREE,          // 释放内存
    OP_GC_MARK,       // GC标记
    OP_GC_SWEEP,      // GC清除
} OpCode;
```

### 2.2 指令格式设计
```c
// 指令结构
typedef struct {
    OpCode opcode;    // 操作码
    uint8_t operand1; // 操作数1（寄存器或常量索引）
    uint8_t operand2; // 操作数2
    uint8_t operand3; // 操作数3
} Instruction;

// 指令示例
// OP_CONSTANT 0 1    // 将常量1加载到寄存器0
// OP_ADD 0 1 2       // 寄存器1 + 寄存器2 -> 寄存器0
// OP_JUMP_IF_FALSE 0 10  // 如果寄存器0为false，跳转到位置10
```

## 3. 内存管理设计

### 3.1 分层内存分配器
```c
// 内存分配器类型
typedef enum {
    ALLOC_TINY,    // 微小对象 (< 16字节)
    ALLOC_SMALL,   // 小对象 (16-256字节)
    ALLOC_MEDIUM,  // 中等对象 (256-4KB)
    ALLOC_LARGE,   // 大对象 (> 4KB)
} AllocType;

// 内存分配器
typedef struct {
    // 微小对象分配器（位图管理）
    TinyAllocator tiny;
    
    // 小对象分配器（空闲链表）
    SmallAllocator small;
    
    // 中等对象分配器（伙伴系统）
    MediumAllocator medium;
    
    // 大对象分配器（直接分配）
    LargeAllocator large;
    
    // 线程本地缓存
    ThreadCache* thread_cache;
    
    // 统计信息
    AllocStats stats;
} MemoryAllocator;
```

### 3.2 垃圾回收器设计
```c
// GC类型
typedef enum {
    GC_NONE,       // 无GC
    GC_MARK_SWEEP, // 标记清除
    GC_COPYING,    // 复制式GC
    GC_GENERATIONAL // 分代GC
} GCType;

// 分代GC设计
typedef struct {
    // 新生代（Eden + Survivor）
    Space* eden;
    Space* survivor1;
    Space* survivor2;
    
    // 老年代
    Space* tenured;
    
    // 大对象空间
    Space* large_object;
    
    // GC统计
    GCStats stats;
    
    // GC触发条件
    size_t threshold;
    size_t allocated_since_gc;
} GenerationalGC;
```

## 4. 执行引擎设计

### 4.1 解释执行引擎
```c
// 解释器主循环
static InterpretResult run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frame_count - 1];
    
    for (;;) {
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_CONSTANT: {
                Constant constant = READ_CONSTANT();
                push(vm, constant);
                break;
            }
            case OP_ADD: {
                double b = AS_NUMBER(pop(vm));
                double a = AS_NUMBER(pop(vm));
                push(vm, NUMBER_VAL(a + b));
                break;
            }
            // ... 其他指令
        }
    }
}
```

### 4.2 JIT编译器设计
```c
// JIT编译器
typedef struct {
    // 热点检测
    HotSpotDetector* detector;
    
    // 代码生成器
    CodeGenerator* generator;
    
    // 优化器
    Optimizer* optimizer;
    
    // 代码缓存
    CodeCache* cache;
} JITCompiler;

// 热点检测
typedef struct {
    uint32_t* counters;    // 执行计数器
    uint32_t threshold;    // 热点阈值
    HotSpot* hotspots;     // 热点列表
} HotSpotDetector;
```

### 4.3 LLVM编译器设计
```c
// LLVM编译器
typedef struct {
    LLVMContext* context;
    LLVMModule* module;
    LLVMBuilder* builder;
    LLVMPassManager* pass_manager;
    
    // 类型映射
    TypeMap* type_map;
    
    // 函数映射
    FunctionMap* function_map;
} LLVMCompiler;

// 代码生成
static LLVMValueRef generate_function(LLVMCompiler* compiler, Function* function) {
    // 创建LLVM函数
    LLVMTypeRef return_type = map_type(compiler, function->return_type);
    LLVMTypeRef* param_types = map_parameter_types(compiler, function);
    
    LLVMTypeRef function_type = LLVMFunctionType(return_type, param_types, 
                                                function->arity, false);
    LLVMValueRef llvm_function = LLVMAddFunction(compiler->module, 
                                                function->name, function_type);
    
    // 生成函数体
    generate_function_body(compiler, function, llvm_function);
    
    return llvm_function;
}
```

## 5. 类型系统设计

### 5.1 类型表示
```c
// 类型定义
typedef struct {
    TypeKind kind;
    union {
        // 基础类型
        struct {
            BasicType basic_type;
        } basic;
        
        // 复合类型
        struct {
            Type* element_type;
            size_t size;
        } array;
        
        struct {
            Type* key_type;
            Type* value_type;
        } dict;
        
        struct {
            Type* element_type;
        } slice;
        
        struct {
            String* name;
            Field* fields;
            size_t field_count;
        } struct_type;
        
        struct {
            String* name;
            Method* methods;
            size_t method_count;
        } class_type;
        
        struct {
            String* name;
            Method* methods;
            size_t method_count;
        } interface_type;
    } data;
} Type;
```

### 5.2 类型检查器
```c
// 类型检查器
typedef struct {
    Scope* current_scope;
    Type* expected_type;
    ErrorList* errors;
    SymbolTable* symbols;
} TypeChecker;

// 类型检查
static bool check_expression(TypeChecker* checker, Expression* expr) {
    switch (expr->type) {
        case EXPR_BINARY: {
            Binary* binary = (Binary*)expr;
            
            // 检查左操作数
            if (!check_expression(checker, binary->left)) {
                return false;
            }
            
            // 检查右操作数
            if (!check_expression(checker, binary->right)) {
                return false;
            }
            
            // 检查操作符类型兼容性
            Type* left_type = get_expression_type(checker, binary->left);
            Type* right_type = get_expression_type(checker, binary->right);
            
            if (!is_compatible(left_type, right_type, binary->operator)) {
                add_type_error(checker, "Incompatible types for operator");
                return false;
            }
            
            return true;
        }
        // ... 其他表达式类型
    }
}
```

## 6. 模块系统设计

### 6.1 模块加载器
```c
// 模块加载器
typedef struct {
    ModuleCache* cache;
    ModuleResolver* resolver;
    ImportStack* import_stack;
} ModuleLoader;

// 模块结构
typedef struct {
    String* name;
    String* path;
    Table* exports;
    Table* globals;
    Function* main_function;
    Module* parent;
    Module** children;
    size_t child_count;
} Module;

// 模块加载
static Module* load_module(ModuleLoader* loader, const char* name) {
    // 检查缓存
    Module* cached = find_module_in_cache(loader->cache, name);
    if (cached != NULL) {
        return cached;
    }
    
    // 解析模块路径
    String* path = resolve_module_path(loader->resolver, name);
    if (path == NULL) {
        return NULL;
    }
    
    // 解析源代码
    Parser* parser = create_parser();
    if (!parse_file(parser, path)) {
        return NULL;
    }
    
    // 创建模块
    Module* module = create_module(name, path);
    
    // 编译模块
    if (!compile_module(module, parser->ast)) {
        destroy_module(module);
        return NULL;
    }
    
    // 添加到缓存
    add_module_to_cache(loader->cache, module);
    
    return module;
}
```

## 7. 异步系统设计

### 7.1 协程调度器
```c
// 协程调度器
typedef struct {
    Coroutine** ready_queue;
    size_t ready_count;
    Coroutine** waiting_queue;
    size_t waiting_count;
    Coroutine* current;
    ThreadPool* thread_pool;
} Scheduler;

// 协程结构
typedef struct {
    CallFrame* frames;
    int frame_count;
    Value* stack;
    int stack_top;
    int stack_capacity;
    CoroutineState state;
    Scheduler* scheduler;
    Promise* waiting_promise;
} Coroutine;

// 协程调度
static void schedule_coroutine(Scheduler* scheduler, Coroutine* coroutine) {
    if (coroutine->state == COROUTINE_READY) {
        // 添加到就绪队列
        scheduler->ready_queue[scheduler->ready_count++] = coroutine;
    } else if (coroutine->state == COROUTINE_WAITING) {
        // 添加到等待队列
        scheduler->waiting_queue[scheduler->waiting_count++] = coroutine;
    }
}

// 协程切换
static void switch_coroutine(Scheduler* scheduler, Coroutine* new_coroutine) {
    Coroutine* old_coroutine = scheduler->current;
    
    // 保存当前协程状态
    if (old_coroutine != NULL) {
        save_coroutine_state(old_coroutine);
    }
    
    // 恢复新协程状态
    scheduler->current = new_coroutine;
    restore_coroutine_state(new_coroutine);
}
```

### 7.2 Promise实现
```c
// Promise状态
typedef enum {
    PROMISE_PENDING,
    PROMISE_FULFILLED,
    PROMISE_REJECTED
} PromiseState;

// Promise结构
typedef struct {
    PromiseState state;
    Value result;
    Value error;
    CallbackList* on_fulfilled;
    CallbackList* on_rejected;
    Coroutine* waiting_coroutine;
} Promise;

// Promise创建
static Promise* create_promise() {
    Promise* promise = ALLOCATE(Promise, 1);
    promise->state = PROMISE_PENDING;
    promise->result = NIL_VAL;
    promise->error = NIL_VAL;
    promise->on_fulfilled = create_callback_list();
    promise->on_rejected = create_callback_list();
    promise->waiting_coroutine = NULL;
    return promise;
}

// Promise解析
static void resolve_promise(Promise* promise, Value value) {
    if (promise->state != PROMISE_PENDING) {
        return; // 已经解析过了
    }
    
    promise->state = PROMISE_FULFILLED;
    promise->result = value;
    
    // 执行回调
    execute_callbacks(promise->on_fulfilled, value);
    
    // 恢复等待的协程
    if (promise->waiting_coroutine != NULL) {
        resume_coroutine(promise->waiting_coroutine, value);
    }
}
```

## 8. 注解系统设计

### 8.1 注解处理器
```c
// 注解处理器
typedef struct {
    AnnotationRegistry* registry;
    AnnotationProcessor* processors;
    size_t processor_count;
} AnnotationSystem;

// 注解结构
typedef struct {
    String* name;
    Value* arguments;
    size_t argument_count;
    SourceLocation location;
} Annotation;

// 注解处理
static void process_annotations(AnnotationSystem* system, 
                               Annotation* annotations, 
                               size_t count,
                               void* target) {
    for (size_t i = 0; i < count; i++) {
        Annotation* annotation = &annotations[i];
        
        // 查找处理器
        AnnotationProcessor* processor = find_processor(system->registry, 
                                                      annotation->name);
        if (processor != NULL) {
            // 执行处理器
            processor->process(processor, annotation, target);
        }
    }
}
```

## 9. AI服务集成设计

### 9.1 AI服务管理器
```c
// AI服务管理器
typedef struct {
    ServiceRegistry* registry;
    ServiceConfig* configs;
    size_t config_count;
    HttpClient* http_client;
} AIServiceManager;

// AI服务调用
static Promise* call_ai_service(AIServiceManager* manager, 
                               const char* service_name,
                               const char* method,
                               Value* arguments) {
    // 查找服务配置
    ServiceConfig* config = find_service_config(manager, service_name);
    if (config == NULL) {
        return create_rejected_promise("Service not found");
    }
    
    // 创建HTTP请求
    HttpRequest* request = create_http_request();
    request->url = config->endpoint;
    request->method = "POST";
    request->headers = config->headers;
    request->body = serialize_arguments(arguments);
    
    // 发送请求
    return http_client_send(manager->http_client, request);
}
```

这个设计文档提供了AQL的具体实现方案，包括虚拟机架构、指令集设计、内存管理、执行引擎等核心组件的详细设计。 