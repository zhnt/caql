/*
** $Id: aarray.h $
** Array implementation for AQL
** See Copyright Notice in aql.h
*/

#ifndef aarray_h
#define aarray_h

#include "aobject.h"
#include "adatatype.h"

/*
** Array structure - Fixed-size, typed array
*/
struct Array {
    CommonHeader;
    DataType dtype;     /* Element data type */
    size_t length;      /* Number of elements */
    size_t capacity;    /* Allocated capacity (same as length for arrays) */
    TValue *data;       /* Element array */
};

/*
** Array creation and destruction
*/
AQL_API Array *aqlA_new(aql_State *L, DataType dtype, size_t length);
AQL_API Array *aqlA_newbuffer(aql_State *L, DataType dtype, size_t length, size_t capacity);
AQL_API void aqlA_free(aql_State *L, Array *arr);

/*
** Array access functions
*/
AQL_API const TValue *aqlA_get(const Array *arr, size_t index);
AQL_API int aqlA_set(Array *arr, size_t index, const TValue *value);
AQL_API size_t aqlA_length(const Array *arr);

/*
** Array bounds checking
*/
static l_inline int aqlA_checkbounds(const Array *arr, size_t index) {
  return (index < arr->length);
}

static l_inline int aqlA_checkrange(const Array *arr, size_t start, size_t end) {
  return (start <= end && end <= arr->length);
}

/*
** Array value access macros are defined in aobject.h
** Note: These macros are already defined in aobject.h:
** - arrvalue(o), arrayvalue(o)
** - setarrayvalue(L,obj,x)
** - ttisarray(o)
** - gco2array(o)
*/

/*
** Array utility functions
*/
AQL_API int aqlA_copy(aql_State *L, Array *dest, const Array *src);
AQL_API Array *aqlA_slice(aql_State *L, const Array *arr, size_t start, size_t end);
AQL_API int aqlA_equal(aql_State *L, const Array *a, const Array *b);
AQL_API aql_Unsigned aqlA_hash(const Array *arr);

/*
** Array iteration macros
*/
#define aqlA_foreach(arr, index, value) \
    for (index = 0; index < (arr)->length && ((value) = &(arr)->data[index]); index++)

#endif