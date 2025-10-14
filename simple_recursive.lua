-- simple_recursive.lua
-- 简单递归倒计数
function countdown(n)
    if n <= 0 then
        return 0
    else
        return countdown(n - 1) + n
    end
end

print(countdown(10))
