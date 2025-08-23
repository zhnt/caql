# AQL 语言语法文档

## 1. 基础语法

### 1.1 变量声明
```aql
-- 变量声明（完整语法）
let x = 10                    -- 显式变量声明
let name: string = "AQL"      -- 带类型注解
let numbers: [int] = [1, 2, 3, 4, 5]  -- 数组类型

-- 类型推断语法糖（推荐）
x := 10                       -- 推断为int
name := "AQL"                 -- 推断为string
numbers := [1, 2, 3, 4, 5]    -- 推断为[int]
scores := {"Alice" = 95, "Bob" = 87}  -- 推断为dict<string,int>

-- 常量声明
const PI = 3.14159            -- 常量必须初始化
const MAX_SIZE: int = 100     -- 常量带类型注解

-- 多变量声明
let a, b, c = 1, 2, 3         -- 多重赋值
let x, y := 10, "hello"       -- 多重类型推断

-- 未初始化变量（需要类型注解）
let uninitialized: int

-- 变量作用域示例
let global_var = 100

function example() {
    let local_var = 200       -- 函数作用域变量
    
    if (true) {
        let block_var = 300   -- 块级作用域变量
    }
    
    -- 变量遮蔽
    let global_var = 400      -- 遮蔽全局变量
}

-- 常量变量（不可重新赋值）
const config = {debug = true, port = 8080}
```

### 1.2 数据类型
```aql
-- 基础类型
nil           -- 空值
bool          -- 布尔值 (true/false)
int           -- 整数 (64位)
float         -- 浮点数 (64位)
string        -- 字符串
function      -- 函数
coroutine     -- 协程
promise       -- 异步Promise

-- 复合类型
array         -- 数组（固定类型，类似Go的array）
dict          -- 字典（键值对，类似Python的dict）
slice         -- 切片（动态数组，类似Go的slice）
struct        -- 结构体
class        -- 类
interface     -- 接口
```

### 1.3 控制流
```aql
-- 条件语句（Python风格）
if condition {
    -- 代码块
} elif condition2 {
    -- 代码块
} else {
    -- 代码块
}

-- 单行if语句
if condition { statement }

-- 条件表达式（三元运算符）
result = condition ? value1 : value2

-- 嵌套条件
if user.active {
    if user.premium {
        print("Welcome premium user!")
    } else {
        print("Welcome active user!")
    }
} elif user.suspended {
    print("Account suspended")
} else {
    print("Please activate your account")
}

-- 条件与逻辑运算符
if age >= 18 and country == "US" {
    print("Eligible to vote")
}

if score >= 90 or bonus > 10 {
    grade = "A"
}

-- 条件中的模式匹配
if user is dict<string, any> and user.active {
    print("Active user: ${user.name}")
}

if data is []int and len(data) > 0 {
    print("Processing ${len(data)} items")
}

-- Switch语句（类似Python的match）
switch value {
    case 1:
        print("One")
        break
    case 2:
        print("Two")
        break
    case 3, 4, 5:
        print("Three to Five")
        break
    default:
        print("Other")
}

-- 模式匹配（类似Python 3.10+的match）
match value {
    case int n if n > 0:
        print("Positive integer: ${n}")
    case string s if len(s) > 10:
        print("Long string: ${s}")
    case []int numbers:
        print("Integer list with ${len(numbers)} items")
    case dict<string, any> data:
        print("Dictionary with keys: ${data.keys()}")
    case nil:
        print("None value")
    default:
        print("Unknown type")
}
```

### 1.4 循环语句
```aql
-- 传统for循环
for i = 1, 10 {
    -- 循环体
}

-- Python风格的for each循环
for item in items {
    print("Item: ${item}")
}

-- 带索引的for each循环
for index, item in enumerate(items) {
    print("Index: ${index}, Item: ${item}")
}

-- 字典遍历
for key, value in data {
    print("${key}: ${value}")
}

-- 范围遍历
for i in range(1, 10) {
    print("Number: ${i}")
}

-- 步长遍历
for i in range(0, 100, 2) {
    print("Even number: ${i}")
}

-- 反向遍历
for i in range(10, 0, -1) {
    print("Countdown: ${i}")
}

-- While循环
while condition {
    -- 循环体
}

-- Do-while循环
do {
    -- 循环体
} while condition

-- 异步循环
for await item in async_iterator() {
    -- 异步循环体
}

-- 异步for each
for await item in async_items {
    print("Async item: ${item}")
}

-- 循环控制语句
for i in range(1, 10) {
    if i == 5 {
        break  -- 跳出循环
    }
    
    if i == 3 {
        continue  -- 跳过当前迭代
    }
    
    print("Number: ${i}")
}

-- 带标签的循环控制
outer_loop: for i in range(1, 5) {
    for j in range(1, 5) {
        if i == 3 and j == 3 {
            break outer_loop  -- 跳出外层循环
        }
        print("${i}, ${j}")
    }
}

-- 异常处理循环
for item in items {
    try {
        result: any = process_item(item)
        print("Processed: ${result}")
    } catch (error) {
        print("Error processing item: ${error.message}")
        continue
    }
}

-- 条件循环
for item in items where item.active {
    print("Active item: ${item.name}")
}

-- 并行循环（类似Python的concurrent.futures）
for item in items parallel {
    result: any = await process_item(item)
    print("Parallel result: ${result}")
}

-- 带超时的循环
for item in items timeout 5000 {
    result: any = await process_item(item)
    print("Result: ${result}")
}
```

## 2. 函数系统

### 2.1 函数定义
```aql
-- 普通函数
function add(a, b) {
    return a + b
}

-- 异步函数
async function fetch_data(url) {
    response = await http.get(url)
    return response.data
}

-- 协程函数
coroutine function process_data(data) {
    for item in data {
        yield process_item(item)
    }
}

-- 函数类型声明
function add(a: int, b: int): int {
    return a + b
}

async function fetch_data(url: string): dict<string, any> {
    response = await http.get(url)
    return response.data
}
```

### 2.2 Lambda和函数式编程支持
```aql
-- Lambda表达式（匿名函数）
add = (a: int, b: int) => a + b
multiply = (x: int, y: int) => x * y
square = (x: int) => x * x
is_even = (n: int) => n % 2 == 0
identity = (x: any) => x
constant = (x: any) => () => x

-- 使用lambda
result: int = add(5, 3)  -- 8
squared: int = square(4)  -- 16
even_check: bool = is_even(6)  -- true

-- Lambda在数组操作中
numbers: []int = [1, 2, 3, 4, 5]
squares: []int = numbers.map(x => x * x)
evens: []int = numbers.filter(x => x % 2 == 0)
sum: int = numbers.reduce((acc, x) => acc + x, 0)

-- Lambda在排序中
users: []dict<string, any> = [
    {"name": "Alice", "age": 25},
    {"name": "Bob", "age": 30},
    {"name": "Charlie", "age": 20}
]
sorted_users: []dict<string, any> = users.sort((a, b) => a.age - b.age)

-- 闭包（捕获外部变量）
function create_counter(): function {
    count: int = 0
    return () => {
        count = count + 1
        return count
    }
}

function create_adder(x: int): function {
    return (y: int) => x + y
}

-- 使用闭包
counter = create_counter()
print(counter())  -- 1
print(counter())  -- 2
print(counter())  -- 3

add_five = create_adder(5)
result: int = add_five(3)  -- 8

-- 高阶函数
function apply_operation(func: function, a: int, b: int): int {
    return func(a, b)
}

result1: int = apply_operation((x, y) => x + y, 5, 3)  -- 8
result2: int = apply_operation((x, y) => x * y, 4, 6)  -- 24

-- 函数式编程风格
numbers: []int = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

-- 链式操作
result: int = numbers
    .filter(x => x % 2 == 0)      -- 过滤偶数
    .map(x => x * x)              -- 平方
    .reduce((acc, x) => acc + x, 0)  -- 求和

print(result)  -- 220 (4 + 16 + 36 + 64 + 100)

-- 异步lambda
async_fetch = async (url: string) => {
    response = await http.get(url)
    return response.data
}

-- 使用异步lambda
data = await async_fetch("https://api.example.com/data")

-- 装饰器lambda
log_execution = (func: function) => {
    return (...args: any) => {
        print("Entering: ${func.name}")
        result = func(...args)
        print("Exiting: ${func.name}")
        return result
    }
}

-- 使用装饰器lambda
@log_execution
function process_data(data: any): any {
    return data * 2
}
```

### 2.3 递归支持
```aql
-- 基础递归：阶乘计算
function factorial(n: int): int {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

-- 尾递归优化（编译器自动优化）
function factorial_tail(n: int, acc: int = 1): int {
    if n <= 1 {
        return acc
    }
    return factorial_tail(n - 1, n * acc)
}

-- 斐波那契数列递归
function fibonacci(n: int): int {
    if n <= 1 {
        return n
    }
    return fibonacci(n - 1) + fibonacci(n - 2)
}

-- 异步递归
async function fetch_recursive(url: string, depth: int = 0): dict<string, any> {
    if depth > 3 {
        return {"error": "Max depth exceeded"}
    }
    
    response: dict<string, any> = await http.get(url)
    
    if response.links {
        for link in response.links {
            child_response: dict<string, any> = await fetch_recursive(link, depth + 1)
            response.children.append(child_response)
        }
    }
    
    return response
}

-- 递归生成器
coroutine function generate_combinations(items: []string, k: int): string {
    if k == 0 {
        yield ""
        return
    }
    
    if len(items) == 0 {
        return
    }
    
    for i, item in enumerate(items) {
        remaining: []string = items[i+1:]
        for combination in generate_combinations(remaining, k - 1) {
            yield item + " " + combination
        }
    }
}
``` 

## 3. 数据类型系统

### 3.1 数组（Array）
```aql
-- 数组定义（固定类型，Python风格类型声明）
numbers: []int = [1, 2, 3, 4, 5]
names: []string = ["Alice", "Bob", "Charlie"]
scores: []float = [95.5, 87.2, 92.8]

-- 数组操作
numbers[0] = 10                    -- 修改元素
first: int = numbers[0]            -- 访问元素
length: int = len(numbers)         -- 获取长度

-- 数组方法
numbers.append(6)                  -- 添加元素
numbers.insert(1, 15)             -- 插入元素
numbers.remove(3)                  -- 删除元素
sorted: []int = numbers.sort()     -- 排序
reversed: []int = numbers.reverse() -- 反转

-- 数组遍历
for i, value in enumerate(numbers) {
    print("Index: ${i}, Value: ${value}")
}

-- 数组推导式
squares: []int = [x*x for x in numbers]
evens: []int = [x for x in numbers if x % 2 == 0]
```

### 3.2 字典（Dict）
```aql
-- 字典定义（Python风格类型声明）
ages: dict<string, int> = {
    "Alice" = 25,
    "Bob" = 30,
    "Charlie" = 28
}

user: dict<string, any> = {
    "name" = "Alice",
    "age" = 25,
    "email" = "alice@example.com",
    "active" = true
}

-- 字典操作
ages["David"] = 35                 -- 添加/修改键值对
age: int = ages["Alice"]           -- 访问值
exists: bool = "Bob" in ages       -- 检查键存在
ages.remove("Charlie")             -- 删除键值对

-- 字典方法
keys: []string = ages.keys()       -- 获取所有键
values: []int = ages.values()      -- 获取所有值
items: []tuple<string, int> = ages.items() -- 获取所有键值对
size: int = ages.size()            -- 获取大小

-- 字典遍历
for key, value in ages {
    print("${key}: ${value}")
}

-- 字典推导式
doubled: dict<string, int> = {k = v*2 for k, v in ages}
adults: dict<string, bool> = {k = v >= 18 for k, v in ages}
```

### 3.3 切片（Slice）
```aql
-- 切片定义（动态数组，Python风格类型声明）
numbers: []int = [1, 2, 3, 4, 5]
names: []string = ["Alice", "Bob"]
prices: []float = [10.5, 20.3, 15.7]

-- 切片操作
numbers.append(6)                  -- 追加元素
numbers.extend([7, 8, 9])         -- 扩展切片
popped: int = numbers.pop()       -- 弹出最后一个元素
first: int = numbers.pop(0)       -- 弹出第一个元素

-- 切片索引
sub: []int = numbers[1:4]         -- 切片操作 [1,4)
last_three: []int = numbers[-3:]  -- 最后三个元素
first_three: []int = numbers[:3]  -- 前三个元素
every_other: []int = numbers[::2] -- 每隔一个元素

-- 切片方法
numbers.sort()                     -- 排序
numbers.reverse()                  -- 反转
index: int = numbers.index(3)     -- 查找元素索引
count: int = numbers.count(2)     -- 统计元素出现次数

-- 切片推导式
squares: []int = [x*x for x in numbers]
filtered: []int = [x for x in numbers if x > 3]
```

## 4. 接口、对象和结构体

### 4.1 接口定义
```aql
-- 接口定义
interface DataProcessor {
    function process(data: any): any
    function validate(data: any): bool
    function get_config(): table
}

-- 接口实现
class MyProcessor implements DataProcessor {
    function process(data) {
        return data * 2
    }
    
    function validate(data) {
        return type(data) == "number"
    }
    
    function get_config() {
        return {name = "MyProcessor", version = "1.0"}
    }
}
```

### 4.2 结构体定义
```aql
-- 结构体定义
struct User {
    string name
    int age
    string email
    table preferences
}

-- 结构体实例化
user = User{
    name = "Alice",
    age = 25,
    email = "alice@example.com",
    preferences = {theme = "dark", lang = "en"}
}

-- 结构体方法
struct Point {
    float x
    float y
    
    function distance(other: Point): float {
        dx = self.x - other.x
        dy = self.y - other.y
        return math.sqrt(dx*dx + dy*dy)
    }
}
```

## 5. 字符串模板

### 5.1 基础字符串模板
```aql
-- 基础模板语法
name = "World"
greeting = `Hello, ${name}!`
print(greeting)  -- 输出: Hello, World!

-- 表达式支持
x = 10
y = 20
result = `Sum: ${x + y}, Product: ${x * y}`
print(result)  -- 输出: Sum: 30, Product: 200

-- 函数调用
items = {"apple", "banana", "orange"}
list = `Items: ${table.concat(items, ", ")}`
print(list)  -- 输出: Items: apple, banana, orange
```

### 5.2 高级模板特性
```aql
-- 条件表达式
user = {name = "Alice", role = "admin"}
message = `Welcome ${user.name}${user.role == "admin" ? " (Administrator)" : ""}!`
print(message)  -- 输出: Welcome Alice (Administrator)!

-- 循环表达式
numbers = {1, 2, 3, 4, 5}
squares = `Squares: ${numbers.map(n => n*n).join(", ")}`
print(squares)  -- 输出: Squares: 1, 4, 9, 16, 25

-- 多行模板
html = `
<html>
    <head>
        <title>${title}</title>
    </head>
    <body>
        <h1>${heading}</h1>
        <p>${content}</p>
    </body>
</html>
`
```

## 6. 异步编程

### 6.1 异步函数
```aql
-- 异步函数定义
async function process_data(data) {
    results = {}
    for item in data {
        processed = await process_item(item)
        table.insert(results, processed)
    }
    return results
}

-- 异步调用
result = await process_data(data)
```

### 6.2 Promise支持
```aql
-- 创建Promise
promise = Promise.new(function(resolve, reject) {
    if success {
        resolve(result)
    } else {
        reject(error)
    }
})

-- Promise链式调用
promise
    :then(function(result) {
        return process(result)
    })
    :catch(function(error) {
        return handle_error(error)
    })
```

### 6.3 协程支持
```aql
-- 协程函数
coroutine function data_generator() {
    for i = 1, 10 {
        yield i * 2
    }
}

-- 使用协程
gen = data_generator()
for value in gen {
    print(value)
}
```

## 7. 注解系统

### 7.1 基础注解
```aql
-- 注解工厂函数
function annotation(handler: function) {
    return function(target: any, ...args: any) {
        -- 将注解信息附加到目标上
        if not target.__annotations__ {
            target.__annotations__ = {}
        }
        
        target.__annotations__[handler.name] = {
            handler = handler,
            args = args
        }
        
        return target
    }
}

-- 基础注解实现
deprecated = annotation(function(target, message) {
    print("Warning: ${target.name} is deprecated. ${message}")
})

async = annotation(function(target) {
    -- 将函数标记为异步
    target.__async__ = true
})

retry = annotation(function(target, max_attempts, delay) {
    -- 添加重试逻辑
    target.__retry_config__ = {
        max_attempts = max_attempts,
        delay = delay
    }
})

-- 使用注解
@deprecated("Use new_function instead")
function old_function() {
    -- 旧函数实现
}

@async
@retry(max_attempts = 3, delay = 1000)
function fetch_data(url: string): promise<any> {
    return http.get(url)
}
```

### 7.2 AI服务注解
```aql
-- AI服务调用注解
@ai_service("openai", "gpt-4")
function generate_text(prompt: string): string {
    -- 函数体可以为空，由注解处理
}

@ai_service("anthropic", "claude-3")
@temperature(0.7)
@max_tokens(1000)
function analyze_sentiment(text: string): dict<string, any> {
    -- 情感分析
}

-- Agent注解
@agent(name = "DataProcessor", version = "1.0")
class DataProcessor {
    @llm_task("数据清洗")
    function clean_data(data: slice<any>): slice<any> {
        -- 数据清洗逻辑
    }
    
    @llm_task("数据分析")
    function analyze_data(data: slice<any>): dict<string, any> {
        -- 数据分析逻辑
    }
}
```

## 8. 模块化系统

### 8.1 模块定义和导入
```aql
-- 模块定义 (math.aql)
-- 方式1：显式导出（推荐，类似Python的__all__）
module math {
    -- 导出常量
    const PI = 3.14159
    const E = 2.71828
    
    -- 导出函数
    function add(a: int, b: int): int {
        return a + b
    }
    
    function multiply(a: int, b: int): int {
        return a * b
    }
    
    -- 私有函数（下划线开头，类似Python）
    function _internal_helper() {
        -- 内部实现
    }
    
    -- 导出结构体
    struct Point {
        float x
        float y
        
        function distance(other: Point): float {
            dx = self.x - other.x
            dy = self.y - other.y
            return sqrt(dx*dx + dy*dy)
        }
    }
    
    -- 导出接口
    interface Calculator {
        function calculate(a: int, b: int): int
    }
    
    -- 导出类
    class BasicCalculator implements Calculator {
        function calculate(a: int, b: int): int {
            return a + b
        }
    }
    
    -- 显式指定导出列表（类似Python的__all__）
    export {PI, E, add, multiply, Point, Calculator, BasicCalculator}
}

-- 方式2：隐式导出（类似Go，大写开头自动导出）
module math {
    -- 大写开头自动导出
    const PI = 3.14159
    const E = 2.71828
    
    function Add(a: int, b: int): int {
        return a + b
    }
    
    function Multiply(a: int, b: int): int {
        return a * b
    }
    
    -- 小写开头私有
    function internalHelper() {
        -- 私有实现
    }
    
    struct Point {
        float X
        float Y
        
        function Distance(other: Point): float {
            dx = self.X - other.X
            dy = self.Y - other.Y
            return sqrt(dx*dx + dy*dy)
        }
    }
    
    -- 自动导出所有大写开头的定义
}

-- 方式3：混合模式（推荐）
module math {
    -- 显式导出常量
    export const PI = 3.14159
    export const E = 2.71828
    
    -- 显式导出函数
    export function add(a: int, b: int): int {
        return a + b
    }
    
    export function multiply(a: int, b: int): int {
        return a * b
    }
    
    -- 私有函数（下划线开头）
    function _internal_helper() {
        -- 内部实现
    }
    
    -- 私有函数（小写开头）
    function internalHelper() {
        -- 私有实现
    }
    
    -- 导出结构体
    export struct Point {
        float x
        float y
        
        function distance(other: Point): float {
            dx = self.x - other.x
            dy = self.y - other.y
            return sqrt(dx*dx + dy*dy)
        }
    }
    
    -- 导出接口
    export interface Calculator {
        function calculate(a: int, b: int): int
    }
    
    -- 导出类
    export class BasicCalculator implements Calculator {
        function calculate(a: int, b: int): int {
            return a + b
        }
    }
}

-- 模块导入
-- 完整导入
import math

-- 别名导入
import math as m

-- 选择性导入（类似Python）
import {add, multiply, PI} from math
import {Point, Calculator} from math

-- 重命名导入
import {add as add_func, multiply as mul} from math

-- 使用导入的模块
result: int = math.add(5, 3)
result2: int = m.multiply(4, 6)
sum: int = add(10, 20)
point: Point = Point{x = 1.0, y = 2.0}
```

### 8.2 模块系统高级特性
```aql
-- 条件导出
module utils {
    -- 根据环境导出不同实现
    if env.NODE_ENV == "production" {
        export function logger(message: string) {
            -- 生产环境日志
            log_to_file(message)
        }
    } else {
        export function logger(message: string) {
            -- 开发环境日志
            print("[DEBUG] ${message}")
        }
    }
    
    -- 动态导出
    export const VERSION = env.APP_VERSION
    export const DEBUG = env.DEBUG == "true"
}

-- 子模块
module math {
    -- 子模块：基础数学
    module basic {
        export function add(a: int, b: int): int { return a + b }
        export function subtract(a: int, b: int): int { return a - b }
        export function multiply(a: int, b: int): int { return a * b }
        export function divide(a: int, b: int): float { return a / b }
    }
    
    -- 子模块：高级数学
    module advanced {
        export function sqrt(x: float): float { return math.sqrt(x) }
        export function pow(x: float, y: float): float { return math.pow(x, y) }
        export function sin(x: float): float { return math.sin(x) }
        export function cos(x: float): float { return math.cos(x) }
    }
    
    -- 重新导出子模块
    export {basic, advanced}
    export {add, subtract, multiply, divide} from basic
    export {sqrt, pow, sin, cos} from advanced
}

-- 使用子模块
import {basic, advanced} from math
import {add, sqrt} from math

result1: int = basic.add(5, 3)
result2: float = advanced.sqrt(16.0)
result3: int = add(10, 20)  -- 直接使用重新导出的函数

-- 循环依赖处理
module a {
    import {func_b} from b
    
    export function func_a() {
        return func_b() + 1
    }
}

module b {
    import {func_a} from a
    
    export function func_b() {
        return func_a() * 2
    }
}

-- 延迟导入（解决循环依赖）
module a {
    export function func_a() {
        -- 延迟导入
        b = import("b")
        return b.func_b() + 1
    }
}

-- 模块别名和命名空间
module ai {
    module openai {
        export function chat(messages: []dict): dict {
            -- OpenAI实现
        }
    }
    
    module anthropic {
        export function chat(messages: []dict): dict {
            -- Anthropic实现
        }
    }
}

-- 使用命名空间
import ai
response1 = ai.openai.chat(messages)
response2 = ai.anthropic.chat(messages)

-- 或者直接导入子模块
import {openai, anthropic} from ai
response1 = openai.chat(messages)
response2 = anthropic.chat(messages)
```

### 8.3 包管理
```aql
-- 包定义 (package.aql)
package "myapp" version "1.0.0" {
    -- 包元数据
    author = "Alice"
    description = "My awesome AQL package"
    license = "MIT"
    
    -- 依赖声明
    dependencies = {
        "http" = ">=1.0.0",
        "json" = ">=2.0.0"
    }
    
    -- 主模块
    main = "src/main.aql"
    
    -- 导出模块
    exports = {
        "math" = "src/math.aql",
        "utils" = "src/utils.aql"
    }
}

-- 包安装和使用
-- 安装包
install "myapp" version "1.0.0"

-- 使用包
import myapp.math
import myapp.utils

-- 使用包中的模块
result: int = myapp.math.add(5, 3)
utils_result: string = myapp.utils.format("Hello")

-- 包版本管理
-- 指定版本范围
import "myapp" version ">=1.0.0,<2.0.0"

-- 使用最新版本
import "myapp" version "latest"

-- 使用特定版本
import "myapp" version "1.2.3"

-- 包别名
import "myapp" as app
result = app.math.add(5, 3)

-- 包内选择性导入
import {math, utils} from "myapp"
result = math.add(5, 3)
```

### 8.4 命名空间和模块热重载
```aql
-- 命名空间定义（使用模块实现）
module ai {
    module openai {
        export function chat(messages: []dict): dict {
            -- OpenAI聊天实现
        }
        
        export function generate_image(prompt: string): string {
            -- 图像生成实现
        }
    }
    
    module anthropic {
        export function chat(messages: []dict): dict {
            -- Anthropic聊天实现
        }
    }
}

-- 使用命名空间
response: dict<string, any> = ai.openai.chat(messages)
image: string = ai.openai.generate_image("A beautiful sunset")
claude_response: dict<string, any> = ai.anthropic.chat(messages)

-- 模块热重载
-- 开发模式下的模块热重载
reload "math"  -- 重新加载math模块

-- 监听模块变化
watch "src/" {
    on_change = function(module_name: string) {
        print("Module ${module_name} changed, reloading...")
        reload module_name
    }
}

-- 条件重载
if env.NODE_ENV == "development" {
    watch "src/" {
        on_change = function(module_name: string) {
            if module_name.ends_with(".aql") {
                reload module_name
                print("Reloaded: ${module_name}")
            }
        }
    }
}
```

## 9. 多种执行模式

### 9.1 解释执行模式
```aql
-- 解释执行（默认模式）
-- 适合开发调试和快速原型
result = interpret("print('Hello World')")

-- 交互式解释器
-- 支持REPL环境，实时执行代码
```

### 9.2 JIT编译模式
```aql
-- JIT编译执行
-- 自动识别热点代码并编译为机器码
jit_result = jit_execute("""
    sum = 0
    for i = 1, 1000000 {
        sum = sum + i
    }
    return sum
""")

-- JIT优化配置
jit.config({
    optimization_level = "aggressive",
    inline_threshold = 100,
    loop_unroll_limit = 4
})
```

### 9.3 LLVM编译模式
```aql
-- LLVM编译执行
-- 生成优化的机器码
llvm_result = llvm_compile("""
    function fibonacci(n: int): int {
        if n <= 1 { return n }
        return fibonacci(n-1) + fibonacci(n-2)
    }
    return fibonacci(30)
""")

-- LLVM优化选项
llvm.config({
    optimization_level = "O3",
    target_arch = "x86_64",
    enable_vectorization = true
})
```

### 9.4 编译执行模式
```aql
-- 静态编译为可执行文件
-- 生成独立的二进制文件
compile_to_binary("main.aql", "app", {
    optimization = "release",
    strip_symbols = true,
    static_linking = true
})

-- 编译配置
compiler.config({
    target_platform = "linux-x86_64",
    optimization_level = "O2",
    debug_info = false,
    runtime_library = "static"
})
```

## 10. AI服务集成

### 10.1 服务调用
```aql
-- 基础AI服务调用
response: dict<string, any> = await @openai.chat({
    model = "gpt-3.5-turbo",
    messages = [{"role": "user", "content": "Hello"}]
})

-- 服务配置
@openai.config({
    api_key = "your-key",
    timeout = 30
})
```

### 10.2 管道操作
```aql
-- 数据管道
result: dict<string, any> = data 
    |> @preprocess.clean()
    |> @analyze.sentiment()
    |> @format.json()

-- 并行处理
results: []dict<string, any> = data 
    -| @service1.process()
    -| @service2.process()
    -> merge_results()
```

### 10.3 服务注册
```aql
-- 注册自定义服务
@register("my_service", {
    endpoint = "http://localhost:8080",
    methods = {"process", "analyze"}
})

-- 使用自定义服务
result: dict<string, any> = await @my_service.process(data)
``` 