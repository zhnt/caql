/*
** $Id: aobject.c $
** Some generic functions over AQL objects
** See Copyright Notice in aql.h
*/

#define aobject_c
#define AQL_CORE

#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aql.h"
#include "aconf.h"
#include "aobject.h"
#include "astate.h"
#include "adebug.h"
#include "acontainer.h"
#include "afunc.h"
#include "astring.h"
#include "atable.h"

#define savestack(L_,p_) ((char *)(p_) - (char *)(L_)->stack.p)
#define restorestack(L_,n_) ((StkId)((char *)(L_)->stack.p + (n_)))

AQL_API void aqlD_callnoyield(aql_State *L, StkId func, int nResults);

/* Temporary math function implementations */
static aql_Number aql_numpow(aql_State *L, aql_Number v1, aql_Number v2) {
  UNUSED(L);
  return pow(v1, v2);
}

static aql_Number aql_numidiv(aql_State *L, aql_Number v1, aql_Number v2) {
  UNUSED(L);
  if (v2 == 0.0) return 0.0;  /* Simple error handling */
  return floor(v1 / v2);
}

/* Temporary conversion function macros */
#define tointegerns(tv, pi) (ttisinteger(tv) ? (*(pi) = ivalue(tv), 1) : \
                             (ttisfloat(tv) ? aql_numbertointeger(fltvalue(tv), pi) : 0))

#define tonumberns(tv, pn) (ttisfloat(tv) ? ((pn) = fltvalue(tv), 1) : \
                           (ttisinteger(tv) ? ((pn) = cast_num(ivalue(tv)), 1) : 0))

/*
** Type names for debugging
*/
static const char *const typenames[AQL_NUMTYPES] = {
  "nil", "boolean", "lightuserdata", "number", "integer", 
  "string", "table", "function", "userdata", "thread",
  "array", "slice", "dict", "vector"
};

const char *aqlO_typename (const TValue *o) {
  int t = ttype(o);
  return (t < AQL_NUMTYPES) ? typenames[t] : "unknown";
}

/*
** Computes ceil(log2(x))
*/
int aqlO_ceillog2 (unsigned int x) {
  static const aql_byte log_2[256] = {  /* log_2[i] = ceil(log2(i - 1)) */
    0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
  };
  int l = 0;
  x--;
  while (x >= 256) { l += 8; x >>= 8; }
  return l + log_2[x];
}

int aqlO_hexavalue (int c) {
  if (lisdigit(c)) return c - '0';
  else return (ltolower(c) - 'a') + 10;
}

/*
** Metamethod support
*/

static const char *const aqlT_eventname[TM_N] = {  /* ORDER TM */
  "__index", "__newindex",
  "__gc", "__mode", "__len", "__eq",
  "__add", "__sub", "__mul", "__mod", "__pow",
  "__div", "__idiv",
  "__band", "__bor", "__bxor", "__shl", "__shr",
  "__unm", "__bnot", "__lt", "__le",
  "__concat", "__call", "__close"
};

static CClosure *newcclosure0(aql_State *L, aql_CFunction fn) {
  CClosure *cl = aqlF_newCclosure(L, 0);
  cl->f = fn;
  return cl;
}

static void settmfield(aql_State *L, Table *mt, TMS event, aql_CFunction fn) {
  TValue key;
  TValue value;

  setsvalue(L, &key, G(L)->tmname[event]);
  setclCvalue(L, &value, newcclosure0(L, fn));
  aqlH_set(L, mt, &key, &value);
  invalidateTMcache(mt);
}

static int is_seq_container_value(const TValue *o) {
  return ttisarray(o) || ttisslice(o);
}

static const TValue *aql_meta_index_arg(aql_State *L, int idx) {
  StkId p = L->ci->func.p + idx;
  return (p < L->top.p) ? s2v(p) : &G(L)->nilvalue;
}

static AQL_ContainerBase *seq_container(const TValue *o) {
  return containervalue(o);
}

static TValue *container_slot(const AQL_ContainerBase *c, size_t idx) {
  return &((TValue *)c->data)[idx];
}

static int container_index_tm(aql_State *L) {
  const TValue *obj = aql_meta_index_arg(L, 1);
  const TValue *key = aql_meta_index_arg(L, 2);
  TValue result;

  if (!ttiscontainer(obj)) {
    aql_pushnil(L);
    return 1;
  }

  AQL_ContainerBase *container = containervalue(obj);
  int rc = -1;

  switch (container->type) {
    case CONTAINER_ARRAY:
    case CONTAINER_SLICE:
    case CONTAINER_VECTOR:
      if (ttisinteger(key)) {
        aql_Integer idx = ivalue(key);
        if (idx >= 0) {
          rc = acontainer_array_get(L, container, (size_t)idx, &result);
        }
      }
      break;
    case CONTAINER_DICT:
      rc = acontainer_dict_get(L, container, key, &result);
      break;
    default:
      rc = -1;
      break;
  }

  if (rc == 0) {
    setobj2s(L, L->top.p, &result);
    L->top.p++;
  } else {
    aql_pushnil(L);
  }
  return 1;
}

static int container_newindex_tm(aql_State *L) {
  const TValue *obj = aql_meta_index_arg(L, 1);
  const TValue *key = aql_meta_index_arg(L, 2);
  const TValue *value = aql_meta_index_arg(L, 3);

  if (!ttiscontainer(obj)) {
    return 0;
  }

  AQL_ContainerBase *container = containervalue(obj);
  switch (container->type) {
    case CONTAINER_ARRAY:
    case CONTAINER_SLICE:
    case CONTAINER_VECTOR:
      if (ttisinteger(key)) {
        aql_Integer idx = ivalue(key);
        if (idx >= 0) {
          cast_void(acontainer_array_set(L, container, (size_t)idx, value));
        }
      }
      break;
    case CONTAINER_DICT:
      cast_void(acontainer_dict_set(L, container, key, value));
      break;
    default:
      break;
  }

  return 0;
}

static int seq_equal(aql_State *L, const TValue *lhs, const TValue *rhs) {
  AQL_ContainerBase *a = seq_container(lhs);
  AQL_ContainerBase *b = seq_container(rhs);
  size_t i;

  if (a->length != b->length) {
    return 0;
  }

  for (i = 0; i < a->length; i++) {
    if (!aqlV_equalobj(L, container_slot(a, i), container_slot(b, i))) {
      return 0;
    }
  }

  return 1;
}

static int seq_compare(aql_State *L, const TValue *lhs, const TValue *rhs,
                       int allow_equal) {
  AQL_ContainerBase *a = seq_container(lhs);
  AQL_ContainerBase *b = seq_container(rhs);
  size_t limit = (a->length < b->length) ? a->length : b->length;
  size_t i;

  for (i = 0; i < limit; i++) {
    TValue *av = container_slot(a, i);
    TValue *bv = container_slot(b, i);
    if (aqlV_equalobj(L, av, bv)) {
      continue;
    }
    return aqlV_lessthan(L, av, bv);
  }

  return allow_equal ? (a->length <= b->length) : (a->length < b->length);
}

static int seq_binary_value(aql_State *L, TMS event,
                            const TValue *lhs, const TValue *rhs,
                            TValue *out) {
  aql_Integer i1, i2;
  aql_Number n1, n2;

  switch (event) {
    case TM_SUB:
      if (ttisinteger(lhs) && ttisinteger(rhs)) {
        setivalue(out, ivalue(lhs) - ivalue(rhs));
        return 1;
      }
      if (tonumberns(lhs, n1) && tonumberns(rhs, n2)) {
        setfltvalue(out, n1 - n2);
        return 1;
      }
      return 0;
    case TM_MUL:
      if (ttisinteger(lhs) && ttisinteger(rhs)) {
        setivalue(out, ivalue(lhs) * ivalue(rhs));
        return 1;
      }
      if (tonumberns(lhs, n1) && tonumberns(rhs, n2)) {
        setfltvalue(out, n1 * n2);
        return 1;
      }
      return 0;
    case TM_DIV:
      if (tonumberns(lhs, n1) && tonumberns(rhs, n2) && n2 != 0) {
        setfltvalue(out, n1 / n2);
        return 1;
      }
      return 0;
    case TM_IDIV:
      if (tointegerns(lhs, &i1) && tointegerns(rhs, &i2) && i2 != 0) {
        setivalue(out, aqlV_idiv(L, i1, i2));
        return 1;
      }
      return 0;
    case TM_MOD:
      if (ttisinteger(lhs) && ttisinteger(rhs) && ivalue(rhs) != 0) {
        setivalue(out, ivalue(lhs) % ivalue(rhs));
        return 1;
      }
      if (tonumberns(lhs, n1) && tonumberns(rhs, n2) && n2 != 0) {
        setfltvalue(out, aqlV_modf(L, n1, n2));
        return 1;
      }
      return 0;
    case TM_POW:
      if (tonumberns(lhs, n1) && tonumberns(rhs, n2)) {
        setfltvalue(out, pow(n1, n2));
        return 1;
      }
      return 0;
    case TM_BAND:
      if (tointegerns(lhs, &i1) && tointegerns(rhs, &i2)) {
        setivalue(out, i1 & i2);
        return 1;
      }
      return 0;
    case TM_BOR:
      if (tointegerns(lhs, &i1) && tointegerns(rhs, &i2)) {
        setivalue(out, i1 | i2);
        return 1;
      }
      return 0;
    case TM_BXOR:
      if (tointegerns(lhs, &i1) && tointegerns(rhs, &i2)) {
        setivalue(out, i1 ^ i2);
        return 1;
      }
      return 0;
    case TM_SHL:
      if (tointegerns(lhs, &i1) && tointegerns(rhs, &i2)) {
        setivalue(out, aqlV_shiftl(i1, i2));
        return 1;
      }
      return 0;
    case TM_SHR:
      if (tointegerns(lhs, &i1) && tointegerns(rhs, &i2)) {
        setivalue(out, aqlV_shiftr(i1, i2));
        return 1;
      }
      return 0;
    default:
      return 0;
  }
}

static int seq_unary_value(aql_State *L, TMS event,
                           const TValue *value, TValue *out) {
  aql_Integer i;
  aql_Number n;

  switch (event) {
    case TM_UNM:
      if (ttisinteger(value)) {
        setivalue(out, intop(-, 0, ivalue(value)));
        return 1;
      }
      if (tonumberns(value, n)) {
        setfltvalue(out, -n);
        return 1;
      }
      return 0;
    case TM_BNOT:
      if (tointegerns(value, &i)) {
        setivalue(out, ~i);
        return 1;
      }
      return 0;
    default:
      return 0;
  }
}

static int seq_binary_tm(aql_State *L, TMS event) {
  const TValue *lhs = s2v(L->top.p - 2);
  const TValue *rhs = s2v(L->top.p - 1);
  AQL_ContainerBase *a = seq_container(lhs);
  AQL_ContainerBase *b = seq_container(rhs);
  AQL_ContainerBase *result;
  size_t i;

  if (a->length != b->length) {
    aql_pushnil(L);
    return 1;
  }

  result = acontainer_new(L, CONTAINER_ARRAY, AQL_DATA_TYPE_ANY, a->length);
  if (result == NULL) {
    aql_pushnil(L);
    return 1;
  }

  result->length = a->length;
  for (i = 0; i < a->length; i++) {
    if (!seq_binary_value(L, event, container_slot(a, i), container_slot(b, i),
                          container_slot(result, i))) {
      acontainer_destroy(L, result);
      aql_pushnil(L);
      return 1;
    }
  }

  aql_pushnil(L);
  setcontainervalue(L, s2v(L->top.p - 1), result);
  return 1;
}

static int seq_unary_tm(aql_State *L, TMS event) {
  const TValue *value = s2v(L->top.p - 2);
  AQL_ContainerBase *src = seq_container(value);
  AQL_ContainerBase *result;
  size_t i;

  result = acontainer_new(L, CONTAINER_ARRAY, AQL_DATA_TYPE_ANY, src->length);
  if (result == NULL) {
    aql_pushnil(L);
    return 1;
  }

  result->length = src->length;
  for (i = 0; i < src->length; i++) {
    if (!seq_unary_value(L, event, container_slot(src, i), container_slot(result, i))) {
      acontainer_destroy(L, result);
      aql_pushnil(L);
      return 1;
    }
  }

  aql_pushnil(L);
  setcontainervalue(L, s2v(L->top.p - 1), result);
  return 1;
}

static int seq_add_tm(aql_State *L) {
  const TValue *lhs = s2v(L->top.p - 2);
  const TValue *rhs = s2v(L->top.p - 1);
  AQL_ContainerBase *a = seq_container(lhs);
  AQL_ContainerBase *b = seq_container(rhs);
  AQL_ContainerBase *result;
  size_t i;

  result = acontainer_new(L, CONTAINER_ARRAY, AQL_DATA_TYPE_ANY,
                          a->length + b->length);
  if (result == NULL) {
    aql_pushnil(L);
    return 1;
  }

  result->length = a->length + b->length;
  for (i = 0; i < a->length; i++) {
    *container_slot(result, i) = *container_slot(a, i);
  }
  for (i = 0; i < b->length; i++) {
    *container_slot(result, a->length + i) = *container_slot(b, i);
  }

  aql_pushnil(L);
  setcontainervalue(L, s2v(L->top.p - 1), result);
  return 1;
}

static int seq_sub_tm(aql_State *L) {
  return seq_binary_tm(L, TM_SUB);
}

static int seq_mul_tm(aql_State *L) {
  return seq_binary_tm(L, TM_MUL);
}

static int seq_div_tm(aql_State *L) {
  return seq_binary_tm(L, TM_DIV);
}

static int seq_idiv_tm(aql_State *L) {
  return seq_binary_tm(L, TM_IDIV);
}

static int seq_mod_tm(aql_State *L) {
  return seq_binary_tm(L, TM_MOD);
}

static int seq_pow_tm(aql_State *L) {
  return seq_binary_tm(L, TM_POW);
}

static int seq_band_tm(aql_State *L) {
  return seq_binary_tm(L, TM_BAND);
}

static int seq_bor_tm(aql_State *L) {
  return seq_binary_tm(L, TM_BOR);
}

static int seq_bxor_tm(aql_State *L) {
  return seq_binary_tm(L, TM_BXOR);
}

static int seq_shl_tm(aql_State *L) {
  return seq_binary_tm(L, TM_SHL);
}

static int seq_shr_tm(aql_State *L) {
  return seq_binary_tm(L, TM_SHR);
}

static int seq_unm_tm(aql_State *L) {
  return seq_unary_tm(L, TM_UNM);
}

static int seq_bnot_tm(aql_State *L) {
  return seq_unary_tm(L, TM_BNOT);
}

static int seq_eq_tm(aql_State *L) {
  const TValue *lhs = s2v(L->top.p - 2);
  const TValue *rhs = s2v(L->top.p - 1);
  aql_pushboolean(L, seq_equal(L, lhs, rhs));
  return 1;
}

static int seq_lt_tm(aql_State *L) {
  const TValue *lhs = s2v(L->top.p - 2);
  const TValue *rhs = s2v(L->top.p - 1);
  aql_pushboolean(L, seq_compare(L, lhs, rhs, 0));
  return 1;
}

static int seq_le_tm(aql_State *L) {
  const TValue *lhs = s2v(L->top.p - 2);
  const TValue *rhs = s2v(L->top.p - 1);
  aql_pushboolean(L, seq_compare(L, lhs, rhs, 1));
  return 1;
}

static void init_container_metatable(aql_State *L, int tt) {
  Table *mt = aqlH_new(L);
  settmfield(L, mt, TM_INDEX, container_index_tm);
  settmfield(L, mt, TM_NEWINDEX, container_newindex_tm);
  settmfield(L, mt, TM_ADD, seq_add_tm);
  settmfield(L, mt, TM_SUB, seq_sub_tm);
  settmfield(L, mt, TM_MUL, seq_mul_tm);
  settmfield(L, mt, TM_DIV, seq_div_tm);
  settmfield(L, mt, TM_IDIV, seq_idiv_tm);
  settmfield(L, mt, TM_MOD, seq_mod_tm);
  settmfield(L, mt, TM_POW, seq_pow_tm);
  settmfield(L, mt, TM_BAND, seq_band_tm);
  settmfield(L, mt, TM_BOR, seq_bor_tm);
  settmfield(L, mt, TM_BXOR, seq_bxor_tm);
  settmfield(L, mt, TM_SHL, seq_shl_tm);
  settmfield(L, mt, TM_SHR, seq_shr_tm);
  settmfield(L, mt, TM_UNM, seq_unm_tm);
  settmfield(L, mt, TM_BNOT, seq_bnot_tm);
  settmfield(L, mt, TM_EQ, seq_eq_tm);
  settmfield(L, mt, TM_LT, seq_lt_tm);
  settmfield(L, mt, TM_LE, seq_le_tm);
  mt->flags = 0;
  G(L)->mt[tt] = mt;
}

void aqlT_initmetamethods(aql_State *L) {
  int i;

  for (i = 0; i < TM_N; i++) {
    G(L)->tmname[i] = aqlStr_newlstr(L, aqlT_eventname[i],
                                     strlen(aqlT_eventname[i]));
  }

  init_container_metatable(L, AQL_TARRAY);
  init_container_metatable(L, AQL_TSLICE);
  init_container_metatable(L, AQL_TVECTOR);
  init_container_metatable(L, AQL_TDICT);
}

const TValue *aqlT_gettm(aql_State *L, Table *events, TMS event) {
  const TValue *tm = aqlH_getshortstr(events, G(L)->tmname[event]);

  if (ttisnil(tm)) {
    if (event <= TM_EQ) {
      events->flags |= cast_byte(1u << event);
    }
    return NULL;
  }

  return tm;
}

const TValue *aqlT_gettmbyobj(aql_State *L, const TValue *o, TMS event) {
  Table *mt;

  switch (ttype(o)) {
    case AQL_TTABLE:
      mt = hvalue(o)->metatable;
      break;
    case AQL_TUSERDATA:
      mt = uvalue(o)->metatable;
      break;
    default:
      mt = G(L)->mt[ttype(o)];
      break;
  }

  return (mt != NULL) ? aqlH_getshortstr(mt, G(L)->tmname[event])
                      : &G(L)->nilvalue;
}

void aqlT_callTM(aql_State *L, const TValue *f, const TValue *p1,
                 const TValue *p2, const TValue *p3) {
  StkId oldtop = L->top.p;
  StkId func = oldtop;

  setobj2s(L, func, f);
  setobj2s(L, func + 1, p1);
  setobj2s(L, func + 2, p2);
  setobj2s(L, func + 3, p3);
  L->top.p = func + 4;

  if (ttisCclosure(s2v(func))) {
    CClosure *ccl = clCvalue(s2v(func));
    cast_void(ccl->f(L));
    L->top.p = oldtop;
    return;
  }
  if (ttislcf(s2v(func))) {
    aql_CFunction cfn = fvalue(s2v(func));
    cast_void(cfn(L));
    L->top.p = oldtop;
    return;
  }

  aqlD_callnoyield(L, func, 0);
  L->top.p = oldtop;
}

int aqlT_callTMres(aql_State *L, const TValue *f, const TValue *p1,
                   const TValue *p2, StkId res) {
  ptrdiff_t result = savestack(L, res);
  StkId oldtop = L->top.p;
  StkId func = oldtop;
  const TValue *src = &G(L)->nilvalue;

  setobj2s(L, func, f);
  setobj2s(L, func + 1, p1);
  setobj2s(L, func + 2, p2);
  L->top.p = func + 3;

  if (ttisCclosure(s2v(func))) {
    CClosure *ccl = clCvalue(s2v(func));
    int nres = ccl->f(L);
    if (nres > 0) {
      src = s2v(L->top.p - nres);
    }
  }
  else if (ttislcf(s2v(func))) {
    aql_CFunction cfn = fvalue(s2v(func));
    int nres = cfn(L);
    if (nres > 0) {
      src = s2v(L->top.p - nres);
    }
  }
  else {
    aqlD_callnoyield(L, func, 1);
    src = s2v(func);
  }

  res = restorestack(L, result);
  setobj2s(L, res, src);
  L->top.p = oldtop;
  return ttypetag(s2v(res));
}

static int callbinTM(aql_State *L, const TValue *p1, const TValue *p2,
                     StkId res, TMS event) {
  const TValue *tm = aqlT_gettmbyobj(L, p1, event);

  if (ttisnil(tm)) {
    tm = aqlT_gettmbyobj(L, p2, event);
  }
  if (ttisnil(tm)) {
    return -1;
  }

  return aqlT_callTMres(L, tm, p1, p2, res);
}

void aqlT_trybinTM(aql_State *L, const TValue *p1, const TValue *p2,
                   StkId res, TMS event) {
  if (callbinTM(L, p1, p2, res, event) < 0) {
    setnilvalue(s2v(res));
  }
}

void aqlT_trybinassocTM(aql_State *L, const TValue *p1, const TValue *p2,
                        int flip, StkId res, TMS event) {
  if (flip) {
    aqlT_trybinTM(L, p2, p1, res, event);
  } else {
    aqlT_trybinTM(L, p1, p2, res, event);
  }
}

void aqlT_trybiniTM(aql_State *L, const TValue *p1, aql_Integer i2,
                    int flip, StkId res, TMS event) {
  TValue aux;
  setivalue(&aux, i2);
  aqlT_trybinassocTM(L, p1, &aux, flip, res, event);
}

int aqlT_callorderTM(aql_State *L, const TValue *p1, const TValue *p2,
                     TMS event) {
  int tag = callbinTM(L, p1, p2, L->top.p, event);

  if (tag >= 0) {
    return !l_isfalse(s2v(L->top.p));
  }

  return aqlG_ordererror(L, p1, p2);
}

int aqlT_callorderiTM(aql_State *L, const TValue *p1, int v2,
                      int flip, int isfloat, TMS event) {
  TValue aux;
  const TValue *p2;

  if (isfloat) {
    setfltvalue(&aux, cast_num(v2));
  } else {
    setivalue(&aux, v2);
  }

  if (flip) {
    p2 = p1;
    p1 = &aux;
  } else {
    p2 = &aux;
  }

  return aqlT_callorderTM(L, p1, p2, event);
}

void aqlG_runerror (aql_State *L, const char *fmt, ...) {
  /* Placeholder - simplified error handling */
  UNUSED(L); UNUSED(fmt);
  /* In a real implementation, this would format the error message
     and throw a runtime error */
}

/* aqlStr_newlstr 现在在 astring.c 中实现 */

/*
** Additional VM support functions (placeholders)
*/
void aqlG_typeerror (aql_State *L, const TValue *o, const char *op) {
  /* Placeholder - simplified type error */
  UNUSED(L); UNUSED(o); UNUSED(op);
  /* In a real implementation, this would format and throw a type error */
}

int aqlG_ordererror (aql_State *L, const TValue *p1, const TValue *p2) {
  /* Placeholder - simplified order error */
  UNUSED(L); UNUSED(p1); UNUSED(p2);
  /* In a real implementation, this would format and throw an order error */
  return 0;
}

const char *aqlL_typename (aql_State *L, const TValue *o) {
  /* Placeholder - simplified typename */
  UNUSED(L); UNUSED(o);
  /* In a real implementation, this would return the type name */
  return "unknown";
}

TString *aqlS_createlngstrobj (aql_State *L, size_t l) {
  /* Create a long string object with uninitialized content */
  char *buffer = (char *)malloc(l + 1);  /* +1 for null terminator */
  if (buffer == NULL) {
    aqlG_runerror(L, "out of memory creating string");
    return NULL;
  }
  buffer[l] = '\0';  /* null terminate */
  TString *ts = aqlStr_newlstr(L, buffer, l);
  free(buffer);  /* aqlStr_newlstr makes its own copy */
  return ts;
}

AQL_API int isdead (global_State *g, GCObject *o) {
  /* Placeholder - simplified GC check */
  UNUSED(g); UNUSED(o);
  /* In a real implementation, this would check if object is marked for GC */
  return 0;
}

/* VM function call support moved to ado.c */

/* Call management functions moved to ado.c */

/*
** Integer arithmetic operations
*/
static aql_Integer intarith (aql_State *L, int op, aql_Integer v1, aql_Integer v2) {
  switch (op) {
    case AQL_OPADD: return intop(+, v1, v2);
    case AQL_OPSUB: return intop(-, v1, v2);
    case AQL_OPMUL: return intop(*, v1, v2);
    case AQL_OPMOD: return aqlV_mod(L, v1, v2);
    case AQL_OPIDIV: return aqlV_idiv(L, v1, v2);
    case AQL_OPBAND: return intop(&, v1, v2);
    case AQL_OPBOR: return intop(|, v1, v2);
    case AQL_OPBXOR: return intop(^, v1, v2);
    case AQL_OPSHL: return aqlV_shiftl(v1, v2);
    case AQL_OPSHR: return aqlV_shiftr(v1, v2);
    case AQL_OPUNM: return intop(-, 0, v1);
    case AQL_OPBNOT: return intop(^, ~l_castS2U(0), v1);
    default: aql_assert(0); return 0;
  }
}

/*
** Floating-point arithmetic operations
*/
static aql_Number numarith (aql_State *L, int op, aql_Number v1, aql_Number v2) {
  switch (op) {
    case AQL_OPADD: return aql_numadd(L, v1, v2);
    case AQL_OPSUB: return aql_numsub(L, v1, v2);
    case AQL_OPMUL: return aql_nummul(L, v1, v2);
    case AQL_OPDIV: return aql_numdiv(L, v1, v2);
    case AQL_OPPOW: return aql_numpow(L, v1, v2);
    case AQL_OPIDIV: return aql_numidiv(L, v1, v2);
    case AQL_OPUNM: return aql_numunm(L, v1);
    case AQL_OPMOD: return aqlV_modf(L, v1, v2);
    default: aql_assert(0); return 0;
  }
}

/*
** Raw arithmetic operations (without metamethods)
*/
int aqlO_rawarith (aql_State *L, int op, const TValue *p1, const TValue *p2,
                   TValue *res) {
  switch (op) {
    case AQL_OPBAND: case AQL_OPBOR: case AQL_OPBXOR:
    case AQL_OPSHL: case AQL_OPSHR:
    case AQL_OPBNOT: {  /* operate only on integers */
      aql_Integer i1; aql_Integer i2;
      if (tointegerns(p1, &i1) && tointegerns(p2, &i2)) {
        setivalue(res, intarith(L, op, i1, i2));
        return 1;
      }
      else return 0;  /* fail */
    }
    case AQL_OPDIV: case AQL_OPPOW: {  /* operate only on floats */
      aql_Number n1; aql_Number n2;
      if (tonumberns(p1, n1) && tonumberns(p2, n2)) {
        setfltvalue(res, numarith(L, op, n1, n2));
        return 1;
      }
      else return 0;  /* fail */
    }
    default: {  /* other operations */
      aql_Number n1; aql_Number n2;
      if (ttisinteger(p1) && ttisinteger(p2)) {
        setivalue(res, intarith(L, op, ivalue(p1), ivalue(p2)));
        return 1;
      }
      else if (tonumberns(p1, n1) && tonumberns(p2, n2)) {
        setfltvalue(res, numarith(L, op, n1, n2));
        return 1;
      }
      else return 0;  /* fail */
    }
  }
}

/*
** Arithmetic operations with metamethod fallback
*/
void aqlO_arith (aql_State *L, int op, const TValue *p1, const TValue *p2,
                 StkId res) {
  if (!aqlO_rawarith(L, op, p1, p2, s2v(res))) {
    /* could not perform raw operation; try metamethod */
    aqlT_trybinTM(L, p1, p2, res, cast(TMS, (op - AQL_OPADD) + TM_ADD));
  }
}

/*
** Helper function to check for negative sign
*/
static int isneg (const char **s) {
  if (**s == '-') { (*s)++; return 1; }
  else if (**s == '+') (*s)++;
  return 0;
}

/*
** Convert string 's' to an AQL number (put in 'result'). Return NULL on
** fail or the address of the ending '\0' on success. ('mode' == 'x')
** means a hexadecimal numeral.
*/
static const char *l_str2dloc (const char *s, aql_Number *result, int mode) {
  char *endptr;
  *result = (mode == 'x') ? aql_strx2number(s, &endptr)  /* try to convert */
                          : aql_str2number(s, &endptr);
  if (endptr == s) return NULL;  /* nothing recognized? */
  while (lisspace(cast_uchar(*endptr))) endptr++;  /* skip trailing spaces */
  return (*endptr == '\0') ? endptr : NULL;  /* OK iff no trailing chars */
}

/*
** Convert string 's' to an AQL number (put in 'result') handling the
** current locale.
*/
static const char *l_str2d (const char *s, aql_Number *result) {
  const char *endptr;
  const char *pmode = strpbrk(s, ".xXnN");  /* look for special chars */
  int mode = pmode ? ltolower(cast_uchar(*pmode)) : 0;
  if (mode == 'n')  /* reject 'inf' and 'nan' */
    return NULL;
  endptr = l_str2dloc(s, result, mode);  /* try to convert */
  if (endptr == NULL) {  /* failed? may be a different locale */
    char buff[L_MAXLENNUM + 1];
    const char *pdot = strchr(s, '.');
    if (pdot == NULL || strlen(s) > L_MAXLENNUM)
      return NULL;  /* string too long or no dot; fail */
    strcpy(buff, s);  /* copy string to buffer */
    buff[pdot - s] = aql_getlocaledecpoint();  /* correct decimal point */
    endptr = l_str2dloc(buff, result, mode);  /* try again */
    if (endptr != NULL)
      endptr = s + (endptr - buff);  /* make relative to 's' */
  }
  return endptr;
}

/*
** Convert string to integer
*/
static const char *l_str2int (const char *s, aql_Integer *result) {
  aql_Unsigned a = 0;
  int empty = 1;
  int neg;
  while (lisspace(cast_uchar(*s))) s++;  /* skip initial spaces */
  neg = isneg(&s);
  if (s[0] == '0' &&
      (s[1] == 'x' || s[1] == 'X')) {  /* hex? */
    s += 2;  /* skip '0x' */
    for (; lisxdigit(cast_uchar(*s)); s++) {
      a = a * 16 + aqlO_hexavalue(*s);
      empty = 0;
    }
  }
  else {  /* decimal */
    for (; lisdigit(cast_uchar(*s)); s++) {
      int d = *s - '0';
      if (a >= MAXBY10 && (a > MAXBY10 || d > MAXLASTD + neg))  /* overflow? */
        return NULL;  /* do not accept it (as integer) */
      a = a * 10 + d;
      empty = 0;
    }
  }
  while (lisspace(cast_uchar(*s))) s++;  /* skip trailing spaces */
  if (empty || *s != '\0') return NULL;  /* something wrong in the numeral */
  else {
    *result = l_castU2S((neg) ? 0u - a : a);
    return s;
  }
}

/*
** Main string to number conversion function
*/
size_t aqlO_str2num (const char *s, TValue *o) {
  aql_debug("AQLO_STR2NUM: 开始转换字符串 '%s'", s ? s : "(null)");
  aql_Integer i; aql_Number n;
  const char *e;
  if ((e = l_str2int(s, &i)) != NULL) {  /* try as an integer */
    aql_debug("AQLO_STR2NUM: 整数转换成功，值=%ld", (long)i);
    setivalue(o, i);
  }
  else if ((e = l_str2d(s, &n)) != NULL) {  /* else try as a float */
    setfltvalue(o, n);
  }
  else
    return 0;  /* conversion failed */
  return (e - s) + 1;  /* success; return string size */
}

int aqlO_utf8esc (char *buff, unsigned long x) {
  int n = 1;  /* number of bytes put in buffer (backwards) */
  aql_assert(x <= 0x7FFFFFFFu);
  if (x < 0x80)  /* ascii? */
    buff[UTF8BUFFSZ - 1] = cast_char(x);
  else {  /* need continuation bytes */
    unsigned int mfb = 0x3f;  /* maximum that fits in first byte */
    do {  /* add continuation bytes */
      buff[UTF8BUFFSZ - (n++)] = cast_char(0x80 | (x & 0x3f));
      x >>= 6;  /* remove added bits */
      mfb >>= 1;  /* now there is one less bit available in first byte */
    } while (x > mfb);  /* still needs continuation byte? */
    buff[UTF8BUFFSZ - n] = cast_char((~mfb << 1) | x);  /* add first byte */
  }
  return n;
}

/*
** Convert a number object to a string, adding it to a buffer
*/
static int tostringbuff (TValue *obj, char *buff) {
  int len;
  aql_assert(ttisnumber(obj));
  if (ttisinteger(obj))
    len = aql_integer2str(buff, MAXNUMBER2STR, ivalue(obj));
  else {
    len = aql_number2str(buff, MAXNUMBER2STR, fltvalue(obj));
    if (buff[strspn(buff, "-0123456789")] == '\0') {  /* looks like an int? */
      buff[len++] = aql_getlocaledecpoint();
      buff[len++] = '0';  /* adds '.0' to result */
    }
  }
  return len;
}

/*
** Convert a number object to an AQL string, replacing the value at 'obj'
*/
void aqlO_tostring (aql_State *L, TValue *obj) {
  char buff[MAXNUMBER2STR];
  size_t len;
  
  if (ttisinteger(obj)) {
    len = aql_integer2str(buff, sizeof(buff), ivalue(obj));
  } else if (ttisfloat(obj)) {
    len = aql_number2str(buff, sizeof(buff), fltvalue(obj));
  } else {
    return;  /* Not a number, cannot convert */
  }
  
  TString *ts = aqlStr_newlstr(L, buff, len);
  setsvalue(L, obj, ts);
}

/*
** {==================================================================
** 'aqlO_pushvfstring'
** ===================================================================
*/

/*
** Size for buffer space used by 'aqlO_pushvfstring'. It should be
** (AQL_IDSIZE + MAXNUMBER2STR) + a minimal space for basic messages,
** so that 'aqlG_addinfo' can work directly on the buffer.
*/

/* Note: Simplified implementation removes complex buffer management for Phase 1 */

/*
** Simplified string formatting function (Phase 1 implementation)
** this function handles only '%d', '%c', '%f', '%p', '%s', and '%%'
   conventional formats, plus AQL-specific '%I' and '%U'
*/
const char *aqlO_pushvfstring (aql_State *L, const char *fmt, va_list argp) {
  static char result_buffer[BUFVFS];  /* Static buffer for result */
  char *dest = result_buffer;
  const char *e;  /* points to next '%' */
  size_t remaining = BUFVFS - 1;  /* Leave space for null terminator */
  
  UNUSED(L);  /* For Phase 1, we don't use the stack */
  
  result_buffer[0] = '\0';  /* Initialize buffer */
  
  while ((e = strchr(fmt, '%')) != NULL && remaining > 0) {
    /* Copy format string up to '%' */
    size_t len = e - fmt;
    if (len > remaining) len = remaining;
    memcpy(dest, fmt, len);
    dest += len;
    remaining -= len;
    
    if (remaining == 0) break;
    
    switch (*(e + 1)) {  /* conversion specifier */
      case 's': {  /* zero-terminated string */
        const char *s = va_arg(argp, char *);
        if (s == NULL) s = "(null)";
        size_t slen = strlen(s);
        if (slen > remaining) slen = remaining;
        memcpy(dest, s, slen);
        dest += slen;
        remaining -= slen;
        break;
      }
      case 'c': {  /* an 'int' as a character */
        if (remaining > 0) {
          *dest++ = cast_uchar(va_arg(argp, int));
          remaining--;
        }
        break;
      }
      case 'd': {  /* an 'int' */
        TValue num;
        setivalue(&num, va_arg(argp, int));
        int len = tostringbuff(&num, dest);
        if (len > (int)remaining) len = remaining;
        dest += len;
        remaining -= len;
        break;
      }
      case 'I': {  /* an 'aql_Integer' */
        TValue num;
        setivalue(&num, cast(aql_Integer, va_arg(argp, l_uacInt)));
        int len = tostringbuff(&num, dest);
        if (len > (int)remaining) len = remaining;
        dest += len;
        remaining -= len;
        break;
      }
      case 'f': {  /* an 'aql_Number' */
        TValue num;
        setfltvalue(&num, cast_num(va_arg(argp, l_uacNumber)));
        int len = tostringbuff(&num, dest);
        if (len > (int)remaining) len = remaining;
        dest += len;
        remaining -= len;
        break;
      }
      case 'p': {  /* a pointer */
        void *p = va_arg(argp, void *);
        int len = aql_pointer2str(dest, remaining, p);
        if (len > (int)remaining) len = remaining;
        dest += len;
        remaining -= len;
        break;
      }
      case '%': {
        if (remaining > 0) {
          *dest++ = '%';
          remaining--;
        }
        break;
    }
    default: {
        /* Skip unknown format specifiers */
        break;
      }
    }
    fmt = e + 2;  /* skip '%' and the specifier */
  }
  
  /* Copy rest of format string */
  if (remaining > 0) {
    size_t len = strlen(fmt);
    if (len > remaining) len = remaining;
    memcpy(dest, fmt, len);
    dest += len;
  }
  
  *dest = '\0';  /* Null terminate */
  return result_buffer;
}

const char *aqlO_pushfstring (aql_State *L, const char *fmt, ...) {
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = aqlO_pushvfstring(L, fmt, argp);
  va_end(argp);
  return msg;
}

/* }================================================================== */

void aqlO_chunkid (char *out, const char *source, size_t srclen) {
  /* Simple placeholder implementation */
  if (srclen > 60) srclen = 60;
  if (*source == '=') {  /* 'literal' source */
    if (srclen > 1) memcpy(out, source + 1, (srclen - 1) * sizeof(char));
    out[srclen - 1] = '\0';
  } else if (*source == '@') {  /* file name */
    if (srclen > 1) memcpy(out, source + 1, (srclen - 1) * sizeof(char));
    out[srclen - 1] = '\0';
  } else {  /* string */
    memcpy(out, "[string]", 8);
    out[8] = '\0';
  }
}
