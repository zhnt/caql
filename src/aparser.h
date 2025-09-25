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
#include "atype.h"
#include "acontainer.h"
#include <stdbool.h>

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
  VVARARG,     /* vararg expression; info = instruction pc */
  VBUILTIN     /* builtin function; info = builtin function id */
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
/* kinds of variables */
#define VDKREG		0   /* regular */
#define RDKCONST	1   /* constant */
#define RDKTOCLOSE	2   /* to-be-closed */
#define RDKCTC		3   /* compile-time constant */

/*
** AQL enhancements for variable management
** These enums extend Lua's basic variable system with:
** - Type inference support
** - Container integration  
** - Multi-mode execution (Script/JIT/AOT)
** - Zero-cost debugging
*/

/* AQL Type inference levels */
typedef enum AQLTypeLevel {
  AQL_TYPE_NONE = 0,      /* No type information */
  AQL_TYPE_INFERRED,      /* Type inferred from usage */
  AQL_TYPE_ANNOTATED,     /* Explicitly annotated type */
  AQL_TYPE_STATIC         /* Statically verified type */
} AQLTypeLevel;

/* AQL Execution modes for variables */
typedef enum AQLExecMode {
  AQL_MODE_SCRIPT = 0,    /* Script mode (Lua-compatible) */
  AQL_MODE_JIT,           /* JIT compilation mode */
  AQL_MODE_AOT            /* AOT compilation mode */
} AQLExecMode;

/*
** description of active local variable (Lua-compatible with AQL enhancements)
*/
typedef union Vardesc {
  /* Lua-compatible core structure */
  struct {
    TValuefields;      /* constant value */
    aql_byte kind;
    aql_byte ridx;  /* register holding the variable */
    short pidx;  /* index in the Proto's 'locvars' array */
    TString *name;  /* variable name */
  } vd;
  
  /* AQL enhanced structure (same memory layout as vd for compatibility) */
  struct {
    TValuefields;      /* constant value (same as vd) */
    aql_byte kind;     /* variable kind (same as vd) */
    aql_byte ridx;     /* register index (same as vd) */
    short pidx;        /* debug index (same as vd) */
    TString *name;     /* variable name (same as vd) */
    
    /* AQL enhancements (additional fields) */
    AQLTypeLevel type_level;      /* Type inference level */
    TypeCategory inferred_type;   /* Inferred type */
    aql_byte confidence;          /* Type confidence (0-100) */
    AQLExecMode exec_mode;        /* Execution mode */
    
    /* Container information */
    ContainerType container_type; /* Container type */
    size_t container_capacity;    /* Container capacity */
    aql_byte container_flags;     /* is_container:1, is_mutable:1, etc. */
    
    #ifdef AQL_DEBUG_BUILD
    /* Debug information (zero-cost in release) */
    int declaration_line;         /* Declaration line */
    int declaration_column;       /* Declaration column */
    aql_uint32 access_count;      /* Access count */
    aql_uint32 modification_count; /* Modification count */
    #endif
  } aql;
  
  TValue k;  /* constant value (Lua-compatible) */
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
  
  /* AQL enhancements for dynamic data */
  struct {
    /* Type inference cache */
    struct {
      TypeCategory *type_cache;    /* Cached type information */
      int cache_size;         /* Cache capacity */
      int cache_used;         /* Cache usage */
    } types;
    
    /* Container registry */
    struct {
      Container **containers; /* Active container instances */
      int container_count;    /* Number of active containers */
      int container_capacity; /* Container array capacity */
    } containers;
    
    /* Execution mode tracking */
    AQLExecMode current_mode;   /* Current execution mode */
    bool mode_locked;           /* Is mode locked for this scope? */
  } aql;
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

/* Expression and statement parsing functions */
AQL_API int aqlP_parse_expression(aql_State *L, const char *expr_str, TValue *result);  /* Unified function using TValue */
AQL_API int aqlP_parse_statement(aql_State *L, const char *stmt_str);
AQL_API int aqlP_execute_file(aql_State *L, const char *filename);

/* Compilation and execution functions moved to aapi.h */

/* Helper functions for TValue manipulation in REPL */
AQL_API void aqlP_print_value(const TValue *v);
AQL_API void aqlP_free_value(TValue *v);

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

/* Code generation functions are in acode.h */

#endif