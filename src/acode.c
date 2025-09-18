/*
** $Id: acode.c $
** Code generator for AQL (based on Lua 5.4 lcode.c)
** See Copyright Notice in aql.h
*/

#define acode_c
#define AQL_CORE

#include "aconf.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "aql.h"

#include "acode.h"
#include "adebug_internal.h"
#include "ado.h"
#include "agc.h"
#include "alex.h"
#include "amem.h"
#include "aobject.h"
#include "aopcodes.h"
#include "aparser.h"
#include "astring.h"
#include "acontainer.h"

/* Use MAXREGS from acode.h */

/* Forward declarations */
static int constfolding (FuncState *fs, int op, expdesc *e1, const expdesc *e2);
static void codeunexpval (FuncState *fs, OpCode op, expdesc *e, int line);
static void codenot (FuncState *fs, expdesc *e);
static void codecomp (FuncState *fs, BinOpr opr, expdesc *e1, expdesc *e2, int line);
static void negatecondition (FuncState *fs, expdesc *e);
static int validop (int op, TValue *v1, TValue *v2);
static Instruction *getinstruction (FuncState *fs, expdesc *e);
static void exp2reg (FuncState *fs, expdesc *e, int reg);
static Instruction *getjumpcontrol (FuncState *fs, int pc);
static void removelastinstruction (FuncState *fs);
static int condjump (FuncState *fs, OpCode op, int A, int B, int cond, int k);
static void removevalues (FuncState *fs, int list);
static void str2K (FuncState *fs, expdesc *e);
static int tointegerns (const TValue *obj, aql_Integer *p);
static int getjump (FuncState *fs, int pc);
static void fixjump (FuncState *fs, int pc, int dest);

/* Check whether expression 'e' has any jump lists. */
#define hasjumps(e)	((e)->t != (e)->f)

/* Note: GETARG_k and SETARG_k are now defined in aopcodes.h */

/*
** If expression is a numeric constant, fills 'v' with its value
** and returns 1. Otherwise, returns 0.
*/
static int tonumeral(const expdesc *e, TValue *v) {
  if (hasjumps(e))
    return 0;  /* not a numeral */
  switch (e->k) {
    case VKINT:
      if (v) setivalue(v, e->u.ival);
      return 1;
    case VKFLT:
      if (v) setfltvalue(v, e->u.nval);
      return 1;
    default: return 0;
  }
}


/*
** Convert a constant in 'v' into an expression description 'e'
*/
static void const2exp (TValue *v, expdesc *e) {
  switch (ttypetag(v)) {
    case AQL_VNUMINT:
      e->k = VKINT; e->u.ival = ivalue(v);
      break;
    case AQL_VNUMFLT:
      e->k = VKFLT; e->u.nval = fltvalue(v);
      break;
    case AQL_VFALSE:
      e->k = VFALSE;
      break;
    case AQL_VTRUE:
      e->k = VTRUE;
      break;
    case AQL_VNIL:
      e->k = VNIL;
      break;
    case AQL_VSHRSTR: case AQL_VLNGSTR:
      e->k = VKSTR; e->u.strval = tsvalue(v);
      break;
    default: aql_assert(0);
  }
}

/*
** Gets the instruction of the indexing operation 'e' (if any).
** The expression must be a table access (VINDEXED).
*/
static Instruction *getinstruction (FuncState *fs, expdesc *e) {
  return &fs->f->code[e->u.info];
}

/*
** Fix an expression to return the number of results 'nresults'.
** 'e' must be a multi-ret expression (function call or vararg).
*/
void aqlK_setreturns (FuncState *fs, expdesc *e, int nresults) {
  Instruction *pc = &fs->f->code[e->u.info];
  if (e->k == VCALL) {  /* expression is an open function call? */
    SETARG_C(*pc, nresults + 1);
  }
  else {
    aql_assert(e->k == VVARARG);
    SETARG_C(*pc, nresults + 1);
    SETARG_A(*pc, fs->freereg);
    aqlK_reserveregs(fs, 1);
  }
}

/*
** Fix an expression to return one result.
** If expression is not a multi-ret expression (function call or vararg)
** then it already returns one result, so nothing needs to be done.
** Function calls become VNONRELOC expressions (as its result comes
** fixed in the base register of the call), while vararg expressions
** become VRELOC (as OP_VARARG puts its results where it wants).
** (Calls are created returning one result, so that does not need
** to be fixed.)
*/
void aqlK_setoneret (FuncState *fs, expdesc *e) {
  if (e->k == VCALL) {  /* expression is an open function call? */
    /* already returns 1 value */
    Instruction inst = *getinstruction(fs, e);
    aql_assert(GETARG_C(inst) == 2);
    e->k = VNONRELOC;  /* result has fixed position */
    e->u.info = GETARG_A(inst);
  }
  else if (e->k == VVARARG) {
    Instruction *pc = getinstruction(fs, e);
    *pc = CREATE_ABC(GET_OPCODE(*pc), GETARG_A(*pc), GETARG_B(*pc), 2);
    e->k = VRELOC;  /* can relocate its simple result */
  }
}

/*
** Ensure that expression 'e' is not a variable (nor a <const>).
** (Expression still may have jump lists.)
*/
void aqlK_dischargevars (FuncState *fs, expdesc *e) {
  printf_debug("[DEBUG] aqlK_dischargevars: entering, e->k=%d\n", e->k);
  
  if (!fs) {
    printf_debug("[ERROR] aqlK_dischargevars: fs is NULL\n");
    return;
  }
  
  if (!e) {
    printf_debug("[ERROR] aqlK_dischargevars: e is NULL\n");
    return;
  }
  
  printf_debug("[DEBUG] aqlK_dischargevars: switching on e->k=%d\n", e->k);
  switch (e->k) {
    case VLOCAL: {  /* already in a register */
      e->u.info = e->u.var.ridx;
      e->k = VNONRELOC;
      break;
    }
    case VUPVAL: {  /* move value to some (pending) register */
      int reg = fs->freereg++;  /* allocate register for result */
      printf_debug("[DEBUG] aqlK_dischargevars: VUPVAL using register %d (freereg was %d)\n", reg, fs->freereg - 1);
      e->u.info = aqlK_codeABC(fs, OP_GETUPVAL, reg, e->u.info, 0);
      e->k = VRELOC;
      break;
    }
    case VINDEXUP: {
      int reg = fs->freereg++;  /* allocate register for result */
      printf_debug("[DEBUG] aqlK_dischargevars: VINDEXUP using register %d (freereg was %d)\n", reg, fs->freereg - 1);
      e->u.info = aqlK_codeABC(fs, OP_GETTABUP, reg, e->u.ind.t, e->u.ind.idx);
      e->k = VRELOC;
      break;
    }
    case VINDEXED: {
      int reg = fs->freereg++;  /* allocate register for result */
      printf_debug("[DEBUG] aqlK_dischargevars: VINDEXED using register %d (freereg was %d)\n", reg, fs->freereg - 1);
      aqlK_codeABC(fs, OP_GETTABUP, reg, e->u.ind.t, e->u.ind.idx);
      e->u.info = fs->pc - 1;
      e->k = VRELOC;
      break;
    }
    case VINDEXI: {
      /* Use GETTABUP for integer indexing */
      int reg = fs->freereg++;  /* allocate register for result */
      printf_debug("[DEBUG] aqlK_dischargevars: VINDEXI using register %d (freereg was %d)\n", reg, fs->freereg - 1);
      aqlK_codeABC(fs, OP_GETTABUP, reg, e->u.ind.t, e->u.ind.idx);
      e->u.info = fs->pc - 1;
      e->k = VRELOC;
      break;
    }
    case VINDEXSTR: {
      /* Use GETTABUP for string indexing */
      int reg = fs->freereg++;  /* allocate register for result */
      printf_debug("[DEBUG] aqlK_dischargevars: VINDEXSTR using register %d (freereg was %d)\n", reg, fs->freereg - 1);
      aqlK_codeABC(fs, OP_GETTABUP, reg, e->u.ind.t, e->u.ind.idx);
      e->u.info = fs->pc - 1;
      e->k = VRELOC;
      break;
    }
    default: 
      printf_debug("[DEBUG] aqlK_dischargevars: default case for e->k=%d\n", e->k);
      break;  /* there is one value available (somewhere) */
  }
}

/*
** Ensures expression value is in register 'reg' (and therefore
** 'e' will become a non-relocatable expression).
** (Expression still may have jump lists.)
*/
static void discharge2reg (FuncState *fs, expdesc *e, int reg) {
  aqlK_dischargevars(fs, e);
  switch (e->k) {
    case VNIL: {
      aqlK_nil(fs, reg, 1);
      break;
    }
    case VFALSE: {
      aqlK_codeABC(fs, OP_LOADFALSE, reg, 0, 0);
      break;
    }
    case VTRUE: {
      aqlK_codeABC(fs, OP_LOADTRUE, reg, 0, 0);
      break;
    }
    case VKINT: {
      aqlK_int(fs, reg, e->u.ival);
      break;
    }
    case VKFLT: {
      aqlK_float(fs, reg, e->u.nval);
      break;
    }
    case VKSTR: {
      aqlK_codek(fs, reg, aqlK_stringK(fs, e->u.strval));
      break;
    }
    case VK: {
      aqlK_codek(fs, reg, e->u.info);
      break;
    }
    case VRELOC: {
      Instruction *pc = &fs->f->code[e->u.info];
      SETARG_A(*pc, reg);  /* instruction will put result in 'reg' */
      break;
    }
    case VNONRELOC: {
      if (reg != e->u.info)
        aqlK_codeABC(fs, OP_MOVE, reg, e->u.info, 0);
      break;
    }
    default: {
      aql_assert(e->k == VVOID || e->k == VJMP);
      return;  /* nothing to do... */
    }
  }
  e->u.info = reg;
  e->k = VNONRELOC;
}

/*
** Ensures expression value is in any register.
*/
static void discharge2anyreg (FuncState *fs, expdesc *e) {
  if (e->k != VNONRELOC) {  /* no fixed register yet? */
    aqlK_reserveregs(fs, 1);  /* get a register */
    discharge2reg(fs, e, fs->freereg-1);  /* put value there */
  }
}

/*
** Check whether expression 'e' matches any pattern in 'list'.
** (patterns are coded in integers: 'o' is the opcode, 'v' is the value)
*/
static int codeexpval (FuncState *fs, OpCode op,
                       expdesc *e1, expdesc *e2, int line) {
  int rk1, rk2;
  /* both operands are "RK" */
  rk1 = aqlK_exp2RK(fs, e1);
  rk2 = aqlK_exp2RK(fs, e2);
  aqlK_codeABC(fs, op, 0, rk1, rk2);
  aqlK_fixline(fs, line);
  return fs->pc - 1;  /* return address of new instruction */
}

/*
** Apply prefix operation 'op' to expression 'e'.
*/
void aqlK_prefix (FuncState *fs, UnOpr op, expdesc *e, int line) {
  static const expdesc ef = {VKINT, {0}, NO_JUMP, NO_JUMP};
  aqlK_dischargevars(fs, e);
  switch (op) {
    case OPR_MINUS: case OPR_BNOT:  /* use 'ef' as fake 2nd operand */
      if (constfolding(fs, op + OP_UNM, e, &ef))
        break;
      /* FALLTHROUGH */
    case OPR_LEN:
      codeunexpval(fs, cast(OpCode, op + OP_UNM), e, line);
      break;
    case OPR_NOT: codenot(fs, e); break;
    default: aql_assert(0);
  }
}

/*
** Process 1st operand 'v' of binary operation 'op' before reading
** 2nd operand.
*/
void aqlK_infix (FuncState *fs, BinOpr op, expdesc *v) {
  switch (op) {
    case OPR_AND: {
      aqlK_goiftrue(fs, v);  /* go ahead only if 'v' is true */
      break;
    }
    case OPR_OR: {
      aqlK_goiffalse(fs, v);  /* go ahead only if 'v' is false */
      break;
    }
    case OPR_CONCAT: {
      aqlK_exp2nextreg(fs, v);  /* operand must be in a register */
      break;
    }
    case OPR_ADD: case OPR_SUB:
    case OPR_MUL: case OPR_DIV: case OPR_IDIV:
    case OPR_MOD: case OPR_POW:
    case OPR_BAND: case OPR_BOR: case OPR_BXOR:
    case OPR_SHL: case OPR_SHR: {
      if (!tonumeral(v, NULL))
        aqlK_exp2RK(fs, v);
      /* else keep numeral, which may be folded with 2nd operand */
      break;
    }
    case OPR_EQ: case OPR_NE:
    case OPR_LT: case OPR_LE: case OPR_GT: case OPR_GE: {
      if (!tonumeral(v, NULL))
        aqlK_exp2RK(fs, v);
      /* else keep numeral, which may be folded with 2nd operand */
      break;
    }
    default: aql_assert(0);
  }
}

/*
** Emit code for binary expressions that "produce values"
** (everything but logical operators 'and'/'or' and comparison
** operators).
** Expression to produce final result will be encoded in 'e1'.
** Because 'luaK_exp2RK' can free registers, its calls must be
** in "stack order" (that is, first on 'e2', which may have more
** recent registers to be released).
*/
static void codebinexpval (FuncState *fs, OpCode op,
                           expdesc *e1, expdesc *e2, int line) {
  int rk1 = aqlK_exp2RK(fs, e1);  /* both operands are "RK" */
  int rk2 = aqlK_exp2RK(fs, e2);
  
  /* Save current freereg before freeing expressions */
  int saved_freereg = fs->freereg;
  
  aqlK_freeexp(fs, e2);
  aqlK_freeexp(fs, e1);
  
  /* Allocate target register, starting from saved freereg */
  int target_reg = saved_freereg;
  
  /* Skip registers that conflict with source operands */
  while (target_reg == rk1 || target_reg == rk2) {
    target_reg++;  /* try next register */
  }
  
  /* Update freereg to point past the allocated register */
  fs->freereg = target_reg + 1;
  
  printf_debug("[DEBUG] codebinexpval: op=%d, rk1=%d, rk2=%d, target_reg=%d, freereg=%d (saved=%d)\n", 
               op, rk1, rk2, target_reg, fs->freereg, saved_freereg);
  aqlK_codeABC(fs, op, target_reg, rk1, rk2);  /* generate opcode */
  e1->u.info = target_reg;  /* result is in target_reg */
  e1->k = VNONRELOC;  /* result is in a specific register */
  aqlK_fixline(fs, line);
}

/*
** Emit code for unary expressions that "produce values"
** (everything but 'not').
** Expression to produce final result will be encoded in 'e'.
*/
static void codeunexpval (FuncState *fs, OpCode op, expdesc *e, int line) {
  int r1 = aqlK_exp2RK(fs, e);  /* opcodes operate only on registers */
  aqlK_freeexp(fs, e);
  e->u.info = aqlK_codeABC(fs, op, 0, r1, 0);  /* generate opcode */
  e->k = VRELOC;  /* all those operations are relocatable */
  aqlK_fixline(fs, line);
}

/*
** Return false if folding can raise an error.
** Bitwise operations need operands convertible to integers; division
** operations cannot have 0 as divisor.
*/
static int validop (int op, TValue *v1, TValue *v2) {
  switch (op) {
    case OP_BAND: case OP_BOR: case OP_BXOR:
    case OP_SHL: case OP_SHR: case OP_BNOT: {  /* conversion errors */
      return (ttisinteger(v1) && ttisinteger(v2));
    }
    case OP_DIV: case OP_DIVI: case OP_MOD:  /* division by 0 */
      return (ttisnumber(v2) && (ttisinteger(v2) ? ivalue(v2) != 0 : fltvalue(v2) != 0));
    default: return 1;  /* everything else is valid */
  }
}

/*
** Try to "constant-fold" an operation; return 1 iff successful.
** (In this case, 'e1' has the final result.)
*/
static int constfolding (FuncState *fs, int op, expdesc *e1,
                                        const expdesc *e2) {
  TValue v1, v2, res;
  if (!tonumeral(e1, &v1) || !tonumeral(e2, &v2) || !validop(op, &v1, &v2))
    return 0;  /* non-numeric operands or not safe to fold */
  /* Simplified arithmetic for constant folding */
  if (op == OP_ADD && ttisinteger(&v1) && ttisinteger(&v2)) {
    setivalue(&res, ivalue(&v1) + ivalue(&v2));
  } else if (op == OP_SUB && ttisinteger(&v1) && ttisinteger(&v2)) {
    setivalue(&res, ivalue(&v1) - ivalue(&v2));
  } else if (op == OP_MUL && ttisinteger(&v1) && ttisinteger(&v2)) {
    setivalue(&res, ivalue(&v1) * ivalue(&v2));
  } else {
    return 0;  /* don't fold complex operations for now */
  }
  if (ttisinteger(&res)) {
    e1->k = VKINT;
    e1->u.ival = ivalue(&res);
  }
  else {  /* folds neither NaN nor 0.0 (to avoid problems with -0.0) */
    aql_Number n = fltvalue(&res);
    if (n != n || n == 0)  /* NaN check and zero check */
      return 0;
    e1->k = VKFLT;
    e1->u.nval = n;
  }
  return 1;
}

/*
** Emit code for binary expressions that "produce values"
** (everything but logical operators 'and'/'or' and comparison
** operators).
** Expression to produce final result will be encoded in 'e1'.
*/
static void finishbinexpval (FuncState *fs, expdesc *e1, expdesc *e2,
                           OpCode op, int v2, int flip, int line,
                           OpCode mmop, TMS event) {
  int v1 = aqlK_exp2RK(fs, e1);
  int pc = aqlK_codeABC(fs, op, 0, v1, v2);
  aqlK_fixline(fs, line);
  e1->u.info = pc;
  e1->k = VRELOC;  /* all those operations are relocatable */
}

/*
** Apply binary operation 'op' over 'e1' & 'e2'.
** Expression to produce final result will be encoded in 'e1'.
*/
void aqlK_posfix (FuncState *fs, BinOpr opr,
                  expdesc *e1, expdesc *e2, int line) {
  aqlK_dischargevars(fs, e2);
  if (foldbinop(opr) && constfolding(fs, opr + AQL_OPADD, e1, e2))
    return;  /* result has been folded */
  switch (opr) {
    case OPR_AND: {
      aql_assert(e1->t == NO_JUMP);  /* list closed by 'luaK_infix' */
      aqlK_concat(fs, &e2->f, e1->f);
      *e1 = *e2;
      break;
    }
    case OPR_OR: {
      aql_assert(e1->f == NO_JUMP);  /* list closed by 'luaK_infix' */
      aqlK_concat(fs, &e2->t, e1->t);
      *e1 = *e2;
      break;
    }
    case OPR_CONCAT: {  /* e1 .. e2 */
      aqlK_exp2val(fs, e2);
      if (e2->k == VRELOC && GET_OPCODE(*getinstruction(fs, e2)) == OP_CONCAT) {
        Instruction *inst = getinstruction(fs, e2);
        aql_assert(e1->u.info == GETARG_B(*inst)-1);
        aqlK_freeexp(fs, e1);
        *inst = CREATE_ABC(GET_OPCODE(*inst), GETARG_A(*inst), e1->u.info, GETARG_C(*inst));
        e1->k = VRELOC; e1->u.info = e2->u.info;
      }
      else {
        aqlK_exp2nextreg(fs, e2);  /* operand must be in register */
        codebinexpval(fs, OP_CONCAT, e1, e2, line);
      }
      break;
    }
    case OPR_ADD: case OPR_SUB: case OPR_MUL: case OPR_DIV:
    case OPR_IDIV: case OPR_MOD: case OPR_POW:
    case OPR_BAND: case OPR_BOR: case OPR_BXOR:
    case OPR_SHL: case OPR_SHR: {
      /* Map BinOpr to correct OpCode */
      OpCode op;
      switch (opr) {
        case OPR_ADD: op = OP_ADD; break;
        case OPR_SUB: op = OP_SUB; break;
        case OPR_MUL: op = OP_MUL; break;
        case OPR_DIV: op = OP_DIV; break;
        case OPR_IDIV: op = OP_DIV; break;  /* Use OP_DIV for now, TODO: implement OP_IDIV */
        case OPR_MOD: op = OP_MOD; break;
        case OPR_POW: op = OP_POW; break;
        case OPR_BAND: op = OP_BAND; break;
        case OPR_BOR: op = OP_BOR; break;
        case OPR_BXOR: op = OP_BXOR; break;
        case OPR_SHL: op = OP_SHL; break;
        case OPR_SHR: op = OP_SHR; break;
        default: aql_assert(0); op = OP_ADD; /* should not happen */
      }
      codebinexpval(fs, op, e1, e2, line);
      break;
    }
    case OPR_EQ: case OPR_LT: case OPR_LE: {
      codecomp(fs, opr, e1, e2, line);
      break;
    }
    case OPR_GT: case OPR_GE: {
      printf_debug("[DEBUG] aqlK_posfix: handling GT/GE, opr=%d\n", opr);
      /* '(a > b)' <=> '(b < a)';  '(a >= b)' <=> '(b <= a)' */
      /* Swap operands */
      expdesc temp = *e1;
      *e1 = *e2;
      *e2 = temp;
      /* Convert operator: OPR_GT -> OPR_LT, OPR_GE -> OPR_LE */
      BinOpr new_opr = (BinOpr)((opr - OPR_GT) + OPR_LT);
      printf_debug("[DEBUG] aqlK_posfix: swapped operands, converted opr %d to %d\n", opr, new_opr);
      codecomp(fs, new_opr, e1, e2, line);
      break;
    }
    case OPR_NE: {
      printf_debug("[DEBUG] aqlK_posfix: handling NE, opr=%d\n", opr);
      /* Use codecomp directly with the original operator - it handles negation internally */
      codecomp(fs, opr, e1, e2, line);
      break;
    }
    default: aql_assert(0);
  }
}

/*
** Change line information associated with current position, by removing
** previous info and adding it again with new line.
*/
void aqlK_fixline (FuncState *fs, int line) {
  fs->f->lineinfo[fs->pc - 1] = line;
}

/*
** Emit instruction 'i', checking for array sizes and saving also its
** line information. Return 'i' position.
*/
int aqlK_code (FuncState *fs, Instruction i) {
  Proto *f = fs->f;
  /* put new instruction in code array */
  aqlM_growvector(fs->ls->L, f->code, fs->pc, f->sizecode, Instruction,
                  MAX_INT, "opcodes");
  f->code[fs->pc] = i;
  /* save corresponding line information */
  aqlM_growvector(fs->ls->L, f->lineinfo, fs->pc, f->sizelineinfo, aql_byte,
                  MAX_INT, "opcodes");
  f->lineinfo[fs->pc] = fs->ls->linenumber;
  return fs->pc++;
}

/*
** Format and emit an 'iABC' instruction. (Assertions check consistency
** of parameters versus opcode.)
*/
int aqlK_codeABC (FuncState *fs, OpCode o, int a, int b, int c) {
  aql_assert(getOpMode(o) == iABC);
  aql_assert(getBMode(o) != OpArgN || b == 0);
  aql_assert(getCMode(o) != OpArgN || c == 0);
  aql_assert(a <= MAXARG_A && b <= MAXARG_B && c <= MAXARG_C);
  return aqlK_code(fs, CREATE_ABC(o, a, b, c));
}

/*
** Format and emit an 'iABx' instruction.
*/
int aqlK_codeABx (FuncState *fs, OpCode o, int a, unsigned int bc) {
  aql_assert(getOpMode(o) == iABx || getOpMode(o) == iAsBx);
  aql_assert(getCMode(o) == OpArgN);
  aql_assert(a <= MAXARG_A && bc <= MAXARG_Bx);
  return aqlK_code(fs, CREATE_ABx(o, a, bc));
}

/*
** Format and emit an 'iAsBx' instruction.
*/
int aqlK_codeAsBx (FuncState *fs, OpCode o, int a, int bc) {
  unsigned int b = bc + MAXARG_sBx;
  printf_debug("[DEBUG] aqlK_codeAsBx: o=%d, a=%d, bc=%d, b=%u\n", o, a, bc, b);
  aql_assert(getOpMode(o) == iAsBx);
  aql_assert(getCMode(o) == OpArgN && (bc >= -MAXARG_sBx && bc <= MAXARG_sBx));
  aql_assert(a <= MAXARG_A);
  return aqlK_code(fs, CREATE_ABx(o, a, b));
}

/*
** Format and emit an 'iAx' instruction.
*/
int aqlK_codeAx (FuncState *fs, OpCode o, unsigned int ax) {
  aql_assert(getOpMode(o) == iAx);
  aql_assert(ax <= MAXARG_Ax);
  return aqlK_code(fs, CREATE_Ax(o, ax));
}

/*
** Emit an "extra argument" instruction (format 'iAx')
*/
int aqlK_codeextraarg (FuncState *fs, int a) {
  aql_assert(a <= MAXARG_Ax);
  return aqlK_code(fs, CREATE_Ax(OP_EXTRAARG, a));
}

/*
** Emit a "load constant" instruction, using either 'OP_LOADK'
** (if constant index 'k' fits in 18 bits) or an 'OP_LOADKX'
** instruction with "extra argument".
*/
int aqlK_codek (FuncState *fs, int reg, int k) {
  if (k <= MAXARG_Bx)
    return aqlK_codeABx(fs, OP_LOADK, reg, k);
  else {
    int p = aqlK_codeABx(fs, OP_LOADKX, reg, 0);
    aqlK_codeextraarg(fs, k);
    return p;
  }
}

/*
** Check register-stack level, keeping track of its maximum size
** in field 'maxstacksize'
*/
void aqlK_checkstack (FuncState *fs, int n) {
  int newstack = fs->freereg + n;
  if (newstack > fs->f->maxstacksize) {
    if (newstack >= MAXREGS)
      aqlX_syntaxerror(fs->ls, "function or expression needs too many registers");
    fs->f->maxstacksize = cast_byte(newstack);
  }
}

/*
** Reserve 'n' registers in register stack
*/
int aqlK_reserveregs (FuncState *fs, int n) {
  aqlK_checkstack(fs, n);
  fs->freereg += n;
  return fs->freereg - n;
}

/*
** Free register 'reg', if it is neither a constant index nor
** a local variable.
*/
static void freereg (FuncState *fs, int reg) {
  if (reg >= fs->nactvar) {
    fs->freereg--;
    aql_assert(reg == fs->freereg);
  }
}

/*
** Free register used by expression 'e' (if any)
*/
void aqlK_freeexp (FuncState *fs, expdesc *e) {
  if (e->k == VNONRELOC)
    freereg(fs, e->u.info);
}

/*
** Free registers used by expressions 'e1' and 'e2' (if any) in proper
** order.
*/
static void freeexps (FuncState *fs, expdesc *e1, expdesc *e2) {
  int r1 = (e1->k == VNONRELOC) ? e1->u.info : -1;
  int r2 = (e2->k == VNONRELOC) ? e2->u.info : -1;
  if (r1 > r2) {
    freereg(fs, r1);
    freereg(fs, r2);
  }
  else {
    freereg(fs, r2);
    freereg(fs, r1);
  }
}

/*
** Add constant 'v' to prototype's list of constants (field 'k').
** Use scanner's table to cache position of constants in constant list
** and try to reuse constants. Because some constants are strings, and
** strings are subject to garbage collection, the scanner cannot cache
** their values directly. Instead, it caches their hash codes in
** field 'h' of the scanner. We use the hash codes to guide the search
** in the constant list and guarantee that reused constants have the
** same value without being affected by garbage collections.
*/
int aqlK_addk (FuncState *fs, TValue *key, TValue *v) {
  aql_State *L = fs->ls->L;
  Proto *f = fs->f;
  int k, oldsize;
  
  /* Simple linear search for existing constants */
  for (k = 0; k < fs->nk; k++) {
    if (ttypetag(&f->k[k]) == ttypetag(v)) {
      /* Check if values are equal */
      if (ttisinteger(v) && ttisinteger(&f->k[k]) && ivalue(v) == ivalue(&f->k[k])) {
        return k;  /* reuse index */
      }
      if (ttisfloat(v) && ttisfloat(&f->k[k]) && fltvalue(v) == fltvalue(&f->k[k])) {
        return k;  /* reuse index */
      }
      if (ttisstring(v) && ttisstring(&f->k[k]) && tsvalue(v) == tsvalue(&f->k[k])) {
        return k;  /* reuse index */
      }
    }
  }
  
  /* constant not found; create a new entry */
  oldsize = f->sizek;
  k = fs->nk;
  aqlM_growvector(L, f->k, k, f->sizek, TValue, MAXARG_Ax, "constants");
  while (oldsize < f->sizek) {
    setnilvalue(&f->k[oldsize++]);
  }
  setobj(L, &f->k[k], v);
  fs->nk++;
  if (iscollectable(v)) {
    aqlC_barrier_(L, obj2gco(f), gcvalue(v));
  } else {
  }
  return k;
}

/*
** Add a string constant to list of constants and return its index.
*/
int aqlK_stringK (FuncState *fs, TString *s) {
  TValue o;
  setsvalue(fs->ls->L, &o, s);
  return aqlK_addk(fs, &o, &o);  /* use string as key and as value */
}

/*
** Add an integer constant to list of constants and return its index.
*/
int aqlK_intK (FuncState *fs, aql_Integer n) {
  TValue o;
  setivalue(&o, n);
  int result = aqlK_addk(fs, &o, &o);
  return result;
}

/*
** Add a float constant to list of constants and return its index.
** Floats with integral values need a different key, to avoid collision
** with actual integers. (This does not need to be done for strings or
** nil/boolean, as they are in different structures.)
*/
int aqlK_numberK (FuncState *fs, aql_Number r) {
  TValue o;
  setfltvalue(&o, r);
  return aqlK_addk(fs, &o, &o);
}

/*
** Check if integer fits in sBx field
*/
static int fitsBx(aql_Integer i) {
  return (i >= -MAXARG_sBx && i <= MAXARG_sBx);
}

/*
** Check if float can be represented as integer
*/
static int aql_numisinteger(aql_Number f, aql_Integer *p) {
  aql_Integer i = (aql_Integer)f;
  if ((aql_Number)i == f) {
    *p = i;
    return 1;
  }
  return 0;
}

/*
** Generate code to load an integer constant into a register.
** If the integer fits in sBx, use LOADI; otherwise, add it to constants.
*/
void aqlK_int (FuncState *fs, int reg, aql_Integer i) {
  printf_debug("[DEBUG] aqlK_int: reg=%d, i=%lld, fitsBx=%d\n", reg, (long long)i, fitsBx(i));
  printf_debug("[DEBUG] aqlK_int: MAXARG_Bx=%d, OFFSET_sBx=%d, MAXARG_sBx=%d\n", 
               MAXARG_Bx, OFFSET_sBx, MAXARG_sBx);
  if (fitsBx(i)) {
    printf_debug("[DEBUG] aqlK_int: calling aqlK_codeAsBx with i=%lld\n", (long long)i);
    aqlK_codeAsBx(fs, OP_LOADI, reg, cast_int(i));
  } else {
    int k = aqlK_intK(fs, i);
    aqlK_codek(fs, reg, k);
  }
}

/*
** Generate code to load a float constant into a register.
** If the float has an integral value that fits in sBx, use LOADF;
** otherwise, add it to constants.
*/
void aqlK_float (FuncState *fs, int reg, aql_Number f) {
  aql_Integer fi;
  /* Check if float can be represented as integer and fits in sBx */
  if (aql_numisinteger(f, &fi) && fitsBx(fi))
    aqlK_codeAsBx(fs, OP_LOADF, reg, cast_int(fi));
  else
    aqlK_codek(fs, reg, aqlK_numberK(fs, f));
}

/*
** Add a boolean constant to list of constants and return its index.
*/
static int boolK (FuncState *fs, int b) {
  TValue o;
  setbvalue(&o, b);
  return aqlK_addk(fs, &o, &o);
}

/*
** Add nil to list of constants and return its index.
*/
static int nilK (FuncState *fs) {
  TValue k, v;
  setnilvalue(&v);
  /* cannot use nil as key; instead use table itself to represent nil */
  sethvalue(fs->ls->L, &k, fs->ls->h);
  return aqlK_addk(fs, &k, &v);
}

/*
** Fix an expression to return the number of results 'nresults'.
** 'e' must be a multi-ret expression (function call or vararg).
*/
void aqlK_setmultret (FuncState *fs, expdesc *e) {
  aqlK_setreturns(fs, e, AQL_MULTRET);
}

/*
** Fix an expression to return one result.
** If expression is not a multi-ret expression (function call or vararg)
** then it already returns one result, so nothing needs to be done.
** Function calls become VNONRELOC expressions (as its result comes
** fixed in the base register of the call), while vararg expressions
** become VRELOC (as OP_VARARG puts its results where it wants).
** (Calls are created returning one result, so that does not need
** to be fixed.)
*/

/*
** Ensure that expression 'e' is in a register.
*/
void aqlK_exp2nextreg (FuncState *fs, expdesc *e) {
  aqlK_dischargevars(fs, e);
  aqlK_freeexp(fs, e);
  aqlK_reserveregs(fs, 1);
  discharge2reg(fs, e, fs->freereg-1);
}

/*
** Ensure that expression 'e' is in any register.
*/
void aqlK_exp2anyreg (FuncState *fs, expdesc *e) {
  aqlK_dischargevars(fs, e);
  if (e->k == VNONRELOC) {  /* expression already in a register? */
    if (!hasjumps(e))  /* no jumps? */
      return;  /* result is already in a register */
    if (e->u.info >= fs->nactvar) {  /* reg. is not a local? */
      exp2reg(fs, e, e->u.info);  /* put final result in it */
      return;
    }
    /* else expression has jumps and cannot change its register
       to hold the jump values, because it is a local variable.
       Go through to the default case. */
  }
  aqlK_exp2nextreg(fs, e);  /* default: use next available register */
}

/*
** Ensure that expression 'e' is either in a register or in an
** "upvalue" (local variable or upvalue).
*/
void aqlK_exp2anyregup (FuncState *fs, expdesc *e) {
  if (e->k != VUPVAL || hasjumps(e))
    aqlK_exp2anyreg(fs, e);
}

/*
** Ensure that expression 'e' is in a valid R/K index
** (that is, it is either in a register or in 'k' with an index
** in the range [0, MAXINDEXRK]).
*/
int aqlK_exp2RK (FuncState *fs, expdesc *e) {
  aqlK_exp2val(fs, e);
  switch (e->k) {  /* move constants to 'k' */
    case VTRUE: e->u.info = boolK(fs, 1); goto vk;
    case VFALSE: e->u.info = boolK(fs, 0); goto vk;
    case VNIL: e->u.info = nilK(fs); goto vk;
    case VKINT: e->u.info = aqlK_intK(fs, e->u.ival); goto vk;
    case VKFLT: e->u.info = aqlK_numberK(fs, e->u.nval); goto vk;
    case VKSTR: e->u.info = aqlK_stringK(fs, e->u.strval); goto vk;
    case VK: vk:
      e->k = VK;
      if (e->u.info <= MAXINDEXRK)  /* constant fits in 'argC'? */
        return RKASK(e->u.info);
      else break;
    default: break;
  }
  /* not a constant in the right range: put it in a register */
  aqlK_exp2anyreg(fs, e);
  return e->u.info;
}

/*
** Generate code to store result of expression 'ex' into variable 'var'.
*/
void aqlK_storevar (FuncState *fs, expdesc *var, expdesc *ex) {
  printf_debug("[DEBUG] aqlK_storevar: entering, var->k=%d, ex->k=%d\n", var->k, ex->k);
  printf_debug("[DEBUG] aqlK_storevar: VINDEXUP=%d, VLOCAL=%d, VUPVAL=%d\n", VINDEXUP, VLOCAL, VUPVAL);
  
  switch (var->k) {
    case VLOCAL: {
      aqlK_freeexp(fs, ex);
      exp2reg(fs, ex, var->u.var.ridx);  /* compute 'ex' into proper place */
      return;
    }
    case VUPVAL: {
      aqlK_exp2anyreg(fs, ex);
      int e = ex->u.info;
      aqlK_codeABC(fs, OP_SETUPVAL, e, var->u.info, 0);
      break;
    }
    case VINDEXUP: {
      printf_debug("[DEBUG] aqlK_storevar: VINDEXUP case, generating SETTABUP\n");
      int e = aqlK_exp2RK(fs, ex);
      printf_debug("[DEBUG] aqlK_storevar: calling aqlK_codeABC(OP_SETTABUP, %d, %d, %d)\n", var->u.ind.t, var->u.ind.idx, e);
      aqlK_codeABC(fs, OP_SETTABUP, var->u.ind.t, var->u.ind.idx, e);
      printf_debug("[DEBUG] aqlK_storevar: SETTABUP instruction generated\n");
      break;
    }
    case VINDEXED: {
      int e = aqlK_exp2RK(fs, ex);
      aqlK_codeABC(fs, OP_SETTABUP, var->u.ind.t, var->u.ind.idx, e);
      break;
    }
    case VINDEXI: {
      int e = aqlK_exp2RK(fs, ex);
      aqlK_codeABC(fs, OP_SETTABUP, var->u.ind.t, var->u.ind.idx, e);
      break;
    }
    case VINDEXSTR: {
      int e = aqlK_exp2RK(fs, ex);
      aqlK_codeABC(fs, OP_SETTABUP, var->u.ind.t, var->u.ind.idx, e);
      break;
    }
    case VRELOC: {
      /* Variable is the result of a previous instruction (like OP_GETTABUP) */
      /* We need to generate OP_SETTABUP to store the value */
      printf_debug("[DEBUG] aqlK_storevar: VRELOC case, handling global assignment\n");
      
      /* For VRELOC from aqlK_indexed, the previous instruction should be OP_GETTABUP */
      /* We need to generate a corresponding OP_SETTABUP instruction */
      Instruction *previous = &fs->f->code[var->u.info];
      OpCode op = GET_OPCODE(*previous);
      printf_debug("[DEBUG] aqlK_storevar: previous instruction opcode=%d (OP_GETTABUP=%d)\n", op, OP_GETTABUP);
      
      if (op == OP_GETTABUP) {
        /* Extract table and key from the previous GETTABUP instruction */
        int table = GETARG_B(*previous);
        int key = GETARG_C(*previous);
        int value = aqlK_exp2RK(fs, ex);
        printf_debug("[DEBUG] aqlK_storevar: generating SETTABUP with table=%d, key=%d, value=%d\n", table, key, value);
        aqlK_codeABC(fs, OP_SETTABUP, table, key, value);
      } else {
        printf_debug("[DEBUG] aqlK_storevar: unexpected previous instruction, falling back\n");
        /* Fallback: convert to any register */
        aqlK_exp2anyreg(fs, var);
      }
      break;
    }
    default: 
      printf_debug("[DEBUG] aqlK_storevar: default case, var->k=%d (invalid)\n", var->k);
      aql_assert(0);  /* invalid var kind to store */
  }
  aqlK_freeexp(fs, ex);
}

/*
* Emit SELF instruction (convert expression 'e' into 'e:key(e,').
*/
void aqlK_self (FuncState *fs, expdesc *e, expdesc *key) {
  int ereg;
  aqlK_exp2anyreg(fs, e);
  ereg = e->u.info;  /* register where 'e' was placed */
  aqlK_freeexp(fs, e);
  e->u.info = fs->freereg;  /* base register for op_self */
  e->k = VNONRELOC;  /* self expression has a fixed register */
  aqlK_reserveregs(fs, 2);  /* function and 'self' produced by OP_SELF */
  /* Use GETTABUP for self indexing */
  aqlK_codeABC(fs, OP_GETTABUP, e->u.info, ereg, aqlK_exp2RK(fs, key));
  aqlK_freeexp(fs, key);
}

/*
** Negate condition 'e' (where 'e' is a comparison).
*/
static void negatecondition (FuncState *fs, expdesc *e) {
  Instruction *pc = getjumpcontrol(fs, e->u.info);
  aql_assert(testTMode(GET_OPCODE(*pc)) && GET_OPCODE(*pc) != OP_TESTSET &&
                                           GET_OPCODE(*pc) != OP_TEST);
  SETARG_k(*pc, (GETARG_k(*pc)) ^ 1);
}

/*
** Emit instruction to jump if 'e' is 'cond' (that is, if 'cond'
** is true, code will jump if 'e' is true.) Return jump position.
** Optimize when 'e' is 'not' something, inverting the condition
** and removing the 'not'.
*/
static int jumponcond (FuncState *fs, expdesc *e, int cond) {
  if (e->k == VRELOC) {
    Instruction ie = *getinstruction(fs, e);
    if (GET_OPCODE(ie) == OP_NOT) {
      removelastinstruction(fs);  /* remove previous OP_NOT */
      return condjump(fs, OP_TEST, GETARG_B(ie), 0, !cond, GETARG_k(ie));
    }
    /* else go through */
  }
  discharge2anyreg(fs, e);
  aqlK_freeexp(fs, e);
  return condjump(fs, OP_TESTSET, NO_REG, e->u.info, 0, cond);
}

/*
** Emit code to go through if 'e' is true, jump otherwise.
*/
void aqlK_goiftrue (FuncState *fs, expdesc *e) {
  int pc;  /* pc of new jump */
  aqlK_dischargevars(fs, e);
  switch (e->k) {
    case VJMP: {  /* condition? */
      negatecondition(fs, e);  /* jump when it is false */
      pc = e->u.info;  /* save jump position */
      break;
    }
    case VK: case VKFLT: case VKINT: case VKSTR: case VTRUE: {
      pc = NO_JUMP;  /* always true; do nothing */
      break;
    }
    default: {
      pc = jumponcond(fs, e, 0);  /* jump when false */
      break;
    }
  }
  aqlK_concat(fs, &e->f, pc);  /* insert new jump in false list */
  aqlK_patchtohere(fs, e->t);  /* true list jumps to here (to go through) */
  e->t = NO_JUMP;
}

/*
** Emit code to go through if 'e' is false, jump otherwise.
*/
void aqlK_goiffalse (FuncState *fs, expdesc *e) {
  printf_debug("[DEBUG] aqlK_goiffalse: entering\n");
  int pc;  /* pc of new jump */
  
  if (!fs) {
    printf_debug("[ERROR] aqlK_goiffalse: fs is NULL\n");
    return;
  }
  
  if (!e) {
    printf_debug("[ERROR] aqlK_goiffalse: e is NULL\n");
    return;
  }
  
  printf_debug("[DEBUG] aqlK_goiffalse: calling aqlK_dischargevars\n");
  aqlK_dischargevars(fs, e);
  printf_debug("[DEBUG] aqlK_goiffalse: aqlK_dischargevars completed, e->k=%d\n", e->k);
  
  /* Handle VJMP case early to avoid switch statement issues */
  if (e->k == VJMP) {
    printf_debug("[DEBUG] aqlK_goiffalse: detected VJMP, handling specially\n");
    negatecondition(fs, e);  /* jump when it is false */
    printf_debug("[DEBUG] aqlK_goiffalse: negatecondition called\n");
    pc = e->u.info;  /* save jump position */
    printf_debug("[DEBUG] aqlK_goiffalse: calling aqlK_concat with pc=%d\n", pc);
    aqlK_concat(fs, &e->t, pc);  /* insert new jump in 't' list */
    printf_debug("[DEBUG] aqlK_goiffalse: aqlK_concat completed, calling aqlK_patchtohere\n");
    aqlK_patchtohere(fs, e->f);  /* false list jumps to here (to go through) */
    printf_debug("[DEBUG] aqlK_goiffalse: aqlK_patchtohere completed\n");
    /* After negation, the true list becomes the false list */
    e->f = e->t;  /* false list is now the negated true list */
    e->t = NO_JUMP;
    printf_debug("[DEBUG] aqlK_goiffalse: VJMP handling completed, e->f=%d\n", e->f);
    return;
  }
  
  printf_debug("[DEBUG] aqlK_goiffalse: about to enter switch statement\n");
  switch (e->k) {
    case VJMP: {
      pc = e->u.info;  /* already jump if true */
      break;
    }
    case VNIL: case VFALSE: {
      pc = NO_JUMP;  /* always false; do nothing */
      break;
    }
    case VVARARG: {
      printf_debug("[DEBUG] aqlK_goiffalse: handling VVARARG case\n");
      /* VVARARG should not appear in condition expressions, but handle it gracefully */
      pc = jumponcond(fs, e, 1);  /* jump when true */
      break;
    }
    default: {
      printf_debug("[DEBUG] aqlK_goiffalse: default case, calling jumponcond with e->k=%d\n", e->k);
      pc = jumponcond(fs, e, 0);  /* jump when false */
      printf_debug("[DEBUG] aqlK_goiffalse: jumponcond completed, pc=%d\n", pc);
      break;
    }
  }
  aqlK_concat(fs, &e->t, pc);  /* insert new jump in 't' list */
  aqlK_patchtohere(fs, e->f);  /* false list jumps to here (to go through) */
  e->f = NO_JUMP;
}

/*
** Code 'not e', doing constant folding.
*/
static void codenot (FuncState *fs, expdesc *e) {
  aqlK_dischargevars(fs, e);
  switch (e->k) {
    case VNIL: case VFALSE: {
      e->k = VTRUE;  /* true == not nil == not false */
      break;
    }
    case VK: case VKFLT: case VKINT: case VKSTR: case VTRUE: {
      e->k = VFALSE;  /* false == not "x" == not 0.5 == not 1 == not true */
      break;
    }
    case VJMP: {
      negatecondition(fs, e);
      break;
    }
    case VRELOC:
    case VNONRELOC: {
      discharge2anyreg(fs, e);
      aqlK_freeexp(fs, e);
      e->u.info = aqlK_codeABC(fs, OP_NOT, 0, e->u.info, 0);
      e->k = VRELOC;
      break;
    }
    default: aql_assert(0);  /* cannot happen */
  }
  /* interchange true and false lists */
  { int temp = e->f; e->f = e->t; e->t = temp; }
  removevalues(fs, e->f);  /* values are useless when negated */
  removevalues(fs, e->t);
}

/*
** Create expression 't[k]'. 't' must have its final result already in a
** register or upvalue. Upvalue 't' can have an arbitrary index ('idx')
** but register 't' must have index < 'fs->nactvar' (so it cannot be
** a pseudo-local, whose index is >= 'fs->nactvar').
*/
void aqlK_indexed (FuncState *fs, expdesc *t, expdesc *k) {
  if (k->k == VKSTR)
    str2K(fs, k);
  aql_assert(!hasjumps(t) &&
             (vkisinreg(t->k) || t->k == VUPVAL));
  int reg = fs->freereg++;  /* allocate register for result */
  printf_debug("[DEBUG] aqlK_indexed: using register %d (freereg was %d)\n", reg, fs->freereg - 1);
  if (t->k == VUPVAL && !vkisvar(k->k))  /* upvalue indexed by constant? */
    aqlK_codeABC(fs, OP_GETTABUP, reg, t->u.info, aqlK_exp2RK(fs, k));
  else  /* register indexed by constant/register */
    aqlK_codeABC(fs, OP_GETTABUP, reg, t->u.info, aqlK_exp2RK(fs, k));
  t->k = VRELOC;
  t->u.info = fs->pc - 1;  /* point to the OP_GETTABLE instruction */
}


/*
** Emit code for comparisons.
** 'e1' was already put in R/K form by 'luaK_infix'.
*/
static void codecomp (FuncState *fs, BinOpr opr, expdesc *e1, expdesc *e2, int line) {
  printf_debug("[DEBUG] codecomp: e1->k=%d, e1->u.info=%d, e2->k=%d, e2->u.info=%d\n", 
    e1->k, e1->u.info, e2->k, e2->u.info);
  
  /* Ensure both operands are in R/K form */
  int rk1 = aqlK_exp2RK(fs, e1);
  int rk2 = aqlK_exp2RK(fs, e2);
  printf_debug("[DEBUG] codecomp: rk1=%d, rk2=%d, freereg=%d\n", rk1, rk2, fs->freereg);
  aqlK_freeexp(fs, e2);
  
  /* Map comparison operators to VM instructions */
  OpCode op;
  int k = 1;  /* default: jump when condition is true */
  
  printf_debug("[DEBUG] codecomp: operator=%d (OPR_NE=%d)\n", opr, OPR_NE);
  switch (opr) {
    case OPR_EQ:
      op = OP_EQ;
      k = 1;  /* jump when equal */
      printf_debug("[DEBUG] codecomp: OPR_EQ -> OP_EQ, k=1\n");
      break;
    case OPR_NE:
      op = OP_EQ;
      k = 0;  /* jump when not equal (i.e., when EQ is false) */
      printf_debug("[DEBUG] codecomp: OPR_NE -> OP_EQ, k=0\n");
      break;
    case OPR_LT:
      op = OP_LT;
      k = 1;  /* jump when less than */
      printf_debug("[DEBUG] codecomp: OPR_LT -> OP_LT, k=1\n");
      break;
    case OPR_LE:
      op = OP_LE;
      k = 1;  /* jump when less or equal */
      printf_debug("[DEBUG] codecomp: OPR_LE -> OP_LE, k=1\n");
      break;
    default: 
      printf_debug("[DEBUG] codecomp: unknown operator %d, defaulting to EQ\n", opr);
      aql_assert(0);
      op = OP_EQ;
      k = 1;
  }
  
  /* Generate conditional jump instruction using condjump */
  printf_debug("[DEBUG] codecomp: generating conditional jump with %s, k=%d\n", 
    (op == OP_EQ) ? "EQ" : (op == OP_LT) ? "LT" : "LE", k);
  
  e1->u.info = condjump(fs, op, 0, rk1, rk2, k);  /* A=0 (unused), B=rk1, C=rk2, k=condition */
  e1->k = VJMP;  /* mark as jump expression */
  
  printf_debug("[DEBUG] codecomp: generated VJMP expression with jump at PC=%d\n", e1->u.info);
  aqlK_fixline(fs, line);
}

/*
** Emit code to adjust stack to 'nvars' values. If there are no
** multret expressions, then number of values is 'nexps'.
** Otherwise, use 'nexps' as a minimum.
*/
void aqlK_setlist (FuncState *fs, int base, int nelems, int tostore) {
  /* AQL uses containers instead of tables */
  int b = (tostore == AQL_MULTRET) ? 0 : tostore;
  /* Simplified: use SETPROP for container element setting */
  for (int i = 0; i < nelems; i++) {
    aqlK_codeABC(fs, OP_SETPROP, base, i, base + i + 1);
  }
  fs->freereg = base + 1;  /* free registers with list values */
}

/*
** Emit bytecode to create a new table. Parameter 'pc' is the address
** of the OP_NEWTABLE instruction that creates the table (or its previous
** instruction). Parameter 'ra' is the register where the table will be
** stored. Parameter 'b' and 'c' are the number of array elements and
** hash elements, respectively.
*/
void aqlK_settablesize (FuncState *fs, int pc, int ra, int asize, int hsize) {
  Instruction *inst = &fs->f->code[pc];
  int rb = (asize > 0) ? aqlO_ceillog2(asize) + 1 : 0;  /* encode array size */
  int extra = asize - ((1 << (rb - 1)) - 1);  /* extra elements */
  int rc = (hsize > 0) ? aqlO_ceillog2(hsize) + 1 : 0;   /* encode hash size */
  if (GETARG_B(*inst) != rb || GETARG_C(*inst) != rc) {
    /* redo instruction if 'b' or 'c' changed */
    *inst = CREATE_ABC(OP_NEWOBJECT, ra, rb, rc);
  }
  if (extra > 0) {
    /* will set extra elements in auxiliary instruction */
    aqlK_codeextraarg(fs, extra);
  }
}

/*
** Emit instruction 'op' with given arguments.
** Add new instruction at the end of code array.
** Update 'lastpc' and 'jpc'.
*/
static int luaK_codeABCk (FuncState *fs, OpCode o, int a, int b, int c, int k) {
  aql_assert(getOpMode(o) == iABC);
  aql_assert(a <= MAXARG_A && b <= MAXARG_B && c <= MAXARG_C && (k & ~1) == 0);
  return aqlK_code(fs, CREATE_ABC(o, a, b, c | (k << 8)));
}

/*
** Emit an "jump" instruction.
** Use 'jump' to keep 'jpc' updated.
*/
int aqlK_jump (FuncState *fs) {
  return aqlK_codeAsBx(fs, OP_JMP, 0, NO_JUMP);
}

/*
** Code a 'return' instruction
*/
int aqlK_ret (FuncState *fs, int first, int nret) {
  OpCode op;
  switch (nret) {
    case 0:
      op = OP_RET_VOID;
      break;
    case 1:
      op = OP_RET_ONE;
      break;
    default:
      op = OP_RET;
      break;
  }
  aqlK_codeABC(fs, op, first, nret + 1, 0);
  return fs->pc - 1;
}

/*
** Code a "conditional jump", from 'v' to 'dest'. If 'v' is
** false, will jump to 'dest'. If 'v' is true, will fall through.
*/
static int condjump (FuncState *fs, OpCode op, int a, int b, int c, int k) {
  /* Use AQL's CREATE_ABCk macro for proper k bit encoding */
  int inst = aqlK_code(fs, CREATE_ABCk(op, a, b, c, k));
  return aqlK_jump(fs);
}

/*
** returns current 'pc' and marks it as a jump target (to avoid wrong
** optimizations with consecutive instructions not in the same basic block).
** discharge list 'list' into 'pc'.
*/
int aqlK_getlabel (FuncState *fs) {
  fs->lasttarget = fs->pc;
  return fs->pc;
}

/*
** Returns the position of the instruction "controlling" a given
** jump (that is, its condition), or the jump itself if it is
** unconditional.
*/
static Instruction *getjumpcontrol (FuncState *fs, int pc) {
  Instruction *pi = &fs->f->code[pc];
  if (pc >= 1 && testTMode(GET_OPCODE(*(pi-1))))
    return pi-1;
  else
    return pi;
}

/*
** Patch destination register for a TESTSET instruction.
** If instruction in position 'node' is not a TESTSET, return 0 ("fails").
** Otherwise, if 'reg' is not 'NO_REG', set it as the destination
** register. Otherwise, change instruction to a simple 'TEST' (produces
** no register value)
*/
static int patchtestreg (FuncState *fs, int node, int reg) {
  Instruction *i = getjumpcontrol(fs, node);
  if (GET_OPCODE(*i) != OP_TESTSET)
    return 0;  /* cannot patch other instructions */
  if (reg != NO_REG && reg != GETARG_B(*i)) {
    SETARG_A(*i, reg);
  } else {
     /* no register to put value or register already has the value;
        change instruction to simple test */
    *i = CREATE_ABC(OP_TEST, GETARG_B(*i), 0, GETARG_C(*i));
  }
  return 1;
}

/*
** Traverse a list of tests ensuring no one produces a value
*/
static void removevalues (FuncState *fs, int list) {
  for (; list != NO_JUMP; list = getjump(fs, list))
    patchtestreg(fs, list, NO_REG);
}

/*
** Traverse a list of tests, patching their destination address and
** registers: tests producing values jump to 'vtarget' (and put their
** values in 'reg'), other tests jump to 'dtarget'.
*/
static void patchlistaux (FuncState *fs, int list, int vtarget, int reg,
                          int dtarget) {
  while (list != NO_JUMP) {
    int next = getjump(fs, list);
    if (patchtestreg(fs, list, reg))
      fixjump(fs, list, vtarget);
    else
      fixjump(fs, list, dtarget);  /* jump to default target */
    list = next;
  }
}

/*
** Ensure all pending jumps to current position are fixed (jumping
** to current position with no further jumps) and reset list of
** pending jumps
*/
void aqlK_patchtohere (FuncState *fs, int list) {
  printf_debug("[DEBUG] aqlK_patchtohere: entering with list=%d\n", list);
  
  if (!fs) {
    printf_debug("[ERROR] aqlK_patchtohere: fs is NULL\n");
    return;
  }
  
  printf_debug("[DEBUG] aqlK_patchtohere: calling aqlK_getlabel\n");
  int hr = aqlK_getlabel(fs);  /* mark "here" as a jump target */
  printf_debug("[DEBUG] aqlK_patchtohere: aqlK_getlabel returned hr=%d\n", hr);
  
  printf_debug("[DEBUG] aqlK_patchtohere: calling aqlK_patchlist\n");
  aqlK_patchlist(fs, list, hr);
  printf_debug("[DEBUG] aqlK_patchtohere: aqlK_patchlist completed\n");
}

/*
** Path all jumps in 'list' to jump to 'target'.
** (The assert means that we cannot fix a jump to a forward address
** because we only know addresses once code is generated.)
*/
void aqlK_patchlist (FuncState *fs, int list, int target) {
  printf_debug("[DEBUG] aqlK_patchlist: entering with list=%d, target=%d, fs->pc=%d\n", list, target, fs->pc);
  
  if (list == NO_JUMP) {
    printf_debug("[DEBUG] aqlK_patchlist: list is NO_JUMP, returning\n");
    return;  /* nothing to patch */
  }
  
  if (target == fs->pc) {
    printf_debug("[DEBUG] aqlK_patchlist: target == fs->pc, this would cause recursion - using patchlistaux instead\n");
    /* Don't call aqlK_patchtohere to avoid infinite recursion */
    patchlistaux(fs, list, target, NO_REG, target);
  } else {
    aql_assert(target < fs->pc);
    patchlistaux(fs, list, target, NO_REG, target);
  }
  printf_debug("[DEBUG] aqlK_patchlist: completed\n");
}

/*
** Path all jumps in 'list' to close upvalues up to given 'level'
** (The assertion checks that jumps either were closing nothing
** or were closing higher levels, from inner blocks.)
*/
void aqlK_patchclose (FuncState *fs, int list, int level) {
  level++;  /* argument is +1 to reserve 0 as non-op */
  for (; list != NO_JUMP; list = getjump(fs, list)) {
    aql_assert(GET_OPCODE(fs->f->code[list]) == OP_JMP &&
                (GETARG_A(fs->f->code[list]) == 0 ||
                 GETARG_A(fs->f->code[list]) >= level));
    SETARG_A(fs->f->code[list], level);
  }
}

/*
** Emit instruction to close upvalues up to given 'level'
** (The assertion checks that there is no previous instruction to set
** an upvalue.)
*/
void aqlK_codeclose (FuncState *fs, int level) {
  aql_assert(level >= fs->bl->nactvar);
  aqlK_codeABC(fs, OP_CLOSE, level, 0, 0);
}

/*
** Create a copy of instruction 'i' setting 'a' as its first argument.
** (Instruction 'i' must have format 'iABC'.)
*/
static Instruction dischargejpc (FuncState *fs) {
  /* Simplified: AQL doesn't use jpc */
  return 0;
}

/*
** Add elements in 'list' to list of pending jumps to "here"
** (current position)
*/
void aqlK_concat (FuncState *fs, int *l1, int l2) {
  if (l2 == NO_JUMP) return;  /* nothing to concatenate */
  else if (*l1 == NO_JUMP)  /* no original list? */
    *l1 = l2;  /* 'l1' points to 'l2' */
  else {
    int list = *l1;
    int next;
    while ((next = getjump(fs, list)) != NO_JUMP)  /* find last element */
      list = next;
    fixjump(fs, list, l2);  /* last element links to 'l2' */
  }
}

/*
** Create a jump instruction and return its position, so its destination
** can be fixed later (with 'fixjump').
*/
static int makejump (FuncState *fs, int list) {
  int j = aqlK_jump(fs);  /* new jump */
  aqlK_concat(fs, &j, list);  /* chain it to the original list */
  return j;
}



/*
** Emit a 'nil' instruction, from 'from' to 'n' values.
** If there is a previous instruction setting exactly the previous
** range, extend its range or delete it (if this instruction
** subsumes the previous one).
*/
void aqlK_nil (FuncState *fs, int from, int n) {
  int l = from + n - 1;  /* last register to set nil */
  Instruction *previous;
  if (fs->pc > fs->lasttarget) {  /* no jumps to current position? */
    previous = &fs->f->code[fs->pc-1];
    if (GET_OPCODE(*previous) == OP_LOADNIL) {  /* previous is LOADNIL? */
      int pfrom = GETARG_A(*previous);  /* get previous range */
      int pl = pfrom + GETARG_B(*previous);
      if ((pfrom <= from && from <= pl + 1) ||
          (from <= pfrom && pfrom <= l + 1)) {  /* can connect both? */
        if (pfrom < from) from = pfrom;  /* from = min(from, pfrom) */
        if (pl > l) l = pl;  /* l = max(l, pl) */
        SETARG_A(*previous, from);
        SETARG_B(*previous, l - from);
        return;
      }  /* else go through */
    }
  }
  aqlK_codeABC(fs, OP_LOADNIL, from, n - 1, 0);  /* else no optimization */
}

/*
** Convert expression to a value (discharge variables).
*/
void aqlK_exp2val (FuncState *fs, expdesc *e) {
  if (hasjumps(e))
    aqlK_exp2anyreg(fs, e);
  else
    aqlK_dischargevars(fs, e);
}

/*
** Gets the destination address of a jump instruction. Used to traverse
** a list of jumps.
*/
static int getjump (FuncState *fs, int pc) {
  int offset = GETARG_sBx(fs->f->code[pc]);
  printf_debug("[DEBUG] getjump: pc=%d, offset=%d\n", pc, offset);
  if (offset == NO_JUMP)  /* point to itself represents end of list */
    return NO_JUMP;  /* end of list */
  else {
    int next = (pc+1)+offset;
    printf_debug("[DEBUG] getjump: next=%d\n", next);
    /* Detect potential infinite loops */
    if (next == pc) {
      printf_debug("[ERROR] getjump: detected self-reference at pc=%d\n", pc);
      return NO_JUMP;  /* break the loop */
    }
    return next;  /* turn offset into absolute position */
  }
}

/*
** Fix jump instruction at position 'pc' to jump to 'dest'.
** (Jump addresses are relative in AQL)
*/
static void fixjump (FuncState *fs, int pc, int dest) {
  Instruction *jmp = &fs->f->code[pc];
  int offset = dest - (pc + 1);
  printf_debug("[DEBUG] fixjump: pc=%d, dest=%d, offset=%d\n", pc, dest, offset);
  printf_debug("[DEBUG] fixjump: OFFSET_sBx=%d, MAXARG_sBx=%d\n", OFFSET_sBx, MAXARG_sBx);
  printf_debug("[DEBUG] fixjump: range check: %d <= %d <= %d\n", 
               -OFFSET_sBx, offset, OFFSET_sBx);
  
  aql_assert(dest != NO_JUMP);
  if (!(-OFFSET_sBx <= offset && offset <= OFFSET_sBx))
    aqlX_syntaxerror(fs->ls, "control structure too long");
  aql_assert(GET_OPCODE(*jmp) == OP_JMP);
  SETARG_sBx(*jmp, offset);
  printf_debug("[DEBUG] fixjump: set sBx=%d for instruction at pc=%d\n", offset, pc);
}

/*
** check whether list has any jump that do not produce a value
** or produce an inverted value
*/
static int need_value (FuncState *fs, int list) {
  for (; list != NO_JUMP; list = getjump(fs, list)) {
    Instruction i = *getjumpcontrol(fs, list);
    if (GET_OPCODE(i) != OP_TESTSET) return 1;
  }
  return 0;  /* not found */
}

/*
** Generate code to load a boolean value
*/
static int code_loadbool (FuncState *fs, int A, int b, int jump) {
  aqlK_getlabel(fs);  /* those instructions may be jump targets */
  if (b) {
    int pc = aqlK_codeABC(fs, OP_LOADTRUE, A, 0, 0);
    if (jump) aqlK_jump(fs);  /* skip next instruction if needed */
    return pc;
  } else {
    int pc = aqlK_codeABC(fs, OP_LOADFALSE, A, 0, 0);
    if (jump) aqlK_jump(fs);  /* skip next instruction if needed */
    return pc;
  }
}

/*
** Ensures expression value is in register reg.
*/
static void exp2reg (FuncState *fs, expdesc *e, int reg) {
  discharge2reg(fs, e, reg);
  if (e->k == VJMP)
    aqlK_concat(fs, &e->t, e->u.info);  /* put this jump in t list */
  if (hasjumps(e)) {
    int final;  /* position after whole expression */
    int p_f = NO_JUMP;  /* position of an eventual LOAD false */
    int p_t = NO_JUMP;  /* position of an eventual LOAD true */
    if (need_value(fs, e->t) || need_value(fs, e->f)) {
      int fj = (e->k == VJMP) ? NO_JUMP : aqlK_jump(fs);
      p_f = code_loadbool(fs, reg, 0, 1);
      p_t = code_loadbool(fs, reg, 1, 0);
      aqlK_patchtohere(fs, fj);
    }
    final = aqlK_getlabel(fs);
    patchlistaux(fs, e->f, final, reg, p_f);
    patchlistaux(fs, e->t, final, reg, p_t);
  }
  e->f = e->t = NO_JUMP;
  e->u.info = reg;
  e->k = VNONRELOC;
}

/*
** Remove last instruction from function.
*/
static void removelastinstruction (FuncState *fs) {
  fs->pc--;
}

/*
** Convert string expression to constant.
*/
static void str2K (FuncState *fs, expdesc *e) {
  aql_assert(e->k == VKSTR);
  e->u.info = aqlK_stringK(fs, e->u.strval);
  e->k = VK;
}


