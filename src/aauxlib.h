/*
** $Id: aauxlib.h $
** Auxiliary functions for building AQL libraries
** See Copyright Notice in aql.h
*/

#ifndef aauxlib_h
#define aauxlib_h

#include <stddef.h>
#include <stdio.h>

#include "aql.h"

/* Buffer size and alignment macros */
#ifndef AQLAI_BUFFERSIZE
#define AQLAI_BUFFERSIZE 8192
#endif

#ifndef AQLAI_MAXALIGN
#define AQLAI_MAXALIGN double u; void *s; long l; aql_Number n;
#endif

/* global table */
#define AQL_GNAME	"_G"

typedef struct aqlL_Buffer aqlL_Buffer;

/* extra error code for 'aqlL_loadfilex' */
#define AQL_ERRFILE     (AQL_ERRERR+1)

/* key, in the registry, for table of loaded modules */
#define AQL_LOADED_TABLE	"_LOADED"

/* key, in the registry, for table of preloaded loaders */
#define AQL_PRELOAD_TABLE	"_PRELOAD"

typedef struct aqlL_Reg {
  const char *name;
  aql_CFunction func;
} aqlL_Reg;

#define AQL_NUMSIZES	(sizeof(aql_Integer)*16 + sizeof(aql_Number))

AQL_API void (aqlL_checkversion_) (aql_State *L, aql_Number ver, size_t sz);
#define aqlL_checkversion(L)  \
	  aqlL_checkversion_(L, AQL_VERSION_NUM, AQL_NUMSIZES)

AQL_API int (aqlL_getmetafield) (aql_State *L, int obj, const char *e);
AQL_API int (aqlL_callmeta) (aql_State *L, int obj, const char *e);
AQL_API const char *(aqlL_tolstring) (aql_State *L, int idx, size_t *len);
AQL_API int (aqlL_argerror) (aql_State *L, int arg, const char *extramsg);
AQL_API int (aqlL_typeerror) (aql_State *L, int arg, const char *tname);
AQL_API const char *(aqlL_checklstring) (aql_State *L, int arg, size_t *l);
AQL_API const char *(aqlL_optlstring) (aql_State *L, int arg,
                                      const char *def, size_t *l);
AQL_API aql_Number (aqlL_checknumber) (aql_State *L, int arg);
AQL_API aql_Number (aqlL_optnumber) (aql_State *L, int arg, aql_Number def);

AQL_API aql_Integer (aqlL_checkinteger) (aql_State *L, int arg);
AQL_API aql_Integer (aqlL_optinteger) (aql_State *L, int arg, aql_Integer def);

AQL_API void (aqlL_checkstack) (aql_State *L, int sz, const char *msg);
AQL_API void (aqlL_checktype) (aql_State *L, int arg, int t);
AQL_API void (aqlL_checkany) (aql_State *L, int arg);

AQL_API int   (aqlL_newmetatable) (aql_State *L, const char *tname);
AQL_API void  (aqlL_setmetatable) (aql_State *L, const char *tname);
AQL_API void *(aqlL_testudata) (aql_State *L, int ud, const char *tname);
AQL_API void *(aqlL_checkudata) (aql_State *L, int ud, const char *tname);

AQL_API void (aqlL_where) (aql_State *L, int lvl);
AQL_API int (aqlL_error) (aql_State *L, const char *fmt, ...);

AQL_API int (aqlL_checkoption) (aql_State *L, int arg, const char *def,
                               const char *const lst[]);

AQL_API int (aqlL_fileresult) (aql_State *L, int stat, const char *fname);
AQL_API int (aqlL_execresult) (aql_State *L, int stat);

/* predefined references */
#define AQL_NOREF       (-2)
#define AQL_REFNIL      (-1)

AQL_API int (aqlL_ref) (aql_State *L, int t);
AQL_API void (aqlL_unref) (aql_State *L, int t, int ref);

AQL_API int (aqlL_loadfilex) (aql_State *L, const char *filename,
                                             const char *mode);

#define aqlL_loadfile(L,f)	aqlL_loadfilex(L,f,NULL)

AQL_API int (aqlL_loadbufferx) (aql_State *L, const char *buff, size_t sz,
                               const char *name, const char *mode);
AQL_API int (aqlL_loadstring) (aql_State *L, const char *s);

AQL_API aql_State *(aqlL_newstate) (void);

AQL_API aql_Integer (aqlL_len) (aql_State *L, int idx);

AQL_API void aqlL_addgsub (aqlL_Buffer *b, const char *s,
                                     const char *p, const char *r);
AQL_API const char *(aqlL_gsub) (aql_State *L, const char *s,
                                const char *p, const char *r);

AQL_API void (aqlL_setfuncs) (aql_State *L, const aqlL_Reg *l, int nup);

AQL_API int (aqlL_getsubtable) (aql_State *L, int idx, const char *fname);

AQL_API void (aqlL_traceback) (aql_State *L, aql_State *L1,
                              const char *msg, int level);

AQL_API void (aqlL_requiref) (aql_State *L, const char *modname,
                             aql_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/

#define aqlL_newlibtable(L,l)	\
  aql_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define aqlL_newlib(L,l)  \
  (aqlL_checkversion(L), aqlL_newlibtable(L,l), aqlL_setfuncs(L,l,0))

#define aqlL_argcheck(L, cond,arg,extramsg)	\
	((void)(aql_likely(cond) || aqlL_argerror(L, (arg), (extramsg))))

#define aqlL_argexpected(L,cond,arg,tname)	\
	((void)(aql_likely(cond) || aqlL_typeerror(L, (arg), (tname))))

#define aqlL_checkstring(L,n)	(aqlL_checklstring(L, (n), NULL))
#define aqlL_optstring(L,n,d)	(aqlL_optlstring(L, (n), (d), NULL))

#define aqlL_typename(L,i)	aql_typename(L, aql_type(L,(i)))

#define aqlL_dofile(L, fn) \
	(aqlL_loadfile(L, fn) || aql_pcall(L, 0, AQL_MULTRET, 0))

#define aqlL_dostring(L, s) \
	(aqlL_loadstring(L, s) || aql_pcall(L, 0, AQL_MULTRET, 0))

#define aqlL_getmetatable(L,n)	(aql_getfield(L, AQL_REGISTRYINDEX, (n)))

#define aqlL_opt(L,f,n,d)	(aql_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define aqlL_loadbuffer(L,s,sz,n)	aqlL_loadbufferx(L,s,sz,n,NULL)

/*
** Perform arithmetic operations on aql_Integer values with wrap-around
** semantics, as the ISO C standard requires.
*/
#if defined(AQL_USE_C89)
#define aqlL_intop(op,v1,v2)  \
	((aql_Integer)((aql_Unsigned)(v1) op (aql_Unsigned)(v2)))
#else
#define aqlL_intop(op,v1,v2)  ((v1) op (v2))
#endif

/* push the value used to represent failure/error */
#define aqlL_pushfail(L)	aql_pushnil(L)

/*
** Internal assertions for in-house debugging
*/
#if !defined(aql_assert)

#if defined(AQLAI_ASSERT)
  #include <assert.h>
  #define aql_assert(c)		assert(c)
#else
  #define aql_assert(c)		((void)0)
#endif

#endif

/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct aqlL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  aql_State *L;
  union {
    AQLAI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[AQLAI_BUFFERSIZE];  /* initial buffer */
  } init;
};

#define aqlL_bufflen(bf)	((bf)->n)
#define aqlL_buffaddr(bf)	((bf)->b)

#define aqlL_addchar(B,c) \
  ((void)((B)->n < (B)->size || aqlL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define aqlL_addsize(B,s)	((B)->n += (s))

#define aqlL_buffsub(B,s)	((B)->n -= (s))

AQL_API void (aqlL_buffinit) (aql_State *L, aqlL_Buffer *B);
AQL_API char *(aqlL_prepbuffsize) (aqlL_Buffer *B, size_t sz);
AQL_API void (aqlL_addlstring) (aqlL_Buffer *B, const char *s, size_t l);
AQL_API void (aqlL_addstring) (aqlL_Buffer *B, const char *s);
AQL_API void (aqlL_addvalue) (aqlL_Buffer *B);
AQL_API void (aqlL_pushresult) (aqlL_Buffer *B);
AQL_API void (aqlL_pushresultsize) (aqlL_Buffer *B, size_t sz);
AQL_API char *(aqlL_buffinitsize) (aql_State *L, aqlL_Buffer *B, size_t sz);

#define aqlL_prepbuffer(B)	aqlL_prepbuffsize(B, AQLAI_BUFFERSIZE)

/* }====================================================== */

/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'AQL_FILEHANDLE' and
** initial structure 'aqlL_Stream' (it may contain other fields
** after that initial structure).
*/

#define AQL_FILEHANDLE          "FILE*"

typedef struct aqlL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  aql_CFunction closef;  /* function to close stream (NULL for closed streams) */
} aqlL_Stream;

/* }====================================================== */

#if defined(AQL_COMPAT_MODULE)

AQL_API void (aqlL_pushmodule) (aql_State *L, const char *modname, int sizehint);
AQL_API void (aqlL_openlib) (aql_State *L, const char *libname,
                            const aqlL_Reg *l, int nup);

#define aqlL_register(L,n,f) (aql_pushcfunction(L, (f)), aql_setglobal(L, (n)))

#endif

/*
** {==================================================================
** AQL Container Library Support
** ===================================================================
*/

/* Container type names for registry */
#define AQL_ARRAYLIB    "aql.array"
#define AQL_SLICELIB    "aql.slice"  
#define AQL_DICTLIB     "aql.dict"
#define AQL_VECTORLIB   "aql.vector"

/* AQL container creation helpers */
AQL_API void *(aqlL_checkarray) (aql_State *L, int arg);
AQL_API void *(aqlL_checkslice) (aql_State *L, int arg);
AQL_API void *(aqlL_checkdict) (aql_State *L, int arg);
AQL_API void *(aqlL_checkvector) (aql_State *L, int arg);

AQL_API void *(aqlL_testarray) (aql_State *L, int arg);
AQL_API void *(aqlL_testslice) (aql_State *L, int arg);
AQL_API void *(aqlL_testdict) (aql_State *L, int arg);
AQL_API void *(aqlL_testvector) (aql_State *L, int arg);

/* }================================================================== */

#endif /* aauxlib_h */ 