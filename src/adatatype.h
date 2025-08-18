/*
** $Id: adatatype.h $
** Data Types for AQL containers
** See Copyright Notice in aql.h
*/

#ifndef adatatype_h
#define adatatype_h

#include "aconf.h"

/*
** AQL Data Types for container elements
*/
typedef enum {
    AQL_DATA_TYPE_UNKNOWN = 0,
    AQL_DATA_TYPE_INT8,
    AQL_DATA_TYPE_UINT8,
    AQL_DATA_TYPE_INT16,
    AQL_DATA_TYPE_UINT16,
    AQL_DATA_TYPE_INT32,
    AQL_DATA_TYPE_UINT32,
    AQL_DATA_TYPE_INT64,
    AQL_DATA_TYPE_UINT64,
    AQL_DATA_TYPE_FLOAT32,
    AQL_DATA_TYPE_FLOAT64,
    AQL_DATA_TYPE_BOOLEAN,
    AQL_DATA_TYPE_STRING,
    AQL_DATA_TYPE_ANY, /* For mixed-type containers (e.g., Array of TValue) */
    AQL_DATA_TYPE_COUNT, /* Total number of data types */
    
    /* Legacy aliases for compatibility */
    DT_INT = AQL_DATA_TYPE_INT32,
    DT_FLOAT = AQL_DATA_TYPE_FLOAT32,
    DT_BOOL = AQL_DATA_TYPE_BOOLEAN,
    DT_STRING = AQL_DATA_TYPE_STRING
} DataType;

/*
** Size information for each data type
*/
AQL_API size_t aqlDT_sizeof(DataType dtype);
AQL_API const char *aqlDT_name(DataType dtype);

/*
** Type checking macros
*/
#define aqlDT_isinteger(dt) ((dt) >= AQL_DATA_TYPE_INT8 && (dt) <= AQL_DATA_TYPE_UINT64)
#define aqlDT_isfloat(dt)   ((dt) == AQL_DATA_TYPE_FLOAT32 || (dt) == AQL_DATA_TYPE_FLOAT64)
#define aqlDT_isnumeric(dt) (aqlDT_isinteger(dt) || aqlDT_isfloat(dt))

#endif