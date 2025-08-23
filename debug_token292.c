#include "src/alex.h"
#include <stdio.h>

int main() {
    printf("Token 292 = %d (0x%x)\n", 292, 292);
    printf("'?' = %d (0x%x)\n", (int)'?', (int)'?');
    printf("TK_QUESTION = %d (0x%x)\n", TK_QUESTION, TK_QUESTION);
    
    // Find what token 292 represents
    if (292 >= FIRST_RESERVED) {
        printf("Token 292 is a reserved token\n");
    } else {
        printf("Token 292 might be a character: '%c'\n", (char)292);
    }
    
    return 0;
}

