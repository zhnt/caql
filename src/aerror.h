/*
** $Id: aerror.h $
** AQL Unified Error Handling System
** See Copyright Notice in aql.h
*/

#ifndef aerror_h
#define aerror_h

#include "aconf.h"

/*
** Error Types - 错误类型枚举
*/
typedef enum {
  AQL_ERROR_SYNTAX = 0,    /* 语法错误 */
  AQL_ERROR_RUNTIME,       /* 运行时错误 */
  AQL_ERROR_TYPE,          /* 类型错误 */
  AQL_ERROR_NAME,          /* 名称错误 (未定义变量等) */
  AQL_ERROR_MEMORY,        /* 内存错误 */
  AQL_ERROR_IO,            /* I/O错误 */
  AQL_ERROR_INTERNAL       /* 内部错误 */
} AQL_ErrorType;

/*
** Error Levels - 错误级别枚举
*/
typedef enum {
  AQL_ERROR_LEVEL_INFO = 0,    /* 信息 */
  AQL_ERROR_LEVEL_WARNING,     /* 警告 */
  AQL_ERROR_LEVEL_ERROR,       /* 错误 */
  AQL_ERROR_LEVEL_FATAL        /* 致命错误 */
} AQL_ErrorLevel;

/*
** Error Structure - 错误结构体
*/
typedef struct AQL_Error {
  AQL_ErrorType type;          /* 错误类型 */
  AQL_ErrorLevel level;        /* 错误级别 */
  int line;                    /* 行号 */
  int column;                  /* 列号 */
  const char *message;         /* 错误消息 */
  const char *suggestion;      /* 修复建议 */
  const char *context;         /* 错误上下文 */
  struct AQL_Error *next;      /* 链表下一个节点 */
} AQL_Error;

/*
** Error Context - 错误上下文管理
*/
typedef struct AQL_ErrorContext {
  AQL_Error *error_list;       /* 错误链表头 */
  int error_count;             /* 错误数量 */
  int max_errors;              /* 最大错误数 */
  int show_suggestions;        /* 是否显示建议 */
  const char *context_name;    /* 上下文名称 */
} AQL_ErrorContext;

/*
** Error Handling API
*/

/* 创建错误对象 */
AQL_API AQL_Error *aqlE_create_error(AQL_ErrorType type, AQL_ErrorLevel level, 
                                     int line, const char *message, 
                                     const char *suggestion);

/* 添加错误到上下文 */
AQL_API int aqlE_add_error(AQL_ErrorContext *ctx, AQL_Error *error);

/* 报告错误 */
AQL_API void aqlE_report_error(AQL_ErrorType type, AQL_ErrorLevel level, 
                               int line, const char *message, 
                               const char *suggestion);

/* 格式化错误消息 */
AQL_API char *aqlE_format_error_message(const AQL_Error *error);

/* 清理错误上下文 */
AQL_API void aqlE_clear_errors(AQL_ErrorContext *ctx);

/* 初始化错误上下文 */
AQL_API void aqlE_init_context(AQL_ErrorContext *ctx, const char *context_name);

/* 销毁错误上下文 */
AQL_API void aqlE_destroy_context(AQL_ErrorContext *ctx);

/* 获取错误类型字符串 */
AQL_API const char *aqlE_get_error_type_string(AQL_ErrorType type);

/* 获取错误级别字符串 */
AQL_API const char *aqlE_get_error_level_string(AQL_ErrorLevel level);

/* 打印错误报告 */
AQL_API void aqlE_print_error_report(const AQL_ErrorContext *ctx);

/* 检查是否有错误 */
AQL_API int aqlE_has_errors(const AQL_ErrorContext *ctx);

/* 检查是否有致命错误 */
AQL_API int aqlE_has_fatal_errors(const AQL_ErrorContext *ctx);

/* 获取错误数量 */
AQL_API int aqlE_get_error_count(const AQL_ErrorContext *ctx);

/*
** 便利宏定义
*/
#define AQLE_REPORT_SYNTAX_ERROR(line, msg, suggestion) \
  aqlE_report_error(AQL_ERROR_SYNTAX, AQL_ERROR_LEVEL_ERROR, line, msg, suggestion)

#define AQLE_REPORT_NAME_ERROR(line, msg, suggestion) \
  aqlE_report_error(AQL_ERROR_NAME, AQL_ERROR_LEVEL_ERROR, line, msg, suggestion)

#define AQLE_REPORT_RUNTIME_ERROR(line, msg, suggestion) \
  aqlE_report_error(AQL_ERROR_RUNTIME, AQL_ERROR_LEVEL_ERROR, line, msg, suggestion)

#define AQLE_REPORT_WARNING(line, msg, suggestion) \
  aqlE_report_error(AQL_ERROR_SYNTAX, AQL_ERROR_LEVEL_WARNING, line, msg, suggestion)


/*
** ============================================================================
** 便利宏定义 (使用 aerror.h 模块)
** ============================================================================
*/

/* 便捷的错误报告宏 */
#define REPORT_SYNTAX_ERROR(line, msg, suggestion) \
  aqlE_report_error(AQL_ERROR_SYNTAX, AQL_ERROR_LEVEL_ERROR, line, msg, suggestion)

#define REPORT_NAME_ERROR(line, msg, suggestion) \
  aqlE_report_error(AQL_ERROR_NAME, AQL_ERROR_LEVEL_ERROR, line, msg, suggestion)

#define REPORT_RUNTIME_ERROR(line, msg, suggestion) \
  aqlE_report_error(AQL_ERROR_RUNTIME, AQL_ERROR_LEVEL_ERROR, line, msg, suggestion)

#define REPORT_WARNING(line, msg, suggestion) \
  aqlE_report_error(AQL_ERROR_SYNTAX, AQL_ERROR_LEVEL_WARNING, line, msg, suggestion)
  

/*
** 全局错误上下文访问
*/
AQL_API AQL_ErrorContext *aqlE_get_global_context(void);

/*
** 控制错误打印行为
*/
AQL_API void aqlE_set_immediate_print(int enable);
AQL_API int aqlE_get_immediate_print(void);

/*
** 词法分析器错误报告接口
*/
AQL_API void aqlE_report_lexical_error(int line, const char *message,
                                       const char *suggestion, const char *near_token);

/*
** 语法分析器错误报告接口  
*/
AQL_API void aqlE_report_syntax_error(int line, const char *message,
                                      const char *suggestion, const char *near_token);

/*
** REPL错误恢复
*/
AQL_API void aqlE_repl_error_recovery(void);

#endif

