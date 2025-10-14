-- global_env_test.lua
-- 测试Lua的全局环境表机制

-- 定义一个全局函数
function test_func()
    return "Hello from global function"
end

-- 通过_ENV访问
print(_ENV.test_func())

-- 查看_ENV的类型和内容
print("_ENV type:", type(_ENV))
print("_ENV.print:", _ENV.print)
print("_ENV.test_func:", _ENV.test_func)

-- 动态设置全局函数
_ENV.dynamic_func = function(x)
    return x * 2
end

print("dynamic_func(5):", dynamic_func(5))

