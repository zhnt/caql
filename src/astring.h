/*
** $Id: astring.h $
** String table (keeps all strings handled by AQL)
** See Copyright Notice in aql.h
*/

#ifndef astring_h
#define astring_h

#include "aobject.h"

/* Forward declarations to break circular dependency */
typedef struct aql_State aql_State;
typedef struct global_State global_State;

/*
** Hash for strings
*/
#define aqlS_hash(str) ((str)->hash)

/*
** Sizes for string table
*/
#if !defined(MINSTRTABSIZE)
#define MINSTRTABSIZE	128
#endif

/* String table structure is defined in astate.h */

/*
** String creation and management
*/
AQL_API TString *aqlStr_newlstr(aql_State *L, const char *str, size_t l);
AQL_API TString *aqlStr_new(aql_State *L, const char *str);
AQL_API TString *aqlStr_createlngstrobj(aql_State *L, size_t l);

/*
** String interning and hash table operations
*/
AQL_API void aqlStr_resize(aql_State *L, int newsize);
AQL_API void aqlStr_clearcache(global_State *g);
AQL_API void aqlStr_init(aql_State *L);
AQL_API void aqlStr_remove(aql_State *L, TString *ts);
AQL_API Udata *aqlS_newudata(aql_State *L, size_t s, int nuvalue);

/*
** String conversion functions
*/
AQL_API int aqlS_eqlngstr(TString *a, TString *b);
AQL_API unsigned int aqlS_hashlongstr(TString *ts);
AQL_API int aqlS_eqstr(TString *a, TString *b);

/*
** String utility functions  
*/
AQL_API const char *aqlS_data(TString *ts);
AQL_API size_t aqlS_len(TString *ts);
AQL_API TString *aqlStr_concat(aql_State *L, TString *a, TString *b);
AQL_API TString *aqlStr_sub(aql_State *L, TString *str, size_t start, size_t end);

/*
** String formatting
*/
AQL_API TString *aqlS_formatf(aql_State *L, const char *fmt, ...);
AQL_API TString *aqlS_formatv(aql_State *L, const char *fmt, va_list args);

/*
** String searching and matching
*/
AQL_API int aqlStr_find(TString *str, TString *pattern, size_t start);
AQL_API int aqlStr_findlast(TString *str, TString *pattern);
AQL_API TString *aqlS_replace(aql_State *L, TString *str, TString *old, TString *new, int count);

/*
** String case conversion
*/
AQL_API TString *aqlS_upper(aql_State *L, TString *str);
AQL_API TString *aqlS_lower(aql_State *L, TString *str);
AQL_API TString *aqlS_title(aql_State *L, TString *str);

/*
** String trimming and whitespace
*/
AQL_API TString *aqlS_trim(aql_State *L, TString *str);
AQL_API TString *aqlS_ltrim(aql_State *L, TString *str);
AQL_API TString *aqlS_rtrim(aql_State *L, TString *str);

/*
** String splitting and joining
*/
AQL_API Slice *aqlS_split(aql_State *L, TString *str, TString *sep, int maxsplit);
AQL_API TString *aqlStr_join(aql_State *L, TString *sep, const Slice *parts);

/*
** String encoding and validation
*/
AQL_API int aqlS_isvalid_utf8(const char *s, size_t len);
AQL_API size_t aqlS_utf8_len(const char *s, size_t len);
AQL_API int aqlS_utf8_char_len(const char *s);

/*
** String prefix and suffix operations
*/
AQL_API int aqlS_startswith(TString *str, TString *prefix);
AQL_API int aqlS_endswith(TString *str, TString *suffix);
AQL_API TString *aqlS_removeprefix(aql_State *L, TString *str, TString *prefix);
AQL_API TString *aqlS_removesuffix(aql_State *L, TString *str, TString *suffix);

/*
** String numeric conversions
*/
AQL_API int aqlS_tonumber(TString *str, aql_Number *n);
AQL_API int aqlS_tointeger(TString *str, aql_Integer *i);
AQL_API TString *aqlS_fromnumber(aql_State *L, aql_Number n);
AQL_API TString *aqlS_frominteger(aql_State *L, aql_Integer i);

/*
** String comparison
*/
AQL_API int aqlS_cmp(TString *a, TString *b);
AQL_API int aqlS_cmpi(TString *a, TString *b);  /* case insensitive */

/*
** String character operations
*/
AQL_API int aqlS_isalpha(TString *str);
AQL_API int aqlS_isdigit(TString *str);
AQL_API int aqlS_isalnum(TString *str);
AQL_API int aqlS_isspace(TString *str);

/*
** String escaping and unescaping
*/
AQL_API TString *aqlS_escape(aql_State *L, TString *str);
AQL_API TString *aqlS_unescape(aql_State *L, TString *str);

/*
** String hashing (extended)
*/
AQL_API unsigned int aqlS_hash_data(const char *str, size_t len, unsigned int seed);
AQL_API unsigned int aqlS_hash_continue(const char *str, size_t len, unsigned int hash);

/*
** String interning check
*/
static l_inline int aqlS_isintern(TString *str) {
  return ttisshrstring(obj2gco(str));
}

/*
** String type checks
*/
#define aqlS_islongstring(str) ttislngstring(obj2gco(str))
#define aqlS_isshortstring(str) ttisshrstring(obj2gco(str))

/*
** String memory optimization
*/
#define aqlS_shrlen(ts) ((ts)->shrlen)
#define aqlS_lnglen(ts) ((ts)->u.lnglen)

/*
** String equality optimization
*/
#define aqlS_eqshrstr(a,b) \
  (aqlS_shrlen(a) == aqlS_shrlen(b) && \
   memcmp(getstr(a), getstr(b), aqlS_shrlen(a)) == 0)

/* Legacy alias for compatibility */
#define eqshrstr(a,b) aqlS_eqshrstr(a,b)

/*
** Maximum sizes for strings
*/
#if !defined(AQL_MAXSHORTLEN)
#define AQL_MAXSHORTLEN 40
#endif

/*
** String cache for API
*/
#if !defined(STRCACHE_N)
#define STRCACHE_N 53
#define STRCACHE_M 2
#endif

/*
** String metamethods support
*/
AQL_API int aqlS_index(aql_State *L);    /* __index */
AQL_API int aqlS_len_mm(aql_State *L);   /* __len */
AQL_API int aqlS_eq_mm(aql_State *L);    /* __eq */
AQL_API int aqlS_lt_mm(aql_State *L);    /* __lt */
AQL_API int aqlS_le_mm(aql_State *L);    /* __le */
AQL_API int aqlS_concat_mm(aql_State *L); /* __concat */

#endif /* astring_h */ 