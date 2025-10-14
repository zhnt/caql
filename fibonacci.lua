-- fibonacci.lua
-- 斐波那契递归函数

function fib(n)
    if n <= 1 then
        return n
    else
        return fib(n - 1) + fib(n - 2)
    end
end

-- 测试 fib(6) = 8
print(fib(6))
