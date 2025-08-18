/*
** $Id: aslice.h $
** Slice implementation for AQL
** See Copyright Notice in aql.h
*/

#ifndef aslice_h
#define aslice_h

#include "aobject.h"
#include "adatatype.h"

/*
** Slice structure - Dynamic array with automatic growth
*/
struct Slice {
    CommonHeader;
    DataType dtype;     /* Element data type */
    size_t length;      /* Current number of elements */
    size_t capacity;    /* Allocated capacity */
    TValue *data;       /* Element array */
};

/*
** Slice creation and destruction
*/
AQL_API Slice *aqlS_new(aql_State *L, DataType dtype);
AQL_API Slice *aqlS_newcap(aql_State *L, DataType dtype, size_t capacity);
AQL_API void aqlS_free(aql_State *L, Slice *slice);

/*
** Slice access functions
*/
AQL_API const TValue *aqlS_get(const Slice *slice, size_t index);
AQL_API int aqlS_set(Slice *slice, size_t index, const TValue *value);
AQL_API int aqlS_push(aql_State *L, Slice *slice, const TValue *value);
AQL_API int aqlS_pop(Slice *slice, TValue *value);
AQL_API size_t aqlS_length(const Slice *slice);

/*
** Slice capacity management
*/
AQL_API int aqlS_reserve(aql_State *L, Slice *slice, size_t capacity);
AQL_API int aqlS_resize(aql_State *L, Slice *slice, size_t length);
AQL_API void aqlS_shrink(aql_State *L, Slice *slice);

/*
** Slice bounds checking
*/
static l_inline int aqlS_checkbounds(const Slice *slice, size_t index) {
  return (index < slice->length);
}

static l_inline int aqlS_checkrange(const Slice *slice, size_t start, size_t end) {
  return (start <= end && end <= slice->length);
}

/*
** Slice value access macros are defined in aobject.h
** Note: These macros are already defined in aobject.h:
** - slicevalue(o)
** - setslicevalue(L,obj,x)
** - ttisslice(o)
** - gco2slice(o)
*/

/*
** Slice utility functions
*/
AQL_API int aqlS_copy(aql_State *L, Slice *dest, const Slice *src);
AQL_API Slice *aqlS_subslice(aql_State *L, const Slice *slice, size_t start, size_t end);
AQL_API int aqlS_concat(aql_State *L, Slice *dest, const Slice *src);
AQL_API int aqlS_equal(aql_State *L, const Slice *a, const Slice *b);
AQL_API aql_Unsigned aqlSlice_hash(const Slice *slice);

/*
** Slice iteration macros
*/
#define aqlS_foreach(slice, index, value) \
    for (index = 0; index < (slice)->length && ((value) = &(slice)->data[index]); index++)

/*
** Slice metamethods (renamed to avoid conflict with astring.h)
*/
AQL_API int aqlSlice_len(aql_State *L);      /* __len */
AQL_API int aqlSlice_index(aql_State *L);    /* __index */
AQL_API int aqlSlice_newindex(aql_State *L); /* __newindex */
AQL_API int aqlSlice_eq(aql_State *L);       /* __eq */

/*
** Additional slice operations
*/
AQL_API int aqlS_insert(aql_State *L, Slice *slice, size_t index, const TValue *value);
AQL_API int aqlS_remove(aql_State *L, Slice *slice, size_t index, TValue *removed);
AQL_API void aqlS_clear(Slice *slice);

#endif