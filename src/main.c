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
#include "aapi.h"
#include "aparser.h"
#include "arepl.h"
#include "ajit.h"
#include "adebug_user.h"


/*
** Print usage information
*/
static void print_usage(const char *progname) {
    printf("AQL Expression Calculator (MVP version) with JIT\n");
    printf("Usage: %s [options] [file]\n\n", progname);
    printf("Options:\n");
    printf("  -h, --help     Show this help message\n");
    printf("  --version      Show version information\n");
    printf("  -i, --interactive  Enter interactive mode (default if no file)\n");
    printf("  -e <expr>      Evaluate expression directly\n");
    printf("  --test         Run comprehensive arithmetic tests\n\n");
    printf("Debug Options:\n");
    printf("  -v             详细模式 (词法+ AST +字节码 + 执行跟踪)\n");
    printf("  -vb            只输出字节码 (类似 luac -l)\n");
    printf("  -vast          只输出 AST\n");
    printf("  -vt            输出执行跟踪\n");
    printf("  -vl            输出词流\n");
    printf("  -vd            详细日志输出 (print_debug激活+ -v模式)\n");
    printf("  -compare       与 Lua 字节码对比 (需要指定 .lua 文件)\n\n");
    printf("JIT Options:\n");
    printf("  --jit-auto     Enable automatic JIT compilation (default)\n");
    printf("  --jit-off      Disable JIT compilation\n");
    printf("  --jit-force    Force JIT compilation for all functions\n");
    printf("  --jit-stats    Show JIT statistics after execution\n\n");
    printf("Examples:\n");
    printf("  %s script.aql         # 执行文件\n", progname);
    printf("  %s -vb script.aql     # 只输出字节码 (类似 luac -l)\n", progname);
    printf("  %s -v script.aql      # 详细模式 (词法+AST+字节码+执行跟踪)\n", progname);
    printf("  %s -vast script.aql   # 只输出 AST\n", progname);
    printf("  %s -vt script.aql     # 输出执行跟踪\n", progname);
    printf("  %s -vl script.aql     # 输出词流\n", progname);
    printf("  %s -vd script.aql     # 详细日志输出\n", progname);
    printf("  %s -e \"2 + 3 * 4\"     # 计算表达式\n", progname);
}

/*
** Print version information
*/
static void print_version(void) {
    printf("AQL Expression Calculator (MVP) version %s\n", AQL_VERSION);
    printf("Built with arithmetic operations, bitwise operations, and basic parsing.\n");
}

static int get_execution_result(aql_State *L, TValue **result) {
    if (!L || !L->ci || !L->ci->func.p || !result) {
        return 0;
    }

    if (L->top.p <= L->ci->func.p + 1) {
        return 0;
    }

    *result = s2v(L->top.p - 1);
    return (*result != NULL);
}

static void clear_execution_stack(aql_State *L) {
    if (L && L->ci && L->ci->func.p) {
        L->top.p = L->ci->func.p + 1;
    }
}

static int execute_expression(aql_State *L, const char *expr, const char *chunkname) {
    size_t expr_len;
    size_t chunk_len;
    char *chunk;
    int status;

    if (!L || !expr) {
        return 0;
    }

    expr_len = strlen(expr);
    chunk_len = expr_len + 9;  /* "return " + ";" + '\0' */
    chunk = (char *)malloc(chunk_len);
    if (!chunk) {
        return 0;
    }

    snprintf(chunk, chunk_len, "return %s;", expr);
    status = aqlP_compile_string(L, chunk, strlen(chunk), chunkname);
    free(chunk);

    if (status != 0) {
        return 0;
    }

    return (aqlP_execute_compiled(L, 0, 1) == 1);
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
        TValue *result = NULL;

        printf("Testing: %s\n", test_expressions[i]);
        if (!execute_expression(L, test_expressions[i], "=(test)")) {
            printf("  Error executing expression\n");
            return 0;
        }

        if (get_execution_result(L, &result)) {
            printf("  Result: ");
            aqlP_print_value(result);
            printf("\n");
        }

        clear_execution_stack(L);
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
    
    /* JIT configuration */
    int jit_mode = 1;  // 0=off, 1=auto, 2=force, 3=stats
    int show_jit_stats = 0;
    
    /* Debug configuration */
    int debug_flags = AQL_DEBUG_NONE;
    
    /* Early exit configuration */
    int stop_after_lex = 0;
    int stop_after_parse = 0;
    int stop_after_compile = 0;
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(progname);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-v") == 0) {
            // 详细模式 (词法+ AST +字节码 + 执行跟踪)
            debug_flags = AQL_DEBUG_LEX | AQL_DEBUG_PARSE | AQL_DEBUG_CODE | AQL_DEBUG_VM;
        } else if (strcmp(argv[i], "-vb") == 0) {
            // 只输出字节码 (类似 luac -l)
            debug_flags = AQL_DEBUG_CODE;
        } else if (strcmp(argv[i], "-vast") == 0) {
            // 只输出 AST
            debug_flags = AQL_DEBUG_PARSE;
        } else if (strcmp(argv[i], "-vt") == 0) {
            // 输出执行跟踪
            debug_flags = AQL_DEBUG_VM;
        } else if (strcmp(argv[i], "-vl") == 0) {
            // 输出词流
            debug_flags = AQL_DEBUG_LEX;
        } else if (strcmp(argv[i], "-vd") == 0) {
            // 详细日志输出 (print_debug激活+ -v模式)
            debug_flags = AQL_DEBUG_ALL;
        } else if (strcmp(argv[i], "-compare") == 0) {
            // 与 Lua 字节码对比 (暂时标记，后续实现)
            printf("Lua bytecode comparison not yet implemented\n");
            return 1;
        } else if (strcmp(argv[i], "-st") == 0) {
            debug_flags |= AQL_DEBUG_LEX;
            stop_after_lex = 1;
        } else if (strcmp(argv[i], "-sa") == 0) {
            debug_flags |= AQL_DEBUG_LEX | AQL_DEBUG_PARSE;
            stop_after_parse = 1;
        } else if (strcmp(argv[i], "-sb") == 0) {
            debug_flags |= AQL_DEBUG_LEX | AQL_DEBUG_PARSE | AQL_DEBUG_CODE;
            stop_after_compile = 1;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interactive") == 0) {
            interactive = 1;
        } else if (strcmp(argv[i], "--test") == 0) {
            run_test = 1;
        } else if (strcmp(argv[i], "--jit-auto") == 0) {
            jit_mode = 1;
        } else if (strcmp(argv[i], "--jit-off") == 0) {
            jit_mode = 0;
        } else if (strcmp(argv[i], "--jit-force") == 0) {
            jit_mode = 2;
        } else if (strcmp(argv[i], "--jit-stats") == 0) {
            show_jit_stats = 1;
            jit_mode = 1;
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
    
    /* Initialize debug system */
    aqlD_init_debug();
    aqlD_set_debug_flags(debug_flags);
    
    /* Initialize JIT if enabled */
    #if AQL_USE_JIT
    if (jit_mode > 0) {
        if (aqlJIT_init(L, JIT_BACKEND_NATIVE) == JIT_ERROR_NONE) {
            //TODO: Add a message to the user that JIT is enabled
            //printf("🚀 AQL JIT enabled\n");
        } else {
            fprintf(stderr, "Warning: JIT initialization failed\n");
            jit_mode = 0;
        }
    }
    #else
    if (jit_mode > 0) {
        fprintf(stderr, "Warning: JIT not available in this build\n");
        jit_mode = 0;
    }
    #endif
    
    int result = 0;
    
    /* Execute based on command line options */
    if (run_test) {
        /* Run internal tests */
        result = run_tests(L) ? 0 : 1;
    } else if (expression) {
        TValue *result_value = NULL;

        /* Evaluate single expression */
        printf("Evaluating: %s\n", expression);
        if (execute_expression(L, expression, "=(command line)")) {
            if (get_execution_result(L, &result_value)) {
                printf("Result: ");
                aqlP_print_value(result_value);
                printf("\n");
            } else {
                printf("Result: (no value)\n");
            }
            clear_execution_stack(L);
        } else {
            fprintf(stderr, "Error: Failed to execute expression\n");
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
            aqlREPL_run(L);
        }
    } else {
        /* Default: interactive mode */
        aqlREPL_run(L);
    }
    
    /* Show JIT statistics if requested */
    if (show_jit_stats && jit_mode > 0) {
        printf("\n=== JIT Performance Report ===\n");
        #if AQL_USE_JIT
        aqlJIT_print_performance_report(L);
        #endif
    }
    
    /* Clean up JIT */
    #if AQL_USE_JIT
    aqlJIT_close(L);
    #endif
    
    /* Clean up */
    aql_close(L);
    return result;
}
