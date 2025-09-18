# AQL - Claude Code 项目指南

## 项目概述
AQL (AI Query Language) 是一个面向AI时代的原生编程语言，基于Lua 5.4架构但进行了现代化重构，专为AI工作流和数据科学优化。

## 核心架构
- **寄存器虚拟机**: 高效的32位固定长度指令集 (64条指令)
- **现代容器系统**: 替代Lua table，采用array<T,N>/slice<T>/dict<K,V>/vector<T,N>
- **分阶段优化**: 解释执行 → SIMD优化 → JIT/AOT编译
- **AI原生设计**: 意图驱动编程、工作流编排、人机协作

## 项目结构
```
src/                    # 核心源码
├── aql.c              # 主程序入口
├── a*.c/h             # 核心组件 (遵循Lua命名约定)
│   ├── avm.c/h        # 虚拟机核心
│   ├── alex.c/h       # 词法分析器
│   ├── aparser.c/h    # 语法分析器
│   ├── acode.c/h      # 代码生成器
│   ├── amem.c/h       # 内存管理
│   ├── agc.c/h        # 垃圾回收器
│   ├── aarray.c/h     # 数组容器
│   ├── aslice.c/h     # 切片容器
│   ├── adict.c/h      # 字典容器
│   ├── avector.c/h    # 向量容器
│   ├── aobject.c/h    # 对象系统
│   ├── aopcodes.c/h   # 操作码定义
│   ├── arepl.c/h      # 交互式解释器
│   ├── astring.c/h    # 字符串处理
│   ├── aerror.c/h     # 错误处理
│   └── ado.c/h        # 数据操作
├── ai_*.c/h           # AI扩展组件
└── lua548/            # Lua 5.4源码和文档，供借鉴学习使用

test/                  # 测试代码
└── *.aql              # 测试代码

docs/                  # 设计文档
├── arch.md            # 架构设计
├── design.md          # 详细设计
├── req.md             # 需求文档
└── design_*.md        # 其他设计文档

bin/                   # 可执行文件目录
├── aql                # 生产版本：高性能，零调试开销
└── aqld               # 调试版本：完整调试信息和分析工具

Makefile               # 构建系统（支持make debug/release）
```

## 构建系统
- **构建工具**: Makefile + CMake 3.12+
- **编译标准**: C11
- **编译选项**: 
  - Debug: `-g -O0 -DAQL_DEBUG`
  - Release: `-O3 -DNDEBUG`
- **快速构建命令**:
  ```bash
  make debug           # 构建调试版本（aqld）
  make release         # 构建生产版本（aql）
  make                 # 构建两个版本
  ```
- **传统CMake构建**:
  ```bash
  mkdir build && cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release
  make -j$(nproc)
  ```

## 关键配置选项
- `AQL_USE_SIMD`: 启用SIMD优化 (默认ON)
- `AQL_USE_JIT`: 启用JIT编译 (默认OFF, Phase 3)
- `AQL_BUILD_TESTS`: 构建测试套件 (默认ON)
- `AQL_BUILD_EXAMPLES`: 构建示例程序 (默认ON)

## 开发约定

### 文件命名
- 核心组件: `a[功能].c/h` (如: `avm.c`, `alex.h`)
- AI扩展: `ai_[功能].c/h` (如: `ai_simd.c`, `ai_jit.h`)
- 类型前缀: `a` 代表AQL核心组件

### 代码风格
- 遵循Lua源码风格
- 使用POSIX标准C
- 避免C++特性，保持C兼容性
- 使用小写+下划线命名
- 关键数据结构以`a`开头 (如: `aql_State`, `TValue`)

### 性能敏感代码
- 使用寄存器VM架构，避免栈操作
- 优先内联小函数
- 使用SIMD友好的内存布局
- 避免动态内存分配在热点路径

## 调试指南

### 快速调试（推荐）
修改代码后，使用调试版本进行开发和测试：
```bash
make debug                    # 构建调试版本
./bin/aqld -v test.aql       # 显示完整编译和执行过程
./bin/aqld -vt test.aql      # 仅查看词法分析
./bin/aqld -va test.aql      # 查看语法分析（AST）
./bin/aqld -vb test.aql      # 查看字节码生成
./bin/aqld -st test.aql      # 词法分析后停止
```

### 调试工具
- **GDB**: 标准C调试
- **Valgrind**: 内存泄漏检测
- **AddressSanitizer**: 编译器内置检查

### 日志调试
```c
#ifdef AQL_DEBUG
  printf("[DEBUG] %s:%d: message\n", __FILE__, __LINE__);
#endif
```

## 测试策略

### 单元测试
- 使用简单的C测试框架
- 每个核心模块独立测试
- 测试文件位于对应模块同级目录

### 集成测试
- 使用.aql示例文件测试完整流程
- 测试嵌入API的正确性
- 性能基准测试

### 测试命令
```bash
make test          # 运行所有测试
./bin/aql examples/hello.aql    # 运行示例（生产版本）
./bin/aqld examples/hello.aql   # 调试运行（详细输出）
./bin/aqld -v examples/hello.aql # 显示完整调试信息
```

## 嵌入API使用

### C API示例
```c
#include "aql.h"

aql_State* L = aqlL_newstate();
aqlL_openlibs(L);

if (aqlL_loadfile(L, "script.aql") || aql_pcall(L, 0, 0, 0)) {
    fprintf(stderr, "Error: %s\n", aql_tostring(L, -1));
}

aql_close(L);
```

### Python API示例
```python
import aql

with aql.State() as L:
    L.load("print('Hello AQL')")
    L.execute()
```

## 常见任务

### 添加新指令
1. 修改`aopcodes.h`添加新操作码
2. 在`avm.c`中实现指令处理
3. 更新`acode.c`生成相应字节码

### 添加新容器类型
1. 创建`a[容器名].c/h`
2. 实现CommonHeader + 容器特定字段
3. 在`aobject.c`中注册类型
4. 添加标准库支持

### 性能优化
1. 使用`AQL_USE_SIMD=ON`启用SIMD
2. 检查热点路径使用`ai_simd.c`
3. 使用`perf`分析性能瓶颈

## 项目阶段

### Phase 1: 核心基础 ✅
- [x] 寄存器VM架构
- [x] 64条指令集设计
- [x] 现代容器系统设计
- [x] C/Python嵌入API
- [ ] 完整编译器实现
- [ ] 垃圾回收器

### Phase 2: AI原生特性 🚧
- [ ] AI语法糖实现
- [ ] SIMD优化
- [ ] 数据科学库
- [ ] 工作流编排

### Phase 3: 高级优化 📋
- [ ] JIT编译器
- [ ] AOT编译
- [ ] 云原生支持
- [ ] IDE集成

## 相关资源

### 设计文档
- `docs/arch.md`: 架构设计
- `docs/design.md`: 详细设计
- `docs/req.md`: 需求文档

### 参考实现
- `lua548/`: Lua 5.4源码参考
- `examples/hello.aql`: 基础语法示例

### 性能基准
- 目标性能: 比Python快3-5倍，接近Go性能
- SIMD优化: 比Pandas快2-10倍
- 内存效率: 比传统方案减少30-50%内存使用

## 贡献指南

### 代码审查要点
- 确保与Lua架构兼容性
- 检查内存管理正确性
- 验证SIMD优化适用性
- 测试嵌入API稳定性

### 提交规范
- 遵循传统提交格式
- 包含性能影响评估
- 更新相关文档
- 添加/更新测试用例