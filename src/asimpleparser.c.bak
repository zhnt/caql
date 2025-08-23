/*
** $Id: asimpleparser.c $
** Simple Expression Parser for AQL MVP
** See Copyright Notice in aql.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "asimpleparser.h"
#include "aql.h"

/*
** Initialize lexer
*/
AQL_API void aqlP_init_lexer(Lexer *lexer, const char *input) {
    lexer->input = input;
    lexer->current = input;
    lexer->current_token.type = TOKEN_EOF;
}

/*
** Skip whitespace
*/
static void skip_whitespace(Lexer *lexer) {
    while (isspace(*lexer->current)) {
        lexer->current++;
    }
}

/*
** Parse number
*/
static Token parse_number(Lexer *lexer) {
    Token token;
    char *endptr;
    
    token.type = TOKEN_NUMBER;
    token.value.number = strtod(lexer->current, &endptr);
    
    if (endptr == lexer->current) {
        token.type = TOKEN_ERROR;
        strcpy(token.value.error_msg, "Invalid number");
    } else {
        lexer->current = endptr;
    }
    
    return token;
}

/*
** Get next token
*/
AQL_API Token aqlP_next_token(Lexer *lexer) {
    Token token;
    
    skip_whitespace(lexer);
    
    if (*lexer->current == '\0') {
        token.type = TOKEN_EOF;
        return token;
    }
    
    /* Numbers */
    if (isdigit(*lexer->current) || *lexer->current == '.') {
        return parse_number(lexer);
    }
    
    /* Single character tokens */
    switch (*lexer->current) {
        case '+':
            token.type = TOKEN_PLUS;
            lexer->current++;
            break;
        case '-':
            token.type = TOKEN_MINUS;
            lexer->current++;
            break;
        case '*':
            if (lexer->current[1] == '*') {
                token.type = TOKEN_POWER;
                lexer->current += 2;
            } else {
                token.type = TOKEN_MULTIPLY;
                lexer->current++;
            }
            break;
        case '/':
            token.type = TOKEN_DIVIDE;
            lexer->current++;
            break;
        case '%':
            token.type = TOKEN_MODULO;
            lexer->current++;
            break;
        case '(':
            token.type = TOKEN_LPAREN;
            lexer->current++;
            break;
        case ')':
            token.type = TOKEN_RPAREN;
            lexer->current++;
            break;
        case '&':
            token.type = TOKEN_AND;
            lexer->current++;
            break;
        case '|':
            token.type = TOKEN_OR;
            lexer->current++;
            break;
        case '^':
            token.type = TOKEN_XOR;
            lexer->current++;
            break;
        case '~':
            token.type = TOKEN_NOT;
            lexer->current++;
            break;
        case '<':
            if (lexer->current[1] == '<') {
                token.type = TOKEN_LSHIFT;
                lexer->current += 2;
            } else {
                token.type = TOKEN_ERROR;
                strcpy(token.value.error_msg, "Unexpected character '<'");
            }
            break;
        case '>':
            if (lexer->current[1] == '>') {
                token.type = TOKEN_RSHIFT;
                lexer->current += 2;
            } else {
                token.type = TOKEN_ERROR;
                strcpy(token.value.error_msg, "Unexpected character '>'");
            }
            break;
        default:
            token.type = TOKEN_ERROR;
            snprintf(token.value.error_msg, sizeof(token.value.error_msg), 
                    "Unexpected character '%c'", *lexer->current);
            lexer->current++;
            break;
    }
    
    return token;
}

/*
** Forward declarations for recursive descent parser
*/
static int parse_expression(aql_State *L, Lexer *lexer);
static int parse_term(aql_State *L, Lexer *lexer);
static int parse_factor(aql_State *L, Lexer *lexer);
static int parse_unary(aql_State *L, Lexer *lexer);
static int parse_primary(aql_State *L, Lexer *lexer);

/*
** Parse primary expression (numbers, parentheses)
*/
static int parse_primary(aql_State *L, Lexer *lexer) {
    Token token = aqlP_next_token(lexer);
    
    switch (token.type) {
        case TOKEN_NUMBER:
            if (token.value.number == floor(token.value.number)) {
                aql_pushinteger(L, (aql_Integer)token.value.number);
            } else {
                aql_pushnumber(L, token.value.number);
            }
            return 1;
            
        case TOKEN_LPAREN: {
            int result = parse_expression(L, lexer);
            if (!result) return 0;
            
            token = aqlP_next_token(lexer);
            if (token.type != TOKEN_RPAREN) {
                fprintf(stderr, "Error: Expected ')'\n");
                return 0;
            }
            return 1;
        }
        
        case TOKEN_ERROR:
            fprintf(stderr, "Lexer error: %s\n", token.value.error_msg);
            return 0;
            
        default:
            fprintf(stderr, "Error: Unexpected token in primary expression\n");
            return 0;
    }
}

/*
** Parse unary expression (~, -)
*/
static int parse_unary(aql_State *L, Lexer *lexer) {
    /* Peek at next token */
    const char *saved_pos = lexer->current;
    Token token = aqlP_next_token(lexer);
    
    if (token.type == TOKEN_MINUS) {
        if (!parse_unary(L, lexer)) return 0;
        aql_arith(L, AQL_OPUNM);
        return 1;
    } else if (token.type == TOKEN_NOT) {
        if (!parse_unary(L, lexer)) return 0;
        aql_arith(L, AQL_OPBNOT);
        return 1;
    } else {
        /* Not a unary operator, restore position and parse primary */
        lexer->current = saved_pos;
        return parse_primary(L, lexer);
    }
}

/*
** Parse factor (**, right associative)
*/
static int parse_factor(aql_State *L, Lexer *lexer) {
    if (!parse_unary(L, lexer)) return 0;
    
    const char *saved_pos = lexer->current;
    Token token = aqlP_next_token(lexer);
    
    if (token.type == TOKEN_POWER) {
        if (!parse_factor(L, lexer)) return 0;  /* Right associative */
        aql_arith(L, AQL_OPPOW);
        return 1;
    } else {
        lexer->current = saved_pos;
        return 1;
    }
}

/*
** Parse term (*, /, %, <<, >>, &)
*/
static int parse_term(aql_State *L, Lexer *lexer) {
    if (!parse_factor(L, lexer)) return 0;
    
    while (1) {
        const char *saved_pos = lexer->current;
        Token token = aqlP_next_token(lexer);
        
        int op = -1;
        switch (token.type) {
            case TOKEN_MULTIPLY: op = AQL_OPMUL; break;
            case TOKEN_DIVIDE: op = AQL_OPDIV; break;
            case TOKEN_MODULO: op = AQL_OPMOD; break;
            case TOKEN_LSHIFT: op = AQL_OPSHL; break;
            case TOKEN_RSHIFT: op = AQL_OPSHR; break;
            case TOKEN_AND: op = AQL_OPBAND; break;
            default:
                lexer->current = saved_pos;
                return 1;
        }
        
        if (!parse_factor(L, lexer)) return 0;
        aql_arith(L, op);
    }
}

/*
** Parse expression (+, -, |, ^)
*/
static int parse_expression(aql_State *L, Lexer *lexer) {
    if (!parse_term(L, lexer)) return 0;
    
    while (1) {
        const char *saved_pos = lexer->current;
        Token token = aqlP_next_token(lexer);
        
        int op = -1;
        switch (token.type) {
            case TOKEN_PLUS: op = AQL_OPADD; break;
            case TOKEN_MINUS: op = AQL_OPSUB; break;
            case TOKEN_OR: op = AQL_OPBOR; break;
            case TOKEN_XOR: op = AQL_OPBXOR; break;
            default:
                lexer->current = saved_pos;
                return 1;
        }
        
        if (!parse_term(L, lexer)) return 0;
        aql_arith(L, op);
    }
}

/*
** Parse and evaluate expression
*/
AQL_API int aqlP_parse_expression(aql_State *L, const char *expr) {
    Lexer lexer;
    aqlP_init_lexer(&lexer, expr);
    
    int result = parse_expression(L, &lexer);
    if (!result) return 0;
    
    /* Check for remaining tokens */
    Token token = aqlP_next_token(&lexer);
    if (token.type != TOKEN_EOF) {
        fprintf(stderr, "Error: Unexpected tokens after expression\n");
        return 0;
    }
    
    return 1;
}

/*
** Execute expressions from file
*/
AQL_API int aqlP_execute_file(aql_State *L, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return 0;
    }
    
    char line[1024];
    int line_number = 0;
    int success = 1;
    
    printf("Executing file: %s\n", filename);
    
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        
        /* Remove newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        /* Skip empty lines and comments */
        char *trimmed = line;
        while (isspace(*trimmed)) trimmed++;
        if (*trimmed == '\0' || *trimmed == '#') continue;
        
        printf("Line %d: %s\n", line_number, trimmed);
        
        if (aqlP_parse_expression(L, trimmed)) {
            if (aql_gettop(L) > 0) {
                if (aql_isinteger(L, -1)) {
                    printf("  Result: %lld\n", aql_tointeger(L, -1));
                } else if (aql_isnumber(L, -1)) {
                    printf("  Result: %.6g\n", (double)aql_tonumber(L, -1));
                } else {
                    printf("  Result: (unknown type)\n");
                }
                aql_pop(L, 1);
            }
        } else {
            fprintf(stderr, "  Error parsing line %d\n", line_number);
            success = 0;
        }
    }
    
    fclose(file);
    return success;
}

/*
** Interactive REPL
*/
AQL_API void aqlP_repl(aql_State *L) {
    char line[1024];
    
    printf("AQL Expression Calculator (MVP)\n");
    printf("Enter expressions to evaluate, or 'quit' to exit.\n");
    printf("Examples: 2 + 3 * 4, (5 + 3) ** 2, 15 & 7, 5 << 2\n\n");
    
    while (1) {
        printf("aql> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\nGoodbye!\n");
            break;
        }
        
        /* Remove newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }
        
        /* Skip empty lines */
        char *trimmed = line;
        while (isspace(*trimmed)) trimmed++;
        if (*trimmed == '\0') continue;
        
        /* Check for quit command */
        if (strcmp(trimmed, "quit") == 0 || strcmp(trimmed, "exit") == 0) {
            printf("Goodbye!\n");
            break;
        }
        
        /* Parse and evaluate expression */
        if (aqlP_parse_expression(L, trimmed)) {
            if (aql_gettop(L) > 0) {
                if (aql_isinteger(L, -1)) {
                    printf("%lld\n", aql_tointeger(L, -1));
                } else if (aql_isnumber(L, -1)) {
                    printf("%.6g\n", (double)aql_tonumber(L, -1));
                } else {
                    printf("(unknown type)\n");
                }
                aql_pop(L, 1);
            }
        } else {
            printf("Error: Invalid expression\n");
        }
        
        printf("\n");
    }
}