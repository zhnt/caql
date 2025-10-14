#include <stdio.h>
#include "src/aopcodes.h"

int main() {
    Instruction loadi_instr = 0x81F30081;
    printf("LOADI 指令编码: 0x%08X\n", loadi_instr);
    printf("GET_OPCODE: %d\n", GET_OPCODE(loadi_instr));
    printf("GETARG_A: %d\n", GETARG_A(loadi_instr));
    printf("GETARG_sBx: %d\n", GETARG_sBx(loadi_instr));
    return 0;
}
