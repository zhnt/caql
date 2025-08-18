/*
** $Id: alimits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** See Copyright Notice in aql.h
*/

#ifndef alimits_h
#define alimits_h

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/*
** 'aql_umem' and 'aql_mem' are unsigned/signed integers big enough to count
** the total memory used by AQL (in bytes). Usually, 'size_t' and
** 'ptrdiff_t' should work, but we use 'long' for 16-bit machines.
*/
#if defined(AQLAI_MEM)		/* { external definitions? */
typedef AQLAI_UMEM aql_umem;
typedef AQLAI_MEM aql_mem;
#elif AQLAI_IS32INT	/* }{ */
typedef size_t aql_umem;
typedef ptrdiff_t aql_mem;
#else  /* 16-bit ints */	/* }{ */
typedef unsigned long aql_umem;
typedef long aql_mem;
#endif				/* } */

/* Note: Compatibility types defined after aql_uint32 definition */

/* chars used as small naturals (so that 'char' is reserved for characters) */
typedef unsigned char aql_byte;
typedef signed char aql_sbyte;

/* maximum value for size_t */
#define MAX_SIZET	((size_t)(~(size_t)0))

/* maximum size visible for AQL (must be representable in a aql_Integer) */
#define MAX_SIZE	(sizeof(size_t) < sizeof(aql_Integer) ? MAX_SIZET \
                          : (size_t)(AQL_MAXINTEGER))

#define MAX_AQLMEM	((aql_umem)(~(aql_umem)0))
#define MAX_AQLLMEM	((aql_mem)(MAX_AQLMEM >> 1))

#define MAX_INT		INT_MAX  /* maximum value of an int */

/*
** floor of the log2 of the maximum signed value for integral type 't'.
** (That is, maximum 'n' such that '2^n' fits in the given signed type.)
*/
#define log2maxs(t)	(sizeof(t) * 8 - 2)

/*
** test whether an unsigned value is a power of 2 (or zero)
*/
#define ispow2(x)	(((x) & ((x) - 1)) == 0)

/* number of chars of a literal string without the ending \0 */
#define LL(x)   (sizeof(x)/sizeof(char) - 1)

/*
** conversion of pointer to unsigned integer: this is for hashing only;
** there is no problem if the integer cannot hold the whole pointer
** value
*/
#if !defined(AQL_USE_C89) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 199901L
#if defined(UINTPTR_MAX)  /* even in C99 this type is optional */
#define AQL_P2I	uintptr_t
#else  /* no 'intptr'? */
#define AQL_P2I	uintmax_t  /* use the largest available integer */
#endif
#else  /* C89 option */
#define AQL_P2I	size_t
#endif

#define point2uint(p)	((unsigned int)((AQL_P2I)(p) & UINT_MAX))

/*
** Internal assertions for in-house debugging
*/
#if defined AQLAI_ASSERT
#undef NDEBUG
#include <assert.h>
#define aql_assert(c)           assert(c)
#endif

#if defined(aql_assert)
#define check_exp(c,e)		(aql_assert(c), (e))
/* to avoid problems with conditions too long */
#define aql_longassert(c)	((c) ? (void)0 : aql_assert(0))
#else
#define aql_assert(c)		((void)0)
#define check_exp(c,e)		(e)
#define aql_longassert(c)	((void)0)
#endif

/*
** assertion for checking API calls
*/
#if !defined(aqlai_apicheck)
#define aqlai_apicheck(l,e)	((void)l, aql_assert(e))
#endif

#define api_check(l,e,msg)	aqlai_apicheck(l,(e) && msg)

/* macro to avoid warnings about unused variables */
#if !defined(UNUSED)
#define UNUSED(x)	((void)(x))
#endif

/* type casts (a macro highlights casts in the code) */
#define cast(t, exp)	((t)(exp))

#define cast_void(i)	cast(void, (i))
#define cast_voidp(i)	cast(void *, (i))
#define cast_num(i)	cast(aql_Number, (i))
#define cast_int(i)	cast(int, (i))
#define cast_uint(i)	cast(unsigned int, (i))
#define cast_byte(i)	cast(aql_byte, (i))
#define cast_uchar(i)	cast(unsigned char, (i))
#define cast_char(i)	cast(char, (i))
#define cast_charp(i)	cast(char *, (i))
#define cast_sizet(i)	cast(size_t, (i))

/* cast a signed aql_Integer to aql_Unsigned */
#if !defined(aql_castS2U)
#define aql_castS2U(i)	((aql_Unsigned)(i))
#endif

/*
** cast a aql_Unsigned to a signed aql_Integer; this cast is
** not strict ISO C, but two-complement architectures should
** work fine.
*/
#if !defined(aql_castU2S)
#define aql_castU2S(i)	((aql_Integer)(i))
#endif

/* Legacy aliases for compatibility */
#define l_castS2U(i)    aql_castS2U(i)
#define l_castU2S(i)    aql_castU2S(i)

/*
** non-return type
*/
#if !defined(aql_noret)

#if defined(__GNUC__)
#define aql_noret		void __attribute__((noreturn))
#define l_noret			void __attribute__((noreturn))
#elif defined(_MSC_VER) && _MSC_VER >= 1200
#define aql_noret		void __declspec(noreturn)
#define l_noret			void __declspec(noreturn)
#else
#define aql_noret		void
#define l_noret			void
#endif

#endif

/*
** Inline functions
*/
#if !defined(AQL_USE_C89)
#define aql_inline	inline
#define l_inline	inline
#elif defined(__GNUC__)
#define aql_inline	__inline__
#define l_inline	__inline__
#else
#define aql_inline	/* empty */
#define l_inline	/* empty */
#endif

#define aql_sinline	static aql_inline
#define l_sinline	static l_inline

/*
** Branch prediction hints (GCC extension)
*/
#if defined(__GNUC__)
#define l_likely(c)	__builtin_expect(!!(c), 1)
#define l_unlikely(c)	__builtin_expect(!!(c), 0)
#else
#define l_likely(c)	(c)
#define l_unlikely(c)	(c)
#endif

/*
** type for virtual-machine instructions;
** must be an unsigned with (at least) 4 bytes (see details in aopcodes.h)
*/
#if AQLAI_IS32INT
typedef unsigned int aql_uint32;
#else
typedef unsigned long aql_uint32;
#endif

typedef aql_uint32 Instruction;

/* Compatibility types for legacy code */
typedef aql_umem lu_mem;
typedef aql_mem l_mem;
typedef aql_uint32 l_uint32;

/*
** Maximum length for short strings, that is, strings that are
** internalized. (Cannot be smaller than reserved words or tags for
** metamethods, as these strings must be internalized;
** #("function") = 8, #("__newindex") = 10.)
*/
#if !defined(AQLAI_MAXSHORTLEN)
#define AQLAI_MAXSHORTLEN	40
#endif

/*
** Initial size for the string table (must be power of 2).
*/
#if !defined(MINSTRTABSIZE)
#define MINSTRTABSIZE	128
#endif

/*
** Size of cache for strings in the API. 'N' is the number of
** sets (better be a prime) and "M" is the size of each set (M == 1
** makes a direct cache.)
*/
#if !defined(STRCACHE_N)
#define STRCACHE_N		53
#define STRCACHE_M		2
#endif

/*
** Metamethod constants for arithmetic operations
*/
typedef enum {
    TM_INDEX = 0,    /* __index */
    TM_NEWINDEX,     /* __newindex */
    TM_GC,           /* __gc */
    TM_MODE,         /* __mode */
    TM_LEN,          /* __len */
    TM_EQ,           /* __eq */
    TM_ADD,          /* __add */
    TM_SUB,          /* __sub */
    TM_MUL,          /* __mul */
    TM_MOD,          /* __mod */
    TM_POW,          /* __pow */
    TM_DIV,          /* __div */
    TM_IDIV,         /* __idiv */
    TM_BAND,         /* __band */
    TM_BOR,          /* __bor */
    TM_BXOR,         /* __bxor */
    TM_SHL,          /* __shl */
    TM_SHR,          /* __shr */
    TM_UNM,          /* __unm */
    TM_BNOT,         /* __bnot */
    TM_LT,           /* __lt */
    TM_LE,           /* __le */
    TM_CONCAT,       /* __concat */
    TM_CALL,         /* __call */
    TM_CLOSE,        /* __close */
    TM_N             /* number of metamethods */
} TMethods;

/*
** Integer operations for VM
*/
#define intop(op,v1,v2) aql_castU2S(aql_castS2U(v1) op aql_castS2U(v2))

/*
** Stack size
*/
#if !defined(AQLAI_MAXSTACK)
#define AQLAI_MAXSTACK		1000000
#endif

#define AQL_MAXSTACK		AQLAI_MAXSTACK

/*
** AQL stack (for C-to-AQL calls)
*/
#if !defined(AQLAI_MAXCCALLS)
#define AQLAI_MAXCCALLS		200
#endif

/*
** Garbage collection parameters
*/
#if !defined(AQLAI_GCPAUSE)
#define AQLAI_GCPAUSE	200  /* 200% */
#endif

#if !defined(AQLAI_GCMUL)
#define AQLAI_GCMUL	100 /* GC runs 'twice the speed' of memory allocation */
#endif

#if !defined(AQLAI_GCSTEPSIZE)
#define AQLAI_GCSTEPSIZE	13
#endif

/*
** Numeric comparison macros
*/
#if !defined(aql_numadd)
#define aql_numadd(L,a,b)      ((a)+(b))
#define aql_numsub(L,a,b)      ((a)-(b))
#define aql_nummul(L,a,b)      ((a)*(b))
#define aql_numdiv(L,a,b)      ((a)/(b))
#define aql_numunm(L,a)        (-(a))
#define aql_numeq(a,b)         ((a)==(b))
#define aql_numlt(a,b)         ((a)<(b))
#define aql_numle(a,b)         ((a)<=(b))
#define aql_numgt(a,b)         ((a)>(b))
#define aql_numge(a,b)         ((a)>=(b))
#define aql_numisnan(a)        (!aql_numeq((a), (a)))
#endif

#endif