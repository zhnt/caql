/*
** $Id: aopcodes.h $
** Opcodes for AQL virtual machine
** See Copyright Notice in aql.h
*/

#ifndef aopcodes_h
#define aopcodes_h

#include "aconf.h"

/*
** Instruction type for AQL VM
*/
/* Note: Instruction is defined in alimits.h */

/*
===========================================================================
  AQL instructions are unsigned 32-bit integers.
  All instructions have an opcode in the first 7 bits.
  Instructions can have the following formats:

        3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
        1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
iABC          C(8)     |      B(8)     |k|     A(8)     |   Op(7)     |
iABCk         C(8)     |      B(8)     |k|     A(8)     |   Op(7)     |
iABx                Bx(17)              |k|     A(8)     |   Op(7)     |
iAsBx              sBx (signed)(17)     |k|     A(8)     |   Op(7)     |
iAx                           Ax(25)                     |   Op(7)     |

  A signed argument is represented in excess K: the represented value is
  the written unsigned value minus K, where K is half the maximum for the
  corresponding unsigned argument.
===========================================================================
*/

enum OpMode {iABC, iABx, iAsBx, iAx, isJ};  /* basic instruction formats */

/*
** size and position of opcode arguments.
** 完全兼容 Lua 5.4 的指令格式:
** - OP: 7 bits (0-127 opcodes)
** - A: 8 bits (0-255 registers)  
** - B: 8 bits (0-255 registers/constants)
** - C: 8 bits (0-255 registers/constants)
** - k: 1 bit (boolean flag)
*/
#define SIZE_C		8
#define SIZE_B		8
#define SIZE_Bx		(SIZE_C + SIZE_B + 1)
#define SIZE_A		8
#define SIZE_Ax		(SIZE_Bx + SIZE_A)
#define SIZE_sJ		(SIZE_Bx + SIZE_A)

#define SIZE_OP		7

#define POS_OP		0

#define POS_A		(POS_OP + SIZE_OP)
#define POS_k		(POS_A + SIZE_A)
#define POS_B		(POS_k + 1)
#define POS_C		(POS_B + SIZE_B)

#define POS_Bx		POS_k
#define POS_Ax		POS_A
#define POS_sJ		POS_A

/*
** limits for opcode arguments.
** we use (signed) 'int' to manipulate most arguments,
** so they must fit in ints.
*/

/* Check whether type 'int' has at least 'b' bits ('b' < 32) */
#define L_INTHASBITS(b)		((UINT_MAX >> ((b) - 1)) >= 1)

#if L_INTHASBITS(SIZE_Bx)
#define MAXARG_Bx	((1<<SIZE_Bx)-1)
#else
#define MAXARG_Bx	INT_MAX
#endif

#define OFFSET_sBx	(MAXARG_Bx>>1)         /* 'sBx' is signed */
#define MAXARG_sBx	(MAXARG_Bx>>1)

#if L_INTHASBITS(SIZE_sJ)
#define MAXARG_sJ	((1<<SIZE_sJ)-1)
#else
#define MAXARG_sJ	INT_MAX
#endif

#define OFFSET_sJ	(MAXARG_sJ>>1)         /* 'sJ' is signed */

#if L_INTHASBITS(SIZE_Ax)
#define MAXARG_Ax	((1<<SIZE_Ax)-1)
#else
#define MAXARG_Ax	INT_MAX
#endif

#if L_INTHASBITS(SIZE_A)
#define MAXARG_A	((1<<SIZE_A)-1)
#else
#define MAXARG_A	INT_MAX
#endif

#if L_INTHASBITS(SIZE_B)
#define MAXARG_B	((1<<SIZE_B)-1)
#else
#define MAXARG_B	INT_MAX
#endif

#if L_INTHASBITS(SIZE_C)
#define MAXARG_C	((1<<SIZE_C)-1)
#else
#define MAXARG_C	INT_MAX
#endif

#define OFFSET_sC	(MAXARG_C >> 1)

#define int2sC(i)	((i) + OFFSET_sC)
#define sC2int(i)	((i) - OFFSET_sC)

/* creates a mask with 'n' 1 bits at position 'p' */
#define MASK1(n,p)	((~((~(aql_Unsigned)0)<<(n)))<<(p))

/* creates a mask with 'n' 0 bits at position 'p' */
#define MASK0(n,p)	(~MASK1(n,p))

/*
** the following macros help to manipulate instructions
*/

#define GET_OPCODE(i)	(cast(OpCode, ((i)>>POS_OP) & MASK1(SIZE_OP,0)))
#define SET_OPCODE(i,o)	((i) = (((i)&MASK0(SIZE_OP,POS_OP)) | \
		((cast(aql_Unsigned, o)<<POS_OP)&MASK1(SIZE_OP,POS_OP))))

#define getarg(i,pos,size)	(cast_int(((i)>>(pos)) & MASK1(size,0)))
#define setarg(i,v,pos,size)	((i) = (((i)&MASK0(size,pos)) | \
                ((cast(aql_Unsigned, v)<<(pos))&MASK1(size,pos))))

#define GETARG_A(i)	getarg(i, POS_A, SIZE_A)
#define SETARG_A(i,v)	setarg(i, v, POS_A, SIZE_A)

#define GETARG_B(i)	getarg(i, POS_B, SIZE_B)
#define GETARG_sB(i)	sC2int(GETARG_B(i))
#define SETARG_B(i,v)	setarg(i, v, POS_B, SIZE_B)

#define GETARG_C(i)	getarg(i, POS_C, SIZE_C)
#define GETARG_sC(i)	sC2int(GETARG_C(i))
#define SETARG_C(i,v)	setarg(i, v, POS_C, SIZE_C)

#define TESTARG_k(i)	(cast_int(((i) & (1u << POS_k))))
#define GETARG_k(i)	getarg(i, POS_k, 1)
#define SETARG_k(i,v)	setarg(i, v, POS_k, 1)

#define GETARG_Bx(i)	getarg(i, POS_Bx, SIZE_Bx)
#define SETARG_Bx(i,v)	setarg(i, v, POS_Bx, SIZE_Bx)

#define GETARG_Ax(i)	getarg(i, POS_Ax, SIZE_Ax)
#define SETARG_Ax(i,v)	setarg(i, v, POS_Ax, SIZE_Ax)

#define GETARG_sBx(i)	(GETARG_Bx(i) - OFFSET_sBx)
#define SETARG_sBx(i,b)	SETARG_Bx((i),cast_uint((b)+OFFSET_sBx))

#define GETARG_sJ(i)	(getarg(i, POS_sJ, SIZE_sJ) - OFFSET_sJ)
#define SETARG_sJ(i,j)	setarg(i, cast_uint((j)+OFFSET_sJ), POS_sJ, SIZE_sJ)

/*
** K operands in instructions
*/
#define BITRK		(1 << (SIZE_B - 1))

/* test whether value is a constant */
#define ISK(x)		((x) & BITRK)

/* gets the index of the constant */
#define INDEXK(r)	((int)(r) & ~BITRK)

#if !defined(MAXINDEXRK)  /* (for debugging only) */
#define MAXINDEXRK	(BITRK - 1)
#endif

/* code a constant index as a RK value */
#define RKASK(x)	((x) | BITRK)

/*
** Invalid register that fits in 8 bits
*/
#define NO_REG		MAXARG_A

/*
** R(x) - register
** K(x) - constant (in constant table)
** RK(x) == if ISK(x) then K(INDEXK(x)) else R(x)
*/

/*
** AQL操作码枚举 - 与Lua 5.4完全兼容
*/
typedef enum {
  /* === Lua 5.4 Compatible Opcodes (0-82) === */
  OP_MOVE,        /* 0   A B     R[A] := R[B] */
  OP_LOADI,       /* 1   A sBx   R[A] := sBx */
  OP_LOADF,       /* 2   A sBx   R[A] := (lua_Number)sBx */
  OP_LOADK,       /* 3   A Bx    R[A] := K[Bx] */
  OP_LOADKX,      /* 4   A       R[A] := K[extra arg] */
  OP_LOADFALSE,   /* 5   A       R[A] := false */
  OP_LFALSESKIP,  /* 6   A       R[A] := false; pc++ */
  OP_LOADTRUE,    /* 7   A       R[A] := true */
  OP_LOADNIL,     /* 8   A B     R[A], R[A+1], ..., R[A+B] := nil */
  OP_GETUPVAL,    /* 9   A B     R[A] := UpValue[B] */
  OP_SETUPVAL,    /* 10  A B     UpValue[B] := R[A] */
  OP_GETTABUP,    /* 11  A B C   R[A] := UpValue[B][K[C]:shortstring] */
  OP_GETTABLE,    /* 12  A B C   R[A] := R[B][R[C]] */
  OP_GETI,        /* 13  A B C   R[A] := R[B][C] */
  OP_GETFIELD,    /* 14  A B C   R[A] := R[B][K[C]:shortstring] */
  OP_SETTABUP,    /* 15  A B C   UpValue[A][K[B]:shortstring] := RK(C) */
  OP_SETTABLE,    /* 16  A B C   R[A][R[B]] := RK(C) */
  OP_SETI,        /* 17  A B C   R[A][B] := RK(C) */
  OP_SETFIELD,    /* 18  A B C   R[A][K[B]:shortstring] := RK(C) */
  OP_NEWTABLE,    /* 19  A B C k R[A] := {} */
  OP_SELF,        /* 20  A B C   R[A+1] := R[B]; R[A] := R[B][RK(C):string] */
  OP_ADDI,        /* 21  A B sC  R[A] := R[B] + sC */
  OP_ADDK,        /* 22  A B C   R[A] := R[B] + K[C]:number */
  OP_SUBK,        /* 23  A B C   R[A] := R[B] - K[C]:number */
  OP_MULK,        /* 24  A B C   R[A] := R[B] * K[C]:number */
  OP_MODK,        /* 25  A B C   R[A] := R[B] % K[C]:number */
  OP_POWK,        /* 26  A B C   R[A] := R[B] ^ K[C]:number */
  OP_DIVK,        /* 27  A B C   R[A] := R[B] / K[C]:number */
  OP_IDIVK,       /* 28  A B C   R[A] := R[B] // K[C]:number */
  OP_BANDK,       /* 29  A B C   R[A] := R[B] & K[C]:integer */
  OP_BORK,        /* 30  A B C   R[A] := R[B] | K[C]:integer */
  OP_BXORK,       /* 31  A B C   R[A] := R[B] ~ K[C]:integer */
  OP_SHRI,        /* 32  A B sC  R[A] := R[B] >> sC */
  OP_SHLI,        /* 33  A B sC  R[A] := sC << R[B] */
  OP_ADD,         /* 34  A B C   R[A] := R[B] + R[C] */
  OP_SUB,         /* 35  A B C   R[A] := R[B] - R[C] */
  OP_MUL,         /* 36  A B C   R[A] := R[B] * R[C] */
  OP_MOD,         /* 37  A B C   R[A] := R[B] % R[C] */
  OP_POW,         /* 38  A B C   R[A] := R[B] ^ R[C] */
  OP_DIV,         /* 39  A B C   R[A] := R[B] / R[C] */
  OP_IDIV,        /* 40  A B C   R[A] := R[B] // R[C] */
  OP_BAND,        /* 41  A B C   R[A] := R[B] & R[C] */
  OP_BOR,         /* 42  A B C   R[A] := R[B] | R[C] */
  OP_BXOR,        /* 43  A B C   R[A] := R[B] ~ R[C] */
  OP_SHL,         /* 44  A B C   R[A] := R[B] << R[C] */
  OP_SHR,         /* 45  A B C   R[A] := R[B] >> R[C] */
  OP_MMBIN,       /* 46  A B C   call C metamethod over R[A] and R[B] */
  OP_MMBINI,      /* 47  A sB C k call C metamethod over R[A] and sB */
  OP_MMBINK,      /* 48  A B C k call C metamethod over R[A] and K[B] */
  OP_UNM,         /* 49  A B     R[A] := -R[B] */
  OP_BNOT,        /* 50  A B     R[A] := ~R[B] */
  OP_NOT,         /* 51  A B     R[A] := not R[B] */
  OP_LEN,         /* 52  A B     R[A] := #R[B] (length operator) */
  OP_CONCAT,      /* 53  A B     R[A] := R[A].. ... ..R[A + B - 1] */
  OP_CLOSE,       /* 54  A       close all upvalues >= R[A] */
  OP_TBC,         /* 55  A       mark variable A "to be closed" */
  OP_JMP,         /* 56  sJ      pc += sJ */
  OP_EQ,          /* 57  A B k   if ((R[A] == R[B]) ~= k) then pc++ */
  OP_LT,          /* 58  A B k   if ((R[A] <  R[B]) ~= k) then pc++ */
  OP_LE,          /* 59  A B k   if ((R[A] <= R[B]) ~= k) then pc++ */
  OP_EQK,         /* 60  A B k   if ((R[A] == K[B]) ~= k) then pc++ */
  OP_EQI,         /* 61  A sB k  if ((R[A] == sB) ~= k) then pc++ */
  OP_LTI,         /* 62  A sB k  if ((R[A] < sB) ~= k) then pc++ */
  OP_LEI,         /* 63  A sB k  if ((R[A] <= sB) ~= k) then pc++ */
  OP_GTI,         /* 64  A sB k  if ((R[A] > sB) ~= k) then pc++ */
  OP_GEI,         /* 65  A sB k  if ((R[A] >= sB) ~= k) then pc++ */
  OP_TEST,        /* 66  A k     if (not R[A] == k) then pc++ */
  OP_TESTSET,     /* 67  A B k   if (not R[B] == k) then pc++ else R[A] := R[B] */
  OP_CALL,        /* 68  A B C   R[A], ... ,R[A+C-2] := R[A](R[A+1], ... ,R[A+B-1]) */
  OP_TAILCALL,    /* 69  A B C k return R[A](R[A+1], ... ,R[A+B-1]) */
  OP_RETURN,      /* 70  A B C k return R[A], ... ,R[A+B-2] */
  OP_RETURN0,     /* 71           return */
  OP_RETURN1,     /* 72  A       return R[A] */
  OP_FORLOOP,     /* 73  A Bx    update counters; if loop continues then pc-=Bx */
  OP_FORPREP,     /* 74  A Bx    <check values and prepare counters>; if not to run then pc+=Bx+1 */
  OP_TFORPREP,    /* 75  A Bx    create upvalue for R[A + 3]; pc+=Bx */
  OP_TFORCALL,    /* 76  A C     R[A+4], ... ,R[A+3+C] := R[A](R[A+1], R[A+2]) */
  OP_TFORLOOP,    /* 77  A Bx    if R[A+2] ~= nil then { R[A]=R[A+2]; pc -= Bx } */
  OP_SETLIST,     /* 78  A B C k R[A][C+i] := R[A+i], 1 <= i <= B */
  OP_CLOSURE,     /* 79  A Bx    R[A] := closure(KPROTO[Bx]) */
  OP_VARARG,      /* 80  A C     R[A], R[A+1], ..., R[A+C-2] = vararg */
  OP_VARARGPREP,  /* 81  A       (adjust vararg parameters) */
  OP_EXTRAARG,    /* 82  Ax      extra (larger) argument for previous opcode */

  /* === AQL Extensions (83+) === */
  OP_NEWOBJECT,   /* 83  A B C   R[A] := new_object(type[B], size[C]) */
  OP_GETPROP,     /* 84  A B C   R[A] := R[B].property[C] or R[B][R[C]] */
  OP_SETPROP,     /* 85  A B C   R[A].property[B] := R[C] or R[A][R[B]] := R[C] */
  OP_INVOKE,      /* 86  A B C   R[A] := R[B]:method[C](args...) */
  OP_ITER_INIT,   /* 87  A B     R[A] := iter_init(R[B]) */
  OP_ITER_NEXT,   /* 88  A B C   R[C] := iter_next(R[A], R[B]) */
  OP_LOADBUILTIN, /* 89  A B     R[A] := builtin_func[B] */
  OP_CALLBUILTIN, /* 90  A B C   call builtin_func[A] with B args, C results */
  OP_SUBI,        /* 91  A B sC  R[A] := R[B] - sC */
  OP_MULI,        /* 92  A B sC  R[A] := R[B] * sC */
  OP_DIVI,        /* 93  A B sC  R[A] := R[B] / sC */
} OpCode;

#define NUM_OPCODES	((int)(OP_DIVI) + 1)

/*===========================================================================
  Notes:
  (*) In OP_CALL, if (B == 0) then B = top. If (C == 0), then 'top' is
  set to last_result+1, so next open instruction (OP_CALL, OP_RETURN*,
  OP_SETLIST) may use 'top'.

  (*) In OP_VARARG, if (C == 0) then use actual number of varargs and
  set top (like in OP_CALL with C == 0).

  (*) In OP_RETURN, if (B == 0) then return up to 'top'.

  (*) In OP_LOADKX and OP_NEWTABLE, the next instruction is always
  OP_EXTRAARG.

  (*) In OP_SETLIST, if (B == 0) then B = 'top'; if b > 0, then B = b.

  (*) In OP_NEWTABLE, B is log2 of the hash part size (or zero for size 1)
  and C is log2 of the array part size (or zero for size 1).

  (*) For comparisons, k specifies what condition the test should accept
  (true or false).

  (*) All 'skips' (pc++) assume that next instruction is a jump.

  (*) In instructions OP_RETURN/OP_TAILCALL, 'B' is the number of values
  to return, where 0 means to return all values on the stack from 'A' on.
===========================================================================*/

/*
** masks for instruction properties. The format is:
** bits 0-2: op mode
** bit 3: instruction set register A
** bit 4: operator is a test (next instruction must be a jump)
** bit 5: instruction uses 'L->top' set by previous instruction (when B == 0)
** bit 6: instruction sets 'L->top' for next instruction (when C == 0)
** bit 7: instruction can raise errors
*/

enum OpArgMask {
  OpArgN,  /* argument is not used */
  OpArgU,  /* argument is used */
  OpArgR,  /* argument is a register or a jump offset */
  OpArgK   /* argument is a constant or register/constant */
};

/* Note: cast macros are defined in alimits.h, but we need local versions */
#undef cast_byte
#undef cast_uint
#define cast_byte(i)	cast(aql_byte, (i))
#define cast_uint(i)	cast(aql_Unsigned, (i))

/*
** Macros to create opmode values - 完全兼容 Lua 5.4
** 参数：mm(metamethod), ot(out_top), it(in_top), test, seta, mode
*/
#define aqlOpMode(mm,ot,it,t,a,m)  \
    (((mm) << 7) | ((ot) << 6) | ((it) << 5) | ((t) << 4) | ((a) << 3) | (m))

/* 为了向后兼容保留的简化宏 */
#define aqlOpModeABC(test,seta,b,c,extra) \
  aqlOpMode(0, 0, 0, test, seta, iABC)

#define aqlOpModeABx(test,seta,b,c) \
  aqlOpMode(0, 0, 0, test, seta, iABx)

#define aqlOpModeAsBx(test,seta,b,c) \
  aqlOpMode(0, 0, 0, test, seta, iAsBx)

#define aqlOpModeAx(test,seta,ax,unused) \
  aqlOpMode(0, 0, 0, test, seta, iAx)

/*
** Additional utility macros
*/
#define GET_OPMODE(op) getOpMode(op)
#define getBMode(op) ((aql_opmode[op] >> 3) & 1)
#define getCMode(op) ((aql_opmode[op] >> 4) & 1)

/*
** Instruction creation macros
*/
#define CREATE_ABC(o,a,b,c) \
  ((cast(Instruction, o)<<POS_OP) \
  | (cast(Instruction, a)<<POS_A) \
  | (cast(Instruction, b)<<POS_B) \
  | (cast(Instruction, c)<<POS_C))

#define CREATE_ABCk(o,a,b,c,k) \
  ((cast(Instruction, o)<<POS_OP) \
  | (cast(Instruction, a)<<POS_A) \
  | (cast(Instruction, b)<<POS_B) \
  | (cast(Instruction, c)<<POS_C) \
  | (cast(Instruction, k)<<POS_k))

#define CREATE_ABx(o,a,bc) \
  ((cast(Instruction, o)<<POS_OP) \
  | (cast(Instruction, a)<<POS_A) \
  | (cast(Instruction, bc)<<POS_Bx))

#define CREATE_Ax(o,a) \
  ((cast(Instruction, o)<<POS_OP) \
  | (cast(Instruction, a)<<POS_Ax))

#define CREATE_AsBx(o,a,bc) \
  ((cast(Instruction, o)<<POS_OP) \
  | (cast(Instruction, a)<<POS_A) \
  | (cast(Instruction, (bc)+OFFSET_sBx)<<POS_Bx))

#define CREATE_sJ(o,j,k) \
  ((cast(Instruction, o)<<POS_OP) \
  | (cast(Instruction, (j)+OFFSET_sJ)<<POS_sJ) \
  | (cast(Instruction, k)<<POS_k))

/*
** Instruction parsing utilities
*/
/* Parse instruction from text format */
int aql_parse_instruction(const char *opcode, const char *arg1, 
                         const char *arg2, const char *arg3, 
                         const char *arg4, int args, Instruction *result);

/*
** 统一的操作码定义 - 所有相关数组在同一个地方维护
** 确保 OpCode enum、aql_opnames 数组、aql_opmode 数组完全同步
*/

/* 操作码名称数组 - 与 OpCode enum 顺序完全一致 */
static const char *const aql_opnames[NUM_OPCODES+1] = {
  /* === Lua 5.4 Compatible Opcodes (0-82) === */
  "MOVE",         /* 0   A B     R[A] := R[B] */
  "LOADI",        /* 1   A sBx   R[A] := sBx */
  "LOADF",        /* 2   A sBx   R[A] := (lua_Number)sBx */
  "LOADK",        /* 3   A Bx    R[A] := K[Bx] */
  "LOADKX",       /* 4   A       R[A] := K[extra arg] */
  "LOADFALSE",    /* 5   A       R[A] := false */
  "LFALSESKIP",   /* 6   A       R[A] := false; pc++ */
  "LOADTRUE",     /* 7   A       R[A] := true */
  "LOADNIL",      /* 8   A B     R[A], R[A+1], ..., R[A+B] := nil */
  "GETUPVAL",     /* 9   A B     R[A] := UpValue[B] */
  "SETUPVAL",     /* 10  A B     UpValue[B] := R[A] */
  "GETTABUP",     /* 11  A B C   R[A] := UpValue[B][K[C]:shortstring] */
  "GETTABLE",     /* 12  A B C   R[A] := R[B][R[C]] */
  "GETI",         /* 13  A B C   R[A] := R[B][C] */
  "GETFIELD",     /* 14  A B C   R[A] := R[B][K[C]:shortstring] */
  "SETTABUP",     /* 15  A B C   UpValue[A][K[B]:shortstring] := RK(C) */
  "SETTABLE",     /* 16  A B C   R[A][R[B]] := RK(C) */
  "SETI",         /* 17  A B C   R[A][B] := RK(C) */
  "SETFIELD",     /* 18  A B C   R[A][K[B]:shortstring] := RK(C) */
  "NEWTABLE",     /* 19  A B C k R[A] := {} */
  "SELF",         /* 20  A B C   R[A+1] := R[B]; R[A] := R[B][RK(C):string] */
  "ADDI",         /* 21  A B sC  R[A] := R[B] + sC */
  "ADDK",         /* 22  A B C   R[A] := R[B] + K[C]:number */
  "SUBK",         /* 23  A B C   R[A] := R[B] - K[C]:number */
  "MULK",         /* 24  A B C   R[A] := R[B] * K[C]:number */
  "MODK",         /* 25  A B C   R[A] := R[B] % K[C]:number */
  "POWK",         /* 26  A B C   R[A] := R[B] ^ K[C]:number */
  "DIVK",         /* 27  A B C   R[A] := R[B] / K[C]:number */
  "IDIVK",        /* 28  A B C   R[A] := R[B] // K[C]:number */
  "BANDK",        /* 29  A B C   R[A] := R[B] & K[C]:integer */
  "BORK",         /* 30  A B C   R[A] := R[B] | K[C]:integer */
  "BXORK",        /* 31  A B C   R[A] := R[B] ~ K[C]:integer */
  "SHRI",         /* 32  A B sC  R[A] := R[B] >> sC */
  "SHLI",         /* 33  A B sC  R[A] := sC << R[B] */
  "ADD",          /* 34  A B C   R[A] := R[B] + R[C] */
  "SUB",          /* 35  A B C   R[A] := R[B] - R[C] */
  "MUL",          /* 36  A B C   R[A] := R[B] * R[C] */
  "MOD",          /* 37  A B C   R[A] := R[B] % R[C] */
  "POW",          /* 38  A B C   R[A] := R[B] ^ R[C] */
  "DIV",          /* 39  A B C   R[A] := R[B] / R[C] */
  "IDIV",         /* 40  A B C   R[A] := R[B] // R[C] */
  "BAND",         /* 41  A B C   R[A] := R[B] & R[C] */
  "BOR",          /* 42  A B C   R[A] := R[B] | R[C] */
  "BXOR",         /* 43  A B C   R[A] := R[B] ~ R[C] */
  "SHL",          /* 44  A B C   R[A] := R[B] << R[C] */
  "SHR",          /* 45  A B C   R[A] := R[B] >> R[C] */
  "MMBIN",        /* 46  A B C   call C metamethod over R[A] and R[B] */
  "MMBINI",       /* 47  A sB C k call C metamethod over R[A] and sB */
  "MMBINK",       /* 48  A B C k call C metamethod over R[A] and K[B] */
  "UNM",          /* 49  A B     R[A] := -R[B] */
  "BNOT",         /* 50  A B     R[A] := ~R[B] */
  "NOT",          /* 51  A B     R[A] := not R[B] */
  "LEN",          /* 52  A B     R[A] := #R[B] (length operator) */
  "CONCAT",       /* 53  A B     R[A] := R[A].. ... ..R[A + B - 1] */
  "CLOSE",        /* 54  A       close all upvalues >= R[A] */
  "TBC",          /* 55  A       mark variable A "to be closed" */
  "JMP",          /* 56  sJ      pc += sJ */
  "EQ",           /* 57  A B k   if ((R[A] == R[B]) ~= k) then pc++ */
  "LT",           /* 58  A B k   if ((R[A] <  R[B]) ~= k) then pc++ */
  "LE",           /* 59  A B k   if ((R[A] <= R[B]) ~= k) then pc++ */
  "EQK",          /* 60  A B k   if ((R[A] == K[B]) ~= k) then pc++ */
  "EQI",          /* 61  A sB k  if ((R[A] == sB) ~= k) then pc++ */
  "LTI",          /* 62  A sB k  if ((R[A] < sB) ~= k) then pc++ */
  "LEI",          /* 63  A sB k  if ((R[A] <= sB) ~= k) then pc++ */
  "GTI",          /* 64  A sB k  if ((R[A] > sB) ~= k) then pc++ */
  "GEI",          /* 65  A sB k  if ((R[A] >= sB) ~= k) then pc++ */
  "TEST",         /* 66  A k     if (not R[A] == k) then pc++ */
  "TESTSET",      /* 67  A B k   if (not R[B] == k) then pc++ else R[A] := R[B] */
  "CALL",         /* 68  A B C   R[A], ... ,R[A+C-2] := R[A](R[A+1], ... ,R[A+B-1]) */
  "TAILCALL",     /* 69  A B C k return R[A](R[A+1], ... ,R[A+B-1]) */
  "RETURN",       /* 70  A B C k return R[A], ... ,R[A+B-2] */
  "RETURN0",      /* 71           return */
  "RETURN1",      /* 72  A       return R[A] */
  "FORLOOP",      /* 73  A Bx    update counters; if loop continues then pc-=Bx */
  "FORPREP",      /* 74  A Bx    <check values and prepare counters>; if not to run then pc+=Bx+1 */
  "TFORPREP",     /* 75  A Bx    create upvalue for R[A + 3]; pc+=Bx */
  "TFORCALL",     /* 76  A C     R[A+4], ... ,R[A+3+C] := R[A](R[A+1], R[A+2]) */
  "TFORLOOP",     /* 77  A Bx    if R[A+2] ~= nil then { R[A]=R[A+2]; pc -= Bx } */
  "SETLIST",      /* 78  A B C k R[A][C+i] := R[A+i], 1 <= i <= B */
  "CLOSURE",      /* 79  A Bx    R[A] := closure(KPROTO[Bx]) */
  "VARARG",       /* 80  A C     R[A], R[A+1], ..., R[A+C-2] = vararg */
  "VARARGPREP",   /* 81  A       (adjust vararg parameters) */
  "EXTRAARG",     /* 82  Ax      extra (larger) argument for previous opcode */

  /* === AQL Extensions (83+) === */
  "NEWOBJECT",    /* 83  A B C   R[A] := new_object(type[B], size[C]) */
  "GETPROP",      /* 84  A B C   R[A] := R[B].property[C] or R[B][R[C]] */
  "SETPROP",      /* 85  A B C   R[A].property[B] := R[C] or R[A][R[B]] := R[C] */
  "INVOKE",       /* 86  A B C   R[A] := R[B]:method[C](args...) */
  "ITER_INIT",    /* 87  A B     R[A] := iter_init(R[B]) */
  "ITER_NEXT",    /* 88  A B C   R[C] := iter_next(R[A], R[B]) */
  "LOADBUILTIN",  /* 89  A B     R[A] := builtin_func[B] */
  "CALLBUILTIN",  /* 90  A B C   call builtin_func[A] with B args, C results */
  "SUBI",         /* 91  A B sC  R[A] := R[B] - sC */
  "MULI",         /* 92  A B sC  R[A] := R[B] * sC */
  "DIVI",         /* 93  A B sC  R[A] := R[B] / sC */
  NULL
};

/* 操作码模式数组 - 与 Lua 5.4 完全兼容 */
static const aql_byte aql_opmode[NUM_OPCODES] = {
  /* === Lua 5.4 Compatible Opcodes (0-82) === */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_MOVE */
  aqlOpMode(0, 0, 0, 0, 1, iAsBx),   /* OP_LOADI */
  aqlOpMode(0, 0, 0, 0, 1, iAsBx),   /* OP_LOADF */
  aqlOpMode(0, 0, 0, 0, 1, iABx),    /* OP_LOADK */
  aqlOpMode(0, 0, 0, 0, 1, iABx),    /* OP_LOADKX */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_LOADFALSE */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_LFALSESKIP */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_LOADTRUE */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_LOADNIL */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_GETUPVAL */
  aqlOpMode(0, 0, 0, 0, 0, iABC),    /* OP_SETUPVAL */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_GETTABUP */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_GETTABLE */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_GETI */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_GETFIELD */
  aqlOpMode(0, 0, 0, 0, 0, iABC),    /* OP_SETTABUP */
  aqlOpMode(0, 0, 0, 0, 0, iABC),    /* OP_SETTABLE */
  aqlOpMode(0, 0, 0, 0, 0, iABC),    /* OP_SETI */
  aqlOpMode(0, 0, 0, 0, 0, iABC),    /* OP_SETFIELD */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_NEWTABLE */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_SELF */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_ADDI */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_ADDK */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_SUBK */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_MULK */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_MODK */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_POWK */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_DIVK */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_IDIVK */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_BANDK */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_BORK */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_BXORK */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_SHRI */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_SHLI */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_ADD */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_SUB */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_MUL */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_MOD */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_POW */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_DIV */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_IDIV */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_BAND */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_BOR */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_BXOR */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_SHL */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_SHR */
  aqlOpMode(1, 0, 0, 0, 0, iABC),    /* OP_MMBIN */
  aqlOpMode(1, 0, 0, 0, 0, iABC),    /* OP_MMBINI */
  aqlOpMode(1, 0, 0, 0, 0, iABC),    /* OP_MMBINK */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_UNM */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_BNOT */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_NOT */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_LEN */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_CONCAT */
  aqlOpMode(0, 0, 0, 0, 0, iABC),    /* OP_CLOSE */
  aqlOpMode(0, 0, 0, 0, 0, iABC),    /* OP_TBC */
  aqlOpMode(0, 0, 0, 0, 0, isJ),     /* OP_JMP */
  aqlOpMode(0, 0, 0, 1, 0, iABC),    /* OP_EQ */
  aqlOpMode(0, 0, 0, 1, 0, iABC),    /* OP_LT */
  aqlOpMode(0, 0, 0, 1, 0, iABC),    /* OP_LE */
  aqlOpMode(0, 0, 0, 1, 0, iABC),    /* OP_EQK */
  aqlOpMode(0, 0, 0, 1, 0, iABC),    /* OP_EQI */
  aqlOpMode(0, 0, 0, 1, 0, iABC),    /* OP_LTI */
  aqlOpMode(0, 0, 0, 1, 0, iABC),    /* OP_LEI */
  aqlOpMode(0, 0, 0, 1, 0, iABC),    /* OP_GTI */
  aqlOpMode(0, 0, 0, 1, 0, iABC),    /* OP_GEI */
  aqlOpMode(0, 0, 0, 1, 0, iABC),    /* OP_TEST */
  aqlOpMode(0, 0, 0, 1, 1, iABC),    /* OP_TESTSET */
  aqlOpMode(0, 1, 1, 0, 1, iABC),    /* OP_CALL */
  aqlOpMode(0, 1, 1, 0, 1, iABC),    /* OP_TAILCALL */
  aqlOpMode(0, 0, 1, 0, 0, iABC),    /* OP_RETURN */
  aqlOpMode(0, 0, 0, 0, 0, iABC),    /* OP_RETURN0 */
  aqlOpMode(0, 0, 0, 0, 0, iABC),    /* OP_RETURN1 */
  aqlOpMode(0, 0, 0, 0, 1, iABx),    /* OP_FORLOOP */
  aqlOpMode(0, 0, 0, 0, 1, iABx),    /* OP_FORPREP */
  aqlOpMode(0, 0, 0, 0, 0, iABx),    /* OP_TFORPREP */
  aqlOpMode(0, 0, 0, 0, 0, iABC),    /* OP_TFORCALL */
  aqlOpMode(0, 0, 0, 0, 1, iABx),    /* OP_TFORLOOP */
  aqlOpMode(0, 0, 1, 0, 0, iABC),    /* OP_SETLIST */
  aqlOpMode(0, 0, 0, 0, 1, iABx),    /* OP_CLOSURE */
  aqlOpMode(0, 1, 0, 0, 1, iABC),    /* OP_VARARG */
  aqlOpMode(0, 0, 1, 0, 1, iABC),    /* OP_VARARGPREP */
  aqlOpMode(0, 0, 0, 0, 0, iAx),     /* OP_EXTRAARG */

  /* === AQL Extensions (83+) === */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_NEWOBJECT */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_GETPROP */
  aqlOpMode(0, 0, 0, 0, 0, iABC),    /* OP_SETPROP */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_INVOKE */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_ITER_INIT */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_ITER_NEXT */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_LOADBUILTIN */
  aqlOpMode(0, 0, 0, 0, 0, iABC),    /* OP_CALLBUILTIN */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_SUBI */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_MULI */
  aqlOpMode(0, 0, 0, 0, 1, iABC),    /* OP_DIVI */
};

#define getOpMode(m)    (cast(enum OpMode, aql_opmode[m] & 7))
#define testAMode(m)    (aql_opmode[m] & (1 << 3))
#define testTMode(m)    (aql_opmode[m] & (1 << 4))
#define testITMode(m)   (aql_opmode[m] & (1 << 5))
#define testOTMode(m)   (aql_opmode[m] & (1 << 6))
#define testMMMode(m)   (aql_opmode[m] & (1 << 7))

#endif /* aopcodes_h */ 