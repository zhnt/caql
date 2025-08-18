/*
** $Id: aapi.h $
** Auxiliary functions from AQL API  
** See Copyright Notice in aql.h
*/

#ifndef aapi_h
#define aapi_h

#include "astate.h"

/* Increments 'L->top', checking for stack overflows */
#define api_incr_top(L)   {L->top++; \
                          api_check(L, L->top <= L->ci->top, \
                                  "stack overflow");}

/*
** If a call returns too many multiple returns, the callee may not have
** stack space to accommodate all results. In this case, this macro
** increases its stack space ('L->ci->top').
*/
#define adjustresults(L,nres) \
    { if ((nres) <= AQL_MULTRET && L->ci->top < L->top) \
        L->ci->top = L->top; }

/* Ensure the stack has at least 'n' elements */
#define api_checknelems(L,n) \
    api_check(L, (n) < (L->top - L->ci->func), \
              "not enough elements in the stack")

/*
** To reduce the overhead of returning from C functions, the presence of
** to-be-closed variables in these functions is coded in the CallInfo's
** field 'nresults', in a way that functions with no to-be-closed variables
** with zero, one, or "all" wanted results have no overhead.
*/

#define hastocloseCfunc(n)    ((n) < AQL_MULTRET)

/* Map [-1, inf) (range of 'nresults') into (-inf, -2] */
#define codeNresults(n)       (-(n) - 3)
#define decodeNresults(n)     (-(n) - 3)

/* basic API check */
#if defined(AQL_USE_APICHECK)
#include <assert.h>
#define api_check(l,e,msg)  assert(e)
#else
#define api_check(l,e,msg)  ((void)0)
#endif

#endif /* aapi_h */ 