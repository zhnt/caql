#include "src/alex.h"
#include <stdio.h>

int main() {
    printf("Searching for token 292...\n");
    
    // Count tokens starting from TK_AND = 257
    int target = 292;
    int current = 257;
    
    printf("TK_AND = %d (current=%d)\n", TK_AND, current++);
    printf("TK_BREAK = %d (current=%d)\n", TK_BREAK, current++);
    printf("TK_DO = %d (current=%d)\n", TK_DO, current++);
    printf("TK_ELSE = %d (current=%d)\n", TK_ELSE, current++);
    printf("TK_ELSEIF = %d (current=%d)\n", TK_ELSEIF, current++);
    printf("TK_END = %d (current=%d)\n", TK_END, current++);
    printf("TK_FALSE = %d (current=%d)\n", TK_FALSE, current++);
    printf("TK_FOR = %d (current=%d)\n", TK_FOR, current++);
    printf("TK_FUNCTION = %d (current=%d)\n", TK_FUNCTION, current++);
    printf("TK_GOTO = %d (current=%d)\n", TK_GOTO, current++);
    printf("TK_IF = %d (current=%d)\n", TK_IF, current++);
    printf("TK_IN = %d (current=%d)\n", TK_IN, current++);
    printf("TK_LOCAL = %d (current=%d)\n", TK_LOCAL, current++);
    printf("TK_NIL = %d (current=%d)\n", TK_NIL, current++);
    printf("TK_NOT = %d (current=%d)\n", TK_NOT, current++);
    printf("TK_OR = %d (current=%d)\n", TK_OR, current++);
    printf("TK_REPEAT = %d (current=%d)\n", TK_REPEAT, current++);
    printf("TK_RETURN = %d (current=%d)\n", TK_RETURN, current++);
    printf("TK_THEN = %d (current=%d)\n", TK_THEN, current++);
    printf("TK_TRUE = %d (current=%d)\n", TK_TRUE, current++);
    printf("TK_UNTIL = %d (current=%d)\n", TK_UNTIL, current++);
    printf("TK_WHILE = %d (current=%d)\n", TK_WHILE, current++);
    
    // AQL specific keywords
    printf("TK_CLASS = %d (current=%d)\n", TK_CLASS, current++);
    printf("TK_INTERFACE = %d (current=%d)\n", TK_INTERFACE, current++);
    printf("TK_STRUCT = %d (current=%d)\n", TK_STRUCT, current++);
    printf("TK_IMPORT = %d (current=%d)\n", TK_IMPORT, current++);
    printf("TK_AS = %d (current=%d)\n", TK_AS, current++);
    printf("TK_ASYNC = %d (current=%d)\n", TK_ASYNC, current++);
    printf("TK_AWAIT = %d (current=%d)\n", TK_AWAIT, current++);
    printf("TK_YIELD = %d (current=%d)\n", TK_YIELD, current++);
    printf("TK_COROUTINE = %d (current=%d)\n", TK_COROUTINE, current++);
    printf("TK_ARRAY = %d (current=%d)\n", TK_ARRAY, current++);
    printf("TK_SLICE = %d (current=%d)\n", TK_SLICE, current++);
    printf("TK_DICT = %d (current=%d)\n", TK_DICT, current++);
    printf("TK_VECTOR = %d (current=%d)\n", TK_VECTOR, current++);
    printf("TK_INT = %d (current=%d)\n", TK_INT, current++);
    printf("TK_FLOAT = %d (current=%d)\n", TK_FLOAT, current++);
    
    printf("\nTarget token 292 should be around current=%d\n", current);
    
    return 0;
}

