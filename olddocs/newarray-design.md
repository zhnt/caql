# AQL 数组系统设计

## 📋 核心设计

### 设计原则
1. **统一表示**：所有数组使用 `ValueGC` 统一表示
2. **嵌套结构**：多维数组通过"数组的数组"实现
3. **独立扩容**：每个维度独立管理容量
4. **类型安全**：编译时和运行时类型检查
5. **GC集成**：完全集成引用计数垃圾回收

### 核心结构

```go
// 16字节紧凑值表示
type ValueGC struct {
    typeAndFlags uint64 // 类型+标志+GC标记 (8字节)
    data         uint64 // GC对象指针 (8字节)
}

// GC管理的数组数据
type GCArrayData struct {
    Length   uint32 // 数组长度
    Capacity uint32 // 数组容量
    // ValueGC 元素紧随其后
}
```

## 🔧 内存布局

### 一维数组
```
┌─────────────────────────────────────┐
│ GCObject Header (16字节)             │
├─────────────────────────────────────┤
│ GCArrayData (8字节)                 │
│ ├─ Length: uint32                   │
│ └─ Capacity: uint32                 │
├─────────────────────────────────────┤
│ Elements (变长)                     │
│ ├─ Element[0]: ValueGC (16字节)     │
│ ├─ Element[1]: ValueGC (16字节)     │
│ └─ ...                              │
└─────────────────────────────────────┘
```

### 多维数组（嵌套）
```
二维数组 [[1, 2], [3, 4]]：

外层数组 -> [指针→行1, 指针→行2]
             │         │
             ▼         ▼
           行1:[1,2]   行2:[3,4]
```

## 🚀 核心操作

### 创建数组
```go
// 基础创建
func NewArrayValueGC(elements []ValueGC) ValueGC

// 简化创建
func NewSimpleArrayValueGC(length int) ValueGC

// 矩阵创建
func NewMatrixValueGC(rows, cols int) ValueGC
```

### 访问操作
```go
// 获取元素
func ArrayGetValueGC(arrayValue ValueGC, index int) (ValueGC, error)

// 设置元素  
func ArraySetValueGC(arrayValue ValueGC, index int, value ValueGC) error

// 矩阵访问
func GetMatrixElementValueGC(matrix ValueGC, row, col int) (ValueGC, error)
func SetMatrixElementValueGC(matrix ValueGC, row, col int, value ValueGC) error
```

### 扩容机制
```go
// 智能追加元素（分级增长策略）
func ArrayAppendValueGC(arrayValue ValueGC, value ValueGC) error

// 动态扩容（逻辑+物理两层优化）
func expandArrayForIndex(arrayValue ValueGC, targetIndex int) (ValueGC, error)

// 两层扩容实现
func reallocateArrayWithStrategy(arrayValue ValueGC, requiredCap int) (ValueGC, error) {
    // 第一层：计算逻辑容量
    currentCap := getCurrentCapacity(arrayValue)
    logicalCap := calculateLogicalCapacity(currentCap, requiredCap)
    
    // 第二层：对齐到物理size class
    actualCap, actualSize := alignToSizeClass(logicalCap)
    
    // 分配新的数组对象
    return allocateArrayWithCapacity(arrayValue, actualCap, actualSize)
}
```

## 📊 扩容策略

### 两层扩容设计

#### 逻辑层：容量增长策略
```go
func calculateLogicalCapacity(currentCap, requiredCap int) int {
    if requiredCap <= currentCap {
        return currentCap
    }
    
    newCap := currentCap
    switch {
    case currentCap <= 16:
        // 小数组：2^n增长 (100%增长率)
        // 适用：临时变量、小集合
        for newCap < requiredCap {
            if newCap == 0 {
                newCap = 4
            } else {
                newCap *= 2  // 4→8→16
            }
        }
        
    case currentCap <= 512:
        // 中等数组：1.5x增长 (50%增长率)  
        // 适用：脚本数据结构
        for newCap < requiredCap {
            newCap += newCap >> 1  // newCap * 1.5
        }
        
    case currentCap <= 4096:
        // 大数组：1.25x增长 (25%增长率)
        // 适用：游戏对象集合
        for newCap < requiredCap {
            newCap += newCap >> 2  // newCap * 1.25
        }
        
    default:
        // 超大数组：固定增长 (≤12.5%内存浪费)
        // 适用：大数据处理
        increment := max(currentCap>>3, 1024)  // 至少1KB增长
        newCap = ((requiredCap + increment - 1) / increment) * increment
    }
    
    return newCap
}
```

#### 物理层：内存分配Size Classes
```go
// 借鉴Go的size classes，优化内存分配
var ArraySizeClasses = []struct {
    Elements int    // 元素数量
    Size     uint32 // 内存大小(字节)
    Waste    float32 // 内存浪费率
}{
    {4,    88,   12.5},   // 小数组起点
    {8,    152,  5.3},    // 2倍增长
    {16,   280,  2.9},    // 2倍增长
    {24,   408,  14.7},   // 1.5倍增长  
    {36,   600,  8.9},    // 1.5倍增长
    {54,   888,  13.0},   // 1.5倍增长
    {81,   1320, 7.4},    // 1.5倍增长
    {128,  2072, 3.9},    // size class对齐
    {192,  3096, 5.2},    // 1.5倍增长
    {256,  4120, 1.9},    // size class对齐
    {384,  6168, 3.1},    // 1.5倍增长
    {512,  8216, 0.95},   // size class对齐
    {640,  10264, 19.5},  // 1.25倍增长
    {800,  12312, 15.6},  // 1.25倍增长
    {1024, 16408, 0.48},  // 1KB边界
    {2048, 32792, 0.24},  // 2KB边界
    {4096, 65560, 0.12},  // 4KB边界
}

func alignToSizeClass(logicalCap int) (int, uint32) {
    // 将逻辑容量对齐到最接近的size class
    for _, sc := range ArraySizeClasses {
        if logicalCap <= sc.Elements {
            return sc.Elements, sc.Size
        }
    }
    
    // 超大数组：按页对齐
    pageSize := 4096
    elementSize := 16  // sizeof(ValueGC)
    arrayHeaderSize := 24  // GCObject + GCArrayData
    
    totalSize := arrayHeaderSize + logicalCap * elementSize
    alignedSize := ((totalSize + pageSize - 1) / pageSize) * pageSize
    actualCap := (alignedSize - arrayHeaderSize) / elementSize
    
    return actualCap, uint32(alignedSize)
}
```

### 内存浪费分析
| 容量范围 | 策略 | 增长率 | 内存浪费 | 使用场景 |
|----------|------|--------|----------|----------|
| 0-16     | 2^n  | 100%   | ≤50%     | 临时变量、小集合 |
| 16-512   | 1.5x | 50%    | ≤33%     | 脚本数据结构 |
| 512-4096 | 1.25x| 25%    | ≤20%     | 游戏对象集合 |
| >4096    | +固定| 变化   | ≤12.5%   | 大数据处理 |

### 多维独立扩容
```go
matrix[0] = append(matrix[0], newValue)  // 只扩容第一行
matrix[1] = append(matrix[1], newValue)  // 只扩容第二行
matrix = append(matrix, newRow)          // 扩容外层数组
```

### 实现示例
```go
// 游戏精灵集合：使用分级策略优化内存
func CreateSpriteArray(initialSize int) ValueGC {
    sprites := NewSimpleArrayValueGC(0)  // 从空数组开始
    
    // 添加1000个精灵，演示分级扩容
    for i := 0; i < 1000; i++ {
        sprite := NewStringValueGC(fmt.Sprintf("Sprite_%d", i))
        
        // 内部自动应用分级策略：
        // 0-16: 2倍增长 (4→8→16)
        // 16-512: 1.5倍增长 (16→24→36→54→81→121→181→271→406)  
        // 512-1024: 1.25倍增长 (512→640→800→1000)
        ArrayAppendValueGC(sprites, sprite)
    }
    
    return sprites
}

// 内存使用对比：
// 传统2倍策略：最大浪费50% (1024个位置实际只用1000个)
// 分级策略：    实际浪费2.4% (1024个位置用1000个，接近满载)
```

## 🛡️ 安全机制

### 循环引用防护
```go
// 隔离分配，避免内存复用
func AllocateIsolated(size int, objType uint8) *GCObject

// 安全拷贝，避免循环引用
func SafeCopyValueGC(v ValueGC) ValueGC
```

### 引用计数管理
```go
// 简化引用计数
func (v ValueGC) IncRef()
func (v ValueGC) DecRef()

// 自动零引用清理
func handleZeroRefCountSimple(v ValueGC)
```

## 📝 字节码指令

### 基础指令
```go
OP_NEW_ARRAY   // NEW_ARRAY A B : R(A) := array(length=B)
OP_ARRAY_GET   // ARRAY_GET A B C : R(A) := R(B)[R(C)]
OP_ARRAY_SET   // ARRAY_SET A B C : R(A)[R(B)] := R(C)
OP_ARRAY_LEN   // ARRAY_LEN A B : R(A) := len(R(B))
```

### 语法支持
```aql
// 数组字面量
let arr = [1, 2, 3];

// 多维数组
let matrix = [[1, 2], [3, 4]];

// 元素访问
let val = matrix[0][1];  // 编译为多个ARRAY_GET

// 元素设置
matrix[0][1] = 99;      // 编译为ARRAY_GET + ARRAY_SET

// 动态扩容
arr[10] = "new";        // 自动扩容到长度11
```

## ⚡ 性能特性

### 时间复杂度
- **元素访问**：O(1)
- **元素设置**：O(1)
- **追加元素**：均摊O(1)
- **多维访问**：O(维度数)

### 空间复杂度
- **内存开销**：16字节对象头 + 8字节数组头 + n×16字节元素
- **扩容浪费**：智能分级策略
  - 小数组(≤16)：≤50%浪费 (快速分配)
  - 中等数组(16-512)：≤33%浪费 (平衡性能)
  - 大数组(512-4096)：≤20%浪费 (节省内存)
  - 超大数组(>4096)：≤12.5%浪费 (内存友好)
- **物理对齐**：Size Class优化，减少内存碎片
- **多维开销**：每个子数组独立的对象头

## 🧪 测试用例

### 基础功能
```go
func TestBasicArray() {
    arr := NewSimpleArrayValueGC(3)
    ArraySetValueGC(arr, 0, NewSmallIntValueGC(42))
    val, _ := ArrayGetValueGC(arr, 0)
    assert.Equal(t, int32(42), val.AsSmallInt())
}
```

### 多维数组
```go
func TestMatrix() {
    matrix := NewMatrixValueGC(2, 3)
    SetMatrixElementValueGC(matrix, 0, 1, NewStringValueGC("test"))
    val, _ := GetMatrixElementValueGC(matrix, 0, 1)
    assert.Equal(t, "test", val.AsString())
}
```

### 动态扩容
```go
func TestDynamicExpansion() {
    arr := NewSimpleArrayValueGC(0)  // 空数组
    ArraySetValueGC(arr, 100, NewSmallIntValueGC(42))  // 自动扩容
    val, _ := ArrayGetValueGC(arr, 100)
    assert.Equal(t, int32(42), val.AsSmallInt())
}
```

## 🎯 设计优势

### ✅ 优势
- **简洁统一**：所有数组操作通过统一接口
- **类型灵活**：支持混合类型和不规则数组
- **自动管理**：引用计数自动内存管理
- **高效访问**：O(1)随机访问性能
- **独立扩容**：每个维度独立管理

### ⚠️ 局限
- **内存碎片**：多个小对象分配
- **指针跳转**：多维访问需要多次间接寻址
- **对象开销**：每个数组都有完整的GC对象头

### 🔄 适用场景
- **脚本语言数组**：动态类型，灵活操作
- **游戏数据结构**：精灵列表，配置数组
- **矩阵运算**：2D/3D游戏坐标
- **嵌套数据**：JSON式数据结构

## 📈 未来扩展

### 近期计划
- [ ] 数组方法：push/pop/slice/concat
- [ ] 批量操作优化
- [ ] 更好的错误信息

### 长期规划
- [ ] 内存池优化
- [ ] SIMD向量化
- [ ] 紧凑内存布局选项

---

**总结**：AQL数组系统采用"数组的数组"设计，通过ValueGC统一表示，实现了简洁灵活的多维数组支持，具有良好的扩展性和类型安全性。 