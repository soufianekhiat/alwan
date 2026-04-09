/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 74: HSPLog color model
 * HSP with logarithmic saturation stretching: S_log = log10(1 + 9*S).
 * No published specification exists; see alwan_types.h for details.
 */

#include "test_common.h"
#include <math.h>

/* ----------------------------------------------------------------
 * HSPLog tests
 * ---------------------------------------------------------------- */

/* Test HSPLog forward: S_log = log10(1 + 9*S), H and P identical to HSP */
static int test_hsplog_forward_known_values(void) {
    /* White: S=0 -> S_log = log10(1) = 0, P = 1.0 */
    {
        alwan_rgb_f64 white = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};
        alwan_hsplog_f64 hsplog;
        int status = alwan_rgb_to_hsplog_f64(&hsplog, &white);
        TEST_ASSERT(status == ALWAN_OK, "HSPLog forward white status");
        TEST_ASSERT(ALWAN_ABS(hsplog.p - ALWAN_LITERAL(1.0)) < ALWAN_TEST_TOLERANCE, "white P should be 1.0");
        TEST_ASSERT(ALWAN_ABS(hsplog.s) < ALWAN_TEST_TOLERANCE, "white S_log should be 0.0");
    }

    /* Black: P = 0, S=0 -> S_log = 0 */
    {
        alwan_rgb_f64 black = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
        alwan_hsplog_f64 hsplog;
        int status = alwan_rgb_to_hsplog_f64(&hsplog, &black);
        TEST_ASSERT(status == ALWAN_OK, "HSPLog forward black status");
        TEST_ASSERT(ALWAN_ABS(hsplog.p) < ALWAN_TEST_TOLERANCE, "black P should be 0.0");
    }

    /* Pure red: S=1.0 -> S_log = log10(1+9) = log10(10) = 1.0 */
    {
        alwan_rgb_f64 red = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
        alwan_hsplog_f64 hsplog;
        int status = alwan_rgb_to_hsplog_f64(&hsplog, &red);
        TEST_ASSERT(status == ALWAN_OK, "HSPLog forward red status");
        TEST_ASSERT(ALWAN_ABS(hsplog.s - ALWAN_LITERAL(1.0)) < ALWAN_TEST_TOLERANCE,
                    "fully saturated S_log should be 1.0");
        alwan_f64 expected_p = ALWAN_SQRT(ALWAN_LUMA_KR_BT601);
        TEST_ASSERT(ALWAN_ABS(hsplog.p - expected_p) < ALWAN_TEST_TOLERANCE, "red P mismatch");
    }

    /* 50% gray: S=0 -> S_log = 0 */
    {
        alwan_rgb_f64 gray = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)};
        alwan_hsplog_f64 hsplog;
        int status = alwan_rgb_to_hsplog_f64(&hsplog, &gray);
        TEST_ASSERT(status == ALWAN_OK, "HSPLog forward gray status");
        TEST_ASSERT(ALWAN_ABS(hsplog.s) < ALWAN_TEST_TOLERANCE, "gray S_log should be 0.0");
    }

    TEST_PASS("hsplog_forward_known_values");
}

/* Test HSPLog log saturation formula: S_log = log10(1 + 9*S) */
static int test_hsplog_log_saturation(void) {
    /* Compare HSPLog saturation against HSP saturation with manual log10 */
    alwan_rgb_f64 test_colors[] = {
        {ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.2)},
        {ALWAN_LITERAL(0.4), ALWAN_LITERAL(0.9), ALWAN_LITERAL(0.1)},
        {ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.8)},
        {ALWAN_LITERAL(0.6), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.5)},
    };
    size_t const num_colors = sizeof(test_colors) / sizeof(test_colors[0]);

    for (size_t i = 0; i < num_colors; i++) {
        alwan_hsp_f64 hsp;
        alwan_hsplog_f64 hsplog;
        alwan_rgb_to_hsp_f64(&hsp, &test_colors[i]);
        alwan_rgb_to_hsplog_f64(&hsplog, &test_colors[i]);

        /* H and P must be identical */
        TEST_ASSERT(ALWAN_ABS(hsplog.h - hsp.h) < ALWAN_TEST_TOLERANCE, "HSPLog H matches HSP H");
        TEST_ASSERT(ALWAN_ABS(hsplog.p - hsp.p) < ALWAN_TEST_TOLERANCE, "HSPLog P matches HSP P");

        /* S_log = log10(1 + 9*S) */
        alwan_f64 expected_s_log = ALWAN_LOG10(ALWAN_LITERAL(1.0) + ALWAN_LITERAL(9.0) * hsp.s);
        TEST_ASSERT(ALWAN_ABS(hsplog.s - expected_s_log) < ALWAN_TEST_TOLERANCE,
                    "HSPLog S_log = log10(1 + 9*S)");
    }

    TEST_PASS("hsplog_log_saturation");
}

/* Test HSPLog log expansion: low saturation values are expanded */
static int test_hsplog_expansion(void) {
    /* For low S values, S_log should be > S (expansion)
     * For S=0: S_log=0 (no change)
     * For S=1: S_log=1 (no change)
     * For 0 < S < 1: S_log > S (expansion) */

    alwan_f64 test_s[] = {
        ALWAN_LITERAL(0.01), ALWAN_LITERAL(0.05), ALWAN_LITERAL(0.1),
        ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.7),
    };
    size_t const n = sizeof(test_s) / sizeof(test_s[0]);

    for (size_t i = 0; i < n; i++) {
        alwan_f64 s = test_s[i];
        alwan_f64 s_log = ALWAN_LOG10(ALWAN_LITERAL(1.0) + ALWAN_LITERAL(9.0) * s);
        TEST_ASSERT(s_log > s, "log10 saturation expands interior values");
    }

    /* Boundary: S=0 -> S_log=0, S=1 -> S_log=1 */
    {
        alwan_f64 s0 = ALWAN_LOG10(ALWAN_LITERAL(1.0) + ALWAN_LITERAL(9.0) * ALWAN_LITERAL(0.0));
        TEST_ASSERT(ALWAN_ABS(s0) < ALWAN_TEST_TOLERANCE, "S=0 maps to S_log=0");
        alwan_f64 s1 = ALWAN_LOG10(ALWAN_LITERAL(1.0) + ALWAN_LITERAL(9.0) * ALWAN_LITERAL(1.0));
        TEST_ASSERT(ALWAN_ABS(s1 - ALWAN_LITERAL(1.0)) < ALWAN_TEST_TOLERANCE, "S=1 maps to S_log=1");
    }

    TEST_PASS("hsplog_expansion");
}

/* Test HSPLog round-trip: RGB -> HSPLog -> RGB */
static int test_hsplog_roundtrip(void) {
    alwan_rgb_f64 test_colors[] = {
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},   /* Red */
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)},   /* Green */
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)},   /* Blue */
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)},   /* Yellow */
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},   /* Cyan */
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)},   /* Magenta */
        {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)},   /* Gray */
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},   /* Black */
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},   /* White */
        {ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.6), ALWAN_LITERAL(0.1)},   /* Arbitrary */
        {ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.5)},   /* Arbitrary */
        {ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.9), ALWAN_LITERAL(0.4)},   /* Arbitrary */
    };
    size_t const num_colors = sizeof(test_colors) / sizeof(test_colors[0]);

    alwan_f64 const tol = ALWAN_LITERAL(1e-6);

    for (size_t i = 0; i < num_colors; i++) {
        alwan_hsplog_f64 hsplog;
        alwan_rgb_f64 roundtrip;
        int s1 = alwan_rgb_to_hsplog_f64(&hsplog, &test_colors[i]);
        int s2 = alwan_hsplog_to_rgb_f64(&roundtrip, &hsplog);
        TEST_ASSERT(s1 == ALWAN_OK && s2 == ALWAN_OK, "HSPLog round-trip status");

        alwan_f64 dr = ALWAN_ABS(roundtrip.r - test_colors[i].r);
        alwan_f64 dg = ALWAN_ABS(roundtrip.g - test_colors[i].g);
        alwan_f64 db = ALWAN_ABS(roundtrip.b - test_colors[i].b);

        if (dr > tol || dg > tol || db > tol) {
            printf("  HSPLog round-trip failed for color %zu:\n", i);
            printf("    Input:  [%.8f, %.8f, %.8f]\n", test_colors[i].r, test_colors[i].g, test_colors[i].b);
            printf("    HSPLog: [%.8f, %.8f, %.8f]\n", hsplog.h, hsplog.s, hsplog.p);
            printf("    Output: [%.8f, %.8f, %.8f]\n", roundtrip.r, roundtrip.g, roundtrip.b);
            printf("    Diff:   [%e, %e, %e]\n", dr, dg, db);
            TEST_ASSERT(0, "HSPLog round-trip error exceeds tolerance");
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("hsplog_roundtrip");
}

/* Test HSPLog map vs per-pixel consistency */
static int test_hsplog_map_consistency(void) {
    size_t const N = 512;
    alwan_f64 rgb_buf[512 * 3];
    alwan_f64 hsplog_map[512 * 3];
    size_t const stride = 3 * sizeof(alwan_f64);

    for (size_t i = 0; i < N; i++) {
        rgb_buf[i * 3 + 0] = (alwan_f64)i / (alwan_f64)(N - 1);
        rgb_buf[i * 3 + 1] = ALWAN_LITERAL(1.0) - (alwan_f64)i / (alwan_f64)(N - 1);
        rgb_buf[i * 3 + 2] = (alwan_f64)((i * 7) % N) / (alwan_f64)(N - 1);
    }

    int status = alwan_rgb_to_hsplog_f64_map_interleave(hsplog_map, rgb_buf, N, stride, stride);
    TEST_ASSERT(status == ALWAN_OK, "HSPLog map status");

    /* SIMD uses FMA and different op ordering vs scalar: float32 ~1e-7 diffs */
    alwan_f64 const tol = ALWAN_LITERAL(1e-6);
    for (size_t i = 0; i < N; i++) {
        alwan_rgb_f64 rgb = {rgb_buf[i * 3], rgb_buf[i * 3 + 1], rgb_buf[i * 3 + 2]};
        alwan_hsplog_f64 hsplog_pixel;
        alwan_rgb_to_hsplog_f64(&hsplog_pixel, &rgb);

        alwan_f64 dh = ALWAN_ABS(hsplog_map[i * 3 + 0] - hsplog_pixel.h);
        alwan_f64 ds = ALWAN_ABS(hsplog_map[i * 3 + 1] - hsplog_pixel.s);
        alwan_f64 dp = ALWAN_ABS(hsplog_map[i * 3 + 2] - hsplog_pixel.p);
        if (dh > tol || ds > tol || dp > tol) {
            printf("  HSPLog map/pixel mismatch at %zu: diff [%e, %e, %e]\n", i, dh, ds, dp);
            TEST_ASSERT(0, "HSPLog map vs per-pixel mismatch");
        }
    }

    printf("  Tested %zu pixels\n", N);
    TEST_PASS("hsplog_map_consistency");
}

/* Test HSPLog null pointer rejection */
static int test_hsplog_null_rejection(void) {
    alwan_rgb_f64 rgb = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)};
    alwan_hsplog_f64 hsplog = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.5)};

    TEST_ASSERT(alwan_rgb_to_hsplog_f64(NULL, &rgb) == ALWAN_E_INVALID, "hsplog null out");
    TEST_ASSERT(alwan_rgb_to_hsplog_f64(&hsplog, NULL) == ALWAN_E_INVALID, "hsplog null in");
    TEST_ASSERT(alwan_hsplog_to_rgb_f64(NULL, &hsplog) == ALWAN_E_INVALID, "hsplog->rgb null out");
    TEST_ASSERT(alwan_hsplog_to_rgb_f64(&rgb, NULL) == ALWAN_E_INVALID, "hsplog->rgb null in");

    TEST_PASS("hsplog_null_rejection");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

typedef int (*test_fn)(void);

typedef struct {
    char const *name;
    test_fn fn;
} test_entry;

static test_entry const tests[] = {
    {"hsplog_forward_known_values", test_hsplog_forward_known_values},
    {"hsplog_log_saturation", test_hsplog_log_saturation},
    {"hsplog_expansion", test_hsplog_expansion},
    {"hsplog_roundtrip", test_hsplog_roundtrip},
    {"hsplog_map_consistency", test_hsplog_map_consistency},
    {"hsplog_null_rejection", test_hsplog_null_rejection},
};

int test_74_hsplog_main(void) {
    printf("=== HSPLog Color Model Tests ===\n");

    int failed = 0;
    int passed = 0;
    size_t const num_tests = sizeof(tests) / sizeof(tests[0]);

    for (size_t i = 0; i < num_tests; i++) {
        int result = tests[i].fn();
        if (result == 0) {
            passed++;
        } else {
            failed++;
            printf("[FAIL] Test '%s' failed\n", tests[i].name);
        }
    }

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed (out of %zu)\n", passed, failed, num_tests);
    printf("========================================\n");

    return (failed > 0) ? 1 : 0;
}
