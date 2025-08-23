#include "src/alex.h"
#include <stdio.h>

int main() {
    printf("FIRST_RESERVED = %d\n", FIRST_RESERVED);
    printf("Token 292 offset = %d\n", 292 - FIRST_RESERVED);
    
    // Count which token this is in the enum
    printf("TK_AND = %d\n", TK_AND);
    printf("TK_BREAK = %d\n", TK_BREAK);
    printf("TK_DO = %d\n", TK_DO);
    printf("TK_ELSE = %d\n", TK_ELSE);
    printf("TK_ELSEIF = %d\n", TK_ELSEIF);
    printf("TK_END = %d\n", TK_END);
    printf("TK_FALSE = %d\n", TK_FALSE);
    printf("TK_FOR = %d\n", TK_FOR);
    printf("TK_FUNCTION = %d\n", TK_FUNCTION);
    printf("TK_GOTO = %d\n", TK_GOTO);
    printf("TK_IF = %d\n", TK_IF);
    printf("TK_IN = %d\n", TK_IN);
    printf("TK_LOCAL = %d\n", TK_LOCAL);
    printf("TK_NIL = %d\n", TK_NIL);
    printf("TK_NOT = %d\n", TK_NOT);
    printf("TK_OR = %d\n", TK_OR);
    
    return 0;
}
