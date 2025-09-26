# 🧪 AQL栈优化测试计划

## 📋 测试概述

本文档详细描述了AQL栈优化的全面测试策略，目标是验证200,000+层递归能力，确保性能、稳定性和兼容性。

## 🎯 测试目标

### 主要目标
- ✅ 验证递归深度从47层提升到200,000+层
- ✅ 确保尾调用优化正常工作
- ✅ 验证容器感知栈管理
- ✅ 保证向后兼容性
- ✅ 监控内存使用和性能

### 性能基准
| 指标 | 当前值 | 目标值 | 验证方法 |
|------|--------|--------|----------|
| 最大递归深度 | 47层 | 200,000+层 | 深度递归测试 |
| 栈内存使用 | N/A | <100MB(200k层) | 内存监控 |
| 重新分配开销 | N/A | <5%执行时间 | 性能分析 |
| 尾调用优化率 | 0% | >95% | 优化统计 |

## 🧪 测试用例设计

### 1. 基础递归测试套件

#### 1.1 渐进深度测试
```aql
// test_progressive_recursion.aql
// 测试10, 100, 1000, 10000层递归

function countdown(n) {
    if n <= 0 {
        return 0
    }
    return countdown(n - 1) + 1
}

// 测试用例
let depths = [10, 100, 1000, 10000]
for depth in depths {
    print("Testing recursion depth:", depth)
    let start_time = time()
    let result = countdown(depth)
    let end_time = time()
    print("Result:", result, "Time:", end_time - start_time, "ms")
}
```

#### 1.2 深度递归测试
```aql
// test_deep_recursion.aql
// 测试50000, 100000, 200000层递归

function deep_countdown(n) {
    if n <= 0 {
        return n
    }
    return deep_countdown(n - 1)
}

// 深度测试用例
let deep_depths = [50000, 100000, 200000]
for depth in deep_depths {
    print("Testing deep recursion:", depth)
    let memory_before = get_memory_usage()
    let start_time = time()
    
    let result = deep_countdown(depth)
    
    let end_time = time()
    let memory_after = get_memory_usage()
    
    print("Depth:", depth)
    print("Result:", result)
    print("Time:", end_time - start_time, "ms")
    print("Memory used:", memory_after - memory_before, "bytes")
    print("---")
}
```

### 2. 尾调用优化测试套件

#### 2.1 尾递归测试
```aql
// test_tail_recursion.aql
// 测试尾调用优化

// 标准尾递归函数
function tail_countdown(n, acc) {
    if n <= 0 {
        return acc
    }
    // 这应该被优化为尾调用
    return tail_countdown(n - 1, acc + 1)
}

// 非尾递归函数（用于对比）
function non_tail_countdown(n) {
    if n <= 0 {
        return 0
    }
    // 这不能被优化为尾调用
    return non_tail_countdown(n - 1) + 1
}

// 测试无限尾递归（理论上应该不会栈溢出）
function infinite_tail_loop(count) {
    print("Tail call count:", count)
    if count > 1000000 {  // 100万次调用
        return count
    }
    return infinite_tail_loop(count + 1)
}

// 执行测试
print("=== 尾调用优化测试 ===")
print("尾递归结果:", tail_countdown(100000, 0))
print("非尾递归结果:", non_tail_countdown(1000))  // 较小数值避免栈溢出
print("无限尾递归测试:", infinite_tail_loop(1))
```

#### 2.2 相互尾调用测试
```aql
// test_mutual_tail_recursion.aql
// 测试相互尾调用优化

function even_tail(n) {
    if n == 0 {
        return true
    }
    return odd_tail(n - 1)  // 尾调用
}

function odd_tail(n) {
    if n == 0 {
        return false
    }
    return even_tail(n - 1)  // 尾调用
}

// 测试大数值的相互尾调用
print("=== 相互尾调用测试 ===")
let test_values = [1000, 10000, 100000, 500000]
for val in test_values {
    print("Testing mutual tail calls with n =", val)
    let result = even_tail(val)
    print("even_tail(" + val + ") =", result)
}
```

### 3. 容器递归测试套件

#### 3.1 容器操作递归
```aql
// test_container_recursion.aql
// 测试容器相关的深度递归

function deep_container_build(n, container) {
    if n <= 0 {
        return container
    }
    
    // 向容器添加元素
    container.add(n)
    
    // 递归调用
    return deep_container_build(n - 1, container)
}

function deep_container_process(container, index) {
    if index >= container.size() {
        return container.size()
    }
    
    // 处理当前元素
    let value = container.get(index)
    container.set(index, value * 2)
    
    // 递归处理下一个元素
    return deep_container_process(container, index + 1)
}

// 执行容器递归测试
print("=== 容器递归测试 ===")
let depths = [1000, 10000, 50000]

for depth in depths {
    print("Testing container recursion with depth:", depth)
    
    let container = []  // 创建容器
    let memory_before = get_memory_usage()
    
    // 构建容器
    deep_container_build(depth, container)
    
    // 处理容器
    let processed_count = deep_container_process(container, 0)
    
    let memory_after = get_memory_usage()
    
    print("Container size:", container.size())
    print("Processed elements:", processed_count)
    print("Memory used:", memory_after - memory_before, "bytes")
    print("---")
}
```

#### 3.2 嵌套容器递归
```aql
// test_nested_container_recursion.aql
// 测试嵌套容器的递归处理

function create_nested_structure(depth, current_depth) {
    if current_depth >= depth {
        return current_depth
    }
    
    let nested = []
    nested.add(current_depth)
    nested.add(create_nested_structure(depth, current_depth + 1))
    
    return nested
}

function flatten_nested_structure(structure, result) {
    if typeof(structure) == "number" {
        result.add(structure)
        return result
    }
    
    for element in structure {
        flatten_nested_structure(element, result)
    }
    
    return result
}

// 执行嵌套容器测试
print("=== 嵌套容器递归测试 ===")
let depths = [100, 500, 1000]

for depth in depths {
    print("Creating nested structure with depth:", depth)
    
    let nested = create_nested_structure(depth, 0)
    let flattened = []
    flatten_nested_structure(nested, flattened)
    
    print("Nested structure depth:", depth)
    print("Flattened elements:", flattened.size())
    print("---")
}
```

### 4. 混合递归测试套件

#### 4.1 直接+间接+尾调用混合
```aql
// test_mixed_recursion.aql
// 混合递归类型测试

// 直接递归
function direct_recursive(n) {
    if n <= 0 return 0
    return direct_recursive(n - 1) + 1
}

// 间接递归
function indirect_a(n) {
    if n <= 0 return 0
    return indirect_b(n - 1) + 1
}

function indirect_b(n) {
    if n <= 0 return 0
    return indirect_a(n - 1) + 1
}

// 尾调用递归
function tail_recursive(n, acc) {
    if n <= 0 return acc
    return tail_recursive(n - 1, acc + 1)
}

// 混合调用函数
function mixed_recursion_test(n, type) {
    if n <= 0 return 0
    
    match type {
        case "direct":
            return direct_recursive(n / 3) + mixed_recursion_test(n - 1, "indirect")
        case "indirect":  
            return indirect_a(n / 3) + mixed_recursion_test(n - 1, "tail")
        case "tail":
            return tail_recursive(n / 3, 0) + mixed_recursion_test(n - 1, "direct")
        default:
            return 0
    }
}

// 执行混合测试
print("=== 混合递归测试 ===")
let test_sizes = [100, 500, 1000]

for size in test_sizes {
    print("Testing mixed recursion with size:", size)
    let result = mixed_recursion_test(size, "direct")
    print("Mixed recursion result:", result)
    print("---")
}
```

### 5. 压力测试套件

#### 5.1 并发深度递归
```aql
// test_concurrent_deep_recursion.aql
// 并发深度递归测试

function concurrent_countdown(n, thread_id) {
    if n <= 0 {
        return thread_id
    }
    return concurrent_countdown(n - 1, thread_id)
}

// 模拟并发测试（如果AQL支持线程）
print("=== 并发深度递归测试 ===")
let thread_count = 4
let depth_per_thread = 50000

for i in range(thread_count) {
    print("Starting thread", i, "with depth", depth_per_thread)
    // 注意：这里假设AQL有某种并发机制
    // 实际实现可能需要根据AQL的并发模型调整
    let result = concurrent_countdown(depth_per_thread, i)
    print("Thread", i, "completed with result:", result)
}
```

#### 5.2 内存压力测试
```aql
// test_memory_pressure.aql
// 内存压力下的递归测试

function memory_intensive_recursion(n, data_size) {
    if n <= 0 {
        return data_size
    }
    
    // 创建大量数据增加内存压力
    let large_data = []
    for i in range(data_size) {
        large_data.add(i * i)
    }
    
    // 递归调用
    let result = memory_intensive_recursion(n - 1, data_size)
    
    // 处理数据（避免被优化掉）
    let sum = 0
    for value in large_data {
        sum += value
    }
    
    return result + sum % 1000  // 返回部分结果避免溢出
}

print("=== 内存压力测试 ===")
let test_configs = [
    {depth: 1000, data_size: 100},
    {depth: 500, data_size: 1000},
    {depth: 100, data_size: 5000}
]

for config in test_configs {
    print("Testing depth:", config.depth, "data size:", config.data_size)
    let memory_before = get_memory_usage()
    
    let result = memory_intensive_recursion(config.depth, config.data_size)
    
    let memory_after = get_memory_usage()
    print("Result:", result)
    print("Memory used:", memory_after - memory_before, "bytes")
    print("---")
}
```

### 6. 边界条件测试套件

#### 6.1 极限递归测试
```aql
// test_extreme_recursion.aql
// 极限条件递归测试

function find_recursion_limit() {
    let depth = 1000
    let max_successful_depth = 0
    
    while depth <= 1000000 {  // 最多测试到100万层
        print("Testing recursion depth:", depth)
        
        try {
            let result = countdown(depth)
            max_successful_depth = depth
            print("Success at depth:", depth)
            depth *= 2  // 指数增长查找极限
        } catch error {
            print("Failed at depth:", depth, "Error:", error)
            break
        }
    }
    
    return max_successful_depth
}

// 二分查找精确极限
function binary_search_limit(low, high) {
    if high - low <= 1 {
        return low
    }
    
    let mid = (low + high) / 2
    print("Testing depth:", mid)
    
    try {
        countdown(mid)
        print("Success at:", mid)
        return binary_search_limit(mid, high)
    } catch error {
        print("Failed at:", mid)
        return binary_search_limit(low, mid)
    }
}

print("=== 极限递归测试 ===")
let rough_limit = find_recursion_limit()
print("Rough limit found:", rough_limit)

let precise_limit = binary_search_limit(rough_limit / 2, rough_limit * 2)
print("Precise recursion limit:", precise_limit)
```

#### 6.2 错误恢复测试
```aql
// test_error_recovery.aql
// 递归错误恢复测试

function recursive_with_error(n, error_at) {
    if n == error_at {
        throw "Intentional error at depth " + n
    }
    
    if n <= 0 {
        return 0
    }
    
    return recursive_with_error(n - 1, error_at) + 1
}

function test_error_recovery() {
    let test_cases = [
        {depth: 1000, error_at: 500},
        {depth: 5000, error_at: 2500},
        {depth: 10000, error_at: 7500}
    ]
    
    for test_case in test_cases {
        print("Testing error recovery - depth:", test_case.depth, "error at:", test_case.error_at)
        
        try {
            recursive_with_error(test_case.depth, test_case.error_at)
            print("ERROR: Should have thrown an exception!")
        } catch error {
            print("Caught expected error:", error)
            
            // 测试错误后的栈状态
            try {
                let recovery_result = countdown(100)  // 简单递归测试恢复
                print("Stack recovered successfully, result:", recovery_result)
            } catch recovery_error {
                print("Stack recovery failed:", recovery_error)
            }
        }
        print("---")
    }
}

print("=== 错误恢复测试 ===")
test_error_recovery()
```

## 📊 性能监控和分析

### 性能指标收集
```aql
// test_performance_monitoring.aql
// 性能监控测试

function performance_test_suite() {
    let test_results = []
    
    let depths = [1000, 5000, 10000, 50000, 100000]
    
    for depth in depths {
        print("Performance test for depth:", depth)
        
        // 收集性能指标
        let stats_before = get_stack_stats()
        let memory_before = get_memory_usage()
        let start_time = precise_time()
        
        // 执行递归
        let result = countdown(depth)
        
        let end_time = precise_time()
        let memory_after = get_memory_usage()
        let stats_after = get_stack_stats()
        
        // 计算指标
        let test_result = {
            depth: depth,
            result: result,
            execution_time: end_time - start_time,
            memory_used: memory_after - memory_before,
            stack_reallocations: stats_after.total_reallocations - stats_before.total_reallocations,
            max_stack_size: stats_after.max_depth_reached,
            tail_call_optimizations: stats_after.tail_call_optimizations - stats_before.tail_call_optimizations
        }
        
        test_results.add(test_result)
        
        // 打印结果
        print("  Execution time:", test_result.execution_time, "ms")
        print("  Memory used:", test_result.memory_used, "bytes")
        print("  Stack reallocations:", test_result.stack_reallocations)
        print("  Max stack depth:", test_result.max_stack_size)
        print("  Tail call optimizations:", test_result.tail_call_optimizations)
        print("---")
    }
    
    return test_results
}

// 生成性能报告
function generate_performance_report(results) {
    print("=== 性能分析报告 ===")
    
    for result in results {
        let time_per_call = result.execution_time / result.depth
        let memory_per_call = result.memory_used / result.depth
        
        print("深度:", result.depth)
        print("  每层调用时间:", time_per_call, "ms")
        print("  每层内存使用:", memory_per_call, "bytes")
        print("  栈重新分配率:", (result.stack_reallocations / result.depth * 100), "%")
        print("---")
    }
}

// 执行性能测试
let performance_results = performance_test_suite()
generate_performance_report(performance_results)
```

## 🎯 测试执行计划

### 阶段1：基础功能验证（第1周）
1. **基础递归测试**：验证10-10000层递归
2. **栈重新分配测试**：确保动态扩展正常工作
3. **兼容性测试**：确保现有功能不受影响

### 阶段2：深度递归测试（第2周）  
1. **深度递归测试**：验证50000-200000层递归
2. **内存使用分析**：监控内存使用模式
3. **性能基准测试**：建立性能基线

### 阶段3：尾调用优化验证（第3周）
1. **尾调用检测测试**：验证尾调用正确识别
2. **尾调用优化测试**：验证栈空间重用
3. **无限尾递归测试**：验证理论无限递归

### 阶段4：容器集成测试（第4周）
1. **容器递归测试**：验证容器操作的深度递归
2. **容器引用管理**：确保栈重新分配时引用正确
3. **嵌套容器测试**：验证复杂容器结构

### 阶段5：压力和边界测试（第5周）
1. **并发递归测试**：验证多线程环境稳定性
2. **内存压力测试**：验证内存不足时的行为
3. **极限递归测试**：找到实际递归极限
4. **错误恢复测试**：验证异常情况下的栈状态

## 🔍 测试自动化

### 自动化测试脚本
```bash
#!/bin/bash
# run_stack_optimization_tests.sh
# 自动化测试脚本

echo "=== AQL栈优化测试套件 ==="
echo "开始时间: $(date)"

# 测试文件列表
test_files=(
    "test_progressive_recursion.aql"
    "test_deep_recursion.aql"
    "test_tail_recursion.aql"
    "test_mutual_tail_recursion.aql"
    "test_container_recursion.aql"
    "test_nested_container_recursion.aql"
    "test_mixed_recursion.aql"
    "test_concurrent_deep_recursion.aql"
    "test_memory_pressure.aql"
    "test_extreme_recursion.aql"
    "test_error_recovery.aql"
    "test_performance_monitoring.aql"
)

# 执行测试
passed=0
failed=0

for test_file in "${test_files[@]}"; do
    echo "执行测试: $test_file"
    
    if ./aql "$test_file" > "results/${test_file%.aql}.output" 2>&1; then
        echo "✅ $test_file - 通过"
        ((passed++))
    else
        echo "❌ $test_file - 失败"
        ((failed++))
    fi
done

echo "=== 测试结果汇总 ==="
echo "通过: $passed"
echo "失败: $failed"
echo "总计: $((passed + failed))"
echo "结束时间: $(date)"

# 生成测试报告
echo "生成详细测试报告..."
./generate_test_report.sh
```

### 持续集成配置
```yaml
# .github/workflows/stack_optimization_tests.yml
name: Stack Optimization Tests

on:
  push:
    paths:
      - 'src/astate.*'
      - 'src/astack_config.h'
      - 'src/aql.h'
  pull_request:
    paths:
      - 'src/astate.*'
      - 'src/astack_config.h'
      - 'src/aql.h'

jobs:
  stack-tests:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Build AQL
      run: make clean && make
      
    - name: Run Stack Optimization Tests
      run: |
        chmod +x tests/stack_optimization/run_tests.sh
        ./tests/stack_optimization/run_tests.sh
        
    - name: Upload Test Results
      uses: actions/upload-artifact@v3
      with:
        name: stack-test-results
        path: tests/stack_optimization/results/
```

## 📋 验收标准

### 功能验收标准
- ✅ 支持至少200,000层递归深度
- ✅ 尾调用优化正确工作，优化率>95%
- ✅ 容器操作在深度递归中正常工作
- ✅ 所有现有测试用例继续通过
- ✅ 内存使用合理（<100MB for 200k层）

### 性能验收标准  
- ✅ 栈重新分配开销<5%总执行时间
- ✅ 深度递归性能线性增长
- ✅ 无内存泄漏
- ✅ 错误恢复后栈状态正常

### 稳定性验收标准
- ✅ 连续运行24小时无崩溃
- ✅ 并发深度递归稳定
- ✅ 异常情况下优雅降级
- ✅ 所有边界条件正确处理

这个全面的测试计划将确保AQL栈优化的成功实施和验证。
