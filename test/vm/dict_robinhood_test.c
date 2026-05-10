#include <stdio.h>
#include <stdlib.h>

#include "../../src/aql.h"
#include "../../src/adict.h"
#include "../../src/aobject.h"
#include "../../src/astring.h"

static void *test_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud;
  (void)osize;
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

static int set_int(aql_State *L, Dict *dict, const char *name, aql_Integer n) {
  TValue key;
  TValue value;
  setsvalue(L, &key, aqlStr_new(L, name));
  setivalue(&value, n);
  return aqlD_set(L, dict, &key, &value);
}

static int expect_int(Dict *dict, const TValue *key, aql_Integer expected) {
  const TValue *value = aqlD_get(dict, key);
  if (value == NULL || !ttisinteger(value) || ivalue(value) != expected) {
    fprintf(stderr, "expected key to resolve to %lld\n", (long long)expected);
    return 0;
  }
  return 1;
}

int main(void) {
  int ok = 1;
  TValue start_key;
  aql_State *L = aql_newstate(test_alloc, NULL);
  if (L == NULL) {
    fprintf(stderr, "failed to create AQL state\n");
    return 1;
  }

  Dict *dict = aqlD_new(L, DT_STRING, AQL_DATA_TYPE_ANY);
  if (dict == NULL) {
    fprintf(stderr, "failed to create dict\n");
    aql_close(L);
    return 1;
  }

  ok &= set_int(L, dict, "print", 0);
  ok &= set_int(L, dict, "type", 1);
  ok &= set_int(L, dict, "len", 2);
  ok &= set_int(L, dict, "tostring", 3);
  ok &= set_int(L, dict, "tonumber", 4);
  ok &= set_int(L, dict, "range", 5);
  ok &= set_int(L, dict, "select", 6);
  ok &= set_int(L, dict, "string", 7);
  ok &= set_int(L, dict, "start", 2);
  ok &= set_int(L, dict, "limit", 6);
  ok &= set_int(L, dict, "step", 2);

  setsvalue(L, &start_key, aqlStr_new(L, "start"));
  ok &= expect_int(dict, &start_key, 2);

  aql_close(L);
  if (ok) {
    puts("dict_robinhood_test passed");
    return 0;
  }
  return 1;
}
