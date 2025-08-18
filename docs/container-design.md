# AQL 容器类型设计

## 1. 概述

AQL的容器类型设计借鉴了Go、TypeScript和Python的优秀特性，同时避免了它们的设计缺陷。通过明确的类型分离和性能优化，为AI应用提供高效、类型安全的数据容器。

## 2. 主流语言容器分析

### 2.1 Go语言设计分析

#### 2.1.1 Array vs Slice的清晰分离
```go
// Array: 固定大小值类型
var arr [5]int = [5]int{1, 2, 3, 4, 5}
// - 长度是类型的一部分: [3]int != [5]int
// - 值传递，栈分配
// - 编译时大小检查
// - 零运行时开销

// Slice: 动态引用类型  
var slice []int = []int{1, 2, 3}
// - 运行时动态扩容
// - 引用传递，堆分配底层数组
// - 三元组结构: {ptr, len, cap}
// - 高效的append操作
```

#### 2.1.2 Go的性能特征
```go
// 内存布局优势
type slice struct {
    ptr    *T      // 8字节: 指向数据的指针
    len    int     // 8字节: 当前长度
    cap    int     // 8字节: 容量
}
// 总计24字节的slice头，无论数据大小

// 扩容策略
// cap < 1024: newcap = oldcap * 2
// cap >= 1024: newcap = oldcap * 1.25
```

#### 2.1.3 Go的优势和局限
```go
// 优势:
// + 类型安全，编译时检查
// + 高性能，接近C数组
// + 内存高效，紧凑布局
// + 语义明确，无歧义

// 局限:
// - 泛型支持较晚 (Go 1.18+)
// - 切片操作容易造成内存泄漏
// - 缺乏高级容器 (set, queue等)
```

### 2.2 TypeScript/JavaScript设计分析

#### 2.2.1 统一但复杂的Array设计
```typescript
// JavaScript只有一种"数组"，但内部实现复杂
let arr1: number[] = [1, 2, 3];           // 密集数组
let arr2: any[] = [1, "hello", {}];       // 混合类型数组
let arr3: number[] = [];
arr3[1000] = 42;                          // 稀疏数组

// TypeScript添加类型，但运行时仍是JS
let tuple: [string, number] = ["hello", 42]; // 编译时元组
// 运行时仍然是普通数组，无类型保证
```

#### 2.2.2 V8引擎的内部优化
```javascript
// V8的多种数组表示
// 1. PACKED_SMI_ELEMENTS: 小整数数组
let smi = [1, 2, 3];

// 2. PACKED_DOUBLE_ELEMENTS: 浮点数组  
let doubles = [1.5, 2.7, 3.14];

// 3. PACKED_ELEMENTS: 对象数组
let objects = [{}, [], "string"];

// 4. HOLEY_*: 稀疏数组 (性能较差)
let sparse = [1, , , 4];  // 索引1,2为空洞

// 类型化数组 (真正的连续内存)
let buffer = new ArrayBuffer(16);
let int32Array = new Int32Array(buffer);   // 真正高效
```

#### 2.2.3 TypeScript的优势和问题
```typescript
// 优势:
// + 渐进式类型系统
// + 丰富的数组方法
// + 强大的类型推断
// + 生态系统庞大

// 问题:
// - 运行时类型擦除
// - 性能不可预测
// - 数组/元组语义混淆
// - 内存使用较高
```

### 2.3 Python设计分析

#### 2.3.1 List的简单但低效设计
```python
# Python list: 动态对象指针数组
import sys

# 内部结构 (CPython)
# typedef struct {
#     PyObject_VAR_HEAD
#     PyObject **ob_item;      // 指向PyObject*数组
#     Py_ssize_t allocated;    // 分配容量
# } PyListObject;

lst = [1, 2, 3]
# 每个元素都是PyObject*，占用大量内存
print(sys.getsizeof(lst))       # list对象: ~72字节
print(sys.getsizeof(lst[0]))    # 单个int: ~28字节
```

#### 2.3.2 Array模块和NumPy
```python
import array
import numpy as np

# array模块: 类型化但功能有限
int_array = array.array('i', [1, 2, 3, 4])
print(sys.getsizeof(int_array))  # ~64字节 + 16字节数据

# NumPy: 科学计算的事实标准
np_array = np.array([1, 2, 3, 4], dtype=np.int32)
print(np_array.nbytes)           # 16字节数据

# 切片语义不同
lst_slice = lst[1:3]             # 拷贝
np_slice = np_array[1:3]         # 视图(共享内存)
```

#### 2.3.3 Python的优势和问题
```python
# 优势:
# + 极简语法，易学易用
# + 强大的切片语法
# + 丰富的内置方法
# + NumPy生态强大

# 问题:
# - 内存效率极低
# - 性能较差 (5-10x慢)
# - 类型安全缺失
# - GIL限制并发
```

## 3. AQL容器设计

### 3.1 设计原则

#### 3.1.1 类型明确性
```aql
// 借鉴Go的明确分离，避免TypeScript的歧义
array<int, 5>    // 固定数组，编译时确定
slice<int>       // 动态切片，运行时管理
dict<string, int> // 哈希表
```

#### 3.1.2 性能优先
```aql
// 零成本抽象: 编译时优化
array<int, 1000> arr;        // 栈分配，无堆开销
slice<int> sl = arr[10:20];  // O(1)切片创建，共享内存
```

#### 3.1.3 内存安全
```aql
// 自动边界检查，防止越界访问
let arr: array<int, 5> = [1, 2, 3, 4, 5];
let val = arr[10];  // 编译时错误 (如果索引是常量)
                    // 运行时检查 (如果索引是变量)
```

### 3.2 核心容器类型

#### 3.2.1 固定数组 array<T, N>
```aql
// 语法设计
array<int, 5> arr1 = [1, 2, 3, 4, 5];    // 完整初始化
array<float, 10> arr2;                    // 零值初始化
array<string, 3> arr3 = ["a", "b", "c"]; // 类型推断

// 内存布局 (栈分配)
// [elem0][elem1][elem2][elem3][elem4]
// 连续内存，无额外开销

// 编译器优化
arr1[2] = 10;  // 编译为直接内存写入，无边界检查
let idx = get_index();
arr1[idx] = 10; // 运行时边界检查

// 性能特征
// - 访问时间: O(1)，接近C数组
// - 内存开销: 0 (纯数据)
// - 缓存友好: 连续内存布局
```

#### 3.2.2 动态切片 slice<T>
```aql
// 语法设计
slice<int> sl1 = [1, 2, 3];              // 字面量创建
slice<int> sl2 = make_slice<int>(10);     // 预分配容量
slice<int> sl3 = make_slice<int>(5, 10);  // 长度5，容量10

// 内部结构 (类似Go)
struct Slice<T> {
    ptr: *T,     // 指向数据的指针
    len: usize,  // 当前长度
    cap: usize,  // 容量
}

// 操作示例
sl1.append(4, 5, 6);                     // 动态扩容
let sub = sl1[1:4];                      // 切片操作，共享内存
sl1.extend(other_slice);                 // 批量添加

// 扩容策略 (优化版Go策略)
// len < 256: newcap = oldcap * 2
// len < 2048: newcap = oldcap * 1.5  
// len >= 2048: newcap = oldcap * 1.25

// 性能特征
// - 访问时间: O(1)
// - 扩容时间: 摊销O(1)
// - 内存开销: 24字节 slice头
// - 切片操作: O(1)，共享底层数组
```

#### 3.2.3 哈希表 dict<K, V>
```aql
// 语法设计
dict<string, int> d1 = {"a": 1, "b": 2}; // 字面量
dict<string, int> d2 = make_dict<string, int>(); // 空字典
dict<string, int> d3 = make_dict<string, int>(100); // 预分配

// 操作示例
d1["c"] = 3;                             // 插入/更新
let val = d1["a"];                       // 访问
let exists = d1.has("key");              // 检查存在
d1.delete("b");                          // 删除

// 内部实现 (Robin Hood哈希)
// - 开放寻址，线性探测
// - 负载因子 < 0.75
// - 自动扩容和缩容

// 性能特征
// - 平均访问: O(1)
// - 最坏访问: O(n) (极少发生)
// - 内存利用率: ~75%
// - 扩容成本: 摊销O(1)
```

### 3.3 高级特性

#### 3.3.1 切片语法
```aql
// 丰富的切片操作 (借鉴Python语法)
let arr: array<int, 10> = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];

let slice1 = arr[1:5];      // [1, 2, 3, 4]
let slice2 = arr[::2];      // [0, 2, 4, 6, 8] (步长2)
let slice3 = arr[-3:];      // [7, 8, 9] (负索引)
let slice4 = arr[::-1];     // [9, 8, 7, 6, 5, 4, 3, 2, 1, 0] (反转)

// 编译器优化
// - 常量索引: 编译时计算，零开销
// - 变量索引: 运行时边界检查
// - 步长为1: 简单指针算术
// - 其他步长: 循环展开优化
```

#### 3.3.2 泛型支持
```aql
// 泛型函数
function map<T, U>(sl: slice<T>, f: function(T) -> U) -> slice<U> {
    let result = make_slice<U>(sl.len);
    for i in 0..sl.len {
        result[i] = f(sl[i]);
    }
    return result;
}

// 泛型结构体
struct Pair<T, U> {
    first: T,
    second: U,
}

// 编译时特化，零运行时成本
let numbers = slice<int>[1, 2, 3];
let strings = numbers.map(|x| x.to_string()); // 自动推断类型
```

#### 3.3.3 向量化支持
```aql
// 向量类型 (SIMD优化)
vector<float, 8> vec1 = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0];
vector<float, 8> vec2 = [2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0];

// 自动向量化操作
let result = vec1 + vec2;    // 编译为AVX指令
let dot = vec1.dot(vec2);    // 向量点积

// 数组的向量化
slice<float> data = [1.0, 2.0, 3.0, ..., 1000.0];
@vectorize
let doubled = data.map(|x| x * 2.0);  // 自动SIMD优化
```

## 4. 性能对比分析

### 4.1 内存效率对比

| 容器类型 | 1M个int32数据 | 开销分析 |
|---------|---------------|----------|
| **C Array** | 4MB | 0字节开销 (baseline) |
| **AQL array<int, 1M>** | 4MB | 0字节开销 (等同C) |
| **AQL slice<int>** | 4MB + 24B | 24字节slice头 |
| **Go []int32** | 4MB + 24B | 24字节slice头 |
| **Rust Vec<i32>** | 4MB + 24B | 24字节Vec头 |
| **JS Int32Array** | 4MB + ~48B | TypedArray开销 |
| **JS Array(优化)** | 8-16MB | 对象指针 + SMI优化 |
| **Python array.array** | 4MB + ~64B | array对象开销 |
| **Python list** | ~32MB | 每个int都是PyObject* |

### 4.2 访问性能对比

| 操作类型 | C Array | AQL | Go | Rust | JS(优化) | Python |
|---------|---------|-----|----|----- |----------|--------|
| **随机访问** | 1.0x | 1.0x | 1.0x | 1.0x | 1.2x | 5.0x |
| **顺序遍历** | 1.0x | 1.0x | 1.0x | 1.0x | 1.1x | 8.0x |
| **边界检查** | N/A | 1.1x | 1.1x | 1.1x | 1.3x | 10.0x |
| **动态扩容** | N/A | 1.2x | 1.2x | 1.1x | 1.5x | 3.0x |

### 4.3 编译优化效果

```aql
// 编译时优化示例
function sum_array(arr: array<int, 1000>) -> int {
    let total = 0;
    for i in 0..1000 {          // 编译时展开循环
        total += arr[i];        // 无边界检查，直接内存访问
    }
    return total;
}

// 编译后的伪代码 (类似C)
int sum_array(int* arr) {
    int total = 0;
    // 循环向量化: 8个元素一组
    for (int i = 0; i < 1000; i += 8) {
        __m256i vec = _mm256_load_si256((__m256i*)(arr + i));
        total += _mm256_reduce_add_epi32(vec);
    }
    return total;
}
```

## 5. 指令集映射

### 5.1 统一容器指令
```c
// AQL使用统一的容器操作指令
NEWOBJECT   type_id, size          // 创建容器
GETPROP     obj, index             // 访问元素  
SETPROP     obj, index, value      // 设置元素
INVOKE      obj, method, args      // 调用方法

// 编译器根据类型优化:
// array<T, N> → 直接内存操作
// slice<T> → 边界检查 + 内存操作
// dict<K, V> → 哈希查找
```

### 5.2 类型特化优化
```c
// 编译时类型特化
// 源码:
let arr: array<int, 5>;
arr[2] = 10;

// 编译为:
MOV [R1 + 8], 10     // 直接内存写入，无开销

// 源码:
let sl: slice<int>;
sl[idx] = 10;

// 编译为:
CMP R2, [R1]         // 边界检查: idx < len
JGE bounds_error
MOV [R1 + 8 + R2*4], 10  // 内存写入
```

## 6. 实现策略

### 6.1 渐进实现路线

#### Phase 1: 基础容器
- 实现 array<T, N> 和 slice<T>
- 基本的访问和修改操作
- 简单的边界检查

#### Phase 2: 高级特性  
- 实现 dict<K, V>
- 切片语法支持
- 泛型系统

#### Phase 3: 性能优化
- 编译器优化
- 向量化支持
- SIMD指令生成

#### Phase 4: 生态完善
- 丰富的容器方法
- 标准库集成
- 调试工具

### 6.2 测试和验证

#### 性能基准测试
```aql
// 微基准测试
benchmark("array_access", || {
    let arr: array<int, 1000>;
    for i in 0..1000 {
        arr[i] = i * 2;
    }
});

benchmark("slice_append", || {
    let sl = slice<int>[];
    for i in 0..1000 {
        sl.append(i);
    }
});
```

#### 内存安全测试
```aql
// 边界检查测试
test("bounds_check", || {
    let arr: array<int, 5>;
    expect_panic(|| arr[10]);  // 应该panic
});

// 切片安全测试  
test("slice_safety", || {
    let arr: array<int, 10>;
    let sl = arr[2:8];
    expect_panic(|| sl[10]);   // 切片越界
});
```

## 7. 总结

AQL的容器设计集合了Go的类型明确性、Rust的内存安全和C的高性能特征，同时避免了TypeScript的运行时类型擦除和Python的性能问题。

### 7.1 关键优势
- **类型安全**: 编译时类型检查，避免运行时错误
- **高性能**: 接近C的执行效率，远超动态语言
- **内存高效**: 紧凑的内存布局，最小化开销
- **语义清晰**: 明确区分不同容器类型，避免歧义
- **零成本抽象**: 编译时优化，无运行时性能损失

### 7.2 预期性能
- **array<T, N>**: 等同C数组性能 (1.0x)
- **slice<T>**: 接近Go slice性能 (1.1x)
- **dict<K, V>**: 接近Go map性能 (1.3x)
- **整体**: 比Python快5-10倍，接近Go/Rust水平

这种设计为AQL提供了强大的数据处理能力，特别适合AI应用中的大规模数据操作需求。 