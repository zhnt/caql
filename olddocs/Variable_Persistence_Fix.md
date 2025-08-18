# AQL变量持久化修复总结

## 问题描述
REPL模式下变量无法在不同输入之间持久化，每次输入后之前定义的变量都会丢失。

### 现象
```aql
>> a = 3
3
>> b = 5  
5
>> a + b
错误: 编译错误: 编译错误: undefined variable: a
```

## 根本原因
在`cmd/aql/main.go`的`executeREPLLine`函数中，每次处理输入都创建新的编译器实例：

```go
// 问题代码
compiler := compiler1.New()  // 每次都创建新实例！
```

这导致：
1. 符号表每次都重置为空
2. 之前定义的变量信息丢失
3. REPL无法进行连续的交互式计算

## 解决方案
将编译器实例移到REPL会话级别，保持符号表状态：

### 修改1：在startREPL中创建持久编译器
```go
// 创建执行器
executor := vm.NewExecutor()

// 创建编译器实例（在整个REPL会话期间保持状态）
compiler := compiler1.New()
```

### 修改2：传递编译器实例
```go
// 执行代码
err := executeREPLLine(input, compiler, executor, out)
```

### 修改3：更新函数签名
```go
func executeREPLLine(input string, compiler *compiler1.Compiler, executor *vm.Executor, out io.Writer) error {
    // ...
    // 编译（使用持久的编译器实例）
    function, err := compiler.Compile(program)
    // ...
}
```

## 测试验证
修复后的REPL可以正确保持变量状态：

```aql
>> let total = 0
0
>> total = total + 10
10
>> total = total + 20
30
>> total
30
>> total * 2
60
```

## 技术细节
- **符号表持久化**：编译器实例的符号表在整个REPL会话期间保持
- **作用域管理**：全局变量正确存储在执行器的全局变量数组中
- **类型安全**：let语句和赋值语句都能正确维护变量状态

## 后续改进
1. 添加变量查看命令（如`.vars`）
2. 支持变量重新定义的警告
3. 实现变量作用域的可视化
4. 添加变量类型信息显示

## 相关文件
- `cmd/aql/main.go` - REPL主要逻辑
- `internal/compiler1/symbol_table.go` - 符号表管理
- `internal/vm/executor.go` - VM执行器和全局变量存储

这个修复为后续的函数编译支持奠定了基础，因为函数定义和调用同样需要符号表的持久化。 