/*
** $Id: aparser.h $
** AQL Parser (based on Lua 5.4 lparser.h)
** See Copyright Notice in aql.h
*/

#ifndef aparser_h
#define aparser_h

#include "alimits.h"
#include "aobject.h"
#include "azio.h"

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

/* kinds of variables */
#define VDKREG		0   /* regular */
#define RDKCONST	1   /* constant */
#define RDKTOCLOSE	2   /* to-be-closed */
#define RDKCTC		3   /* compile-time constant */

/*
** description of active local variable
*/
typedef union Vardesc {
  struct {
    TValuefields;      /* constant value */
    aql_byte kind;
    aql_byte ridx;  /* register holding the variable */
    short pidx;  /* index in the Proto's 'locvars' array */
    TString *name;  /* variable name */
  } vd;
  TValue k;  /* constant value */
} Vardesc;

/*
** description of pending goto statements and label statements
*/
typedef struct LabelDesc {
  TString *name;  /* label identifier */
  int pc;  /* position in code */
  int line;  /* line where it appeared */
  aql_byte nactvar;  /* number of active variables in that position */
  aql_byte close;  /* goto that escapes upvalues */
} LabelDesc;

/*
** list of labels or gotos
*/
typedef struct LabelList {
  LabelDesc *arr;  /* array */
  int n;  /* number of entries in use */
  int size;  /* array size */
} LabelList;

/*
** dynamic data used by the parser
*/
typedef struct Dyndata {
  struct {  /* list of active local variables */
    Vardesc *arr;
    int n;
    int size;
  } actvar;
  LabelList gt;  /* list of pending gotos */
  LabelList label;   /* list of active labels */
} Dyndata;

/*
** control of blocks
*/
struct BlockCnt;  /* defined in aparser.c */

/*
** function states
*/
typedef struct FuncState {
  Proto *f;  /* current function header */
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  struct BlockCnt *bl;  /* chain of current blocks */
  int pc;  /* next position to code (equivalent to 'ncode') */
  int lasttarget;   /* 'label' of last 'jump label' */
  int previousline;  /* last line that was saved in 'lineinfo' */
  int nk;  /* number of elements in 'k' */
  int np;  /* number of elements in 'p' */
  int nabslineinfo;  /* number of elements in 'abslineinfo' */
  int firstlocal;  /* index of first local var (in Dyndata array) */
  int firstlabel;  /* index of first label (in Dyndata's 'label->arr') */
  short ndebugvars;  /* number of elements in 'f->locvars' */
  aql_byte nactvar;  /* number of active local variables */
  aql_byte nups;  /* number of upvalues */
  aql_byte freereg;  /* first free register */
  aql_byte iwthabs;  /* instructions issued since last absolute line info */
  aql_byte needclose;  /* function needs to close upvalues when returning */
} FuncState;

AQL_API LClosure *aqlY_parser (aql_State *L, struct Zio *z, Mbuffer *buff,
                                Dyndata *dyd, const char *name, int firstchar);

/* REPL and expression parsing functions */
AQL_API int aqlP_parse_expression(const char *expr_str, double *result);
AQL_API int aqlP_execute_file(aql_State *L, const char *filename);
AQL_API void aqlP_repl(aql_State *L);

/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/
#define NO_JUMP (-1)

/*
** grep "ORDER OPR" if you change these enums  (ORDER OP)
*/
typedef enum BinOpr {
  /* arithmetic operators */
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_MOD, OPR_POW,
  OPR_DIV, OPR_IDIV,
  /* bitwise operators */
  OPR_BAND, OPR_BOR, OPR_BXOR, OPR_SHL, OPR_SHR,
  /* string concatenation */
  OPR_CONCAT,
  /* relational operators */
  OPR_EQ, OPR_LT, OPR_LE, OPR_NE, OPR_GT, OPR_GE,
  /* logical operators */
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;

/* true if operation is foldable (that is, it is arithmetic or bitwise) */
#define foldbinop(op)	((op) <= OPR_SHR)

typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;

/*
** kinds of constructor
*/
typedef enum ConsKind { CONS_RECORD, CONS_LIST, CONS_MIXED } ConsKind;

/*
** nodes for constructor
*/
typedef struct ConsControl {
  expdesc v;  /* last list item read */
  expdesc *t;  /* table descriptor */
  int nh;  /* total number of 'record' elements */
  int na;  /* number of array elements already stored */
  int tostore;  /* number of array elements pending to be stored */
} ConsControl;

#endif
** $Id: aparser.h $
** AQL Parser (based on Lua 5.4 lparser.h)
** See Copyright Notice in aql.h
*/

#ifndef aparser_h
#define aparser_h

#include "alimits.h"
#include "aobject.h"
#include "azio.h"

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

/* kinds of variables */
#define VDKREG		0   /* regular */
#define RDKCONST	1   /* constant */
#define RDKTOCLOSE	2   /* to-be-closed */
#define RDKCTC		3   /* compile-time constant */

/*
** description of active local variable
*/
typedef union Vardesc {
  struct {
    TValuefields;      /* constant value */
    aql_byte kind;
    aql_byte ridx;  /* register holding the variable */
    short pidx;  /* index in the Proto's 'locvars' array */
    TString *name;  /* variable name */
  } vd;
  TValue k;  /* constant value */
} Vardesc;

/*
** description of pending goto statements and label statements
*/
typedef struct LabelDesc {
  TString *name;  /* label identifier */
  int pc;  /* position in code */
  int line;  /* line where it appeared */
  aql_byte nactvar;  /* number of active variables in that position */
  aql_byte close;  /* goto that escapes upvalues */
} LabelDesc;

/*
** list of labels or gotos
*/
typedef struct LabelList {
  LabelDesc *arr;  /* array */
  int n;  /* number of entries in use */
  int size;  /* array size */
} LabelList;

/*
** dynamic data used by the parser
*/
typedef struct Dyndata {
  struct {  /* list of active local variables */
    Vardesc *arr;
    int n;
    int size;
  } actvar;
  LabelList gt;  /* list of pending gotos */
  LabelList label;   /* list of active labels */
} Dyndata;

/*
** control of blocks
*/
struct BlockCnt;  /* defined in aparser.c */

/*
** function states
*/
typedef struct FuncState {
  Proto *f;  /* current function header */
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  struct BlockCnt *bl;  /* chain of current blocks */
  int pc;  /* next position to code (equivalent to 'ncode') */
  int lasttarget;   /* 'label' of last 'jump label' */
  int previousline;  /* last line that was saved in 'lineinfo' */
  int nk;  /* number of elements in 'k' */
  int np;  /* number of elements in 'p' */
  int nabslineinfo;  /* number of elements in 'abslineinfo' */
  int firstlocal;  /* index of first local var (in Dyndata array) */
  int firstlabel;  /* index of first label (in Dyndata's 'label->arr') */
  short ndebugvars;  /* number of elements in 'f->locvars' */
  aql_byte nactvar;  /* number of active local variables */
  aql_byte nups;  /* number of upvalues */
  aql_byte freereg;  /* first free register */
  aql_byte iwthabs;  /* instructions issued since last absolute line info */
  aql_byte needclose;  /* function needs to close upvalues when returning */
} FuncState;

AQL_API LClosure *aqlY_parser (aql_State *L, struct Zio *z, Mbuffer *buff,
                                Dyndata *dyd, const char *name, int firstchar);

/* REPL and expression parsing functions */
AQL_API int aqlP_parse_expression(const char *expr_str, double *result);
AQL_API int aqlP_execute_file(aql_State *L, const char *filename);
AQL_API void aqlP_repl(aql_State *L);

/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/
#define NO_JUMP (-1)

/*
** grep "ORDER OPR" if you change these enums  (ORDER OP)
*/
typedef enum BinOpr {
  /* arithmetic operators */
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_MOD, OPR_POW,
  OPR_DIV, OPR_IDIV,
  /* bitwise operators */
  OPR_BAND, OPR_BOR, OPR_BXOR, OPR_SHL, OPR_SHR,
  /* string concatenation */
  OPR_CONCAT,
  /* relational operators */
  OPR_EQ, OPR_LT, OPR_LE, OPR_NE, OPR_GT, OPR_GE,
  /* logical operators */
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;

/* true if operation is foldable (that is, it is arithmetic or bitwise) */
#define foldbinop(op)	((op) <= OPR_SHR)

typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;

/*
** kinds of constructor
*/
typedef enum ConsKind { CONS_RECORD, CONS_LIST, CONS_MIXED } ConsKind;

/*
** nodes for constructor
*/
typedef struct ConsControl {
  expdesc v;  /* last list item read */
  expdesc *t;  /* table descriptor */
  int nh;  /* total number of 'record' elements */
  int na;  /* number of array elements already stored */
  int tostore;  /* number of array elements pending to be stored */
} ConsControl;

#endif