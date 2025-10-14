#include "aopcodes.h"
#include <stdio.h>

int main() {
    printf("=== AQL Opcodes ===\n");
    printf("OP_MOVE = %d\n", OP_MOVE);
    printf("OP_LOADI = %d\n", OP_LOADI);
    printf("OP_LOADF = %d\n", OP_LOADF);
    printf("OP_LOADK = %d\n", OP_LOADK);
    printf("OP_LOADKX = %d\n", OP_LOADKX);
    printf("OP_LOADFALSE = %d\n", OP_LOADFALSE);
    printf("OP_LFALSESKIP = %d\n", OP_LFALSESKIP);
    printf("OP_LOADTRUE = %d\n", OP_LOADTRUE);
    printf("OP_LOADNIL = %d\n", OP_LOADNIL);
    printf("OP_GETUPVAL = %d\n", OP_GETUPVAL);
    printf("OP_SETUPVAL = %d\n", OP_SETUPVAL);
    printf("OP_GETTABUP = %d\n", OP_GETTABUP);
    printf("OP_GETTABLE = %d\n", OP_GETTABLE);
    printf("OP_GETI = %d\n", OP_GETI);
    printf("OP_GETFIELD = %d\n", OP_GETFIELD);
    printf("OP_SETTABUP = %d\n", OP_SETTABUP);
    printf("OP_SETTABLE = %d\n", OP_SETTABLE);
    printf("OP_SETI = %d\n", OP_SETI);
    printf("OP_SETFIELD = %d\n", OP_SETFIELD);
    printf("OP_NEWTABLE = %d\n", OP_NEWTABLE);
    printf("OP_SELF = %d\n", OP_SELF);
    printf("OP_ADD = %d\n", OP_ADD);
    printf("OP_SUB = %d\n", OP_SUB);
    printf("OP_MUL = %d\n", OP_MUL);
    printf("OP_DIV = %d\n", OP_DIV);
    printf("OP_JMP = %d\n", OP_JMP);
    printf("OP_TEST = %d\n", OP_TEST);
    printf("OP_TESTSET = %d\n", OP_TESTSET);
    printf("OP_RETURN1 = %d\n", OP_RETURN1);
    return 0;
}
