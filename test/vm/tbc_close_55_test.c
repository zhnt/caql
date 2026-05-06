#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/aql.h"
#include "../../src/ado.h"
#include "../../src/afunc.h"
#include "../../src/amem.h"
#include "../../src/aobject.h"
#include "../../src/aopcodes.h"
#include "../../src/astring.h"
#include "../../src/astate.h"
#include "../../src/atable.h"
#include "../../src/avm.h"

extern int aqlD_closeprotected(aql_State *L, ptrdiff_t level, int status);

static void *test_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

static Table *close_obj1 = NULL;
static Table *close_obj2 = NULL;
static int close_count = 0;
static int close_order[4];
static int close_saw_error = 0;

static int assert_int(const char *name, int actual, int expected) {
  if (actual != expected) {
    fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
    return 0;
  }
  return 1;
}

static int assert_ptr(const char *name, const void *actual, const void *expected) {
  if (actual != expected) {
    fprintf(stderr, "%s: expected %p, got %p\n", name, expected, actual);
    return 0;
  }
  return 1;
}

static int close_recorder(aql_State *L) {
  TValue *last = s2v(L->top.p - 1);
  TValue *obj = last;

  if (ttisstring(last)) {
    if (strcmp(svalue(last), "boom") == 0)
      close_saw_error = 1;
    obj = s2v(L->top.p - 2);
  }

  if (ttistable(obj)) {
    Table *t = hvalue(obj);
    if (t == close_obj1)
      close_order[close_count++] = 1;
    else if (t == close_obj2)
      close_order[close_count++] = 2;
    else
      close_order[close_count++] = 99;
  }
  return 0;
}

static int close_error_on_obj2(aql_State *L) {
  TValue *last = s2v(L->top.p - 1);
  TValue *obj = ttisstring(last) ? s2v(L->top.p - 2) : last;
  if (ttistable(obj)) {
    Table *t = hvalue(obj);
    if (t == close_obj1)
      close_order[close_count++] = 1;
    else if (t == close_obj2)
      close_order[close_count++] = 2;
    if (t == close_obj2)
      aqlG_runerror(L, "close boom");
  }
  return 0;
}

static void set_tm(aql_State *L, Table *mt, TMS event, aql_CFunction fn) {
  TValue key;
  TValue value;
  setsvalue(L, &key, G(L)->tmname[event]);
  setfvalue(&value, fn);
  aqlH_set(L, mt, &key, &value);
  mt->flags = 0;
}

static Table *new_closable_table(aql_State *L, aql_CFunction fn) {
  Table *mt = aqlH_new(L);
  Table *t = aqlH_new(L);
  set_tm(L, mt, TM_CLOSE, fn);
  t->metatable = mt;
  return t;
}

static void mark_nonclosable(aql_State *L, void *ud) {
  StkId slot = cast(StkId, ud);
  aqlF_newtbcupval(L, slot);
}

typedef void (*SetupRegisters)(aql_State *L, StkId base);

typedef struct VmCase {
  Instruction *code;
  int sizecode;
  int maxstacksize;
  SetupRegisters setup;
} VmCase;

static void run_vm_case(aql_State *L, void *ud) {
  VmCase *vc = cast(VmCase *, ud);
  StkId oldtop = L->top.p;
  CallInfo *oldci = L->ci;
  Proto *p = aqlF_newproto(L);
  LClosure *cl = aqlF_newLclosure(L, 0);

  p->code = aqlM_newvector(L, vc->sizecode, Instruction);
  p->sizecode = vc->sizecode;
  p->maxstacksize = cast_byte(vc->maxstacksize);
  for (int i = 0; i < vc->sizecode; i++)
    p->code[i] = vc->code[i];
  cl->p = p;

  setclLvalue2s(L, oldtop, cl);
  L->top.p = oldtop + 1 + vc->maxstacksize;
  if (vc->setup != NULL)
    vc->setup(L, oldtop + 1);

  L->ci = &L->base_ci;
  L->ci->func.p = oldtop;
  L->ci->top.p = oldtop + 1 + vc->maxstacksize + AQL_MINSTACK;
  L->ci->u.l.savedpc = p->code;
  L->ci->u.l.trap = 0;
  L->ci->callstatus = 0;
  L->ci->nresults = 0;

  aqlV_execute(L, L->ci);
  L->ci = oldci;
  L->top.p = oldtop;
}

static int run_vm_expect_ok(aql_State *L, const char *name, VmCase *vc) {
  int status = aqlD_rawrunprotected(L, run_vm_case, vc);
  if (status != AQL_OK) {
    fprintf(stderr, "%s: expected AQL_OK, got %d\n", name, status);
    return 0;
  }
  return 1;
}

static int run_vm_expect_runerror(aql_State *L, const char *name, VmCase *vc) {
  int status = aqlD_rawrunprotected(L, run_vm_case, vc);
  if (status != AQL_ERRRUN) {
    fprintf(stderr, "%s: expected AQL_ERRRUN, got %d\n", name, status);
    return 0;
  }
  return 1;
}

static void setup_tbc_nil(aql_State *L, StkId base) {
  setnilvalue(s2v(base));
  (void)L;
}

static void setup_tbc_nonclosable(aql_State *L, StkId base) {
  setivalue(s2v(base), 17);
  (void)L;
}

static void setup_two_closable(aql_State *L, StkId base) {
  close_obj1 = new_closable_table(L, close_recorder);
  close_obj2 = new_closable_table(L, close_recorder);
  sethvalue(L, s2v(base), close_obj1);
  sethvalue(L, s2v(base + 1), close_obj2);
}

static void setup_tforprep_closing_slot(aql_State *L, StkId base) {
  close_obj1 = new_closable_table(L, close_recorder);
  setnilvalue(s2v(base + 2));
  sethvalue(L, s2v(base + 3), close_obj1);
}

int main(void) {
  int ok = 1;
  aql_State *L = aql_newstate(test_alloc, NULL);
  if (L == NULL) {
    fprintf(stderr, "failed to create AQL state\n");
    return 1;
  }

  StkId base = L->top.p;
  ok &= assert_ptr("initial tbclist starts at stack base",
                   L->tbclist.p, L->stack.p);

  setnilvalue(s2v(base));
  L->top.p = base + 1;
  aqlF_newtbcupval(L, base);
  ok &= assert_ptr("nil TBC is a no-op", L->tbclist.p, L->stack.p);

  setbfvalue(s2v(base));
  L->top.p = base + 1;
  aqlF_newtbcupval(L, base);
  ok &= assert_ptr("false TBC is a no-op", L->tbclist.p, L->stack.p);

  setivalue(s2v(base), 17);
  L->top.p = base + 1;
  ok &= assert_int("non-closable TBC raises runtime error",
                   aqlD_rawrunprotected(L, mark_nonclosable, base), AQL_ERRRUN);
  L->top.p = base;

  close_obj1 = new_closable_table(L, close_recorder);
  close_obj2 = new_closable_table(L, close_recorder);
  sethvalue(L, s2v(base), close_obj1);
  sethvalue(L, s2v(base + 1), close_obj2);
  L->top.p = base + 2;
  aqlF_newtbcupval(L, base);
  aqlF_newtbcupval(L, base + 1);
  aqlF_close(L, base, AQL_OK, 0);
  ok &= assert_int("__close called for both values", close_count, 2);
  ok &= assert_int("__close order is reverse declaration order", close_order[0], 2);
  ok &= assert_int("__close final order entry", close_order[1], 1);
  ok &= assert_ptr("tbclist restored after close", L->tbclist.p, L->stack.p);
  L->top.p = base;

  close_count = 0;
  close_saw_error = 0;
  close_obj1 = new_closable_table(L, close_recorder);
  sethvalue(L, s2v(base), close_obj1);
  L->top.p = base + 1;
  aqlF_newtbcupval(L, base);
  setsvalue2s(L, L->top.p, aqlStr_newlstr(L, "boom", 4));
  L->top.p++;
  aqlF_close(L, base, AQL_ERRRUN, 0);
  ok &= assert_int("__close called on error path", close_count, 1);
  ok &= assert_int("__close receives error object", close_saw_error, 1);

  Instruction tbc_nil_code[] = {
    CREATE_ABC(OP_TBC, 0, 0, 0),
    CREATE_ABC(OP_RETURN0, 0, 0, 0)
  };
  VmCase tbc_nil_case = { tbc_nil_code, 2, 2, setup_tbc_nil };
  close_count = 0;
  ok &= run_vm_expect_ok(L, "OP_TBC nil no-op", &tbc_nil_case);
  ok &= assert_int("OP_TBC nil does not close", close_count, 0);

  Instruction tbc_error_code[] = {
    CREATE_ABC(OP_TBC, 0, 0, 0),
    CREATE_ABC(OP_RETURN0, 0, 0, 0)
  };
  VmCase tbc_error_case = { tbc_error_code, 2, 2, setup_tbc_nonclosable };
  ok &= run_vm_expect_runerror(L, "OP_TBC non-closable", &tbc_error_case);

  Instruction close_code[] = {
    CREATE_ABC(OP_TBC, 0, 0, 0),
    CREATE_ABC(OP_TBC, 1, 0, 0),
    CREATE_ABC(OP_CLOSE, 0, 0, 0),
    CREATE_ABC(OP_RETURN0, 0, 0, 0)
  };
  VmCase close_case = { close_code, 4, 4, setup_two_closable };
  close_count = 0;
  ok &= run_vm_expect_ok(L, "OP_CLOSE closes marked values", &close_case);
  ok &= assert_int("OP_CLOSE closes both values", close_count, 2);
  ok &= assert_int("OP_CLOSE preserves reverse order", close_order[0], 2);
  ok &= assert_int("OP_CLOSE final order entry", close_order[1], 1);

  Instruction tforprep_code[] = {
    CREATE_ABx(OP_TFORPREP, 0, 0),
    CREATE_ABC(OP_CLOSE, 2, 0, 0),
    CREATE_ABC(OP_RETURN0, 0, 0, 0)
  };
  VmCase tforprep_case = { tforprep_code, 3, 6, setup_tforprep_closing_slot };
  close_count = 0;
  ok &= run_vm_expect_ok(L, "OP_TFORPREP marks closing slot", &tforprep_case);
  ok &= assert_int("generic-for exit closes TFORPREP slot", close_count, 1);
  ok &= assert_int("TFORPREP close target is swapped closing var", close_order[0], 1);

  close_count = 0;
  setup_two_closable(L, base);
  L->top.p = base + 2;
  aqlF_newtbcupval(L, base);
  aqlF_newtbcupval(L, base + 1);
  ok &= assert_int("aqlD_closeprotected returns original status",
                   aqlD_closeprotected(L, savestack(L, base), AQL_OK), AQL_OK);
  ok &= assert_int("aqlD_closeprotected closes marked values", close_count, 2);
  ok &= assert_int("aqlD_closeprotected reverse order", close_order[0], 2);
  ok &= assert_int("aqlD_closeprotected final order entry", close_order[1], 1);
  L->top.p = base;

  close_count = 0;
  close_obj1 = new_closable_table(L, close_error_on_obj2);
  close_obj2 = new_closable_table(L, close_error_on_obj2);
  sethvalue(L, s2v(base), close_obj1);
  sethvalue(L, s2v(base + 1), close_obj2);
  L->top.p = base + 2;
  aqlF_newtbcupval(L, base);
  aqlF_newtbcupval(L, base + 1);
  ok &= assert_int("aqlD_closeprotected returns close error",
                   aqlD_closeprotected(L, savestack(L, base), AQL_OK), AQL_ERRRUN);
  ok &= assert_int("aqlD_closeprotected continues after close error", close_count, 2);
  ok &= assert_int("aqlD_closeprotected error path first close", close_order[0], 2);
  ok &= assert_int("aqlD_closeprotected error path second close", close_order[1], 1);

  aql_close(L);
  if (ok) {
    puts("tbc_close_55_test passed");
    return 0;
  }
  return 1;
}
