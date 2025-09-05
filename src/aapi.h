/*
** $Id: aapi.h $
** AQL API
** See Copyright Notice in aql.h
*/

#ifndef aapi_h
#define aapi_h

#include "aql.h"

/* Compilation functions */
AQL_API int aqlP_compile_string(aql_State *L, const char *code, size_t len, const char *name);

/* Execution functions */
AQL_API int aqlP_execute_compiled(aql_State *L, int nargs, int nresults);

/* High-level API functions */
AQL_API int aql_loadstring(aql_State *L, const char *source);
AQL_API int aql_loadfile(aql_State *L, const char *filename);
AQL_API int aql_loadfile_with_return(aql_State *L, const char *filename);
AQL_API int aql_execute(aql_State *L, int nargs, int nresults);

#endif