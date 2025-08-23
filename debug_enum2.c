#include "src/alex.h"
#include <stdio.h>

int main() {
    printf("Looking for token 292 (offset %d from FIRST_RESERVED):\n", 292 - FIRST_RESERVED);
    
    printf("TK_PLUS = %d\n", TK_PLUS);
    printf("TK_MINUS = %d\n", TK_MINUS);
    printf("TK_MUL = %d\n", TK_MUL);
    printf("TK_DIV = %d\n", TK_DIV);
    printf("TK_MOD = %d\n", TK_MOD);
    printf("TK_POW = %d\n", TK_POW);
    printf("TK_EQ = %d\n", TK_EQ);
    printf("TK_NE = %d\n", TK_NE);
    printf("TK_LT = %d\n", TK_LT);
    printf("TK_GT = %d\n", TK_GT);
    printf("TK_LE = %d\n", TK_LE);
    printf("TK_GE = %d\n", TK_GE);
    
    return 0;
}
