/*
** $Id: alex.h $
** Lexical Analyzer for AQL
** See Copyright Notice in aql.h
*/

#ifndef alex_h
#define alex_h

#include "aobject.h"
#include "azio.h"
#ifndef ALEX_SKIP_ASTATE
#include "astate.h"
#endif

/* Forward declarations for parser types */
typedef struct FuncState FuncState;
typedef struct Dyndata Dyndata;

/*
** TOKEN TYPES
*/
#define FIRST_RESERVED	257

/* Terminal symbols denoted by reserved words */
enum RESERVED {
  /* control structures */
  TK_AND = FIRST_RESERVED, TK_BREAK, TK_CONTINUE,
  TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR,
  TK_FUNCTION, TK_GOTO, TK_IF, TK_IN, TK_LOCAL, TK_NIL, TK_NOT,
  TK_OR, TK_REPEAT, TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE,
  
  /* AQL specific keywords */
  TK_CLASS, TK_INTERFACE, TK_STRUCT, TK_IMPORT, TK_AS,
  TK_ASYNC, TK_AWAIT, TK_YIELD, TK_COROUTINE,
  TK_ARRAY, TK_SLICE, TK_DICT, TK_VECTOR,
  TK_INT, TK_FLOAT, TK_STRING, TK_BOOL,
  TK_CONST, TK_VAR, TK_LET, TK_TYPE, TK_GENERIC,
  TK_ELIF,
  
  /* AI-specific keywords (Phase 2) */
  TK_AI, TK_INTENT, TK_WORKFLOW, TK_PARALLEL,
  
  /* Basic arithmetic operators */
  TK_PLUS, TK_MINUS, TK_MUL, TK_DIV, TK_MOD, TK_POW,
  TK_DIV_KW,  /* div - integer division keyword */
  
  /* Comparison operators */
  TK_EQ, TK_NE, TK_LT, TK_GT, TK_LE, TK_GE,
  
  /* Logical operators */
  TK_LAND, TK_LOR, TK_LNOT,  /* && || ! */
  
  /* Bitwise operators */
  TK_BAND, TK_BOR, TK_BXOR, TK_BNOT,  /* & | ^ ~ */
  TK_SHL, TK_SHR,  /* << >> */
  
  /* Punctuation */
  TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE, TK_LBRACKET, TK_RBRACKET,
  TK_SEMICOLON, TK_COMMA, TK_DOT, TK_COLON,
  
  /* Assignment operators */
  TK_ASSIGN, TK_PLUSEQ, TK_MINUSEQ, TK_MULEQ, TK_DIVEQ,
  
  /* other terminal symbols */
  TK_IDIV, TK_CONCAT, TK_DOTS, TK_DBCOLON, TK_EOS,
  TK_FLT, TK_INT_LITERAL, TK_NAME, TK_STRING_INTERP,
  
  /* AQL specific operators */
  TK_ARROW, TK_PIPE, TK_QUESTION, TK_NULLCOAL, TK_TERNARY,
  TK_ANDEQ, TK_OREQ, TK_XOREQ,
  
  /* Type annotations */
  TK_TYPEANNOT,
  
  /* Comments and whitespace (for IDE support) */
  TK_COMMENT, TK_LINECOMMENT, TK_WHITESPACE
};

/* Number of reserved words */
#define NUM_RESERVED	(cast_int(TK_WHILE-FIRST_RESERVED+1))

/*
** Lexical state
*/
typedef union {
  aql_Number r;
  aql_Integer i;
  TString *ts;
} SemInfo;  /* semantics information */

typedef struct Token {
  int token;
  SemInfo seminfo;
} Token;

typedef struct LexState {
  int current;  /* current character (charint) */
  int linenumber;  /* input line counter */
  int lastline;  /* line of last token 'consumed' */
  int column;  /* current column position */
  Token t;  /* current token */
  Token lookahead;  /* look ahead token */
  struct FuncState *fs;  /* current function (parser) */
  struct aql_State *L;
  ZIO *z;  /* input stream */
  Mbuffer *buff;  /* buffer for tokens */
  Table *h;  /* to avoid collection/reuse strings */
  struct Dyndata *dyd;  /* dynamic structures used by the parser */
  TString *source;  /* current source name */
  TString *envn;  /* environment variable name */
} LexState;

/*
** Lexical analyzer functions
*/
AQL_API void aqlX_init(aql_State *L);
AQL_API void aqlX_setinput(aql_State *L, LexState *ls, ZIO *z,
                           TString *source, int firstchar);
AQL_API TString *aqlX_newstring(LexState *ls, const char *str, size_t l);
AQL_API void aqlX_next(LexState *ls);
AQL_API int aqlX_lookahead(LexState *ls);
AQL_API l_noret aqlX_syntaxerror(LexState *ls, const char *s);
AQL_API const char *aqlX_token2str(LexState *ls, int token);

/*
** Keywords and identifiers
*/
AQL_API const char *const aqlX_tokens[];
AQL_API const TString *aqlX_keywords[NUM_RESERVED];
AQL_API void aqlX_init_keywords(aql_State *L);

/*
** String and number parsing
*/
AQL_API int aqlX_check_next1(LexState *ls, int c);
AQL_API int aqlX_check_next2(LexState *ls, const char *set);
AQL_API void aqlX_read_numeral(LexState *ls, SemInfo *seminfo);
AQL_API int aqlX_read_string(LexState *ls, int del, SemInfo *seminfo);
AQL_API void aqlX_read_long_string(LexState *ls, SemInfo *seminfo, int sep);

/*
** AQL-specific lexical features
*/

/* Type annotation parsing */
AQL_API int aqlX_read_type_annotation(LexState *ls);

/* Generic type parsing */
AQL_API int aqlX_parse_generic_params(LexState *ls);

/* String interpolation support */
AQL_API int aqlX_read_interpolated_string(LexState *ls, SemInfo *seminfo);

/* Multi-line string literals */
AQL_API void aqlX_read_raw_string(LexState *ls, SemInfo *seminfo);

/* Operator precedence for AQL extensions */
typedef struct {
  aql_byte left;   /* left priority for each binary operator */
  aql_byte right;  /* right priority */
} OpPriority;

AQL_API const OpPriority aqlX_priority[];

/*
** Error handling and diagnostics
*/
AQL_API void aqlX_checklimit(FuncState *fs, int val, int limit, const char *what);
AQL_API l_noret aqlX_lexerror(LexState *ls, const char *msg, int token);

/*
** Position tracking for better error messages
*/
typedef struct {
  int line;
  int column;
  size_t offset;
} SourcePos;

AQL_API void aqlX_savepos(LexState *ls, SourcePos *pos);
AQL_API void aqlX_setpos(LexState *ls, const SourcePos *pos);
AQL_API const char *aqlX_getline(LexState *ls, int line);

/*
** Unicode and UTF-8 support
*/
AQL_API int aqlX_utf8_decode(const char *s, int *cp);
AQL_API int aqlX_utf8_encode(int cp, char *s);
AQL_API int aqlX_utf8_validate(const char *s, size_t len);
AQL_API int aqlX_is_identifier_start(int cp);
AQL_API int aqlX_is_identifier_cont(int cp);

/*
** Fast character classification
*/
#define aqlX_isalpha(c)     (isalpha(c) || (c) == '_')
#define aqlX_isalnum(c)     (isalnum(c) || (c) == '_')
#define aqlX_isdigit(c)     isdigit(c)
#define aqlX_isxdigit(c)    isxdigit(c)
#define aqlX_isspace(c)     isspace(c)

/*
** Lexer state management
*/
AQL_API void aqlX_pushlex(aql_State *L, LexState *ls);
AQL_API void aqlX_poplex(aql_State *L);

/*
** Debug support
*/
#if defined(AQL_DEBUG_LEX)
AQL_API void aqlX_debug_token(LexState *ls, const char *msg);
AQL_API void aqlX_dump_tokens(LexState *ls);
#else
#define aqlX_debug_token(ls, msg)  ((void)0)
#define aqlX_dump_tokens(ls)       ((void)0)
#endif

/*
** Special constants for lexer
*/
#define LEX_EOF         (-1)
#define LEX_MAXLONGSTR  100000  /* Maximum length of long string */
#define LEX_BUFFERSIZE  512     /* Size of lexer buffer */

#endif /* alex_h */ 