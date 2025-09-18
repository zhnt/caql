# AQL Built-in Print Function - 极简设计

## 🎯 设计哲学：Less is More

Lua用13行代码解决print问题，我们的设计也应该如此：**极简、实用、立即交付**。

## 🔍 现状分析

### ✅ 已有完善基础
- `aqlP_print_value()` 已存在且功能完整（支持所有基础类型 + 数组 + range）
- 内置函数框架已就绪（OP_BUILTIN指令已支持）
- 仅缺少 **Dict** 类型的支持

### ❌ 当前问题
- print内置函数只是占位符，会抛出`"print builtin not yet implemented"`
- 用户无法使用最基本的输出功能

## 💡 极简方案：13行代码解决

### 核心实现
```c
// src/abuiltin.c - 13行代码实现完整print功能
#include "aql.h"
#include "aparser.h"  // 为了使用 aqlP_print_value

int aqlB_print(aql_State *L) {
    int n = aql_gettop(L);  // 获取参数个数
    for (int i = 1; i <= n; i++) {
        if (i > 1) printf("\t");  // 参数间用制表符分隔（Lua风格）
        aqlP_print_value(index2value(L, i));  // 复用现有完善实现
    }
    printf("\n");  // 结尾换行
    return 0;  // 无返回值
}
```

### Dict支持补充
```c
// src/aparser.c - 在aqlP_print_value中添加dict支持（约5行）
// 在现有的类型switch中添加：
case AQL_TDICT: {
    printf("{");
    // 遍历dict元素，格式：key1: value1, key2: value2
    printf("}");
    break;
}
```

### VM集成
```c
// src/avm.c - 修改OP_BUILTIN中的print分支（1行修改）
case 0: /* print */
    aqlB_print(L);  // 直接调用，移除错误抛出
    break;
```

## 📋 功能规格

### 支持类型（复用现有aqlP_print_value）
```
✅ nil         -> nil
✅ boolean     -> true/false  
✅ integer     -> 123
✅ float       -> 3.14159
✅ string      -> "hello"
✅ array       -> [1, 2, 3]
✅ range       -> range(1, 10, 1)
✅ dict        -> {key1: value1, key2: value2}  ← 新增
✅ function    -> function: 0x地址
```

### 输出格式（Lua兼容）
```aql
print("Hello")                    // Hello
print(1, 2, 3)                    // 1    2    3
print("Result:", 42)              // Result: 42
print([1, 2, 3])                  // [1, 2, 3]
print({name="AQL", ver=1.0})        // {name: "AQL", ver: 1.0}
```

## 🚀 实施步骤

### Step 1: 添加Dict支持（5分钟）
```bash
# 在aqlP_print_value中添加dict case
vim src/aparser.c +2355
# 添加AQL_TDICT的switch case
```

### Step 2: 实现aqlB_print（5分钟）  
```bash
# 创建src/abuiltin.c
# 复制上面的13行代码
```

### Step 3: 修改VM调用（2分钟）
```bash
# 修改src/avm.c中的OP_BUILTIN print分支
vim src/avm.c +1240
# 替换错误抛出为函数调用
```

### Step 4: 添加头文件声明（2分钟）
```bash
# 在src/aparser.h中添加aqlB_print声明
```

### Step 5: 构建测试（1分钟）
```bash
make debug
./bin/aqld -e 'print("Hello AQL!")'
```

**总时间：15分钟完成 🔥**

## ✅ 验证测试

### 基础测试（REPL模式）
```bash
echo 'print("Hello")' | ./bin/aqld                # Hello
echo 'print(42)' | ./bin/aqld                     # 42  
echo 'print(true)' | ./bin/aqld                   # true
echo 'print(nil)' | ./bin/aqld                    # nil
```

### 多参数测试（REPL模式）
```bash
echo 'print(1, 2, 3)' | ./bin/aqld                # 1    2    3
echo 'print("Result:", 42)' | ./bin/aqld          # Result: 42
```

### 文件模式测试（需要let包装）
```aql
// test.aql
let sum = 5
let _ = print("Sum is:", sum)  // 输出: Sum is: 5
```

⚠️ **重要限制**：在文件模式中，print调用必须包装在`let`语句中，因为AQL要求顶级代码必须是语句而不是表达式。

## 🔮 未来标准库（不要现在实现）

> ⚠️ **注意**：以下功能属于过度设计，应该放到未来的标准库中

### 格式化输出
```aql
-- 未来标准库函数
printf("Value: %d, Name: %s", value, name)
```

### 结构化输出
```aql
-- 未来标准库函数  
print_json(obj)      -- JSON格式
print_debug(obj)     -- 调试格式（带类型信息）
print_table(obj)     -- 表格格式（数据科学）
```

### 高级功能
```aql
-- 未来标准库函数
print.format = "json"     -- 设置默认格式
print.buffer_size = 4096  -- 设置缓冲区大小
print.theme = "compact"   -- 设置主题
```

## 🎉 结论

**极简方案优势：**
- ✅ **15分钟交付**：立即可用
- ✅ **代码复用**：基于成熟的aqlP_print_value
- ✅ **Lua兼容**：行为和格式与Lua一致
- ✅ **零维护**：简单到不需要维护
- ✅ **易扩展**：未来可以在标准库中增强

**避免过度设计：**
- ❌ 不要复杂的缓冲区管理
- ❌ 不要循环引用检测  
- ❌ 不要自定义格式化器
- ❌ 不要输出重定向抽象

> **"好的设计不是没有什么可以添加，而是没有什么可以去除"** -  Antoine de Saint-Exupéry

**立即行动：15分钟让AQL拥有可用的print功能！** 🚀🔥

---
*设计文档页数：2页（而不是566行）*  
*实现代码：13行（而不是几千行）*
*交付时间：15分钟（而不是几个月）*  
*这就是极简设计的威力！* ✨