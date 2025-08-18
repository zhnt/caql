/*
** $Id: avm.c $
** AQL virtual machine
** See Copyright Notice in aql.h
*/

#define avm_c
#define AQL_CORE

#include "aql.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "aopcodes.h"
#include "avm.h"
#include "ado.h"
#include "agc.h"
#include "amem.h"
#include "astring.h"
#include "adatatype.h"
#include "aarray.h"
#include "aslice.h"
#include "adict.h"
#include "avector.h"

/* Missing function declarations */
static int aqlV_toboolean(const TValue *obj);
/* Container access functions are in their respective header files */

/*
** Convert TValue to boolean according to AQL rules:
** - nil and false are falsy
** - everything else is truthy (including 0, empty string, empty containers)
*/
static int aqlV_toboolean(const TValue *obj) {
  switch (rawtt(obj)) {
    case AQL_TNIL: return 0;
    case AQL_TBOOLEAN: return bvalue(obj);
    default: return 1;  /* All other values are truthy */
  }
}

/*
** {==================================================================
** AQL VM Core - Register-based Virtual Machine
** ==================================================================
*/

/*
** Instruction execution macro
*/
#define iABC(i)     cast_int(GETARG_A(i))
#define iABx(i)     cast_int(GETARG_Bx(i))
#define iAsBx(i)    cast_int(GETARG_sBx(i))
#define iAx(i)      cast_int(GETARG_Ax(i))

#define RA(i)   (base + iABC(i))
#define RB(i)   (base + GETARG_B(i))
#define RC(i)   (base + GETARG_C(i))
#define RKB(i)  (ISK(GETARG_B(i)) ? k + INDEXK(GETARG_B(i)) : base + GETARG_B(i))
#define RKC(i)  (ISK(GETARG_C(i)) ? k + INDEXK(GETARG_C(i)) : base + GETARG_C(i))
#define vRA(i)  s2v(RA(i))  /* Convert StkId to TValue* */
#define vRB(i)  s2v(RB(i))  /* Convert StkId to TValue* */
#define vRC(i)  s2v(RC(i))  /* Convert StkId to TValue* */
#define vRKB(i) (ISK(GETARG_B(i)) ? k + INDEXK(GETARG_B(i)) : s2v(base + GETARG_B(i)))
#define vRKC(i) (ISK(GETARG_C(i)) ? k + INDEXK(GETARG_C(i)) : s2v(base + GETARG_C(i)))

/*
** Macros for repetitive instruction patterns
*/

/* Arithmetic operations */
#define ARITH_OP(op) do { \
  arith_op(L, s2v(RA(i)), s2v(RB(i)), s2v(RC(i)), TM_##op); \
  continue; \
} while(0)

#define ARITH_OP_K(op) do { \
  arith_op(L, s2v(RA(i)), s2v(RB(i)), k + GETARG_C(i), TM_##op); \
  continue; \
} while(0)

#define ARITH_OP_I(op) do { \
  TValue kval; \
  setivalue(&kval, GETARG_C(i)); \
  arith_op(L, s2v(RA(i)), s2v(RB(i)), &kval, TM_##op); \
  continue; \
} while(0)

/* Bitwise operations */
#define BITWISE_OP(op) do { \
  arith_op(L, s2v(RA(i)), s2v(RB(i)), s2v(RC(i)), TM_##op); \
  continue; \
} while(0)

/* Comparison operations */
#define CMP_OP(cmpfunc) do { \
  const TValue *rb = vRKB(i); \
  const TValue *rc = vRKC(i); \
  int cond = cmpfunc(L, rb, rc) == GETARG_A(i); \
  if (cond) pc++; \
  continue; \
} while(0)

/* Return operations */
#define RET_OP(setup, a_arg, b_arg) do { \
  setup; \
  if (aqlD_poscall(L, ci, b_arg)) \
    return 1; \
  else { \
    ci = L->ci; \
    goto newframe; \
  } \
} while(0)

/*
** Forward declarations for comparison functions
*/
static int aqlV_compare_array_fast (aql_State *L, const Array *a1, const Array *a2);
static int aqlV_compare_slice_fast (aql_State *L, const Slice *s1, const Slice *s2);
static int aqlV_compare_dict_fast (aql_State *L, const Dict *d1, const Dict *d2);
static int aqlV_compare_vector_fast (aql_State *L, const Vector *v1, const Vector *v2);

/*
** Arithmetic operations
*/
static void arith_op (aql_State *L, TValue *ra, const TValue *rb, const TValue *rc, TMS op) {
  TValue tempb, tempc;
  const TValue *b, *c;
  
  if (ttisinteger(rb) && ttisinteger(rc)) {
    aql_Integer ib = ivalue(rb);
    aql_Integer ic = ivalue(rc);
    aql_Integer res;
    
    switch (op) {
      case TM_ADD: res = intop(+, ib, ic); break;
      case TM_SUB: res = intop(-, ib, ic); break;
      case TM_MUL: res = intop(*, ib, ic); break;
      case TM_MOD: res = aqlV_mod(L, ib, ic); break;
      case TM_IDIV: res = aqlV_idiv(L, ib, ic); break;
      case TM_BAND: res = intop(&, ib, ic); break;
      case TM_BOR: res = intop(|, ib, ic); break;
      case TM_BXOR: res = intop(^, ib, ic); break;
      case TM_SHL: res = aqlV_shiftl(ib, ic); break;
      case TM_SHR: res = aqlV_shiftl(ib, -ic); break;
      default: aql_assert(0); res = 0;  /* to avoid warnings */
    }
    setivalue(ra, res);
  } else if (ttisnumber(rb) && ttisnumber(rc)) {
    aql_Number nb = ttisinteger(rb) ? cast_num(ivalue(rb)) : fltvalue(rb);
    aql_Number nc = ttisinteger(rc) ? cast_num(ivalue(rc)) : fltvalue(rc);
    aql_Number res;
    
    switch (op) {
      case TM_ADD: res = nb + nc; break;
      case TM_SUB: res = nb - nc; break;
      case TM_MUL: res = nb * nc; break;
      case TM_DIV: res = nb / nc; break;
      case TM_MOD: res = aqlV_modf(L, nb, nc); break;
      case TM_POW: res = pow(nb, nc); break;
      default: aql_assert(0); res = 0;  /* to avoid warnings */
    }
    setfltvalue(ra, res);
  } else {
    aqlG_runerror(L, "attempt to perform arithmetic on a %s and a %s",
                  aqlL_typename(L, rb), aqlL_typename(L, rc));
  }
}

/*
** Comparison operations
*/
static int aqlV_lessthan_internal (aql_State *L, const TValue *l, const TValue *r) {
  int res;
  if (ttisnumber(l) && ttisnumber(r)) {
    return aqlV_lessthan(L, l, r);
  } else if (ttisstring(l) && ttisstring(r)) {
    return aqlV_lessthan(L, l, r);
  } else {
    return aqlG_ordererror(L, l, r);
  }
}

static int aqlV_lessequal_internal (aql_State *L, const TValue *l, const TValue *r) {
  return aqlV_lessequal(L, l, r);
}

/*
** AQL Container Operations
*/
static void create_array (aql_State *L, StkId ra, int size, DataType dtype) {
  Array *arr = (Array*)aqlM_newobject(L, AQL_TARRAY, sizeof(Array));
  arr->dtype = dtype;
  arr->length = size;
  arr->capacity = size;
  arr->data = (TValue*)aqlM_newvector(L, size, TValue);
  for (int i = 0; i < size; i++) {
    setnilvalue(&arr->data[i]);
  }
  setarrayvalue(L, s2v(ra), arr);
}

static void create_slice (aql_State *L, StkId ra, int capacity, DataType dtype) {
  Slice *slice = (Slice*)aqlM_newobject(L, AQL_TSLICE, sizeof(Slice));
  slice->dtype = dtype;
  slice->length = 0;
  slice->capacity = capacity;
  slice->data = aqlM_newvector(L, capacity, TValue);
  setslicevalue(L, s2v(ra), slice);
}

static void create_dict (aql_State *L, StkId ra, int capacity, DataType key_type, DataType value_type) {
  Dict *dict = (Dict*)aqlM_newobject(L, AQL_TDICT, sizeof(Dict));
  dict->key_type = key_type;
  dict->value_type = value_type;
  dict->length = 0;
  dict->capacity = capacity > 0 ? capacity : 8;
  dict->mask = dict->capacity - 1;
  dict->entries = aqlM_newvector(L, dict->capacity, DictEntry);
  
  for (size_t i = 0; i < dict->capacity; i++) {
    setnilvalue(&dict->entries[i].key);
    setnilvalue(&dict->entries[i].value);
    dict->entries[i].hash = 0;
    dict->entries[i].hash = 0;
    dict->entries[i].distance = 0;
    setnilvalue(&dict->entries[i].key);
    setnilvalue(&dict->entries[i].value);
  }
  setdictvalue(L, s2v(ra), dict);
}

static void create_vector (aql_State *L, StkId ra, int length, DataType dtype) {
  Vector *vec = (Vector*)aqlM_newobject(L, AQL_TVECTOR, sizeof(Vector));
  vec->dtype = dtype;
  vec->length = length;
  vec->simd_width = 8;  /* Default SIMD width */
  vec->alignment = 64;  /* Default alignment */
  vec->capacity = length;
  vec->data = aqlM_newvector(L, length * sizeof(aql_Number), char);
  setvectorvalue(L, s2v(ra), vec);
}

/*
** Main VM execution function
*/
AQL_API int aqlV_execute (aql_State *L, CallInfo *ci) {
  LClosure *cl;
  TValue *k;
  StkId base;
  const Instruction *pc;
  
  aql_assert(ci == L->ci);
  cl = clLvalue(s2v(ci->func));
  base = ci->func + 1;
  pc = ci->u.l.savedpc;
  k = cl->p->k;
  
  for (;;) {
    Instruction i = *pc++;
    StkId ra;
    
    aql_assert(base == ci->func + 1);
    aql_assert(base <= L->top && L->top <= L->stack_last);
    
    switch (GET_OPCODE(i)) {
      /* Base operations */
      case OP_MOVE: {
        setobj2s(L, RA(i), s2v(RB(i)));
        continue;
      }
      case OP_LOADI: {
        setivalue(s2v(RA(i)), iAsBx(i));
        continue;
      }
      case OP_LOADF: {
        setfltvalue(s2v(RA(i)), cast_num(iAsBx(i)));
        continue;
      }
      case OP_LOADK: {
        TValue *kv = k + iABx(i);
        setobj2s(L, RA(i), kv);
        continue;
      }
      case OP_LOADKX: {
        TValue *kv = k + iAx(*pc++);
        setobj2s(L, RA(i), kv);
        continue;
      }
      case OP_LOADFALSE: {
        setbfvalue(s2v(RA(i)));
        continue;
      }
      case OP_LOADTRUE: {
        setbtvalue(s2v(RA(i)));
        continue;
      }
      case OP_LOADNIL: {
        int b = GETARG_B(i);
        do {
          setnilvalue(s2v(base + GETARG_A(i) + b));
        } while (b-- > 0);
        continue;
      }
      case OP_GETUPVAL: {
        int b = GETARG_B(i);
        setobj2s(L, RA(i), cl->upvals[b]->v.p);
        continue;
      }
      case OP_SETUPVAL: {
        UpVal *uv = cl->upvals[GETARG_B(i)];
        setobj(L, uv->v.p, s2v(RA(i)));
        aqlC_objbarrier(L, uv, s2v(RA(i)));
        continue;
      }
      case OP_EXTRAARG: {
        /* This instruction provides an extra argument for the previous instruction.
           The argument is encoded in the Ax field and should be processed by the 
           preceding instruction. This instruction itself does nothing. */
        aql_assert(0);  /* should never be executed directly */
        continue;
      }
      
      /* Arithmetic operations */
      case OP_ADD: ARITH_OP(ADD);
      case OP_ADDK: ARITH_OP_K(ADD);
      case OP_ADDI: ARITH_OP_I(ADD);
      case OP_SUB: ARITH_OP(SUB);
      case OP_SUBK: ARITH_OP_K(SUB);
      case OP_SUBI: ARITH_OP_I(SUB);
      case OP_MUL: ARITH_OP(MUL);
      case OP_MULK: ARITH_OP_K(MUL);
      case OP_MULI: ARITH_OP_I(MUL);
      case OP_DIV: ARITH_OP(DIV);
      case OP_DIVK: ARITH_OP_K(DIV);
      case OP_DIVI: ARITH_OP_I(DIV);
      case OP_MOD: ARITH_OP(MOD);
      case OP_POW: ARITH_OP(POW);
      
      /* Unary operations */
      case OP_UNM: {
        if (ttisinteger(s2v(RB(i)))) {
          setivalue(s2v(RA(i)), intop(-, 0, ivalue(s2v(RB(i)))));
        } else if (ttisfloat(s2v(RB(i)))) {
          setfltvalue(s2v(RA(i)), -fltvalue(s2v(RB(i))));
        } else {
          aqlG_runerror(L, "attempt to negate a %s", aqlL_typename(L, s2v(RB(i))));
        }
        continue;
      }
      case OP_LEN: {
        const TValue *rb = s2v(RB(i));
        StkId ra = RA(i);
        
        switch (ttypetag(rb)) {
          case AQL_VSHRSTR: case AQL_VLNGSTR: {
            setivalue(s2v(ra), (aql_Integer)tsvalue(rb)->shrlen);
            break;
          }
          case AQL_TARRAY: {
            setivalue(s2v(ra), (aql_Integer)arrvalue(rb)->length);
            break;
          }
          case AQL_TSLICE: {
            setivalue(s2v(ra), (aql_Integer)slicevalue(rb)->length);
            break;
          }
          case AQL_TDICT: {
            setivalue(s2v(ra), (aql_Integer)dictvalue(rb)->length);
            break;
          }
          case AQL_TVECTOR: {
            setivalue(s2v(ra), (aql_Integer)vectorvalue(rb)->length);
            break;
          }
          default: {
            aqlG_runerror(L, "attempt to get length of a %s value", 
                          aqlL_typename(L, rb));
            break;
          }
        }
        continue;
      }
      
      /* Bitwise operations */
      case OP_BAND: BITWISE_OP(BAND);
      case OP_BOR: BITWISE_OP(BOR);
      case OP_BXOR: BITWISE_OP(BXOR);
      case OP_SHL: BITWISE_OP(SHL);
      case OP_SHR: BITWISE_OP(SHR);
      case OP_BNOT: {
        if (ttisinteger(s2v(RB(i)))) {
          setivalue(s2v(RA(i)), ~ivalue(s2v(RB(i))));
        } else {
          aqlG_runerror(L, "attempt to perform bitwise operation on a %s",
                        aqlL_typename(L, s2v(RB(i))));
        }
        continue;
      }
      case OP_NOT: {
        setbvalue(s2v(RA(i)), !aqlV_toboolean(s2v(RB(i))));
        continue;
      }
      case OP_SHRI: {
        if (ttisinteger(s2v(RB(i)))) {
          setivalue(s2v(RA(i)), aqlV_shiftl(ivalue(s2v(RB(i))), -GETARG_C(i)));
        } else {
          aqlG_runerror(L, "attempt to perform bitwise operation on a %s",
                        aqlL_typename(L, s2v(RB(i))));
        }
        continue;
      }
      
      /* Comparison operations */
      case OP_EQ: CMP_OP(aqlV_equalobj);
      case OP_LT: CMP_OP(aqlV_lessthan_internal);
      case OP_LE: CMP_OP(aqlV_lessequal_internal);
      case OP_EQI: {
        TValue *ra = s2v(RA(i));
        aql_Integer imm = GETARG_B(i);
        int cond = (ttisinteger(ra) && ivalue(ra) == imm) == GETARG_C(i);
        if (cond) pc++;
        continue;
      }
      case OP_LTI: {
        TValue *ra = s2v(RA(i));
        aql_Integer imm = GETARG_B(i);
        int cond = (ttisinteger(ra) && ivalue(ra) < imm) == GETARG_C(i);
        if (cond) pc++;
        continue;
      }
      case OP_TEST: {
        TValue *ra = s2v(RA(i));
        int cond = (!aqlV_toboolean(ra)) == GETARG_B(i);
        if (cond) pc++;
        continue;
      }
      case OP_TESTSET: {
        TValue *rb = s2v(RB(i));
        int cond = (!aqlV_toboolean(rb)) == GETARG_C(i);
        if (cond) {
          pc++;
        } else {
          setobj2s(L, RA(i), rb);
        }
        continue;
      }
      
      /* Jumps */
      case OP_JMP: {
        pc += iAsBx(i);
        continue;
      }
      
      /* AQL Container operations */
      case OP_NEWOBJECT: {
        int type = GETARG_B(i);
        int size = GETARG_C(i);
        
        switch (type) {
          case 0: /* array<int> */
            create_array(L, RA(i), size, AQL_DATA_TYPE_INT32);
            break;
          case 1: /* slice<string> */
            create_slice(L, RA(i), size, AQL_TSTRING);
            break;
          case 2: /* dict<string,int> */
            create_dict(L, RA(i), size, AQL_DATA_TYPE_STRING, AQL_DATA_TYPE_INT32);
            break;
          case 3: /* vector<float> */
            create_vector(L, RA(i), size, AQL_DATA_TYPE_FLOAT32);
            break;
          default:
            create_array(L, RA(i), size, AQL_TNIL);
            break;
        }
        continue;
      }
      case OP_GETPROP: {
        TValue *obj = s2v(RB(i));
        TValue *key = s2v(RC(i));
        
        if (ttisarray(obj)) {
          aql_Integer idx;
          if (tointeger(key, &idx)) {
            aqlV_getarray(L, obj, cast_int(idx), RA(i));
          } else {
            setnilvalue(s2v(RA(i)));
          }
        } else if (ttisslice(obj)) {
          aql_Integer idx;
          if (tointeger(key, &idx)) {
            aqlV_getslice(L, obj, cast_int(idx), RA(i));
          } else {
            setnilvalue(s2v(RA(i)));
          }
        } else if (ttisdict(obj)) {
          aqlV_getdict(L, obj, key, RA(i));
        } else if (ttisvector(obj)) {
          aql_Integer idx;
          if (tointeger(key, &idx)) {
            aqlV_getvector(L, obj, key, RA(i));
        } else {
            setnilvalue(s2v(RA(i)));
          }
        } else {
          setnilvalue(s2v(RA(i)));
        }
        continue;
      }
      case OP_SETPROP: {
        TValue *obj = s2v(RA(i));
        TValue *key = s2v(RB(i));
        TValue *val = s2v(RC(i));
        
        if (ttisarray(obj)) {
          aql_Integer idx;
          if (tointeger(key, &idx)) {
            aqlV_setarray(L, obj, cast_int(idx), val);
          }
        } else if (ttisslice(obj)) {
          aql_Integer idx;
          if (tointeger(key, &idx)) {
            aqlV_setslice(L, obj, cast_int(idx), val);
          }
        } else if (ttisdict(obj)) {
          aqlV_setdict(L, obj, key, val);
        } else if (ttisvector(obj)) {
          aql_Integer idx;
          if (tointeger(key, &idx)) {
            aqlV_setvector(L, obj, key, val);
          }
        }
        continue;
      }
      
      /* Function calls and returns */
      case OP_CALL: {
        int b = GETARG_B(i);
        int nresults = GETARG_C(i) - 1;
        if (b != 0) L->top = RA(i) + b;  /* else previous instruction set top */
        if (aqlD_precall(L, RA(i), nresults)) {  /* C function? */
          if (nresults >= 0)
            L->top = ci->top;  /* adjust results */
          /* else previous instruction set top */
          base = ci->func + 1;
        } else {  /* AQL function */
          ci = L->ci;
          goto newframe;  /* restart aqlV_execute over new Lua function */
        }
        continue;
      }
      case OP_TAILCALL: {
        int b = GETARG_B(i);
        if (b != 0) L->top = RA(i) + b;
        aqlD_pretailcall(L, ci, RA(i), b - 1, 0);  /* add missing parameters */
        goto newframe;  /* restart aqlV_execute over new Lua function */
      }
      case OP_RET: 
        RET_OP(
          {
        int b = GETARG_B(i);
        if (b != 0) L->top = RA(i) + b - 1;
          },
          GETARG_A(i), GETARG_B(i)
        );
      case OP_RET_VOID: 
        RET_OP(L->top = RA(i), GETARG_A(i), 1);
      case OP_RET_ONE: 
        RET_OP(L->top = RA(i) + 1, GETARG_A(i), 2);
      
      /* Closures */
      case OP_CLOSURE: {
        Proto *p = cl->p->p[iABx(i)];
        /* TODO: Implement aqlF_pushclosure for closure creation */
        aqlG_runerror(L, "closure creation not yet implemented");
        continue;
      }
      
      /* Iteration */
      case OP_FORLOOP: {
        StkId ra = RA(i);
        if (ttisinteger(s2v(ra))) {
          aql_Integer idx = ivalue(s2v(ra));
          setivalue(s2v(ra), idx + 1);
          pc += iAsBx(i);
        }
        continue;
      }
      case OP_FORPREP: {
        StkId ra = RA(i);
        TValue *init = s2v(ra);
        TValue *limit = s2v(ra + 1);
        TValue *step = s2v(ra + 2);
        
        /* Check if we should run the loop at all */
        aql_Integer init_val = ttisinteger(init) ? ivalue(init) : 0;
        aql_Integer limit_val = ttisinteger(limit) ? ivalue(limit) : 0;
        aql_Integer step_val = ttisinteger(step) ? ivalue(step) : 1;
        
        if ((step_val > 0 && init_val <= limit_val) ||
            (step_val < 0 && init_val >= limit_val)) {
          /* Loop will run, continue to loop body */
          pc += iAsBx(i);
        } else {
          /* Loop won't run, skip to end */
          pc += iAsBx(i) + 1;
        }
        continue;
      }
      
      /* Extended operations */
      case OP_YIELD: {
        /* TODO: Implement aqlD_yield for coroutine yielding */
        aqlG_runerror(L, "yield operation not yet implemented");
        return 0;
      }
      
      case OP_BUILTIN: {
        int builtin_id = GETARG_B(i);
        TValue *arg1 = s2v(RC(i));
        TValue *arg2 = s2v(base + GETARG_Ax(*pc++));
        
        switch (builtin_id) {
          case 0: /* print */
            /* TODO: Implement aqlB_print */
            aqlG_runerror(L, "print builtin not yet implemented");
            break;
          case 1: /* type */
            /* TODO: Implement aqlB_type */
            aqlG_runerror(L, "type builtin not yet implemented");
            break;
          case 2: /* len */
            /* TODO: Implement aqlB_len */
            aqlG_runerror(L, "len builtin not yet implemented");
            break;
          case 3: /* tostring */
            /* TODO: Implement aqlB_tostring */
            aqlG_runerror(L, "tostring builtin not yet implemented");
            break;
          case 4: /* tonumber */
            /* TODO: Implement aqlB_tonumber */
            aqlG_runerror(L, "tonumber builtin not yet implemented");
            break;
          default:
            setnilvalue(s2v(RA(i)));
            break;
        }
        continue;
      }
      
      case OP_VARARG: {
        int nvar = cl->p->is_vararg;
        int n = GETARG_C(i);
        int nparams = cl->p->numparams;
        
        if (nvar) {
          /* Get actual varargs from the call frame */
          int nfixparams = cl->p->numparams;
          CallInfo *previous_ci = ci - 1;
          StkId previous_base = previous_ci->func + 1;
          int nactual = cast_int(ci->func - previous_base) - nfixparams - 1;
          
          if (n == 0) n = nactual;
          
          for (int j = 0; j < n && j < nactual; j++) {
            setobj2s(L, RA(i) + j, s2v(previous_base + nfixparams + 1 + j));
          }
          for (int j = nactual; j < n; j++) {
            setnilvalue(s2v(RA(i) + j));
          }
        } else {
          /* No varargs available */
          if (n == 0) n = 0;
          for (int j = 0; j < n; j++) {
            setnilvalue(s2v(RA(i) + j));
          }
        }
        continue;
      }
      
      /* Upvalue and table operations */
      case OP_GETTABUP: {
        const TValue *upval = cl->upvals[GETARG_B(i)]->v.p;
        TValue *key = k + GETARG_C(i);
        
        if (ttisarray(upval)) {
          aql_Integer idx;
          if (tointeger(key, &idx)) {
            aqlV_getarray(L, upval, cast_int(idx), RA(i));
          } else {
            setnilvalue(s2v(RA(i)));
          }
        } else if (ttisslice(upval)) {
          aql_Integer idx;
          if (tointeger(key, &idx)) {
            aqlV_getslice(L, upval, cast_int(idx), RA(i));
          } else {
            setnilvalue(s2v(RA(i)));
          }
        } else if (ttisdict(upval)) {
          aqlV_getdict(L, upval, key, RA(i));
        } else if (ttisvector(upval)) {
          aql_Integer idx;
          if (tointeger(key, &idx)) {
            aqlV_getvector(L, upval, key, RA(i));
          } else {
            setnilvalue(s2v(RA(i)));
          }
        } else {
          aqlG_runerror(L, "attempt to index a %s value", aqlL_typename(L, upval));
        }
        continue;
      }
      case OP_SETTABUP: {
        TValue *upval = cl->upvals[GETARG_A(i)]->v.p;
        TValue *key = k + GETARG_B(i);
        TValue *val = k + GETARG_C(i);
        
        if (ttisarray(upval)) {
          aql_Integer idx;
          if (tointeger(key, &idx)) {
            aqlV_setarray(L, upval, cast_int(idx), val);
          }
        } else if (ttisslice(upval)) {
          aql_Integer idx;
          if (tointeger(key, &idx)) {
            aqlV_setslice(L, upval, cast_int(idx), val);
          }
        } else if (ttisdict(upval)) {
          aqlV_setdict(L, upval, key, val);
        } else if (ttisvector(upval)) {
          aql_Integer idx;
          if (tointeger(key, &idx)) {
            aqlV_setvector(L, upval, key, val);
          }
        } else {
          aqlG_runerror(L, "attempt to index a %s value", aqlL_typename(L, upval));
        }
        continue;
      }
      case OP_CLOSE: {
        aqlF_close(L, base + GETARG_A(i), 0);
        continue;
      }
      case OP_TBC: {
        aqlF_newtbcupval(L, base + GETARG_A(i));
        continue;
      }
      case OP_CONCAT: {
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        L->top = RA(i) + b;
        aqlV_concat(L, c - b + 1);
        setobj2s(L, RA(i), s2v(L->top - 1));
        L->top = ci->top;
        continue;
      }
      
      /* Method invocation - Container separation version */
      case OP_INVOKE: {
        TValue *obj = s2v(RB(i));
        TValue *method_name = k + GETARG_C(i);
        
        if (ttisdict(obj)) {
          /* Dict objects can have method-like access */
          const TValue *method_res = aqlD_get(dictvalue(obj), method_name);
          if (method_res != NULL && ttisfunction(method_res)) {
            /* Set up method call with self parameter */
            setobj2s(L, RA(i) + 1, obj);  /* self parameter */
            setobj2s(L, RA(i), method_res);
            /* Continue with OP_CALL for actual call */
            continue;
          } else {
            aqlG_runerror(L, "method '%s' not found in dict", svalue(method_name));
          }
        } else if (ttisarray(obj) || ttisslice(obj) || ttisvector(obj)) {
          /* Arrays, slices, and vectors are data containers, not objects with methods */
          aqlG_runerror(L, "attempt to call method on data container %s", aqlL_typename(L, obj));
        } else {
          aqlG_runerror(L, "attempt to call method on %s", aqlL_typename(L, obj));
        }
        continue;
      }
      
      /* Coroutine operations */
      case OP_RESUME: {
        TValue *co = s2v(RB(i));
        if (ttisthread(co)) {
          aql_State *co_state = thvalue(co);
          int nresults;
          int status = aql_resume(co_state, NULL, GETARG_C(i) - 1, &nresults);
          if (status == AQL_YIELD || status == AQL_OK) {
            StkId res = RA(i);
            for (int j = 0; j < nresults && j < GETARG_C(i) - 1; j++) {
              setobj2s(L, res + j, s2v(co_state->stack + j));
            }
          }
        } else {
          aqlG_runerror(L, "attempt to resume a %s", aqlL_typename(L, co));
        }
        continue;
      }
      
      default: {
        aql_assert(0);
        return 0;
      }
    }
newframe:
    cl = clLvalue(s2v(ci->func));
    base = ci->func + 1;
    pc = ci->u.l.savedpc;
    k = cl->p->k;
  }
}

/*
** {==================================================================
** Helper Functions for VM Operations
** ==================================================================
*/

/* 
** aqlV_settable removed - AQL uses container-specific access functions:
** aqlV_setarray, aqlV_setslice, aqlV_setdict, aqlV_setvector 
*/

/*
** Optimized string concatenation with pre-calculated total length
** Reduces from O(n) allocations to O(1) allocation
** Reduces from O(n²) copies to O(n) copies
*/
AQL_API void aqlV_concat (aql_State *L, int n) {
  if (n <= 1) return;  /* nothing to concatenate */
  
  StkId top = L->top;
  StkId base = top - n;
  
  /* Phase 1: Convert all values to strings and calculate total length */
  size_t total_len = 0;
  for (int i = 0; i < n; i++) {
    StkId o = base + i;
    if (!(ttisstring(s2v(o)) || (aqlO_tostring(L, s2v(o)), 1))) {
      aqlG_typeerror(L, s2v(o), "concatenate");
    }
    size_t len = vslen(s2v(o));
    
    /* Check for overflow */
    if (total_len > SIZE_MAX - len) {
      aqlG_runerror(L, "string length overflow in concatenation");
    }
    total_len += len;
  }
  
  /* Phase 2: Create result string and copy all parts */
  if (total_len == 0) {
    /* All strings are empty - return empty string */
    TString *empty = aqlStr_newlstr(L, "", 0);
    setsvalue2s(L, base, empty);
  } else {
    /* Single allocation for final result */
    TString *result = aqlS_createlngstrobj(L, total_len);
    char *buffer = getstr(result);
    char *pos = buffer;
    
    /* Sequential copy - no overlapping, maximum cache efficiency */
    for (int i = 0; i < n; i++) {
      StkId o = base + i;
      size_t len = vslen(s2v(o));
      if (len > 0) {
        memcpy(pos, svalue(s2v(o)), len);
        pos += len;
      }
    }
    
    aql_assert(pos == buffer + total_len);
    setsvalue2s(L, base, result);
  }
  
  /* Adjust stack top */
  L->top = base + 1;
}

/*
** {==================================================================
** Arithmetic Helper Functions
** ==================================================================
*/

AQL_API aql_Integer aqlV_idiv (aql_State *L, aql_Integer m, aql_Integer n) {
  if (l_castS2U(n) + 1u <= 1u) {  /* special cases: -1 or 0 */
    if (n == 0)
      aqlG_runerror(L, "attempt to divide by zero");
    return intop(-, 0, m);   /* m / -1 == -m */
  } else {
    return m / n;
  }
}

AQL_API aql_Integer aqlV_mod (aql_State *L, aql_Integer m, aql_Integer n) {
  if (l_castS2U(n) + 1u <= 1u) {  /* special cases: -1 or 0 */
    if (n == 0)
      aqlG_runerror(L, "attempt to perform 'n%%0'");
    return 0;   /* m % -1 == 0 */
  } else {
    return m % n;
  }
}

AQL_API aql_Number aqlV_modf (aql_State *L, aql_Number m, aql_Number n) {
  if (n == 0.0)
    aqlG_runerror(L, "attempt to perform 'n%%0'");
  return m - floor(m/n) * n;
}

AQL_API aql_Integer aqlV_shiftl (aql_Integer x, aql_Integer y) {
  if (y < 0) {  /* shift right? */
    if (y <= -(aql_Integer)AQL_INTEGER_BITS) return 0;
    else return intop(>>, x, -y);
  } else {  /* shift left */
    if (y >= (aql_Integer)AQL_INTEGER_BITS) return 0;
    else return intop(<<, x, y);
  }
}

AQL_API aql_Integer aqlV_shiftr (aql_Integer x, aql_Integer y) {
  if (y < 0) {  /* shift left? */
    if (y <= -(aql_Integer)AQL_INTEGER_BITS) return 0;
    else return intop(<<, x, -y);
  } else {  /* shift right */
    if (y >= (aql_Integer)AQL_INTEGER_BITS) return 0;
    else return intop(>>, x, y);
  }
}

/*
** {==================================================================
** Precise Number Comparison Functions (Lua 5.4 style)
** ==================================================================
*/

/*
** Check whether integer 'i' is less than float 'f'. If 'i' has an
** exact representation as a float ('l_intfitsf'), compare numbers as
** floats. Otherwise, use the equivalence 'i < f <=> i < ceil(f)'.
** If 'ceil(f)' is out of integer range, either 'f' is greater than
** all integers or less than all integers.
*/
static int LTintfloat (aql_Integer i, aql_Number f) {
  if (l_intfitsf(i))
    return aql_numlt(cast_num(i), f);  /* compare them as floats */
  else {  /* i < f <=> i < ceil(f) */
    aql_Integer fi;
    if (aqlV_flttointeger(f, &fi, F2Iceil))  /* fi = ceil(f) */
      return i < fi;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f > 0;  /* greater? */
  }
}

/*
** Check whether integer 'i' is less than or equal to float 'f'.
*/
static int LEintfloat (aql_Integer i, aql_Number f) {
  if (l_intfitsf(i))
    return aql_numle(cast_num(i), f);  /* compare them as floats */
  else {  /* i <= f <=> i <= floor(f) */
    aql_Integer fi;
    if (aqlV_flttointeger(f, &fi, F2Ifloor))  /* fi = floor(f) */
      return i <= fi;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f > 0;  /* greater? */
  }
}

/*
** Check whether float 'f' is less than integer 'i'.
*/
static int LTfloatint (aql_Number f, aql_Integer i) {
  if (l_intfitsf(i))
    return aql_numlt(f, cast_num(i));  /* compare them as floats */
  else {  /* f < i <=> floor(f) < i */
    aql_Integer fi;
    if (aqlV_flttointeger(f, &fi, F2Ifloor))  /* fi = floor(f) */
      return fi < i;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f < 0;  /* less? */
  }
}

/*
** Check whether float 'f' is less than or equal to integer 'i'.
*/
static int LEfloatint (aql_Number f, aql_Integer i) {
  if (l_intfitsf(i))
    return aql_numle(f, cast_num(i));  /* compare them as floats */
  else {  /* f <= i <=> ceil(f) <= i */
    aql_Integer fi;
    if (aqlV_flttointeger(f, &fi, F2Iceil))  /* fi = ceil(f) */
      return fi <= i;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f < 0;  /* less? */
  }
}

/*
** Return 'l < r', for numbers.
*/
static int LTnum (const TValue *l, const TValue *r) {
  aql_assert(ttisnumber(l) && ttisnumber(r));
  if (ttisinteger(l)) {
    aql_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li < ivalue(r);  /* both are integers */
    else  /* 'l' is int and 'r' is float */
      return LTintfloat(li, fltvalue(r));  /* l < r ? */
  }
  else {
    aql_Number lf = fltvalue(l);  /* 'l' must be float */
    if (ttisfloat(r))
      return aql_numlt(lf, fltvalue(r));  /* both are float */
    else  /* 'l' is float and 'r' is int */
      return LTfloatint(lf, ivalue(r));
  }
}

/*
** Return 'l <= r', for numbers.
*/
static int LEnum (const TValue *l, const TValue *r) {
  aql_assert(ttisnumber(l) && ttisnumber(r));
  if (ttisinteger(l)) {
    aql_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li <= ivalue(r);  /* both are integers */
    else  /* 'l' is int and 'r' is float */
      return LEintfloat(li, fltvalue(r));  /* l <= r ? */
  }
  else {
    aql_Number lf = fltvalue(l);  /* 'l' must be float */
    if (ttisfloat(r))
      return aql_numle(lf, fltvalue(r));  /* both are float */
    else  /* 'l' is float and 'r' is int */
      return LEfloatint(lf, ivalue(r));
  }
}

/*
** Compare two strings 'ts1' x 'ts2', returning an integer less-equal-
** greater than zero if 'ts1' is less-equal-greater than 'ts2'.
** The code uses 'strcoll' to respect locales for each segment.
*/
static int l_strcmp (const TString *ts1, const TString *ts2) {
  const char *s1 = getstr(ts1);
  size_t rl1 = tsslen(ts1);  /* real length */
  const char *s2 = getstr(ts2);
  size_t rl2 = tsslen(ts2);
  for (;;) {  /* for each segment */
    int temp = strcoll(s1, s2);
    if (temp != 0)  /* not equal? */
      return temp;  /* done */
    else {  /* strings are equal up to a '\0' */
      size_t zl1 = strlen(s1);  /* index of first '\0' in 's1' */
      size_t zl2 = strlen(s2);  /* index of first '\0' in 's2' */
      if (zl2 == rl2)  /* 's2' is finished? */
        return (zl1 == rl1) ? 0 : 1;  /* check 's1' */
      else if (zl1 == rl1)  /* 's1' is finished? */
        return -1;  /* 's1' is less than 's2' ('s2' is not finished) */
      /* both strings longer than 'zl'; go on comparing after the '\0' */
      zl1++; zl2++;
      s1 += zl1; rl1 -= zl1; s2 += zl2; rl2 -= zl2;
    }
  }
}

/*
** return 'l < r' for non-numbers.
*/
static int lessthanothers (aql_State *L, const TValue *l, const TValue *r) {
  aql_assert(!ttisnumber(l) || !ttisnumber(r));
  if (ttisstring(l) && ttisstring(r))  /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) < 0;
  else
    return aqlG_ordererror(L, l, r);  /* no metamethod support yet */
}

/*
** return 'l <= r' for non-numbers.
*/
static int lessequalothers (aql_State *L, const TValue *l, const TValue *r) {
  aql_assert(!ttisnumber(l) || !ttisnumber(r));
  if (ttisstring(l) && ttisstring(r))  /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) <= 0;
  else
    return aqlG_ordererror(L, l, r);  /* no metamethod support yet */
}

/*
** {==================================================================
** Object Comparison Functions
** ==================================================================
*/

AQL_API int aqlV_equalobj (aql_State *L, const TValue *t1, const TValue *t2) {
  const TValue *tm;
  if (ttypetag(t1) != ttypetag(t2)) {
    if (ttype(t1) != ttype(t2) || ttype(t1) != AQL_TNUMBER)
      return 0;  /* different types and not numbers */
    else {
      aql_Number v1, v2;
      return (tonumber(t1, &v1) && tonumber(t2, &v2) && aql_numeq(v1, v2));
    }
  }
  /* values have same type and same variant */
  switch (ttypetag(t1)) {
    case AQL_TNIL: return 1;
    case AQL_TBOOLEAN: return bvalue(t1) == bvalue(t2);
    case AQL_VNUMINT: return ivalue(t1) == ivalue(t2);
    case AQL_VNUMFLT: return aql_numeq(fltvalue(t1), fltvalue(t2));
    case AQL_TSTRING: return eqshrstr(tsvalue(t1), tsvalue(t2));
    case AQL_TARRAY: {
      Array *a1 = arrvalue(t1);
      Array *a2 = arrvalue(t2);
      
      /* Fast path: same object pointer */
      if (a1 == a2) return 1;
      
      /* Length check first */
      if (a1->length != a2->length) return 0;
      if (a1->length == 0) return 1;  /* both empty */
      
      /* Data type consistency check */
      if (a1->dtype != a2->dtype) return 0;
      
      /* Type-specialized comparison for performance */
      return aqlV_compare_array_fast(L, a1, a2);
    }
    case AQL_TSLICE: {
      Slice *s1 = slicevalue(t1);
      Slice *s2 = slicevalue(t2);
      
      /* Fast path: same object pointer */
      if (s1 == s2) return 1;
      
      /* Length check first */
      if (s1->length != s2->length) return 0;
      if (s1->length == 0) return 1;  /* both empty */
      
      /* Data type consistency check */
      if (s1->dtype != s2->dtype) return 0;
      
      /* Type-specialized comparison for performance */
      return aqlV_compare_slice_fast(L, s1, s2);
    }
    case AQL_TDICT: {
      Dict *d1 = dictvalue(t1);
      Dict *d2 = dictvalue(t2);
      
      /* Fast path: same object pointer */
      if (d1 == d2) return 1;
      
      /* Size check first */
      if (d1->length != d2->length) return 0;
      if (d1->length == 0) return 1;  /* both empty */
      
      /* Type compatibility check */
      if (d1->key_type != d2->key_type || d1->value_type != d2->value_type) return 0;
      
      return aqlV_compare_dict_fast(L, d1, d2);
    }
    case AQL_TVECTOR: {
      Vector *v1 = vectorvalue(t1);
      Vector *v2 = vectorvalue(t2);
      
      /* Fast path: same object pointer */
      if (v1 == v2) return 1;
      
      /* Length check first */
      if (v1->length != v2->length) return 0;
      if (v1->length == 0) return 1;  /* both empty */
      
      /* Data type consistency check */
      if (v1->dtype != v2->dtype) return 0;
      
      /* Type-specialized comparison for performance */
      return aqlV_compare_vector_fast(L, v1, v2);
    }
    default: return aqlV_equalobj(L, t1, t2);  /* use generic comparison */
  }
  return 0;  /* cannot happen */
}

/*
** Main operation less than; return 'l < r'.
*/
AQL_API int aqlV_lessthan (aql_State *L, const TValue *l, const TValue *r) {
  if (ttisnumber(l) && ttisnumber(r))  /* both operands are numbers? */
    return LTnum(l, r);
  else 
    return lessthanothers(L, l, r);
}

/*
** Main operation less than or equal to; return 'l <= r'.
*/
AQL_API int aqlV_lessequal (aql_State *L, const TValue *l, const TValue *r) {
  if (ttisnumber(l) && ttisnumber(r))  /* both operands are numbers? */
    return LEnum(l, r);
  else 
    return lessequalothers(L, l, r);
}

/*
** {==================================================================
** Conversion Functions
** ==================================================================
*/

AQL_API int aqlV_tonumber_ (const TValue *obj, aql_Number *n) {
  TValue v;
  if (ttisinteger(obj)) {
    *n = cast_num(ivalue(obj));
    return 1;
  } else if (ttisfloat(obj)) {
    *n = fltvalue(obj);
    return 1;
  } else if (ttisstring(obj) && aqlO_str2num(svalue(obj), &v) == 1) {
    *n = (ttisinteger(&v)) ? cast_num(ivalue(&v)) : fltvalue(&v);
    return 1;
  }
  else
    return 0;  /* conversion failed */
}

AQL_API int aqlV_tointeger (const TValue *obj, aql_Integer *p, F2Imod mode) {
  aql_Number n;
  if (ttisinteger(obj)) {
    *p = ivalue(obj);
    return 1;
  } else if (ttisfloat(obj)) {
    n = fltvalue(obj);
    return aqlV_flttointeger(n, p, mode);
  } else if (ttisstring(obj)) {
    TValue temp;
    if (aqlO_str2num(svalue(obj), &temp) == 1) {
      if (ttisinteger(&temp)) {
        *p = ivalue(&temp);
      return 1;
    } else {
        n = fltvalue(&temp);
      return aqlV_flttointeger(n, p, mode);
      }
    }
  }
  return 0;  /* conversion failed */
}

AQL_API int aqlV_tointegerns (const TValue *obj, aql_Integer *p) {
  return aqlV_tointeger(obj, p, F2Ieq);
}

AQL_API int aqlV_flttointeger (aql_Number n, aql_Integer *p, F2Imod mode) {
  aql_Number f = l_floor(n);
  if (n != f) {  /* not an integral value? */
    switch (mode) {
      case F2Ieq: return 0;  /* fails if not an integral value */
      case F2Ifloor: break;  /* takes floor */
      case F2Iceil: f += 1; break;  /* takes ceil */
      default: aql_assert(0);
    }
  }
  return aql_numbertointeger(f, p);
}

/*
** {==================================================================
** Fast Container Comparison Functions
** ==================================================================
*/

/*
** Fast array comparison with type specialization
*/
static int aqlV_compare_array_fast (aql_State *L, const Array *a1, const Array *a2) {
  aql_assert(a1->length == a2->length && a1->dtype == a2->dtype);
  
  switch (a1->dtype) {
    case DT_INT: {
      /* Direct memory comparison for integers */
      for (size_t i = 0; i < a1->length; i++) {
        if (ivalue(&a1->data[i]) != ivalue(&a2->data[i])) return 0;
      }
      return 1;
    }
    case DT_FLOAT: {
      /* Float comparison with NaN handling */
      for (size_t i = 0; i < a1->length; i++) {
        aql_Number v1 = fltvalue(&a1->data[i]);
        aql_Number v2 = fltvalue(&a2->data[i]);
        if (!aql_numeq(v1, v2)) return 0;
      }
      return 1;
    }
    case DT_BOOL: {
      /* Direct boolean comparison */
      for (size_t i = 0; i < a1->length; i++) {
        if (bvalue(&a1->data[i]) != bvalue(&a2->data[i])) return 0;
      }
      return 1;
    }
    case DT_STRING: {
      /* String comparison optimization */
      for (size_t i = 0; i < a1->length; i++) {
        if (!eqshrstr(tsvalue(&a1->data[i]), tsvalue(&a2->data[i]))) return 0;
      }
      return 1;
    }
    default: {
      /* Fallback to recursive comparison for complex types */
      for (size_t i = 0; i < a1->length; i++) {
        if (!aqlV_equalobj(L, &a1->data[i], &a2->data[i])) return 0;
      }
      return 1;
    }
  }
}

/*
** Fast slice comparison with type specialization
*/
static int aqlV_compare_slice_fast (aql_State *L, const Slice *s1, const Slice *s2) {
  aql_assert(s1->length == s2->length && s1->dtype == s2->dtype);
  
  switch (s1->dtype) {
    case DT_INT: {
      for (size_t i = 0; i < s1->length; i++) {
        if (ivalue(&s1->data[i]) != ivalue(&s2->data[i])) return 0;
      }
      return 1;
    }
    case DT_FLOAT: {
      for (size_t i = 0; i < s1->length; i++) {
        aql_Number v1 = fltvalue(&s1->data[i]);
        aql_Number v2 = fltvalue(&s2->data[i]);
        if (!aql_numeq(v1, v2)) return 0;
      }
      return 1;
    }
    case DT_BOOL: {
      for (size_t i = 0; i < s1->length; i++) {
        if (bvalue(&s1->data[i]) != bvalue(&s2->data[i])) return 0;
      }
      return 1;
    }
    case DT_STRING: {
      for (size_t i = 0; i < s1->length; i++) {
        if (!eqshrstr(tsvalue(&s1->data[i]), tsvalue(&s2->data[i]))) return 0;
      }
      return 1;
    }
    default: {
      for (size_t i = 0; i < s1->length; i++) {
        if (!aqlV_equalobj(L, &s1->data[i], &s2->data[i])) return 0;
      }
      return 1;
    }
  }
}

/*
** Fast vector comparison with SIMD potential
*/
static int aqlV_compare_vector_fast (aql_State *L, const Vector *v1, const Vector *v2) {
  aql_assert(v1->length == v2->length && v1->dtype == v2->dtype);
  
  switch (v1->dtype) {
    case DT_INT: {
      /* Potential SIMD optimization for integer vectors */
      const aql_Integer *data1 = (const aql_Integer *)v1->data;
      const aql_Integer *data2 = (const aql_Integer *)v2->data;
      return memcmp(data1, data2, v1->length * sizeof(aql_Integer)) == 0;
    }
    case DT_FLOAT: {
      /* Float vector comparison with epsilon handling */
      const aql_Number *data1 = (const aql_Number *)v1->data;
      const aql_Number *data2 = (const aql_Number *)v2->data;
      for (size_t i = 0; i < v1->length; i++) {
        if (!aql_numeq(data1[i], data2[i])) return 0;
      }
      return 1;
    }
    case DT_BOOL: {
      /* Direct memory comparison for booleans */
      const char *data1 = (const char *)v1->data;
      const char *data2 = (const char *)v2->data;
      return memcmp(data1, data2, v1->length * sizeof(char)) == 0;
    }
    default: {
      /* Fallback for complex types */
      return 0;  /* Not implemented for complex vector types yet */
    }
  }
}

/*
** Fast dictionary comparison
*/
static int aqlV_compare_dict_fast (aql_State *L, const Dict *d1, const Dict *d2) {
  aql_assert(d1->length == d2->length);
  aql_assert(d1->key_type == d2->key_type && d1->value_type == d2->value_type);
  
  /* For each entry in dict1, find corresponding entry in dict2 */
  for (size_t i = 0; i < d1->capacity; i++) {
    const DictEntry *entry1 = &d1->entries[i];
    if (entry1->flags != DICT_OCCUPIED) continue;
    
    /* Find matching key in dict2 */
    int found = 0;
    for (size_t j = 0; j < d2->capacity; j++) {
      const DictEntry *entry2 = &d2->entries[j];
      if (entry2->flags != DICT_OCCUPIED) continue;
      
      if (aqlV_equalobj(L, &entry1->key, &entry2->key)) {
        if (!aqlV_equalobj(L, &entry1->value, &entry2->value)) {
          return 0;  /* Same key, different value */
        }
        found = 1;
        break;
      }
    }
    
    if (!found) return 0;  /* Key not found in dict2 */
  }
  
  return 1;
}

/*
** {==================================================================
** Concatenation
** ==================================================================
*/




/*
** {==================================================================
** VM Execution Control
** ==================================================================
*/

AQL_API void aqlV_finishOp (aql_State *L) {
  /* Placeholder for any post-execution cleanup */
  (void)L;
}

/*
** {==================================================================
** Metamethod Support (Phase 1 - Minimal Implementation)
** ==================================================================
*/

/*
** Finish a table access when the fast access fails.
** In AQL Phase 1, we implement minimal metamethod support
** focusing on clear error messages for type-safe containers.
*/
AQL_API void aqlV_finishget (aql_State *L, const TValue *t, TValue *key,
                            StkId val, const TValue *slot) {
  if (slot != NULL) {
    /* Fast path succeeded late, just copy the value */
    setobj2s(L, val, slot);
    return;
  }
  
  /* Phase 1: Simple error handling for type-safe containers */
  if (ttisarray(t)) {
    setnilvalue(s2v(val));  /* Array access out of bounds returns nil */
  } else if (ttisslice(t)) {
    setnilvalue(s2v(val));  /* Slice access out of bounds returns nil */
  } else if (ttisdict(t)) {
    setnilvalue(s2v(val));  /* Dict key not found returns nil */
  } else if (ttisvector(t)) {
    setnilvalue(s2v(val));  /* Vector access out of bounds returns nil */
  } else {
    /* Unsupported type for indexing */
    aqlG_typeerror(L, t, "indexable container");
  }
}

/*
** Finish a table assignment when the fast access fails.
** In AQL Phase 1, we implement strict container boundaries
** to maintain type safety and performance.
*/
AQL_API void aqlV_finishset (aql_State *L, const TValue *t, TValue *key,
                            TValue *val, const TValue *slot) {
  UNUSED(slot);  /* Currently not used in Phase 1 implementation */
  UNUSED(key);   /* Key validation done in fast path */
  UNUSED(val);   /* Value validation done in fast path */
  
  /* Phase 1: Strict container type checking */
  if (ttisarray(t)) {
    aqlG_runerror(L, "attempt to modify fixed-size array out of bounds");
  } else if (ttisvector(t)) {
    aqlG_runerror(L, "attempt to modify fixed-size vector out of bounds");
  } else if (ttisslice(t)) {
    aqlG_runerror(L, "attempt to access slice out of bounds");
  } else if (ttisdict(t)) {
    aqlG_runerror(L, "attempt to access dict with invalid key");
  } else {
    /* Unsupported type for assignment */
    aqlG_typeerror(L, t, "assignable container");
  }
}

/* }================================================================== */

/*
** {==================================================================
** Copyright Notice
** ==================================================================
**
** Copyright (C) 2024 AQL Team. All rights reserved.
**
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/