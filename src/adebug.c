#include "adebug.h"
#include <string.h>

/* 全局调试标志位 */
static int aql_debug_flags = 0;

/* 运行时控制函数实现 */
void aql_debug_enable_flag(AQL_NewDebugFlags flag) {
    aql_debug_flags |= flag;
}

void aql_debug_enable_verbose_all(void) {
    aql_debug_flags = AQL_FLAG_V;
}

void aql_debug_disable_all(void) {
    aql_debug_flags = 0;
}

int aql_debug_is_enabled(AQL_NewDebugFlags flag) {
    return (aql_debug_flags & flag) != 0;
}

/* 基础输出函数实现 */
void aql_output_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "❌ [ERROR] ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void aql_output_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("ℹ️  [INFO] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void aql_output_debug(const char *format, ...) {
#ifdef AQL_DEBUG_BUILD
    va_list args;
    va_start(args, format);
    printf("🔧 [DEBUG] ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
#endif
}

void aql_output_if_enabled(AQL_NewDebugFlags flag, const char *format, ...) {
    if (!aql_debug_is_enabled(flag)) return;
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void aql_output_ast_if_enabled(int level, const char *format, ...) {
    if (!aql_debug_is_enabled(AQL_FLAG_VAST)) return;
    
    va_list args;
    char buffer[256];
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    /* 树形缩进 */
    for (int i = 0; i < level; i++) {
        printf(i == level - 1 ? "├── " : "│   ");
    }
    if (level == 0) printf("└── ");
    
    printf("%s\n", buffer);
}

/* 格式化输出样式定义 */
static const char* format_headers[] = {
    /* AQL_FORMAT_TOKENS */
    "┌─────────────────────────────────────────────────────────────────┐\n"
    "│ %-63s │\n"
    "├─────────────┬─────────────────┬─────────┬─────────────────────┤\n"
    "│    类型     │      内容       │  行号   │        位置         │\n"
    "├─────────────┼─────────────────┼─────────┼─────────────────────┤\n",
    
    /* AQL_FORMAT_AST */
    "┌─────────────────────────────────────────────────────────────────┐\n"
    "│ %-63s │\n"
    "└─────────────────────────────────────────────────────────────────┘\n",
    
    /* AQL_FORMAT_BYTECODE */
    "┌─────────────────────────────────────────────────────────────────┐\n"
    "│ %-63s │\n"
    "├─────┬─────────────┬─────────┬─────────┬─────────┬─────────────────┤\n"
    "│ PC  │    指令     │    A    │    B    │    C    │      说明       │\n"
    "├─────┼─────────────┼─────────┼─────────┼─────────┼─────────────────┤\n",
    
    /* AQL_FORMAT_TRACE */
    "" /* 执行跟踪无需开始格式 */
};

static const char* format_footers[] = {
    /* AQL_FORMAT_TOKENS */
    "└─────────────┴─────────────────┴─────────┴─────────────────────┘\n\n",
    
    /* AQL_FORMAT_AST */
    "\n",
    
    /* AQL_FORMAT_BYTECODE */
    "└─────┴─────────────┴─────────┴─────────┴─────────┴─────────────────┘\n\n",
    
    /* AQL_FORMAT_TRACE */
    "" /* 执行跟踪无需结束格式 */
};

/* 检查是否应该输出格式 */
static int aql_should_output_format(AQL_FormatType type) {
    switch (type) {
        case AQL_FORMAT_TOKENS:   return aql_debug_is_enabled(AQL_FLAG_VL);
        case AQL_FORMAT_AST:      return aql_debug_is_enabled(AQL_FLAG_VAST);
        case AQL_FORMAT_BYTECODE: return aql_debug_is_enabled(AQL_FLAG_VB);
        case AQL_FORMAT_TRACE:    return aql_debug_is_enabled(AQL_FLAG_VT);
        default:                  return 0;
    }
}

/* 格式化输出函数实现 */
void aql_format_begin_impl(AQL_FormatType type, const char *title) {
    if (!aql_should_output_format(type)) return;
    
    if (type < sizeof(format_headers)/sizeof(format_headers[0])) {
        printf(format_headers[type], title);
    }
}

void aql_format_end_impl(AQL_FormatType type) {
    if (!aql_should_output_format(type)) return;
    
    if (type < sizeof(format_footers)/sizeof(format_footers[0])) {
        printf("%s", format_footers[type]);
    }
}

void aql_format_tree_node_impl(int level, const char *format, ...) {
    if (!aql_debug_is_enabled(AQL_FLAG_VAST)) return;
    
    va_list args;
    char buffer[256];
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    /* 树形缩进 */
    for (int i = 0; i < level; i++) {
        printf(i == level - 1 ? "├── " : "│   ");
    }
    if (level == 0) printf("└── ");
    
    printf("%s\n", buffer);
}

void aql_format_table_row_impl(const char *col1, const char *col2, const char *col3, const char *col4, const char *col5, const char *col6) {
    if (!aql_debug_is_enabled(AQL_FLAG_VB)) return;
    
    printf("│ %-3s │ %-11s │ %-7s │ %-7s │ %-7s │ %-15s │\n", 
           col1 ? col1 : "", 
           col2 ? col2 : "", 
           col3 ? col3 : "", 
           col4 ? col4 : "", 
           col5 ? col5 : "", 
           col6 ? col6 : "");
}

void aql_format_item_impl(const char *format, ...) {
    if (!aql_debug_is_enabled(AQL_FLAG_VL)) return;
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
