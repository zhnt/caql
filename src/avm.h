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
AQL_API StkId aqlF_close (aql_State *L, StkId level, int status, int yy);
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
** Tables - Now handled by atable.h
** ===================================================================
*/

/* Table definitions moved to atable.h for better organization */

/* }================================================================== */

/* VM特定调试宏声明 */
#ifdef AQL_DEBUG_BUILD
/* 上下文设置函数 */
void aql_vt_set_context(aql_State *L, CallInfo *ci, Instruction i, 
                       const Instruction *pc, LClosure *cl, 
                       StkId base, const char *func_name);

/* 指令特定调试宏声明 */
void aql_vt_loadi_before(void);
void aql_vt_loadi_after(void);
void aql_vt_add_before(void);
void aql_vt_add_after(void);
void aql_vt_mul_before(void);
void aql_vt_mul_after(void);
void aql_vt_sub_before(void);
void aql_vt_sub_after(void);
void aql_vt_div_before(void);
void aql_vt_div_after(void);
void aql_vt_call_before(void);
void aql_vt_call_after(void);
void aql_vt_jmp_before(void);
void aql_vt_jmp_after(void);
void aql_vt_closure_before(void);
void aql_vt_closure_after(void);
void aql_vt_return_before(void);
void aql_vt_return_after(void);

/* 通用调试函数声明 */
void aql_vt_load_before(const char *op_name);
void aql_vt_load_after(const char *op_name);
void aql_vt_arith_before(const char *op_name);
void aql_vt_arith_after(const char *op_name);
void aql_vt_control_before(const char *op_name);
void aql_vt_control_after(const char *op_name);
void aql_vt_jump_before(const char *op_name);
void aql_vt_jump_after(const char *op_name);
void aql_vt_object_before(const char *op_name);
void aql_vt_object_after(const char *op_name);
void aql_vt_compare_before(const char *op_name);
void aql_vt_compare_after(const char *op_name);
void aql_vt_test_before(const char *op_name);
void aql_vt_test_after(const char *op_name);
void aql_vt_for_before(const char *op_name);
void aql_vt_for_after(const char *op_name);
void aql_vt_table_before(const char *op_name);
void aql_vt_table_after(const char *op_name);
void aql_vt_unary_before(const char *op_name);
void aql_vt_unary_after(const char *op_name);
void aql_vt_arithk_before(const char *op_name);
void aql_vt_arithk_after(const char *op_name);
void aql_vt_arithi_before(const char *op_name);
void aql_vt_arithi_after(const char *op_name);

/* 指令特定调试宏定义 */
#define AQL_INFO_VT_LOADI_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_loadi_before(); \
        } \
    } while(0)
#define AQL_INFO_VT_LOADI_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_loadi_after(); \
        } \
    } while(0)
#define AQL_INFO_VT_ADD_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_add_before(); \
        } \
    } while(0)
#define AQL_INFO_VT_ADD_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_add_after(); \
        } \
    } while(0)
#define AQL_INFO_VT_MUL_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_mul_before(); \
        } \
    } while(0)
#define AQL_INFO_VT_MUL_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_mul_after(); \
        } \
    } while(0)
#define AQL_INFO_VT_SUB_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_sub_before(); \
        } \
    } while(0)
#define AQL_INFO_VT_SUB_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_sub_after(); \
        } \
    } while(0)
#define AQL_INFO_VT_DIV_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_div_before(); \
        } \
    } while(0)
#define AQL_INFO_VT_DIV_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_div_after(); \
        } \
    } while(0)
#define AQL_INFO_VT_CALL_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_call_before(); \
        } \
    } while(0)
#define AQL_INFO_VT_CALL_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_call_after(); \
        } \
    } while(0)
#define AQL_INFO_VT_JMP_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_jmp_before(); \
        } \
    } while(0)
#define AQL_INFO_VT_JMP_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_jmp_after(); \
        } \
    } while(0)
#define AQL_INFO_VT_CLOSURE_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_closure_before(); \
        } \
    } while(0)
#define AQL_INFO_VT_CLOSURE_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_closure_after(); \
        } \
    } while(0)
#define AQL_INFO_VT_RETURN_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_return_before(); \
        } \
    } while(0)
#define AQL_INFO_VT_RETURN_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_return_after(); \
        } \
    } while(0)

/* 比较指令调试宏 */
#define AQL_INFO_VT_EQ_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_compare_before("EQ"); \
        } \
    } while(0)
#define AQL_INFO_VT_EQ_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_compare_after("EQ"); \
        } \
    } while(0)
#define AQL_INFO_VT_LT_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_compare_before("LT"); \
        } \
    } while(0)
#define AQL_INFO_VT_LT_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_compare_after("LT"); \
        } \
    } while(0)
#define AQL_INFO_VT_LE_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_compare_before("LE"); \
        } \
    } while(0)
#define AQL_INFO_VT_LE_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_compare_after("LE"); \
        } \
    } while(0)
#define AQL_INFO_VT_LEI_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_compare_before("LEI"); \
        } \
    } while(0)
#define AQL_INFO_VT_LEI_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_compare_after("LEI"); \
        } \
    } while(0)

/* 测试指令调试宏 */
#define AQL_INFO_VT_TEST_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_test_before("TEST"); \
        } \
    } while(0)
#define AQL_INFO_VT_TEST_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_test_after("TEST"); \
        } \
    } while(0)
#define AQL_INFO_VT_TESTSET_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_test_before("TESTSET"); \
        } \
    } while(0)
#define AQL_INFO_VT_TESTSET_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_test_after("TESTSET"); \
        } \
    } while(0)

/* For循环指令调试宏 */
#define AQL_INFO_VT_FORLOOP_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_for_before("FORLOOP"); \
        } \
    } while(0)
#define AQL_INFO_VT_FORLOOP_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_for_after("FORLOOP"); \
        } \
    } while(0)
#define AQL_INFO_VT_FORPREP_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_for_before("FORPREP"); \
        } \
    } while(0)
#define AQL_INFO_VT_FORPREP_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_for_after("FORPREP"); \
        } \
    } while(0)

/* 表操作指令调试宏 */
#define AQL_INFO_VT_GETTABLE_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_table_before("GETTABLE"); \
        } \
    } while(0)
#define AQL_INFO_VT_GETTABLE_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_table_after("GETTABLE"); \
        } \
    } while(0)
#define AQL_INFO_VT_SETTABLE_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_table_before("SETTABLE"); \
        } \
    } while(0)
#define AQL_INFO_VT_SETTABLE_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_table_after("SETTABLE"); \
        } \
    } while(0)

/* 一元操作指令调试宏 */
#define AQL_INFO_VT_UNM_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_unary_before("UNM"); \
        } \
    } while(0)
#define AQL_INFO_VT_UNM_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_unary_after("UNM"); \
        } \
    } while(0)
#define AQL_INFO_VT_LEN_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_unary_before("LEN"); \
        } \
    } while(0)
#define AQL_INFO_VT_LEN_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_unary_after("LEN"); \
        } \
    } while(0)

/* 常量算术指令调试宏 */
#define AQL_INFO_VT_ADDK_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_arithk_before("ADDK"); \
        } \
    } while(0)
#define AQL_INFO_VT_ADDK_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_arithk_after("ADDK"); \
        } \
    } while(0)
#define AQL_INFO_VT_SUBK_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_arithk_before("SUBK"); \
        } \
    } while(0)
#define AQL_INFO_VT_SUBK_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_arithk_after("SUBK"); \
        } \
    } while(0)
#define AQL_INFO_VT_MULK_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_arithk_before("MULK"); \
        } \
    } while(0)
#define AQL_INFO_VT_MULK_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_arithk_after("MULK"); \
        } \
    } while(0)

/* 立即数算术指令调试宏 */
#define AQL_INFO_VT_ADDI_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_arithi_before("ADDI"); \
        } \
    } while(0)
#define AQL_INFO_VT_ADDI_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_arithi_after("ADDI"); \
        } \
    } while(0)
#define AQL_INFO_VT_SUBI_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_arithi_before("SUBI"); \
        } \
    } while(0)
#define AQL_INFO_VT_SUBI_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_arithi_after("SUBI"); \
        } \
    } while(0)
#define AQL_INFO_VT_MULI_BEFORE() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_arithi_before("MULI"); \
        } \
    } while(0)
#define AQL_INFO_VT_MULI_AFTER() \
    do { \
        if (aql_debug_is_enabled(AQL_FLAG_VT)) { \
            aql_vt_arithi_after("MULI"); \
        } \
    } while(0)

#else
/* 生产版本：所有指令特定调试宏为空 */
#define AQL_INFO_VT_LOADI_BEFORE()    ((void)0)
#define AQL_INFO_VT_LOADI_AFTER()     ((void)0)
#define AQL_INFO_VT_ADD_BEFORE()      ((void)0)
#define AQL_INFO_VT_ADD_AFTER()       ((void)0)
#define AQL_INFO_VT_MUL_BEFORE()      ((void)0)
#define AQL_INFO_VT_MUL_AFTER()       ((void)0)
#define AQL_INFO_VT_SUB_BEFORE()      ((void)0)
#define AQL_INFO_VT_SUB_AFTER()       ((void)0)
#define AQL_INFO_VT_DIV_BEFORE()      ((void)0)
#define AQL_INFO_VT_DIV_AFTER()       ((void)0)
#define AQL_INFO_VT_CALL_BEFORE()     ((void)0)
#define AQL_INFO_VT_CALL_AFTER()      ((void)0)
#define AQL_INFO_VT_JMP_BEFORE()      ((void)0)
#define AQL_INFO_VT_JMP_AFTER()       ((void)0)
#define AQL_INFO_VT_CLOSURE_BEFORE()  ((void)0)
#define AQL_INFO_VT_CLOSURE_AFTER()   ((void)0)
#define AQL_INFO_VT_RETURN_BEFORE()   ((void)0)
#define AQL_INFO_VT_RETURN_AFTER()    ((void)0)

/* 比较指令调试宏 - 生产版本 */
#define AQL_INFO_VT_EQ_BEFORE()       ((void)0)
#define AQL_INFO_VT_EQ_AFTER()        ((void)0)
#define AQL_INFO_VT_LT_BEFORE()       ((void)0)
#define AQL_INFO_VT_LT_AFTER()        ((void)0)
#define AQL_INFO_VT_LE_BEFORE()       ((void)0)
#define AQL_INFO_VT_LE_AFTER()        ((void)0)
#define AQL_INFO_VT_LEI_BEFORE()      ((void)0)
#define AQL_INFO_VT_LEI_AFTER()       ((void)0)

/* 测试指令调试宏 - 生产版本 */
#define AQL_INFO_VT_TEST_BEFORE()     ((void)0)
#define AQL_INFO_VT_TEST_AFTER()      ((void)0)
#define AQL_INFO_VT_TESTSET_BEFORE()  ((void)0)
#define AQL_INFO_VT_TESTSET_AFTER()   ((void)0)

/* For循环指令调试宏 - 生产版本 */
#define AQL_INFO_VT_FORLOOP_BEFORE()  ((void)0)
#define AQL_INFO_VT_FORLOOP_AFTER()   ((void)0)
#define AQL_INFO_VT_FORPREP_BEFORE()  ((void)0)
#define AQL_INFO_VT_FORPREP_AFTER()   ((void)0)

/* 表操作指令调试宏 - 生产版本 */
#define AQL_INFO_VT_GETTABLE_BEFORE() ((void)0)
#define AQL_INFO_VT_GETTABLE_AFTER()  ((void)0)
#define AQL_INFO_VT_SETTABLE_BEFORE() ((void)0)
#define AQL_INFO_VT_SETTABLE_AFTER()  ((void)0)

/* 一元操作指令调试宏 - 生产版本 */
#define AQL_INFO_VT_UNM_BEFORE()      ((void)0)
#define AQL_INFO_VT_UNM_AFTER()       ((void)0)
#define AQL_INFO_VT_LEN_BEFORE()      ((void)0)
#define AQL_INFO_VT_LEN_AFTER()       ((void)0)

/* 常量算术指令调试宏 - 生产版本 */
#define AQL_INFO_VT_ADDK_BEFORE()     ((void)0)
#define AQL_INFO_VT_ADDK_AFTER()      ((void)0)
#define AQL_INFO_VT_SUBK_BEFORE()     ((void)0)
#define AQL_INFO_VT_SUBK_AFTER()      ((void)0)
#define AQL_INFO_VT_MULK_BEFORE()     ((void)0)
#define AQL_INFO_VT_MULK_AFTER()      ((void)0)

/* 立即数算术指令调试宏 - 生产版本 */
#define AQL_INFO_VT_ADDI_BEFORE()     ((void)0)
#define AQL_INFO_VT_ADDI_AFTER()      ((void)0)
#define AQL_INFO_VT_SUBI_BEFORE()     ((void)0)
#define AQL_INFO_VT_SUBI_AFTER()      ((void)0)
#define AQL_INFO_VT_MULI_BEFORE()     ((void)0)
#define AQL_INFO_VT_MULI_AFTER()      ((void)0)
#endif

#endif /* avm_h */ 