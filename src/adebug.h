#ifndef ADEBUG_H
#define ADEBUG_H

#include <stdio.h>
#include <stdarg.h>

/* 调试标志位定义 */
typedef enum {
    AQL_FLAG_VD   = 0x01,  /* 详细调试 (-vd) */
    AQL_FLAG_VT   = 0x02,  /* 执行跟踪 (-vt) */
    AQL_FLAG_VAST = 0x04,  /* AST输出 (-vast) */
    AQL_FLAG_VL   = 0x08,  /* 词法分析 (-vl) */
    AQL_FLAG_VB   = 0x10,  /* 字节码输出 (-vb) */
    AQL_FLAG_V    = 0x1F   /* 全部详细信息 (-v) */
} AQL_NewDebugFlags;

/* 格式化输出类型 */
typedef enum {
    AQL_FORMAT_TOKENS,    /* 词法分析输出 */
    AQL_FORMAT_AST,       /* AST树形输出 */
    AQL_FORMAT_BYTECODE,  /* 字节码表格输出 */
    AQL_FORMAT_TRACE      /* 执行跟踪输出 */
} AQL_FormatType;

/* 运行时控制函数声明 */
void aql_debug_enable_flag(AQL_NewDebugFlags flag);
void aql_debug_enable_verbose_all(void);
void aql_debug_disable_all(void);
int aql_debug_is_enabled(AQL_NewDebugFlags flag);

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
    
    /* 格式化输出函数在生产版本中也是空操作 */
    #define aql_format_begin(...)   ((void)0)
    #define aql_format_end(...)     ((void)0)
    #define aql_format_tree_node(...) ((void)0)
    #define aql_format_table_row(...) ((void)0)
    #define aql_format_item(...)    ((void)0)

#elif defined(AQL_VM_BUILD)
    /* aqlm字节码工具 */
    #define aql_error(...)          aql_output_error(__VA_ARGS__)
    #define aql_info(...)           aql_output_info(__VA_ARGS__)
    #define aql_info_vd(...)        aql_output_if_enabled(AQL_FLAG_VD, __VA_ARGS__)
    #define aql_info_vt(...)        aql_output_if_enabled(AQL_FLAG_VT, __VA_ARGS__)
    #define aql_info_vast(...)      ((void)0)  /* aqlm不支持AST */
    #define aql_info_vl(...)        ((void)0)  /* aqlm不支持词法 */
    #define aql_info_vb(...)        aql_output_if_enabled(AQL_FLAG_VB, __VA_ARGS__)
    #define aql_debug(...)          aql_output_if_enabled(AQL_FLAG_VD, __VA_ARGS__)
    
    /* 格式化输出 - 有条件编译 */
    #define aql_format_begin(type, title) aql_format_begin_impl(type, title)
    #define aql_format_end(type)    aql_format_end_impl(type)
    #define aql_format_table_row(...) aql_format_table_row_impl(__VA_ARGS__)
    #define aql_format_item(...)    aql_format_item_impl(__VA_ARGS__)
    #define aql_format_tree_node(...) ((void)0)  /* aqlm不需要AST格式 */

#elif defined(AQL_DEBUG_BUILD)
    /* aqld调试版本 */
    #define aql_error(...)          aql_output_error(__VA_ARGS__)
    #define aql_info(...)           aql_output_info(__VA_ARGS__)
    #define aql_info_vd(...)        aql_output_if_enabled(AQL_FLAG_VD, __VA_ARGS__)
    #define aql_info_vt(...)        aql_output_if_enabled(AQL_FLAG_VT, __VA_ARGS__)
    #define aql_info_vast(level, ...) aql_output_ast_if_enabled(level, __VA_ARGS__)
    #define aql_info_vl(...)        aql_output_if_enabled(AQL_FLAG_VL, __VA_ARGS__)
    #define aql_info_vb(...)        aql_output_if_enabled(AQL_FLAG_VB, __VA_ARGS__)
    #define aql_debug(...)          aql_output_debug(__VA_ARGS__)
    
    /* 格式化输出 - 完整支持 */
    #define aql_format_begin(type, title) aql_format_begin_impl(type, title)
    #define aql_format_end(type)    aql_format_end_impl(type)
    #define aql_format_tree_node(level, ...) aql_format_tree_node_impl(level, __VA_ARGS__)
    #define aql_format_table_row(...) aql_format_table_row_impl(__VA_ARGS__)
    #define aql_format_item(...)    aql_format_item_impl(__VA_ARGS__)

#else
    /* 默认构建 - 与VM_BUILD相同 */
    #define aql_error(...)          aql_output_error(__VA_ARGS__)
    #define aql_info(...)           aql_output_info(__VA_ARGS__)
    #define aql_info_vd(...)        aql_output_if_enabled(AQL_FLAG_VD, __VA_ARGS__)
    #define aql_info_vt(...)        aql_output_if_enabled(AQL_FLAG_VT, __VA_ARGS__)
    #define aql_info_vast(...)      ((void)0)
    #define aql_info_vl(...)        ((void)0)
    #define aql_info_vb(...)        aql_output_if_enabled(AQL_FLAG_VB, __VA_ARGS__)
    #define aql_debug(...)          ((void)0)
    
    #define aql_format_begin(type, title) aql_format_begin_impl(type, title)
    #define aql_format_end(type)    aql_format_end_impl(type)
    #define aql_format_table_row(...) aql_format_table_row_impl(__VA_ARGS__)
    #define aql_format_item(...)    aql_format_item_impl(__VA_ARGS__)
    #define aql_format_tree_node(...) ((void)0)
#endif

/* 便利宏 - 格式化输出的开始/结束 */
#define aql_info_vl_begin()     aql_format_begin(AQL_FORMAT_TOKENS, "词法分析结果")
#define aql_info_vl_end()       aql_format_end(AQL_FORMAT_TOKENS)
#define aql_info_vast_begin()   aql_format_begin(AQL_FORMAT_AST, "抽象语法树")
#define aql_info_vast_end()     aql_format_end(AQL_FORMAT_AST)
#define aql_info_vb_begin()     aql_format_begin(AQL_FORMAT_BYTECODE, "字节码")
#define aql_info_vb_end()       aql_format_end(AQL_FORMAT_BYTECODE)

#endif /* ADEBUG_H */
