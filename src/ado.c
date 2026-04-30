/*
** $Id: ado.c $
** Stack and Call structure of AQL
** See Copyright Notice in aql.h
*/

#define ado_c
#define AQL_CORE

#include "aconf.h"

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#include "aql.h"

#include "aapi.h"
#include "adebug.h"
#include "adebug_user.h"
#include "ado.h"
#include "afunc.h"
#include "agc.h"
#include "amem.h"
#include "aobject.h"
#include "aopcodes.h"
#include "aparser.h"
#include "astate.h"
#include "astack_config.h"
#include "astring.h"
#include "adict.h"
#include "acontainer.h"
/* #include "avm.h" - removed to avoid conflicts */
#include "azio.h"

/* Forward declarations to avoid circular dependencies */
AQL_API int aqlV_execute(aql_State *L, CallInfo *ci);
extern Dict *get_globals_dict(aql_State *L);
extern CallInfo *aqlE_extendCI(aql_State *L);

#define errorstatus(s)	((s) > AQL_YIELD)

/* maximum number of C calls */
#ifndef AQL_MAXCCALLS
#define AQL_MAXCCALLS		200
#endif

/*
** {======================================================
** Error-recovery functions
** =======================================================
*/

/*
** Set error object at the top of the stack
*/
AQL_API void aqlD_seterrorobj(aql_State *L, int errcode, StkId oldtop) {
  switch (errcode) {
    case AQL_ERRMEM: {  /* memory error? */
      /* For now, just set a simple error message */
      /* In full implementation, would use preregistered message */
      setnilvalue(s2v(oldtop));
      break;
    }
    case AQL_OK: {  /* special case only for closing upvalues */
      setnilvalue(s2v(oldtop));  /* no error message */
      break;
    }
    default: {
      /* For other errors, set nil for now */
      setnilvalue(s2v(oldtop));
      break;
    }
  }
  L->top.p = oldtop + 1;
}

/*
** Throw an error
*/
AQL_API l_noret aqlD_throw(aql_State *L, int errcode) {
  if (L->errorJmp) {  /* thread has an error handler? */
    L->errorJmp->status = errcode;  /* set status */
    longjmp(L->errorJmp->b, 1);  /* jump back */
  } else {  /* thread has no error handler */
    global_State *g = G(L);
    /* errcode = aqlE_resetthread(L, errcode); */ /* close all upvalues - function not available yet */
    if (g->panic) {  /* panic function? */
      /* aql_unlock(L); */ /* function not available yet */
      g->panic(L);  /* call panic function (last chance to jump out) */
    }
    /* Basic error throwing - for MVP, just exit */
    fprintf(stderr, "AQL Error: Exception thrown with code %d\n", errcode);
    exit(errcode);
  }
}

/*
** Run protected function
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};

static void f_call(aql_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  aqlD_callnoyield(L, c->func, c->nresults);
}

/*
** Execute a protected call
*/
AQL_API int aqlD_pcall(aql_State *L, Pfunc func, void *u,
                       ptrdiff_t oldtop, ptrdiff_t ef) {
  int status;
  CallInfo *old_ci = L->ci;
  aql_byte old_allowhooks = L->allowhook;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = aqlD_rawrunprotected(L, func, u);
  if (l_unlikely(status != AQL_OK)) {  /* an error occurred? */
    L->ci = old_ci;
    L->allowhook = old_allowhooks;
    L->errfunc = old_errfunc;
    aqlD_seterrorobj(L, status, restorestack(L, oldtop));
  }
  return status;
}

/*
** Execute a protected function
*/
AQL_API int aqlD_rawrunprotected(aql_State *L, Pfunc f, void *ud) {
  int oldnCcalls = L->nCcalls;
  struct aql_longjmp lj;
  lj.status = AQL_OK;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  if (setjmp(lj.b) == 0) {
    (*f)(L, ud);
  }
  L->errorJmp = lj.previous;  /* restore old error handler */
  L->nCcalls = oldnCcalls;
  return lj.status;
}

/* }====================================================== */

/*
** {======================================================
** Stack reallocation
** =======================================================
*/

/*
** Change all stack pointers to offsets (like Lua's relstack)
*/
static void relstack(aql_State *L) {
  CallInfo *ci;
  UpVal *up;
  
  aql_debug("[DEBUG] relstack: converting pointers to offsets (Lua-style)\n");
  
  /* Convert main stack pointers to offsets - Lua style */
  L->top.offset = savestack(L, L->top.p);
  L->stack_last.offset = savestack(L, L->stack_last.p);
  aql_debug("[DEBUG] relstack: L->top offset = %ld\n", (long)L->top.offset);
  
  /* Convert CallInfo pointers to offsets - Lua style with safety checks */
  int ci_count = 0;
  for (ci = L->ci; ci != NULL && ci_count < AQL_MAX_CALLINFO_REALLOC; ci = ci->previous, ci_count++) {
    /* Safety check: ensure CallInfo is valid */
    if ((char*)ci < (char*)0x1000 || (char*)ci > (char*)0x7fffffffffff) {
      aql_debug("[ERROR] relstack: invalid CallInfo pointer %p at index %d\n", (void*)ci, ci_count);
      break;
    }
    
    ci->top.offset = savestack(L, ci->top.p);
    ci->func.offset = savestack(L, ci->func.p);
    aql_debug("[DEBUG] relstack: ci=%p, top_offset=%ld, func_offset=%ld (count=%d)\n", 
                 (void*)ci, (long)ci->top.offset, (long)ci->func.offset, ci_count);
  }
  aql_debug("[DEBUG] relstack: processed %d CallInfo entries\n", ci_count);
  
  /* Convert upvalue pointers to offsets - Lua style */
  for (up = L->openupval; up != NULL; up = up->u.open.next) {
    if (up->v.p >= s2v(L->stack.p) && up->v.p < s2v(L->stack_last.p)) {
      up->v.offset = savestack(L, (StkId)((char*)up->v.p - offsetof(StackValue, val)));
      aql_debug("[DEBUG] relstack: upvalue offset = %ld\n", (long)up->v.offset);
    }
  }
  
  aql_debug("[DEBUG] relstack: conversion completed\n");
}

/*
** Change all offsets back to pointers (like Lua's correctstack)
*/
static void correctstack(aql_State *L, int newsize) {
  CallInfo *ci;
  UpVal *up;
  
  aql_debug("[DEBUG] correctstack: converting offsets back to pointers (Lua-style)\n");
  
  /* Convert main stack offsets back to pointers - Lua style */
  L->top.p = restorestack(L, L->top.offset);
  L->stack_last.p = restorestack(L, L->stack_last.offset);
  aql_debug("[DEBUG] correctstack: restored L->top = %p\n", (void*)L->top.p);
  
  /* Convert CallInfo offsets back to pointers - Lua style with safety checks */
  int ci_count = 0;
  for (ci = L->ci; ci != NULL && ci_count < AQL_MAX_CALLINFO_REALLOC; ci = ci->previous, ci_count++) {
    /* Safety check: ensure CallInfo is valid */
    if ((char*)ci < (char*)0x1000 || (char*)ci > (char*)0x7fffffffffff) {
      aql_debug("[ERROR] correctstack: invalid CallInfo pointer %p at index %d\n", (void*)ci, ci_count);
      break;
    }
    
    /* Restore stack pointers */
    ci->top.p = restorestack(L, ci->top.offset);
    ci->func.p = restorestack(L, ci->func.offset);
    aql_debug("[DEBUG] correctstack: restored ci=%p, top=%p, func=%p (count=%d)\n", 
                 (void*)ci, (void*)ci->top.p, (void*)ci->func.p, ci_count);
    
    /* Safety check: ensure restored pointers are reasonable */
    if (ci->top.p < L->stack.p || ci->top.p > L->stack.p + newsize + EXTRA_STACK) {
      aql_debug("[ERROR] correctstack: restored ci->top %p out of bounds\n", (void*)ci->top.p);
    }
    if (ci->func.p < L->stack.p || ci->func.p > L->stack.p + newsize + EXTRA_STACK) {
      aql_debug("[ERROR] correctstack: restored ci->func %p out of bounds\n", (void*)ci->func.p);
    }
  }
  aql_debug("[DEBUG] correctstack: processed %d CallInfo entries\n", ci_count);
  
  /* Convert upvalue offsets back to pointers - Lua style with safety */
  int up_count = 0;
  for (up = L->openupval; up != NULL && up_count < AQL_MAX_UPVALUE_REALLOC; up = up->u.open.next, up_count++) {
    /* Safety check: ensure upvalue is valid */
    if ((char*)up < (char*)0x1000 || (char*)up > (char*)0x7fffffffffff) {
      aql_debug("[ERROR] correctstack: invalid upvalue pointer %p at index %d\n", (void*)up, up_count);
      break;
    }
    
    if (up->v.offset >= 0 && up->v.offset < (ptrdiff_t)(newsize * sizeof(StackValue))) {
      StkId restored = restorestack(L, up->v.offset);
      up->v.p = s2v(restored);
      aql_debug("[DEBUG] correctstack: restored upvalue = %p (from offset %ld, count=%d)\n", 
                   (void*)up->v.p, (long)up->v.offset, up_count);
    } else {
      aql_debug("[DEBUG] correctstack: skipping upvalue with invalid offset %ld\n", (long)up->v.offset);
    }
  }
  aql_debug("[DEBUG] correctstack: processed %d upvalue entries\n", up_count);
  
  aql_debug("[DEBUG] correctstack: conversion completed\n");
}

/*
** Reallocate stack with new size
*/
int aqlD_reallocstack_impl(aql_State *L, int newsize, int raiseerror) {
  int oldsize = stacksize(L);
  int lim = oldsize;
  StackValue *newstack;
  global_State *g = G(L);
  
  aql_debug("[DEBUG] reallocstack: starting Lua-style stack reallocation (oldsize=%d, newsize=%d)\n", 
               oldsize, newsize);
  
  aql_assert(newsize <= AQL_MAXSTACK || newsize == ERRORSTACKSIZE);
  aql_assert(L->stack_last.p - L->stack.p == oldsize - EXTRA_STACK);
  
  /* Step 1: Convert all pointers to offsets (like Lua's relstack) */
  relstack(L);
  
  /* Step 2: Disable emergency GC during reallocation (like Lua) */
  aql_byte old_gcemergency = g->gcemergency;
  g->gcemergency = 1;  /* Stop emergency collection */
  aql_debug("[DEBUG] reallocstack: disabled emergency GC\n");
  
  /* Step 3: Reallocate stack memory */
  newstack = aqlM_reallocvector(L, L->stack.p, oldsize + EXTRA_STACK, newsize + EXTRA_STACK, StackValue);
  
  /* Step 4: Restore GC state */
  g->gcemergency = old_gcemergency;
  aql_debug("[DEBUG] reallocstack: restored emergency GC\n");
  
  /* Step 5: Handle reallocation failure */
  if (newstack == NULL) {
    aql_debug("[ERROR] reallocstack: reallocation failed\n");
    correctstack(L, stacksize(L));  /* restore pointers from offsets */
    if (raiseerror) {
      aqlG_runerror(L, "stack overflow");
    }
    return 0;  /* failure */
  }
  
  /* Step 6: Update stack pointer and convert offsets back to pointers */
  L->stack.p = newstack;
  correctstack(L, newsize);
  
  /* Step 7: Initialize new stack segment and update stack_last */
  L->stack_last.p = L->stack.p + newsize;
  for (; lim < newsize + EXTRA_STACK; lim++)
    setnilvalue(s2v(newstack + lim)); /* erase new segment */
  
  aql_debug("[DEBUG] reallocstack: Lua-style stack reallocation completed successfully\n");
  aql_debug("[DEBUG] reallocstack: new stack range: %p to %p (size=%d)\n", 
               (void*)L->stack.p, (void*)L->stack_last.p, newsize);
  return 1;  /* success */
}

/*
** Smart stack growth calculation based on current depth and usage patterns
*/
static int aql_calculate_smart_stack_size(aql_State *L, int current_size, int needed) {
  int current_depth = L->nCcalls;
  int new_size;
  
  /* Phase 1: Small recursion (0-1000 calls) - Exponential growth */
  if (current_depth < 1000) {
    new_size = current_size * 2;  /* Double the size */
  }
  /* Phase 2: Medium recursion (1000-50000 calls) - Linear growth */
  else if (current_depth < 50000) {
    new_size = current_size + 5000;  /* Add 5000 elements */
  }
  /* Phase 3: Deep recursion (50000+ calls) - Conservative growth */
  else {
    new_size = current_size + 10000;  /* Add 10000 elements */
  }
  
  /* Ensure we meet minimum requirements */
  if (new_size < needed) {
    new_size = needed + 1000;  /* Add safety buffer */
  }
  
  aql_debug("[DEBUG] Smart growth: depth=%d, old_size=%d, new_size=%d, needed=%d\n", 
               current_depth, current_size, new_size, needed);
  
  return new_size;
}

/*
** Try to grow stack by at least 'n' elements with smart growth strategy
*/
AQL_API int aqlD_growstack(aql_State *L, int n, int raiseerror) {
  int size = stacksize(L);
  if (l_unlikely(size > AQL_MAXSTACK)) {
    /* if stack is larger than maximum, thread is already using the
       extra space reserved for errors, that is, thread is handling
       a stack error; cannot grow further than that. */
    aql_assert(stacksize(L) == ERRORSTACKSIZE);
    if (raiseerror)
      aqlD_throw(L, AQL_ERRERR);  /* error inside message handler */
    return 0;  /* if not 'raiseerror', just signal it */
  } else {
    int needed = cast_int(L->top.p - L->stack.p) + n;
    int newsize = aql_calculate_smart_stack_size(L, size, needed);
    
    if (newsize > AQL_MAXSTACK)  /* cannot cross the limit */
      newsize = AQL_MAXSTACK;
    if (newsize < size)  /* overflow in 'newsize'? */
      newsize = AQL_MAXSTACK;  /* assume maximum possible size */
      
    return aqlD_reallocstack_impl(L, newsize, raiseerror);
  }
}

/*
** Shrink stack to its current top
*/
AQL_API void aqlD_shrinkstack(aql_State *L) {
  int inuse = cast_int(L->top.p - L->stack.p);
  int goodsize = inuse + (inuse / 8) + 2*EXTRA_STACK;
  if (goodsize > AQL_MAXSTACK)
    goodsize = AQL_MAXSTACK;  /* respect stack limit */
  if (stacksize(L) > AQL_MAXSTACK)  /* had been handling stack overflow? */
    /* aqlE_freeCI(L); */ /* free all CIs (list grew because of an error) - function not available yet */
    (void)0; /* placeholder */
  else
    /* aqlE_shrinkCI(L); */ /* shrink list - function not available yet */
    (void)0; /* placeholder */
  /* if thread is currently not handling a stack overflow and its
     good size is smaller than current size, shrink its stack */
  if (inuse <= (AQL_MAXSTACK - EXTRA_STACK) &&
      goodsize < stacksize(L))
    (void)aqlD_reallocstack_impl(L, goodsize, 0);  /* don't fail */
  else  /* don't change stack */
    condmovestack(L,,);  /* (change only for debugging) */
}

/*
** Increment top
*/
AQL_API void aqlD_inctop(aql_State *L) {
  aqlD_checkstack(L, 1);
  L->top.p++;
}

/* }====================================================== */

/*
** {======================================================
** Call and return functions
** =======================================================
*/

/*
** Move results from function call to proper place
*/
static void moveresults(aql_State *L, StkId res, int nres, int wanted) {
  aql_debug("[DEBUG] moveresults: res=%p, nres=%d, wanted=%d, L->top=%p\n", 
               (void*)res, nres, wanted, (void*)L->top.p);
  
  switch (wanted) {
    case 0:  /* no values needed */
      L->top.p = res;
      return;
    case 1:  /* one value needed */
      if (nres == 0) {  /* no results? */
        aql_debug("[DEBUG] moveresults: no results, setting nil\n");
        setnilvalue(s2v(res));  /* adjust with nil */
      } else {  /* at least one result */
        StkId src = L->top.p - nres;
        aql_debug("[DEBUG] moveresults: moving result from %p to %p\n", (void*)src, (void*)res);
        
        /* Safety check: ensure src is within stack bounds */
        if (src < L->stack.p || src >= L->stack_last.p) {
          aql_debug("[ERROR] moveresults: src %p out of bounds (stack=%p, stack_last=%p)\n", 
                       (void*)src, (void*)L->stack.p, (void*)L->stack_last.p);
          aqlG_runerror(L, "moveresults: source pointer out of bounds");
          return;
        }
        
        /* Safety check: ensure res is within stack bounds */
        if (res < L->stack.p || res >= L->stack_last.p) {
          aql_debug("[ERROR] moveresults: res %p out of bounds (stack=%p, stack_last=%p)\n", 
                       (void*)res, (void*)L->stack.p, (void*)L->stack_last.p);
          aqlG_runerror(L, "moveresults: result pointer out of bounds");
          return;
        }
        
        aql_debug("[DEBUG] moveresults: source value type=%d\n", rawtt(s2v(src)));
        
        /* Additional safety check: verify src and res pointers are valid */
        if ((char*)src < (char*)L->stack.p || (char*)src >= (char*)L->stack_last.p) {
          aql_debug("[ERROR] moveresults: src pointer %p invalid after bounds check\n", (void*)src);
          aqlG_runerror(L, "moveresults: invalid source pointer");
          return;
        }
        if ((char*)res < (char*)L->stack.p || (char*)res >= (char*)L->stack_last.p) {
          aql_debug("[ERROR] moveresults: res pointer %p invalid after bounds check\n", (void*)res);
          aqlG_runerror(L, "moveresults: invalid result pointer");
          return;
        }
        
        aql_debug("[DEBUG] moveresults: about to call setobj2s(res=%p, src=%p)\n", (void*)res, (void*)src);
        setobj2s(L, res, s2v(src));  /* move it to proper place */
        aql_debug("[DEBUG] moveresults: setobj2s completed successfully\n");
      }
      L->top.p = res + 1;
      return;
    case AQL_MULTRET:
      wanted = nres;  /* we want all results */
      /* fall through */
    default:
      if (wanted <= nres) {
        /* move wanted results */
        aql_debug("[DEBUG] moveresults: moving %d results from L->top-nres=%p to res=%p\n", 
                     wanted, (void*)(L->top.p - nres), (void*)res);
        for (int i = 0; i < wanted; i++) {
          StkId src = L->top.p - nres + i;
          StkId dst = res + i;
          aql_debug("[DEBUG] moveresults: [%d] moving from %p (type=%d, val=%d) to %p\n", 
                       i, (void*)src, rawtt(s2v(src)), 
                       (ttisinteger(s2v(src)) ? ivalue(s2v(src)) : -999), (void*)dst);
          setobj2s(L, dst, s2v(src));
        }
        L->top.p = res + wanted;
      } else {
        /* move all results and fill with nil */
        for (int i = 0; i < nres; i++)
          setobj2s(L, res + i, s2v(L->top.p - nres + i));
        for (int i = nres; i < wanted; i++)
          setnilvalue(s2v(res + i));
        L->top.p = res + wanted;
      }
      return;
  }
}

/*
** Post-call function (handles function return)
*/
AQL_API int aqlD_poscall(aql_State *L, CallInfo *ci, int nres) {
  int wanted = ci->nresults;
  
  aql_debug("[DEBUG] aqlD_poscall: ci=%p, nres=%d, wanted=%d\n", (void*)ci, nres, wanted);
  
  /* Move results to proper place */
  moveresults(L, ci->func.p, nres, wanted);
  
  /* CRITICAL: Close upvalues before returning (like Lua) */
  /* This preserves captured variables by copying them from stack to upvalue storage */
  StkId func_base = ci->func.p + 1;  /* base of function being returned from */
  aql_debug("[DEBUG] aqlD_poscall: closing upvalues at level %p\n", (void*)func_base);
  aqlF_closeupval(L, func_base);
  
  /* Check if this is returning to the base C frame. */
  if (ci->previous == NULL || ci->previous == &L->base_ci) {
    aql_debug("[DEBUG] aqlD_poscall: base call, returning 1 (exit VM)\n");
    return 1;  /* Exit VM execution */
  }
  
  /* Restore previous CallInfo */
  L->ci = ci->previous;
  aql_debug("[DEBUG] aqlD_poscall: restored previous CallInfo, returning 0 (continue)\n");
  
  /* Continue execution in caller */
  return 0;
}

/*
** Prepare a tail call
*/
AQL_API int aqlD_pretailcall(aql_State *L, CallInfo *ci, StkId func, int narg1, int delta) {
  /* Tail call optimization implementation (based on Lua's luaD_pretailcall) */
  aql_debug("[DEBUG] aqlD_pretailcall: func=%p, narg1=%d, delta=%d\n", 
               (void*)func, narg1, delta);
  
  /* Check if it's a Lua function */
  if (ttisclosure(s2v(func))) {
    LClosure *cl = clLvalue(s2v(func));
    Proto *p = cl->p;
    int fsize = p->maxstacksize;  /* frame size */
    int nfixparams = p->numparams;
    
    aql_debug("[DEBUG] pretailcall: Lua function, fsize=%d, nfixparams=%d, narg1=%d\n", 
                 fsize, nfixparams, narg1);
    
    /* Move down function and arguments (like Lua) */
    ci->func.p -= delta;  /* restore 'func' (if vararg) */
    for (int i = 0; i < narg1; i++) {  /* move down function and arguments */
      setobj2s(L, ci->func.p + i, s2v(func + i));
      aql_debug("[DEBUG] pretailcall: moved func+arg %d from %p to %p\n", 
                   i, (void*)(func + i), (void*)(ci->func.p + i));
    }
    func = ci->func.p;  /* moved-down function */
    
    /* Complete missing arguments with nil (like Lua) */
    for (; narg1 <= nfixparams; narg1++) {
      setnilvalue(s2v(func + narg1));
      aql_debug("[DEBUG] pretailcall: set nil for missing arg %d\n", narg1);
    }
    
    /* Update call info for new function with safety buffer */
    int safe_fsize = fsize + AQL_FUNCTION_STACK_SAFETY;  /* Add safety buffer */
    ci->top.p = func + 1 + safe_fsize;  /* top for new function */
    ci->u.l.savedpc = p->code;  /* starting point */
    ci->callstatus |= CIST_TAIL;  /* Mark as tail call - CRITICAL for TCO */
    L->top.p = func + narg1;  /* set top */
    
    aql_debug("[DEBUG] pretailcall: tail call optimized, reusing CallInfo (no stack growth)\n");
    aql_debug("[DEBUG] pretailcall: ci->callstatus=0x%x, CIST_TAIL=%d\n", 
                 ci->callstatus, (ci->callstatus & CIST_TAIL) ? 1 : 0);
    return -1;  /* Lua function, continue execution without new CallInfo */
  } else {
    /* C function - not optimized for now */
    aql_debug("[DEBUG] pretailcall: C function, no optimization\n");
    return 0;
  }
}

/*
** Call a function (without yielding)
*/
AQL_API void aqlD_callnoyield(aql_State *L, StkId func, int nResults) {
  L->nCcalls++;
  if (l_unlikely(L->nCcalls >= AQL_MAXCCALLS)) {
    aqlD_throw(L, AQL_ERRERR);
  }
  if (aqlD_precall(L, func, nResults) != NULL) {  /* is a AQL function? */
    aqlV_execute(L, L->ci);  /* call it */
  }
  L->nCcalls--;
}

/*
** Call a function
*/
AQL_API void aqlD_call(aql_State *L, StkId func, int nResults) {
  aqlD_callnoyield(L, func, nResults);
}

/*
** Prepare a function call
*/
AQL_API CallInfo *aqlD_precall(aql_State *L, StkId func, int nResults) {
  TValue *f = s2v(func);
  ptrdiff_t func_offset = savestack(L, func);  /* Save func offset before potential reallocation */
  
  if (ttisLclosure(f)) {
    /* AQL function call - set up proper CallInfo */
    LClosure *cl = clLvalue(f);
    Proto *p = cl->p;
    int n = cast_int(L->top.p - func) - 1;  /* number of arguments */
    
    aql_debug("[DEBUG] aqlD_precall: AQL function, proto=%p, code=%p, nargs=%d, numparams=%d\n", 
                 (void*)p, (void*)p->code, n, p->numparams);
    aql_debug("[DEBUG] aqlD_precall: p->maxstacksize=%d, func=%p, L->top=%p, L->stack_last=%p\n",
                 p->maxstacksize, (void*)func, (void*)L->top.p, (void*)L->stack_last.p);
    
    /* Check stack overflow and handle with dynamic stack growth */
    if (L->top.p + p->maxstacksize > L->stack_last.p) {
      aql_debug("[DEBUG] aqlD_precall: stack overflow detected at depth %d\n", L->nCcalls);
      
      /* Calculate needed stack space */
      int needed = cast_int((L->top.p + p->maxstacksize) - L->stack_last.p) + EXTRA_STACK;
      aql_debug("[DEBUG] aqlD_precall: attempting stack growth, needed=%d\n", needed);
      
      /* Attempt stack reallocation (fixed upvalue bug enables this) */
      if (!aqlD_growstack(L, needed, 0)) {
        aql_debug("[DEBUG] aqlD_precall: failed to grow stack, aborting\n");
        aqlG_runerror(L, "stack overflow - unable to allocate memory");
        return NULL;
      }
      
      /* CRITICAL: Restore func pointer after stack reallocation */
      func = restorestack(L, func_offset);
      f = s2v(func);  /* Update f pointer as well */
      
      aql_debug("[DEBUG] aqlD_precall: stack grown successfully to %d elements\n", stacksize(L));
      aql_debug("[DEBUG] aqlD_precall: restored func pointer to %p after reallocation\n", (void*)func);
    }
    
    /* Adjust arguments to match parameters */
    for (int i = n; i < p->numparams; i++) {
      setnilvalue(s2v(L->top.p++));  /* fill missing parameters with nil */
    }
    
    /* CRITICAL: Copy arguments to the correct positions for the function */
    /* Arguments are currently at func+1, func+2, ... func+n */
    /* They need to stay there because base will be set to func+1 */
    aql_debug("[DEBUG] aqlD_precall: arguments already in correct position at func+1 to func+%d\n", n);
    
    /* Create new CallInfo for the function */
    CallInfo *ci = aqlE_extendCI(L);
    ci->func.p = func;
    ci->u.l.savedpc = p->code;  /* Start at beginning of function code */
    ci->callstatus = 0;  /* AQL function */
    /* Set stack top with safety buffer for compiler register allocation issues */
    int safe_stacksize = p->maxstacksize + AQL_FUNCTION_STACK_SAFETY;  /* Add safety buffer for complex functions */
    ci->top.p = func + 1 + safe_stacksize;
    aql_debug("[DEBUG] aqlD_precall: setting ci->top with maxstacksize=%d + safety=%d = %d\n",
                 p->maxstacksize, AQL_FUNCTION_STACK_SAFETY, safe_stacksize);
    aql_debug("[DEBUG] aqlD_precall: ci->top=%p, func=%p, func+1=%p\n",
                 (void*)ci->top.p, (void*)func, (void*)(func + 1));
    
    /* Ensure we have enough stack space for the safety buffer */
    if (ci->top.p > L->stack_last.p) {
      aql_debug("[DEBUG] aqlD_precall: need more stack space for safety buffer\n");
      int additional_needed = cast_int(ci->top.p - L->stack_last.p) + EXTRA_STACK;
      if (aqlD_growstack(L, additional_needed, 0)) {
        /* Restore func pointer after potential stack reallocation */
        func = restorestack(L, func_offset);
        ci->func.p = func;
        ci->top.p = func + 1 + safe_stacksize;
        aql_debug("[DEBUG] aqlD_precall: stack grown for safety buffer, new ci->top=%p\n", 
                     (void*)ci->top.p);
      } else {
        /* Fallback: use original maxstacksize */
        ci->top.p = func + 1 + p->maxstacksize;
        aql_debug("[DEBUG] aqlD_precall: using original maxstacksize, ci->top=%p\n", 
                     (void*)ci->top.p);
      }
    }
    ci->nresults = nResults;  /* Set expected results */
    ci->previous = L->ci;  /* Set previous CallInfo for return */
    
    /* CRITICAL: Set the base pointer for the function */
    /* In AQL/Lua, function parameters start at func+1 */
    /* This makes register 0 in the function point to the first parameter */
    
    /* Adjust L->ci to point to new CallInfo */
    L->ci = ci;
    
    aql_debug("[DEBUG] aqlD_precall: AQL function CallInfo set up, func=%p, base will be %p\n", 
                 (void*)func, (void*)(func + 1));
    return ci;  /* Return new CallInfo for AQL function */
  } else if (ttisCclosure(f)) {
    /* C function call */
    CClosure *ccl = clCvalue(f);
    int n = cast_int(L->top.p - func) - 1;  /* number of arguments */
    
    /* Call C function */
    int nres = ccl->f(L);
    
    /* Adjust results */
    if (nResults == AQL_MULTRET) {
      nResults = nres;
    } else {
      /* Adjust to expected number of results */
      while (nres > nResults) {
        L->top.p--;
        nres--;
      }
      while (nres < nResults) {
        setnilvalue(s2v(L->top.p++));
        nres++;
      }
    }
    
    return NULL;  /* C function - already processed */
  } else {
    /* Not a function */
    aqlG_typeerror(L, f, "call");
    return NULL;
  }
}

/* }====================================================== */

/*
** {======================================================
** Protected compilation and execution
** =======================================================
*/

struct CompileS {  /* data for protected compilation */
  const char *code;
  size_t len;
  const char *name;
};

static void f_compile(aql_State *L, void *ud) {
  struct CompileS *c = cast(struct CompileS *, ud);
  
  /* Create input stream */
  struct Zio z;
  aqlZ_init_string(L, &z, c->code, c->len);
  
  /* Compile using the main parser */
  Mbuffer buff;
  aqlZ_initbuffer(L, &buff);
  
  Dyndata dyd;
  memset(&dyd, 0, sizeof(dyd));
  
  /* Parse and generate bytecode */
  LClosure *cl = aqlY_parser(L, &z, &buff, &dyd, c->name, c->code[0]);
  
  aqlZ_freebuffer(L, &buff);
  aqlZ_cleanup_string(L, &z);
  
  if (cl) {
    if (aqlD_get_debug_flags() & AQL_DEBUG_CODE) {
      aqlD_print_function_bytecode(cl->p, c->name);
    }

    /* Push compiled function onto stack */
    setclLvalue2s(L, L->top.p, cl);
    L->top.p++;
    
    /* Set up global environment as first upvalue (like Lua's _ENV) */
    if (cl->nupvalues >= 1) {
      /* First, ensure upvalues are initialized */
      if (cl->upvals[0] == NULL) {
        aqlF_initupvals(L, cl);
      }
      
      /*
      ** AQL uses a dict-backed global environment. Create it eagerly for
      ** compiled chunks so source-level _ENV accesses do not start from nil.
      */
      Dict *globals_dict = get_globals_dict(L);
      if (globals_dict != NULL) {
        setobj(L, cl->upvals[0]->v.p, &G(L)->l_globals);
      } else {
        setnilvalue(cl->upvals[0]->v.p);
      }
    }
  } else {
    aqlD_throw(L, AQL_ERRSYNTAX);  /* Compilation failed */
  }
}

/*
** Protected compilation
*/
AQL_API int aqlD_protectedcompile(aql_State *L, const char *code, size_t len, const char *name) {
  struct CompileS c;
  c.code = code;
  c.len = len;
  c.name = name;
  
  return aqlD_rawrunprotected(L, f_compile, &c);
}

struct ExecuteS {  /* data for protected execution */
  int nargs;
  int nresults;
};

static void f_execute(aql_State *L, void *ud) {
  struct ExecuteS *e = cast(struct ExecuteS *, ud);

  if (!L || !L->ci || !L->ci->func.p) {
    aqlD_throw(L, AQL_ERRRUN);
  }

  if (L->top.p < L->stack.p + e->nargs + 1) {
    aqlD_throw(L, AQL_ERRRUN);
  }

  /*
  ** Execute compiled chunks through the normal call pipeline instead of
  ** hand-building a CallInfo. This matches Lua's protected-call flow and
  ** keeps call/return bookkeeping in one place.
  */
  aqlD_callnoyield(L, L->top.p - (e->nargs + 1), e->nresults);
}

/*
** Protected execution
*/
AQL_API int aqlD_protectedexecute(aql_State *L, int nargs, int nresults) {
  struct ExecuteS e;
  e.nargs = nargs;
  e.nresults = nresults;
  
  return aqlD_rawrunprotected(L, f_execute, &e);
}

/* }====================================================== */
