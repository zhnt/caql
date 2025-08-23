# AQL 容器系统重构计划

## 🎯 重构目标

**现状**: 4个容器类型（array/slice/vector/dict）90%代码重复
**目标**: 统一基类架构，减少90%冗余代码
**预期收益**: 
- 代码量: 500行 → 50行
- 维护成本: 降低80%
- 一致性: 100%
- 性能: 零开销

## 📋 重构策略

### 阶段1: 基类完善 (第1天)
- [ ] 完善 `acontainer.h` 统一接口
- [ ] 实现 `acontainer.c` 通用操作
- [ ] 建立容器类型枚举

### 阶段2: 逐个容器重构 (第2-4天)
- [ ] **Day 2**: `aarray.c` → `acontainer` 基类
- [ ] **Day 3**: `aslice.c` + `avector.c` → 统一实现
- [ ] **Day 4**: `adict.c` → 特化容器

### 阶段3: 清理和测试 (第5天)
- [ ] 移除所有重复代码
- [ ] 更新测试用例
- [ ] 性能回归验证

## 🔧 技术方案

### 统一数据结构
```c
// acontainer.h
typedef struct AQL_ContainerBase {
    CommonHeader;
    DataType dtype;        /* 元素类型 */
    ContainerType type;    /* 容器类型 */
    size_t length;         /* 当前长度 */
    size_t capacity;       /* 分配容量 */
    void *data;           /* 数据指针 */
    uint32_t flags;       /* 容器标志 */
} AQL_ContainerBase;

// 容器类型枚举
typedef enum {
    CONTAINER_ARRAY,
    CONTAINER_SLICE, 
    CONTAINER_VECTOR,
    CONTAINER_DICT
} ContainerType;

// 统一操作接口
typedef struct ContainerOps {
    int (*get)(aql_State*, AQL_ContainerBase*, size_t, TValue*);
    int (*set)(aql_State*, AQL_ContainerBase*, size_t, const TValue*);
    int (*resize)(aql_State*, AQL_ContainerBase*, size_t);
    int (*append)(aql_State*, AQL_ContainerBase*, const TValue*);
    void (*destroy)(aql_State*, AQL_ContainerBase*);
} ContainerOps;
```

### 容器特化策略

#### 1. 数组容器 (aarray.c)
```c
// 特化实现
static const ContainerOps array_ops = {
    .get = array_get,
    .set = array_set,
    .resize = array_resize,
    .append = array_append,
    .destroy = array_destroy
};

// 简化实现
AQL_Array* aql_array_new(aql_State *L, DataType dtype, size_t capacity) {
    return container_new(L, CONTAINER_ARRAY, dtype, capacity, &array_ops);
}
```

#### 2. 切片容器 (aslice.c)  
```c
// 切片特化 - 共享数组数据
AQL_Slice* aql_slice_new(aql_State *L, AQL_Array *array, size_t start, size_t end) {
    AQL_ContainerBase *slice = container_new(L, CONTAINER_SLICE, array->dtype, 0, &slice_ops);
    slice->data = array->data + start;
    slice->length = end - start;
    return (AQL_Slice*)slice;
}
```

#### 3. 向量容器 (avector.c)
```c
// 向量特化 - 固定大小数组
AQL_Vector* aql_vector_new(aql_State *L, DataType dtype, size_t size) {
    return container_new(L, CONTAINER_VECTOR, dtype, size, &vector_ops);
}
```

#### 4. 字典容器 (adict.c)
```c
// 字典特化 - 哈希表实现
AQL_Dict* aql_dict_new(aql_State *L, DataType key_type, DataType value_type) {
    return container_new(L, CONTAINER_DICT, value_type, 0, &dict_ops);
}
```

## 🔄 重构步骤

### Step 1: 创建统一基类
```bash
# acontainer.h 需要添加
- ContainerType 枚举
- AQL_ContainerBase 结构
- 统一操作接口
- 内存管理宏

# acontainer.c 需要实现
- container_new() 统一创建
- container_resize() 通用调整
- container_bounds_check() 边界检查
- container_grow_policy() 增长策略
```

### Step 2: 逐个容器迁移

#### aarray.c 重构示例
**Before:**
```c
// 当前重复代码约150行
struct AQL_Array {
    CommonHeader;
    DataType dtype;
    size_t length;
    size_t capacity;
    TValue *data;
};

// 重复实现get/set/resize等
```

**After:**
```c
// 重构后约30行
struct AQL_Array {
    AQL_ContainerBase base;
};

// 使用统一操作 + 特化回调
```

### Step 3: 移除重复代码

#### 待删除的重复函数
- [ ] `aql_index2addr` - 4个文件重复
- [ ] `checkbounds` - 每个容器重复  
- [ ] `resize_policy` - 统一增长策略
- [ ] `copy_data` - 统一内存复制

### Step 4: 兼容性层

#### 向后兼容API
```c
// 保持现有API不变，内部转发到基类
static inline TValue* aql_array_get(aql_State *L, AQL_Array *arr, size_t idx) {
    return container_get(L, (AQL_ContainerBase*)arr, idx);
}
```

## 🧪 测试策略

### 单元测试
- [ ] 每个容器功能测试
- [ ] 边界条件测试  
- [ ] 内存管理测试
- [ ] 性能对比测试

### 集成测试
- [ ] 现有测试用例验证
- [ ] JIT/AOT兼容性测试
- [ ] 垃圾回收测试
- [ ] 序列化/反序列化测试

## 📊 迁移检查清单

### 每日任务
- [ ] **Day 1**: 完善acontainer基类
- [ ] **Day 2**: 重构aarray.c + 测试
- [ ] **Day 3**: 重构aslice.c + avector.c
- [ ] **Day 4**: 重构adict.c + 清理
- [ ] **Day 5**: 全面测试 + 文档更新

### 验证标准
- [ ] 所有现有测试通过
- [ ] 性能零退化
- [ ] API向后兼容
- [ ] 内存使用无泄漏

## 🔍 风险缓解

### 低风险措施
- 保持API签名不变
- 增量重构，逐个容器
- 完整测试覆盖
- 版本控制回滚机制

### 应急方案
- 如有问题可快速回滚到原实现
- 并行维护两套实现直到稳定
- 分阶段合并到主分支

## ✅ 成功指标

重构完成后将验证：
- ✅ 代码行数减少80%
- ✅ 测试覆盖率保持100%  
- ✅ 性能基准测试通过
- ✅ 内存使用无增长
- ✅ 所有现有功能正常

**准备开始第一阶段重构？**