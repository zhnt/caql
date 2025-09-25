/*
** $Id: arepl.c $
** AQL REPL (Read-Eval-Print Loop) implementation
** Based on Lua's lua.c REPL architecture
** See Copyright Notice in aql.h
*/

#define arepl_c
#define AQL_CORE

#include "aconf.h"

#include <stdio.h>

#include "adebug_user.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "aql.h"
#include "aobject.h"
#include "astate.h"
#include "aparser.h"
#include "aapi.h"
#include "aerror.h"
#include "azio.h"

/*
** Check if return value should be suppressed for side-effect functions
*/
static int should_suppress_return_value(const char *line, const TValue *result) {
  if (!line || !result) return 0;
  
  /* Skip whitespace at the beginning */
  while (*line == ' ' || *line == '\t') line++;
  
  /* Side-effect functions that shouldn't show return values */
  if (strncmp(line, "print(", 6) == 0) {
    /* print() already produced output, suppress nil return value */
    return (ttisnil(result));
  }
  
  /* Future side-effect functions can be added here:
   * - debug functions that output to console
   * - file write operations  
   * - system commands
   */
  
  return 0;  /* Don't suppress by default */
}

/*
** Try to compile line as 'return <line>;' and execute it.
** This is only used for REPL single-line input to show expression results.
** Similar to Lua's addreturn() + docall()
*/
static int aql_addreturn(aql_State *L, const char *line) {
  printf_debug("[DEBUG] aql_addreturn: entering with line='%s'\n", line ? line : "NULL");
  
  /* Safety checks */
  if (!L || !line) {
    printf_debug("[Error] Internal error: NULL pointer in aql_addreturn\n");
    return -1;
  }
  
  printf_debug("[DEBUG] aql_addreturn: safety checks passed\n");
  
  /* Follow Lua's approach: always try adding 'return' first */
  size_t line_len = strlen(line);
  size_t ret_len = line_len + 10;  /* "return " + line + ";" + null terminator */
  char *retline = malloc(ret_len);
  if (!retline) {
    printf("[Error] Memory allocation failed in aql_addreturn\n");
    return -1;
  }
  
  snprintf(retline, ret_len, "return %s;", line);
  printf_debug("[DEBUG] aql_addreturn: created retline='%s'\n", retline);
  
  /* Try to compile as return statement */
  printf_debug("[DEBUG] aql_addreturn: calling aqlP_compile_string\n");
  int status = aqlP_compile_string(L, retline, strlen(retline), "=stdin");
  printf_debug("[DEBUG] aql_addreturn: aqlP_compile_string returned status=%d\n", status);
  
  if (status == 0) {
    /* Check if there were any errors reported despite status=0 */
    if (aqlE_has_errors(aqlE_get_global_context())) {
      printf_debug("[DEBUG] aql_addreturn: compilation returned success but has errors, treating as failure\n");
      /* Clear any errors from this attempt (Lua style) */
      aqlE_clear_errors(aqlE_get_global_context());
      free(retline);
      return -1;  /* Treat as compilation failure */
    }
    
    /* Compilation successful - execute the compiled code */
    printf_debug("[DEBUG] aql_addreturn: compilation successful, executing\n");
    status = aqlP_execute_compiled(L, 0, 1);  /* 0 args, 1 result expected */
    printf_debug("[DEBUG] aql_addreturn: aqlP_execute_compiled returned status=%d\n", status);
    
    if (status == 1) {  /* aqlV_execute returns 1 for successful completion */
      /* Print the result from stack */
      //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: checking for result on stack, L->top=%p, L->ci->func=%p", 
               //(void*)L->top, (void*)L->ci->func);
      //fflush(stdout);
      
      /* Safety checks for stack access */
      if (!L->top || !L->ci || !L->ci->func) {
        printf_debug("[Error] Invalid stack state after execution\n");
        free(retline);
        return -1;
      }
      
      if (L->top > L->ci->func + 1) {  /* Has result? */
        TValue *result = s2v(L->top - 1);
        if (result) {
          /* Check if we should suppress return value display */
          if (should_suppress_return_value(line, result)) {
            printf_debug("[DEBUG] aql_addreturn: suppressing return value for side-effect function\n");
          } else {
            //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: found result on stack, printing");
            //fflush(stdout);
            aqlP_print_value(result);
            printf("\n");
          }
        } else {
          printf_debug("[Error] Invalid result pointer\n");
        }
      } else {
        //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: no result on stack");
        //fflush(stdout);
      }
      
      /* Clean up stack safely */
      if (L->ci && L->ci->func) {
        L->top = L->ci->func + 1;
        //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: cleaned up stack, L->top reset");
        //fflush(stdout);
      }
      
      free(retline);
      return 0;  /* Success */
    } else {
      //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: execution failed with status=%d", status);
      //fflush(stdout);
    }
    
    /* Clean up stack for failed execution */
    if (L->ci && L->ci->func) {
      L->top = L->ci->func + 1;
      //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: cleaned up stack after execution failure");
      //fflush(stdout);
    } else {
      printf_debug("[Warning] Cannot clean up stack: invalid call info\n");
    }
  } else {
    printf_debug("[DEBUG] aql_addreturn: compilation failed, will try as statement\n");
  }
  
  /* Clear any errors from this attempt (Lua style) */
  aqlE_clear_errors(aqlE_get_global_context());
  
  free(retline);
  return -1;  /* Failed - let multiline try as statement */
}

/*
** Read multiple lines until a complete AQL statement
** Similar to Lua's multiline()
*/
static int aql_multiline(aql_State *L, const char *line) {
  printf_debug("[DEBUG] aql_multiline: entering with line='%s'\n", line ? line : "NULL");
  
  /* Safety checks */
  if (!L || !line) {
    printf("[Error] Internal error: NULL pointer in aql_multiline\n");
    return -1;
  }
  
  printf_debug("[DEBUG] aql_multiline: safety checks passed\n");
  
  /* Try to compile and execute as statement (without 'return') */
  printf_debug("[DEBUG] aql_multiline: calling aqlP_compile_string with line='%s', len=%zu\n", line, strlen(line));
  int status = aqlP_compile_string(L, line, strlen(line), "=stdin");
  printf_debug("[DEBUG] aql_multiline: aqlP_compile_string returned status=%d\n", status);
  
  if (status == 0) {
    /* Check if there were any errors reported despite status=0 */
    if (aqlE_has_errors(aqlE_get_global_context())) {
      printf_debug("[DEBUG] aql_multiline: compilation returned success but has errors, treating as failure\n");
      /* Print the actual errors for debugging */
      aqlE_print_error_report(aqlE_get_global_context());
      return -1;  /* Treat as compilation failure */
    }
    
    /* Compilation successful - execute the compiled code */
    printf_debug("[DEBUG] aql_multiline: compilation successful, executing\n");
    status = aqlP_execute_compiled(L, 0, 0);  /* 0 args, 0 results for statements */
    AQL_DEBUG(AQL_DEBUG_REPL, "aql_multiline: execute returned status=%d", status);
    fflush(stdout);
    
    if (status == 1) {  /* aqlV_execute returns 1 for successful completion */
      AQL_DEBUG(AQL_DEBUG_REPL, "aql_multiline: statement executed successfully");
      fflush(stdout);
      
      /* Clean up stack */
      L->top = L->ci->func + 1;
      AQL_DEBUG(AQL_DEBUG_REPL, "aql_multiline: cleaned up stack");
      fflush(stdout);
      
      return 0;  /* Success */
    } else {
      AQL_DEBUG(AQL_DEBUG_REPL, "aql_multiline: execution failed with status=%d", status);
      fflush(stdout);
    }
    
    /* Clean up stack for failed execution */
    L->top = L->ci->func + 1;
    AQL_DEBUG(AQL_DEBUG_REPL, "aql_multiline: cleaned up stack after execution failure");
    fflush(stdout);
  } else {
    printf_debug("[DEBUG] aql_multiline: compilation failed\n");
  }
  
  return -1;  /* Failed */
}

/*
** Check if input is incomplete (needs more lines)
*/
static int aql_is_incomplete(const char *line) {
  if (!line) return 0;
  
  int brace_count = 0;
  int paren_count = 0;
  int bracket_count = 0;
  bool in_string = false;
  bool in_comment = false;
  char quote_char = 0;
  const char *last_non_space = NULL;
  
  for (const char *p = line; *p; p++) {
    if (in_comment) {
      if (*p == '\n') in_comment = false;
      continue;
    }
    
    if (in_string) {
      if (*p == quote_char && (p == line || *(p-1) != '\\')) {
        in_string = false;
        quote_char = 0;
      }
      continue;
    }
    
    /* Track last non-whitespace character */
    if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
      last_non_space = p;
    }
    
    switch (*p) {
      case '"':
      case '\'':
        in_string = true;
        quote_char = *p;
        break;
      case '-':
        if (*(p+1) == '-') {
          in_comment = true;
          p++;  /* skip next '-' */
        }
        break;
      case '{': brace_count++; break;
      case '}': brace_count--; break;
      case '(': paren_count++; break;
      case ')': paren_count--; break;
      case '[': bracket_count++; break;
      case ']': bracket_count--; break;
    }
  }
  
  /* Check for unmatched brackets/braces/parens */
  if (brace_count > 0 || paren_count > 0 || bracket_count > 0 || in_string || in_comment) {
    return 1;
  }
  
  /* Check if line ends with an operator (indicating incomplete expression) */
  if (last_non_space) {
    char last_char = *last_non_space;
    /* Check for binary operators that need a right operand */
    if (last_char == '+' || last_char == '-' || last_char == '*' || 
        last_char == '/' || last_char == '%' || last_char == '=' ||
        last_char == '<' || last_char == '>' || last_char == '!' ||
        last_char == '&' || last_char == '|' || last_char == '^') {
      return 1;
    }
    
    /* Check for ** (power operator) */
    if (last_char == '*' && last_non_space > line && *(last_non_space-1) == '*') {
      return 1;
    }
  }
  
  return 0;
}

/*
** Check if line starts with a statement keyword
*/
static int is_statement_keyword(const char *line) {
  /* Skip leading whitespace */
  while (isspace(*line)) line++;
  
  /* Check for statement keywords */
  if (strncmp(line, "function ", 9) == 0) return 1;
  if (strncmp(line, "let ", 4) == 0) return 1;
  if (strncmp(line, "if ", 3) == 0) return 1;
  if (strncmp(line, "while ", 6) == 0) return 1;
  if (strncmp(line, "for ", 4) == 0) return 1;
  if (strncmp(line, "return ", 7) == 0) return 1;
  if (strncmp(line, "break", 5) == 0) return 1;
  if (strncmp(line, "continue", 8) == 0) return 1;
  if (strncmp(line, "{", 1) == 0) return 1;  /* block statement */
  
  return 0;
}

/*
** Read a line and try to load (compile) it first as an expression (by
** adding "return " in front of it) and second as a statement.
** Similar to Lua's loadline()
*/
static int aql_loadline(aql_State *L, const char *line) {
  printf_debug("[DEBUG] aql_loadline: entering with line='%s'\n", line ? line : "NULL");
  
  int status;
  
  /* Safety checks */
  if (!L) {
    printf("[Error] Internal error: NULL state in aql_loadline\n");
    return -1;
  }
  
  /* Skip empty lines */
  if (!line || strlen(line) == 0) return 0;
  
  /* Check if this is obviously a statement */
  if (is_statement_keyword(line)) {
    printf_debug("[DEBUG] aql_loadline: detected statement keyword, trying as statement first\n");
    /* Try as statement first */
    status = aql_multiline(L, line);
    if (status == 0) {
      return 0;  /* Statement succeeded */
    }
  } else {
    /* First try: parse as expression (like Lua's addreturn) */
    status = aql_addreturn(L, line);
    if (status == 0) {
      return 0;  /* Expression succeeded */
    }
    
    /* Second try: parse as statement */
    status = aql_multiline(L, line);
    if (status == 0) {
      return 0;  /* Statement succeeded */
    }
  }
  
  /* Check if input is incomplete */
  if (aql_is_incomplete(line)) {
    return 1;  /* Needs more input */
  }
  
  return -1;  /* Parse error */
}

/*
** Print any results on the stack (similar to Lua's l_print)
*/
static void aql_print_results(aql_State *L) {
  /* For expressions, the result should be in a TValue */
  /* For now, this is handled by the expression parser */
  /* TODO: Implement proper stack-based result printing */
}

/*
** Main REPL loop (similar to Lua's doREPL)
*/
void aqlREPL_run(aql_State *L) {
  if (!L) return;
  
  /* Disable immediate error printing for cleaner REPL experience */
  aqlE_set_immediate_print(0);
  
  printf("AQL %s Interactive Mode\n", AQL_VERSION);
  printf("Type 'exit' or press Ctrl+C to quit.\n");
  printf("Expressions are evaluated automatically, statements are executed.\n\n");
  
  char line[4096];  /* Buffer for single line input */
  char multiline_buffer[8192] = {0};  /* Buffer for multi-line statements */
  bool in_multiline = false;
  
  while (1) {
    /* Show appropriate prompt */
    if (in_multiline) {
      printf("...> ");
    } else {
      printf("aql> ");
    }
    fflush(stdout);
    
    if (!fgets(line, sizeof(line), stdin)) {
      break;  /* EOF */
    }
    
    /* Remove trailing newline */
    size_t len = strlen(line);
    if (len > 0 && line[len-1] == '\n') {
      line[len-1] = '\0';
    }
    
    /* Check for exit command */
    if (!in_multiline && (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0)) {
      break;
    }
    
    /* Handle multi-line input */
    if (in_multiline) {
      /* Append to multi-line buffer */
      strcat(multiline_buffer, "\n");
      strcat(multiline_buffer, line);
      
      /* Check if multi-line input is complete */
      if (!aql_is_incomplete(multiline_buffer)) {
        /* Process complete multi-line input */
        aqlE_clear_errors(aqlE_get_global_context());
        
        int status = aql_loadline(L, multiline_buffer);
        if (status == 0) {
          aql_print_results(L);
        } else if (status > 0) {
          /* Still incomplete, continue */
          continue;
        } else {
          printf("Error: Failed to parse multi-line input\n");
        }
        
        /* Reset multi-line state */
        in_multiline = false;
        multiline_buffer[0] = '\0';
        
        /* Report any errors */
        if (aqlE_has_errors(aqlE_get_global_context())) {
          aqlE_print_error_report(aqlE_get_global_context());
        }
      }
      continue;
    }
    
    /* Skip empty lines */
    if (strlen(line) == 0) continue;
    
    /* Clear previous errors */
    aqlE_clear_errors(aqlE_get_global_context());
    
    /* Check if input is incomplete (needs multi-line) */
    if (aql_is_incomplete(line)) {
      /* Start multi-line input */
      strcpy(multiline_buffer, line);
      in_multiline = true;
      continue;
    }
    
    /* Process single-line input using Lua-style loadline */
    int status = -1;
    
    /* Wrap in error handling to prevent segfaults */
    if (L && L->ci) {
      status = aql_loadline(L, line);
    } else {
      printf("[Error] Invalid interpreter state\n");
      continue;
    }
    
    if (status == 0) {
      /* Success - print results */
      if (L && L->ci) {
        aql_print_results(L);
      }
    } else if (status == 1) {
      /* Input was incomplete, start multi-line mode */
      strcpy(multiline_buffer, line);
      in_multiline = true;
    } else {
      printf_debug("[DEBUG] Failed to parse input (status=%d)\n", status);
      /* Error already reported during compilation, no need for additional message */
    }
    
    /* Clear any accumulated errors for REPL (don't print detailed reports) */
    if (aqlE_has_errors(aqlE_get_global_context())) {
      printf_debug("[DEBUG] Clearing %d accumulated errors\n", aqlE_get_error_count(aqlE_get_global_context()));
      aqlE_clear_errors(aqlE_get_global_context());
    }
  }
  
  printf("\nGoodbye!\n");
}
