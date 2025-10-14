-- 最简单的尾递归调用例子

-- 例子1：倒数计数器（最简单）
function countdown(n)
    print(n)
    if n <= 0 then
        return "Done!"
    else
        return countdown(n - 1)  -- 尾调用：递归调用是最后一个操作
    end
end


-- 测试
print("=== 倒数计数器 ===")
print(countdown(3))
