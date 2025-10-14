# AQL函数和闭包系统推进规划

## 📊 当前状态评估

### ✅ 已完成的核心功能
1. **基础函数调用机制** - CALL, RETURN, RETURN0, RETURN1 ✅
2. **闭包创建** - CLOSURE指令和基本pushclosure ✅  
3. **Upvalue基础访问** - GETUPVAL, SETUPVAL (带安全检查) ✅
4. **多返回值支持** - 完整实现 ✅
5. **嵌套函数支持** - 正确的Proto层次结构 ✅
6. **字节码解析扩展** - .upvalues和.upvalue语法 ✅

### ❌ 存在的问题
1. **递归调用段错误** - CLOSURE在递归中重复创建导致问题
2. **缺少全局环境支持** - 无法像Lua那样通过_ENV访问函数
3. **upvalue生命周期管理不完整** - 缺少open/closed状态转换
4. **变参系统不完善** - VARARG相关指令需要完善

## 🚀 推进路线规划

### 阶段1: 修复递归调用问题 (优先级: 🔥高)

#### 1.1 实现全局环境系统
```c
// 目标：支持像Lua一样的全局函数访问
// 当前问题：递归中使用CLOSURE重复创建函数导致段错误
// 解决方案：实现_ENV全局环境表

// 需要实现的指令：
- SETTABUP  // 设置函数到全局环境
- GETTABUP  // 从全局环境获取函数 (已有解析，需完善执行)

// 字节码模式：
.main
CLOSURE   R0, 0            # 创建函数
SETTABUP  0, K0, R0        # _ENV["func_name"] = function
GETTABUP  R1, 0, K0        # R1 = _ENV["func_name"] (递归调用时)
CALL      R1, 2, 2         # 调用函数
```

#### 1.2 完善GETTABUP/SETTABUP执行
```c
// 在avm_core.c中完善这两个指令的执行逻辑
// 确保能正确访问全局环境表
```

### 阶段2: 完善upvalue生命周期管理 (优先级: 🔥高)

#### 2.1 实现upvalue关闭机制
```c
// 目标：正确处理upvalue的open/closed状态转换
// 当前问题：函数返回时upvalue没有正确关闭

void aqlF_close(aql_State *L, StkId level, int status) {
  // 关闭指定栈位置及以上的所有open upvalues
  // 将open upvalue转为closed upvalue
}

// 在RETURN指令中调用upvalue关闭
vmcase(OP_RETURN) {
  // ... 现有逻辑
  if (k) aqlF_close(L, base, CLOSEPROTECT);  // 关闭upvalue
  // ... 返回逻辑
}
```

#### 2.2 优化upvalue创建和访问
```c
// 完善pushclosure中的upvalue初始化
// 确保upvalue正确继承和创建
static void pushclosure(aql_State *L, Proto *p, UpVal **encup, 
                       StkId base, StkId ra) {
  // 更robust的upvalue创建逻辑
}
```

### 阶段3: 完善变参系统 (优先级: 🟡中)

#### 3.1 实现VARARGPREP指令
```c
// 目标：支持可变参数函数
vmcase(OP_VARARGPREP) {
  int numparams = GETARG_A(i);
  ProtectNT(aqlT_adjustvarargs(L, numparams, ci, cl->p));
  vmbreak;
}
```

#### 3.2 完善VARARG指令
```c
// 获取变参到指定位置
vmcase(OP_VARARG) {
  int n = GETARG_C(i) - 1;
  Protect(aqlT_getvarargs(L, ci, ra, n));
  vmbreak;
}
```

### 阶段4: 性能优化 (优先级: 🟢低)

#### 4.1 实现尾调用优化
```c
// TAILCALL指令的完整实现
// 避免栈增长，优化递归性能
vmcase(OP_TAILCALL) {
  // 尾调用优化逻辑
}
```

#### 4.2 内存管理优化
```c
// 优化Proto和Closure的内存分配
// 改进GC性能
```

## 📋 具体实施计划

### 第一步：修复递归调用 (1-2天)

1. **完善GETTABUP/SETTABUP执行逻辑**
   - 在`avm_core.c`中实现完整的全局环境访问
   - 确保能正确设置和获取全局函数

2. **创建递归测试用例**
   - 使用GETTABUP模式而非CLOSURE重复创建
   - 验证递归调用不再段错误

3. **更新字节码生成器**
   - 在`aqlm.c`中支持全局环境相关的字节码生成

### 第二步：upvalue生命周期管理 (2-3天)

1. **实现aqlF_close函数**
   - 正确处理open->closed转换
   - 在函数返回时调用

2. **完善pushclosure**
   - 更robust的upvalue创建和继承逻辑
   - 处理嵌套闭包的复杂情况

3. **添加upvalue调试支持**
   - 更详细的调试输出
   - 帮助诊断upvalue问题

### 第三步：变参系统 (1-2天)

1. **实现VARARGPREP和VARARG指令**
2. **添加变参测试用例**
3. **完善相关的栈管理逻辑**

### 第四步：性能优化和完善 (按需)

1. **TAILCALL优化**
2. **内存管理改进**
3. **调试信息完善**

## 🎯 成功标准

### 阶段1完成标准：
- ✅ 递归函数调用不再段错误
- ✅ 支持10层以上的递归深度
- ✅ 全局环境访问正常工作

### 阶段2完成标准：
- ✅ 复杂嵌套闭包正确工作
- ✅ upvalue生命周期管理正确
- ✅ 内存泄漏检查通过

### 阶段3完成标准：
- ✅ 变参函数正确工作
- ✅ 支持...语法的函数调用

### 最终目标：
- ✅ 通过所有Lua兼容性测试
- ✅ 性能达到可接受水平
- ✅ 内存使用稳定

## 💡 技术重点

### 1. 全局环境实现
```c
// 核心思路：维护一个全局环境表
// 函数通过名字而非重复创建来访问
TValue *global_env;  // 全局环境表
```

### 2. upvalue状态管理
```c
// 关键：正确区分open和closed状态
typedef struct UpVal {
  CommonHeader;
  union {
    TValue value;      // closed upvalue的值
    struct {           // open upvalue
      struct UpVal *next;
      struct UpVal **previous;
    } open;
  } u;
  TValue *v;          // 指向实际值的位置
} UpVal;
```

### 3. 递归优化策略
```assembly
# 推荐的递归字节码模式：
.main
CLOSURE   R0, 0            # 只创建一次
SETTABUP  0, K0, R0        # 设置到全局环境
# ... 调用逻辑

.function recursive_func 1
.code
# 基础情况检查
GETTABUP  R1, 0, K0        # 从全局环境获取函数引用
CALL      R1, 2, 2         # 递归调用
```

---
*本文档为AQL函数和闭包系统的推进规划，基于Lua 5.4分析和当前AQL实现状态制定。*


