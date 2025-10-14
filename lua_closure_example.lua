-- Lua闭包示例：计数器生成器
-- 每次调用makeCounter都会创建一个独立的计数器

function makeCounter(start)
    local count = start or 0  -- 外层变量，会被闭包捕获
    
    return function()  -- 返回一个闭包
        count = count + 1  -- 访问并修改外层变量
        return count
    end
end

-- 创建两个独立的计数器
local counter1 = makeCounter(10)  -- 从10开始计数
local counter2 = makeCounter(100) -- 从100开始计数

print("Counter1:")
print(counter1())  -- 输出: 11
print(counter1())  -- 输出: 12
print(counter1())  -- 输出: 13

print("\nCounter2:")
print(counter2())  -- 输出: 101
print(counter2())  -- 输出: 102

print("\nCounter1 again:")
print(counter1())  -- 输出: 14 (保持独立状态)
