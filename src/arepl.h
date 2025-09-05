/*
** $Id: arepl.h $
** AQL REPL (Read-Eval-Print Loop) interface
** See Copyright Notice in aql.h
*/

#ifndef arepl_h
#define arepl_h

#include "aql.h"

/*
** Main REPL function - runs the interactive loop
*/
AQL_API void aqlREPL_run(aql_State *L);

#endif

