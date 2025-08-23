/*
** $Id: aql.h $
** AQL header file for C integration
** See Copyright Notice in aql.h
*/

#ifndef aql_h
#define aql_h

#include <stdarg.h>
#include <stddef.h>

#include "aconf.h"

#define AQL_VERSION_MAJOR	"1"
#define AQL_VERSION_MINOR	"0"
#define AQL_VERSION_RELEASE	"0"

#define AQL_VERSION_NUM		100
#define AQL_VERSION_RELEASE_NUM	(AQL_VERSION_NUM * 100 + 0)

#define AQL_VERSION	"AQL " AQL_VERSION_MAJOR "." AQL_VERSION_MINOR "." AQL_VERSION_RELEASE
#define AQL_RELEASE	AQL_VERSION "." AQL_VERSION_RELEASE
#define AQL_COPYRIGHT	AQL_RELEASE "  Copyright (C) 2025 AQL Team"
#define AQL_AUTHORS	"AQL Team"

/* mark for precompiled code ('<esc>AQL') */
#define AQL_SIGNATURE	"\x1bAQL"

/* option for multiple returns in 'aql_pcall' and 'aql_call' */
#define AQL_MULTRET	(-1)

/*
** Pseudo-indices
** (-AQLAI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define AQLAI_MAXSTACK		1000000
#define AQL_REGISTRYINDEX	(-AQLAI_MAXSTACK - 1000)
#define aql_upvalueindex(i)	(AQL_REGISTRYINDEX - (i))

/* thread status */
#define AQL_OK		0
#define AQL_YIELD	1
#define AQL_ERRRUN	2
#define AQL_ERRSYNTAX	3
#define AQL_ERRMEM	4
#define AQL_ERRERR	5

typedef struct aql_State aql_State;

/*
** basic types
*/
#define AQL_TNONE		(-1)
#define AQL_TNIL		0
#define AQL_TBOOLEAN		1
#define AQL_TLIGHTUSERDATA	2
#define AQL_TNUMBER		3
#define AQL_TSTRING		4
#define AQL_TTABLE		5
#define AQL_TFUNCTION		6
#define AQL_TUSERDATA		7
#define AQL_TTHREAD		8

/* AQL container types */
#define AQL_TARRAY		9
#define AQL_TSLICE		10
#define AQL_TDICT		11
#define AQL_TVECTOR		12

#define AQL_NUMTYPES		14

/* minimum AQL stack available to a C function */
#define AQL_MINSTACK	20

/* predefined values in the registry */
#define AQL_RIDX_MAINTHREAD	1
#define AQL_RIDX_GLOBALS	2
#define AQL_RIDX_LAST		AQL_RIDX_GLOBALS

/* Note: aql_Number, aql_Integer, aql_Unsigned, aql_KContext are defined in aconf.h */

/*
** Type for C functions registered with AQL
*/
typedef int (*aql_CFunction) (aql_State *L);

/*
** Type for continuation functions
*/
typedef int (*aql_KFunction) (aql_State *L, int status, aql_KContext ctx);

/*
** Type for functions that read/write blocks when loading/dumping AQL chunks
*/
typedef const char * (*aql_Reader) (aql_State *L, void *ud, size_t *sz);
typedef int (*aql_Writer) (aql_State *L, const void *p, size_t sz, void *ud);

/*
** Note: aql_Alloc is defined in aconf.h
*/

/*
** Type for warning functions
*/
typedef void (*aql_WarnFunction) (void *ud, const char *msg, int tocont);

/*
** Forward declaration for debug structure
*/
typedef struct aql_Debug aql_Debug;

/*
** Type for debug hook function
*/
typedef void (*aql_Hook) (aql_State *L, aql_Debug *ar);

/*
** generic extra include file
*/
#if defined(AQL_USER_H)
#include AQL_USER_H
#endif

/*
** RCS ident string
*/
extern const char aql_ident[];

/*
** state manipulation
*/
AQL_API aql_State *(aql_newstate) (aql_Alloc f, void *ud);
AQL_API void       (aql_close) (aql_State *L);
AQL_API aql_State *(aql_newthread) (aql_State *L);
AQL_API int        (aql_resetthread) (aql_State *L, aql_State *from);

AQL_API aql_CFunction (aql_atpanic) (aql_State *L, aql_CFunction panicf);

AQL_API aql_Number (aql_version) (aql_State *L);

/*
** basic stack manipulation
*/
AQL_API int   (aql_absindex) (aql_State *L, int idx);
AQL_API int   (aql_gettop) (aql_State *L);
AQL_API void  (aql_settop) (aql_State *L, int idx);
AQL_API void  (aql_pushvalue) (aql_State *L, int idx);
AQL_API void  (aql_rotate) (aql_State *L, int idx, int n);
AQL_API void  (aql_copy) (aql_State *L, int fromidx, int toidx);
AQL_API int   (aql_checkstack) (aql_State *L, int n);

AQL_API void  (aql_xmove) (aql_State *from, aql_State *to, int n);

/*
** access functions (stack -> C)
*/
AQL_API int             (aql_isnumber) (aql_State *L, int idx);
AQL_API int             (aql_isstring) (aql_State *L, int idx);
AQL_API int             (aql_iscfunction) (aql_State *L, int idx);
AQL_API int             (aql_isinteger) (aql_State *L, int idx);
AQL_API int             (aql_isuserdata) (aql_State *L, int idx);
AQL_API int             (aql_type) (aql_State *L, int idx);
AQL_API const char     *(aql_typename) (aql_State *L, int tp);

AQL_API aql_Number      (aql_tonumberx) (aql_State *L, int idx, int *isnum);
AQL_API aql_Integer     (aql_tointegerx) (aql_State *L, int idx, int *isnum);
AQL_API int             (aql_toboolean) (aql_State *L, int idx);
AQL_API const char     *(aql_tolstring) (aql_State *L, int idx, size_t *len);
AQL_API aql_Unsigned    (aql_rawlen) (aql_State *L, int idx);
AQL_API aql_CFunction   (aql_tocfunction) (aql_State *L, int idx);
AQL_API void	       *(aql_touserdata) (aql_State *L, int idx);
AQL_API aql_State      *(aql_tothread) (aql_State *L, int idx);
AQL_API const void     *(aql_topointer) (aql_State *L, int idx);

/*
** Comparison and arithmetic functions
*/
#define AQL_OPADD	0	/* ORDER TM, ORDER OP */
#define AQL_OPSUB	1
#define AQL_OPMUL	2
#define AQL_OPMOD	3
#define AQL_OPPOW	4
#define AQL_OPDIV	5
#define AQL_OPIDIV	6
#define AQL_OPBAND	7
#define AQL_OPBOR	8
#define AQL_OPBXOR	9
#define AQL_OPSHL	10
#define AQL_OPSHR	11
#define AQL_OPUNM	12
#define AQL_OPBNOT	13

AQL_API void  (aql_arith) (aql_State *L, int op);

#define AQL_OPEQ	0
#define AQL_OPLT	1
#define AQL_OPLE	2

AQL_API int   (aql_rawequal) (aql_State *L, int idx1, int idx2);
AQL_API int   (aql_compare) (aql_State *L, int idx1, int idx2, int op);

/*
** push functions (C -> stack)
*/
AQL_API void        (aql_pushnil) (aql_State *L);
AQL_API void        (aql_pushnumber) (aql_State *L, aql_Number n);
AQL_API void        (aql_pushinteger) (aql_State *L, aql_Integer n);
AQL_API const char *(aql_pushlstring) (aql_State *L, const char *s, size_t len);
AQL_API const char *(aql_pushstring) (aql_State *L, const char *s);
AQL_API const char *(aql_pushvfstring) (aql_State *L, const char *fmt, va_list argp);
AQL_API const char *(aql_pushfstring) (aql_State *L, const char *fmt, ...);
AQL_API void        (aql_pushcclosure) (aql_State *L, aql_CFunction fn, int n);
AQL_API void        (aql_pushboolean) (aql_State *L, int b);
AQL_API void        (aql_pushlightuserdata) (aql_State *L, void *p);
AQL_API int         (aql_pushthread) (aql_State *L);

/*
** get functions (AQL -> stack) - Container Separation Architecture
*/
AQL_API int (aql_getglobal) (aql_State *L, const char *name);

/* Container-specific access APIs */
AQL_API int (aql_getarray) (aql_State *L, int idx, aql_Integer n);
AQL_API int (aql_getslice) (aql_State *L, int idx, aql_Integer start, aql_Integer end);
AQL_API int (aql_getdict) (aql_State *L, int idx, const char *key);
AQL_API int (aql_getvector) (aql_State *L, int idx, aql_Integer n);

/* Container creation */
AQL_API void (aql_createarray) (aql_State *L, aql_Integer size);
AQL_API void (aql_createslice) (aql_State *L, aql_Integer start, aql_Integer end);
AQL_API void (aql_createdict) (aql_State *L);
AQL_API void (aql_createvector) (aql_State *L, aql_Integer size);

AQL_API void *(aql_newuserdatauv) (aql_State *L, size_t sz, int nuvalue);
AQL_API int   (aql_getmetatable) (aql_State *L, int objindex);
AQL_API int   (aql_getiuservalue) (aql_State *L, int idx, int n);

/*
** set functions (stack -> AQL) - Container Separation Architecture
*/
AQL_API void  (aql_setglobal) (aql_State *L, const char *name);

/* Container-specific assignment APIs */
AQL_API void (aql_setarray) (aql_State *L, int idx, aql_Integer n);
AQL_API void (aql_setslice) (aql_State *L, int idx, aql_Integer start, aql_Integer end);
AQL_API void (aql_setdict) (aql_State *L, int idx, const char *key);
AQL_API void (aql_setvector) (aql_State *L, int idx, aql_Integer n);

AQL_API int   (aql_setmetatable) (aql_State *L, int objindex);
AQL_API int   (aql_setiuservalue) (aql_State *L, int idx, int n);

/*
** 'load' and 'call' functions (load and run AQL code)
*/
AQL_API void  (aql_callk) (aql_State *L, int nargs, int nresults,
                           aql_KContext ctx, aql_KFunction k);
#define aql_call(L,n,r)		aql_callk(L, (n), (r), 0, NULL)

AQL_API int   (aql_pcallk) (aql_State *L, int nargs, int nresults, int errfunc,
                            aql_KContext ctx, aql_KFunction k);
#define aql_pcall(L,n,r,f)	aql_pcallk(L, (n), (r), (f), 0, NULL)

AQL_API int   (aql_load) (aql_State *L, aql_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

AQL_API int (aql_dump) (aql_State *L, aql_Writer writer, void *data, int strip);

/*
** coroutine functions
*/
AQL_API int  (aql_yieldk)     (aql_State *L, int nresults, aql_KContext ctx,
                               aql_KFunction k);
AQL_API int  (aql_resume)     (aql_State *L, aql_State *from, int narg,
                               int *nres);
AQL_API int  (aql_status)     (aql_State *L);
AQL_API int  (aql_isyieldable) (aql_State *L);

#define aql_yield(L,n)		aql_yieldk(L, (n), 0, NULL)

/*
** Warning-related functions
*/
AQL_API void (aql_setwarnf) (aql_State *L, aql_WarnFunction f, void *ud);
AQL_API void (aql_warning)  (aql_State *L, const char *msg, int tocont);

/*
** garbage-collection function and options
*/
#define AQL_GCSTOP		0
#define AQL_GCRESTART		1
#define AQL_GCCOLLECT		2
#define AQL_GCCOUNT		3
#define AQL_GCCOUNTB		4
#define AQL_GCSTEP		5
#define AQL_GCSETPAUSE		6
#define AQL_GCSETSTEPMUL	7
#define AQL_GCISRUNNING		9
#define AQL_GCGEN		10
#define AQL_GCINC		11

AQL_API int (aql_gc) (aql_State *L, int what, ...);

/*
** miscellaneous functions
*/
AQL_API int   (aql_error) (aql_State *L);
AQL_API int   (aql_next) (aql_State *L, int idx);
AQL_API void  (aql_concat) (aql_State *L, int n);
AQL_API void  (aql_len) (aql_State *L, int idx);
AQL_API size_t   (aql_stringtonumber) (aql_State *L, const char *s);
AQL_API aql_Alloc (aql_getallocf) (aql_State *L, void **ud);
AQL_API void      (aql_setallocf) (aql_State *L, aql_Alloc f, void *ud);

AQL_API void (aql_toclose) (aql_State *L, int idx);
AQL_API void (aql_closeslot) (aql_State *L, int idx);

/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define aql_getextraspace(L)	((void *)((char *)(L) - AQL_EXTRASPACE))

#define aql_tonumber(L,i)	aql_tonumberx(L,(i),NULL)
#define aql_tointeger(L,i)	aql_tointegerx(L,(i),NULL)

#define aql_pop(L,n)		aql_settop(L, -(n)-1)

#define aql_newtable(L)		aql_createtable(L, 0, 0)

#define aql_register(L,n,f) (aql_pushcfunction(L, (f)), aql_setglobal(L, (n)))

#define aql_pushcfunction(L,f)	aql_pushcclosure(L, (f), 0)

#define aql_isfunction(L,n)	(aql_type(L, (n)) == AQL_TFUNCTION)
#define aql_istable(L,n)	(aql_type(L, (n)) == AQL_TTABLE)
#define aql_islightuserdata(L,n)	(aql_type(L, (n)) == AQL_TLIGHTUSERDATA)
#define aql_isnil(L,n)		(aql_type(L, (n)) == AQL_TNIL)
#define aql_isboolean(L,n)	(aql_type(L, (n)) == AQL_TBOOLEAN)
#define aql_isthread(L,n)	(aql_type(L, (n)) == AQL_TTHREAD)
#define aql_isnone(L,n)		(aql_type(L, (n)) == AQL_TNONE)
#define aql_isnoneornil(L, n)	(aql_type(L, (n)) <= 0)

/* AQL container type checks */
#define aql_isarray(L,n)	(aql_type(L, (n)) == AQL_TARRAY)
#define aql_isslice(L,n)	(aql_type(L, (n)) == AQL_TSLICE)
#define aql_isdict(L,n)		(aql_type(L, (n)) == AQL_TDICT)
#define aql_isvector(L,n)	(aql_type(L, (n)) == AQL_TVECTOR)

#define aql_pushliteral(L, s)	aql_pushstring(L, s)

#define aql_pushglobaltable(L)  \
	((void)aql_rawgeti(L, AQL_REGISTRYINDEX, AQL_RIDX_GLOBALS))

#define aql_tostring(L,i)	aql_tolstring(L, (i), NULL)

#define aql_insert(L,idx)	aql_rotate(L, (idx), 1)

#define aql_remove(L,idx)	(aql_rotate(L, (idx), -1), aql_pop(L, 1))

#define aql_replace(L,idx)	(aql_copy(L, -1, (idx)), aql_pop(L, 1))

/* }============================================================== */

/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(AQL_COMPAT_APIINTCASTS)

#define aql_pushunsigned(L,n)	aql_pushinteger(L, (aql_Integer)(n))
#define aql_tounsignedx(L,i,is)	((aql_Unsigned)aql_tointegerx(L,i,is))
#define aql_tounsigned(L,i)	aql_tounsignedx(L,(i),NULL)

#endif
/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/

/*
** Event codes
*/
#define AQL_HOOKCALL	0
#define AQL_HOOKRET	1
#define AQL_HOOKLINE	2
#define AQL_HOOKCOUNT	3
#define AQL_HOOKTAILCALL 4

/*
** Event masks
*/
#define AQL_MASKCALL	(1 << AQL_HOOKCALL)
#define AQL_MASKRET	(1 << AQL_HOOKRET)
#define AQL_MASKLINE	(1 << AQL_HOOKLINE)
#define AQL_MASKCOUNT	(1 << AQL_HOOKCOUNT)

/* aql_Debug and aql_Hook already defined earlier in this file */

AQL_API int (aql_getstack) (aql_State *L, int level, aql_Debug *ar);
AQL_API int (aql_getinfo) (aql_State *L, const char *what, aql_Debug *ar);
AQL_API const char *(aql_getlocal) (aql_State *L, const aql_Debug *ar, int n);
AQL_API const char *(aql_setlocal) (aql_State *L, const aql_Debug *ar, int n);
AQL_API const char *(aql_getupvalue) (aql_State *L, int funcindex, int n);
AQL_API const char *(aql_setupvalue) (aql_State *L, int funcindex, int n);

AQL_API void *(aql_upvalueid) (aql_State *L, int fidx, int n);
AQL_API void  (aql_upvaluejoin) (aql_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

AQL_API void (aql_sethook) (aql_State *L, aql_Hook func, int mask, int count);
AQL_API aql_Hook (aql_gethook) (aql_State *L);
AQL_API int (aql_gethookmask) (aql_State *L);
AQL_API int (aql_gethookcount) (aql_State *L);

AQL_API int (aql_setcstacklimit) (aql_State *L, unsigned int limit);

struct aql_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'AQL', 'C', 'main', 'tail' */
  const char *source;	/* (S) */
  size_t srclen;	/* (S) */
  int currentline;	/* (l) */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  unsigned char nups;	/* (u) number of upvalues */
  unsigned char nparams;/* (u) number of parameters */
  char isvararg;        /* (u) */
  char istailcall;	/* (t) */
  unsigned short ftransfer;   /* (r) index of first value transferred */
  unsigned short ntransfer;   /* (r) number of transferred values */
  char short_src[AQL_IDXLEN]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};

/* }====================================================================== */

/******************************************************************************
* Copyright (C) 2024 AQL Team.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

#endif /* aql_h */ 