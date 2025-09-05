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
** Try to compile line as 'return <line>;' and execute it.
** This is only used for REPL single-line input to show expression results.
** Similar to Lua's addreturn() + docall()
*/
static int aql_addreturn(aql_State *L, const char *line) {
  /* Follow Lua's approach: always try adding 'return' first */
  size_t line_len = strlen(line);
  size_t ret_len = line_len + 10;  /* "return " + line + ";" + null terminator */
  char *retline = malloc(ret_len);
  if (!retline) return -1;
  
  snprintf(retline, ret_len, "return %s;", line);
  //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: trying with return: '%s'", retline);
  //fflush(stdout);
  
  /* Try to compile as return statement */
  int status = aqlP_compile_string(L, retline, strlen(retline), "=stdin");
  //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: compile returned status=%d", status);
  //fflush(stdout);
  
  if (status == 0) {
    /* Compilation successful - execute the compiled code */
    //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: compilation successful, executing");
    //fflush(stdout);
    status = aqlP_execute_compiled(L, 0, 1);  /* 0 args, 1 result expected */
    //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: execute returned status=%d", status);
    //fflush(stdout);
    
    if (status == 1) {  /* aqlV_execute returns 1 for successful completion */
      /* Print the result from stack */
      //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: checking for result on stack, L->top=%p, L->ci->func=%p", 
               //(void*)L->top, (void*)L->ci->func);
      //fflush(stdout);
      if (L->top > L->ci->func + 1) {  /* Has result? */
        TValue *result = s2v(L->top - 1);
        //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: found result on stack, printing");
        //fflush(stdout);
        aqlP_print_value(result);
        printf("\n");
      } else {
        //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: no result on stack");
        //fflush(stdout);
      }
      
      /* Clean up stack */
      L->top = L->ci->func + 1;
      //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: cleaned up stack, L->top reset");
      //fflush(stdout);
      
      free(retline);
      return 0;  /* Success */
    } else {
      //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: execution failed with status=%d", status);
      //fflush(stdout);
    }
    
    /* Clean up stack for failed execution */
    L->top = L->ci->func + 1;
    //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: cleaned up stack after execution failure");
    //fflush(stdout);
  } else {
    //AQL_DEBUG(AQL_DEBUG_REPL, "aql_addreturn: compilation failed, will try as statement");
    //fflush(stdout);
  }
  
  free(retline);
  return status;  /* Return compilation/execution status */
}

/*
** Read multiple lines until a complete AQL statement
** Similar to Lua's multiline()
*/
static int aql_multiline(aql_State *L, const char *line) {
  /* Try to compile and execute as statement (without 'return') */
  AQL_DEBUG(AQL_DEBUG_REPL, "aql_multiline: trying to compile as statement: '%s'", line);
  fflush(stdout);
  
  int status = aqlP_compile_string(L, line, strlen(line), "=stdin");
  AQL_DEBUG(AQL_DEBUG_REPL, "aql_multiline: compile returned status=%d", status);
  fflush(stdout);
  
  if (status == 0) {
    /* Compilation successful - execute the compiled code */
    AQL_DEBUG(AQL_DEBUG_REPL, "aql_multiline: compilation successful, executing");
    fflush(stdout);
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
    AQL_DEBUG(AQL_DEBUG_REPL, "aql_multiline: compilation failed");
    fflush(stdout);
  }
  
  return status;  /* Return compilation/execution status */
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
** Read a line and try to load (compile) it first as an expression (by
** adding "return " in front of it) and second as a statement.
** Similar to Lua's loadline()
*/
static int aql_loadline(aql_State *L, const char *line) {
  int status;
  
  /* Skip empty lines */
  if (!line || strlen(line) == 0) return 0;
  
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
    int status = aql_loadline(L, line);
    
    if (status == 0) {
      /* Success - print results */
      aql_print_results(L);
    } else if (status == 1) {
      /* Input was incomplete, start multi-line mode */
      strcpy(multiline_buffer, line);
      in_multiline = true;
    } else {
      printf("Error: Failed to parse input\n");
    }
    
    /* Report any accumulated errors */
    if (aqlE_has_errors(aqlE_get_global_context())) {
      aqlE_print_error_report(aqlE_get_global_context());
    }
  }
  
  printf("\nGoodbye!\n");
}
