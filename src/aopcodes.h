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

enum OpMode {iABC, iABx, iAsBx, iAx};  /* basic instruction formats */

/*
** size and position of opcode arguments.
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
** grep "ORDER OP" if you change these enums  (ORDER OP)
*/

typedef enum {
/*----------------------------------------------------------------------
name		args	description
------------------------------------------------------------------------*/

/* === 基础指令组 (0-15): 加载存储指令 === */
OP_MOVE,        /* A B     R(A) := R(B)                                    */
OP_LOADI,       /* A sBx   R(A) := sBx                                     */
OP_LOADF,       /* A sBx   R(A) := (aql_Number)sBx                        */
OP_LOADK,       /* A Bx    R(A) := K(Bx)                                  */
OP_LOADKX,      /* A       R(A) := K(extra arg)                           */
OP_LOADFALSE,   /* A       R(A) := false                                  */
OP_LOADTRUE,    /* A       R(A) := true                                   */
OP_LOADNIL,     /* A B     R(A), R(A+1), ..., R(A+B) := nil               */
OP_GETUPVAL,    /* A B     R(A) := UpValue[B]                             */
OP_SETUPVAL,    /* A B     UpValue[B] := R(A)                             */
OP_GETTABUP,    /* A B C   R(A) := UpValue[B][RK(C)]                      */
OP_SETTABUP,    /* A B C   UpValue[A][RK(B)] := RK(C)                     */
OP_CLOSE,       /* A       close all upvalues >= R(A)                     */
OP_TBC,         /* A       mark variable A "to be closed"                 */
OP_CONCAT,      /* A B     R(A) := R(A).. ... ..R(A+B-1)                 */
OP_EXTRAARG,    /*   Ax    extra (larger) argument for previous opcode    */

/* === 算术运算组 (16-31): 算术指令 (含K/I优化) === */
OP_ADD,         /* A B C   R(A) := R(B) + R(C)                            */
OP_ADDK,        /* A B C   R(A) := R(B) + K[C]                            */
OP_ADDI,        /* A B sC  R(A) := R(B) + sC                              */
OP_SUB,         /* A B C   R(A) := R(B) - R(C)                            */
OP_SUBK,        /* A B C   R(A) := R(B) - K[C]                            */
OP_SUBI,        /* A B sC  R(A) := R(B) - sC                              */
OP_MUL,         /* A B C   R(A) := R(B) * R(C)                            */
OP_MULK,        /* A B C   R(A) := R(B) * K[C]                            */
OP_MULI,        /* A B sC  R(A) := R(B) * sC                              */
OP_DIV,         /* A B C   R(A) := R(B) / R(C)                            */
OP_DIVK,        /* A B C   R(A) := R(B) / K[C]                            */
OP_DIVI,        /* A B sC  R(A) := R(B) / sC                              */
OP_MOD,         /* A B C   R(A) := R(B) % R(C)                            */
OP_POW,         /* A B C   R(A) := R(B) ^ R(C)                            */
OP_UNM,         /* A B     R(A) := -R(B)                                  */
OP_LEN,         /* A B     R(A) := length of R(B)                         */

/* === 位运算组 (32-39): 位运算指令 === */
OP_BAND,        /* A B C   R(A) := R(B) & R(C)                            */
OP_BOR,         /* A B C   R(A) := R(B) | R(C)                            */
OP_BXOR,        /* A B C   R(A) := R(B) ~ R(C)                            */
OP_SHL,         /* A B C   R(A) := R(B) << R(C)                           */
OP_SHR,         /* A B C   R(A) := R(B) >> R(C)                           */
OP_BNOT,        /* A B     R(A) := ~R(B)                                  */
OP_NOT,         /* A B     R(A) := not R(B)                               */
OP_SHRI,        /* A B sC  R(A) := R(B) >> sC                             */

/* === 比较控制组 (40-47): 比较和跳转指令 === */
OP_JMP,         /* sJ      pc += sJ                                       */
OP_EQ,          /* A B k   if ((R(B) == R(C)) ~= k) then pc++             */
OP_LT,          /* A B k   if ((R(B) <  R(C)) ~= k) then pc++             */
OP_LE,          /* A B k   if ((R(B) <= R(C)) ~= k) then pc++             */
OP_TEST,        /* A k     if (not R(A) == k) then pc++                   */
OP_TESTSET,     /* A B k   if (not R(B) == k) then pc++ else R(A) := R(B) */
OP_EQI,         /* A sB k  if ((R(A) == sB) ~= k) then pc++               */
OP_LTI,         /* A sB k  if ((R(A) < sB) ~= k) then pc++                */

/* === 函数调用组 (48-55): 函数和循环指令 === */
OP_CALL,        /* A B C   R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1)) */
OP_TAILCALL,    /* A B     return R(A)(R(A+1), ... ,R(A+B-1))             */
OP_RET,         /* A B     return R(A), ... ,R(A+B-2)                     */
OP_RET_VOID,    /* A       return                                         */
OP_RET_ONE,     /* A       return R(A)                                    */
OP_FORLOOP,     /* A sBx   update counters; if loop continues then pc-=sBx */
OP_FORPREP,     /* A sBx   check values and prepare counters; if not to run then pc+=sBx+1 */
OP_CLOSURE,     /* A Bx    R(A) := closure(KPROTO[Bx])                    */

/* === AQL容器组 (56-59): AQL专用容器操作 === */
OP_NEWOBJECT,   /* A B C   R(A) := new_object(type[B], size[C])           */
OP_GETPROP,     /* A B C   R(A) := R(B).property[C] or R(B)[R(C)]        */
OP_SETPROP,     /* A B C   R(A).property[B] := R(C) or R(A)[R(B)] := R(C) */
OP_INVOKE,      /* A B C   R(A) := R(B):method[C](args...)               */

/* === AQL扩展组 (60-65): AQL特有功能 === */
OP_YIELD,       /* A B     yield R(A), R(A+1), ..., R(A+B-1)             */
OP_RESUME,      /* A B C   R(A), ..., R(A+C-2) := resume(R(B))           */
OP_BUILTIN,     /* A B C   R(A) := builtin_func[B](R(C), R(D))           */
OP_VARARG,      /* A C     R(A), R(A+1), ..., R(A+C-2) = vararg          */
OP_ITER_INIT,   /* A B     R(A) := iter_init(R(B))                       */
OP_ITER_NEXT    /* A B C   R(C) := iter_next(R(A), R(B))                 */

} OpCode;

#define NUM_OPCODES	((int)(OP_ITER_NEXT) + 1)

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

AQL_API const aql_byte aql_opmode[NUM_OPCODES];

#define getOpMode(m)    (cast(enum OpMode, aql_opmode[m] & 7))
#define testAMode(m)    (aql_opmode[m] & (1 << 3))
#define testTMode(m)    (aql_opmode[m] & (1 << 4))
#define testITMode(m)   (aql_opmode[m] & (1 << 5))
#define testOTMode(m)   (aql_opmode[m] & (1 << 6))
#define testMMMode(m)   (aql_opmode[m] & (1 << 7))

/* Note: cast macros are defined in alimits.h, but we need local versions */
#undef cast_byte
#undef cast_uint
#define cast_byte(i)	cast(aql_byte, (i))
#define cast_uint(i)	cast(aql_Unsigned, (i))

AQL_API const char *const aql_opnames[NUM_OPCODES+1];  /* opcode names */

/*
** Macros to create opmode values
** Parameters: test, seta, b_mode, c_mode, extra_mode
*/
#define aqlOpModeABC(test,seta,b,c,extra) \
  (cast_byte((cast_byte(iABC) << 0) | (cast_byte(seta) << 3) | \
             (cast_byte(test) << 4) | (cast_byte(0) << 5) | \
             (cast_byte(0) << 6) | (cast_byte(0) << 7)))

#define aqlOpModeABx(test,seta,b,c) \
  (cast_byte((cast_byte(iABx) << 0) | (cast_byte(seta) << 3) | \
             (cast_byte(test) << 4) | (cast_byte(0) << 5) | \
             (cast_byte(0) << 6) | (cast_byte(0) << 7)))

#define aqlOpModeAsBx(test,seta,b,c) \
  (cast_byte((cast_byte(iAsBx) << 0) | (cast_byte(seta) << 3) | \
             (cast_byte(test) << 4) | (cast_byte(0) << 5) | \
             (cast_byte(0) << 6) | (cast_byte(0) << 7)))

#define aqlOpModeAx(test,seta,ax,unused) \
  (cast_byte((cast_byte(iAx) << 0) | (cast_byte(seta) << 3) | \
             (cast_byte(test) << 4) | (cast_byte(0) << 5) | \
             (cast_byte(0) << 6) | (cast_byte(0) << 7)))

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

#endif /* aopcodes_h */ 