-- tail_fibonacci.lua
-- 尾递归斐波那契数列

function fib_tail(n, a, b)
    if n == 0 then
        return a
    elseif n == 1 then
        return b
    else
        return fib_tail(n - 1, b, a + b)  -- 尾调用
    end
end

-- 包装函数，提供默认参数
function fib(n)
    return fib_tail(n, 0, 1)
end

-- 测试 fib(10) = 55
print(fib(10))
