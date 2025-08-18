/*
** $Id: aconf.h $
** Configuration file for AQL
** See Copyright Notice in aql.h
*/

#ifndef aconf_h
#define aconf_h

#include "alimits.h"

/*
** AQL_API is a mark for all core API functions and some key macros.
** AQL_LIB_API is a mark for library functions that are exported.
*/
#if defined(AQL_BUILD_AS_DLL)	/* { */

#if defined(AQL_CORE) || defined(AQL_LIB)	/* { */
#define AQL_API __declspec(dllexport)
#else						/* }{ */
#define AQL_API __declspec(dllimport)
#endif					/* } */

#else				/* }{ */

#define AQL_API		extern

#endif				/* } */

#define AQL_LIB_API	AQL_API

/*
** Extra space for memory allocations
*/
#define AQL_EXTRASPACE    (sizeof(void *))

/*
** Extra stack space for VM operations
*/
#define EXTRA_STACK   5

/*
** Integer type constants
*/
#define AQL_INT_INT		1
#define AQL_INT_LONG		2
#define AQL_INT_LONGLONG	3

/*
** Float type constants  
*/
#define AQL_FLOAT_FLOAT		1
#define AQL_FLOAT_DOUBLE	2
#define AQL_FLOAT_LONGDOUBLE	3

/*
** Integer type definitions
*/
#define AQL_INT_TYPE	AQL_INT_LONGLONG

#if defined(AQL_INT_TYPE)	/* { */

#if AQL_INT_TYPE == AQL_INT_INT		/* { int */
#define AQL_INTEGER		int
#define AQL_INTEGER_FRMLEN	""
#define AQLAI_UACINT		long

#elif AQL_INT_TYPE == AQL_INT_LONG	/* }{ long */
#define AQL_INTEGER		long
#define AQL_INTEGER_FRMLEN	"l"
#define AQLAI_UACINT		long

#elif AQL_INT_TYPE == AQL_INT_LONGLONG	/* }{ long long */
#define AQL_INTEGER		long long
#define AQL_INTEGER_FRMLEN	"ll"
#define AQLAI_UACINT		long long

#endif				/* } */

#endif				/* } */

/*
** Floating-point type definitions
*/
#define AQL_FLOAT_TYPE	AQL_FLOAT_DOUBLE

#if defined(AQL_FLOAT_TYPE)	/* { */

#if AQL_FLOAT_TYPE == AQL_FLOAT_FLOAT		/* { float */
#define AQL_NUMBER	float
#define AQLAI_UACNUMBER	double
#define AQL_NUMBER_FRMLEN	""
#define aql_str2number(s,p)	((AQL_NUMBER)strtof((s), (p)))
#define AQL_NUMBER_SCAN		"%f"
#define AQL_NUMBER_FMT_T	float

#elif AQL_FLOAT_TYPE == AQL_FLOAT_LONGDOUBLE	/* }{ long double */
#define AQL_NUMBER	long double
#define AQLAI_UACNUMBER	long double
#define AQL_NUMBER_FRMLEN	"L"
#define aql_str2number(s,p)	strtold((s), (p))
#define AQL_NUMBER_SCAN		"%Lg"
#define AQL_NUMBER_FMT_T	long double

#elif AQL_FLOAT_TYPE == AQL_FLOAT_DOUBLE	/* }{ double */
#define AQL_NUMBER	double
#define AQLAI_UACNUMBER	double
#define AQL_NUMBER_FRMLEN	""
#define aql_str2number(s,p)	strtod((s), (p))
#define AQL_NUMBER_SCAN		"%lf"
#define AQL_NUMBER_FMT_T	double

#endif					/* } */

#endif					/* } */

/*
** Other type definitions
*/
typedef AQL_INTEGER aql_Integer;
typedef AQL_NUMBER aql_Number;
typedef unsigned AQL_INTEGER aql_Unsigned;
typedef void* aql_KContext;
/* Note: l_signalT is defined in alimits.h */

/*
** Maximum integers for conversion
*/
#define AQL_MAXINTEGER		LLONG_MAX
#define AQL_MININTEGER		LLONG_MIN

/* Number of bits in aql_Integer */
#define AQL_INTEGER_BITS    (sizeof(aql_Integer) * 8)

/* Convert a number to an integer with rounding */
#define aql_numbertointeger(n,p) \
  ((n) >= (aql_Number)(AQL_MININTEGER) && \
   (n) < -(aql_Number)(AQL_MININTEGER) && \
      (*(p) = (aql_Integer)(n), 1))

/*
** Buffer size constants
*/
#if !defined(AQL_BUFFERSIZE)
#define AQL_BUFFERSIZE   ((int)(16 * sizeof(void*) * sizeof(aql_Number)))
#endif

#if !defined(AQL_IDSIZE)
#define AQL_IDSIZE	60  /* size of identifier strings */
#endif

#if !defined(AQL_IDXLEN)
#define AQL_IDXLEN	60  /* size of source location strings */
#endif

#if !defined(BUFVFS)
#define BUFVFS		200  /* size of buffer for string formatting */
#endif

/*
** Number formatting constants
*/
#define MAXNUMBER2STR	44  /* max length for number to string conversion */

/*
** String formatting function macros
*/
#define AQL_INTEGER_FMT_STR		"%" AQL_INTEGER_FRMLEN "d"
#define aql_integer2str(s,sz,n)  \
	((void)((sz) != 1), sprintf((s), AQL_INTEGER_FMT_STR, (AQLF_I_TYPE)(n)))

#define AQL_NUMBER_FMT_STR		"%" AQL_NUMBER_FRMLEN "g"
#define aql_number2str(s,sz,n)	\
	((void)((sz) != 1), sprintf((s), AQL_NUMBER_FMT_STR, (AQLF_N_TYPE)(n)))

#define aql_pointer2str(s,sz,p)	\
	((void)((sz) != 1), sprintf((s), "%p", (p)))

#define AQLF_I_TYPE	AQL_INTEGER
#define AQLF_N_TYPE	AQL_NUMBER

/*
** Types for va_arg
*/
typedef AQL_INTEGER l_uacInt;
typedef AQL_NUMBER l_uacNumber;

/*
** Hexadecimal number parsing
*/
#if !defined(aql_strx2number)
#define aql_strx2number(s,p)	strtod((s), (p))  /* fallback for hex */
#endif

/*
** Locale support
*/
#if !defined(aql_getlocaledecpoint)
#define aql_getlocaledecpoint()		(localeconv()->decimal_point[0])
#endif

/*
** Character classification
*/
#include <ctype.h>
#define lisdigit(c)	isdigit(c)
#define lisxdigit(c)	isxdigit(c)
#define lisspace(c)	isspace(c)
#define ltolower(c)	tolower(c)

/*
** String to number conversion limits
*/
#define L_MAXLENNUM	200  /* max length of a numeral */
#define MAXBY10		cast(aql_Unsigned, AQL_MAXINTEGER / 10)
#define MAXLASTD	cast_int(AQL_MAXINTEGER % 10)

/*
** Memory alignment
*/
#if !defined(AQL_MAXALIGN)
#define AQL_MAXALIGN aql_Number n; double u; void *s; aql_Integer i; long l
#endif

/*
** Memory allocation function type
*/
typedef void* (*aql_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);

/*
** Configuration functions (implemented in aconf.c)
*/
AQL_API void aql_detect_capabilities(void);
AQL_API int aql_is_aligned(void *ptr, size_t alignment);
AQL_API size_t aql_align_size(size_t size, size_t alignment);
AQL_API void aql_panic(const char *msg);

#endif