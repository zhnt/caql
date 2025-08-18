/*
** $Id: avector.h $
** Vector implementation for AQL
** See Copyright Notice in aql.h
*/

#ifndef avector_h
#define avector_h

#include "aobject.h"
#include "adatatype.h"

/*
** Vector structure - SIMD-optimized numeric array
*/
struct Vector {
    CommonHeader;
    DataType dtype;       /* Element data type (numeric types only) */
    size_t length;        /* Number of elements */
    size_t capacity;      /* Allocated capacity */
    aql_byte simd_width;  /* SIMD width (4, 8, 16, etc.) */
    aql_byte alignment;   /* Memory alignment (16, 32, 64 bytes) */
    void *data;           /* SIMD-aligned data buffer */
};

/*
** Vector creation and destruction
*/
AQL_API Vector *aqlV_new(aql_State *L, DataType dtype, size_t length);
AQL_API Vector *aqlV_newcap(aql_State *L, DataType dtype, size_t length, size_t capacity);
AQL_API void aqlV_free(aql_State *L, Vector *vec);

/*
** Vector access functions (type-safe)
*/
AQL_API int aqlV_geti32(const Vector *vec, size_t index, int32_t *value);
AQL_API int aqlV_seti32(Vector *vec, size_t index, int32_t value);
AQL_API int aqlV_geti64(const Vector *vec, size_t index, int64_t *value);
AQL_API int aqlV_seti64(Vector *vec, size_t index, int64_t value);
AQL_API int aqlV_getf32(const Vector *vec, size_t index, float *value);
AQL_API int aqlV_setf32(Vector *vec, size_t index, float value);
AQL_API int aqlV_getf64(const Vector *vec, size_t index, double *value);
AQL_API int aqlV_setf64(Vector *vec, size_t index, double value);

/*
** Vector generic access (through TValue)
*/
AQL_API const TValue *aqlV_get(const Vector *vec, size_t index);
AQL_API int aqlV_set(Vector *vec, size_t index, const TValue *value);
AQL_API size_t aqlV_length(const Vector *vec);

/*
** Vector capacity management
*/
AQL_API int aqlV_reserve(aql_State *L, Vector *vec, size_t capacity);
AQL_API int aqlV_resize(aql_State *L, Vector *vec, size_t length);

/*
** Vector bounds checking
*/
static l_inline int aqlV_checkbounds(const Vector *vec, size_t index) {
  return (index < vec->length);
}

/*
** Vector raw data access (for SIMD operations)
*/
AQL_API void *aqlV_data(Vector *vec);
AQL_API const void *aqlV_cdata(const Vector *vec);
AQL_API size_t aqlV_elementsize(const Vector *vec);

/*
** Vector value access macros are defined in aobject.h
** Note: These macros are already defined in aobject.h:
** - vectorvalue(o), vecvalue(o)
** - setvectorvalue(L,obj,x)
** - ttisvector(o)
** - gco2vector(o)
*/

/*
** Vector SIMD operations
*/
AQL_API int aqlV_add(aql_State *L, Vector *result, const Vector *a, const Vector *b);
AQL_API int aqlV_sub(aql_State *L, Vector *result, const Vector *a, const Vector *b);
AQL_API int aqlV_mul(aql_State *L, Vector *result, const Vector *a, const Vector *b);
AQL_API int aqlV_div(aql_State *L, Vector *result, const Vector *a, const Vector *b);

/*
** Vector reduction operations
*/
AQL_API int aqlV_sum(const Vector *vec, TValue *result);
AQL_API int aqlV_min(const Vector *vec, TValue *result);
AQL_API int aqlV_max(const Vector *vec, TValue *result);
AQL_API int aqlV_dot(const Vector *a, const Vector *b, TValue *result);

/*
** Vector utility functions
*/
AQL_API int aqlV_copy(aql_State *L, Vector *dest, const Vector *src);
AQL_API Vector *aqlV_slice(aql_State *L, const Vector *vec, size_t start, size_t end);
AQL_API int aqlV_fill(Vector *vec, const TValue *value);

/*
** Vector iteration macros
*/
#define aqlV_foreach(vec, index) \
    for (index = 0; index < (vec)->length; index++)

/*
** Vector metamethods
*/
AQL_API int aqlV_len(aql_State *L);      /* __len */
AQL_API int aqlV_index(aql_State *L);    /* __index */
AQL_API int aqlV_newindex(aql_State *L); /* __newindex */
AQL_API int aqlV_add_mm(aql_State *L);   /* __add */
AQL_API int aqlV_sub_mm(aql_State *L);   /* __sub */
AQL_API int aqlV_mul_mm(aql_State *L);   /* __mul */
AQL_API int aqlV_div_mm(aql_State *L);   /* __div */

#endif