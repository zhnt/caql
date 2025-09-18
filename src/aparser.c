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
#include <stdbool.h>
#include <setjmp.h>

#include "aql.h"
#include "aapi.h"
#include "acode.h"
#include "acodegen.h"
#include "adebug_user.h"
#include "ado.h"
#include "arange.h"
#include "aarray.h"

/* Forward declarations for token collection (from alex.c) */
extern void start_token_collection(void);
extern void finish_token_collection(void);

#ifdef AQL_DEBUG_BUILD
/* AST node collection for debug output */
static AQL_ASTInfo *debug_ast_nodes = NULL;
static int debug_ast_count = 0;
static int debug_ast_capacity = 0;
static int debug_collecting_ast = 0;

/* Bytecode instruction collection for debug output */
static AQL_InstrInfo *debug_bytecode = NULL;
static int debug_bytecode_count = 0;
static int debug_bytecode_capacity = 0;
static int debug_collecting_bytecode = 0;

/* AST collection functions */
static void start_ast_collection(void) {
    if (aql_debug_enabled && (aql_debug_flags & AQL_DEBUG_PARSE)) {
        debug_collecting_ast = 1;
        debug_ast_count = 0;
        if (!debug_ast_nodes) {
            debug_ast_capacity = 64;
            debug_ast_nodes = malloc(debug_ast_capacity * sizeof(AQL_ASTInfo));
        }
    }
}

static void add_debug_ast_node(const char *type, const char *value, int line, int children_count) {
    if (!debug_collecting_ast) return;
    
    if (debug_ast_count >= debug_ast_capacity) {
        debug_ast_capacity *= 2;
        debug_ast_nodes = realloc(debug_ast_nodes, debug_ast_capacity * sizeof(AQL_ASTInfo));
    }
    
    AQL_ASTInfo *node = &debug_ast_nodes[debug_ast_count++];
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->line = line;
    node->children_count = children_count;
}

static void finish_ast_collection(void) {
    if (!debug_collecting_ast) return;
    
    aqlD_print_ast_header();
    for (int i = 0; i < debug_ast_count; i++) {
        aqlD_print_ast_node(&debug_ast_nodes[i], 0);
        if (debug_ast_nodes[i].value) {
            free((void*)debug_ast_nodes[i].value);
        }
    }
    aqlD_print_ast_footer(debug_ast_count);
    
    debug_collecting_ast = 0;
    debug_ast_count = 0;
}

/* Bytecode collection functions */
static void start_bytecode_collection(void) {
    if (aql_debug_enabled && (aql_debug_flags & AQL_DEBUG_CODE)) {
        debug_collecting_bytecode = 1;
        debug_bytecode_count = 0;
        if (!debug_bytecode) {
            debug_bytecode_capacity = 64;
            debug_bytecode = malloc(debug_bytecode_capacity * sizeof(AQL_InstrInfo));
        }
    }
}

static void collect_bytecode_from_proto(Proto *proto) {
    if (!debug_collecting_bytecode || !proto) return;
    
    /* Collect instructions from the proto */
    for (int pc = 0; pc < proto->sizecode; pc++) {
        if (debug_bytecode_count >= debug_bytecode_capacity) {
            debug_bytecode_capacity *= 2;
            debug_bytecode = realloc(debug_bytecode, debug_bytecode_capacity * sizeof(AQL_InstrInfo));
        }
        
        Instruction inst = proto->code[pc];
        OpCode op = GET_OPCODE(inst);
        
        AQL_InstrInfo *instr = &debug_bytecode[debug_bytecode_count++];
        instr->pc = pc;
        instr->opname = aql_opnames[op];
        instr->opcode = op;
        instr->a = GETARG_A(inst);
        instr->b = GETARG_B(inst);
        instr->c = GETARG_C(inst);
        instr->bx = GETARG_Bx(inst);
        instr->sbx = GETARG_sBx(inst);
        
        /* Determine instruction format */
        if (op <= OP_EXTRAARG) {
            /* Most instructions use ABC format */
            if (op == OP_LOADK || op == OP_LOADKX || op == OP_CLOSURE) {
                instr->format = "ABx";
            } else if (op == OP_LOADI || op == OP_LOADF) {
                instr->format = "AsBx";
            } else if (op == OP_EXTRAARG) {
                instr->format = "Ax";
            } else {
                instr->format = "ABC";
            }
        } else {
            instr->format = "ABC";  /* Default for other instructions */
        }
        
        /* Generate description */
        static char desc_buf[256];
        switch (op) {
            case OP_LOADI:
                snprintf(desc_buf, sizeof(desc_buf), "R(%d) := %d", instr->a, instr->sbx);
                break;
            case OP_LOADK:
                snprintf(desc_buf, sizeof(desc_buf), "R(%d) := K(%d)", instr->a, instr->bx);
                break;
            case OP_ADD:
                snprintf(desc_buf, sizeof(desc_buf), "R(%d) := R(%d) + R(%d)", instr->a, instr->b, instr->c);
                break;
            case OP_SUB:
                snprintf(desc_buf, sizeof(desc_buf), "R(%d) := R(%d) - R(%d)", instr->a, instr->b, instr->c);
                break;
            case OP_MUL:
                snprintf(desc_buf, sizeof(desc_buf), "R(%d) := R(%d) * R(%d)", instr->a, instr->b, instr->c);
                break;
            case OP_DIV:
                snprintf(desc_buf, sizeof(desc_buf), "R(%d) := R(%d) / R(%d)", instr->a, instr->b, instr->c);
                break;
            case OP_RET_ONE:
                snprintf(desc_buf, sizeof(desc_buf), "return R(%d)", instr->a);
                break;
            case OP_RET_VOID:
                snprintf(desc_buf, sizeof(desc_buf), "return");
                break;
            default:
                snprintf(desc_buf, sizeof(desc_buf), "%s %d %d %d", instr->opname, instr->a, instr->b, instr->c);
                break;
        }
        instr->description = strdup(desc_buf);
    }
}

static void finish_bytecode_collection(Proto *proto) {
    if (!debug_collecting_bytecode) return;
    
    /* Collect bytecode from the completed proto */
    collect_bytecode_from_proto(proto);
    
    /* Print bytecode debug output */
    aqlD_print_bytecode_header();
    
    /* Print constants if any */
    if (proto && proto->sizek > 0) {
        aqlD_print_constants_pool(proto->k, proto->sizek);
    }
    
    /* Print instructions */
    aqlD_print_instruction_header();
    for (int i = 0; i < debug_bytecode_count; i++) {
        aqlD_print_instruction(&debug_bytecode[i]);
        if (debug_bytecode[i].description) {
            free((void*)debug_bytecode[i].description);
        }
    }
    aqlD_print_bytecode_footer(debug_bytecode_count);
    
    debug_collecting_bytecode = 0;
    debug_bytecode_count = 0;
}

#else
/* Release build - no debug collections */
static void start_ast_collection(void) { /* no-op */ }
static void finish_ast_collection(void) { /* no-op */ }
static void add_debug_ast_node(const char *type, const char *value, int line, int children_count) { 
    (void)type; (void)value; (void)line; (void)children_count; /* no-op */ 
}
static void start_bytecode_collection(void) { /* no-op */ }
static void finish_bytecode_collection(Proto *proto) { (void)proto; /* no-op */ }
#endif

#include "afunc.h"
#include "alex.h"
#include "amem.h"
#include "aobject.h"
#include "aopcodes.h"
#include "aparser.h"
#include "adict.h"
#include "astring.h"
#include "acontainer.h"
#include "aerror.h"

/* Helper function to convert expkind to string */
static const char *expkind_to_string(expkind k) {
    switch (k) {
        case VVOID: return "VOID";
        case VNIL: return "NIL";
        case VTRUE: return "TRUE";
        case VFALSE: return "FALSE";
        case VKFLT: return "FLOAT";
        case VKINT: return "INTEGER";
        case VKSTR: return "STRING";
        case VNONRELOC: return "NONRELOC";
        case VLOCAL: return "LOCAL_VAR";
        default: return "UNKNOWN";
    }
}

/* Helper function to convert binary operator to string */
static const char *binopr_to_string(BinOpr op) {
    switch (op) {
        case OPR_ADD: return "+";
        case OPR_SUB: return "-";
        case OPR_MUL: return "*";
        case OPR_MOD: return "%";
        case OPR_POW: return "**";
        case OPR_DIV: return "/";
        case OPR_IDIV: return "//";
        case OPR_BAND: return "&";
        case OPR_BOR: return "|";
        case OPR_BXOR: return "^";
        case OPR_SHL: return "<<";
        case OPR_SHR: return ">>";
        case OPR_CONCAT: return "..";
        case OPR_EQ: return "==";
        case OPR_LT: return "<";
        case OPR_LE: return "<=";
        case OPR_NE: return "!=";
        case OPR_GT: return ">";
        case OPR_GE: return ">=";
        case OPR_AND: return "&&";
        case OPR_OR: return "||";
        default: return "UNKNOWN_OP";
    }
}

/* VM execution is now handled in aapi.c */

/* maximum number of local variables per function (must be smaller
   than 250, due to the bytecode format) */
#define MAXVARS		200

/* maximum number of C calls */
#ifndef AQL_MAXCCALLS
#define AQL_MAXCCALLS		200
#endif

/* maximum number of upvalues per function */
#ifndef MAXUPVAL
#define MAXUPVAL		255
#endif

/* Variable management functions */
static int new_localvar (LexState *ls, TString *name);
static void adjustlocalvars (LexState *ls, int nvars);
static void removevars (FuncState *fs, int tolevel);
static int searchupvalue (FuncState *fs, TString *name);
static void markupval (FuncState *fs, int level);
static void singlevar (LexState *ls, expdesc *var);

static void singlevar_unified (LexState *ls, expdesc *var);


/* Helper functions */
static void init_exp (expdesc *e, expkind k, int i);
static TString *str_checkname (LexState *ls);
static int testnext (LexState *ls, int c);
static void checknext (LexState *ls, int c);
static void check_match (LexState *ls, int what, int who, int where);
static void codestring (expdesc *e, TString *s);

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
  int breaklist;  /* list of jumps out of this loop */
  int continuelist;  /* list of jumps to loop start */
  
  /* AQL enhancements for block scope management */
  struct {
    /* Type inference scope */
    int type_scope_start;       /* Starting index for type scope */
    bool type_inference_enabled; /* Is type inference active? */
    
    /* Container scope management */
    int container_scope_start;  /* Starting index for container scope */
    bool auto_cleanup;          /* Auto-cleanup containers on exit? */
    
    /* Execution mode for this block */
    AQLExecMode block_mode;     /* Execution mode for this block */
  } aql;
} BlockCnt;

/*
** prototypes for recursive non-terminal functions
*/
static void statement (LexState *ls);
static void expr (LexState *ls, expdesc *v);
static void retstat (LexState *ls);
static int explist (LexState *ls, expdesc *v);

/* Forward declarations for expression evaluation */
static int expdesc_is_true(expdesc *e);

/* aparser.c now focuses only on parsing, REPL moved to arepl.c */

/* Check if expression is considered true */
static int expdesc_is_true(expdesc *e) {
  switch (e->k) {
    case VFALSE: case VNIL: return 0;
    case VKINT: return e->u.ival != 0;
    case VKFLT: return e->u.nval != 0.0;
    default: return 1;  /* non-false values are true */
  }
}


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
** Initialize expression descriptor
*/
static void init_exp (expdesc *e, expkind k, int i) {
  e->f = e->t = NO_JUMP;
  e->k = k;
  e->u.info = i;
}

/*
** Create string constant expression
*/
static void codestring (expdesc *e, TString *s) {
  init_exp(e, VKSTR, 0);
  e->u.strval = s;
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
  printf_debug("[DEBUG] check: expecting token %d, got token %d\n", c, ls->t.token);
  if (ls->t.token != c) {
    printf_debug("[ERROR] check: token mismatch, calling error_expected\n");
    error_expected(ls, c);
  }
  printf_debug("[DEBUG] check: token matched successfully\n");
}

/*
** Check that next token is 'c' and skip it.
*/
static void checknext (LexState *ls, int c) {
  printf_debug("[DEBUG] checknext: calling check for token %d\n", c);
  check(ls, c);
  printf_debug("[DEBUG] checknext: check passed, calling aqlX_next, current token before = %d\n", ls->t.token);
  aqlX_next(ls);
  printf_debug("[DEBUG] checknext: aqlX_next completed, current token after = %d\n", ls->t.token);
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

/* Forward declarations for missing functions */
static int newupvalue (FuncState *fs, TString *name, expdesc *v);
static int searchupvalue (FuncState *fs, TString *name);
/* aqlK_indexed is now defined in acode.c */



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
** Enhanced with AQL variable information management.
*/
static int new_localvar (LexState *ls, TString *name) {
  aql_State *L = ls->L;
  FuncState *fs = ls->fs;
  Dyndata *dyd = ls->dyd;
  Vardesc *var;
  
  checklimit(fs, dyd->actvar.n + 1 - fs->firstlocal,
                 MAXVARS, "local variables");
  
  /* Grow variable array (Lua-compatible) */
  aqlM_growvector(L, dyd->actvar.arr, dyd->actvar.n + 1,
                  dyd->actvar.size, Vardesc, SHRT_MAX, "local variables");
  
  /* Initialize variable with AQL enhancements */
  var = &dyd->actvar.arr[dyd->actvar.n];
  
  /* Initialize Lua-compatible fields */
  var->vd.kind = VDKREG;  /* default */
  var->vd.name = name;
  var->vd.ridx = 0;       /* Will be set by adjustlocalvars */
  var->vd.pidx = -1;      /* Will be set by adjustlocalvars */
  
  /* Initialize AQL enhancement fields */
  var->aql.type_level = AQL_TYPE_NONE;
  var->aql.inferred_type = TYPENONE;
  var->aql.confidence = 0;
  var->aql.exec_mode = dyd->aql.current_mode;
  
  /* Initialize container information */
  var->aql.container_type = CONTAINER_NONE;
  var->aql.container_capacity = 0;
  var->aql.container_flags = 0x02;  /* is_mutable=1, is_container=0 */
  
  /* Initialize debug information */
  #ifdef AQL_DEBUG_BUILD
  var->aql.declaration_line = ls->linenumber;
  var->aql.declaration_column = ls->column;
  var->aql.access_count = 0;
  var->aql.modification_count = 0;
  #endif
  
  dyd->actvar.n++;
  
  printf_debug("[DEBUG] new_localvar: created variable '%s' at index %d with unified AQL enhancements\n", 
               getstr(name), dyd->actvar.n - 1 - fs->firstlocal);
  
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
  printf_debug("[DEBUG] init_var: vidx=%d, ridx=%d, name='%s'\n", 
               vidx, e->u.var.ridx, getstr(getlocalvardesc(fs, vidx)->vd.name));
}

/*
** Start the scope for the last 'nvars' created variables.
** Enhanced with AQL type inference and container management.
*/
static void adjustlocalvars (LexState *ls, int nvars) {
  FuncState *fs = ls->fs;
  int reglevel = aqlY_nvarstack(fs);
  int i;
  
  printf_debug("[DEBUG] adjustlocalvars: activating %d variables, reglevel=%d\n", 
               nvars, reglevel);
  
  for (i = 0; i < nvars; i++) {
    int vidx = fs->nactvar++;
    Vardesc *var = getlocalvardesc(fs, vidx);
    
    /* Assign register (Lua-compatible) */
    var->vd.ridx = reglevel++;
    
    /* Register for debug information */
    var->vd.pidx = registerlocalvar(ls, fs, var->vd.name);
    
    printf_debug("[DEBUG] adjustlocalvars: variable '%s' -> register %d, debug index %d\n",
                 getstr(var->vd.name), var->vd.ridx, var->vd.pidx);
    
    /* AQL enhancement: Auto-infer type from context if not set */
    if (var->aql.type_level == AQL_TYPE_NONE) {
      /* Simple heuristic: if variable name suggests a type, infer it */
      const char *name = getstr(var->vd.name);
      if (strstr(name, "count") || strstr(name, "size") || strstr(name, "len")) {
        var->aql.inferred_type = TYPEINT;
        var->aql.type_level = AQL_TYPE_INFERRED;
        var->aql.confidence = 70;  /* Medium confidence */
        printf_debug("[DEBUG] adjustlocalvars: inferred type INT for variable '%s'\n", name);
      } else if (strstr(name, "list") || strstr(name, "array")) {
        var->aql.container_type = CONTAINER_ARRAY;
        var->aql.container_flags |= 0x01;  /* set is_container=1 */
        var->aql.container_capacity = 16;  /* Default capacity */
        printf_debug("[DEBUG] adjustlocalvars: inferred container ARRAY for variable '%s'\n", name);
      }
    }
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
    
    
    aqlK_exp2anyregup(fs, var);  /* but could be a constant */
    codestring(&key, varname);  /* key is variable name */
    
    /* For global variables, we need to set up for both reading and writing */
    /* Store the table (env) and key information for later use in assignment */
    var->u.ind.t = var->u.info;  /* table is the environment upvalue */
    var->u.ind.idx = aqlK_exp2RK(fs, &key);  /* key index */
    var->k = VINDEXUP;  /* mark as indexed upvalue for proper assignment handling */
    
  }
}

static void enterblock (FuncState *fs, BlockCnt *bl, aql_byte isloop) {
  Dyndata *dyd = fs->ls->dyd;
  
  /* Initialize Lua-compatible fields */
  bl->isloop = isloop;
  bl->nactvar = fs->nactvar;
  bl->firstlabel = dyd->label.n;
  bl->firstgoto = dyd->gt.n;
  bl->upval = 0;
  bl->insidetbc = (fs->bl != NULL && fs->bl->insidetbc);
  bl->breaklist = NO_JUMP;  /* initialize break list */
  bl->continuelist = NO_JUMP;  /* initialize continue list */
  bl->previous = fs->bl;
  
  /* Initialize AQL-specific fields */
  bl->aql.type_scope_start = dyd->aql.types.cache_used;
  bl->aql.type_inference_enabled = true;
  bl->aql.container_scope_start = dyd->aql.containers.container_count;
  bl->aql.auto_cleanup = true;
  bl->aql.block_mode = dyd->aql.current_mode;
  
  /* Link to function state */
  fs->bl = bl;
  aql_assert(fs->freereg == aqlY_nvarstack(fs));
  
  printf_debug("[DEBUG] enterblock: entered block, nactvar=%d, isloop=%d, mode=%d\n", 
               bl->nactvar, isloop, bl->aql.block_mode);
}

static void leaveblock (FuncState *fs) {
  BlockCnt *bl = fs->bl;
  LexState *ls = fs->ls;
  Dyndata *dyd = ls->dyd;
  int hasclose = 0;
  int stklevel = reglevel(fs, bl->nactvar);  /* level outside the block */
  
  printf_debug("[DEBUG] leaveblock: leaving block, cleaning up from level %d\n", 
               bl->nactvar);
  
  /* Clean up variables (Lua-compatible) */
  removevars(fs, bl->nactvar);  /* remove block locals */
  aql_assert(bl->nactvar == fs->nactvar);  /* back to level on entry */
  
  /* AQL enhancement: Clean up type information */
  dyd->aql.types.cache_used = bl->aql.type_scope_start;
  
  /* AQL enhancement: Clean up containers if auto-cleanup is enabled */
  if (bl->aql.auto_cleanup) {
    dyd->aql.containers.container_count = bl->aql.container_scope_start;
    printf_debug("[DEBUG] leaveblock: cleaned up containers to count %d\n", 
                 dyd->aql.containers.container_count);
  }
  
  if (bl->isloop)  /* has to fix pending breaks? */
    hasclose = 1;  /* simplified: assume we need close for loops */
  if (!hasclose && bl->previous && bl->upval)  /* still need a 'close'? */
    /* emit close instruction if needed */;
  
  /* For loop blocks, preserve register reservation for for-loop control */
  if (bl->isloop && stklevel > 0) {
    /* This is a for-loop block - preserve the for-loop control registers */
    /* For-loop uses 4 consecutive registers starting from some base position */
    /* We need to ensure freereg doesn't go below the for-loop base + 4 */
    printf_debug("[DEBUG] leaveblock: for-loop block, stklevel=%d, preserving for-loop registers\n", stklevel);
    /* stklevel represents the register level outside the loop block */
    /* For-loop control registers are managed by the loop itself, so use stklevel */
    fs->freereg = stklevel;
  } else {
    fs->freereg = stklevel;  /* free registers normally */
  }
  dyd->label.n = bl->firstlabel;  /* remove local labels */
  fs->bl = bl->previous;  /* current block now is previous one */
  
  printf_debug("[DEBUG] leaveblock: completed, freereg=%d\n", fs->freereg);
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
  printf_debug("[DEBUG] statlist: entering, current token = %d\n", ls->t.token);
  while (!block_follow(ls, 1)) {
    printf_debug("[DEBUG] statlist: in loop, current token = %d\n", ls->t.token);
    if (ls->t.token == TK_RETURN) {
      statement(ls);
      return;  /* 'return' must be last statement */
    }
    statement(ls);
    printf_debug("[DEBUG] statlist: statement completed, current token = %d\n", ls->t.token);
  }
  printf_debug("[DEBUG] statlist: exiting loop, current token = %d\n", ls->t.token);
}

/*
** Simple expression parsing - basic literals and variables
*/
static void simpleexp (LexState *ls, expdesc *v) {
  /* simpleexp -> FLT | INT | STRING | NIL | TRUE | FALSE | NAME | '(' expr ')' */
  printf_debug("[DEBUG] simpleexp: entering, current token = %d\n", ls->t.token);
  switch (ls->t.token) {
    case TK_FLT: {
      init_exp(v, VKFLT, 0);
      v->u.nval = ls->t.seminfo.r;
      char float_buf[32];
      snprintf(float_buf, sizeof(float_buf), "%.2f", v->u.nval);
      add_debug_ast_node("FLOAT", float_buf, ls->linenumber, 0);
      aqlX_next(ls);  /* skip number */
      break;
    }
    case TK_INT_LITERAL: {
      init_exp(v, VKINT, 0);
      v->u.ival = ls->t.seminfo.i;
      char int_buf[32];
      snprintf(int_buf, sizeof(int_buf), "%lld", (long long)v->u.ival);
      add_debug_ast_node("INTEGER", int_buf, ls->linenumber, 0);
      aqlX_next(ls);  /* skip number */
      break;
    }
    case TK_STRING: {
      codestring(v, ls->t.seminfo.ts);
      aqlX_next(ls);  /* skip string */
      break;
    }
    case TK_NIL: {
      init_exp(v, VNIL, 0);
      aqlX_next(ls);  /* skip nil */
      break;
    }
    case TK_TRUE: {
      init_exp(v, VTRUE, 0);
      aqlX_next(ls);  /* skip true */
      break;
    }
    case TK_FALSE: {
      init_exp(v, VFALSE, 0);
      aqlX_next(ls);  /* skip false */
      break;
    }
    case TK_NAME: {
      /* Use unified variable lookup that works for all execution modes */
      singlevar_unified(ls, v);
      
      /* Check for function call syntax: name ( args... ) */
      if (ls->t.token == TK_LPAREN) {
        /* Function call detected */
        int line = ls->linenumber;
        int nargs = 0;
        
        aqlX_next(ls);  /* skip '(' */
        
        /* Parse arguments using explist */
        if (ls->t.token != TK_RPAREN) {  /* are there arguments? */
          expdesc arg;
          nargs = explist(ls, &arg);  /* parse comma-separated argument list */
          aqlK_exp2nextreg(ls->fs, &arg);  /* ensure last argument is in register */
        }
        
        checknext(ls, TK_RPAREN);  /* check and skip ')' */
        
        /* Generate function call instruction */
        if (v->k == VBUILTIN) {
          /* Builtin function call - use OP_BUILTIN */
          int result_reg = ls->fs->freereg++;
          aqlK_codeABC(ls->fs, OP_BUILTIN, result_reg, v->u.info, nargs);
          init_exp(v, VNONRELOC, result_reg);  /* result is in result_reg */
        } else {
          /* Regular function call - use OP_CALL with the function in register, arguments already set up */
          aqlK_codeABC(ls->fs, OP_CALL, v->u.info, nargs + 1, 2);  /* 1 result */
          /* Set up expression for function result */
          init_exp(v, VNONRELOC, v->u.info);  /* result is in the same register */
        }
      }
      
      return;
    }
    case '[': {  /* Array literal: [expr, expr, ...] */
      int line = ls->linenumber;
      aqlX_next(ls);  /* skip '[' */
      
      /* Parse array elements and store their register indices */
      int element_regs[32];  /* Support up to 32 elements for now */
      int element_count = 0;
      
      /* Parse array elements */
      if (ls->t.token != ']') {
        do {
          if (element_count >= 32) {
            aqlX_syntaxerror(ls, "too many array elements (max 32)");
          }
          expdesc element;
          expr(ls, &element);
          aqlK_exp2nextreg(ls->fs, &element);
          element_regs[element_count] = element.u.info;
          element_count++;
        } while (testnext(ls, ','));
      }
      
      check_match(ls, ']', '[', line);
      
      /* Create array with the correct size */
      int array_reg = ls->fs->freereg++;
      aqlK_codeABC(ls->fs, OP_NEWOBJECT, array_reg, 0, element_count);  /* type=0 for array */
      
      /* Set array elements using the stored register indices */
      if (element_count > 0) {
        for (int i = 0; i < element_count; i++) {
          /* Create a register for the index */
          int index_reg = ls->fs->freereg++;
          aqlK_codeAsBx(ls->fs, OP_LOADI, index_reg, i);
          
          /* Generate OP_SETPROP to set array[index] = element */
          aqlK_codeABC(ls->fs, OP_SETPROP, array_reg, index_reg, element_regs[i]);
        }
      }
      
      init_exp(v, VNONRELOC, array_reg);
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
  printf_debug("[DEBUG] getbinopr: input token=%d (TK_GE=%d)\n", op, TK_GE);
  switch (op) {
    case TK_PLUS: 
      return OPR_ADD;
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
    case TK_GE: 
      printf_debug("[DEBUG] getbinopr: TK_GE -> OPR_GE (%d)\n", OPR_GE);
      return OPR_GE;
    case TK_LAND: return OPR_AND;  /* && */
    case TK_LOR: return OPR_OR;    /* || */
    case TK_AND: return OPR_AND;   /* and */
    case TK_OR: return OPR_OR;     /* or */
    default: 
      printf_debug("[DEBUG] getbinopr: unknown token %d, returning OPR_NOBINOPR\n", op);
      return OPR_NOBINOPR;
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
  else {
    simpleexp(ls, v);
  }
  /* expand while operators have priorities higher than 'limit' */
  op = getbinopr(ls->t.token);
  
  while (op != OPR_NOBINOPR && priority[op].left > limit) {
    expdesc v2;
    BinOpr nextop;
    int line = ls->linenumber;
    
    /* Add AST node for binary operation */
    add_debug_ast_node("BINARY_OP", binopr_to_string(op), line, 2);
    
    aqlX_next(ls);  /* skip operator */
    
    /* Standard Lua-style code generation */
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
  printf_debug("[DEBUG] expr: entering, current token = %d\n", ls->t.token);
  subexpr(ls, v, 0);
  printf_debug("[DEBUG] expr: exiting, current token = %d\n", ls->t.token);
}

/*
** AQL return statement implementation (based on Lua's retstat)
*/
static void retstat (LexState *ls) {
  /* stat -> RETURN [explist] [';'] */
  
  FuncState *fs = ls->fs;
  expdesc e;
  int nret;  /* number of values being returned */
  int first = fs->freereg;  /* first slot to be returned */
  
  if (block_follow(ls, 1) || ls->t.token == ';')
    nret = 0;  /* return no values */
  else {
    nret = explist(ls, &e);  /* optional return values */
    if (hasmultret(e.k)) {
      aqlK_setmultret(fs, &e);
      if (e.k == VCALL && nret == 1) {  /* tail call? */
        /* TODO: implement tail call optimization */
      }
      nret = AQL_MULTRET;  /* return all values */
    }
    else {
      if (nret == 1) {  /* only one single value? */
        
        aqlK_exp2anyreg(fs, &e);  /* can use original slot */
        first = e.u.info;  /* get the register where expression was placed */
        
      }
      else {  /* values must go to the top of the stack */
        aqlK_exp2nextreg(fs, &e);
        aql_assert(nret == fs->freereg - first);
      }
    }
  }
  aqlK_ret(fs, first, nret);
  testnext(ls, ';');  /* skip optional semicolon */
}

/*
** AQL expression list implementation (based on Lua's explist)
*/
static int explist (LexState *ls, expdesc *v) {
  /* explist -> expr { ',' expr } */
  
  int n = 1;  /* at least one expression */
  expr(ls, v);
  
  while (testnext(ls, ',')) {
    aqlK_exp2nextreg(ls->fs, v);
    expr(ls, v);
    n++;
  }
  
  return n;
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
** AQL test_then_block: [IF | ELIF] cond '{' block '}'
*/
static void test_then_block (LexState *ls, int *escapelist) {
  /* test_then_block -> [IF | ELIF] cond '{' block '}' */
  printf_debug("[DEBUG] test_then_block: entering\n");
  BlockCnt bl;
  FuncState *fs = ls->fs;
  expdesc v;
  int jf;  /* instruction to skip 'then' code (if condition is false) */
  
  printf_debug("[DEBUG] test_then_block: skipping IF/ELIF token\n");
  aqlX_next(ls);  /* skip IF or ELIF */
  printf_debug("[DEBUG] test_then_block: parsing condition expression\n");
  expr(ls, &v);  /* read condition */
  printf_debug("[DEBUG] test_then_block: condition parsed, checking for '{'\n");
  checknext(ls, '{');
  
  /* Generate conditional jump */
  printf_debug("[DEBUG] test_then_block: calling aqlK_goiffalse\n");
  aqlK_goiffalse(fs, &v);  /* jump to next elif/else if condition is false */
  printf_debug("[DEBUG] test_then_block: aqlK_goiffalse completed\n");
  enterblock(fs, &bl, 0);
  jf = v.f;
  
  statlist(ls);  /* 'then' part */
  leaveblock(fs);
  
  printf_debug("[DEBUG] test_then_block: checking for closing '}'\n");
  checknext(ls, '}');  /* match closing brace */
  printf_debug("[DEBUG] test_then_block: closing '}' matched\n");
  
  if (ls->t.token == TK_ELSE ||
      ls->t.token == TK_ELIF)  /* followed by 'else'/'elif'? */
    aqlK_concat(fs, escapelist, aqlK_jump(fs));  /* must jump over it */
  aqlK_patchtohere(fs, jf);
  aqlK_patchtohere(fs, v.t);  /* patch true jumps to here too */
}

/*
** AQL if statement: if expr { statlist } [elif expr { statlist }]* [else { statlist }]
*/
static void ifstat (LexState *ls, int line) {
  /* ifstat -> IF cond '{' block '}' {ELIF cond '{' block '}'}* [ELSE '{' block '}'] */
  printf_debug("[DEBUG] ifstat: entering if statement parsing\n");
  FuncState *fs = ls->fs;
  int escapelist = NO_JUMP;  /* exit list for finished parts */
  
  if (!fs) {
    printf_debug("[ERROR] ifstat: fs is NULL\n");
    return;
  }
  
  printf_debug("[DEBUG] ifstat: calling test_then_block\n");
  
  test_then_block(ls, &escapelist);  /* IF cond '{' block '}' */
  while (ls->t.token == TK_ELIF)
    test_then_block(ls, &escapelist);  /* ELIF cond '{' block '}' */
  if (testnext(ls, TK_ELSE)) {
    checknext(ls, '{');
    block(ls);  /* 'else' part */
    check_match(ls, '}', '{', line);
  }
  aqlK_patchtohere(fs, escapelist);  /* patch escape list to 'if' end */
}

/*
** AQL condition parsing for while loop (use goiffalse instead of goiftrue)
*/
static int whilecond (LexState *ls) {
  /* whilecond -> expr */
  expdesc v;
  expr(ls, &v);  /* read condition */
  if (v.k == VNIL) v.k = VFALSE;  /* 'falses' are all equal here */
  aqlK_goiffalse(ls->fs, &v);  /* jump when condition is false (exit loop) */
  return v.f;  /* return false list (exit when condition is false) */
}

/*
** break statement: break
*/
static void breakstat (LexState *ls) {
  FuncState *fs = ls->fs;
  BlockCnt *bl = fs->bl;
  int upval = 0;
  
  /* Find the innermost loop */
  while (bl && !bl->isloop) {
    if (bl->upval) upval = 1;
    bl = bl->previous;
  }
  
  if (!bl) {
    aqlX_syntaxerror(ls, "break statement not inside a loop");
  }
  
  /* Generate jump to loop exit */
  if (upval) {
    /* Need to close upvalues */
    aqlK_codeABC(fs, OP_CLOSE, reglevel(fs, bl->nactvar), 0, 0);
  }
  
  /* Add to break list - we'll patch this later */
  aqlK_concat(fs, &bl->breaklist, aqlK_jump(fs));
}

/*
** continue statement: continue  
*/
static void continuestat (LexState *ls) {
  FuncState *fs = ls->fs;
  BlockCnt *bl = fs->bl;
  int upval = 0;
  
  /* Find the innermost loop */
  while (bl && !bl->isloop) {
    if (bl->upval) upval = 1;
    bl = bl->previous;
  }
  
  if (!bl) {
    aqlX_syntaxerror(ls, "continue statement not inside a loop");
  }
  
  /* Generate jump to loop start */
  if (upval) {
    /* Need to close upvalues */
    aqlK_codeABC(fs, OP_CLOSE, reglevel(fs, bl->nactvar), 0, 0);
  }
  
  /* Add to continue list - we'll patch this later */
  aqlK_concat(fs, &bl->continuelist, aqlK_jump(fs));
}

/*
** Fix for instruction at position 'pc' to jump to 'dest'.
** (Jump addresses are relative in AQL). 'back' true means a back jump.
*/
static void fixforjump (FuncState *fs, int pc, int dest, int back) {
  Instruction *jmp = &fs->f->code[pc];
  int offset = dest - (pc + 1);
  if (back)
    offset = -offset;
  if (offset > MAXARG_sBx) {
    aqlX_syntaxerror(fs->ls, "control structure too long");
  }
  SETARG_sBx(*jmp, offset);
}

/*
** AQL numeric for statement: for name = start, end [, step] { statlist }
*/
static void forstat_numeric (LexState *ls, int line, TString *varname) {
  /* forstat_numeric -> NAME '=' exp ',' exp [',' exp] '{' block '}' */
  /* Note: varname is already parsed, FOR and NAME tokens consumed */
  printf_debug("[DEBUG] forstat_numeric: entering numeric for statement parsing\n");
  FuncState *fs = ls->fs;
  int base = fs->freereg;
  BlockCnt bl;
  int prep, endfor;
  
  /* FOR and variable name already consumed by caller */
  printf_debug("[DEBUG] forstat_numeric: loop variable = %s\n", getstr(varname));
  
  checknext(ls, TK_ASSIGN);  /* expect '=' */
  
  /* Parse initial value */
  expdesc init;
  expr(ls, &init);
  printf_debug("[DEBUG] forstat: initial value expr, k=%d\n", init.k);
  aqlK_exp2anyreg(fs, &init);  /* evaluate to any register first */
  if (init.u.info != base) {
    /* Move to correct register if needed */
    aqlK_codeABC(fs, OP_MOVE, base, init.u.info, 0);
    printf_debug("[DEBUG] forstat: moved initial value from R%d to R%d\n", init.u.info, base);
  }
  aqlK_reserveregs(fs, 1);  /* reserve base register */
  printf_debug("[DEBUG] forstat: parsed initial value, stored in register %d\n", base);
  
  checknext(ls, ',');  /* expect ',' */
  
  /* Parse limit value */
  expdesc limit;
  expr(ls, &limit);
  printf_debug("[DEBUG] forstat: limit value expr, k=%d\n", limit.k);
  aqlK_exp2anyreg(fs, &limit);  /* evaluate to any register first */
  if (limit.u.info != base + 1) {
    /* Move to correct register if needed */
    aqlK_codeABC(fs, OP_MOVE, base + 1, limit.u.info, 0);
    printf_debug("[DEBUG] forstat: moved limit value from R%d to R%d\n", limit.u.info, base + 1);
  }
  aqlK_reserveregs(fs, 1);  /* reserve base+1 register */
  printf_debug("[DEBUG] forstat: parsed limit value, stored in register %d\n", base + 1);
  
  /* Parse optional step value - 智能步长推断版本 */
  if (testnext(ls, ',')) {
    expdesc step;
    expr(ls, &step);
    printf_debug("[DEBUG] forstat: step value expr, k=%d\n", step.k);
    aqlK_exp2anyreg(fs, &step);  /* evaluate to any register first */
    if (step.u.info != base + 2) {
      /* Move to correct register if needed */
      aqlK_codeABC(fs, OP_MOVE, base + 2, step.u.info, 0);
      printf_debug("[DEBUG] forstat: moved step value from R%d to R%d\n", step.u.info, base + 2);
    }
    aqlK_reserveregs(fs, 1);  /* reserve base+2 register */
    printf_debug("[DEBUG] forstat: parsed explicit step value, stored in register %d\n", base + 2);
  } else {
    /* 智能步长：用nil标记需要运行时推断 */
    aqlK_nil(fs, base + 2, 1);  /* 设置步长寄存器为nil */
    aqlK_reserveregs(fs, 1);
    printf_debug("[DEBUG] forstat: using smart step inference, nil at register %d\n", base + 2);
  }
  
  /* Create loop variable */
  new_localvar(ls, varname);
  
  checknext(ls, '{');  /* expect '{' */
  
  /* Generate FORPREP instruction */
  prep = aqlK_codeAsBx(fs, OP_FORPREP, base, 0);
  printf_debug("[DEBUG] forstat: generated FORPREP at PC=%d\n", prep);
  
  /* Enter loop block */
  enterblock(fs, &bl, 1);  /* isloop = 1 */
  
  /* CRITICAL: Protect for-loop control registers */
  /* For-loop uses 4 consecutive registers starting from base */
  /* We need to ensure these registers are not reused during loop body execution */
  int for_loop_base = base;  /* Save the for-loop base register */
  int for_loop_end = base + 4;  /* End of for-loop control registers */
  
  /* Force new allocations to start after for-loop control registers */
  int old_freereg = fs->freereg;
  if (fs->freereg < for_loop_end) {
    fs->freereg = for_loop_end;  /* Ensure we don't allocate in for-loop control range */
  }
  printf_debug("[DEBUG] forstat: protected for-loop registers %d-%d, freereg: %d -> %d\n", 
               for_loop_base, for_loop_end-1, old_freereg, fs->freereg);
  
  adjustlocalvars(ls, 1);  /* adjust for loop variable */
  aqlK_reserveregs(fs, 1);
  
  /* Fix: Loop variable should be at base+3 (Lua standard) */
  /* The for loop control registers are: base+0=internal_index, base+1=limit, base+2=step, base+3=control_variable */
  Vardesc *loopvar = getlocalvardesc(fs, fs->nactvar - 1);
  loopvar->vd.ridx = base + 3;  /* Force loop variable to correct position */
  
  printf_debug("[DEBUG] forstat: loop variable '%s' at register %d, freereg=%d\n", 
               getstr(loopvar->vd.name), loopvar->vd.ridx, fs->freereg);
  
  /* Parse loop body */
  block(ls);
  
  /* Leave loop block */
  leaveblock(fs);
  
  checknext(ls, '}');  /* expect '}' */
  
  /* Generate FORLOOP instruction */
  endfor = aqlK_codeAsBx(fs, OP_FORLOOP, base, 0);
  printf_debug("[DEBUG] forstat: generated FORLOOP at PC=%d\n", endfor);
  
  /* Fix jump addresses - similar to Lua's forbody */
  fixforjump(fs, prep, aqlK_getlabel(fs), 0);   /* FORPREP jumps to after loop */
  fixforjump(fs, endfor, prep + 1, 1);          /* FORLOOP jumps back to after FORPREP */
  
  /* Patch break statements to jump here */
  aqlK_patchtohere(fs, bl.breaklist);
  
  /* Patch continue statements to jump to FORLOOP instruction */
  aqlK_patchlist(fs, bl.continuelist, endfor);
  printf_debug("[DEBUG] forstat: continue jumps patched to FORLOOP at PC=%d\n", endfor);
  
  printf_debug("[DEBUG] forstat: for statement parsing completed\n");
}

/*
** Convert range(...) to numeric for loop
*/
static void forstat_range_to_numeric(LexState *ls, int line, TString *varname, 
                                     expdesc *start, expdesc *stop, expdesc *step) {
  printf_debug("[DEBUG] forstat_range_to_numeric: converting to numeric for loop\n");
  FuncState *fs = ls->fs;
  int base = fs->freereg;
  BlockCnt bl;
  int prep, endfor;
  
  /* Generate code for start, stop, step expressions */
  /* Ensure they are in consecutive registers starting from base */
  printf_debug("[DEBUG] forstat_range_to_numeric: base=%d, freereg=%d\n", base, fs->freereg);
  
  /* Use aqlK_exp2anyreg and then move to ensure correct register allocation */
  aqlK_exp2anyreg(fs, start);  /* evaluate start to any register */
  if (start->u.info != base) {
    aqlK_codeABC(fs, OP_MOVE, base, start->u.info, 0);
    printf_debug("[DEBUG] forstat_range_to_numeric: moved start from R%d to R%d\n", start->u.info, base);
  }
  aqlK_reserveregs(fs, 1);  /* reserve base register */
  printf_debug("[DEBUG] forstat_range_to_numeric: start allocated to register %d\n", base);
  
  aqlK_exp2anyreg(fs, stop);   /* evaluate stop to any register */
  if (stop->u.info != base + 1) {
    aqlK_codeABC(fs, OP_MOVE, base + 1, stop->u.info, 0);
    printf_debug("[DEBUG] forstat_range_to_numeric: moved stop from R%d to R%d\n", stop->u.info, base + 1);
  }
  aqlK_reserveregs(fs, 1);  /* reserve base+1 register */
  printf_debug("[DEBUG] forstat_range_to_numeric: stop allocated to register %d\n", base + 1);
  
  /* Note: No stop-1 adjustment needed since numeric for loop now uses left-closed, right-open semantics */
  
  aqlK_exp2anyreg(fs, step);   /* evaluate step to any register */
  if (step->u.info != base + 2) {
    aqlK_codeABC(fs, OP_MOVE, base + 2, step->u.info, 0);
    printf_debug("[DEBUG] forstat_range_to_numeric: moved step from R%d to R%d\n", step->u.info, base + 2);
  }
  aqlK_reserveregs(fs, 1);  /* reserve base+2 register */
  printf_debug("[DEBUG] forstat_range_to_numeric: step allocated to register %d\n", base + 2);
  
  printf_debug("[DEBUG] forstat_range_to_numeric: generated start/stop/step code\n");
  
  /* Create loop variable */
  new_localvar(ls, varname);
  
  checknext(ls, '{');  /* expect '{' */
  
  /* Generate FORPREP instruction */
  prep = aqlK_codeAsBx(fs, OP_FORPREP, base, 0);
  printf_debug("[DEBUG] forstat_range_to_numeric: generated FORPREP at PC=%d\n", prep);
  
  /* Enter loop block */
  enterblock(fs, &bl, 1);  /* isloop = 1 */
  
  /* CRITICAL: Reserve registers 0,1,2,3 for for-loop control before any other allocation */
  /* This must be done before adjustlocalvars to prevent conflicts */
  int old_freereg = fs->freereg;
  fs->freereg = base + 4;  /* Force allocation to start from register 4 */
  printf_debug("[DEBUG] forstat_range_to_numeric: reserved registers 0-3 for for-loop, freereg: %d -> %d\n", 
               old_freereg, fs->freereg);
  
  adjustlocalvars(ls, 1);  /* adjust for loop variable */
  aqlK_reserveregs(fs, 1);
  
  /* Fix: Loop variable should be at base+3 (Lua standard) */
  /* The for loop control registers are: base+0=internal_index, base+1=limit, base+2=step, base+3=control_variable */
  Vardesc *loopvar = getlocalvardesc(fs, fs->nactvar - 1);
  loopvar->vd.ridx = base + 3;  /* Force loop variable to correct position */
  
  printf_debug("[DEBUG] forstat_range_to_numeric: loop variable '%s' at register %d, freereg=%d\n", 
               getstr(loopvar->vd.name), loopvar->vd.ridx, fs->freereg);
  
  /* Parse loop body */
  block(ls);
  
  /* Leave loop block */
  leaveblock(fs);
  
  checknext(ls, '}');  /* expect '}' */
  
  /* Generate FORLOOP instruction */
  endfor = aqlK_codeAsBx(fs, OP_FORLOOP, base, 0);
  printf_debug("[DEBUG] forstat_range_to_numeric: generated FORLOOP at PC=%d\n", endfor);
  
  /* Fix jump addresses - similar to numeric for loop */
  fixforjump(fs, prep, aqlK_getlabel(fs), 0);   /* FORPREP jumps to after loop */
  fixforjump(fs, endfor, prep + 1, 1);          /* FORLOOP jumps back to after FORPREP */
  
  /* Patch break statements to jump here */
  aqlK_patchtohere(fs, bl.breaklist);
  
  /* Patch continue statements to jump to FORLOOP instruction */
  aqlK_patchlist(fs, bl.continuelist, endfor);
  printf_debug("[DEBUG] forstat_range_to_numeric: continue jumps patched to FORLOOP at PC=%d\n", endfor);
  
  printf_debug("[DEBUG] forstat_range_to_numeric: completed\n");
}

/*
** AQL for-in statement: for name in iterable { statlist }
** Optimized implementation for range objects
*/
static void forinstat_range (LexState *ls, int line, TString *varname) {
  /* forinstat_range -> NAME IN expr '{' block '}' */
  /* Note: FOR and NAME tokens already consumed by caller */
  printf_debug("[DEBUG] forinstat_range: entering for-in statement parsing\n");
  FuncState *fs = ls->fs;
  BlockCnt bl;
  int iterator_reg, value_reg;
  
  /* FOR and variable name already consumed by caller */
  printf_debug("[DEBUG] forinstat_range: loop variable = %s\n", getstr(varname));
  
  checknext(ls, TK_IN);  /* expect 'in' */
  
  /* Check if the iterable is a range(...) function call */
  /* If so, we can optimize by converting directly to numeric for loop */
  
  /* Look ahead to see if this is range(...) */
  if (ls->t.token == TK_NAME) {
    /* Check if the name is 'range' */
    TString *name = ls->t.seminfo.ts;
    if (strcmp(getstr(name), "range") == 0) {
      printf_debug("[DEBUG] forinstat_range: detected range() call, converting to numeric for\n");
      
      /* This is range(...) - parse it as a function call and extract arguments */
      /* Then convert to numeric for loop */
      
      /* Skip 'range' and expect '(' */
      printf_debug("[DEBUG] Before skipping range, current token = %d\n", ls->t.token);
      aqlX_next(ls);  /* skip 'range' */
      printf_debug("[DEBUG] After skipping range, current token = %d\n", ls->t.token);
      checknext(ls, TK_LPAREN);  /* expect and consume '(' */
      printf_debug("[DEBUG] After consuming '(', current token = %d\n", ls->t.token);
      
      /* Parse range arguments */
      expdesc start, stop, step;
      int nargs = 0;
      
      /* Parse first argument */
      expr(ls, &start);
      nargs++;
      printf_debug("[DEBUG] forinstat_range: parsed start, k=%d\n", start.k);
      
      if (testnext(ls, ',')) {
        /* Two or three arguments: range(start, stop [, step]) */
        expr(ls, &stop);
        nargs++;
        printf_debug("[DEBUG] forinstat_range: parsed stop, k=%d\n", stop.k);
        
        if (testnext(ls, ',')) {
          /* Three arguments: range(start, stop, step) */
          expr(ls, &step);
          nargs++;
          printf_debug("[DEBUG] forinstat_range: parsed step, k=%d\n", step.k);
        } else {
          /* Two arguments: range(start, stop) - default step = 1 */
          init_exp(&step, VKINT, 0);
          step.u.ival = 1;  /* step = 1 */
          printf_debug("[DEBUG] forinstat_range: default step = 1\n");
        }
      } else {
        /* One argument: range(stop) - start=0, step=1 */
        stop = start;  /* move first arg to stop */
        init_exp(&start, VKINT, 0);  /* start = 0 */
        start.u.ival = 0;
        init_exp(&step, VKINT, 0);   /* step = 1 */
        step.u.ival = 1;
        printf_debug("[DEBUG] forinstat_range: one arg, start=0, step=1\n");
      }
      
      checknext(ls, TK_RPAREN);  /* expect ')' */
      
      /* Now we have start, stop, step expressions */
      /* Convert to numeric for loop: for var = start, stop-1, step */
      printf_debug("[DEBUG] forinstat_range: converting range(%d args) to numeric for\n", nargs);
      
      /* Note: No adjustment needed here since numeric for loop now uses left-closed, right-open semantics */
      printf_debug("[DEBUG] forinstat_range: stop value passed directly to numeric for loop\n");
      
      /* Call the numeric for loop implementation */
      forstat_range_to_numeric(ls, line, varname, &start, &stop, &step);
      return;  /* Early return - we're done */
    }
  }
  
  /* If we get here, it's not a range() call */
  /* Fall back to generic iterator protocol for containers */
  printf_debug("[DEBUG] forinstat_range: using generic iterator protocol\n");
  
  /* Parse the iterable expression */
  expdesc iterable;
  expr(ls, &iterable);
  aqlK_exp2nextreg(fs, &iterable);
  
  checknext(ls, '{');  /* expect '{' */
  
  /* Generate iterator setup code */
  iterator_reg = fs->freereg;
  int state_reg = fs->freereg + 1;
  value_reg = fs->freereg + 2;
  aqlK_reserveregs(fs, 3);
  
  /* Generate ITER_INIT instruction to initialize iterator */
  aqlK_codeABC(fs, OP_ITER_INIT, iterator_reg, iterable.u.info, 0);
  
  /* Create loop variable */
  new_localvar(ls, varname);
  
  /* Enter loop block */
  enterblock(fs, &bl, 1);  /* isloop = 1 */
  adjustlocalvars(ls, 1);  /* adjust for loop variable */
  
  /* Loop start label */
  int loopstart = aqlK_getlabel(fs);
  
  /* Generate ITER_NEXT instruction */
  aqlK_codeABC(fs, OP_ITER_NEXT, iterator_reg, state_reg, value_reg);
  
  /* Test if iteration is finished (value is nil) */
  int test_jump = aqlK_codeABC(fs, OP_TEST, value_reg, 0, 0);  /* if value is nil, jump to exit */
  
  /* Move iterator value to loop variable */
  Vardesc *loopvar = getlocalvardesc(fs, fs->nactvar - 1);
  aqlK_codeABC(fs, OP_MOVE, loopvar->vd.ridx, value_reg, 0);
  
  /* Parse loop body */
  block(ls);
  
  /* Jump back to loop start */
  aqlK_codeAsBx(fs, OP_JMP, 0, loopstart - aqlK_getlabel(fs) - 1);
  
  /* Patch test jump to exit loop */
  aqlK_patchtohere(fs, test_jump);
  
  /* Leave loop block */
  leaveblock(fs);
  
  checknext(ls, '}');  /* expect '}' */
  
  printf_debug("[DEBUG] forinstat_range: generic iterator protocol completed\n");
}

/*
** AQL while statement: while expr { statlist }
*/
static void whilestat (LexState *ls, int line) {
  /* whilestat -> WHILE cond '{' block '}' */
  printf_debug("[DEBUG] whilestat: entering while statement parsing\n");
  FuncState *fs = ls->fs;
  int whileinit;
  int condexit;
  BlockCnt bl;
  
  aqlX_next(ls);  /* skip WHILE */
  whileinit = aqlK_getlabel(fs);  /* save loop start position */
  printf_debug("[DEBUG] whilestat: loop start at PC=%d\n", whileinit);
  
  condexit = whilecond(ls);  /* parse condition, get false exit list */
  printf_debug("[DEBUG] whilestat: condition parsed, condexit=%d\n", condexit);
  
  checknext(ls, '{');
  printf_debug("[DEBUG] whilestat: opening brace found\n");
  
  enterblock(fs, &bl, 1);  /* enter loop block (isloop=1) */
  statlist(ls);  /* parse loop body */
  
  /* Handle continue statements - patch them to jump back to loop start */
  aqlK_patchlist(fs, bl.continuelist, whileinit);
  printf_debug("[DEBUG] whilestat: continue jumps patched to PC=%d\n", whileinit);
  
  leaveblock(fs);
  
  check_match(ls, '}', '{', line);
  printf_debug("[DEBUG] whilestat: closing brace found\n");
  
  /* Jump back to loop start - equivalent to luaK_jumpto(fs, whileinit) */
  aqlK_patchlist(fs, aqlK_jump(fs), whileinit);
  printf_debug("[DEBUG] whilestat: back jump generated to PC=%d\n", whileinit);
  
  /* Patch condition exit jumps and break statements to here (loop exit) */
  aqlK_patchtohere(fs, condexit);
  aqlK_patchtohere(fs, bl.breaklist);
  printf_debug("[DEBUG] whilestat: exit jumps and break statements patched\n");
}

/*
** AQL let statement: let name [: type] = expr
*/
static void letstat (LexState *ls) {
  /* letstat -> LET NAME [':' type] '=' expr */
  printf_debug("[DEBUG] letstat: entering\n");
  FuncState *fs = ls->fs;
  expdesc e, var;
  int vidx;
  TString *varname;
  
  printf_debug("[DEBUG] letstat: calling aqlX_next to skip LET\n");
  aqlX_next(ls);  /* skip LET */
  
  printf_debug("[DEBUG] letstat: calling str_checkname for variable name\n");
  varname = str_checkname(ls);  /* variable name */
  printf_debug("[DEBUG] letstat: variable name = '%s'\n", getstr(varname));
  
  /* optional type annotation */
  if (testnext(ls, ':')) {
    printf_debug("[DEBUG] letstat: found type annotation, skipping\n");
    /* TODO: parse type annotation */
    str_checkname(ls);  /* skip type for now */
  }
  
  printf_debug("[DEBUG] letstat: calling checknext for '='\n");
  checknext(ls, TK_ASSIGN);
  printf_debug("[DEBUG] letstat: calling expr for initialization\n");
  expr(ls, &e);  /* parse initialization expression */
  printf_debug("[DEBUG] letstat: expr returned, e.k = %d\n", e.k);
  
  /* Determine scope: local vs global based on block nesting */
  if (fs->bl != NULL && fs->bl->previous != NULL) {
    /* Inside a block (not top-level) - create local variable */
    printf_debug("[DEBUG] letstat: creating local variable in block scope (nesting level > 1)\n");
    
    /* Create new local variable (Lua-compatible) */
    new_localvar(ls, varname);
    adjustlocalvars(ls, 1);  /* activate the variable */
    
    /* Generate code to store the expression result in the local variable */
    /* The local variable is now in register ridx */
    Vardesc *localvar = getlocalvardesc(fs, fs->nactvar - 1);
    int reg = localvar->vd.ridx;
    
    printf_debug("[DEBUG] letstat: local variable '%s' assigned to register %d\n", 
                 getstr(varname), reg);
    
    /* Discharge expression to the allocated register */
    aqlK_exp2nextreg(fs, &e);
    if (e.u.info != reg) {
      /* Move result to the correct register if needed */
      aqlK_codeABC(fs, OP_MOVE, reg, e.u.info, 0);
    }
    
    printf_debug("[DEBUG] letstat: local variable assignment completed\n");
  } else {
    /* Top-level scope - create global variable */
    const char *source_name = getstr(ls->source);
    printf_debug("[DEBUG] letstat: source='%s', creating global variable at top-level\n", source_name);
    
    /* Always create global variable at top level */
    init_exp(&var, VUPVAL, 0);  /* _ENV upvalue */
    
    /* Store expression result as global variable */
    printf_debug("[DEBUG] letstat: storing to global variable\n");
    expdesc key;
    int keyidx, validx;
    
    /* Create string constant for variable name */
    codestring(&key, varname);
    keyidx = aqlK_exp2RK(fs, &key);  /* Convert to R/K form */
    
    /* Evaluate expression and convert to R/K form */
    validx = aqlK_exp2RK(fs, &e);
    
    /* Generate OP_SETTABUP: _ENV[varname] = expr */
    /* OP_SETTABUP A B C: UpValue[A][RK(B)] := RK(C) */
    aqlK_codeABC(fs, OP_SETTABUP, 0, keyidx, validx);
    printf_debug("[DEBUG] letstat: global variable assignment completed\n");
  }
  
  printf_debug("[DEBUG] letstat: completed successfully, current token = %d\n", ls->t.token);
}

/*
** AQL type-inferred declaration: name := expr
*/
static void inferredstat (LexState *ls) {
  /* inferredstat -> NAME ':=' expr */
  FuncState *fs = ls->fs;
  expdesc e;
  int vidx;
  TString *varname;
  
  varname = str_checkname(ls);  /* variable name */
  vidx = new_localvar(ls, varname);
  
  checknext(ls, TK_ASSIGN);  /* check ':=' */
  expr(ls, &e);  /* parse initialization expression */
  
  /* Store expression result to register */
  aqlK_exp2nextreg(fs, &e);
  adjustlocalvars(ls, 1);
}

/*
** Helper function: check if expression is a function call
*/
static int is_function_call(expdesc *e) {
  return (e->k == VBUILTIN || e->k == VCALL);
}

/*
** Helper function: mark function call as statement (no return value saved)
*/
static void mark_statement_call(FuncState *fs, expdesc *e) {
  if (e->k == VBUILTIN) {
    /* Builtin function calls are already handled correctly */
    return;
  }
  if (e->k == VCALL) {
    /* Mark regular function call to not save return value */
    Instruction *inst = &fs->f->code[e->u.info];
    SETARG_C(*inst, 1);  /* 1 = no return value saved */
  }
}

/*
** Forward declaration
*/
static void assignment (LexState *ls);

/*
** Assignment from already parsed variable (for exprstat)
*/
static void assignment_from_var(LexState *ls, expdesc *var) {
  printf_debug("[DEBUG] assignment_from_var: entering, var->k=%d\n", var->k);
  if (testnext(ls, TK_ASSIGN)) {  /* name = expr or name := expr */
    printf_debug("[DEBUG] assignment_from_var: found assignment operator\n");
    expdesc e;
    expr(ls, &e);  /* parse right-hand side */
    printf_debug("[DEBUG] assignment_from_var: calling aqlK_storevar\n");
    aqlK_storevar(ls->fs, var, &e);
  }
  else {
    printf_debug("[DEBUG] assignment_from_var: no assignment operator found\n");
    aqlX_syntaxerror(ls, "'=' or ':=' expected in assignment");
  }
}

/*
** Expression statement (Lua-style): func | assignment
** Properly handle both assignments and function calls
*/
static void exprstat(LexState *ls) {
  FuncState *fs = ls->fs;
  expdesc v;
  
  /* Parse the primary expression */
  printf_debug("[DEBUG] exprstat: calling singlevar_unified\n");
  singlevar_unified(ls, &v);
  printf_debug("[DEBUG] exprstat: singlevar_unified returned, v.k=%d\n", v.k);
  
  /* Check what follows */
  if (ls->t.token == TK_ASSIGN || ls->t.token == '=') {
    /* Assignment: name = expr */
    assignment_from_var(ls, &v);
  }
  else if (ls->t.token == TK_LPAREN && v.k == VBUILTIN) {
    /* Builtin function call: name(...) */
    int nargs = 0;
    
    aqlX_next(ls);  /* skip '(' */
    
    /* Parse arguments */
    if (ls->t.token != TK_RPAREN) {
      expdesc arg;
      nargs = explist(ls, &arg);
      aqlK_exp2nextreg(fs, &arg);
    }
    
    checknext(ls, TK_RPAREN);
    
    /* Generate builtin call */
    int result_reg = fs->freereg++;
    aqlK_codeABC(fs, OP_BUILTIN, result_reg, v.u.info, nargs);
    init_exp(&v, VNONRELOC, result_reg);
    
    /* Result not used in statement context */
    aqlK_exp2nextreg(fs, &v);
  }
  else {
    /* Not supported as statement */
    aqlX_syntaxerror(ls, "syntax error (only assignments and builtin calls allowed as statements)");
  }
}

/*
** AQL assignment: name := expr or name = expr (backward compatibility)
*/
static void assignment (LexState *ls) {
  expdesc v;
  singlevar_unified(ls, &v);
  assignment_from_var(ls, &v);
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
    case TK_FOR: {  /* stat -> forstat or forinstat */
      /* Look ahead to determine which type of for loop */
      aqlX_next(ls);  /* skip FOR */
      TString *varname = str_checkname(ls);  /* get variable name */
      
      printf_debug("[DEBUG] After parsing variable name, current token = %d\n", ls->t.token);
      printf_debug("[DEBUG] TK_ASSIGN = %d, TK_IN = %d\n", TK_ASSIGN, TK_IN);
      
      if (ls->t.token == TK_ASSIGN) {
        /* Numeric for loop: for i = start, end [, step] */
        printf_debug("[DEBUG] Detected numeric for loop\n");
        /* Put back the tokens and call original forstat */
        /* Note: This is a simplified approach - in a full implementation, 
           we'd need proper token pushback or refactor the functions */
        forstat_numeric(ls, line, varname);
      } else if (ls->t.token == TK_IN) {
        /* For-in loop: for i in iterable */
        printf_debug("[DEBUG] Detected for-in loop\n");
        forinstat_range(ls, line, varname);
      } else {
        aqlX_syntaxerror(ls, "'=' or 'in' expected after for variable");
      }
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
      retstat(ls);
      break;
    }
    case TK_BREAK: {  /* stat -> breakstat */
      aqlX_next(ls);  /* skip BREAK */
      breakstat(ls);
      break;
    }
    case TK_CONTINUE: {  /* stat -> continuestat */
      aqlX_next(ls);  /* skip CONTINUE */
      continuestat(ls);
      break;
    }
    default: {  /* stat -> func | assignment */
      exprstat(ls);
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
  
  /* Generate final return instruction for main function */
  aqlK_codeABC(fs, OP_RET_VOID, 0, 0, 0);  /* return with no values */
  
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
  
  /* Start debug collections */
  start_token_collection();
  start_ast_collection();
  start_bytecode_collection();
  
  
  LexState lexstate;
  FuncState funcstate;
  LClosure *cl = aqlF_newLclosure(L, 1);  /* create main closure */
  
  
  setclLvalue2s(L, L->top, cl);  /* anchor it (to avoid being collected) */
  L->top++;  /* increment top for the closure */
  
  /* TODO: lexstate.h = aqlH_new(L); */ /* create table for scanner */
  lexstate.h = NULL;  /* temporary fix */
  /* TODO: sethvalue2s(L, L->top, lexstate.h); */ /* anchor it */
  /* TODO: L->top++; */ /* increment for scanner table */
  funcstate.f = cl->p = aqlF_newproto(L);
  /* TODO: aqlC_objbarrier(L, cl, cl->p); */
  funcstate.f->source = aqlStr_newlstr(L, name, strlen(name));  /* create and anchor TString */
  /* TODO: aqlC_objbarrier(L, funcstate.f, funcstate.f->source); */
  lexstate.buff = buff;
  lexstate.dyd = dyd;
  dyd->actvar.n = dyd->gt.n = dyd->label.n = 0;
  
  /* Initialize AQL enhancement fields */
  dyd->aql.types.type_cache = NULL;
  dyd->aql.types.cache_size = 0;
  dyd->aql.types.cache_used = 0;
  dyd->aql.containers.containers = NULL;
  dyd->aql.containers.container_count = 0;
  dyd->aql.containers.container_capacity = 0;
  dyd->aql.current_mode = AQL_MODE_SCRIPT;  /* Default to script mode */
  dyd->aql.mode_locked = false;
  
  aqlX_setinput(L, &lexstate, z, funcstate.f->source, firstchar);
  
  
  mainfunc(&lexstate, &funcstate);
  
  
  aql_assert(!funcstate.prev && funcstate.nups == 1 && !lexstate.fs);
  /* all scopes should be correctly finished */
  aql_assert(dyd->actvar.n == 0 && dyd->gt.n == 0 && dyd->label.n == 0);
  L->top--;  /* remove closure from stack (we return it directly) */
  
  /* Finish debug collections and output debug info */
  finish_token_collection();
  
  /* Check if we should exit early for token-only debug */
  if ((aql_debug_flags & AQL_DEBUG_LEX) && !(aql_debug_flags & ~AQL_DEBUG_LEX)) {
    printf("\n✅ Lexical analysis completed successfully\n");
    exit(0);  /* Exit after showing tokens only */
  }
  
  finish_ast_collection();
  finish_bytecode_collection(cl->p);  /* Pass the generated proto */
  
  return cl;  /* closure is returned directly */
}



/*
** Convert expdesc to TValue for REPL mode
*/
static int expdesc_to_tvalue(aql_State *L, expdesc *e, TValue *result) {
  switch (e->k) {
    case VKINT:
      setivalue(result, e->u.ival);
      return 0;
    case VKFLT:
      setfltvalue(result, e->u.nval);
      return 0;
    case VKSTR:
      setsvalue2n(L, result, e->u.strval);
      return 0;
    case VNIL:
      setnilvalue(result);
      return 0;
    case VTRUE:
      setbvalue(result, 1);
      return 0;
    case VFALSE:
      setbvalue(result, 0);
      return 0;
    default:
      setnilvalue(result);  /* default to nil for unsupported types */
      return -1;  /* unsupported expression type */
  }
}

/*
** Global state for string concatenation (temporary workaround)
*/
aql_State *g_current_L = NULL;



/*
** Simple variable table for REPL mode
*/
#define MAX_VARIABLES 100

typedef struct {
  TString *name;
  TValue value;
} Variable;

static Variable g_variables[MAX_VARIABLES];
static int g_var_count = 0;

/*
** Find variable by name, return index or -1 if not found
*/
static int find_variable(TString *name) {
  for (int i = 0; i < g_var_count; i++) {
    if (eqstr(g_variables[i].name, name)) {
      return i;
    }
  }
  return -1;
}

/*
** Set variable value (create if not exists)
*/
static int set_variable(TString *name, const TValue *value) {
  int idx = find_variable(name);
  if (idx >= 0) {
    /* Update existing variable */
    g_variables[idx].value = *value;
    return 0;
  } else if (g_var_count < MAX_VARIABLES) {
    /* Create new variable */
    g_variables[g_var_count].name = name;
    g_variables[g_var_count].value = *value;
    g_var_count++;
    return 0;
  }
  return -1;  /* table full */
}

/*
** Get variable value by name
*/
static int get_variable(TString *name, TValue *result) {
  int idx = find_variable(name);
  if (idx >= 0) {
    *result = g_variables[idx].value;
    return 0;
  }
  return -1;  /* not found */
}

/*
** Unified variable lookup for all execution modes
** Simplified version without extra abstraction layers
*/
/* Builtin function table */
static const struct {
  const char *name;
  int id;
} builtin_functions[] = {
  {"print", 0},
  {"type", 1},
  {"len", 2},
  {"tostring", 3},
  {"string", 3},    /* alias for tostring */
  {"tonumber", 4},
  {"range", 5},
  {NULL, -1}  /* sentinel */
};

/* Check if a name is a builtin function */
static int get_builtin_id(TString *name) {
  const char *str = getstr(name);
  for (int i = 0; builtin_functions[i].name != NULL; i++) {
    if (strcmp(str, builtin_functions[i].name) == 0) {
      return builtin_functions[i].id;
    }
  }
  return -1;  /* not a builtin */
}

static void singlevar_unified(LexState *ls, expdesc *var) {
  TString *varname = str_checkname(ls);
  FuncState *fs = ls->fs;
  
  /* First check if this is a builtin function */
  int builtin_id = get_builtin_id(varname);
  if (builtin_id >= 0) {
    /* This is a builtin function - set up for OP_BUILTIN */
    init_exp(var, VBUILTIN, builtin_id);
    return;
  }
  
  if (fs != NULL) {
    /* Compilation mode - use standard Lua variable lookup */
    singlevaraux(fs, varname, var, 1);
    if (var->k == VVOID) {
      /* Handle global variables */
      expdesc key;
      singlevaraux(fs, ls->envn, var, 1);  /* get environment variable */
      aql_assert(var->k != VVOID);  /* this one must exist */
      init_exp(&key, VKSTR, 0);
      key.u.strval = varname;  /* key is variable name */
      aqlK_indexed(fs, var, &key);  /* env[varname] */
    }
    else {
      /* Variable found by singlevaraux, but we need to check if it's actually a global */
      /* In AQL, all top-level variables should be treated as globals for assignment */
      printf_debug("[DEBUG] singlevar_unified: found variable '%s' with type k=%d\n", getstr(varname), var->k);
      if (var->k != VLOCAL && var->k != VUPVAL) {
        printf_debug("[DEBUG] singlevar_unified: forcing '%s' to be global variable\n", getstr(varname));
        /* This might be a global variable that was incorrectly identified */
        /* Force it to be treated as a global variable */
        expdesc key;
        singlevaraux(fs, ls->envn, var, 1);  /* get environment variable */
        aql_assert(var->k != VVOID);  /* this one must exist */
        init_exp(&key, VKSTR, 0);
        key.u.strval = varname;  /* key is variable name */
        aqlK_indexed(fs, var, &key);  /* env[varname] */
        printf_debug("[DEBUG] singlevar_unified: after aqlK_indexed, var->k=%d\n", var->k);
      }
    }
  } else {
    /* REPL mode - use simple variable table */
    TValue value;
    if (get_variable(varname, &value) == 0) {
      /* Convert TValue to expdesc */
      int tag = ttypetag(&value);
      switch (tag) {
        case AQL_VNUMINT:
          init_exp(var, VKINT, 0);
          var->u.ival = ivalue(&value);
          break;
        case AQL_VNUMFLT:
          init_exp(var, VKFLT, 0);
          var->u.nval = fltvalue(&value);
          break;
        case AQL_VSHRSTR:
        case AQL_VLNGSTR:
          init_exp(var, VKSTR, 0);
          var->u.strval = tsvalue(&value);
          break;
        case AQL_VNIL:
          init_exp(var, VNIL, 0);
          break;
        case AQL_VFALSE:
          init_exp(var, VFALSE, 0);
          break;
        case AQL_VTRUE:
          init_exp(var, VTRUE, 0);
          break;
        default:
          init_exp(var, VNIL, 0);
          break;
      }
    } else {
      /* Variable not found in REPL mode */
      char msg[256];
      snprintf(msg, sizeof(msg), "Undefined variable '%s'", getstr(varname));
      REPORT_NAME_ERROR(1, msg, "Check variable name spelling or declare it with 'let'");
      init_exp(var, VNIL, 0);
    }
  }
}


/*
** Parse and evaluate an expression string
** Uses Lua-style precedence with direct evaluation for REPL
** Returns 0 on success, -1 on error
*/
AQL_API int aqlP_parse_expression(aql_State *L, const char *expr_str, TValue *result) {
  if (!expr_str || !result || !L) return -1;
  
  /* Set global L for string concatenation */
  g_current_L = L;
  
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
  
  /* Create a minimal FuncState and Proto for REPL expression evaluation */
  Proto f;
  memset(&f, 0, sizeof(f));
  f.source = source;
  f.maxstacksize = 2;  /* Minimal stack size for simple expressions */
  
  FuncState fs;
  memset(&fs, 0, sizeof(fs));
  fs.f = &f;
  fs.prev = NULL;
  fs.ls = &ls;
  fs.bl = NULL;
  fs.pc = 0;
  fs.lasttarget = 0;
  fs.previousline = 0;
  fs.nk = 0;
  fs.np = 0;
  fs.nabslineinfo = 0;
  fs.firstlocal = 0;
  fs.firstlabel = 0;
  fs.ndebugvars = 0;
  fs.nactvar = 0;
  fs.nups = 0;
  fs.freereg = 0;
  fs.iwthabs = 0;
  fs.needclose = 0;
  
  ls.fs = &fs;
  
  /* Get first token */
  aqlX_next(&ls);
  
  /* Parse expression using Lua-style expr/subexpr */
  expdesc v;
  expr(&ls, &v);
  
  /* Convert to TValue */
  int parse_result = expdesc_to_tvalue(L, &v, result);
  
  /* Check for end of input */
  if (parse_result == 0 && ls.t.token != TK_EOS) {
    parse_result = -1;  /* unexpected tokens after expression */
  }
  

  
    /* Cleanup */
  aqlZ_freebuffer(L, &buff);
  aqlZ_cleanup_string(L, &z);
  /* Note: Don't close L yet if result contains references to L's memory */
  /* aql_close(L); */

  /* Clear global L */
  g_current_L = NULL;

  return parse_result;
}

/*
** Print a TValue to stdout for REPL
*/
AQL_API void aqlP_print_value(const TValue *v) {
  int tag = ttypetag(v);
  
  switch (tag) {
    case AQL_VNUMINT:
      printf("%lld", (long long)ivalue(v));
      break;
    case AQL_VNUMFLT:
      printf("%.6g", fltvalue(v));
      break;
    case AQL_VSHRSTR:
    case AQL_VLNGSTR: {
      TString *ts = tsvalue(v);
      const char *str = getstr(ts);
      size_t len = tsslen(ts);
      printf("\"%.*s\"", (int)len, str);
      break;
    }
    case AQL_VNIL:
      printf("nil");
      break;
    case AQL_VFALSE:
      printf("false");
      break;
    case AQL_VTRUE:
      printf("true");
      break;
    case AQL_TRANGE: {
      RangeObject *range = rangevalue(v);
      if (range) {
        printf("range(%lld, %lld, %lld)", 
               (long long)range->start, 
               (long long)range->stop, 
               (long long)range->step);
      } else {
        printf("range(invalid)");
      }
      break;
    }
    case AQL_VARRAY: {
      Array *arr = arrayvalue(v);
      if (arr) {
        printf("[");
        for (size_t i = 0; i < arr->length; i++) {
          if (i > 0) printf(", ");
          aqlP_print_value(&arr->data[i]);
        }
        printf("]");
      } else {
        printf("array(invalid)");
      }
      break;
    }
    case AQL_VDICT: {
      Dict *dict = dictvalue(v);
      if (dict) {
        printf("{");
        // TODO: 遍历dict并格式化输出 key: value, key: value
        printf("}");
      } else {
        printf("dict(invalid)");
      }
      break;
    }
    default:
      printf("(unknown type %d)", tag);
      break;
  }
}

/*
** Free resources associated with a TValue (currently no-op for simple types)
*/
AQL_API void aqlP_free_value(TValue *v) {
  /* For now, no explicit cleanup needed as GC handles string memory */
  UNUSED(v);
}


/*
** Execute an AQL source file with automatic last expression display
** Returns 1 on success, 0 on error
*/
AQL_API int aqlP_execute_file(aql_State *L, const char *filename) {
  if (!L || !filename) return 0;
  
  //printf("Executing file: %s\n", filename);
  
  /* Load and compile the file with automatic return for last expression */
  if (aql_loadfile_with_return(L, filename) != 0) {
    printf("Error: Failed to load file '%s'\n", filename);
    return 0;
  }
  
  /* Execute the compiled function with 1 result expected (for last expression) */
  if (aql_execute(L, 0, 1) != 0) {
    printf("Error: Failed to execute file '%s'\n", filename);
    return 0;
  }
  
  /* Check if there's a result on the stack (last expression value) */
  
  if (L->top > L->ci->func + 1) {  /* Has result? */
    TValue *result = s2v(L->top - 1);
    /* Only print non-nil results */
    if (!ttisnil(result)) {
      aqlP_print_value(result);
      printf("\n");
    } else {
    }
    /* Clean up stack */
    L->top = L->ci->func + 1;
  } else {
  }
  
  printf_debug("File '%s' executed successfully\n", filename);
  return 1;  /* success */
}

/*
** Check if input starts with a statement keyword
*/
static int is_statement(const char *input) {
  /* Skip leading whitespace */
  while (isspace(*input)) input++;
  
  /* Check for statement keywords */
  if (strncmp(input, "let ", 4) == 0) return 1;
  if (strncmp(input, "const ", 6) == 0) return 1;
  if (strncmp(input, "var ", 4) == 0) return 1;
  if (strncmp(input, "if ", 3) == 0) return 1;
  if (strncmp(input, "while ", 6) == 0) return 1;
  if (strncmp(input, "for ", 4) == 0) return 1;
  
  /* Check for type-inferred declaration: name := expr */
  const char *p = input;
  /* Skip identifier characters */
  if (isalpha(*p) || *p == '_') {
    while (isalnum(*p) || *p == '_') p++;
    /* Skip whitespace */
    while (isspace(*p)) p++;
    /* Check for ':=' */
    if (p[0] == ':' && p[1] == '=') return 1;
  }
  
  if (strncmp(input, "function ", 9) == 0) return 1;
  if (strncmp(input, "class ", 6) == 0) return 1;
  if (strncmp(input, "return ", 7) == 0) return 1;
  if (strncmp(input, "break", 5) == 0) return 1;
  
  return 0;  /* assume expression */
}

/*
** Parse and execute a statement (for REPL)
*/
AQL_API int aqlP_parse_statement(aql_State *L, const char *stmt_str) {
  if (!stmt_str || !L) return -1;
  
  /* Use the provided aql_State instead of creating a new one */
  
  struct Zio z;
  aqlZ_init_string(L, &z, stmt_str, strlen(stmt_str));
  
  LexState ls;
  memset(&ls, 0, sizeof(ls));
  ls.L = L;
  ls.z = &z;
  ls.linenumber = 1;
  ls.lastline = 1;
  ls.current = zgetc(&z);
  
  ls.t.token = 0;
  ls.lookahead.token = TK_EOS;
  
  TString *source = aqlStr_newlstr(L, "repl", 4);
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
  
  /* Parse statement */
  int result = 0;
  switch (ls.t.token) {
    case TK_LET: {
      printf("Parsing let statement...\n");
      /* For now, just acknowledge the let statement */
      aqlX_next(&ls);  /* skip 'let' */
      if (ls.t.token == TK_NAME) {
        TString *varname = ls.t.seminfo.ts;
        printf("Variable name: %s\n", getstr(varname));
        aqlX_next(&ls);  /* skip name */
               if (ls.t.token == TK_ASSIGN) {
          aqlX_next(&ls);  /* skip '=' */
          printf("Assignment detected\n");
          
          /* Parse and evaluate the expression */
          expdesc e;
          expr(&ls, &e);
          
          /* Convert to TValue */
          TValue value;
          if (expdesc_to_tvalue(L, &e, &value) == 0) {
            /* Store the variable */
            if (set_variable(varname, &value) == 0) {
              printf("Variable '%s' set to ", getstr(varname));
              aqlP_print_value(&value);
              printf("\n");
              result = 0;
            } else {
              REPORT_RUNTIME_ERROR(ls.linenumber, "Variable table full", 
                                   "Maximum number of variables exceeded");
              result = -1;
            }
          } else {
            REPORT_RUNTIME_ERROR(ls.linenumber, "Failed to evaluate expression", 
                                 "Check expression syntax and variable references");
            result = -1;
          }
        } else {
          REPORT_SYNTAX_ERROR(ls.linenumber, "Expected '=' after variable name", 
                              "Use 'let variable = value' syntax");
          result = -1;
        }
      } else {
        REPORT_SYNTAX_ERROR(ls.linenumber, "Expected variable name after 'let'", 
                            "Use 'let variable = value' syntax");
        result = -1;
      }
      break;
    }
    case TK_NAME: {
      printf("Parsing assignment statement...\n");
      /* Assignment: name = expression */
      TString *varname = ls.t.seminfo.ts;
      printf("Variable name: %s\n", getstr(varname));
      aqlX_next(&ls);  /* skip name */
      if (ls.t.token == TK_ASSIGN) {
        aqlX_next(&ls);  /* skip ':=' */
        printf("Type-inferred assignment detected\n");
        
        /* Parse and evaluate the expression */
        expdesc e;
        expr(&ls, &e);
        
        /* Convert to TValue */
        TValue value;
        if (expdesc_to_tvalue(L, &e, &value) == 0) {
          /* Store the variable */
          if (set_variable(varname, &value) == 0) {
            printf("Variable '%s' set to ", getstr(varname));
            aqlP_print_value(&value);
            printf("\n");
            result = 0;
          } else {
            REPORT_RUNTIME_ERROR(ls.linenumber, "Variable table full", 
                                 "Maximum number of variables exceeded");
            result = -1;
          }
        } else {
          REPORT_RUNTIME_ERROR(ls.linenumber, "Failed to evaluate expression", 
                               "Check expression syntax and variable references");
          result = -1;
        }
      } else {
        REPORT_SYNTAX_ERROR(ls.linenumber, "Expected '=' after variable name", 
                            "Use 'variable = value' syntax");
        result = -1;
      }
      break;
    }
    case TK_IF: {
      printf("Parsing if statement...\n");
      /* For now, just parse the tokens without full execution */
      aqlX_next(&ls);  /* skip 'if' */
      
      /* Parse condition */
      expdesc e;
      expr(&ls, &e);
      printf("Condition parsed\n");
      
      /* Expect '{' */
      if (ls.t.token == '{') {
        aqlX_next(&ls);  /* skip '{' */
        printf("Opening brace found\n");
        
        /* Skip statements until '}' */
        int brace_count = 1;
        while (brace_count > 0 && ls.t.token != TK_EOS) {
          if (ls.t.token == '{') brace_count++;
          else if (ls.t.token == '}') brace_count--;
          aqlX_next(&ls);
        }
        printf("If statement parsed successfully\n");
        result = 0;
      } else {
        REPORT_SYNTAX_ERROR(ls.linenumber, "Expected '{' after if condition", 
                            "Use 'if condition { ... }' syntax");
        result = -1;
      }
      break;
    }
    default: {
      REPORT_SYNTAX_ERROR(ls.linenumber, "Unsupported statement type", 
                          "Use 'let', assignment, or expression statements");
      result = -1;
      break;
    }
  }
  
  /* Cleanup */
  aqlZ_freebuffer(L, &buff);
  aqlZ_cleanup_string(L, &z);
  
  return result;
}