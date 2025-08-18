/*
** $Id: amem.h $
** Interface to Memory Manager
** See Copyright Notice in aql.h
*/

#ifndef amem_h
#define amem_h

#include <stddef.h>
#include "aconf.h"

/* Forward declaration to break circular dependency */
typedef struct aql_State aql_State;

/*
** This macro reallocs a vector to a new size, 'n'.
** The vector 'v' must be previously allocated to 'oldn' elements.
** The macro reallocates the vector to 'n' elements and returns a 
** pointer to the new vector.
*/
#define aqlM_reallocv(L,v,oldn,n,t) \
	(cast_charp(aqlM_malloc(L, cast_sizet(n) * sizeof(t))))

#define aqlM_freemem(L, b, s)	aqlM_free(L, (b), (s))
#define aqlM_free(L, b, s)	aqlM_realloc(L, (b), (s), 0)
#define aqlM_freearray(L, b, n)   aqlM_free(L, (b), (n)*sizeof((b)[0]))

#define aqlM_malloc(L,s)	aqlM_realloc(L, NULL, 0, (s))
#define aqlM_new(L,t)		cast(t*, aqlM_malloc(L, sizeof(t)))
#define aqlM_newvector(L,n,t) \
		cast(t*, aqlM_reallocv(L, NULL, 0, n, t))

#define aqlM_newobject(L,tag,s)	aqlM_malloc_tagged(L, (s), tag)

/*
** Arrays of chars do not need any test
*/
#define aqlM_reallocvchar(L,b,on,n)  \
  cast_charp(aqlM_realloc(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define aqlM_growvector(L,v,nelems,size,t,limit,e) \
	((v)=cast(t*, aqlM_growaux(L,v,nelems,&(size),sizeof(t), \
                         (limit),"" e "")))

#define aqlM_reallocvector(L, v,oldn,n,t) \
   (cast(t*, aqlM_realloc(L, v, cast_sizet(oldn) * sizeof(t), \
                              cast_sizet(n) * sizeof(t))))

AQL_API void *aqlM_realloc(aql_State *L, void *block, size_t oldsize, size_t size);
AQL_API void *aqlM_saferealloc(aql_State *L, void *block, size_t oldsize, size_t size);
AQL_API void *aqlM_growaux(aql_State *L, void *block, int nelems, int *size,
                         int size_elem, int limit, const char *what);
AQL_API void *aqlM_malloc_tagged(aql_State *L, size_t size, int tag);
AQL_API void aqlM_checksize(aql_State *L, size_t size, const char *what);

/* Note: aqlM_free and aqlM_malloc are macros defined above */

/*
** About the realloc function:
** void *frealloc (void *ud, void *ptr, size_t osize, size_t nsize);
** ('osize' is the old size, 'nsize' is the new size)
**
** - frealloc(ud, p, x, 0) frees the block pointed to by 'p' and
**   returns NULL.
** - frealloc(ud, NULL, x, s) creates a new block of size 's' and
**   returns its address.
** - frealloc(ud, p, x, s) reallocates the block pointed to by 'p' to
**   a new size 's', copying the old contents and returning the (possibly new)
**   address of the block.
** AQL assumes that frealloc never fails when shrinking a block.
** (That is, when called with nsize < osize, frealloc returns
** a valid pointer to a block with the requested size.)
*/

typedef void * (*aql_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);

/*
** Type for arrays of bytes  
*/
typedef aql_byte *aql_Buffer;

/*
** SIMD-aligned memory allocation
*/
#if AQL_USE_SIMD
AQL_API void *aqlM_alignedalloc(aql_State *L, size_t size, size_t alignment);
AQL_API void aqlM_alignedfree(aql_State *L, void *ptr, size_t size);

#define aqlM_newsimd(L,t,n) \
  cast(t*, aqlM_alignedalloc(L, (n) * sizeof(t), AQL_SIMD_ALIGNMENT))
#define aqlM_freesimd(L,p,n,t) \
  aqlM_alignedfree(L, (p), (n) * sizeof(t))
#else
#define aqlM_newsimd(L,t,n)     aqlM_newvector(L,n,t)
#define aqlM_freesimd(L,p,n,t)  aqlM_freearray(L,p,n)
#endif

#endif /* amem_h */ 