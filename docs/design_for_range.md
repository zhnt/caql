# AQL For Range 设计文档

## 1. 语法设计

### 基本语法
```aql
for variable in range(args...) {
    // 循环体
}
```

### Range函数签名
```aql
range(stop)                    // [0, stop)
range(start, stop)             // [start, stop)  
range(start, stop, step)       // [start, stop) with step
```

## 2. 语义定义

### 参数规则
- `stop`: 结束值（不包含）
- `start`: 起始值（默认为0）
- `step`: 步长（默认为1或-1，根据start和stop自动推断）

### 智能步长推断
```aql
range(1, 5)      // [1, 2, 3, 4] (自动推断step=1)
range(5, 1)      // [5, 4, 3, 2] (自动推断step=-1)
range(5, 1, -1)  // [5, 4, 3, 2] (显式负步长)
range(1, 5, 2)   // [1, 3] (显式正步长)
```

### 步长推断规则
- 当`start < stop`且未指定step时，自动推断`step = 1`
- 当`start > stop`且未指定step时，自动推断`step = -1`  
- 当`start == stop`时，返回空序列
- 显式指定step时，使用指定值

## 3. 实现示例

### 测试用例
```aql
// 测试1：基本用法
for i in range(5) {
    print(i)  // 输出: 0, 1, 2, 3, 4
}

// 测试2：指定起始值（正向）
for i in range(2, 7) {
    print(i)  // 输出: 2, 3, 4, 5, 6
}

// 测试3：自动推断负步长
for i in range(10, 1) {
    print(i)  // 输出: 10, 9, 8, 7, 6, 5, 4, 3, 2
}

// 测试4：显式负步长
for i in range(10, 0, -2) {
    print(i)  // 输出: 10, 8, 6, 4, 2
}

// 测试5：变量参数
let start = 5
let stop = 1
for i in range(start, stop) {
    print(i)  // 输出: 5, 4, 3, 2 (自动推断step=-1)
}

// 测试6：空范围
for i in range(5, 5) {
    print(i)  // 不输出（start == stop）
}
```

## 4. 与现有for循环的对比

| 特性 | 传统for循环 | for range |
|------|-------------|-----------|
| 语法 | `for i = 1, 5, 1` | `for i in range(1, 6)` |
| 区间 | 闭区间 [1, 5] | 左闭右开 [1, 6) |
| 步长推断 | 总是默认1 | 智能推断 |
| 可读性 | 中等 | 高 |

## 5. 实现架构

### 5.1 Range对象设计
```c
typedef struct RangeObject {
    CommonHeader;  // AQL对象通用头部
    aql_Integer start;
    aql_Integer stop; 
    aql_Integer step;
    aql_Integer current;
    aql_Integer count;     // 预计算的迭代次数
    int finished;
} RangeObject;
```

### 5.2 智能步长推断算法
```c
static aql_Integer infer_step(aql_Integer start, aql_Integer stop) {
    if (start < stop) return 1;   // 正向
    if (start > stop) return -1;  // 反向
    return 1;  // start == stop，返回1（但count=0）
}
```

### 5.3 预计算优化
```c
static aql_Integer calculate_count(aql_Integer start, aql_Integer stop, aql_Integer step) {
    if (step == 0) return 0;  // 错误情况
    if (start == stop) return 0;
    
    if (step > 0) {
        return (stop > start) ? (stop - start + step - 1) / step : 0;
    } else {
        return (start > stop) ? (start - stop - step - 1) / (-step) : 0;
    }
}
```

### 5.4 Range内置函数实现
```c
// range(stop)
static int aql_range1(aql_State *L) {
    aql_Integer stop = aqlL_checkinteger(L, 1);
    return create_range(L, 0, stop, 1);
}

// range(start, stop)  
static int aql_range2(aql_State *L) {
    aql_Integer start = aqlL_checkinteger(L, 1);
    aql_Integer stop = aqlL_checkinteger(L, 2);
    aql_Integer step = infer_step(start, stop);
    return create_range(L, start, stop, step);
}

// range(start, stop, step)
static int aql_range3(aql_State *L) {
    aql_Integer start = aqlL_checkinteger(L, 1);
    aql_Integer stop = aqlL_checkinteger(L, 2);
    aql_Integer step = aqlL_checkinteger(L, 3);
    if (step == 0) aqlL_error(L, "range() step argument must not be zero");
    return create_range(L, start, stop, step);
}
```

### 5.5 迭代器协议
复用现有的泛型for循环基础设施：
```c
// Range对象的__iter方法
static int range_iter(aql_State *L) {
    // 返回自身作为迭代器
    aql_pushvalue(L, 1);
    return 1;
}

// Range对象的__next方法  
static int range_next(aql_State *L) {
    RangeObject *range = (RangeObject*)aql_touserdata(L, 1);
    
    if (range->finished || range->count <= 0) {
        aql_pushnil(L);
        return 1;
    }
    
    aql_pushinteger(L, range->current);
    range->current += range->step;
    range->count--;
    
    if (range->count <= 0) {
        range->finished = 1;
    }
    
    return 1;
}
```

## 6. 性能优化

### 6.1 预计算优势
- **避免运行时判断**：预先计算迭代次数，避免每次循环都判断结束条件
- **内存预分配**：如果需要，可以根据count预分配内存
- **编译器优化**：编译器可以基于已知的迭代次数进行循环优化

### 6.2 与传统for循环的性能对比
```c
// 传统for循环：每次都要判断条件
for (i = start; condition_check(i, stop, step); i += step) {
    // 循环体
}

// Range优化：预计算次数
for (count = precalculated_count; count > 0; count--) {
    // 循环体，i直接递增
}
```

## 7. 实现步骤

### 7.1 第一阶段：基础Range对象
1. 在`aobject.h`中定义RangeObject结构
2. 实现Range对象的创建和销毁
3. 添加基本的迭代器方法

### 7.2 第二阶段：内置函数
1. 实现`range()`的三个重载版本
2. 注册到全局函数表
3. 添加错误处理和参数验证

### 7.3 第三阶段：泛型for循环集成
1. 确保Range对象可以被`for in`语法识别
2. 测试迭代器协议的正确性
3. 性能测试和优化

### 7.4 第四阶段：测试和文档
1. 编写完整的测试用例
2. 性能基准测试
3. 用户文档和示例
