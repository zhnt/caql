/*
** $Id: aundump.h $
** Load precompiled AQL chunks
** See Copyright Notice in aql.h
*/

#ifndef aundump_h
#define aundump_h

#include "aobject.h"
#include "azio.h"

/*
** AQL Binary Format Constants
*/
#define AQL_SIGNATURE    "\x1bAQL"  /* Binary chunk signature */
#define AQL_BINARY_VERSION  0x01    /* Binary format version */
#define AQL_BINARY_FORMAT   0       /* Format identifier */

/*
** Bytecode file format sizes (for validation)
*/
#define AQLC_INT_SIZE      sizeof(int)
#define AQLC_NUMBER_SIZE   sizeof(aql_Number)
#define AQLC_INTEGER_SIZE  sizeof(aql_Integer)
#define AQLC_INSTRUCTION_SIZE sizeof(Instruction)

/*
** Load states for undumping
*/
typedef struct LoadState {
  aql_State *L;
  ZIO *Z;
  const char *name;
  int swap;  /* byte order swapping needed */
} LoadState;

/*
** Undump functions
*/
AQL_API LClosure *aqlU_undump(aql_State *L, ZIO *Z, const char *name);
AQL_API int aqlU_header(LoadState *S);
AQL_API aql_Byte aqlU_byte(LoadState *S);
AQL_API size_t aqlU_size_t(LoadState *S);
AQL_API int aqlU_int(LoadState *S);
AQL_API aql_Integer aqlU_integer(LoadState *S);
AQL_API aql_Number aqlU_number(LoadState *S);
AQL_API TString *aqlU_string(LoadState *S);
AQL_API void aqlU_block(LoadState *S, void *b, size_t size);

/*
** Validation functions
*/
AQL_API void aqlU_check_header(LoadState *S);
AQL_API void aqlU_check_sizes(LoadState *S);
AQL_API void aqlU_error(LoadState *S, const char *msg);

/*
** Dump functions (for saving bytecode)
*/
typedef struct DumpState {
  aql_State *L;
  aql_Writer writer;
  void *data;
  int strip;  /* strip debug information */
  int status; /* error status */
} DumpState;

AQL_API int aqlU_dump(aql_State *L, const Proto *f, aql_Writer w, void *data, int strip);
AQL_API void aqlU_dump_header(DumpState *D);
AQL_API void aqlU_dump_byte(DumpState *D, aql_Byte b);
AQL_API void aqlU_dump_size_t(DumpState *D, size_t x);
AQL_API void aqlU_dump_int(DumpState *D, int x);
AQL_API void aqlU_dump_integer(DumpState *D, aql_Integer x);
AQL_API void aqlU_dump_number(DumpState *D, aql_Number x);
AQL_API void aqlU_dump_string(DumpState *D, const TString *s);
AQL_API void aqlU_dump_block(DumpState *D, const void *b, size_t size);

/*
** AQL-specific container serialization
*/
AQL_API void aqlU_dump_array_type(DumpState *D, const Array *arr);
AQL_API void aqlU_dump_slice_type(DumpState *D, const Slice *slice);
AQL_API void aqlU_dump_dict_type(DumpState *D, const Dict *dict);
AQL_API void aqlU_dump_vector_type(DumpState *D, const Vector *vec);

AQL_API Array *aqlU_load_array_type(LoadState *S);
AQL_API Slice *aqlU_load_slice_type(LoadState *S);
AQL_API Dict *aqlU_load_dict_type(LoadState *S);
AQL_API Vector *aqlU_load_vector_type(LoadState *S);

/*
** Versioning and compatibility
*/
AQL_API int aqlU_check_version(LoadState *S);
AQL_API int aqlU_is_compatible(int version, int format);
AQL_API void aqlU_write_version_info(DumpState *D);

/*
** Compression support (optional)
*/
#if defined(AQL_USE_COMPRESSION)
AQL_API int aqlU_compress_chunk(aql_State *L, const void *data, size_t size, 
                                void **compressed, size_t *compressed_size);
AQL_API int aqlU_decompress_chunk(aql_State *L, const void *compressed, size_t compressed_size,
                                  void **data, size_t *size);
#endif

/*
** Debug information handling
*/
AQL_API void aqlU_dump_debug_info(DumpState *D, const Proto *f);
AQL_API void aqlU_load_debug_info(LoadState *S, Proto *f);
AQL_API void aqlU_strip_debug_info(Proto *f);

/*
** Validation and security
*/
AQL_API int aqlU_verify_chunk(aql_State *L, const void *data, size_t size);
AQL_API int aqlU_check_integrity(LoadState *S);
AQL_API void aqlU_validate_proto(LoadState *S, const Proto *f);

/*
** Error codes for undump operations
*/
#define UNDUMP_OK           0
#define UNDUMP_ERROR_IO     1
#define UNDUMP_ERROR_FORMAT 2
#define UNDUMP_ERROR_VERSION 3
#define UNDUMP_ERROR_CORRUPT 4
#define UNDUMP_ERROR_MEMORY 5

/*
** Utility macros
*/
#define aqlU_read_byte(S)       ((aql_Byte)aqlU_byte(S))
#define aqlU_write_byte(D, b)   aqlU_dump_byte(D, (aql_Byte)(b))

#endif /* aundump_h */ 