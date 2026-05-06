/*
** $Id: afunc.c $
** Auxiliary functions to manipulate prototypes and closures
** Based on Lua's lfunc.c
*/

#define afunc_c
#define AQL_CORE

#include "aql.h"

#include <stddef.h>
#include <stdio.h>

#include "adebug.h"
#include "ado.h"
#include "afunc.h"
#include "agc.h"
#include "amem.h"
#include "aobject.h"
#include "astate.h"



CClosure *aqlF_newCclosure (aql_State *L, int nupvals) {
  GCObject *o = aqlC_newobj(L, AQL_VCCL, sizeCclosure(nupvals));
  CClosure *c = gco2ccl(o);
  c->nupvalues = cast_byte(nupvals);
  return c;
}


LClosure *aqlF_newLclosure (aql_State *L, int nupvals) {
  GCObject *o = aqlC_newobj(L, AQL_VLCL, sizeLclosure(nupvals));
  LClosure *c = gco2lcl(o);
  c->p = NULL;
  c->nupvalues = cast_byte(nupvals);
  while (nupvals--) c->upvals[nupvals] = NULL;
  return c;
}


/*
** fill a closure with new closed upvalues
*/
void aqlF_initupvals (aql_State *L, LClosure *cl) {
  int i;
  for (i = 0; i < cl->nupvalues; i++) {
    GCObject *o = aqlC_newobj(L, AQL_VUPVAL, sizeof(UpVal));
    UpVal *uv = gco2upv(o);
    uv->v.p = &uv->u.value;  /* make it closed */
    setnilvalue(uv->v.p);
    cl->upvals[i] = uv;
    /* TODO: implement proper GC barrier for UpVal when agc is ready */
    /* aqlC_objbarrier(L, cl, uv); */
    (void)L; /* avoid warning */
  }
}


/*
** Create a new upvalue at the given level, and link it to the list of
** open upvalues of 'L' after entry 'prev'.
**/
static UpVal *newupval (aql_State *L, StkId level, UpVal **prev) {
  GCObject *o = aqlC_newobj(L, AQL_VUPVAL, sizeof(UpVal));
  UpVal *uv = gco2upv(o);
  UpVal *next = *prev;
  uv->v.p = s2v(level);  /* current value lives in the stack */
  uv->u.open.next = next;  /* link it to list of open upvalues */
  uv->u.open.previous = prev;
  if (next)
    next->u.open.previous = &uv->u.open.next;
  *prev = uv;
  if (!isintwups(L)) {  /* thread not in list of threads with upvalues? */
    L->twups = G(L)->twups;  /* link it to the list */
    G(L)->twups = L;
  }
  return uv;
}


/*
** Find and reuse, or create if it does not exist, an upvalue
** at the given level.
*/
UpVal *aqlF_findupval (aql_State *L, StkId level) {
  UpVal **pp = &L->openupval;
  UpVal *p;
  aql_debug("[DEBUG] aqlF_findupval: looking for level=%p, value_type=%d\n", 
               (void*)level, ttype(s2v(level)));
  if (ttisinteger(s2v(level))) {
    aql_debug("[DEBUG] aqlF_findupval: level points to integer %lld\n", 
                 (long long)ivalue(s2v(level)));
  }
  aql_assert(isintwups(L) || L->openupval == NULL);
  while ((p = *pp) != NULL && uplevel(p) >= level) {  /* search for it */
    aql_assert(!isdead(G(L), p));
    if (uplevel(p) == level) {  /* corresponding upvalue? */
      aql_debug("[DEBUG] aqlF_findupval: found existing upvalue %p\n", (void*)p);
      return p;  /* return it */
    }
    pp = &p->u.open.next;
  }
  /* not found: create a new upvalue after 'pp' */
  aql_debug("[DEBUG] aqlF_findupval: creating new upvalue for level=%p\n", (void*)level);
  UpVal *new_uv = newupval(L, level, pp);
  aql_debug("[DEBUG] aqlF_findupval: created upvalue %p pointing to %p\n", 
               (void*)new_uv, (void*)level);
  return new_uv;
}


/*
** Call closing method for object 'obj' with error message 'err'. The
** boolean 'yy' controls whether the call is yieldable.
** (This function assumes EXTRA_STACK.)
*/
static void callclosemethod (aql_State *L, TValue *obj, TValue *err, int yy) {
  StkId oldtop = L->top.p;
  StkId func = oldtop;
  const TValue *tm = aqlT_gettmbyobj(L, obj, TM_CLOSE);

  if (ttisnil(tm))
    return;

  setobj2s(L, func, tm);
  setobj2s(L, func + 1, obj);
  if (err != NULL) {
    setobj2s(L, func + 2, err);
    L->top.p = func + 3;
  }
  else {
    L->top.p = func + 2;
  }

  if (ttisCclosure(s2v(func))) {
    CClosure *ccl = clCvalue(s2v(func));
    cast_void(ccl->f(L));
  }
  else if (ttislcf(s2v(func))) {
    aql_CFunction cfn = fvalue(s2v(func));
    cast_void(cfn(L));
  }
  else if (yy) {
    aqlD_call(L, func, 0);
  }
  else {
    aqlD_callnoyield(L, func, 0);
  }
  L->top.p = oldtop;
}


/*
** Check whether object at given level has a close metamethod and raise
** an error if not.
*/
static void checkclosemth (aql_State *L, StkId level) {
  const TValue *tm = aqlT_gettmbyobj(L, s2v(level), TM_CLOSE);
  if (ttisnil(tm))
    aqlG_runerror(L, "variable got a non-closable value");
}


/*
** Prepare and call a closing method.
** If status is CLOSEKTOP, the call to the closing method will be pushed
** at the top of the stack. Otherwise, values can be pushed right after
** the 'level' of the upvalue being closed, as everything after that
** won't be used again.
*/
static void prepcallclosemth (aql_State *L, StkId level, int status, int yy) {
  TValue *uv = s2v(level);
  TValue *errobj;
  switch (status) {
    case AQL_OK:
      L->top.p = level + 1;
      /* FALLTHROUGH */
    case CLOSEKTOP:
      errobj = NULL;
      break;
    default:
      errobj = s2v(level + 1);
      aqlD_seterrorobj(L, status, level + 1);
      break;
  }
  callclosemethod(L, uv, errobj, yy);
}


/*
** Maximum value for deltas in 'tbclist', dependent on the type
** of delta. (This macro assumes that an 'L' is in scope where it
** is used.)
*/
#define MAXDELTA  \
	((256ul << ((sizeof(L->stack.p->tbclist.delta) - 1) * 8)) - 1)


/*
** Insert a variable in the list of to-be-closed variables.
*/
void aqlF_newtbcupval (aql_State *L, StkId level) {
  if (l_isfalse(s2v(level)))
    return;

  checkclosemth(L, level);
  while (cast_sizet(level - L->tbclist.p) > MAXDELTA) {
    L->tbclist.p += MAXDELTA;
    L->tbclist.p->tbclist.delta = 0;
  }
  level->tbclist.delta = cast(unsigned short, level - L->tbclist.p);
  L->tbclist.p = level;
}


void aqlF_unlinkupval (UpVal *uv) {
  aql_assert(upisopen(uv));
  *uv->u.open.previous = uv->u.open.next;
  if (uv->u.open.next)
    uv->u.open.next->u.open.previous = uv->u.open.previous;
}


/*
** Close all upvalues up to the given stack level.
*/
void aqlF_closeupval (aql_State *L, StkId level) {
  UpVal *uv;
  StkId upl;  /* stack index pointed by 'uv' */
  aql_debug("[DEBUG] aqlF_closeupval: closing upvalues >= level %p\n", (void*)level);
  int closed_count = 0;
  while ((uv = L->openupval) != NULL && (upl = uplevel(uv)) >= level) {
    TValue *slot = &uv->u.value;  /* new position for value */
    aql_debug("[DEBUG] aqlF_closeupval: closing upvalue %p, stack_pos=%p, value_type=%d\n", 
                 (void*)uv, (void*)upl, ttype(uv->v.p));
    if (ttisinteger(uv->v.p)) {
      aql_debug("[DEBUG] aqlF_closeupval: copying integer %lld from stack to upvalue\n", 
                   (long long)ivalue(uv->v.p));
    }
    aql_assert(uplevel(uv) < L->top.p);
    aqlF_unlinkupval(uv);  /* remove upvalue from 'openupval' list */
    setobj(L, slot, uv->v.p);  /* move value to upvalue slot */
    uv->v.p = slot;  /* now current value lives here */
    aql_debug("[DEBUG] aqlF_closeupval: upvalue %p now points to internal storage, value_type=%d\n", 
                 (void*)uv, ttype(slot));
    if (!iswhite(uv)) {  /* neither white nor dead? */
      nw2black(uv);  /* closed upvalues cannot be gray */
      /* TODO: implement GC barrier when agc is ready */
      /* aqlC_barrier(L, uv, slot); */
    }
    closed_count++;
  }
  aql_debug("[DEBUG] aqlF_closeupval: closed %d upvalues\n", closed_count);
}


/*
** Remove first element from the tbclist plus its dummy nodes.
*/
static void poptbclist (aql_State *L) {
  StkId tbc = L->tbclist.p;
  aql_assert(tbc->tbclist.delta > 0);
  tbc -= tbc->tbclist.delta;
  while (tbc > L->stack.p && tbc->tbclist.delta == 0)
    tbc -= MAXDELTA;
  L->tbclist.p = tbc;
}


/*
** Close all upvalues and to-be-closed variables up to the given stack
** level. Return restored 'level'.
*/
StkId aqlF_close (aql_State *L, StkId level, int status, int yy) {
  ptrdiff_t levelrel = savestack(L, level);
  aqlF_closeupval(L, level);  /* first, close the upvalues */
  while (L->tbclist.p >= level) {
    StkId tbc = L->tbclist.p;
    poptbclist(L);
    prepcallclosemth(L, tbc, status, yy);
    level = restorestack(L, levelrel);
  }
  return restorestack(L, levelrel);
}


Proto *aqlF_newproto (aql_State *L) {
  GCObject *o = aqlC_newobj(L, AQL_VPROTO, sizeof(Proto));
  Proto *f = gco2p(o);
  f->k = NULL;
  f->sizek = 0;
  f->p = NULL;
  f->sizep = 0;
  f->code = NULL;
  f->sizecode = 0;
  f->lineinfo = NULL;
  f->sizelineinfo = 0;
  f->abslineinfo = NULL;
  f->sizeabslineinfo = 0;
  f->upvalues = NULL;
  f->sizeupvalues = 0;
  f->numparams = 0;
  f->is_vararg = 0;
  f->maxstacksize = 0;
  f->locvars = NULL;
  f->sizelocvars = 0;
  f->linedefined = 0;
  f->lastlinedefined = 0;
  f->source = NULL;
  return f;
}


void aqlF_freeproto (aql_State *L, Proto *f) {
  aqlM_freearray(L, f->code, f->sizecode);
  aqlM_freearray(L, f->p, f->sizep);
  aqlM_freearray(L, f->k, f->sizek);
  aqlM_freearray(L, f->lineinfo, f->sizelineinfo);
  aqlM_freearray(L, f->abslineinfo, f->sizeabslineinfo);
  aqlM_freearray(L, f->locvars, f->sizelocvars);
  aqlM_freearray(L, f->upvalues, f->sizeupvalues);
  aqlM_free(L, f, sizeof(Proto));
}


/*
** Look for n-th local variable at line 'line' in function 'func'.
** Returns NULL if not found.
*/
const char *aqlF_getlocalname (const Proto *f, int local_number, int pc) {
  int i;
  for (i = 0; i<f->sizelocvars && f->locvars[i].startpc <= pc; i++) {
    if (pc < f->locvars[i].endpc) {  /* is variable active? */
      local_number--;
      if (local_number == 0)
        return getstr(f->locvars[i].varname);
    }
  }
  return NULL;  /* not found */
}
