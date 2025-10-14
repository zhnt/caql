#!/usr/bin/env python3
import time
import subprocess
import statistics

def iterative_sum(n):
    """迭代版本的求和：1+2+...+n"""
    total = 0
    for i in range(1, n + 1):
        total += i
    return total

def formula_sum(n):
    """公式版本的求和：n*(n+1)/2"""
    return n * (n + 1) // 2

def recursive_sum(n):
    """递归版本的求和"""
    if n <= 0:
        return 0
    return n + recursive_sum(n - 1)

def benchmark_python_methods():
    """测试Python的不同求和方法"""
    n = 10000
    expected = 50005000
    
    print("🐍 Python性能对比测试 (sum 1..10000)")
    print("=" * 50)
    
    # 测试公式方法
    times = []
    for _ in range(1000):  # 多次测试以获得准确结果
        start = time.perf_counter()
        result = formula_sum(n)
        end = time.perf_counter()
        times.append(end - start)
        assert result == expected
    
    print(f"📐 公式方法 (n*(n+1)/2):")
    print(f"   平均时间: {statistics.mean(times)*1000000:.2f}纳秒")
    print(f"   最快时间: {min(times)*1000000:.2f}纳秒")
    
    # 测试迭代方法
    times = []
    for _ in range(100):
        start = time.perf_counter()
        result = iterative_sum(n)
        end = time.perf_counter()
        times.append(end - start)
        assert result == expected
    
    print(f"🔄 迭代方法 (for循环):")
    print(f"   平均时间: {statistics.mean(times)*1000:.2f}毫秒")
    print(f"   最快时间: {min(times)*1000:.2f}毫秒")
    
    # 测试内置sum方法
    times = []
    for _ in range(100):
        start = time.perf_counter()
        result = sum(range(1, n + 1))
        end = time.perf_counter()
        times.append(end - start)
        assert result == expected
    
    print(f"⚡ 内置sum方法:")
    print(f"   平均时间: {statistics.mean(times)*1000:.2f}毫秒")
    print(f"   最快时间: {min(times)*1000:.2f}毫秒")
    
    # 测试递归方法（小心栈溢出）
    try:
        import sys
        sys.setrecursionlimit(15000)
        
        times = []
        for _ in range(10):
            start = time.perf_counter()
            result = recursive_sum(n)
            end = time.perf_counter()
            times.append(end - start)
            assert result == expected
        
        print(f"🔁 递归方法:")
        print(f"   平均时间: {statistics.mean(times)*1000:.2f}毫秒")
        print(f"   最快时间: {min(times)*1000:.2f}毫秒")
        
    except RecursionError:
        print(f"🔁 递归方法: ❌ RecursionError (栈溢出)")

def benchmark_aql():
    """测试AQL递归性能"""
    times = []
    for _ in range(10):
        start = time.perf_counter()
        result = subprocess.run(['./bin/aqlm', 'test/vm/bytecode/call/recursive_10000.by'], 
                              capture_output=True, text=True)
        end = time.perf_counter()
        
        if result.returncode == 0 and result.stdout.strip() == '50005000':
            times.append(end - start)
    
    if times:
        print(f"⚡ AQL递归方法:")
        print(f"   平均时间: {statistics.mean(times)*1000:.2f}毫秒")
        print(f"   最快时间: {min(times)*1000:.2f}毫秒")
    else:
        print(f"⚡ AQL递归方法: ❌ 测试失败")

if __name__ == "__main__":
    benchmark_python_methods()
    print()
    benchmark_aql()
    
    print("\n💡 分析:")
    print("1. Python不支持尾调用优化，递归会栈溢出")
    print("2. 公式方法最快，O(1)时间复杂度")
    print("3. 内置sum利用了C优化，比纯Python循环快")
    print("4. 递归方法最慢，且有栈溢出风险")
    print("5. AQL作为VM需要包含进程启动开销")
