/*
** $Id: aapi.c $
** AQL API
** See Copyright Notice in aql.h
*/

#define aapi_c
#define AQL_CORE

#include "aconf.h"

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#include "aql.h"

#include "acode.h"
#include "adebug_internal.h"
#include "ado.h"
#include "afunc.h"
#include "alex.h"
#include "amem.h"
#include "aobject.h"
#include "aopcodes.h"
#include "aparser.h"
#include "adict.h"
#include "astring.h"
#include "acontainer.h"
#include "aerror.h"
#include "astate.h"

/* Forward declaration for VM execution */
AQL_API int aqlV_execute(aql_State *L, CallInfo *ci);

/* Forward declaration for global dict access */
extern Dict *get_globals_dict(aql_State *L);

/* Protected execution functions from ado.c */
AQL_API int aqlD_protectedcompile(aql_State *L, const char *code, size_t len, const char *name);
AQL_API int aqlD_protectedexecute(aql_State *L, int nargs, int nresults);

/*
** Compile a string into bytecode (similar to luaL_loadbuffer)
** This is the core compilation function that AQL REPL needs
*/
AQL_API int aqlP_compile_string(aql_State *L, const char *code, size_t len, const char *name) {
  if (!code || !L) return -1;
  
  
  /* Use protected compilation from ado.c */
  int status = aqlD_protectedcompile(L, code, len, name);
  
  if (status == AQL_OK) {
    return 0;  /* Success */
  } else {
    return -1;  /* Compilation failed */
  }
}

/*
** Execute compiled bytecode (similar to lua_pcall)
** Assumes compiled function is on top of stack
*/
AQL_API int aqlP_execute_compiled(aql_State *L, int nargs, int nresults) {
  
  /* Use protected execution from ado.c */
  int status = aqlD_protectedexecute(L, nargs, nresults);
  
  if (status == AQL_OK) {
    return 1;  /* Success - return 1 for compatibility with existing code */
  } else {
    return -1;  /* Execution failed */
  }
}

/*
** Load a string and compile it (similar to luaL_loadstring)
*/
AQL_API int aql_loadstring(aql_State *L, const char *source) {
  int status = aqlP_compile_string(L, source, strlen(source), "=loadstring");
  if (status != 0) {
    return status;
  }
  return 0;
}

/*
** Load a file and compile it with automatic return for last expression
*/
AQL_API int aql_loadfile_with_return(aql_State *L, const char *filename) {
  if (!filename || !L) {
    return -1;
  }
  
  
  /* Open file */
  FILE *f = fopen(filename, "r");
  if (!f) {
    return -1;
  }
  
  /* Get file size */
  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  fseek(f, 0, SEEK_SET);
  
  if (file_size < 0) {
    fclose(f);
    return -1;
  }
  
  /* Allocate buffer */
  char *buffer = (char*)malloc(file_size + 1);
  if (!buffer) {
    fclose(f);
    return -1;
  }
  
  /* Read file content */
  size_t bytes_read = fread(buffer, 1, file_size, f);
  buffer[bytes_read] = '\0';
  fclose(f);
  
  
  /* Find the last non-empty line and check if it's an expression */
  char *last_line_start = NULL;
  char *p = buffer + bytes_read - 1;
  
  
  /* Skip trailing whitespace */
  while (p >= buffer && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
    p--;
  }
  
  if (p >= buffer) {
    /* Find start of last line */
    while (p >= buffer && *p != '\n' && *p != '\r') {
      p--;
    }
    last_line_start = p + 1;
    
    /* Check if last line looks like an expression (not a statement) */
    char *line_copy = strdup(last_line_start);
    if (line_copy) {
      /* Remove trailing whitespace from line copy */
      char *end = line_copy + strlen(line_copy) - 1;
      while (end >= line_copy && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
      }
      
      /* Check if it's not a statement keyword */
      int is_stmt = (strncmp(line_copy, "let ", 4) == 0 ||
                     strncmp(line_copy, "if ", 3) == 0 ||
                     strncmp(line_copy, "while ", 6) == 0 ||
                     strncmp(line_copy, "return ", 7) == 0 ||
                     strstr(line_copy, ":=") != NULL);
      
      if (!is_stmt && strlen(line_copy) > 0) {
        /* Last line is an expression, modify buffer to add return */
        size_t line_start_offset = last_line_start - buffer;
        size_t line_len = strlen(line_copy);
        size_t new_size = bytes_read + 7; /* "return " = 7 chars */
        
        
        char *new_buffer = (char*)malloc(new_size + 1);
        if (new_buffer) {
          /* Copy everything before last line */
          memcpy(new_buffer, buffer, line_start_offset);
          /* Add "return " */
          memcpy(new_buffer + line_start_offset, "return ", 7);
          /* Copy the last line */
          memcpy(new_buffer + line_start_offset + 7, line_copy, line_len);
          new_buffer[line_start_offset + 7 + line_len] = '\0';
          
          free(buffer);
          buffer = new_buffer;
          bytes_read = line_start_offset + 7 + line_len;
          
        }
      } else {
      }
      free(line_copy);
    }
  }
  
  /* Compile the (possibly modified) file content */
  int status = aqlP_compile_string(L, buffer, bytes_read, filename);
  
  /* Clean up */
  free(buffer);
  
  if (status == 0) {
    return 0;
  } else {
    return status;
  }
}

/*
** Load a file and compile it (similar to luaL_loadfile)
*/
AQL_API int aql_loadfile(aql_State *L, const char *filename) {
  if (!filename || !L) {
    return -1;
  }
  
  
  /* Open file */
  FILE *f = fopen(filename, "r");
  if (!f) {
    return -1;
  }
  
  /* Get file size */
  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  fseek(f, 0, SEEK_SET);
  
  if (file_size < 0) {
    fclose(f);
    return -1;
  }
  
  /* Allocate buffer */
  char *buffer = (char*)malloc(file_size + 1);
  if (!buffer) {
    fclose(f);
    return -1;
  }
  
  /* Read file content */
  size_t bytes_read = fread(buffer, 1, file_size, f);
  buffer[bytes_read] = '\0';
  fclose(f);
  
  
  /* Compile the file content */
  int status = aqlP_compile_string(L, buffer, bytes_read, filename);
  
  /* Clean up */
  free(buffer);
  
  if (status == 0) {
    return 0;
  } else {
    return status;
  }
}

/*
** Execute a compiled function (similar to lua_pcall)
*/
AQL_API int aql_execute(aql_State *L, int nargs, int nresults) {
  int status = aqlP_execute_compiled(L, nargs, nresults);
  if (status == 1) {
    /* Status 1 means successful execution in VM */
    return 0;
  } else {
    return status;
  }
}
