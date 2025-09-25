#ifndef AQL_TEST_ASSERTION_H
#define AQL_TEST_ASSERTION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Test result tracking */
extern int test_passed;
extern int test_failed;
extern int test_total;
extern char current_test_name[256];
extern char current_suite_name[256];
extern int suite_header_printed;

/* Colors for output */
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_RESET   "\033[0m"

/* Test macros */
#define TEST_SUITE(name) \
    do { \
        snprintf(current_suite_name, sizeof(current_suite_name), "%s", name); \
        if (!suite_header_printed) { \
            printf("=== SUITE %s\n", name); \
            suite_header_printed = 1; \
        } \
    } while(0)

#define TEST_CASE(name) \
    do { \
        snprintf(current_test_name, sizeof(current_test_name), "%s", name); \
        printf("=== RUN   %s/%s\n", current_suite_name, name); \
        test_total++; \
    } while(0)

#define TEST_PASS() \
    do { \
        test_passed++; \
        printf(COLOR_GREEN "--- PASS: %s/%s" COLOR_RESET "\n", current_suite_name, current_test_name); \
    } while(0)

#define TEST_FAIL(msg, ...) \
    do { \
        test_failed++; \
        printf(COLOR_RED "--- FAIL: %s/%s" COLOR_RESET "\n", current_suite_name, current_test_name); \
        printf("    " msg "\n", ##__VA_ARGS__); \
    } while(0)

/* Basic assertions */
#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            TEST_FAIL("Expected true, got false: %s", #expr); \
            return 0; \
        } \
    } while(0)

#define ASSERT_FALSE(expr) \
    do { \
        if (expr) { \
            TEST_FAIL("Expected false, got true: %s", #expr); \
            return 0; \
        } \
    } while(0)

#define ASSERT_EQ_INT(a, b) \
    do { \
        int _a = (a); \
        int _b = (b); \
        if (_a != _b) { \
            TEST_FAIL("Expected %d, got %d", _b, _a); \
            return 0; \
        } \
    } while(0)

#define ASSERT_NE_INT(a, b) \
    do { \
        int _a = (a); \
        int _b = (b); \
        if (_a == _b) { \
            TEST_FAIL("Expected not %d, but got %d", _b, _a); \
            return 0; \
        } \
    } while(0)

#define ASSERT_EQ_STR(a, b) \
    do { \
        const char* _a = (a); \
        const char* _b = (b); \
        if (!_a || !_b || strcmp(_a, _b) != 0) { \
            TEST_FAIL("Expected \"%s\", got \"%s\"", _b ? _b : "(null)", _a ? _a : "(null)"); \
            return 0; \
        } \
    } while(0)

#define ASSERT_NE_STR(a, b) \
    do { \
        const char* _a = (a); \
        const char* _b = (b); \
        if (_a && _b && strcmp(_a, _b) == 0) { \
            TEST_FAIL("Expected not \"%s\", but got \"%s\"", _b, _a); \
            return 0; \
        } \
    } while(0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            TEST_FAIL("Expected NULL, got %p", (void*)(ptr)); \
            return 0; \
        } \
    } while(0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            TEST_FAIL("Expected non-NULL, got NULL"); \
            return 0; \
        } \
    } while(0)

/* AQL-specific assertions */
#define ASSERT_AQL_OUTPUT(code, expected) \
    do { \
        char* output = run_aql_code(code); \
        if (!output || strcmp(output, expected) != 0) { \
            TEST_FAIL("AQL output mismatch:\n  Code: %s\n  Expected: \"%s\"\n  Got: \"%s\"", \
                     code, expected, output ? output : "(null)"); \
            if (output) free(output); \
            return 0; \
        } \
        if (output) free(output); \
    } while(0)

#define ASSERT_AQL_ERROR(code, error_type) \
    do { \
        int result = run_aql_code_expect_error(code); \
        if (result != error_type) { \
            TEST_FAIL("AQL error mismatch:\n  Code: %s\n  Expected error: %d\n  Got: %d", \
                     code, error_type, result); \
            return 0; \
        } \
    } while(0)

/* Test function signature */
typedef int (*test_func_t)(void);

/* Test registration */
typedef struct test_case {
    const char* name;
    test_func_t func;
    struct test_case* next;
} test_case_t;

typedef struct test_suite {
    const char* name;
    test_case_t* cases;
    struct test_suite* next;
} test_suite_t;

/* Test registration functions */
void register_test_suite(const char* suite_name);
void register_test_case(const char* suite_name, const char* case_name, test_func_t func);
void run_all_tests(void);
void print_test_summary(void);

/* Utility functions */
char* run_aql_code(const char* code);
char* run_aql_code_with_binary(const char* code, const char* aql_binary);
int run_aql_code_expect_error(const char* code);
int run_aql_code_expect_error_with_binary(const char* code, const char* aql_binary);
char* read_file_content(const char* filename);
int compare_files(const char* file1, const char* file2);

#endif /* AQL_TEST_ASSERTION_H */
