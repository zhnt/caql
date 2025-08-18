# AQL架构设计文档

## 1. 项目概览

### 1.1 核心定位
AQL (Agent Query Language) 是专为AI服务编排设计的现代编程语言，采用极简设计理念，提供高性能的解释执行和编译执行双重架构。

### 1.2 设计原则
- **极简高效**：手写解析器，无外部依赖
- **内存管理**：创新的混合GC系统
- **双重执行**：解释执行（开发）+ 编译执行（生产）
- **AI友好**：原生异步和服务编排支持

## 2. 整体架构

### 2.1 简化系统架构图
```
┌─────────────────────────────────────────────────────────┐
│                  AQL源代码 (.aql)                       │
└─────────────────────┬───────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────┐
│               手写前端处理                              │
│  ┌─────────────┐  ┌─────────────┐                      │
│  │ Lexer       │  │ Parser      │                      │
│  │ (纯Go实现)   │  │ (递归下降)   │                      │
│  └─────────────┘  └─────────────┘                      │
└─────────────────────┬───────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────┐
│                  解释执行引擎                            │
│  ┌─────────────────────────────────────────────────────┐│
│  │                AQL虚拟机                            ││
│  │  ├─ 16字节Value系统                                ││
│  │  ├─ 轻量级栈帧                                     ││
│  │  ├─ 核心指令集                                     ││
│  │  └─ GC协作执行                                     ││
│  └─────────────────────────────────────────────────────┘│
└─────────────────────┬───────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────┐
│                双重GC系统                               │
│  ┌─────────────────┐  ┌─────────────────┐             │
│  │ 引用计数GC       │  │ 标记清除GC       │             │
│  │ ├─ 95%对象      │  │ ├─ 5%循环引用   │             │
│  │ ├─ 即时回收      │  │ ├─ 定期清理     │             │
│  │ └─ 零延迟       │  │ └─ 并发执行     │             │
│  └─────────────────┘  └─────────────────┘             │
└─────────────────────────────────────────────────────────┘

未来扩展:
┌─────────────────────────────────────────────────────────┐
│              编译执行引擎 (Phase 2)                      │
│  ┌─────────────┐  ┌─────────────┐                      │
│  │ 预编译优化   │  │ JIT编译     │                      │
│  │ (静态分析)   │  │ (热点优化)   │                      │
│  └─────────────┘  └─────────────┘                      │
└─────────────────────────────────────────────────────────┘
```

### 2.2 核心组件关系
```go
// 系统组件层次
AQL Runtime System
├── Frontend Processing
│   ├── Lexer (手写)
│   ├── Parser (递归下降)
│   └── Compiler (AST->Bytecode)
├── Execution Engines
│   ├── Interpreter Engine
│   │   ├── VM Core
│   │   ├── Stack Management
│   │   └── Instruction Execution
│   └── Compiler Engine
│       ├── IR Generation
│       ├── Optimization
│       └── Native Code Generation
└── Memory Management
    ├── Unified GC Manager
    ├── Reference Counting GC
    ├── Mark-Sweep GC
    ├── Generational GC
    └── Memory Allocator
```

## 3. GC系统设计 (核心创新)

### 3.1 混合GC架构

AQL采用创新的**三重混合GC系统**，结合不同GC算法的优势：

```go
// 统一GC管理器
type UnifiedGCManager struct {
    // 快速路径：引用计数
    refCountGC    *RefCountGC
    
    // 慢速路径：三色标记  
    markSweepGC   *MarkSweepGC
    
    // 分代管理
    generationalGC *GenerationalGC
    
    // 执行策略
    strategy      GCStrategy
    
    // 性能监控
    monitor       *GCMonitor
}
```

### 3.2 统一Value系统设计

**核心Value结构**：
```go
// 16字节紧凑Value设计
type Value struct {
    typeTag uint8      // 类型标签 (1字节)
    flags   uint8      // 标志位 (1字节)
    data    [14]byte   // 统一数据存储 (14字节)
}

// 类型标签定义
const (
    TypeTagNil = iota
    TypeTagBool
    TypeTagSmallInt    // 64位整数，内联存储
    TypeTagFloat       // 32位浮点，内联存储
    TypeTagDouble      // 64位浮点，内联存储
    TypeTagString      // 字符串，GC管理
    TypeTagArray       // 数组，GC管理
    TypeTagObject      // 对象，GC管理
    TypeTagFunction    // 函数，GC管理
    TypeTagBigInt      // 大整数，GC管理
    TypeTagStruct      // 自定义结构体，GC管理
)

// 标志位定义
const (
    FlagInline   = 1 << 0 // 内联存储
    FlagMarked   = 1 << 1 // GC标记位
    FlagConst    = 1 << 2 // 常量标记
    FlagWeakRef  = 1 << 3 // 弱引用
    FlagImmutable= 1 << 4 // 不可变对象
)
```

**内联存储策略**：
```go
// 类型具体存储方式
类型        | 存储位置           | 数据布局
----------|-------------------|------------------
Nil       | 仅类型标签         | 无数据
Bool      | data[0]           | 1字节布尔值
SmallInt  | data[0:8]         | 8字节64位整数
Float     | data[0:4]         | 4字节32位浮点
Double    | data[0:8]         | 8字节64位浮点
String    | data[0:8]+data[8:12] | 指针(8字节)+长度(4字节)
Array     | data[0:8]+data[8:12] | 指针(8字节)+长度(4字节)
Object    | data[0:8]         | 指针(8字节)
```

**GC对象头设计**：
```go
// 优化的GC对象头（16字节，8字节对齐）
type GCObjectHeader struct {
    RefCount      uint32    // 引用计数 (4字节)
    Size          uint32    // 对象大小 (4字节)
    ExtendedType  uint16    // 扩展类型ID (2字节，支持65K+用户类型)
    ObjectType    uint8     // 基础对象类型 (1字节)
    Flags         uint8     // 状态标志 (1字节)
    Reserved      uint32    // 保留字段 (4字节，未来扩展用)
} // 总计16字节，缓存友好，8字节对齐

// GC对象标志
const (
    GCFlagMarked     = 1 << 0  // 标记-清除标记
    GCFlagCyclic     = 1 << 1  // 可能循环引用
    GCFlagFinalizer  = 1 << 2  // 需要finalizer
    GCFlagWeakRef    = 1 << 3  // 被弱引用指向
)

// 扩展类型ID范围分配
const (
    ExtTypeSystemStart = 1      // 系统类型起始ID (1-100)
    ExtTypeUserFunc    = 101    // 用户函数起始ID (101-500)
    ExtTypeUserClass   = 501    // 用户类起始ID (501-65535)
)
```

### 3.3 双重GC策略（简化设计）

**策略1：引用计数GC（主要策略）**
- **适用对象**：95%的常规对象
- **回收时机**：引用计数归零立即回收
- **优势**：确定性回收，零延迟，实现简单
- **处理范围**：无循环引用的对象

```go
type RefCountGC struct {
    allocator    *ObjectAllocator
    freeList     *FreeList
    finalizers   map[*GCObject]func()
}

func (gc *RefCountGC) DecRef(obj *GCObject) {
    newCount := atomic.AddUint32(&obj.Header.RefCount, ^uint32(0))
    
    if newCount == 0 {
        if obj.Header.Flags & GCFlagCyclic == 0 {
            // 快速路径：无循环引用，立即回收
            gc.deallocateImmediate(obj)
        } else {
            // 慢速路径：可能有循环引用，交给标记清除
            gc.markSweepGC.AddCandidate(obj)
        }
    }
}

func (gc *RefCountGC) deallocateImmediate(obj *GCObject) {
    // 1. 执行finalizer
    if finalizer, exists := gc.finalizers[obj]; exists {
        finalizer()
        delete(gc.finalizers, obj)
    }
    
    // 2. 递减子对象引用计数
    gc.decrementChildren(obj)
    
    // 3. 回收内存
    gc.freeList.Add(obj)
}
```

**策略2：标记清除GC（辅助策略）**
- **适用对象**：5%的复杂对象（循环引用）
- **回收时机**：内存压力触发或定期执行
- **优势**：完整性保证，处理循环引用
- **执行模式**：增量并发执行

```go
type MarkSweepGC struct {
    candidates   []*GCObject     // 循环引用候选对象
    greyQueue    *Queue          // 灰色对象队列
    isMarking    atomic.Bool     // 是否正在标记
}

func (gc *MarkSweepGC) ConcurrentCollection() {
    // 1. 短暂STW：初始标记
    runtime.StopTheWorld("gc mark start")
    gc.markRoots()
    gc.isMarking.Store(true)
    runtime.StartTheWorld()
    
    // 2. 并发标记：与程序并行
    for !gc.greyQueue.Empty() {
        obj := gc.greyQueue.Pop()
        gc.markChildren(obj)
    }
    
    // 3. 短暂STW：最终清理
    runtime.StopTheWorld("gc mark end")
    gc.sweep()
    gc.isMarking.Store(false)
    runtime.StartTheWorld()
}
```

**统一GC管理器**：
```go
type UnifiedGCManager struct {
    refCountGC   *RefCountGC
    markSweepGC  *MarkSweepGC
    allocator    *ObjectAllocator
    
    // 性能统计
    stats        GCStats
    
    // 配置参数
    config       GCConfig
}

func (gc *UnifiedGCManager) Allocate(size int, objType uint8) *GCObject {
    obj := gc.allocator.Allocate(size)
    obj.Header.ObjectType = objType
    obj.Header.RefCount = 1
    
    // 根据对象类型设置循环引用标志
    if gc.mightHaveCycles(objType) {
        obj.Header.Flags |= GCFlagCyclic
    }
    
    return obj
}
```

### 3.4 GC性能优化

**写屏障优化**：
```go
func (gc *UnifiedGCManager) WriteBarrier(objID uint64, fieldOffset int, newValue Value) {
    if gc.isMarking.Load() {
        // Dijkstra写屏障：标记新值
        if newValue.IsGCObject() {
            gc.markSweepGC.MarkGrey(newValue.ObjectID())
        }
        
        // 记录修改，用于重新标记
        gc.recordModification(objID, fieldOffset)
    }
    
    // 分代写屏障：老年代引用年轻代
    if gc.isOldGenObject(objID) && gc.isYoungGenObject(newValue.ObjectID()) {
        gc.generationalGC.RecordCrossGenReference(objID, newValue.ObjectID())
    }
    
    // 执行实际修改
    gc.setField(objID, fieldOffset, newValue)
}
```

**内存分配器集成**：
```go
type AQLAllocator struct {
    // 分级分配器
    smallAllocator   *FixedSizeAllocator    // <256字节
    mediumAllocator  *SlabAllocator         // 256B-4KB
    largeAllocator   *DirectAllocator       // >4KB
    
    // GC集成
    gcManager        *UnifiedGCManager
    
    // 性能优化
    allocationSites  map[uintptr]*AllocationProfile
    threadLocalCache []*ThreadLocalHeap
}
```

### 3.4 内存布局优化

**Value内存对齐**：
```go
// 确保Value结构体缓存友好
type Value struct {
    typeTag uint8      // 1字节
    flags   uint8      // 1字节
    data    [14]byte   // 14字节
} // 总计16字节，正好一个缓存行的一半

// 小对象优化：内联字段存储
type SmallStruct struct {
    header  GCObjectHeader     // 16字节，8字节对齐
    fields  [3]Value          // 48字节，共64字节（缓存友好）
}

// 大对象：分离字段存储
type LargeStruct struct {
    header    GCObjectHeader   // 16字节，8字节对齐
    fieldCount uint32         // 4字节
    fieldsPtr  *Value         // 8字节，指向字段数组
    _         uint32          // 4字节填充，保持8字节对齐
}
```

**字符串内存优化**：
```go
// 短字符串内联存储（≤13字节）
func NewShortString(s string) Value {
    if len(s) <= 13 {
        var v Value
        v.typeTag = TypeTagString
        v.flags = FlagInline
        copy(v.data[0:1], []byte{uint8(len(s))})  // 长度
        copy(v.data[1:], []byte(s))               // 内容
        return v
    }
    return NewLongString(s)  // 堆分配
}

// 长字符串堆存储
type StringObject struct {
    header  GCObjectHeader   // 16字节，8字节对齐
    length  uint32          // 4字节，字符串长度
    _       uint32          // 4字节填充，保持8字节对齐
    data    []byte          // 字符串数据（slice header 24字节，8字节对齐）
}
```

## 4. 简化执行引擎

### 4.1 统一VM架构

**核心VM设计**：
```go
type AQLVirtualMachine struct {
    // 执行状态
    currentFrame    *StackFrame
    callStack       []*StackFrame
    
    // 指令系统
    instructions    []Instruction
    
    // GC集成
    gcManager       *UnifiedGCManager
    
    // 执行模式
    mode           ExecutionMode
    
    // 性能监控
    profiler       *Profiler
}

// 执行模式简化
type ExecutionMode int
const (
    ModeInterpreted ExecutionMode = iota  // 解释执行
    ModeCompiled                          // 预编译执行（未来）
)
```

**核心指令集**：
```go
// MVP阶段核心指令
const (
    // 基础数据操作
    OP_CONST OpCode = iota    // 加载常量
    OP_MOVE                   // 寄存器间移动
    OP_COPY                   // 值复制（处理GC）
    
    // 算术运算
    OP_ADD, OP_SUB, OP_MUL, OP_DIV
    OP_MOD, OP_NEG
    
    // 比较运算
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE
    
    // 逻辑运算
    OP_AND, OP_OR, OP_NOT
    
    // 控制流
    OP_JMP                    // 无条件跳转
    OP_JMPIF                  // 条件跳转
    OP_CALL                   // 函数调用
    OP_RETURN                 // 函数返回
    
    // 对象操作
    OP_NEW_ARRAY              // 创建数组
    OP_NEW_OBJECT             // 创建对象
    OP_GET_FIELD              // 获取字段
    OP_SET_FIELD              // 设置字段
    OP_GET_INDEX              // 数组索引
    OP_SET_INDEX              // 数组赋值
    
    // GC相关
    OP_GC_WRITE_BARRIER       // 写屏障
    OP_GC_INC_REF            // 增加引用计数
    OP_GC_DEC_REF            // 减少引用计数
    
    // 异步操作（未来扩展）
    OP_ASYNC_CALL            // 异步调用
    OP_AWAIT                 // await操作
)
```

### 4.2 栈帧设计

**轻量级栈帧**：
```go
type StackFrame struct {
    // 函数信息
    function    *Function
    pc          uint32        // 程序计数器
    
    // 寄存器区域
    registers   []Value       // 局部变量+临时值
    
    // 链接信息
    parent      *StackFrame
    
    // GC支持
    gcRoots     []int        // 需要GC扫描的寄存器索引
}

// 栈帧池化避免频繁分配
type StackFramePool struct {
    pool    sync.Pool
    maxSize int
}
```

### 4.3 执行优化策略

**固定策略代替动态切换**：
```go
type SimpleExecutor struct {
    vm          *AQLVirtualMachine
    mode        ExecutionMode
    
    // 预编译缓存（未来）
    compiled    map[string]*CompiledCode
}

func (e *SimpleExecutor) Execute(code *AQLProgram) Value {
    // MVP: 纯解释执行
    if e.mode == ModeInterpreted {
        return e.vm.InterpretedRun(code.Instructions)
    }
    
    // 未来: 预编译执行
    if e.mode == ModeCompiled {
        if compiled, exists := e.compiled[code.Hash]; exists {
            return compiled.Run()
        }
        // 编译后缓存
        compiled := e.compile(code)
        e.compiled[code.Hash] = compiled
        return compiled.Run()
    }
    
    panic("unsupported execution mode")
}

```

## 5. 性能目标和扩展计划

### 5.1 MVP性能目标

**现实化指标**：
```
GC性能：
- 95%的GC暂停 < 1ms
- 99%的GC暂停 < 5ms  
- GC开销 < 8%的CPU时间
- 内存开销 < 15%额外空间

执行性能：
- 启动时间：< 50ms
- 解释执行：比Python快2-3倍
- 内存使用：比Node.js少20%
- 函数调用：< 200ns开销

内存效率：
- Value系统：16字节紧凑设计
- 对象开销：< 25%额外开销
- 堆利用率：> 80%
```

### 5.2 未来扩展计划

**Phase 1: 核心GC和VM (当前)**
- 双重GC系统实现
- 解释执行引擎
- 基础Value系统
- 手写前端处理

**Phase 2: 编译执行 (6个月后)**
- 预编译优化
- 静态分析
- 死代码消除
- 常量折叠

**Phase 3: JIT优化 (12个月后)**
- 热点检测
- 动态编译
- 内联优化
- 类型特化

**Phase 4: 并发和异步 (18个月后)**
- 协程系统
- 异步I/O
- 并发GC
- AI服务集成

## 6. 核心设计原则

### 6.1 简化优于复杂
- **双重GC**代替三重GC，降低复杂度
- **固定策略**代替动态切换，提高可预测性  
- **MVP优先**，避免过度工程化

### 6.2 性能与维护性平衡
- **内联存储**最大化，减少堆分配
- **缓存友好**的内存布局设计
- **GC协作**的指令设计

### 6.3 渐进式演进
- **核心先行**：GC和VM优先
- **功能递增**：逐步添加高级特性
- **向后兼容**：保持API稳定性

## 7. 总结

AQL简化架构的核心特点：

1. **务实的GC系统**：双重策略，95%引用计数 + 5%标记清除
2. **统一Value设计**：16字节紧凑结构，内联存储优化
3. **简化执行引擎**：MVP解释执行，未来扩展编译
4. **内存布局优化**：缓存友好，对象头紧凑
5. **渐进式发展**：核心稳定，功能递增

这个简化架构将为AQL提供**稳定可靠**的内存管理和**高效简洁**的执行性能，完美匹配"像Python一样简洁，像C++一样高效，像Go一样内存安全"的设计目标。 