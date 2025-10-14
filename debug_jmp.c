#include <stdio.h>
#include "src/aopcodes.h"

int main() {
    Instruction jmp_instr = 0x800000B8;
    printf("JMP 指令编码: 0x%08X\n", jmp_instr);
    printf("GETARG_sJ: %d\n", GETARG_sJ(jmp_instr));
    printf("OFFSET_sJ: %d\n", OFFSET_sJ);
    printf("SIZE_sJ: %d\n", SIZE_sJ);
    printf("POS_sJ: %d\n", POS_sJ);
    return 0;
}
