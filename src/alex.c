/*
** $Id: alex.c $
** Lexical Analyzer for AQL
** See Copyright Notice in aql.h
*/

#define alex_c
#define AQL_CORE

#include <locale.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include "adebug_user.h"
#include <setjmp.h>

#ifdef AQL_DEBUG_BUILD
/* Token collection for debug output - only in debug builds */
static AQL_TokenInfo *debug_tokens = NULL;
static int debug_token_count = 0;
static int debug_token_capacity = 0;
static int debug_collecting_tokens = 0;

/* Token collection functions */
void start_token_collection(void) {
    if (aql_debug_enabled && (aql_debug_flags & AQL_DEBUG_LEX)) {
        debug_collecting_tokens = 1;
        debug_token_count = 0;
        if (!debug_tokens) {
            debug_token_capacity = 64;
            debug_tokens = malloc(debug_token_capacity * sizeof(AQL_TokenInfo));
        }
    }
}

static void add_debug_token(int token_type, const char *value, int line, int column) {
    if (!debug_collecting_tokens) return;
    
    if (debug_token_count >= debug_token_capacity) {
        debug_token_capacity *= 2;
        debug_tokens = realloc(debug_tokens, debug_token_capacity * sizeof(AQL_TokenInfo));
    }
    
    AQL_TokenInfo *token = &debug_tokens[debug_token_count++];
    token->type = token_type;
    token->name = aqlD_token_name(token_type);
    token->value = value ? strdup(value) : NULL;
    token->line = line;
    token->column = column;
}

void finish_token_collection(void) {
    if (!debug_collecting_tokens) return;
    
    aqlD_print_tokens_header();
    for (int i = 0; i < debug_token_count; i++) {
        aqlD_print_token(i, &debug_tokens[i]);
        if (debug_tokens[i].value) {
            free((void*)debug_tokens[i].value);
        }
    }
    aqlD_print_tokens_footer(debug_token_count);
    
    debug_collecting_tokens = 0;
    debug_token_count = 0;
}

#else
/* Release build - no token collection */
void start_token_collection(void) { /* no-op */ }
void finish_token_collection(void) { /* no-op */ }
static void add_debug_token(int token_type, const char *value, int line, int column) { 
    (void)token_type; (void)value; (void)line; (void)column; /* no-op */ 
}
#endif


/* Include core AQL headers */
#include "aql.h"
#include "alex.h"          /* includes aobject.h, astate.h, azio.h with forward declarations */
#include "aerror.h"        /* for error reporting functions */
#include "ado.h"           /* for aql_longjmp structure */

/* Reserved words and their token values */
static const char *const aql_tokens [] = {
    "and", "break", "do", "else", "elseif", "end", "false", "for",
    "function", "goto", "if", "in", "local", "nil", "not", "or",
    "repeat", "return", "then", "true", "until", "while",
    
    /* AQL specific keywords */
    "class", "interface", "struct", "import", "as",
    "async", "await", "yield", "coroutine",
    "array", "slice", "dict", "vector",
    "int", "float", "string", "bool",
    "const", "var", "let", "type", "generic",
    "elif", 
    
    /* AI-specific keywords (Phase 2) */
    "ai", "intent", "workflow", "parallel",
    
    /* Math keywords */
    "div",  /* integer division */
    
    /* operators */
    "//", "..", "...", "==", ">=", "<=", "~=", "<<", ">>", "::",
    
    /* AQL specific operators */
    "->", "|", "?", "??", "+=", "-=", "*=", "/=", "&=", "|=", "^=",
    
    "<eof>", "<number>", "<integer>", "<name>", "<string>"
};



/* Helper macros and definitions */
#define EOZ (-1)

#define next(ls)	do { \
  int old_current = ls->current; \
  ls->current = zgetc(ls->z); \
  if (old_current == '\n' || old_current == '\r') { \
    ls->column = 1; \
  } else { \
    ls->column++; \
  } \
  AQL_DEBUG(AQL_DEBUG_LEX, "next: %c (%d) -> %c (%d)", \
         (old_current >= 32 && old_current < 127) ? old_current : '?', old_current, \
         (ls->current >= 32 && ls->current < 127) ? ls->current : '?', ls->current); \
} while(0)
#define currIsNewline(ls)	(ls->current == '\n' || ls->current == '\r')

/* Forward declarations */
static void inclinenumber(LexState *ls);
static void save_and_next(LexState *ls);
static void save(LexState *ls, int c);
static int check_next1(LexState *ls, int c);
static void read_string(LexState *ls, int del, SemInfo *seminfo);
static int read_numeral(LexState *ls, SemInfo *seminfo);
static int isreserved(TString *ts);

/*
** =======================================================
** LEXICAL ANALYZER
** =======================================================
*/

static void inclinenumber(LexState *ls) {
  int old = ls->current;
  aql_assert(currIsNewline(ls));
  next(ls);  /* skip '\n' or '\r' */
  if (currIsNewline(ls) && ls->current != old)
    next(ls);  /* skip '\n\r' or '\r\n' */
  if (++ls->linenumber >= MAX_INT)
    aqlX_lexerror(ls, "chunk has too many lines", 0);
}

static void save(LexState *ls, int c) {
  
  Mbuffer *b = ls->buff;
  
  if (!b) {
    return;
  }
  
  if (!b->buffer) {
    /* Initialize buffer using AQL's buffer system */
    aqlZ_resizebuffer(ls->L, b, 32);
  }
  
  
  if (aqlZ_bufflen(b) + 1 > aqlZ_sizebuffer(b)) {
    size_t newsize;
    if (aqlZ_sizebuffer(b) >= MAX_SIZE/2) {
      aqlX_lexerror(ls, "lexical element too long", 0);
    }
    newsize = aqlZ_sizebuffer(b) * 2;
    aqlZ_resizebuffer(ls->L, b, newsize);
  }
  
  b->buffer[aqlZ_bufflen(b)++] = cast_char(c);
  
}

static void save_and_next(LexState *ls) {
  
  save(ls, ls->current);
  next(ls);
}

static int check_next1(LexState *ls, int c) {
  if (ls->current == c) {
    next(ls);
    return 1;
  }
  else return 0;
}

int aqlX_check_next1(LexState *ls, int c) {
  return check_next1(ls, c);
}

static void read_string(LexState *ls, int del, SemInfo *seminfo) {
  save_and_next(ls);  /* keep delimiter (for error messages) */
  while (ls->current != del) {
    switch (ls->current) {
      case EOZ:
        aqlX_lexerror(ls, "unfinished string", TK_EOS);
        break;  /* to avoid warnings */
      case '\n':
      case '\r':
        aqlX_lexerror(ls, "unfinished string", TK_STRING);
        break;  /* to avoid warnings */
      case '\\': {  /* escape sequences */
        int c;  /* final character to be saved */
        save_and_next(ls);  /* keep '\\' for error messages */
        switch (ls->current) {
          case 'a': c = '\a'; goto read_save;
          case 'b': c = '\b'; goto read_save;
          case 'f': c = '\f'; goto read_save;
          case 'n': c = '\n'; goto read_save;
          case 'r': c = '\r'; goto read_save;
          case 't': c = '\t'; goto read_save;
          case 'v': c = '\v'; goto read_save;
          case '\n': case '\r':
            inclinenumber(ls); c = '\n'; goto only_save;
          case '\\': case '\"': case '\'':
            c = ls->current; goto read_save;
          case EOZ: goto no_save;  /* will raise an error next loop */
          default: {
            if (!isdigit(ls->current))
              aqlX_lexerror(ls, "invalid escape sequence", TK_STRING);
            c = 0;
            int i;
            for (i = 0; i < 3 && isdigit(ls->current); i++) {
              c = 10*c + (ls->current - '0');
              save_and_next(ls);
            }
            if (c > UCHAR_MAX)
              aqlX_lexerror(ls, "escape sequence too large", TK_STRING);
            goto only_save;
          }
        }
       read_save:
         next(ls);
         /* go through */
       only_save:
         save(ls, c);
         /* go through */
       no_save: break;
      }
      default:
        save_and_next(ls);
    }
  }
  save_and_next(ls);  /* skip delimiter */
  seminfo->ts = aqlX_newstring(ls, aqlZ_buffer(ls->buff) + 1,
                                   aqlZ_bufflen(ls->buff) - 2);
}

static int read_numeral(LexState *ls, SemInfo *seminfo) {
  
  /* Simple integer parsing for now */
  aql_Integer value = 0;
  do {
    
    value = value * 10 + (ls->current - '0');
    
    save_and_next(ls);
    
  } while (isdigit(ls->current));
  
  
  seminfo->i = value;
  return TK_INT_LITERAL;
}

static int isreserved(TString *ts) {
  const char *s = getstr(ts);
  
  
  /* Check for mathematical keywords */
  if (strcmp(s, "div") == 0) {
    return TK_DIV_KW;
  }
  
  /* Check for basic keywords that we need */
  if (strcmp(s, "let") == 0) return TK_LET;
  if (strcmp(s, "const") == 0) return TK_CONST;
  if (strcmp(s, "var") == 0) return TK_VAR;
  if (strcmp(s, "true") == 0) return TK_TRUE;
  if (strcmp(s, "false") == 0) return TK_FALSE;
  if (strcmp(s, "nil") == 0) return TK_NIL;
  
  /* Control flow keywords */
  if (strcmp(s, "if") == 0) return TK_IF;
  if (strcmp(s, "else") == 0) return TK_ELSE;
  if (strcmp(s, "elif") == 0) return TK_ELIF;
  if (strcmp(s, "while") == 0) return TK_WHILE;
  if (strcmp(s, "for") == 0) return TK_FOR;
  if (strcmp(s, "in") == 0) return TK_IN;
  if (strcmp(s, "do") == 0) return TK_DO;
  if (strcmp(s, "end") == 0) return TK_END;
  if (strcmp(s, "break") == 0) return TK_BREAK;
  if (strcmp(s, "continue") == 0) return TK_CONTINUE;
  if (strcmp(s, "return") == 0) {
    return TK_RETURN;
  }
  
  /* Logical operators */
  if (strcmp(s, "and") == 0) return TK_AND;
  if (strcmp(s, "or") == 0) return TK_OR;
  if (strcmp(s, "not") == 0) return TK_NOT;
  
  /* For now, return 0 for other keywords */
  return 0;  /* not reserved */
}

/*
** =======================================================
** MAIN LEXICAL ANALYZER
** =======================================================
*/

static int aql_lex(LexState *ls, SemInfo *seminfo) {
  
  int token_type;
  const char *token_value = NULL;
  
  /* Macro to collect token and return */
  #ifdef AQL_DEBUG_BUILD
  #define RETURN_TOKEN(type, value) do { \
    token_type = (type); \
    token_value = (value); \
    add_debug_token(token_type, token_value, ls->linenumber, ls->column); \
    return token_type; \
  } while(0)
  #else
  #define RETURN_TOKEN(type, value) return (type)
  #endif
  
  aqlZ_resetbuffer(ls->buff);
  
  
  for (;;) {
    
    switch (ls->current) {
      case '\n': case '\r': {  /* line breaks */
        inclinenumber(ls);
        break;
      }
      case ' ': case '\f': case '\t': case '\v': {  /* spaces */
        next(ls);
        break;
      }
      
      /* Numbers */
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9': {
        int start_col = ls->column;
        token_type = read_numeral(ls, seminfo);
        char num_buf[32];
        if (token_type == TK_INT_LITERAL) {
          snprintf(num_buf, sizeof(num_buf), "%lld", seminfo->i);
        } else {
          snprintf(num_buf, sizeof(num_buf), "%.2f", seminfo->r);
        }
        #ifdef AQL_DEBUG_BUILD
        add_debug_token(token_type, num_buf, ls->linenumber, start_col);
        #endif
        return token_type;
      }
      
      /* Arithmetic operators */
      case '+': {
        int start_col = ls->column;
        next(ls);
        #ifdef AQL_DEBUG_BUILD
        add_debug_token(TK_PLUS, NULL, ls->linenumber, start_col);
        #endif
        return TK_PLUS;
      }
      case '-': {
        next(ls);
        RETURN_TOKEN(TK_MINUS, NULL);
      }
      case '*': {
        next(ls);
        if (check_next1(ls, '*')) RETURN_TOKEN(TK_POW, NULL);  /* '**' */
        else RETURN_TOKEN(TK_MUL, NULL);  /* '*' */
      }
      case '/': {
        next(ls);
        if (ls->current == '/') {  /* '//' comment */
          next(ls);
          /* Skip single-line comment until end of line */
          while (!currIsNewline(ls) && ls->current != EOZ)
            next(ls);
          break;  /* continue loop to skip whitespace */
        }
        else if (ls->current == '*') {  /* block comment */
          next(ls);
          /* Skip multi-line comment until end marker */
          while (ls->current != EOZ) {
            if (ls->current == '*') {
              next(ls);
              if (ls->current == '/') {
                next(ls);  /* skip the '/' */
                break;     /* end of comment */
              }
            }
            else {
              if (currIsNewline(ls)) {
                inclinenumber(ls);  /* handle newlines in multi-line comments */
              }
              else {
                next(ls);
              }
            }
          }
          break;  /* continue loop to skip whitespace */
        }
        else {
          RETURN_TOKEN(TK_DIV, NULL);
        }
      }
      case '%': {
        next(ls);
        return TK_MOD;
      }
      
      /* Parentheses */
      case '(': {
        next(ls);
        return TK_LPAREN;
      }
      case ')': {
        next(ls);
        return TK_RPAREN;
      }
      
      /* Comparison operators */
      case '=': {
        next(ls);
        if (check_next1(ls, '=')) return TK_EQ;  /* '==' */
        else return TK_ASSIGN;  /* '=' */
      }
      case '>': {
        printf_debug("[DEBUG] alex: found '>', checking for compound operators\n");
        next(ls);
        if (check_next1(ls, '=')) {
          printf_debug("[DEBUG] alex: found '>=', returning TK_GE\n");
          return TK_GE;  /* '>=' */
        }
        else if (check_next1(ls, '>')) {
          printf_debug("[DEBUG] alex: found '>>', returning TK_SHR\n");
          return TK_SHR;  /* '>>' */
        }
        else {
          printf_debug("[DEBUG] alex: found '>', returning TK_GT\n");
          return TK_GT;  /* '>' */
        }
      }
      case '<': {
        next(ls);
        if (check_next1(ls, '=')) return TK_LE;  /* '<=' */
        else if (check_next1(ls, '<')) return TK_SHL;  /* '<<' */
        else return TK_LT;  /* '<' */
      }
      case '!': {
        next(ls);
        if (check_next1(ls, '=')) return TK_NE;  /* '!=' */
        else return TK_LNOT;  /* '!' */
      }
      
      /* Logical operators */
      case '&': {
        next(ls);
        if (check_next1(ls, '&')) return TK_LAND;  /* '&&' */
        else return TK_BAND;  /* '&' */
      }
      case '|': {
        next(ls);
        if (check_next1(ls, '|')) return TK_LOR;  /* '||' */
        else return TK_BOR;  /* '|' */
      }
      case '^': {
        next(ls);
        return TK_BXOR;  /* '^' */
      }
      case '~': {
        next(ls);
        return TK_BNOT;  /* '~' */
      }
      
      /* Ternary operators */
      case '?': {
        next(ls);
        if (check_next1(ls, '?')) return TK_NULLCOAL;  /* '??' */
        else return TK_QUESTION;  /* '?' */
      }
      case ':': {
        next(ls);
        if (check_next1(ls, ':')) return TK_DBCOLON;  /* '::' */
        else if (check_next1(ls, '=')) return TK_ASSIGN;  /* ':=' */
        else return TK_COLON;  /* ':' */
      }
      
      /* Strings */
      case '"': case '\'': {
        read_string(ls, ls->current, seminfo);
        return TK_STRING;
      }
      
      /* End of input */
      case EOZ: {
        return TK_EOS;
      }
      
      /* Default: identifiers or single-char tokens */
      default: {
        if (aqlX_isalpha(ls->current)) {  /* identifier or reserved word? */
          TString *ts;
          do {
            save_and_next(ls);
          } while (aqlX_isalnum(ls->current));
          ts = aqlX_newstring(ls, aqlZ_buffer(ls->buff),
                             aqlZ_bufflen(ls->buff));
          seminfo->ts = ts;
          int reserved = isreserved(ts);
          if (reserved) {  /* reserved word? */
            RETURN_TOKEN(reserved, getstr(ts));
          } else {
            RETURN_TOKEN(TK_NAME, getstr(ts));
          }
        }
        else {  /* single-char tokens */
          int c = ls->current;
          next(ls);
          char char_str[2] = {c, '\0'};
          RETURN_TOKEN(c, char_str);
        }
      }
    }
  }
}

/*
** =======================================================
** INTERFACE FUNCTIONS
** =======================================================
*/

void aqlX_next(LexState *ls) {
  
  ls->lastline = ls->linenumber;
  if (ls->lookahead.token != TK_EOS) {  /* is there a look-ahead token? */
    ls->t = ls->lookahead;  /* use this one */
    ls->lookahead.token = TK_EOS;  /* and discharge it */
  }
  else {
    ls->t.token = aql_lex(ls, &ls->t.seminfo);  /* read next token */
  }
}

int aqlX_lookahead(LexState *ls) {
  aql_assert(ls->lookahead.token == TK_EOS);
  ls->lookahead.token = aql_lex(ls, &ls->lookahead.seminfo);
  return ls->lookahead.token;
}

/*
** Set input source for lexer
*/
void aqlX_setinput(aql_State *L, LexState *ls, ZIO *z, TString *source,
                   int firstchar) {
  
  ls->t.token = 0;
  ls->L = L;
  ls->current = firstchar;
  ls->lookahead.token = TK_EOS;
  ls->z = z;
  ls->fs = NULL;
  ls->linenumber = 1;
  ls->column = 1;
  ls->lastline = 1;
  ls->source = source;
  ls->envn = aqlStr_newlstr(L, "_ENV", strlen("_ENV"));  /* get env name */
  aqlZ_resizebuffer(ls->L, ls->buff, 32);  /* initialize buffer */
  
}

/*
** Convert token to string for error messages
*/
const char *aqlX_token2str(LexState *ls, int token) {
  if (token < FIRST_RESERVED) {  /* single-byte symbols? */
    if (isprint(token))
      return aqlO_pushfstring(ls->L, "'%c'", token);
    else  /* control character */
      return aqlO_pushfstring(ls->L, "'<\\%d>'", token);
  }
  else {
    const char *s = aql_tokens[token - FIRST_RESERVED];
    if (token < TK_EOS)  /* reserved words? */
      return aqlO_pushfstring(ls->L, "'%s'", s);
    else  /* other symbols */
      return s;
  }
}

/*
** Forward declaration for unified error reporting
*/
/* Error reporting functions are in aerror.h */

/*
** Lexical error with unified error handling
*/
l_noret aqlX_lexerror(LexState *ls, const char *msg, int token) {
  const char *near_token = NULL;
  if (token) {
    near_token = aqlX_token2str(ls, token);
  }
  
  /* Use unified error reporting */
  aqlE_report_lexical_error(ls->linenumber, msg, 
                      "Check syntax and character encoding", near_token);
  exit(1);  /* For now, just exit - could be improved with error recovery */
}

/*
** Syntax error with unified error handling
*/
l_noret aqlX_syntaxerror(LexState *ls, const char *msg) {
  const char *near_token = aqlX_token2str(ls, ls->t.token);
  
  /* Use unified error reporting */
  aqlE_report_syntax_error(ls->linenumber, msg,
                     "Check syntax and token order", near_token);
  
  /* Use longjmp for error recovery instead of exit() */
  if (ls->L && ls->L->errorJmp) {
    longjmp(ls->L->errorJmp->b, 1);  /* Jump back to error handler */
  } else {
    exit(1);  /* Fallback: exit if no error recovery is set up */
  }
}

/*
** UTF-8 support functions
*/
int aqlX_utf8_decode(const char *s, int *cp) {
  unsigned char c = (unsigned char)*s;
  if (c < 0x80) {
    *cp = c;
    return 1;
  } else if (c < 0xC2) {
    return 0;  /* invalid */
  } else if (c < 0xE0) {
    if ((s[1] & 0xC0) != 0x80) return 0;
    *cp = ((c & 0x1F) << 6) | (s[1] & 0x3F);
    return 2;
  } else if (c < 0xF0) {
    if ((s[1] & 0xC0) != 0x80) return 0;
    if ((s[2] & 0xC0) != 0x80) return 0;
    *cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    return 3;
  } else if (c < 0xF5) {
    if ((s[1] & 0xC0) != 0x80) return 0;
    if ((s[2] & 0xC0) != 0x80) return 0;
    if ((s[3] & 0xC0) != 0x80) return 0;
    *cp = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | 
          ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    return 4;
  }
  return 0;  /* invalid */
}

int aqlX_utf8_encode(int cp, char *s) {
  if (cp < 0x80) {
    s[0] = (char)cp;
    return 1;
  } else if (cp < 0x800) {
    s[0] = (char)(0xC0 | (cp >> 6));
    s[1] = (char)(0x80 | (cp & 0x3F));
    return 2;
  } else if (cp < 0x10000) {
    s[0] = (char)(0xE0 | (cp >> 12));
    s[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    s[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
  } else if (cp < 0x110000) {
    s[0] = (char)(0xF0 | (cp >> 18));
    s[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    s[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    s[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
  }
  return 0;  /* invalid code point */
}

/*
** Character classification functions are defined as macros in alex.h
*/

/*
** String creation function
*/
TString *aqlX_newstring(LexState *ls, const char *str, size_t l) {
  return aqlStr_newlstr(ls->L, str, l);
}
