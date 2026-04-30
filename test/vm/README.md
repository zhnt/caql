# AQL VM 集成测试框架设计文档

## 概述

AQL VM集成测试框架旨在系统性地测试`avm_core.c`的所有功能，确保VM核心行为对齐 Lua 5.5.1，同时覆盖 AQL 容器等显式扩展。

## 目录结构

```
test/vm/
├── README.md                    # 测试框架说明文档
├── vmtest_runner.c             # 测试运行器（已完成）
├── bytecode/                   # 字节码测试用例（重命名）
│   ├── basic/                  # 基础指令测试
│   │   ├── load_store/         # 加载存储指令
│   │   │   ├── loadi.by        # LOADI指令测试
│   │   │   ├── loadi.expected  # 期望结果
│   │   │   ├── loadf.by        # LOADF指令测试
│   │   │   ├── loadf.expected
│   │   │   ├── loadk.by        # LOADK指令测试
│   │   │   ├── loadk.expected
│   │   │   ├── loadtrue.by     # 布尔值加载
│   │   │   ├── loadtrue.expected
│   │   │   ├── loadfalse.by
│   │   │   ├── loadfalse.expected
│   │   │   ├── loadnil.by      # nil值加载
│   │   │   ├── loadnil.expected
│   │   │   └── move.by         # MOVE指令测试
│   │   │   └── move.expected
│   │   ├── arithmetic/         # 算术运算指令
│   │   │   ├── add.by          # 加法运算
│   │   │   ├── add.expected
│   │   │   ├── sub.by          # 减法运算
│   │   │   ├── sub.expected
│   │   │   ├── mul.by          # 乘法运算
│   │   │   ├── mul.expected
│   │   │   ├── div.by          # 除法运算
│   │   │   ├── div.expected
│   │   │   ├── mod.by          # 取模运算
│   │   │   ├── mod.expected
│   │   │   ├── pow.by          # 幂运算
│   │   │   ├── pow.expected
│   │   │   ├── unm.by          # 取负运算
│   │   │   ├── unm.expected
│   │   │   ├── addi.by         # 立即数加法
│   │   │   ├── addi.expected
│   │   │   ├── addk.by         # 常量加法
│   │   │   └── addk.expected
│   │   ├── bitwise/            # 位运算指令
│   │   │   ├── band.by         # 按位与
│   │   │   ├── band.expected
│   │   │   ├── bor.by          # 按位或
│   │   │   ├── bor.expected
│   │   │   ├── bxor.by         # 按位异或
│   │   │   ├── bxor.expected
│   │   │   ├── bnot.by         # 按位取反
│   │   │   ├── bnot.expected
│   │   │   ├── shl.by          # 左移
│   │   │   ├── shl.expected
│   │   │   ├── shr.by          # 右移
│   │   │   └── shr.expected
│   │   ├── comparison/         # 比较指令
│   │   │   ├── eq.by           # 等于比较
│   │   │   ├── eq.expected
│   │   │   ├── lt.by           # 小于比较
│   │   │   ├── lt.expected
│   │   │   ├── le.by           # 小于等于比较
│   │   │   ├── le.expected
│   │   │   ├── eqi.by          # 立即数等于比较
│   │   │   ├── eqi.expected
│   │   │   ├── lti.by          # 立即数小于比较
│   │   │   ├── lti.expected
│   │   │   ├── test.by         # 条件测试
│   │   │   ├── test.expected
│   │   │   ├── testset.by      # 条件设置
│   │   │   └── testset.expected
│   │   ├── control_flow/       # 控制流指令
│   │   │   ├── jmp.by          # 无条件跳转
│   │   │   ├── jmp.expected
│   │   │   ├── conditional_jump.by  # 条件跳转
│   │   │   ├── conditional_jump.expected
│   │   │   ├── loop_simple.by  # 简单循环
│   │   │   ├── loop_simple.expected
│   │   │   ├── forloop.by      # for循环
│   │   │   ├── forloop.expected
│   │   │   ├── forprep.by      # for循环准备
│   │   │   └── forprep.expected
│   │   └── return/             # 返回指令
│   │       ├── return0.by      # 无返回值
│   │       ├── return0.expected
│   │       ├── return1.by      # 单返回值
│   │       ├── return1.expected
│   │       ├── return_multi.by # 多返回值
│   │       └── return_multi.expected
│   ├── intermediate/           # 中级功能测试
│   │   ├── tables/             # 表操作
│   │   │   ├── newtable.by     # 创建表
│   │   │   ├── newtable.expected
│   │   │   ├── gettable.by     # 表访问
│   │   │   ├── gettable.expected
│   │   │   ├── settable.by     # 表设置
│   │   │   ├── settable.expected
│   │   │   ├── geti.by         # 整数索引访问
│   │   │   ├── geti.expected
│   │   │   ├── seti.by         # 整数索引设置
│   │   │   ├── seti.expected
│   │   │   ├── getfield.by     # 字段访问
│   │   │   ├── getfield.expected
│   │   │   ├── setfield.by     # 字段设置
│   │   │   ├── setfield.expected
│   │   │   ├── setlist.by      # 列表设置
│   │   │   ├── setlist.expected
│   │   │   ├── len.by          # 长度操作
│   │   │   └── len.expected
│   │   ├── strings/            # 字符串操作
│   │   │   ├── concat.by       # 字符串连接
│   │   │   ├── concat.expected
│   │   │   ├── string_len.by   # 字符串长度
│   │   │   ├── string_len.expected
│   │   │   ├── string_compare.by # 字符串比较
│   │   │   └── string_compare.expected
│   │   ├── functions/          # 函数操作
│   │   │   ├── call_simple.by  # 简单函数调用
│   │   │   ├── call_simple.expected
│   │   │   ├── call_multi_args.by # 多参数调用
│   │   │   ├── call_multi_args.expected
│   │   │   ├── call_multi_ret.by  # 多返回值调用
│   │   │   ├── call_multi_ret.expected
│   │   │   ├── tailcall.by     # 尾调用
│   │   │   ├── tailcall.expected
│   │   │   ├── vararg.by       # 可变参数
│   │   │   ├── vararg.expected
│   │   │   ├── closure.by      # 闭包创建
│   │   │   └── closure.expected
│   │   └── upvalues/           # 上值操作
│   │       ├── getupval.by     # 获取上值
│   │       ├── getupval.expected
│   │       ├── setupval.by     # 设置上值
│   │       ├── setupval.expected
│   │       ├── close.by        # 关闭上值
│   │       ├── close.expected
│   │       ├── tbc.by          # 待关闭变量
│   │       └── tbc.expected
│   ├── advanced/               # 高级功能测试
│   │   ├── metamethods/        # 元方法
│   │   │   ├── mmbin.by        # 二元元方法
│   │   │   ├── mmbin.expected
│   │   │   ├── mmbini.by       # 立即数二元元方法
│   │   │   ├── mmbini.expected
│   │   │   ├── mmbink.by       # 常量二元元方法
│   │   │   └── mmbink.expected
│   │   ├── iterators/          # 迭代器
│   │   │   ├── tforprep.by     # 迭代器准备
│   │   │   ├── tforprep.expected
│   │   │   ├── tforcall.by     # 迭代器调用
│   │   │   ├── tforcall.expected
│   │   │   ├── tforloop.by     # 迭代器循环
│   │   │   └── tforloop.expected
│   │   └── gc/                 # 垃圾回收相关
│   │       ├── gc_simple.by    # 简单GC测试
│   │       ├── gc_simple.expected
│   │       ├── gc_upval.by     # 上值GC测试
│   │       └── gc_upval.expected
│   ├── aql_extensions/         # AQL特有功能
│   │   ├── containers/         # 容器操作
│   │   │   ├── newobject.by    # 创建对象
│   │   │   ├── newobject.expected
│   │   │   ├── getprop.by      # 属性获取
│   │   │   ├── getprop.expected
│   │   │   ├── setprop.by      # 属性设置
│   │   │   ├── setprop.expected
│   │   │   ├── invoke.by       # 方法调用
│   │   │   └── invoke.expected
│   │   ├── builtins/           # 内置函数
│   │   │   ├── builtin_math.by # 数学函数
│   │   │   ├── builtin_math.expected
│   │   │   ├── builtin_string.by # 字符串函数
│   │   │   ├── builtin_string.expected
│   │   │   ├── builtin_table.by  # 表函数
│   │   │   └── builtin_table.expected
│   │   └── coroutines/         # 协程
│   │       ├── yield.by        # yield操作
│   │       ├── yield.expected
│   │       ├── resume.by       # resume操作
│   │       └── resume.expected
│   ├── integration/            # 集成测试
│   │   ├── complex_expr.by     # 复杂表达式
│   │   ├── complex_expr.expected
│   │   ├── nested_calls.by     # 嵌套调用
│   │   ├── nested_calls.expected
│   │   ├── mixed_operations.by # 混合操作
│   │   ├── mixed_operations.expected
│   │   ├── fibonacci.by        # 斐波那契数列
│   │   ├── fibonacci.expected
│   │   ├── quicksort.by        # 快速排序
│   │   ├── quicksort.expected
│   │   ├── lua_compat.by       # Lua兼容性测试
│   │   └── lua_compat.expected
│   └── regression/             # 回归测试
│       ├── bug_fixes/          # 已修复的bug测试
│       │   ├── issue_001.by    # 特定问题测试
│       │   ├── issue_001.expected
│       │   └── ...
│       └── edge_cases/         # 边界情况测试
│           ├── stack_overflow.by
│           ├── stack_overflow.expected
│           ├── large_numbers.by
│           ├── large_numbers.expected
│           └── ...
```

## 测试策略

### 阶段1: 基础指令测试（当前阶段）
- ✅ 加载存储指令 (LOADI, LOADF, LOADK, MOVE等)
- ✅ 基础算术运算 (ADD, SUB, MUL, DIV)
- ✅ 返回指令 (RETURN0, RETURN1)
- 🔄 位运算指令 (BAND, BOR, BXOR等)
- 🔄 比较指令 (EQ, LT, LE等)
- 🔄 控制流指令 (JMP, TEST等)

### 阶段2: 中级功能测试
- 表操作指令
- 字符串操作
- 简单函数调用
- 上值操作

### 阶段3: 高级功能测试
- 元方法
- 迭代器
- 垃圾回收

### 阶段4: AQL扩展测试
- 容器操作
- 内置函数
- 协程

### 阶段5: 集成和回归测试
- 复杂场景测试
- 性能测试
- Lua 5.5.1 核心兼容性测试

## 实施计划

### 立即行动项
1. 重命名目录结构
2. 为现有测试添加.expected文件
3. 创建基础指令测试用例
4. 逐步完善avm_core.c中缺失的指令实现

### 长期目标
1. 建立完整的指令覆盖率
2. 与 Lua 5.5.1 核心语义保持兼容
3. 建立自动化回归测试
4. 性能基准测试

## 工具和脚本

### vmtest命令
```bash
# 运行所有测试
make vmtest

# 运行特定类别测试
./bin/vmtest test/vm/bytecode/basic bin/aqlm

# 运行单个测试
./bin/vmtest test/vm/bytecode/basic bin/aqlm -t add

# 详细模式
./bin/vmtest test/vm/bytecode/basic bin/aqlm -v
```

### 测试生成脚本
- `generate_test.sh` - 自动生成测试用例模板
- `update_expected.sh` - 批量更新期望结果
- `coverage_report.sh` - 生成指令覆盖率报告

## 维护指南

### 添加新测试
1. 在合适的目录下创建.by文件
2. 运行测试生成.expected文件
3. 验证结果正确性
4. 提交到版本控制

### 修复失败测试
1. 分析失败原因
2. 修复avm_core.c中的实现
3. 重新运行测试验证
4. 更新文档

### 回归测试
1. 每次修改avm_core.c后运行完整测试套件
2. 确保所有现有测试仍然通过
3. 添加新的回归测试防止问题再次出现
