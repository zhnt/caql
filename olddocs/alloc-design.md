# AQL内存分配器设计

## 概述

AQL内存分配器是GC系统的核心组件，负责高效、对齐、GC友好的内存分配。设计哲学是**"像Lua一样简洁，像Go一样高效"**，实现渐进式演进的多级分配策略。

## 1. 设计目标与挑战

### 1.1 核心目标

```
性能目标：
- 分配延迟：    < 100ns (小对象)
- 内存利用率：  > 85%
- 碎片率：      < 10%
- 对齐要求：    16字节对齐

功能目标：
- GC协作：      深度集成引用计数和标记清除
- 类型感知：    根据对象类型优化分配策略
- 缓存友好：    所有对象缓存行对齐
- 可配置：      支持不同使用场景的调优
```

### 1.2 技术挑战

1. **内存碎片控制**：避免长期运行产生的外部碎片
2. **多线程安全**：高并发环境下的无锁或低锁争用
3. **GC集成复杂性**：分配器与GC系统的深度耦合
4. **性能与简洁性平衡**：避免过度工程化

## 2. 业界经验与启示

### 2.1 Lua分配器的简洁哲学

**Lua的统一接口设计**：
```c
// Lua分配器接口 - 一个函数处理所有情况
typedef void * (*lua_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);

// 使用示例
void* lua_default_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud; (void)osize;  // 未使用的参数
    
    if (nsize == 0) {
        free(ptr);           // 释放内存
        return NULL;
    } else {
        return realloc(ptr, nsize);  // 分配/重分配
    }
}
```

**Lua启示**：
- ✅ **统一接口**：分配/释放/重分配通过一个函数
- ✅ **用户可定制**：嵌入应用可提供自己的分配器
- ✅ **零依赖**：不依赖复杂的内存管理库
- ✅ **简单有效**：适合资源受限的嵌入式环境

### 2.2 Go分配器的高效策略

**Go的多级分配架构**：
```
┌────────────────────────────────────────────────────────────┐
│                    Go Runtime Allocator                    │
├────────────────┬─────────────────┬─────────────────────────┤
│   小对象分配    │    大对象分配    │      巨大对象分配        │
│   (<32KB)      │   (32KB-1MB)    │        (>=1MB)         │
├────────────────┼─────────────────┼─────────────────────────┤
│ • 67个Size类    │ • 直接从heap分配 │ • 直接从OS申请          │
│ • 线程本地缓存  │ • 页级管理       │ • 特殊GC处理           │
│ • 中心缓存池    │ • span管理      │ • mmap/munmap          │
└────────────────┴─────────────────┴─────────────────────────┘
```

**Go的Size Classes（关键部分）**：
```go
// Go运行时的size class表（简化版）
var sizeClasses = []sizeClass{
    {size: 8,     pages: 1, objects: 512},   // 最小分配单位
    {size: 16,    pages: 1, objects: 256},   // 适合指针
    {size: 24,    pages: 1, objects: 170},   // 小结构体
    {size: 32,    pages: 1, objects: 128},   // interface{}
    {size: 48,    pages: 1, objects: 85},    // 中等结构体
    {size: 64,    pages: 1, objects: 64},    // 1缓存行
    {size: 80,    pages: 1, objects: 51},    // 
    {size: 96,    pages: 1, objects: 42},    // 
    {size: 112,   pages: 1, objects: 36},    // 
    {size: 128,   pages: 1, objects: 32},    // 2缓存行
    // ... 更多size class到32KB
}
```

**Go的核心创新**：
- ✅ **线程本地缓存**：每个P有自己的mcache，减少锁争用
- ✅ **中心缓存管理**：mcentral协调span的分配和回收
- ✅ **页级内存管理**：mheap管理大块内存的分配
- ✅ **GC深度集成**：与三色标记、写屏障无缝协作

### 2.3 其他语言的经验

**JavaScript V8**：
- 分代堆设计：新生代用copying GC，老生代用mark-sweep
- 内联缓存：优化对象属性访问

**Python**：
- PyMalloc：专门为小对象(<512字节)优化的分配器
- 内存池：减少系统调用开销

**Java HotSpot**：
- TLAB (Thread Local Allocation Buffer)：线程本地分配缓冲区
- 分代假设：大部分对象在年轻代就死掉

## 3. AQL分配器架构设计

### 3.1 整体架构：渐进式三阶段演进

```
Phase 1: MVP分配器 (当前目标)
┌─────────────────────────────────────────────────────────┐
│                  SimpleAllocator                        │
├─────────────────┬─────────────────┬───────────────────┤
│   小对象分配     │   中等对象分配   │    大对象分配      │
│   (16-256字节)   │  (256B-4KB)    │     (>4KB)       │
├─────────────────┼─────────────────┼───────────────────┤
│ • 8个Size类     │ • Slab分配器    │ • 直接malloc      │
│ • FreeList管理  │ • 块级管理      │ • 页对齐          │
│ • 16字节对齐    │ • 减少碎片      │ • 大对象标记       │
└─────────────────┴─────────────────┴───────────────────┘

Phase 2: 优化分配器 (性能优化)
┌─────────────────────────────────────────────────────────┐
│                OptimizedAllocator                       │
│  ┌─────────────────────────────────────────────────────┐│
│  │              SimpleAllocator                        ││
│  └─────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────┐│
│  │ • 线程本地缓存 (ThreadLocalCache)                   ││
│  │ • 中心缓存池 (CentralPool)                          ││
│  │ • 批量分配优化                                      ││
│  │ • 更智能的size class选择                            ││
│  └─────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────┘

Phase 3: 高级分配器 (生产优化)
┌─────────────────────────────────────────────────────────┐
│                AdvancedAllocator                        │
│  ┌─────────────────────────────────────────────────────┐│
│  │            OptimizedAllocator                       ││
│  └─────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────┐│
│  │ • 内存压缩器 (MemoryCompactor)                      ││
│  │ • 分配分析器 (AllocationProfiler)                   ││
│  │ • 自适应调优 (AdaptiveTuning)                       ││
│  │ • NUMA感知优化                                      ││
│  └─────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────┘
```

### 3.2 Phase 1: MVP分配器详细设计

**核心接口**：
```go
// AQL分配器主接口 - 借鉴Lua的简洁性
type AQLAllocator interface {
    // 分配GC对象
    Allocate(size uint32, objType ObjectType) *GCObject
    
    // 释放GC对象
    Deallocate(obj *GCObject)
    
    // 获取分配统计
    Stats() *AllocationStats
    
    // 配置调优
    Configure(config *AllocatorConfig)
}

// 简单分配器实现
type SimpleAllocator struct {
    // === 三级分配策略 ===
    small  [8]*FixedSizeAllocator  // 小对象：8个size class
    medium *SlabAllocator          // 中等对象：slab分配
    large  *DirectAllocator        // 大对象：直接分配
    
    // === GC集成 ===
    gcManager *UnifiedGCManager    // GC管理器
    stats     *AllocationStats     // 分配统计
    config    *AllocatorConfig     // 分配器配置
    
    // === 同步控制 ===
    mutex sync.RWMutex             // 保护分配器状态
}
```

**AQL简化的Size Classes（8个，而不是Go的67个）**：
```go
// AQL Size Classes - 优化的8级分类
const (
    SizeClass16  = 0  // 16字节  - GCObjectHeader only
    SizeClass32  = 1  // 32字节  - 小数据 + header
    SizeClass48  = 2  // 48字节  - StringObject, ArrayObject (缓存友好)
    SizeClass64  = 3  // 64字节  - SmallObject (正好1个缓存行)
    SizeClass96  = 4  // 96字节  - 中等对象
    SizeClass128 = 5  // 128字节 - 大一点的对象 (2个缓存行)
    SizeClass192 = 6  // 192字节 - 更大对象 (3个缓存行)
    SizeClass256 = 7  // 256字节 - 小对象上限 (4个缓存行)
)

// Size Class配置表
var sizeClassTable = [8]SizeClassInfo{
    {size: 16,  objectsPerPage: 256, wasteRatio: 0.00}, // 完美匹配
    {size: 32,  objectsPerPage: 128, wasteRatio: 0.00}, // 完美匹配
    {size: 48,  objectsPerPage: 85,  wasteRatio: 0.02}, // 很少浪费
    {size: 64,  objectsPerPage: 64,  wasteRatio: 0.00}, // 缓存行对齐
    {size: 96,  objectsPerPage: 42,  wasteRatio: 0.03}, // 可接受
    {size: 128, objectsPerPage: 32,  wasteRatio: 0.00}, // 2缓存行
    {size: 192, objectsPerPage: 21,  wasteRatio: 0.05}, // 可接受
    {size: 256, objectsPerPage: 16,  wasteRatio: 0.00}, // 4缓存行
}

type SizeClassInfo struct {
    size           uint32  // 对象大小
    objectsPerPage int     // 每页对象数量
    wasteRatio     float64 // 内存浪费比例
}
```

## 4. 核心组件设计

### 4.1 小对象分配器 (FixedSizeAllocator)

```go
// 固定大小分配器 - 管理特定size class的对象
type FixedSizeAllocator struct {
    sizeClass    int           // size class编号
    objectSize   uint32        // 对象大小
    pageSize     uint32        // 页大小 (通常4KB)
    objectsPerPage int         // 每页对象数量
    
    // 内存页管理
    pages        []*AllocPage  // 分配页列表
    currentPage  *AllocPage    // 当前分配页
    freePages    []*AllocPage  // 空闲页列表
    
    // 性能优化
    fastPath     *FreeList     // 快速分配路径
    stats        AllocatorStats
    mutex        sync.Mutex
}

// 分配页结构
type AllocPage struct {
    memory      unsafe.Pointer // 页内存起始地址
    freeList    *FreeList      // 页内空闲对象链表
    objectCount int            // 已分配对象数量
    maxObjects  int            // 最大对象数量
    next        *AllocPage     // 下一个页
}

// 空闲链表节点
type FreeList struct {
    next *FreeList
}

// 高效分配实现
func (fsa *FixedSizeAllocator) Allocate() unsafe.Pointer {
    // 快速路径：从空闲链表获取
    if obj := fsa.fastPath; obj != nil {
        fsa.fastPath = obj.next
        return unsafe.Pointer(obj)
    }
    
    // 慢速路径：需要新页或查找空闲页
    return fsa.allocateSlow()
}

func (fsa *FixedSizeAllocator) allocateSlow() unsafe.Pointer {
    fsa.mutex.Lock()
    defer fsa.mutex.Unlock()
    
    // 尝试当前页
    if fsa.currentPage != nil && fsa.currentPage.objectCount < fsa.currentPage.maxObjects {
        return fsa.allocateFromPage(fsa.currentPage)
    }
    
    // 尝试空闲页
    if len(fsa.freePages) > 0 {
        page := fsa.freePages[len(fsa.freePages)-1]
        fsa.freePages = fsa.freePages[:len(fsa.freePages)-1]
        fsa.currentPage = page
        return fsa.allocateFromPage(page)
    }
    
    // 分配新页
    page := fsa.allocateNewPage()
    fsa.currentPage = page
    return fsa.allocateFromPage(page)
}
```

### 4.2 中等对象分配器 (SlabAllocator)

```go
// Slab分配器 - 处理256B-4KB的中等对象
type SlabAllocator struct {
    slabs       map[uint32]*Slab  // size -> slab映射
    slabList    []*Slab           // 所有slab列表
    chunkSize   uint32            // chunk大小 (通常64KB)
    
    mutex       sync.RWMutex
    stats       SlabStats
}

// Slab结构 - 管理相同大小的对象
type Slab struct {
    objectSize   uint32        // 对象大小
    chunkSize    uint32        // chunk大小
    chunks       []*SlabChunk  // chunk列表
    partialList  *SlabChunk    // 部分使用的chunk
    emptyList    *SlabChunk    // 空的chunk
    fullList     *SlabChunk    // 满的chunk
    
    mutex        sync.Mutex
}

// Slab Chunk - 实际的内存块
type SlabChunk struct {
    memory       unsafe.Pointer // 内存起始地址
    freeList     *FreeList      // 空闲对象链表
    objectCount  int            // 已分配对象数量
    maxObjects   int            // 最大对象数量
    next         *SlabChunk     // 链表指针
    prev         *SlabChunk
}

// Slab分配实现
func (sa *SlabAllocator) Allocate(size uint32) unsafe.Pointer {
    // 对齐到16字节边界
    alignedSize := Align16(size)
    
    sa.mutex.RLock()
    slab, exists := sa.slabs[alignedSize]
    sa.mutex.RUnlock()
    
    if !exists {
        // 创建新的slab
        slab = sa.createSlab(alignedSize)
    }
    
    return slab.Allocate()
}

func (slab *Slab) Allocate() unsafe.Pointer {
    slab.mutex.Lock()
    defer slab.mutex.Unlock()
    
    // 优先从部分使用的chunk分配
    if slab.partialList != nil {
        chunk := slab.partialList
        obj := chunk.freeList
        chunk.freeList = obj.next
        chunk.objectCount++
        
        // 如果chunk满了，移到full list
        if chunk.objectCount >= chunk.maxObjects {
            slab.removeFromPartial(chunk)
            slab.addToFull(chunk)
        }
        
        return unsafe.Pointer(obj)
    }
    
    // 从空chunk分配
    if slab.emptyList != nil {
        chunk := slab.emptyList
        slab.removeFromEmpty(chunk)
        slab.addToPartial(chunk)
        return slab.allocateFromChunk(chunk)
    }
    
    // 分配新chunk
    chunk := slab.allocateNewChunk()
    slab.addToPartial(chunk)
    return slab.allocateFromChunk(chunk)
}
```

### 4.3 大对象分配器 (DirectAllocator)

```go
// 直接分配器 - 处理>4KB的大对象
type DirectAllocator struct {
    largeObjects map[unsafe.Pointer]*LargeObjectInfo // 大对象追踪
    totalSize    uint64                              // 总分配大小
    objectCount  int                                 // 对象数量
    
    mutex        sync.RWMutex
    stats        DirectAllocStats
}

// 大对象信息
type LargeObjectInfo struct {
    size      uint32    // 对象大小
    allocTime time.Time // 分配时间
    objType   ObjectType // 对象类型
}

// 大对象分配 - 直接从OS申请，页对齐
func (da *DirectAllocator) Allocate(size uint32, objType ObjectType) unsafe.Pointer {
    // 对齐到页边界 (4KB)
    alignedSize := AlignPage(size)
    
    // 使用mmap分配大块内存 (如果支持)
    ptr := allocateAlignedMemory(alignedSize)
    if ptr == nil {
        return nil
    }
    
    // 记录大对象信息
    da.mutex.Lock()
    da.largeObjects[ptr] = &LargeObjectInfo{
        size:      size,
        allocTime: time.Now(),
        objType:   objType,
    }
    da.totalSize += uint64(alignedSize)
    da.objectCount++
    da.mutex.Unlock()
    
    return ptr
}

func (da *DirectAllocator) Deallocate(ptr unsafe.Pointer) {
    da.mutex.Lock()
    defer da.mutex.Unlock()
    
    info, exists := da.largeObjects[ptr]
    if !exists {
        return // 错误：试图释放未知对象
    }
    
    delete(da.largeObjects, ptr)
    da.totalSize -= uint64(AlignPage(info.size))
    da.objectCount--
    
    // 释放内存回OS
    freeAlignedMemory(ptr, AlignPage(info.size))
}
```

## 5. GC集成设计

### 5.1 分配时的GC协作

```go
// 统一分配入口 - 与GC深度集成
func (sa *SimpleAllocator) Allocate(size uint32, objType ObjectType) *GCObject {
    // 1. 计算总大小 (对象大小 + GC头)
    totalSize := uint32(unsafe.Sizeof(GCObjectHeader{})) + size
    alignedSize := Align16(totalSize)
    
    // 2. 选择合适的分配策略
    var ptr unsafe.Pointer
    switch {
    case alignedSize <= 256:
        ptr = sa.allocateSmall(alignedSize)
    case alignedSize <= 4096:
        ptr = sa.medium.Allocate(alignedSize)
    default:
        ptr = sa.large.Allocate(alignedSize, objType)
    }
    
    if ptr == nil {
        // 分配失败，可能需要触发GC
        if sa.gcManager.ShouldTriggerGC() {
            sa.gcManager.ForceGC()
            // 重试分配
            ptr = sa.retryAllocation(alignedSize, objType)
        }
        if ptr == nil {
            return nil // 内存不足
        }
    }
    
    // 3. 初始化GC对象头
    obj := (*GCObject)(ptr)
    obj.Header = NewGCObjectHeader(objType, size)
    
    // 4. 设置GC相关标志
    if MightHaveCycles(objType) {
        obj.Header.SetCyclic()
    }
    
    // 5. 更新统计信息
    sa.stats.IncAllocation(alignedSize)
    sa.gcManager.stats.IncAllocation(alignedSize)
    sa.gcManager.stats.IncObjectType(objType)
    
    // 6. 检查是否需要触发增量GC
    if sa.gcManager.ShouldTriggerIncrementalGC() {
        go sa.gcManager.RunIncrementalGC()
    }
    
    return obj
}

// 判断对象类型是否可能有循环引用
func MightHaveCycles(objType ObjectType) bool {
    switch objType {
    case ObjectTypeArray, ObjectTypeStruct, ObjectTypeClosure:
        return true // 这些类型可能包含对其他对象的引用
    case ObjectTypeString, ObjectTypeFunction:
        return false // 这些类型通常不含循环引用
    default:
        return true // 保守估计
    }
}
```

### 5.2 释放时的GC协作

```go
// 统一释放入口
func (sa *SimpleAllocator) Deallocate(obj *GCObject) {
    if obj == nil {
        return
    }
    
    // 1. 更新统计信息
    size := obj.Size() + uint32(unsafe.Sizeof(GCObjectHeader{}))
    sa.stats.IncDeallocation(size)
    sa.gcManager.stats.IncDeallocation(size)
    sa.gcManager.stats.DecObjectType(obj.Type())
    
    // 2. 清理GC状态
    obj.Header.ClearMarked()
    
    // 3. 根据大小选择释放策略
    switch {
    case size <= 256:
        sa.deallocateSmall(obj)
    case size <= 4096:
        sa.medium.Deallocate(unsafe.Pointer(obj))
    default:
        sa.large.Deallocate(unsafe.Pointer(obj))
    }
    
    // 4. 检查是否需要内存压缩
    if sa.gcManager.ShouldCompactMemory() {
        go sa.gcManager.RunCompaction()
    }
}
```

## 6. 性能优化策略

### 6.1 内存对齐优化

```go
// 确保所有分配都是16字节对齐，缓存友好
func Align16(size uint32) uint32 {
    return (size + 15) &^ 15
}

// 页对齐 (4KB边界)
func AlignPage(size uint32) uint32 {
    const pageSize = 4096
    return (size + pageSize - 1) &^ (pageSize - 1)
}

// 缓存行对齐 (64字节边界)
func AlignCacheLine(size uint32) uint32 {
    const cacheLineSize = 64
    return (size + cacheLineSize - 1) &^ (cacheLineSize - 1)
}
```

### 6.2 快速路径优化

```go
// 内联的快速分配路径
//go:inline
func (sa *SimpleAllocator) FastAllocate(sizeClass int) *GCObject {
    if sizeClass < 0 || sizeClass >= 8 {
        return nil // 越界检查
    }
    
    allocator := sa.small[sizeClass]
    if allocator.fastPath != nil {
        // 快速路径：直接从空闲链表获取
        obj := (*GCObject)(unsafe.Pointer(allocator.fastPath))
        allocator.fastPath = allocator.fastPath.next
        
        // 重置对象头（避免残留数据）
        obj.Header = GCObjectHeader{}
        
        return obj
    }
    
    // 回退到慢速路径
    return nil
}
```

### 6.3 批量操作优化

```go
// 批量分配 - 减少锁争用
func (sa *SimpleAllocator) AllocateBatch(count int, sizeClass int) []*GCObject {
    if count <= 0 || sizeClass < 0 || sizeClass >= 8 {
        return nil
    }
    
    allocator := sa.small[sizeClass]
    objects := make([]*GCObject, 0, count)
    
    allocator.mutex.Lock()
    defer allocator.mutex.Unlock()
    
    for i := 0; i < count && len(objects) < count; i++ {
        if ptr := allocator.allocateFromCurrentPage(); ptr != nil {
            objects = append(objects, (*GCObject)(ptr))
        } else {
            break // 当前页已满，避免复杂的页分配逻辑
        }
    }
    
    return objects
}

// 批量释放
func (sa *SimpleAllocator) DeallocateBatch(objects []*GCObject) {
    if len(objects) == 0 {
        return
    }
    
    // 按size class分组
    groups := make(map[int][]*GCObject)
    for _, obj := range objects {
        if obj != nil {
            size := obj.Size() + uint32(unsafe.Sizeof(GCObjectHeader{}))
            sizeClass := SizeClass(size)
            if sizeClass >= 0 && sizeClass < 8 {
                groups[sizeClass] = append(groups[sizeClass], obj)
            }
        }
    }
    
    // 批量释放每个size class
    for sizeClass, objs := range groups {
        sa.small[sizeClass].DeallocateBatch(objs)
    }
}
```

## 7. 监控与调试

### 7.1 分配统计

```go
// 详细的分配统计信息
type AllocationStats struct {
    // 按size class统计
    SmallAllocStats   [8]SizeClassStats  // 8个小对象size class
    MediumAllocStats  SlabStats          // 中等对象统计
    LargeAllocStats   DirectAllocStats   // 大对象统计
    
    // 总体统计
    TotalAllocations   uint64    // 总分配次数
    TotalDeallocations uint64    // 总释放次数
    TotalBytesAllocated uint64   // 总分配字节数
    TotalBytesFreed    uint64    // 总释放字节数
    
    // 性能指标
    AverageAllocTime   time.Duration // 平均分配时间
    AverageAllocSize   uint64        // 平均分配大小
    FragmentationRatio float64       // 碎片率
    
    // 错误统计
    AllocationFailures uint64 // 分配失败次数
    InvalidDeallocations uint64 // 无效释放次数
}

// Size Class统计
type SizeClassStats struct {
    Allocations    uint64        // 分配次数
    Deallocations  uint64        // 释放次数
    BytesAllocated uint64        // 分配字节数
    BytesFreed     uint64        // 释放字节数
    PagesAllocated int           // 分配页数
    ObjectsPerPage float64       // 平均每页对象数
    WasteRatio     float64       // 浪费比例
}

// 生成分配报告
func (stats *AllocationStats) Report() string {
    var report strings.Builder
    
    report.WriteString("AQL Allocator Performance Report\n")
    report.WriteString("=================================\n\n")
    
    // 总体统计
    report.WriteString(fmt.Sprintf("Total Allocations:     %d\n", stats.TotalAllocations))
    report.WriteString(fmt.Sprintf("Total Deallocations:   %d\n", stats.TotalDeallocations))
    report.WriteString(fmt.Sprintf("Bytes Allocated:       %d (%.2f MB)\n", 
        stats.TotalBytesAllocated, float64(stats.TotalBytesAllocated)/1024/1024))
    report.WriteString(fmt.Sprintf("Average Alloc Size:    %d bytes\n", stats.AverageAllocSize))
    report.WriteString(fmt.Sprintf("Average Alloc Time:    %v\n", stats.AverageAllocTime))
    report.WriteString(fmt.Sprintf("Fragmentation Ratio:   %.2f%%\n", stats.FragmentationRatio*100))
    
    // Size Class详情
    report.WriteString("\nSize Class Distribution:\n")
    report.WriteString("Size | Allocs | Deallocs | Waste% | Pages\n")
    report.WriteString("-----|--------|----------|--------|------\n")
    
    for i, scs := range stats.SmallAllocStats {
        size := [8]uint32{16, 32, 48, 64, 96, 128, 192, 256}[i]
        report.WriteString(fmt.Sprintf("%4d | %6d | %8d | %5.1f%% | %4d\n",
            size, scs.Allocations, scs.Deallocations, 
            scs.WasteRatio*100, scs.PagesAllocated))
    }
    
    return report.String()
}
```

### 7.2 内存泄漏检测

```go
// 分配器级别的内存泄漏检测
type AllocationTracker struct {
    allocations map[unsafe.Pointer]*AllocationInfo
    mutex       sync.RWMutex
    enabled     bool
}

type AllocationInfo struct {
    Size      uint32
    Type      ObjectType
    Timestamp time.Time
    StackTrace []uintptr // 分配时的调用栈
}

func (tracker *AllocationTracker) TrackAllocation(ptr unsafe.Pointer, size uint32, objType ObjectType) {
    if !tracker.enabled {
        return
    }
    
    tracker.mutex.Lock()
    defer tracker.mutex.Unlock()
    
    // 捕获调用栈
    stackTrace := make([]uintptr, 10)
    n := runtime.Callers(3, stackTrace) // 跳过3层调用
    
    tracker.allocations[ptr] = &AllocationInfo{
        Size:       size,
        Type:       objType,
        Timestamp:  time.Now(),
        StackTrace: stackTrace[:n],
    }
}

func (tracker *AllocationTracker) CheckLeaks(maxAge time.Duration) []*AllocationInfo {
    tracker.mutex.RLock()
    defer tracker.mutex.RUnlock()
    
    var leaks []*AllocationInfo
    threshold := time.Now().Add(-maxAge)
    
    for _, info := range tracker.allocations {
        if info.Timestamp.Before(threshold) {
            leaks = append(leaks, info)
        }
    }
    
    return leaks
}
```

## 8. 配置与调优

### 8.1 分配器配置

```go
// 分配器配置
type AllocatorConfig struct {
    // Size Class配置
    EnableSmallAllocator  bool     // 启用小对象分配器
    SmallObjectThreshold  uint32   // 小对象阈值 (默认256字节)
    
    // Slab配置
    EnableSlabAllocator   bool     // 启用slab分配器
    SlabChunkSize         uint32   // slab chunk大小 (默认64KB)
    MediumObjectThreshold uint32   // 中等对象阈值 (默认4KB)
    
    // 大对象配置
    EnableDirectAllocator bool     // 启用直接分配器
    LargeObjectThreshold  uint32   // 大对象阈值 (默认4KB)
    
    // 性能优化
    EnableFastPath        bool     // 启用快速分配路径
    EnableBatchAllocation bool     // 启用批量分配
    BatchSize             int      // 批量分配大小
    
    // 调试选项
    EnableTracking        bool     // 启用分配追踪
    EnableLeakDetection   bool     // 启用内存泄漏检测
    VerboseLogging        bool     // 详细日志
}

// 默认配置
var DefaultAllocatorConfig = AllocatorConfig{
    EnableSmallAllocator:  true,
    SmallObjectThreshold:  256,
    
    EnableSlabAllocator:   true,
    SlabChunkSize:         64 * 1024, // 64KB
    MediumObjectThreshold: 4 * 1024,  // 4KB
    
    EnableDirectAllocator: true,
    LargeObjectThreshold:  4 * 1024,  // 4KB
    
    EnableFastPath:        true,
    EnableBatchAllocation: false, // MVP阶段暂时关闭
    BatchSize:             16,
    
    EnableTracking:        false,
    EnableLeakDetection:   false,
    VerboseLogging:        false,
}
```

### 8.2 自适应调优

```go
// 分配器自适应调优
type AllocatorTuner struct {
    allocator *SimpleAllocator
    config    *AllocatorConfig
    
    // 监控指标
    lastTuneTime     time.Time
    tuneInterval     time.Duration
    
    // 调优策略
    targetWasteRatio float64 // 目标浪费比例
    targetAllocTime  time.Duration // 目标分配时间
}

func (tuner *AllocatorTuner) AutoTune() {
    if time.Since(tuner.lastTuneTime) < tuner.tuneInterval {
        return
    }
    
    stats := tuner.allocator.Stats()
    
    // 根据碎片率调整策略
    if stats.FragmentationRatio > tuner.targetWasteRatio {
        // 碎片过多，触发压缩
        tuner.allocator.gcManager.TriggerCompaction()
    }
    
    // 根据分配时间调整快速路径
    if stats.AverageAllocTime > tuner.targetAllocTime {
        if !tuner.config.EnableBatchAllocation {
            tuner.config.EnableBatchAllocation = true
            tuner.allocator.Configure(tuner.config)
        }
    }
    
    tuner.lastTuneTime = time.Now()
}
```

## 9. 未来扩展计划

### 9.1 Phase 2: 优化分配器

**目标功能**：
- 线程本地缓存 (Thread Local Cache)
- 中心缓存池 (Central Pool)
- 更智能的size class选择
- 批量分配优化

**预期收益**：
- 分配性能提升2-3倍
- 锁争用减少80%
- 内存利用率提升到90%+

### 9.2 Phase 3: 高级分配器

**目标功能**：
- 内存压缩器 (Memory Compactor)
- 分配分析器 (Allocation Profiler)
- NUMA感知优化
- 自适应调优

**预期收益**：
- 长期运行碎片率<5%
- 自动性能调优
- 大规模多核扩展性

## 10. 总结

AQL内存分配器设计充分借鉴了Lua的简洁哲学和Go的高效策略，形成了适合AI服务编排场景的**渐进式三阶段架构**：

### 核心优势

1. **简洁实用**：8个size class vs Go的67个，降低复杂度
2. **缓存友好**：所有分配16字节对齐，对象大小是缓存行倍数
3. **GC协作**：分配器与双重GC策略深度集成
4. **渐进演进**：MVP先行，逐步添加优化功能

### 关键创新

1. **类型感知分配**：根据ObjectType优化分配策略
2. **统一对象头**：16字节GCObjectHeader，8字节对齐
3. **三级分配策略**：小对象FixedSize + 中等对象Slab + 大对象Direct
4. **智能GC触发**：基于分配压力自动触发增量GC

这个设计为AQL提供了**高性能**、**低碎片**、**GC友好**的内存分配基础，完美支持"像Python一样简洁，像C++一样高效，像Go一样内存安全"的设计目标。 