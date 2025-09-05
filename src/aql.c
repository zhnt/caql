/*
** $Id: aql.c $
** Core AQL API Implementation
** See Copyright Notice in aql.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <time.h>

#include "aql.h"
#include "astate.h"
#include "aobject.h"
#include "amem.h"

/*
** Simple allocator used by AQL internally
*/
__attribute__((unused))
static void *aql_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud; (void)osize;  /* not used */
    if (nsize == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, nsize);
}

/*
** Forward declarations
*/
#include "aapi.h"

/*
** Helper functions for type checking and conversion
*/
static int tointeger(const TValue *obj, aql_Integer *p) {
    if (ttisinteger(obj)) {
        *p = ivalue(obj);
        return 1;
    }
    else if (ttisfloat(obj)) {
        aql_Number n = fltvalue(obj);
        *p = (aql_Integer)n;
        return (n == (aql_Number)*p);  /* check if conversion is exact */
    }
    return 0;
}

static int ttypenv(const TValue *o) {
    return ttype(o);
}

/*
** Helper function - convert stack index to TValue
*/
static const TValue *index2value(aql_State *L, int idx) {
    CallInfo *ci = L->ci;
    if (idx > 0) {
        StkId o = ci->func + idx;
        if (o >= L->top) return &G(L)->nilvalue;
        else return s2v(o);
    }
    else if (idx < 0) {  /* negative index */
        if (L->top + idx <= ci->func) return &G(L)->nilvalue;
        return s2v(L->top + idx);
    }
    else if (idx == AQL_REGISTRYINDEX)
        return &G(L)->l_registry;
    else return &G(L)->nilvalue;
}

/*
** Get top of stack
*/
AQL_API int aql_gettop(aql_State *L) {
    return cast_int(L->top - (L->ci->func + 1));
}

/*
** Set top of stack
*/
AQL_API void aql_settop(aql_State *L, int idx) {
    StkId func = L->ci->func;
    if (idx >= 0) {
        while (L->top < (func + 1) + idx)
            setnilvalue(s2v(L->top++));
        L->top = (func + 1) + idx;
    }
    else {
        L->top += idx+1;  /* `subtract' index (index is negative) */
    }
}

/*
** Push nil value
*/
AQL_API void aql_pushnil(aql_State *L) {
    setnilvalue(s2v(L->top));
    L->top++;
}

/*
** Push boolean value
*/
AQL_API void aql_pushboolean(aql_State *L, int b) {
    setbvalue(s2v(L->top), (b != 0));
    L->top++;
}

/*
** Push integer value
*/
AQL_API void aql_pushinteger(aql_State *L, aql_Integer n) {
    setivalue(s2v(L->top), n);
    L->top++;
}

/*
** Push number value
*/
AQL_API void aql_pushnumber(aql_State *L, aql_Number n) {
    setfltvalue(s2v(L->top), n);
    L->top++;
}

/*
** Push value from another stack position
*/
AQL_API void aql_pushvalue(aql_State *L, int idx) {
    setobj2s(L, L->top, index2value(L, idx));
    L->top++;
}

/*
** Convert to integer with optional success flag
*/
AQL_API aql_Integer aql_tointegerx(aql_State *L, int idx, int *pisnum) {
    aql_Integer res = 0;
    const TValue *o = index2value(L, idx);
    int isnum = tointeger(o, &res);
    if (pisnum) *pisnum = isnum;
    return res;
}

/*
** Convert to userdata
*/
AQL_API void *aql_touserdata(aql_State *L, int idx) {
    const TValue *o = index2value(L, idx);
    switch (ttypenv(o)) {
        case AQL_TUSERDATA: return getudatamem(uvalue(o));
        case AQL_TLIGHTUSERDATA: return pvalue(o);
        default: return NULL;
    }
}

/*
** Resume coroutine (placeholder)
*/
AQL_API int aql_resume(aql_State *L, aql_State *from, int nargs, int *nresults) {
    (void)from; (void)nargs; (void)nresults;
    fprintf(stderr, "AQL Runtime Error: coroutines not implemented in MVP\n");
    return AQL_ERRRUN;
}

/* aql_pop is defined as a macro in aql.h */

/*
** Type of value at given index
*/
AQL_API int aql_type(aql_State *L, int idx) {
    const TValue *o = index2value(L, idx);
    return (o != &G(L)->nilvalue ? ttypenv(o) : AQL_TNONE);
}

/* aql_tostring is defined as a macro in aql.h */

/*
** Check if value is boolean and get its value
*/
AQL_API int aql_toboolean(aql_State *L, int idx) {
    const TValue *o = index2value(L, idx);
    return !l_isfalse(o);
}

/*
** Push string (placeholder)
*/
AQL_API const char *aql_pushstring(aql_State *L, const char *s) {
    if (s == NULL) {
        aql_pushnil(L);
        return NULL;
    }
    /* For MVP, we'll just push nil and return the string */
    aql_pushnil(L);
    return s;
}

/* l_isfalse is defined as a macro in aobject.h */

/* All object utility functions are implemented in aobject.c to avoid duplicates */

/* String creation placeholders - implemented in aobject.c */

/* Parser placeholder - removed, now implemented in azio.c */

/* VM Arithmetic implementation */
AQL_API void aql_arith(aql_State *L, int op) {
    StkId o2 = L->top - 1;  /* second operand */
    StkId o1 = L->top - 2;  /* first operand */
    StkId res = o1;         /* result goes to first operand */
    
    /* Check if we have enough operands on stack */
    if (o1 < L->stack) {
        fprintf(stderr, "AQL Error: not enough operands on stack for arithmetic\n");
        return;
    }
    
    /* Perform the arithmetic operation using AQL's object system */
    aqlO_arith(L, op, s2v(o1), s2v(o2), res);
    
    /* Pop the second operand (result is now in first operand position) */
    L->top = o2;
}

/*
** Missing AQL API functions
*/
AQL_API int aql_isinteger(aql_State *L, int idx) {
    /* Basic type checking for integers */
    if (!L || !L->top) return 0;
    
    StkId stk = L->top + idx;
    if (stk < L->stack || stk >= L->top) return 0;
    
    TValue *o = s2v(stk);
    /* Check if it's an integer type */
    return ttisinteger(o);
}

AQL_API int aql_isnumber(aql_State *L, int idx) {
    /* Basic type checking for numbers */
    if (!L || !L->top) return 0;
    
    StkId stk = L->top + idx;
    if (stk < L->stack || stk >= L->top) return 0;
    
    TValue *o = s2v(stk);
    /* Check if it's a number type (integer or float) */
    return ttisnumber(o);
}

AQL_API aql_Number aql_tonumberx(aql_State *L, int idx, int *pisnum) {
    /* Convert to number with success flag */
    if (!L || !L->top) {
        if (pisnum) *pisnum = 0;
        return 0;
    }
    
    StkId stk = L->top + idx;
    if (stk < L->stack || stk >= L->top) {
        if (pisnum) *pisnum = 0;
        return 0;
    }
    
    TValue *o = s2v(stk);
    if (ttisinteger(o)) {
        if (pisnum) *pisnum = 1;
        return cast_num(ivalue(o));
    } else if (ttisfloat(o)) {
        if (pisnum) *pisnum = 1;
        return fltvalue(o);
    } else {
        if (pisnum) *pisnum = 0;
        return 0;
    }
}

/*
** API Functions for compilation and execution (similar to Lua's lua_load/lua_pcall)
*/

/* API functions moved to aapi.c */

/* Exception handling functions moved to ado.c */

/*
** Function manipulation - moved to afunc.c
*/ 