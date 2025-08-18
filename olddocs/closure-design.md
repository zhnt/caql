## AQL闭包系统设计文档 v3.0
## 极简设计 - 学习Lua的精简哲学

## 概述

AQL闭包系统采用**Lua式极简设计**：
- **统一Callable**：函数和闭包使用同一个结构
- **简单Upvalue**：仿照Lua的upvalue机制
- **最小编译器扩展**：在现有Compiler基础上最小化扩展
- **精简文件组织**：避免过度抽象

### 设计原则
1. **Simple is Beautiful**：优先选择简单方案
2. **Lua-like Design**：学习Lua的成功经验
3. **Minimal Extension**：最小化对现有代码的修改
4. **Progressive Enhancement**：功能可以渐进添加

## 核心设计（极简版）

### 1. 统一的Callable结构
```go
// 单一的Callable结构（类似Lua的Closure）
type Callable struct {
    Function *vm.Function    // 函数对象（复用现有）
    Upvalues []*Upvalue     // upvalue数组（可为空）
    
    // 可选的优化信息
    IsInlinable bool        // 是否可内联
    CallCount   int32       // 调用计数（用于JIT决策）
}

// 创建普通函数（无upvalue）
func NewFunction(function *vm.Function) *Callable {
    return &Callable{
        Function: function,
        Upvalues: nil,  // 普通函数无upvalue
    }
}

// 创建闭包（有upvalue）
func NewClosure(function *vm.Function, upvalues []*Upvalue) *Callable {
    return &Callable{
        Function: function,
        Upvalues: upvalues,
    }
}

// 统一的调用接口
func (c *Callable) Call(executor *vm.Executor, args []vm.ValueGC) (vm.ValueGC, error) {
    if len(c.Upvalues) == 0 {
        // 普通函数调用
        return executor.CallFunction(c.Function, args)
    } else {
        // 闭包调用
        return executor.CallClosure(c.Function, c.Upvalues, args)
    }
}
```

### 2. 简化的Upvalue（学习Lua）
```go
// Upvalue 结构（简化版，类似Lua）
type Upvalue struct {
    // 值存储（二选一）
    Stack *vm.ValueGC  // 指向栈变量（开放状态）
    Value vm.ValueGC   // 堆上值（关闭状态）
    
    // 状态
    IsClosed bool      // 是否已关闭到堆
    
    // 调试信息（可选）
    Name string        // 变量名（仅用于调试）
}

// Upvalue操作（极简API）
func (uv *Upvalue) Get() vm.ValueGC {
    if uv.IsClosed {
        return uv.Value
    }
    return *uv.Stack
}

func (uv *Upvalue) Set(value vm.ValueGC) {
    if uv.IsClosed {
        uv.Value = value
    } else {
        *uv.Stack = value
    }
}

func (uv *Upvalue) Close() {
    if !uv.IsClosed {
        uv.Value = *uv.Stack
        uv.Stack = nil
        uv.IsClosed = true
    }
}
```

### 3. 最小编译器扩展
```go
// 扩展现有的 compiler1.Compiler（最小修改）
// 在 internal/compiler1/compiler.go 中添加

// 编译函数文字时检测自由变量
func (c *Compiler) compileFunctionLiteral(expr *parser1.FunctionLiteral) (int, error) {
    // ... 现有代码保持不变 ...
    
    // 检查自由变量
    freeVars := c.symbolTable.FreeSymbols
    
    if len(freeVars) == 0 {
        // 普通函数
        functionValue := vm.NewFunctionValueGCFromID(functionID)
        constIndex := c.addConstant(functionValue)
        reg := c.allocateRegister()
        c.emit(vm.OP_LOADK, reg, constIndex)
        return reg, nil
    } else {
        // 闭包：生成MAKE_CLOSURE指令
        return c.emitMakeClosureInstructions(functionID, freeVars)
    }
}

// 生成创建闭包的指令
func (c *Compiler) emitMakeClosureInstructions(functionID int, freeVars []*Symbol) (int, error) {
    // 1. 加载函数
    functionValue := vm.NewFunctionValueGCFromID(functionID)
    constIndex := c.addConstant(functionValue)
    funcReg := c.allocateRegister()
    c.emit(vm.OP_LOADK, funcReg, constIndex)
    
    // 2. 加载捕获变量
    for i, freeVar := range freeVars {
        captureReg := c.allocateRegister()
        if freeVar.Scope == GLOBAL_SCOPE {
            c.emit(vm.OP_GET_GLOBAL, captureReg, freeVar.Index)
        } else {
            c.emit(vm.OP_GET_LOCAL, captureReg, freeVar.Index)
        }
        
        // 移动到连续位置
        expectedReg := funcReg + 1 + i
        if captureReg != expectedReg {
            c.emit(vm.OP_MOVE, expectedReg, captureReg, 0)
        }
    }
    
    // 3. 创建闭包
    closureReg := c.allocateRegister()
    c.emit(vm.OP_MAKE_CLOSURE, closureReg, funcReg, len(freeVars))
    
    return closureReg, nil
}
```

### 4. VM执行扩展（最小修改）
```go
// 在 internal/vm/executor.go 中扩展

// 扩展现有的executeCall支持闭包
func (e *Executor) executeCall(inst Instruction) error {
    frame := e.CurrentFrame
    funcValue := frame.GetRegister(inst.A)
    
    // 统一处理函数和闭包
    var callable *Callable
    
    if funcValue.IsFunction() {
        // 普通函数
        function := funcValue.AsFunction().(*Function)
        callable = NewFunction(function)
    } else if funcValue.IsClosure() {
        // 现有的闭包处理逻辑
        closure := funcValue.AsClosure()
        callable = &Callable{
            Function: closure.Function,
            Upvalues: convertMapToUpvalues(closure.Captures), // 转换现有格式
        }
    } else {
        return fmt.Errorf("not callable: %T", funcValue)
    }
    
    // 统一调用
    return e.callCallable(callable, inst)
}

// 新的统一调用方法
func (e *Executor) callCallable(callable *Callable, inst Instruction) error {
    // ... 获取参数，创建栈帧等现有逻辑 ...
    
    newFrame := NewStackFrame(callable.Function, frame, frame.PC+1)
    newFrame.SetParameters(args)
    
    // 设置upvalue环境
    if callable.Upvalues != nil {
        newFrame.Upvalues = callable.Upvalues
    }
    
    e.CurrentFrame = newFrame
    e.CallDepth++
    
    return nil
}

// 扩展栈帧支持upvalue
func (sf *StackFrame) GetUpvalue(index int) *Upvalue {
    if sf.Upvalues == nil || index >= len(sf.Upvalues) {
        return nil
    }
    return sf.Upvalues[index]
}
```

## 极简文件组织

### 文件映射（最少文件）
```go
// 方案：只添加3个文件，最小扩展

// 1. internal/vm/callable.go - 统一的Callable和Upvalue
package vm

type Callable struct { /* ... */ }
type Upvalue struct { /* ... */ }

// 2. internal/compiler1/closure.go - 编译器闭包扩展
package compiler1

func (c *Compiler) emitMakeClosureInstructions(...) { /* ... */ }
func (c *Compiler) analyzeFreeVariables(...) { /* ... */ }

// 3. internal/vm/upvalue_instructions.go - upvalue相关指令
package vm

func (e *Executor) executeGetUpvalue(inst Instruction) error { /* ... */ }
func (e *Executor) executeSetUpvalue(inst Instruction) error { /* ... */ }
func (e *Executor) executeCloseUpvalue(inst Instruction) error { /* ... */ }
```

### 指令扩展（仅添加必要指令）
```go
// 在现有的 internal/vm/function.go 中添加
const (
    // ... 现有指令 ...
    
    // 仅添加4个闭包指令
    OP_MAKE_CLOSURE  // 创建闭包
    OP_GET_UPVALUE   // 获取upvalue
    OP_SET_UPVALUE   // 设置upvalue
    OP_CLOSE_UPVALUE // 关闭upvalue（栈帧销毁时）
)
```

## 优化策略（可选，渐进添加）

### 简单的编译时优化
```go
// internal/compiler1/simple_optimizer.go（可选文件）
package compiler1

// 简单的内联决策
func (c *Compiler) shouldInline(function *Function) bool {
    return len(function.Instructions) <= 10 && // 小函数
           c.symbolTable.FreeSymbols == nil     // 无捕获变量
}

// 简单的栈分配决策
func (c *Compiler) shouldStackAlloc(freeVars []*Symbol) bool {
    // 如果所有自由变量都是局部的，可以考虑栈分配
    for _, freeVar := range freeVars {
        if freeVar.Scope == GLOBAL_SCOPE {
            return false
        }
    }
    return true
}
```

### 运行时优化（可选）
```go
// internal/vm/jit_hints.go（可选文件）
package vm

// 简单的JIT提示
type JITHints struct {
    HotFunctions map[int]int  // 函数ID -> 调用次数
    Threshold    int          // JIT编译阈值
}

func (hints *JITHints) RecordCall(functionID int) bool {
    hints.HotFunctions[functionID]++
    return hints.HotFunctions[functionID] >= hints.Threshold
}
```

## 最终的极简架构

### 文件组织（仅5个新文件）
```
internal/
├── compiler1/              # 现有前端编译器
│   ├── compiler.go         # 现有（稍作扩展）
│   ├── symbol_table.go     # 现有（稍作扩展）
│   └── closure.go          # 新增：闭包编译支持
└── vm/                     # 现有VM
    ├── executor.go         # 现有（稍作扩展）
    ├── function.go         # 现有（添加新指令）
    ├── callable.go         # 新增：Callable和Upvalue
    ├── upvalue_ops.go      # 新增：upvalue指令实现
    └── simple_optimizer.go # 可选：简单优化
```

### 使用方式（极简）
```go
// 现有代码完全不变
compiler := compiler1.New()
function, err := compiler.Compile(program)

// 新的闭包支持自动生效，无需额外代码
// 编译器自动检测闭包并生成正确的指令
```

### 性能预期
- **零开销抽象**：普通函数调用性能不受影响
- **最小闭包开销**：仅在需要时创建upvalue
- **简单优化**：基本的内联和栈分配优化

这个极简设计的特点：
1. **文件最少**：只添加5个文件
2. **抽象最少**：只有Callable和Upvalue两个核心概念  
3. **修改最少**：对现有代码的修改降到最低
4. **功能完整**：支持完整的闭包语义

就像Lua一样：**简单、强大、高效**！ 