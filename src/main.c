/*
** $Id: main.c $
** Main entry point for AQL interpreter (MVP version)
** See Copyright Notice in aql.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aql.h"
#include "aobject.h"
#include "astate.h"
#include "avm.h"
#include "asimpleparser.h"

/*
** Print usage information
*/
static void print_usage(const char *progname) {
    printf("AQL Expression Calculator (MVP version)\n");
    printf("Usage: %s [options] [file]\n\n", progname);
    printf("Options:\n");
    printf("  -h, --help     Show this help message\n");
    printf("  -v, --version  Show version information\n");
    printf("  -i, --interactive  Enter interactive mode (default if no file)\n");
    printf("  -e <expr>      Evaluate expression directly\n\n");
    printf("Examples:\n");
    printf("  %s                    # Interactive mode\n", progname);
    printf("  %s script.aql         # Execute file\n", progname);
    printf("  %s -e \"2 + 3 * 4\"     # Evaluate expression\n", progname);
    printf("  %s -i script.aql      # Execute file then enter interactive mode\n", progname);
}

/*
** Print version information
*/
static void print_version(void) {
    printf("AQL Expression Calculator (MVP) version %s\n", AQL_VERSION);
    printf("Built with arithmetic operations, bitwise operations, and basic parsing.\n");
}

/*
** Test mode - run comprehensive arithmetic tests
*/
static int run_tests(aql_State *L) {
    printf("Running comprehensive VM arithmetic test...\n");
    
    const char *test_expressions[] = {
        "42 + 24",
        "100 - 25", 
        "7 * 8",
        "84 / 12",
        "17 % 5",
        "15 & 7",
        "8 | 4", 
        "5 << 2",
        "20 >> 2",
        "-42",
        "3.14 + 2.86",
        "(5 + 3) * 2",
        "2 ** 3",
        "~15",
        "10 ^ 7",
        NULL
    };
    
    for (int i = 0; test_expressions[i]; i++) {
        printf("Testing: %s\n", test_expressions[i]);
        if (aqlP_parse_expression(L, test_expressions[i])) {
            if (aql_gettop(L) > 0) {
                if (aql_isinteger(L, -1)) {
                    printf("  Result: %lld\n", aql_tointeger(L, -1));
                } else if (aql_isnumber(L, -1)) {
                    printf("  Result: %.6g\n", (double)aql_tonumber(L, -1));
                }
                aql_pop(L, 1);
            }
        } else {
            printf("  Error parsing expression\n");
            return 0;
        }
    }
    
    printf("All arithmetic tests completed successfully!\n");
    return 1;
}

/*
** Simple allocator for testing
*/
static void *test_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud; (void)osize;  /* not used */
    if (nsize == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, nsize);
}

/*
** Main entry point
*/
int main(int argc, char *argv[]) {
    const char *progname = argv[0];
    int interactive = 0;
    int run_test = 0;
    const char *filename = NULL;
    const char *expression = NULL;
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(progname);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interactive") == 0) {
            interactive = 1;
        } else if (strcmp(argv[i], "--test") == 0) {
            run_test = 1;
        } else if (strcmp(argv[i], "-e") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: -e requires an expression\n");
                return 1;
            }
            expression = argv[++i];
        } else if (argv[i][0] != '-') {
            if (filename) {
                fprintf(stderr, "Error: Multiple files specified\n");
                return 1;
            }
            filename = argv[i];
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_usage(progname);
            return 1;
        }
    }
    
    /* Create AQL state */
    aql_State *L = aql_newstate(test_alloc, NULL);
    if (L == NULL) {
        fprintf(stderr, "Error: Failed to create AQL state\n");
        return 1;
    }
    
    int result = 0;
    
    /* Execute based on command line options */
    if (run_test) {
        /* Run internal tests */
        result = run_tests(L) ? 0 : 1;
    } else if (expression) {
        /* Evaluate single expression */
        printf("Evaluating: %s\n", expression);
        if (aqlP_parse_expression(L, expression)) {
            if (aql_gettop(L) > 0) {
                if (aql_isinteger(L, -1)) {
                    printf("Result: %lld\n", aql_tointeger(L, -1));
                } else if (aql_isnumber(L, -1)) {
                    printf("Result: %.6g\n", (double)aql_tonumber(L, -1));
                } else {
                    printf("Result: (unknown type)\n");
                }
                aql_pop(L, 1);
            }
        } else {
            fprintf(stderr, "Error: Failed to parse expression\n");
            result = 1;
        }
    } else if (filename) {
        /* Execute file */
        if (!aqlP_execute_file(L, filename)) {
            result = 1;
        }
        
        /* Enter interactive mode if requested */
        if (interactive) {
            printf("\nEntering interactive mode...\n");
            aqlP_repl(L);
        }
    } else {
        /* Default: interactive mode */
        aqlP_repl(L);
    }
    
    /* Clean up */
    aql_close(L);
    return result;
}