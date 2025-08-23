/*
** $Id: ado.h $
** Stack and Call structure of AQL
** See Copyright Notice in aql.h
*/

#ifndef ado_h
#define ado_h

#include <setjmp.h>

#include "aconf.h"
#include "aobject.h"
#include "astate.h"
#include "agc.h"

/* Forward declarations to avoid circular dependency */
typedef struct Zio ZIO;

/*
** Macro to check stack size and grow it if needed.
** Parameters are: State, number of stack slots needed, pre-action,
** post-action (for error handling)
*/
#define aqlD_checkstack(L,n) \
  if (L->stack_last - L->top <= (n)) \
    aqlD_growstack(L, n, 0); \
  else condmovestack(L,,);

#define aqlD_checkstackaux(L,n,pre,pos) \
  if (L->stack_last - L->top <= (n)) \
    { pre; aqlD_growstack(L, n, 1); pos; } \
  else condmovestack(L,pre,pos);

#define aqlD_reallocstack(L, newsize, raiseerror) \
  aqlM_reallocvector(L, L->stack, stacksize(L), newsize, StackValue)

#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((StkId)((char *)L->stack + (n)))

/* macro to increment stack top */
#define incr_top(L) {L->top++; aqlD_checkstack(L,0);}

/* macro decrement stack top */
#define decr_top(L) (L->top--)

/* macro to check GC when we are allocating new objects */
#define aqlD_checkgc(L,c) \
  { condchangemem(L,{c;},{}); if (G(L)->GCdebt > 0) aqlC_step(L); }

/*
** Macro to check for arithmetic overflow in addition
*/
#define aqlD_checkoverflow(L,a,op,b) \
  if (cast_uint(a) op cast_uint(b) > cast_uint(AQL_MAXINTEGER)) \
    aqlD_arith(L, AQL_OPADD, a, b, AQL_TINTEGER);

/*
** In general, to call a function on the top of the stack, the caller
** must provide the following info:
** - 'func': StkId pointing to the function to be called;
** - 'nArgs': number of arguments already pushed;
** - 'nResults': number of results expected by the caller.
** (The difference 'L->top - func' is the number of elements on the stack
** above the function, including arguments and the function itself.)
**
** When the function is not a AQL function, it is called through the
** C API using the type 'aql_CFunction'. In this case, there are no
** upvalues and the number of arguments is the number of values on the
** stack above the function.
**
** When the function is a AQL function, the protocol is more complex:
** - the function's arguments are moved to the beginning of the stack;
** - the function gets a new CallInfo structure;
** - the function's upvalues are copied to the CallInfo structure;
** - the function's code is executed.
**
** The CallInfo structure maintains the call state for AQL functions,
** including the function being called, the number of arguments, the
** number of results expected, and the state of local variables.
*/

/*
** {================================================================
** Error-recovery functions (based on long jumps)
** =================================================================
*/

/* chain list of long jump buffers */
struct aql_longjmp {
  struct aql_longjmp *previous;
  jmp_buf b;
  volatile int status;  /* error code */
};

AQL_API void aqlD_seterrorobj(aql_State *L, int errcode, StkId oldtop);
AQL_API int aqlD_protectedparser(aql_State *L, ZIO *z, const char *name,
                                const char *mode);
AQL_API void aqlD_hook(aql_State *L, int event, int line,
                       int ftransfer, int ntransfer);
AQL_API void aqlD_hookcall(aql_State *L, CallInfo *ci);
AQL_API void aqlD_hookret(aql_State *L, CallInfo *ci);
AQL_API int aqlD_pretailcall(aql_State *L, CallInfo *ci, StkId func,
                            int narg1, int delta);
AQL_API int aqlD_precall(aql_State *L, StkId func, int nResults);
AQL_API void aqlD_call(aql_State *L, StkId func, int nResults);
AQL_API void aqlD_callnoyield(aql_State *L, StkId func, int nResults);
AQL_API int aqlD_pcall(aql_State *L, aql_CFunction func, void *u,
                       ptrdiff_t oldtop, ptrdiff_t ef);
AQL_API int aqlD_poscall(aql_State *L, CallInfo *ci, int nres);
AQL_API int aqlD_growstack(aql_State *L, int n, int raiseerror);
AQL_API void aqlD_shrinkstack(aql_State *L);
AQL_API void aqlD_inctop(aql_State *L);

/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (aql_State *L, void *ud);

AQL_API l_noret aqlD_throw(aql_State *L, int errcode);
AQL_API int aqlD_rawrunprotected(aql_State *L, Pfunc f, void *ud);

/* }================================================================ */

/*
** {================================================================
** Stack reallocation
** =================================================================
*/

/*
** Compute new size for the stack ensuring minimum size 'needed'
** and not exceeding MAXSTACK
*/
static l_inline int stacksize_needed(int used, int needed) {
  int limit = (AQL_MAXSTACK - EXTRA_STACK) / 2;  /* max that can double */
  if (l_unlikely(used + needed > AQL_MAXSTACK))
    return AQL_MAXSTACK;  /* result would be larger than maximum */
  while (used < needed)
    used *= 2;
  return (used <= limit) ? used : AQL_MAXSTACK;
}

/* }================================================================ */

/*
** {================================================================
** Function calls
** =================================================================
*/

/* Pfunc type already defined earlier */

AQL_API void aqlD_call(aql_State *L, StkId func, int nresults);
AQL_API void aqlD_callnoyield(aql_State *L, StkId func, int nresults);
AQL_API int aqlD_pcall(aql_State *L, aql_CFunction func, void *u,
                       ptrdiff_t oldtop, ptrdiff_t ef);
AQL_API int aqlD_pccall(aql_State *L, aql_CFunction func, void *u,
                        ptrdiff_t oldtop, ptrdiff_t ef);
AQL_API void aqlD_callhook(aql_State *L, int event, int line);
AQL_API int aqlD_tryfuncTM(aql_State *L, StkId func);

/* }================================================================ */

/*
** {================================================================
** GC interface
** =================================================================
*/

/*
** macro to control inclusion of some hard tests on stack reallocation
*/
#if !defined(HARDSTACKTESTS)
#define condmovestack(L,pre,pos)	((void)0)
#else
/* realloc stack keeping the same size */
#define condmovestack(L,pre,pos)  \
  { int sz_ = stacksize(L); pre; aqlD_reallocstack((L), sz_, 0); pos; }
#endif

#if !defined(HARDMEMTESTS)
#define condchangemem(L,pre,pos)	((void)0)
#else
#define condchangemem(L,pre,pos)  \
	{ if (G(L)->gcrunning) { pre; aqlC_fullgc(L, 0); pos; } }
#endif

/* }================================================================ */

/*
** {================================================================
** Debug interface
** =================================================================
*/

/*
** Event codes
*/
#define AQL_HOOKCALL	0
#define AQL_HOOKRET	1
#define AQL_HOOKLINE	2
#define AQL_HOOKCOUNT	3
#define AQL_HOOKTAILCALL 4

/*
** Event masks
*/
#define AQL_MASKCALL	(1 << AQL_HOOKCALL)
#define AQL_MASKRET	(1 << AQL_HOOKRET)
#define AQL_MASKLINE	(1 << AQL_HOOKLINE)
#define AQL_MASKCOUNT	(1 << AQL_HOOKCOUNT)

/*
** {================================================================
** Coroutine support
** =================================================================
*/

#define AQL_YIELD	1
#define AQL_ERRRUN	2
#define AQL_ERRSYNTAX	3
#define AQL_ERRMEM	4
#define AQL_ERRERR	5

AQL_API int aql_resume(aql_State *L, aql_State *from, int nargs,
                       int *nresults);
AQL_API int aql_isyieldable(aql_State *L);
AQL_API int aql_yieldk(aql_State *L, int nresults, aql_KContext ctx,
                       aql_KFunction k);

#define aql_yield(L,n)		aql_yieldk(L, (n), 0, NULL)

AQL_API int aql_status(aql_State *L);

/* }================================================================ */

/*
** {================================================================
** Warning system
** =================================================================
*/

AQL_API void aql_setwarnf(aql_State *L, aql_WarnFunction f, void *ud);
AQL_API void aql_warning(aql_State *L, const char *msg, int tocont);

/* }================================================================ */

#endif /* ado_h */ 