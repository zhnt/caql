#!/usr/bin/env python3
import sys
import time
import statistics

# 增加递归限制
sys.setrecursionlimit(15000)

def sum_recursive(n):
    if n <= 0:
        return 0
    return n + sum_recursive(n - 1)

def benchmark_python():
    times = []
    for i in range(10):
        start = time.perf_counter()
        result = sum_recursive(10000)
        end = time.perf_counter()
        times.append(end - start)
        assert result == 50005000, f"错误结果: {result}"
    
    return times

if __name__ == "__main__":
    print("Python递归性能测试 (10次):")
    times = benchmark_python()
    
    print(f"平均时间: {statistics.mean(times):.6f}秒")
    print(f"最快时间: {min(times):.6f}秒") 
    print(f"最慢时间: {max(times):.6f}秒")
    print(f"标准差: {statistics.stdev(times):.6f}秒")
