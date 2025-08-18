# AQL寄存器池化优化详细设计

## 1. 函数大小分类：基于MaxStackSize

### 1.1 分类标准 (基于实际代码)

```go
// 基于internal/compiler1/compiler.go的实际实现
func (c *Compiler) calculateOptimalStackSize() int {
    baseSize := c.maxRegisters              // 实际使用的寄存器数
    safetyMargin := int(float64(baseSize) * 0.5)  // 50%安全余量
    optimalSize := baseSize + safetyMargin
    
    // 范围：256-8192个寄存器
    const minStackSize = 256
    const maxStackSize = 8192
    
    if optimalSize < minStackSize {
        optimalSize = minStackSize
    } else if optimalSize > maxStackSize {
        optimalSize = maxStackSize
    }
    
    return optimalSize
}
```

**函数大小分类标准**：
```go
// 基于实际MaxStackSize的函数分类
type FunctionSize int

const (
    SmallFunction  FunctionSize = iota  // 256-512 寄存器
    MediumFunction                      // 512-1024 寄存器
    LargeFunction                       // 1024-2048 寄存器
    HugeFunction                        // >2048 寄存器
)

// 分类函数
func classifyFunction(maxStackSize int) FunctionSize {
    switch {
    case maxStackSize <= 512:
        return SmallFunction
    case maxStackSize <= 1024:
        return MediumFunction
    case maxStackSize <= 2048:
        return LargeFunction
    default:
        return HugeFunction
    }
}
```

### 1.2 编译期确定寄存器使用上限

**是的，编译期能确定寄存器使用上限**：

```go
// 编译器在编译过程中跟踪寄存器使用
type Compiler struct {
    nextRegister int  // 下一个可用寄存器
    maxRegisters int  // 最大寄存器使用数 (确定的上限)
    
    // 寄存器管理
    freeRegisters []int  // 空闲寄存器池
    registerStack []int  // 寄存器栈
}

// 在分配寄存器时更新最大使用量
func (c *Compiler) allocateRegister() int {
    // ... 分配逻辑
    
    if c.nextRegister > c.maxRegisters {
        c.maxRegisters = c.nextRegister  // 更新最大使用量
    }
    
    return reg
}
```

**编译期能确定的信息**：
1. **精确的寄存器使用量**：通过静态分析确定
2. **函数调用深度**：通过递归分析确定
3. **局部变量数量**：通过符号表确定
4. **临时变量需求**：通过表达式复杂度确定

## 2. 寄存器池化实现

### 2.1 基于sync.Pool的实现

```go
// 寄存器池管理器
type RegisterPoolManager struct {
    // 分大小的池
    pools map[int]*sync.Pool
    
    // 统计信息
    stats *RegisterPoolStats
    
    // 配置
    config *RegisterPoolConfig
    
    // 保护并发访问
    mutex sync.RWMutex
}

type RegisterPoolStats struct {
    // 池命中率
    PoolHits   uint64
    PoolMisses uint64
    
    // 分配统计
    SmallAllocs  uint64
    MediumAllocs uint64
    LargeAllocs  uint64
    HugeAllocs   uint64
    
    // 重用统计
    ReuseRate float64
    
    // 内存节约
    MemorySaved uint64
}

type RegisterPoolConfig struct {
    // 预分配大小
    PreallocSizes []int
    
    // 池大小限制
    MaxPoolSize int
    
    // 清理策略
    CleanupInterval time.Duration
    
    // 启用统计
    EnableStats bool
}

// 全局寄存器池
var globalRegisterPool *RegisterPoolManager

func init() {
    globalRegisterPool = NewRegisterPoolManager(&RegisterPoolConfig{
        PreallocSizes: []int{16, 32, 64, 128, 256, 512, 1024},
        MaxPoolSize:   1000,
        CleanupInterval: 5 * time.Minute,
        EnableStats:   true,
    })
}

func NewRegisterPoolManager(config *RegisterPoolConfig) *RegisterPoolManager {
    rpm := &RegisterPoolManager{
        pools:  make(map[int]*sync.Pool),
        stats:  &RegisterPoolStats{},
        config: config,
    }
    
    // 为预定义大小创建池
    for _, size := range config.PreallocSizes {
        rpm.createPool(size)
    }
    
    return rpm
}

func (rpm *RegisterPoolManager) createPool(size int) {
    rpm.mutex.Lock()
    defer rpm.mutex.Unlock()
    
    pool := &sync.Pool{
        New: func() interface{} {
            registers := make([]ValueGC, size)
            // 初始化为nil值
            for i := range registers {
                registers[i] = NewNilValueGC()
            }
            return registers
        },
    }
    
    rpm.pools[size] = pool
}
```

### 2.2 智能大小匹配

```go
// 智能大小匹配算法
func (rpm *RegisterPoolManager) GetRegisters(requestedSize int) []ValueGC {
    // 1. 找到最佳匹配大小
    bestSize := rpm.findBestSize(requestedSize)
    
    // 2. 尝试从池中获取
    if pool, exists := rpm.pools[bestSize]; exists {
        if registers, ok := pool.Get().([]ValueGC); ok {
            // 池命中
            atomic.AddUint64(&rpm.stats.PoolHits, 1)
            
            // 如果大小不匹配，截取合适的部分
            if len(registers) > requestedSize {
                return registers[:requestedSize]
            }
            return registers
        }
    }
    
    // 3. 池未命中，创建新的
    atomic.AddUint64(&rpm.stats.PoolMisses, 1)
    
    registers := make([]ValueGC, requestedSize)
    for i := range registers {
        registers[i] = NewNilValueGC()
    }
    
    return registers
}

func (rpm *RegisterPoolManager) findBestSize(requestedSize int) int {
    // 找到最小的大于等于requestedSize的预分配大小
    for _, size := range rpm.config.PreallocSizes {
        if size >= requestedSize {
            return size
        }
    }
    
    // 如果没有预分配大小满足，返回请求大小
    return requestedSize
}

func (rpm *RegisterPoolManager) PutRegisters(registers []ValueGC) {
    if len(registers) == 0 {
        return
    }
    
    size := len(registers)
    
    // 清理寄存器内容
    for i := range registers {
        registers[i] = NewNilValueGC()
    }
    
    // 只有预分配大小才放回池中
    if pool, exists := rpm.pools[size]; exists {
        pool.Put(registers)
    }
    // 非标准大小的寄存器直接丢弃，让GC处理
}
```

## 3. 集成到NewStackFrame

### 3.1 优化后的NewStackFrame

```go
// 优化后的栈帧创建
func NewStackFrame(function *Function, caller *StackFrame, returnAddr int) *StackFrame {
    // 从池中获取寄存器
    registers := globalRegisterPool.GetRegisters(function.MaxStackSize)
    
    frame := &StackFrame{
        Function:     function,
        PC:           0,
        Registers:    registers,
        Base:         0,
        Caller:       caller,
        ReturnAddr:   returnAddr,
        ExpectedRets: 1,
        
        // 标记这个栈帧使用了池化寄存器
        usingPooledRegisters: true,
        originalRegisterSize: function.MaxStackSize,
    }
    
    // 寄存器已经在池中初始化为nil，无需重复初始化
    return frame
}

// 扩展StackFrame结构
type StackFrame struct {
    Function *Function
    PC       int
    Registers []ValueGC
    Base      int
    Caller    *StackFrame
    ReturnAddr int
    ExpectedRets int
    Upvalues  []*Upvalue
    
    // 池化支持
    usingPooledRegisters bool
    originalRegisterSize int
}
```

### 3.2 栈帧销毁优化

```go
// 栈帧销毁时的处理
func (frame *StackFrame) Destroy() {
    if frame.usingPooledRegisters && frame.Registers != nil {
        // 将寄存器返回到池中
        globalRegisterPool.PutRegisters(frame.Registers)
        frame.Registers = nil
    }
    
    // 清理其他资源
    frame.Function = nil
    frame.Caller = nil
    frame.Upvalues = nil
}

// 在执行器中调用
func (e *Executor) returnFromFunction() {
    if e.CurrentFrame != nil {
        caller := e.CurrentFrame.Caller
        e.CurrentFrame.Destroy()  // 销毁当前栈帧
        e.CurrentFrame = caller
        e.CallDepth--
    }
}
```

## 4. 高级优化策略

### 4.1 栈帧重用 + 寄存器复用

```go
// 栈帧池化
type StackFramePool struct {
    pools map[int]*sync.Pool  // 按函数大小分类的栈帧池
    mutex sync.RWMutex
}

var globalStackFramePool *StackFramePool

func init() {
    globalStackFramePool = &StackFramePool{
        pools: make(map[int]*sync.Pool),
    }
    
    // 为常用大小创建池
    for _, size := range []int{256, 512, 1024, 2048} {
        globalStackFramePool.createPool(size)
    }
}

func (sfp *StackFramePool) createPool(size int) {
    sfp.mutex.Lock()
    defer sfp.mutex.Unlock()
    
    pool := &sync.Pool{
        New: func() interface{} {
            return &StackFrame{
                Registers: make([]ValueGC, size),
                // 其他字段在使用时初始化
            }
        },
    }
    
    sfp.pools[size] = pool
}

// 优化的栈帧获取
func (sfp *StackFramePool) GetStackFrame(function *Function) *StackFrame {
    size := function.MaxStackSize
    bestSize := sfp.findBestSize(size)
    
    if pool, exists := sfp.pools[bestSize]; exists {
        if frame, ok := pool.Get().(*StackFrame); ok {
            // 重用栈帧
            frame.Function = function
            frame.PC = 0
            frame.Base = 0
            frame.ReturnAddr = -1
            frame.ExpectedRets = 1
            frame.Caller = nil
            frame.Upvalues = nil
            
            // 确保寄存器数组大小合适
            if len(frame.Registers) < size {
                frame.Registers = make([]ValueGC, size)
            } else {
                frame.Registers = frame.Registers[:size]
            }
            
            // 重置寄存器内容
            for i := range frame.Registers {
                frame.Registers[i] = NewNilValueGC()
            }
            
            return frame
        }
    }
    
    // 回退到原始方法
    return NewStackFrame(function, nil, -1)
}

func (sfp *StackFramePool) PutStackFrame(frame *StackFrame) {
    if frame == nil {
        return
    }
    
    size := len(frame.Registers)
    
    // 清理栈帧内容
    frame.Function = nil
    frame.Caller = nil
    frame.Upvalues = nil
    
    // 放回池中
    if pool, exists := sfp.pools[size]; exists {
        pool.Put(frame)
    }
}
```

### 4.2 预分配配合协程级内存管理

```go
// 协程级内存管理器
type CoroutineMemoryManager struct {
    // 每个协程的专用寄存器池
    registerPool *RegisterPoolManager
    
    // 栈帧池
    stackFramePool *StackFramePool
    
    // 内存统计
    stats *CoroutineMemoryStats
}

type CoroutineMemoryStats struct {
    TotalAllocations uint64
    TotalDeallocations uint64
    PeakMemoryUsage uint64
    CurrentMemoryUsage uint64
    ReuseRate float64
}

// 协程级内存管理
func NewCoroutineMemoryManager() *CoroutineMemoryManager {
    return &CoroutineMemoryManager{
        registerPool: NewRegisterPoolManager(&RegisterPoolConfig{
            PreallocSizes: []int{16, 32, 64, 128, 256},
            MaxPoolSize:   500,  // 协程级池更小
            EnableStats:   true,
        }),
        stackFramePool: NewStackFramePool(),
        stats: &CoroutineMemoryStats{},
    }
}

// 在执行器中使用协程级内存管理
type Executor struct {
    CurrentFrame *StackFrame
    MaxCallDepth int
    CallDepth    int
    Globals      []ValueGC
    
    // 协程级内存管理
    memoryManager *CoroutineMemoryManager
    
    // GC优化
    gcOptimizer *GCOptimizer
    enableGCOpt bool
}

func NewExecutorWithCoroutineMemory() *Executor {
    return &Executor{
        MaxCallDepth: 1000,
        Globals:      make([]ValueGC, 0, 256),
        memoryManager: NewCoroutineMemoryManager(),
        enableGCOpt:   true,
    }
}
```

## 5. 性能测试和监控

### 5.1 基准测试

```go
// 性能基准测试
func BenchmarkStackFrameCreation(b *testing.B) {
    function := &Function{
        Name:         "testFunc",
        MaxStackSize: 32,
    }
    
    b.Run("Original", func(b *testing.B) {
        for i := 0; i < b.N; i++ {
            frame := NewStackFrame(function, nil, -1)
            _ = frame
        }
    })
    
    b.Run("Pooled", func(b *testing.B) {
        for i := 0; i < b.N; i++ {
            frame := globalStackFramePool.GetStackFrame(function)
            globalStackFramePool.PutStackFrame(frame)
        }
    })
}

// 内存使用测试
func BenchmarkMemoryUsage(b *testing.B) {
    function := &Function{
        Name:         "testFunc",
        MaxStackSize: 256,
    }
    
    b.Run("WithoutPool", func(b *testing.B) {
        var frames []*StackFrame
        for i := 0; i < b.N; i++ {
            frame := NewStackFrame(function, nil, -1)
            frames = append(frames, frame)
        }
        
        // 防止编译器优化
        runtime.KeepAlive(frames)
    })
    
    b.Run("WithPool", func(b *testing.B) {
        for i := 0; i < b.N; i++ {
            frame := globalStackFramePool.GetStackFrame(function)
            globalStackFramePool.PutStackFrame(frame)
        }
    })
}
```

### 5.2 性能监控

```go
// 性能监控和报告
type PerformanceMonitor struct {
    // 寄存器池统计
    registerPoolStats *RegisterPoolStats
    
    // 栈帧池统计
    stackFramePoolStats *StackFramePoolStats
    
    // 内存使用统计
    memoryStats *MemoryStats
    
    // 监控间隔
    interval time.Duration
    
    // 停止信号
    stopChan chan struct{}
}

func (pm *PerformanceMonitor) Start() {
    ticker := time.NewTicker(pm.interval)
    defer ticker.Stop()
    
    for {
        select {
        case <-ticker.C:
            pm.collectStats()
            pm.reportStats()
        case <-pm.stopChan:
            return
        }
    }
}

func (pm *PerformanceMonitor) collectStats() {
    // 收集寄存器池统计
    pm.registerPoolStats = globalRegisterPool.GetStats()
    
    // 收集栈帧池统计
    pm.stackFramePoolStats = globalStackFramePool.GetStats()
    
    // 收集内存统计
    pm.memoryStats = collectMemoryStats()
}

func (pm *PerformanceMonitor) reportStats() {
    fmt.Printf("=== AQL Performance Report ===\n")
    fmt.Printf("Register Pool Hit Rate: %.2f%%\n", pm.registerPoolStats.HitRate())
    fmt.Printf("Stack Frame Pool Hit Rate: %.2f%%\n", pm.stackFramePoolStats.HitRate())
    fmt.Printf("Memory Usage: %d bytes\n", pm.memoryStats.CurrentUsage)
    fmt.Printf("Memory Saved: %d bytes\n", pm.registerPoolStats.MemorySaved)
}
```

## 6. 实施建议

### 6.1 实施顺序

1. **Phase 1**：实现基础寄存器池化
   - 实现 `RegisterPoolManager`
   - 集成到 `NewStackFrame`
   - 基本性能测试

2. **Phase 2**：优化和扩展
   - 实现栈帧池化
   - 添加性能监控
   - 优化池的配置

3. **Phase 3**：高级特性
   - 协程级内存管理
   - 自适应池大小
   - 高级性能分析

### 6.2 配置建议

```go
// 推荐的生产环境配置
var ProductionConfig = &RegisterPoolConfig{
    PreallocSizes: []int{16, 32, 64, 128, 256, 512, 1024},
    MaxPoolSize:   1000,
    CleanupInterval: 5 * time.Minute,
    EnableStats:   true,
}

// 推荐的开发环境配置
var DevelopmentConfig = &RegisterPoolConfig{
    PreallocSizes: []int{16, 32, 64, 128, 256},
    MaxPoolSize:   100,
    CleanupInterval: 1 * time.Minute,
    EnableStats:   true,
}
```

### 6.3 预期性能提升

基于理论分析和类似系统的经验：

- **函数调用开销**：减少60-80%
- **内存分配次数**：减少90%+
- **GC压力**：减少50-70%
- **整体性能**：提升20-40%

这个设计充分利用了AQL现有的架构，特别是编译期确定的 `MaxStackSize`，实现了高效的寄存器池化系统。 