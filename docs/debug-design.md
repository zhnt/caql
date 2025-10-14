# AQL 调试系统设计文档

## 概述

AQL 采用分层调试系统，通过编译时宏控制和运行时标志位，实现从生产版本的零开销到调试版本的全功能支持。

## 设计原则

1. **编译时优化**: 生产版本完全无调试代码开销
2. **分离关注点**: 不同工具有不同的调试需求  
3. **格式统一**: 结构化输出有统一的漂亮格式
4. **运行时控制**: 支持细粒度的调试选项组合

## 三个版本定位

```
aql     - 生产版本，完全无调试输出，最精简 (-DAQL_PRODUCTION_BUILD)
aqlm    - 字节码执行工具，基础调试功能 (-DAQL_VM_BUILD)  
aqld    - 完整调试版本，全功能调试支持 (-DAQL_DEBUG_BUILD)
```

## 调试输出分类

### 基础输出
- `aql_error(...)` - 错误信息，所有版本都有
- `aql_info(...)` - 基础信息，非生产版本有

### 格式化输出 (带统一格式)
- `aql_info_vl` - 词法分析输出 (Lexer tokens)
- `aql_info_vast` - 抽象语法树输出 (AST)  
- `aql_info_vb` - 字节码输出 (Bytecode)
- `aql_info_vt` - 执行跟踪输出 (Trace)
- `aql_info_vd` - 详细调试信息 (Debug)

### 内部调试
- `aql_debug(...)` - 内部调试信息，仅调试构建版本

## 命令行参数

### aql (生产版本)
```bash
aql script.aql                    # 纯执行，无任何调试输出
```

### aqlm (字节码工具)
```bash
aqlm script.by                    # 基础执行
aqlm -v script.by                 # 详细模式 (vd + vt + vb)
aqlm -vt script.by                # 仅执行跟踪
aqlm -vd script.by                # 仅详细调试信息
aqlm -vb script.by                # 仅字节码输出
aqlm -q script.by                 # 静默模式
```

### aqld (调试版本)
```bash
aqld script.aql                   # 基础执行
aqld -v script.aql                # 详细模式 (vl + vast + vb + vt + vd)
aqld -vl script.aql               # 仅词法分析输出
aqld -vast script.aql             # 仅AST输出  
aqld -vb script.aql               # 仅字节码输出
aqld -vt script.aql               # 仅执行跟踪
aqld -vd script.aql               # 仅详细调试信息
aqld -vb -vt script.aql           # 组合：字节码 + 执行跟踪
```

## 宏定义系统

### 编译时控制宏

```c
// adebug.h
#ifdef AQL_PRODUCTION_BUILD
    // 生产版本：完全无调试代码
    #define aql_error(...)          fprintf(stderr, __VA_ARGS__)
    #define aql_info(...)           ((void)0)
    #define aql_info_vd(...)        ((void)0)
    #define aql_info_vt(...)        ((void)0)
    #define aql_info_vast(...)      ((void)0)
    #define aql_info_vl(...)        ((void)0)
    #define aql_info_vb(...)        ((void)0)
    #define aql_debug(...)          ((void)0)
    
    // 格式化输出函数在生产版本中也是空操作
    #define aql_format_begin(...)   ((void)0)
    #define aql_format_end(...)     ((void)0)
    #define aql_format_tree_node(...) ((void)0)
    #define aql_format_table_row(...) ((void)0)
    #define aql_format_item(...)    ((void)0)

#elif defined(AQL_VM_BUILD)  // aqlm字节码工具
    #define aql_error(...)          fprintf(stderr, __VA_ARGS__)
    #define aql_info(...)           printf(__VA_ARGS__)
    #define aql_info_vd(...)        aql_output_if_enabled(AQL_FLAG_VD, __VA_ARGS__)
    #define aql_info_vt(...)        aql_output_if_enabled(AQL_FLAG_VT, __VA_ARGS__)
    #define aql_info_vast(...)      ((void)0)  // aqlm不支持AST
    #define aql_info_vl(...)        ((void)0)  // aqlm不支持词法
    #define aql_info_vb(...)        aql_output_if_enabled(AQL_FLAG_VB, __VA_ARGS__)
    #define aql_debug(...)          ((void)0)
    
    // 格式化输出 - 有条件编译
    #define aql_format_begin(type, title) aql_format_begin_impl(type, title)
    #define aql_format_end(type)    aql_format_end_impl(type)
    #define aql_format_table_row(...) aql_format_table_row_impl(__VA_ARGS__)
    #define aql_format_item(...)    aql_format_item_impl(__VA_ARGS__)
    #define aql_format_tree_node(...) ((void)0)  // aqlm不需要AST格式

#elif defined(AQL_DEBUG_BUILD)  // aqld调试版本
    #define aql_error(...)          fprintf(stderr, __VA_ARGS__)
    #define aql_info(...)           printf(__VA_ARGS__)
    #define aql_info_vd(...)        aql_output_if_enabled(AQL_FLAG_VD, __VA_ARGS__)
    #define aql_info_vt(...)        aql_output_if_enabled(AQL_FLAG_VT, __VA_ARGS__)
    #define aql_info_vast(level, ...) aql_output_ast_if_enabled(level, __VA_ARGS__)
    #define aql_info_vl(...)        aql_output_if_enabled(AQL_FLAG_VL, __VA_ARGS__)
    #define aql_info_vb(...)        aql_output_if_enabled(AQL_FLAG_VB, __VA_ARGS__)
    #define aql_debug(...)          aql_output_debug(__VA_ARGS__)
    
    // 格式化输出 - 完整支持
    #define aql_format_begin(type, title) aql_format_begin_impl(type, title)
    #define aql_format_end(type)    aql_format_end_impl(type)
    #define aql_format_tree_node(level, ...) aql_format_tree_node_impl(level, __VA_ARGS__)
    #define aql_format_table_row(...) aql_format_table_row_impl(__VA_ARGS__)
    #define aql_format_item(...)    aql_format_item_impl(__VA_ARGS__)
#endif
```

### 运行时控制

```c
// adebug.c
typedef enum {
    AQL_FLAG_VD   = 0x01,  // 详细调试 (-vd)
    AQL_FLAG_VT   = 0x02,  // 执行跟踪 (-vt)
    AQL_FLAG_VAST = 0x04,  // AST输出 (-vast)
    AQL_FLAG_VL   = 0x08,  // 词法分析 (-vl)
    AQL_FLAG_VB   = 0x10,  // 字节码输出 (-vb)
    AQL_FLAG_V    = 0x1F   // 全部详细信息 (-v)
} AQL_DebugFlags;

static int aql_debug_flags = 0;

void aql_debug_enable_flag(AQL_DebugFlags flag);
void aql_debug_enable_verbose_all(void);
void aql_output_if_enabled(AQL_DebugFlags flag, const char *fmt, ...);
```

## 格式化输出系统

### 输出类型

```c
typedef enum {
    AQL_FORMAT_TOKENS,    // 词法分析输出
    AQL_FORMAT_AST,       // AST树形输出  
    AQL_FORMAT_BYTECODE,  // 字节码表格输出
    AQL_FORMAT_TRACE      // 执行跟踪输出 (简洁格式)
} AQL_FormatType;
```

### 格式化函数

```c
// 结构化输出控制
void aql_format_begin_impl(AQL_FormatType type, const char *title);
void aql_format_end_impl(AQL_FormatType type);

// 不同类型的格式化输出
void aql_format_tree_node_impl(int level, const char *format, ...);    // AST树形
void aql_format_table_row_impl(const char *col1, ...);                 // 字节码表格
void aql_format_item_impl(const char *format, ...);                    // 通用项目
```

## 输出效果示例

### 词法分析输出 (`aqld -vl script.aql`)
```
┌─────────────────────────────────────────────────────────────────┐
│                           词法分析结果                            │
├─────────────┬─────────────────┬─────────┬─────────────────────┤
│    类型     │      内容       │  行号   │        位置         │
├─────────────┼─────────────────┼─────────┼─────────────────────┤
│ NUMBER      │ 2               │ 1       │ 0:1                 │
│ PLUS        │ +               │ 1       │ 2:3                 │
│ NUMBER      │ 3               │ 1       │ 4:5                 │
└─────────────┴─────────────────┴─────────┴─────────────────────┘
```

### AST输出 (`aqld -vast script.aql`)
```
┌─────────────────────────────────────────────────────────────────┐
│                           抽象语法树                            │
└─────────────────────────────────────────────────────────────────┘
└── BinaryExpression
    ├── operator: +
    ├── Number
    │   └── value: 2
    └── Number
        └── value: 3
```

### 字节码输出 (`aqld -vb script.aql` 或 `aqlm -vb script.by`)
```
┌─────────────────────────────────────────────────────────────────┐
│                            字节码                               │
├─────┬─────────────┬─────────┬─────────┬─────────┬─────────────────┤
│ PC  │    指令     │    A    │    B    │    C    │      说明       │
├─────┼─────────────┼─────────┼─────────┼─────────┼─────────────────┤
│ 0   │ LOADI       │ R0      │ 2       │ -       │ 加载立即数      │
│ 1   │ LOADI       │ R1      │ 3       │ -       │ 加载立即数      │
│ 2   │ ADD         │ R2      │ R0      │ R1      │ 加法运算        │
│ 3   │ RETURN1     │ R2      │ -       │ -       │ 返回单值        │
└─────┴─────────────┴─────────┴─────────┴─────────┴─────────────────┘
```

### 执行跟踪输出 (`aqlm -vt script.by` 或 `aqld -vt script.aql`)
```
PC=0 <LOADI,op=1> A=[R0,nil],sBx=2
PC=1 <LOADI,op=1> A=[R1,nil],sBx=3  
PC=2 <ADD,op=13> A=[R2,nil],B=0,C=1
PC=3 <RETURN1,op=72> A=[R2,5],B=0,C=0
```

## 代码使用示例

### 词法分析器中
```c
// alex.c
void aql_lex_analyze(const char *source) {
    aql_format_begin(AQL_FORMAT_TOKENS, "词法分析结果");
    
    Token token;
    while ((token = aql_lex_next_token()).type != TOKEN_EOF) {
        aql_info_vl("│ %-11s │ %-15s │ %-7d │ %-19s │\n",
                   token_type_name(token.type), token.value, 
                   token.line, token.position);
    }
    
    aql_format_end(AQL_FORMAT_TOKENS);
}
```

### 语法分析器中  
```c
// aparser.c
void aql_ast_print(ASTNode *root) {
    aql_format_begin(AQL_FORMAT_AST, "抽象语法树");
    aql_ast_print_node(root, 0);
    aql_format_end(AQL_FORMAT_AST);
}

void aql_ast_print_node(ASTNode *node, int level) {
    aql_info_vast(level, "%s", ast_node_type_name(node->type));
    
    switch (node->type) {
        case AST_BINARY_OP:
            aql_info_vast(level + 1, "operator: %s", node->binary_op.operator);
            aql_ast_print_node(node->binary_op.left, level + 1);
            aql_ast_print_node(node->binary_op.right, level + 1);
            break;
    }
}
```

### 代码生成器中
```c
// acodegen.c  
void aql_bytecode_print(Proto *proto) {
    aql_format_begin(AQL_FORMAT_BYTECODE, "字节码");
    
    for (int i = 0; i < proto->sizecode; i++) {
        Instruction inst = proto->code[i];
        OpCode op = GET_OPCODE(inst);
        
        aql_info_vb("│ %-3d │ %-11s │ R%-6d │ %-7d │ %-7d │ %-15s │\n",
                   i, aql_opnames[op], GETARG_A(inst), 
                   GETARG_B(inst), GETARG_C(inst),
                   get_instruction_description(op));
    }
    
    aql_format_end(AQL_FORMAT_BYTECODE);
}
```

### 虚拟机执行中
```c
// avm_core.c
void aqlV_execute2(aql_State *L, CallInfo *ci) {
    // ...
    for (;;) {
        Instruction i;
        OpCode op = GET_OPCODE(i);
        
        // 执行跟踪 - 简洁格式，无边框
        aql_info_vt("PC=%d <%s,op=%d> A=[R%d,%s],B=%d,C=%d\n", 
                   pc_num, aql_opnames[op], (int)op, 
                   GETARG_A(i), get_register_value_str(base + GETARG_A(i)),
                   GETARG_B(i), GETARG_C(i));
        
        // 详细调试信息
        aql_info_vd("🔧 [VM] 执行指令: %s at PC=%d\n", aql_opnames[op], pc_num);
        
        // 内部调试 (仅调试构建)
        aql_debug("[DEBUG] Instruction: 0x%08X, base=%p\n", i, (void*)base);
        
        // 指令执行...
    }
}
```

## 文件结构

```
src/
├── adebug.h          # 调试系统统一接口和宏定义
├── adebug.c          # 调试系统实现 (运行时控制、格式化输出)
└── adebug_user.c     # 保持现有用户接口 (向后兼容)
```

**注意**: VM执行跟踪代码保持在 `avm_core.c` 中，通过新的宏系统进行重构，无需单独文件。

## 编译配置

```makefile
# Makefile 配置
aql: CFLAGS += -DAQL_PRODUCTION_BUILD -O2
aqlm: CFLAGS += -DAQL_VM_BUILD -g
aqld: CFLAGS += -DAQL_DEBUG_BUILD -g -O0

aql: $(AQL_SOURCES)
	$(CC) $(CFLAGS) -o bin/aql $(AQL_SOURCES)

aqlm: $(AQLM_SOURCES)  
	$(CC) $(CFLAGS) -o bin/aqlm $(AQLM_SOURCES)

aqld: $(AQLD_SOURCES)
	$(CC) $(CFLAGS) -o bin/aqld $(AQLD_SOURCES)
```

## 实施计划

1. **第一阶段**: 创建 `adebug.h` 和 `adebug.c`，定义基础宏系统
2. **第二阶段**: 重构现有VM跟踪代码，使用新的宏系统  
3. **第三阶段**: 实现格式化输出系统
4. **第四阶段**: 在各模块中应用统一的调试输出
5. **第五阶段**: 创建不同的编译目标 (aql/aqlm/aqld)

## 优势

1. **零开销**: 生产版本完全无调试代码
2. **精确控制**: 每个调试选项独立控制
3. **格式统一**: 结构化输出有一致的美观格式
4. **扩展性**: 易于添加新的调试类别和格式
5. **向后兼容**: 保持现有调试接口
6. **开发友好**: 宏名称直观，使用简单

## 未来扩展

- 单步调试支持 (断点、变量查看、调用栈)
- DAP (Debug Adapter Protocol) 协议支持
- IDE 集成调试
- 性能分析和热点检测
- 内存使用跟踪和泄漏检测