#!/usr/bin/env python3
import time

def fib_recursive(n):
    """递归版斐波那契 - O(2^n)复杂度"""
    if n <= 1:
        return n
    return fib_recursive(n - 1) + fib_recursive(n - 2)

def fib_iterative(n):
    """迭代版斐波那契 - O(n)复杂度"""
    if n <= 1:
        return n
    a, b = 0, 1
    for _ in range(2, n + 1):
        a, b = b, a + b
    return b

def count_calls(n, memo={}):
    """计算递归版本的函数调用次数"""
    if n in memo:
        return memo[n]
    
    if n <= 1:
        memo[n] = 1
        return 1
    
    calls = 1 + count_calls(n-1, memo) + count_calls(n-2, memo)
    memo[n] = calls
    return calls

def analyze_complexity():
    print("🔍 斐波那契算法复杂度分析")
    print("=" * 50)
    
    # 分析递归调用次数
    print("📊 递归版本的函数调用次数:")
    for n in range(1, 21):
        calls = count_calls(n, {})
        print(f"fib({n:2d}): {calls:8d} 次调用")
        if n >= 10 and n % 5 == 0:
            ratio = calls / (2 ** (n-2)) if n > 2 else 1
            print(f"       调用次数 / 2^(n-2) ≈ {ratio:.2f}")
    
    print("\n📈 复杂度增长对比:")
    print("n    递归调用次数    2^n      n")
    print("-" * 35)
    for n in [5, 10, 15, 20, 25, 30]:
        calls = count_calls(n, {})
        power_2n = 2 ** n
        print(f"{n:2d}   {calls:10d}   {power_2n:8d}   {n:2d}")
    
    print("\n⏱️  实际执行时间对比:")
    for n in [20, 25, 30, 35]:
        # 递归版本
        start = time.perf_counter()
        result_rec = fib_recursive(n)
        time_rec = time.perf_counter() - start
        
        # 迭代版本
        start = time.perf_counter()
        result_iter = fib_iterative(n)
        time_iter = time.perf_counter() - start
        
        print(f"fib({n}): 递归={time_rec*1000:.1f}ms, 迭代={time_iter*1000000:.1f}μs, 结果={result_rec}")
        
        if time_rec > 1:  # 如果递归版本超过1秒就停止
            print("递归版本太慢，停止测试...")
            break

def visualize_recursion_tree():
    """可视化递归调用树"""
    print("\n🌲 递归调用树可视化 (fib(5)):")
    print("""
                    fib(5)
                   /      \\
               fib(4)      fib(3)
              /     \\      /     \\
          fib(3)   fib(2) fib(2) fib(1)
          /   \\    /   \\  /   \\
      fib(2) fib(1) fib(1) fib(0) fib(1) fib(0)
      /   \\
   fib(1) fib(0)
    """)
    
    print("🔢 调用次数统计:")
    print("fib(0): 3次")  
    print("fib(1): 5次")
    print("fib(2): 3次")
    print("fib(3): 2次") 
    print("fib(4): 1次")
    print("fib(5): 1次")
    print("总计: 15次调用")
    
    actual_calls = count_calls(5, {})
    print(f"验证: count_calls(5) = {actual_calls}")

def explain_complexity():
    print("\n💡 为什么是 O(2^n) 复杂度？")
    print("=" * 40)
    
    print("""
1. **重复计算问题**:
   - fib(n) = fib(n-1) + fib(n-2)
   - 每个fib(k)都会被多次计算
   - fib(3)在计算fib(5)时被调用2次
   - fib(2)在计算fib(5)时被调用3次
   
2. **递归树分析**:
   - 树的深度 ≈ n
   - 每层的节点数大约翻倍
   - 总节点数 ≈ 2^n
   
3. **数学推导**:
   设T(n)为计算fib(n)的时间复杂度
   T(n) = T(n-1) + T(n-2) + O(1)
   
   这个递推关系的解是 T(n) = O(φ^n)
   其中φ = (1+√5)/2 ≈ 1.618 (黄金比例)
   
   由于 φ^n < 2^n，所以 T(n) = O(2^n)
   
4. **实际增长**:
   - fib(10): 177次调用
   - fib(20): 21,891次调用  
   - fib(30): 2,692,537次调用
   - fib(40): 331,160,281次调用 (约3.3亿次!)
   
5. **解决方案**:
   - 动态规划: O(n)时间，O(n)空间
   - 迭代方法: O(n)时间，O(1)空间  
   - 矩阵快速幂: O(log n)时间
    """)

if __name__ == "__main__":
    analyze_complexity()
    visualize_recursion_tree()
    explain_complexity()

