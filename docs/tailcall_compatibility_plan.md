# 尾调用兼容性修复计划

## 阶段1: 基本尾调用修复 (立即执行)

### 1.1 修复TAILCALL指令的upvalue关闭
```c
// 在avm_core.c OP_TAILCALL中添加
if (TESTARG_k(i)) {
  aqlF_close(L, base, CLOSEKTOP);  /* close upvalues from current call */
}
```

### 1.2 修复tail_simple.by的条件跳转逻辑
- 确保LEI + JMP模式与Lua一致
- 验证基础情况和递归情况的正确跳转

### 1.3 调试返回值传递
- 检查RETURN指令在尾调用后的行为
- 确保返回值正确传递到调用者

## 阶段2: 变参数支持 (可选，后续)

### 2.1 添加nextraargs字段到CallInfo
### 2.2 实现完整的delta计算
### 2.3 支持变参数函数的尾调用

## 兼容性验证

### 测试用例优先级:
1. ✅ 固定参数函数尾调用 (tail_simple.by)
2. ✅ 递归函数尾调用 (fibonacci尾递归版本)  
3. ⭕ 变参数函数尾调用 (延后)
