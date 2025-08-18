/*
** $Id: aconf.c $
** Configuration functions for AQL
** See Copyright Notice in aql.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <locale.h>

#include "aconf.h"

/*
** Detect system capabilities
*/
AQL_API void aql_detect_capabilities(void) {
    /* Detect endianness */
    union { uint32_t i; char c[4]; } test = { 0x01020304 };
    int is_little_endian = (test.c[0] == 4);
    
    /* Detect pointer size */
    size_t ptr_size = sizeof(void*);
    
    /* Detect integer sizes */
    size_t int_size = sizeof(int);
    size_t long_size = sizeof(long);
    size_t longlong_size = sizeof(long long);
    
    /* Print capabilities for debugging */
    #ifdef DEBUG
    printf("AQL System Capabilities:\n");
    printf("  Endianness: %s\n", is_little_endian ? "Little" : "Big");
    printf("  Pointer size: %zu bytes\n", ptr_size);
    printf("  Int size: %zu bytes\n", int_size);
    printf("  Long size: %zu bytes\n", long_size);
    printf("  Long long size: %zu bytes\n", longlong_size);
    printf("  AQL_INTEGER: %zu bytes\n", sizeof(aql_Integer));
    printf("  AQL_NUMBER: %zu bytes\n", sizeof(aql_Number));
    #endif
    
    /* Suppress unused variable warnings */
    (void)is_little_endian;
    (void)ptr_size;
    (void)int_size;
    (void)long_size;
    (void)longlong_size;
}

/*
** Check if a pointer is aligned to a specific boundary
*/
AQL_API int aql_is_aligned(void *ptr, size_t alignment) {
    if (ptr == NULL) return 1;
    if (alignment == 0) return 1;
    if (!ispow2(alignment)) return 0;  /* alignment must be power of 2 */
    
    uintptr_t addr = (uintptr_t)ptr;
    return (addr & (alignment - 1)) == 0;
}

/*
** Align a size to a specific boundary
*/
AQL_API size_t aql_align_size(size_t size, size_t alignment) {
    if (alignment == 0) return size;
    if (!ispow2(alignment)) return size;  /* alignment must be power of 2 */
    
    return (size + alignment - 1) & ~(alignment - 1);
}

/*
** Default panic function
*/
AQL_API void aql_panic(const char *msg) {
    fprintf(stderr, "AQL PANIC: %s\n", msg ? msg : "unknown error");
    abort();
}

/*
** Version information functions
*/
const char *aql_version_string = "AQL 1.0.0";
int aql_version_major = 1;
int aql_version_minor = 0;
int aql_version_patch = 0;