#include <stdio.h>
#include <stdlib.h>

#include "../../src/aql.h"
#include "../../src/ado.h"
#include "../../src/aobject.h"
#include "../../src/astate.h"
#include "../../src/atable.h"

static TValue protected_lhs;
static TValue protected_rhs;
static TValue protected_result;
static TMS protected_event;

static void *test_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

static int add_returns_42(aql_State *L) {
  setivalue(s2v(L->top.p), 42);
  L->top.p++;
  return 1;
}

static void set_tm(aql_State *L, Table *mt, TMS event, aql_CFunction fn) {
  TValue key;
  TValue value;
  setsvalue(L, &key, G(L)->tmname[event]);
  setfvalue(&value, fn);
  aqlH_set(L, mt, &key, &value);
  mt->flags = 0;
}

static Table *new_table_with_mt(aql_State *L, Table *mt) {
  Table *t = aqlH_new(L);
  t->metatable = mt;
  return t;
}

static int assert_int(const char *name, int actual, int expected) {
  if (actual != expected) {
    fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
    return 0;
  }
  return 1;
}

static void run_trybin(aql_State *L, void *ud) {
  (void)ud;
  aqlT_trybinTM(L, &protected_lhs, &protected_rhs, L->top.p, protected_event);
}

int main(void) {
  int ok = 1;
  aql_State *L = aql_newstate(test_alloc, NULL);
  if (L == NULL) {
    fprintf(stderr, "failed to create AQL state\n");
    return 1;
  }

  sethvalue(L, &protected_lhs, aqlH_new(L));
  sethvalue(L, &protected_rhs, aqlH_new(L));
  protected_event = TM_ADD;
  ok &= assert_int("Lua 5.5 table + table without __add raises runtime error",
                   aqlD_rawrunprotected(L, run_trybin, NULL), AQL_ERRRUN);

  setfltvalue(&protected_lhs, 1.5);
  setivalue(&protected_rhs, 2);
  protected_event = TM_BAND;
  ok &= assert_int("Lua 5.5 non-integer bitwise without __band raises runtime error",
                   aqlD_rawrunprotected(L, run_trybin, NULL), AQL_ERRRUN);

  Table *add_mt = aqlH_new(L);
  set_tm(L, add_mt, TM_ADD, add_returns_42);
  sethvalue(L, &protected_lhs, new_table_with_mt(L, add_mt));
  sethvalue(L, &protected_rhs, aqlH_new(L));
  protected_event = TM_ADD;
  aqlT_trybinTM(L, &protected_lhs, &protected_rhs, L->top.p, protected_event);
  setobj(L, &protected_result, s2v(L->top.p));
  ok &= assert_int("__add metamethod still provides the arithmetic result",
                   ttisinteger(&protected_result) ? (int)ivalue(&protected_result) : -1,
                   42);

  aql_close(L);
  if (ok) {
    puts("metamethod_missing_55_test passed");
    return 0;
  }
  return 1;
}
