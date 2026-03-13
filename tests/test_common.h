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
#include <stdint.h>

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

/* Default relative tolerance for numerical comparisons.
 * Looser than ALWAN_TEST_TOLERANCE — suitable for multi-step pipelines
 * (LUT sampling, video encode/decode, CLF roundtrips) where accumulated
 * floating-point error is expected. */
#if ALWAN_SCALAR_IS_FLOAT
#  define TEST_REL_EPSILON ALWAN_LITERAL(1e-5)
#else
#  define TEST_REL_EPSILON ALWAN_LITERAL(1e-10)
#endif

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

/* ============================================================================
 * D65 White Point (Y=1 normalized)
 * ============================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const g_d65_xyz_y1[] = {
#include "reference_values/test_d65_white.csv"
};
ALWAN_DIAG_POP

/* ============================================================================
 * Pixel Format Helpers
 * ============================================================================ */

static alwan_pixel_format const TEST_PIXEL_FMTS[4] = {
    ALWAN_PIXEL_U8, ALWAN_PIXEL_U16, ALWAN_PIXEL_F32, ALWAN_PIXEL_F64
};

static inline char const *test_fmt_name(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return "U8";
    case ALWAN_PIXEL_U16: return "U16";
    case ALWAN_PIXEL_F32: return "F32";
    case ALWAN_PIXEL_F64: return "F64";
    }
    return "?";
}

static inline size_t test_fmt_elem_size(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return sizeof(uint8_t);
    case ALWAN_PIXEL_U16: return sizeof(uint16_t);
    case ALWAN_PIXEL_F32: return sizeof(float);
    case ALWAN_PIXEL_F64: return sizeof(double);
    }
    return 0;
}

/* Scatter: alwan_scalar -> typed (single channel, clamped to [0,1] for int) */
static inline void test_scatter1(void *out, alwan_pixel_format fmt,
                                  alwan_scalar const *in, size_t count) {
    for (size_t i = 0; i < count; i++) {
        alwan_scalar v = in[i];
        switch (fmt) {
        case ALWAN_PIXEL_U8:  ((uint8_t *)out)[i]  = (v < 0) ? 0 : (v > 1) ? 255
                                : (uint8_t)(v * 255 + 0.5); break;
        case ALWAN_PIXEL_U16: ((uint16_t *)out)[i] = (v < 0) ? 0 : (v > 1) ? 65535
                                : (uint16_t)(v * 65535 + 0.5); break;
        case ALWAN_PIXEL_F32: ((float *)out)[i]    = (float)v; break;
        case ALWAN_PIXEL_F64: ((double *)out)[i]   = (double)v; break;
        }
    }
}

/* Collect: typed -> alwan_scalar (single channel) */
static inline void test_collect1(alwan_scalar *out, void const *in,
                                  alwan_pixel_format fmt, size_t count) {
    for (size_t i = 0; i < count; i++) {
        switch (fmt) {
        case ALWAN_PIXEL_U8:  out[i] = (alwan_scalar)((uint8_t const *)in)[i]
                                / ALWAN_LITERAL(255.0); break;
        case ALWAN_PIXEL_U16: out[i] = (alwan_scalar)((uint16_t const *)in)[i]
                                / ALWAN_LITERAL(65535.0); break;
        case ALWAN_PIXEL_F32: out[i] = (alwan_scalar)((float const *)in)[i]; break;
        case ALWAN_PIXEL_F64: out[i] = (alwan_scalar)((double const *)in)[i]; break;
        }
    }
}

#endif /* ALWAN_TEST_COMMON_H */
