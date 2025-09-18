/*
** $Id: avm.c $
** AQL virtual machine
** See Copyright Notice in aql.h
*/

#define avm_c
#define AQL_CORE


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>

#include "aconf.h"
#include "adebug_user.h"
#include "aparser.h"

/* Debug build macro for conditional compilation */
#ifdef AQL_DEBUG_BUILD
#define AQL_DEBUG_BUILD_ONLY(code) do { code } while(0)
#else
#define AQL_DEBUG_BUILD_ONLY(code) do { } while(0)
#endif

/* Register tracking function - moved after includes */

#include "aql.h"
#include "aopcodes.h"
#include "avm.h"
#include "azio.h"
#include "arange.h"

/* Execution tracing control */
#ifdef AQL_DEBUG_BUILD
static int trace_execution = 1;  /* Enable execution tracing by default in debug builds */
#endif

/* Register tracking function */
#ifdef AQL_DEBUG_BUILD
static void debug_trace_registers(aql_State *L, StkId base, const Instruction *pc, const Proto *p) {
  if (aql_debug_enabled && (aql_debug_flags & AQL_DEBUG_REG)) {
    int active_regs = (int)(L->top - base);
    int max_stack_size = p->maxstacksize;
    int reg_count = (max_stack_size > 4) ? 4 : max_stack_size; /* Limit to 4 registers */
    
    /* Ensure we show at least the first register if maxstacksize > 0 */
    if (reg_count == 0 && max_stack_size > 0) reg_count = 1;
    
    printf("📊 Register State (PC=%d, Active=%d, MaxStack=%d, L->top=%p, base=%p):\n", 
           (int)(pc - p->code), active_regs, max_stack_size, (void*)L->top, (void*)base);
    
    for (int j = 0; j < reg_count; j++) {
      printf("  R[%d]: ", j);
      
      /* Always try to read the register value, regardless of active_regs */
      TValue *reg = s2v(base + j);
      if (ttisnil(reg)) {
        printf("nil\n");
      } else if (ttisinteger(reg)) {
        printf("%lld (integer)\n", (long long)ivalue(reg));
      } else if (ttisfloat(reg)) {
        printf("%.6g (float)\n", fltvalue(reg));
      } else if (ttisboolean(reg)) {
        printf("%s (boolean)\n", bvalue(reg) ? "true" : "false");
      } else if (ttisstring(reg)) {
        printf("\"%s\" (string)\n", getstr(tsvalue(reg)));
      } else {
        printf("<type %d> (raw=%d)\n", rawtt(reg), rawtt(reg));
      }
    }
    printf("\n");
  }
}
#else
#define debug_trace_registers(L, base, pc, p) ((void)0)
#endif
#include "ado.h"
#include "agc.h"
#include "amem.h"
#include "astring.h"
#include "adatatype.h"
#include "aarray.h"
#include "aslice.h"
#include "adict.h"

/* Forward declaration for global dict access */
extern Dict *get_globals_dict(aql_State *L);
#include "avector.h"

/* Missing function declarations */
static int aqlV_toboolean(const TValue *obj);
/* Container access functions are in their respective header files */

/*
** Convert TValue to boolean according to AQL rules:
** - nil, false, and 0 are falsy
** - everything else is truthy (including empty string, empty containers)
*/
static int aqlV_toboolean(const TValue *obj) {
  switch (rawtt(obj)) {
    case AQL_TNIL: return 0;
    case AQL_TBOOLEAN: return bvalue(obj);
    case AQL_VNUMINT: return ivalue(obj) != 0;  /* 0 is falsy, non-zero is truthy */
    case AQL_VNUMFLT: return fltvalue(obj) != 0.0;  /* 0.0 is falsy, non-zero is truthy */
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

/////* Extract k flag from instruction (for negated comparisons) */
//#define GETARG_k(i)	(GETARG_C(i) & 1)

/* Safe register access with bounds checking */
static StkId safe_RA(StkId base, Instruction i, aql_State *L) {
  int reg = GETARG_A(i);
  StkId result = base + reg;
  if (result < L->stack || result >= L->stack_last) {
    printf_debug("[ERROR] RA register %d out of bounds\n", reg);
    return NULL;
  }
  return result;
}

static StkId safe_RB(StkId base, Instruction i, aql_State *L) {
  int reg = GETARG_B(i);
  StkId result = base + reg;
  if (result < L->stack || result >= L->stack_last) {
    printf_debug("[ERROR] RB register %d out of bounds\n", reg);
    return NULL;
  }
  return result;
}

static StkId safe_RC(StkId base, Instruction i, aql_State *L) {
  int reg = GETARG_C(i);
  StkId result = base + reg;
  if (result < L->stack || result >= L->stack_last) {
    printf_debug("[ERROR] RC register %d out of bounds\n", reg);
    return NULL;
  }
  return result;
}

#define RA(i)   safe_RA(base, i, L)
#define RB(i)   safe_RB(base, i, L)
#define RC(i)   safe_RC(base, i, L)
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
  AQL_DEBUG(AQL_DEBUG_VM, "ARITH_OP macro called for " #op); \
   \
  arith_op(L, s2v(RA(i)), vRKB(i), vRKC(i), TM_##op); \
  AQL_DEBUG(AQL_DEBUG_VM, "ARITH_OP completed, result = %lld", (long long)ivalue(s2v(RA(i)))); \
   \
} while(0)

#define ARITH_OP_K(op) do { \
  arith_op(L, s2v(RA(i)), s2v(RB(i)), k + GETARG_C(i), TM_##op); \
} while(0)

#define ARITH_OP_I(op) do { \
  TValue kval; \
  setivalue(&kval, sC2int(GETARG_C(i))); \
  printf_debug("[DEBUG] ARITH_OP_I: GETARG_C(i)=%d, sC2int(C)=%d, kval=%lld\n", GETARG_C(i), sC2int(GETARG_C(i)), ivalue(&kval)); \
  arith_op(L, s2v(RA(i)), s2v(RB(i)), &kval, TM_##op); \
} while(0)

/* Bitwise operations */
#define BITWISE_OP(op) do { \
  printf_debug("[DEBUG] BITWISE_OP: executing %s, A=%d, B=%d, C=%d\n", #op, GETARG_A(i), GETARG_B(i), GETARG_C(i)); \
  const TValue *rb = vRKB(i); \
  const TValue *rc = vRKC(i); \
  printf_debug("[DEBUG] BITWISE_OP: rb type=%d, rc type=%d\n", rawtt(rb), rawtt(rc)); \
  if (ttisinteger(rb) && ttisinteger(rc)) { \
    aql_Integer result; \
    switch (TM_##op) { \
      case TM_BAND: result = ivalue(rb) & ivalue(rc); break; \
      case TM_BOR:  result = ivalue(rb) | ivalue(rc); break; \
      case TM_BXOR: result = ivalue(rb) ^ ivalue(rc); break; \
      case TM_SHL:  result = aqlV_shiftl(ivalue(rb), ivalue(rc)); break; \
      case TM_SHR:  result = aqlV_shiftl(ivalue(rb), -ivalue(rc)); break; \
      default: aql_assert(0); result = 0; \
    } \
    printf_debug("[DEBUG] BITWISE_OP: %s(%lld, %lld) = %lld\n", #op, (long long)ivalue(rb), (long long)ivalue(rc), (long long)result); \
    setivalue(s2v(RA(i)), result); \
  } else { \
    aqlG_runerror(L, "attempt to perform bitwise operation on non-integer values"); \
  } \
  continue; \
} while(0)

/* Comparison operations */
#define CMP_OP(cmpfunc) do { \
  StkId ra = RA(i); \
  if (!ra) { \
    printf_debug("[ERROR] CMP_OP: RA register out of bounds\n"); \
    return 0; \
  } \
  printf_debug("[DEBUG] CMP_OP: GETARG_B(i)=%d, ISK(B)=%d, GETARG_C(i)=%d, ISK(C)=%d\n", \
    GETARG_B(i), ISK(GETARG_B(i)), GETARG_C(i), ISK(GETARG_C(i))); \
  const TValue *rb = vRKB(i); \
  const TValue *rc = vRKC(i); \
  printf_debug("[DEBUG] CMP_OP: rb type=%d, rc type=%d\n", rawtt(rb), rawtt(rc)); \
  printf_debug("[DEBUG] CMP_OP: rb value = %d, rc value = %d\n", \
    ttisinteger(rb) ? (int)ivalue(rb) : -999, \
    ttisinteger(rc) ? (int)ivalue(rc) : -999); \
  int cond; \
  cond = cmpfunc(L, rb, rc); \
  printf_debug("[DEBUG] CMP_OP: %s(rb, rc) = %d\n", #cmpfunc, cond); \
  /* Note: Negation for >=, >, != is handled by using different operator combinations */ \
  /* The k flag is not used due to conflicts with RKASK encoding */ \
  printf_debug("[DEBUG] CMP_OP: final result = %d\n", cond); \
  /* Store boolean result in register A */ \
  if (cond) { \
    setbtvalue(s2v(ra)); \
  } else { \
    setbfvalue(s2v(ra)); \
  } \
  continue; \
} while(0)

/* Return operations */
#define RET_OP(setup, a_arg, b_arg) do { \
  setup; \
  \
  /* Trace register state before return */ \
  debug_trace_registers(L, base, pc, cl->p); \
  \
  /* Print execution completion if tracing is enabled */ \
  AQL_DEBUG_BUILD_ONLY( \
    if (trace_execution && (L->top > base)) { \
      aqlD_print_execution_footer(s2v(base + (a_arg))); \
    } \
  ); \
  \
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
  AQL_DEBUG(AQL_DEBUG_VM, "arith_op function called with op=%d", op);
  
  
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
      case TM_DIV: {
        /* Convert to float division for accurate results */
        aql_Number nb = cast_num(ib);
        aql_Number nc = cast_num(ic);
        aql_Number fres = nb / nc;
        setfltvalue(ra, fres);
        return;  /* Skip setivalue below */
      }
      case TM_MOD: res = aqlV_mod(L, ib, ic); break;
      case TM_POW: {
        /* Convert to float power for accurate results */
        aql_Number nb = cast_num(ib);
        aql_Number nc = cast_num(ic);
        aql_Number fres = pow(nb, nc);
        setfltvalue(ra, fres);
        return;  /* Skip setivalue below */
      }
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
  } else if (ttisstring(rb) && ttisstring(rc) && op == TM_ADD) {
    /* String concatenation */
    AQL_DEBUG(AQL_DEBUG_VM, "arith_op: performing string concatenation");
    
    TString *sb = tsvalue(rb);
    TString *sc = tsvalue(rc);
    size_t lb = tsslen(sb);
    size_t lc = tsslen(sc);
    size_t total_len = lb + lc;
    
    /* Create new string with concatenated content */
    char *buffer = (char*)malloc(total_len + 1);
    if (buffer == NULL) {
      aqlG_runerror(L, "not enough memory for string concatenation");
      return;
    }
    
    memcpy(buffer, getstr(sb), lb);
    memcpy(buffer + lb, getstr(sc), lc);
    buffer[total_len] = '\0';
    
    /* Create new TString and set result */
    TString *result = aqlStr_newlstr(L, buffer, total_len);
    free(buffer);
    
    setsvalue(L, ra, result);
    AQL_DEBUG(AQL_DEBUG_VM, "arith_op: string concatenation result='%s'", getstr(result));
  } else if ((ttisstring(rb) && ttisinteger(rc)) || (ttisinteger(rb) && ttisstring(rc))) {
    /* String + Integer concatenation */
    if (op != TM_ADD) {
      aqlG_runerror(L, "attempt to perform arithmetic on a %s and a %s",
                    aqlL_typename(L, rb), aqlL_typename(L, rc));
      return;
    }
    
    AQL_DEBUG(AQL_DEBUG_VM, "arith_op: performing string+integer concatenation");
    
    const char *str_part;
    size_t str_len;
    aql_Integer int_part;
    
    if (ttisstring(rb)) {
      /* string + integer */
      TString *sb = tsvalue(rb);
      str_part = getstr(sb);
      str_len = tsslen(sb);
      int_part = ivalue(rc);
    } else {
      /* integer + string */
      TString *sc = tsvalue(rc);
      str_part = getstr(sc);
      str_len = tsslen(sc);
      int_part = ivalue(rb);
    }
    
    /* Convert integer to string */
    char int_buffer[32];
    int int_len = snprintf(int_buffer, sizeof(int_buffer), "%lld", (long long)int_part);
    
    /* Calculate total length */
    size_t total_len = str_len + int_len;
    
    /* Create concatenated string */
    char *buffer = (char*)malloc(total_len + 1);
    if (buffer == NULL) {
      aqlG_runerror(L, "not enough memory for string concatenation");
      return;
    }
    
    if (ttisstring(rb)) {
      /* string + integer: str_part + int_buffer */
      memcpy(buffer, str_part, str_len);
      memcpy(buffer + str_len, int_buffer, int_len);
    } else {
      /* integer + string: int_buffer + str_part */
      memcpy(buffer, int_buffer, int_len);
      memcpy(buffer + int_len, str_part, str_len);
    }
    buffer[total_len] = '\0';
    
    /* Create new TString and set result */
    TString *result = aqlStr_newlstr(L, buffer, total_len);
    free(buffer);
    
    setsvalue(L, ra, result);
    AQL_DEBUG(AQL_DEBUG_VM, "arith_op: string+integer concatenation result='%s'", getstr(result));
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
  printf_debug("[DEBUG] aqlV_execute: entering VM execution\n");
  
  /* Critical safety checks */
  if (!L) {
    printf_debug("[ERROR] aqlV_execute: L is NULL\n");
    return 0;  /* Execution failed */
  }
  
  if (!ci) {
    printf_debug("[ERROR] aqlV_execute: ci is NULL\n");
    return 0;  /* Execution failed */
  }
  
  if (ci != L->ci) {
    printf_debug("[ERROR] aqlV_execute: ci != L->ci\n");
    return 0;  /* Execution failed */
  }
  
  if (!ci->func) {
    printf_debug("[ERROR] aqlV_execute: ci->func is NULL\n");
    return 0;  /* Execution failed */
  }
  
  TValue *func_val = s2v(ci->func);
  if (!ttisclosure(func_val)) {
    printf_debug("[ERROR] aqlV_execute: function is not a closure\n");
    return 0;  /* Execution failed */
  }
  
  LClosure *cl;
  TValue *k;
  StkId base;
  const Instruction *pc;
  
  cl = clLvalue(func_val);
  if (!cl) {
    printf_debug("[ERROR] aqlV_execute: cl is NULL\n");
    return 0;  /* Execution failed */
  }
  
  if (!cl->p) {
    printf_debug("[ERROR] aqlV_execute: cl->p is NULL\n");
    return 0;  /* Execution failed */
  }
  
  if (!cl->p->code) {
    printf_debug("[ERROR] aqlV_execute: cl->p->code is NULL\n");
    return 0;  /* Execution failed */
  }
  
  base = ci->func + 1;
  pc = ci->u.l.savedpc;
  k = cl->p->k;
  
  printf_debug("[DEBUG] aqlV_execute: safety checks passed, starting execution\n");
  
  AQL_DEBUG(AQL_DEBUG_VM, "aqlV_execute: starting execution loop");
  
#ifdef AQL_DEBUG_BUILD
  /* Initialize execution tracing if enabled */
  int trace_execution = (aql_debug_enabled && (aql_debug_flags & AQL_DEBUG_VM));
  int trace_registers = (aql_debug_enabled && (aql_debug_flags & AQL_DEBUG_REG));
  
  if (trace_execution) {
    aqlD_print_execution_header();
  }
  
  if (trace_registers) {
    aqlD_print_register_header();
  }
  
  /* For register change tracking */
  TValue *prev_registers = NULL;
  int max_registers = 32;  /* Initial estimate */
  if (trace_registers) {
    prev_registers = (TValue*)malloc(max_registers * sizeof(TValue));
    /* Initialize previous registers to nil */
    for (int i = 0; i < max_registers; i++) {
      setnilvalue(&prev_registers[i]);
    }
  }
#endif
  
#ifdef AQL_DEBUG_BUILD
  /* Print execution header if tracing is enabled */
  if (trace_execution) {
    aqlD_print_execution_header();
  }
#endif
  
  for (;;) {
    /* Safety check: ensure pc is within bounds */
    if (pc < cl->p->code || pc >= cl->p->code + cl->p->sizecode) {
      printf_debug("[ERROR] aqlV_execute: PC out of bounds\n");
      return 0;  /* Execution failed */
    }
    
    Instruction i = *pc++;
    StkId ra;
    OpCode op = GET_OPCODE(i);
    int current_pc = (int)(pc - cl->p->code - 1);  /* Calculate current PC */
    
    /* Safety check: validate opcode */
    if (op >= NUM_OPCODES) {
      printf_debug("[ERROR] aqlV_execute: invalid opcode %d at pc=%d\n", op, current_pc);
      return 0;  /* Execution failed */
    }
    
    /* Print debug info based on instruction format */
    if (op == OP_LOADI || op == OP_LOADF || op == OP_JMP || op == OP_FORPREP || op == OP_FORLOOP) {
      /* iAsBx format */
      printf_debug("[DEBUG] aqlV_execute: PC=%d, opcode=%d (%s), A=%d, sBx=%d\n", 
        current_pc, op, aql_opnames[op], GETARG_A(i), GETARG_sBx(i));
    } else if (op == OP_LOADK) {
      /* iABx format */
      printf_debug("[DEBUG] aqlV_execute: PC=%d, opcode=%d (%s), A=%d, Bx=%d\n", 
        current_pc, op, aql_opnames[op], GETARG_A(i), GETARG_Bx(i));
    } else {
      /* iABC format */
      printf_debug("[DEBUG] aqlV_execute: PC=%d, opcode=%d (%s), A=%d, B=%d, C=%d\n", 
        current_pc, op, aql_opnames[op], GETARG_A(i), GETARG_B(i), GETARG_C(i));
    }
    
    /* Debug: Only check critical register conflicts */
    
    //AQL_DEBUG(AQL_DEBUG_VM, "aqlV_execute: executing instruction opcode=%d A=%d B=%d C=%d", 
     //      op, GETARG_A(i), GETARG_B(i), GETARG_C(i));
    
#ifdef AQL_DEBUG_BUILD
    /* Print execution trace if enabled */
    if (trace_execution) {
      AQL_VMState vm_state;
      vm_state.pc = current_pc;
      vm_state.opname = aql_opnames[op];
      
      /* Generate instruction description */
      static char desc_buf[256];
      switch (op) {
        case OP_LOADI:
          snprintf(desc_buf, sizeof(desc_buf), "LOADI R(%d) := %d", 
                   GETARG_A(i), GETARG_sBx(i));
          break;
        case OP_LOADK:
          snprintf(desc_buf, sizeof(desc_buf), "LOADK R(%d) := K(%d)", 
                   GETARG_A(i), GETARG_Bx(i));
          break;
        case OP_ADD:
          snprintf(desc_buf, sizeof(desc_buf), "ADD R(%d) := R(%d) + R(%d)", 
                   GETARG_A(i), GETARG_B(i), GETARG_C(i));
          break;
        case OP_SUB:
          snprintf(desc_buf, sizeof(desc_buf), "SUB R(%d) := R(%d) - R(%d)", 
                   GETARG_A(i), GETARG_B(i), GETARG_C(i));
          break;
        case OP_MUL:
          snprintf(desc_buf, sizeof(desc_buf), "MUL R(%d) := R(%d) * R(%d)", 
                   GETARG_A(i), GETARG_B(i), GETARG_C(i));
          break;
        case OP_DIV:
          snprintf(desc_buf, sizeof(desc_buf), "DIV R(%d) := R(%d) / R(%d)", 
                   GETARG_A(i), GETARG_B(i), GETARG_C(i));
          break;
        case OP_FORPREP:
          snprintf(desc_buf, sizeof(desc_buf), "FORPREP A=%d B=%d C=%d", 
                   GETARG_A(i), GETARG_B(i), GETARG_C(i));
          break;
        case OP_FORLOOP:
          snprintf(desc_buf, sizeof(desc_buf), "FORLOOP A=%d B=%d C=%d", 
                   GETARG_A(i), GETARG_B(i), GETARG_C(i));
          break;
        case OP_BUILTIN:
          snprintf(desc_buf, sizeof(desc_buf), "BUILTIN A=%d B=%d C=%d", 
                   GETARG_A(i), GETARG_B(i), GETARG_C(i));
          break;
        case OP_GETTABUP:
          snprintf(desc_buf, sizeof(desc_buf), "GETTABUP R(%d) := UPVAL[%d][R(%d)]", 
                   GETARG_A(i), GETARG_B(i), GETARG_C(i));
          break;
        case OP_SETTABUP:
          snprintf(desc_buf, sizeof(desc_buf), "SETTABUP UPVAL[%d][R(%d)] := R(%d)", 
                   GETARG_B(i), GETARG_C(i), GETARG_A(i));
          break;
        case OP_RET_ONE:
          snprintf(desc_buf, sizeof(desc_buf), "RET_ONE return R(%d)", GETARG_A(i));
          break;
        case OP_RET_VOID:
          snprintf(desc_buf, sizeof(desc_buf), "RET_VOID return");
          break;
        case OP_MOVE:
          snprintf(desc_buf, sizeof(desc_buf), "MOVE R(%d) := R(%d)", 
                   GETARG_A(i), GETARG_B(i));
          break;
        default:
          snprintf(desc_buf, sizeof(desc_buf), "%s A=%d B=%d C=%d", 
                   aql_opnames[op], GETARG_A(i), GETARG_B(i), GETARG_C(i));
          break;
      }
      vm_state.description = desc_buf;
      
      aqlD_print_vm_state(&vm_state);
      
      /* Print register states BEFORE instruction execution */
      int reg_count = (int)(L->top - base);
      if (reg_count > 0 && trace_execution) {
        printf("  Before: ");
        /* Print registers directly without debug flag check */
        printf("📊 Registers: ");
        for (int j = 0; j < reg_count && j < 8; j++) {
          TValue *val = &s2v(base)[j];
          char buffer[32];
          aqlD_format_value(val, buffer, sizeof(buffer));
          printf("R%d=%s ", j, buffer);
        }
        if (reg_count > 8) {
          printf("... (%d more)", reg_count - 8);
        }
        printf("\n");
      }
    }
    
#endif
    
    /* Enhanced register tracking - simplified approach */
    int max_stack_size = cl->p->maxstacksize;
    
    aql_assert(base == ci->func + 1);
    aql_assert(base <= L->top && L->top <= L->stack_last);
    
    switch (op) {
      /* Base operations */
      case OP_MOVE: {
        setobj2s(L, RA(i), s2v(RB(i)));
#ifdef AQL_DEBUG_BUILD
        if (trace_execution) {
          int reg_count = (int)(L->top - base);
          if (reg_count > 0) {
            printf("  After:  📊 Registers: ");
            for (int j = 0; j < reg_count && j < 8; j++) {
              TValue *val = &s2v(base)[j];
              char buffer[32];
              aqlD_format_value(val, buffer, sizeof(buffer));
              printf("R%d=%s ", j, buffer);
            }
            if (reg_count > 8) {
              printf("... (%d more)", reg_count - 8);
            }
            printf("\n");
          }
        }
#endif
        continue;
      }
      case OP_LOADI: {
        setivalue(s2v(RA(i)), iAsBx(i));
#ifdef AQL_DEBUG_BUILD
        if (trace_execution) {
          int reg_count = (int)(L->top - base);
          if (reg_count > 0) {
            printf("  After:  ");
            aqlD_print_registers(s2v(base), reg_count);
          }
        }
#endif
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
      case OP_ADD: {
        TValue *rb = vRKB(i);
        TValue *rc = vRKC(i);
        printf_debug("[DEBUG] OP_ADD: A=%d, B=%d, C=%d, rb_val=%lld, rc_val=%lld\n", 
                     GETARG_A(i), GETARG_B(i), GETARG_C(i), 
                     ttisinteger(rb) ? (long long)ivalue(rb) : -999,
                     ttisinteger(rc) ? (long long)ivalue(rc) : -999);
        ARITH_OP(ADD); 
        printf_debug("[DEBUG] OP_ADD result: %lld\n", 
                     ttisinteger(s2v(RA(i))) ? (long long)ivalue(s2v(RA(i))) : -999);
        continue;
      }
      case OP_ADDK: ARITH_OP_K(ADD); continue;
      case OP_ADDI: {
        printf_debug("[DEBUG] OP_ADDI: A=%d, B=%d, C=%d, sC2int(C)=%d\n", 
                     GETARG_A(i), GETARG_B(i), GETARG_C(i), sC2int(GETARG_C(i)));
        printf_debug("[DEBUG] OP_ADDI: before - R[%d]=%lld\n", 
                     GETARG_A(i), ttisinteger(s2v(RA(i))) ? (long long)ivalue(s2v(RA(i))) : -999);
        ARITH_OP_I(ADD); 
        printf_debug("[DEBUG] OP_ADDI: after - R[%d]=%lld\n", 
                     GETARG_A(i), ttisinteger(s2v(RA(i))) ? (long long)ivalue(s2v(RA(i))) : -999);
        continue;
      }
      case OP_SUB: ARITH_OP(SUB); continue;
      case OP_SUBK: ARITH_OP_K(SUB); continue;
      case OP_SUBI: ARITH_OP_I(SUB); continue;
      case OP_MUL: {
        ARITH_OP(MUL);
#ifdef AQL_DEBUG_BUILD
        if (trace_execution) {
          int reg_count = (int)(L->top - base);
          if (reg_count > 0) {
            printf("  After:  ");
            aqlD_print_registers(s2v(base), reg_count);
          }
        }
#endif
        continue;
      }
      case OP_MULK: ARITH_OP_K(MUL); continue;
      case OP_MULI: ARITH_OP_I(MUL); continue;
      case OP_DIV: ARITH_OP(DIV); continue;
      case OP_DIVK: ARITH_OP_K(DIV); continue;
      case OP_DIVI: ARITH_OP_I(DIV); continue;
      case OP_MOD: ARITH_OP(MOD); continue;
      case OP_POW: ARITH_OP(POW); continue;
      
      /* Unary operations */
      case OP_UNM: {
        const TValue *rb = vRKB(i);
        if (ttisinteger(rb)) {
          setivalue(s2v(RA(i)), intop(-, 0, ivalue(rb)));
        } else if (ttisfloat(rb)) {
          setfltvalue(s2v(RA(i)), -fltvalue(rb));
        } else {
          aqlG_runerror(L, "attempt to negate a %s", aqlL_typename(L, rb));
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
      case OP_BAND: {
        printf_debug("[DEBUG] VM: executing OP_BAND\n");
        const TValue *rb = vRKB(i);
        const TValue *rc = vRKC(i);
        if (ttisinteger(rb) && ttisinteger(rc)) {
          aql_Integer result = ivalue(rb) & ivalue(rc);
          printf_debug("[DEBUG] OP_BAND: %lld & %lld = %lld\n", (long long)ivalue(rb), (long long)ivalue(rc), (long long)result);
          setivalue(s2v(RA(i)), result);
        } else {
          aqlG_runerror(L, "attempt to perform bitwise operation on non-integer values");
        }
        continue;
      }
      case OP_BOR: {
        printf_debug("[DEBUG] VM: executing OP_BOR\n");
        const TValue *rb = vRKB(i);
        const TValue *rc = vRKC(i);
        if (ttisinteger(rb) && ttisinteger(rc)) {
          aql_Integer result = ivalue(rb) | ivalue(rc);
          printf_debug("[DEBUG] OP_BOR: %lld | %lld = %lld\n", (long long)ivalue(rb), (long long)ivalue(rc), (long long)result);
          setivalue(s2v(RA(i)), result);
        } else {
          aqlG_runerror(L, "attempt to perform bitwise operation on non-integer values");
        }
        continue;
      }
      case OP_BXOR: {
        printf_debug("[DEBUG] VM: executing OP_BXOR\n");
        const TValue *rb = vRKB(i);
        const TValue *rc = vRKC(i);
        if (ttisinteger(rb) && ttisinteger(rc)) {
          aql_Integer result = ivalue(rb) ^ ivalue(rc);
          printf_debug("[DEBUG] OP_BXOR: %lld ^ %lld = %lld\n", (long long)ivalue(rb), (long long)ivalue(rc), (long long)result);
          setivalue(s2v(RA(i)), result);
        } else {
          aqlG_runerror(L, "attempt to perform bitwise operation on non-integer values");
        }
        continue;
      }
      case OP_SHL: {
        printf_debug("[DEBUG] VM: executing OP_SHL\n");
        const TValue *rb = vRKB(i);
        const TValue *rc = vRKC(i);
        if (ttisinteger(rb) && ttisinteger(rc)) {
          aql_Integer result = aqlV_shiftl(ivalue(rb), ivalue(rc));
          printf_debug("[DEBUG] OP_SHL: %lld << %lld = %lld\n", (long long)ivalue(rb), (long long)ivalue(rc), (long long)result);
          setivalue(s2v(RA(i)), result);
        } else {
          aqlG_runerror(L, "attempt to perform bitwise operation on non-integer values");
        }
        continue;
      }
      case OP_SHR: {
        printf_debug("[DEBUG] VM: executing OP_SHR\n");
        const TValue *rb = vRKB(i);
        const TValue *rc = vRKC(i);
        if (ttisinteger(rb) && ttisinteger(rc)) {
          aql_Integer result = aqlV_shiftl(ivalue(rb), -ivalue(rc));
          printf_debug("[DEBUG] OP_SHR: %lld >> %lld = %lld\n", (long long)ivalue(rb), (long long)ivalue(rc), (long long)result);
          setivalue(s2v(RA(i)), result);
        } else {
          aqlG_runerror(L, "attempt to perform bitwise operation on non-integer values");
        }
        continue;
      }
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
        printf_debug("[DEBUG] VM: executing OP_NOT\n");
        TValue *rb = s2v(RB(i));
        int bool_val = aqlV_toboolean(rb);
        int result = !bool_val;
        printf_debug("[DEBUG] OP_NOT: RB boolean value = %d, result = %d\n", bool_val, result);
        setbvalue(s2v(RA(i)), result);
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
      case OP_EQ: {
        printf_debug("[DEBUG] VM: executing OP_EQ\n");
        const TValue *rb = vRKB(i);
        const TValue *rc = vRKC(i);
        int cond = aqlV_equalobj(L, rb, rc);
        printf_debug("[DEBUG] OP_EQ: aqlV_equalobj(rb, rc) = %d\n", cond);
        
        /* Store comparison result in register A for TEST instruction */
        StkId ra = RA(i);
        if (cond) {
          setbtvalue(s2v(ra));
        } else {
          setbfvalue(s2v(ra));
        }
        
        /* docondjump: Lua-style conditional jump */
        int k = GETARG_k(i);
        printf_debug("[DEBUG] OP_EQ: cond=%d, k=%d, %s\n", cond, k, 
                     (cond != k) ? "skipping next instruction" : "executing next instruction");
        if (cond != k) {
          pc++;  /* skip next instruction (JMP) */
          printf_debug("[DEBUG] OP_EQ: skipped JMP, new PC=%d\n", (int)(pc - cl->p->code));
        }
        /* else: execute next instruction (JMP) */
        continue;
      }
      case OP_LT: {
        printf_debug("[DEBUG] VM: executing OP_LT\n");
        const TValue *rb = vRKB(i);
        const TValue *rc = vRKC(i);
        int cond = aqlV_lessthan_internal(L, rb, rc);
        printf_debug("[DEBUG] OP_LT: aqlV_lessthan_internal(rb, rc) = %d\n", cond);
        
        /* Store comparison result in register A for TEST instruction */
        StkId ra = RA(i);
        if (cond) {
          setbtvalue(s2v(ra));
        } else {
          setbfvalue(s2v(ra));
        }
        
        /* docondjump: Lua-style conditional jump */
        int k = GETARG_k(i);
        printf_debug("[DEBUG] OP_LT: cond=%d, k=%d, %s\n", cond, k, 
                     (cond != k) ? "skipping next instruction" : "executing next instruction");
        if (cond != k) {
          pc++;  /* skip next instruction (JMP) */
          printf_debug("[DEBUG] OP_LT: skipped JMP, new PC=%d\n", (int)(pc - cl->p->code));
        }
        /* else: execute next instruction (JMP) */
        continue;
      }
      case OP_LE: {
        printf_debug("[DEBUG] VM: executing OP_LE\n");
        const TValue *rb = vRKB(i);
        const TValue *rc = vRKC(i);
        int cond = aqlV_lessequal_internal(L, rb, rc);
        printf_debug("[DEBUG] OP_LE: aqlV_lessequal_internal(rb, rc) = %d\n", cond);
        
        /* Store comparison result in register A for TEST instruction */
        StkId ra = RA(i);
        if (cond) {
          setbtvalue(s2v(ra));
        } else {
          setbfvalue(s2v(ra));
        }
        
        /* docondjump: Lua-style conditional jump */
        int k = GETARG_k(i);
        printf_debug("[DEBUG] OP_LE: cond=%d, k=%d, %s\n", cond, k, 
                     (cond != k) ? "skipping next instruction" : "executing next instruction");
        if (cond != k) {
          pc++;  /* skip next instruction (JMP) */
          printf_debug("[DEBUG] OP_LE: skipped JMP, new PC=%d\n", (int)(pc - cl->p->code));
        }
        /* else: execute next instruction (JMP) */
        continue;
      }
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
        int k = GETARG_k(i);
        int isfalse = aqlV_toboolean(ra) == 0;  /* ra is false? */
        printf_debug("[DEBUG] OP_TEST: ra=%s, k=%d, isfalse=%d\n", 
                     aqlV_toboolean(ra) ? "true" : "false", k, isfalse);
        
        if (isfalse == k) {
          /* condition matches k, skip next instruction */
          pc++;
          printf_debug("[DEBUG] OP_TEST: condition matches k, skipping next (pc++)\n");
        } else {
          printf_debug("[DEBUG] OP_TEST: condition doesn't match k, will execute next instruction\n");
        }
        continue;
      }
      case OP_TESTSET: {
        StkId ra = RA(i);
        TValue *rb = s2v(RB(i));
        int k = GETARG_k(i);
        int isfalse = aqlV_toboolean(rb) == 0;  /* rb is false? */
        printf_debug("[DEBUG] OP_TESTSET: rb=%s, k=%d, isfalse=%d\n", 
                     aqlV_toboolean(rb) ? "true" : "false", k, isfalse);
        
        if (isfalse == k) {
          /* condition matches k, skip next instruction */
          pc++;
          printf_debug("[DEBUG] OP_TESTSET: condition matches k, skipping next (pc++)\n");
        } else {
          /* condition doesn't match k, set register and execute next jump */
          setobj2s(L, ra, rb);
          printf_debug("[DEBUG] OP_TESTSET: condition doesn't match k, copied value and will execute next JMP\n");
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
        if (ttisinteger(s2v(ra + 2))) {  /* integer loop? */
          aql_Integer count = ivalue(s2v(ra + 1));  /* iteration counter */
          aql_Integer step = ivalue(s2v(ra + 2));
          aql_Integer idx = ivalue(s2v(ra));  /* internal index */
          aql_Integer old_control = ivalue(s2v(ra + 3));  /* old control variable */
          
          printf_debug("[DEBUG] OP_FORLOOP: count=%lld, step=%lld, idx=%lld, old_control=%lld\n", 
                       (long long)count, (long long)step, (long long)idx, (long long)old_control);
          
          /* Decrement counter first */
          count--;
          setivalue(s2v(ra + 1), count);  /* update counter */
          
          if (count > 0) {  /* still more iterations? */
            idx += step;  /* add step to index */
            setivalue(s2v(ra), idx);  /* update internal index */
            
            printf_debug("[DEBUG] OP_FORLOOP: before update ra+3, addr=%p, val=%lld\n", 
                         (void*)s2v(ra + 3), (long long)ivalue(s2v(ra + 3)));
            
            setivalue(s2v(ra + 3), idx);  /* update control variable */
            
            printf_debug("[DEBUG] OP_FORLOOP: after update ra+3, addr=%p, val=%lld\n", 
                         (void*)s2v(ra + 3), (long long)ivalue(s2v(ra + 3)));
            printf_debug("[DEBUG] OP_FORLOOP: new_idx=%lld, new_control=%lld, remaining_count=%lld\n", 
                         (long long)idx, (long long)ivalue(s2v(ra + 3)), (long long)count);
            
            pc -= GETARG_sBx(i);  /* jump back to loop start */
          }
          /* else: loop finished, continue to next instruction */
        }
#ifdef AQL_DEBUG_BUILD
        if (trace_execution) {
          int reg_count = (int)(L->top - base);
          if (reg_count > 0) {
            printf("  After:  ");
            aqlD_print_registers(s2v(base), reg_count);
          }
        }
#endif
        continue;
      }
      case OP_FORPREP: {
        StkId ra = RA(i);
        TValue *pinit = s2v(ra);
        TValue *plimit = s2v(ra + 1);
        TValue *pstep = s2v(ra + 2);
        
        if (ttisinteger(pinit) && ttisinteger(plimit)) {
          aql_Integer init = ivalue(pinit);
          aql_Integer limit = ivalue(plimit);
          aql_Integer step;
          
          printf_debug("[DEBUG] OP_FORPREP: init=%lld, limit=%lld\n", (long long)init, (long long)limit);
          
          // 智能步长推断
          if (ttisinteger(pstep)) {
            // 用户提供了步长，使用用户指定的
            step = ivalue(pstep);
            printf_debug("[DEBUG] OP_FORPREP: explicit step=%lld\n", (long long)step);
          } else if (ttisnil(pstep)) {
            // 未提供步长，智能推断
            step = (init > limit) ? -1 : 1;
            // 更新寄存器中的步长值
            setivalue(s2v(ra + 2), step);
            printf_debug("[DEBUG] OP_FORPREP: smart step inference, step=%lld\n", (long long)step);
          } else {
            printf_debug("[DEBUG] OP_FORPREP: invalid step type, ttag=%d\n", ttypetag(pstep));
            aqlG_runerror(L, "'for' step must be an integer or omitted");
            continue;
          }
          
          if (step == 0) {
            aqlG_runerror(L, "'for' step is zero");
            continue;
          }
          
          /* Set control variable to init */
          setivalue(s2v(ra + 3), init);  /* control variable */
          
          /* Check if loop should run at all */
          if ((step > 0 && init > limit) || (step < 0 && init < limit)) {
            int jump_offset = GETARG_sBx(i);
            int old_pc_offset = pc - cl->p->code;
            pc += jump_offset;  /* skip the loop (like OP_JMP) */
            int new_pc_offset = pc - cl->p->code;
            printf_debug("[DEBUG] OP_FORPREP: skipping loop, jump_offset=%d, PC=%d -> PC=%d\n", 
                         jump_offset, old_pc_offset, new_pc_offset);
        } else {
            /* Calculate iteration count for half-open interval [init, limit) - Python style */
            aql_Unsigned count;
            if (step > 0) {  /* ascending loop */
              /* For half-open interval: count = ceil((limit - init) / step) */
              if (init < limit) {
                count = ((aql_Unsigned)(limit - init) + (aql_Unsigned)step - 1) / (aql_Unsigned)step;
              } else {
                count = 0;  /* no iterations */
              }
            } else {  /* descending loop - step < 0 */
              /* For half-open interval: count = ceil((init - limit) / |step|) */
              if (init > limit) {
                count = ((aql_Unsigned)(init - limit) + (aql_Unsigned)(-step) - 1) / (aql_Unsigned)(-step);
              } else {
                count = 0;  /* no iterations */
              }
            }
            /* Store counter in place of limit */
            setivalue(s2v(ra + 1), (aql_Integer)count);
            /* Internal index starts at init */
            setivalue(s2v(ra), init);
            /* Initialize control variable to init */
            setivalue(s2v(ra + 3), init);
          }
          /* else: continue to loop body */
        } else {
          aqlG_runerror(L, "'for' initial value, limit and step must be integers");
        }
#ifdef AQL_DEBUG_BUILD
        if (trace_execution) {
          int reg_count = (int)(L->top - base);
          if (reg_count > 0) {
            printf("  After:  ");
            aqlD_print_registers(s2v(base), reg_count);
          }
        }
#endif
        continue;
      }
      
      /* Extended operations */
      case OP_YIELD: {
        /* TODO: Implement aqlD_yield for coroutine yielding */
        aqlG_runerror(L, "yield operation not yet implemented");
        return 0;
      }
      
      case OP_BUILTIN: {
        ra = base + GETARG_A(i);  /* 初始化ra */
        int builtin_id = GETARG_B(i);
        
        switch (builtin_id) {
          case 0: /* print */ {
            int nargs = GETARG_C(i);  /* 参数数量 */
            StkId func_base = ra - nargs;  /* 参数在结果寄存器之前 */
            printf_debug("[DEBUG] print: nargs=%d, ra=%p, func_base=%p\n", 
                         nargs, (void*)ra, (void*)func_base);
            for (int j = 0; j < nargs; j++) {
              if (j > 0) printf("\t");
              printf_debug("[DEBUG] print: arg[%d] from register %p\n", 
                           j, (void*)(func_base + j));
              
              /* Print value without quotes for strings */
              TValue *val = s2v(func_base + j);
              int tag = ttypetag(val);
              switch (tag) {
                case AQL_VNUMINT:
                  printf("%lld", (long long)ivalue(val));
                  break;
                case AQL_VNUMFLT:
                  printf("%.6g", fltvalue(val));
                  break;
                case AQL_VSHRSTR:
                case AQL_VLNGSTR: {
                  TString *ts = tsvalue(val);
                  const char *str = getstr(ts);
                  size_t len = tsslen(ts);
                  printf("%.*s", (int)len, str);  /* No quotes for print */
                  break;
                }
                case AQL_VNIL:
                  printf("nil");
                  break;
                case AQL_VFALSE:
                  printf("false");
                  break;
                case AQL_VTRUE:
                  printf("true");
                  break;
                default:
                  /* For other types, fall back to aqlP_print_value */
                  aqlP_print_value(val);
                  break;
              }
            }
            printf("\n");
            /* Set return value to nil (Lua-compatible behavior) */
            setnilvalue(s2v(ra));
            break;
          }
          case 1: /* type */
            /* TODO: Implement aqlB_type */
            aqlG_runerror(L, "type builtin not yet implemented");
            break;
          case 2: /* len */
            /* TODO: Implement aqlB_len */
            aqlG_runerror(L, "len builtin not yet implemented");
            break;
          case 3: /* tostring/string */ {
            int nargs = GETARG_C(i);
            StkId func_base = ra - nargs;
            
            printf_debug("[DEBUG] string: nargs=%d, ra=%p, func_base=%p\n", 
                         nargs, (void*)ra, (void*)func_base);
            
            if (nargs != 1) {
              aqlG_runerror(L, "string() takes exactly 1 argument (%d given)", nargs);
              return 0;
            }
            
            TValue *arg = s2v(func_base);
            printf_debug("[DEBUG] string: arg type=%d\n", ttypetag(arg));
            
            /* Convert value to string based on type */
            switch (ttypetag(arg)) {
              case AQL_VNUMINT: {
                /* Integer to string */
                aql_Integer val = ivalue(arg);
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%lld", (long long)val);
                TString *ts = aqlStr_new(L, buffer);
                setsvalue2s(L, ra, ts);
                printf_debug("[DEBUG] string: converted integer %lld to string '%s'\n", 
                             (long long)val, buffer);
                break;
              }
              case AQL_VNUMFLT: {
                /* Float to string */
                aql_Number val = fltvalue(arg);
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%.6g", val);
                TString *ts = aqlStr_new(L, buffer);
                setsvalue2s(L, ra, ts);
                printf_debug("[DEBUG] string: converted float %.6g to string '%s'\n", 
                             val, buffer);
                break;
              }
              case AQL_VSHRSTR:
              case AQL_VLNGSTR: {
                /* String to string (copy) */
                setobj2s(L, ra, arg);
                printf_debug("[DEBUG] string: copied string value\n");
                break;
              }
              case AQL_VNIL: {
                /* nil to "nil" */
                TString *ts = aqlStr_new(L, "nil");
                setsvalue2s(L, ra, ts);
                printf_debug("[DEBUG] string: converted nil to 'nil'\n");
                break;
              }
              case AQL_VFALSE: {
                /* false to "false" */
                TString *ts = aqlStr_new(L, "false");
                setsvalue2s(L, ra, ts);
                printf_debug("[DEBUG] string: converted false to 'false'\n");
                break;
              }
              case AQL_VTRUE: {
                /* true to "true" */
                TString *ts = aqlStr_new(L, "true");
                setsvalue2s(L, ra, ts);
                printf_debug("[DEBUG] string: converted true to 'true'\n");
                break;
              }
              default: {
                /* Other types: use type name + address */
                char buffer[64];
                snprintf(buffer, sizeof(buffer), "other: %p", (void*)arg);
                TString *ts = aqlStr_new(L, buffer);
                setsvalue2s(L, ra, ts);
                printf_debug("[DEBUG] string: converted other type to '%s'\n", buffer);
                break;
              }
            }
            break;
          }
          case 4: /* tonumber */
            /* TODO: Implement aqlB_tonumber */
            aqlG_runerror(L, "tonumber builtin not yet implemented");
            break;
          case 5: /* range */
            /* Handle range function based on number of arguments */
            {
              int nargs = GETARG_C(i);  /* Get number of arguments */
              StkId func_base = ra - nargs;  /* 参数在结果寄存器之前 */
              
              printf("[DEBUG] OP_BUILTIN range: nargs=%d, ra=%p, func_base=%p\n", 
                     nargs, (void*)ra, (void*)func_base);
              
              /* Read arguments directly from registers */
              aql_Integer args[3];
              for (int j = 0; j < nargs && j < 3; j++) {
                TValue *arg = s2v(func_base + j);
                printf("[DEBUG] Reading arg[%d] from register %p\n", j, (void*)(func_base + j));
                if (!ttisinteger(arg)) {
                  aqlG_runerror(L, "range() argument #%d must be an integer", j + 1);
                  return 0;
                }
                args[j] = ivalue(arg);
                printf("[DEBUG] arg[%d] = %lld\n", j, (long long)args[j]);
              }
              
              /* Create range object directly */
              RangeObject *range = NULL;
              printf("[DEBUG] Creating range object...\n");
              switch (nargs) {
                case 1:
                  range = aqlR_new(L, 0, args[0], 1);  /* range(stop) */
                  printf("[DEBUG] Created range(0, %lld, 1)\n", (long long)args[0]);
                  break;
                case 2:
                  range = aqlR_new(L, args[0], args[1], aqlR_infer_step(args[0], args[1]));  /* range(start, stop) */
                  printf("[DEBUG] Created range(%lld, %lld, auto)\n", (long long)args[0], (long long)args[1]);
                  break;
                case 3:
                  range = aqlR_new(L, args[0], args[1], args[2]);  /* range(start, stop, step) */
                  printf("[DEBUG] Created range(%lld, %lld, %lld)\n", (long long)args[0], (long long)args[1], (long long)args[2]);
                  break;
                default:
                  aqlG_runerror(L, "range() takes 1, 2, or 3 arguments (%d given)", nargs);
                  return 0;
              }
              
              if (range == NULL) {
                printf("[DEBUG] Range creation failed!\n");
                return 0;  /* Error occurred */
              }
              
              printf("[DEBUG] Range created successfully, storing in ra\n");
              /* Store result in result register */
              setrangevalue(L, s2v(ra), range);
              printf("[DEBUG] Range stored, continuing execution\n");
              printf("[DEBUG] About to break from OP_BUILTIN, PC should be %d\n", (int)(pc - cl->p->code));
            }
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
        AQL_DEBUG(AQL_DEBUG_VM, "OP_GETTABUP: A=%d B=%d C=%d", GETARG_A(i), GETARG_B(i), GETARG_C(i));
        AQL_DEBUG(AQL_DEBUG_VM, "OP_GETTABUP: cl=%p, cl->upvals=%p", (void*)cl, (void*)cl->upvals);
        
        
        AQL_DEBUG(AQL_DEBUG_VM, "OP_GETTABUP: cl->nupvalues=%d", cl->nupvalues);
        
        
        if (GETARG_B(i) >= cl->nupvalues) {
          AQL_DEBUG(AQL_DEBUG_VM, "OP_GETTABUP: upval access out of bounds (%d >= %d), setting nil", GETARG_B(i), cl->nupvalues);
          
          setnilvalue(s2v(RA(i)));
          continue;
        }
        
        if (!cl->upvals[GETARG_B(i)]) {
          AQL_DEBUG(AQL_DEBUG_VM, "OP_GETTABUP: upval[%d] is NULL, setting nil", GETARG_B(i));
          
          setnilvalue(s2v(RA(i)));
          continue;
        }
        
        const TValue *upval = cl->upvals[GETARG_B(i)]->v.p;
        const TValue *key = vRKC(i);  /* Use RK(C) for key */
        
        AQL_DEBUG(AQL_DEBUG_VM, "OP_GETTABUP: upval type=%d, key type=%d", ttype(upval), ttype(key));
        
        
        /* Check if upval is a dict (global environment) */
        if (ttisdict(upval)) {
          AQL_DEBUG(AQL_DEBUG_VM, "OP_GETTABUP: accessing global dict with key");
          
          /* Debug key information */
          if (ttisstring(key)) {
            TString *keystr = tsvalue(key);
            printf_debug("[DEBUG] OP_GETTABUP: key is string '%s', length=%d\n", 
                        getstr(keystr), (int)tsslen(keystr));
          }
          
          const TValue *result = aqlD_get(dictvalue(upval), key);
          if (result != NULL) {
            setobj2s(L, RA(i), result);
            AQL_DEBUG(AQL_DEBUG_VM, "OP_GETTABUP: found global variable, type=%d", ttype(result));
            
          } else {
            setnilvalue(s2v(RA(i)));
            AQL_DEBUG(AQL_DEBUG_VM, "OP_GETTABUP: global variable not found, setting nil");
            
          }
          continue;
        }
        
        /* If upval is nil, try to get/create global dict */
        if (ttisnil(upval)) {
          AQL_DEBUG(AQL_DEBUG_VM, "OP_GETTABUP: upval is nil, trying to get globals dict");
          
          
          /* Get globals dict from global state */
          Dict *globals_dict = get_globals_dict(L);
          if (globals_dict) {
            const TValue *result = aqlD_get(globals_dict, key);
            if (result != NULL) {
              setobj2s(L, RA(i), result);
              AQL_DEBUG(AQL_DEBUG_VM, "OP_GETTABUP: found global variable in globals dict, type=%d", ttype(result));
              
            } else {
              setnilvalue(s2v(RA(i)));
              AQL_DEBUG(AQL_DEBUG_VM, "OP_GETTABUP: global variable not found in globals dict, setting nil");
              
            }
          } else {
            setnilvalue(s2v(RA(i)));
            AQL_DEBUG(AQL_DEBUG_VM, "OP_GETTABUP: failed to get globals dict, setting nil");
            
          }
          continue;
        }
        
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
        AQL_DEBUG(AQL_DEBUG_VM, "OP_SETTABUP: A=%d B=%d C=%d", GETARG_A(i), GETARG_B(i), GETARG_C(i));
        
        
        TValue *upval = cl->upvals[GETARG_A(i)]->v.p;
        TValue *key = vRKB(i);  /* Use RK(B) for key, consistent with GETTABUP */
        TValue *val = vRKC(i);  /* Use RK(C) for value */
        
        AQL_DEBUG(AQL_DEBUG_VM, "OP_SETTABUP: upval type=%d, key type=%d, val type=%d", ttype(upval), ttype(key), ttype(val));
        
        /* Debug key and value information */
        if (ttisstring(key)) {
          TString *keystr = tsvalue(key);
          printf_debug("[DEBUG] OP_SETTABUP: key is string '%s', length=%d\n", 
                      getstr(keystr), (int)tsslen(keystr));
        }
        if (ttisinteger(val)) {
          printf_debug("[DEBUG] OP_SETTABUP: value is integer %lld\n", (long long)ivalue(val));
        }
        
        /* Check if upval is a dict (global environment) */
        if (ttisdict(upval)) {
          AQL_DEBUG(AQL_DEBUG_VM, "OP_SETTABUP: setting global variable in dict");
          
          aqlV_setdict(L, upval, key, val);
          continue;
        }
        
        /* If upval is nil, try to get/create global dict */
        if (ttisnil(upval)) {
          AQL_DEBUG(AQL_DEBUG_VM, "OP_SETTABUP: upval is nil, trying to get/create globals dict");
          
          
          /* Get or create globals dict */
          Dict *globals_dict = get_globals_dict(L);
          if (globals_dict) {
            AQL_DEBUG(AQL_DEBUG_VM, "OP_SETTABUP: setting global variable in globals dict");
            
            /* Debug key and value information */
            if (ttisstring(key)) {
              TString *keystr = tsvalue(key);
              printf_debug("[DEBUG] OP_SETTABUP: key is string '%s', length=%d\n", 
                          getstr(keystr), (int)tsslen(keystr));
            }
            if (ttisinteger(val)) {
              printf_debug("[DEBUG] OP_SETTABUP: value is integer %lld\n", (long long)ivalue(val));
            }
            
            aqlD_set(L, globals_dict, key, val);
            
            /* Update the upvalue to point to the globals dict */
            setdictvalue(L, upval, globals_dict);
          } else {
            AQL_DEBUG(AQL_DEBUG_VM, "OP_SETTABUP: failed to get/create globals dict");
            
          }
          continue;
        }
        
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
        } else if (ttisrange(obj)) {
          /* Range objects support iterator protocol methods */
          RangeObject *range = rangevalue(obj);
          if (strcmp(getstr(tsvalue(method_name)), "__iter") == 0) {
            /* __iter method returns the iterator (self) */
            setobj2s(L, RA(i), obj);  /* Return self as iterator */
            continue;
          } else if (strcmp(getstr(tsvalue(method_name)), "__next") == 0) {
            /* __next method returns next value or nil */
            if (range->finished || range->count <= 0) {
              /* Iteration finished, return nil */
              setnilvalue(s2v(RA(i)));
            } else {
              /* Return current value */
              setivalue(s2v(RA(i)), range->current);
              /* Advance iterator */
              range->current += range->step;
              range->count--;
              if (range->count <= 0) {
                range->finished = 1;
              }
            }
            continue;
          } else {
            aqlG_runerror(L, "range object does not support method '%s'", svalue(method_name));
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
      
      /* Iterator operations */
      case OP_ITER_INIT: {
        StkId ra = RA(i);
        TValue *iterable = s2v(RB(i));
        
        printf_debug("[DEBUG] OP_ITER_INIT: initializing iterator for type %d\n", rawtt(iterable));
        
        /* Initialize iterator based on container type */
        if (ttisarray(iterable)) {
          /* Array iterator */
          Array *arr = arrayvalue(iterable);
          setivalue(s2v(ra), 0);  /* index = 0 */
          setobj2s(L, ra + 1, iterable);  /* store array reference */
          printf_debug("[DEBUG] OP_ITER_INIT: array iterator initialized, length=%zu\n", arr->length);
        } else if (ttisdict(iterable)) {
          /* Dict iterator */
          Dict *dict = dictvalue(iterable);
          setivalue(s2v(ra), 0);  /* index = 0 */
          setobj2s(L, ra + 1, iterable);  /* store dict reference */
          printf_debug("[DEBUG] OP_ITER_INIT: dict iterator initialized, size=%zu\n", dict->size);
        } else if (ttisrange(iterable)) {
          /* Range iterator - already has built-in iteration */
          RangeObject *range = rangevalue(iterable);
          setivalue(s2v(ra), range->current);  /* current value */
          setobj2s(L, ra + 1, iterable);  /* store range reference */
          printf_debug("[DEBUG] OP_ITER_INIT: range iterator initialized, current=%lld\n", (long long)range->current);
        } else {
          aqlG_runerror(L, "attempt to iterate over non-iterable type %s", aqlL_typename(L, iterable));
        }
        continue;
      }
      
      case OP_ITER_NEXT: {
        StkId ra = RA(i);      /* iterator state */
        StkId rb = base + GETARG_B(i);  /* state register (unused for now) */
        StkId rc = base + GETARG_C(i);  /* value register */
        
        TValue *container = s2v(ra + 1);  /* container reference */
        
        printf_debug("[DEBUG] OP_ITER_NEXT: getting next value for type %d\n", rawtt(container));
        
        if (ttisarray(container)) {
          /* Array iteration */
          Array *arr = arrayvalue(container);
          aql_Integer index = ivalue(s2v(ra));
          
          if (index >= (aql_Integer)arr->length) {
            setnilvalue(s2v(rc));  /* end of iteration */
            printf_debug("[DEBUG] OP_ITER_NEXT: array iteration finished at index %lld\n", (long long)index);
          } else {
            setobj2s(L, rc, &arr->data[index]);  /* get current element */
            setivalue(s2v(ra), index + 1);  /* increment index */
            printf_debug("[DEBUG] OP_ITER_NEXT: array element at index %lld\n", (long long)index);
          }
        } else if (ttisdict(container)) {
          /* Dict iteration */
          Dict *dict = dictvalue(container);
          aql_Integer index = ivalue(s2v(ra));
          
          /* Find next non-empty entry */
          while (index < (aql_Integer)dict->capacity) {
            DictEntry *entry = &dict->entries[index];
            if (!aqlD_entry_empty(entry)) {
              setobj2s(L, rc, &entry->value);  /* return value (could also return key) */
              setivalue(s2v(ra), index + 1);  /* increment index */
              printf_debug("[DEBUG] OP_ITER_NEXT: dict entry at index %lld\n", (long long)index);
              break;
            }
            index++;
          }
          
          if (index >= (aql_Integer)dict->capacity) {
            setnilvalue(s2v(rc));  /* end of iteration */
            printf_debug("[DEBUG] OP_ITER_NEXT: dict iteration finished\n");
          }
        } else if (ttisrange(container)) {
          /* Range iteration */
          RangeObject *range = rangevalue(container);
          
          if (range->finished || range->count <= 0) {
            setnilvalue(s2v(rc));  /* end of iteration */
            printf_debug("[DEBUG] OP_ITER_NEXT: range iteration finished\n");
          } else {
            setivalue(s2v(rc), range->current);  /* return current value */
            range->current += range->step;  /* advance */
            range->count--;
            if (range->count <= 0) {
              range->finished = 1;
            }
            printf_debug("[DEBUG] OP_ITER_NEXT: range value %lld\n", (long long)ivalue(s2v(rc)));
          }
        } else {
          setnilvalue(s2v(rc));  /* unknown type, end iteration */
          printf_debug("[DEBUG] OP_ITER_NEXT: unknown container type, ending iteration\n");
        }
        continue;
      }
      
      default: {
        aql_assert(0);
        return 0;
      }
    }
    
    /* Trace register state after each instruction */
    debug_trace_registers(L, base, pc, cl->p);
    
#ifdef AQL_DEBUG_BUILD
    /* Print register state after instruction execution */
    if (trace_registers) {
      int current_reg_count = (int)(L->top - base);
      if (current_reg_count > 0) {
        /* Find which registers changed */
        int changed_regs[32];  /* Max 32 changed registers to track */
        int changed_count = 0;
        
        for (int j = 0; j < current_reg_count && j < max_registers && changed_count < 32; j++) {
          TValue *current_reg = s2v(base + j);
          TValue *prev_reg = &prev_registers[j];
          
          /* Check if register changed */
          int changed = 0;
          if (rawtt(current_reg) != rawtt(prev_reg)) {
            changed = 1;
          } else {
            switch (rawtt(current_reg)) {
              case AQL_TNIL:
                /* nil values are always equal */
                break;
              case AQL_TBOOLEAN:
                if (bvalue(current_reg) != bvalue(prev_reg)) {
                  changed = 1;
                }
                break;
              case AQL_TNUMBER:
                if (ttisinteger(current_reg)) {
                  if (!ttisinteger(prev_reg) || ivalue(current_reg) != ivalue(prev_reg)) {
                    changed = 1;
                  }
                } else {
                  if (ttisinteger(prev_reg) || fltvalue(current_reg) != fltvalue(prev_reg)) {
                    changed = 1;
                  }
                }
                break;
              default:
                /* For complex types, assume changed if pointers differ */
                if (gcvalue(current_reg) != gcvalue(prev_reg)) {
                  changed = 1;
                }
                break;
            }
          }
          
          if (changed) {
            changed_regs[changed_count++] = j;
          }
        }
        
        /* Print register state with changes highlighted */
        aqlD_print_register_state(s2v(base), current_reg_count, changed_regs, changed_count);
        
        /* Update previous register state for next iteration */
        for (int j = 0; j < current_reg_count; j++) {
          setobj(L, &prev_registers[j], s2v(base + j));
        }
      }
    }
#endif
    
    /* Trace register state after instruction execution */
    debug_trace_registers(L, base, pc, cl->p);
    
newframe:
    cl = clLvalue(s2v(ci->func));
    base = ci->func + 1;
    pc = ci->u.l.savedpc;
    k = cl->p->k;
  }
  
#ifdef AQL_DEBUG_BUILD
  /* Clean up register tracking memory */
  if (prev_registers) {
    free(prev_registers);
  }
#endif
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