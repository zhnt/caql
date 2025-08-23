# AQL 统一错误系统设计文档

## 1. 设计原则

### 1.1 核心原则
- **单一真相源**: 所有子系统的错误通过统一机制报告和处理
- **上下文完整**: 错误信息包含完整的技术和调试上下文
- **可扩展性**: 支持新错误类型和自定义错误处理器
- **性能友好**: 错误处理对正常执行路径影响最小

### 1.2 错误层级
```
用户代码
    ↓
API层 (aapi) ← 统一错误入口
    ↓
子系统层 (变量/类型/内存/编译) 
    ↓
基础设施层 (内存/GC/系统)
```

## 2. 错误类型系统

### 2.1 错误码枚举
```c
typedef enum AqlErrorCode {
    // 通用错误 (0-99)
    AQL_ERROR_NONE = 0,
    AQL_ERROR_MEMORY,
    AQL_ERROR_INTERNAL,
    AQL_ERROR_SYSTEM,
    
    // 词法分析 (100-199)
    AQL_ERROR_LEX_UNEXPECTED_CHAR = 100,
    AQL_ERROR_LEX_UNTERMINATED_STRING,
    AQL_ERROR_LEX_INVALID_NUMBER,
    
    // 语法分析 (200-299)
    AQL_ERROR_PARSE_UNEXPECTED_TOKEN = 200,
    AQL_ERROR_PARSE_UNCLOSED_PAREN,
    AQL_ERROR_PARSE_INVALID_EXPRESSION,
    
    // 变量系统 (300-399)
    AQL_ERROR_VAR_UNDEFINED = 300,
    AQL_ERROR_VAR_REDEFINED,
    AQL_ERROR_VAR_TYPE_MISMATCH,
    AQL_ERROR_VAR_CONST_ASSIGNMENT,
    AQL_ERROR_VAR_SCOPE_ACCESS,
    
    // 类型系统 (400-499)
    AQL_ERROR_TYPE_INCOMPATIBLE = 400,
    AQL_ERROR_TYPE_UNKNOWN,
    AQL_ERROR_TYPE_CAST_FAILED,
    
    // 运行时 (500-599)
    AQL_ERROR_RUNTIME_DIVISION_BY_ZERO = 500,
    AQL_ERROR_RUNTIME_INDEX_OUT_OF_BOUNDS,
    AQL_ERROR_RUNTIME_NULL_POINTER,
    
    // 内存管理 (600-699)
    AQL_ERROR_GC_INVALID_OBJECT = 600,
    AQL_ERROR_GC_MEMORY_EXHAUSTED,
    
    // AI服务 (700-799)
    AQL_ERROR_AI_SERVICE_UNAVAILABLE = 700,
    AQL_ERROR_AI_INVALID_RESPONSE
} AqlErrorCode;
```

### 2.2 错误严重程度
```c
typedef enum AqlErrorLevel {
    AQL_LEVEL_DEBUG,
    AQL_LEVEL_INFO,
    AQL_LEVEL_WARNING,
    AQL_LEVEL_ERROR,
    AQL_LEVEL_FATAL
} AqlErrorLevel;
```

## 3. 统一错误结构

### 3.1 核心错误结构
```c
typedef struct AqlError {
    AqlErrorCode code;
    AqlErrorLevel level;
    const char* message;
    
    // 引用计数 - 内存安全管理
    _Atomic int ref_count;
    
    // 位置信息
    struct {
        const char* filename;
        uint32_t line;
        uint32_t column;
        uint32_t offset;
    } location;
    
    // 上下文信息
    struct {
        const char* function;
        const char* variable;
        const char* scope;
        const char* module;
    } context;
    
    // 堆栈跟踪
    struct {
        const char** frames;
        size_t count;
        size_t capacity;
    } stack_trace;
    
    // 错误链 - 支持引用计数
    struct AqlError* cause;
    
    // 扩展数据
    void* user_data;
    void (*free_user_data)(void*);
    
    // 创建时间戳 - 用于频率限制
    uint64_t timestamp;
    
    // 内存管理标志
    uint8_t flags;
} AqlError;
```

### 3.2 轻量级错误句柄
```c
typedef struct AqlErrorHandle {
    AqlErrorCode code;
    uint32_t location_hash;  // filename+line的哈希
    const char* message;
} AqlErrorHandle;
```

## 4. 错误报告器

### 4.1 全局错误管理器
```c
typedef struct AqlErrorReporter {
    // 处理器链表 - 线程安全保护
    struct ErrorHandler {
        void (*handler)(AqlError* error, void* user_data);
        void* user_data;
        struct ErrorHandler* next;
    } *handlers;
    
    // 默认处理器
    void (*default_handler)(AqlError* error);
    
    // 统计信息 - 原子操作保护
    struct {
        _Atomic size_t total_errors;
        _Atomic size_t errors_by_code[1024];
        _Atomic size_t errors_by_level[5];
        _Atomic uint64_t last_error_time;
    } stats;
    
    // 配置
    struct {
        bool enable_stack_trace;
        bool enable_context_info;
        AqlErrorLevel report_level;
        
        // 频率限制配置
        uint32_t rate_limit_window;     // 时间窗口(ms)
        uint32_t rate_limit_threshold;  // 阈值
        bool enable_rate_limiting;      // 启用频率限制
    } config;
    
    // 线程安全保护
    pthread_rwlock_t handlers_lock;     // 处理器链表读写锁
    pthread_mutex_t rate_limit_mutex;   // 频率限制互斥锁
    
    // 频率限制状态
    struct {
        uint64_t window_start;          // 窗口开始时间
        _Atomic uint32_t error_count;   // 当前窗口错误数
        _Atomic bool rate_limited;      // 是否被限制
        _Atomic uint64_t suppressed_count; // 被抑制的错误数
    } rate_limit;
} AqlErrorReporter;

// 全局实例
extern AqlErrorReporter g_aql_error_reporter;
```

### 4.2 引用计数管理API
```c
// 错误对象生命周期管理
AqlError* aql_error_create(AqlErrorCode code, const char* message);
AqlError* aql_error_retain(AqlError* error);
void aql_error_release(AqlError* error);

// 安全的错误链管理
void aql_error_set_cause(AqlError* error, AqlError* cause);
AqlError* aql_error_get_cause(const AqlError* error);

// 错误对象标志
#define AQL_ERROR_FLAG_STATIC    0x01  // 静态分配，不需要释放
#define AQL_ERROR_FLAG_POOLED    0x02  // 来自对象池
#define AQL_ERROR_FLAG_CHAINED   0x04  // 有错误链

// 引用计数实现
static inline AqlError* aql_error_retain(AqlError* error) {
    if (error && !(error->flags & AQL_ERROR_FLAG_STATIC)) {
        atomic_fetch_add(&error->ref_count, 1);
    }
    return error;
}

static inline void aql_error_release(AqlError* error) {
    if (!error || (error->flags & AQL_ERROR_FLAG_STATIC)) {
        return;
    }
    
    if (atomic_fetch_sub(&error->ref_count, 1) == 1) {
        // 引用计数为0，释放资源
        if (error->cause) {
            aql_error_release(error->cause);
        }
        if (error->user_data && error->free_user_data) {
            error->free_user_data(error->user_data);
        }
        if (error->stack_trace.frames) {
            free(error->stack_trace.frames);
        }
        free(error);
    }
}
```

### 4.3 线程安全API
```c
// 线程安全的错误报告
void aql_error_report_safe(AqlErrorCode code, const char* message, ...);
void aql_error_report_at_safe(AqlErrorCode code, const char* filename, 
                             uint32_t line, uint32_t column, const char* message, ...);

// 线程安全的处理器管理
int aql_error_add_handler_safe(void (*handler)(AqlError*, void*), void* user_data);
int aql_error_remove_handler_safe(void (*handler)(AqlError*, void*));
void aql_error_set_default_handler_safe(void (*handler)(AqlError*));

// 线程安全的统计信息获取
void aql_error_get_stats_safe(size_t* total_errors, size_t* errors_by_level);
void aql_error_reset_stats_safe(void);
```

### 4.4 频率限制API
```c
// 频率限制配置
typedef struct AqlRateLimitConfig {
    uint32_t window_ms;         // 时间窗口(毫秒)
    uint32_t threshold;         // 错误阈值
    bool enable;                // 启用频率限制
    bool log_suppressed;        // 记录被抑制的错误
} AqlRateLimitConfig;

// 频率限制管理
void aql_error_set_rate_limit(const AqlRateLimitConfig* config);
void aql_error_get_rate_limit_stats(uint64_t* suppressed_count, bool* is_limited);
void aql_error_reset_rate_limit(void);

// 内部频率检查函数
static bool aql_error_check_rate_limit(AqlErrorReporter* reporter) {
    if (!reporter->config.enable_rate_limiting) {
        return true;  // 未启用频率限制
    }
    
    uint64_t now = get_current_time_ms();
    
    pthread_mutex_lock(&reporter->rate_limit_mutex);
    
    // 检查是否需要重置窗口
    if (now - reporter->rate_limit.window_start >= reporter->config.rate_limit_window) {
        reporter->rate_limit.window_start = now;
        atomic_store(&reporter->rate_limit.error_count, 0);
        atomic_store(&reporter->rate_limit.rate_limited, false);
    }
    
    uint32_t current_count = atomic_load(&reporter->rate_limit.error_count);
    
    if (current_count >= reporter->config.rate_limit_threshold) {
        atomic_store(&reporter->rate_limit.rate_limited, true);
        atomic_fetch_add(&reporter->rate_limit.suppressed_count, 1);
        pthread_mutex_unlock(&reporter->rate_limit_mutex);
        return false;  // 被频率限制
    }
    
    atomic_fetch_add(&reporter->rate_limit.error_count, 1);
    pthread_mutex_unlock(&reporter->rate_limit_mutex);
    return true;  // 允许报告
}
```

### 4.5 核心API (增强版)
```c
// 基础错误报告 - 支持频率限制
void aql_error_report(AqlErrorCode code, const char* message, ...);
void aql_error_report_at(AqlErrorCode code, const char* filename, 
                        uint32_t line, uint32_t column, const char* message, ...);

// 带上下文错误报告 - 线程安全
void aql_error_report_context(AqlErrorCode code, const char* filename,
                            uint32_t line, uint32_t column,
                            const char* context, const char* message, ...);

// 强制报告 - 绕过频率限制
void aql_error_report_force(AqlErrorCode code, const char* message, ...);

// 配置管理 - 线程安全
void aql_error_configure(bool enable_stack_trace, bool enable_context_info,
                        AqlErrorLevel min_level);
void aql_error_configure_rate_limit(uint32_t window_ms, uint32_t threshold, bool enable);
```

## 5. 子系统协同接口

### 5.1 变量系统错误接口
```c
// 变量系统专用错误助手
static inline void var_error_undefined(const char* name, 
                                     const char* filename, uint32_t line) {
    aql_error_report_context(
        AQL_ERROR_VAR_UNDEFINED, 
        filename, line, 0,
        name, "undefined variable: %s", name
    );
}

static inline void var_error_redefined(const char* name, 
                                     const char* filename, uint32_t line,
                                     const char* prev_filename, uint32_t prev_line) {
    aql_error_report_context(
        AQL_ERROR_VAR_REDEFINED,
        filename, line, 0,
        name, "redefinition of variable: %s (previously defined at %s:%d)",
        name, prev_filename, prev_line
    );
}

static inline void var_error_type_mismatch(const char* name,
                                         const char* filename, uint32_t line,
                                         const char* expected, const char* actual) {
    aql_error_report_context(
        AQL_ERROR_VAR_TYPE_MISMATCH,
        filename, line, 0,
        name, "type mismatch for variable %s: expected %s, got %s",
        name, expected, actual
    );
}
```

### 5.2 编译器错误接口
```c
// 编译阶段错误
void compiler_error_lex(const char* filename, uint32_t line, uint32_t column,
                       AqlErrorCode code, const char* token);
void compiler_error_parse(const char* filename, uint32_t line, uint32_t column,
                        AqlErrorCode code, const char* expected, const char* actual);
```

### 5.3 运行时错误接口
```c
// 运行时错误
void runtime_error_arithmetic(const char* filename, uint32_t line,
                            AqlErrorCode code, const char* operation);
void runtime_error_bounds(const char* filename, uint32_t line,
                        uint32_t index, uint32_t size);
```

## 6. 错误格式化与输出

### 6.1 标准格式化器
```c
void aql_error_format(const AqlError* error, char* buffer, size_t buffer_size);
void aql_error_print(const AqlError* error, FILE* stream);
```

### 6.2 JSON格式化器
```c
void aql_error_format_json(const AqlError* error, char* buffer, size_t buffer_size);
```

## 7. 性能优化

### 7.1 错误缓存
```c
typedef struct AqlErrorCache {
    AqlErrorHandle* cache;
    size_t capacity;
    size_t count;
    size_t hits;
    size_t misses;
} AqlErrorCache;
```

### 7.2 惰性堆栈生成
```c
void aql_error_lazy_stack_trace(AqlError* error);
```

## 8. 调试支持

### 8.1 调试模式
```c
#ifdef AQL_DEBUG_ERROR
    #define AQL_ERROR_DEBUG(code, ...) \
        aql_error_report(code, __VA_ARGS__)
#else
    #define AQL_ERROR_DEBUG(code, ...) \
        ((void)0)
#endif
```

### 8.2 断言宏
```c
#define AQL_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            aql_error_report(AQL_ERROR_INTERNAL, "Assertion failed: %s", message); \
            abort(); \
        } \
    } while(0)
```

## 9. 与变量系统的集成示例

### 9.1 变量声明阶段
```c
// 变量声明时检查重复定义
Variable* var_declare(const char* name, const char* filename, uint32_t line) {
    Variable* existing = find_variable(name);
    if (existing != NULL) {
        var_error_redefined(name, filename, line, 
                          existing->filename, existing->line);
        return NULL;
    }
    // ... 创建变量
}
```

### 9.2 变量访问阶段
```c
// 变量访问时检查未定义
Value var_get(const char* name, const char* filename, uint32_t line) {
    Variable* var = find_variable(name);
    if (var == NULL) {
        var_error_undefined(name, filename, line);
        return NIL_VAL;
    }
    return var->value;
}
```

### 9.3 类型检查阶段
```c
// 类型检查时报告类型不匹配
bool var_check_type(Variable* var, const char* expected_type,
                   const char* filename, uint32_t line) {
    if (!type_compatible(var->type, expected_type)) {
        var_error_type_mismatch(var->name, filename, line,
                              expected_type, var->type);
        return false;
    }
    return true;
}
```

## 10. 配置与初始化

### 10.1 配置结构
```c
typedef struct AqlErrorConfig {
    bool enable_colors;
    bool enable_stack_trace;
    bool enable_context;
    AqlErrorLevel min_report_level;
    size_t max_stack_depth;
    const char* log_file;
} AqlErrorConfig;
```

### 10.2 初始化函数
```c
// 错误系统初始化
int aql_error_system_init(const AqlErrorConfig* config);
void aql_error_system_cleanup(void);


```

## 11. 增强实现示例

### 11.1 线程安全的错误报告实现
```c
// 线程安全的错误报告实现
void aql_error_report_safe(AqlErrorCode code, const char* message, ...) 

// 线程安全的处理器添加
int aql_error_add_handler_safe(void (*handler)(AqlError*, void*), void* user_data) 
```

### 11.2 错误链管理实现
```c
// 安全的错误链设置
void aql_error_set_cause(AqlError* error, AqlError* cause) 

// 获取错误链
AqlError* aql_error_get_cause(const AqlError* error) {
    return error ? error->cause : NULL;
}
```

### 11.3 频率限制增强实现
```c
// 获取当前时间戳 (毫秒)
static uint64_t get_current_time_ms(void) 

// 频率限制配置
void aql_error_configure_rate_limit(uint32_t window_ms, uint32_t threshold, bool enable) 

// 获取频率限制统计
void aql_error_get_rate_limit_stats(uint64_t* suppressed_count, bool* is_limited) 
```

这个设计提供了：
1. **统一的错误处理机制** - 所有子系统共享
2. **丰富的上下文信息** - 便于调试
3. **可扩展的处理器** - 支持自定义错误处理
4. **性能优化** - 缓存和惰性生成
5. **清晰的变量系统接口** - 简化变量错误的报告和处理