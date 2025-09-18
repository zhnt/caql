/*
** $Id: arange.c $
** Range implementation for AQL
** See Copyright Notice in aql.h
*/

#include "arange.h"
#include "amem.h"
#include "agc.h"
#include "astring.h"
#include "aobject.h"
#include "aql.h"
#include "aauxlib.h"
#include "ado.h"
#include "astate.h"
#include <stdio.h>
#include <string.h>

/* Forward declaration for accessing function arguments */
static const TValue *aql_index2addr(aql_State *L, int idx) {
  /* Simplified implementation - assumes positive indices for now */
  if (idx > 0 && idx <= (L->top - L->stack)) {
    return s2v(L->stack + idx - 1);
  }
  return NULL;
}

/* Simple implementation of argument checking for range functions */
static aql_Integer check_integer_arg(aql_State *L, int arg) {
  const TValue *o = aql_index2addr(L, arg);
  if (o == NULL || !ttisinteger(o)) {
    aqlG_runerror(L, "argument #%d must be an integer", arg);
    return 0;
  }
  return ivalue(o);
}

/*
** Create a new range object
*/
AQL_API RangeObject *aqlR_new(aql_State *L, aql_Integer start, aql_Integer stop, aql_Integer step) {
  /* Validate step parameter */
  if (step == 0) {
    aqlG_runerror(L, "range() step argument must not be zero");
    return NULL;
  }
  
  /* Allocate memory for range object */
  RangeObject *range = (RangeObject*)aqlM_newobject(L, AQL_TRANGE, sizeof(RangeObject));
  if (range == NULL) {
    aqlG_runerror(L, "not enough memory for range object");
    return NULL;
  }
  
  /* Initialize range properties */
  range->start = start;
  range->stop = stop;
  range->step = step;
  range->current = start;
  range->count = aqlR_calculate_count(start, stop, step);
  range->finished = (range->count <= 0) ? 1 : 0;
  
  return range;
}

/*
** Free a range object
*/
AQL_API void aqlR_free(aql_State *L, RangeObject *range) {
  /* Range objects don't have additional allocated memory, so nothing special to free */
  UNUSED(L);
  UNUSED(range);
}

/*
** Infer step size based on start and stop values
*/
AQL_API aql_Integer aqlR_infer_step(aql_Integer start, aql_Integer stop) {
  if (start < stop) return 1;   /* Forward direction */
  if (start > stop) return -1;  /* Reverse direction */
  return 1;  /* start == stop, return 1 (but count will be 0) */
}

/*
** Calculate the number of iterations for a range
*/
AQL_API aql_Integer aqlR_calculate_count(aql_Integer start, aql_Integer stop, aql_Integer step) {
  if (step == 0) return 0;  /* Invalid step */
  if (start == stop) return 0;  /* Empty range */
  
  if (step > 0) {
    /* Forward range: start < stop */
    if (stop <= start) return 0;  /* Invalid forward range */
    return (stop - start + step - 1) / step;
  } else {
    /* Reverse range: start > stop */
    if (stop >= start) return 0;  /* Invalid reverse range */
    return (start - stop - step - 1) / (-step);
  }
}

/*
** range(stop) - single parameter version
*/
AQL_API int aqlR_range1(aql_State *L) {
  aql_Integer stop = check_integer_arg(L, 1);
  RangeObject *range = aqlR_new(L, 0, stop, 1);  /* start=0, step=1 */
  
  if (range == NULL) {
    return 0;  /* Error already set by aqlR_new */
  }
  
  /* Push the range object onto the stack */
  setrangevalue(L, s2v(L->top), range);
  L->top++;  /* Increment stack top */
  
  return 1;  /* Return 1 value (the range object) */
}

/*
** range(start, stop) - two parameter version with smart step inference
*/
AQL_API int aqlR_range2(aql_State *L) {
  aql_Integer start = check_integer_arg(L, 1);
  aql_Integer stop = check_integer_arg(L, 2);
  aql_Integer step = aqlR_infer_step(start, stop);
  
  RangeObject *range = aqlR_new(L, start, stop, step);
  if (range == NULL) {
    return 0;  /* Error already set by aqlR_new */
  }
  
  /* Push the range object onto the stack */
  setrangevalue(L, s2v(L->top), range);
  L->top++;  /* Increment stack top */
  
  return 1;  /* Return 1 value (the range object) */
}

/*
** range(start, stop, step) - three parameter version
*/
AQL_API int aqlR_range3(aql_State *L) {
  aql_Integer start = check_integer_arg(L, 1);
  aql_Integer stop = check_integer_arg(L, 2);
  aql_Integer step = check_integer_arg(L, 3);
  
  if (step == 0) {
    aqlG_runerror(L, "range() step argument must not be zero");
    return 0;
  }
  
  RangeObject *range = aqlR_new(L, start, stop, step);
  if (range == NULL) {
    return 0;  /* Error already set by aqlR_new */
  }
  
  /* Push the range object onto the stack */
  setrangevalue(L, s2v(L->top), range);
  L->top++;  /* Increment stack top */
  
  return 1;  /* Return 1 value (the range object) */
}

/*
** __iter method for range objects - returns the iterator (self)
*/
AQL_API int aqlR_iter(aql_State *L) {
  /* Check that we have a range object */
  if (!aql_isrange(L, 1)) {
    aqlG_typeerror(L, aql_index2addr(L, 1), "range");
    return 0;
  }
  
  /* For range objects, __iter returns self */
  setobj2s(L, L->top, aql_index2addr(L, 1));  /* Copy self to result */
  L->top++;  /* Increment stack top */
  return 1;
}

/*
** __next method for range objects - returns next value or nil when finished
*/
AQL_API int aqlR_next(aql_State *L) {
  /* Check that we have a range object */
  if (!aql_isrange(L, 1)) {
    aqlG_typeerror(L, aql_index2addr(L, 1), "range");
    return 0;
  }
  
  RangeObject *range = rangevalue(aql_index2addr(L, 1));
  
  /* Check if iteration is finished */
  if (range->finished || range->count <= 0) {
    setnilvalue(s2v(L->top));
    L->top++;
    return 1;
  }
  
  /* Return current value */
  setivalue(s2v(L->top), range->current);
  L->top++;
  
  /* Advance to next value */
  range->current += range->step;
  range->count--;
  
  /* Mark as finished if no more iterations */
  if (range->count <= 0) {
    range->finished = 1;
  }
  
  return 1;
}

/*
** Register range builtin functions
*/
AQL_API void aqlR_register_builtins(aql_State *L) {
  /* Register range functions in the global builtin table */
  /* This will be called during AQL initialization */
  /* For now, we'll register them manually in the VM builtin switch */
  UNUSED(L);
}