/*
** $Id: arange.h $
** Range implementation for AQL
** See Copyright Notice in aql.h
*/

#ifndef arange_h
#define arange_h

#include "aobject.h"
#include "adatatype.h"

/*
** Range object structure - implements Python-style range functionality
*/
typedef struct RangeObject {
  CommonHeader;
  aql_Integer start;    /* Starting value */
  aql_Integer stop;     /* Ending value (exclusive) */
  aql_Integer step;     /* Step size */
  aql_Integer current;  /* Current value during iteration */
  aql_Integer count;    /* Pre-calculated iteration count */
  int finished;         /* Iteration completion flag */
} RangeObject;

/* Range creation and basic operations */
AQL_API RangeObject *aqlR_new(aql_State *L, aql_Integer start, aql_Integer stop, aql_Integer step);
AQL_API void aqlR_free(aql_State *L, RangeObject *range);

/* Range builtin functions */
AQL_API int aqlR_range1(aql_State *L);  /* range(stop) */
AQL_API int aqlR_range2(aql_State *L);  /* range(start, stop) */
AQL_API int aqlR_range3(aql_State *L);  /* range(start, stop, step) */

/* Iterator protocol methods */
AQL_API int aqlR_iter(aql_State *L);    /* __iter method */
AQL_API int aqlR_next(aql_State *L);    /* __next method */

/* Utility functions */
AQL_API aql_Integer aqlR_infer_step(aql_Integer start, aql_Integer stop);
AQL_API aql_Integer aqlR_calculate_count(aql_Integer start, aql_Integer stop, aql_Integer step);

/* Register range builtin functions */
AQL_API void aqlR_register_builtins(aql_State *L);

#endif