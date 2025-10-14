#!/usr/bin/env python3
import subprocess
import time
import statistics

def benchmark_aql():
    times = []
    for i in range(10):
        start = time.perf_counter()
        result = subprocess.run(['./bin/aqlm', 'test/vm/bytecode/call/recursive_10000.by'], 
                              capture_output=True, text=True)
        end = time.perf_counter()
        
        if result.returncode == 0 and result.stdout.strip() == '50005000':
            times.append(end - start)
        else:
            print(f"第{i+1}次测试失败")
            
    return times

if __name__ == "__main__":
    print("AQL递归性能测试 (10次):")
    times = benchmark_aql()
    
    if times:
        print(f"平均时间: {statistics.mean(times):.6f}秒")
        print(f"最快时间: {min(times):.6f}秒") 
        print(f"最慢时间: {max(times):.6f}秒")
        print(f"标准差: {statistics.stdev(times):.6f}秒")
    else:
        print("测试失败")

