# AQL组件关系设计文档 (Component Relationship Design)

## 1. 文档概述

本文档详细描述了AQL (AI Query Language) 的组件架构和依赖关系设计，包括与Lua 5.4的对比分析。

## 2. AQL整体架构设计

### 2.1 分层架构概览

AQL采用经典的分层架构设计，确保组件间的清晰依赖关系：

```
┌─────────────────────────────────────────────────┐
│                编译器层                          │
│        (源码 → 字节码转换)                        │
│ alex → aparser → acode → adump                  │
└─────────────────┬───────────────────────────────┘
                  ▼ (生成字节码)
┌─────────────────────────────────────────────────┐
│                 API层                           │
│             (对外接口)                           │
│ aapi ◄──► aauxlib ──► 标准库(*lib)              │
└─────────────────┬───────────────────────────────┘
                  ▼ (调用)
┌─────────────────────────────────────────────────┐
│               运行时层                           │
│            (高级服务)                            │
│ astate ◄──► atm ──► adebug                      │
│ astring ◄─ 容器系统 ◄─ afunc                    │
└─────────────────┬───────────────────────────────┘
                  ▼ (使用)
┌─────────────────────────────────────────────────┐
│              虚拟机层                            │
│            (执行引擎)                            │
│ aundump ──► avm ──► ado                         │
│ azio ──► agc ◄──► amem                          │
└─────────────────┬───────────────────────────────┘
                  ▼ (依赖)
┌─────────────────────────────────────────────────┐
│             对象系统层                           │
│            (数据表示)                            │
│ aobject ◄──► TValue ◄──► CommonHeader           │
│ Array/Slice/Dict/Vector/Function                │
└─────────────────┬───────────────────────────────┘
                  ▼ (基于)
┌─────────────────────────────────────────────────┐
│            基础定义层                            │
│         (核心数据结构)                           │
│ aopcodes ◄──► aconf ◄──► 基础类型定义             │
└─────────────────────────────────────────────────┘

横向性能优化:
┌───────────┐    ┌─────────────┐
│ ai_simd   │    │  ai_jit     │
│(数据处理) │    │(热路径优化) │
└───────────┘    └─────────────┘
```

### 2.2 依赖关系说明

- **自底向上依赖**: 上层模块依赖下层模块，下层为上层提供基础设施
- **单向依赖**: 避免循环依赖，确保架构清晰
- **基础层**: 定义核心数据结构和操作码
- **横向优化**: SIMD/JIT为各层提供性能加速

## 3. 组件详细依赖关系

### 3.1 基础定义层 (Foundation Layer)

**核心组件**:
- `aconf.h` - 基础配置和平台适配
- `aopcodes.h` - 指令集定义 (64条指令)
- 基础类型定义 - `lu_byte`, `Instruction`, `OpMode`

**依赖关系**:
```c
// 最底层，仅依赖标准C库
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
```

**提供服务**:
- 指令集枚举 (`enum AQLOpCode`)
- 指令格式定义 (`iABC`, `iABx`, `iAsBx`, `iAx`)
- 基础类型别名和限制

### 3.2 对象系统层 (Object Layer)

**核心组件**:
- `aobject.h/.c` - 统一对象表示
- `TValue` - 统一值类型
- `CommonHeader` - 对象头定义
- 容器实现 - `Array`, `Slice`, `Dict`, `Vector`

**依赖关系**:
```c
// 依赖基础定义层
#include "aconf.h"
#include "aopcodes.h"  // 需要指令定义

// 对象定义
typedef struct TValue {
    TValuefields;
} TValue;

typedef struct Array {
    CommonHeader;
    DataType dtype;
    size_t length;
    TValue data[];
} Array;
```

**提供服务**:
- 统一对象表示系统
- 类型安全的容器实现
- 内存布局定义

### 3.3 虚拟机层 (VM Layer)

**核心组件**:
- `avm.h/.c` - 虚拟机执行循环
- `ado.h/.c` - 执行控制和保护调用
- `agc.h/.c` - 垃圾回收器
- `amem.h/.c` - 内存管理
- `azio.h/.c` - 输入输出抽象

**依赖关系**:
```c
// 依赖对象系统层
#include "aobject.h"
#include "aopcodes.h"

// VM执行函数
void aql_execute(aql_State *L, CallInfo *ci);

// 内存管理
void *aql_realloc(aql_State *L, void *ptr, size_t osize, size_t nsize);

// 垃圾回收
void aql_gc(aql_State *L, int what, int data);
```

**提供服务**:
- 字节码执行引擎
- 内存和垃圾回收管理
- 执行状态控制

### 3.4 运行时层 (Runtime Layer)

**核心组件**:
- `astate.h/.c` - VM状态管理
- `atm.h/.c` - 元方法系统 (简化版)
- `adebug.h/.c` - 调试支持
- `astring.h/.c` - 字符串管理
- 容器操作 - `aarray.h/.c`, `aslice.h/.c`, `adict.h/.c`, `avector.h/.c`
- `afunc.h/.c` - 函数和闭包

**依赖关系**:
```c
// 依赖虚拟机层
#include "avm.h"
#include "ado.h"
#include "agc.h"
#include "amem.h"

// 状态管理
typedef struct aql_State {
    CommonHeader;
    TValue *stack;          // 数据栈
    CallInfo *ci;           // 调用信息
    global_State *l_G;      // 全局状态
    // ... 其他字段
} aql_State;
```

**提供服务**:
- 高级运行时服务
- 容器操作接口
- 调试和状态管理

### 3.5 API层 (API Layer)

**核心组件**:
- `aapi.h/.c` - C API实现
- `aauxlib.h/.c` - 辅助库
- 标准库 - `*lib.c` 文件

**依赖关系**:
```c
// 依赖运行时层
#include "astate.h"
#include "adebug.h"
#include "aobject.h"

// API函数示例
aql_State *aql_newstate(void);
int aql_pcall(aql_State *L, int nargs, int nresults, int errfunc);
void aql_pushstring(aql_State *L, const char *s);
```

**提供服务**:
- 对外C接口
- 便利函数封装
- 标准库实现

### 3.6 编译器层 (Compiler Layer)

**核心组件**:
- `alex.h/.c` - 词法分析器
- `aparser.h/.c` - 语法分析器
- `acode.h/.c` - 代码生成器
- `adump.h/.c` - 字节码序列化
- `aundump.h/.c` - 字节码反序列化

**依赖关系**:
```c
// 编译器依赖API层（用于报错等）
#include "aapi.h"
#include "aobject.h"
#include "aopcodes.h"

// 编译函数
int aql_compile(aql_State *L, aql_Reader reader, void *data, const char *name);
```

**提供服务**:
- 源码到字节码转换
- 语法错误报告
- 字节码文件生成

## 4. 横向性能优化组件

### 4.1 SIMD优化模块

**组件**: `ai_simd.h/.c`

**服务对象**: VM层、Runtime层的数值计算

**接口设计**:
```c
// SIMD接口抽象
typedef struct VectorOps {
    void (*vadd)(void* dst, void* src1, void* src2, size_t len);
    void (*vmul)(void* dst, void* src1, void* src2, size_t len);
    void (*vdot)(void* dst, void* src1, void* src2, size_t len);
} VectorOps;

// 运行时选择最优实现
VectorOps* select_vector_impl(void);
```

### 4.2 JIT编译模块

**组件**: `ai_jit.h/.c` (Phase 3)

**服务对象**: VM层的热点代码

**接口设计**:
```c
// JIT接口
int jit_should_compile(aql_State *L, Proto *p);
void *jit_compile(aql_State *L, Proto *p);
int jit_execute(aql_State *L, void *compiled_code);
```

## 5. Lua 5.4 依赖关系对比

### 5.1 Lua的分层架构

```
API层:         lapi.h, lauxlib.h, lualib.h
编译器层:      llex.h, lparser.h, lcode.h, lundump.h
执行引擎层:    ldo.h, lvm.h, ldebug.h
VM核心层:      lstate.h, ltm.h, lgc.h, ltable.h, lstring.h, lfunc.h
数据结构层:    lobject.h, lzio.h, lmem.h, lopcodes.h
基础层:        lua.h, luaconf.h, llimits.h
```

### 5.2 AQL vs Lua 架构对比

| 层次 | Lua 5.4 | AQL 设计 | 改进点 |
|------|---------|----------|--------|
| **基础层** | lua.h + llimits.h + lopcodes.h | aconf.h + aopcodes.h | ✅ 更简洁的配置 |
| **对象层** | lobject.h (unified table) | aobject.h + 容器分离 | ✅ 类型安全，性能优化 |
| **VM层** | lstate.h + lvm.h + lgc.h | astate.h + avm.h + agc.h | ✅ 结构兼容 |
| **编译器层** | llex.h + lparser.h + lcode.h | alex.h + aparser.h + acode.h | ✅ 结构兼容 |
| **API层** | lapi.h + lauxlib.h | aapi.h + aauxlib.h | ✅ 结构兼容 |
| **优化层** | 无 | ai_simd.h + ai_jit.h | ✅ 现代性能优化 |

### 5.3 关键改进点

1. **容器系统革新**:
   - Lua: unified table (array + hash)
   - AQL: 类型分离 (array/slice/dict/vector)

2. **指令集优化**:
   - Lua: 82条指令
   - AQL: 64条指令 + K/I优化

3. **性能优化支持**:
   - Lua: 无内置SIMD/JIT框架
   - AQL: 横向优化模块支持

4. **AI扩展预留**:
   - Lua: 通用脚本语言
   - AQL: 为AI特性预留扩展空间

## 6. 依赖关系验证

### 6.1 依赖检查规则

1. **无循环依赖**: 任何模块不能直接或间接依赖自己
2. **单向依赖**: 上层依赖下层，下层不知道上层
3. **最小依赖**: 只包含必要的头文件
4. **接口稳定**: 下层接口变化要考虑上层影响

### 6.2 编译顺序

```bash
# 正确的编译顺序 (自底向上)
1. 基础层:    aconf.c, aopcodes.c
2. 对象层:    aobject.c, aarray.c, aslice.c, adict.c, avector.c
3. VM层:      avm.c, ado.c, agc.c, amem.c, azio.c
4. 运行时层:  astate.c, atm.c, adebug.c, astring.c, afunc.c
5. API层:     aapi.c, aauxlib.c, *lib.c
6. 编译器层:  alex.c, aparser.c, acode.c, adump.c
7. 优化层:    ai_simd.c, ai_jit.c (可选)
```

## 7. 总结

### 7.1 架构优势

✅ **清晰的分层结构**: 6层架构，职责明确
✅ **单向依赖关系**: 避免循环依赖，易于维护  
✅ **借鉴成熟设计**: 基于Lua 5.4验证的架构
✅ **现代化改进**: 容器分离、性能优化、AI扩展

### 7.2 设计原则

1. **自底向上构建**: 基础设施在下，业务逻辑在上
2. **接口导向设计**: 清晰的模块边界和接口定义
3. **性能优先**: SIMD/JIT横向优化支持
4. **扩展友好**: 为未来AI特性预留空间

### 7.3 实施建议

1. **Phase 1**: 实现基础层到API层的核心功能
2. **Phase 2**: 添加AI特性和高级优化
3. **Phase 3**: 完善JIT编译和高级工具

这种架构设计确保了AQL既能继承Lua的成功经验，又能面向AI时代的新需求！🎉 