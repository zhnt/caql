/*
** $Id: afunc.h $
** Auxiliary functions to manipulate prototypes and closures
** AQL - Array Query Language
*/

#ifndef afunc_h
#define afunc_h

#include "aobject.h"

#define sizeCclosure(n)	(cast_int(offsetof(CClosure, upvalue)) + \
                         cast_int(sizeof(TValue)) * (n))

#define sizeLclosure(n)	(cast_int(offsetof(LClosure, upvals)) + \
                         cast_int(sizeof(TValue *)) * (n))

/* test whether thread is in 'twups' list */
#define isintwups(L)	(L->twups != L)

/*
** maximum number of upvalues in a closure (both C and AQL). (Value
** must fit in a VM register.)
*/
#define MAXUPVAL	255

#define upisopen(up)	((up)->v.p != &(up)->u.value)

#define uplevel(up)	check_exp(upisopen(up), cast(StkId, (up)->v.p))

/*
** maximum number of misses before giving up the cache of closures
** in prototypes
*/
#define MAXMISS		10

/* special status to close upvalues preserving the top of the stack */
#define CLOSEKTOP	(-1)

AQL_API Proto *aqlF_newproto (aql_State *L);
AQL_API CClosure *aqlF_newCclosure (aql_State *L, int nupvals);
AQL_API LClosure *aqlF_newLclosure (aql_State *L, int nupvals);
AQL_API void aqlF_initupvals (aql_State *L, LClosure *cl);
AQL_API UpVal *aqlF_findupval (aql_State *L, StkId level);
AQL_API void aqlF_newtbcupval (aql_State *L, StkId level);
AQL_API void aqlF_closeupval (aql_State *L, StkId level);
AQL_API StkId aqlF_close (aql_State *L, StkId level, int status, int yy);
AQL_API void aqlF_unlinkupval (UpVal *uv);
AQL_API void aqlF_freeproto (aql_State *L, Proto *f);
AQL_API const char *aqlF_getlocalname (const Proto *func, int local_number,
                                       int pc);

#endif