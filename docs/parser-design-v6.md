# AQL Parser 设计文档 v6
# 基于Lua 5.4架构的AQL语法完整实现

## 1. 设计目标

**核心原则**: 完全基于Lua 5.4解析器架构，实现100% AQL语法兼容的解析器。

**语法适配**: 保持Lua解析器主流程，但完全采用AQL语法（C风格{}、:=、elif、switch、match等）。

**类型兼容**: 所有结构体内部类型与现有AQL代码库100%兼容。

## 2. 类型映射表

### 2.1 基本类型映射
| Lua类型 | AQL现有类型 | 字段用途 |
|---------|-------------|----------|
| `lua_Integer` | `aql_Integer` | 整型常量 |
| `lua_Number` | `aql_Number` | 浮点常量 |
| `lu_byte` | `aql_byte` | 字节类型 |
| `int` | `int` | 常规整数 |
| `short` | `short` | 短整型 |
| `TString` | `TString` | 字符串类型 |
| `TValue` | `TValue` | 通用值类型 |

### 2.2 核心数据结构

#### 2.2.1 表达式描述符（AExpDesc）
```c
typedef enum {
    AEXP_VOID,      /* 空表达式 */
    AEXP_NIL,       /* nil常量 */
    AEXP_TRUE,      /* true常量 */
    AEXP_FALSE,     /* false常量 */
    AEXP_K,         /* 常量池索引 */
    AEXP_KFLT,      /* 浮点常量 */
    AEXP_KINT,      /* 整数常量 */
    AEXP_KSTR,      /* 字符串常量 */
    AEXP_NONRELOC,  /* 固定寄存器 */
    AEXP_LOCAL,     /* 局部变量 */
    AEXP_UPVAL,     /* upvalue变量 */
    AEXP_INDEXED,   /* 索引变量 */
    AEXP_CALL,      /* 函数调用 */
    AEXP_ARRAY,     /* 数组构造器 */
    AEXP_SLICE,     /* 切片构造器 */
    AEXP_DICT,      /* 字典构造器 */
    AEXP_VECTOR     /* 向量构造器 */
} AExpKind;

typedef struct AExpDesc {
    AExpKind k;
    union {
        aql_Integer ival;    /* AEXP_KINT */
        aql_Number nval;     /* AEXP_KFLT */
        TString *strval;     /* AEXP_KSTR */
        int info;            /* 通用信息 */
        struct {
            short idx;       /* 索引 */
            aql_byte t;      /* 表寄存器 */
        } ind;
        struct {
            aql_byte ridx;   /* 寄存器索引 */
            unsigned short vidx; /* 变量索引 */
        } var;
    } u;
    int t;  /* true跳转列表 */
    int f;  /* false跳转列表 */
} AExpDesc;
```

#### 2.2.2 函数状态（AFuncState）
```c
typedef struct AFuncState {
    AProto *f;              /* 当前函数原型 */
    struct AFuncState *prev; /* 外围函数 */
    struct ALexState *ls;   /* 词法状态 */
    struct ABlockCnt *bl;   /* 当前块的链 */
    int pc;                 /* 下一个要编码的位置 */
    int lasttarget;         /* 最后一个'跳转标签'的'label' */
    int jpc;                /* 等待目标的跳转列表 */
    int nk;                 /* 在'k'中的常量数量 */
    int np;                 /* 原型数量 */
    int firstlocal;         /* 第一个局部变量索引 */
    short ndebugvars;       /* 调试变量数量 */
    aql_byte nactvar;       /* 活动局部变量数量 */
    aql_byte nups;          /* upvalue数量 */
    aql_byte freereg;       /* 第一个空闲寄存器 */
} AFuncState;
```

#### 2.2.3 块控制结构（ABlockCnt）
```c
typedef struct ABlockCnt {
    struct ABlockCnt *previous; /* 链 */
    int firstlabel;             /* 此块中第一个标签的索引 */
    int firstgoto;              /* 此块中第一个待处理goto的索引 */
    aql_byte nactvar;           /* 块外活动局部变量数量 */
    aql_byte upval;             /* 如果块中某个变量是upvalue则为true */
    aql_byte isloop;            /* 如果'块'是循环则为true */
    aql_byte insidetbc;         /* 如果在待关闭变量的作用域内则为true */
} ABlockCnt;
```

## 3. 解析器主流程（完全适配AQL语法）

### 3.1 解析入口
```
aqlY_parser()  // 主解析函数
    ├── aqlX_setinput()  // 初始化词法状态
    ├── aqlF_newLclosure()  // 创建主闭包
    ├── aqlY_mainfunc()  // 解析主函数
    └── 返回AClosure
```

### 3.2 主函数解析流程
```
aqlY_mainfunc()
    ├── aqlY_open_func()  // 初始化AFuncState
    ├── aqlY_setvararg()  // 设置可变参数
    ├── aqlY_statlist()   // 解析语句列表
    ├── aqlX_check(TK_EOS)
    └── aqlY_close_func()  // 关闭函数
```

### 3.3 语句解析流程
```
aqlY_statlist()  // 语句列表
    ├── while (fs->ls->t.token != TK_EOS)
    ├── aqlY_statement()  // 解析单个语句
    └── 处理return语句

aqlY_statement()  // 语句解析
    ├── TK_IF: aqlY_ifstat()      // if语句
    ├── TK_WHILE: aqlY_whilestat() // while循环
    ├── TK_FOR: aqlY_forstat()    // for循环
    ├── TK_LET: aqlY_letstat()    // let声明
    ├── TK_CONST: aqlY_conststat() // const声明
    ├── TK_SWITCH: aqlY_switchstat() // switch语句
    ├── TK_MATCH: aqlY_matchstat()   // match语句
    ├── TK_RETURN: aqlY_retstat()    // return语句
    ├── TK_BREAK: aqlY_breakstat()   // break语句
    ├── TK_CONTINUE: aqlY_contstat() // continue语句
    └── default: aqlY_exprstat()     // 表达式语句
```

### 3.4 表达式解析流程
```
aqlY_expr() -> aqlY_subexpr(limit=0)  // 表达式解析
    ├── aqlY_unary()  // 一元运算处理
    ├── aqlY_simpleexp()  // 简单表达式
    │   ├── TK_INT: 整型常量
    │   ├── TK_FLT: 浮点常量
    │   ├── TK_STRING: 字符串常量
    │   ├── TK_NIL: nil值
    │   ├── TK_TRUE/TK_FALSE: 布尔值
    │   ├── '[': aqlY_array_constructor()  // 数组构造
    │   ├── '{': aqlY_dict_constructor()   // 字典构造
    │   ├── '<': aqlY_vector_constructor() // 向量构造
    │   └── '(': 括号表达式
    ├── aqlY_suffixedexp()  // 后缀表达式
    │   ├── '.' 成员访问
    │   ├── '[' 索引访问
    │   └── '()' 函数调用
    └── 处理二元运算符（优先级解析）
```

### 3.5 控制结构解析（AQL语法适配）

#### 3.5.1 If语句（AQL风格）
```
aqlY_ifstat()
    ├── aqlY_test_then_block()  // IF条件块
    ├── while (TK_ELIF) aqlY_test_then_block()  // elif链
    ├── [TK_ELSE aqlY_block()]  // else块
    └── check_match(TK_RBRACE, TK_LBRACE)
```

#### 3.5.2 While循环（AQL风格）
```
aqlY_whilestat()
    ├── aqlY_expr()  // 条件表达式
    ├── aqlY_enterblock(isloop=1)
    ├── aqlY_checknext(TK_LBRACE)
    ├── aqlY_block()  // 循环体
    ├── aqlY_leaveblock()
    └── check_match(TK_RBRACE, TK_LBRACE)
```

#### 3.5.3 For循环（AQL风格）
```
aqlY_forstat()
    ├── 数值for: aqlY_fornum()  // for i=1,10 {...}
    └── 泛型for: aqlY_forlist() // for x in items {...}

aqlY_fornum()
    ├── 创建控制变量
    ├── 解析初始值、限制值、步长值
    ├── aqlY_forbody()  // for循环体
    └── 生成循环控制代码

aqlY_forlist()
    ├── 创建迭代变量
    ├── 解析迭代表达式
    ├── aqlY_forbody()  // for循环体
    └── 生成迭代控制代码
```

## 4. 完整函数列表

### 4.1 核心解析函数
```c
/* 主解析器函数 */
AClosure *aqlY_parser(aql_State *L, struct AZio *z, AMbuffer *buff, 
                     ADyndata *dyd, const char *name, int firstchar);
static void aqlY_mainfunc(aql_State *L, ALexState *ls, AMbuffer *buff);

/* 语句解析 */
static void aqlY_statlist(AFuncState *fs);
static void aqlY_statement(AFuncState *fs);

/* 表达式解析 */
static void aqlY_expr(ALexState *ls, AExpDesc *v);
static int aqlY_subexpr(ALexState *ls, AExpDesc *v, int limit);
static void aqlY_simpleexp(ALexState *ls, AExpDesc *v);
static void aqlY_suffixedexp(ALexState *ls, AExpDesc *v);
```

### 4.2 控制结构函数
```c
/* 条件语句 */
static void aqlY_ifstat(AFuncState *fs);
static void aqlY_test_then_block(AFuncState *fs, AExpDesc *v);

/* 循环语句 */
static void aqlY_whilestat(AFuncState *fs);
static void aqlY_forstat(AFuncState *fs);
static void aqlY_fornum(AFuncState *fs, TString *varname, int line);
static void aqlY_forlist(AFuncState *fs, TString *indexname);
static void aqlY_forbody(AFuncState *fs, int base, int line, int nvars, int isnum);

/* 变量声明 */
static void aqlY_letstat(AFuncState *fs);
static void aqlY_conststat(AFuncState *fs);
```

### 4.3 高级控制结构
```c
/* switch语句 */
static void aqlY_switchstat(AFuncState *fs);
static void aqlY_case_clause(AFuncState *fs, AExpDesc *control);

/* match语句 */
static void aqlY_matchstat(AFuncState *fs);
static void aqlY_match_case(AFuncState *fs, AExpDesc *value);

/* 三元运算 */
static void aqlY_ternary_expr(AFuncState *fs, AExpDesc *cond);
```

### 4.4 构造器函数
```c
/* 容器构造器 */
static void aqlY_array_constructor(AFuncState *fs, AExpDesc *v);
static void aqlY_dict_constructor(AFuncState *fs, AExpDesc *v);
static void aqlY_vector_constructor(AFuncState *fs, AExpDesc *v);
static void aqlY_slice_constructor(AFuncState *fs, AExpDesc *v);
```

### 4.5 作用域管理
```c
/* 作用域管理 */
static void aqlY_enterblock(AFuncState *fs, ABlockCnt *bl, aql_byte isloop);
static void aqlY_leaveblock(AFuncState *fs);

/* 变量管理 */
static int aqlY_new_localvar(ALexState *ls, TString *name);
static void aqlY_adjust_assign(AFuncState *fs, int nvars, AExpDesc *e, int line);
```

### 4.6 错误处理
```c
static void aqlY_error_expected(ALexState *ls, int token);
static void aqlY_check_condition(ALexState *ls, int c, const char *msg);
static void aqlY_check_match(ALexState *ls, int what, int who, int where);
```

## 5. Phase 1完整语法实现

### 5.1 基础AQL语法规则
```
/* Phase 1：核心AQL语法 */

chunk ::= {statement [';']} [last_statement [';']]

statement ::=
    | variable_declaration
    | assignment_statement
    | control_flow_statement
    | expression_statement

variable_declaration ::=
    | 'let' NAME [':' type] '=' expression
    | 'const' NAME [':' type] '=' expression
    | NAME ':=' expression

assignment_statement ::=
    | variable '=' expression
    | variable ':=' expression

control_flow_statement ::=
    | 'if' expression block {elif_clause} [else_clause]
    | 'while' expression block
    | 'for' NAME '=' expression ',' expression [',' expression] block
    | 'for' NAME 'in' expression block

elif_clause ::= 'elif' expression block
else_clause ::= 'else' block

block ::= '{' {statement [';']} '}'

expression ::=
    | nil | false | true | NUMBER | STRING
    | expression '?' expression ':' expression
    | expression binary_operator expression
    | unary_operator expression
    | '(' expression ')'

binary_operator ::=
    | '+' | '-' | '*' | '/' | '%'
    | '==' | '!=' | '<' | '<=' | '>' | '>='
    | 'and' | 'or'

unary_operator ::= '+' | '-' | 'not'

type ::= 'int' | 'float' | 'string' | 'bool'
```

### 5.2 Phase 1测试用例
```aql
/* 变量声明测试 */
let x = 42
const pi = 3.14159
let name: string = "AQL"
let flag := true

/* 控制流测试 */
if x > 0 {
    print("positive")
} elif x == 0 {
    print("zero")
} else {
    print("negative")
}

/* 循环测试 */
for i = 1, 10 {
    sum = sum + i
}

for item in items {
    print(item)
}

/* 表达式测试 */
result = x > 0 ? "positive" : "non-positive"
value = name ?? "default"
```

## 6. Phase 2扩展计划

### 6.1 高级语法支持
```c
/* Phase 2：高级AQL特性 */

/* 函数定义 */
function name(params) { ... }
(params) => expression

/* switch/match */
switch value {
    case 1: { ... }
    default: { ... }
}

match value {
    case pattern: { ... }
}

/* 容器类型 */
[1, 2, 3]            // 数组构造
{key: value}         // 字典构造
<1.0, 2.0, 3.0>      // 向量构造
[1:5]                // 切片构造

/* 模式匹配 */
match x {
    case int n: { ... }
    case string s: { ... }
}
```

## 7. 内存布局与优化

### 7.1 寄存器分配策略
```
┌─────────────────────────────────────────┐
│              AFuncState                 │
│  ┌─────────────────────────────────────┐ │
│  │             寄存器布局                │ │
│  │  ┌─────────┬─────────────────────┐  │ │
│  │  │ 索引0   │ 全局变量/常量        │  │ │
│  │  ├─────────┼─────────────────────┤  │ │
│  │  │ 索引1   │ 局部变量1            │  │ │
│  │  ├─────────┼─────────────────────┤  │ │
│  │  │ ...     │ ...                 │  │ │
│  │  ├─────────┼─────────────────────┤  │ │
│  │  │ 索引n   │ 第一个空闲寄存器     │  │ │
│  │  └─────────┴─────────────────────┘  │ │
│  └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

### 7.2 作用域链管理
```
┌─────────────────────────────────────────┐
│              作用域链                   │
│  BlockCnt 0 → BlockCnt 1 → BlockCnt 2   │
│  (最外层)   (函数体)    (if语句)        │
└─────────────────────────────────────────┘
```

## 8. 错误处理机制

### 8.1 智能错误提示
```c
static void aqlY_provide_suggestion(ALexState *ls, int expected) {
    switch (ls->t.token) {
        case TK_END:
            aqlX_syntaxerror(ls, "use '}' instead of 'end' in AQL");
            break;
        case TK_THEN:
            aqlX_syntaxerror(ls, "use '{' instead of 'then' in AQL");
            break;
        case TK_DO:
            aqlX_syntaxerror(ls, "use '{' instead of 'do' in AQL");
            break;
    }
}
```

### 8.2 语法错误定位
```c
static void aqlY_syntax_error(ALexState *ls, const char *msg) {
    const char *tokenstr = aqlX_token2str(ls, ls->t.token);
    aqlX_syntaxerror(ls, aqlO_pushfstring(ls->L, "%s near '%s'", msg, tokenstr));
}
```

## 9. 总结

本设计文档v6完整实现了基于Lua 5.4架构的AQL语法解析器，特点包括：

1. **100%类型兼容**：所有结构体使用现有AQL代码库类型
2. **完整流程适配**：严格按照Lua解析器主流程，适配AQL语法
3. **分阶段实现**：Phase 1完成基础语法，Phase 2扩展高级特性
4. **清晰架构**：递归下降解析器，结构清晰，易于维护
5. **错误友好**：智能错误提示，便于调试

该设计为AQL语言提供了坚实的解析基础，完全支持现代编程语言特性。