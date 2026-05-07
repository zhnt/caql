#ifndef ADEBUG_H
#define ADEBUG_H

#include <stdio.h>
#include <stdarg.h>

/* Canonical debug flag mask.  Keep old AQL_FLAG_* names as aliases while
** migrating the older adebug_user API onto this single runtime state. */
typedef unsigned int AQLDebugMask;

typedef enum {
    AQL_DBG_NONE    = 0x000,
    AQL_DBG_DETAIL  = 0x001,  /* 详细调试 (-vd) */
    AQL_DBG_VMTRACE = 0x002,  /* 执行跟踪 (-vt) */
    AQL_DBG_AST     = 0x004,  /* AST输出 (-vast) */
    AQL_DBG_LEX     = 0x008,  /* 词法分析 (-vl) */
    AQL_DBG_CODE    = 0x010,  /* 字节码输出 (-vb) */
    AQL_DBG_REG     = 0x020,
    AQL_DBG_MEM     = 0x040,
    AQL_DBG_GC      = 0x080,
    AQL_DBG_REPL    = 0x100,

    AQL_FLAG_VD   = AQL_DBG_DETAIL,
    AQL_FLAG_VT   = AQL_DBG_VMTRACE,
    AQL_FLAG_VAST = AQL_DBG_AST,
    AQL_FLAG_VL   = AQL_DBG_LEX,
    AQL_FLAG_VB   = AQL_DBG_CODE,
    AQL_FLAG_V    = AQL_DBG_DETAIL | AQL_DBG_VMTRACE | AQL_DBG_AST |
                    AQL_DBG_LEX | AQL_DBG_CODE,
    AQL_DBG_ALL   = AQL_FLAG_V | AQL_DBG_REG | AQL_DBG_MEM |
                    AQL_DBG_GC | AQL_DBG_REPL
} AQL_NewDebugFlags;

/* 格式化输出类型 */
typedef enum {
    AQL_FORMAT_TOKENS,    /* 词法分析输出 */
    AQL_FORMAT_AST,       /* AST树形输出 */
    AQL_FORMAT_BYTECODE,  /* 字节码表格输出 */
    AQL_FORMAT_TRACE      /* 执行跟踪输出 */
} AQL_FormatType;

typedef enum {
    AQL_DEBUG_TOOL_AQL,
    AQL_DEBUG_TOOL_AQLVM
} AQLDebugTool;

typedef enum {
    AQL_DEBUG_PARSE_ERROR = -1,
    AQL_DEBUG_PARSE_NO_MATCH = 0,
    AQL_DEBUG_PARSE_MATCH = 1
} AQLDebugParseResult;

/* 运行时控制函数声明 */
void aql_debug_enable_flag(AQL_NewDebugFlags flag);
void aql_debug_enable_verbose_all(void);
void aql_debug_disable_all(void);
int aql_debug_is_enabled(AQL_NewDebugFlags flag);
void aql_debug_set_flags(int flags);
AQLDebugMask aql_debug_get_flags(void);
void aql_debug_set_enabled(int enabled);
int aql_debug_is_any_enabled(void);
AQLDebugParseResult aql_debug_parse_option(AQLDebugTool tool,
                                           const char *arg,
                                           AQLDebugMask *mask,
                                           int *stop_after_lex,
                                           int *stop_after_parse,
                                           int *stop_after_compile);

/* 格式化输出函数声明 */
void aql_format_begin_impl(AQL_FormatType type, const char *title);
void aql_format_end_impl(AQL_FormatType type);
void aql_format_tree_node_impl(int level, const char *format, ...);
void aql_format_table_row_impl(const char *col1, const char *col2, const char *col3, const char *col4, const char *col5, const char *col6);
void aql_format_item_impl(const char *format, ...);

/* 基础输出函数声明 */
void aql_output_error(const char *format, ...);
void aql_output_info(const char *format, ...);
void aql_output_debug(const char *format, ...);
void aql_output_if_enabled(AQL_NewDebugFlags flag, const char *format, ...);
void aql_output_ast_if_enabled(int level, const char *format, ...);

/* 编译时宏系统 */
#ifdef AQL_PRODUCTION_BUILD
    /* 生产版本：完全无调试代码 */
    #define aql_error(...)          aql_output_error(__VA_ARGS__)
    #define aql_info(...)           ((void)0)
    #define aql_info_vd(...)        ((void)0)
    #define aql_info_vt(...)        ((void)0)
    #define aql_info_vast(...)      ((void)0)
    #define aql_info_vl(...)        ((void)0)
    #define aql_info_vb(...)        ((void)0)
    #define aql_debug(...)          ((void)0)
    
    /* 通用指令跟踪宏 */
    #define AQL_INFO_VT_BEFORE()    ((void)0)
    #define AQL_INFO_VT_AFTER()     ((void)0)
    
    /* 格式化输出函数在生产版本中也是空操作 */
    #define aql_format_begin(...)   ((void)0)
    #define aql_format_end(...)     ((void)0)
    #define aql_format_tree_node(...) ((void)0)
    #define aql_format_table_row(...) ((void)0)
    #define aql_format_item(...)    ((void)0)

#elif defined(AQL_DEBUG_BUILD)
    /* 调试版本：根据运行时标志控制 */
    #define aql_error(...)          aql_output_error(__VA_ARGS__)
    #define aql_info(...)           aql_output_info(__VA_ARGS__)
    #define aql_info_vd(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VD)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vt(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vast(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VAST)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vl(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VL)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vb(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VB)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_debug(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VD)) { \
                aql_output_debug(__VA_ARGS__); \
            } \
        } while(0)
    
    /* 通用指令跟踪宏 */
    #define AQL_INFO_VT_BEFORE() \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                /* 通用BEFORE逻辑将在avm_core.c中实现 */ \
            } \
        } while(0)
    #define AQL_INFO_VT_AFTER() \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                /* 通用AFTER逻辑将在avm_core.c中实现 */ \
            } \
        } while(0)
    
    /* 格式化输出 - 有条件编译 */
    #define aql_format_begin(type, title) aql_format_begin_impl(type, title)
    #define aql_format_end(type)    aql_format_end_impl(type)
    #define aql_format_table_row(...) aql_format_table_row_impl(__VA_ARGS__)
    #define aql_format_item(...)    aql_format_item_impl(__VA_ARGS__)
    #define aql_format_tree_node(level, ...) aql_format_tree_node_impl(level, __VA_ARGS__)

#else
    /* 默认构建 - 与DEBUG_BUILD相同 */
    #define aql_error(...)          aql_output_error(__VA_ARGS__)
    #define aql_info(...)           aql_output_info(__VA_ARGS__)
    #define aql_info_vd(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VD)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vt(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vast(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VAST)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vl(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VL)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_info_vb(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VB)) { \
                printf(__VA_ARGS__); \
            } \
        } while(0)
    #define aql_debug(...) \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VD)) { \
                aql_output_debug(__VA_ARGS__); \
            } \
        } while(0)
    
    /* 通用指令跟踪宏 */
    #define AQL_INFO_VT_BEFORE() \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                /* 通用BEFORE逻辑将在avm_core.c中实现 */ \
            } \
        } while(0)
    #define AQL_INFO_VT_AFTER() \
        do { \
            if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
                /* 通用AFTER逻辑将在avm_core.c中实现 */ \
            } \
        } while(0)
    
    /* 格式化输出 */
    #define aql_format_begin(type, title) aql_format_begin_impl(type, title)
    #define aql_format_end(type)    aql_format_end_impl(type)
    #define aql_format_table_row(...) aql_format_table_row_impl(__VA_ARGS__)
    #define aql_format_item(...)    aql_format_item_impl(__VA_ARGS__)
    #define aql_format_tree_node(level, ...) aql_format_tree_node_impl(level, __VA_ARGS__)
#endif

/* 便利宏 - 格式化输出的开始/结束 */
#define aql_info_vl_begin()     aql_format_begin(AQL_FORMAT_TOKENS, "词法分析结果")
#define aql_info_vl_end()       aql_format_end(AQL_FORMAT_TOKENS)
#define aql_info_vast_begin()   aql_format_begin(AQL_FORMAT_AST, "抽象语法树")
#define aql_info_vast_end()     aql_format_end(AQL_FORMAT_AST)
#define aql_info_vb_begin()     aql_format_begin(AQL_FORMAT_BYTECODE, "字节码")
#define aql_info_vb_end()       aql_format_end(AQL_FORMAT_BYTECODE)

#endif /* ADEBUG_H */
