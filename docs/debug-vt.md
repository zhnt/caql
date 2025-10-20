# AQL VM 调试输出设计文档

## 概述

本文档描述了AQL虚拟机`-vt`（verbose trace）调试输出的设计思路和预期效果。

## 设计目标

1. **清晰分层**：指令执行、函数调用、寄存器状态分别显示
2. **前后对比**：BEFORE和状态要能直接对比
3. **问题定位**：一眼就能看出是指令执行问题还是脚本逻辑问题
4. **信息完整**：显示所有相关寄存器和参数
5. **性能优化**：正式版本零开销，调试版本高效

## 设计思路

### 混合方案设计

采用混合方案，结合统一接口和专门处理函数的优势：

1. **统一宏定义**：在`adebug.h`中定义通用调试宏
2. **指令分发**：在`print_instruction_trace`中根据指令类型分发
3. **专门处理**：为每种指令格式定义专门的处理函数
4. **编译优化**：正式版本中所有调试代码被完全优化掉

### 核心架构

```c
// 在adebug.h中定义通用宏
#ifdef AQL_DEBUG_BUILD
#define AQL_VT_BEFORE() \
    if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
        print_instruction_trace(L, ci, i, pc-1, cl, base, func_name, 1); \
    }

#define AQL_VT_() \
    if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
        print_instruction_trace(L, ci, i, pc-1, cl, base, func_name, 0); \
    }
#else
#define AQL_VT_BEFORE() ((void)0)
#define AQL_VT_() ((void)0)
#endif

// 在print_instruction_trace中根据指令类型分发
static void print_instruction_trace(aql_State *L, CallInfo *ci, Instruction i, 
                                   const Instruction *pc, LClosure *cl, StkId base, 
                                   const char *func_name, int is_before) {
    OpCode op = GET_OPCODE(i);
    
    switch (op) {
        case OP_LOADI:
            print_instruction_trace_abx(L, ci, i, pc, cl, base, func_name, is_before);
            break;
        case OP_MUL:
            print_instruction_trace_abc(L, ci, i, pc, cl, base, func_name, is_before);
            break;
        case OP_JMP:
            print_instruction_trace_asbx(L, ci, i, pc, cl, base, func_name, is_before);
            break;
        // ... 其他指令
    }
}
```

### AQL_VT_格式识别机制

**问题**：`AQL_VT_`如何知道该调用`abc`还是`abx`格式的处理函数？

**解决方案**：使用指令类型分发机制

1. **统一接口**：`AQL_VT_BEFORE()`和`AQL_VT_()`都调用`print_instruction_trace`
2. **指令分发**：`print_instruction_trace`根据`GET_OPCODE(i)`确定指令类型
3. **格式识别**：每个指令类型对应明确的格式处理函数

**优势**：
- `AQL_VT_`不需要知道具体格式
- 新增指令只需要在switch中添加case
- 编译时优化，正式版本零开销

### 指令格式处理函数

为每种指令格式定义专门的处理函数：

- `print_instruction_trace_abc()` - 处理iABC格式指令
- `print_instruction_trace_abx()` - 处理iABx格式指令  
- `print_instruction_trace_asbx()` - 处理iAsBx格式指令
- `print_instruction_trace_ax()` - 处理iAx格式指令

### 宏定义 vs 函数定义的选择

#### 方案A：宏定义方式（推荐）
```c
#ifdef AQL_DEBUG_BUILD
#define print_instruction_trace_abx(L, ci, i, pc, cl, base, func_name, is_before) \
    do { \
        int a = GETARG_A(i); \
        int bx = GETARG_sBx(i); \
        printf("PC=%d [%s] <%s,op=%d> %s", pc_num, func_name, aql_opnames[op], op, \
               is_before ? "BEFORE" : ""); \
        printf("  A=[R%d", a); \
        /* ... 显示寄存器值 ... */ \
        printf("]  sBx=%d\n", bx); \
    } while(0)
#else
#define print_instruction_trace_abx(L, ci, i, pc, cl, base, func_name, is_before) ((void)0)
#endif
```

```

**推荐使用方案A（宏定义方式）**，理由：
1. **性能最优**：正式版本零开销，调试版本直接展开
2. **代码一致**：所有调试相关都是宏定义
3. **编译优化**：编译器可以完全优化掉调试代码
4. **直接使用printf**：不需要额外的`aql_info_vt`宏层
5. **易于维护**：宏定义集中管理

## 预期输出效果

### 1. 基本指令执行

```
PC=0 [main] CLOSURE : A=[R0=?], Bx=0
PC=0 [main] CLOSURE :  A=[R0=func_multiply], Bx=0

PC=1 [main] LOADI :   A=[R1=?], sBx=5
PC=1 [main] LOADI :    A=[R1=5], sBx=5

PC=2 [main] LOADI :   A=[R2=?], sBx=3
PC=2 [main] LOADI :    A=[R2=3], sBx=3
```

### 2. 函数调用完整流程

```
PC=3 [main] CALL :    A=[R0=func], B=[R1=5], C=[R2=3]
--- CALL: func=multiply, args=2, rets=1 ---
  Args: R0=5, R1=3
--- ENTER: func=multiply ---

PC=0 [multiply] MUL : A=[R2=?], B=[R0=5], C=[R1=3]
PC=0 [multiply] MUL :  A=[R2=15], B=[R0=5], C=[R1=3]

PC=2 [multiply] RETURN1 : A=[R2=15]
--- RETURN: func=multiply, rets=1 ---
  Return: R2=15
--- EXIT: func=multiply ---

PC=3 [main] CALL :     A=[R0=15], B=[R1=5], C=[R2=3]
```

### 3. 复杂函数调用示例

```
PC=4 [main] MOVE :    A=[R3=?], B=[R0=15]
PC=4 [main] MOVE :     A=[R3=15], B=[R0=15]

PC=5 [main] CLOSURE : A=[R4=?], Bx=0
PC=5 [main] CLOSURE :  A=[R4=func_add], Bx=0

PC=6 [main] LOADI :   A=[R5=?], sBx=1
PC=6 [main] LOADI :    A=[R5=1], sBx=1

PC=7 [main] LOADI :   A=[R6=?], sBx=1
PC=7 [main] LOADI :    A=[R6=1], sBx=1

PC=8 [main] CALL :    A=[R4=func], B=[R5=1], C=[R6=1]
--- CALL: func=add, args=2, rets=1 ---
  Args: R0=1, R1=1
--- ENTER: func=add ---

PC=0 [add] ADD :      A=[R2=?], B=[R0=1], C=[R1=1]
PC=0 [add] ADD :       A=[R2=2], B=[R0=1], C=[R1=1]

PC=2 [add] RETURN1 :  A=[R2=2]
--- RETURN: func=add, rets=1 ---
  Return: R2=2
--- EXIT: func=add ---

PC=8 [main] CALL :     A=[R4=2], B=[R5=1], C=[R6=1]

PC=9 [main] ADD :     A=[R7=?], B=[R3=15], C=[R4=2]
PC=9 [main] ADD :      A=[R7=17], B=[R3=15], C=[R4=2]

PC=11 [main] RETURN1 : A=[R7=17]
--- RETURN: func=main, rets=1 ---
  Return: R7=17
--- EXIT: func=main ---
```

### 4. 不同指令格式的显示

#### iABC格式（如MUL）
```
PC=0 [multiply] MUL : A=[R2=?], B=[R0=5], C=[R1=3]
PC=0 [multiply] MUL :  A=[R2=15], B=[R0=5], C=[R1=3]
```

#### iABx格式（如LOADI）
```
PC=1 [main] LOADI :   A=[R1=?], sBx=5
PC=1 [main] LOADI :    A=[R1=5], sBx=5
```

#### iAsBx格式（如JMP）
```
PC=5 [main] JMP :     A=[R0=0], sBx=2
PC=5 [main] JMP :      A=[R0=0], sBx=2
```

### 5. 寄存器值显示格式

#### 整数
```
A=[R0=42]
```

#### 浮点数
```
A=[R1=3.14]
```

#### 字符串
```
A=[R2="hello"]
```

#### 函数
```
A=[R3=func]
```

#### 未初始化
```
A=[R4=?]
```

#### nil值
```
A=[R5=nil]
```

### 6. 函数名显示

#### 主函数
```
PC=0 [main] LOADI :   A=[R1=?], sBx=5
```

#### 命名函数
```
PC=0 [multiply] MUL : A=[R2=?], B=[R0=5], C=[R1=3]
```

#### 匿名函数
```
PC=0 [func_0x7f8990406010] MUL : A=[R2=?], B=[R0=5], C=[R1=3]
```

## 关键设计问题

### 1. 指令信息在哪里打印？

**问题分析**：
- 信息在指令执行前打印
- 信息需要在指令执行后打印
- 但不同指令的执行位置不同

**解决方案**：
- **简单指令**（如LOADI, MOVE等）：在指令执行后立即打印
- **复杂指令**（如CALL, RETURN等）：需要特殊处理
  - CALL指令：在被调用函数返回后打印
  - RETURN指令：在函数退出后打印

**实现位置**：
```c
// 在avm_core.c的指令执行循环中
for (;;) {
    Instruction i = *pc++;
    
    // 打印
    AQL_VT_();
    
    // 执行指令
    switch (GET_OPCODE(i)) {
        case OP_LOADI:
            // 执行LOADI
            AQL_VT_();
            break;
            
        case OP_CALL:
            // 执行CALL
            // 不在这里打印，等函数返回后打印
            break;
            
        case OP_RETURN1:
            // 执行RETURN1
            // 不在这里打印，等函数退出后打印
            break;
    }
}
```

### 2. 函数名信息如何取得？

**问题分析**：
- 当前显示`func_0x7f8990406010`这样的地址
- 需要显示有意义的函数名

**解决方案**：

#### 方案A：从闭包信息获取
```c
// 在print_instruction_trace函数中
const char *get_function_name(LClosure *cl) {
    if (!cl || !cl->p) return "unknown";
    
    // 尝试从source获取函数名
    if (cl->p->source) {
        const char *source = getstr(cl->p->source);
        if (source && strlen(source) > 0) {
            return source;
        }
    }
    
    // 如果source为空，使用函数地址
    static char func_id[32];
    snprintf(func_id, sizeof(func_id), "func_%p", (void*)cl->p);
    return func_id;
}
```

#### 方案B：从字节码解析器获取
```c
// 在aqlm.c解析.upvalue时记录函数名
typedef struct {
    const char *name;
    LClosure *closure;
} FunctionInfo;

// 维护函数名映射表
static FunctionInfo function_map[MAX_FUNCTIONS];
static int function_count = 0;

// 在解析时记录
void record_function_name(const char *name, LClosure *closure) {
    if (function_count < MAX_FUNCTIONS) {
        function_map[function_count].name = name;
        function_map[function_count].closure = closure;
        function_count++;
    }
}
```

**推荐方案**：
结合方案A和方案B：
1. 优先从闭包source获取函数名
2. 如果source为空，从函数映射表查找
3. 最后才使用地址作为标识

## 实现步骤

### 第一阶段：基础格式改进
1. 修复/显示格式
2. 统一寄存器值显示
3. 改进函数名显示

### 第二阶段：函数调用信息
1. 添加CALL/ENTER/EXIT标记
2. 显示函数参数和返回值
3. 处理嵌套函数调用

### 第三阶段：高级功能
1. 添加颜色支持
2. 优化长输出换行
3. 添加性能统计信息

## 调试效果预期

使用新的调试输出，开发者可以：

1. **快速定位问题**：
   - 一眼看出是哪个函数、哪条指令出问题
   - 清楚看到指令执行前后的状态变化

2. **跟踪执行流程**：
   - 完整的函数调用链
   - 清晰的参数传递和返回值

3. **分析性能问题**：
   - 函数调用频率
   - 指令执行效率

4. **验证脚本逻辑**：
   - 寄存器值变化是否符合预期
   - 函数调用参数是否正确

## 总结

新的调试输出设计将大大提升AQL VM的调试体验，让开发者能够快速定位和解决问题。关键是要正确处理指令信息的打印时机，以及实现有意义的函数名显示。

**核心优势**：
1. **性能最优**：正式版本零开销，调试版本直接展开
2. **代码一致**：所有调试相关都是宏定义，直接使用printf
3. **维护简单**：统一的宏定义，格式一致
4. **灵活控制**：可以精确控制每个指令的时机
5. **编译优化**：编译器可以完全优化掉调试代码