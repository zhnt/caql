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
#include "adebug_internal.h"
#include "ado.h"
#include "afunc.h"
#include "agc.h"
#include "amem.h"
#include "aobject.h"
#include "aopcodes.h"
#include "aparser.h"
#include "astate.h"
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
  L->top = oldtop + 1;
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
AQL_API int aqlD_pcall(aql_State *L, aql_CFunction func, void *u,
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
  
  printf_debug("[DEBUG] relstack: converting pointers to offsets\n");
  
  /* Convert main stack pointers to offsets */
  L->top = (StkId)(L->top - L->stack);
  printf_debug("[DEBUG] relstack: L->top offset = %ld\n", (long)(ptrdiff_t)L->top);
  
  /* Convert CallInfo pointers to offsets */
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top = (StkId)(ci->top - L->stack);
    ci->func = (StkId)(ci->func - L->stack);
    printf_debug("[DEBUG] relstack: ci=%p, top_offset=%ld, func_offset=%ld\n", 
                 (void*)ci, (long)(ptrdiff_t)ci->top, (long)(ptrdiff_t)ci->func);
  }
  
  /* Convert upvalue pointers to offsets */
  for (up = L->openupval; up != NULL; up = up->u.open.next) {
    if (up->v.p >= s2v(L->stack) && up->v.p < s2v(L->stack_last)) {
      StackValue *stk = (StackValue*)((char*)up->v.p - offsetof(StackValue, val));
      up->v.p = (TValue*)(stk - L->stack);
      printf_debug("[DEBUG] relstack: upvalue offset = %ld\n", (long)(ptrdiff_t)up->v.p);
    }
  }
  
  printf_debug("[DEBUG] relstack: conversion completed\n");
}

/*
** Change all offsets back to pointers (like Lua's correctstack)
*/
static void correctstack(aql_State *L) {
  CallInfo *ci;
  UpVal *up;
  
  printf_debug("[DEBUG] correctstack: converting offsets back to pointers\n");
  
  /* Convert main stack offsets back to pointers */
  L->top = L->stack + (ptrdiff_t)L->top;
  printf_debug("[DEBUG] correctstack: restored L->top = %p\n", (void*)L->top);
  
  /* Convert CallInfo offsets back to pointers */
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top = L->stack + (ptrdiff_t)ci->top;
    ci->func = L->stack + (ptrdiff_t)ci->func;
    printf_debug("[DEBUG] correctstack: restored ci=%p, top=%p, func=%p\n", 
                 (void*)ci, (void*)ci->top, (void*)ci->func);
  }
  
  /* Convert upvalue offsets back to pointers */
  for (up = L->openupval; up != NULL; up = up->u.open.next) {
    if ((ptrdiff_t)up->v.p >= 0 && (ptrdiff_t)up->v.p < stacksize(L)) {
      StackValue *stk = L->stack + (ptrdiff_t)up->v.p;
      up->v.p = s2v(stk);
      printf_debug("[DEBUG] correctstack: restored upvalue = %p\n", (void*)up->v.p);
    }
  }
  
  printf_debug("[DEBUG] correctstack: conversion completed\n");
}

/*
** Reallocate stack with new size
*/
void aqlD_reallocstack_impl(aql_State *L, int newsize, int raiseerror) {
  int oldsize = stacksize(L);
  int lim = oldsize;
  StackValue *newstack;
  global_State *g = G(L);
  
  printf_debug("[DEBUG] reallocstack: starting Lua-style stack reallocation (oldsize=%d, newsize=%d)\n", 
               oldsize, newsize);
  
  aql_assert(newsize <= AQL_MAXSTACK || newsize == ERRORSTACKSIZE);
  aql_assert(L->stack_last - L->stack == oldsize - EXTRA_STACK);
  
  /* Step 1: Convert all pointers to offsets (like Lua's relstack) */
  relstack(L);
  
  /* Step 2: Disable emergency GC during reallocation (like Lua) */
  aql_byte old_gcemergency = g->gcemergency;
  g->gcemergency = 1;  /* Stop emergency collection */
  printf_debug("[DEBUG] reallocstack: disabled emergency GC\n");
  
  /* Step 3: Reallocate stack memory */
  newstack = aqlM_reallocvector(L, L->stack, oldsize + EXTRA_STACK, newsize + EXTRA_STACK, StackValue);
  
  /* Step 4: Restore GC state */
  g->gcemergency = old_gcemergency;
  printf_debug("[DEBUG] reallocstack: restored emergency GC\n");
  
  /* Step 5: Handle reallocation failure */
  if (newstack == NULL) {
    printf_debug("[ERROR] reallocstack: reallocation failed\n");
    correctstack(L);  /* restore pointers from offsets */
    if (raiseerror) {
      aqlG_runerror(L, "stack overflow");
    }
    return;
  }
  
  /* Step 6: Update stack pointer and convert offsets back to pointers */
  L->stack = newstack;
  correctstack(L);
  
  /* Step 7: Initialize new stack segment and update stack_last */
  L->stack_last = L->stack + newsize;
  for (; lim < newsize + EXTRA_STACK; lim++)
    setnilvalue(s2v(newstack + lim)); /* erase new segment */
  
  printf_debug("[DEBUG] reallocstack: Lua-style stack reallocation completed successfully\n");
}

/*
** Try to grow stack by at least 'n' elements
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
    int newsize = 2 * size;  /* tentative new size */
    int needed = cast_int(L->top - L->stack) + n;
    if (newsize < needed)  /* but must respect what was asked for */
      newsize = needed;
    if (newsize > AQL_MAXSTACK)  /* cannot cross the limit */
      newsize = AQL_MAXSTACK;
    if (newsize < size)  /* overflow in 'newsize'? */
      newsize = AQL_MAXSTACK;  /* assume maximum possible size */
    aqlD_reallocstack_impl(L, newsize, raiseerror);
    return 1;
  }
}

/*
** Shrink stack to its current top
*/
AQL_API void aqlD_shrinkstack(aql_State *L) {
  int inuse = cast_int(L->top - L->stack);
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
    aqlD_reallocstack_impl(L, goodsize, 0);  /* don't fail */
  else  /* don't change stack */
    condmovestack(L,,);  /* (change only for debugging) */
}

/*
** Increment top
*/
AQL_API void aqlD_inctop(aql_State *L) {
  aqlD_checkstack(L, 1);
  L->top++;
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
  printf_debug("[DEBUG] moveresults: res=%p, nres=%d, wanted=%d, L->top=%p\n", 
               (void*)res, nres, wanted, (void*)L->top);
  
  switch (wanted) {
    case 0:  /* no values needed */
      L->top = res;
      return;
    case 1:  /* one value needed */
      if (nres == 0) {  /* no results? */
        printf_debug("[DEBUG] moveresults: no results, setting nil\n");
        setnilvalue(s2v(res));  /* adjust with nil */
      } else {  /* at least one result */
        StkId src = L->top - nres;
        printf_debug("[DEBUG] moveresults: moving result from %p to %p\n", (void*)src, (void*)res);
        
        /* Safety check: ensure src is within stack bounds */
        if (src < L->stack || src >= L->stack_last) {
          printf_debug("[ERROR] moveresults: src %p out of bounds (stack=%p, stack_last=%p)\n", 
                       (void*)src, (void*)L->stack, (void*)L->stack_last);
          aqlG_runerror(L, "moveresults: source pointer out of bounds");
          return;
        }
        
        /* Safety check: ensure res is within stack bounds */
        if (res < L->stack || res >= L->stack_last) {
          printf_debug("[ERROR] moveresults: res %p out of bounds (stack=%p, stack_last=%p)\n", 
                       (void*)res, (void*)L->stack, (void*)L->stack_last);
          aqlG_runerror(L, "moveresults: result pointer out of bounds");
          return;
        }
        
        printf_debug("[DEBUG] moveresults: source value type=%d\n", rawtt(s2v(src)));
        
        /* Additional safety check: verify src and res pointers are valid */
        if ((char*)src < (char*)L->stack || (char*)src >= (char*)L->stack_last) {
          printf_debug("[ERROR] moveresults: src pointer %p invalid after bounds check\n", (void*)src);
          aqlG_runerror(L, "moveresults: invalid source pointer");
          return;
        }
        if ((char*)res < (char*)L->stack || (char*)res >= (char*)L->stack_last) {
          printf_debug("[ERROR] moveresults: res pointer %p invalid after bounds check\n", (void*)res);
          aqlG_runerror(L, "moveresults: invalid result pointer");
          return;
        }
        
        printf_debug("[DEBUG] moveresults: about to call setobj2s(res=%p, src=%p)\n", (void*)res, (void*)src);
        setobj2s(L, res, s2v(src));  /* move it to proper place */
        printf_debug("[DEBUG] moveresults: setobj2s completed successfully\n");
      }
      L->top = res + 1;
      return;
    case AQL_MULTRET:
      wanted = nres;  /* we want all results */
      /* fall through */
    default:
      if (wanted <= nres) {
        /* move wanted results */
        printf_debug("[DEBUG] moveresults: moving %d results from L->top-nres=%p to res=%p\n", 
                     wanted, (void*)(L->top - nres), (void*)res);
        for (int i = 0; i < wanted; i++) {
          StkId src = L->top - nres + i;
          StkId dst = res + i;
          printf_debug("[DEBUG] moveresults: [%d] moving from %p (type=%d, val=%d) to %p\n", 
                       i, (void*)src, rawtt(s2v(src)), 
                       (ttisinteger(s2v(src)) ? ivalue(s2v(src)) : -999), (void*)dst);
          setobj2s(L, dst, s2v(src));
        }
        L->top = res + wanted;
      } else {
        /* move all results and fill with nil */
        for (int i = 0; i < nres; i++)
          setobj2s(L, res + i, s2v(L->top - nres + i));
        for (int i = nres; i < wanted; i++)
          setnilvalue(s2v(res + i));
        L->top = res + wanted;
      }
      return;
  }
}

/*
** Post-call function (handles function return)
*/
AQL_API int aqlD_poscall(aql_State *L, CallInfo *ci, int nres) {
  int wanted = ci->nresults;
  
  printf_debug("[DEBUG] aqlD_poscall: ci=%p, nres=%d, wanted=%d\n", (void*)ci, nres, wanted);
  
  /* Move results to proper place */
  moveresults(L, ci->func, nres, wanted);
  
  /* Check if this is the base call (main function) */
  if (ci->previous == NULL) {
    printf_debug("[DEBUG] aqlD_poscall: base call, returning 1 (exit VM)\n");
    return 1;  /* Exit VM execution */
  }
  
  /* Restore previous CallInfo */
  L->ci = ci->previous;
  printf_debug("[DEBUG] aqlD_poscall: restored previous CallInfo, returning 0 (continue)\n");
  
  /* Continue execution in caller */
  return 0;
}

/*
** Prepare a tail call
*/
AQL_API int aqlD_pretailcall(aql_State *L, CallInfo *ci, StkId func, int narg1, int delta) {
  /* Tail call optimization implementation (based on Lua's luaD_pretailcall) */
  printf_debug("[DEBUG] aqlD_pretailcall: func=%p, narg1=%d, delta=%d\n", 
               (void*)func, narg1, delta);
  
  /* Check if it's a Lua function */
  if (ttisclosure(s2v(func))) {
    LClosure *cl = clLvalue(s2v(func));
    Proto *p = cl->p;
    int fsize = p->maxstacksize;  /* frame size */
    int nfixparams = p->numparams;
    
    printf_debug("[DEBUG] pretailcall: Lua function, fsize=%d, nfixparams=%d, narg1=%d\n", 
                 fsize, nfixparams, narg1);
    
    /* Move down function and arguments (like Lua) */
    ci->func -= delta;  /* restore 'func' (if vararg) */
    for (int i = 0; i < narg1; i++) {  /* move down function and arguments */
      setobj2s(L, ci->func + i, s2v(func + i));
      printf_debug("[DEBUG] pretailcall: moved func+arg %d from %p to %p\n", 
                   i, (void*)(func + i), (void*)(ci->func + i));
    }
    func = ci->func;  /* moved-down function */
    
    /* Complete missing arguments with nil (like Lua) */
    for (; narg1 <= nfixparams; narg1++) {
      setnilvalue(s2v(func + narg1));
      printf_debug("[DEBUG] pretailcall: set nil for missing arg %d\n", narg1);
    }
    
    /* Update call info for new function */
    ci->top = func + 1 + fsize;  /* top for new function */
    ci->u.l.savedpc = p->code;  /* starting point */
    L->top = func + narg1;  /* set top */
    
    printf_debug("[DEBUG] pretailcall: tail call setup complete, L->top=%p\n", (void*)L->top);
    return -1;  /* Lua function, continue execution */
  } else {
    /* C function - not optimized for now */
    printf_debug("[DEBUG] pretailcall: C function, no optimization\n");
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
  
  if (ttisLclosure(f)) {
    /* AQL function call - set up proper CallInfo */
    LClosure *cl = clLvalue(f);
    Proto *p = cl->p;
    int n = cast_int(L->top - func) - 1;  /* number of arguments */
    
    printf_debug("[DEBUG] aqlD_precall: AQL function, proto=%p, code=%p, nargs=%d, numparams=%d\n", 
                 (void*)p, (void*)p->code, n, p->numparams);
    
    /* Check stack overflow and handle gracefully */
    if (L->top + p->maxstacksize > L->stack_last) {
      printf_debug("[DEBUG] aqlD_precall: stack overflow detected at depth %d\n", L->nCcalls);
      
      /* TEMPORARY FIX: Avoid stack reallocation due to memory corruption bug */
      /* Instead, limit recursion depth to prevent crashes */
      if (L->nCcalls >= 8) {  /* Conservative limit */
        printf_debug("[DEBUG] aqlD_precall: recursion depth limit reached (%d), stopping\n", L->nCcalls);
        aqlG_runerror(L, "recursion depth limit exceeded (max 8 levels)");
        return NULL;
      }
      
      printf_debug("[DEBUG] aqlD_precall: attempting stack growth\n");
      int needed = cast_int((L->top + p->maxstacksize) - L->stack_last) + EXTRA_STACK;
      if (!aqlD_growstack(L, needed, 0)) {
        printf_debug("[DEBUG] aqlD_precall: failed to grow stack, aborting\n");
        aqlG_runerror(L, "stack overflow");
        return NULL;
      }
      printf_debug("[DEBUG] aqlD_precall: stack grown successfully\n");
    }
    
    /* Adjust arguments to match parameters */
    for (int i = n; i < p->numparams; i++) {
      setnilvalue(s2v(L->top++));  /* fill missing parameters with nil */
    }
    
    /* CRITICAL: Copy arguments to the correct positions for the function */
    /* Arguments are currently at func+1, func+2, ... func+n */
    /* They need to stay there because base will be set to func+1 */
    printf_debug("[DEBUG] aqlD_precall: arguments already in correct position at func+1 to func+%d\n", n);
    
    /* Create new CallInfo for the function */
    CallInfo *ci = aqlE_extendCI(L);
    ci->func = func;
    ci->u.l.savedpc = p->code;  /* Start at beginning of function code */
    ci->callstatus = 0;  /* AQL function */
    ci->top = func + 1 + p->maxstacksize;  /* Set stack top */
    ci->nresults = nResults;  /* Set expected results */
    ci->previous = L->ci;  /* Set previous CallInfo for return */
    
    /* CRITICAL: Set the base pointer for the function */
    /* In AQL/Lua, function parameters start at func+1 */
    /* This makes register 0 in the function point to the first parameter */
    
    /* Adjust L->ci to point to new CallInfo */
    L->ci = ci;
    
    printf_debug("[DEBUG] aqlD_precall: AQL function CallInfo set up, func=%p, base will be %p\n", 
                 (void*)func, (void*)(func + 1));
    return ci;  /* Return new CallInfo for AQL function */
  } else if (ttisCclosure(f)) {
    /* C function call */
    CClosure *ccl = clCvalue(f);
    int n = cast_int(L->top - func) - 1;  /* number of arguments */
    
    /* Call C function */
    int nres = ccl->f(L);
    
    /* Adjust results */
    if (nResults == AQL_MULTRET) {
      nResults = nres;
    } else {
      /* Adjust to expected number of results */
      while (nres > nResults) {
        L->top--;
        nres--;
      }
      while (nres < nResults) {
        setnilvalue(s2v(L->top++));
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
    /* Push compiled function onto stack */
    setclLvalue2s(L, L->top, cl);
    L->top++;
    
    /* Set up global environment as first upvalue (like Lua's _ENV) */
    if (cl->nupvalues >= 1) {
      /* First, ensure upvalues are initialized */
      if (cl->upvals[0] == NULL) {
        aqlF_initupvals(L, cl);
      }
      
      /* Get global dict directly from global state */
      const TValue *globals = &G(L)->l_globals;
      if (ttisdict(globals)) {
        /* Set global dict as first upvalue */
        setobj(L, cl->upvals[0]->v.p, globals);
      } else {
        /* Set nil for now, will be created on first global variable access */
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
  printf_debug("[DEBUG] f_execute: entering\n");
  
  struct ExecuteS *e = cast(struct ExecuteS *, ud);
  
  if (!L) {
    printf_debug("[ERROR] f_execute: L is NULL\n");
    aqlD_throw(L, AQL_ERRRUN);
  }
  
  if (!L->ci) {
    printf_debug("[ERROR] f_execute: L->ci is NULL\n");
    aqlD_throw(L, AQL_ERRRUN);
  }
  
  if (!L->ci->func) {
    printf_debug("[ERROR] f_execute: L->ci->func is NULL\n");
    aqlD_throw(L, AQL_ERRRUN);
  }
  
  if (L->top <= L->ci->func) {
    printf_debug("[ERROR] f_execute: L->top <= L->ci->func\n");
    aqlD_throw(L, AQL_ERRRUN);
  }
  
  /* Check that we have a function on the stack */
  if (L->top <= L->ci->func + 1) {
    aqlD_throw(L, AQL_ERRRUN);
  }
  
  TValue *func_val = s2v(L->top - 1);
  if (!ttisclosure(func_val)) {
    aqlD_throw(L, AQL_ERRRUN);
  }
  
  /* Create a new call frame for the function */
  CallInfo *ci = L->ci;
  LClosure *cl = clLvalue(func_val);
  
  /* Initialize _ENV upvalue with global dictionary (like Lua does) */
  printf_debug("[DEBUG] f_execute: cl->nupvalues=%d\n", cl->nupvalues);
  printf_debug("[DEBUG] f_execute: TEMPORARILY SKIPPING _ENV initialization to isolate crash\n");
  
  /* Set up the call frame */
  printf_debug("[DEBUG] f_execute: setting up call frame\n");
  ci->func = L->top - 1;  /* Function is at top - 1 */
  ci->u.l.savedpc = cl->p->code;  /* Start at beginning of code */
  printf_debug("[DEBUG] f_execute: call frame set up, func=%p, savedpc=%p\n", (void*)ci->func, (void*)ci->u.l.savedpc);
  
  /* Adjust stack for function call */
  printf_debug("[DEBUG] f_execute: adjusting stack for function call\n");
  L->top = ci->func + 1 + e->nargs;  /* func + args */
  
  /* Call the virtual machine */
  printf_debug("[DEBUG] f_execute: calling aqlV_execute\n");
  int status = aqlV_execute(L, ci);
  printf_debug("[DEBUG] f_execute: aqlV_execute returned status=%d\n", status);
  if (status != 1) {  /* execution failed? */
    aqlD_throw(L, AQL_ERRRUN);
  }
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
