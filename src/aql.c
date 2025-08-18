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
static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud; (void)osize;  /* not used */
    if (nsize == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, nsize);
}

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

static int tonumber(const TValue *obj, aql_Number *n) {
    if (ttisfloat(obj)) {
        *n = fltvalue(obj);
        return 1;
    }
    else if (ttisinteger(obj)) {
        *n = cast_num(ivalue(obj));
        return 1;
    }
    return 0;
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
** Convert to number
*/
AQL_API aql_Number aql_tonumberx(aql_State *L, int idx, int *pisnum) {
    aql_Number res = 0;
    const TValue *o = index2value(L, idx);
    int isnum = tonumber(o, &res);
    if (pisnum) *pisnum = isnum;
    return res;
}

/* aql_tonumber is defined as a macro in aql.h */

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

/*
** Check if value is integer
*/
AQL_API int aql_isinteger(aql_State *L, int idx) {
    const TValue *o = index2value(L, idx);
    return ttisinteger(o);
}

/*
** Check if value is number (integer or float)
*/
AQL_API int aql_isnumber(aql_State *L, int idx) {
    const TValue *o = index2value(L, idx);
    return (ttisinteger(o) || ttisfloat(o));
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

/* These functions are implemented in aobject.c */

/* Parser placeholder */
AQL_API void aqlZ_fill(aql_State *L, int n) {
    /* No-op placeholder */
}

/* VM Arithmetic implementation */
AQL_API void aql_arith(aql_State *L, int op) {
    StkId top = L->top;
    const TValue *o1, *o2;
    TValue res;
    
    /* Check if we have enough operands */
    if (op >= AQL_OPUNM) {  /* unary operations */
        if (top <= L->ci->func + 1) {
            aqlG_runerror(L, "attempt to perform arithmetic on empty stack");
            return;
        }
        o1 = s2v(top - 1);
        o2 = o1;  /* for unary operations, use same operand */
    } else {  /* binary operations */
        if (top <= L->ci->func + 2) {
            aqlG_runerror(L, "attempt to perform arithmetic with insufficient operands");
            return;
        }
        o1 = s2v(top - 2);  /* first operand */
        o2 = s2v(top - 1);  /* second operand */
    }
    
    /* Try raw arithmetic first */
    if (aqlO_rawarith(L, op, o1, o2, &res)) {
        if (op >= AQL_OPUNM) {  /* unary operation */
            setobj2s(L, top - 1, &res);
        } else {  /* binary operation */
            L->top--;  /* remove second operand */
            setobj2s(L, top - 2, &res);  /* store result in first operand position */
        }
    } else {
        /* Raw arithmetic failed, try metamethods */
        aqlT_trybinTM(L, o1, o2, L->top - 2, cast(TMS, op));
        if (op < AQL_OPUNM) {  /* binary operation */
            L->top--;  /* remove second operand */
        }
    }
}

/*
** Missing runtime functions for linking
*/

/* GC barrier functions */
AQL_API void aqlC_barrier_(aql_State *L, GCObject *o, GCObject *v) {
    UNUSED(L); UNUSED(o); UNUSED(v);
    /* GC barrier - placeholder */
}

AQL_API void aqlC_barrierback_(aql_State *L, GCObject *o) {
    UNUSED(L); UNUSED(o);
    /* GC backward barrier - placeholder */
}

/* Error handling */
AQL_API void aqlD_throw(aql_State *L, int errcode) {
    fprintf(stderr, "AQL Error: error code %d\n", errcode);
    exit(1);  /* Simplified - just exit */
}

/* Function/upvalue management */
AQL_API void aqlF_close(aql_State *L, StkId level) {
    UNUSED(L); UNUSED(level);
    /* Close upvalues - placeholder */
}

AQL_API UpVal *aqlF_newtbcupval(aql_State *L, StkId level) {
    UNUSED(L); UNUSED(level);
    /* Create new to-be-closed upvalue - placeholder */
    return NULL;
}

/* Memory management */
AQL_API void *aqlM_realloc(aql_State *L, void *block, size_t osize, size_t nsize) {
    UNUSED(L); UNUSED(osize);
    if (nsize == 0) {
        free(block);
        return NULL;
    }
    return realloc(block, nsize);
} 