/*
** $Id: afunc.c $
** Auxiliary functions to manipulate prototypes and closures
** Based on Lua's lfunc.c
*/

#define afunc_c
#define AQL_CORE

#include "aql.h"

#include <stddef.h>

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
    /* TODO: implement proper GC barrier for UpVal */
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
  aql_assert(isintwups(L) || L->openupval == NULL);
  while ((p = *pp) != NULL && uplevel(p) >= level) {  /* search for it */
    aql_assert(!isdead(G(L), p));
    if (uplevel(p) == level)  /* corresponding upvalue? */
      return p;  /* return it */
    pp = &p->u.open.next;
  }
  /* not found: create a new upvalue after 'pp' */
  return newupval(L, level, pp);
}


/*
** Call closing method for object 'obj' with error message 'err'. The
** boolean 'yy' controls whether the call is yieldable.
** (This function assumes EXTRA_STACK.)
*/
static void callclosemethod (aql_State *L, TValue *obj, TValue *err, int yy) {
  StkId top = L->top;
  /* TODO: implement metamethod support */
  /* const TValue *tm = aqlT_gettmbyobj(L, obj, TM_CLOSE); */
  /* setobj2s(L, top, tm); */
  /* setobj2s(L, top + 1, obj); */
  /* setobj2s(L, top + 2, err); */
  /* L->top = top + 3; */
  /* if (yy)
    aqlD_call(L, top, 0);
  else
    aqlD_callnoyield(L, top, 0); */
  (void)L; (void)obj; (void)err; (void)yy; (void)top; /* avoid warnings */
}


/*
** Check whether object at given level has a close metamethod and raise
** an error if not.
*/
static void checkclosemth (aql_State *L, StkId level) {
  /* TODO: implement metamethod support */
  /* const TValue *tm = aqlT_gettmbyobj(L, s2v(level), TM_CLOSE); */
  /* if (ttisnil(tm)) { */
    /* int idx = cast_int(level - L->ci->func); */
    /* const char *vname = aqlG_findlocal(L, L->ci, idx, NULL); */
    /* if (vname == NULL) vname = "?"; */
    /* aqlG_runerror(L, "variable '%s' got a non-closable value", vname); */
  /* } */
  (void)L; (void)level; /* avoid warnings */
}


/*
** Prepare and call a closing method.
** If status is CLOSEKTOP, the call to the closing method will be pushed
** at the top of the stack. Otherwise, values can be pushed right after
** the 'level' of the upvalue being closed, as everything after that
** won't be used again.
*/
static void prepcallclosemth (aql_State *L, StkId level, int status, int yy) {
  /* TODO: implement close method support */
  /* TValue *uv = s2v(level); */
  /* TValue *errobj; */
  /* if (status == CLOSEKTOP)
    errobj = &G(L)->nilvalue;
  else {
    errobj = s2v(level + 1);
    aqlD_seterrorobj(L, status, level + 1);
  }
  callclosemethod(L, uv, errobj, yy); */
  (void)L; (void)level; (void)status; (void)yy; /* avoid warnings */
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
  /* TODO: implement to-be-closed variables support */
  /* aql_assert(level > L->tbclist.p);
  if (l_isfalse(s2v(level)))
    return;
  checkclosemth(L, level);
  while (cast_uint(level - L->tbclist.p) > MAXDELTA) {
    L->tbclist.p += MAXDELTA;
    L->tbclist.p->tbclist.delta = 0;
  }
  level->tbclist.delta = cast(unsigned short, level - L->tbclist.p);
  L->tbclist.p = level; */
  (void)L; (void)level; /* avoid warnings */
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
  while ((uv = L->openupval) != NULL && (upl = uplevel(uv)) >= level) {
    TValue *slot = &uv->u.value;  /* new position for value */
    aql_assert(uplevel(uv) < L->top.p);
    aqlF_unlinkupval(uv);  /* remove upvalue from 'openupval' list */
    setobj(L, slot, uv->v.p);  /* move value to upvalue slot */
    uv->v.p = slot;  /* now current value lives here */
    if (!iswhite(uv)) {  /* neither white nor dead? */
      nw2black(uv);  /* closed upvalues cannot be gray */
      aqlC_barrier_(L, obj2gco(uv), obj2gco(slot));
    }
  }
}


/*
** Remove first element from the tbclist plus its dummy nodes.
*/
static void poptbclist (aql_State *L) {
  /* TODO: implement to-be-closed variables support */
  /* StkId tbc = L->tbclist.p;
  aql_assert(tbc->tbclist.delta > 0);
  tbc -= tbc->tbclist.delta;
  while (tbc > L->stack && tbc->tbclist.delta == 0)
    tbc -= MAXDELTA;
  L->tbclist.p = tbc; */
  (void)L; /* avoid warnings */
}


/*
** Close all upvalues and to-be-closed variables up to the given stack
** level. Return restored 'level'.
*/
StkId aqlF_close (aql_State *L, StkId level, int status, int yy) {
  /* TODO: implement to-be-closed variables support */
  ptrdiff_t levelrel = savestack(L, level);
  aqlF_closeupval(L, level);  /* first, close the upvalues */
  /* while (L->tbclist >= level) {
    StkId tbc = L->tbclist;
    poptbclist(L);
    prepcallclosemth(L, tbc, status, yy);
    level = restorestack(L, levelrel);
  } */
  (void)status; (void)yy; (void)levelrel; /* avoid warnings */
  return level;
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