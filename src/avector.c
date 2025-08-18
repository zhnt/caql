/*
** $Id: avector.c $
** Vector implementation for AQL
** See Copyright Notice in aql.h
*/

#include "avector.h"
#include "amem.h"
#include "agc.h"
#include "adatatype.h"
#include "aobject.h"
#include "astate.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* Temporary implementation of aql_index2addr - should be moved to aql.c later */
static const TValue *aql_index2addr(aql_State *L, int idx) {
  /* Simplified implementation - assumes positive indices for now */
  if (idx > 0 && idx <= (L->top - L->stack)) {
    return s2v(L->stack + idx - 1);
  }
  return NULL;
}

/* Temporary aligned memory allocation functions */
static void *aqlM_alignedalloc(aql_State *L, size_t size, size_t alignment) {
  /* Simple aligned allocation using posix_memalign or fallback */
  void *ptr;
#ifdef _POSIX_C_SOURCE
  if (posix_memalign(&ptr, alignment, size) == 0) {
    return ptr;
  }
#endif
  /* Fallback to regular allocation */
  return aqlM_malloc(L, size);
}

static void aqlM_alignedfree(aql_State *L, void *ptr, size_t size, size_t alignment) {
  /* Simple free - in production should track allocation method */
  UNUSED(size); UNUSED(alignment);
  aqlM_freemem(L, ptr, size);
}

/*
** Default alignment for SIMD operations
*/
#define DEFAULT_ALIGNMENT 32  /* 256-bit SIMD (AVX2) */
#define MIN_ALIGNMENT 16      /* 128-bit SIMD (SSE) */

/*
** Get element size for data type
*/
static size_t element_size(DataType dtype) {
  switch (dtype) {
    case AQL_DATA_TYPE_INT8:   return sizeof(int8_t);
    case AQL_DATA_TYPE_UINT8:  return sizeof(uint8_t);
    case AQL_DATA_TYPE_INT16:  return sizeof(int16_t);
    case AQL_DATA_TYPE_UINT16: return sizeof(uint16_t);
    case AQL_DATA_TYPE_INT32:  return sizeof(int32_t);
    case AQL_DATA_TYPE_UINT32: return sizeof(uint32_t);
    case AQL_DATA_TYPE_INT64:  return sizeof(int64_t);
    case AQL_DATA_TYPE_UINT64: return sizeof(uint64_t);
    case AQL_DATA_TYPE_FLOAT32: return sizeof(float);
    case AQL_DATA_TYPE_FLOAT64: return sizeof(double);
    default: return 0;
  }
}

/*
** Check if data type is numeric (valid for vectors)
*/
static int is_numeric_type(DataType dtype) {
  switch (dtype) {
    case AQL_DATA_TYPE_INT8:
    case AQL_DATA_TYPE_UINT8:
    case AQL_DATA_TYPE_INT16:
    case AQL_DATA_TYPE_UINT16:
    case AQL_DATA_TYPE_INT32:
    case AQL_DATA_TYPE_UINT32:
    case AQL_DATA_TYPE_INT64:
    case AQL_DATA_TYPE_UINT64:
    case AQL_DATA_TYPE_FLOAT32:
    case AQL_DATA_TYPE_FLOAT64:
      return 1;
    default:
      return 0;
  }
}

/*
** Create a new vector with specified type and length
*/
AQL_API Vector *aqlV_new(aql_State *L, DataType dtype, size_t length) {
  return aqlV_newcap(L, dtype, length, length);
}

/*
** Create a new vector with specified capacity
*/
AQL_API Vector *aqlV_newcap(aql_State *L, DataType dtype, size_t length, size_t capacity) {
  if (!is_numeric_type(dtype)) return NULL;
  if (capacity < length) capacity = length;
  
  Vector *vec = (Vector*)aqlM_newobject(L, AQL_TVECTOR, sizeof(Vector));
  if (vec == NULL) return NULL;
  
  vec->dtype = dtype;
  vec->length = length;
  vec->capacity = capacity;
  vec->simd_width = 8;  /* Default SIMD width */
  vec->alignment = DEFAULT_ALIGNMENT;
  
  size_t elem_size = element_size(dtype);
  if (elem_size == 0) {
    aqlM_freemem(L, vec, sizeof(Vector));
    return NULL;
  }
  
  /* Allocate aligned memory for SIMD operations */
  size_t total_size = capacity * elem_size;
  vec->data = aqlM_alignedalloc(L, total_size, vec->alignment);
  if (vec->data == NULL && total_size > 0) {
    aqlM_freemem(L, vec, sizeof(Vector));
    return NULL;
  }
  
  /* Initialize to zero */
  if (vec->data != NULL) {
    memset(vec->data, 0, total_size);
  }
  
  return vec;
}

/*
** Free a vector and its data
*/
AQL_API void aqlV_free(aql_State *L, Vector *vec) {
  if (vec == NULL) return;
  
  if (vec->data != NULL) {
    size_t total_size = vec->capacity * element_size(vec->dtype);
    aqlM_alignedfree(L, vec->data, total_size, vec->alignment);
  }
  aqlM_freemem(L, vec, sizeof(Vector));
}

/*
** Type-safe getter for int32
*/
AQL_API int aqlV_geti32(const Vector *vec, size_t index, int32_t *value) {
  if (vec == NULL || value == NULL) return 0;
  if (!aqlV_checkbounds(vec, index)) return 0;
  if (vec->dtype != AQL_DATA_TYPE_INT32) return 0;
  
  int32_t *data = (int32_t*)vec->data;
  *value = data[index];
  return 1;
}

/*
** Type-safe setter for int32
*/
AQL_API int aqlV_seti32(Vector *vec, size_t index, int32_t value) {
  if (vec == NULL) return 0;
  if (!aqlV_checkbounds(vec, index)) return 0;
  if (vec->dtype != AQL_DATA_TYPE_INT32) return 0;
  
  int32_t *data = (int32_t*)vec->data;
  data[index] = value;
  return 1;
}

/*
** Type-safe getter for int64
*/
AQL_API int aqlV_geti64(const Vector *vec, size_t index, int64_t *value) {
  if (vec == NULL || value == NULL) return 0;
  if (!aqlV_checkbounds(vec, index)) return 0;
  if (vec->dtype != AQL_DATA_TYPE_INT64) return 0;
  
  int64_t *data = (int64_t*)vec->data;
  *value = data[index];
  return 1;
}

/*
** Type-safe setter for int64
*/
AQL_API int aqlV_seti64(Vector *vec, size_t index, int64_t value) {
  if (vec == NULL) return 0;
  if (!aqlV_checkbounds(vec, index)) return 0;
  if (vec->dtype != AQL_DATA_TYPE_INT64) return 0;
  
  int64_t *data = (int64_t*)vec->data;
  data[index] = value;
  return 1;
}

/*
** Type-safe getter for float32
*/
AQL_API int aqlV_getf32(const Vector *vec, size_t index, float *value) {
  if (vec == NULL || value == NULL) return 0;
  if (!aqlV_checkbounds(vec, index)) return 0;
  if (vec->dtype != AQL_DATA_TYPE_FLOAT32) return 0;
  
  float *data = (float*)vec->data;
  *value = data[index];
  return 1;
}

/*
** Type-safe setter for float32
*/
AQL_API int aqlV_setf32(Vector *vec, size_t index, float value) {
  if (vec == NULL) return 0;
  if (!aqlV_checkbounds(vec, index)) return 0;
  if (vec->dtype != AQL_DATA_TYPE_FLOAT32) return 0;
  
  float *data = (float*)vec->data;
  data[index] = value;
  return 1;
}

/*
** Type-safe getter for float64
*/
AQL_API int aqlV_getf64(const Vector *vec, size_t index, double *value) {
  if (vec == NULL || value == NULL) return 0;
  if (!aqlV_checkbounds(vec, index)) return 0;
  if (vec->dtype != AQL_DATA_TYPE_FLOAT64) return 0;
  
  double *data = (double*)vec->data;
  *value = data[index];
  return 1;
}

/*
** Type-safe setter for float64
*/
AQL_API int aqlV_setf64(Vector *vec, size_t index, double value) {
  if (vec == NULL) return 0;
  if (!aqlV_checkbounds(vec, index)) return 0;
  if (vec->dtype != AQL_DATA_TYPE_FLOAT64) return 0;
  
  double *data = (double*)vec->data;
  data[index] = value;
  return 1;
}

/*
** Generic getter through TValue
*/
AQL_API const TValue *aqlV_get(const Vector *vec, size_t index) {
  /* Note: Vectors store raw numeric data, not TValues */
  /* This would require a temporary TValue conversion */
  UNUSED(vec); UNUSED(index);
  return NULL;  /* Not implemented in this simplified version */
}

/*
** Generic setter through TValue
*/
AQL_API int aqlV_set(Vector *vec, size_t index, const TValue *value) {
  if (vec == NULL || value == NULL) return 0;
  if (!aqlV_checkbounds(vec, index)) return 0;
  
  /* Convert TValue to appropriate type and set */
  switch (vec->dtype) {
    case AQL_DATA_TYPE_INT32:
      if (ttisinteger(value)) {
        return aqlV_seti32(vec, index, (int32_t)ivalue(value));
      } else if (ttisfloat(value)) {
        return aqlV_seti32(vec, index, (int32_t)fltvalue(value));
      }
      break;
    case AQL_DATA_TYPE_INT64:
      if (ttisinteger(value)) {
        return aqlV_seti64(vec, index, (int64_t)ivalue(value));
      } else if (ttisfloat(value)) {
        return aqlV_seti64(vec, index, (int64_t)fltvalue(value));
      }
      break;
    case AQL_DATA_TYPE_FLOAT32:
      if (ttisfloat(value)) {
        return aqlV_setf32(vec, index, (float)fltvalue(value));
      } else if (ttisinteger(value)) {
        return aqlV_setf32(vec, index, (float)ivalue(value));
      }
      break;
    case AQL_DATA_TYPE_FLOAT64:
      if (ttisfloat(value)) {
        return aqlV_setf64(vec, index, (double)fltvalue(value));
      } else if (ttisinteger(value)) {
        return aqlV_setf64(vec, index, (double)ivalue(value));
      }
      break;
    default:
      return 0;
  }
  
  return 0;
}

/*
** Get vector length
*/
AQL_API size_t aqlV_length(const Vector *vec) {
  return (vec != NULL) ? vec->length : 0;
}

/*
** Reserve capacity for vector
*/
AQL_API int aqlV_reserve(aql_State *L, Vector *vec, size_t capacity) {
  if (vec == NULL) return 0;
  if (capacity <= vec->capacity) return 1;  /* Already have enough */
  
  size_t elem_size = element_size(vec->dtype);
  size_t new_size = capacity * elem_size;
  size_t old_size = vec->capacity * elem_size;
  
  void *new_data = aqlM_alignedalloc(L, new_size, vec->alignment);
  if (new_data == NULL) return 0;
  
  /* Copy old data */
  if (vec->data != NULL) {
    memcpy(new_data, vec->data, vec->length * elem_size);
    aqlM_alignedfree(L, vec->data, old_size, vec->alignment);
  }
  
  vec->data = new_data;
  vec->capacity = capacity;
  
  return 1;
}

/*
** Resize vector to new length
*/
AQL_API int aqlV_resize(aql_State *L, Vector *vec, size_t length) {
  if (vec == NULL) return 0;
  
  if (length > vec->capacity) {
    /* Need to grow capacity */
    size_t new_capacity = length * 2;  /* Grow by 2x */
    if (!aqlV_reserve(L, vec, new_capacity)) {
      return 0;
    }
  }
  
  /* Initialize new elements to zero if growing */
  if (length > vec->length) {
    size_t elem_size = element_size(vec->dtype);
    char *data = (char*)vec->data;
    memset(data + vec->length * elem_size, 0, (length - vec->length) * elem_size);
  }
  
  vec->length = length;
  return 1;
}

/*
** Get raw data pointer
*/
AQL_API void *aqlV_data(Vector *vec) {
  return (vec != NULL) ? vec->data : NULL;
}

/*
** Get const raw data pointer
*/
AQL_API const void *aqlV_cdata(const Vector *vec) {
  return (vec != NULL) ? vec->data : NULL;
}

/*
** Get element size
*/
AQL_API size_t aqlV_elementsize(const Vector *vec) {
  return (vec != NULL) ? element_size(vec->dtype) : 0;
}

/*
** Vector addition
*/
AQL_API int aqlV_add(aql_State *L, Vector *result, const Vector *a, const Vector *b) {
  if (result == NULL || a == NULL || b == NULL) return 0;
  if (a->dtype != b->dtype || a->length != b->length) return 0;
  
  if (result->dtype != a->dtype || result->length != a->length) {
    /* Resize result vector if needed */
    if (!aqlV_resize(L, result, a->length)) return 0;
    result->dtype = a->dtype;
  }
  
  size_t length = a->length;
  
  switch (a->dtype) {
    case AQL_DATA_TYPE_INT32: {
      int32_t *res = (int32_t*)result->data;
      const int32_t *pa = (const int32_t*)a->data;
      const int32_t *pb = (const int32_t*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] + pb[i];
      }
      break;
    }
    case AQL_DATA_TYPE_INT64: {
      int64_t *res = (int64_t*)result->data;
      const int64_t *pa = (const int64_t*)a->data;
      const int64_t *pb = (const int64_t*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] + pb[i];
      }
      break;
    }
    case AQL_DATA_TYPE_FLOAT32: {
      float *res = (float*)result->data;
      const float *pa = (const float*)a->data;
      const float *pb = (const float*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] + pb[i];
      }
      break;
    }
    case AQL_DATA_TYPE_FLOAT64: {
      double *res = (double*)result->data;
      const double *pa = (const double*)a->data;
      const double *pb = (const double*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] + pb[i];
      }
      break;
    }
    default:
      return 0;
  }
  
  return 1;
}

/*
** Vector subtraction
*/
AQL_API int aqlV_sub(aql_State *L, Vector *result, const Vector *a, const Vector *b) {
  if (result == NULL || a == NULL || b == NULL) return 0;
  if (a->dtype != b->dtype || a->length != b->length) return 0;
  
  if (result->dtype != a->dtype || result->length != a->length) {
    if (!aqlV_resize(L, result, a->length)) return 0;
    result->dtype = a->dtype;
  }
  
  size_t length = a->length;
  
  switch (a->dtype) {
    case AQL_DATA_TYPE_INT32: {
      int32_t *res = (int32_t*)result->data;
      const int32_t *pa = (const int32_t*)a->data;
      const int32_t *pb = (const int32_t*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] - pb[i];
      }
      break;
    }
    case AQL_DATA_TYPE_INT64: {
      int64_t *res = (int64_t*)result->data;
      const int64_t *pa = (const int64_t*)a->data;
      const int64_t *pb = (const int64_t*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] - pb[i];
      }
      break;
    }
    case AQL_DATA_TYPE_FLOAT32: {
      float *res = (float*)result->data;
      const float *pa = (const float*)a->data;
      const float *pb = (const float*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] - pb[i];
      }
      break;
    }
    case AQL_DATA_TYPE_FLOAT64: {
      double *res = (double*)result->data;
      const double *pa = (const double*)a->data;
      const double *pb = (const double*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] - pb[i];
      }
      break;
    }
    default:
      return 0;
  }
  
  return 1;
}

/*
** Vector multiplication
*/
AQL_API int aqlV_mul(aql_State *L, Vector *result, const Vector *a, const Vector *b) {
  if (result == NULL || a == NULL || b == NULL) return 0;
  if (a->dtype != b->dtype || a->length != b->length) return 0;
  
  if (result->dtype != a->dtype || result->length != a->length) {
    if (!aqlV_resize(L, result, a->length)) return 0;
    result->dtype = a->dtype;
  }
  
  size_t length = a->length;
  
  switch (a->dtype) {
    case AQL_DATA_TYPE_INT32: {
      int32_t *res = (int32_t*)result->data;
      const int32_t *pa = (const int32_t*)a->data;
      const int32_t *pb = (const int32_t*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] * pb[i];
      }
      break;
    }
    case AQL_DATA_TYPE_INT64: {
      int64_t *res = (int64_t*)result->data;
      const int64_t *pa = (const int64_t*)a->data;
      const int64_t *pb = (const int64_t*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] * pb[i];
      }
      break;
    }
    case AQL_DATA_TYPE_FLOAT32: {
      float *res = (float*)result->data;
      const float *pa = (const float*)a->data;
      const float *pb = (const float*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] * pb[i];
      }
      break;
    }
    case AQL_DATA_TYPE_FLOAT64: {
      double *res = (double*)result->data;
      const double *pa = (const double*)a->data;
      const double *pb = (const double*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] * pb[i];
      }
      break;
    }
    default:
      return 0;
  }
  
  return 1;
}

/*
** Vector division
*/
AQL_API int aqlV_div(aql_State *L, Vector *result, const Vector *a, const Vector *b) {
  if (result == NULL || a == NULL || b == NULL) return 0;
  if (a->dtype != b->dtype || a->length != b->length) return 0;
  
  if (result->dtype != a->dtype || result->length != a->length) {
    if (!aqlV_resize(L, result, a->length)) return 0;
    result->dtype = a->dtype;
  }
  
  size_t length = a->length;
  
  switch (a->dtype) {
    case AQL_DATA_TYPE_INT32: {
      int32_t *res = (int32_t*)result->data;
      const int32_t *pa = (const int32_t*)a->data;
      const int32_t *pb = (const int32_t*)b->data;
      for (size_t i = 0; i < length; i++) {
        if (pb[i] == 0) return 0;  /* Division by zero */
        res[i] = pa[i] / pb[i];
      }
      break;
    }
    case AQL_DATA_TYPE_INT64: {
      int64_t *res = (int64_t*)result->data;
      const int64_t *pa = (const int64_t*)a->data;
      const int64_t *pb = (const int64_t*)b->data;
      for (size_t i = 0; i < length; i++) {
        if (pb[i] == 0) return 0;  /* Division by zero */
        res[i] = pa[i] / pb[i];
      }
      break;
    }
    case AQL_DATA_TYPE_FLOAT32: {
      float *res = (float*)result->data;
      const float *pa = (const float*)a->data;
      const float *pb = (const float*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] / pb[i];  /* IEEE allows division by zero */
      }
      break;
    }
    case AQL_DATA_TYPE_FLOAT64: {
      double *res = (double*)result->data;
      const double *pa = (const double*)a->data;
      const double *pb = (const double*)b->data;
      for (size_t i = 0; i < length; i++) {
        res[i] = pa[i] / pb[i];  /* IEEE allows division by zero */
      }
      break;
    }
    default:
      return 0;
  }
  
  return 1;
}

/*
** Vector sum reduction
*/
AQL_API int aqlV_sum(const Vector *vec, TValue *result) {
  if (vec == NULL || result == NULL) return 0;
  if (vec->length == 0) {
    setnilvalue(result);
    return 1;
  }
  
  switch (vec->dtype) {
    case AQL_DATA_TYPE_INT32: {
      const int32_t *data = (const int32_t*)vec->data;
      int64_t sum = 0;  /* Use larger type to avoid overflow */
      for (size_t i = 0; i < vec->length; i++) {
        sum += data[i];
      }
      setivalue(result, (aql_Integer)sum);
      break;
    }
    case AQL_DATA_TYPE_INT64: {
      const int64_t *data = (const int64_t*)vec->data;
      int64_t sum = 0;
      for (size_t i = 0; i < vec->length; i++) {
        sum += data[i];
      }
      setivalue(result, (aql_Integer)sum);
      break;
    }
    case AQL_DATA_TYPE_FLOAT32: {
      const float *data = (const float*)vec->data;
      double sum = 0.0;  /* Use higher precision */
      for (size_t i = 0; i < vec->length; i++) {
        sum += data[i];
      }
      setfltvalue(result, (aql_Number)sum);
      break;
    }
    case AQL_DATA_TYPE_FLOAT64: {
      const double *data = (const double*)vec->data;
      double sum = 0.0;
      for (size_t i = 0; i < vec->length; i++) {
        sum += data[i];
      }
      setfltvalue(result, (aql_Number)sum);
      break;
    }
    default:
      return 0;
  }
  
  return 1;
}

/*
** Vector min reduction
*/
AQL_API int aqlV_min(const Vector *vec, TValue *result) {
  if (vec == NULL || result == NULL) return 0;
  if (vec->length == 0) {
    setnilvalue(result);
    return 1;
  }
  
  switch (vec->dtype) {
    case AQL_DATA_TYPE_INT32: {
      const int32_t *data = (const int32_t*)vec->data;
      int32_t min_val = data[0];
      for (size_t i = 1; i < vec->length; i++) {
        if (data[i] < min_val) min_val = data[i];
      }
      setivalue(result, (aql_Integer)min_val);
      break;
    }
    case AQL_DATA_TYPE_INT64: {
      const int64_t *data = (const int64_t*)vec->data;
      int64_t min_val = data[0];
      for (size_t i = 1; i < vec->length; i++) {
        if (data[i] < min_val) min_val = data[i];
      }
      setivalue(result, (aql_Integer)min_val);
      break;
    }
    case AQL_DATA_TYPE_FLOAT32: {
      const float *data = (const float*)vec->data;
      float min_val = data[0];
      for (size_t i = 1; i < vec->length; i++) {
        if (data[i] < min_val) min_val = data[i];
      }
      setfltvalue(result, (aql_Number)min_val);
      break;
    }
    case AQL_DATA_TYPE_FLOAT64: {
      const double *data = (const double*)vec->data;
      double min_val = data[0];
      for (size_t i = 1; i < vec->length; i++) {
        if (data[i] < min_val) min_val = data[i];
      }
      setfltvalue(result, (aql_Number)min_val);
      break;
    }
    default:
      return 0;
  }
  
  return 1;
}

/*
** Vector max reduction
*/
AQL_API int aqlV_max(const Vector *vec, TValue *result) {
  if (vec == NULL || result == NULL) return 0;
  if (vec->length == 0) {
    setnilvalue(result);
    return 1;
  }
  
  switch (vec->dtype) {
    case AQL_DATA_TYPE_INT32: {
      const int32_t *data = (const int32_t*)vec->data;
      int32_t max_val = data[0];
      for (size_t i = 1; i < vec->length; i++) {
        if (data[i] > max_val) max_val = data[i];
      }
      setivalue(result, (aql_Integer)max_val);
      break;
    }
    case AQL_DATA_TYPE_INT64: {
      const int64_t *data = (const int64_t*)vec->data;
      int64_t max_val = data[0];
      for (size_t i = 1; i < vec->length; i++) {
        if (data[i] > max_val) max_val = data[i];
      }
      setivalue(result, (aql_Integer)max_val);
      break;
    }
    case AQL_DATA_TYPE_FLOAT32: {
      const float *data = (const float*)vec->data;
      float max_val = data[0];
      for (size_t i = 1; i < vec->length; i++) {
        if (data[i] > max_val) max_val = data[i];
      }
      setfltvalue(result, (aql_Number)max_val);
      break;
    }
    case AQL_DATA_TYPE_FLOAT64: {
      const double *data = (const double*)vec->data;
      double max_val = data[0];
      for (size_t i = 1; i < vec->length; i++) {
        if (data[i] > max_val) max_val = data[i];
      }
      setfltvalue(result, (aql_Number)max_val);
      break;
    }
    default:
      return 0;
  }
  
  return 1;
}

/*
** Vector dot product
*/
AQL_API int aqlV_dot(const Vector *a, const Vector *b, TValue *result) {
  if (a == NULL || b == NULL || result == NULL) return 0;
  if (a->dtype != b->dtype || a->length != b->length) return 0;
  
  switch (a->dtype) {
    case AQL_DATA_TYPE_INT32: {
      const int32_t *pa = (const int32_t*)a->data;
      const int32_t *pb = (const int32_t*)b->data;
      int64_t dot = 0;  /* Use larger type to avoid overflow */
      for (size_t i = 0; i < a->length; i++) {
        dot += (int64_t)pa[i] * pb[i];
      }
      setivalue(result, (aql_Integer)dot);
      break;
    }
    case AQL_DATA_TYPE_INT64: {
      const int64_t *pa = (const int64_t*)a->data;
      const int64_t *pb = (const int64_t*)b->data;
      int64_t dot = 0;
      for (size_t i = 0; i < a->length; i++) {
        dot += pa[i] * pb[i];
      }
      setivalue(result, (aql_Integer)dot);
      break;
    }
    case AQL_DATA_TYPE_FLOAT32: {
      const float *pa = (const float*)a->data;
      const float *pb = (const float*)b->data;
      double dot = 0.0;  /* Use higher precision */
      for (size_t i = 0; i < a->length; i++) {
        dot += (double)pa[i] * pb[i];
      }
      setfltvalue(result, (aql_Number)dot);
      break;
    }
    case AQL_DATA_TYPE_FLOAT64: {
      const double *pa = (const double*)a->data;
      const double *pb = (const double*)b->data;
      double dot = 0.0;
      for (size_t i = 0; i < a->length; i++) {
        dot += pa[i] * pb[i];
      }
      setfltvalue(result, (aql_Number)dot);
      break;
    }
    default:
      return 0;
  }
  
  return 1;
}

/*
** Copy vector contents
*/
AQL_API int aqlV_copy(aql_State *L, Vector *dest, const Vector *src) {
  if (dest == NULL || src == NULL) return 0;
  
  /* Resize destination if needed */
  if (dest->dtype != src->dtype || dest->length != src->length) {
    if (!aqlV_resize(L, dest, src->length)) return 0;
    dest->dtype = src->dtype;
  }
  
  size_t elem_size = element_size(src->dtype);
  memcpy(dest->data, src->data, src->length * elem_size);
  
  return 1;
}

/*
** Create a slice of vector
*/
AQL_API Vector *aqlV_slice(aql_State *L, const Vector *vec, size_t start, size_t end) {
  if (vec == NULL || start >= vec->length || end > vec->length || start >= end) {
    return NULL;
  }
  
  size_t length = end - start;
  Vector *slice = aqlV_new(L, vec->dtype, length);
  if (slice == NULL) return NULL;
  
  size_t elem_size = element_size(vec->dtype);
  const char *src_data = (const char*)vec->data;
  memcpy(slice->data, src_data + start * elem_size, length * elem_size);
  
  return slice;
}

/*
** Fill vector with value
*/
AQL_API int aqlV_fill(Vector *vec, const TValue *value) {
  if (vec == NULL || value == NULL) return 0;
  
  switch (vec->dtype) {
    case AQL_DATA_TYPE_INT32: {
      int32_t val = ttisinteger(value) ? (int32_t)ivalue(value) : 
                    ttisfloat(value) ? (int32_t)fltvalue(value) : 0;
      int32_t *data = (int32_t*)vec->data;
      for (size_t i = 0; i < vec->length; i++) {
        data[i] = val;
      }
      break;
    }
    case AQL_DATA_TYPE_INT64: {
      int64_t val = ttisinteger(value) ? (int64_t)ivalue(value) : 
                    ttisfloat(value) ? (int64_t)fltvalue(value) : 0;
      int64_t *data = (int64_t*)vec->data;
      for (size_t i = 0; i < vec->length; i++) {
        data[i] = val;
      }
      break;
    }
    case AQL_DATA_TYPE_FLOAT32: {
      float val = ttisfloat(value) ? (float)fltvalue(value) : 
                  ttisinteger(value) ? (float)ivalue(value) : 0.0f;
      float *data = (float*)vec->data;
      for (size_t i = 0; i < vec->length; i++) {
        data[i] = val;
      }
      break;
    }
    case AQL_DATA_TYPE_FLOAT64: {
      double val = ttisfloat(value) ? (double)fltvalue(value) : 
                   ttisinteger(value) ? (double)ivalue(value) : 0.0;
      double *data = (double*)vec->data;
      for (size_t i = 0; i < vec->length; i++) {
        data[i] = val;
      }
      break;
    }
    default:
      return 0;
  }
  
  return 1;
}

/*
** Vector metamethods
*/

/* __len metamethod */
AQL_API int aqlV_len(aql_State *L) {
  Vector *vec = vectorvalue(aql_index2addr(L, 1));
  aql_pushinteger(L, (aql_Integer)vec->length);
  return 1;
}

/* __index metamethod */
AQL_API int aqlV_index(aql_State *L) {
  Vector *vec = vectorvalue(aql_index2addr(L, 1));
  aql_Integer index = aql_tointeger(L, 2) - 1;  /* Convert to 0-based */
  
  if (index < 0 || (size_t)index >= vec->length) {
    aql_pushnil(L);
    return 1;
  }
  
  /* Convert raw data to TValue and push */
  switch (vec->dtype) {
    case AQL_DATA_TYPE_INT32: {
      int32_t *data = (int32_t*)vec->data;
      aql_pushinteger(L, (aql_Integer)data[index]);
      break;
    }
    case AQL_DATA_TYPE_INT64: {
      int64_t *data = (int64_t*)vec->data;
      aql_pushinteger(L, (aql_Integer)data[index]);
      break;
    }
    case AQL_DATA_TYPE_FLOAT32: {
      float *data = (float*)vec->data;
      aql_pushnumber(L, (aql_Number)data[index]);
      break;
    }
    case AQL_DATA_TYPE_FLOAT64: {
      double *data = (double*)vec->data;
      aql_pushnumber(L, (aql_Number)data[index]);
      break;
    }
    default:
      aql_pushnil(L);
      break;
  }
  
  return 1;
}

/* __newindex metamethod */
AQL_API int aqlV_newindex(aql_State *L) {
  Vector *vec = vectorvalue(aql_index2addr(L, 1));
  aql_Integer index = aql_tointeger(L, 2) - 1;  /* Convert to 0-based */
  const TValue *value = aql_index2addr(L, 3);
  
  if (index < 0 || (size_t)index >= vec->length) {
    aqlG_runerror(L, "vector index out of bounds");
  }
  
  if (!aqlV_set(vec, (size_t)index, value)) {
    aqlG_runerror(L, "invalid value for vector element");
  }
  
  return 0;
}

/* __add metamethod */
AQL_API int aqlV_add_mm(aql_State *L) {
  Vector *a = vectorvalue(aql_index2addr(L, 1));
  Vector *b = vectorvalue(aql_index2addr(L, 2));
  
  Vector *result = aqlV_new(L, a->dtype, a->length);
  if (result == NULL || !aqlV_add(L, result, a, b)) {
    aqlG_runerror(L, "vector addition failed");
  }
  
  aql_pushnil(L);  /* Allocate stack space */
  setvectorvalue(L, s2v(L->top - 1), result);
  return 1;
}

/* __sub metamethod */
AQL_API int aqlV_sub_mm(aql_State *L) {
  Vector *a = vectorvalue(aql_index2addr(L, 1));
  Vector *b = vectorvalue(aql_index2addr(L, 2));
  
  Vector *result = aqlV_new(L, a->dtype, a->length);
  if (result == NULL || !aqlV_sub(L, result, a, b)) {
    aqlG_runerror(L, "vector subtraction failed");
  }
  
  aql_pushnil(L);  /* Allocate stack space */
  setvectorvalue(L, s2v(L->top - 1), result);
  return 1;
}

/* __mul metamethod */
AQL_API int aqlV_mul_mm(aql_State *L) {
  Vector *a = vectorvalue(aql_index2addr(L, 1));
  Vector *b = vectorvalue(aql_index2addr(L, 2));
  
  Vector *result = aqlV_new(L, a->dtype, a->length);
  if (result == NULL || !aqlV_mul(L, result, a, b)) {
    aqlG_runerror(L, "vector multiplication failed");
  }
  
  aql_pushnil(L);  /* Allocate stack space */
  setvectorvalue(L, s2v(L->top - 1), result);
  return 1;
}

/* __div metamethod */
AQL_API int aqlV_div_mm(aql_State *L) {
  Vector *a = vectorvalue(aql_index2addr(L, 1));
  Vector *b = vectorvalue(aql_index2addr(L, 2));
  
  Vector *result = aqlV_new(L, a->dtype, a->length);
  if (result == NULL || !aqlV_div(L, result, a, b)) {
    aqlG_runerror(L, "vector division failed");
  }
  
  aql_pushnil(L);  /* Allocate stack space */
  setvectorvalue(L, s2v(L->top - 1), result);
  return 1;
}