/*
** $Id: aparser_types.h $
** AQL Parser Types (based on Lua 5.4 lparser.h)
** See Copyright Notice in aql.h
*/

#ifndef aparser_types_h
#define aparser_types_h

#include "alimits.h"
#include "aobject.h"

/* kinds of variables/expressions */
typedef enum {
  VVOID,       /* when 'expdesc' describes the last expression of a list,
                   this kind means an empty list (so, no expression) */
  VNIL,        /* constant nil */
  VTRUE,       /* constant true */
  VFALSE,      /* constant false */
  VK,          /* constant in 'k'; info = index of constant in 'k' */
  VKFLT,       /* floating constant; nval = numerical float value */
  VKINT,       /* integer constant; ival = numerical integer value */
  VKSTR,       /* string constant; strval = TString address;
                   (string is fixed by the lexer) */
  VNONRELOC,   /* expression has its value in a fixed register;
                   info = result register */
  VLOCAL,      /* local variable; var.ridx = register index;
                   var.vidx = relative index in 'actvar.arr'  */
  VUPVAL,      /* upvalue variable; info = index of upvalue in 'upvalues' */
  VCONST,      /* compile-time <const> variable;
                   info = absolute index in 'actvar.arr'  */
  VINDEXED,    /* indexed variable;
                   ind.t = table register;
                   ind.idx = key's R index */
  VINDEXUP,    /* indexed upvalue;
                   ind.t = table upvalue;
                   ind.idx = key's K index */
  VINDEXI,     /* indexed variable with constant integer;
                   ind.t = table register;
                   ind.idx = key's value */
  VINDEXSTR,   /* indexed variable with literal string;
                   ind.t = table register;
                   ind.idx = key's K index */
  VJMP,        /* expression is a test/comparison;
                   info = pc of corresponding jump instruction */
  VRELOC,      /* expression can put result in any register;
                   info = instruction pc */
  VCALL,       /* expression is a function call; info = instruction pc */
  VVARARG      /* vararg expression; info = instruction pc */
} expkind;

#define vkisvar(k)     (VLOCAL <= (k) && (k) <= VINDEXSTR)
#define vkisindexed(k) (VINDEXED <= (k) && (k) <= VINDEXSTR)

typedef struct expdesc {
  expkind k;
  union {
    aql_Integer ival;    /* for VKINT */
    aql_Number nval;     /* for VKFLT */
    TString *strval;     /* for VKSTR */
    int info;            /* for generic use */
    struct {             /* for indexed variables */
      short idx;         /* index (R or "long" K) */
      aql_byte t;        /* table (register or upvalue) */
    } ind;
    struct {             /* for local variables */
      aql_byte ridx;     /* register holding the variable */
      unsigned short vidx;  /* compiler index (in 'actvar.arr')  */
    } var;
  } u;
  int t;  /* patch list of 'exit when true' */
  int f;  /* patch list of 'exit when false' */
} expdesc;

#endif