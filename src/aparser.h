/*
** $Id: aparser.h $
** Parser for AQL
** See Copyright Notice in aql.h
*/

#ifndef aparser_h
#define aparser_h

#include "alex.h"
#include "acode.h"
#include "aobject.h"
#include "aopcodes.h"

/*
** Expression and variable descriptor
*/
typedef enum {
  VVOID,    /* when 'expdesc' describes the last expression of a list,
               this kind means an empty list (so, no expression) */
  VNIL,     /* constant nil */
  VTRUE,    /* constant true */
  VFALSE,   /* constant false */
  VK,       /* constant in 'k'; info = index of constant in 'k' */
  VKFLT,    /* floating constant; nval = numerical float value */
  VKINT,    /* integer constant; ival = numerical integer value */
  VKSTR,    /* string constant; strval = TString address */
  VNONRELOC,/* expression has its value in a fixed register; info = register */
  VLOCAL,   /* local variable; var.ridx = register, var.vidx = relative index */
  VUPVAL,   /* upvalue variable; info = index of upvalue in 'upvalues' */
  VCONST,   /* compile-time <const> variable; info = absolute index in 'actvar' */
  VINDEXED, /* indexed variable; ind.t = table register; ind.idx = key's R/K; ind.vt = table's variable type */
  VINDEXUP, /* indexed upvalue; ind.t = table upvalue; ind.idx = key's R/K; ind.vt = table's variable type */
  VINDEXI,  /* indexed variable with integer key; ind.t = table register; ind.idx = key's value */
  VINDEXSTR,/* indexed variable with literal string key; ind.t = table register; ind.idx = key's K */
  VJMP,     /* expression is a test/comparison; info = pc of corresponding jump instruction */
  VRELOC,   /* expression can put result in any register; info = instruction pc */
  VCALL,    /* expression is a function call; info = instruction pc */
  VVARARG,  /* vararg expression; info = instruction pc */
  
  /* AQL-specific expression types */
  VARRAY,    /* array literal; info = register containing array */
  VSLICE,    /* slice literal; info = register containing slice */
  VDICT,     /* dict literal; info = register containing dict */
  VVECTOR,   /* vector literal; info = register containing vector */
  VTYPE,     /* type expression; info = type index */
  VGENERIC,  /* generic type; info = generic index */
  VLAMBDA,   /* lambda expression; info = function index */
  VASYNC,    /* async expression; info = coroutine register */
  VAWAIT,    /* await expression; info = awaited register */
  VYIELD,    /* yield expression; info = yielded register */
  VPIPE,     /* pipe expression; info = pipeline register */
  VAI        /* AI expression; info = AI call register */
} expkind;

typedef struct expdesc {
  expkind k;
  union {
    aql_Integer ival;    /* for VKINT */
    aql_Number nval;     /* for VKFLT */
    TString *strval;     /* for VKSTR */
    int info;            /* for generic use */
    struct {             /* for indexed variables */
      short idx;         /* index (R or 'long' K) */
      aql_byte t;        /* table (register or upvalue) */
      aql_byte vt;       /* variable type VLOCAL or VUPVAL */
    } ind;
    struct {             /* for local variables */
      aql_byte ridx;     /* register holding the variable */
      unsigned short vidx; /* compiler index (in 'actvar') */
    } var;
  } u;
  int t;  /* patch list of 'exit when true' */
  int f;  /* patch list of 'exit when false' */
} expdesc;

/*
** Kinds of variables/expressions
*/
#define vkisvar(k)     (VLOCAL <= (k) && (k) <= VINDEXSTR)
#define vkisindexed(k) (VINDEXED <= (k) && (k) <= VINDEXSTR)

/*
** Description of an active local variable
*/
typedef union Vardesc {
  struct {
    TValuefields;  /* variable value */
    aql_byte ridx;  /* register holding the variable */
    aql_byte pidx;  /* index in the upvalue list */
    TString *name;  /* variable name */
  } vd;
  TValue k;  /* constant value (if it is a compile-time constant) */
} Vardesc;

/*
** Description of pending goto statements and label statements
*/
typedef struct Labeldesc {
  TString *name;  /* label identifier */
  int pc;  /* position in code */
  int line;  /* line where it appeared */
  aql_byte nactvar;  /* local level where it appears in current block */
  aql_byte close;  /* goto that escapes upvalues */
} Labeldesc;

/*
** List of labels or gotos
*/
typedef struct Labellist {
  Labeldesc *arr;  /* array */
  int n;  /* number of entries in use */
  int size;  /* array size */
} Labellist;

/*
** Dynamic data structures used by the parser
*/
typedef struct Dyndata {
  struct {  /* list of all active local variables */
    Vardesc *arr;
    int n;
    int size;
  } actvar;
  Labellist gt;  /* list of pending gotos */
  Labellist label;   /* list of active labels */
} Dyndata;

/* state needed to generate code for a given function */
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
  int firstlabel;  /* index of first label (in 'dyd->label->arr') */
  short ndebugvars;  /* number of elements in 'f->locvars' */
  aql_byte nactvar;  /* number of active local variables */
  aql_byte nups;  /* number of upvalues */
  aql_byte freereg;  /* first free register */
  aql_byte iwthabs;  /* instructions issued since last absolute line info */
  aql_byte needclose;  /* function needs to close upvalues when returning */
} FuncState;

/*
** Prototypes
*/
AQL_API LClosure *aqlY_parser(aql_State *L, ZIO *z, Mbuffer *buff,
                              Dyndata *dyd, const char *name, int firstchar);
AQL_API l_noret aqlY_syntaxerror(LexState *ls, const char *msg);
AQL_API l_noret aqlY_semerror(LexState *ls, const char *msg);

/*
** AQL-specific parsing functions
*/

/* Type system parsing */
AQL_API void aqlY_parse_type(LexState *ls, expdesc *e);
AQL_API void aqlY_parse_generic_type(LexState *ls, expdesc *e);
AQL_API int aqlY_parse_type_params(LexState *ls);
AQL_API void aqlY_check_type_compat(LexState *ls, expdesc *e1, expdesc *e2);

/* Container literal parsing */
AQL_API void aqlY_parse_array_literal(LexState *ls, expdesc *e);
AQL_API void aqlY_parse_slice_literal(LexState *ls, expdesc *e);
AQL_API void aqlY_parse_dict_literal(LexState *ls, expdesc *e);
AQL_API void aqlY_parse_vector_literal(LexState *ls, expdesc *e);

/* Class and interface parsing */
AQL_API void aqlY_parse_class(LexState *ls);
AQL_API void aqlY_parse_interface(LexState *ls);
AQL_API void aqlY_parse_struct(LexState *ls);

/* Function and closure parsing */
AQL_API void aqlY_parse_function(LexState *ls, expdesc *e, int needself);
AQL_API void aqlY_parse_lambda(LexState *ls, expdesc *e);
AQL_API void aqlY_parse_method(LexState *ls, expdesc *e);

/* Async/await parsing */
AQL_API void aqlY_parse_async(LexState *ls, expdesc *e);
AQL_API void aqlY_parse_await(LexState *ls, expdesc *e);
AQL_API void aqlY_parse_yield(LexState *ls, expdesc *e);

/* Import/module parsing */
AQL_API void aqlY_parse_import(LexState *ls);
AQL_API void aqlY_parse_export(LexState *ls);

/* AI-specific parsing (Phase 2) */
AQL_API void aqlY_parse_ai_call(LexState *ls, expdesc *e);
AQL_API void aqlY_parse_intent(LexState *ls);
AQL_API void aqlY_parse_workflow(LexState *ls);
AQL_API void aqlY_parse_parallel(LexState *ls, expdesc *e);

/* Pipeline and operator parsing */
AQL_API void aqlY_parse_pipe(LexState *ls, expdesc *e);
AQL_API void aqlY_parse_assignment_op(LexState *ls, expdesc *e, int op);

/*
** Expression parsing
*/
AQL_API void aqlY_expression(LexState *ls, expdesc *e);
AQL_API void aqlY_expr(LexState *ls, expdesc *e);
AQL_API int aqlY_explist(LexState *ls, expdesc *e);

/*
** Statement parsing  
*/
AQL_API void aqlY_statement(LexState *ls);
AQL_API void aqlY_statlist(LexState *ls);
AQL_API void aqlY_block(LexState *ls);

/*
** Error recovery and reporting
*/
AQL_API void aqlY_error_expected(LexState *ls, int token);
AQL_API void aqlY_type_error(LexState *ls, const char *expected, const char *got);
AQL_API void aqlY_scope_error(LexState *ls, const char *name);

/*
** Symbol table management
*/
AQL_API int aqlY_new_localvar(LexState *ls, TString *name);
AQL_API void aqlY_adjustlocalvars(LexState *ls, int nvars);
AQL_API void aqlY_removevars(LexState *ls, int tolevel);
AQL_API int aqlY_searchupvalue(FuncState *fs, TString *name);
AQL_API int aqlY_newupvalue(FuncState *fs, TString *name, expdesc *v);

/*
** Block and scope management
*/
typedef struct BlockCnt {
  struct BlockCnt *previous;  /* chain */
  int firstlabel;  /* index of first label in this block */
  int firstgoto;  /* index of first pending goto in this block */
  aql_byte nactvar;  /* # active locals outside the block */
  aql_byte upval;  /* true if some variable in the block is an upvalue */
  aql_byte isloop;  /* true if 'block' is a loop */
  aql_byte insidetbc;  /* true if inside the scope of a to-be-closed var. */
} BlockCnt;

AQL_API void aqlY_enterblock(FuncState *fs, BlockCnt *bl, aql_byte isloop);
AQL_API void aqlY_leaveblock(FuncState *fs);

/*
** Debugging and analysis support
*/
#if defined(AQL_DEBUG_PARSER)
AQL_API void aqlY_debug_expr(LexState *ls, expdesc *e, const char *msg);
AQL_API void aqlY_dump_ast(LexState *ls);
AQL_API void aqlY_print_scope(FuncState *fs);
#else
#define aqlY_debug_expr(ls, e, msg)  ((void)0)
#define aqlY_dump_ast(ls)            ((void)0)
#define aqlY_print_scope(fs)         ((void)0)
#endif

/*
** Parser constants
*/
#define MAX_LEXPARS      200  /* maximum number of expressions in list */
#define MAX_CCALLS       200  /* maximum number of nested C calls */
#define MAX_VARS         250  /* maximum number of local variables per function */
#define MAX_UPVALS       80   /* maximum number of upvalues per function */

/*
** Priority levels for operators
*/
#define UNARY_PRIORITY   12  /* priority for unary operators */

/*
** Macros for common operations
*/
#define hasmultret(k)       ((k) == VCALL || (k) == VVARARG)
#define eqstr(a,b)          ((a) == (b))

#endif /* aparser_h */ 