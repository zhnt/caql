/*
** $Id: azio.h $
** Buffered streams for AQL
** See Copyright Notice in aql.h
*/

#ifndef azio_h
#define azio_h

#include <stdarg.h>
#include <stdio.h>
#include "aconf.h"
#include "aql.h"
#include "amem.h"

#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : aqlZ_fill(z))

/*
** Buffered stream structure
*/
struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  aql_Reader reader;		/* reader function */
  void *data;			/* additional data */
  aql_State *L;			/* AQL state (for memory allocation) */
};

/*
** ZIO functions
*/
AQL_API void aqlZ_init(aql_State *L, ZIO *z, aql_Reader reader, void *data);
AQL_API int aqlZ_fill(ZIO *z);
AQL_API size_t aqlZ_read(ZIO *z, void *b, size_t n);

/* --------- Mbuffer --------- */

/*
** Buffer for lexer/parser
*/
typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define aqlZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->n = (buff)->buffsize = 0)

#define aqlZ_buffer(buff)	((buff)->buffer)
#define aqlZ_sizebuffer(buff)	((buff)->buffsize)
#define aqlZ_bufflen(buff)	((buff)->n)

#define aqlZ_buffremove(buff,i)	((buff)->n -= (i))
#define aqlZ_resetbuffer(buff) ((buff)->n = 0)

#define aqlZ_resizebuffer(L, buff, size) \
	((buff)->buffer = aqlM_reallocvchar(L, (buff)->buffer, \
				(buff)->buffsize, size), \
	(buff)->buffsize = size)

#define aqlZ_freebuffer(L, buff)	aqlZ_resizebuffer(L, (buff), 0)

/*
** Buffer operations
*/
AQL_API void aqlZ_pushchar(aql_State *L, Mbuffer *buff, int c);
AQL_API void aqlZ_clearerrbuffer(aql_State *L);
AQL_API char *aqlZ_openspace(aql_State *L, Mbuffer *buff, size_t n);
AQL_API void aqlZ_addsize(Mbuffer *buff, size_t n);

/*
** String buffer operations
*/
AQL_API void aqlZ_pushstring(aql_State *L, Mbuffer *buff, const char *s, size_t len);
AQL_API void aqlZ_pushfstring(aql_State *L, Mbuffer *buff, const char *fmt, ...);
AQL_API void aqlZ_pushvfstring(aql_State *L, Mbuffer *buff, const char *fmt, va_list argp);

/*
** File I/O integration
*/
AQL_API int aqlZ_lookahead(ZIO *z);
AQL_API void aqlZ_pushback(ZIO *z, int c);

/*
** Error handling for streams
*/
AQL_API void aqlZ_error(aql_State *L, const char *msg);
AQL_API void aqlZ_ioerror(aql_State *L, const char *filename, const char *msg);

/*
** Reader functions for different sources
*/
typedef struct FileReaderData {
  FILE *f;
  char buff[AQL_BUFFERSIZE];
} FileReaderData;

typedef struct StringReaderData {
  const char *s;
  size_t size;
} StringReaderData;

AQL_API const char *aqlZ_file_reader(aql_State *L, void *data, size_t *size);
AQL_API const char *aqlZ_string_reader(aql_State *L, void *data, size_t *size);
AQL_API const char *aqlZ_buffer_reader(aql_State *L, void *data, size_t *size);

/*
** High-level file operations
*/
AQL_API ZIO *aqlZ_open_file(aql_State *L, const char *filename, const char *mode);
AQL_API ZIO *aqlZ_open_string(aql_State *L, const char *s, size_t len);
AQL_API ZIO *aqlZ_open_buffer(aql_State *L, const char *buffer, size_t size);
AQL_API void aqlZ_close(ZIO *z);

/*
** Stream utilities
*/
AQL_API int aqlZ_getline(ZIO *z, Mbuffer *buff);
AQL_API size_t aqlZ_copyuntil(ZIO *z, Mbuffer *buff, int delimiter);
AQL_API int aqlZ_skip_bom(ZIO *z);  /* Skip UTF-8 BOM if present */

/*
** UTF-8 support for streams
*/
AQL_API int aqlZ_getutf8(ZIO *z);     /* Read one UTF-8 character */
AQL_API void aqlZ_pushutf8(aql_State *L, Mbuffer *buff, int cp);  /* Push UTF-8 character */

#endif /* azio_h */ 