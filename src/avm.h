/*
** $Id: avm.h $
** AQL virtual machine
** See Copyright Notice in aql.h
*/

#ifndef avm_h
#define avm_h

#include <float.h>

#include "aconf.h"
#include "aobject.h"
#include "astate.h"

/* limit for table tag-method chains (to avoid infinite loops) */
#define MAXTAGLOOP	2000

/*
** 'l_intfitsf' checks whether a given integer is in the range that
** can be converted to a float without rounding. Used in comparisons.
*/

#if !defined(l_intfitsf)

/* number of bits in the mantissa of a float */
#if defined(AQL_NUMBER) && defined(aql_Number)
  /* Use the configured float type for AQL */
  #if sizeof(aql_Number) == sizeof(float)
    #define l_floatatt(n)   (FLT_##n)
  #elif sizeof(aql_Number) == sizeof(long double)
    #define l_floatatt(n)   (LDBL_##n)
  #else
    #define l_floatatt(n)   (DBL_##n)
  #endif
#else
  /* Default to double precision */
  #define l_floatatt(n)   (DBL_##n)
#endif
#define NBITS           (l_floatatt(MANT_DIG))

/* number of bits in the mantissa of a float */
#define NANTBITS        (NBITS - 1)

/* check whether some integers may not fit in a float, so that we can
   use the conversion (aql_Number)i == i  only for "safe" integers */
#if (AQL_MAXINTEGER >> NANTBITS) > 0

/* maximum integer that fits in a float */
#define MAXINTFITSF	((aql_Unsigned)1 << NANTBITS)

/* check whether 'i' is in the range [-MAXINTFITSF, MAXINTFITSF] */
#define l_intfitsf(i)  \
  ((aql_Unsigned)((i) + MAXINTFITSF) <= (2 * MAXINTFITSF))

#else  /* all integers fit in a float precisely */

#define l_intfitsf(i)	1

#endif

#endif				/* l_intfitsf */

/*
** Try to convert a value from string to a number value.
** If string is not a valid numeral, returns 0 and does not change 'n'.
** Otherwise, returns 1 and sets 'n' to the numerical value.
*/
#if !defined(aql_strx2number)
#define aql_strx2number(s,p,n)  aql_str2number(s,n)
#endif

/*
** Rounding modes for float->integer coercion
*/
typedef enum {
  F2Ieq,     /* no rounding; accepts only integral values */
  F2Ifloor,  /* takes the floor of the number */
  F2Iceil    /* takes the ceil of the number */
} F2Imod;

/* Default rounding mode for AQL */
#define AQL_FLOORN2INT  F2Ieq

/* convert a float to an integer, with specified rounding mode */
#if !defined(aql_floorn2int)
#define aql_floorn2int(n,F2I) \
  ((n) - (aql_Number)(F2I) + (aql_Number)(F2I))
#endif

#define tonumber(o,n) \
  (ttisfloat(o) ? (*(n) = fltvalue(o), 1) : aqlV_tonumber_(o,n))

#define tonumberns(o,n) \
  (ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
                  (ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))

#define tointeger(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                            : aqlV_tointeger(o,i,AQL_FLOORN2INT))

/* Note: tointegerns and intop macros are defined in aconf.h */

/*
** Precise number comparison macros - AQL style
*/
#if !defined(aql_numlt)
#define aql_numlt(a,b)		((a) < (b))
#endif

#if !defined(aql_numle)
#define aql_numle(a,b)		((a) <= (b))
#endif

#if !defined(aql_numeq)
#define aql_numeq(a,b)		((a) == (b))
#endif



/*
** Floor function for number conversion
*/
#if !defined(l_floor)
#include <math.h>
#define l_floor(x)		(floor(x))
#endif

#define aqlV_rawequalobj(t1,t2)		aqlV_equalobj(NULL,t1,t2)

/*
** fast track for 'gettable': if 't' is a table and 't[k]' is present,
** return 1 with 'slot' pointing to 't[k]' (position of final result).
** Otherwise, return 0 (meaning it will have to try metamethod)
** with 'slot' pointing to an empty 't[k]' (if 't' is a table) or NULL
** (otherwise). 'f' is the raw get function to use.
*/
#define aqlV_fastget(L,t,k,slot,f) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = f(hvalue(t), k),  /* else, do raw access */  \
      !isempty(slot)))  /* result not empty? */

/*
** Special case of 'aqlV_fastget' for integers, inlining the fast case
** of 'aqlH_getint'.
*/
#define aqlV_fastgeti(L,t,k,slot) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = (l_castS2U(k) - 1u < hvalue(t)->alimit) \
              ? &hvalue(t)->array[k - 1] \
              : aqlH_getint(hvalue(t), k), \
      !isempty(slot)))  /* result not empty? */

/*
** Finish a fast set operation (when fast get succeeds). In that case,
** 'slot' points to the place to put the value.
*/
#define aqlV_finishfastset(L,t,slot,v) \
    { setobj2t(L, cast(TValue *,slot), v); \
      aqlC_barrierback(L, gcvalue(t), v); }

/*
** AQL Container fast access macros
*/
#define aqlV_fastgetarray(L,arr,k,slot) \
  (((k) >= 0 && (size_t)(k) < gco2array(gcvalue(arr))->length) \
   ? (slot = &gco2array(gcvalue(arr))->data[k], !isempty(slot)) \
   : (slot = NULL, 0))

#define aqlV_fastgetslice(L,slice,k,slot) \
  (((k) >= 0 && (size_t)(k) < gco2slice(gcvalue(slice))->length) \
   ? (slot = &gco2slice(gcvalue(slice))->data[k], !isempty(slot)) \
   : (slot = NULL, 0))

#define aqlV_fastsetarray(L,arr,k,v) \
  { if ((k) >= 0 && (size_t)(k) < gco2array(gcvalue(arr))->length) \
      setobj2t(L, &gco2array(gcvalue(arr))->data[k], v); }

#define aqlV_fastsetslice(L,slice,k,v) \
  { if ((k) >= 0 && (size_t)(k) < gco2slice(gcvalue(slice))->length) \
      setobj2t(L, &gco2slice(gcvalue(slice))->data[k], v); }

AQL_API int aqlV_equalobj(aql_State *L, const TValue *t1, const TValue *t2);
AQL_API int aqlV_lessthan(aql_State *L, const TValue *l, const TValue *r);
AQL_API int aqlV_lessequal(aql_State *L, const TValue *l, const TValue *r);
AQL_API int aqlV_tonumber_(const TValue *obj, aql_Number *n);
AQL_API int aqlV_tointeger(const TValue *obj, aql_Integer *p, F2Imod mode);
AQL_API int aqlV_tointegerns(const TValue *obj, aql_Integer *p);
AQL_API int aqlV_flttointeger(aql_Number n, aql_Integer *p, F2Imod mode);
AQL_API void aqlV_finishget(aql_State *L, const TValue *t, TValue *key,
                           StkId val, const TValue *slot);
AQL_API void aqlV_finishset(aql_State *L, const TValue *t, TValue *key,
                           TValue *val, const TValue *slot);

/*
** Legacy Lua table access removed in AQL container separation architecture.
** AQL uses type-specific container access instead of unified table access.
*/

/*
** AQL Container operations - Type-safe container access
*/
#define aqlV_getarray(L,arr,k,v) { const TValue *slot; \
  if (aqlV_fastgetarray(L,arr,k,slot)) { setobj2s(L, v, slot); } \
  else { setnilvalue(s2v(v)); }}

#define aqlV_getslice(L,slice,k,v) { const TValue *slot; \
  if (aqlV_fastgetslice(L,slice,k,slot)) { setobj2s(L, v, slot); } \
  else { setnilvalue(s2v(v)); }}

#define aqlV_getdict(L,dict,k,v) { \
  const TValue *res = aqlD_get(dictvalue(dict), k); \
  if (res != NULL) { setobj2s(L, v, res); } \
  else { setnilvalue(s2v(v)); }}

#define aqlV_getvector(L,vec,k,v) { \
  aql_Integer idx; \
  if (tointeger(k, &idx) && idx >= 0 && (size_t)idx < vecvalue(vec)->length) { \
    /* Get vector element based on data type */ \
    Vector *vector = vecvalue(vec); \
    switch (vector->dtype) { \
      case DT_INT: setivalue(s2v(v), ((aql_Integer*)vector->data)[idx]); break; \
      case DT_FLOAT: setfltvalue(s2v(v), ((aql_Number*)vector->data)[idx]); break; \
      default: setnilvalue(s2v(v)); break; \
    } \
  } else { setnilvalue(s2v(v)); }}

#define aqlV_setarray(L,arr,k,v) \
  { aqlV_fastsetarray(L,arr,k,v); \
    aqlC_barrierback(L, gcvalue(arr), v); }

#define aqlV_setslice(L,slice,k,v) \
  { aqlV_fastsetslice(L,slice,k,v); \
    aqlC_barrierback(L, gcvalue(slice), v); }

#define aqlV_setdict(L,dict,k,v) { \
  aqlD_set(L, dictvalue(dict), k, v); \
  aqlC_barrierback(L, gcvalue(dict), v); }

#define aqlV_setvector(L,vec,k,v) { \
  aql_Integer idx; \
  if (tointeger(k, &idx) && idx >= 0 && (size_t)idx < vecvalue(vec)->length) { \
    Vector *vector = vecvalue(vec); \
    switch (vector->dtype) { \
      case DT_INT: \
        if (ttisinteger(v)) ((aql_Integer*)vector->data)[idx] = ivalue(v); \
        break; \
      case DT_FLOAT: \
        if (ttisfloat(v)) ((aql_Number*)vector->data)[idx] = fltvalue(v); \
        break; \
      default: \
        /* Other data types not yet supported */ \
        break; \
    } \
    aqlC_barrierback(L, gcvalue(vec), v); \
  }}

/*
** Execute a protected call.
*/
AQL_API int aqlV_execute(aql_State *L, CallInfo *ci);
AQL_API void aqlV_finishOp(aql_State *L);
AQL_API void aqlV_concat(aql_State *L, int total);
AQL_API aql_Integer aqlV_idiv(aql_State *L, aql_Integer m, aql_Integer n);
AQL_API aql_Integer aqlV_mod(aql_State *L, aql_Integer m, aql_Integer n);
AQL_API aql_Number aqlV_modf(aql_State *L, aql_Number m, aql_Number n);
AQL_API aql_Integer aqlV_shiftl(aql_Integer x, aql_Integer y);

/*
** {================================================================
** Macros for arithmetic/bitwise/comparison opcodes in 'aqlV_execute'
** =================================================================
*/

#define l_addi(L,a,b)	intop(+, a, b)
#define l_subi(L,a,b)	intop(-, a, b)
#define l_muli(L,a,b)	intop(*, a, b)
#define l_band(a,b)	intop(&, a, b)
#define l_bor(a,b)	intop(|, a, b)
#define l_bxor(a,b)	intop(^, a, b)
#define l_lshift(a,b)	aqlV_shiftl(a, b)
#define l_rshift(a,b)	aqlV_shiftl(a, -(b))

/* }================================================================ */

/* Forward declarations already defined in aobject.h */

/*
** {==================================================================
** Function Prototypes
** ===================================================================
*/
AQL_API Proto *aqlF_newproto (aql_State *L);
AQL_API CClosure *aqlF_newCclosure (aql_State *L, int nelems);
AQL_API LClosure *aqlF_newLclosure (aql_State *L, int nelems);
AQL_API void aqlF_initupvals (aql_State *L, LClosure *cl);
AQL_API UpVal *aqlF_findupval (aql_State *L, StkId level);
AQL_API void aqlF_newtbcupval (aql_State *L, StkId level);
AQL_API void aqlF_close (aql_State *L, StkId level, int status);
AQL_API void aqlF_unlinkupval (UpVal *uv);
AQL_API void aqlF_freeproto (aql_State *L, Proto *f);
AQL_API const char *aqlF_getlocalname (const Proto *func, int local_number,
                                      int pc);

/* }================================================================== */

/*
** {==================================================================
** AQL Closures
** ===================================================================
*/

/* Note: ClosureHeader, CClosure, LClosure, and Closure are defined in aobject.h */

#define isLfunction(o)	ttisLclosure(o)

#define getproto(o)	(clLvalue(o)->p)

/* }================================================================== */

/*
** {==================================================================
** Tables
** ===================================================================
*/

typedef struct Node {
  TValue i_val;
  union {
    struct Node *next;  /* for chaining */
    aql_Unsigned hnext;  /* index for next node */
  } u;
  TValue i_key;
} Node;

/* copy a value into a key */
#define setnodekey(L,node,obj) \
	{ Node *n_=(node); const TValue *io_=(obj); \
	  n_->i_key.value_ = io_->value_; n_->i_key.tt_ = io_->tt_; \
	  (void)L; checkliveness(L,io_); }

typedef struct Table {
  CommonHeader;
  aql_byte flags;  /* 1<<p means tagmethod(p) is not present */
  aql_byte lsizenode;  /* log2 of size of 'node' array */
  unsigned int alimit;  /* "limit" of 'array' array */
  TValue *array;  /* array part */
  Node *node;
  Node *lastfree;  /* any free position is before this position */
  struct Table *metatable;
  GCObject *gclist;
/* Table already defined in aobject.h */
}Table;

/*
** Macros to manipulate keys inserted in nodes
*/
#define keytt(node)		((node)->i_key.tt_)
#define keyval(node)		((node)->i_key.value_)

#define keyisnil(node)		(keytt(node) == AQL_TNIL)
#define keyisinteger(node)	(keytt(node) == AQL_VNUMINT)
#define keyival(node)		(keyval(node).i)
#define keyisshrstr(node)	(keytt(node) == AQL_VSHRSTR)
#define keystrval(node)		(gco2ts(keyval(node).gc))

#define setnilkey(node)		(keytt(node) = AQL_TNIL)

#define keyiscollectable(node)	(keytt(node) & BIT_ISCOLLECTABLE)

#define gckey(n)	(keyval(n).gc)
#define gckeyN(n)	(keyiscollectable(n) ? gckey(n) : NULL)

/* }================================================================== */

#endif /* avm_h */ 