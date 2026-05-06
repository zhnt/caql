#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/aql.h"
#include "../../src/ado.h"
#include "../../src/afunc.h"
#include "../../src/amem.h"
#include "../../src/aobject.h"
#include "../../src/aopcodes.h"
#include "../../src/astate.h"
#include "../../src/avm.h"

static void *test_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

static int after_error;

static void raise_runerror(aql_State *L, void *ud) {
  (void)ud;
  aqlG_runerror(L, "runtime %s", "failure");
  after_error = 1;
}

static void raise_typeerror(aql_State *L, void *ud) {
  TValue value;
  (void)ud;
  setnilvalue(&value);
  aqlG_typeerror(L, &value, "index");
  after_error = 1;
}

static void raise_string_typeerror(aql_State *L, void *ud) {
  TValue value;
  (void)ud;
  setsvalue(L, &value, aqlStr_newlstr(L, "abc", 3));
  aqlG_typeerror(L, &value, "index");
  after_error = 1;
}

static void raise_ordererror(aql_State *L, void *ud) {
  TValue lhs;
  TValue rhs;
  (void)ud;
  setnilvalue(&lhs);
  setbtvalue(&rhs);
  (void)aqlG_ordererror(L, &lhs, &rhs);
  after_error = 1;
}

static void raise_lua_frame_error(aql_State *L, void *ud) {
  (void)ud;
  aqlG_runerror(L, "framed failure");
  after_error = 1;
}

static void run_unknown_opcode(aql_State *L, void *ud) {
  StkId oldtop = L->top.p;
  CallInfo *oldci = L->ci;
  Proto *p = aqlF_newproto(L);
  LClosure *cl = aqlF_newLclosure(L, 0);
  (void)ud;

  p->source = aqlStr_newlstr(L, "@unknown_opcode.aql", 19);
  p->code = aqlM_newvector(L, 1, Instruction);
  p->sizecode = 1;
  p->maxstacksize = 2;
  p->code[0] = cast(Instruction, 120);
  cl->p = p;

  setclLvalue2s(L, oldtop, cl);
  L->top.p = oldtop + 1;
  L->ci = &L->base_ci;
  L->ci->func.p = oldtop;
  L->ci->top.p = oldtop + 3;
  L->ci->u.l.savedpc = p->code;
  L->ci->u.l.trap = 0;
  L->ci->callstatus = 0;
  L->ci->nresults = 0;
  aqlV_execute(L, L->ci);
  L->ci = oldci;
  after_error = 1;
}

static int expect_runtime_message(aql_State *L, const char *name, Pfunc fn,
                                  const char *message_part) {
  int status;
  StkId oldtop = L->top.p;
  after_error = 0;
  status = aqlD_rawrunprotected(L, fn, NULL);
  if (status != AQL_ERRRUN || after_error ||
      L->top.p != oldtop + 1 || !ttisstring(s2v(oldtop)) ||
      strstr(svalue(s2v(oldtop)), message_part) == NULL) {
    fprintf(stderr, "%s: expected runtime message containing '%s'\n",
            name, message_part);
    if (L->top.p == oldtop + 1 && ttisstring(s2v(oldtop)))
      fprintf(stderr, "%s: actual message '%s'\n", name, svalue(s2v(oldtop)));
    return 0;
  }
  L->top.p = oldtop;
  return 1;
}

static int expect_pcall_message(aql_State *L, const char *name, Pfunc fn,
                                const char *message_part) {
  int status;
  StkId oldtop = L->top.p;
  ptrdiff_t oldtop_offset = savestack(L, oldtop);
  after_error = 0;
  status = aqlD_pcall(L, fn, NULL, oldtop_offset, 0);
  if (status != AQL_ERRRUN || after_error ||
      L->top.p != oldtop + 1 || !ttisstring(s2v(oldtop)) ||
      strstr(svalue(s2v(oldtop)), message_part) == NULL) {
    fprintf(stderr, "%s: expected pcall message containing '%s'\n",
            name, message_part);
    if (L->top.p == oldtop + 1 && ttisstring(s2v(oldtop)))
      fprintf(stderr, "%s: actual message '%s'\n", name, svalue(s2v(oldtop)));
    return 0;
  }
  L->top.p = oldtop;
  return 1;
}

static int expect_lua_frame_source_line(aql_State *L) {
  int status;
  StkId oldtop = L->top.p;
  ptrdiff_t oldtop_offset = savestack(L, oldtop);
  CallInfo *oldci = L->ci;
  Proto *p = aqlF_newproto(L);
  LClosure *cl = aqlF_newLclosure(L, 0);
  p->source = aqlStr_newlstr(L, "@runtime_error_55.aql", 21);
  p->code = aqlM_newvector(L, 1, Instruction);
  p->sizecode = 1;
  p->code[0] = CREATE_ABC(OP_RETURN0, 0, 0, 0);
  p->lineinfo = aqlM_newvector(L, 1, aql_byte);
  p->sizelineinfo = 1;
  p->lineinfo[0] = 42;
  cl->p = p;

  setclLvalue2s(L, oldtop, cl);
  L->top.p = oldtop + 1;
  L->ci = &L->base_ci;
  L->ci->func.p = oldtop;
  L->ci->u.l.savedpc = p->code + 1;
  L->ci->callstatus = 0;

  after_error = 0;
  status = aqlD_pcall(L, raise_lua_frame_error, NULL, oldtop_offset, 0);
  L->ci = oldci;
  if (status != AQL_ERRRUN || after_error ||
      L->top.p != oldtop + 1 || !ttisstring(s2v(oldtop)) ||
      strstr(svalue(s2v(oldtop)), "runtime_error_55.aql:42: framed failure") == NULL) {
    fprintf(stderr, "lua-frame error: expected source:line message\n");
    if (L->top.p == oldtop + 1 && ttisstring(s2v(oldtop)))
      fprintf(stderr, "lua-frame error: actual message '%s'\n", svalue(s2v(oldtop)));
    return 0;
  }
  L->top.p = oldtop;
  return 1;
}

static int expect_precreated_memerrmsg(aql_State *L) {
  TString *msg = G(L)->memerrmsg;
  if (msg == NULL || strcmp(getstr(msg), "not enough memory") != 0) {
    fprintf(stderr, "memerrmsg: expected precreated 'not enough memory'\n");
    return 0;
  }
  return 1;
}

static int expect_error_object(aql_State *L, const char *name, int errcode,
                               const char *message_part) {
  StkId oldtop = L->top.p;
  aqlD_seterrorobj(L, errcode, oldtop);
  if (L->top.p != oldtop + 1 || !ttisstring(s2v(oldtop)) ||
      strstr(svalue(s2v(oldtop)), message_part) == NULL) {
    fprintf(stderr, "%s: expected error object containing '%s'\n",
            name, message_part);
    if (L->top.p == oldtop + 1 && ttisstring(s2v(oldtop)))
      fprintf(stderr, "%s: actual message '%s'\n", name, svalue(s2v(oldtop)));
    return 0;
  }
  if (errcode == AQL_ERRMEM && tsvalue(s2v(oldtop)) != G(L)->memerrmsg) {
    fprintf(stderr, "%s: expected precreated memory error object\n", name);
    return 0;
  }
  L->top.p = oldtop;
  return 1;
}

int main(void) {
  int ok = 1;
  aql_State *L = aql_newstate(test_alloc, NULL);
  if (L == NULL) {
    fprintf(stderr, "failed to create AQL state\n");
    return 1;
  }

  ok &= expect_runtime_message(L, "aqlG_runerror", raise_runerror,
                               "runtime failure");
  ok &= expect_runtime_message(L, "aqlG_typeerror", raise_typeerror,
                               "attempt to index a nil value");
  ok &= expect_runtime_message(L, "aqlG_typeerror string", raise_string_typeerror,
                               "attempt to index a string value");
  ok &= expect_runtime_message(L, "aqlG_ordererror", raise_ordererror,
                               "attempt to compare nil with boolean");
  ok &= expect_pcall_message(L, "aqlD_pcall", raise_runerror,
                             "runtime failure");
  ok &= expect_pcall_message(L, "unknown opcode default", run_unknown_opcode,
                             "unknown opcode 120");
  ok &= expect_lua_frame_source_line(L);
  ok &= expect_precreated_memerrmsg(L);
  ok &= expect_error_object(L, "AQL_ERRMEM", AQL_ERRMEM, "not enough memory");

  aql_close(L);
  if (ok) {
    puts("runtime_error_55_test passed");
    return 0;
  }
  return 1;
}
