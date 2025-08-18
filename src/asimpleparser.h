/*
** $Id: asimpleparser.h $
** Simple Expression Parser for AQL MVP
** See Copyright Notice in aql.h
*/

#ifndef asimpleparser_h
#define asimpleparser_h

#include "aql.h"

/*
** Token types for simple expressions
*/
typedef enum {
    TOKEN_EOF = 0,
    TOKEN_NUMBER,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_MODULO,
    TOKEN_POWER,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_XOR,
    TOKEN_LSHIFT,
    TOKEN_RSHIFT,
    TOKEN_NOT,
    TOKEN_ERROR
} TokenType;

/*
** Token structure
*/
typedef struct {
    TokenType type;
    union {
        aql_Number number;
        char error_msg[64];
    } value;
} Token;

/*
** Simple lexer state
*/
typedef struct {
    const char *input;
    const char *current;
    Token current_token;
} Lexer;

/*
** Simple parser functions
*/
AQL_API void aqlP_init_lexer(Lexer *lexer, const char *input);
AQL_API Token aqlP_next_token(Lexer *lexer);
AQL_API int aqlP_parse_expression(aql_State *L, const char *expr);
AQL_API int aqlP_execute_file(aql_State *L, const char *filename);
AQL_API void aqlP_repl(aql_State *L);

#endif /* asimpleparser_h */