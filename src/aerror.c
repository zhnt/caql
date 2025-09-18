/*
** $Id: aerror.c $
** AQL Unified Error Handling System Implementation
** See Copyright Notice in aql.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aerror.h"
#include "amem.h"

/*
** 全局错误上下文
*/
static AQL_ErrorContext g_error_ctx = {NULL, 0, 100, 1, "Global"};

/*
** 控制是否立即打印错误（用于 REPL 模式）
*/
static int g_immediate_error_print = 1;

/*
** 错误类型字符串映射
*/
static const char *error_type_strings[] = {
  "Syntax Error",
  "Runtime Error", 
  "Type Error",
  "Name Error",
  "Memory Error",
  "I/O Error",
  "Internal Error"
};

/*
** 错误级别字符串映射
*/
static const char *error_level_strings[] = {
  "Info",
  "Warning",
  "Error", 
  "Fatal"
};

/*
** 创建错误对象
*/
AQL_API AQL_Error *aqlE_create_error(AQL_ErrorType type, AQL_ErrorLevel level, 
                                     int line, const char *message, 
                                     const char *suggestion) {
  AQL_Error *error = (AQL_Error *)malloc(sizeof(AQL_Error));
  if (!error) return NULL;
  
  error->type = type;
  error->level = level;
  error->line = line;
  error->column = 0;  /* 暂时不支持列号 */
  
  /* 复制消息字符串 */
  if (message) {
    size_t len = strlen(message) + 1;
    char *msg_copy = (char *)malloc(len);
    if (msg_copy) {
      strcpy(msg_copy, message);
      error->message = msg_copy;
    } else {
      error->message = "Memory allocation failed for error message";
    }
  } else {
    error->message = "Unknown error";
  }
  
  /* 复制建议字符串 */
  if (suggestion) {
    size_t len = strlen(suggestion) + 1;
    char *sug_copy = (char *)malloc(len);
    if (sug_copy) {
      strcpy(sug_copy, suggestion);
      error->suggestion = sug_copy;
    } else {
      error->suggestion = NULL;
    }
  } else {
    error->suggestion = NULL;
  }
  
  error->context = NULL;
  error->next = NULL;
  
  return error;
}

/*
** 释放错误对象
*/
static void free_error(AQL_Error *error) {
  if (!error) return;
  
  if (error->message) free((void *)error->message);
  if (error->suggestion) free((void *)error->suggestion);
  if (error->context) free((void *)error->context);
  free(error);
}

/*
** 添加错误到上下文
*/
AQL_API int aqlE_add_error(AQL_ErrorContext *ctx, AQL_Error *error) {
  if (!ctx || !error) return 0;
  
  /* 检查错误数量限制 */
  if (ctx->error_count >= ctx->max_errors) {
    free_error(error);
    return 0;
  }
  
  /* 添加到链表头部 */
  error->next = ctx->error_list;
  ctx->error_list = error;
  ctx->error_count++;
  
  return 1;
}

/*
** 报告错误
*/
AQL_API void aqlE_report_error(AQL_ErrorType type, AQL_ErrorLevel level, 
                               int line, const char *message, 
                               const char *suggestion) {
  AQL_Error *error = aqlE_create_error(type, level, line, message, suggestion);
  if (error) {
    aqlE_add_error(&g_error_ctx, error);
    
    /* 根据标志决定是否立即打印错误 */
    if (g_immediate_error_print) {
      char *formatted = aqlE_format_error_message(error);
      if (formatted) {
        fprintf(stderr, "%s\n", formatted);
        free(formatted);
      }
    }
  }
}

/*
** 格式化错误消息
*/
AQL_API char *aqlE_format_error_message(const AQL_Error *error) {
  if (!error) return NULL;
  
  const char *type_str = aqlE_get_error_type_string(error->type);
  const char *level_str = aqlE_get_error_level_string(error->level);
  
  /* 计算所需缓冲区大小 */
  size_t base_len = strlen(type_str) + strlen(level_str) + strlen(error->message) + 50;
  size_t suggestion_len = error->suggestion ? strlen(error->suggestion) + 20 : 0;
  size_t total_len = base_len + suggestion_len;
  
  char *buffer = (char *)malloc(total_len);
  if (!buffer) return NULL;
  
  if (error->line > 0) {
    snprintf(buffer, total_len, "[%s] %s at line %d: %s", 
             level_str, type_str, error->line, error->message);
  } else {
    snprintf(buffer, total_len, "[%s] %s: %s", 
             level_str, type_str, error->message);
  }
  
  /* 添加建议 */
  if (error->suggestion && g_error_ctx.show_suggestions) {
    strcat(buffer, "\n  Suggestion: ");
    strcat(buffer, error->suggestion);
  }
  
  return buffer;
}

/*
** 清理错误上下文
*/
AQL_API void aqlE_clear_errors(AQL_ErrorContext *ctx) {
  if (!ctx) return;
  
  AQL_Error *current = ctx->error_list;
  while (current) {
    AQL_Error *next = current->next;
    free_error(current);
    current = next;
  }
  
  ctx->error_list = NULL;
  ctx->error_count = 0;
}

/*
** 初始化错误上下文
*/
AQL_API void aqlE_init_context(AQL_ErrorContext *ctx, const char *context_name) {
  if (!ctx) return;
  
  ctx->error_list = NULL;
  ctx->error_count = 0;
  ctx->max_errors = 100;
  ctx->show_suggestions = 1;
  
  if (context_name) {
    size_t len = strlen(context_name) + 1;
    char *name_copy = (char *)malloc(len);
    if (name_copy) {
      strcpy(name_copy, context_name);
      ctx->context_name = name_copy;
    } else {
      ctx->context_name = "Unknown";
    }
  } else {
    ctx->context_name = "Default";
  }
}

/*
** 销毁错误上下文
*/
AQL_API void aqlE_destroy_context(AQL_ErrorContext *ctx) {
  if (!ctx) return;
  
  aqlE_clear_errors(ctx);
  
  if (ctx->context_name && strcmp(ctx->context_name, "Global") != 0 && 
      strcmp(ctx->context_name, "Default") != 0 && 
      strcmp(ctx->context_name, "Unknown") != 0) {
    free((void *)ctx->context_name);
  }
  
  ctx->context_name = NULL;
}

/*
** 获取错误类型字符串
*/
AQL_API const char *aqlE_get_error_type_string(AQL_ErrorType type) {
  if (type >= 0 && type < sizeof(error_type_strings)/sizeof(error_type_strings[0])) {
    return error_type_strings[type];
  }
  return "Unknown Error Type";
}

/*
** 获取错误级别字符串
*/
AQL_API const char *aqlE_get_error_level_string(AQL_ErrorLevel level) {
  if (level >= 0 && level < sizeof(error_level_strings)/sizeof(error_level_strings[0])) {
    return error_level_strings[level];
  }
  return "Unknown Level";
}

/*
** 打印错误报告
*/
AQL_API void aqlE_print_error_report(const AQL_ErrorContext *ctx) {
  if (!ctx || !ctx->error_list) {
    printf("No errors to report.\n");
    return;
  }
  
  printf("\n=== Error Report for %s ===\n", ctx->context_name);
  printf("Total errors: %d\n\n", ctx->error_count);
  
  AQL_Error *current = ctx->error_list;
  int count = 1;
  
  while (current) {
    char *formatted = aqlE_format_error_message(current);
    if (formatted) {
      printf("%d. %s\n", count++, formatted);
      free(formatted);
    }
    current = current->next;
  }
  
  printf("\n=== End of Error Report ===\n");
}

/*
** 检查是否有错误
*/
AQL_API int aqlE_has_errors(const AQL_ErrorContext *ctx) {
  return ctx && ctx->error_count > 0;
}

/*
** 检查是否有致命错误
*/
AQL_API int aqlE_has_fatal_errors(const AQL_ErrorContext *ctx) {
  if (!ctx || !ctx->error_list) return 0;
  
  AQL_Error *current = ctx->error_list;
  while (current) {
    if (current->level == AQL_ERROR_LEVEL_FATAL) {
      return 1;
    }
    current = current->next;
  }
  
  return 0;
}

/*
** 获取错误数量
*/
AQL_API int aqlE_get_error_count(const AQL_ErrorContext *ctx) {
  return ctx ? ctx->error_count : 0;
}

/*
** 获取全局错误上下文
*/
AQL_API AQL_ErrorContext *aqlE_get_global_context(void) {
  return &g_error_ctx;
}

/*
** 词法分析器错误报告接口
*/
AQL_API void aqlE_report_lexical_error(int line, const char *message, 
                                       const char *suggestion, const char *near_token) {
  char full_message[512];
  
  if (near_token) {
    snprintf(full_message, sizeof(full_message), "%s near '%s'", message, near_token);
  } else {
    snprintf(full_message, sizeof(full_message), "%s", message);
  }
  
  aqlE_report_error(AQL_ERROR_SYNTAX, AQL_ERROR_LEVEL_ERROR, line, full_message, suggestion);
}

/*
** 语法分析器错误报告接口
*/
AQL_API void aqlE_report_syntax_error(int line, const char *message,
                                      const char *suggestion, const char *near_token) {
  char full_message[512];
  
  if (near_token) {
    snprintf(full_message, sizeof(full_message), "%s near '%s'", message, near_token);
  } else {
    snprintf(full_message, sizeof(full_message), "%s", message);
  }
  
  aqlE_report_error(AQL_ERROR_SYNTAX, AQL_ERROR_LEVEL_ERROR, line, full_message, suggestion);
}

/*
** 设置是否立即打印错误
*/
AQL_API void aqlE_set_immediate_print(int enable) {
  g_immediate_error_print = enable;
}

/*
** 获取当前立即打印设置
*/
AQL_API int aqlE_get_immediate_print(void) {
  return g_immediate_error_print;
}

/*
** REPL错误恢复
*/
AQL_API void aqlE_repl_error_recovery(void) {
  if (aqlE_has_errors(&g_error_ctx)) {
    printf("Errors cleared. You can continue entering commands.\n");
    aqlE_clear_errors(&g_error_ctx);
  }
}

