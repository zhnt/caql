/*
** $Id: astate.h $
** Global State
** See Copyright Notice in aql.h
*/

#ifndef astate_h
#define astate_h

#include "aconf.h"
#include "aobject.h"

/*
** Atomic type (relative to signals) to better ensure that 'aql_sethook'
** is thread safe (based on Lua's design)
*/
#if !defined(l_signalT)
#include <signal.h>
#define l_signalT	sig_atomic_t
#endif

/*
** Information about a call.
** About union 'u':
** - field 'l' is used only for AQL functions;
** - field 'c' is used only for C functions.
** About union 'u2':
** - field 'funcidx' is used only by C functions while doing a
** protected call;
** - field 'nyield' is used only while a function is "doing" an
** yield (from the yield until the next resume);
** - field 'nres' is used only while closing tbc variables when
** returning from a C function;
** - field 'transferinfo' is used only during call/returnhooks,
** before the actual call starts or after the actual return ends.
*/
typedef struct CallInfo {
  StkId func;  /* function index in the stack */
  StkId	top;  /* top for this function */
  struct CallInfo *previous, *next;  /* dynamic call link */
  union {
    struct {  /* only for AQL functions */
      const Instruction *savedpc;
      volatile l_signalT trap;
      int nextraargs;  /* # of extra arguments in vararg functions */
    } l;
    struct {  /* only for C functions */
      aql_KFunction k;  /* continuation in case of yields */
      ptrdiff_t old_errfunc;
      aql_KContext ctx;  /* context info. in case of yields */
    } c;
  } u;
  union {
    int funcidx;  /* called-function index */
    int nyield;  /* number of values yielded */
    int nres;  /* number of values returned */
    struct {  /* info about transferred values (for call/return hooks) */
      unsigned short ftransfer;  /* offset of first value transferred */
      unsigned short ntransfer;  /* number of values transferred */
    } transferinfo;
  } u2;
  short nresults;  /* expected number of results from this function */
  unsigned short callstatus;
} CallInfo;

/*
** Bits in CallInfo status
*/
#define CIST_OAH	(1<<0)	/* original value of 'allowhook' */
#define CIST_C		(1<<1)	/* call is running a C function */
#define CIST_FRESH	(1<<2)	/* call is on a fresh "aql_execute" frame */
#define CIST_HOOKED	(1<<3)	/* call is running a debug hook */
#define CIST_YPCALL	(1<<4)	/* doing a yieldable protected call */

/*
** 'per thread' state
*/
struct aql_State {
  CommonHeader;
  StkId top;  /* first free slot in the stack */
  struct global_State *l_G;
  struct CallInfo *ci;  /* call info for current function */
  StkId stack_last;  /* last free slot in the stack */
  StkId stack;  /* stack base */
  UpVal *openupval;  /* list of open upvalues in this stack */
  StkId func;  /* current function (CallInfo funcOff) */
  GCObject *gclist;
  struct aql_State *twups;  /* list of threads with open upvalues */
  struct aql_longjmp *errorJmp;  /* current error recover point */
  CallInfo base_ci;  /* CallInfo for first level (C calling AQL) */
  volatile aql_Hook hook;
  ptrdiff_t errfunc;  /* current error handling function (stack index) */
  l_uint32 nCcalls;  /* number of nested (non-yieldable | C) calls */
  int oldpc;  /* last pc traced */
  int basehookcount;
  int hookcount;
  volatile l_signalT hookmask;
  aql_byte status;
  aql_byte allowhook;
  unsigned short nci;  /* number of items in 'ci' list */
};

#define G(L)	(L->l_G)

/*
** Union of all collectable objects (only for conversions)
** ISO C99, 6.5.2.3 p.5:
** "... if a union contains several structures that share a common
** initial sequence of members, ... it is permitted to inspect the
** common initial part of any of them anywhere that a declaration of
** the complete type of the union is visible."
*/
/* Note: GCUnion is defined in aobject.h */

/*
** Note: cast_u and gco2* macros are defined in aobject.h
*/

/*
** Atomic type (relative to signals) to better ensure that 'aql_sethook'
** is atomic (i.e., a signal will not interrupt it and call the hook)
** and 'hookmask' is coherent (i.e., will not see a partial change
** made by the signal)
*/
/* Note: l_signalT is defined in alimits.h */

/*
** Extra stack space to handle TM calls and some other extras. This
** space is not included in 'stack_last'. It is used only to avoid stack
** checks, either because the element will be promptly popped or because
** there will be a stack check soon after the push. Function frames
** never use this extra space, so it does not need to be kept clean.
*/
#define EXTRA_STACK   5

#define stacksize(th)	cast_int((th)->stack_last - (th)->stack)

/* kinds of Garbage Collection */
#define KGC_INC		0	/* incremental gc */
#define KGC_GEN		1	/* generational gc */

/*
** String table (hash table for strings)
*/
typedef struct stringtable {
  TString **hash;
  int nuse;  /* number of elements */
  int size;
} stringtable;

/* Note: CallInfo is now defined earlier in this file */

/* Additional CallInfo status bits */
#define CIST_TAIL	(1<<5)	/* call was tail called */
#define CIST_HOOKYIELD	(1<<6)	/* last hook called yielded */
#define CIST_FIN	(1<<7)	/* call is running a finalizer */
#define CIST_TRAN	(1<<8)	/* 'ci' has transfer information */
#define CIST_CLSRET	(1<<9)  /* function is closing tbc variables */
/* Bits 10-12 are used for CIST_RECST (see below) */
#define CIST_RECST	10
#if defined(AQL_COMPAT_LT_LE)
#define CIST_LEQ	(1<<13)  /* using __lt for __le */
#endif

/*
** Field CIST_RECST stores the "recover status", used to keep the error
** status while closing to-be-closed variables in coroutines, so the
** original error can be recovered and be re-raised. (Three bits are
** enough for error status.)
*/
#define getcistrecst(ci)     (((ci)->callstatus >> CIST_RECST) & 7)
#define setcistrecst(ci,st)  \
  check_exp(((st) & 7) == (st),   /* status must fit in three bits */  \
            ((ci)->callstatus = ((ci)->callstatus & ~(7 << CIST_RECST))  \
                                | ((st) << CIST_RECST)))

/* active function is a AQL function */
#define isAql(ci)	(!(ci)->callstatus & CIST_C)

/* call is running AQL code (not a hook) */
#define isAqlcode(ci)	(isAql(ci) && !(ci)->callstatus & CIST_HOOKED)

/* assume that CIST_OAH has offset 0 and that 'v' is strictly 0/1 */
#define setoah(st,v)	((st) = ((st) & ~CIST_OAH) | (v))
#define getoah(st)	((st) & CIST_OAH)

/*
** 'global state', shared by all threads of this state
*/
typedef struct global_State {
  aql_Alloc frealloc;  /* function to reallocate memory */
  void *ud;         /* auxiliary data to 'frealloc' */
  l_mem totalbytes;  /* number of bytes currently allocated - GCdebt */
  l_mem GCdebt;  /* bytes allocated not yet compensated by the collector */
  aql_mem GCestimate;  /* an estimate of the non-garbage memory in use */
  aql_mem lastatomic;  /* see function 'genstep' in file 'lgc.c' */
  stringtable strt;  /* hash table for strings */
  TValue l_registry;
  TValue nilvalue;  /* a nil value */
  unsigned int seed;  /* randomized seed for hashes */
  aql_byte currentwhite;
  aql_byte gcstate;  /* state of garbage collector */
  aql_byte gckind;  /* kind of GC running */
  aql_byte genminormul, genmajormul;  /* control for GC generational mode */
  aql_byte gcrunning;  /* true if GC is running */
  aql_byte gcemergency;  /* true if this is an emergency collection */
  aql_byte gcpause;  /* size of pause between successive GCs */
  aql_byte gcstepmul;  /* GC "speed" */
  aql_byte gcstepsize;  /* (log2 of) GC granularity */
  GCObject *allgc;  /* list of all collectable objects */
  GCObject **sweepgc;  /* current position of sweep in list */
  GCObject *finobj;  /* list of collectable objects with finalizers */
  GCObject *gray;  /* list of gray objects */
  GCObject *grayagain;  /* list of objects to be traversed atomically */
  GCObject *weak;  /* list of tables with weak values */
  GCObject *ephemeron;  /* list of ephemeron tables (weak keys) */
  GCObject *allweak;  /* list of all-weak tables */
  GCObject *tobefnz;  /* list of userdata to be GC */
  GCObject *fixedgc;  /* list of objects not to be collected */
  /* fields for generational collector */
  GCObject *survival;  /* list of survival objects */
  GCObject *old1;  /* list of old1 objects */
  GCObject *reallyold;  /* list of really old objects */
  GCObject *firstold1;  /* first OLD1 object in the list (after survival) */
  GCObject *finoold;  /* list of survival/old objects with finalizers */
  struct aql_State *twups;  /* list of threads with open upvalues */
  aql_CFunction panic;  /* to be called in unprotected errors */
  struct aql_State *mainthread;
  TString *memerrmsg;  /* message for memory-allocation errors */
  TString *tmname[TM_N];  /* array with tag-method names */
  struct Table *mt[AQL_NUMTYPES];  /* metatables for basic types */
  TString *strcache[STRCACHE_N][STRCACHE_M];  /* cache for strings in API */
  aql_WarnFunction warnf;  /* warning function */
  void *ud_warn;         /* auxiliary data to 'warnf' */
} global_State;

/*
** Duplicate aql_State definition removed - already defined earlier in this file
*/

/*
** AQL thread status; 0 is OK (so that 'if (status)' means an error)
*/
#define AQL_OK		0
#define AQL_YIELD	1
#define AQL_ERRRUN	2
#define AQL_ERRSYNTAX	3
#define AQL_ERRMEM	4
#define AQL_ERRERR	5

typedef struct aql_Debug aql_Debug;  /* activation record */

/* Functions to be called by the debugger in specific events */
typedef void (*aql_Hook) (aql_State *L, aql_Debug *ar);

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

#endif /* astate_h */ 