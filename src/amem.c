/*
** $Id: amem.c $
** Interface to Memory Manager
** See Copyright Notice in aql.h
*/

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <setjmp.h>

#include "amem.h"
#include "ado.h"
#include "aconf.h"
#include "aobject.h"
#include "astate.h"
#include "astring.h"
#include "aarray.h"
#include "aslice.h"
#include "adict.h"
#include "avector.h"
#include "agc.h"


/* Temporary helper functions and macros for MVP */
#define sizelstring(l) (sizeof(TString) + (l) + 1)
#define sizeofarray(n) (sizeof(Array) + (n) * sizeof(TValue))
#define sizeofslice(n) (sizeof(Slice) + (n) * sizeof(TValue))
#define sizeofdict(n) (sizeof(Dict) + (n) * sizeof(DictEntry))
#define sizeofvector(n, dt) (sizeof(Vector) + (n) * aqlDT_sizeof(dt))

/* Temporary MemStats structure */
typedef struct aql_MemStats {
  size_t total_bytes;
  size_t gc_debt;
  size_t gc_estimate;
  int gc_stepmul;
  int gc_stepsize;
} aql_MemStats;

/*
** About the realloc function:
** void * frealloc (void *ud, void *ptr, size_t osize, size_t nsize);
** ('osize' is the old size, 'nsize' is the new size)
**
** - frealloc(ud, NULL, x, s) creates a new block of size 's' (no matter 'x').
** - frealloc(ud, p, x, 0) frees the block 'p' (which must have size 'x')
**   and returns NULL.
** - frealloc(ud, NULL, 0, 0) does nothing (which is equivalent to free(NULL))
**
** frealloc returns NULL if it cannot create or reallocate the area.
*/

#define MINSIZEARRAY	4

/*
** Default allocation function
*/
static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;  /* not used */
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}

/*
** Generic allocation routine
*/
void *aqlM_realloc_(aql_State *L, void *block, size_t osize, size_t size) {
  void *newblock;
  global_State *g = G(L);
  aql_assert((osize == 0) == (block == NULL));
  newblock = (*g->frealloc)(g->ud, block, osize, size);
  if (newblock == NULL && size > 0) {
    aql_assert(size > 0);
    aqlD_throw(L, AQL_ERRMEM);  /* memory error */
  }
  aql_assert((size == 0) == (newblock == NULL));
  g->GCdebt = (g->GCdebt + size) - osize;
  return newblock;
}

/*
** Wrapper function for API compatibility
*/
void *aqlM_realloc(aql_State *L, void *block, size_t oldsize, size_t size) {
  return aqlM_realloc_(L, block, oldsize, size);
}

/*
** Create new objects (implementation of aqlM_malloc_tagged)
*/
void *aqlM_malloc_tagged(aql_State *L, size_t size, int tag) {
  global_State *g = G(L);
  GCObject *o = cast(GCObject *, aqlM_realloc(L, NULL, 0, size));
  o->marked = aqlC_white(g);
  o->tt_ = tag;
  o->next = g->allgc;
  g->allgc = o;
  return o;
}

/*
** Free an object
*/
void aqlM_freeobject(aql_State *L, GCObject *o, size_t size) {
  switch (o->tt_) {
    case AQL_TSTRING: {
      aqlM_freemem(L, o, sizelstring(gco2ts(o)->shrlen));
      break;
    }
    case AQL_TARRAY: {
      Array *arr = gco2array(o);
      aqlM_freemem(L, o, sizeofarray(arr->length));
      break;
    }
    case AQL_TSLICE: {
      Slice *slice = gco2slice(o);
      if (slice->data)
        aqlM_freemem(L, slice->data, slice->capacity * aqlDT_sizeof(slice->dtype));
      aqlM_freemem(L, o, sizeof(Slice));
      break;
    }
    case AQL_TDICT: {
      Dict *dict = gco2dict(o);
      if (dict->entries)
        aqlM_freemem(L, dict->entries, dict->capacity * sizeof(DictEntry));
      aqlM_freemem(L, o, sizeof(Dict));
      break;
    }
    case AQL_TVECTOR: {
      Vector *vec = gco2vector(o);
      aqlM_freemem(L, o, sizeofvector(vec->length, vec->dtype));
      break;
    }
    default: {
      aqlM_freemem(L, o, size);
      break;
    }
  }
}

/*
** Memory statistics
*/
void aqlM_getstats(aql_State *L, aql_MemStats *stats) {
  global_State *g = G(L);
  stats->total_bytes = g->totalbytes;
  stats->gc_debt = g->GCdebt;
  stats->gc_estimate = g->GCestimate;
  stats->gc_stepmul = g->gcstepmul;
  stats->gc_stepsize = g->gcstepsize;
}

/*
** SIMD-aligned allocation
*/
void *aqlM_alignedalloc(aql_State *L, size_t size, size_t alignment) {
  size_t total_size = size + alignment + sizeof(void*);
  void *raw_ptr = aqlM_realloc(L, NULL, 0, total_size);
  if (raw_ptr == NULL) return NULL;
  
  /* Calculate aligned address */
  uintptr_t addr = (uintptr_t)raw_ptr + sizeof(void*);
  uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
  void *aligned_ptr = (void*)aligned_addr;
  
  /* Store the original pointer just before the aligned block */
  *((void**)aligned_ptr - 1) = raw_ptr;
  
  return aligned_ptr;
}

void aqlM_alignedfree(aql_State *L, void *ptr, size_t size, size_t alignment) {
  if (ptr == NULL) return;
  
  /* Retrieve the original pointer */
  void *raw_ptr = *((void**)ptr - 1);
  size_t total_size = size + alignment + sizeof(void*);
  
  aqlM_realloc(L, raw_ptr, total_size, 0);
}

/*
** Memory error handling
*/
void aqlM_error(aql_State *L) {
  aqlD_throw(L, AQL_ERRMEM);
}

/*
** Grow arrays
*/
void *aqlM_growaux_(aql_State *L, void *block, int nelems, int *size,
                    int size_elems, int limit, const char *what) {
  void *newblock;
  int newsize;
  if (nelems + 1 <= *size)  /* does one extra element still fit? */
    return block;  /* nothing to be done */
  if (*size >= limit / 2) {  /* cannot double it? */
    if (*size >= limit)  /* cannot grow even a little? */
      aqlG_runerror(L, "too many %s (limit is %d)", what, limit);
    newsize = limit;  /* still have at least one free place */
  }
  else {
    newsize = (*size) * 2;
    if (newsize < MINSIZEARRAY)
      newsize = MINSIZEARRAY;  /* minimum size */
  }
  aql_assert(newsize >= nelems + 1);
  newblock = aqlM_realloc(L, block, cast_sizet(*size) * size_elems, cast_sizet(newsize) * size_elems);
  *size = newsize;  /* update only when everything else is OK */
  return newblock;
}

/*
** Shrink arrays
*/
void *aqlM_shrinkvector_(aql_State *L, void *block, int *size,
                         int final_n, int size_elem) {
  void *newblock;
  size_t oldsize = cast_sizet(*size) * size_elem;
  size_t newsize = cast_sizet(final_n) * size_elem;
  aql_assert(newsize <= oldsize);
  newblock = aqlM_realloc_(L, block, oldsize, newsize);
  *size = final_n;
  return newblock;
}

/*
** Initialize memory manager
*/
void aqlM_init(aql_State *L) {
  global_State *g = G(L);
  if (g->frealloc == NULL) {
    g->frealloc = l_alloc;
    g->ud = NULL;
  }
}

/*
** Set custom allocator
*/
void aqlM_setallocator(aql_State *L, aql_Alloc f, void *ud) {
  global_State *g = G(L);
  g->frealloc = f;
  g->ud = ud;
} 