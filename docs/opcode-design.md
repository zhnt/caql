# AQL 指令集设计

## 1. 概述

AQL指令集采用基于寄存器的设计，借鉴了Lua虚拟机的优秀架构，同时针对AI服务编排场景进行了优化。使用32位固定长度指令编码，支持多种指令格式。

## 2. 指令格式

参考Lua的指令格式设计，AQL支持以下格式：

### 2.1 基本指令格式
```
iABC:  [OP:6] [A:8] [C:9] [B:9]     ; 32位总长度
iABx:  [OP:6] [A:8] [Bx:18]         ; 用于常量索引
iAsBx: [OP:6] [A:8] [sBx:18]        ; 用于有符号跳转
iAx:   [OP:6] [Ax:26]               ; 用于大参数
```

### 2.2 字段定义
- OP: 操作码 (6位, 支持64个基础指令)
- A: 目标寄存器 (8位, 256个寄存器)
- B, C: 源操作数 (各9位, 512个寄存器/常量)
- Bx: 无符号大参数 (18位, 用于常量表索引)
- sBx: 有符号大参数 (18位, 用于跳转偏移)
- Ax: 超大参数 (26位, 用于扩展指令)

## 3. 指令分类

### 3.1 基础指令组 (0-15): 加载存储指令
- 0  MOVE      R(A) := R(B)                       ; 寄存器移动 (iABC)
- 1  LOADI     R(A) := sBx                        ; 加载立即整数 (iAsBx)
- 2  LOADF     R(A) := (float)sBx                 ; 加载立即浮点数 (iAsBx)
- 3  LOADK     R(A) := Kst(Bx)                    ; 加载常量 (iABx)
- 4  LOADKX    R(A) := Kst(extra arg)             ; 加载大常量 (iABx+iAx)
- 5  LOADFALSE R(A) := false                      ; 加载false (iABC)
- 6  LOADTRUE  R(A) := true                       ; 加载true (iABC)
- 7  LOADNIL   R(A), R(A+1), ..., R(A+B) := nil  ; 批量加载nil (iABC)
- 8  GETUPVAL  R(A) := UpValue[B]                 ; 获取upvalue (iABC)
- 9  SETUPVAL  UpValue[B] := R(A)                 ; 设置upvalue (iABC)
- 10 GETTABUP  R(A) := UpValue[B][RK(C)]          ; 从upvalue表获取 (iABC)
- 11 SETTABUP  UpValue[A][RK(B)] := RK(C)         ; 设置upvalue表 (iABC)
- 12 CLOSE     close all upvalues >= R(A)         ; 关闭upvalue (iABC)
- 13 TBC       mark variable A "to be closed"     ; 标记待关闭变量 (iABC)
- 14 CONCAT    R(A) := R(A).. ... ..R(A+B-1)     ; 字符串连接 (iABC)
- 15 EXTRAARG  Ax                                 ; 扩展参数 (iAx)

### 3.2 算术运算组 (16-31): 算术指令 (含K/I优化)
- 16 ADD       R(A) := R(B) + R(C)               ; 寄存器加法 (iABC)
- 17 ADDK      R(A) := R(B) + K[C]               ; 常量加法 (iABC)
- 18 ADDI      R(A) := R(B) + sC                 ; 立即数加法 (iABC, -128~127)
- 19 SUB       R(A) := R(B) - R(C)               ; 寄存器减法 (iABC)
- 20 SUBK      R(A) := R(B) - K[C]               ; 常量减法 (iABC)
- 21 SUBI      R(A) := R(B) - sC                 ; 立即数减法 (iABC, -128~127)
- 22 MUL       R(A) := R(B) * R(C)               ; 寄存器乘法 (iABC)
- 23 MULK      R(A) := R(B) * K[C]               ; 常量乘法 (iABC)
- 24 MULI      R(A) := R(B) * sC                 ; 立即数乘法 (iABC, -128~127)
- 25 DIV       R(A) := R(B) / R(C)               ; 寄存器除法 (iABC)
- 26 DIVK      R(A) := R(B) / K[C]               ; 常量除法 (iABC)
- 27 DIVI      R(A) := R(B) / sC                 ; 立即数除法 (iABC, -128~127)
- 28 MOD       R(A) := R(B) % R(C)               ; 取模运算 (iABC)
- 29 POW       R(A) := R(B) ^ R(C)               ; 幂运算 (iABC)
- 30 UNM       R(A) := -R(B)                     ; 取负 (iABC)
- 31 LEN       R(A) := length of R(B)            ; 长度操作 (iABC)

### 3.3 位运算组 (32-39): 位运算指令
- 32 BAND      R(A) := R(B) & R(C)               ; 按位与 (iABC)
- 33 BOR       R(A) := R(B) | R(C)               ; 按位或 (iABC)
- 34 BXOR      R(A) := R(B) ~ R(C)               ; 按位异或 (iABC)
- 35 SHL       R(A) := R(B) << R(C)              ; 左移 (iABC)
- 36 SHR       R(A) := R(B) >> R(C)              ; 右移 (iABC)
- 37 BNOT      R(A) := ~R(B)                     ; 按位取反 (iABC)
- 38 NOT       R(A) := not R(B)                  ; 逻辑取反 (iABC)
- 39 SHRI      R(A) := R(B) >> sC                ; 立即数右移 (iABC)

### 3.4 比较控制组 (40-47): 比较和跳转指令
- 40 JMP       pc += sBx; if (A) close upvalues >= R(A-1) ; 无条件跳转 (iAsBx)
- 41 EQ        if ((R(B) == R(C)) ~= A) then pc++         ; 相等比较 (iABC)
- 42 LT        if ((R(B) < R(C)) ~= A) then pc++          ; 小于比较 (iABC)
- 43 LE        if ((R(B) <= R(C)) ~= A) then pc++         ; 小于等于比较 (iABC)
- 44 TEST      if (not R(A) == C) then pc++               ; 测试指令 (iABC)
- 45 TESTSET   if (not R(B) == C) then pc++ else R(A) := R(B) ; 测试并设置 (iABC)
- 46 EQI       if ((R(A) == sB) ~= k) then pc++           ; 立即数相等比较 (iABC)
- 47 LTI       if ((R(A) < sB) ~= k) then pc++            ; 立即数小于比较 (iABC)

### 3.5 函数调用组 (48-55): 函数和循环指令
- 48 CALL      R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1)) ; 函数调用 (iABC)
- 49 TAILCALL  return R(A)(R(A+1), ... ,R(A+B-1))                 ; 尾调用优化 (iABC)
- 50 RET       return R(A), ... ,R(A+B-2)                         ; 通用返回 (iABC)
- 51 RET_VOID  return                                              ; 无返回值优化 (iABC)
- 52 RET_ONE   return R(A)                                        ; 单返回值优化 (iABC)
- 53 FORLOOP   update counters; if loop continues then pc-=Bx     ; 数值for循环 (iABx)
- 54 FORPREP   check values and prepare counters; if not to run then pc+=Bx+1 ; for循环准备 (iABx)
- 55 CLOSURE   R(A) := closure(KPROTO[Bx])                        ; 创建闭包 (iABx)

### 3.6 AQL容器组 (56-59): AQL专用容器操作
- 56 NEWOBJECT R(A) := new_object(type[B], size[C])              ; 创建对象 (iABC)
- 57 GETPROP   R(A) := R(B).property[C] or R(B)[R(C)]           ; 获取属性/索引 (iABC)
- 58 SETPROP   R(A).property[B] := R(C) or R(A)[R(B)] := R(C)   ; 设置属性/索引 (iABC)
- 59 INVOKE    R(A) := R(B):method[C](args...)                   ; 方法调用 (iABC)

### 3.7 AQL扩展组 (60-63): AQL特有功能
- 60 YIELD     yield R(A), R(A+1), ..., R(A+B-1)               ; 协程让出 (iABC)
- 61 RESUME    R(A), ..., R(A+C-2) := resume(R(B))             ; 协程恢复 (iABC)
- 62 BUILTIN   R(A) := builtin_func[B](R(C), R(D))             ; 内置函数调用 (iABC)
- 63 VARARG    R(A), R(A+1), ..., R(A+C-2) = vararg            ; 变长参数 (iABC)

### 3.6 循环指令 (扩展Lua设计)
#### 3.6.1 数值循环
- FORLOOP   R(A)+=R(A+2); if R(A) <?= R(A+1) then { pc+=sBx; R(A+3)=R(A) } ; 数值循环 (iAsBx)
- FORPREP   R(A)-=R(A+2); pc+=sBx                              ; 循环准备 (iAsBx)

#### 3.6.2 迭代器循环
- TFORCALL  R(A+3), ... ,R(A+2+C) := R(A)(R(A+1), R(A+2))     ; 泛型for调用 (iABC)
- TFORLOOP  if R(A+1) ~= nil then { R(A)=R(A+1); pc += sBx }   ; 泛型for循环 (iAsBx)

#### 3.6.3 AQL特有循环
- FORRANGE  R(A) := range(RK(B), RK(C), RK(D))                 ; 创建范围迭代器 (iABC)
- FOREACH   for item in R(B) do { R(A) := item; pc += sBx }    ; for-each循环 (iAsBx)
- ENUMERATE R(A) := enumerate(R(B))                            ; 创建带索引迭代器 (iABC)

#### 3.6.4 异步循环
- ASYNCFOR  for await item in R(B) do { R(A) := item }         ; 异步for循环 (iABC)
- PARALLEL  parallel for item in R(B) do { R(A) := item }      ; 并行for循环 (iABC)

### 3.7 异步协程指令 (保留核心异步支持)

- YIELD     R(A), ... ,R(A+B-2) := yield R(A), ... ,R(A+B-2)  ; 协程让出 (iABC)
- RESUME    R(A) := resume R(B)                                ; 恢复协程 (iABC)
- AWAIT     R(A) := await R(B)                                 ; 等待异步结果 (iABC)
- ASYNC     R(A) := async_func R(B)                            ; 创建异步函数 (iABC)

### 3.8 模式匹配指令 (AQL核心特性)
- MATCH     R(A) := match R(B) cases[C]                        ; 开始模式匹配 (iABC)
- EXTRACT   R(A) := extract_from_pattern(R(B), index[C])       ; 提取匹配值 (iABC)

### 3.9 向量化指令 (精简SIMD支持)
- VMETA     R(A) := vector_op(op[B], R(C), R(D))              ; 通用向量操作 (iABC)
- VWINDOW   R(A) := window_op(R(B), R(C), op[D])              ; 通用窗口操作 (iABC)
- VMAP      R(A) := map_func(R(B), func[C])                   ; 向量映射 (iABC)

### 3.10 元编程和内置函数
- BUILTIN   R(A) := builtin_func[B](R(C), R(D))               ; 内置函数调用 (iABC)
- META      R(A) := meta_op(opcode[B], R(C), R(D))            ; 元操作 (iABC)

### 3.11 扩展指令 (使用EXTRAARG模式)
- EXTRAARG  Ax                                                 ; 扩展参数 (iAx)

## 4. 指令编码规则 (完全采用Lua的设计)

### 4.1 操作数编码格式
```c
// 指令格式定义
enum OpMode {iABC, iABx, iAsBx, iAx};

// 字段大小定义
#define SIZE_C    9     // C字段: 9位
#define SIZE_B    9     // B字段: 9位  
#define SIZE_Bx   18    // Bx字段: 18位 (B+C)
#define SIZE_A    8     // A字段: 8位
#define SIZE_Ax   26    // Ax字段: 26位 (A+B+C)
#define SIZE_OP   6     // 操作码: 6位

// 字段位置定义
#define POS_OP    0                    // 操作码起始位置
#define POS_A     (POS_OP + SIZE_OP)   // A字段起始位置 (6)
#define POS_C     (POS_A + SIZE_A)     // C字段起始位置 (14)
#define POS_B     (POS_C + SIZE_C)     // B字段起始位置 (23)
#define POS_Bx    POS_C                // Bx字段起始位置 (14)
#define POS_Ax    POS_A                // Ax字段起始位置 (6)
```

### 4.2 参数大小限制 (跨平台兼容)
```c
// 动态计算参数最大值，适应不同平台的int位数
#if SIZE_Bx < LUAI_BITSINT-1
#define MAXARG_Bx    ((1<<SIZE_Bx)-1)      // 262143 (18位)
#define MAXARG_sBx   (MAXARG_Bx>>1)        // 131071 (有符号)
#else
#define MAXARG_Bx    MAX_INT               // 平台int最大值
#define MAXARG_sBx   MAX_INT
#endif

#if SIZE_Ax < LUAI_BITSINT-1
#define MAXARG_Ax    ((1<<SIZE_Ax)-1)      // 67108863 (26位)
#else
#define MAXARG_Ax    MAX_INT
#endif

#define MAXARG_A     ((1<<SIZE_A)-1)       // 255 (8位)
#define MAXARG_B     ((1<<SIZE_B)-1)       // 511 (9位)
#define MAXARG_C     ((1<<SIZE_C)-1)       // 511 (9位)
```

### 4.3 位操作宏定义
```c
// 位掩码生成宏
#define MASK1(n,p)   ((~((~(Instruction)0)<<(n)))<<(p))  // n个1位掩码在位置p
#define MASK0(n,p)   (~MASK1(n,p))                       // n个0位掩码在位置p

// 通用字段操作宏
#define getarg(i,pos,size)    (cast(int, ((i)>>pos) & MASK1(size,0)))
#define setarg(i,v,pos,size)  ((i) = (((i)&MASK0(size,pos)) | \
                               ((cast(Instruction, v)<<pos)&MASK1(size,pos))))

// 操作码操作
#define GET_OPCODE(i)    (cast(OpCode, ((i)>>POS_OP) & MASK1(SIZE_OP,0)))
#define SET_OPCODE(i,o)  ((i) = (((i)&MASK0(SIZE_OP,POS_OP)) | \
                          ((cast(Instruction, o)<<POS_OP)&MASK1(SIZE_OP,POS_OP))))

// 各字段访问宏
#define GETARG_A(i)      getarg(i, POS_A, SIZE_A)
#define SETARG_A(i,v)    setarg(i, v, POS_A, SIZE_A)
#define GETARG_B(i)      getarg(i, POS_B, SIZE_B)
#define SETARG_B(i,v)    setarg(i, v, POS_B, SIZE_B)
#define GETARG_C(i)      getarg(i, POS_C, SIZE_C)
#define SETARG_C(i,v)    setarg(i, v, POS_C, SIZE_C)
#define GETARG_Bx(i)     getarg(i, POS_Bx, SIZE_Bx)
#define SETARG_Bx(i,v)   setarg(i, v, POS_Bx, SIZE_Bx)
#define GETARG_Ax(i)     getarg(i, POS_Ax, SIZE_Ax)
#define SETARG_Ax(i,v)   setarg(i, v, POS_Ax, SIZE_Ax)

// 有符号参数的Excess K编码
#define GETARG_sBx(i)    (GETARG_Bx(i)-MAXARG_sBx)
#define SETARG_sBx(i,b)  SETARG_Bx((i),cast(unsigned int, (b)+MAXARG_sBx))
```

### 4.4 RK编码机制 (寄存器/常量复用)
```c
// RK编码: 最高位区分寄存器和常量
#define BITRK        (1 << (SIZE_B - 1))  // 0x100 (第9位)
#define ISK(x)       ((x) & BITRK)        // 测试是否为常量
#define INDEXK(r)    ((int)(r) & ~BITRK)  // 获取常量索引
#define RKASK(x)     ((x) | BITRK)        // 标记为常量索引
#define MAXINDEXRK   (BITRK - 1)          // 最大RK索引值 (255)

// 无效寄存器标记
#define NO_REG       MAXARG_A             // 255 (8位最大值)
```

### 4.5 指令创建宏
```c
// 创建各种格式的指令
#define CREATE_ABC(o,a,b,c) ((cast(Instruction, o)<<POS_OP) \
    | (cast(Instruction, a)<<POS_A) \
    | (cast(Instruction, b)<<POS_B) \
    | (cast(Instruction, c)<<POS_C))

#define CREATE_ABx(o,a,bc) ((cast(Instruction, o)<<POS_OP) \
    | (cast(Instruction, a)<<POS_A) \
    | (cast(Instruction, bc)<<POS_Bx))

#define CREATE_Ax(o,a) ((cast(Instruction, o)<<POS_OP) \
    | (cast(Instruction, a)<<POS_Ax))
```

### 4.6 参数类型解释
- **R(x)**: 寄存器 x
- **Kst(x)**: 常量表中索引为 x 的常量
- **RK(x)**: 如果 ISK(x) 则为 Kst(INDEXK(x))，否则为 R(x)
- **sBx**: 有符号跳转偏移 (实际值 = Bx - MAXARG_sBx)
- **UpValue[x]**: 闭包的upvalue x

### 4.7 指令属性系统
```c
// 指令属性掩码 (每个指令8位属性)
enum OpArgMask {
  OpArgN,  // 参数未使用
  OpArgU,  // 参数被使用
  OpArgR,  // 参数是寄存器或跳转偏移
  OpArgK   // 参数是常量或寄存器/常量
};

// 属性位布局:
// 位0-1: 指令格式 (iABC, iABx, iAsBx, iAx)
// 位2-3: C参数模式
// 位4-5: B参数模式  
// 位6: 指令是否设置A寄存器
// 位7: 指令是否为测试指令(下一条必须是跳转)

// 属性查询宏
#define getOpMode(m)  (cast(enum OpMode, luaP_opmodes[m] & 3))
#define getBMode(m)   (cast(enum OpArgMask, (luaP_opmodes[m] >> 4) & 3))
#define getCMode(m)   (cast(enum OpArgMask, (luaP_opmodes[m] >> 2) & 3))
#define testAMode(m)  (luaP_opmodes[m] & (1 << 6))  // 是否设置A寄存器
#define testTMode(m)  (luaP_opmodes[m] & (1 << 7))  // 是否为测试指令
```

### 4.8 Excess K编码详解
```c
// 有符号数编码原理 (以sBx为例)
// 18位字段范围: 0 ~ 262143
// 有符号范围: -131071 ~ +131071
// K = MAXARG_sBx = 131071

// 编码: 存储值 = 实际值 + K
// 解码: 实际值 = 存储值 - K

// 示例:
// 实际值 -100 → 存储为 131071 - 100 = 130971
// 实际值 +100 → 存储为 131071 + 100 = 131171
// 实际值    0 → 存储为 131071 + 0   = 131071
```

### 4.9 指令编码最佳实践

#### 4.9.1 类型安全规范
```c
// 1. 使用强制类型转换确保类型安全
cast(Instruction, value)  // 转换为指令类型
cast(OpCode, value)       // 转换为操作码类型
cast(int, value)          // 转换为整数类型

// 2. 参数范围检查
assert(opcode <= 63);           // 操作码不超过6位
assert(reg_a <= MAXARG_A);      // A寄存器编号有效
assert(const_idx <= MAXARG_Bx); // 常量索引有效

// 3. RK编码正确性验证
if (ISK(operand)) {
    int const_idx = INDEXK(operand);
    assert(const_idx < constants_count);
} else {
    assert(operand < registers_count);
}
```

#### 4.9.2 跨平台兼容性
```c
// 1. 条件编译适应不同平台
#if LUAI_BITSINT >= 32
    // 32位或更大平台的优化路径
#else
    // 16位平台的兼容路径
#endif

// 2. 字节序无关的位操作
// 使用位移和掩码，避免依赖特定字节序
#define SAFE_SHIFT(val, pos) ((val) << (pos))
#define SAFE_MASK(size) ((1U << (size)) - 1)

// 3. 对齐要求处理
typedef union {
    Instruction u;
    struct { char c[4]; } s;
} InstrUnion;  // 确保4字节对齐
```

#### 4.9.3 性能优化技巧
```c
// 1. 批量指令操作
// 使用位运算同时设置多个字段
Instruction instr = CREATE_ABC(opcode, reg_a, reg_b, reg_c);

// 2. 条件编译优化常用路径
#ifdef FAST_PATH
    #define QUICK_GETARG_A(i) ((i) >> 6 & 0xFF)  // 直接位操作
#else
    #define QUICK_GETARG_A(i) GETARG_A(i)        // 通用宏
#endif

// 3. 内联函数减少宏展开开销
static inline int fast_getarg_a(Instruction i) {
    return (i >> POS_A) & MAXARG_A;
}
```

### 4.10 调试和验证工具

#### 4.10.1 指令反汇编
```c
// 指令格式化输出
void print_instruction(Instruction instr, int pc) {
    OpCode op = GET_OPCODE(instr);
    printf("%4d: %-12s", pc, opcode_names[op]);
    
    switch (getOpMode(op)) {
        case iABC:
            printf("A=%d B=%d C=%d", 
                   GETARG_A(instr), GETARG_B(instr), GETARG_C(instr));
            break;
        case iABx:
            printf("A=%d Bx=%d", 
                   GETARG_A(instr), GETARG_Bx(instr));
            break;
        case iAsBx:
            printf("A=%d sBx=%d", 
                   GETARG_A(instr), GETARG_sBx(instr));
            break;
        case iAx:
            printf("Ax=%d", GETARG_Ax(instr));
            break;
    }
}
```

#### 4.10.2 编码正确性验证
```c
// 往返编码测试
void test_encoding_roundtrip() {
    // 测试ABC格式
    OpCode op = OP_ADD;
    int a = 10, b = 20, c = 30;
    Instruction instr = CREATE_ABC(op, a, b, c);
    
    assert(GET_OPCODE(instr) == op);
    assert(GETARG_A(instr) == a);
    assert(GETARG_B(instr) == b);
    assert(GETARG_C(instr) == c);
    
    // 测试sBx编码
    int offset = -1000;
    SETARG_sBx(instr, offset);
    assert(GETARG_sBx(instr) == offset);
}
```

### 4.11 与AQL特性的集成

#### 4.11.1 AQL编译器的指令选择逻辑 (借鉴Lua优化)
```c
// AQL编译器智能选择算术指令版本
static void aql_codearith(FuncState *fs, BinOpr opr, 
                          expdesc *e1, expdesc *e2, int line) {
    
    // 第一优先级: 立即数优化 (I版本) - 性能最佳
    if (isSmallInt(e2) && fitsInSC(e2->u.ival)) {
        switch (opr) {
            case OPR_ADD: codeABI(fs, OP_ADDI, e1, e2, line); break;
            case OPR_SUB: codeABI(fs, OP_SUBI, e1, e2, line); break;
            case OPR_MUL: codeABI(fs, OP_MULI, e1, e2, line); break;
            case OPR_DIV: codeABI(fs, OP_DIVI, e1, e2, line); break;
            default: goto use_k_or_reg;
        }
        return;
    }
    
use_k_or_reg:
    // 第二优先级: 常量表优化 (K版本) - 性能良好
    if (isConstant(e2) && aql_exp2K(fs, e2)) {
        switch (opr) {
            case OPR_ADD: codeABK(fs, OP_ADDK, e1, e2, line); break;
            case OPR_SUB: codeABK(fs, OP_SUBK, e1, e2, line); break;
            case OPR_MUL: codeABK(fs, OP_MULK, e1, e2, line); break;
            case OPR_DIV: codeABK(fs, OP_DIVK, e1, e2, line); break;
            default: goto use_reg;
        }
        return;
    }
    
use_reg:
    // 第三优先级: 通用寄存器版本 (功能完整)
    switch (opr) {
        case OPR_ADD: codeABC(fs, OP_ADD, e1, e2, line); break;
        case OPR_SUB: codeABC(fs, OP_SUB, e1, e2, line); break;
        case OPR_MUL: codeABC(fs, OP_MUL, e1, e2, line); break;
        case OPR_DIV: codeABC(fs, OP_DIV, e1, e2, line); break;
        case OPR_MOD: codeABC(fs, OP_MOD, e1, e2, line); break;
        case OPR_POW: codeABC(fs, OP_POW, e1, e2, line); break;
    }
}

// 立即数范围检查 (8位有符号: -128 ~ +127)
static int fitsInSC(lua_Integer i) {
    return (i >= -128 && i <= 127);
}

// 小整数检查 (常用的小数值)
static int isSmallInt(expdesc *e) {
    return (e->k == VKINT && !hasJumps(e));
}
```

#### 4.11.2 性能对比和适用场景
```c
// 性能提升效果 (相对于通用寄存器版本)
struct PerformanceGain {
    InstrType type;
    float speedup;      // 相对速度提升
    char* use_case;     // 典型使用场景
};

const struct PerformanceGain aql_optimizations[] = {
    // I版本 - 立即数优化
    {OP_ADDI, 2.0f, "x + 1, i += 5, count++"},
    {OP_SUBI, 2.0f, "x - 2, i -= 10, ptr--"},  
    {OP_MULI, 1.8f, "x * 2, size * 4"},
    {OP_DIVI, 1.6f, "x / 2, half = n / 2"},
    
    // K版本 - 常量表优化  
    {OP_ADDK, 1.5f, "x + PI, y + 3.14159"},
    {OP_SUBK, 1.5f, "temp - OFFSET"},
    {OP_MULK, 1.4f, "area * SCALE_FACTOR"},
    {OP_DIVK, 1.3f, "distance / SPEED_OF_LIGHT"},
    
    // 通用版本 - 基准性能
    {OP_ADD,  1.0f, "a + b, x + y (动态值)"},
    {OP_SUB,  1.0f, "end - start"},
    {OP_MUL,  1.0f, "width * height"},
    {OP_DIV,  1.0f, "sum / count"}
};
```

#### 4.11.3 AQL的K/I优化示例
```aql
// AQL源代码示例
let x = a + 5        // 编译为: ADDI R1, R0, 5    (立即数优化)
let y = b + PI       // 编译为: ADDK R2, R1, K[0] (常量表优化) 
let z = c + d        // 编译为: ADD  R3, R2, R3   (寄存器版本)

// 容器索引优化
arr[0] = value       // 编译为: SETPROP R0, 0, R1    (整数索引)
arr[i] = value       // 编译为: SETPROP R0, R1, R2   (动态索引)

// 函数返回优化
func void_func() {   // 编译为: RET_VOID          (无返回值优化)
    // ...
}

func single_func() { // 编译为: RET_ONE R0        (单返回值优化)
    return x + 1
}

// 循环计数器优化  
for i in 0..100 {    // 循环增量使用ADDI优化
    // i++  编译为: ADDI R_i, R_i, 1
}

// 数组字面量初始化
let arr = [1, 2, 3, 4, 5]  // 编译为:
// NEWOBJECT R0, TYPE_ARRAY, 5    (创建数组)
// LOADK R1, K[1]; LOADK R2, K[2]; ... (加载常量)
// BUILTIN R0, BUILTIN_ARRAY_INIT, R1 (批量初始化)
```

#### 4.11.2 向量化指令编码
```c
// VMETA指令用于向量操作
enum VectorOp {
    VOP_ADD = 0,    // 向量加法
    VOP_MUL = 1,    // 向量乘法
    VOP_DOT = 2,    // 点积
    VOP_MAX = 3,    // 最大值
    // ...
};

// VMETA A B C: R(A) := vector_op[B](R(C), R(C+1))
Instruction vec_add = CREATE_ABC(OP_VMETA,
                                 result_reg,  // A: 结果向量
                                 VOP_ADD,     // B: 向量操作类型
                                 vec1_reg);   // C: 第一个向量寄存器
```

#### 4.11.3 模式匹配指令编码
```c
// MATCH指令的特殊编码
// MATCH A B C: R(A) := match R(B) with patterns[C]
Instruction match_instr = CREATE_ABC(OP_MATCH,
                                     result_reg,    // A: 匹配结果
                                     value_reg,     // B: 被匹配的值
                                     pattern_idx);  // C: 模式表索引

// EXTRACT指令提取匹配结果
// EXTRACT A B C: R(A) := extract_field[C] from match_result[B]
Instruction extract_instr = CREATE_ABC(OP_EXTRACT,
                                       field_reg,   // A: 提取的字段
                                       match_reg,   // B: 匹配结果
                                       field_idx);  // C: 字段索引
```

## 5. 优化考虑

### 5.1 性能优化
- 常用指令使用更短的编码
- 关键路径指令优化
- 指令融合机会
- 寄存器分配优化

### 5.2 内存优化
- 指令编码紧凑
- 常量池优化
- 寄存器重用

### 5.3 并发优化
- 原子操作支持
- 协程切换开销最小化
- 异步操作效率优化

## 6. 扩展性

预留操作码空间用于：
- 新的AI服务指令
- JIT优化指令
- 特定领域扩展
- 调试和分析指令

## 7. 指令属性系统 (借鉴Lua的OpMode设计)

### 7.1 指令属性编码
```c
// 指令属性掩码 (每个指令8位属性)
// 位0-1: 指令格式 (iABC, iABx, iAsBx, iAx)
// 位2-3: C参数模式
// 位4-5: B参数模式  
// 位6: 指令是否设置A寄存器
// 位7: 指令是否为测试指令(下一条必须是跳转)

enum OpArgMask {
  OpArgN,  // 参数未使用
  OpArgU,  // 参数被使用
  OpArgR,  // 参数是寄存器或跳转偏移
  OpArgK   // 参数是常量或寄存器/常量
};
```

### 7.2 指令优化特性
- **Test模式指令**: EQ, LT, LE, TEST, TESTSET (下一条指令必须是跳转)
- **寄存器设置**: 大部分指令设置A寄存器，比较指令不设置
- **参数模式**: 精确定义每个参数的使用方式，便于编译器优化

## 8. AQL容器系统实现细节

### 8.1 容器操作指令映射

#### 8.1.1 替代Lua表操作的策略
```c
// Lua表操作 → AQL容器操作的完整映射

// 1. 对象创建
Lua: NEWTABLE R0, arraysize, hashsize
AQL: NEWOBJECT R0, TYPE_ARRAY/SLICE/DICT, capacity

// 2. 属性/索引访问统一
Lua: GETTABLE R0, R1, R2    // R0 = R1[R2]
Lua: GETFIELD R0, R1, K[2]  // R0 = R1.field
Lua: GETI R0, R1, 5         // R0 = R1[5]
AQL: GETPROP R0, R1, R2     // 统一处理所有情况

// 3. 属性/索引设置统一  
Lua: SETTABLE R0, R1, R2    // R0[R1] = R2
Lua: SETFIELD R0, K[1], R2  // R0.field = R2
Lua: SETI R0, 5, R2         // R0[5] = R2
AQL: SETPROP R0, R1, R2     // 统一处理所有情况

// 4. 方法调用
Lua: SELF R0, R1, K[2]; CALL R0, 2, 1  // R0, R1 = R1:method(); R0(R1)
AQL: INVOKE R0, R1, "method", R2        // R0 = R1:method(R2)

// 5. 批量初始化
Lua: SETLIST R0, count, startindex     // R0[start..end] = R1..Rn
AQL: BUILTIN R0, BUILTIN_INIT_BATCH, R1 // 调用内置批量初始化
```

#### 8.1.2 INVOKE指令的强大功能
```c
// INVOKE指令可以替代多个Lua指令的组合
// INVOKE A B C D: R(A) = R(B):method[C](R(D), ...)

// 数组操作示例
INVOKE R0, R1, "append", R2      // arr.append(value)
INVOKE R0, R1, "insert", R2, R3  // arr.insert(index, value)  
INVOKE R0, R1, "remove", R2      // arr.remove(index)
INVOKE R0, R1, "length", 0       // arr.length()

// 字典操作示例
INVOKE R0, R1, "set", R2, R3     // dict.set(key, value)
INVOKE R0, R1, "get", R2         // dict.get(key)
INVOKE R0, R1, "keys", 0         // dict.keys()
INVOKE R0, R1, "values", 0       // dict.values()

// 切片操作示例
INVOKE R0, R1, "slice", R2, R3   // slice[start:end]
INVOKE R0, R1, "concat", R2      // slice1.concat(slice2)
```

### 8.2 批量操作的内置函数实现
```c
// 内置函数表中的批量操作支持
builtin_functions[] = {
    // 批量初始化函数 (替代SETLIST)
    {"array_init", array_init_impl},      // 从栈批量初始化数组
    {"dict_init", dict_init_impl},        // 从键值对批量初始化字典
    {"slice_from_range", slice_range_impl}, // 从范围创建切片
    
    // 批量操作函数
    {"array_fill", array_fill_impl},      // 填充数组
    {"slice_map", slice_map_impl},        // 映射操作
    {"dict_merge", dict_merge_impl},      // 字典合并
};

// 示例: 数组字面量编译过程
// AQL源码: let arr = [1, 2, 3, 4, 5]
// 编译结果:
NEWOBJECT R0, TYPE_ARRAY, 5           // 创建容量为5的数组
LOADK R1, K[1]                        // 加载常量1
LOADK R2, K[2]                        // 加载常量2  
LOADK R3, K[3]                        // 加载常量3
LOADK R4, K[4]                        // 加载常量4
LOADK R5, K[5]                        // 加载常量5
BUILTIN R0, BUILTIN_ARRAY_INIT, R1    // 调用批量初始化: R0.init(R1..R5)
```

## 9. 调试和扩展支持

### 9.1 调试指令
- LINE      记录当前行号信息
- SETBREAK  设置断点
- GETINFO   获取调试信息

### 9.2 性能分析
- PROFILE   性能分析点
- TRACE     执行跟踪 

### 9.3 AQL vs Lua 指令集对比

| 特性 | Lua 5.4 (82条) | AQL Phase 1 (64条) | 说明 |
|------|---------------|-------------------|------|
| **数据结构** | table统一(10条指令) | 容器分离(4条指令) | AQL类型更明确，指令更简洁 |
| **函数返回** | RETURN/RETURN0/RETURN1 | RET/RET_VOID/RET_ONE | AQL使用更清晰的命名 |
| **比较操作** | 9条比较指令 | 8条比较指令 | AQL精简了低频比较指令 |
| **元方法** | 3条MM指令 | 0条 | AQL无metatable系统 |
| **位运算K版本** | 3条BANDK系列 | 0条 | AQL暂不支持，可通过通用版本实现 |
| **循环指令** | 5条FOR指令 | 2条FOR指令 | AQL保留核心循环功能 |
| **扩展性** | 固定82条 | 64条+预留空间 | AQL为AI特性预留扩展空间 |
| **容器优势** | table歧义性 | 类型安全容器 | array/slice/dict明确区分 |
| **AI就绪** | 无 | BUILTIN/INVOKE支持 | AQL可通过内置函数扩展AI功能 |

### 9.4 设计决策总结

#### 9.4.1 保留的Lua优化
✅ **K/I版本算术指令**: ADDK/ADDI等，性能提升明显  
✅ **返回值优化指令**: RET_VOID/RET_ONE，避免通用返回开销  
✅ **尾调用优化**: TAILCALL，实现O(1)栈空间递归  
✅ **立即数比较**: EQI/LTI，常用模式优化

#### 9.4.2 简化的设计选择  
🔄 **统一容器操作**: 4条指令替代Lua的10条表操作  
🔄 **精简比较指令**: 8条覆盖所有比较需求  
🔄 **方法调用统一**: INVOKE替代SELF+CALL组合  
🔄 **批量操作软件化**: BUILTIN替代SETLIST等专用指令

#### 9.4.3 删除的冗余功能
❌ **元方法系统**: AQL无metatable，删除3条MM指令  
❌ **位运算K版本**: 使用频率低，可通过通用版本实现  
❌ **部分比较指令**: EQK等可通过编译器优化处理

### 8.4 精简后的指令统计和布局

```
=== AQL Phase 1 完整指令集 (总计64条) ===

=== 基础指令组 (0-15): 16条 ===
加载存储指令:
├── 0-8: MOVE, LOADI, LOADF, LOADK, LOADKX, LOADFALSE, LOADTRUE, LOADNIL, GETUPVAL (9条)
└── 9-15: SETUPVAL, GETTABUP, SETTABUP, CLOSE, TBC, CONCAT, EXTRAARG (7条)

=== 算术运算组 (16-31): 16条 ===  
算术指令 (含K/I优化):
├── 16-19: ADD, ADDK, ADDI, SUB (4条)
├── 20-23: SUBK, SUBI, MUL, MULK (4条)  
├── 24-27: MULI, DIV, DIVK, DIVI (4条)
└── 28-31: MOD, POW, UNM, LEN (4条)

=== 位运算组 (32-39): 8条 ===
位运算指令:
├── 32-35: BAND, BOR, BXOR, SHL (4条)
└── 36-39: SHR, BNOT, NOT, SHRI (4条)

=== 比较控制组 (40-47): 8条 ===
比较和跳转指令:
├── 40-43: JMP, EQ, LT, LE (4条)  
└── 44-47: TEST, TESTSET, EQI, LTI (4条)

=== 函数调用组 (48-55): 8条 ===
函数和循环指令:
├── 48-51: CALL, TAILCALL, RET, RET_VOID (4条)
└── 52-55: RET_ONE, FORLOOP, FORPREP, CLOSURE (4条)

=== AQL容器组 (56-59): 4条 ===
AQL专用容器操作:
└── 56-59: NEWOBJECT, GETPROP, SETPROP, INVOKE (4条)

=== AQL扩展组 (60-63): 4条 ===
AQL特有功能:
└── 60-63: YIELD, RESUME, BUILTIN, VARARG (4条)

=== 指令优化说明 ===
• 完整兼容Lua核心功能，但简化了table系统
• K版本 (常量表): 性能提升 1.3-1.5x，适用于 x + PI  
• I版本 (立即数): 性能提升 1.6-2.0x，适用于 x + 1, i++
• 容器系统: 用4条通用指令替代Lua的10条表操作指令
• 函数返回优化: RET_VOID/RET_ONE避免了通用返回的开销
• 尾调用优化: TAILCALL实现O(1)栈空间的递归调用
```

### 8.4.1 向量化优化支持级别

**Level 1: 基础向量支持**
- 向量数据类型创建和管理
- 基础向量算术运算
- 向量内存操作

**Level 2: SIMD指令映射**
- 直接映射到x86 SSE/AVX指令
- ARM NEON指令集支持
- 自动向量宽度检测

**Level 3: 自动向量化**
- 编译器自动识别向量化机会
- 循环向量化优化
- 数据并行处理

**Level 4: AI加速支持**
- 张量操作向量化
- 矩阵乘法优化
- GPU向量化卸载

### 8.5 向量化优化实现策略

#### 8.5.1 硬件抽象层
```c
// 向量化抽象接口
typedef struct VectorOps {
    void (*vadd)(void* dst, void* src1, void* src2, size_t len);
    void (*vmul)(void* dst, void* src1, void* src2, size_t len);
    void (*vdot)(void* dst, void* src1, void* src2, size_t len);
    // ... 其他向量操作
} VectorOps;

// 运行时检测和选择最优实现
static VectorOps* select_vector_impl() {
    if (cpu_supports_avx512()) return &avx512_ops;
    if (cpu_supports_avx2()) return &avx2_ops;
    if (cpu_supports_sse4()) return &sse4_ops;
    return &scalar_ops;
}
```

#### 8.5.2 编译器优化集成
- **LLVM集成**: 直接生成LLVM向量化IR
- **自动向量化**: 识别可向量化的循环模式
- **内在函数**: 直接映射到硬件SIMD指令

#### 8.5.3 窗口函数向量化优化 (类似pandas)
```aql
// 传统标量实现 (慢)
function rolling_sum_scalar(data: []float, window: int): []float {
    result: []float = []
    for i in range(window-1, len(data)) {
        sum: float = 0.0
        for j in range(i-window+1, i+1) {
            sum += data[j]  // 标量加法，无法并行
        }
        result.append(sum)
    }
    return result
}

// 向量化实现 (快)
@vectorize
function rolling_sum_vector(data: []float, window: int): []float {
    result: []float = []
    
    // 使用向量化滑动窗口
    for i in range(window-1, len(data), 8) {  // 步长8，处理8个窗口
        // 加载8个连续的窗口数据到向量寄存器
        vec_data: vector<float> = load_vector(data[i-window+1:i+8])
        
        // 使用VADD指令并行计算8个窗口的和
        vec_sums: vector<float> = vector_rolling_sum(vec_data, window)
        
        // 存储结果
        result.extend(vec_sums.to_slice())
    }
    return result
}

// 专门的窗口函数指令
function pandas_like_operations(data: []float): dict<string, []float> {
    return {
        // 滚动窗口求和 (使用VSUM + 滑动窗口)
        "rolling_sum_3": data.rolling(3).sum(),      // VSUM指令
        
        // 滚动窗口平均 (使用VSUM + VDIV)
        "rolling_mean_5": data.rolling(5).mean(),    // VSUM + scalar divide
        
        // 滚动窗口最大值 (使用VMAX)
        "rolling_max_7": data.rolling(7).max(),      // VMAX指令
        
        // 滚动窗口标准差 (组合向量指令)
        "rolling_std_10": data.rolling(10).std(),    // VSUB + VMUL + VSUM + VSQRT
        
        // 指数加权移动平均 (使用VMUL + VADD)
        "ewm_alpha_0.1": data.ewm(alpha=0.1).mean()  // VMUL + VADD循环
    }
}

// 高级窗口函数向量化
@window_vectorize(size=8)  // 一次处理8个窗口
function advanced_rolling_ops(prices: []float, volumes: []float) {
    // 向量化的VWAP计算
    vwap: []float = rolling_apply(window=20) {
        price_vol: vector<float> = VMUL(prices, volumes)    // 价格*成交量
        total_vol: vector<float> = VSUM(volumes)            // 总成交量
        return VDIV(VSUM(price_vol), total_vol)             // VWAP = Σ(P*V) / ΣV
    }
    
    // 向量化的移动相关系数
    correlation: []float = rolling_apply(window=30) {
        mean_x: vector<float> = VSUM(prices) / 30
        mean_y: vector<float> = VSUM(volumes) / 30
        
        x_diff: vector<float> = VSUB(prices, mean_x)
        y_diff: vector<float> = VSUB(volumes, mean_y)
        
        numerator: vector<float> = VSUM(VMUL(x_diff, y_diff))
        denom_x: vector<float> = VSQRT(VSUM(VMUL(x_diff, x_diff)))
        denom_y: vector<float> = VSQRT(VSUM(VMUL(y_diff, y_diff)))
        
        return VDIV(numerator, VMUL(denom_x, denom_y))
    }
    
    return {vwap, correlation}
}
```

#### 8.5.4 性能对比分析
```aql

```

### 8.5 精简优化效果对比

### 8.6 精简设计原则

1. **保留Lua核心**: 完全兼容Lua的40条基础指令
2. **通用化替代专用化**: 用4条通用数据指令替代18条专用指令
3. **软件层实现高级功能**: AI、类型检查、模块等通过内置函数实现
4. **向量化统一接口**: 3条通用向量指令支持所有SIMD操作
5. **最小化指令集**: 55条指令覆盖所有核心功能
6. **预留扩展空间**: 8个槽位用于未来扩展

### 8.7 功能实现策略

#### **通过指令实现 (核心功能)**
- 数据操作: NEWOBJECT, GETPROP, SETPROP, INVOKE
- 异步编程: YIELD, RESUME, AWAIT, ASYNC  
- 模式匹配: MATCH, EXTRACT
- 向量化: VMETA, VWINDOW, VMAP

#### **通过内置函数实现 (扩展功能)**
```c
builtin_functions[] = {
    // AI服务
    {"openai_chat", openai_chat_impl},
    {"anthropic_chat", anthropic_chat_impl},
    
    // 类型系统
    {"typeof", typeof_impl},
    {"isinstance", isinstance_impl},
    {"cast", cast_impl},
    
    // 窗口函数
    {"hhv", hhv_impl},     // 替代VWINMAX
    {"sma", sma_impl},     // 替代VWINMEAN
    {"ema", ema_impl},     // 替代VEWM
    
    // 模块系统
    {"import", import_impl},
    {"export", export_impl},
    
    // 字符串模板
    {"template", template_impl},
    {"interpolate", interpolate_impl},
};
```

## 10. 设计优势总结

### 10.1 性能优势
- **K/I优化**: 关键算术操作1.5-2.0x性能提升
- **返回优化**: RET_VOID/RET_ONE避免通用返回开销
- **容器类型安全**: 编译时类型检查，运行时无额外开销
- **SIMD就绪**: 向量容器支持自动向量化

### 10.2 架构优势  
- **指令精简**: 64条vs Lua的82条，减少18条冗余指令
- **扩展灵活**: BUILTIN机制支持动态功能扩展
- **类型明确**: array/slice/dict替代模糊的table概念
- **AI就绪**: 为AI特性预留了充足的指令空间

### 10.3 实现优势
- **简化VM**: 更少的指令处理逻辑，更易维护
- **统一操作**: GETPROP/SETPROP/INVOKE统一容器操作
- **软件扩展**: 复杂功能通过内置函数实现，易于优化
- **向前兼容**: 为Phase 2的AI特性打下坚实基础

这种精简而强大的设计既保持了虚拟机的高效性，又为未来的AI原生特性提供了充足的扩展空间。🎉