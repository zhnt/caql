/*
** $Id: adict.h $
** Dict implementation for AQL
** See Copyright Notice in aql.h
*/

#ifndef adict_h
#define adict_h

#include "aobject.h"
#include "adatatype.h"

/*
** Dictionary entry states
*/
#define DICT_EMPTY      0
#define DICT_OCCUPIED   1
#define DICT_DELETED    2

/*
** Dictionary entry - Open addressing with Robin Hood hashing
*/
typedef struct DictEntry {
    TValue key;           /* Key value */
    TValue value;         /* Value data */
    aql_Unsigned hash;    /* Hash value for quick comparison */
    aql_byte distance;    /* Distance from ideal position (Robin Hood) */
    aql_byte flags;       /* Entry state flags */
} DictEntry;

/*
** Dict structure - Hash table with Robin Hood hashing
*/
struct Dict {
    CommonHeader;
    DataType key_type;    /* Key data type constraint */
    DataType value_type;  /* Value data type constraint */
    size_t size;          /* Number of entries */
    size_t length;        /* Number of entries (compatibility) */
    size_t capacity;      /* Hash table capacity (power of 2) */
    size_t mask;          /* Hash mask (capacity - 1) */
    aql_byte load_factor; /* Load factor threshold (0-255 for 0.0-1.0) */
    DictEntry *entries;   /* Hash table array */
};

/*
** Dict creation and destruction
*/
AQL_API Dict *aqlD_new(aql_State *L, DataType key_type, DataType value_type);
AQL_API Dict *aqlD_newcap(aql_State *L, DataType key_type, DataType value_type, size_t capacity);
AQL_API void aqlD_free(aql_State *L, Dict *dict);

/*
** Dict access functions
*/
AQL_API const TValue *aqlD_get(const Dict *dict, const TValue *key);
AQL_API int aqlD_set(aql_State *L, Dict *dict, const TValue *key, const TValue *value);
AQL_API int aqlD_delete(Dict *dict, const TValue *key);
AQL_API size_t aqlD_size(const Dict *dict);

/*
** Dict capacity management
*/
AQL_API int aqlD_reserve(aql_State *L, Dict *dict, size_t capacity);
AQL_API void aqlD_clear(Dict *dict);

/*
** Dict internal functions
*/
AQL_API aql_Unsigned aqlD_hash(const TValue *key);
AQL_API int aqlD_keyequal(const TValue *k1, const TValue *k2);

/*
** Dict entry access helpers
*/
static l_inline DictEntry *aqlD_getentry(Dict *dict, size_t index) {
  return &dict->entries[index];
}

static l_inline int aqlD_entry_empty(const DictEntry *entry) {
  return ttisnil(&entry->key);
}

static l_inline int aqlD_entry_deleted(const DictEntry *entry) {
  return ttisnil(&entry->key) && !ttisnil(&entry->value);
}

/*
** Dict value access macros are defined in aobject.h
** Note: These macros are already defined in aobject.h:
** - dictvalue(o)
** - setdictvalue(L,obj,x)
** - ttisdict(o)
** - gco2dict(o)
*/

/*
** Dict utility functions
*/
AQL_API int aqlD_copy(aql_State *L, Dict *dest, const Dict *src);
AQL_API int aqlD_merge(aql_State *L, Dict *dest, const Dict *src);

/*
** Dict iteration
*/
typedef struct DictIterator {
    Dict *dict;
    size_t index;
    DictEntry *entry;
} DictIterator;

AQL_API void aqlD_iter_init(DictIterator *iter, Dict *dict);
AQL_API int aqlD_iter_next(DictIterator *iter);

/*
** Dict iteration macro
*/
#define aqlD_foreach(dict, iter, key, value) \
    for (aqlD_iter_init(&iter, dict); \
         aqlD_iter_next(&iter) && \
         ((key) = &iter.entry->key, (value) = &iter.entry->value); )

/*
** Dict metamethods
*/
AQL_API int aqlD_len(aql_State *L);      /* __len */
AQL_API int aqlD_index(aql_State *L);    /* __index */
AQL_API int aqlD_newindex(aql_State *L); /* __newindex */

#endif