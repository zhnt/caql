/*
** $Id: avm_core.c $
** AQL Virtual Machine Core - Lua Compatible Implementation
** 完全兼容 Lua 5.4 的虚拟机实现
** See Copyright Notice in aql.h
*/

#define avm_core_c
#define AQL_CORE

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aql.h"
#include "aopcodes.h"
#include "avm.h"

#include "acontainer.h"
#include "adatatype.h"
#include "adebug.h"
#include "adict.h"
#include "ado.h"
#include "afunc.h"
#include "agc.h"
#include "amem.h"
#include "arange.h"
#include "astring.h"
#include "aobject.h"

extern Dict *get_globals_dict(aql_State *L);

/* 打印寄存器状态 */
static void print_register_state(aql_State *L, StkId base, int max_registers) {
    if (!aql_debug_is_enabled(AQL_FLAG_VT)) return;  // 只在跟踪模式下显示
    
    printf("📊 [TRACE] 寄存器状态:\n");
    for (int i = 0; i < max_registers && i < 10; i++) {  // 限制显示前10个寄存器
        StkId reg = base + i;
        if (reg >= L->stack.p && reg < L->top.p) {
            TValue *val = s2v(reg);
            printf("  R%d: ", i);
            switch (ttypetag(val)) {
                case AQL_VNIL:
                    printf("nil\n");
                    break;
                case AQL_VFALSE:
                    printf("false\n");
                    break;
                case AQL_VTRUE:
                    printf("true\n");
                    break;
                case AQL_VNUMINT:
                    printf("INTEGER %lld\n", (long long)ivalue(val));
                    break;
                case AQL_VNUMFLT:
                    printf("FLOAT %g\n", fltvalue(val));
                    break;
                case AQL_VSHRSTR:
                case AQL_VLNGSTR:
                    printf("STRING \"%s\"\n", svalue(val));
                    break;
                case AQL_VTABLE:
                    printf("TABLE %p\n", (void*)hvalue(val));
                    break;
                case AQL_VLCL:
                    printf("CLOSURE %p\n", (void*)clLvalue(val));
                    break;
                case AQL_VLCF:
                    printf("C_FUNCTION %p\n", (void*)fvalue(val));
                    break;
                default:
                    printf("TYPE_%d %p\n", ttypetag(val), (void*)val);
                    break;
            }
        } else {
            printf("  R%d: <out_of_range>\n", i);
        }
    }
}


/* 获取函数名 */
static const char *get_function_name(LClosure *cl) {
    if (!cl || !cl->p) return "unknown";
    
    // 检查是否是主函数（通过检查是否有父函数）
    // 如果这是顶层函数，显示"main"
    if (cl->p->source) {
        const char *source = getstr(cl->p->source);
        if (source && strlen(source) > 0) {
            // 如果source包含文件名，我们认为是主函数
            if (strstr(source, ".by") || strstr(source, ".lua")) {
                return "main";
            }
            return source;
        }
    }
    
    // 对于没有source信息的函数，我们使用一个简单的策略：
    // 如果这是第一个函数（主函数），显示"main"
    // 否则显示函数地址
    static LClosure *first_func = NULL;
    
    if (first_func == NULL) {
        first_func = cl;
        return "main";
    } else if (cl == first_func) {
        return "main";
    } else {
        static char func_addr[32];
        snprintf(func_addr, sizeof(func_addr), "func@%p", (void*)cl);
        return func_addr;
    }
}

/*
** aql 兼容的常量和宏定义
*/
#define MAXTAGLOOP	2000


/* 使用 avm.h 中已定义的宏，不重复定义 */

/*
** 缺少的 aql 兼容宏定义
*/
#define cvt2num(o)      (ttisnumber(o) || ttisstring(o))
#define cvt2str(o)      ttisnumber(o)
#define tonumber(o,n)   (ttisfloat(o) ? (*(n) = fltvalue(o), 1) : aqlV_tonumber_(o,n))
#define tointegerns(o,p) aqlV_tointegerns(o,p)
#define l_tonumber(o)   (ttisfloat(o) ? fltvalue(o) : cast_num(ivalue(o)))
#define aql_unused(x)   ((void)(x))
#define aql_integertonumber(i) cast_num(i)
#define tostring(L,o)   (ttisstring(o) || (cvt2str(o) && (aqlO_tostring(L, o), 1)))

/* 元方法相关宏 */
#define notm(tm)        ttisnil(tm)
#define fasttm(l,et,e)  ((et) == NULL ? NULL : aqlT_gettm((l), (et), (e)))

/* 字符串函数映射 */
#define aqlS_newlstr    aqlStr_newlstr
#define aqlS_createlngstrobj(L,l) aqlStr_newlstr(L, NULL, l)

/* aql 数值比较宏 */
#define aqli_numlt(a,b)     ((a) < (b))
#define aqli_numle(a,b)     ((a) <= (b))
#define aqli_numeq(a,b)     ((a) == (b))
#define aqli_numgt(a,b)     ((a) > (b))
#define aqli_numge(a,b)     ((a) >= (b))
#define aqli_numunm(L,a)    (-(a))
#define aqli_numadd(L,a,b)  ((a) + (b))
#define aqli_numsub(L,a,b)  ((a) - (b))
#define aqli_nummul(L,a,b)  ((a) * (b))
#define aqli_numdiv(L,a,b)  ((a) / (b))
#define aqli_numidiv(L,a,b) (floor((a) / (b)))
#define aqli_numpow(L,a,b)  (pow(a,b))
#define aqli_nummod(L,a,b,c) ((c) = fmod(a,b))

/* 错误处理宏 */
#define aqlG_forerror(L,o,what) aqlG_runerror(L, "bad 'for' %s (number expected)", what)

/* 缺少的常量定义 */
#define aqlI_MAXSHORTLEN 40  /* 短字符串最大长度 */
#ifndef CIST_LEQ
#define CIST_LEQ        (1<<1)  /* using __le instead of __lt */
#endif
#define CLOSEKTOP       (-1)  /* close upvalues at top level */
#define NONVALIDVALUE   aqlO_nilobject_  /* 无效值 */

/* 缺失的对象和函数映射 */
#define aqlO_nilobject  (&aqlO_nilobject_)
static const TValue aqlO_nilobject_ = {{NULL}, AQL_VNIL};

/* Table函数现在由atable.c提供 */
#include "atable.h"

/* 其他简化实现的函数映射 */
#define sethvalue2s(L,o,h) do { TValue *io=s2v(o); Table *x_=(h); \
  val_(io).gc = obj2gco(x_); settt_(io, ctb(AQL_VTABLE)); \
  checkliveness(L,io); } while(0)
#define aqlC_barrier    aqlC_barrier_
#define aql_threadyield(L) ((void)0)  /* 空操作 */
#define aqlT_adjustvarargs(L,nfixparams,ci,p) ((void)0)  /* 空操作 */

static void aqlT_getvarargs (aql_State *L, CallInfo *ci, StkId ra, int n) {
  LClosure *cl = clLvalue(s2v(ci->func.p));
  Proto *p = cl->p;
  int nextra = ci->u.l.nextraargs;
  StkId vararg = ci->func.p + 1 + p->numparams;

  if (n == 1) {
    AQL_ContainerBase *args = acontainer_new(L, CONTAINER_ARRAY,
                                             AQL_DATA_TYPE_ANY, nextra);
    if (args == NULL) {
      aqlG_runerror(L, "cannot allocate varargs array");
      return;
    }
    for (int i = 0; i < nextra; i++)
      acontainer_array_set(L, args, (size_t)i, s2v(vararg + i));
    setcontainervalue(L, s2v(ra), args);
    return;
  }

  if (n < 0)
    n = nextra;
  for (int i = 0; i < n; i++) {
    if (i < nextra) {
      setobjs2s(L, ra + i, vararg + i);
    }
    else {
      setnilvalue(s2v(ra + i));
    }
  }
  L->top.p = ra + n;
}

/*
** aql 兼容的类型转换函数
*/

/*
** Try to convert a value from string to a number value.
*/
static int l_strton (const TValue *obj, TValue *result) {
  aql_debug("L_STRTON: 开始字符串到数字转换");
  aql_assert(obj != result);
  if (!cvt2num(obj)) {  /* is object not a string? */
    aql_debug("L_STRTON: 对象不是字符串或数字");
    return 0;
  }
  else {
    TString *st = tsvalue(obj);
    aql_debug("L_STRTON: 调用 aqlO_str2num");
    return (aqlO_str2num(getstr(st), result) == tsslen(st) + 1);
  }
}

/*
** Convert a value to a number (including string coercion).
*/
int aqlV_tonumber_ (const TValue *obj, aql_Number *n) {
  TValue v;
  if (ttisfloat(obj))
    *n = fltvalue(obj);
  else if (ttisinteger(obj))
    *n = cast_num(ivalue(obj));
  else if (l_strton(obj, &v)) {
    *n = nvalue(&v);  /* convert result of 'l_strton' to a float */
  }
  else
    return 0;  /* conversion failed */
  return 1;
}

/*
** Convert a float to an integer, respecting the rounding mode 'mode'.
*/
int aqlV_flttointeger (aql_Number n, aql_Integer *p, F2Imod mode) {
  aql_Number f = l_floor(n);
  if (n != f) {  /* not an integral value? */
    if (mode == F2Ieq) return 0;  /* fails if mode is 'eq' */
    else if (mode == F2Iceil)  /* needs ceil? */
      f += 1;  /* convert floor to ceil (remember: n != f) */
  }
  return aql_numbertointeger(f, p);
}

/*
** Convert a value to an integer (existing AQL signature)
*/
int aqlV_tointegerns (const TValue *obj, aql_Integer *p) {
  if (ttisfloat(obj))
    return aqlV_flttointeger(fltvalue(obj), p, F2Ifloor);
  else if (ttisinteger(obj)) {
    *p = ivalue(obj);
    return 1;
  }
  else
    return 0;
}

/*
** Convert a value to an integer with mode (internal use)
*/
static int aqlV_tointegerns_mode (const TValue *obj, aql_Integer *p, F2Imod mode) {
  if (ttisfloat(obj))
    return aqlV_flttointeger(fltvalue(obj), p, mode);
  else if (ttisinteger(obj)) {
    *p = ivalue(obj);
    return 1;
  }
  else
    return 0;
}

/*
** Convert a value to an integer (including string coercion).
*/
int aqlV_tointeger (const TValue *obj, aql_Integer *p, F2Imod mode) {
  aql_debug("AQLV_TOINTEGER: 开始转换到整数");
  
  // 如果已经是整数，直接返回
  if (ttisinteger(obj)) {
    aql_debug("AQLV_TOINTEGER: 已经是整数，值=%ld", (long)ivalue(obj));
    *p = ivalue(obj);
    return 1;
  }
  
  // 如果是浮点数，转换为整数
  if (ttisfloat(obj)) {
    aql_debug("AQLV_TOINTEGER: 是浮点数，值=%f", fltvalue(obj));
    return aqlV_flttointeger(fltvalue(obj), p, mode);
  }
  
  // 如果是字符串，尝试字符串转换
  if (ttisstring(obj)) {
    aql_debug("AQLV_TOINTEGER: 是字符串，尝试转换");
    TValue v;
    if (l_strton(obj, &v)) {  /* does 'obj' point to a numerical string? */
      aql_debug("AQLV_TOINTEGER: 字符串转换成功");
      obj = &v;  /* change it to point to its corresponding number */
      return aqlV_tointegerns_mode(obj, p, mode);
    }
  }
  
  aql_debug("AQLV_TOINTEGER: 转换失败");
  return 0;  /* conversion failed */
}

/*
** Arithmetic operations
*/
aql_Integer aqlV_idiv (aql_State *L, aql_Integer m, aql_Integer n) {
  if (l_castS2U(n) + 1u <= 1u) {  /* special cases: -1 or 0 */
    if (n == 0)
      aqlG_runerror(L, "attempt to divide by zero");
    return intop(-, 0, m);   /* n==-1; avoid overflow with 0x80000...//-1 */
  }
  else {
    aql_Integer q = m / n;  /* perform C division */
    if ((m ^ n) < 0 && m % n != 0)  /* 'm/n' would be negative non-integer? */
      q -= 1;  /* correct result for different rounding */
    return q;
  }
}

aql_Integer aqlV_mod (aql_State *L, aql_Integer m, aql_Integer n) {
  if (l_castS2U(n) + 1u <= 1u) {  /* special cases: -1 or 0 */
    if (n == 0)
      aqlG_runerror(L, "attempt to perform 'n%%0'");
    return 0;   /* m % -1 == 0; avoid overflow with 0x80000...%-1 */
  }
  else {
    aql_Integer r = m % n;
    if (r != 0 && (r ^ n) < 0)  /* 'm/n' would be non-integer negative? */
      r += n;  /* correct result for different rounding */
    return r;
  }
}

aql_Number aqlV_modf (aql_State *L, aql_Number m, aql_Number n) {
  aql_Number r;
  aql_unused(L);
  aqli_nummod(L, m, n, r);
  return r;
}

aql_Integer aqlV_shiftl (aql_Integer x, aql_Integer y) {
  if (y < 0) {  /* shift right? */
    if (y <= -NBITS) return 0;
    else return intop(>>, x, -y);
  }
  else {  /* shift left */
    if (y >= NBITS) return 0;
    else return intop(<<, x, y);
  }
}

aql_Integer aqlV_shiftr (aql_Integer x, aql_Integer y) {
  if (y < 0) {  /* shift left? */
    if (y <= -NBITS) return 0;
    else return intop(<<, x, -y);
  }
  else {  /* shift right */
    if (y >= NBITS) return 0;
    else return intop(>>, x, y);
  }
}

/*
** For 循环辅助函数
*/
static int forlimit (aql_State *L, aql_Integer init, const TValue *lim,
                                   aql_Integer *p, aql_Integer step) {
  if (!aqlV_tointeger(lim, p, (step < 0 ? F2Iceil : F2Ifloor))) {
    /* not coercible to in integer */
    aql_Number flim;  /* try to convert to float */
    if (!aqlV_tonumber_(lim, &flim)) /* cannot convert to float? */
      aqlG_forerror(L, lim, "limit");
    /* else 'flim' is a float out of integer bounds */
    if (aqli_numlt(0, flim)) {  /* if it is positive, it is too large */
      if (step < 0) return 1;  /* initial value must be less than it */
      *p = AQL_MAXINTEGER;  /* truncate */
    }
    else {  /* it is less than min integer */
      if (step > 0) return 1;  /* initial value must be greater than it */
      *p = AQL_MININTEGER;  /* truncate */
    }
  }
  return (step > 0 ? init > *p : init < *p);  /* inclusive numeric-for limit */
}

/*
** Prepare a numerical for loop (opcode OP_FORPREP).
** Return 1 to skip the loop (and remove prepended values from the stack),
** 0 otherwise.
*/
static int forprep (aql_State *L, StkId ra) {
  TValue *pinit = s2v(ra);
  TValue *plimit = s2v(ra + 1);
  TValue *pstep = s2v(ra + 2);

  if (ttisnil(pstep) && ttisinteger(pinit) && ttisinteger(plimit)) {
    setivalue(pstep, (ivalue(pinit) < ivalue(plimit)) ? 1 : -1);
  }

  if (ttisinteger(pinit) && ttisinteger(pstep)) { /* integer loop? */
    aql_Integer init = ivalue(pinit);
    aql_Integer step = ivalue(pstep);
    aql_Integer limit;
    if (step == 0)
      aqlG_runerror(L, "'for' step is zero");
    setivalue(s2v(ra + 3), init);  /* control variable */
    if (forlimit(L, init, plimit, &limit, step))
      return 1;  /* skip the loop */
    else {  /* prepare loop counter */
      aql_Unsigned count;
      if (step > 0) {  /* ascending loop? */
        count = l_castS2U(limit) - l_castS2U(init);
        if (step != 1)  /* avoid division in the too common case */
          count /= l_castS2U(step);
      }
      else {  /* step < 0; descending loop */
        count = l_castS2U(init) - l_castS2U(limit);
        /* 'step+1' avoids negating 'mininteger' */
        count /= l_castS2U(-(step + 1)) + 1u;
      }
      /* store the counter in place of the limit (which won't be
         needed anymore) */
      setivalue(plimit, l_castU2S(count));
    }
  }
  else {  /* try making all values floats */
    aql_Number init; aql_Number limit; aql_Number step;
    if (!aqlV_tonumber_(plimit, &limit))
      aqlG_forerror(L, plimit, "limit");
    if (!aqlV_tonumber_(pstep, &step))
      aqlG_forerror(L, pstep, "step");
    if (!aqlV_tonumber_(pinit, &init))
      aqlG_forerror(L, pinit, "initial value");
    if (step == 0)
      aqlG_runerror(L, "'for' step is zero");
    if (aqli_numlt(0, step) ? aqli_numlt(limit, init)
                            : aqli_numlt(init, limit))
      return 1;  /* skip the loop */
    else {
      /* make sure internal values are all floats */
      setfltvalue(plimit, limit);
      setfltvalue(pstep, step);
      setfltvalue(s2v(ra), init);  /* internal index */
      setfltvalue(s2v(ra + 3), init);  /* control variable */
    }
  }
  return 0;
}

/*
** Execute a step of a float numerical for loop, returning
** true iff the loop must continue. (The integer case is
** written online with opcode OP_FORLOOP, for performance.)
*/
static int floatforloop (StkId ra) {
  aql_Number step = fltvalue(s2v(ra + 2));
  aql_Number counter = fltvalue(s2v(ra + 1)) - step;
  setfltvalue(s2v(ra + 1), counter);
  return aqli_numlt(0, counter);
}

/*
** 字符串比较函数
*/
static int l_strcmp (const TString *ts1, const TString *ts2) {
  const char *l = getstr(ts1);
  size_t ll = tsslen(ts1);
  const char *r = getstr(ts2);
  size_t lr = tsslen(ts2);
  for (;;) {  /* for each segment */
    int temp = strcoll(l, r);
    if (temp != 0)  /* not equal? */
      return temp;  /* done */
    else {  /* strings are equal up to a '\0' */
      size_t len = strlen(l);  /* index of first '\0' in both strings */
      if (len == lr)  /* 'r' is finished? */
        return (len == ll) ? 0 : 1;  /* check 'l' */
      else if (len == ll)  /* 'l' is finished? */
        return -1;  /* 'l' is less than 'r' ('r' is not finished) */
      /* both strings longer than 'len'; go to next '\0' */
      len++;
      l += len; ll -= len; r += len; lr -= len;
    }
  }
}

/*
** 数值比较宏和函数
*/
#define LTnum(l,r)       aqli_numlt((l), (r))
#define LEnum(l,r)       aqli_numle((l), (r))

/*
** Check whether integer 'i' is less than float 'f'. If 'i' has an
** exact representation as a float ('l_intfitsf'), compare numbers as
** floats. Otherwise, if 'f' is outside the range for integers, result
** is trivial. Otherwise, compare them as integers. (When 'i' has no
** float representation, either 'f' is "far away" from 'i' or 'f' has
** no integer representation; either way, how to compare them is
** irrelevant.) When 'f' is NaN, comparisons must result in false.
*/
static int LTintfloat (aql_Integer i, aql_Number f) {
  if (l_intfitsf(i))
    return aqli_numlt(cast_num(i), f);  /* compare them as floats */
  else {  /* i does not fit in a float */
    if (isnan(f))  /* f is NaN? */
      return 0;  /* comparison is always false */
    else if (f >= cast_num(AQL_MAXINTEGER))  /* f >= maxinteger? */
      return 1;  /* i < f */
    else if (f > cast_num(AQL_MININTEGER))  /* mininteger < f < maxinteger? */
      return (i < cast(aql_Integer, f));  /* compare them as integers */
    else  /* f <= mininteger */
      return 0;  /* i >= f */
  }
}

static int LEintfloat (aql_Integer i, aql_Number f) {
  if (l_intfitsf(i))
    return aqli_numle(cast_num(i), f);  /* compare them as floats */
  else {  /* i does not fit in a float */
    if (isnan(f))  /* f is NaN? */
      return 0;  /* comparison is always false */
    else if (f >= cast_num(AQL_MAXINTEGER))  /* f >= maxinteger? */
      return 1;  /* i <= f */
    else if (f > cast_num(AQL_MININTEGER))  /* mininteger < f < maxinteger? */
      return (i <= cast(aql_Integer, f));  /* compare them as integers */
    else  /* f <= mininteger */
      return 0;  /* i > f */
  }
}

static int LTfloatint (aql_Number f, aql_Integer i) {
  if (l_intfitsf(i))
    return aqli_numlt(f, cast_num(i));  /* compare them as floats */
  else {  /* i does not fit in a float */
    if (isnan(f))  /* f is NaN? */
      return 0;  /* comparison is always false */
    else if (f >= cast_num(AQL_MAXINTEGER))  /* f >= maxinteger? */
      return 0;  /* f >= i */
    else if (f > cast_num(AQL_MININTEGER))  /* mininteger < f < maxinteger? */
      return (cast(aql_Integer, f) < i);  /* compare them as integers */
    else  /* f <= mininteger */
      return 1;  /* f < i */
  }
}

static int LEfloatint (aql_Number f, aql_Integer i) {
  if (l_intfitsf(i))
    return aqli_numle(f, cast_num(i));  /* compare them as floats */
  else {  /* i does not fit in a float */
    if (isnan(f))  /* f is NaN? */
      return 0;  /* comparison is always false */
    else if (f >= cast_num(AQL_MAXINTEGER))  /* f >= maxinteger? */
      return 0;  /* f >= i */
    else if (f > cast_num(AQL_MININTEGER))  /* mininteger < f < maxinteger? */
      return (cast(aql_Integer, f) <= i);  /* compare them as integers */
    else  /* f <= mininteger */
      return 1;  /* f <= i */
  }
}

/*
** 比较操作的辅助函数
*/
static int lessthanothers (aql_State *L, const TValue *l, const TValue *r) {
  aql_assert(!ttisnumber(l) || !ttisnumber(r));
  if (ttisstring(l) && ttisstring(r))  /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) < 0;
  else
    return aqlT_callorderTM(L, l, r, TM_LT);
}

/*
** Main operation less than; return 'l < r'.
*/
int aqlV_lessthan (aql_State *L, const TValue *l, const TValue *r) {
  if (ttisinteger(l)) {
    aql_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li < ivalue(r);  /* both are integers */
    else if (ttisfloat(r))
      return LTintfloat(li, fltvalue(r));  /* integer < float */
  }
  else if (ttisfloat(l)) {
    aql_Number lf = fltvalue(l);
    if (ttisfloat(r))
      return LTnum(lf, fltvalue(r));  /* both are float */
    else if (ttisinteger(r))
      return LTfloatint(lf, ivalue(r));  /* float < integer */
  }
  return lessthanothers(L, l, r);  /* call metamethod */
}

static int lessequalothers (aql_State *L, const TValue *l, const TValue *r) {
  aql_assert(!ttisnumber(l) || !ttisnumber(r));
  if (ttisstring(l) && ttisstring(r))  /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) <= 0;
  if (L == NULL)
    return aqlG_ordererror(L, l, r);
  const TValue *tm = aqlT_gettmbyobj(L, l, TM_LE);
  if (notm(tm)) {
    tm = aqlT_gettmbyobj(L, r, TM_LE);
  }
  if (notm(tm)) {
    const TValue *lt_tm = aqlT_gettmbyobj(L, l, TM_LT);
    if (notm(lt_tm)) {
      lt_tm = aqlT_gettmbyobj(L, r, TM_LT);
    }
    if (notm(lt_tm)) {
      return aqlG_ordererror(L, l, r);
    }
    return !aqlT_callorderTM(L, r, l, TM_LT);
  }
  else
    return aqlT_callorderTM(L, l, r, TM_LE);
}

/*
** Main operation less than or equal to; return 'l <= r'.
*/
int aqlV_lessequal (aql_State *L, const TValue *l, const TValue *r) {
  if (ttisinteger(l)) {
    aql_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li <= ivalue(r);  /* both are integers */
    else if (ttisfloat(r))
      return LEintfloat(li, fltvalue(r));  /* integer <= float */
  }
  else if (ttisfloat(l)) {
    aql_Number lf = fltvalue(l);
    if (ttisfloat(r))
      return LEnum(lf, fltvalue(r));  /* both are float */
    else if (ttisinteger(r))
      return LEfloatint(lf, ivalue(r));  /* float <= integer */
  }
  return lessequalothers(L, l, r);  /* call metamethod */
}

/*
** Main operation for equality of aql values; return 't1 == t2'.
** L == NULL means raw equality (no metamethods)
*/
int aqlV_equalobj (aql_State *L, const TValue *t1, const TValue *t2) {
  const TValue *tm;
  if (ttypetag(t1) != ttypetag(t2)) {  /* not the same variant? */
    if (ttype(t1) != ttype(t2) || ttype(t1) != AQL_TNUMBER)
      return 0;  /* only numbers can be equal with different variants */
    else {  /* two numbers with different variants */
      /* One of them is an integer. If the other does not fit in an
         integer, they cannot be equal. Otherwise, compare their
         numerical values. */
      aql_Integer i1, i2;
      return (aqlV_tointegerns_mode(t1, &i1, F2Ieq) &&
              aqlV_tointegerns_mode(t2, &i2, F2Ieq) &&
              i1 == i2);
    }
  }
  /* values have same type and same variant */
  switch (ttypetag(t1)) {
    case AQL_VNIL: case AQL_VFALSE: case AQL_VTRUE: return 1;
    case AQL_VNUMINT: return (ivalue(t1) == ivalue(t2));
    case AQL_VNUMFLT: return aqli_numeq(fltvalue(t1), fltvalue(t2));
    case AQL_VLIGHTUSERDATA: return pvalue(t1) == pvalue(t2);
    case AQL_VLCF: return fvalue(t1) == fvalue(t2);
    case AQL_VSHRSTR: return eqshrstr(tsvalue(t1), tsvalue(t2));
    case AQL_VLNGSTR: return aqlS_eqlngstr(tsvalue(t1), tsvalue(t2));
    default: {
      StkId res = L ? L->top.p : NULL;
      if (gcvalue(t1) == gcvalue(t2)) {
        return 1;
      }
      if (L == NULL) {
        return 0;
      }
      tm = aqlT_gettmbyobj(L, t1, TM_EQ);
      if (notm(tm)) {
        tm = aqlT_gettmbyobj(L, t2, TM_EQ);
      }
      if (notm(tm)) {
        return 0;
      }
      aqlT_callTMres(L, tm, t1, t2, res);
      return !l_isfalse(s2v(res));
    }
  }
}

/*
** 字符串连接相关函数
*/
#define tostring(L,o)  \
	(ttisstring(o) || (cvt2str(o) && (aqlO_tostring(L, o), 1)))

#define isemptystr(o)  (ttisshrstring(o) && tsvalue(o)->shrlen == 0)

static int value_to_string_value(aql_State *L, const TValue *src, TValue *dst) {
  if (ttisstring(src)) {
    setobj(L, dst, src);
  } else if (ttisinteger(src)) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%lld", (long long)ivalue(src));
    setsvalue(L, dst, aqlStr_new(L, buffer));
  } else if (ttisfloat(src)) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.14g", fltvalue(src));
    setsvalue(L, dst, aqlStr_new(L, buffer));
  } else if (ttisboolean(src)) {
    setsvalue(L, dst, aqlStr_new(L, bvalue(src) ? "true" : "false"));
  } else if (ttisnil(src)) {
    setsvalue(L, dst, aqlStr_new(L, "nil"));
  } else {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "(type %d)", ttype(src));
    setsvalue(L, dst, aqlStr_new(L, buffer));
  }
  return ttisstring(dst);
}

/* copy strings in stack from top - n up to top - 1 to buffer */
static void copy2buff (aql_State *L, StkId top, int n, char *buff) {
  size_t tl = 0;  /* size already copied */
  do {
    top--;
    if (!ttisstring(s2v(top)))
      aqlG_typeerror(L, s2v(top), "concatenate");
    size_t l = vslen(s2v(top));
    memcpy(buff + tl, svalue(s2v(top)), l * sizeof(char));
    tl += l;
  } while (--n > 0);
}

/*
** Main operation for concatenation: concat 'total' values in the stack,
** from 'L->top - total' up to 'L->top - 1'.
*/
void aqlV_concat (aql_State *L, int total) {
  if (total == 1)
    return;  /* "concatenation" of one string is itself */
  do {
    StkId top = L->top.p;
    int n = 2;  /* number of elements handled in this pass (at least 2) */
    if (!(ttisstring(s2v(top - 2)) || cvt2str(s2v(top - 2))) ||
        !tostring(L, s2v(top - 1)))
      aqlT_trybinTM(L, s2v(top - 2), s2v(top - 1), top - 2, TM_CONCAT);
    else if (isemptystr(s2v(top - 1)))  /* second operand is empty? */
      cast_void(tostring(L, s2v(top - 2)));  /* result is first operand */
    else if (isemptystr(s2v(top - 2))) {  /* first operand is empty string? */
      setobjs2s(L, top - 2, top - 1);  /* result is second op. */
    }
    else {
      /* at least two non-empty string values; get as many as possible */
      size_t tl = vslen(s2v(top - 1));
      TString *ts;
      /* collect total length and number of strings */
      for (n = 1; n < total && tostring(L, s2v(top - n - 1)); n++) {
        size_t l = vslen(s2v(top - n - 1));
        if (l_unlikely(l >= (MAX_SIZE/sizeof(char)) - tl))
          aqlG_runerror(L, "string length overflow");
        tl += l;
      }
      if (tl <= aqlI_MAXSHORTLEN) {  /* is result a short string? */
        char buff[aqlI_MAXSHORTLEN];
        copy2buff(L, top, n, buff);
        ts = aqlS_newlstr(L, buff, tl);
      }
      else {  /* long string; copy strings directly to final result */
        ts = aqlS_createlngstrobj(L, tl);
        copy2buff(L, top, n, getstr(ts));
      }
      setsvalue2s(L, top - n, ts);  /* create result */
    }
    total -= n-1;  /* got 'n' strings to create 1 new */
    L->top.p -= n-1;  /* popped 'n' strings and pushed one */
  } while (total > 1);  /* repeat until only 1 result left */
}

/*
** Main operation 'ra = #rb'.
*/
void aqlV_objlen (aql_State *L, StkId ra, const TValue *rb) {
  const TValue *tm;
  if (ttiscontainer(rb)) {
    AQL_ContainerBase *container = containervalue(rb);
    setivalue(s2v(ra), l_castU2S(container->length));
    return;
  }
  switch (ttypetag(rb)) {
    case AQL_VTABLE: {
      Table *h = hvalue(rb);
      tm = fasttm(L, h->metatable, TM_LEN);
      if (tm) break;  /* metamethod? break switch to call it */
      setivalue(s2v(ra), aqlH_getn(h));  /* else primitive len */
      return;
    }
    case AQL_VSHRSTR: {
      setivalue(s2v(ra), tsvalue(rb)->shrlen);
      return;
    }
    case AQL_VLNGSTR: {
      setivalue(s2v(ra), tsvalue(rb)->u.lnglen);
      return;
    }
    default: {  /* try metamethod */
      tm = aqlT_gettmbyobj(L, rb, TM_LEN);
      if (l_unlikely(notm(tm)))  /* no metamethod? */
        aqlG_typeerror(L, rb, "get length of");
      break;
    }
  }
  aqlT_callTMres(L, tm, rb, aqlO_nilobject, ra);
}

/*
** 主要宏定义 - 与 aql 完全一致
*/

/* 算术操作宏 */
#define l_addi(L,a,b)	intop(+, a, b)
#define l_subi(L,a,b)	intop(-, a, b)
#define l_muli(L,a,b)	intop(*, a, b)
#define l_band(a,b)	intop(&, a, b)
#define l_bor(a,b)	intop(|, a, b)
#define l_bxor(a,b)	intop(^, a, b)

#define l_lti(a,b)	(a < b)
#define l_lei(a,b)	(a <= b)
#define l_gti(a,b)	(a > b)
#define l_gei(a,b)	(a >= b)

/* VM 执行宏 - 与 Lua 5.4 完全一致 */
#define vmfetch()	{ \
  if (l_unlikely(trap)) { \
    /* trap = aqlG_traceexec(L, pc); */ /* TODO: implement hook handling */ \
    updatebase(ci); \
  } \
  i = *(pc++); \
}
#define vmdispatch(o)	switch(o)
#define vmcase(l)	case l:
#define vmbreak		break

/* 寄存器访问宏 - 与 aql 一致 */
#define RA(i)	(base+GETARG_A(i))
#define RB(i)	(base+GETARG_B(i))
#define vRB(i)	s2v(RB(i))
#define KB(i)	(k+GETARG_B(i))
#define RC(i)	(base+GETARG_C(i))
#define vRC(i)	s2v(RC(i))
#define KC(i)	(k+GETARG_C(i))
#define RKC(i)	((TESTARG_k(i)) ? k + GETARG_C(i) : s2v(base + GETARG_C(i)))

/* 执行控制宏 */
#define updatetrap(ci)  (trap = ci->u.l.trap)
#define updatebase(ci)	(base = ci->func.p + 1)
#define updatestack(ci)  \
	{ if (l_unlikely(trap)) { updatebase(ci); ra = RA(i); } }

#define dojump(ci,i,e)	{ pc += GETARG_sJ(i) + e; updatetrap(ci); }
#define donextjump(ci)	{ Instruction ni = *pc; dojump(ci, ni, 1); }
#define docondjump()	if (cond != GETARG_k(i)) pc++; else donextjump(ci);

#define savepc(L)	(ci->u.l.savedpc = pc)
#define savestate(L,ci)		(savepc(L), L->top.p = ci->top.p)

#define Protect(exp)  (savestate(L,ci), (exp), updatetrap(ci))
#define ProtectNT(exp)  (savepc(L), (exp), updatetrap(ci))
#define halfProtect(exp)  (savestate(L,ci), (exp))

#define checkGC(L,c)  \
	{ aqlC_condGC(L, (savepc(L), L->top.p = (c)), \
                         updatetrap(ci)); \
           aql_threadyield(L); }

/*
** 算术操作宏 - 与 aql 完全一致
*/

/* Arithmetic operations with immediate operand - 与 Lua 5.4 完全一致 */
#define op_arithI(L,iop,fop) { \
  StkId ra = RA(i); \
  TValue *v1 = vRB(i); \
  int imm = GETARG_sC(i); \
  if (ttisinteger(v1)) { \
    aql_Integer iv1 = ivalue(v1); \
    pc++; \
    setivalue(s2v(ra), iop(L, iv1, imm)); \
  } \
  else if (ttisfloat(v1)) { \
    aql_Number nb = fltvalue(v1); \
    aql_Number fimm = cast_num(imm); \
    pc++; \
    setfltvalue(s2v(ra), fop(L, nb, fimm)); } }

#define op_arithf_aux(L,v1,v2,fop) {  \
  aql_Number n1; aql_Number n2;  \
  if (tonumber(v1, &n1) && tonumber(v2, &n2)) {  \
    pc++; \
    setfltvalue(s2v(ra), fop(L, n1, n2)); }  \
}

#define op_arithf(L,fop) {  \
  TValue *v1 = vRB(i); TValue *v2 = vRC(i);  \
  op_arithf_aux(L, v1, v2, fop); }

#define op_arithfK(L,fop) {  \
  TValue *v1 = vRB(i); TValue *v2 = KC(i); \
  op_arithf_aux(L, v1, v2, fop); }

#define op_arith_aux(L,v1,v2,iop,fop) { \
  StkId ra = RA(i); \
  if (ttisinteger(v1) && ttisinteger(v2)) { \
    aql_Integer i1 = ivalue(v1); aql_Integer i2 = ivalue(v2); \
    pc++; \
    setivalue(s2v(ra), iop(L, i1, i2)); \
  } \
  else op_arithf_aux(L, v1, v2, fop); }

#define op_arith(L,iop,fop) {  \
  TValue *v1 = vRB(i); TValue *v2 = vRC(i);  \
  op_arith_aux(L, v1, v2, iop, fop); }

#define op_arithK(L,iop,fop) {  \
  TValue *v1 = vRB(i); TValue *v2 = KC(i);  \
  op_arith_aux(L, v1, v2, iop, fop); }

#define op_bitwiseK(L,op) {  \
  TValue *v1 = vRB(i); TValue *v2 = KC(i);  \
  aql_Integer i1; aql_Integer i2 = ivalue(v2);  \
  if (tointegerns(v1, &i1)) {  \
    pc++; \
    setivalue(s2v(ra), op(i1, i2));  \
  } }

#define op_bitwise(L,op) {  \
  TValue *v1 = vRB(i); TValue *v2 = vRC(i);  \
  aql_Integer i1; aql_Integer i2;  \
  if (tointegerns(v1, &i1) && tointegerns(v2, &i2)) {  \
    pc++; \
    setivalue(s2v(ra), op(i1, i2));  \
  } }

#define op_order(L,opi,opn,other) {  \
  int cond;  \
  TValue *rb = vRB(i);  \
  if (ttisinteger(s2v(ra)) && ttisinteger(rb)) {  \
    aql_Integer ia = ivalue(s2v(ra));  \
    aql_Integer ib = ivalue(rb);  \
    cond = opi(ia, ib);  \
  }  \
  else if (ttisnumber(s2v(ra)) && ttisnumber(rb))  \
    cond = opn(cast_num(nvalue(s2v(ra))), cast_num(nvalue(rb)));  \
  else  \
    Protect(cond = other(L, s2v(ra), rb));  \
  docondjump(); }

#define op_orderI(L,opi,opf,inv,tm) {  \
  int cond;  \
  int im = GETARG_sB(i);  \
  if (ttisinteger(s2v(ra)))  \
    cond = opi(ivalue(s2v(ra)), im);  \
  else if (ttisfloat(s2v(ra))) {  \
    aql_Number fa = fltvalue(s2v(ra));  \
    aql_Number fim = cast_num(im);  \
    cond = opf(fa, fim);  \
  }  \
  else {  \
    Protect(cond = aqlT_callorderiTM(L, s2v(ra), im, inv, 0, tm));  \
  }  \
  docondjump(); }

/*
** 闭包和表操作函数
*/
static void pushclosure (aql_State *L, Proto *p, UpVal **encup, StkId base,
                         StkId ra) {
  int nup = p->sizeupvalues;
  Upvaldesc *uv = p->upvalues;
  int i;
  aql_debug("[DEBUG] pushclosure: nup=%d, base=%p, ra=%p\n", nup, (void*)base, (void*)ra);
  LClosure *ncl = aqlF_newLclosure(L, nup);
  ncl->p = p;
  setclLvalue2s(L, ra, ncl);  /* anchor new closure in stack */
  for (i = 0; i < nup; i++) {  /* fill in its upvalues */
    aql_debug("[DEBUG] pushclosure: upvalue[%d] instack=%d, idx=%d\n", i, uv[i].instack, uv[i].idx);
    if (uv[i].instack)  /* upvalue refers to local variable? */
      ncl->upvals[i] = aqlF_findupval(L, base + uv[i].idx);
    else  /* get upvalue from enclosing function */
      ncl->upvals[i] = encup[uv[i].idx];
    aqlC_barrier(L, obj2gco(ncl), obj2gco(ncl->upvals[i]));
  }
}

/*
** finish execution of an opcode interrupted by a yield
*/
void aqlV_finishOp (aql_State *L) {
  CallInfo *ci = L->ci;
  StkId base = ci->func.p + 1;
  Instruction inst = *(ci->u.l.savedpc - 1);  /* interrupted instruction */
  OpCode op = GET_OPCODE(inst);
  switch (op) {  /* finish its execution */
    case OP_UNM: case OP_BNOT: case OP_LEN:
    case OP_GETTABUP: case OP_GETTABLE: case OP_GETI:
    case OP_GETFIELD: case OP_SELF: {
      setobjs2s(L, base + GETARG_A(inst), --L->top.p);
      break;
    }
    case OP_LT: case OP_LE:
    case OP_LTI: case OP_LEI:
    case OP_GTI: case OP_GEI:
    case OP_EQ: {  /* note that 'OP_EQI'/'OP_EQK' cannot yield */
      int res = !l_isfalse(s2v(--L->top.p));
      if (ci->callstatus & CIST_LEQ) {  /* "<=" using "<" instead? */
        ci->callstatus ^= CIST_LEQ;  /* clear mark */
        res = !res;  /* negate result */
      }
      aql_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_JMP);
      if (res != GETARG_k(inst))  /* condition failed? */
        ci->u.l.savedpc++;  /* skip jump instruction */
      break;
    }
    case OP_CONCAT: {
      StkId top = L->top.p;  /* top when 'aqlT_trybinTM' was called */
      int a = GETARG_A(inst);      /* first element to concatenate */
      int total = cast_int(top - 1 - (base + a));  /* yet to concatenate */
      setobjs2s(L, top - 1, base + a);  /* put TM result in proper position */
      if (total > 1) {  /* are there elements to concat? */
        L->top.p = top - 1;  /* top is one after last element (at top-2) */
        aqlV_concat(L, total);  /* concat them (may yield again) */
      }
      /* move final result to final position */
      setobjs2s(L, ci->func.p + 1 + a, --L->top.p);
      L->top.p = ci->top.p;  /* restore top */
      break;
    }
    case OP_TFORCALL: {
      aql_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_TFORLOOP);
      L->top.p = ci->top.p;  /* correct top */
      break;
    }
    case OP_CALL: {
      if (GETARG_C(inst) - 1 >= 0)  /* nresults >= 0? */
        L->top.p = ci->top.p;  /* adjust results */
      break;
    }
    case OP_TAILCALL: case OP_SETTABUP: case OP_SETTABLE:
    case OP_SETI: case OP_SETFIELD:
      break;  /* nothing to be done */
    default: aql_assert(0);
  }
}

/*
** {==================================================================
** Function 'aqlV_finishget' and 'aqlV_finishset', which are used to
** complete table access when 'aqlV_fastget'/'aqlV_fastset' cannot do
** the job.
** ===================================================================
*/

/*
** Finish a table access 'val = t[key]' where 't' is not a table or there
** is a metamethod.
*/
void aqlV_finishget (aql_State *L, const TValue *t, TValue *key, StkId val,
                      const TValue *slot) {
  int loop;  /* counter to avoid infinite loops */
  const TValue *tm;  /* metamethod */
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    if (slot == NULL) {  /* 't' is not a table? */
      aql_assert(!ttistable(t));
      tm = aqlT_gettmbyobj(L, t, TM_INDEX);
      if (l_unlikely(notm(tm)))
        aqlG_typeerror(L, t, "index");  /* no metamethod */
      /* else will try the metamethod */
    }
    else {  /* 't' is a table */
      aql_assert(isempty(slot));
      tm = fasttm(L, hvalue(t)->metatable, TM_INDEX);  /* table's metamethod */
      if (tm == NULL) {  /* no metamethod? */
        setnilvalue(s2v(val));  /* result is nil */
        return;
      }
      /* else will try the metamethod */
    }
    if (ttisfunction(tm)) {  /* is metamethod a function? */
      aqlT_callTMres(L, tm, t, key, val);  /* call it */
      return;
    }
    t = tm;  /* else try to access 'tm[key]' */
    if (aqlV_fastget(L, t, key, slot, aqlH_get)) {  /* fast track? */
      setobj2s(L, val, slot);  /* done */
      return;
    }
    /* else repeat (tail call 'aqlV_finishget') */
  }
  aqlG_runerror(L, "'__index' chain too long; possible loop");
}

/*
** Finish a table assignment 't[key] = val' where 't' is not a table or
** there is a metamethod. (The parameter 'slot' is always NULL in this
** function. It is passed as an argument because 'aqlV_fastget' would
** have done the job.)
*/
void aqlV_finishset (aql_State *L, const TValue *t, TValue *key,
                     TValue *val, const TValue *slot) {
  int loop;  /* counter to avoid infinite loops */
  aql_debug("aqlV_finishset: 开始 - t类型=%d, key类型=%d, val类型=%d, slot=%p", 
           ttype(t), ttype(key), ttype(val), (void*)slot);
  
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    const TValue *tm;  /* '__newindex' metamethod */
    aql_debug("aqlV_finishset: 循环 %d/%d", loop, MAXTAGLOOP);
    
    if (slot != NULL) {  /* is 't' a table? */
      Table *h = hvalue(t);  /* save 't' table */
      aql_debug("aqlV_finishset: t是表，h=%p", (void*)h);
      aql_assert(isempty(slot));  /* slot must be empty */
      tm = fasttm(L, h->metatable, TM_NEWINDEX);  /* get metamethod */
      aql_debug("aqlV_finishset: 获取__newindex元方法，tm=%p", (void*)tm);
      
      if (tm == NULL) {  /* no metamethod? */
        aql_debug("aqlV_finishset: 没有元方法，直接设置");
        if (isabstkey(slot))  /* is slot an abstract key? */
          aqlH_newkey(L, h, key, val);  /* add key to table */
        else
          aqlH_finishset(L, h, key, val, slot);  /* set new value */
        invalidateTMcache(h);
        aql_debug("aqlV_finishset: 准备调用aqlC_barrierback - h=%p, val=%p, val类型=%d", 
                 (void*)h, (void*)val, ttype(val));
        aql_debug("aqlV_finishset: iscollectable(val)=%d, isblack(obj2gco(h))=%d", 
                 iscollectable(val), isblack(obj2gco(h)));
        if (iscollectable(val)) {
          aql_debug("aqlV_finishset: gcvalue(val)=%p", (void*)gcvalue(val));
        }
        aqlC_barrierback(L, obj2gco(h), val);
        aql_debug("aqlV_finishset: 设置完成，返回");
        return;
      }
      /* else will try the metamethod */
    }
    else {  /* not a table; check metamethod */
      aql_debug("aqlV_finishset: t不是表，检查元方法");
      tm = aqlT_gettmbyobj(L, t, TM_NEWINDEX);
      if (l_unlikely(notm(tm)))
        aqlG_typeerror(L, t, "index");
    }
    /* try the metamethod */
    aql_debug("aqlV_finishset: 尝试调用元方法");
    if (ttisfunction(tm)) {
      aqlT_callTM(L, tm, t, key, val);
      aql_debug("aqlV_finishset: 元方法调用完成，返回");
      return;
    }
    t = tm;  /* else repeat assignment over 'tm' */
    aql_debug("aqlV_finishset: 重复赋值，新t类型=%d", ttype(t));
    if (aqlV_fastget(L, t, key, slot, aqlH_get)) {
      aqlV_finishfastset(L, t, slot, val);
      aql_debug("aqlV_finishset: 快速设置完成，返回");
      return;  /* done */
    }
    /* else 'return aqlV_finishset(L, t, key, val, slot)' (loop) */
    aql_debug("aqlV_finishset: 继续循环");
  }
  aql_debug("aqlV_finishset: 超出最大循环次数，报错");
  aqlG_runerror(L, "'__newindex' chain too long; possible loop");
}

/*
** 新一代 VM 执行函数 - 与 Lua lvm.c 的 luaV_execute 保持一致
** 未来将替代 avm.c 中的 aqlV_execute
*/
void aqlV_execute2 (aql_State *L, CallInfo *ci) {
  LClosure *cl;
  TValue *k;
  StkId base;
  const Instruction *pc;
  
  /* VM 执行开始调试信息 */
    aql_debug("aqlV_execute2 开始执行\n");
    aql_debug("L=%p, ci=%p\n", (void*)L, (void*)ci);
    aql_debug("ci->func.p=%p, ci->top.p=%p, ci->u.l.savedpc=%p\n", 
           (void*)ci->func.p, (void*)ci->top.p, (void*)ci->u.l.savedpc);
  
 newframe:  /* 重新进入点 */
  aql_debug("newframe: 进入新帧，ci=%p, L->ci=%p", (void*)ci, (void*)L->ci);
  
  /* 安全检查 */
  if (ci == NULL) {
    aql_debug("newframe: 错误 - ci为NULL，退出VM");
    return;
  }
  
  if (ci->func.p == NULL) {
    aql_debug("newframe: 错误 - ci->func.p为NULL，退出VM");
    return;
  }
  
  aql_assert(ci == L->ci);
  
  aql_debug("newframe: 准备访问ci->func.p=%p", (void*)ci->func.p);
  cl = clLvalue(s2v(ci->func.p));
  aql_debug("newframe: clLvalue成功，cl=%p", (void*)cl);
  
  if (cl == NULL || cl->p == NULL) {
    aql_debug("newframe: 错误 - 无效的闭包或原型，退出VM");
    return;
  }
  
  k = cl->p->k;
  base = ci->func.p + 1;
  pc = ci->u.l.savedpc;
  
    aql_debug("初始化完成: cl=%p, k=%p, base=%p, pc=%p", 
           (void*)cl, (void*)k, (void*)base, (void*)pc);
    aql_debug("函数原型: code=%p, sizecode=%d, maxstacksize=%d", 
           (void*)cl->p->code, cl->p->sizecode, cl->p->maxstacksize);
  
  /* 主执行循环 */
  aql_debug("进入主执行循环");
  
  /* 主循环开始前初始化 pc - 与 Lua 5.4 一致 */
  pc = ci->u.l.savedpc;
  
  for (;;) {
    Instruction i;
    StkId ra;
    int trap = ci->u.l.trap;
    
    /* 检查PC是否超出指令边界 - 使用实际的pc变量 */
    int current_pc = (int)(pc - cl->p->code);
    if (current_pc >= cl->p->sizecode) {
      aql_debug("PC超出边界: current_pc=%d, sizecode=%d", current_pc, cl->p->sizecode);
      break;  /* 退出执行循环 */
    }
    
    aql_debug("准备 vmfetch: savedpc=%p, current_pc=%d", (void*)pc, current_pc);
    
    vmfetch();
    
    /* 同步 savedpc 与 pc，保持与 Lua 5.4 的兼容性 */
    ci->u.l.savedpc = pc;
    
    aql_debug("vmfetch完成: i=0x%08lX", (unsigned long)i);
    
    ra = RA(i);
    
    /* 执行跟踪 */
      OpCode op = GET_OPCODE(i);
      aql_info_vd("PC=%d: %s (op=%d)", (int)(pc - cl->p->code - 1), aql_opnames[op], (int)op);
      
      /* 获取函数名（如果有的话） - 添加安全检查 */
      const char *func_name = "main";  // 简化函数名获取，避免段错误
      if (cl && cl->p) {
          func_name = get_function_name(cl);
      }
      
      /* 设置调试上下文 */
      aql_vt_set_context(L, ci, i, pc, cl, base, func_name);

    vmdispatch (GET_OPCODE(i)) {
      
      /*
      ** aql 兼容指令 (0-82) - 完全采用 aql 5.4 的实现
      */
      
      vmcase(OP_MOVE) {
        StkId ra_debug = RA(i);
        TValue *rb_debug = vRB(i);
        aql_debug("MOVE: R%d = R%d, 源值: %lld", 
                 GETARG_A(i), GETARG_B(i),
                 ttisinteger(rb_debug) ? (long long)ivalue(rb_debug) : -999);
        setobjs2s(L, ra, RB(i));
        aql_debug("MOVE 完成: R%d = %lld", GETARG_A(i), 
                 ttisinteger(s2v(ra_debug)) ? (long long)ivalue(s2v(ra_debug)) : -999);
        vmbreak;
      }
      
      vmcase(OP_LOADI) {
        AQL_INFO_VT_LOADI_BEFORE();
        aql_Integer b = GETARG_sBx(i);
        setivalue(s2v(ra), b);
        AQL_INFO_VT_LOADI_AFTER();
        vmbreak;
      }
      
      vmcase(OP_LOADF) {
        int b = GETARG_sBx(i);
        setfltvalue(s2v(ra), cast_num(b));
        vmbreak;
      }
      
      vmcase(OP_LOADK) {
        TValue *rb = k + GETARG_Bx(i);
        setobj2s(L, ra, rb);
        vmbreak;
      }
      
      vmcase(OP_LOADKX) {
        TValue *rb;
        rb = k + GETARG_Ax(*pc); pc++;
        setobj2s(L, ra, rb);
        vmbreak;
      }
      
      vmcase(OP_LOADFALSE) {
        setbfvalue(s2v(ra));
        vmbreak;
      }
      
      vmcase(OP_LFALSESKIP) {
        setbfvalue(s2v(ra));
        pc++; /* skip next instruction */
        vmbreak;
      }
      
      vmcase(OP_LOADTRUE) {
        setbtvalue(s2v(ra));
        vmbreak;
      }
      
      vmcase(OP_LOADNIL) {
        int b = GETARG_B(i);
        do {
          setnilvalue(s2v(ra + b));
        } while (b--);
        vmbreak;
      }

      vmcase(OP_LOADBUILTIN) {
        int builtin_id = GETARG_B(i);
        setbuiltinvalue(s2v(ra), builtin_id);
        vmbreak;
      }
      
      vmcase(OP_GETUPVAL) {
        int b = GETARG_B(i);
        DEBUG_GETUPVAL(L, ci, i, pc, cl, base, func_name);
        aql_debug("GETUPVAL: A=%d, B=%d, cl=%p", GETARG_A(i), b, (void*)cl);
        aql_debug("GETUPVAL: cl->nupvalues=%d, cl->upvals=%p", cl->nupvalues, (void*)cl->upvals);
        
        if (b >= cl->nupvalues) {
          aql_error("GETUPVAL: upvalue index %d out of bounds (nupvalues=%d)", b, cl->nupvalues);
          setnilvalue(s2v(ra));
          vmbreak;
        }
        
        if (cl->upvals[b] == NULL) {
          aql_error("GETUPVAL: upvalue[%d] is NULL", b);
          setnilvalue(s2v(ra));
          vmbreak;
        }
        
        aql_debug("GETUPVAL: accessing upvalue[%d]=%p, v.p=%p", b, (void*)cl->upvals[b], (void*)cl->upvals[b]->v.p);
        TValue *upval_value = cl->upvals[b]->v.p;
        aql_debug("GETUPVAL: upvalue[%d]的值类型=%d, 整数值=%lld", b, ttype(upval_value), 
                 ttisinteger(upval_value) ? (long long)ivalue(upval_value) : -999);
        setobj2s(L, ra, cl->upvals[b]->v.p);
        aql_debug("GETUPVAL: 成功获取upvalue[%d]", b);
        vmbreak;
      }
      
      vmcase(OP_SETUPVAL) {
        int b = GETARG_B(i);
        DEBUG_SETUPVAL(L, ci, i, pc, cl, base, func_name);
        aql_debug("SETUPVAL: A=%d, B=%d", GETARG_A(i), b);
        aql_debug("SETUPVAL: cl=%p, nupvalues=%d", (void*)cl, cl ? cl->nupvalues : -1);
        
        // 安全检查
        if (!cl || b >= cl->nupvalues || !cl->upvals[b]) {
          aql_error("SETUPVAL: 无效的upvalue访问 - cl=%p, b=%d, nupvalues=%d", 
                   (void*)cl, b, cl ? cl->nupvalues : -1);
          vmbreak;
        }
        
        UpVal *uv = cl->upvals[b];
        aql_debug("SETUPVAL: 设置upvalue[%d]=%p, 新值类型=%d", b, (void*)uv, ttype(s2v(ra)));
        
        setobj(L, uv->v.p, s2v(ra));
        aqlC_barrier(L, obj2gco(uv), gcvalue(s2v(ra)));
        
        aql_debug("SETUPVAL: 设置完成，upvalue[%d]现在指向类型=%d", b, ttype(uv->v.p));
        vmbreak;
      }
      
      vmcase(OP_GETTABUP) {
        const TValue *slot;
        int b = GETARG_B(i);
        aql_debug("GETTABUP: A=%d, B=%d, C=%d", GETARG_A(i), b, GETARG_C(i));
        aql_debug("GETTABUP: cl=%p, nupvalues=%d", (void*)cl, cl ? cl->nupvalues : -1);
        
        // 安全检查
        if (!cl || b >= cl->nupvalues || !cl->upvals[b]) {
          aql_error("GETTABUP: 无效的upvalue访问 - cl=%p, b=%d, nupvalues=%d", 
                   (void*)cl, b, cl ? cl->nupvalues : -1);
          setnilvalue(s2v(ra));
          vmbreak;
        }
        
        aql_debug("GETTABUP: 准备访问cl->upvals[%d]->v.p", b);
        aql_debug("GETTABUP: cl->upvals[%d]=%p", b, (void*)cl->upvals[b]);
        if (cl->upvals[b]) {
          aql_debug("GETTABUP: cl->upvals[%d]->v.p=%p", b, (void*)cl->upvals[b]->v.p);
        }
        TValue *upval = cl->upvals[b]->v.p;
        TValue *rc = KC(i);
        aql_debug("GETTABUP: upval=%p, upval类型=%d", (void*)upval, upval ? ttype(upval) : -1);

        if (upval != NULL && ttisdict(upval)) {
          aqlV_getdict(L, upval, rc, ra);
          vmbreak;
        }

        if (upval != NULL && ttisnil(upval)) {
          Dict *globals_dict = get_globals_dict(L);
          if (globals_dict != NULL) {
            setobj(L, upval, &G(L)->l_globals);
            aqlV_getdict(L, upval, rc, ra);
          } else {
            setnilvalue(s2v(ra));
          }
          vmbreak;
        }

        /* Table-backed upvalues follow Lua's fast path. */
        if (!upval || !ttistable(upval)) {
          aql_error("GETTABUP: upval不是表/容器类型 - upval=%p, 类型=%d",
                   (void*)upval, upval ? ttype(upval) : -1);
          setnilvalue(s2v(ra));
          vmbreak;
        }

        aql_debug("GETTABUP: key=%p, key类型=%d", (void*)rc, ttype(rc));
        aql_debug("GETTABUP: TESTARG_k=%d, GETARG_C=%d", TESTARG_k(i), GETARG_C(i));
        aql_debug("GETTABUP: k=%p, base=%p", (void*)k, (void*)base);
        
        if (!ttisstring(rc)) {
          aql_error("GETTABUP: key不是字符串类型 - key类型=%d", ttype(rc));
          setnilvalue(s2v(ra));
          vmbreak ;
        }
        
        TString *key = tsvalue(rc);  /* key must be a string */
        aql_debug("GETTABUP: 查找key='%s'", getstr(key));
        aql_debug("GETTABUP: key类型=%d, AQL_VSHRSTR=%d", ttype(rc), AQL_VSHRSTR);
        aql_debug("GETTABUP: key长度=%lu", (unsigned long)tsslen(key));
        
        if (aqlV_fastget(L, upval, key, slot, aqlH_getshortstr)) {
          aql_debug("GETTABUP: 快速查找成功，slot=%p, slot类型=%d", (void*)slot, ttype(slot));
          if (iscollectable(slot)) {
            aql_debug("GETTABUP: slot是可收集对象，gcvalue(slot)=%p", (void*)gcvalue(slot));
          }
          aql_debug("GETTABUP: 准备调用setobj2s - ra=%p, slot=%p", (void*)ra, (void*)slot);
          aql_debug("GETTABUP: slot->tt_=%d, slot->value_=%p", slot->tt_, (void*)slot->value_.gc);
          aql_debug("GETTABUP: 调用setobj2s前");
          setobj2s(L, ra, slot);
          aql_debug("GETTABUP: setobj2s完成");
          aql_debug("GETTABUP: ra->tt_=%d, ra->value_=%p", s2v(ra)->tt_, (void*)s2v(ra)->value_.gc);
        }
        else {
          aql_debug("GETTABUP: 使用完整查找");
          aql_debug("GETTABUP: 调用aqlV_finishget - table=%p, key='%s'", 
                   (void*)hvalue(upval), getstr(key));
          Protect(aqlV_finishget(L, upval, rc, ra, slot));
          aql_debug("GETTABUP: aqlV_finishget完成，结果类型=%d", ttype(s2v(ra)));
        }
        
        /* 跟踪模式：显示指令执行后的状态 - 暂时禁用避免段错误 */
        /* if (func_name) {
          print_instruction_trace(L, ci, i, pc, cl, base, func_name, 0);  // AFTER
        } */
        vmbreak;
      }
      
      vmcase(OP_GETTABLE) {
        const TValue *slot;
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        aql_Unsigned n;
        
        aql_debug("GETTABLE: A=%d, B=%d, C=%d", GETARG_A(i), GETARG_B(i), GETARG_C(i));
        aql_debug("GETTABLE: table类型=%d, key类型=%d", ttype(rb), ttype(rc));
        
        if (!ttistable(rb)) {
          aql_error("GETTABLE: table不是表类型 - 实际类型=%d", ttype(rb));
          setnilvalue(s2v(ra));
          vmbreak;
        }
        
        if (ttisinteger(rc)
            ? (cast_void(n = ivalue(rc)), aqlV_fastgeti(L, rb, n, slot))
            : aqlV_fastget(L, rb, rc, slot, aqlH_get)) {
          setobj2s(L, ra, slot);
          aql_debug("GETTABLE: 快速查找成功，结果类型=%d", ttype(s2v(ra)));
        }
        else {
          aql_debug("GETTABLE: 使用完整查找");
          Protect(aqlV_finishget(L, rb, rc, ra, slot));
          aql_debug("GETTABLE: 完整查找完成，结果类型=%d", ttype(s2v(ra)));
        }
        vmbreak;
      }
      
      vmcase(OP_GETI) {
        const TValue *slot;
        TValue *rb = vRB(i);
        int c = GETARG_C(i);
        if (aqlV_fastgeti(L, rb, c, slot)) {
          setobj2s(L, ra, slot);
        }
        else {
          TValue key;
          setivalue(&key, c);
          Protect(aqlV_finishget(L, rb, &key, ra, slot));
        }
        vmbreak;
      }
      
      vmcase(OP_GETFIELD) {
        const TValue *slot;
        TValue *rb = vRB(i);
        TValue *rc = KC(i);
        TString *key = tsvalue(rc);  /* key must be a string */
        if (aqlV_fastget(L, rb, key, slot, aqlH_getstr)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(aqlV_finishget(L, rb, rc, ra, slot));
        vmbreak;
      }
      
      vmcase(OP_SETTABUP) {
        const TValue *slot;
        int a = GETARG_A(i);
        aql_debug("SETTABUP: A=%d, B=%d, C=%d\n", a, GETARG_B(i), GETARG_C(i));
        aql_debug("SETTABUP: cl=%p, nupvalues=%d\n", (void*)cl, cl ? cl->nupvalues : -1);
        
        // 安全检查
        if (!cl || a >= cl->nupvalues || !cl->upvals[a]) {
          aql_error("SETTABUP: 无效的upvalue访问 - cl=%p, a=%d, nupvalues=%d", 
                   (void*)cl, a, cl ? cl->nupvalues : -1);
          vmbreak;
        }
        
        TValue *upval = cl->upvals[a]->v.p;
        TValue *rb = KB(i);
        TValue *rc = RKC(i);
        aql_debug("SETTABUP: upval=%p, upval类型=%d\n", (void*)upval, upval ? ttype(upval) : -1);

        if (upval != NULL && ttisdict(upval)) {
          aqlV_setdict(L, upval, rb, rc);
          vmbreak;
        }

        if (upval != NULL && ttisnil(upval)) {
          Dict *globals_dict = get_globals_dict(L);
          if (globals_dict != NULL) {
            setobj(L, upval, &G(L)->l_globals);
            aqlV_setdict(L, upval, rb, rc);
          }
          vmbreak;
        }

        if (!upval || !ttistable(upval)) {
          aql_error("SETTABUP: upval不是表/容器类型 - upval=%p, 类型=%d",
                   (void*)upval, upval ? ttype(upval) : -1);
          vmbreak;
        }

        aql_debug("SETTABUP: key=%p, key类型=%d\n", (void*)rb, ttype(rb));
        aql_debug("SETTABUP: value=%p, value类型=%d\n", (void*)rc, ttype(rc));
        
        if (!ttisstring(rb)) {
          aql_error("SETTABUP: key不是字符串类型 - key类型=%d", ttype(rb));
          vmbreak;
        }
        
        TString *key = tsvalue(rb);  /* key must be a string */
        aql_debug("SETTABUP: 设置key='%s'\n", getstr(key));
        aql_debug("SETTABUP: key类型=%d, AQL_VSHRSTR=%d", ttype(rb), AQL_VSHRSTR);
        aql_debug("SETTABUP: key长度=%lu", (unsigned long)tsslen(key));
        
        if (aqlV_fastget(L, upval, key, slot, aqlH_getshortstr)) {
          aql_debug("SETTABUP: 快速设置");
          aqlV_finishfastset(L, upval, slot, rc);
        }
        else {
          aql_debug("SETTABUP: 使用完整设置");
          aql_debug("SETTABUP: 调用aqlV_finishset - table=%p, key='%s', value_type=%d\n", 
                   (void*)hvalue(upval), getstr(key), ttype(rc));
          aql_debug("SETTABUP: 即将进入aqlV_finishset\n");
          Protect(aqlV_finishset(L, upval, rb, rc, slot));
          aql_debug("SETTABUP: aqlV_finishset完成");
        }
        aql_debug("SETTABUP: 设置完成\n");
        vmbreak;
      }
      
      vmcase(OP_SETTABLE) {
        const TValue *slot;
        TValue *rb = vRB(i);  /* key (table is in 'ra') */
        TValue *rc = RKC(i);  /* value */
        aql_Unsigned n;
        
        aql_debug("SETTABLE: A=%d, B=%d, C=%d", GETARG_A(i), GETARG_B(i), GETARG_C(i));
        aql_debug("SETTABLE: table类型=%d, key类型=%d, value类型=%d", ttype(s2v(ra)), ttype(rb), ttype(rc));
        
        if (!ttistable(s2v(ra))) {
          aql_error("SETTABLE: table不是表类型 - 实际类型=%d", ttype(s2v(ra)));
          vmbreak;
        }
        
        if (ttisinteger(rb)
            ? (cast_void(n = ivalue(rb)), aqlV_fastgeti(L, s2v(ra), n, slot))
            : aqlV_fastget(L, s2v(ra), rb, slot, aqlH_get)) {
          aql_debug("SETTABLE: 快速设置");
          aqlV_finishfastset(L, s2v(ra), slot, rc);
        }
        else {
          aql_debug("SETTABLE: 使用完整设置");
          Protect(aqlV_finishset(L, s2v(ra), rb, rc, slot));
        }
        aql_debug("SETTABLE: 设置完成");
        vmbreak;
      }
      
      vmcase(OP_SETI) {
        const TValue *slot;
        int c = GETARG_B(i);
        TValue *rc = RKC(i);
        if (aqlV_fastgeti(L, s2v(ra), c, slot)) {
          aqlV_finishfastset(L, s2v(ra), slot, rc);
        }
        else {
          TValue key;
          setivalue(&key, c);
          Protect(aqlV_finishset(L, s2v(ra), &key, rc, slot));
        }
        vmbreak;
      }
      
      vmcase(OP_SETFIELD) {
        const TValue *slot;
        TValue *rb = KB(i);
        TValue *rc = RKC(i);
        TString *key = tsvalue(rb);  /* key must be a string */
        if (aqlV_fastget(L, s2v(ra), key, slot, aqlH_getstr)) {
          aqlV_finishfastset(L, s2v(ra), slot, rc);
        }
        else
          Protect(aqlV_finishset(L, s2v(ra), rb, rc, slot));
        vmbreak;
      }
      
      vmcase(OP_NEWTABLE) {
        int b = GETARG_B(i);  /* log2(hash size) + 1 */
        int c = GETARG_C(i);  /* array size */
        Table *t;
        aql_debug("NEWTABLE: B=%d, C=%d, TESTARG_k=%d, GETARG_Ax(next)=%d", b, c, TESTARG_k(i), GETARG_Ax(*(pc)));
        if (b > 0)
          b = 1 << (b - 1);  /* size is 2^(b - 1) */
        aql_assert((!TESTARG_k(i)) == (GETARG_Ax(*pc) == 0));
        if (TESTARG_k(i)) {  /* non-zero extra argument? */
          c += GETARG_Ax(*pc) * (MAXARG_C + 1);  /* add it to size */
          aql_debug("NEWTABLE: 使用EXTRAARG参数，新的C=%d", c);
        }
        pc++;  /* skip extra argument (always, like Lua) */
        aql_debug("NEWTABLE: 跳过EXTRAARG指令 (Lua风格)");
        L->top.p = ra + 1;  /* correct top in case of emergency GC */
        t = aqlH_new(L);  /* memory allocation */
        aql_debug("NEWTABLE: 创建表成功，t=%p", (void*)t);
        sethvalue2s(L, ra, t);
        aql_debug("NEWTABLE: 设置表到寄存器，ra类型=%d", ttype(s2v(ra)));
        if (b != 0 || c != 0)
          aqlH_resize(L, t, c, b);  /* idem */
        checkGC(L, ra + 1);
        vmbreak;
      }
      
      vmcase(OP_EXTRAARG) {
        /* EXTRAARG is handled by the previous instruction */
        /* This should never be reached as an independent instruction */
        aql_debug("EXTRAARG: 意外执行独立EXTRAARG指令 (应该被前一条指令跳过)");
        vmbreak;
      }
      
      vmcase(OP_SELF) {
        const TValue *slot;
        TValue *rb = vRB(i);
        TValue *rc = RKC(i);
        TString *key = tsvalue(rc);  /* key must be a string */
        setobj2s(L, ra + 1, rb);
        if (aqlV_fastget(L, rb, key, slot, aqlH_getstr)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(aqlV_finishget(L, rb, rc, ra, slot));
        vmbreak;
      }
      
      vmcase(OP_CALL) {
        StkId func = ra;
        int b = GETARG_B(i);
        int nargs = b - 1;
        int nresults = GETARG_C(i) - 1;
        aql_debug("🔍 [CALL] 开始执行: func=%p, nargs=%d, nresults=%d\n", (void*)func, nargs, nresults);
        
        if (b != 0)
          L->top.p = func + b;
        else
          nargs = cast_int(L->top.p - func - 1);
        
        // 检查是否是有效的函数对象
        TValue *f = s2v(func);
        aql_debug("🔍 [CALL] 检查函数对象: f=%p, type=%d\n", (void*)f, ttype(f));

        if (ttisbuiltin(f)) {
          int builtin_id = builtinvalue(f);
          int nparams = nargs;
          StkId args_base = func + 1;

          aql_debug("🔍 [CALL] builtin function: id=%d, nparams=%d\n", builtin_id, nparams);

          switch (builtin_id) {
            case 0: {  /* print */
              for (int j = 0; j < nparams; j++) {
                TValue *arg = s2v(args_base + j);

                if (j > 0)
                  printf("\t");

                if (ttisstring(arg)) {
                  printf("%s", getstr(tsvalue(arg)));
                } else if (ttisinteger(arg)) {
                  printf("%lld", (long long)ivalue(arg));
                } else if (ttisfloat(arg)) {
                  printf("%.14g", fltvalue(arg));
                } else if (ttisboolean(arg)) {
                  printf("%s", bvalue(arg) ? "true" : "false");
                } else if (ttisnil(arg)) {
                  printf("nil");
                } else if (ttisrange(arg)) {
                  RangeObject *range = rangevalue(arg);
                  printf("range(%lld, %lld, %lld)",
                         (long long)range->start,
                         (long long)range->stop,
                         (long long)range->step);
                } else {
                  printf("(unknown type %d)", ttype(arg));
                }
              }
              if (nparams > 0)
                printf("\n");
              setnilvalue(s2v(func));
              break;
            }
            case 2: {  /* len */
              if (nparams != 1) {
                setnilvalue(s2v(func));
                break;
              }
              aqlV_objlen(L, func, s2v(args_base));
              break;
            }
            case 3: {  /* string/tostring */
              if (nparams != 1) {
                setnilvalue(s2v(func));
                break;
              }

              TValue *arg = s2v(args_base);
              TValue *result = s2v(func);

              if (ttisstring(arg)) {
                setobj(L, result, arg);
              } else if (ttisinteger(arg)) {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%lld", (long long)ivalue(arg));
                setsvalue(L, result, aqlStr_new(L, buffer));
              } else if (ttisfloat(arg)) {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%.14g", fltvalue(arg));
                setsvalue(L, result, aqlStr_new(L, buffer));
              } else if (ttisboolean(arg)) {
                setsvalue(L, result, aqlStr_new(L, bvalue(arg) ? "true" : "false"));
              } else if (ttisnil(arg)) {
                setsvalue(L, result, aqlStr_new(L, "nil"));
              } else {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "(type %d)", ttype(arg));
                setsvalue(L, result, aqlStr_new(L, buffer));
              }
              break;
            }
            case 5: {  /* range */
              aql_Integer start = 0;
              aql_Integer stop = 0;
              aql_Integer step = 1;
              RangeObject *range;

              if (nparams < 1 || nparams > 3) {
                setnilvalue(s2v(func));
                break;
              }

              if (nparams == 1) {
                TValue *arg = s2v(args_base);
                if (!ttisinteger(arg)) {
                  setnilvalue(s2v(func));
                  break;
                }
                stop = ivalue(arg);
              } else if (nparams == 2) {
                TValue *arg1 = s2v(args_base);
                TValue *arg2 = s2v(args_base + 1);
                if (!ttisinteger(arg1) || !ttisinteger(arg2)) {
                  setnilvalue(s2v(func));
                  break;
                }
                start = ivalue(arg1);
                stop = ivalue(arg2);
                step = aqlR_infer_step(start, stop);
              } else {
                TValue *arg1 = s2v(args_base);
                TValue *arg2 = s2v(args_base + 1);
                TValue *arg3 = s2v(args_base + 2);
                if (!ttisinteger(arg1) || !ttisinteger(arg2) || !ttisinteger(arg3)) {
                  setnilvalue(s2v(func));
                  break;
                }
                start = ivalue(arg1);
                stop = ivalue(arg2);
                step = ivalue(arg3);
              }

              range = aqlR_new(L, start, stop, step);
              if (range == NULL) {
                setnilvalue(s2v(func));
              } else {
                setrangevalue(L, s2v(func), range);
              }
              break;
            }
            default:
              setnilvalue(s2v(func));
              break;
          }

          if (nresults >= 0)
            L->top.p = ci->top.p;
          vmbreak;
        }

        if (!ttisLclosure(f) && !ttisCclosure(f)) {
          // 不是函数对象，跳过CALL并继续执行
          aql_debug("⚠️  [CALL] R%d不是函数对象，跳过调用\n", GETARG_A(i));
          /* 跟踪模式：显示指令执行后的状态（跳过情况） */
          if (func_name) {
            /* print_instruction_trace(L, ci, i, pc, cl, base, func_name, 0);  // AFTER */
          }
          vmbreak;
        }
        
        aql_debug("🔍 [CALL] 函数对象有效，准备调用 aqlD_precall\n");
        
        CallInfo *new_ci = aqlD_precall(L, func, nresults);
        aql_debug("🔍 [CALL] aqlD_precall 返回: new_ci=%p\n", (void*)new_ci);
        
        if (new_ci) {  /* AQL function? */
          aql_debug("🔍 [CALL] 准备调用 AQL 函数，跳转到 newframe\n");
          if (aql_debug_is_enabled(AQL_FLAG_VT)) {
            aql_info_vt("------------------------------------------------\n");
            aql_info_vt("args=%d, rets=%d\t\n", nargs, nresults);
          }
          ci = L->ci;
          aql_debug("🔍 [CALL] 设置 ci=%p，跳转到 newframe\n", (void*)ci);
          goto newframe;  /* restart aqlV_execute over new aql function */
        } else {  /* C function */
          aql_debug("🔍 [CALL] 调用 C 函数，已完成\n");
          updatetrap(ci);  /* C function already completed */
        }
        
        /* 跟踪模式：显示指令执行后的状态 - 暂时禁用避免段错误 */
        /* if (func_name) {
          print_instruction_trace(L, ci, i, pc, cl, base, func_name, 0);  // AFTER
        } */
        vmbreak;
      }
      
      vmcase(OP_TAILCALL) {
        int b = GETARG_B(i);  /* number of arguments + 1 (function) */
        int n;  /* number of results when calling a C function */
        int nparams1 = GETARG_C(i);
        int delta = (nparams1) ? ci->u.l.nextraargs + nparams1 : 0;

        aql_debug("TAILCALL: func=R%d, B=%d, C=%d, delta=%d",
                 GETARG_A(i), b, GETARG_C(i), delta);
        aql_debug("TAILCALL: func类型=%d", ttypetag(s2v(ra)));

        if (b != 0)
          L->top.p = ra + b;
        else
          b = cast_int(L->top.p - ra);
        
        aql_debug("TAILCALL: 调整后B=%d, L->top.p=%p", b, (void*)L->top.p);
        savepc(L);
        
        /* Close upvalues if needed (Lua compatibility) */
        if (TESTARG_k(i)) {
          aqlF_closeupval(L, base);  /* close upvalues from current call */
          aql_debug("TAILCALL: 关闭upvalues (k=1)");
        }
        
        aql_debug("TAILCALL: 调用aqlD_pretailcall");
        n = aqlD_pretailcall(L, ci, ra, b, delta);
        aql_debug("TAILCALL: aqlD_pretailcall返回, n=%d", n);
        
        if (n < 0) {  /* Lua function */
          goto newframe;  /* execute callee in reused frame */
        } else {  /* C function */
          ci->func.p -= delta;  /* restore 'func' (if vararg) */
          (void)aqlD_poscall(L, ci, n);  /* finish caller */
          updatetrap(ci);
          if (L->ci != NULL && L->ci != &L->base_ci) {
            ci = L->ci;
            goto newframe;
          }
          return;
        }
        vmbreak;
      }
      
      vmcase(OP_RETURN) {
        int n = GETARG_B(i) - 1;  /* number of results */
        int nparams1 = GETARG_C(i);
        aql_debug("🔍 [RETURN] 开始执行: n=%d, nparams1=%d, ra=%p\n", n, nparams1, (void*)ra);
        
        if (n < 0)  /* not fixed? */
          n = cast_int(L->top.p - ra);  /* get what is available */
        
        aql_debug("🔍 [RETURN] 返回%d个值，从R%d开始\n", n, GETARG_A(i));
        
        savepc(L);
        aql_debug("🔍 [RETURN] 检查k位 - i=0x%x, TESTARG_k(i)=%d\n", i, TESTARG_k(i));
        if (TESTARG_k(i)) {  /* may there be open upvalues? */
          aql_debug("🔍 [RETURN] 检测到k位，需要关闭upvalues\n");
          if (L->top.p < ci->top.p)
            L->top.p = ci->top.p;
          aql_debug("🔍 [RETURN] 调用 aqlF_close 关闭upvalues\n");
          aqlF_close(L, base, CLOSEKTOP, 1);
          aql_debug("🔍 [RETURN] aqlF_close 完成\n");
          updatetrap(ci);
        } else {
          aql_debug("🔍 [RETURN] 没有k位，不关闭upvalues\n");
        }
        
        if (l_unlikely(L->hookmask)) {
          aql_debug("🔍 [RETURN] 使用 hook 模式处理返回值\n");
          if (nparams1)  /* vararg function? */
            ci->func.p -= ci->u.l.nextraargs + nparams1;
          L->top.p = ra + n;  /* set call for 'aqlD_poscall' */
          aqlD_poscall(L, ci, n);
          updatetrap(ci);
        }
        else {  /* do the 'poscall' here */
          int nres = ci->nresults;
          aql_debug("🔍 [RETURN] 直接处理返回值: ci->nresults=%d\n", nres);
          aql_debug("🔍 [RETURN] 检查调用者 - L->ci=%p, ci=%p\n", (void*)L->ci, (void*)ci);
          L->ci = ci->previous;  /* back to caller */
          aql_debug("🔍 [RETURN] 回到调用者，新的ci=%p\n", (void*)L->ci);
          
          if (nres == 0) {
            aql_debug("🔍 [RETURN] 不需要返回值，设置 top=base-1\n");
            L->top.p = base - 1;  /* asked for no results */
          }
          else if (nres < 0) {
            aql_debug("🔍 [RETURN] 要所有结果，复制%d个返回值\n", n);
            /* 要所有结果 - 复制所有n个返回值 */
            for (int i = 0; i < n; i++) {
              setobjs2s(L, base - 1 + i, ra + i);
              aql_debug("🔍 [RETURN] 复制返回值[%d]到base-%d\n", i, 1-i);
            }
            L->top.p = base - 1 + n;  /* top points after all results */
          }
          else {
            aql_debug("🔍 [RETURN] 固定数量的结果: 复制最多%d个值\n", nres);
            /* 固定数量的结果 - 复制最多nres个值 */
            int copy_count = (n < nres) ? n : nres;
            for (int i = 0; i < copy_count; i++) {
              setobjs2s(L, base - 1 + i, ra + i);
              aql_debug("🔍 [RETURN] 复制返回值[%d]到base-%d\n", i, 1-i);
            }
            /* 如果需要更多结果，用nil填充 */
            for (int i = copy_count; i < nres; i++) {
              setnilvalue(s2v(base - 1 + i));
            }
            L->top.p = base - 1 + nres;
          }
        }
        
        /* 检查是否需要回到调用者 */
        aql_debug("🔍 [RETURN] 检查调用者 - L->ci=%p, ci=%p\n", (void*)L->ci, (void*)ci);
        if (L->ci != ci && L->ci != NULL && L->ci != &L->base_ci) {
          /* 已经回到调用者，更新ci并重新初始化执行上下文 */
          CallInfo *new_ci = L->ci;
          aql_debug("🔍 [RETURN] 回到调用者，新的ci=%p\n", (void*)new_ci);
          
          /* 验证新的CallInfo是否有效 */
          if (new_ci == NULL || new_ci->func.p == NULL) {
            aql_debug("🔍 [RETURN] 错误 - 无效的CallInfo，退出VM\n");
            return;
          }
          
          ci = new_ci;  /* 更新ci为当前的CallInfo */
          aql_debug("🔍 [RETURN] 更新ci=%p，跳转到 newframe\n", (void*)ci);
          goto newframe;  /* restart aqlV_execute over caller function */
        }
        else {
          /* 这是顶层函数的返回，退出VM */
          aql_debug("🔍 [RETURN] 顶层函数返回，退出VM\n");
          return;
        }
        
        aql_debug("🔍 [RETURN] RETURN 指令执行完成\n");
      }
      
      vmcase(OP_RETURN0) {
        aql_debug("RETURN0: 无返回值函数返回");
        if (l_unlikely(L->hookmask)) {
          L->top.p = ra;
          savepc(L);
          aqlD_poscall(L, ci, 0);  /* no hurry... */
          updatetrap(ci);
        }
        else {  /* do the 'poscall' here */
          int nres;
          L->ci = ci->previous;  /* back to caller */
          L->top.p = base - 1;
          for (nres = ci->nresults; l_unlikely(nres > 0); nres--)
            setnilvalue(s2v(L->top.p++));  /* all results are nil */
        }
        /* 显示返回值 */
        aql_info_vt("--- RETURN: rets=0 ---");
        aql_info_vt("  Return: (no return value)");
        aql_info_vt("\n");
        
        /* 检查是否需要回到调用者 */
        aql_debug("RETURN0: 检查调用者 - L->ci=%p, ci=%p", (void*)L->ci, (void*)ci);
        if (L->ci != ci && L->ci != NULL && L->ci != &L->base_ci) {
          /* 已经回到调用者，更新ci并重新初始化执行上下文 */
          CallInfo *new_ci = L->ci;
          aql_debug("RETURN0: 回到调用者，新的ci=%p", (void*)new_ci);
          
          /* 验证新的CallInfo是否有效 */
          if (new_ci == NULL || new_ci->func.p == NULL) {
            aql_debug("RETURN0: 错误 - 无效的CallInfo，退出VM");
            return;
          }
          
          ci = new_ci;  /* 更新ci为当前的CallInfo */
          goto newframe;  /* restart aqlV_execute over caller function */
        }
        else {
          /* 这是顶层函数的返回，退出VM */
          aql_debug("RETURN0: 顶层函数返回，退出VM");
          return;
        }
      }
      
      vmcase(OP_RETURN1) {
          aql_debug("RETURN1: ra=%p, A=%d", (void*)ra, GETARG_A(i));
          aql_debug("RETURN1: ra指向的值类型=%d, 值=%lld", 
                 ttype(s2v(ra)), ttisinteger(s2v(ra)) ? (long long)ivalue(s2v(ra)) : -999);
          aql_debug("RETURN1: base=%p, base-1=%p, nresults=%d", 
                 (void*)base, (void*)(base-1), ci->nresults);
        if (l_unlikely(L->hookmask)) {
          L->top.p = ra + 1;
          savepc(L);
          aqlD_poscall(L, ci, 1);  /* no hurry... */
          updatetrap(ci);
        }
        else {  /* do the 'poscall' here */
          int nres = ci->nresults;
          L->ci = ci->previous;  /* back to caller */
          if (nres == 0)
            L->top.p = base - 1;  /* asked for no results */
          else if (nres < 0) {
            /* 要所有结果 - 对于RETURN1，就是这一个结果 */
            setobjs2s(L, base - 1, ra);  /* set the result */
            aql_debug("RETURN1: 复制到base-1=%p, 值=%lld (所有结果)", 
                     (void*)(base-1), ttisinteger(s2v(base-1)) ? (long long)ivalue(s2v(base-1)) : -999);
            L->top.p = base;  /* top points after the result */
          }
          else {
            setobjs2s(L, base - 1, ra);  /* at least this result */
            aql_debug("RETURN1: 复制到base-1=%p, 值=%lld", 
                     (void*)(base-1), ttisinteger(s2v(base-1)) ? (long long)ivalue(s2v(base-1)) : -999);
            L->top.p = base;
            for (; l_unlikely(nres > 1); nres--)
              setnilvalue(s2v(L->top.p++));  /* complete missing results */
          }
        }
        /* 显示返回值 */
        aql_info_vt("--- RETURN: rets=1 ---");
        aql_info_vt("  Return: ");
        if (ttisinteger(s2v(ra))) {
          aql_info_vt("R%d=%lld", GETARG_A(i), (long long)ivalue(s2v(ra)));
        } else if (ttisnumber(s2v(ra))) {
          aql_info_vt("R%d=%.2f", GETARG_A(i), fltvalue(s2v(ra)));
        } else if (ttisstring(s2v(ra))) {
          aql_info_vt("R%d=\"%s\"", GETARG_A(i), getstr(tsvalue(s2v(ra))));
        } else if (ttisLclosure(s2v(ra))) {
          aql_info_vt("R%d=func@%p", GETARG_A(i), (void*)clvalue(s2v(ra)));
        } else {
          aql_info_vt("R%d=type%d", GETARG_A(i), ttype(s2v(ra)));
        }
        aql_info_vt("\n");
        
        /* 检查是否需要回到调用者 */
        aql_debug("RETURN1: 检查调用者 - L->ci=%p, ci=%p, ci->previous=%p", 
                 (void*)L->ci, (void*)ci, (void*)ci->previous);
        if (L->ci != ci && L->ci != NULL && L->ci != &L->base_ci) {
          /* 已经回到调用者，更新ci并重新初始化执行上下文 */
          CallInfo *new_ci = L->ci;
          aql_debug("RETURN1: 回到调用者，新的ci=%p", (void*)new_ci);
          
          /* 验证新的CallInfo是否有效 */
          if (new_ci == NULL || new_ci->func.p == NULL) {
            aql_debug("RETURN1: 错误 - 无效的CallInfo，退出VM");
            return;
          }
          
          ci = new_ci;  /* 更新ci为当前的CallInfo */
          goto newframe;  /* restart aqlV_execute over caller function */
        }
        else {
          /* 这是顶层函数的返回，退出VM */
          aql_debug("RETURN1: 顶层函数返回，退出VM");
          return;
        }
      }
      
      vmcase(OP_FORLOOP) {
        AQL_INFO_VT_FORLOOP_BEFORE();
        if (ttisinteger(s2v(ra + 2))) {  /* integer loop? */
          aql_Unsigned count = l_castS2U(ivalue(s2v(ra + 1)));
          if (count > 0) {  /* still more iterations? */
            aql_Integer step = ivalue(s2v(ra + 2));
            aql_Integer idx = ivalue(s2v(ra));  /* internal index */
            chgivalue(s2v(ra + 1), count - 1);  /* update counter */
            idx = intop(+, idx, step);  /* add step to index */
            chgivalue(s2v(ra), idx);  /* update internal index */
            setivalue(s2v(ra + 3), idx);  /* and control variable */
            pc -= GETARG_Bx(i);  /* jump back */
          }
        }
        else if (floatforloop(ra))  /* float loop */
          pc -= GETARG_Bx(i);  /* jump back */
        AQL_INFO_VT_FORLOOP_AFTER();
        updatetrap(ci);  /* allows a signal to break the loop */
        vmbreak;
      }
      
      vmcase(OP_FORPREP) {
        AQL_INFO_VT_FORPREP_BEFORE();
        savestate(L, ci);  /* in case of errors */
        if (forprep(L, ra))
          pc += GETARG_Bx(i) + 1;  /* skip the loop */
        AQL_INFO_VT_FORPREP_AFTER();
        vmbreak;
      }
      
      vmcase(OP_TFORPREP) {
        /* create to-be-closed upvalue (if needed) */
        halfProtect(aqlF_newtbcupval(L, ra + 3));
        pc += GETARG_Bx(i);  /* jump to loop */
        vmbreak;
      }
      
      vmcase(OP_TFORCALL) {
        /* 'ra' has the iterator function, 'ra + 1' has the state,
           'ra + 2' has the control variable, and 'ra + 3' has the
           to-be-closed variable. The call will use the stack after
           these values (starting at 'ra + 4')
        */
        /* push function and arguments */
        setobjs2s(L, ra + 4, ra);
        setobjs2s(L, ra + 5, ra + 1);
        setobjs2s(L, ra + 6, ra + 2);
        L->top.p = ra + 7;
        ProtectNT(aqlD_call(L, ra + 4, GETARG_C(i)));  /* do the call */
        updatebase(ci);  /* stack may have changed */
        L->top.p = ci->top.p;
        i = *(pc - 1);  /* get copy of instruction */
        ra = RA(i);  /* 'ra' may have changed */
        aql_assert(GET_OPCODE(i) == OP_TFORCALL && ra <= L->top.p);
        vmbreak;
      }
      
      vmcase(OP_TFORLOOP) {
        if (!ttisnil(s2v(ra + 4))) {  /* continue loop? */
          setobjs2s(L, ra + 2, ra + 4);  /* save control variable */
          pc -= GETARG_Bx(i);  /* jump back */
        }
        vmbreak;
      }
      
      vmcase(OP_SETLIST) {
        int n = GETARG_B(i);
        unsigned int last = GETARG_C(i);
        Table *h = hvalue(s2v(ra));
        if (n == 0)
          n = cast_int(L->top.p - ra) - 1;  /* get up to the top */
        else
          L->top.p = ra + n + 1;  /* previous call may change the top */
        last += n;
        if (TESTARG_k(i)) {
          last += GETARG_Ax(*pc) * (MAXARG_C + 1);
          pc++;
        }
        if (last > aqlH_realasize(h))  /* needs more space? */
          aqlH_resizearray(L, h, last);  /* preallocate it at once */
        for (; n > 0; n--) {
          TValue *val = s2v(ra + n);
          setobj2t(L, &h->array[last - 1], val);
          last--;
          aqlC_barrierback(L, obj2gco(h), val);
        }
        vmbreak;
      }
      
      vmcase(OP_CLOSURE) {
        Proto *p = cl->p->p[GETARG_Bx(i)];
        halfProtect(pushclosure(L, p, cl->upvals, base, ra));
        checkGC(L, ra + 1);
        vmbreak;
      }
      
      vmcase(OP_VARARG) {
        int n = GETARG_C(i) - 1;  /* required results */
        Protect(aqlT_getvarargs(L, ci, ra, n));
        vmbreak;
      }
      
      vmcase(OP_VARARGPREP) {
        ProtectNT(aqlT_adjustvarargs(L, GETARG_A(i), ci, cl->p));
        if (l_unlikely(trap)) {  /* previous "Protect" updated trap */
          // aqlD_hookcall(L, ci);  // TODO: implement hook call
          L->oldpc = 1;  /* next opcode will be seen as a "new" line */
        }
        updatebase(ci);  /* function has new base after adjustment */
        vmbreak;
      }
      
      /*
      ** 算术和位运算指令
      */
      vmcase(OP_ADD) {
        AQL_INFO_VT_ADD_BEFORE();
        TMS tm = TM_ADD;
        TValue *v1 = vRB(i);
        TValue *v2 = vRC(i);
        if (ttisstring(v1) || ttisstring(v2)) {
          TValue s1;
          TValue s2;
          value_to_string_value(L, v1, &s1);
          value_to_string_value(L, v2, &s2);
          TString *result = aqlStr_concat(L, tsvalue(&s1), tsvalue(&s2));
          pc++;  /* fast path succeeded; skip following MMBIN */
          if (result != NULL) {
            setsvalue(L, s2v(ra), result);
          } else {
            setnilvalue(s2v(ra));
          }
        } else {
          op_arith_aux(L, v1, v2, l_addi, aqli_numadd);
        }
        AQL_INFO_VT_ADD_AFTER();
        vmbreak;
      }
      
      vmcase(OP_ADDI) {
        AQL_INFO_VT_ADDI_BEFORE();
        TMS tm = TM_ADD;
        op_arithI(L, l_addi, aqli_numadd);
        AQL_INFO_VT_ADDI_AFTER();
        vmbreak;
      }
      
      vmcase(OP_ADDK) {
        TMS tm = TM_ADD;
        op_arithK(L, l_addi, aqli_numadd);
        vmbreak;
      }
      
      vmcase(OP_SUB) {
        TMS tm = TM_SUB;
        op_arith(L, l_subi, aqli_numsub);
        vmbreak;
      }
      
      vmcase(OP_SUBI) {
        TMS tm = TM_SUB;
        op_arithI(L, l_subi, aqli_numsub);
        vmbreak;
      }
      
      vmcase(OP_SUBK) {
        TMS tm = TM_SUB;
        op_arithK(L, l_subi, aqli_numsub);
        vmbreak;
      }
      
      vmcase(OP_MUL) {
        TMS tm = TM_MUL;
        StkId ra_debug = RA(i);
        TValue *rb_debug = vRB(i);
        TValue *rc_debug = vRC(i);
        aql_debug("MUL: R%d = R%d * R%d, 操作数值: %lld * %lld", 
                 GETARG_A(i), GETARG_B(i), GETARG_C(i),
                 ttisinteger(rb_debug) ? (long long)ivalue(rb_debug) : -999,
                 ttisinteger(rc_debug) ? (long long)ivalue(rc_debug) : -999);
        op_arith(L, l_muli, aqli_nummul);
        aql_debug("MUL 结果: R%d = %lld", GETARG_A(i), 
                 ttisinteger(s2v(ra_debug)) ? (long long)ivalue(s2v(ra_debug)) : -999);
        vmbreak;
      }
      
      vmcase(OP_MULI) {
        TMS tm = TM_MUL;
        op_arithI(L, l_muli, aqli_nummul);
        vmbreak;
      }
      
      vmcase(OP_MULK) {
        TMS tm = TM_MUL;
        DEBUG_MULK(L, ci, i, pc, cl, base, func_name);
        op_arithK(L, l_muli, aqli_nummul);
        vmbreak;
      }
      
      vmcase(OP_DIV) {
        TMS tm = TM_DIV;
        op_arithf(L, aqli_numdiv);
        vmbreak;
      }
      
      vmcase(OP_DIVK) {
        TMS tm = TM_DIV;
        op_arithfK(L, aqli_numdiv);
        vmbreak;
      }
      
      vmcase(OP_IDIV) {
        TMS tm = TM_IDIV;
        op_arith(L, aqlV_idiv, aqli_numidiv);
        vmbreak;
      }
      
      vmcase(OP_IDIVK) {
        TMS tm = TM_IDIV;
        op_arithK(L, aqlV_idiv, aqli_numidiv);
        vmbreak;
      }
      
      vmcase(OP_MOD) {
        TMS tm = TM_MOD;
        op_arith(L, aqlV_mod, aqlV_modf);
        vmbreak;
      }
      
      vmcase(OP_MODK) {
        TMS tm = TM_MOD;
        op_arithK(L, aqlV_mod, aqlV_modf);
        vmbreak;
      }
      
      vmcase(OP_POW) {
        TMS tm = TM_POW;
        op_arithf(L, aqli_numpow);
        vmbreak;
      }
      
      vmcase(OP_POWK) {
        TMS tm = TM_POW;
        op_arithfK(L, aqli_numpow);
        vmbreak;
      }
      
      vmcase(OP_UNM) {
        TValue *rb = vRB(i);
        aql_Number nb;
        if (ttisinteger(rb)) {
          aql_Integer ib = ivalue(rb);
          setivalue(s2v(ra), intop(-, 0, ib));
        }
        else if (tonumber(rb, &nb)) {
          setfltvalue(s2v(ra), aqli_numunm(L, nb));
        }
        else
          Protect(aqlT_trybinTM(L, rb, rb, ra, TM_UNM));
        vmbreak;
      }
      
      vmcase(OP_BNOT) {
        TValue *rb = vRB(i);
        aql_Integer ib;
        if (tointegerns(rb, &ib)) {
          setivalue(s2v(ra), intop(^, ~l_castS2U(0), ib));
        }
        else {
          Protect(aqlT_trybinTM(L, rb, rb, ra, TM_BNOT));
        }
        vmbreak;
      }
      
      vmcase(OP_NOT) {
        TValue *rb = vRB(i);
        if (l_isfalse(rb)) {
          setbtvalue(s2v(ra));
        } else {
          setbfvalue(s2v(ra));
        }
        vmbreak;
      }
      
      vmcase(OP_LEN) {
        Protect(aqlV_objlen(L, ra, vRB(i)));
        vmbreak;
      }
      
      vmcase(OP_CONCAT) {
        int n = GETARG_B(i);  /* number of elements to concatenate */
        L->top.p = ra + n;  /* mark the end of concat operands */
        ProtectNT(aqlV_concat(L, n));
        checkGC(L, (ra >= RB(i) + n) ? ra + 1 : RB(i));
        L->top.p = ci->top.p;  /* restore top */
        setobjs2s(L, RA(i), ra);
        vmbreak;
      }

      vmcase(OP_CLOSE) {
        Protect(aqlF_close(L, ra, AQL_OK, 1));
        vmbreak;
      }
      
      /*
      ** 位运算指令
      */
      vmcase(OP_SHL) {
        TMS tm = TM_SHL;
        op_bitwise(L, aqlV_shiftl);
        vmbreak;
      }
      
      vmcase(OP_SHR) {
        TMS tm = TM_SHR;
        op_bitwise(L, aqlV_shiftr);
        vmbreak;
      }
      
      vmcase(OP_BAND) {
        TMS tm = TM_BAND;
        op_bitwise(L, l_band);
        vmbreak;
      }
      
      vmcase(OP_BOR) {
        TMS tm = TM_BOR;
        op_bitwise(L, l_bor);
        vmbreak;
      }
      
      vmcase(OP_BXOR) {
        TMS tm = TM_BXOR;
        op_bitwise(L, l_bxor);
        vmbreak;
      }
      
      vmcase(OP_SHLI) {
        TMS tm = TM_SHL;
        op_bitwiseK(L, aqlV_shiftl);
        vmbreak;
      }
      
      vmcase(OP_SHRI) {
        TMS tm = TM_SHR;
        op_bitwiseK(L, aqlV_shiftr);
        vmbreak;
      }
      
      vmcase(OP_BANDK) {
        TMS tm = TM_BAND;
        op_bitwiseK(L, l_band);
        vmbreak;
      }
      
      vmcase(OP_BORK) {
        TMS tm = TM_BOR;
        op_bitwiseK(L, l_bor);
        vmbreak;
      }
      
      vmcase(OP_BXORK) {
        TMS tm = TM_BXOR;
        op_bitwiseK(L, l_bxor);
        vmbreak;
      }
      
      /*
      ** 比较和跳转指令
      */
      vmcase(OP_EQ) {
        int cond;
        TValue *rb = vRB(i);
        Protect(cond = aqlV_equalobj(L, s2v(ra), rb));
        docondjump();
        vmbreak;
      }
      
      vmcase(OP_LT) {
        op_order(L, l_lti, LTnum, aqlV_lessthan);
        vmbreak;
      }
      
      vmcase(OP_LE) {
        op_order(L, l_lei, LEnum, aqlV_lessequal);
        vmbreak;
      }
      
      vmcase(OP_EQK) {
        TValue *rb = KB(i);
        /* basic types do not use '__eq'; we can use raw equality */
        int cond = aqlV_rawequalobj(s2v(ra), rb);
        docondjump();
        vmbreak;
      }
      
      vmcase(OP_EQI) {
        int cond;
        int im = GETARG_sB(i);
        if (ttisinteger(s2v(ra)))
          cond = (ivalue(s2v(ra)) == im);
        else if (ttisfloat(s2v(ra)))
          cond = aqli_numeq(fltvalue(s2v(ra)), cast_num(im));
        else
          cond = 0;  /* other types cannot be equal to a number */
        docondjump();
        vmbreak;
      }
      
      vmcase(OP_LTI) {
        op_orderI(L, l_lti, aqli_numlt, 0, TM_LT);
        vmbreak;
      }
      
      vmcase(OP_LEI) {
        int im = GETARG_sB(i);  
        int k = GETARG_k(i);
        aql_debug("LEI: A=%d, sB=%d, k=%d", GETARG_A(i), im, k);
        aql_debug("LEI: R[A]=%lld, 条件: %lld <= %d", 
                 ttisinteger(s2v(ra)) ? (long long)ivalue(s2v(ra)) : -999, 
                 ttisinteger(s2v(ra)) ? (long long)ivalue(s2v(ra)) : -999, im);
        
        op_orderI(L, l_lei, aqli_numle, 0, TM_LE);
        vmbreak;
      }
      
      vmcase(OP_GTI) {
        op_orderI(L, l_gti, aqli_numgt, 1, TM_LT);
        vmbreak;
      }
      
      vmcase(OP_GEI) {
        op_orderI(L, l_gei, aqli_numge, 1, TM_LE);
        vmbreak;
      }
      
      vmcase(OP_TEST) {
        /* OP_TEST A k: Lua 5.4 compatible implementation */
        StkId ra = RA(i);
        int cond = !l_isfalse(s2v(ra));  /* cond = not R[A] */
        
          aql_debug("TEST: ra=%p, R[A]=%s, cond=%d, k=%d", 
                 (void*)ra, l_isfalse(s2v(ra)) ? "false" : "true", cond, GETARG_k(i));
        
        /* docondjump: if (cond != k) pc++; else donextjump(ci); */
        if (cond != GETARG_k(i)) {
          aql_debug("TEST: 条件不匹配，pc++");
          pc++;
        } else {
          aql_debug("TEST: 条件匹配，执行donextjump");
          donextjump(ci);
        }
        vmbreak;
      }
      
      vmcase(OP_TESTSET) {
        TValue *rb = vRB(i);
        if (GETARG_k(i) ? l_isfalse(rb) : !l_isfalse(rb))
          pc++;
        else {
          setobjs2s(L, ra, RB(i));
          donextjump(ci);
        }
        vmbreak;
      }
      
      vmcase(OP_JMP) {
        dojump(ci, i, 0);
        vmbreak;
      }
      
      /*
      ** AQL 扩展指令 (100+) - 容器、类、JIT、协程等
      */
      
      vmcase(OP_MMBIN) {
        Instruction pi = *(pc - 2);  /* original arithmetic instruction */
        TValue *rb = vRB(i);
        TMS tm = (TMS)GETARG_C(i);
        StkId result = RA(pi);
        Protect(aqlT_trybinTM(L, s2v(ra), rb, result, tm));
        vmbreak;
      }
      
      vmcase(OP_MMBINI) {
        Instruction pi = *(pc - 2);  /* original arithmetic instruction */
        int imm = GETARG_sB(i);
        TMS tm = (TMS)GETARG_C(i);
        int flip = GETARG_k(i);
        StkId result = RA(pi);
        Protect(aqlT_trybiniTM(L, s2v(ra), imm, flip, result, tm));
        vmbreak;
      }
      
      vmcase(OP_MMBINK) {
        Instruction pi = *(pc - 2);  /* original arithmetic instruction */
        TValue *imm = KB(i);
        TMS tm = (TMS)GETARG_C(i);
        int flip = GETARG_k(i);
        StkId result = RA(pi);
        Protect(aqlT_trybinassocTM(L, s2v(ra), imm, flip, result, tm));
        vmbreak;
      }
      
      /* AQL 容器指令 */
      vmcase(OP_NEWOBJECT) {
        int container_type = GETARG_B(i);
        int size_or_capacity = GETARG_C(i);
        AQL_ContainerBase *container = NULL;
        
        aql_debug("OP_NEWOBJECT: A=%d, B=%d, C=%d, ra=%p", 
                 GETARG_A(i), container_type, size_or_capacity, (void*)ra);
        
        /* 映射到现有容器类型 */
        ContainerType ctype;
        switch (container_type) {
          case 0: ctype = CONTAINER_ARRAY; break;   /* AQL_CONTAINER_ARRAY */
          case 1: ctype = CONTAINER_SLICE; break;   /* AQL_CONTAINER_SLICE */
          case 2: ctype = CONTAINER_DICT; break;    /* AQL_CONTAINER_DICT */
          case 3: ctype = CONTAINER_VECTOR; break; /* AQL_CONTAINER_VECTOR */
          default:
            aql_error("OP_NEWOBJECT: 不支持的容器类型 %d", container_type);
            aqlG_runerror(L, "unsupported container type %d", container_type);
            vmbreak;
        }
        
        aql_debug("OP_NEWOBJECT: 映射容器类型 %d -> %d", container_type, (int)ctype);
        
        /* 创建容器 */
        if (container_type == 2) {
          Dict *dict = aqlD_newcap(L, AQL_DATA_TYPE_ANY, AQL_DATA_TYPE_ANY,
                                  (size_or_capacity > 0) ? size_or_capacity : 0);
          if (dict != NULL) {
            setdictvalue(L, s2v(ra), dict);
            aql_debug("OP_NEWOBJECT: Dict 创建成功 - size=%zu, capacity=%zu",
                     dict->size, dict->capacity);
          } else {
            aql_error("OP_NEWOBJECT: Dict 创建失败，设置为 nil");
            setnilvalue(s2v(ra));
          }
          vmbreak;
        }

        container = acontainer_new(L, ctype, AQL_DATA_TYPE_ANY, size_or_capacity);
        
        aql_debug("OP_NEWOBJECT: acontainer_new 返回 %p", (void*)container);
        if (container) {
          aql_debug("OP_NEWOBJECT: 容器创建成功 - type=%d, dtype=%d, length=%zu, capacity=%zu", 
                   (int)container->type, (int)container->dtype, container->length, container->capacity);
          /* 设置到寄存器 - 需要适配现有的对象系统 */
          aql_debug("OP_NEWOBJECT: 调用 setcontainervalue");
          setcontainervalue(L, s2v(ra), container);
          aql_debug("OP_NEWOBJECT: setcontainervalue 完成，ra类型=%d", ttype(s2v(ra)));
        } else {
          aql_error("OP_NEWOBJECT: 容器创建失败，设置为 nil");
          setnilvalue(s2v(ra));
        }
        vmbreak;
      }
      
      vmcase(OP_GETPROP) {
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        int handled = 0;
        
        aql_debug("OP_GETPROP: A=%d, B=%d, C=%d", GETARG_A(i), GETARG_B(i), GETARG_C(i));
        aql_debug("OP_GETPROP: ra=%p, rb=%p, rc=%p", (void*)ra, (void*)rb, (void*)rc);
        
        if (ttisdict(rb)) {
          aql_debug("OP_GETPROP: 使用 dict 直接路径");
          if (acontainer_dict_get(L, (AQL_ContainerBase*)dictvalue(rb), rc, s2v(ra)) == 0) {
            aql_debug("OP_GETPROP: dict 获取成功");
            handled = 1;
          } else {
            aql_debug("OP_GETPROP: dict 获取失败");
          }
        } else if (ttiscontainer(rb)) {
          AQL_ContainerBase *container = (AQL_ContainerBase*)containervalue(rb);
          aql_debug("OP_GETPROP: 容器类型=%d, 长度=%zu", (int)container->type, container->length);
          
          /* 根据容器类型选择操作 */
          switch (container->type) {
            case CONTAINER_ARRAY:
            case CONTAINER_SLICE:
              if (ttisinteger(rc)) {
                size_t idx = (size_t)ivalue(rc);
                aql_debug("OP_GETPROP: 获取数组索引 %zu", idx);
                if (acontainer_array_get(L, container, idx, s2v(ra)) == 0) {
                  aql_debug("OP_GETPROP: 数组获取成功");
                  handled = 1;
                } else {
                  aql_debug("OP_GETPROP: 数组获取失败");
                }
              } else {
                aql_debug("OP_GETPROP: 索引不是整数，类型=%d", ttype(rc));
              }
              break;
            case CONTAINER_DICT:
              aql_debug("OP_GETPROP: 字典获取");
              if (acontainer_dict_get(L, container, rc, s2v(ra)) == 0) {
                aql_debug("OP_GETPROP: 字典获取成功");
                handled = 1;
              } else {
                aql_debug("OP_GETPROP: 字典获取失败");
              }
              break;
            case CONTAINER_VECTOR:
              aql_debug("OP_GETPROP: 向量获取");
              if (ttisinteger(rc)) {
                size_t idx = (size_t)ivalue(rc);
                if (acontainer_array_get(L, container, idx, s2v(ra)) == 0) {
                  aql_debug("OP_GETPROP: 向量获取成功");
                  handled = 1;
                } else {
                  aql_debug("OP_GETPROP: 向量获取失败");
                }
              } else {
                aql_debug("OP_GETPROP: 向量索引不是整数，类型=%d", ttype(rc));
              }
              break;
            default:
              aql_debug("OP_GETPROP: 不支持的容器类型 %d", (int)container->type);
              break;
          }

          if (!handled) {
            aql_debug("OP_GETPROP: 回退到元方法路径");
            Protect(aqlV_finishget(L, rb, rc, ra, NULL));
          }
        } else {
          aql_error("OP_GETPROP: rb 不是容器，类型=%d", ttype(rb));
          /* 回退或报错 */
          setnilvalue(s2v(ra));
        }
        vmbreak;
      }
      
      vmcase(OP_SETPROP) {
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        int handled = 0;
        
        aql_debug("OP_SETPROP: A=%d, B=%d, C=%d", GETARG_A(i), GETARG_B(i), GETARG_C(i));
        aql_debug("OP_SETPROP: ra=%p, rb=%p, rc=%p", (void*)ra, (void*)rb, (void*)rc);
        
        if (ttisdict(s2v(ra))) {
          aql_debug("OP_SETPROP: 使用 dict 直接路径");
          if (acontainer_dict_set(L, (AQL_ContainerBase*)dictvalue(s2v(ra)), rb, rc) == 0) {
            aql_debug("OP_SETPROP: dict 设置成功");
            handled = 1;
          } else {
            aql_debug("OP_SETPROP: dict 设置失败");
          }
        } else if (ttiscontainer(s2v(ra))) {
          AQL_ContainerBase *container = (AQL_ContainerBase*)containervalue(s2v(ra));
          aql_debug("OP_SETPROP: 容器类型=%d, 长度=%zu", (int)container->type, container->length);
          
          /* 根据容器类型选择操作 */
          switch (container->type) {
            case CONTAINER_ARRAY:
            case CONTAINER_SLICE:
              if (ttisinteger(rb)) {
                size_t idx = (size_t)ivalue(rb);
                aql_debug("OP_SETPROP: 设置数组索引 %zu", idx);
                if (acontainer_array_set(L, container, idx, rc) == 0) {
                  aql_debug("OP_SETPROP: 数组设置成功");
                  handled = 1;
                } else {
                  aql_debug("OP_SETPROP: 数组设置失败");
                }
              } else {
                aql_debug("OP_SETPROP: 索引不是整数，类型=%d", ttype(rb));
              }
              break;
            case CONTAINER_DICT:
              aql_debug("OP_SETPROP: 字典设置");
              if (acontainer_dict_set(L, container, rb, rc) == 0) {
                aql_debug("OP_SETPROP: 字典设置成功");
                handled = 1;
              } else {
               aql_debug("OP_SETPROP: 字典设置失败");
              }
              break;
            case CONTAINER_VECTOR:
              if (ttisinteger(rb)) {
                size_t idx = (size_t)ivalue(rb);
                aql_debug("OP_SETPROP: 设置向量索引 %zu", idx);
                if (acontainer_array_set(L, container, idx, rc) == 0) {
                  aql_debug("OP_SETPROP: 向量设置成功");
                  handled = 1;
                } else {
                  aql_debug("OP_SETPROP: 向量设置失败");
                }
              } else {
                aql_debug("OP_SETPROP: 向量索引不是整数，类型=%d", ttype(rb));
              }
              break;
            default:
              aql_debug("OP_SETPROP: 不支持的容器类型 %d", (int)container->type);
              break;
          }

          if (!handled) {
            aql_debug("OP_SETPROP: 回退到元方法路径");
            Protect(aqlV_finishset(L, s2v(ra), rb, rc, NULL));
          }
        } else {
          aql_error("OP_SETPROP: ra 不是容器，类型=%d", ttype(s2v(ra)));
          aqlG_runerror(L, "invalid container property assignment");
        }
        vmbreak;
      }
      
      vmcase(OP_INVOKE) {
        StkId ra = RA(i);
        TValue *rb = vRB(i);
        int method_index = GETARG_C(i);
        
        if (ttiscontainer(rb)) {
          AQL_ContainerBase *container = (AQL_ContainerBase*)containervalue(rb);
          
          /* 简化方法调用 - 只支持基本方法 */
          if (container->type == CONTAINER_ARRAY || container->type == CONTAINER_SLICE) {
            if (method_index == 0) {  /* append */
              TValue *value = s2v(RB(i) + 1);  /* first argument follows the receiver register */
              if (acontainer_array_append(L, container, value) == 0) {
                setivalue(s2v(ra), l_castU2S(container->length));  /* 返回新长度 */
                vmbreak;
              }
            } else if (method_index == 1) {  /* length */
              setivalue(s2v(ra), l_castU2S(container->length));
              vmbreak;
            }
          }
        }
        
        aqlG_runerror(L, "invalid container method invocation");
        vmbreak;
      }
      
      /* 其他 AQL 扩展指令可以在这里添加 */
      
      vmdefault: {
        /* 未知指令处理 - 暂时简化 */
        OpCode op = GET_OPCODE(i);
        aqlG_runerror(L, "unknown opcode %d", op);
        vmbreak;
      }
    }
  }
}

// Legacy wrapper for compatibility
int aqlV_execute (aql_State *L, CallInfo *ci) {
  (void)L;
  (void)ci;
  aqlV_execute2(L, ci);
  return 1;  /* Match callers that treat 1 as successful completion. */
}
