/*
** $Id: adatatype.c $
** AQL Data Type Implementation
** See Copyright Notice in aql.h
*/

#include "adatatype.h"
#include <stddef.h>

/*
** Size information for each data type
*/
AQL_API size_t aqlDT_sizeof(DataType dtype) {
  switch (dtype) {
    case AQL_DATA_TYPE_INT8:    return sizeof(signed char);
    case AQL_DATA_TYPE_UINT8:   return sizeof(unsigned char);
    case AQL_DATA_TYPE_INT16:   return sizeof(short);
    case AQL_DATA_TYPE_UINT16:  return sizeof(unsigned short);
    case AQL_DATA_TYPE_INT32:   return sizeof(int);
    case AQL_DATA_TYPE_UINT32:  return sizeof(unsigned int);
    case AQL_DATA_TYPE_INT64:   return sizeof(long long);
    case AQL_DATA_TYPE_UINT64:  return sizeof(unsigned long long);
    case AQL_DATA_TYPE_FLOAT32: return sizeof(float);
    case AQL_DATA_TYPE_FLOAT64: return sizeof(double);
    case AQL_DATA_TYPE_BOOLEAN: return sizeof(char);
    case AQL_DATA_TYPE_STRING:  return sizeof(void*);  /* Pointer to string */
    case AQL_DATA_TYPE_ANY:     return sizeof(void*);  /* Pointer to TValue */
    default:                    return 0;
  }
}

/*
** Name strings for each data type
*/
AQL_API const char *aqlDT_name(DataType dtype) {
  switch (dtype) {
    case AQL_DATA_TYPE_UNKNOWN: return "unknown";
    case AQL_DATA_TYPE_INT8:    return "int8";
    case AQL_DATA_TYPE_UINT8:   return "uint8";
    case AQL_DATA_TYPE_INT16:   return "int16";
    case AQL_DATA_TYPE_UINT16:  return "uint16";
    case AQL_DATA_TYPE_INT32:   return "int32";
    case AQL_DATA_TYPE_UINT32:  return "uint32";
    case AQL_DATA_TYPE_INT64:   return "int64";
    case AQL_DATA_TYPE_UINT64:  return "uint64";
    case AQL_DATA_TYPE_FLOAT32: return "float32";
    case AQL_DATA_TYPE_FLOAT64: return "float64";
    case AQL_DATA_TYPE_BOOLEAN: return "boolean";
    case AQL_DATA_TYPE_STRING:  return "string";
    case AQL_DATA_TYPE_ANY:     return "any";
    default:                    return "invalid";
  }
}