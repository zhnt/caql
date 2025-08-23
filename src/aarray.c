/*
** $Id: aarray.c $
** Array implementation for AQL
** See Copyright Notice in aql.h
*/

#include "aarray.h"
#include "acontainer.h"  /* 添加统一容器头文件 */
#include "amem.h"
#include "agc.h"
#include "astring.h"
#include "aobject.h"
#include "aql.h"
#include <stdio.h>
#include <string.h>

/* Forward declarations to avoid circular dependencies */
#include "astate.h"

/* Temporary implementation of aql_index2addr - should be moved to aql.c later */
static const TValue *aql_index2addr(aql_State *L, int idx) {
  /* Simplified implementation - assumes positive indices for now */
  if (idx > 0 && idx <= (L->top - L->stack)) {
    return s2v(L->stack + idx - 1);
  }
  return NULL;
}

/*
** Create a new array with specified data type and length
*/
AQL_API Array *aqlA_new(aql_State *L, DataType dtype, size_t length) {
  /* 使用统一容器创建函数 */
  AQL_ContainerBase *base = acontainer_new(L, CONTAINER_ARRAY, dtype, length);
  if (base == NULL) return NULL;
  
  /* 初始化所有元素为nil */
  if (length > 0 && base->data) {
    TValue *data = (TValue*)base->data;
    for (size_t i = 0; i < length; i++) {
      setnilvalue(&data[i]);
    }
  }
  
  return (Array*)base;
}

/*
** Create a new array with buffer (capacity >= length)
*/
AQL_API Array *aqlA_newbuffer(aql_State *L, DataType dtype, size_t length, size_t capacity) {
  if (capacity < length) capacity = length;
  
  /* 使用统一容器创建函数 */
  AQL_ContainerBase *base = acontainer_new(L, CONTAINER_ARRAY, dtype, capacity);
  if (base == NULL) return NULL;
  
  /* 设置实际长度 */
  base->length = length;
  
  /* 初始化有效元素为nil */
  if (length > 0 && base->data) {
    TValue *data = (TValue*)base->data;
    for (size_t i = 0; i < length; i++) {
      setnilvalue(&data[i]);
    }
  }
  
  return (Array*)base;
}

/*
** Free an array and its data - 使用统一容器销毁
*/
AQL_API void aqlA_free(aql_State *L, Array *arr) {
  if (arr == NULL) return;
  
  /* 使用统一容器销毁函数 */
  acontainer_destroy(L, (AQL_ContainerBase*)arr);
}

/*
** Get element at index (bounds checked)
*/
AQL_API const TValue *aqlA_get(const Array *arr, size_t index) {
  if (arr == NULL || index >= arr->length) {
    return NULL;
  }
  return &arr->data[index];
}

/*
** Set element at index (bounds checked)
*/
AQL_API int aqlA_set(Array *arr, size_t index, const TValue *value) {
  if (arr == NULL || index >= arr->length || value == NULL) {
    return 0;  /* Failure */
  }
  
  setobj(NULL, &arr->data[index], value);
  return 1;  /* Success */
}

/*
** Get array length
*/
AQL_API size_t aqlA_length(const Array *arr) {
  return (arr != NULL) ? arr->length : 0;
}

/*
** Copy array contents from src to dest
*/
AQL_API int aqlA_copy(aql_State *L, Array *dest, const Array *src) {
  if (dest == NULL || src == NULL) return 0;
  
  size_t copy_len = (dest->length < src->length) ? dest->length : src->length;
  
  for (size_t i = 0; i < copy_len; i++) {
    setobj(L, &dest->data[i], &src->data[i]);
  }
  
  /* Fill remaining elements with nil if dest is larger */
  for (size_t i = copy_len; i < dest->length; i++) {
    setnilvalue(&dest->data[i]);
  }
  
  return 1;
}

/*
** Create a slice (sub-array) from start to end (exclusive)
*/
AQL_API Array *aqlA_slice(aql_State *L, const Array *arr, size_t start, size_t end) {
  if (arr == NULL || start > end || end > arr->length) {
    return NULL;
  }
  
  size_t slice_len = end - start;
  Array *slice = aqlA_new(L, arr->dtype, slice_len);
  if (slice == NULL) return NULL;
  
  for (size_t i = 0; i < slice_len; i++) {
    setobj(L, &slice->data[i], &arr->data[start + i]);
  }
  
  return slice;
}

/*
** Array comparison for equality
*/
int aqlA_equal(aql_State *L, const Array *a, const Array *b) {
  if (a == b) return 1;
  if (a == NULL || b == NULL) return 0;
  if (a->length != b->length || a->dtype != b->dtype) return 0;
  
  for (size_t i = 0; i < a->length; i++) {
    /* Simple TValue comparison for now */
    const TValue *tv1 = &a->data[i];
    const TValue *tv2 = &b->data[i];
    
    if (rawtt(tv1) != rawtt(tv2)) return 0;
    
    if (ttisinteger(tv1)) {
      if (ivalue(tv1) != ivalue(tv2)) return 0;
    } else if (ttisfloat(tv1)) {
      if (fltvalue(tv1) != fltvalue(tv2)) return 0;
    } else if (ttisstring(tv1)) {
      if (tsvalue(tv1) != tsvalue(tv2)) return 0; /* Pointer comparison for now */
    } else if (ttisnil(tv1)) {
      /* Both are nil, continue */
    } else {
      /* For other types, use pointer comparison */
      if (gcvalue(tv1) != gcvalue(tv2)) return 0;
    }
  }
  
  return 1;
}

/*
** Array hash function (for use as Dict keys)
*/
aql_Unsigned aqlA_hash(const Array *arr) {
  if (arr == NULL || arr->length == 0) return 0;
  
  aql_Unsigned hash = (aql_Unsigned)arr->length;
  aql_Unsigned step = (arr->length >> 5) + 1;  /* Hash sampling */
  
  for (size_t i = 0; i < arr->length; i += step) {
    /* Simple hash for TValue - combine type and value */
    aql_Unsigned val_hash = 0;
    const TValue *tv = &arr->data[i];
    if (ttisinteger(tv)) {
      val_hash = (aql_Unsigned)ivalue(tv);
    } else if (ttisfloat(tv)) {
      /* Simple float hash */
      union { aql_Number f; aql_Unsigned u; } fu;
      fu.f = fltvalue(tv);
      val_hash = fu.u;
    } else if (ttisstring(tv)) {
      val_hash = tsvalue(tv)->hash;
    } else {
      val_hash = (aql_Unsigned)(uintptr_t)tv;
    }
    hash ^= val_hash + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  }
  
  return hash;
}

/*
** Array metamethod implementations
*/

/* __len metamethod */
AQL_API int aqlA_len(aql_State *L) {
  Array *arr = arrayvalue(aql_index2addr(L, 1));
  aql_pushinteger(L, (aql_Integer)arr->length);
  return 1;
}

/* __index metamethod */
AQL_API int aqlA_index(aql_State *L) {
  Array *arr = arrayvalue(aql_index2addr(L, 1));
  aql_Integer idx = aql_tointeger(L, 2);
  
  if (idx < 0 || (size_t)idx >= arr->length) {
    aql_pushnil(L);
    return 1;
  }
  
  aql_pushvalue(L, (int)(idx + 1));  /* Arrays are 0-indexed internally, 1-indexed in AQL */
  return 1;
}

/* __newindex metamethod */
AQL_API int aqlA_newindex(aql_State *L) {
  Array *arr = arrayvalue(aql_index2addr(L, 1));
  aql_Integer idx = aql_tointeger(L, 2);
  
  if (idx < 0 || (size_t)idx >= arr->length) {
    aqlG_runerror(L, "array index out of bounds: %d", idx);
    return 0;
  }
  
  const TValue *value = aql_index2addr(L, 3);
  setobj(L, &arr->data[idx], value);
  return 0;
}

/* __eq metamethod */
AQL_API int aqlA_eq(aql_State *L) {
  Array *a = arrayvalue(aql_index2addr(L, 1));
  Array *b = arrayvalue(aql_index2addr(L, 2));
  aql_pushboolean(L, aqlA_equal(L, a, b));
  return 1;
}

/*
** Array iterator for for-loops
*/
typedef struct ArrayIterator {
  Array *arr;
  size_t index;
} ArrayIterator;

AQL_API int aqlA_iter_next(aql_State *L) {
  ArrayIterator *iter = (ArrayIterator*)aql_touserdata(L, 1);
  
  if (iter->index >= iter->arr->length) {
    aql_pushnil(L);
    return 1;
  }
  
  aql_pushinteger(L, (aql_Integer)iter->index);
  aql_pushvalue(L, (int)(iter->index + 2));  /* Value is at stack position */
  iter->index++;
  return 2;
}

/*
** Array string representation for debugging
*/
AQL_API int aqlA_tostring(aql_State *L) {
  Array *arr = arrayvalue(aql_index2addr(L, 1));
  char *b;
  
  /* Initialize buffer */
  b = (char*)aqlM_newvector(L, 256, char);
  
  /* Simple string formatting for array */
  sprintf(b, "Array[%zu]{", arr->length);
  
  /* For now, just return basic array info */
  /* TODO: Implement proper string formatting */
  TString *ts = aqlStr_newlstr(L, b, strlen(b));
  setsvalue2s(L, L->top - 1, ts);
  
  aqlM_freearray(L, b, 256);
  return 1;
}