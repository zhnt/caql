/*
** $Id: aobject.h $
** Type definitions for AQL objects
** See Copyright Notice in aql.h
*/

#ifndef aobject_h
#define aobject_h

#include <stdarg.h>
#include "aql.h"
#include "aconf.h"
#include "aopcodes.h"

/*
** Extra internal types for collectable non-values
** (Basic types AQL_TNIL through AQL_TVECTOR are defined in aql.h)
*/
#define AQL_TUPVAL	AQL_NUMTYPES  /* upvalues */
#define AQL_TPROTO	(AQL_NUMTYPES+1)  /* function prototypes */
#define AQL_TDEADKEY	(AQL_NUMTYPES+2)  /* removed keys in tables */

/*
** number of all possible types (including AQL_TNONE but excluding DEADKEY)
*/
#define AQL_TOTALTYPES		(AQL_TPROTO + 2)

/*
** tags for Tagged Values have the following use of bits:
** bits 0-3: actual tag (a AQL_T* constant)
** bits 4-5: variant bits
** bit 6: whether value is collectable
*/

/* add variant bits to a type */
#define makevariant(t,v)	((t) | ((v) << 4))

/*
** Union of all AQL values
*/
typedef union Value {
  struct GCObject *gc;    /* collectable objects */
  void *p;         /* light userdata */
  aql_CFunction f; /* light C functions */
  aql_Integer i;   /* integer numbers */
  aql_Number n;    /* float numbers */
  /* not used, but may avoid warnings for uninitialized value */
  aql_byte ub;
} Value;

/*
** Tagged Values. This is the basic representation of values in AQL:
** an actual value plus a tag with its type.
*/
#define TValuefields	Value value_; aql_byte tt_

typedef struct TValue {
  TValuefields;
} TValue;

#define val_(o)		((o)->value_)
#define valraw(o)	(val_(o))

/* raw type tag of a TValue */
#define rawtt(o)	((o)->tt_)

/* tag with no variants (bits 0-3) */
#define novariant(t)	((t) & 0x0F)

/* type tag of a TValue (bits 0-3 for tags + variant bits 4-5) */
#define withvariant(t)	((t) & 0x3F)
#define ttypetag(o)	withvariant(rawtt(o))

/* type of a TValue */
#define ttype(o)	(novariant(rawtt(o)))

/* Macros to test type */
#define checktag(o,t)		(rawtt(o) == (t))
#define checktype(o,t)		(ttype(o) == (t))

/* Macros for internal tests */
/* collectable object has the same tag as the original value */
#define righttt(obj)		(ttypetag(obj) == gcvalue(obj)->tt)

/*
** Any value being manipulated by the program either is non
** collectable, or the collectable object has the right tag
** and it is not dead. The option 'L == NULL' allows other
** macros using this one to be used where L is not available.
*/
#define checkliveness(L,obj) \
	((void)L, aql_longassert(!iscollectable(obj) || \
		(righttt(obj) && (L == NULL || !isdead(G(L),gcvalue(obj))))))

/* Macros to set values */
/* set a value's tag */
#define settt_(o,t)	((o)->tt_=(t))

/* main macro to copy values (from 'obj2' to 'obj1') */
#define setobj(L,obj1,obj2) \
	{ TValue *io1=(obj1); const TValue *io2=(obj2); \
          io1->value_ = io2->value_; settt_(io1, io2->tt_); \
	  checkliveness(L,io1); aql_assert(!isnonstrictnil(io1)); }

/*
** Different types of assignments, according to source and destination.
** (They are mostly equal now, but may be different in the future.)
*/
/* from stack to stack */
#define setobjs2s(L,o1,o2)	setobj(L,s2v(o1),s2v(o2))
/* to stack (not from same stack) */
#define setobj2s(L,o1,o2)	setobj(L,s2v(o1),o2)
/* from table to same table */
#define setobjt2t	setobj
/* to new object */
#define setobj2n	setobj
/* to table */
#define setobj2t	setobj

/*
** Entries in an AQL stack. Field 'tbclist' forms a list of all
** to-be-closed variables active in this stack. Dummy entries are
** used when the distance between two tbc variables does not fit
** in an unsigned short. They are represented by delta==0, and
** their real delta is always the maximum value that fits in
** that field.
*/
typedef union StackValue {
  TValue val;
  struct {
    TValuefields;
    unsigned short delta;
  } tbclist;
} StackValue;

/* index to stack elements */
typedef StackValue *StkId;

/*
** When reallocating the stack, change all pointers to the stack into
** proper offsets.
*/
typedef union {
  StkId p;  /* actual pointer */
  ptrdiff_t offset;  /* used while the stack is being reallocated */
} StkIdRel;

/* convert a 'StackValue' to a 'TValue' */
#define s2v(o)	(&(o)->val)

/*
** {==================================================================
** Nil
** ===================================================================
*/
/* Standard nil */
#define AQL_VNIL	makevariant(AQL_TNIL, 0)
/* Empty slot (which might be different from a slot containing nil) */
#define AQL_VEMPTY	makevariant(AQL_TNIL, 1)
/* Value returned for a key not found in a table (absent key) */
#define AQL_VABSTKEY	makevariant(AQL_TNIL, 2)

/* macro to test for (any kind of) nil */
#define ttisnil(v)		checktype((v), AQL_TNIL)
/* macro to test for a standard nil */
#define ttisstrictnil(o)	checktag((o), AQL_VNIL)

#define setnilvalue(obj) settt_(obj, AQL_VNIL)

#define isabstkey(v)		checktag((v), AQL_VABSTKEY)

/*
** macro to detect non-standard nils (used only in assertions)
*/
#define isnonstrictnil(v)	(ttisnil(v) && !ttisstrictnil(v))

/*
** By default, entries with any kind of nil are considered empty.
** (In any definition, values associated with absent keys must also
** be accepted as empty.)
*/
#define isempty(v)		ttisnil(v)

/* macro defining a value corresponding to an absent key */
#define ABSTKEYCONSTANT		{NULL}, AQL_VABSTKEY

/* mark an entry as empty */
#define setempty(v)		settt_(v, AQL_VEMPTY)

/* }================================================================== */

/*
** {==================================================================
** Booleans
** ===================================================================
*/
#define AQL_VFALSE	makevariant(AQL_TBOOLEAN, 0)
#define AQL_VTRUE	makevariant(AQL_TBOOLEAN, 1)

#define ttisboolean(o)		checktype((o), AQL_TBOOLEAN)
#define ttisfalse(o)		checktag((o), AQL_VFALSE)
#define ttistrue(o)		checktag((o), AQL_VTRUE)

#define l_isfalse(o)	(ttisfalse(o) || ttisnil(o))

#define bvalue(o)		check_exp(ttisboolean(o), val_(o).ub)

#define setbfvalue(obj)		settt_(obj, AQL_VFALSE)
#define setbtvalue(obj)		settt_(obj, AQL_VTRUE)

#define setbvalue(obj,x) \
  { TValue *io=(obj); val_(io).ub=(x); settt_(io, (x) ? AQL_VTRUE : AQL_VFALSE); }

/* }================================================================== */

/*
** {==================================================================
** Threads
** ===================================================================
*/
#define AQL_VTHREAD		makevariant(AQL_TTHREAD, 0)

#define ttisthread(o)		checktag((o), ctb(AQL_VTHREAD))

#define thvalue(o)	check_exp(ttisthread(o), gco2th(val_(o).gc))

#define setthvalue(L,obj,x) \
  { TValue *io = (obj); aql_State *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(AQL_VTHREAD)); \
    checkliveness(L,io); }

#define setthvalue2s(L,o,t)	setthvalue(L,s2v(o),t)

/* }================================================================== */

/*
** {==================================================================
** Collectable Objects
** ===================================================================
*/
/*
** Common Header for all collectable objects (in macro form, to be
** included in other objects)
*/
#define CommonHeader	struct GCObject *next; aql_byte tt_; aql_byte marked

/* Common type for all collectable objects */
typedef struct GCObject {
  CommonHeader;
} GCObject;

/* Bit mark for collectable types */
#define BIT_ISCOLLECTABLE	(1 << 6)

#define iscollectable(o)	(rawtt(o) & BIT_ISCOLLECTABLE)

/* mark a tag as collectable */
#define ctb(t)			((t) | BIT_ISCOLLECTABLE)

#define gcvalue(o)	check_exp(iscollectable(o), val_(o).gc)

#define gcvalueraw(v)	((v).gc)

#define setgcovalue(L,obj,x) \
  { TValue *io = (obj); GCObject *i_g=(x); \
    val_(io).gc = i_g; settt_(io, ctb(i_g->tt)); }

/* }================================================================== */

/*
** {==================================================================
** Numbers
** ===================================================================
*/
/* Variant tags for numbers */
#define AQL_VNUMINT	makevariant(AQL_TNUMBER, 0)  /* integer numbers */
#define AQL_VNUMFLT	makevariant(AQL_TNUMBER, 1)  /* float numbers */

#define ttisnumber(o)		checktype((o), AQL_TNUMBER)
#define ttisfloat(o)		checktag((o), AQL_VNUMFLT)
#define ttisinteger(o)		checktag((o), AQL_VNUMINT)

#define nvalue(o)	check_exp(ttisnumber(o), \
	(ttisinteger(o) ? cast_num(ivalue(o)) : fltvalue(o)))
#define fltvalue(o)	check_exp(ttisfloat(o), val_(o).n)
#define ivalue(o)	check_exp(ttisinteger(o), val_(o).i)

#define fltvalueraw(v)	((v).n)
#define ivalueraw(v)	((v).i)

#define setfltvalue(obj,x) \
  { TValue *io=(obj); val_(io).n=(x); settt_(io, AQL_VNUMFLT); }

#define chgfltvalue(obj,x) \
  { TValue *io=(obj); aql_assert(ttisfloat(io)); val_(io).n=(x); }

#define setivalue(obj,x) \
  { TValue *io=(obj); val_(io).i=(x); settt_(io, AQL_VNUMINT); }

#define chgivalue(obj,x) \
  { TValue *io=(obj); aql_assert(ttisinteger(io)); val_(io).i=(x); }

/* }================================================================== */

/*
** {==================================================================
** Strings
** ===================================================================
*/
/* Variant tags for strings */
#define AQL_VSHRSTR	makevariant(AQL_TSTRING, 0)  /* short strings */
#define AQL_VLNGSTR	makevariant(AQL_TSTRING, 1)  /* long strings */

#define ttisstring(o)		checktype((o), AQL_TSTRING)
#define ttisshrstring(o)	checktag((o), ctb(AQL_VSHRSTR))
#define ttislngstring(o)	checktag((o), ctb(AQL_VLNGSTR))

#define tsvalueraw(v)	(gco2ts((v).gc))

#define tsvalue(o)	check_exp(ttisstring(o), gco2ts(val_(o).gc))

#define setsvalue(L,obj,x) \
  { TValue *io = (obj); TString *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(x_->tt_)); \
    checkliveness(L,io); }

/* set a string to the stack */
#define setsvalue2s(L,o,s)	setsvalue(L,s2v(o),s)

/* set a string to a new object */
#define setsvalue2n	setsvalue

/*
** Header for a string value.
*/
typedef struct TString {
  CommonHeader;
  aql_byte extra;  /* reserved words for short strings; "has hash" for longs */
  aql_byte shrlen;  /* length for short strings, 0xFF for long strings */
  unsigned int hash;
  union {
    size_t lnglen;  /* length for long strings */
    struct TString *hnext;  /* linked list for hash table */
  } u;
  char contents[1];
} TString;

/*
** Get the actual string (array of bytes) from a 'TString'. (Generic
** version and specialized versions for long and short strings.)
*/
#define getstr(ts)	((ts)->contents)
#define getlngstr(ts)	check_exp((ts)->shrlen == 0xFF, (ts)->contents)
#define getshrstr(ts)	check_exp((ts)->shrlen != 0xFF, (ts)->contents)

/* get string value from TValue */
#define svalue(o)       getstr(tsvalue(o))

/* get string length from TValue */  
#define vslen(o)        tsslen(tsvalue(o))

/* get string length from 'TString *s' */
#define tsslen(s)  \
	((s)->shrlen != 0xFF ? (s)->shrlen : (s)->u.lnglen)

/* }================================================================== */

/*
** {==================================================================
** Userdata
** ===================================================================
*/
/*
** Light userdata should be a variant of userdata, but for compatibility
** reasons they are also different types.
*/
#define AQL_VLIGHTUSERDATA	makevariant(AQL_TLIGHTUSERDATA, 0)
#define AQL_VUSERDATA		makevariant(AQL_TUSERDATA, 0)

#define ttislightuserdata(o)	checktag((o), AQL_VLIGHTUSERDATA)
#define ttisfulluserdata(o)	checktag((o), ctb(AQL_VUSERDATA))

#define pvalue(o)	check_exp(ttislightuserdata(o), val_(o).p)
#define uvalue(o)	check_exp(ttisfulluserdata(o), gco2u(val_(o).gc))

#define pvalueraw(v)	((v).p)

#define setpvalue(obj,x) \
  { TValue *io=(obj); val_(io).p=(x); settt_(io, AQL_VLIGHTUSERDATA); }

#define setuvalue(L,obj,x) \
  { TValue *io = (obj); Udata *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(AQL_VUSERDATA)); \
    checkliveness(L,io); }

/* Ensures that addresses after this type are always fully aligned. */
typedef union UValue {
  TValue uv;
  AQL_MAXALIGN;  /* ensures maximum alignment for udata bytes */
} UValue;

/*
** Header for userdata with user values;
** memory area follows the end of this structure.
*/
typedef struct Udata {
  CommonHeader;
  unsigned short nuvalue;  /* number of user values */
  size_t len;  /* number of bytes */
  struct Table *metatable;
  GCObject *gclist;
  UValue uv[1];  /* user values */
} Udata;

/*
** Header for userdata with no user values. These userdata do not need
** to be gray during GC, and therefore do not need a 'gclist' field.
** To simplify, the code always use 'Udata' for both kinds of userdata,
** making sure it never accesses 'gclist' on userdata with no user values.
** This structure here is used only to compute the correct size for
** this representation. (The 'bindata' field in its end ensures correct
** alignment for binary data following this header.)
*/
typedef struct Udata0 {
  CommonHeader;
  unsigned short nuvalue;  /* number of user values */
  size_t len;  /* number of bytes */
  struct Table *metatable;
  union {AQL_MAXALIGN;} bindata;
} Udata0;

/* compute the offset of the memory area of a userdata */
#define udatamemoffset(nuv) \
	((nuv) == 0 ? offsetof(Udata0, bindata)  \
                    : offsetof(Udata, uv) + (sizeof(UValue) * (nuv)))

/* get the address of the memory block inside 'Udata' */
#define getudatamem(u)	(cast_charp(u) + udatamemoffset((u)->nuvalue))

/* compute the size of a userdata */
#define sizeudata(nuv,nb)	(udatamemoffset(nuv) + (nb))

/* }================================================================== */

/*
** {==================================================================
** Prototypes
** ===================================================================
*/
#define AQL_VPROTO	makevariant(AQL_TPROTO, 0)

/*
** Description of an upvalue for function prototypes
*/
typedef struct Upvaldesc {
  TString *name;  /* upvalue name (for debug information) */
  aql_byte instack;  /* whether it is in stack (register) */
  aql_byte idx;  /* index of upvalue (in stack or in outer function's list) */
  aql_byte kind;  /* kind of corresponding variable */
} Upvaldesc;

/*
** Description of a local variable for function prototypes
** (used for debug information)
*/
typedef struct LocVar {
  TString *varname;
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} LocVar;

/*
** Associates the absolute line source for a given instruction ('pc').
** The array 'lineinfo' gives, for each instruction, the difference in
** lines from the previous instruction. When that difference does not
** fit into a byte, AQL saves the absolute line for that instruction.
** (AQL also saves the absolute line periodically, to speed up the
** computation of a line number: we can use binary search in the
** absolute-line array, but we must traverse the 'lineinfo' array
** linearly to compute a line.)
*/
typedef struct AbsLineInfo {
  int pc;
  int line;
} AbsLineInfo;

/*
** Function Prototypes
*/
typedef struct Proto {
  CommonHeader;
  aql_byte numparams;  /* number of fixed (named) parameters */
  aql_byte is_vararg;
  aql_byte maxstacksize;  /* number of registers needed by this function */
  int sizeupvalues;  /* size of 'upvalues' */
  int sizek;  /* size of 'k' */
  int sizecode;
  int sizelineinfo;
  int sizep;  /* size of 'p' */
  int sizelocvars;
  int sizeabslineinfo;  /* size of 'abslineinfo' */
  int linedefined;  /* debug information  */
  int lastlinedefined;  /* debug information  */
  TValue *k;  /* constants used by the function */
  Instruction *code;  /* opcodes */
  struct Proto **p;  /* functions defined inside the function */
  Upvaldesc *upvalues;  /* upvalue information */
  aql_byte *lineinfo;  /* information about source lines (debug information) */
  AbsLineInfo *abslineinfo;  /* idem */
  LocVar *locvars;  /* information about local variables (debug information) */
  TString  *source;  /* used for debug information */
  GCObject *gclist;
} Proto;

/* }================================================================== */

/*
** {==================================================================
** Functions
** ===================================================================
*/
#define AQL_VUPVAL	makevariant(AQL_TUPVAL, 0)

/* Variant tags for functions */
#define AQL_VLCL	makevariant(AQL_TFUNCTION, 0)  /* AQL closure */
#define AQL_VLCF	makevariant(AQL_TFUNCTION, 1)  /* light C function */
#define AQL_VCCL	makevariant(AQL_TFUNCTION, 2)  /* C closure */

#define ttisfunction(o)		checktype(o, AQL_TFUNCTION)
#define ttisLclosure(o)		checktag((o), ctb(AQL_VLCL))
#define ttislcf(o)		checktag((o), AQL_VLCF)
#define ttisCclosure(o)		checktag((o), ctb(AQL_VCCL))
#define ttisclosure(o)         (ttisLclosure(o) || ttisCclosure(o))

#define isLfunction(o)	ttisLclosure(o)

#define clvalue(o)	check_exp(ttisclosure(o), gco2cl(val_(o).gc))
#define clLvalue(o)	check_exp(ttisLclosure(o), gco2lcl(val_(o).gc))
#define fvalue(o)	check_exp(ttislcf(o), val_(o).f)
#define clCvalue(o)	check_exp(ttisCclosure(o), gco2ccl(val_(o).gc))

#define fvalueraw(v)	((v).f)

#define setclLvalue(L,obj,x) \
  { TValue *io = (obj); LClosure *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(AQL_VLCL)); \
    checkliveness(L,io); }

#define setclLvalue2s(L,o,cl)	setclLvalue(L,s2v(o),cl)

#define setfvalue(obj,x) \
  { TValue *io=(obj); val_(io).f=(x); settt_(io, AQL_VLCF); }

#define setclCvalue(L,obj,x) \
  { TValue *io = (obj); CClosure *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(AQL_VCCL)); \
    checkliveness(L,io); }

/*
** Upvalues for AQL closures
*/
typedef struct UpVal {
  CommonHeader;
  union {
    TValue *p;  /* points to stack or to its own value */
    ptrdiff_t offset;  /* used while the stack is being reallocated */
  } v;
  union {
    struct {  /* (when open) */
      struct UpVal *next;  /* linked list */
      struct UpVal **previous;
    } open;
    TValue value;  /* the value (when closed) */
  } u;
} UpVal;

#define ClosureHeader \
	CommonHeader; aql_byte nupvalues; GCObject *gclist

typedef struct CClosure {
  ClosureHeader;
  aql_CFunction f;
  TValue upvalue[1];  /* list of upvalues */
} CClosure;

typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;
  UpVal *upvals[1];  /* list of upvalues */
} LClosure;

typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;

#define getproto(o)	(clLvalue(o)->p)

/* }================================================================== */

/*
** {==================================================================
** AQL Container Types (Container Separation Architecture)
** ===================================================================
*/

/* Forward declarations for container types */
typedef struct Array Array;
typedef struct Slice Slice;
typedef struct Dict Dict;
typedef struct Vector Vector;
typedef struct RangeObject RangeObject;

/* Variant tags for AQL containers */
#define AQL_VARRAY	makevariant(AQL_TARRAY, 0)
#define AQL_VSLICE	makevariant(AQL_TSLICE, 0)
#define AQL_VDICT	makevariant(AQL_TDICT, 0)
#define AQL_VVECTOR	makevariant(AQL_TVECTOR, 0)
#define AQL_VRANGE	makevariant(AQL_TRANGE, 0)

/* Container type tests */
#define ttisarray(o)		checktag((o), ctb(AQL_VARRAY))
#define ttisslice(o)		checktag((o), ctb(AQL_VSLICE))
#define ttisdict(o)		checktag((o), ctb(AQL_VDICT))
#define ttisvector(o)		checktag((o), ctb(AQL_VVECTOR))
#define ttisrange(o)		checktag((o), ctb(AQL_VRANGE))

/* Container value accessors */
#define arrayvalue(o)	check_exp(ttisarray(o), gco2array(val_(o).gc))
#define slicevalue(o)	check_exp(ttisslice(o), gco2slice(val_(o).gc))
#define dictvalue(o)	check_exp(ttisdict(o), gco2dict(val_(o).gc))
#define vectorvalue(o)	check_exp(ttisvector(o), gco2vector(val_(o).gc))
#define rangevalue(o)	check_exp(ttisrange(o), gco2range(val_(o).gc))

/* Alternative names for compatibility */
#define arrvalue(o)     arrayvalue(o)
#define vecvalue(o)     vectorvalue(o)

/* Container setters */
#define setarrayvalue(L,obj,x) \
  { TValue *io = (obj); Array *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(AQL_VARRAY)); \
    checkliveness(L,io); }

#define setslicevalue(L,obj,x) \
  { TValue *io = (obj); Slice *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(AQL_VSLICE)); \
    checkliveness(L,io); }

#define setdictvalue(L,obj,x) \
  { TValue *io = (obj); Dict *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(AQL_VDICT)); \
    checkliveness(L,io); }

#define setvectorvalue(L,obj,x) \
  { TValue *io = (obj); Vector *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(AQL_VVECTOR)); \
    checkliveness(L,io); }

#define setrangevalue(L,obj,x) \
  { TValue *io = (obj); RangeObject *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(AQL_VRANGE)); \
    checkliveness(L,io); }

/* }================================================================== */

/*
** {==================================================================
** Legacy Table Support (for compatibility during transition)
** ===================================================================
*/
#define AQL_VTABLE	makevariant(AQL_TTABLE, 0)

#define ttistable(o)		checktag((o), ctb(AQL_VTABLE))

/* Forward declaration for legacy table */
typedef struct Table Table;

#define hvalue(o)	check_exp(ttistable(o), gco2t(val_(o).gc))

#define sethvalue(L,obj,x) \
  { TValue *io = (obj); Table *x_ = (x); \
    val_(io).gc = obj2gco(x_); settt_(io, ctb(AQL_VTABLE)); \
    checkliveness(L,io); }

/* }================================================================== */

/*
** {==================================================================
** GC Object Type Conversions
** ===================================================================
*/

/* Conversion macros (avoiding the ones in astate.h that cause conflicts) */
#define gco2ts(o)  check_exp(novariant((o)->tt) == AQL_TSTRING, (TString *)(o))
#define gco2u(o)   check_exp((o)->tt == AQL_TUSERDATA, (Udata *)(o))
#define gco2lcl(o) check_exp((o)->tt == AQL_VLCL, (LClosure *)(o))
#define gco2ccl(o) check_exp((o)->tt == AQL_VCCL, (CClosure *)(o))
#define gco2cl(o)  check_exp(novariant((o)->tt) == AQL_TFUNCTION, (Closure *)(o))
#define gco2t(o)   check_exp((o)->tt == AQL_TTABLE, (Table *)(o))
#define gco2p(o)   check_exp((o)->tt == AQL_TPROTO, (Proto *)(o))
#define gco2th(o)  check_exp((o)->tt == AQL_TTHREAD, (aql_State *)(o))
#define gco2upv(o) check_exp((o)->tt == AQL_TUPVAL, (UpVal *)(o))

/* AQL container conversions */
#define gco2array(o)  check_exp((o)->tt == AQL_TARRAY, (Array *)(o))
#define gco2slice(o)  check_exp((o)->tt == AQL_TSLICE, (Slice *)(o))
#define gco2dict(o)   check_exp((o)->tt == AQL_TDICT, (Dict *)(o))
#define gco2vector(o) check_exp((o)->tt == AQL_TVECTOR, (Vector *)(o))
#define gco2range(o)  check_exp((o)->tt == AQL_TRANGE, (RangeObject *)(o))

/* Note: cast_u is defined in aconf.h */

/* Convert object to GCObject */
#define obj2gco(v) \
	check_exp(novariant((v)->tt) < AQL_NUMTYPES, (&(((union GCUnion *)(v))->gc)))

/* 
** Union for all GC objects 
** Note: Container-specific fields are handled in their respective headers
** to avoid circular dependencies
*/
union GCUnion {
  GCObject gc;  /* common header */
  struct TString ts;
  struct Udata u;
  union Closure cl;
  struct Proto p;
  struct UpVal upv;
  /* Container types and legacy table support handled separately */
};

/* }================================================================== */

/*
** size of buffer for 'aqlO_utf8esc' function
*/
#define UTF8BUFFSZ	8

/*
** Function declarations
*/
AQL_API int aqlO_utf8esc (char *buff, unsigned long x);
AQL_API int aqlO_ceillog2 (unsigned int x);
AQL_API int aqlO_rawarith (aql_State *L, int op, const TValue *p1,
                             const TValue *p2, TValue *res);
AQL_API void aqlO_arith (aql_State *L, int op, const TValue *p1,
                           const TValue *p2, StkId res);
AQL_API size_t aqlO_str2num (const char *s, TValue *o);
AQL_API int aqlO_hexavalue (int c);
AQL_API void aqlO_tostring (aql_State *L, TValue *obj);
AQL_API const char *aqlO_pushvfstring (aql_State *L, const char *fmt,
                                                       va_list argp);
AQL_API const char *aqlO_pushfstring (aql_State *L, const char *fmt, ...);
AQL_API void aqlO_chunkid (char *out, const char *source, size_t srclen);
AQL_API const char *aqlO_typename (const TValue *o);

/*
** VM arithmetic function declarations (placeholders for missing functions)
*/
AQL_API aql_Integer aqlV_mod (aql_State *L, aql_Integer m, aql_Integer n);
AQL_API aql_Integer aqlV_idiv (aql_State *L, aql_Integer m, aql_Integer n);
AQL_API aql_Integer aqlV_shiftl (aql_Integer x, aql_Integer y);
AQL_API aql_Integer aqlV_shiftr (aql_Integer x, aql_Integer y);
AQL_API aql_Number aqlV_modf (aql_State *L, aql_Number m, aql_Number n);

/*
** Metamethod types and constants (placeholder)
*/
typedef int TMS;
#define TM_ADD   0

/*
** Metamethod function declarations (placeholder)
*/
AQL_API void aqlT_trybinTM (aql_State *L, const TValue *p1, const TValue *p2,
                            StkId res, TMS event);

/*
** VM function declarations (placeholder)
*/
AQL_API void aqlV_concat (aql_State *L, int total);
AQL_API void aqlG_runerror (aql_State *L, const char *fmt, ...);

/*
** Function call declarations (placeholder)
*/
AQL_API int aqlD_precall (aql_State *L, StkId func, int nResults);
AQL_API int aqlD_poscall (aql_State *L, struct CallInfo *ci, int nres);
AQL_API int aqlD_pretailcall (aql_State *L, struct CallInfo *ci, StkId func, int narg1, int delta);

/*
** String function declarations (placeholder)
*/
AQL_API TString *aqlStr_newlstr (aql_State *L, const char *str, size_t l);
AQL_API TString *aqlS_createlngstrobj (aql_State *L, size_t l);

/*
** Error handling function declarations (placeholder)
*/
AQL_API void aqlG_typeerror (aql_State *L, const TValue *o, const char *op);
AQL_API int aqlG_ordererror (aql_State *L, const TValue *p1, const TValue *p2);
AQL_API const char *aqlL_typename (aql_State *L, const TValue *o);

/*
** Forward declaration for GC
*/
typedef struct global_State global_State;

/*
** GC function declarations (placeholder)
*/
AQL_API int isdead (global_State *g, GCObject *o);

/*
** Note: G(L) macro is defined in astate.h
*/

#endif