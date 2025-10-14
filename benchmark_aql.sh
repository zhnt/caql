#!/bin/bash

echo "AQL递归性能测试 (10次):"

times=()
for i in {1..10}; do
    # 使用高精度时间测量
    start=$(python3 -c "import time; print(time.perf_counter())")
    ./bin/aqlm test/vm/bytecode/call/recursive_10000.by >/dev/null 2>&1
    end=$(python3 -c "import time; print(time.perf_counter())")
    
    # 计算执行时间
    duration=$(python3 -c "print($end - $start)")
    times+=($duration)
    echo "第$i次: ${duration}秒"
done

# 计算统计数据
python3 -c "
import statistics
times = [${times[*]}]
print(f'平均时间: {statistics.mean(times):.6f}秒')
print(f'最快时间: {min(times):.6f}秒')
print(f'最慢时间: {max(times):.6f}秒') 
print(f'标准差: {statistics.stdev(times):.6f}秒')
"

