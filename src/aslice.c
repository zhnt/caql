/*
** $Id: aslice.c $
** Slice implementation for AQL
** See Copyright Notice in aql.h
*/

#include "aslice.h"
#include "amem.h"
#include "agc.h"
#include "astring.h"
#include "aobject.h"
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
** Default initial capacity for empty slices
*/
#define DEFAULT_SLICE_CAPACITY 8

/*
** Growth factor for slice expansion (multiply by 1.5)
*/
#define SLICE_GROWTH_FACTOR(cap) ((cap) + (cap) / 2)

/*
** Create a new slice with specified data type
*/
AQL_API Slice *aqlS_new(aql_State *L, DataType dtype) {
  return aqlS_newcap(L, dtype, DEFAULT_SLICE_CAPACITY);
}

/*
** Create a new slice with specified capacity
*/
AQL_API Slice *aqlS_newcap(aql_State *L, DataType dtype, size_t capacity) {
  Slice *slice = (Slice*)aqlM_newobject(L, AQL_TSLICE, sizeof(Slice));
  if (slice == NULL) return NULL;
  
  slice->dtype = dtype;
  slice->length = 0;
  slice->capacity = capacity;
  
  if (capacity > 0) {
    slice->data = (TValue*)aqlM_newvector(L, capacity, TValue);
    if (slice->data == NULL) {
      aqlM_freemem(L, slice, sizeof(Slice));
      return NULL;
    }
  } else {
    slice->data = NULL;
  }
  
  return slice;
}

/*
** Free a slice and its data
*/
AQL_API void aqlS_free(aql_State *L, Slice *slice) {
  if (slice == NULL) return;
  
  if (slice->data != NULL) {
    aqlM_freearray(L, slice->data, slice->capacity);
  }
  aqlM_freemem(L, slice, sizeof(Slice));
}

/*
** Get element at index (bounds checked)
*/
AQL_API const TValue *aqlS_get(const Slice *slice, size_t index) {
  if (slice == NULL || index >= slice->length) {
    return NULL;
  }
  return &slice->data[index];
}

/*
** Set element at index (bounds checked)
*/
AQL_API int aqlS_set(Slice *slice, size_t index, const TValue *value) {
  if (slice == NULL || index >= slice->length || value == NULL) {
    return 0;  /* Failure */
  }
  
  setobj(NULL, &slice->data[index], value);
  return 1;  /* Success */
}

/*
** Reserve capacity for slice (grow if necessary)
*/
AQL_API int aqlS_reserve(aql_State *L, Slice *slice, size_t capacity) {
  if (slice == NULL) return 0;
  
  if (capacity <= slice->capacity) return 1;  /* Already have enough capacity */
  
  TValue *new_data = (TValue*)aqlM_reallocvector(L, slice->data, 
                                                slice->capacity, capacity, TValue);
  if (new_data == NULL) return 0;  /* Memory allocation failed */
  
  slice->data = new_data;
  slice->capacity = capacity;
  return 1;
}

/*
** Push element to end of slice
*/
AQL_API int aqlS_push(aql_State *L, Slice *slice, const TValue *value) {
  if (slice == NULL || value == NULL) return 0;
  
  /* Grow if necessary */
  if (slice->length >= slice->capacity) {
    size_t new_cap = (slice->capacity == 0) ? DEFAULT_SLICE_CAPACITY : 
                     SLICE_GROWTH_FACTOR(slice->capacity);
    if (!aqlS_reserve(L, slice, new_cap)) {
      return 0;  /* Failed to grow */
    }
  }
  
  setobj(L, &slice->data[slice->length], value);
  slice->length++;
  return 1;
}

/*
** Pop element from end of slice
*/
AQL_API int aqlS_pop(Slice *slice, TValue *value) {
  if (slice == NULL || slice->length == 0) return 0;
  
  slice->length--;
  if (value != NULL) {
    setobj(NULL, value, &slice->data[slice->length]);
  }
  setnilvalue(&slice->data[slice->length]);  /* Clear the slot */
  return 1;
}

/*
** Resize slice to new length
*/
AQL_API int aqlS_resize(aql_State *L, Slice *slice, size_t length) {
  if (slice == NULL) return 0;
  
  /* Grow capacity if necessary */
  if (length > slice->capacity) {
    if (!aqlS_reserve(L, slice, length)) {
      return 0;
    }
  }
  
  /* Initialize new elements to nil if growing */
  for (size_t i = slice->length; i < length; i++) {
    setnilvalue(&slice->data[i]);
  }
  
  slice->length = length;
  return 1;
}

/*
** Shrink slice capacity to fit length
*/
AQL_API void aqlS_shrink(aql_State *L, Slice *slice) {
  if (slice == NULL || slice->length >= slice->capacity) return;
  
  if (slice->length == 0) {
    if (slice->data != NULL) {
      aqlM_freearray(L, slice->data, slice->capacity);
      slice->data = NULL;
    }
    slice->capacity = 0;
    return;
  }
  
  TValue *new_data = (TValue*)aqlM_reallocvector(L, slice->data, 
                                                slice->capacity, slice->length, TValue);
  if (new_data != NULL) {  /* Only shrink if realloc succeeds */
    slice->data = new_data;
    slice->capacity = slice->length;
  }
}

/*
** Get slice length
*/
AQL_API size_t aqlS_length(const Slice *slice) {
  return (slice != NULL) ? slice->length : 0;
}

/*
** Copy slice contents from src to dest
*/
AQL_API int aqlS_copy(aql_State *L, Slice *dest, const Slice *src) {
  if (dest == NULL || src == NULL) return 0;
  
  /* Resize destination to match source */
  if (!aqlS_resize(L, dest, src->length)) return 0;
  
  /* Copy elements */
  for (size_t i = 0; i < src->length; i++) {
    setobj(L, &dest->data[i], &src->data[i]);
  }
  
  return 1;
}

/*
** Create a sub-slice from start to end (exclusive)
*/
AQL_API Slice *aqlS_subslice(aql_State *L, const Slice *slice, size_t start, size_t end) {
  if (slice == NULL || start > end || end > slice->length) {
    return NULL;
  }
  
  size_t subslice_len = end - start;
  Slice *sub = aqlS_newcap(L, slice->dtype, subslice_len);
  if (sub == NULL) return NULL;
  
  for (size_t i = 0; i < subslice_len; i++) {
    setobj(L, &sub->data[i], &slice->data[start + i]);
  }
  sub->length = subslice_len;
  
  return sub;
}

/*
** Concatenate src slice to dest slice
*/
AQL_API int aqlS_concat(aql_State *L, Slice *dest, const Slice *src) {
  if (dest == NULL || src == NULL) return 0;
  
  size_t old_len = dest->length;
  size_t new_len = old_len + src->length;
  
  /* Ensure capacity */
  if (!aqlS_resize(L, dest, new_len)) return 0;
  
  /* Copy elements from src */
  for (size_t i = 0; i < src->length; i++) {
    setobj(L, &dest->data[old_len + i], &src->data[i]);
  }
  
  return 1;
}

/*
** Slice comparison for equality
*/
int aqlS_equal(aql_State *L, const Slice *a, const Slice *b) {
  if (a == b) return 1;
  if (a == NULL || b == NULL) return 0;
  if (a->length != b->length || a->dtype != b->dtype) return 0;
  
  for (size_t i = 0; i < a->length; i++) {
    /* Simple TValue comparison */
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
** Slice metamethod implementations
** Note: Renamed from aqlS_len to aqlSlice_len to avoid conflict with astring.h
*/

/* __len metamethod */
AQL_API int aqlSlice_len(aql_State *L) {
  Slice *slice = slicevalue(aql_index2addr(L, 1));
  aql_pushinteger(L, (aql_Integer)slice->length);
  return 1;
}

/* __index metamethod */
AQL_API int aqlSlice_index(aql_State *L) {
  Slice *slice = slicevalue(aql_index2addr(L, 1));
  aql_Integer idx = aql_tointeger(L, 2);
  
  if (idx < 0 || (size_t)idx >= slice->length) {
    aql_pushnil(L);
    return 1;
  }
  
  aql_pushvalue(L, (int)(idx + 1));
  return 1;
}

/* __newindex metamethod */
AQL_API int aqlSlice_newindex(aql_State *L) {
  Slice *slice = slicevalue(aql_index2addr(L, 1));
  aql_Integer idx = aql_tointeger(L, 2);
  
  if (idx < 0) {
    aqlG_runerror(L, "slice index cannot be negative: %d", idx);
    return 0;
  }
  
  /* Auto-grow slice if index is beyond current length */
  if ((size_t)idx >= slice->length) {
    if (!aqlS_resize(L, slice, (size_t)idx + 1)) {
      aqlG_runerror(L, "failed to resize slice");
      return 0;
    }
  }
  
  const TValue *value = aql_index2addr(L, 3);
  setobj(L, &slice->data[idx], value);
  return 0;
}

/* __eq metamethod */
AQL_API int aqlSlice_eq(aql_State *L) {
  Slice *a = slicevalue(aql_index2addr(L, 1));
  Slice *b = slicevalue(aql_index2addr(L, 2));
  aql_pushboolean(L, aqlS_equal(L, a, b));
  return 1;
}

/*
** Slice helper methods for common operations
*/

/* Insert element at specific position */
AQL_API int aqlS_insert(aql_State *L, Slice *slice, size_t index, const TValue *value) {
  if (slice == NULL || value == NULL || index > slice->length) return 0;
  
  /* Grow if necessary */
  if (slice->length >= slice->capacity) {
    size_t new_cap = SLICE_GROWTH_FACTOR(slice->capacity);
    if (!aqlS_reserve(L, slice, new_cap)) return 0;
  }
  
  /* Shift elements to the right */
  for (size_t i = slice->length; i > index; i--) {
    setobj(L, &slice->data[i], &slice->data[i-1]);
  }
  
  /* Insert new element */
  setobj(L, &slice->data[index], value);
  slice->length++;
  return 1;
}

/* Remove element at specific position */
AQL_API int aqlS_remove(aql_State *L, Slice *slice, size_t index, TValue *removed) {
  UNUSED(L);
  if (slice == NULL || index >= slice->length) return 0;
  
  /* Save removed value if requested */
  if (removed != NULL) {
    setobj(NULL, removed, &slice->data[index]);
  }
  
  /* Shift elements to the left */
  for (size_t i = index; i < slice->length - 1; i++) {
    setobj(NULL, &slice->data[i], &slice->data[i+1]);
  }
  
  slice->length--;
  setnilvalue(&slice->data[slice->length]);  /* Clear last slot */
  return 1;
}

/* Clear all elements */
AQL_API void aqlS_clear(Slice *slice) {
  if (slice == NULL) return;
  
  for (size_t i = 0; i < slice->length; i++) {
    setnilvalue(&slice->data[i]);
  }
  slice->length = 0;
}