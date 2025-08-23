/*
** $Id: aparser.c $
** AQL Parser
** See Copyright Notice in aql.h
*/

#define aparser_c
#define AQL_CORE

#include "aconf.h"

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "aql.h"
#include "acodegen.h"
#include "adebug.h"
#include "ado.h"
#include "afunc.h"
#include "alex.h"
#include "amem.h"
#include "aobject.h"
#include "aopcodes.h"
#include "aparser.h"
#include "astring.h"
#include "acontainer.h"

/* maximum number of local variables per function (must be smaller
   than 250, due to the bytecode format) */
#define MAXVARS		200

#define hasmultret(k)		((k) == VCALL || (k) == VVARARG)

/* because all strings are unified by the scanner, the parser
   can use pointer equality for string equality */
#define eqstr(a,b)	((a) == (b))

/*
** nodes for block list (list of active blocks)
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

/*
** prototypes for recursive non-terminal functions
*/
static void statement (LexState *ls);
static void expr (LexState *ls, expdesc *v);

/* Forward declarations for expression evaluation */
static int expdesc_is_true(expdesc *e);

static l_noret error_expected (LexState *ls, int token) {
  aqlX_syntaxerror(ls,
      aqlO_pushfstring(ls->L, "%s expected", aqlX_token2str(ls, token)));
}

static l_noret errorlimit (FuncState *fs, int limit, const char *what) {
  aql_State *L = fs->ls->L;
  const char *msg;
  int line = fs->f->linedefined;
  const char *where = (line == 0)
                      ? "main function"
                      : aqlO_pushfstring(L, "function at line %d", line);
  msg = aqlO_pushfstring(L, "too many %s (limit is %d) in %s",
                             what, limit, where);
  aqlX_syntaxerror(fs->ls, msg);
}

static void checklimit (FuncState *fs, int v, int l, const char *what) {
  if (v > l) errorlimit(fs, l, what);
}

/*
** Test whether next token is 'c'; if so, skip it.
*/
static int testnext (LexState *ls, int c) {
  if (ls->t.token == c) {
    aqlX_next(ls);
    return 1;
  }
  else return 0;
}

/*
** Check that next token is 'c'.
*/
static void check (LexState *ls, int c) {
  if (ls->t.token != c)
    error_expected(ls, c);
}

/*
** Check that next token is 'c' and skip it.
*/
static void checknext (LexState *ls, int c) {
  check(ls, c);
  aqlX_next(ls);
}

#define check_condition(ls,c,msg)	{ if (!(c)) aqlX_syntaxerror(ls, msg); }

/*
** Check that next token is 'what' and skip it. In case of error,
** raise an error that the expected 'what' should match a 'who'
** in line 'where' (if that is not the current line).
*/
static void check_match (LexState *ls, int what, int who, int where) {
  if (l_unlikely(!testnext(ls, what))) {
    if (where == ls->linenumber)  /* all in the same line? */
      error_expected(ls, what);  /* do not need a complex message */
    else {
      aqlX_syntaxerror(ls, aqlO_pushfstring(ls->L,
             "%s expected (to close %s at line %d)",
              aqlX_token2str(ls, what), aqlX_token2str(ls, who), where));
    }
  }
}

static TString *str_checkname (LexState *ls) {
  TString *ts;
  check(ls, TK_NAME);
  ts = ls->t.seminfo.ts;
  aqlX_next(ls);
  return ts;
}

static void init_exp (expdesc *e, expkind k, int i) {
  e->f = e->t = NO_JUMP;
  e->k = k;
  e->u.info = i;
}

static void codestring (expdesc *e, TString *s) {
  e->f = e->t = NO_JUMP;
  e->k = VKSTR;
  e->u.strval = s;
}

static void codename (LexState *ls, expdesc *e) {
  codestring(e, str_checkname(ls));
}

/*
** Register a new local variable in the active 'Proto' (for debug
** information).
*/
static int registerlocalvar (LexState *ls, FuncState *fs, TString *varname) {
  Proto *f = fs->f;
  int oldsize = f->sizelocvars;
  aqlM_growvector(ls->L, f->locvars, fs->ndebugvars, f->sizelocvars,
                  LocVar, SHRT_MAX, "local variables");
  while (oldsize < f->sizelocvars)
    f->locvars[oldsize++].varname = NULL;
  f->locvars[fs->ndebugvars].varname = varname;
  f->locvars[fs->ndebugvars].startpc = fs->pc;
  /* TODO: aqlC_objbarrier(ls->L, f, varname); */
  return fs->ndebugvars++;
}

/*
** Create a new local variable with the given 'name'. Return its index
** in the function.
*/
static int new_localvar (LexState *ls, TString *name) {
  aql_State *L = ls->L;
  FuncState *fs = ls->fs;
  Dyndata *dyd = ls->dyd;
  Vardesc *var;
  checklimit(fs, dyd->actvar.n + 1 - fs->firstlocal,
                 MAXVARS, "local variables");
  aqlM_growvector(L, dyd->actvar.arr, dyd->actvar.n + 1,
                  dyd->actvar.size, Vardesc, SHRT_MAX, "local variables");
  var = &dyd->actvar.arr[dyd->actvar.n++];
  var->vd.kind = VDKREG;  /* default */
  var->vd.name = name;
  return dyd->actvar.n - 1 - fs->firstlocal;
}

#define new_localvarliteral(ls,v) \
    new_localvar(ls,  \
      aqlX_newstring(ls, "" v, (sizeof(v)/sizeof(char)) - 1));

/*
** Return the "variable description" (Vardesc) of a given variable.
** (Unless noted otherwise, all variables are referred to by their
** compiler indices.)
*/
static Vardesc *getlocalvardesc (FuncState *fs, int vidx) {
  return &fs->ls->dyd->actvar.arr[fs->firstlocal + vidx];
}

/*
** Convert 'nvar', a compiler index level, to its corresponding
** register. For that, search for the highest variable below that level
** that is in a register and uses its register index ('ridx') plus one.
*/
static int reglevel (FuncState *fs, int nvar) {
  while (nvar-- > 0) {
    Vardesc *vd = getlocalvardesc(fs, nvar);  /* get previous variable */
    if (vd->vd.kind != RDKCTC)  /* is in a register? */
      return vd->vd.ridx + 1;
  }
  return 0;  /* no variables in registers */
}

/*
** Return the number of variables in the register stack for the given
** function.
*/
int aqlY_nvarstack (FuncState *fs) {
  return reglevel(fs, fs->nactvar);
}

/*
** Get the debug-information entry for current variable 'vidx'.
*/
static LocVar *localdebuginfo (FuncState *fs, int vidx) {
  Vardesc *vd = getlocalvardesc(fs,  vidx);
  if (vd->vd.kind == RDKCTC)
    return NULL;  /* no debug info. for constants */
  else {
    int idx = vd->vd.pidx;
    aql_assert(idx < fs->ndebugvars);
    return &fs->f->locvars[idx];
  }
}

/*
** Create an expression representing variable 'vidx'
*/
static void init_var (FuncState *fs, expdesc *e, int vidx) {
  e->f = e->t = NO_JUMP;
  e->k = VLOCAL;
  e->u.var.vidx = vidx;
  e->u.var.ridx = getlocalvardesc(fs, vidx)->vd.ridx;
}

/*
** Start the scope for the last 'nvars' created variables.
*/
static void adjustlocalvars (LexState *ls, int nvars) {
  FuncState *fs = ls->fs;
  int reglevel = aqlY_nvarstack(fs);
  int i;
  for (i = 0; i < nvars; i++) {
    int vidx = fs->nactvar++;
    Vardesc *var = getlocalvardesc(fs, vidx);
    var->vd.ridx = reglevel++;
    var->vd.pidx = registerlocalvar(ls, fs, var->vd.name);
  }
}

/*
** Close the scope for all variables up to level 'tolevel'.
** (debug info.)
*/
static void removevars (FuncState *fs, int tolevel) {
  fs->ls->dyd->actvar.n -= (fs->nactvar - tolevel);
  while (fs->nactvar > tolevel) {
    LocVar *var = localdebuginfo(fs, --fs->nactvar);
    if (var)  /* does it have debug information? */
      var->endpc = fs->pc;
  }
}

/*
** Search the upvalues of the function 'fs' for one
** with the given 'name'.
*/
static int searchupvalue (FuncState *fs, TString *name) {
  int i;
  Upvaldesc *up = fs->f->upvalues;
  for (i = 0; i < fs->nups; i++) {
    if (eqstr(up[i].name, name)) return i;
  }
  return -1;  /* not found */
}

static Upvaldesc *allocupvalue (FuncState *fs) {
  Proto *f = fs->f;
  int oldsize = f->sizeupvalues;
  checklimit(fs, fs->nups + 1, MAXUPVAL, "upvalues");
  aqlM_growvector(fs->ls->L, f->upvalues, fs->nups, f->sizeupvalues,
                  Upvaldesc, MAXUPVAL, "upvalues");
  while (oldsize < f->sizeupvalues)
    f->upvalues[oldsize++].name = NULL;
  return &f->upvalues[fs->nups++];
}

static int newupvalue (FuncState *fs, TString *name, expdesc *v) {
  Upvaldesc *up = allocupvalue(fs);
  FuncState *prev = fs->prev;
  if (v->k == VLOCAL) {
    up->instack = 1;
    up->idx = v->u.var.ridx;
    up->kind = getlocalvardesc(prev, v->u.var.vidx)->vd.kind;
    aql_assert(eqstr(name, getlocalvardesc(prev, v->u.var.vidx)->vd.name));
  }
  else {
    up->instack = 0;
    up->idx = cast_byte(v->u.info);
    up->kind = prev->f->upvalues[v->u.info].kind;
    aql_assert(eqstr(name, prev->f->upvalues[v->u.info].name));
  }
  up->name = name;
  /* TODO: aqlC_objbarrier(fs->ls->L, fs->f, name); */
  return fs->nups - 1;
}

/*
** Look for an active local variable with the name 'n' in the
** function 'fs'. If found, initialize 'var' with it and return
** its expression kind; otherwise return -1.
*/
static int searchvar (FuncState *fs, TString *n, expdesc *var) {
  int i;
  for (i = cast_int(fs->nactvar) - 1; i >= 0; i--) {
    Vardesc *vd = getlocalvardesc(fs, i);
    if (eqstr(n, vd->vd.name)) {  /* found? */
      if (vd->vd.kind == RDKCTC)  /* compile-time constant? */
        init_exp(var, VCONST, fs->firstlocal + i);
      else  /* real variable */
        init_var(fs, var, i);
      return var->k;
    }
  }
  return -1;  /* not found */
}

/*
** Mark block where variable at given level was defined
** (to emit close instructions later).
*/
static void markupval (FuncState *fs, int level) {
  BlockCnt *bl = fs->bl;
  while (bl->nactvar > level)
    bl = bl->previous;
  bl->upval = 1;
  fs->needclose = 1;
}

/*
** Find a variable with the given name 'n'. If it is an upvalue, add
** this upvalue into all intermediate functions. If it is a global, set
** 'var' as 'void' as a flag.
*/
static void singlevaraux (FuncState *fs, TString *n, expdesc *var, int base) {
  if (fs == NULL)  /* no more levels? */
    init_exp(var, VVOID, 0);  /* default is global */
  else {
    int v = searchvar(fs, n, var);  /* look up locals at current level */
    if (v >= 0) {  /* found? */
      if (v == VLOCAL && !base)
        markupval(fs, var->u.var.vidx);  /* local will be used as an upval */
    }
    else {  /* not found as local at current level; try upvalues */
      int idx = searchupvalue(fs, n);  /* try existing upvalues */
      if (idx < 0) {  /* not found? */
        singlevaraux(fs->prev, n, var, 0);  /* try upper levels */
        if (var->k == VLOCAL || var->k == VUPVAL)  /* local or upvalue? */
          idx  = newupvalue(fs, n, var);  /* will be a new upvalue */
        else  /* it is a global or a constant */
          return;  /* don't need to do anything at this level */
      }
      init_exp(var, VUPVAL, idx);  /* new or old upvalue */
    }
  }
}

/*
** Find a variable with the given name 'n', handling global variables
** too.
*/
static void singlevar (LexState *ls, expdesc *var) {
  TString *varname = str_checkname(ls);
  FuncState *fs = ls->fs;
  singlevaraux(fs, varname, var, 1);
  if (var->k == VVOID) {  /* global name? */
    expdesc key;
    singlevaraux(fs, ls->envn, var, 1);  /* get environment variable */
    aql_assert(var->k != VVOID);  /* this one must exist */
    /* TODO: aqlK_exp2anyregup(fs, var); */ /* but could be a constant */
    codestring(&key, varname);  /* key is variable name */
    /* TODO: aqlK_indexed(fs, var, &key); */ /* env[varname] */
  }
}

static void enterblock (FuncState *fs, BlockCnt *bl, aql_byte isloop) {
  bl->isloop = isloop;
  bl->nactvar = fs->nactvar;
  bl->firstlabel = fs->ls->dyd->label.n;
  bl->firstgoto = fs->ls->dyd->gt.n;
  bl->upval = 0;
  bl->insidetbc = (fs->bl != NULL && fs->bl->insidetbc);
  bl->previous = fs->bl;
  fs->bl = bl;
  aql_assert(fs->freereg == aqlY_nvarstack(fs));
}

static void leaveblock (FuncState *fs) {
  BlockCnt *bl = fs->bl;
  LexState *ls = fs->ls;
  int hasclose = 0;
  int stklevel = reglevel(fs, bl->nactvar);  /* level outside the block */
  removevars(fs, bl->nactvar);  /* remove block locals */
  aql_assert(bl->nactvar == fs->nactvar);  /* back to level on entry */
  if (bl->isloop)  /* has to fix pending breaks? */
    hasclose = 1;  /* simplified: assume we need close for loops */
  if (!hasclose && bl->previous && bl->upval)  /* still need a 'close'? */
    /* emit close instruction if needed */;
  fs->freereg = stklevel;  /* free registers */
  ls->dyd->label.n = bl->firstlabel;  /* remove local labels */
  fs->bl = bl->previous;  /* current block now is previous one */
}

#define enterlevel(ls)	/* TODO: aqlE_incCstack(ls->L) */
#define leavelevel(ls) /* TODO: ((ls)->L->nCcalls--) */

/*
** check whether current token is in the follow set of a block.
*/
static int block_follow (LexState *ls, int withuntil) {
  switch (ls->t.token) {
    case TK_ELSE: case TK_ELIF:
    case '}': case TK_EOS:
      return 1;
    default: return 0;
  }
}

static void statlist (LexState *ls) {
  /* statlist -> { stat [';'] } */
  while (!block_follow(ls, 1)) {
    if (ls->t.token == TK_RETURN) {
      statement(ls);
      return;  /* 'return' must be last statement */
    }
    statement(ls);
  }
}

/*
** Simple expression parsing - basic literals and variables
*/
static void simpleexp (LexState *ls, expdesc *v) {
  /* simpleexp -> FLT | INT | STRING | NIL | TRUE | FALSE | NAME | '(' expr ')' */
  switch (ls->t.token) {
    case TK_FLT: {
      init_exp(v, VKFLT, 0);
      v->u.nval = ls->t.seminfo.r;
      break;
    }
    case TK_INT: {
      init_exp(v, VKINT, 0);
      v->u.ival = ls->t.seminfo.i;
      break;
    }
    case TK_STRING: {
      codestring(v, ls->t.seminfo.ts);
      break;
    }
    case TK_NIL: {
      init_exp(v, VNIL, 0);
      break;
    }
    case TK_TRUE: {
      init_exp(v, VTRUE, 0);
      break;
    }
    case TK_FALSE: {
      init_exp(v, VFALSE, 0);
      break;
    }
    case TK_NAME: {
      singlevar(ls, v);
      return;
    }
    case TK_LPAREN: {
      int line = ls->linenumber;
      aqlX_next(ls);
      expr(ls, v);
      check_match(ls, TK_RPAREN, TK_LPAREN, line);
      /* TODO: discharge variables */
      return;
    }
    default: {
      aqlX_syntaxerror(ls, "unexpected symbol");
    }
  }
  aqlX_next(ls);
}

static UnOpr getunopr (int op) {
  switch (op) {
    case TK_NOT: return OPR_NOT;    /* ! */
    case TK_MINUS: return OPR_MINUS;
    case TK_BNOT: return OPR_BNOT;  /* ~ */
    case '#': return OPR_LEN;       /* TODO: add TK_LEN to lexer */
    default: return OPR_NOUNOPR;
  }
}

static BinOpr getbinopr (int op) {
  switch (op) {
    case TK_PLUS: return OPR_ADD;
    case TK_MINUS: return OPR_SUB;
    case TK_MUL: return OPR_MUL;
    case TK_MOD: return OPR_MOD;
    case TK_POW: return OPR_POW;  /* TODO: add TK_POW to lexer */
    case TK_DIV: return OPR_DIV;
    case TK_IDIV: return OPR_IDIV;
    case TK_DIV_KW: return OPR_IDIV;  /* div keyword maps to integer division */
    case TK_BAND: return OPR_BAND;
    case TK_BOR: return OPR_BOR;
    case TK_BXOR: return OPR_BXOR;
    case TK_SHL: return OPR_SHL;
    case TK_SHR: return OPR_SHR;
    case TK_CONCAT: return OPR_CONCAT;
    case TK_NE: return OPR_NE;
    case TK_EQ: return OPR_EQ;
    case TK_LT: return OPR_LT;
    case TK_LE: return OPR_LE;
    case TK_GT: return OPR_GT;
    case TK_GE: return OPR_GE;
    case TK_LAND: return OPR_AND;  /* && */
    case TK_LOR: return OPR_OR;    /* || */
    default: return OPR_NOBINOPR;
  }
}

/*
** Priority table for binary operators.
*/
static const struct {
  aql_byte left;  /* left priority for each binary operator */
  aql_byte right; /* right priority */
} priority[] = {  /* ORDER OPR */
  /* arithmetic */
  {10, 10}, {10, 10},           /* '+' '-' */
  {11, 11}, {11, 11},           /* '*' '%' */
  {14, 13},                     /* '^' (right associative) */
  {11, 11}, {11, 11},           /* '/' '//' */
  /* bitwise */
  {6, 6}, {4, 4}, {5, 5},       /* '&' '|' '^' */
  {7, 7}, {7, 7},               /* '<<' '>>' */
  /* concatenation */
  {9, 8},                       /* '..' (right associative) */
  /* comparison */
  {3, 3}, {3, 3}, {3, 3},       /* '==', '<', '<=' */
  {3, 3}, {3, 3}, {3, 3},       /* '~=', '>', '>=' */
  /* logical */
  {2, 2}, {1, 1}                /* 'and', 'or' */
};

#define UNARY_PRIORITY	12  /* priority for unary operators */



/*
** subexpr -> (simpleexp | unop subexpr) { binop subexpr }
** where 'binop' is any binary operator with a priority higher than 'limit'
*/
static BinOpr subexpr (LexState *ls, expdesc *v, int limit) {
  BinOpr op;
  UnOpr uop;
  enterlevel(ls);
  uop = getunopr(ls->t.token);
  if (uop != OPR_NOUNOPR) {  /* prefix (unary) operator? */
    int line = ls->linenumber;
    aqlX_next(ls);  /* skip operator */
    subexpr(ls, v, UNARY_PRIORITY);
    aqlK_prefix(ls->fs, uop, v, line);
  }
  else simpleexp(ls, v);
  /* expand while operators have priorities higher than 'limit' */
  op = getbinopr(ls->t.token);
  while (op != OPR_NOBINOPR && priority[op].left > limit) {
    expdesc v2;
    BinOpr nextop;
    int line = ls->linenumber;
    aqlX_next(ls);  /* skip operator */
    aqlK_infix(ls->fs, op, v);
    /* read sub-expression with higher priority */
    nextop = subexpr(ls, &v2, priority[op].right);
    aqlK_posfix(ls->fs, op, v, &v2, line);
    op = nextop;
  }
  
  /* Handle ternary operator ? : (right associative, lowest precedence) */
  if (ls->t.token == TK_QUESTION && limit == 0) {
    expdesc vtrue, vfalse;
    int line = ls->linenumber;
    
    aqlX_next(ls);  /* skip '?' */
    
    /* Parse true expression (with lowest precedence to allow nested ternary) */
    subexpr(ls, &vtrue, 0);
    
    /* Expect ':' */
    if (ls->t.token != TK_COLON) {
      aqlX_syntaxerror(ls, "':' expected in ternary operator");
    }
    aqlX_next(ls);  /* skip ':' */
    
    /* Parse false expression (right associative, so use limit) */
    subexpr(ls, &vfalse, limit);
    
    /* Evaluate condition and select result (simplified for REPL) */
    int condition_true = expdesc_is_true(v);
    *v = condition_true ? vtrue : vfalse;
  }
  
  leavelevel(ls);
  return op;  /* return first untreated operator */
}

static void expr (LexState *ls, expdesc *v) {
  subexpr(ls, v, 0);
}

static void block (LexState *ls) {
  /* block -> statlist */
  FuncState *fs = ls->fs;
  BlockCnt bl;
  enterblock(fs, &bl, 0);
  statlist(ls);
  leaveblock(fs);
}

/*
** AQL if statement: if expr { statlist } { elif expr { statlist } } [ else { statlist } ]
*/
static void ifstat (LexState *ls, int line) {
  /* ifstat -> IF cond '{' block '}' {ELIF cond '{' block '}'} [ELSE '{' block '}'] */
  FuncState *fs = ls->fs;
  expdesc v;
  int escapelist = NO_JUMP;  /* exit list for finished parts */
  
  /* IF part */
  aqlX_next(ls);  /* skip IF */
  expr(ls, &v);  /* read condition */
  checknext(ls, '{');
  /* TODO: handle conditional jump */
  block(ls);  /* 'then' part */
  check_match(ls, '}', '{', line);
  
  /* ELIF parts */
  while (ls->t.token == TK_ELIF) {
    aqlX_next(ls);  /* skip ELIF */
    expr(ls, &v);  /* read condition */
    checknext(ls, '{');
    /* TODO: handle conditional jump */
    block(ls);  /* 'elif' part */
    check_match(ls, '}', '{', line);
  }
  
  /* ELSE part */
  if (testnext(ls, TK_ELSE)) {
    checknext(ls, '{');
    block(ls);  /* 'else' part */
    check_match(ls, '}', '{', line);
  }
  
  /* TODO: patch escape list */
}

/*
** AQL while statement: while expr { statlist }
*/
static void whilestat (LexState *ls, int line) {
  /* whilestat -> WHILE cond '{' block '}' */
  FuncState *fs = ls->fs;
  int whileinit;
  int condexit;
  BlockCnt bl;
  expdesc v;
  
  aqlX_next(ls);  /* skip WHILE */
  whileinit = fs->pc;  /* save loop start position */
  expr(ls, &v);  /* read condition */
  /* TODO: condexit = handle condition */
  enterblock(fs, &bl, 1);
  checknext(ls, '{');
  block(ls);
  check_match(ls, '}', '{', line);
  /* TODO: jump back to loop start */
  leaveblock(fs);
  /* TODO: patch condition exit */
}

/*
** AQL let statement: let name [: type] = expr
*/
static void letstat (LexState *ls) {
  /* letstat -> LET NAME [':' type] '=' expr */
  FuncState *fs = ls->fs;
  expdesc e;
  int vidx;
  TString *varname;
  
  aqlX_next(ls);  /* skip LET */
  varname = str_checkname(ls);  /* variable name */
  vidx = new_localvar(ls, varname);
  
  /* optional type annotation */
  if (testnext(ls, ':')) {
    /* TODO: parse type annotation */
    str_checkname(ls);  /* skip type for now */
  }
  
  checknext(ls, '=');
  expr(ls, &e);  /* parse initialization expression */
  /* TODO: store expression result */
  adjustlocalvars(ls, 1);
}

/*
** AQL assignment: name := expr or name = expr
*/
static void assignment (LexState *ls) {
  expdesc v;
  singlevar(ls, &v);
  
  if (testnext(ls, TK_ASSIGN) || testnext(ls, '=')) {  /* := or = */
    expdesc e;
    expr(ls, &e);  /* parse right-hand side */
    /* TODO: store assignment */
  }
  else {
    aqlX_syntaxerror(ls, "unexpected symbol");
  }
}

/*
** parse a single statement
*/
static void statement (LexState *ls) {
  int line = ls->linenumber;  /* may be needed for error messages */
  enterlevel(ls);
  switch (ls->t.token) {
    case ';': {  /* stat -> ';' (empty statement) */
      aqlX_next(ls);  /* skip ';' */
      break;
    }
    case TK_IF: {  /* stat -> ifstat */
      ifstat(ls, line);
      break;
    }
    case TK_WHILE: {  /* stat -> whilestat */
      whilestat(ls, line);
      break;
    }
    case TK_LET: {  /* stat -> letstat */
      letstat(ls);
      break;
    }
    case '{': {  /* stat -> '{' block '}' */
      aqlX_next(ls);  /* skip '{' */
      block(ls);
      check_match(ls, '}', '{', line);
      break;
    }
    case TK_RETURN: {  /* stat -> retstat */
      aqlX_next(ls);  /* skip RETURN */
      /* TODO: implement return statement */
      break;
    }
    case TK_BREAK: {  /* stat -> breakstat */
      aqlX_next(ls);  /* skip BREAK */
      /* TODO: implement break statement */
      break;
    }
    default: {  /* stat -> assignment or expression statement */
      assignment(ls);
      break;
    }
  }
  aql_assert(ls->fs->f->maxstacksize >= ls->fs->freereg &&
             ls->fs->freereg >= aqlY_nvarstack(ls->fs));
  ls->fs->freereg = aqlY_nvarstack(ls->fs);  /* free registers */
  leavelevel(ls);
}

static void open_func (LexState *ls, FuncState *fs, BlockCnt *bl) {
  Proto *f = fs->f;
  fs->prev = ls->fs;  /* linked list of funcstates */
  fs->ls = ls;
  ls->fs = fs;
  fs->pc = 0;
  fs->previousline = f->linedefined;
  fs->iwthabs = 0;
  fs->lasttarget = 0;
  fs->freereg = 0;
  fs->nk = 0;
  fs->nabslineinfo = 0;
  fs->np = 0;
  fs->nups = 0;
  fs->ndebugvars = 0;
  fs->nactvar = 0;
  fs->needclose = 0;
  fs->firstlocal = ls->dyd->actvar.n;
  fs->firstlabel = ls->dyd->label.n;
  fs->bl = NULL;
  f->source = ls->source;
  /* TODO: aqlC_objbarrier(ls->L, f, f->source); */
  f->maxstacksize = 2;  /* registers 0/1 are always valid */
  enterblock(fs, bl, 0);
}

static void close_func (LexState *ls) {
  aql_State *L = ls->L;
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  /* TODO: generate final return instruction */
  leaveblock(fs);
  aql_assert(fs->bl == NULL);
  /* TODO: finish code generation */
  ls->fs = fs->prev;
  /* TODO: trigger garbage collection if needed */
}

/*
** compiles the main function, which is a regular vararg function with an
** upvalue named AQL_ENV
*/
static void mainfunc (LexState *ls, FuncState *fs) {
  BlockCnt bl;
  Upvaldesc *env;
  open_func(ls, fs, &bl);
  fs->f->is_vararg = 1;  /* main function is always declared vararg */
  env = allocupvalue(fs);  /* ...set environment upvalue */
  env->instack = 1;
  env->idx = 0;
  env->kind = VDKREG;
  env->name = ls->envn;
  /* TODO: aqlC_objbarrier(ls->L, fs->f, env->name); */
  aqlX_next(ls);  /* read first token */
  statlist(ls);  /* parse main body */
  check(ls, TK_EOS);
  close_func(ls);
}

LClosure *aqlY_parser (aql_State *L, struct Zio *z, Mbuffer *buff,
                       Dyndata *dyd, const char *name, int firstchar) {
  LexState lexstate;
  FuncState funcstate;
  LClosure *cl = aqlF_newLclosure(L, 1);  /* create main closure */
  setclLvalue2s(L, L->top, cl);  /* anchor it (to avoid being collected) */
  /* TODO: aqlD_inctop(L); */
  /* TODO: lexstate.h = aqlH_new(L); */ /* create table for scanner */
  lexstate.h = NULL;  /* temporary fix */
  /* TODO: sethvalue2s(L, L->top, lexstate.h); */ /* anchor it */
  /* TODO: aqlD_inctop(L); */
  funcstate.f = cl->p = aqlF_newproto(L);
  /* TODO: aqlC_objbarrier(L, cl, cl->p); */
  funcstate.f->source = aqlStr_newlstr(L, name, strlen(name));  /* create and anchor TString */
  /* TODO: aqlC_objbarrier(L, funcstate.f, funcstate.f->source); */
  lexstate.buff = buff;
  lexstate.dyd = dyd;
  dyd->actvar.n = dyd->gt.n = dyd->label.n = 0;
  aqlX_setinput(L, &lexstate, z, funcstate.f->source, firstchar);
  mainfunc(&lexstate, &funcstate);
  aql_assert(!funcstate.prev && funcstate.nups == 1 && !lexstate.fs);
  /* all scopes should be correctly finished */
  aql_assert(dyd->actvar.n == 0 && dyd->gt.n == 0 && dyd->label.n == 0);
  L->top--;  /* remove scanner's table */
  return cl;  /* closure is on the stack, too */
}

/*
** Simple evaluation of expdesc for REPL mode
** This is a temporary solution until full code generation is implemented
*/
static int eval_expdesc(expdesc *e, double *result) {
  switch (e->k) {
    case VKINT:
      *result = (double)e->u.ival;
      return 0;
    case VKFLT:
      *result = e->u.nval;
      return 0;
    case VNIL:
      *result = 0.0;
      return 0;
    case VTRUE:
      *result = 1.0;
      return 0;
    case VFALSE:
      *result = 0.0;
      return 0;
    default:
      return -1;  /* unsupported expression type for simple evaluation */
  }
}

/*
** Simple allocator for temporary state
*/
static void *simple_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  UNUSED(ud); UNUSED(osize);
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  return realloc(ptr, nsize);
}

/*
** Parse and evaluate an expression string using a hybrid approach
** Uses the correct Lua-style precedence but with simplified evaluation
** Returns 0 on success, -1 on error
*/

/* Forward declarations for hybrid parser */
static int parse_expression_hybrid(const char **pos, double *result);
static int parse_term_hybrid(const char **pos, double *result);
static int parse_factor_hybrid(const char **pos, double *result);
static int parse_power_hybrid(const char **pos, double *result);

/* Skip whitespace */
static void skip_whitespace_hybrid(const char **pos) {
  while (**pos && (**pos == ' ' || **pos == '\t')) {
    (*pos)++;
  }
}

/* Parse a number */
static int parse_number_hybrid(const char **pos, double *result) {
  skip_whitespace_hybrid(pos);
  char *endptr;
  *result = strtod(*pos, &endptr);
  if (endptr == *pos) return -1;  /* no number found */
  *pos = endptr;
  return 0;
}

/* Parse power (highest precedence, right associative): factor ^ power | factor */
static int parse_power_hybrid(const char **pos, double *result) {
  if (parse_factor_hybrid(pos, result) != 0) return -1;
  
  skip_whitespace_hybrid(pos);
  if (**pos == '^') {
    (*pos)++;
    double right;
    if (parse_power_hybrid(pos, &right) != 0) return -1;  /* right associative */
    *result = pow(*result, right);
  }
  return 0;
}

/* Parse factor: number | '(' expression ')' */
static int parse_factor_hybrid(const char **pos, double *result) {
  skip_whitespace_hybrid(pos);
  
  if (**pos == '(') {
    (*pos)++;
    if (parse_expression_hybrid(pos, result) != 0) return -1;
    skip_whitespace_hybrid(pos);
    if (**pos != ')') return -1;
    (*pos)++;
    return 0;
  }
  
  return parse_number_hybrid(pos, result);
}

/* Parse term: power { ('*'|'/') power } */
static int parse_term_hybrid(const char **pos, double *result) {
  if (parse_power_hybrid(pos, result) != 0) return -1;
  
  while (1) {
    skip_whitespace_hybrid(pos);
    char op = **pos;
    if (op != '*' && op != '/') break;
    
    (*pos)++;
    double right;
    if (parse_power_hybrid(pos, &right) != 0) return -1;
    
    if (op == '*') {
      *result *= right;
    } else {  /* op == '/' */
      if (right == 0.0) return -1;  /* division by zero */
      *result /= right;
    }
  }
  return 0;
}

/* Parse expression: term { ('+'|'-') term } */
static int parse_expression_hybrid(const char **pos, double *result) {
  if (parse_term_hybrid(pos, result) != 0) return -1;
  
  while (1) {
    skip_whitespace_hybrid(pos);
    char op = **pos;
    if (op != '+' && op != '-') break;
    
    (*pos)++;
    double right;
    if (parse_term_hybrid(pos, &right) != 0) return -1;
    
    if (op == '+') {
      *result += right;
    } else {  /* op == '-' */
      *result -= right;
    }
  }
  return 0;
}

/*
** Priority table for operators (Lua-style)
** Lower numbers = lower precedence
*/
static const struct {
  aql_byte left;   /* left priority for each binary operator */
  aql_byte right;  /* right priority */
} op_priority[] = {
  /* Ternary (handled separately) */
  {0, 0},          /* ? : (placeholder) */
  /* Logical OR */
  {1, 1},          /* || */
  /* Logical AND */  
  {2, 2},          /* && */
  /* Bitwise OR */
  {3, 3},          /* | */
  /* Bitwise XOR */
  {4, 4},          /* ^ */
  /* Bitwise AND */
  {5, 5},          /* & */
  /* Equality */
  {6, 6}, {6, 6},  /* == != */
  /* Relational */
  {7, 7}, {7, 7}, {7, 7}, {7, 7},  /* < > <= >= */
  /* Shift */
  {8, 8}, {8, 8},  /* << >> */
  /* Additive */
  {9, 9}, {9, 9},  /* + - */
  /* Multiplicative */
  {10, 10}, {10, 10}, {10, 10},  /* * / % */
  /* Power (right associative) */
  {12, 11},        /* ^ */
};

#define UNARY_PRIORITY_V3  13  /* priority for unary operators */

/*
** Map token to binary operator index in priority table
*/
static int get_binop_priority_index(int token) {
  switch (token) {
    case TK_LOR:   return 1;   /* || */
    case TK_LAND:  return 2;   /* && */
    case TK_BOR:   return 3;   /* | */
    case TK_BXOR:  return 4;   /* ^ */
    case TK_BAND:  return 5;   /* & */
    case TK_EQ:    return 6;   /* == */
    case TK_NE:    return 7;   /* != */
    case TK_LT:    return 8;   /* < */
    case TK_GT:    return 9;   /* > */
    case TK_LE:    return 10;  /* <= */
    case TK_GE:    return 11;  /* >= */
    case TK_SHL:   return 12;  /* << */
    case TK_SHR:   return 13;  /* >> */
    case TK_PLUS:  return 14;  /* + */
    case TK_MINUS: return 15;  /* - */
    case TK_MUL:   return 16;  /* * */
    case TK_DIV:   return 17;  /* / */
    case TK_MOD:   return 18;  /* % */
    default:       return -1;  /* not a binary operator */
  }
}

/*
** Apply binary operation (simplified evaluation)
*/
static int apply_binop_v3(int token, double left, double right, double *result) {
  switch (token) {
    case TK_PLUS:  *result = left + right; break;
    case TK_MINUS: *result = left - right; break;
    case TK_MUL:   *result = left * right; break;
    case TK_DIV:   
      if (right == 0.0) return -1;  /* division by zero */
      *result = left / right; 
      break;
    case TK_MOD:   
      if (right == 0.0) return -1;  /* modulo by zero */
      *result = fmod(left, right); 
      break;
    
    /* Comparison operators */
    case TK_EQ:    *result = (left == right) ? 1.0 : 0.0; break;
    case TK_NE:    *result = (left != right) ? 1.0 : 0.0; break;
    case TK_LT:    *result = (left < right) ? 1.0 : 0.0; break;
    case TK_GT:    *result = (left > right) ? 1.0 : 0.0; break;
    case TK_LE:    *result = (left <= right) ? 1.0 : 0.0; break;
    case TK_GE:    *result = (left >= right) ? 1.0 : 0.0; break;
    
    /* Bitwise operators */
    case TK_BAND: {
      long long ileft = (long long)left;
      long long iright = (long long)right;
      *result = (double)(ileft & iright);
      break;
    }
    case TK_BOR: {
      long long ileft = (long long)left;
      long long iright = (long long)right;
      *result = (double)(ileft | iright);
      break;
    }
    case TK_BXOR: {
      long long ileft = (long long)left;
      long long iright = (long long)right;
      *result = (double)(ileft ^ iright);
      break;
    }
    case TK_SHL: {
      long long ileft = (long long)left;
      long long iright = (long long)right;
      if (iright < 0 || iright >= 64) return -1;
      *result = (double)(ileft << iright);
      break;
    }
    case TK_SHR: {
      long long ileft = (long long)left;
      long long iright = (long long)right;
      if (iright < 0 || iright >= 64) return -1;
      *result = (double)(ileft >> iright);
      break;
    }
    
    /* Logical operators */
    case TK_LAND:  *result = (left != 0.0 && right != 0.0) ? 1.0 : 0.0; break;
    case TK_LOR:   *result = (left != 0.0 || right != 0.0) ? 1.0 : 0.0; break;
    
    default:
      return -1;  /* unsupported operation */
  }
  return 0;
}

/*
** Apply unary operation
*/
static int apply_unop_v3(int token, double operand, double *result) {
  switch (token) {
    case TK_PLUS:  *result = operand; break;
    case TK_MINUS: *result = -operand; break;
    case TK_LNOT:  *result = (operand == 0.0) ? 1.0 : 0.0; break;
    case TK_BNOT:  {
      long long ioperand = (long long)operand;
      *result = (double)(~ioperand);
      break;
    }
    default:
      return -1;  /* unsupported unary operator */
  }
  return 0;
}

/*
** Check if token is a unary operator
*/
static int is_unary_op(int token) {
  return (token == TK_PLUS || token == TK_MINUS || 
          token == TK_LNOT || token == TK_BNOT);
}

/*
** Lua-style precedence climbing parser for expressions
** Based on Lua 5.4's subexpr function
*/

static int subexpr_v3(LexState *ls, double *result, int limit) {
  double left_val;
  int op_idx;
  
  /* Handle unary operators */
  if (is_unary_op(ls->t.token)) {
    int unop = ls->t.token;
    aqlX_next(ls);  /* skip unary operator */
    
    if (subexpr_v3(ls, &left_val, UNARY_PRIORITY_V3) != 0) {
      return -1;
    }
    
    if (apply_unop_v3(unop, left_val, result) != 0) {
      return -1;
    }
    left_val = *result;  /* Update left_val for binary operators */
  }
  else {
    /* Parse primary expression (number or parenthesized expression) */
    if (ls->t.token == TK_INT) {
      *result = (double)ls->t.seminfo.i;
      aqlX_next(ls);
    }
    else if (ls->t.token == TK_FLT) {
      *result = ls->t.seminfo.r;
      aqlX_next(ls);
    }
    else if (ls->t.token == TK_LPAREN) {
      aqlX_next(ls);  /* skip '(' */
      if (subexpr_v3(ls, result, 0) != 0) {
        return -1;
      }
      if (ls->t.token != TK_RPAREN) {
        return -1;  /* missing ')' */
      }
      aqlX_next(ls);  /* skip ')' */
    }
    else {
      return -1;  /* unexpected token */
    }
    left_val = *result;
  }
  
  /* Handle binary operators using precedence climbing */
  while ((op_idx = get_binop_priority_index(ls->t.token)) >= 0 &&
         op_priority[op_idx].left > limit) {
    int binop = ls->t.token;
    double right_val;
    
    aqlX_next(ls);  /* skip binary operator */
    
    /* Parse right operand with higher precedence */
    if (subexpr_v3(ls, &right_val, op_priority[op_idx].right) != 0) {
      return -1;
    }
    
    /* Apply binary operation */
    if (apply_binop_v3(binop, left_val, right_val, &left_val) != 0) {
      return -1;
    }
  }
  
  /* Handle ternary operator ? : (right associative, lowest precedence) */
  printf("DEBUG: Checking ternary - token=%d (TK_INT=%d, TK_QUESTION=%d), limit=%d\n", ls->t.token, TK_INT, TK_QUESTION, limit);
  if (ls->t.token == TK_QUESTION && limit == 0) {
    double true_val, false_val;
    
    printf("DEBUG: Found ternary operator, condition = %.6g\n", left_val);
    aqlX_next(ls);  /* skip '?' */
    
    /* Parse true expression (with lowest precedence to allow nested ternary) */
    printf("DEBUG: Parsing true expression...\n");
    if (subexpr_v3(ls, &true_val, 0) != 0) {
      printf("DEBUG: Error parsing true expression\n");
      return -1;
    }
    printf("DEBUG: True expression = %.6g\n", true_val);
    
    /* Expect ':' */
    printf("DEBUG: Current token = %d, expecting TK_COLON = %d\n", ls->t.token, TK_COLON);
    if (ls->t.token != TK_COLON) {
      printf("Error: Expected ':' in ternary operator, got token %d\n", ls->t.token);
      return -1;
    }
    aqlX_next(ls);  /* skip ':' */
    
    /* Parse false expression (right associative, so use limit-1) */
    printf("DEBUG: Parsing false expression...\n");
    if (subexpr_v3(ls, &false_val, limit) != 0) {
      printf("DEBUG: Error parsing false expression\n");
      return -1;
    }
    printf("DEBUG: False expression = %.6g\n", false_val);
    
    /* Evaluate condition and select result */
    left_val = (left_val != 0.0) ? true_val : false_val;
    printf("DEBUG: Ternary result = %.6g\n", left_val);
  }
  
  *result = left_val;
  return 0;
}

/*
** Lua-style expression parser (v3 - precedence climbing)
*/
AQL_API int aqlP_parse_expression_v3(const char *expr_str, double *result) {
  if (!expr_str || !result) return -1;
  
  /* Set up lexer state */
  aql_State *L = aql_newstate(simple_alloc, NULL);
  if (!L) return -1;
  
  aqlStr_init(L);
  
  struct Zio z;
  aqlZ_init_string(L, &z, expr_str, strlen(expr_str));
  
  LexState ls;
  memset(&ls, 0, sizeof(ls));
  ls.L = L;
  ls.z = &z;
  ls.linenumber = 1;
  ls.lastline = 1;
  ls.current = zgetc(&z);
  
  ls.t.token = 0;
  ls.lookahead.token = TK_EOS;
  
  TString *source = aqlStr_newlstr(L, "expr", 4);
  ls.source = source;
  
  Mbuffer buff;
  aqlZ_initbuffer(L, &buff);
  ls.buff = &buff;
  
  TString *envn = aqlStr_newlstr(L, "_ENV", 4);
  ls.envn = envn;
  
  ls.h = NULL;
  ls.dyd = NULL;
  ls.fs = NULL;
  
  /* Get first token */
  aqlX_next(&ls);
  
  /* Parse expression using precedence climbing */
  int parse_result = subexpr_v3(&ls, result, 0);
  
  /* Check for end of input */
  if (parse_result == 0 && ls.t.token != TK_EOS) {
    parse_result = -1;  /* unexpected tokens after expression */
  }
  
  /* Cleanup */
  aqlZ_freebuffer(L, &buff);
  aqlZ_cleanup_string(L, &z);
  aql_close(L);
  
  return parse_result;
}

AQL_API int aqlP_parse_expression(const char *expr_str, double *result) {
  if (!expr_str || !result) return -1;
  
  /* Set up lexer state */
  aql_State *L = aql_newstate(simple_alloc, NULL);
  if (!L) return -1;
  
  aqlStr_init(L);
  
  struct Zio z;
  aqlZ_init_string(L, &z, expr_str, strlen(expr_str));
  
  LexState ls;
  memset(&ls, 0, sizeof(ls));
  ls.L = L;
  ls.z = &z;
  ls.linenumber = 1;
  ls.lastline = 1;
  ls.current = zgetc(&z);
  
  ls.t.token = 0;
  ls.lookahead.token = TK_EOS;
  
  TString *source = aqlStr_newlstr(L, "expr", 4);
  ls.source = source;
  
  Mbuffer buff;
  aqlZ_initbuffer(L, &buff);
  ls.buff = &buff;
  
  TString *envn = aqlStr_newlstr(L, "_ENV", 4);
  ls.envn = envn;
  
  ls.h = NULL;
  ls.dyd = NULL;
  ls.fs = NULL;
  
  /* Get first token */
  aqlX_next(&ls);
  
  /* Parse expression using Lua-style expr/subexpr */
  expdesc v;
  expr(&ls, &v);
  
  /* Evaluate the expression */
  int parse_result = eval_expdesc(&v, result);
  
  /* Check for end of input */
  if (parse_result == 0 && ls.t.token != TK_EOS) {
    parse_result = -1;  /* unexpected tokens after expression */
  }
  
  /* Cleanup */
  aqlZ_freebuffer(L, &buff);
  aqlZ_cleanup_string(L, &z);
  aql_close(L);
  
  return parse_result;
}


/*
** Execute an AQL source file
** Returns 1 on success, 0 on error
*/
AQL_API int aqlP_execute_file(aql_State *L, const char *filename) {
  if (!L || !filename) return 0;
  
  FILE *f = fopen(filename, "r");
  if (!f) {
    printf("Error: Cannot open file '%s'\n", filename);
    return 0;
  }
  
  /* TODO: Implement file parsing and execution */
  printf("Executing file: %s\n", filename);
  printf("(File execution not yet implemented)\n");
  
  fclose(f);
  return 1;  /* success for now */
}

/*
** Start the Read-Eval-Print Loop (REPL)
*/
AQL_API void aqlP_repl(aql_State *L) {
  if (!L) return;
  
  printf("AQL %s Interactive Mode\n", AQL_VERSION);
  printf("Type 'exit' or press Ctrl+C to quit.\n\n");
  
  char line[1024];
  while (1) {
    printf("aql> ");
    fflush(stdout);
    
    if (!fgets(line, sizeof(line), stdin)) {
      break;  /* EOF */
    }
    
    /* Remove trailing newline */
    size_t len = strlen(line);
    if (len > 0 && line[len-1] == '\n') {
      line[len-1] = '\0';
    }
    
    /* Skip empty lines */
    if (strlen(line) == 0) continue;
    
    /* Check for exit command */
    if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
      break;
    }
    

    
    /* Try to parse as expression using hybrid parser */
    double result;
    if (aqlP_parse_expression(line, &result) == 0) {
      printf("%.6g\n", result);
    } else {
      printf("Error: Invalid expression\n");
    }
  }
  
  printf("\nGoodbye!\n");
}

/*
** Simplified code generation functions for expression evaluation
** These are placeholders until full code generation is implemented
*/

/* Apply unary operator to expression (simplified for direct evaluation) */
void aqlK_prefix(FuncState *fs, UnOpr op, expdesc *e, int line) {
  UNUSED(fs); UNUSED(line);
  
  switch (op) {
    case OPR_MINUS:
      if (e->k == VKINT) {
        e->u.ival = -e->u.ival;
      } else if (e->k == VKFLT) {
        e->u.nval = -e->u.nval;
      } else {
        /* Convert other types to numbers and negate */
        double val = 0.0;
        if (e->k == VTRUE) val = 1.0;
        else if (e->k == VFALSE || e->k == VNIL) val = 0.0;
        e->k = VKFLT;
        e->u.nval = -val;
      }
      break;
    case OPR_NOT: {
      /* Convert to boolean and negate */
      int is_true = 0;
      if (e->k == VTRUE) is_true = 1;
      else if (e->k == VKINT) is_true = (e->u.ival != 0);
      else if (e->k == VKFLT) is_true = (e->u.nval != 0.0);
      /* else VFALSE, VNIL are already false */
      
      e->k = is_true ? VFALSE : VTRUE;
      break;
    }
    case OPR_LEN:
      /* For now, length of numbers is undefined, return 0 */
      e->k = VKINT;
      e->u.ival = 0;
      break;
    case OPR_BNOT:
      /* Bitwise NOT - convert to integer first */
      if (e->k == VKINT) {
        e->u.ival = ~e->u.ival;
      } else if (e->k == VKFLT) {
        e->k = VKINT;
        e->u.ival = ~(aql_Integer)e->u.nval;
      } else {
        e->k = VKINT;
        e->u.ival = ~0;  /* NOT of non-numbers */
      }
      break;
    default:
      break;
  }
}

/* Prepare for binary operation (currently no-op) */
void aqlK_infix(FuncState *fs, BinOpr op, expdesc *v) {
  UNUSED(fs); UNUSED(op); UNUSED(v);
  /* TODO: prepare registers if needed */
}

/* Convert expression to numeric value for calculations */
static double expdesc_to_number(expdesc *e) {
  switch (e->k) {
    case VKINT: return (double)e->u.ival;
    case VKFLT: return e->u.nval;
    case VTRUE: return 1.0;
    case VFALSE: case VNIL: return 0.0;
    default: return 0.0;
  }
}

/* Convert expression to integer value for bitwise operations */
static aql_Integer expdesc_to_integer(expdesc *e) {
  switch (e->k) {
    case VKINT: return e->u.ival;
    case VKFLT: return (aql_Integer)e->u.nval;
    case VTRUE: return 1;
    case VFALSE: case VNIL: return 0;
    default: return 0;
  }
}

/* Check if expression represents a true value */
static int expdesc_is_true(expdesc *e) {
  switch (e->k) {
    case VFALSE: case VNIL: return 0;
    case VTRUE: return 1;
    case VKINT: return (e->u.ival != 0);
    case VKFLT: return (e->u.nval != 0.0);
    default: return 1;  /* non-false values are true */
  }
}

/* Apply binary operator to two expressions (simplified for direct evaluation) */
void aqlK_posfix(FuncState *fs, BinOpr op, expdesc *e1, expdesc *e2, int line) {
  UNUSED(fs); UNUSED(line);
  
  /* Handle logical operators first (short-circuit evaluation) */
  if (op == OPR_AND) {
    if (!expdesc_is_true(e1)) {
      /* e1 is false, result is e1 */
      return;
    } else {
      /* e1 is true, result is e2 */
      *e1 = *e2;
      return;
    }
  }
  
  if (op == OPR_OR) {
    if (expdesc_is_true(e1)) {
      /* e1 is true, result is e1 */
      return;
    } else {
      /* e1 is false, result is e2 */
      *e1 = *e2;
      return;
    }
  }
  
  /* For arithmetic and comparison operators, convert to numbers */
  double left = expdesc_to_number(e1);
  double right = expdesc_to_number(e2);
  double result = 0.0;
  int is_int = 0;
  
  switch (op) {
    case OPR_ADD: result = left + right; break;
    case OPR_SUB: result = left - right; break;
    case OPR_MUL: result = left * right; break;
    case OPR_DIV: 
      if (right == 0.0) {
        /* Division by zero - set to infinity */
        result = (left >= 0) ? HUGE_VAL : -HUGE_VAL;
      } else {
        result = left / right; 
      }
      break;
    case OPR_IDIV:
      if (right == 0.0) {
        /* Division by zero - set to infinity */
        result = (left >= 0) ? HUGE_VAL : -HUGE_VAL;
      } else {
        result = floor(left / right);  /* Integer division: floor(a/b) */
      }
      is_int = 1;  /* Result is always integer */
      break;
    case OPR_MOD: 
      if (right == 0.0) {
        result = 0.0;  /* Modulo by zero */
      } else {
        result = fmod(left, right); 
      }
      break;
    case OPR_POW: 
      result = pow(left, right); 
      break;
    case OPR_EQ: result = (left == right) ? 1.0 : 0.0; is_int = 1; break;
    case OPR_NE: result = (left != right) ? 1.0 : 0.0; is_int = 1; break;
    case OPR_LT: result = (left < right) ? 1.0 : 0.0; is_int = 1; break;
    case OPR_LE: result = (left <= right) ? 1.0 : 0.0; is_int = 1; break;
    case OPR_GT: result = (left > right) ? 1.0 : 0.0; is_int = 1; break;
    case OPR_GE: result = (left >= right) ? 1.0 : 0.0; is_int = 1; break;
    
    /* Bitwise operators */
    case OPR_BAND: {
      aql_Integer ileft = expdesc_to_integer(e1);
      aql_Integer iright = expdesc_to_integer(e2);
      e1->k = VKINT;
      e1->u.ival = ileft & iright;
      return;
    }
    case OPR_BOR: {
      aql_Integer ileft = expdesc_to_integer(e1);
      aql_Integer iright = expdesc_to_integer(e2);
      e1->k = VKINT;
      e1->u.ival = ileft | iright;
      return;
    }
    case OPR_BXOR: {
      aql_Integer ileft = expdesc_to_integer(e1);
      aql_Integer iright = expdesc_to_integer(e2);
      e1->k = VKINT;
      e1->u.ival = ileft ^ iright;
      return;
    }
    case OPR_SHL: {
      aql_Integer ileft = expdesc_to_integer(e1);
      aql_Integer iright = expdesc_to_integer(e2);
      e1->k = VKINT;
      e1->u.ival = ileft << iright;
      return;
    }
    case OPR_SHR: {
      aql_Integer ileft = expdesc_to_integer(e1);
      aql_Integer iright = expdesc_to_integer(e2);
      e1->k = VKINT;
      e1->u.ival = ileft >> iright;
      return;
    }
    
    default:
      return;  /* unsupported operation */
  }
  
  /* Store result back in e1 */
  if (is_int || (result == floor(result) && result >= INT_MIN && result <= INT_MAX)) {
    e1->k = VKINT;
    e1->u.ival = (aql_Integer)result;
  } else {
    e1->k = VKFLT;
    e1->u.nval = result;
  }
}