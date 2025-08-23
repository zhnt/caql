/*
** $Id: azio.c $
** AQL Buffered streams (based on Lua lzio.c)
** See Copyright Notice in aql.h
*/

#define azio_c
#define AQL_CORE

#include "aconf.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "aql.h"

#include "alimits.h"
#include "amem.h"
#include "astate.h"
#include "azio.h"

#define AQL_MINBUFFER 32

int aqlZ_fill (ZIO *z) {
  size_t size;
  aql_State *L = z->L;
  const char *buff;
  /* TODO: aql_unlock(L); */
  buff = z->reader(L, z->data, &size);
  /* TODO: aql_lock(L); */
  if (buff == NULL || size == 0)
    return EOZ;
  z->n = size - 1;  /* discount char being returned */
  z->p = buff;
  return cast_uchar(*(z->p++));
}


void aqlZ_init (aql_State *L, ZIO *z, aql_Reader reader, void *data) {
  z->L = L;
  z->reader = reader;
  z->data = data;
  z->n = 0;
  z->p = NULL;
}


/* --------------------------------------------------------------- read --- */
size_t aqlZ_read (ZIO *z, void *b, size_t n) {
  while (n) {
    size_t m;
    if (z->n == 0) {  /* no bytes in buffer? */
      if (aqlZ_fill(z) == EOZ)  /* try to read more */
        return n;  /* no more input; return number of missing bytes */
      else {
        z->n++;  /* aqlZ_fill consumed first byte; put it back */
        z->p--;
      }
    }
    m = (n <= z->n) ? n : z->n;  /* min. between n and z->n */
    memcpy(b, z->p, m);
    z->n -= m;
    z->p += m;
    b = (char *)b + m;
    n -= m;
  }
  return 0;
}

/*
** Buffer operations
*/
void aqlZ_pushchar(aql_State *L, Mbuffer *buff, int c) {
  if (buff->n >= buff->buffsize) {
    size_t newsize = buff->buffsize * 2;
    if (newsize < AQL_MINBUFFER) newsize = AQL_MINBUFFER;
    aqlZ_resizebuffer(L, buff, newsize);
  }
  buff->buffer[buff->n++] = cast_byte(c);
}

void aqlZ_clearerrbuffer(aql_State *L) {
  /* For MVP, this is a no-op */
  UNUSED(L);
}

char *aqlZ_openspace(aql_State *L, Mbuffer *buff, size_t n) {
  if (buff->n + n > buff->buffsize) {
    size_t newsize = buff->buffsize;
    while (newsize < buff->n + n) {
      newsize *= 2;
      if (newsize < AQL_MINBUFFER) newsize = AQL_MINBUFFER;
    }
    aqlZ_resizebuffer(L, buff, newsize);
  }
  return &buff->buffer[buff->n];
}

void aqlZ_addsize(Mbuffer *buff, size_t n) {
  buff->n += n;
}

void aqlZ_pushstring(aql_State *L, Mbuffer *buff, const char *s, size_t len) {
  char *space = aqlZ_openspace(L, buff, len);
  memcpy(space, s, len);
  aqlZ_addsize(buff, len);
}

void aqlZ_pushfstring(aql_State *L, Mbuffer *buff, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  aqlZ_pushvfstring(L, buff, fmt, argp);
  va_end(argp);
}

void aqlZ_pushvfstring(aql_State *L, Mbuffer *buff, const char *fmt, va_list argp) {
  /* Simple implementation - estimate size and format */
  size_t estimated_size = strlen(fmt) + 256;  /* rough estimate */
  char *space = aqlZ_openspace(L, buff, estimated_size);
  int len = vsnprintf(space, estimated_size, fmt, argp);
  if (len < 0 || (size_t)len >= estimated_size) {
    /* Format failed or truncated, try with larger buffer */
    estimated_size = strlen(fmt) + 1024;
    space = aqlZ_openspace(L, buff, estimated_size);
    len = vsnprintf(space, estimated_size, fmt, argp);
  }
  if (len > 0) {
    aqlZ_addsize(buff, (size_t)len);
  }
}

/*
** String reader for parsing string literals
*/

static const char *string_reader(aql_State *L, void *data, size_t *size) {
  StringReaderData *srd = (StringReaderData *)data;
  UNUSED(L);
  
  if (srd->pos >= srd->len) {
    *size = 0;
    return NULL;
  }
  
  *size = srd->len - srd->pos;
  const char *result = srd->s + srd->pos;
  srd->pos = srd->len;  /* mark as consumed */
  return result;
}

/*
** Initialize ZIO for string input
*/
void aqlZ_init_string(aql_State *L, ZIO *z, const char *s, size_t len) {
  StringReaderData *srd = (StringReaderData *)aqlM_malloc(L, sizeof(StringReaderData));
  srd->s = s;
  srd->len = len;
  srd->pos = 0;
  aqlZ_init(L, z, string_reader, srd);
}

/*
** Clean up string ZIO
*/
void aqlZ_cleanup_string(aql_State *L, ZIO *z) {
  if (z->data) {
    aqlM_freemem(L, z->data, sizeof(StringReaderData));
    z->data = NULL;
  }
}