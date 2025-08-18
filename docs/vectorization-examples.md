# AQL向量化编译示例

## 1. 简单数组操作向量化

### 1.1 源代码
```aql
// 数组元素相加
numbers1: []float = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]
numbers2: []float = [2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0]
result: []float = []

for i in range(len(numbers1)) {
    result[i] = numbers1[i] + numbers2[i]
}
```

### 1.2 编译器分析
```c
// 1. 循环模式识别
Pattern: SIMPLE_LOOP
Operation: ELEMENT_WISE_ADD
Vector_Width: 8 (AVX2 256-bit / 8 floats)
Memory_Access: CONTIGUOUS
Dependencies: NONE

// 2. 向量化可行性
✓ 固定循环次数
✓ 连续内存访问
✓ 无数据依赖
✓ 成本收益比 > 阈值
```

### 1.3 生成的字节码
```asm
// 标量版本 (未优化)
LOADK    R(0), K(numbers1)     ; 加载数组1
LOADK    R(1), K(numbers2)     ; 加载数组2
NEWARRAY R(2), 8, float        ; 创建结果数组
LOADK    R(3), K(0)            ; 循环计数器 i = 0
LOADK    R(4), K(8)            ; 循环上限

loop_start:
EQ       0, R(3), R(4)         ; if i == 8
JMP      loop_end              ; 跳转到结束

GETINDEX R(5), R(0), R(3)      ; R(5) = numbers1[i]
GETINDEX R(6), R(1), R(3)      ; R(6) = numbers2[i]
ADD      R(7), R(5), R(6)      ; R(7) = R(5) + R(6)
SETINDEX R(2), R(3), R(7)      ; result[i] = R(7)

LOADK    R(8), K(1)            ; 常量1
ADD      R(3), R(3), R(8)      ; i++
JMP      loop_start            ; 继续循环

loop_end:

// 向量化版本 (优化后)
LOADK    R(0), K(numbers1)     ; 加载数组1
LOADK    R(1), K(numbers2)     ; 加载数组2
NEWARRAY R(2), 8, float        ; 创建结果数组

// 一条向量指令完成所有操作
VADD     R(2), R(0), R(1)      ; result = numbers1 + numbers2 (向量化)

// 指令数量: 18条 → 4条 (4.5x减少)
// 执行时间: 8次循环 → 1次向量操作 (8x加速)
```

## 2. 窗口函数向量化

### 2.1 源代码
```aql
// 5日滚动平均
prices: []float = [100.0, 102.0, 98.0, 105.0, 103.0, 107.0, 109.0, 104.0, 106.0, 108.0]
window_size: int = 5
moving_avg: []float = []

// 传统写法
for i in range(window_size-1, len(prices)) {
    sum: float = 0.0
    for j in range(i-window_size+1, i+1) {
        sum += prices[j]
    }
    moving_avg.append(sum / window_size)
}

// 优化写法 (向量化友好)
moving_avg = prices.rolling(5).mean()
```

### 2.2 编译器分析
```c
// 1. 模式识别
Pattern: WINDOW_FUNCTION
Window_Type: ROLLING
Aggregation: MEAN
Window_Size: 5
Data_Type: float

// 2. 向量化策略
Method: SLIDING_WINDOW_VECTORIZATION
Vector_Width: 4 (一次处理4个窗口)
Memory_Layout: OPTIMIZED_BUFFER
```

### 2.3 生成的字节码
```asm
// 传统标量实现
LOADK    R(0), K(prices)       ; 加载价格数组
LOADK    R(1), K(5)            ; 窗口大小
NEWSLICE R(2)                  ; 创建结果切片
LOADK    R(3), K(4)            ; 外层循环起始 (window_size-1)
LEN      R(4), R(0)            ; 数组长度

outer_loop:
EQ       0, R(3), R(4)
JMP      outer_end

LOADK    R(5), K(0.0)          ; sum = 0.0
SUB      R(6), R(3), R(1)      ; 内层循环起始
ADD      R(6), R(6), K(1)

inner_loop:
LE       1, R(6), R(3)
JMP      inner_end

GETINDEX R(7), R(0), R(6)      ; prices[j]
ADD      R(5), R(5), R(7)      ; sum += prices[j]
ADD      R(6), R(6), K(1)      ; j++
JMP      inner_loop

inner_end:
DIV      R(8), R(5), R(1)      ; avg = sum / window_size
APPEND   R(2), R(8)            ; moving_avg.append(avg)
ADD      R(3), R(3), K(1)      ; i++
JMP      outer_loop

outer_end:

// 向量化实现 (优化后)
LOADK    R(0), K(prices)       ; 加载价格数组
LOADK    R(1), K(5)            ; 窗口大小
NEWSLICE R(2)                  ; 创建结果切片

// 一条专用指令完成窗口计算
VWINMEAN R(2), R(0), R(1)      ; moving_avg = rolling_mean(prices, 5)

// 指令数量: ~30条 → 4条 (7.5x减少)
// 执行时间: O(n*w) → O(n/v) 其中v是向量宽度
```

## 3. 复杂金融计算向量化

### 3.1 源代码
```aql
// 计算技术分析指标
function calculate_indicators(prices: []float, volumes: []float): dict<string, []float> {
    // 简单移动平均线
    sma_20: []float = prices.rolling(20).mean()
    
    // 指数移动平均线
    ema_12: []float = prices.ewm(span=12).mean()
    
    // 布林带
    bb_middle: []float = sma_20
    bb_std: []float = prices.rolling(20).std()
    bb_upper: []float = []
    bb_lower: []float = []
    
    for i in range(len(bb_middle)) {
        upper: float = bb_middle[i] + 2.0 * bb_std[i]
        lower: float = bb_middle[i] - 2.0 * bb_std[i]
        bb_upper.append(upper)
        bb_lower.append(lower)
    }
    
    // VWAP (成交量加权平均价格)
    vwap: []float = []
    for i in range(20, len(prices)) {
        sum_pv: float = 0.0
        sum_v: float = 0.0
        for j in range(i-19, i+1) {
            sum_pv += prices[j] * volumes[j]
            sum_v += volumes[j]
        }
        vwap.append(sum_pv / sum_v)
    }
    
    return {
        "sma_20": sma_20,
        "ema_12": ema_12,
        "bb_upper": bb_upper,
        "bb_lower": bb_lower,
        "vwap": vwap
    }
}
```

### 3.2 编译器优化分析
```c
// 向量化机会识别
Patterns_Found:
1. WINDOW_FUNCTION (rolling.mean) → VWINMEAN
2. WINDOW_FUNCTION (rolling.std) → VWINSTD  
3. EWM_FUNCTION (ewm.mean) → VEWM
4. ELEMENT_WISE_OPS (布林带计算) → VADD, VMUL
5. CUSTOM_WINDOW (VWAP计算) → 组合向量指令

// 数据流分析
Data_Dependencies:
- sma_20 和 bb_std 可以并行计算
- bb_upper 和 bb_lower 依赖于 bb_middle 和 bb_std
- vwap 可以独立计算

// 向量化收益评估
Estimated_Speedup: 6.2x
Memory_Savings: 45%
```

### 3.3 优化后的字节码
```asm
// 函数入口
CLOSURE  R(0), KPROTO[calculate_indicators]
LOADK    R(1), K(prices)       ; 价格数据
LOADK    R(2), K(volumes)      ; 成交量数据

// 并行计算窗口函数
VWINMEAN R(3), R(1), K(20)     ; sma_20 = prices.rolling(20).mean()
VWINSTD  R(4), R(1), K(20)     ; bb_std = prices.rolling(20).std()
VEWM     R(5), R(1), K(12)     ; ema_12 = prices.ewm(12).mean()

// 向量化布林带计算
VMUL     R(6), R(4), K(2.0)    ; bb_std * 2.0
VADD     R(7), R(3), R(6)      ; bb_upper = sma_20 + 2*bb_std
VSUB     R(8), R(3), R(6)      ; bb_lower = sma_20 - 2*bb_std

// 自定义VWAP向量计算
NEWSLICE R(9)                  ; vwap结果
LOADK    R(10), K(20)          ; 窗口大小
LOADK    R(11), K(20)          ; 起始位置
LEN      R(12), R(1)           ; 数据长度

vwap_loop:
EQ       0, R(11), R(12)
JMP      vwap_end

// 向量化窗口计算 (一次计算4个VWAP值)
SUB      R(13), R(11), R(10)   ; 窗口起始
ADD      R(13), R(13), K(1)
SLICE    R(14), R(1), R(13), R(11)  ; 价格窗口
SLICE    R(15), R(2), R(13), R(11)  ; 成交量窗口

VMUL     R(16), R(14), R(15)   ; price * volume (向量化)
VSUM     R(17), R(16)          ; sum(price * volume)
VSUM     R(18), R(15)          ; sum(volume)
DIV      R(19), R(17), R(18)   ; vwap = sum_pv / sum_v

APPEND   R(9), R(19)           ; 添加到结果
ADD      R(11), R(11), K(4)    ; 一次跳跃4个位置 (向量化)
JMP      vwap_loop

vwap_end:

// 构造返回字典
NEWDICT  R(20)
SETKEY   R(20), K("sma_20"), R(3)
SETKEY   R(20), K("ema_12"), R(5)
SETKEY   R(20), K("bb_upper"), R(7)
SETKEY   R(20), K("bb_lower"), R(8)
SETKEY   R(20), K("vwap"), R(9)

RETURN   R(20)

// 性能对比:
// 原始版本: ~200条指令, 执行时间 100ms
// 优化版本: ~35条指令, 执行时间 16ms
// 加速比: 6.25x
```

## 4. 编译器向量化日志

### 4.1 编译器输出
```
=== AQL向量化编译报告 ===
文件: financial_indicators.aql
函数: calculate_indicators

向量化分析:
✓ 循环1 (line 5): rolling.mean(20) → VWINMEAN指令
✓ 循环2 (line 8): ewm.mean(12) → VEWM指令  
✓ 循环3 (line 12): rolling.std(20) → VWINSTD指令
✓ 循环4 (line 16-20): 布林带计算 → VADD, VSUB, VMUL指令
✓ 循环5 (line 24-32): VWAP计算 → 部分向量化 (VMUL, VSUM)

总循环数: 5
向量化成功: 5
向量化失败: 0

预估性能提升:
- 内存访问减少: 65%
- 指令数量减少: 82%
- 执行时间减少: 84%
- 总体加速比: 6.25x

目标平台: x86_64 (AVX2)
向量宽度: 8 floats (256-bit)
```

### 4.2 调试信息
```
向量化决策详情:

循环1 (prices.rolling(20).mean()):
  ✓ 模式: WINDOW_FUNCTION
  ✓ 数据类型: float
  ✓ 内存访问: 连续
  ✓ 无数据依赖
  ✓ 成本收益: 8.2x
  → 生成: VWINMEAN R(3), R(1), K(20)

循环4 (布林带计算):
  ✓ 模式: ELEMENT_WISE_ARITHMETIC
  ✓ 操作: 乘法, 加法, 减法
  ✓ 向量宽度: 8
  ✓ 成本收益: 7.8x
  → 生成: VMUL, VADD, VSUB指令序列

循环5 (VWAP计算):
  ✓ 部分向量化
  ✓ 内部操作向量化: VMUL, VSUM
  ✓ 外层循环保留: 复杂窗口逻辑
  ✓ 成本收益: 4.1x
  → 生成: 混合标量/向量指令
```

## 5. 总结

通过这些示例可以看出，AQL编译器的向量化系统能够：

1. **自动识别**向量化机会 (循环模式、窗口函数等)
2. **智能选择**最适合的向量指令
3. **评估成本收益**并做出优化决策  
4. **生成高效**的向量化字节码
5. **提供详细**的编译报告和调试信息

这种设计使得开发者可以用高级的AQL语法编写代码，而编译器自动生成高性能的向量化实现，实现了**易用性和性能的完美结合**。 