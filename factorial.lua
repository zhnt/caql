-- factorial.lua
-- 尾递归计算阶乘


-- 尾递归版本（使用累积器）
function factorial_tail(n, acc)
    acc = acc or 1  -- 默认累积器为1
    if n <= 1 then
        return acc  -- 尾调用：直接返回累积器
    else
        return factorial_tail(n - 1, n * acc)  -- 尾调用：递归调用是最后一个操作
    end
end

-- 对外接口函数
function factorial(n)
    return factorial_tail(n, 1)
end

print("尾递归版本:", factorial(5))         -- 120  

