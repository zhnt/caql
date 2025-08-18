# AQL垃圾回收与Value系统协同设计

## 概述

AQL的内存管理系统采用**双重策略GC** + **统一Value系统**的创新架构，实现"像Python一样简洁，像C++一样高效，像Go一样内存安全"的设计目标。

## 1. 设计哲学与目标

### 1.1 核心理念

- **协同设计**：Value系统与GC系统深度集成，避免分层开销
- **性能优先**：内联存储最大化，减少堆分配和GC压力
- **简洁实用**：双重策略代替复杂的多重GC，降低实现复杂度
- **渐进演进**：MVP先行，为未来扩展预留空间

### 1.2 性能目标

```
GC性能指标：
- 95%的GC暂停     < 1ms
- 99%的GC暂停     < 5ms  
- GC开销          < 8%的CPU时间
- 内存开销        < 15%额外空间

内存效率指标：
- Value大小       16字节固定
- 对象开销        < 25%额外开销
- 堆利用率        > 80%
- 缓存命中率      > 90%
```

## 2. 统一Value系统设计

### 2.1 核心Value结构

```go
// 16字节紧凑Value设计，缓存友好
type Value struct {
    typeAndFlags uint64    // 类型+标志位 (8字节)
    data         uint64    // 统一数据存储 (8字节)
} // 总计16字节，正好缓存行的1/4

// 类型掩码和标志位
const (
    VALUE_TYPE_MASK     = 0x07    // 低3位：基础类型
    VALUE_FLAG_MASK     = 0xF8    // 位3-7：标志位
    VALUE_EXTENDED_MASK = 0xFF00  // 位8-15：扩展标志
)

// 基础类型枚举（3位，支持8种）
const (
    ValueTypeNil ValueType = iota
    ValueTypeSmallInt     // 30位整数，内联存储
    ValueTypeDouble       // 64位浮点，内联存储
    ValueTypeString       // 字符串，GC管理
    ValueTypeBool         // 布尔值，内联存储
    ValueTypeFunction     // 函数，GC管理
    ValueTypeArray        // 数组，GC管理
    ValueTypeStruct       // 结构体，GC管理
)

// Value标志位（5位）
const (
    FlagInline    = 1 << 3  // 内联存储标记
    FlagMarked    = 1 << 4  // GC标记位（三色标记）
    FlagConst     = 1 << 5  // 常量标记
    FlagWeak      = 1 << 6  // 弱引用标记
    FlagImmutable = 1 << 7  // 不可变对象标记
)
```

### 2.2 内联存储策略

```go
// 类型具体存储方式（性能优化核心）
类型            | 存储位置      | 数据布局                    | GC参与
---------------|--------------|---------------------------|--------
Nil            | 仅类型标签    | 无数据                     | 否
Bool           | data低位     | 1位布尔值                   | 否
SmallInt       | data        | 62位有符号整数              | 否
Double         | data        | 64位IEEE754浮点             | 否
String(≤13B)   | data        | 长度+内容，内联存储          | 否
String(>13B)   | data        | GC对象ID                   | 是
Array          | data        | GC对象ID                   | 是
Function       | data        | GC对象ID                   | 是
Struct         | data        | GC对象ID                   | 是

// 内联存储检查
func (v Value) IsInline() bool {
    return (v.typeAndFlags & FlagInline) != 0
}

func (v Value) RequiresGC() bool {
    typ := v.Type()
    return typ >= ValueTypeString && !v.IsInline()
}
```

### 2.3 智能存储优化

```go
// 字符串智能存储
func NewStringValue(s string) Value {
    if len(s) <= 13 { // 内联存储阈值
        return NewInlineStringValue(s)
    }
    return NewHeapStringValue(s) // GC管理
}

func NewInlineStringValue(s string) Value {
    var data uint64
    length := uint8(len(s))
    
    // 数据布局：[长度1字节][内容13字节]
    data = uint64(length)
    for i, b := range []byte(s) {
        data |= uint64(b) << (8 + i*8)
    }
    
    return Value{
        typeAndFlags: uint64(ValueTypeString) | FlagInline,
        data:         data,
    }
}

// 数值智能存储
func NewNumberValue(n float64) Value {
    // 尝试30位整数存储（2位符号扩展）
    if n == float64(int64(n)) && 
       n >= -536870912 && n <= 536870911 {
        return NewSmallIntValue(int32(n))
    }
    // 否则64位浮点存储
    return NewDoubleValue(n)
}
```

## 3. GC对象头设计

### 3.1 优化的对象头布局

```go
// 16字节GC对象头，8字节对齐，缓存友好
type GCObjectHeader struct {
    RefCount      uint32    // 引用计数 (4字节)
    Size          uint32    // 对象大小 (4字节)
    ExtendedType  uint16    // 扩展类型ID (2字节)
    ObjectType    uint8     // 基础类型 (1字节)
    Flags         uint8     // GC标志 (1字节)
    Reserved      uint32    // 保留字段 (4字节)
} // 总计16字节，对齐友好

// GC标志位定义
const (
    GCFlagMarked     = 1 << 0  // 标记-清除标记
    GCFlagCyclic     = 1 << 1  // 可能循环引用
    GCFlagFinalizer  = 1 << 2  // 需要finalizer
    GCFlagWeakRef    = 1 << 3  // 被弱引用指向
    GCFlagGenOld     = 1 << 4  // 老年代对象
    GCFlagPinned     = 1 << 5  // 不可移动对象
    GCFlagLargeObj   = 1 << 6  // 大对象标记
    GCFlagThreadSafe = 1 << 7  // 线程安全对象
)

// 扩展类型ID范围（支持65536种类型）
const (
    ExtTypeSystemStart = 1      // 系统类型 (1-100)
    ExtTypeUserFunc    = 101    // 用户函数 (101-500)
    ExtTypeUserClass   = 501    // 用户类 (501-65535)
)
```

### 3.2 内存布局优化

```go
// 缓存友好的对象布局
type GCObject struct {
    Header GCObjectHeader    // 16字节，第一个缓存行
    Data   []byte           // 实际数据，对齐到下个缓存行
}

// 小对象内联优化（<64字节）
type SmallObject struct {
    Header GCObjectHeader    // 16字节
    Fields [3]Value         // 48字节，总共64字节（1缓存行）
}

// 字符串对象优化
type StringObject struct {
    Header GCObjectHeader    // 16字节
    Length uint32           // 4字节
    _      uint32           // 4字节填充
    Data   []byte           // 字符串数据（slice header 24字节）
} // 总计48字节，缓存友好

// 数组对象优化
type ArrayObject struct {
    Header   GCObjectHeader  // 16字节
    Length   uint32         // 4字节
    Capacity uint32         // 4字节
    Elements []Value        // Value数组（slice header 24字节）
} // 总计48字节，缓存友好
```

## 4. 双重GC策略设计

### 4.1 架构概览

```go
type UnifiedGCManager struct {
    // === 主要策略：引用计数GC ===
    refCountGC    *RefCountGC    // 处理95%的对象
    
    // === 辅助策略：标记清除GC ===
    markSweepGC   *MarkSweepGC   // 处理5%的循环引用
    
    // === 内存分配器 ===
    allocator     *ObjectAllocator
    
    // === 性能监控 ===
    stats         *GCStats
    monitor       *PerformanceMonitor
    
    // === 配置参数 ===
    config        GCConfig
}
```

### 4.2 引用计数GC（主策略）

```go
type RefCountGC struct {
    allocator    *ObjectAllocator
    freeList     []*FreeListNode  // 分级空闲列表
    finalizers   map[uint64]func() // 对象终结器
    
    // 性能优化
    zeroRefQueue chan *GCObject   // 零引用对象队列
    batchSize    int              // 批量处理大小
}

// 高性能引用计数操作
func (gc *RefCountGC) IncRef(objID uint64) {
    if obj := gc.getObject(objID); obj != nil {
        atomic.AddUint32(&obj.Header.RefCount, 1)
    }
}

func (gc *RefCountGC) DecRef(objID uint64) {
    if obj := gc.getObject(objID); obj != nil {
        newCount := atomic.AddUint32(&obj.Header.RefCount, ^uint32(0))
        
        if newCount == 0 {
            if obj.Header.Flags&GCFlagCyclic == 0 {
                // 快速路径：立即回收
                gc.deallocateImmediate(obj)
            } else {
                // 慢速路径：可能循环引用
                gc.markSweepGC.AddCandidate(obj)
            }
        }
    }
}

// 批量回收优化
func (gc *RefCountGC) deallocateImmediate(obj *GCObject) {
    // 1. 执行finalizer
    if finalizer, exists := gc.finalizers[obj.ID]; exists {
        finalizer()
        delete(gc.finalizers, obj.ID)
    }
    
    // 2. 递减子对象引用（批量操作）
    gc.decrementChildrenBatch(obj)
    
    // 3. 添加到空闲列表
    gc.addToFreeList(obj)
}
```

### 4.3 标记清除GC（辅助策略）

```go
type MarkSweepGC struct {
    candidates   *CandidateSet     // 循环引用候选集合
    greyQueue    *LockFreeQueue    // 灰色对象队列
    isMarking    atomic.Bool       // 标记阶段标志
    
    // 并发控制
    markWorkers  int               // 标记工作线程数
    sweepWorkers int               // 清扫工作线程数
}

// 增量并发标记（三色标记简化为双色）
func (gc *MarkSweepGC) IncrementalMark() {
    const MaxMarkTime = 100 * time.Microsecond // 100μs时间片
    
    startTime := time.Now()
    
    for !gc.greyQueue.Empty() && time.Since(startTime) < MaxMarkTime {
        obj := gc.greyQueue.Pop()
        gc.markBlack(obj)
        gc.scanChildren(obj)
    }
}

// 双色标记（简化三色标记）
func (gc *MarkSweepGC) markBlack(obj *GCObject) {
    // 简化设计：只用白色（未标记）和黑色（已标记）
    atomic.OrUint8(&obj.Header.Flags, GCFlagMarked)
}

// 并发清扫
func (gc *MarkSweepGC) ConcurrentSweep() {
    for obj := range gc.candidates.Iterate() {
        if obj.Header.Flags&GCFlagMarked == 0 {
            // 未标记对象，回收
            gc.deallocate(obj)
        } else {
            // 清除标记，准备下轮
            atomic.AndUint8(&obj.Header.Flags, ^GCFlagMarked)
        }
    }
}
```

### 4.4 写屏障与VM集成

```go
// 轻量级写屏障（只在标记阶段激活）
func (gc *UnifiedGCManager) WriteBarrier(dst *GCObject, fieldOffset int, newValue Value) {
    // 1. 执行实际写入
    gc.setObjectField(dst, fieldOffset, newValue)
    
    // 2. 只在标记阶段需要屏障
    if gc.markSweepGC.isMarking.Load() && newValue.RequiresGC() {
        newObj := gc.getObject(newValue.ObjectID())
        
        // Dijkstra写屏障：标记新值为灰色
        if newObj != nil && newObj.Header.Flags&GCFlagMarked == 0 {
            gc.markSweepGC.greyQueue.Push(newObj)
        }
    }
    
    // 3. 更新引用计数
    if newValue.RequiresGC() {
        gc.refCountGC.IncRef(newValue.ObjectID())
    }
}

// VM指令集成
const (
    OP_SET_FIELD_GC OpCode = iota + 200  // 带GC的字段设置
    OP_SET_INDEX_GC                      // 带GC的数组设置
    OP_MOVE_GC                           // 带GC的值移动
)

// VM执行器集成
func (vm *AQLVirtualMachine) executeSetFieldGC(inst Instruction) {
    obj := vm.getRegister(inst.A).AsObject()
    value := vm.getRegister(inst.C)
    
    // 获取旧值并递减引用
    oldValue := vm.getObjectField(obj, inst.B)
    if oldValue.RequiresGC() {
        vm.gcManager.refCountGC.DecRef(oldValue.ObjectID())
    }
    
    // 设置新值（包含写屏障）
    vm.gcManager.WriteBarrier(obj, inst.B, value)
}
```

## 5. 分配器与内存管理

### 5.1 分级内存分配器

```go
type ObjectAllocator struct {
    // 小对象分配器（<256字节）
    smallAllocs   [8]*FixedSizeAllocator  // 16,32,48...128字节
    
    // 中等对象分配器（256B-4KB）
    mediumAllocs  *SlabAllocator
    
    // 大对象分配器（>4KB）
    largeAllocs   *DirectAllocator
    
    // 线程本地缓存
    threadLocal   []*ThreadLocalHeap
    
    // 内存统计
    stats         AllocatorStats
}

// 智能分配策略
func (alloc *ObjectAllocator) Allocate(size int, objType uint8) *GCObject {
    switch {
    case size <= 256:
        return alloc.allocateSmall(size, objType)
    case size <= 4096:
        return alloc.allocateMedium(size, objType)
    default:
        return alloc.allocateLarge(size, objType)
    }
}

// 小对象快速分配
func (alloc *ObjectAllocator) allocateSmall(size int, objType uint8) *GCObject {
    sizeClass := (size + 15) / 16  // 16字节对齐
    allocator := alloc.smallAllocs[sizeClass]
    
    obj := allocator.Get()
    if obj == nil {
        obj = allocator.AllocateNew()
    }
    
    // 初始化对象头
    obj.Header = GCObjectHeader{
        RefCount:     1,
        Size:         uint32(size),
        ObjectType:   objType,
        ExtendedType: 0,
        Flags:        0,
        Reserved:     0,
    }
    
    return obj
}
```

### 5.2 内存布局优化

```go
// 缓存行对齐的内存池
type AlignedMemoryPool struct {
    pools    []*MemoryChunk
    freeList *AlignedFreeList
    
    // 64字节缓存行对齐
    alignment int
}

func (pool *AlignedMemoryPool) AllocateAligned(size int) unsafe.Pointer {
    // 确保分配的内存缓存行对齐
    alignedSize := (size + pool.alignment - 1) & ^(pool.alignment - 1)
    return pool.allocateRaw(alignedSize)
}

// 对象内存预分配
type ObjectTemplate struct {
    HeaderSize int
    FieldCount int
    TotalSize  int
    Alignment  int
}

var CommonTemplates = map[uint8]*ObjectTemplate{
    ValueTypeString: {16, 1, 48, 8},   // 字符串对象
    ValueTypeArray:  {16, 2, 48, 8},   // 数组对象  
    ValueTypeStruct: {16, 0, 16, 8},   // 基础结构体
}
```

## 6. 性能优化策略

### 6.1 Value操作优化

```go
// 内联函数优化关键路径
//go:inline
func (v Value) Type() ValueType {
    return ValueType(v.typeAndFlags & VALUE_TYPE_MASK)
}

//go:inline  
func (v Value) IsInline() bool {
    return (v.typeAndFlags & FlagInline) != 0
}

//go:inline
func (v Value) RequiresGC() bool {
    return v.Type() >= ValueTypeString && !v.IsInline()
}

// 快速类型转换（避免断言开销）
//go:inline
func (v Value) AsSmallInt() int32 {
    return int32(v.data)
}

//go:inline
func (v Value) AsDouble() float64 {
    return *(*float64)(unsafe.Pointer(&v.data))
}

// 批量Value操作
func CopyValues(dst, src []Value) {
    copy(dst, src)  // 利用Go运行时优化
}

func ZeroValues(values []Value) {
    nilValue := NewNilValue()
    for i := range values {
        values[i] = nilValue
    }
}
```

### 6.2 GC触发优化

```go
type GCTrigger struct {
    // 自适应触发阈值
    allocatedBytes   uint64
    triggerThreshold uint64
    
    // 增量GC控制
    incrementalBudget time.Duration
    lastGCTime       time.Time
    
    // 内存压力检测
    memoryPressure   int32  // 0-100的压力值
}

func (trigger *GCTrigger) ShouldTriggerGC() bool {
    // 1. 内存阈值检查
    if trigger.allocatedBytes > trigger.triggerThreshold {
        return true
    }
    
    // 2. 时间间隔检查
    if time.Since(trigger.lastGCTime) > 10*time.Second {
        return true
    }
    
    // 3. 内存压力检查
    if atomic.LoadInt32(&trigger.memoryPressure) > 80 {
        return true
    }
    
    return false
}

// 自适应阈值调整
func (trigger *GCTrigger) AdjustThreshold(pauseTime time.Duration) {
    if pauseTime < 500*time.Microsecond {
        // GC太快，提高阈值
        trigger.triggerThreshold = uint64(float64(trigger.triggerThreshold) * 1.1)
    } else if pauseTime > 2*time.Millisecond {
        // GC太慢，降低阈值
        trigger.triggerThreshold = uint64(float64(trigger.triggerThreshold) * 0.9)
    }
}
```

### 6.3 并发优化

```go
// 无锁对象池
type LockFreeObjectPool struct {
    head unsafe.Pointer  // *poolNode
    tail unsafe.Pointer  // *poolNode
}

type poolNode struct {
    next   unsafe.Pointer
    object *GCObject
}

func (pool *LockFreeObjectPool) Get() *GCObject {
    for {
        head := atomic.LoadPointer(&pool.head)
        if head == nil {
            return nil
        }
        
        node := (*poolNode)(head)
        next := atomic.LoadPointer(&node.next)
        
        if atomic.CompareAndSwapPointer(&pool.head, head, next) {
            return node.object
        }
    }
}

// 并发安全的引用计数
func AtomicIncRef(refCount *uint32) uint32 {
    return atomic.AddUint32(refCount, 1)
}

func AtomicDecRef(refCount *uint32) uint32 {
    return atomic.AddUint32(refCount, ^uint32(0))
}
```

## 7. 调试与监控

### 7.1 GC统计信息

```go
type GCStats struct {
    // 总体统计
    TotalAllocations    uint64
    TotalDeallocations  uint64
    BytesAllocated      uint64
    BytesFreed          uint64
    
    // GC执行统计
    RefCountGCRuns      uint64
    MarkSweepGCRuns     uint64
    TotalGCTime         time.Duration
    MaxPauseTime        time.Duration
    
    // 性能指标
    AverageAllocSize    uint64
    HeapUtilization     float64
    GCOverhead          float64
    
    // 错误统计
    MemoryLeaks         uint64
    DoubleFrees         uint64
    UseAfterFrees       uint64
}

func (stats *GCStats) Report() string {
    return fmt.Sprintf(`
GC Performance Report:
======================
Total Allocations:    %d
Total Deallocations:  %d
Heap Utilization:     %.2f%%
GC Overhead:          %.2f%%
Max Pause Time:       %v
Average Allocation:   %d bytes
Memory Leaks:         %d
`, stats.TotalAllocations, stats.TotalDeallocations,
   stats.HeapUtilization*100, stats.GCOverhead*100,
   stats.MaxPauseTime, stats.AverageAllocSize, 
   stats.MemoryLeaks)
}
```

### 7.2 内存泄漏检测

```go
type LeakDetector struct {
    objectTracker map[uint64]*AllocationInfo
    stackTraces   map[uint64][]uintptr
    mutex         sync.RWMutex
}

type AllocationInfo struct {
    ObjectID    uint64
    Size        uint32
    Type        uint8
    AllocTime   time.Time
    StackTrace  []uintptr
}

func (detector *LeakDetector) TrackAllocation(obj *GCObject) {
    if !detector.enabled {
        return
    }
    
    detector.mutex.Lock()
    defer detector.mutex.Unlock()
    
    // 记录分配信息
    info := &AllocationInfo{
        ObjectID:   obj.ID,
        Size:       obj.Header.Size,
        Type:       obj.Header.ObjectType,
        AllocTime:  time.Now(),
        StackTrace: captureStackTrace(3), // 跳过3层调用栈
    }
    
    detector.objectTracker[obj.ID] = info
}

func (detector *LeakDetector) CheckLeaks() []*AllocationInfo {
    detector.mutex.RLock()
    defer detector.mutex.RUnlock()
    
    var leaks []*AllocationInfo
    threshold := time.Now().Add(-10 * time.Minute) // 10分钟阈值
    
    for _, info := range detector.objectTracker {
        if info.AllocTime.Before(threshold) {
            leaks = append(leaks, info)
        }
    }
    
    return leaks
}
```

## 8. 配置与调优

### 8.1 GC配置参数

```go
type GCConfig struct {
    // 引用计数GC配置
    RefCountEnabled      bool
    RefCountBatchSize    int
    RefCountQueueSize    int
    
    // 标记清除GC配置  
    MarkSweepEnabled     bool
    MarkSweepWorkers     int
    MarkSweepThreshold   uint64
    
    // 性能调优参数
    MaxPauseTime         time.Duration
    GCPercentage         int     // 堆增长百分比触发GC
    AllocatorType        string  // "slab", "buddy", "tcmalloc"
    
    // 调试选项
    EnableLeakDetection  bool
    EnableGCTracing      bool
    EnablePerformanceLog bool
}

var DefaultGCConfig = GCConfig{
    RefCountEnabled:      true,
    RefCountBatchSize:    100,
    RefCountQueueSize:    1000,
    
    MarkSweepEnabled:     true,
    MarkSweepWorkers:     2,
    MarkSweepThreshold:   64 * 1024 * 1024, // 64MB
    
    MaxPauseTime:         1 * time.Millisecond,
    GCPercentage:         200,
    AllocatorType:        "slab",
    
    EnableLeakDetection:  false,
    EnableGCTracing:      false,
    EnablePerformanceLog: false,
}
```

### 8.2 运行时调优

```go
// 动态调优接口
type GCTuner interface {
    AdjustTriggerThreshold(stats *GCStats)
    AdjustWorkerCount(pressure int)
    AdjustBatchSize(latency time.Duration)
}

type AdaptiveGCTuner struct {
    config *GCConfig
    stats  *GCStats
}

func (tuner *AdaptiveGCTuner) AdjustTriggerThreshold(stats *GCStats) {
    // 根据GC开销调整触发阈值
    if stats.GCOverhead > 0.10 { // 超过10%开销
        // 降低GC频率
        tuner.config.GCPercentage += 10
    } else if stats.GCOverhead < 0.05 { // 低于5%开销
        // 提高GC频率
        tuner.config.GCPercentage -= 5
    }
    
    // 限制范围
    if tuner.config.GCPercentage < 100 {
        tuner.config.GCPercentage = 100
    } else if tuner.config.GCPercentage > 500 {
        tuner.config.GCPercentage = 500
    }
}
```

## 9. 测试与验证

### 9.1 性能测试

```go
func BenchmarkValueOperations(b *testing.B) {
    values := make([]Value, 1000)
    
    b.Run("CreateSmallInt", func(b *testing.B) {
        for i := 0; i < b.N; i++ {
            values[i%1000] = NewSmallIntValue(int32(i))
        }
    })
    
    b.Run("CreateString", func(b *testing.B) {
        for i := 0; i < b.N; i++ {
            values[i%1000] = NewStringValue("test string")
        }
    })
    
    b.Run("TypeCheck", func(b *testing.B) {
        for i := 0; i < b.N; i++ {
            _ = values[i%1000].Type()
        }
    })
}

func BenchmarkGCOperations(b *testing.B) {
    gc := NewUnifiedGCManager(DefaultGCConfig)
    
    b.Run("Allocation", func(b *testing.B) {
        for i := 0; i < b.N; i++ {
            obj := gc.Allocate(64, ValueTypeStruct)
            gc.refCountGC.DecRef(obj.ID)
        }
    })
    
    b.Run("RefCountIncDec", func(b *testing.B) {
        obj := gc.Allocate(64, ValueTypeStruct)
        b.ResetTimer()
        
        for i := 0; i < b.N; i++ {
            gc.refCountGC.IncRef(obj.ID)
            gc.refCountGC.DecRef(obj.ID)
        }
    })
}
```

### 9.2 正确性验证

```go
func TestGCCorrectness(t *testing.T) {
    gc := NewUnifiedGCManager(GCConfig{
        EnableLeakDetection: true,
        EnableGCTracing:     true,
    })
    
    t.Run("NoMemoryLeaks", func(t *testing.T) {
        initialMem := gc.stats.BytesAllocated
        
        // 分配和释放1000个对象
        for i := 0; i < 1000; i++ {
            obj := gc.Allocate(64, ValueTypeStruct)
            gc.refCountGC.DecRef(obj.ID)
        }
        
        // 强制GC
        gc.markSweepGC.ForceCollection()
        
        finalMem := gc.stats.BytesAllocated
        if finalMem != initialMem {
            t.Errorf("Memory leak detected: %d bytes", finalMem-initialMem)
        }
    })
    
    t.Run("CircularReference", func(t *testing.T) {
        // 创建循环引用
        obj1 := gc.Allocate(64, ValueTypeStruct)
        obj2 := gc.Allocate(64, ValueTypeStruct)
        
        // obj1 -> obj2 -> obj1
        gc.SetObjectField(obj1, 0, NewGCObjectValue(obj2.ID))
        gc.SetObjectField(obj2, 0, NewGCObjectValue(obj1.ID))
        
        // 释放根引用
        gc.refCountGC.DecRef(obj1.ID)
        gc.refCountGC.DecRef(obj2.ID)
        
        // 标记清除应该回收这些对象
        gc.markSweepGC.ForceCollection()
        
        // 验证对象已被回收
        if gc.GetObject(obj1.ID) != nil || gc.GetObject(obj2.ID) != nil {
            t.Error("Circular reference not collected")
        }
    })
}
```

## 10. 总结

AQL的GC与Value系统协同设计实现了以下关键特性：

### 10.1 性能优势

1. **内联存储优化**：95%的小值避免堆分配
2. **缓存友好布局**：16字节Value，8字节对齐对象头
3. **双重GC策略**：95%即时回收 + 5%标记清除
4. **批量操作优化**：减少系统调用和锁竞争

### 10.2 内存安全

1. **引用计数**：防止悬挂指针
2. **标记清除**：处理循环引用
3. **类型安全**：运行时类型检查
4. **内存对齐**：避免对齐错误

### 10.3 可扩展性

1. **扩展类型ID**：支持65K+用户定义类型
2. **模块化设计**：GC策略可独立演进
3. **配置化参数**：运行时调优能力
4. **监控接口**：完整的性能分析

这个设计为AQL提供了**高性能**、**内存安全**、**易于扩展**的内存管理基础，完美支持AI服务编排的高并发、低延迟需求。 