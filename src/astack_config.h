/*
** $Id: astack_config.h $
** AQL Stack Configuration
** Centralized configuration for all stack-related parameters
** See Copyright Notice in aql.h
*/

#ifndef astack_config_h
#define astack_config_h

/*
** =======================================================================
** AQL Stack Configuration - Centralized Management
** =======================================================================
** 
** This file centralizes all stack-related configuration parameters
** to avoid duplication and provide clear documentation.
*/

/*
** Basic Stack Parameters
*/

/* Minimum stack space available to a C function */
#define AQL_MINSTACK                20

/* Initial stack size when AQL state is created */
#define AQL_BASIC_STACK_SIZE        (10 * AQL_MINSTACK)  /* 200 elements */

/* Extra stack buffer for overflow detection and safety */
#define AQL_EXTRA_STACK             20

/* Total initial stack allocation */
#define AQL_INITIAL_STACK_TOTAL     (AQL_BASIC_STACK_SIZE + AQL_EXTRA_STACK)  /* 220 elements */

/*
** Stack Growth Parameters
*/

/* Maximum VM stack size (theoretical limit) */
#define AQL_MAXSTACK_SIZE           1000000

/* Stack growth increment when overflow detected */
#define AQL_STACK_GROWTH_FACTOR     2  /* Double the size when growing */

/* Minimum growth size to avoid frequent reallocations */
#define AQL_MIN_STACK_GROWTH        50

/*
** Recursion Depth Limits
*/

/* Maximum C call depth (prevents C stack overflow) */
#define AQL_MAXCCALLS               200

/* Conservative recursion depth limit (can be adjusted) */
#define AQL_SAFE_RECURSION_DEPTH    45  /* Empirically determined safe depth */

/* Recursion depth warning threshold */
#define AQL_RECURSION_WARNING       40

/*
** Stack Memory Calculation Helpers
*/

/* Estimate stack elements needed for N recursive calls */
#define AQL_STACK_FOR_RECURSION(n)  ((n) * 4 + AQL_MINSTACK)

/* Check if recursion depth is within safe limits */
#define AQL_IS_SAFE_RECURSION(n)    ((n) <= AQL_SAFE_RECURSION_DEPTH)

/*
** Configuration Validation
*/
#if AQL_BASIC_STACK_SIZE < AQL_MINSTACK
#error "AQL_BASIC_STACK_SIZE must be at least AQL_MINSTACK"
#endif

#if AQL_EXTRA_STACK < 5
#error "AQL_EXTRA_STACK must be at least 5 for safety"
#endif

#if AQL_MAXCCALLS < 50
#error "AQL_MAXCCALLS must be at least 50 for practical use"
#endif

/*
** Backward Compatibility Macros
** (for existing code that uses the old names)
*/
#define BASIC_STACK_SIZE            AQL_BASIC_STACK_SIZE
#define EXTRA_STACK                 AQL_EXTRA_STACK
#define AQLAI_MAXSTACK              AQL_MAXSTACK_SIZE
#define AQLAI_MAXCCALLS             AQL_MAXCCALLS

#endif /* astack_config_h */
