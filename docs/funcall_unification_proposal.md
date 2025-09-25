# funcall_v2 和 funcall_v3 融合方案

## 问题分析

当前 AQL 解析器中存在两个功能相似的函数调用处理函数：

### funcall_v2
- **用途**: 处理表达式中的函数调用 (`simpleexp`)
- **假设**: 参数在函数寄存器之前被解析
- **参数位置**: `func_reg - nargs` 到 `func_reg - 1`

### funcall_v3  
- **用途**: 处理语句中的函数调用 (`exprstat`)
- **明确参数**: 接收显式的 `args_start_reg` 参数
- **参数位置**: `args_start_reg` 到 `args_start_reg + nargs - 1`

## 融合方案

### 方案1: 统一接口 (推荐)

创建一个统一的 `funcall_unified` 函数：

```c
/*
** 统一的函数调用处理
** 参数:
**   ls: 词法状态
**   v: 函数表达式描述符
**   func_reg: 函数所在寄存器 (-1 表示需要自动分配)
**   args_start_reg: 参数起始寄存器 (-1 表示需要推断)
**   nargs: 参数个数
**   line: 行号
*/
static void funcall_unified(LexState *ls, expdesc *v, int func_reg, 
                           int args_start_reg, int nargs, int line) {
    FuncState *fs = ls->fs;
    
    /* 确保函数在寄存器中 */
    if (func_reg < 0) {
        aqlK_exp2nextreg(fs, v);
        func_reg = v->u.info;
    }
    
    /* 推断参数位置 */
    if (args_start_reg < 0) {
        /* funcall_v2 模式: 参数在函数寄存器之前 */
        args_start_reg = func_reg - nargs;
    }
    
    printf_debug("[DEBUG] funcall_unified: func_reg=%d, args_start_reg=%d, nargs=%d\n", 
                 func_reg, args_start_reg, nargs);
    
    /* 移动参数到正确位置 */
    int target_start_reg = func_reg + 1;
    for (int i = 0; i < nargs; i++) {
        int src_reg = args_start_reg + i;
        int dst_reg = target_start_reg + i;
        if (src_reg != dst_reg) {
            aqlK_codeABC(fs, OP_MOVE, dst_reg, src_reg, 0);
        }
    }
    
    /* 更新寄存器状态 */
    fs->freereg = target_start_reg + nargs;
    
    /* 生成调用指令 */
    aqlK_codeABC(fs, OP_CALL, func_reg, nargs + 1, 1);
    
    /* 设置返回值 */
    init_exp(v, VNONRELOC, func_reg);
}
```

### 方案2: 保留两个函数，统一实现

保持现有接口，但让两个函数都调用统一的内部实现：

```c
static void funcall_internal(LexState *ls, expdesc *v, int func_reg, 
                            int args_start_reg, int nargs, int line);

static void funcall_v2(LexState *ls, expdesc *v, int nargs, int line) {
    aqlK_exp2nextreg(ls->fs, v);
    int func_reg = v->u.info;
    int args_start_reg = func_reg - nargs;
    funcall_internal(ls, v, func_reg, args_start_reg, nargs, line);
}

static void funcall_v3(LexState *ls, expdesc *v, int func_reg, 
                      int args_start_reg, int nargs, int line) {
    funcall_internal(ls, v, func_reg, args_start_reg, nargs, line);
}
```

## 推荐实施步骤

1. **实现统一函数**: 创建 `funcall_unified` 或 `funcall_internal`
2. **更新调用点**: 
   - `simpleexp` 中调用 `funcall_unified(ls, v, -1, -1, nargs, line)`
   - `exprstat` 中调用 `funcall_unified(ls, &v, func_reg, args_start_reg, nargs, line)`
3. **测试验证**: 确保所有函数参数测试通过
4. **清理代码**: 移除 `funcall_v2` 和 `funcall_v3`

## 优势

- **代码复用**: 消除重复逻辑
- **维护性**: 单一实现，更容易维护和调试
- **一致性**: 确保两种调用方式行为完全一致
- **扩展性**: 未来添加新功能只需修改一处

## 风险评估

- **低风险**: 核心逻辑相同，主要是参数传递方式的统一
- **测试覆盖**: 现有的77个回归测试可以验证正确性
- **回滚方案**: 如有问题可以快速回滚到当前版本


