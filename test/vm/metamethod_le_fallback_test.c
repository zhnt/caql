#include <stdio.h>
#include <stdlib.h>

#include "../../src/aql.h"
#include "../../src/aobject.h"
#include "../../src/astate.h"
#include "../../src/atable.h"

static void *test_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

static int table_rank(const TValue *o) {
  const TValue *rank;
  aql_assert(ttistable(o));
  rank = aqlH_getint(hvalue(o), 1);
  aql_assert(ttisinteger(rank));
  return (int)ivalue(rank);
}

static int lt_by_rank(aql_State *L) {
  const TValue *lhs = s2v(L->top.p - 2);
  const TValue *rhs = s2v(L->top.p - 1);
  setbvalue(s2v(L->top.p), table_rank(lhs) < table_rank(rhs));
  L->top.p++;
  return 1;
}

static int le_always_false(aql_State *L) {
  setbfvalue(s2v(L->top.p));
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

static Table *new_ranked_table(aql_State *L, Table *mt, int rank) {
  TValue key;
  TValue value;
  Table *t = aqlH_new(L);
  setivalue(&key, 1);
  setivalue(&value, rank);
  aqlH_set(L, t, &key, &value);
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

int main(void) {
  TValue lhs;
  TValue rhs;
  int ok = 1;
  aql_State *L = aql_newstate(test_alloc, NULL);
  if (L == NULL) {
    fprintf(stderr, "failed to create AQL state\n");
    return 1;
  }

  Table *lt_only_mt = aqlH_new(L);
  set_tm(L, lt_only_mt, TM_LT, lt_by_rank);
  sethvalue(L, &lhs, new_ranked_table(L, lt_only_mt, 1));
  sethvalue(L, &rhs, new_ranked_table(L, lt_only_mt, 2));
  ok &= assert_int("__le fallback accepts lower lhs",
                   aqlV_lessequal(L, &lhs, &rhs), 1);
  ok &= assert_int("__le fallback rejects higher lhs",
                   aqlV_lessequal(L, &rhs, &lhs), 0);

  Table *le_preferred_mt = aqlH_new(L);
  set_tm(L, le_preferred_mt, TM_LT, lt_by_rank);
  set_tm(L, le_preferred_mt, TM_LE, le_always_false);
  sethvalue(L, &lhs, new_ranked_table(L, le_preferred_mt, 1));
  sethvalue(L, &rhs, new_ranked_table(L, le_preferred_mt, 2));
  ok &= assert_int("__le is preferred over __lt fallback",
                   aqlV_lessequal(L, &lhs, &rhs), 0);

  aql_close(L);
  if (ok) {
    puts("metamethod_le_fallback_test passed");
    return 0;
  }
  return 1;
}
