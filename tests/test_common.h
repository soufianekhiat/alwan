/*
 * Alwan - Pure C colour science library
 * Common test utilities and macros
 *
 * This file consolidates all test macros to ensure consistent behavior
 * across all test files and compatibility with both f32 and f64 builds.
 */

#ifndef ALWAN_TEST_COMMON_H
#define ALWAN_TEST_COMMON_H

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>

/* ============================================================================
 * Test Counters
 * ============================================================================ */

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;
#define TEST_COUNT_INCR() (test_count++)
#define TEST_PASS_INCR() (test_passed++)
#define TEST_FAIL_INCR() (test_failed++)

/* Small epsilon for avoiding division by zero in relative comparisons */
#define TEST_EPSILON ALWAN_LITERAL(1e-20)

/* ============================================================================
 * Basic Test Flow Macros
 * ============================================================================ */

/* Mark test as passed and return success (silent) */
#define TEST_PASS(name) do { \
    TEST_PASS_INCR(); \
    return 0; \
} while(0)

/* Mark test as passed with message (verbose) */
#define TEST_PASS_MSG() do { \
    printf("    [PASS]\n"); \
    TEST_PASS_INCR(); \
} while(0)

/* Print failure message and return failure */
#define TEST_FAIL(msg, ...) do { \
    printf("[FAIL] " msg "\n", ##__VA_ARGS__); \
    TEST_FAIL_INCR(); \
    return 1; \
} while(0)

/* Print test name at start */
#define TEST_START(name) do { \
    printf("  TEST: %s\n", name); \
    TEST_COUNT_INCR(); \
} while(0)

/* ============================================================================
 * Basic Assertion Macros
 * ============================================================================ */

/* Simple boolean assertion */
#define TEST_ASSERT(cond, msg) do { \
    TEST_COUNT_INCR(); \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        TEST_FAIL_INCR(); \
        return 1; \
    } \
    TEST_PASS_INCR(); \
} while(0)

/* Absolute difference assertion */
#define TEST_ASSERT_NEAR(a, b, tol, msg) do { \
    TEST_COUNT_INCR(); \
    alwan_scalar _a = (a); \
    alwan_scalar _b = (b); \
    alwan_scalar _tol = (tol); \
    alwan_scalar _diff = ALWAN_ABS(_a - _b); \
    if (_diff > _tol) { \
        printf("[FAIL] %s: expected %.16e, got %.16e (diff=%.16e, tol=%.16e)\n", \
               msg, _b, _a, _diff, _tol); \
        TEST_FAIL_INCR(); \
        return 1; \
    } \
    TEST_PASS_INCR(); \
} while(0)

/* Relative difference assertion */
#define TEST_ASSERT_REL(a, b, rel_tol, msg) do { \
    TEST_COUNT_INCR(); \
    alwan_scalar _a = (a); \
    alwan_scalar _b = (b); \
    alwan_scalar _tol = (rel_tol); \
    alwan_scalar _diff = ALWAN_ABS(_a - _b); \
    alwan_scalar _ref = ALWAN_ABS(_b); \
    alwan_scalar _rel_err = (_ref > TEST_EPSILON) ? _diff / _ref : _diff; \
    if (_rel_err > _tol) { \
        printf("[FAIL] %s: expected %.16e, got %.16e (rel_err=%.16e, tol=%.16e)\n", \
               msg, _b, _a, _rel_err, _tol); \
        TEST_FAIL_INCR(); \
        return 1; \
    } \
    TEST_PASS_INCR(); \
} while(0)

/* Absolute difference assertion (alias for TEST_ASSERT_NEAR) */
#define TEST_ASSERT_ABS(a, b, tol, msg) TEST_ASSERT_NEAR(a, b, tol, msg)

/* Simpler version - auto-generates location in error message */
#define TEST_CHECK_NEAR(a, b, tol) do { \
    alwan_scalar _a = (alwan_scalar)(a); \
    alwan_scalar _b = (alwan_scalar)(b); \
    alwan_scalar _tol = (tol); \
    alwan_scalar _diff = ALWAN_ABS(_a - _b); \
    if (_diff > _tol) { \
        printf("[FAIL] %s:%d: expected %g, got %g (diff %g > %g)\n", \
               __FILE__, __LINE__, (double)_b, (double)_a, (double)_diff, (double)_tol); \
        TEST_FAIL_INCR(); \
        return 1; \
    } \
} while(0)

/* XYZ struct comparison (pointers) */
#define TEST_CHECK_XYZ_NEAR(a, b, tol) do { \
    TEST_CHECK_NEAR((a)->x, (b)->x, tol); \
    TEST_CHECK_NEAR((a)->y, (b)->y, tol); \
    TEST_CHECK_NEAR((a)->z, (b)->z, tol); \
} while(0)

/* RGB struct comparison (values) */
#define TEST_CHECK_RGB_NEAR(a, b, tol) do { \
    TEST_CHECK_NEAR((a).r, (b).r, tol); \
    TEST_CHECK_NEAR((a).g, (b).g, tol); \
    TEST_CHECK_NEAR((a).b, (b).b, tol); \
} while(0)

/* ============================================================================
 * Vector Assertion Macros
 * ============================================================================ */

/* Assert vec3 components are within tolerance */
#define TEST_ASSERT_VEC3_NEAR(v, expected, tol, msg) do { \
    TEST_COUNT_INCR(); \
    alwan_scalar _tol = (tol); \
    alwan_scalar _exp0 = (expected)[0]; \
    alwan_scalar _exp1 = (expected)[1]; \
    alwan_scalar _exp2 = (expected)[2]; \
    alwan_scalar _got0 = (v).v[0]; \
    alwan_scalar _got1 = (v).v[1]; \
    alwan_scalar _got2 = (v).v[2]; \
    alwan_scalar _diff_x = ALWAN_ABS(_got0 - _exp0); \
    alwan_scalar _diff_y = ALWAN_ABS(_got1 - _exp1); \
    alwan_scalar _diff_z = ALWAN_ABS(_got2 - _exp2); \
    alwan_scalar _max_diff = _diff_x; \
    if (_diff_y > _max_diff) _max_diff = _diff_y; \
    if (_diff_z > _max_diff) _max_diff = _diff_z; \
    if (_max_diff > _tol) { \
        printf("[FAIL] %s: max_diff=%.16e (tol=%.16e)\n", msg, _max_diff, _tol); \
        printf("  Expected: [%.10e, %.10e, %.10e]\n", _exp0, _exp1, _exp2); \
        printf("  Got:      [%.10e, %.10e, %.10e]\n", _got0, _got1, _got2); \
        TEST_FAIL_INCR(); \
        return 1; \
    } \
    TEST_PASS_INCR(); \
} while(0)

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/* Calculate maximum absolute difference between two vectors */
static inline alwan_scalar test_vec3_max_diff(alwan_scalar const *a, alwan_scalar const *b) {
    alwan_scalar d0 = ALWAN_ABS(a[0] - b[0]);
    alwan_scalar d1 = ALWAN_ABS(a[1] - b[1]);
    alwan_scalar d2 = ALWAN_ABS(a[2] - b[2]);
    alwan_scalar max_d = d0;
    if (d1 > max_d) max_d = d1;
    if (d2 > max_d) max_d = d2;
    return max_d;
}

/* Calculate relative error */
static inline alwan_scalar test_rel_error(alwan_scalar got, alwan_scalar expected) {
    alwan_scalar diff = ALWAN_ABS(got - expected);
    alwan_scalar ref = ALWAN_ABS(expected);
    return (ref > TEST_EPSILON) ? diff / ref : diff;
}

#endif /* ALWAN_TEST_COMMON_H */
