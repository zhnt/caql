/*
** $Id: aopcodes.c $
** Opcodes for AQL virtual machine
** See Copyright Notice in aql.h
*/

#include <stdio.h>
#include "aopcodes.h"

/*
** Opcode names for debugging and disassembly - AQL Instruction Set
*/
const char *const aql_opnames[NUM_OPCODES+1] = {
  /* === 基础指令组 (0-15): 加载存储指令 === */
  "MOVE",         /* A B     R(A) := R(B)                                    */
  "LOADI",        /* A sBx   R(A) := sBx                                     */
  "LOADF",        /* A sBx   R(A) := (aql_Number)sBx                        */
  "LOADK",        /* A Bx    R(A) := K(Bx)                                  */
  "LOADKX",       /* A       R(A) := K(extra arg)                           */
  "LOADFALSE",    /* A       R(A) := false                                  */
  "LOADTRUE",     /* A       R(A) := true                                   */
  "LOADNIL",      /* A B     R(A), R(A+1), ..., R(A+B) := nil               */
  "GETUPVAL",     /* A B     R(A) := UpValue[B]                             */
  "SETUPVAL",     /* A B     UpValue[B] := R(A)                             */
  "GETTABUP",     /* A B C   R(A) := UpValue[B][RK(C)]                      */
  "SETTABUP",     /* A B C   UpValue[A][RK(B)] := RK(C)                     */
  "CLOSE",        /* A       close all upvalues >= R(A)                     */
  "TBC",          /* A       mark variable A "to be closed"                 */
  "CONCAT",       /* A B     R(A) := R(A).. ... ..R(A+B-1)                 */
  "EXTRAARG",     /*   Ax    extra (larger) argument for previous opcode    */

  /* === 算术运算组 (16-31): 算术指令 (含K/I优化) === */
  "ADD",          /* A B C   R(A) := R(B) + R(C)                            */
  "ADDK",         /* A B C   R(A) := R(B) + K[C]                            */
  "ADDI",         /* A B sC  R(A) := R(B) + sC                              */
  "SUB",          /* A B C   R(A) := R(B) - R(C)                            */
  "SUBK",         /* A B C   R(A) := R(B) - K[C]                            */
  "SUBI",         /* A B sC  R(A) := R(B) - sC                              */
  "MUL",          /* A B C   R(A) := R(B) * R(C)                            */
  "MULK",         /* A B C   R(A) := R(B) * K[C]                            */
  "MULI",         /* A B sC  R(A) := R(B) * sC                              */
  "DIV",          /* A B C   R(A) := R(B) / R(C)                            */
  "DIVK",         /* A B C   R(A) := R(B) / K[C]                            */
  "DIVI",         /* A B sC  R(A) := R(B) / sC                              */
  "MOD",          /* A B C   R(A) := R(B) % R(C)                            */
  "POW",          /* A B C   R(A) := R(B) ^ R(C)                            */
  "UNM",          /* A B     R(A) := -R(B)                                  */
  "LEN",          /* A B     R(A) := length of R(B)                         */

  /* === 位运算组 (32-39): 位运算指令 === */
  "BAND",         /* A B C   R(A) := R(B) & R(C)                            */
  "BOR",          /* A B C   R(A) := R(B) | R(C)                            */
  "BXOR",         /* A B C   R(A) := R(B) ~ R(C)                            */
  "SHL",          /* A B C   R(A) := R(B) << R(C)                           */
  "SHR",          /* A B C   R(A) := R(B) >> R(C)                           */
  "BNOT",         /* A B     R(A) := ~R(B)                                  */
  "NOT",          /* A B     R(A) := not R(B)                               */
  "SHRI",         /* A B sC  R(A) := R(B) >> sC                             */

  /* === 比较控制组 (40-47): 比较和跳转指令 === */
  "JMP",          /* sJ      pc += sJ                                       */
  "EQ",           /* A B k   if ((R(B) == R(C)) ~= k) then pc++             */
  "LT",           /* A B k   if ((R(B) <  R(C)) ~= k) then pc++             */
  "LE",           /* A B k   if ((R(B) <= R(C)) ~= k) then pc++             */
  "TEST",         /* A k     if (not R(A) == k) then pc++                   */
  "TESTSET",      /* A B k   if (not R(B) == k) then pc++ else R(A) := R(B) */
  "EQI",          /* A sB k  if ((R(A) == sB) ~= k) then pc++               */
  "LTI",          /* A sB k  if ((R(A) < sB) ~= k) then pc++                */

  /* === 函数调用组 (48-55): 函数和循环指令 === */
  "CALL",         /* A B C   R(A), ... ,R(A+C-2) := R(A)(R(A+1), ... ,R(A+B-1)) */
  "TAILCALL",     /* A B     return R(A)(R(A+1), ... ,R(A+B-1))             */
  "RET",          /* A B     return R(A), ... ,R(A+B-2)                     */
  "RET_VOID",     /* A       return                                         */
  "RET_ONE",      /* A       return R(A)                                    */
  "FORLOOP",      /* A sBx   update counters; if loop continues then pc-=sBx */
  "FORPREP",      /* A sBx   check values and prepare counters; if not to run then pc+=sBx+1 */
  "CLOSURE",      /* A Bx    R(A) := closure(KPROTO[Bx])                    */

  /* === AQL容器组 (56-59): AQL专用容器操作 === */
  "NEWOBJECT",    /* A B C   R(A) := new_object(type[B], size[C])           */
  "GETPROP",      /* A B C   R(A) := R(B).property[C] or R(B)[R(C)]        */
  "SETPROP",      /* A B C   R(A).property[B] := R(C) or R(A)[R(B)] := R(C) */
  "INVOKE",       /* A B C   R(A) := R(B):method[C](args...)               */

  /* === AQL扩展组 (60-63): AQL特有功能 === */
  "YIELD",        /* A B     yield R(A), R(A+1), ..., R(A+B-1)             */
  "RESUME",       /* A B C   R(A), ..., R(A+C-2) := resume(R(B))           */
  "BUILTIN",      /* A B C   R(A) := builtin_func[B](R(C), R(D))           */
  "VARARG",       /* A C     R(A), R(A+1), ..., R(A+C-2) = vararg          */
  NULL
};

/*
** Opcode mode table - defines argument types for each AQL instruction
*/
const aql_byte aql_opmode[NUM_OPCODES] = {
/* === 基础指令组 (0-15) === */
  aqlOpModeABC(0, 1, OpArgR, OpArgR, OpArgN),   /* OP_MOVE */
  aqlOpModeAsBx(0, 1, OpArgK, OpArgN),          /* OP_LOADI */
  aqlOpModeAsBx(0, 1, OpArgK, OpArgN),          /* OP_LOADF */
  aqlOpModeABx(0, 1, OpArgK, OpArgN),           /* OP_LOADK */
  aqlOpModeABC(0, 1, OpArgN, OpArgN, OpArgN),   /* OP_LOADKX */
  aqlOpModeABC(0, 1, OpArgN, OpArgN, OpArgN),   /* OP_LOADFALSE */
  aqlOpModeABC(0, 1, OpArgN, OpArgN, OpArgN),   /* OP_LOADTRUE */
  aqlOpModeABC(0, 1, OpArgU, OpArgN, OpArgN),   /* OP_LOADNIL */
  aqlOpModeABC(0, 1, OpArgU, OpArgN, OpArgN),   /* OP_GETUPVAL */
  aqlOpModeABC(0, 0, OpArgU, OpArgN, OpArgN),   /* OP_SETUPVAL */
  aqlOpModeABC(0, 1, OpArgU, OpArgK, OpArgN),   /* OP_GETTABUP */
  aqlOpModeABC(0, 0, OpArgK, OpArgK, OpArgN),   /* OP_SETTABUP */
  aqlOpModeABC(0, 0, OpArgN, OpArgN, OpArgN),   /* OP_CLOSE */
  aqlOpModeABC(0, 0, OpArgN, OpArgN, OpArgN),   /* OP_TBC */
  aqlOpModeABC(0, 1, OpArgR, OpArgN, OpArgN),   /* OP_CONCAT */
  aqlOpModeAx(0, 0, OpArgU, OpArgN),            /* OP_EXTRAARG */

/* === 算术运算组 (16-31) === */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_ADD */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_ADDK */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_ADDI */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_SUB */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_SUBK */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_SUBI */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_MUL */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_MULK */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_MULI */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_DIV */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_DIVK */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_DIVI */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_MOD */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_POW */
  aqlOpModeABC(0, 1, OpArgR, OpArgN, OpArgN),   /* OP_UNM */
  aqlOpModeABC(0, 1, OpArgR, OpArgN, OpArgN),   /* OP_LEN */

/* === 位运算组 (32-39) === */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_BAND */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_BOR */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_BXOR */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_SHL */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_SHR */
  aqlOpModeABC(0, 1, OpArgR, OpArgN, OpArgN),   /* OP_BNOT */
  aqlOpModeABC(0, 1, OpArgR, OpArgN, OpArgN),   /* OP_NOT */
  aqlOpModeABC(0, 1, OpArgK, OpArgK, OpArgN),   /* OP_SHRI */

/* === 比较控制组 (40-47) === */
  aqlOpModeAsBx(0, 0, OpArgR, OpArgN),          /* OP_JMP */
  aqlOpModeABC(1, 0, OpArgK, OpArgK, OpArgN),   /* OP_EQ */
  aqlOpModeABC(1, 0, OpArgK, OpArgK, OpArgN),   /* OP_LT */
  aqlOpModeABC(1, 0, OpArgK, OpArgK, OpArgN),   /* OP_LE */
  aqlOpModeABC(1, 0, OpArgR, OpArgN, OpArgN),   /* OP_TEST */
  aqlOpModeABC(1, 1, OpArgR, OpArgU, OpArgN),   /* OP_TESTSET */
  aqlOpModeABC(1, 0, OpArgK, OpArgK, OpArgN),   /* OP_EQI */
  aqlOpModeABC(1, 0, OpArgK, OpArgK, OpArgN),   /* OP_LTI */

/* === 函数调用组 (48-55) === */
  aqlOpModeABC(0, 0, OpArgU, OpArgU, OpArgN),   /* OP_CALL */
  aqlOpModeABC(0, 0, OpArgU, OpArgU, OpArgN),   /* OP_TAILCALL */
  aqlOpModeABC(0, 0, OpArgU, OpArgU, OpArgN),   /* OP_RET */
  aqlOpModeABC(0, 0, OpArgU, OpArgU, OpArgN),   /* OP_RET_VOID */
  aqlOpModeABC(0, 0, OpArgU, OpArgU, OpArgN),   /* OP_RET_ONE */
  aqlOpModeAsBx(0, 0, OpArgR, OpArgN),          /* OP_FORLOOP */
  aqlOpModeAsBx(0, 0, OpArgR, OpArgN),          /* OP_FORPREP */
  aqlOpModeABx(0, 1, OpArgU, OpArgN),           /* OP_CLOSURE */

/* === AQL容器组 (56-59) === */
  aqlOpModeABC(0, 1, OpArgU, OpArgU, OpArgN),   /* OP_NEWOBJECT */
  aqlOpModeABC(0, 1, OpArgR, OpArgR, OpArgN),   /* OP_GETPROP */
  aqlOpModeABC(0, 0, OpArgR, OpArgR, OpArgN),   /* OP_SETPROP */
  aqlOpModeABC(0, 1, OpArgR, OpArgK, OpArgN),   /* OP_INVOKE */

/* === AQL扩展组 (60-63) === */
  aqlOpModeABC(0, 0, OpArgU, OpArgU, OpArgN),   /* OP_YIELD */
  aqlOpModeABC(0, 1, OpArgR, OpArgR, OpArgN),   /* OP_RESUME */
  aqlOpModeABC(0, 1, OpArgU, OpArgU, OpArgN),   /* OP_BUILTIN */
  aqlOpModeABC(0, 0, OpArgU, OpArgU, OpArgN),   /* OP_VARARG */
};

/*
** Instruction manipulation utilities
*/
Instruction aqlO_create_ABC(OpCode o, int a, int b, int c) {
  return ((cast(Instruction, o) << POS_OP) |
          (cast(Instruction, a) << POS_A) |
          (cast(Instruction, b) << POS_B) |
          (cast(Instruction, c) << POS_C));
}

Instruction aqlO_create_ABx(OpCode o, int a, unsigned int bx) {
  return ((cast(Instruction, o) << POS_OP) |
          (cast(Instruction, a) << POS_A) |
          (cast(Instruction, bx) << POS_Bx));
}

Instruction aqlO_create_AsBx(OpCode o, int a, int sbx) {
  return ((cast(Instruction, o) << POS_OP) |
          (cast(Instruction, a) << POS_A) |
          (cast(Instruction, sbx + MAXARG_sBx) << POS_Bx));
}

Instruction aqlO_create_Ax(OpCode o, unsigned int ax) {
  return ((cast(Instruction, o) << POS_OP) |
          (cast(Instruction, ax) << POS_Ax));
}

/*
** Disassembly utilities for debugging
*/
void aqlO_disasm_instruction(Instruction i, int pc) {
  OpCode op = GET_OPCODE(i);
  int a = GETARG_A(i);
  
  printf("%4d\t%-10s\t", pc, aql_opnames[op]);
  
  switch (GET_OPMODE(op)) {
    case iABC: {
      int b = GETARG_B(i);
      int c = GETARG_C(i);
      printf("%d", a);
      if (getBMode(op) != OpArgN) printf(" %d", ISK(b) ? -1-INDEXK(b) : b);
      if (getCMode(op) != OpArgN) printf(" %d", ISK(c) ? -1-INDEXK(c) : c);
      break;
    }
    case iABx: {
      unsigned int bx = GETARG_Bx(i);
      printf("%d %d", a, bx);
      break;
    }
    case iAsBx: {
      int sbx = GETARG_sBx(i);
      printf("%d %d", a, sbx);
      break;
    }
    case iAx: {
      unsigned int ax = GETARG_Ax(i);
      printf("%d", ax);
      break;
    }
  }
  printf("\n");
} 