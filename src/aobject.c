/*
** $Id: aobject.c $
** Some generic functions over AQL objects
** See Copyright Notice in aql.h
*/

#define aobject_c
#define AQL_CORE

#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aql.h"
#include "aconf.h"
#include "aobject.h"
#include "astate.h"

/* Temporary math function implementations */
static aql_Number aql_numpow(aql_State *L, aql_Number v1, aql_Number v2) {
  UNUSED(L);
  return pow(v1, v2);
}

static aql_Number aql_numidiv(aql_State *L, aql_Number v1, aql_Number v2) {
  UNUSED(L);
  if (v2 == 0.0) return 0.0;  /* Simple error handling */
  return floor(v1 / v2);
}

/* Temporary conversion function macros */
#define tointegerns(tv, pi) (ttisinteger(tv) ? (*(pi) = ivalue(tv), 1) : \
                             (ttisfloat(tv) ? aql_numbertointeger(fltvalue(tv), pi) : 0))

#define tonumberns(tv, pn) (ttisfloat(tv) ? ((pn) = fltvalue(tv), 1) : \
                           (ttisinteger(tv) ? ((pn) = cast_num(ivalue(tv)), 1) : 0))

/*
** Type names for debugging
*/
static const char *const typenames[AQL_NUMTYPES] = {
  "nil", "boolean", "lightuserdata", "number", "integer", 
  "string", "table", "function", "userdata", "thread",
  "array", "slice", "dict", "vector"
};

const char *aqlO_typename (const TValue *o) {
  int t = ttype(o);
  return (t < AQL_NUMTYPES) ? typenames[t] : "unknown";
}

/*
** Computes ceil(log2(x))
*/
int aqlO_ceillog2 (unsigned int x) {
  static const aql_byte log_2[256] = {  /* log_2[i] = ceil(log2(i - 1)) */
    0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
  };
  int l = 0;
  x--;
  while (x >= 256) { l += 8; x >>= 8; }
  return l + log_2[x];
}

int aqlO_hexavalue (int c) {
  if (lisdigit(c)) return c - '0';
  else return (ltolower(c) - 'a') + 10;
}

/*
** VM arithmetic helper functions are implemented in avm.c
*/

void aqlT_trybinTM (aql_State *L, const TValue *p1, const TValue *p2,
                    StkId res, TMS event) {
  /* Placeholder - just fail silently for now */
  UNUSED(L); UNUSED(p1); UNUSED(p2); UNUSED(res); UNUSED(event);
  setnilvalue(s2v(res));
}

void aqlG_runerror (aql_State *L, const char *fmt, ...) {
  /* Placeholder - simplified error handling */
  UNUSED(L); UNUSED(fmt);
  /* In a real implementation, this would format the error message
     and throw a runtime error */
}

/* aqlStr_newlstr 现在在 astring.c 中实现 */

/*
** Additional VM support functions (placeholders)
*/
void aqlG_typeerror (aql_State *L, const TValue *o, const char *op) {
  /* Placeholder - simplified type error */
  UNUSED(L); UNUSED(o); UNUSED(op);
  /* In a real implementation, this would format and throw a type error */
}

int aqlG_ordererror (aql_State *L, const TValue *p1, const TValue *p2) {
  /* Placeholder - simplified order error */
  UNUSED(L); UNUSED(p1); UNUSED(p2);
  /* In a real implementation, this would format and throw an order error */
  return 0;
}

const char *aqlL_typename (aql_State *L, const TValue *o) {
  /* Placeholder - simplified typename */
  UNUSED(L); UNUSED(o);
  /* In a real implementation, this would return the type name */
  return "unknown";
}

TString *aqlS_createlngstrobj (aql_State *L, size_t l) {
  /* Placeholder - simplified long string creation */
  UNUSED(L); UNUSED(l);
  /* In a real implementation, this would create a long string object */
  return NULL;
}

AQL_API int isdead (global_State *g, GCObject *o) {
  /* Placeholder - simplified GC check */
  UNUSED(g); UNUSED(o);
  /* In a real implementation, this would check if object is marked for GC */
  return 0;
}

/* VM function call support moved to ado.c */

/* Call management functions moved to ado.c */

/*
** Integer arithmetic operations
*/
static aql_Integer intarith (aql_State *L, int op, aql_Integer v1, aql_Integer v2) {
  switch (op) {
    case AQL_OPADD: return intop(+, v1, v2);
    case AQL_OPSUB: return intop(-, v1, v2);
    case AQL_OPMUL: return intop(*, v1, v2);
    case AQL_OPMOD: return aqlV_mod(L, v1, v2);
    case AQL_OPIDIV: return aqlV_idiv(L, v1, v2);
    case AQL_OPBAND: return intop(&, v1, v2);
    case AQL_OPBOR: return intop(|, v1, v2);
    case AQL_OPBXOR: return intop(^, v1, v2);
    case AQL_OPSHL: return aqlV_shiftl(v1, v2);
    case AQL_OPSHR: return aqlV_shiftr(v1, v2);
    case AQL_OPUNM: return intop(-, 0, v1);
    case AQL_OPBNOT: return intop(^, ~l_castS2U(0), v1);
    default: aql_assert(0); return 0;
  }
}

/*
** Floating-point arithmetic operations
*/
static aql_Number numarith (aql_State *L, int op, aql_Number v1, aql_Number v2) {
  switch (op) {
    case AQL_OPADD: return aql_numadd(L, v1, v2);
    case AQL_OPSUB: return aql_numsub(L, v1, v2);
    case AQL_OPMUL: return aql_nummul(L, v1, v2);
    case AQL_OPDIV: return aql_numdiv(L, v1, v2);
    case AQL_OPPOW: return aql_numpow(L, v1, v2);
    case AQL_OPIDIV: return aql_numidiv(L, v1, v2);
    case AQL_OPUNM: return aql_numunm(L, v1);
    case AQL_OPMOD: return aqlV_modf(L, v1, v2);
    default: aql_assert(0); return 0;
  }
}

/*
** Raw arithmetic operations (without metamethods)
*/
int aqlO_rawarith (aql_State *L, int op, const TValue *p1, const TValue *p2,
                   TValue *res) {
  switch (op) {
    case AQL_OPBAND: case AQL_OPBOR: case AQL_OPBXOR:
    case AQL_OPSHL: case AQL_OPSHR:
    case AQL_OPBNOT: {  /* operate only on integers */
      aql_Integer i1; aql_Integer i2;
      if (tointegerns(p1, &i1) && tointegerns(p2, &i2)) {
        setivalue(res, intarith(L, op, i1, i2));
        return 1;
      }
      else return 0;  /* fail */
    }
    case AQL_OPDIV: case AQL_OPPOW: {  /* operate only on floats */
      aql_Number n1; aql_Number n2;
      if (tonumberns(p1, n1) && tonumberns(p2, n2)) {
        setfltvalue(res, numarith(L, op, n1, n2));
        return 1;
      }
      else return 0;  /* fail */
    }
    default: {  /* other operations */
      aql_Number n1; aql_Number n2;
      if (ttisinteger(p1) && ttisinteger(p2)) {
        setivalue(res, intarith(L, op, ivalue(p1), ivalue(p2)));
        return 1;
      }
      else if (tonumberns(p1, n1) && tonumberns(p2, n2)) {
        setfltvalue(res, numarith(L, op, n1, n2));
        return 1;
      }
      else return 0;  /* fail */
    }
  }
}

/*
** Arithmetic operations with metamethod fallback
*/
void aqlO_arith (aql_State *L, int op, const TValue *p1, const TValue *p2,
                 StkId res) {
  if (!aqlO_rawarith(L, op, p1, p2, s2v(res))) {
    /* could not perform raw operation; try metamethod */
    aqlT_trybinTM(L, p1, p2, res, cast(TMS, (op - AQL_OPADD) + TM_ADD));
  }
}

/*
** Helper function to check for negative sign
*/
static int isneg (const char **s) {
  if (**s == '-') { (*s)++; return 1; }
  else if (**s == '+') (*s)++;
  return 0;
}

/*
** Convert string 's' to an AQL number (put in 'result'). Return NULL on
** fail or the address of the ending '\0' on success. ('mode' == 'x')
** means a hexadecimal numeral.
*/
static const char *l_str2dloc (const char *s, aql_Number *result, int mode) {
  char *endptr;
  *result = (mode == 'x') ? aql_strx2number(s, &endptr)  /* try to convert */
                          : aql_str2number(s, &endptr);
  if (endptr == s) return NULL;  /* nothing recognized? */
  while (lisspace(cast_uchar(*endptr))) endptr++;  /* skip trailing spaces */
  return (*endptr == '\0') ? endptr : NULL;  /* OK iff no trailing chars */
}

/*
** Convert string 's' to an AQL number (put in 'result') handling the
** current locale.
*/
static const char *l_str2d (const char *s, aql_Number *result) {
  const char *endptr;
  const char *pmode = strpbrk(s, ".xXnN");  /* look for special chars */
  int mode = pmode ? ltolower(cast_uchar(*pmode)) : 0;
  if (mode == 'n')  /* reject 'inf' and 'nan' */
    return NULL;
  endptr = l_str2dloc(s, result, mode);  /* try to convert */
  if (endptr == NULL) {  /* failed? may be a different locale */
    char buff[L_MAXLENNUM + 1];
    const char *pdot = strchr(s, '.');
    if (pdot == NULL || strlen(s) > L_MAXLENNUM)
      return NULL;  /* string too long or no dot; fail */
    strcpy(buff, s);  /* copy string to buffer */
    buff[pdot - s] = aql_getlocaledecpoint();  /* correct decimal point */
    endptr = l_str2dloc(buff, result, mode);  /* try again */
    if (endptr != NULL)
      endptr = s + (endptr - buff);  /* make relative to 's' */
  }
  return endptr;
}

/*
** Convert string to integer
*/
static const char *l_str2int (const char *s, aql_Integer *result) {
  aql_Unsigned a = 0;
  int empty = 1;
  int neg;
  while (lisspace(cast_uchar(*s))) s++;  /* skip initial spaces */
  neg = isneg(&s);
  if (s[0] == '0' &&
      (s[1] == 'x' || s[1] == 'X')) {  /* hex? */
    s += 2;  /* skip '0x' */
    for (; lisxdigit(cast_uchar(*s)); s++) {
      a = a * 16 + aqlO_hexavalue(*s);
      empty = 0;
    }
  }
  else {  /* decimal */
    for (; lisdigit(cast_uchar(*s)); s++) {
      int d = *s - '0';
      if (a >= MAXBY10 && (a > MAXBY10 || d > MAXLASTD + neg))  /* overflow? */
        return NULL;  /* do not accept it (as integer) */
      a = a * 10 + d;
      empty = 0;
    }
  }
  while (lisspace(cast_uchar(*s))) s++;  /* skip trailing spaces */
  if (empty || *s != '\0') return NULL;  /* something wrong in the numeral */
  else {
    *result = l_castU2S((neg) ? 0u - a : a);
    return s;
  }
}

/*
** Main string to number conversion function
*/
size_t aqlO_str2num (const char *s, TValue *o) {
  aql_Integer i; aql_Number n;
  const char *e;
  if ((e = l_str2int(s, &i)) != NULL) {  /* try as an integer */
    setivalue(o, i);
  }
  else if ((e = l_str2d(s, &n)) != NULL) {  /* else try as a float */
    setfltvalue(o, n);
  }
  else
    return 0;  /* conversion failed */
  return (e - s) + 1;  /* success; return string size */
}

int aqlO_utf8esc (char *buff, unsigned long x) {
  int n = 1;  /* number of bytes put in buffer (backwards) */
  aql_assert(x <= 0x7FFFFFFFu);
  if (x < 0x80)  /* ascii? */
    buff[UTF8BUFFSZ - 1] = cast_char(x);
  else {  /* need continuation bytes */
    unsigned int mfb = 0x3f;  /* maximum that fits in first byte */
    do {  /* add continuation bytes */
      buff[UTF8BUFFSZ - (n++)] = cast_char(0x80 | (x & 0x3f));
      x >>= 6;  /* remove added bits */
      mfb >>= 1;  /* now there is one less bit available in first byte */
    } while (x > mfb);  /* still needs continuation byte? */
    buff[UTF8BUFFSZ - n] = cast_char((~mfb << 1) | x);  /* add first byte */
  }
  return n;
}

/*
** Convert a number object to a string, adding it to a buffer
*/
static int tostringbuff (TValue *obj, char *buff) {
  int len;
  aql_assert(ttisnumber(obj));
  if (ttisinteger(obj))
    len = aql_integer2str(buff, MAXNUMBER2STR, ivalue(obj));
  else {
    len = aql_number2str(buff, MAXNUMBER2STR, fltvalue(obj));
    if (buff[strspn(buff, "-0123456789")] == '\0') {  /* looks like an int? */
      buff[len++] = aql_getlocaledecpoint();
      buff[len++] = '0';  /* adds '.0' to result */
    }
  }
  return len;
}

/*
** Convert a number object to an AQL string, replacing the value at 'obj'
*/
void aqlO_tostring (aql_State *L, TValue *obj) {
  /* Simplified implementation to avoid circular dependencies */
  UNUSED(L); UNUSED(obj);
  /* TODO: In a complete implementation, this would:
     1. Format the number using tostringbuff
     2. Create a new TString with aqlStr_newlstr
     3. Set the obj to the new string value
  */
}

/*
** {==================================================================
** 'aqlO_pushvfstring'
** ===================================================================
*/

/*
** Size for buffer space used by 'aqlO_pushvfstring'. It should be
** (AQL_IDSIZE + MAXNUMBER2STR) + a minimal space for basic messages,
** so that 'aqlG_addinfo' can work directly on the buffer.
*/

/* Note: Simplified implementation removes complex buffer management for Phase 1 */

/*
** Simplified string formatting function (Phase 1 implementation)
** this function handles only '%d', '%c', '%f', '%p', '%s', and '%%'
   conventional formats, plus AQL-specific '%I' and '%U'
*/
const char *aqlO_pushvfstring (aql_State *L, const char *fmt, va_list argp) {
  static char result_buffer[BUFVFS];  /* Static buffer for result */
  char *dest = result_buffer;
  const char *e;  /* points to next '%' */
  size_t remaining = BUFVFS - 1;  /* Leave space for null terminator */
  
  UNUSED(L);  /* For Phase 1, we don't use the stack */
  
  result_buffer[0] = '\0';  /* Initialize buffer */
  
  while ((e = strchr(fmt, '%')) != NULL && remaining > 0) {
    /* Copy format string up to '%' */
    size_t len = e - fmt;
    if (len > remaining) len = remaining;
    memcpy(dest, fmt, len);
    dest += len;
    remaining -= len;
    
    if (remaining == 0) break;
    
    switch (*(e + 1)) {  /* conversion specifier */
      case 's': {  /* zero-terminated string */
        const char *s = va_arg(argp, char *);
        if (s == NULL) s = "(null)";
        size_t slen = strlen(s);
        if (slen > remaining) slen = remaining;
        memcpy(dest, s, slen);
        dest += slen;
        remaining -= slen;
        break;
      }
      case 'c': {  /* an 'int' as a character */
        if (remaining > 0) {
          *dest++ = cast_uchar(va_arg(argp, int));
          remaining--;
        }
        break;
      }
      case 'd': {  /* an 'int' */
        TValue num;
        setivalue(&num, va_arg(argp, int));
        int len = tostringbuff(&num, dest);
        if (len > (int)remaining) len = remaining;
        dest += len;
        remaining -= len;
        break;
      }
      case 'I': {  /* an 'aql_Integer' */
        TValue num;
        setivalue(&num, cast(aql_Integer, va_arg(argp, l_uacInt)));
        int len = tostringbuff(&num, dest);
        if (len > (int)remaining) len = remaining;
        dest += len;
        remaining -= len;
        break;
      }
      case 'f': {  /* an 'aql_Number' */
        TValue num;
        setfltvalue(&num, cast_num(va_arg(argp, l_uacNumber)));
        int len = tostringbuff(&num, dest);
        if (len > (int)remaining) len = remaining;
        dest += len;
        remaining -= len;
        break;
      }
      case 'p': {  /* a pointer */
        void *p = va_arg(argp, void *);
        int len = aql_pointer2str(dest, remaining, p);
        if (len > (int)remaining) len = remaining;
        dest += len;
        remaining -= len;
        break;
      }
      case '%': {
        if (remaining > 0) {
          *dest++ = '%';
          remaining--;
        }
        break;
    }
    default: {
        /* Skip unknown format specifiers */
        break;
      }
    }
    fmt = e + 2;  /* skip '%' and the specifier */
  }
  
  /* Copy rest of format string */
  if (remaining > 0) {
    size_t len = strlen(fmt);
    if (len > remaining) len = remaining;
    memcpy(dest, fmt, len);
    dest += len;
  }
  
  *dest = '\0';  /* Null terminate */
  return result_buffer;
}

const char *aqlO_pushfstring (aql_State *L, const char *fmt, ...) {
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = aqlO_pushvfstring(L, fmt, argp);
  va_end(argp);
  return msg;
}

/* }================================================================== */

void aqlO_chunkid (char *out, const char *source, size_t srclen) {
  /* Simple placeholder implementation */
  if (srclen > 60) srclen = 60;
  if (*source == '=') {  /* 'literal' source */
    if (srclen > 1) memcpy(out, source + 1, (srclen - 1) * sizeof(char));
    out[srclen - 1] = '\0';
  } else if (*source == '@') {  /* file name */
    if (srclen > 1) memcpy(out, source + 1, (srclen - 1) * sizeof(char));
    out[srclen - 1] = '\0';
  } else {  /* string */
    memcpy(out, "[string]", 8);
    out[8] = '\0';
  }
} 