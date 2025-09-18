/*
** $Id: adict.c $
** Dict implementation for AQL
** See Copyright Notice in aql.h
*/

#include "adict.h"
#include "acontainer.h"  /* 添加统一容器头文件 */
#include "amem.h"
#include "agc.h"
#include "astring.h"
#include "aobject.h"
#include "astate.h"
#include <string.h>
#include <stdio.h>

/* Temporary implementation of aql_index2addr - should be moved to aql.c later */
static const TValue *aql_index2addr(aql_State *L, int idx) {
  /* Simplified implementation - assumes positive indices for now */
  if (idx > 0 && idx <= (L->top - L->stack)) {
    return s2v(L->stack + idx - 1);
  }
  return NULL;
}

/*
** Default initial capacity for dicts
*/
#define DEFAULT_DICT_CAPACITY 16
#define MAX_LOAD_FACTOR 192  /* 0.75 * 256 */
#define MIN_CAPACITY 8

/*
** Hash function for TValue keys
*/
AQL_API aql_Unsigned aqlD_hash(const TValue *key) {
  if (ttisstring(key)) {
    return tsvalue(key)->hash;
  } else if (ttisinteger(key)) {
    aql_Integer i = ivalue(key);
    return (aql_Unsigned)((i ^ (i >> 32)) * 0x9e3779b97f4a7c15ULL);
  } else if (ttisfloat(key)) {
    /* Convert float to integer for hashing */
    union { aql_Number n; aql_Unsigned u; } conv;
    conv.n = fltvalue(key);
    return conv.u;
  } else if (ttisboolean(key)) {
    return bvalue(key) ? 1 : 0;
  } else if (ttisnil(key)) {
    return 0;
  } else {
    /* For other types, use pointer value */
    return (aql_Unsigned)(uintptr_t)gcvalue(key);
  }
}

/*
** Key equality comparison
*/
AQL_API int aqlD_keyequal(const TValue *k1, const TValue *k2) {
  if (rawtt(k1) != rawtt(k2)) return 0;
  
  switch (novariant(rawtt(k1))) {
    case AQL_TNIL: return 1;
    case AQL_TBOOLEAN: return bvalue(k1) == bvalue(k2);
    case AQL_TNUMBER:
      if (ttisinteger(k1) && ttisinteger(k2)) {
        return ivalue(k1) == ivalue(k2);
      } else if (ttisfloat(k1) && ttisfloat(k2)) {
        return fltvalue(k1) == fltvalue(k2);
      } else {
        return 0;  /* Mixed int/float not equal */
      }
    case AQL_TSTRING:
      return eqshrstr(tsvalue(k1), tsvalue(k2));
    default:
      return gcvalue(k1) == gcvalue(k2);
  }
}

/*
** Create a new dict with specified key and value types
*/
AQL_API Dict *aqlD_new(aql_State *L, DataType key_type, DataType value_type) {
  return aqlD_newcap(L, key_type, value_type, DEFAULT_DICT_CAPACITY);
}

/*
** Create a new dict with specified capacity - 使用统一容器基类
*/
AQL_API Dict *aqlD_newcap(aql_State *L, DataType key_type, DataType value_type, size_t capacity) {
  /* Ensure capacity is power of 2 and at least MIN_CAPACITY */
  if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;
  capacity = 1 << aqlO_ceillog2((unsigned int)capacity);
  
  /* 使用统一容器创建函数 */
  AQL_ContainerBase *base = acontainer_new(L, CONTAINER_DICT, value_type, capacity);
  if (base == NULL) return NULL;
  
  Dict *dict = (Dict*)base;
  
  /* 设置字典特定字段 */
  dict->key_type = key_type;
  dict->value_type = value_type;
  dict->size = 0;
  dict->mask = capacity - 1;
  dict->load_factor = MAX_LOAD_FACTOR;
  
  /* 初始化字典特定的union字段 */
  base->u.dict.bucket_count = capacity;
  base->u.dict.hash_mask = capacity - 1;
  base->u.dict.load_factor = 0.75;
  
  /* 分配并初始化条目数组 - 使用独立内存避免冲突 */
  dict->entries = (DictEntry*)aqlM_newvector(L, capacity, DictEntry);
  if (dict->entries == NULL) {
    /* TODO: 释放 base */
    return NULL;
  }
  
  /* 初始化所有条目为空 */
  for (size_t i = 0; i < capacity; i++) {
    setnilvalue(&dict->entries[i].key);
    setnilvalue(&dict->entries[i].value);
    dict->entries[i].hash = 0;
    dict->entries[i].distance = 0;
    dict->entries[i].flags = 0;
  }
  
  return dict;
}

/*
** Free a dict and its data - 使用统一容器销毁
*/
AQL_API void aqlD_free(aql_State *L, Dict *dict) {
  if (dict == NULL) return;
  
  /* 使用统一容器销毁函数 */
  acontainer_destroy(L, (AQL_ContainerBase*)dict);
}

/*
** Find entry for key (Robin Hood hashing)
*/
static DictEntry *findentry(const Dict *dict, const TValue *key) {
  if (dict->capacity == 0) {
    printf_debug("[DEBUG] findentry: dict capacity is 0\n");
    return NULL;
  }
  
  aql_Unsigned hash = aqlD_hash(key);
  size_t index = hash & dict->mask;
  aql_byte distance = 0;
  
  if (ttisstring(key)) {
    TString *keystr = tsvalue(key);
    printf_debug("[DEBUG] findentry: searching for key '%s', hash=%u, index=%zu\n", 
                getstr(keystr), hash, index);
  }
  
  while (1) {
    DictEntry *entry = &dict->entries[index];
    
    if (aqlD_entry_empty(entry)) {
      printf_debug("[DEBUG] findentry: found empty entry at index %zu, key_type=%d\n", 
                  index, ttype(&entry->key));
      return NULL;  /* Key not found */
    }
    
    printf_debug("[DEBUG] findentry: checking entry at index %zu, hash=%llu, distance=%d\n", 
                index, (unsigned long long)entry->hash, entry->distance);
    if (ttisstring(&entry->key)) {
      TString *entrystr = tsvalue(&entry->key);
      printf_debug("[DEBUG] findentry: entry key is string '%s'\n", getstr(entrystr));
    }
    
    if (entry->hash == hash && aqlD_keyequal(&entry->key, key)) {
      printf_debug("[DEBUG] findentry: found matching entry!\n");
      return entry;  /* Found */
    }
    
    /* Robin Hood: if we've traveled further than this entry, key doesn't exist */
    if (distance > entry->distance) {
      return NULL;
    }
    
    distance++;
    index = (index + 1) & dict->mask;
  }
}

/*
** Get value for key
*/
AQL_API const TValue *aqlD_get(const Dict *dict, const TValue *key) {
  if (dict == NULL || key == NULL) {
    printf_debug("[DEBUG] aqlD_get: dict=%p, key=%p\n", (void*)dict, (void*)key);
    return NULL;
  }
  
  if (ttisstring(key)) {
    TString *keystr = tsvalue(key);
    printf_debug("[DEBUG] aqlD_get: looking for key '%s', dict size=%zu, dict=%p\n", 
                getstr(keystr), dict->size, (void*)dict);
  }
  
  DictEntry *entry = findentry(dict, key);
  if (entry) {
    printf_debug("[DEBUG] aqlD_get: found entry, value type=%d\n", ttype(&entry->value));
  } else {
    printf_debug("[DEBUG] aqlD_get: entry not found\n");
  }
  return entry ? &entry->value : NULL;
}

/*
** Resize dict to new capacity
*/
static int dict_resize(aql_State *L, Dict *dict, size_t new_capacity) {
  /* Save old data */
  DictEntry *old_entries = dict->entries;
  size_t old_capacity = dict->capacity;
  
  /* Allocate new array */
  dict->entries = (DictEntry*)aqlM_newvector(L, new_capacity, DictEntry);
  if (dict->entries == NULL) {
    dict->entries = old_entries;  /* Restore on failure */
    return 0;
  }
  
  dict->capacity = new_capacity;
  dict->mask = new_capacity - 1;
  dict->size = 0;
  
  /* Initialize new entries */
  for (size_t i = 0; i < new_capacity; i++) {
    dict->entries[i].hash = 0;
    dict->entries[i].distance = 0;
    setnilvalue(&dict->entries[i].key);
    setnilvalue(&dict->entries[i].value);
  }
  
  /* Rehash all old entries */
  for (size_t i = 0; i < old_capacity; i++) {
    DictEntry *old_entry = &old_entries[i];
    if (!aqlD_entry_empty(old_entry)) {
      aqlD_set(L, dict, &old_entry->key, &old_entry->value);
    }
  }
  
  /* Free old array */
  aqlM_freearray(L, old_entries, old_capacity);
  return 1;
}

/*
** Set key-value pair (Robin Hood hashing)
*/
AQL_API int aqlD_set(aql_State *L, Dict *dict, const TValue *key, const TValue *value) {
  if (dict == NULL || key == NULL || value == NULL) {
    printf_debug("[DEBUG] aqlD_set: dict=%p, key=%p, value=%p\n", (void*)dict, (void*)key, (void*)value);
    return 0;
  }
  
  if (ttisstring(key)) {
    TString *keystr = tsvalue(key);
    printf_debug("[DEBUG] aqlD_set: setting key '%s', dict size=%zu, dict=%p\n", 
                getstr(keystr), dict->size, (void*)dict);
  }
  if (ttisinteger(value)) {
    printf_debug("[DEBUG] aqlD_set: value is integer %lld\n", (long long)ivalue(value));
  }
  
  /* Check load factor and resize if necessary */
  if ((dict->size + 1) * 256 > dict->capacity * dict->load_factor) {
    if (!dict_resize(L, dict, dict->capacity * 2)) {
      return 0;  /* Failed to resize */
    }
  }
  
  aql_Unsigned hash = aqlD_hash(key);
  size_t index = hash & dict->mask;
  aql_byte distance = 0;
  
  if (ttisstring(key)) {
    TString *keystr = tsvalue(key);
    printf_debug("[DEBUG] aqlD_set: computed hash=%llu for key '%s'\n", 
                (unsigned long long)hash, getstr(keystr));
  }
  
  /* Create entry to insert */
  DictEntry to_insert;
  to_insert.hash = hash;
  to_insert.distance = distance;
  printf_debug("[DEBUG] aqlD_set: before setobj, hash=%llu\n", (unsigned long long)to_insert.hash);
  setobj(L, &to_insert.key, key);
  setobj(L, &to_insert.value, value);
  printf_debug("[DEBUG] aqlD_set: after setobj, hash=%llu\n", (unsigned long long)to_insert.hash);
  
  printf_debug("[DEBUG] aqlD_set: created entry with hash=%llu\n", (unsigned long long)to_insert.hash);
  
  while (1) {
    DictEntry *entry = &dict->entries[index];
    
    if (aqlD_entry_empty(entry)) {
      /* Empty slot, insert here */
      *entry = to_insert;
      printf_debug("[DEBUG] aqlD_set: stored entry at index %zu with hash=%llu\n", 
                  index, (unsigned long long)entry->hash);
      dict->size++;
      dict->length = dict->size;  /* Keep length in sync */
      return 1;
    }
    
    if (entry->hash == hash && aqlD_keyequal(&entry->key, key)) {
      /* Update existing key */
      setobj(L, &entry->value, value);
      return 1;
    }
    
    /* Robin Hood: if we've traveled further, swap and continue */
    if (to_insert.distance > entry->distance) {
      DictEntry temp = *entry;
      *entry = to_insert;
      to_insert = temp;
      /* Update distance for the displaced entry */
      to_insert.distance = distance;
    }
    
    to_insert.distance++;
    index = (index + 1) & dict->mask;
  }
}

/*
** Delete key from dict
*/
AQL_API int aqlD_delete(Dict *dict, const TValue *key) {
  if (dict == NULL || key == NULL) return 0;
  
  DictEntry *entry = findentry(dict, key);
  if (entry == NULL) return 0;  /* Key not found */
  
  /* Mark as deleted and shift entries back */
  size_t index = entry - dict->entries;
  size_t next_index = (index + 1) & dict->mask;
  
  while (!aqlD_entry_empty(&dict->entries[next_index]) && 
         dict->entries[next_index].distance > 0) {
    dict->entries[index] = dict->entries[next_index];
    dict->entries[index].distance--;
    
    index = next_index;
    next_index = (next_index + 1) & dict->mask;
  }
  
  /* Clear the final entry */
  dict->entries[index].hash = 0;
  dict->entries[index].distance = 0;
  setnilvalue(&dict->entries[index].key);
  setnilvalue(&dict->entries[index].value);
  
  dict->size--;
  dict->length = dict->size;
  return 1;
}

/*
** Get dict size
*/
AQL_API size_t aqlD_size(const Dict *dict) {
  return (dict != NULL) ? dict->size : 0;
}

/*
** Reserve capacity for dict
*/
AQL_API int aqlD_reserve(aql_State *L, Dict *dict, size_t capacity) {
  if (dict == NULL) return 0;
  
  /* Round up to power of 2 */
  capacity = 1 << aqlO_ceillog2((unsigned int)capacity);
  
  if (capacity <= dict->capacity) return 1;  /* Already have enough */
  
  return dict_resize(L, dict, capacity);
}

/*
** Clear all entries
*/
AQL_API void aqlD_clear(Dict *dict) {
  if (dict == NULL) return;
  
  for (size_t i = 0; i < dict->capacity; i++) {
    dict->entries[i].hash = 0;
    dict->entries[i].distance = 0;
    setnilvalue(&dict->entries[i].key);
    setnilvalue(&dict->entries[i].value);
  }
  
  dict->size = 0;
  dict->length = 0;
}

/*
** Copy dict contents from src to dest
*/
AQL_API int aqlD_copy(aql_State *L, Dict *dest, const Dict *src) {
  if (dest == NULL || src == NULL) return 0;
  
  aqlD_clear(dest);
  
  for (size_t i = 0; i < src->capacity; i++) {
    const DictEntry *entry = &src->entries[i];
    if (!aqlD_entry_empty(entry)) {
      if (!aqlD_set(L, dest, &entry->key, &entry->value)) {
        return 0;  /* Failed to set */
      }
    }
  }
  
  return 1;
}

/*
** Merge src dict into dest dict
*/
AQL_API int aqlD_merge(aql_State *L, Dict *dest, const Dict *src) {
  if (dest == NULL || src == NULL) return 0;
  
  for (size_t i = 0; i < src->capacity; i++) {
    const DictEntry *entry = &src->entries[i];
    if (!aqlD_entry_empty(entry)) {
      if (!aqlD_set(L, dest, &entry->key, &entry->value)) {
        return 0;  /* Failed to set */
      }
    }
  }
  
  return 1;
}

/*
** Dict iterator implementation
*/
AQL_API void aqlD_iter_init(DictIterator *iter, Dict *dict) {
  if (iter == NULL || dict == NULL) return;
  
  iter->dict = dict;
  iter->index = 0;
  iter->entry = NULL;
}

AQL_API int aqlD_iter_next(DictIterator *iter) {
  if (iter == NULL || iter->dict == NULL) return 0;
  
  while (iter->index < iter->dict->capacity) {
    DictEntry *entry = &iter->dict->entries[iter->index];
    iter->index++;
    
    if (!aqlD_entry_empty(entry)) {
      iter->entry = entry;
      return 1;  /* Found next entry */
    }
  }
  
  iter->entry = NULL;
  return 0;  /* No more entries */
}

/*
** Dict metamethod implementations
*/

/* __len metamethod */
AQL_API int aqlD_len(aql_State *L) {
  Dict *dict = dictvalue(aql_index2addr(L, 1));
  aql_pushinteger(L, (aql_Integer)dict->size);
  return 1;
}

/* __index metamethod */
AQL_API int aqlD_index(aql_State *L) {
  Dict *dict = dictvalue(aql_index2addr(L, 1));
  const TValue *key = aql_index2addr(L, 2);
  
  const TValue *value = aqlD_get(dict, key);
  if (value != NULL) {
    /* TODO: Properly convert TValue to stack value */
    aql_pushnil(L);  /* Placeholder for now */
  } else {
    aql_pushnil(L);
  }
  return 1;
}

/* __newindex metamethod */
AQL_API int aqlD_newindex(aql_State *L) {
  Dict *dict = dictvalue(aql_index2addr(L, 1));
  const TValue *key = aql_index2addr(L, 2);
  const TValue *value = aql_index2addr(L, 3);
  
  if (ttisnil(value)) {
    aqlD_delete(dict, key);  /* Setting to nil deletes the key */
  } else {
    if (!aqlD_set(L, dict, key, value)) {
      aqlG_runerror(L, "failed to set dict entry");
    }
  }
  return 0;
}

/*
** Dict comparison for equality
*/
int aqlD_equal(const Dict *a, const Dict *b) {
  if (a == b) return 1;
  if (a == NULL || b == NULL) return 0;
  if (a->size != b->size) return 0;
  
  /* Check all entries in a exist in b with same values */
  for (size_t i = 0; i < a->capacity; i++) {
    const DictEntry *entry = &a->entries[i];
    if (!aqlD_entry_empty(entry)) {
      const TValue *b_value = aqlD_get(b, &entry->key);
      if (b_value == NULL || !aqlD_keyequal(&entry->value, b_value)) {
        return 0;
      }
    }
  }
  
  return 1;
}