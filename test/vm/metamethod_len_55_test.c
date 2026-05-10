#include <stdio.h>
#include <stdlib.h>

#include "../../src/aql.h"
#include "../../src/aobject.h"
#include "../../src/astate.h"
#include "../../src/atable.h"

extern void aqlV_objlen(aql_State *L, StkId ra, const TValue *rb);

static void *test_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

static int len_checks_same_arg(aql_State *L) {
  const TValue *lhs = s2v(L->top.p - 2);
  const TValue *rhs = s2v(L->top.p - 1);
  int same_table = ttistable(lhs) && ttistable(rhs) &&
                   hvalue(lhs) == hvalue(rhs);
  setivalue(s2v(L->top.p), same_table ? 77 : -1);
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

static int assert_int(const char *name, int actual, int expected) {
  if (actual != expected) {
    fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
    return 0;
  }
  return 1;
}

int main(void) {
  int ok = 1;
  TValue obj;
  StkId res;
  aql_State *L = aql_newstate(test_alloc, NULL);
  if (L == NULL) {
    fprintf(stderr, "failed to create AQL state\n");
    return 1;
  }

  Table *mt = aqlH_new(L);
  set_tm(L, mt, TM_LEN, len_checks_same_arg);
  sethvalue(L, &obj, aqlH_new(L));
  hvalue(&obj)->metatable = mt;

  res = L->top.p;
  aqlV_objlen(L, res, &obj);
  ok &= assert_int("Lua 5.5 __len receives object twice",
                   ttisinteger(s2v(res)) ? (int)ivalue(s2v(res)) : -999,
                   77);

  aql_close(L);
  if (ok) {
    puts("metamethod_len_55_test passed");
    return 0;
  }
  return 1;
}
