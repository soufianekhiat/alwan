/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 73: HSP and HSY color models
 * References:
 *   HSP: Darel Rex Finley (2006), http://alienryderflex.com/hsp.html
 *   HSY: Kuzma Shapran "HCY" (chilliant.com); Krita KoColorConversions.cpp
 */

#include "test_common.h"

/* ----------------------------------------------------------------
 * HSP tests
 * ---------------------------------------------------------------- */

/* Test HSP forward: P = sqrt(Pr*R^2 + Pg*G^2 + Pb*B^2) with BT.601 weights */
static int test_hsp_forward_known_values(void) {
    /* White: P = sqrt(0.299 + 0.587 + 0.114) = 1.0 */
    {
        alwan_rgb white = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};
        alwan_hsp hsp;
        int status = alwan_rgb_to_hsp(&hsp, &white);
        TEST_ASSERT(status == ALWAN_OK, "HSP forward white status");
        TEST_ASSERT(ALWAN_ABS(hsp.p - ALWAN_LITERAL(1.0)) < ALWAN_TEST_TOLERANCE, "white P should be 1.0");
        TEST_ASSERT(ALWAN_ABS(hsp.s) < ALWAN_TEST_TOLERANCE, "white S should be 0.0");
    }

    /* Black: P = 0 */
    {
        alwan_rgb black = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
        alwan_hsp hsp;
        int status = alwan_rgb_to_hsp(&hsp, &black);
        TEST_ASSERT(status == ALWAN_OK, "HSP forward black status");
        TEST_ASSERT(ALWAN_ABS(hsp.p) < ALWAN_TEST_TOLERANCE, "black P should be 0.0");
    }

    /* Pure red: P = sqrt(0.299) */
    {
        alwan_rgb red = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
        alwan_hsp hsp;
        int status = alwan_rgb_to_hsp(&hsp, &red);
        TEST_ASSERT(status == ALWAN_OK, "HSP forward red status");
        alwan_scalar expected_p = ALWAN_SQRT(ALWAN_LUMA_KR_BT601);
        TEST_ASSERT(ALWAN_ABS(hsp.p - expected_p) < ALWAN_TEST_TOLERANCE, "red P mismatch");
        TEST_ASSERT(ALWAN_ABS(hsp.s - ALWAN_LITERAL(1.0)) < ALWAN_TEST_TOLERANCE, "red S should be 1.0");
        TEST_ASSERT(ALWAN_ABS(hsp.h) < ALWAN_TEST_TOLERANCE, "red H should be 0.0");
    }

    /* Pure green: P = sqrt(0.587) */
    {
        alwan_rgb green = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)};
        alwan_hsp hsp;
        int status = alwan_rgb_to_hsp(&hsp, &green);
        TEST_ASSERT(status == ALWAN_OK, "HSP forward green status");
        alwan_scalar expected_p = ALWAN_SQRT(ALWAN_LUMA_KG_BT601);
        TEST_ASSERT(ALWAN_ABS(hsp.p - expected_p) < ALWAN_TEST_TOLERANCE, "green P mismatch");
    }

    /* Pure blue: P = sqrt(0.114) */
    {
        alwan_rgb blue = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)};
        alwan_hsp hsp;
        int status = alwan_rgb_to_hsp(&hsp, &blue);
        TEST_ASSERT(status == ALWAN_OK, "HSP forward blue status");
        alwan_scalar expected_p = ALWAN_SQRT(ALWAN_LUMA_KB_BT601);
        TEST_ASSERT(ALWAN_ABS(hsp.p - expected_p) < ALWAN_TEST_TOLERANCE, "blue P mismatch");
    }

    /* 50% gray: P = sqrt(0.299*0.25 + 0.587*0.25 + 0.114*0.25) = 0.5 */
    {
        alwan_rgb gray = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)};
        alwan_hsp hsp;
        int status = alwan_rgb_to_hsp(&hsp, &gray);
        TEST_ASSERT(status == ALWAN_OK, "HSP forward gray status");
        TEST_ASSERT(ALWAN_ABS(hsp.p - ALWAN_LITERAL(0.5)) < ALWAN_TEST_TOLERANCE, "gray P should be 0.5");
        TEST_ASSERT(ALWAN_ABS(hsp.s) < ALWAN_TEST_TOLERANCE, "gray S should be 0.0");
    }

    TEST_PASS("hsp_forward_known_values");
}

/* Test HSP round-trip: RGB -> HSP -> RGB */
static int test_hsp_roundtrip(void) {
    alwan_rgb test_colors[] = {
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

    alwan_scalar const tol = ALWAN_LITERAL(1e-6);

    for (size_t i = 0; i < num_colors; i++) {
        alwan_hsp hsp;
        alwan_rgb roundtrip;
        int s1 = alwan_rgb_to_hsp(&hsp, &test_colors[i]);
        int s2 = alwan_hsp_to_rgb(&roundtrip, &hsp);
        TEST_ASSERT(s1 == ALWAN_OK && s2 == ALWAN_OK, "HSP round-trip status");

        alwan_scalar dr = ALWAN_ABS(roundtrip.r - test_colors[i].r);
        alwan_scalar dg = ALWAN_ABS(roundtrip.g - test_colors[i].g);
        alwan_scalar db = ALWAN_ABS(roundtrip.b - test_colors[i].b);

        if (dr > tol || dg > tol || db > tol) {
            printf("  HSP round-trip failed for color %zu:\n", i);
            printf("    Input:  [%.8f, %.8f, %.8f]\n", test_colors[i].r, test_colors[i].g, test_colors[i].b);
            printf("    HSP:    [%.8f, %.8f, %.8f]\n", hsp.h, hsp.s, hsp.p);
            printf("    Output: [%.8f, %.8f, %.8f]\n", roundtrip.r, roundtrip.g, roundtrip.b);
            printf("    Diff:   [%e, %e, %e]\n", dr, dg, db);
            TEST_ASSERT(0, "HSP round-trip error exceeds tolerance");
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("hsp_roundtrip");
}

/* Test HSP map vs per-pixel consistency */
static int test_hsp_map_consistency(void) {
    size_t const N = 512;
    alwan_scalar rgb_buf[512 * 3];
    alwan_scalar hsp_map[512 * 3];
    size_t const stride = 3 * sizeof(alwan_scalar);

    /* Generate test colors */
    for (size_t i = 0; i < N; i++) {
        rgb_buf[i * 3 + 0] = (alwan_scalar)i / (alwan_scalar)(N - 1);
        rgb_buf[i * 3 + 1] = ALWAN_LITERAL(1.0) - (alwan_scalar)i / (alwan_scalar)(N - 1);
        rgb_buf[i * 3 + 2] = (alwan_scalar)((i * 7) % N) / (alwan_scalar)(N - 1);
    }

    int status = alwan_rgb_to_hsp_map_interleave(hsp_map, rgb_buf, N, stride, stride);
    TEST_ASSERT(status == ALWAN_OK, "HSP map status");

    alwan_scalar const tol = ALWAN_LITERAL(1e-10);
    for (size_t i = 0; i < N; i++) {
        alwan_rgb rgb = {rgb_buf[i * 3], rgb_buf[i * 3 + 1], rgb_buf[i * 3 + 2]};
        alwan_hsp hsp_pixel;
        alwan_rgb_to_hsp(&hsp_pixel, &rgb);

        alwan_scalar dh = ALWAN_ABS(hsp_map[i * 3 + 0] - hsp_pixel.h);
        alwan_scalar ds = ALWAN_ABS(hsp_map[i * 3 + 1] - hsp_pixel.s);
        alwan_scalar dp = ALWAN_ABS(hsp_map[i * 3 + 2] - hsp_pixel.p);
        if (dh > tol || ds > tol || dp > tol) {
            printf("  HSP map/pixel mismatch at %zu: diff [%e, %e, %e]\n", i, dh, ds, dp);
            TEST_ASSERT(0, "HSP map vs per-pixel mismatch");
        }
    }

    printf("  Tested %zu pixels\n", N);
    TEST_PASS("hsp_map_consistency");
}

/* Test HSP null pointer rejection */
static int test_hsp_null_rejection(void) {
    alwan_rgb rgb = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)};
    alwan_hsp hsp = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.5)};

    TEST_ASSERT(alwan_rgb_to_hsp(NULL, &rgb) == ALWAN_E_INVALID, "hsp null out");
    TEST_ASSERT(alwan_rgb_to_hsp(&hsp, NULL) == ALWAN_E_INVALID, "hsp null in");
    TEST_ASSERT(alwan_hsp_to_rgb(NULL, &hsp) == ALWAN_E_INVALID, "hsp->rgb null out");
    TEST_ASSERT(alwan_hsp_to_rgb(&rgb, NULL) == ALWAN_E_INVALID, "hsp->rgb null in");

    TEST_PASS("hsp_null_rejection");
}

/* ----------------------------------------------------------------
 * HSY tests
 * ---------------------------------------------------------------- */

/* Test HSY forward: Y = kr*R + kg*G + kb*B (BT.601) */
static int test_hsy_forward_known_values(void) {
    alwan_scalar const kr = ALWAN_LUMA_KR_BT601;
    alwan_scalar const kg = ALWAN_LUMA_KG_BT601;
    alwan_scalar const kb = ALWAN_LUMA_KB_BT601;

    /* White: Y = 1.0 */
    {
        alwan_rgb white = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};
        alwan_hsy hsy;
        int status = alwan_rgb_to_hsy(&hsy, &white);
        TEST_ASSERT(status == ALWAN_OK, "HSY forward white status");
        TEST_ASSERT(ALWAN_ABS(hsy.y - ALWAN_LITERAL(1.0)) < ALWAN_TEST_TOLERANCE, "white Y should be 1.0");
        TEST_ASSERT(ALWAN_ABS(hsy.s) < ALWAN_TEST_TOLERANCE, "white S should be 0.0");
    }

    /* Black: Y = 0 */
    {
        alwan_rgb black = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
        alwan_hsy hsy;
        int status = alwan_rgb_to_hsy(&hsy, &black);
        TEST_ASSERT(status == ALWAN_OK, "HSY forward black status");
        TEST_ASSERT(ALWAN_ABS(hsy.y) < ALWAN_TEST_TOLERANCE, "black Y should be 0.0");
    }

    /* Pure red: Y = kr = 0.299 */
    {
        alwan_rgb red = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
        alwan_hsy hsy;
        int status = alwan_rgb_to_hsy(&hsy, &red);
        TEST_ASSERT(status == ALWAN_OK, "HSY forward red status");
        TEST_ASSERT(ALWAN_ABS(hsy.y - kr) < ALWAN_TEST_TOLERANCE, "red Y should be kr");
    }

    /* Pure green: Y = kg = 0.587 */
    {
        alwan_rgb green = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)};
        alwan_hsy hsy;
        int status = alwan_rgb_to_hsy(&hsy, &green);
        TEST_ASSERT(status == ALWAN_OK, "HSY forward green status");
        TEST_ASSERT(ALWAN_ABS(hsy.y - kg) < ALWAN_TEST_TOLERANCE, "green Y should be kg");
    }

    /* Pure blue: Y = kb = 0.114 */
    {
        alwan_rgb blue = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)};
        alwan_hsy hsy;
        int status = alwan_rgb_to_hsy(&hsy, &blue);
        TEST_ASSERT(status == ALWAN_OK, "HSY forward blue status");
        TEST_ASSERT(ALWAN_ABS(hsy.y - kb) < ALWAN_TEST_TOLERANCE, "blue Y should be kb");
    }

    /* 50% gray: Y = 0.5 */
    {
        alwan_rgb gray = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)};
        alwan_hsy hsy;
        int status = alwan_rgb_to_hsy(&hsy, &gray);
        TEST_ASSERT(status == ALWAN_OK, "HSY forward gray status");
        TEST_ASSERT(ALWAN_ABS(hsy.y - ALWAN_LITERAL(0.5)) < ALWAN_TEST_TOLERANCE, "gray Y should be 0.5");
    }

    TEST_PASS("hsy_forward_known_values");
}

/* Test HSY round-trip: RGB -> HSY -> RGB */
static int test_hsy_roundtrip(void) {
    alwan_rgb test_colors[] = {
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

    alwan_scalar const tol = ALWAN_LITERAL(1e-6);

    for (size_t i = 0; i < num_colors; i++) {
        alwan_hsy hsy;
        alwan_rgb roundtrip;
        int s1 = alwan_rgb_to_hsy(&hsy, &test_colors[i]);
        int s2 = alwan_hsy_to_rgb(&roundtrip, &hsy);
        TEST_ASSERT(s1 == ALWAN_OK && s2 == ALWAN_OK, "HSY round-trip status");

        alwan_scalar dr = ALWAN_ABS(roundtrip.r - test_colors[i].r);
        alwan_scalar dg = ALWAN_ABS(roundtrip.g - test_colors[i].g);
        alwan_scalar db = ALWAN_ABS(roundtrip.b - test_colors[i].b);

        if (dr > tol || dg > tol || db > tol) {
            printf("  HSY round-trip failed for color %zu:\n", i);
            printf("    Input:  [%.8f, %.8f, %.8f]\n", test_colors[i].r, test_colors[i].g, test_colors[i].b);
            printf("    HSY:    [%.8f, %.8f, %.8f]\n", hsy.h, hsy.s, hsy.y);
            printf("    Output: [%.8f, %.8f, %.8f]\n", roundtrip.r, roundtrip.g, roundtrip.b);
            printf("    Diff:   [%e, %e, %e]\n", dr, dg, db);
            TEST_ASSERT(0, "HSY round-trip error exceeds tolerance");
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("hsy_roundtrip");
}

/* Test HSY luma preservation: after round-trip, luma must match */
static int test_hsy_luma_preservation(void) {
    alwan_scalar const kr = ALWAN_LUMA_KR_BT601;
    alwan_scalar const kg = ALWAN_LUMA_KG_BT601;
    alwan_scalar const kb = ALWAN_LUMA_KB_BT601;

    alwan_rgb test_colors[] = {
        {ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.6), ALWAN_LITERAL(0.1)},
        {ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.5)},
        {ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.9), ALWAN_LITERAL(0.4)},
        {ALWAN_LITERAL(0.7), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.9)},
    };
    size_t const num_colors = sizeof(test_colors) / sizeof(test_colors[0]);

    for (size_t i = 0; i < num_colors; i++) {
        alwan_hsy hsy;
        alwan_rgb_to_hsy(&hsy, &test_colors[i]);

        /* Y component should equal BT.601 luma */
        alwan_scalar expected_y = kr * test_colors[i].r + kg * test_colors[i].g + kb * test_colors[i].b;
        TEST_ASSERT(ALWAN_ABS(hsy.y - expected_y) < ALWAN_TEST_TOLERANCE, "HSY luma matches BT.601");

        /* After inverse, the luma of the reconstructed RGB should still match */
        alwan_rgb rt;
        alwan_hsy_to_rgb(&rt, &hsy);
        alwan_scalar rt_y = kr * rt.r + kg * rt.g + kb * rt.b;
        TEST_ASSERT(ALWAN_ABS(rt_y - hsy.y) < ALWAN_LITERAL(1e-6), "HSY luma preserved after round-trip");
    }

    TEST_PASS("hsy_luma_preservation");
}

/* Test HSY map vs per-pixel consistency */
static int test_hsy_map_consistency(void) {
    size_t const N = 512;
    alwan_scalar rgb_buf[512 * 3];
    alwan_scalar hsy_map[512 * 3];
    size_t const stride = 3 * sizeof(alwan_scalar);

    for (size_t i = 0; i < N; i++) {
        rgb_buf[i * 3 + 0] = (alwan_scalar)i / (alwan_scalar)(N - 1);
        rgb_buf[i * 3 + 1] = ALWAN_LITERAL(1.0) - (alwan_scalar)i / (alwan_scalar)(N - 1);
        rgb_buf[i * 3 + 2] = (alwan_scalar)((i * 7) % N) / (alwan_scalar)(N - 1);
    }

    int status = alwan_rgb_to_hsy_map_interleave(hsy_map, rgb_buf, N, stride, stride);
    TEST_ASSERT(status == ALWAN_OK, "HSY map status");

    alwan_scalar const tol = ALWAN_LITERAL(1e-10);
    for (size_t i = 0; i < N; i++) {
        alwan_rgb rgb = {rgb_buf[i * 3], rgb_buf[i * 3 + 1], rgb_buf[i * 3 + 2]};
        alwan_hsy hsy_pixel;
        alwan_rgb_to_hsy(&hsy_pixel, &rgb);

        alwan_scalar dh = ALWAN_ABS(hsy_map[i * 3 + 0] - hsy_pixel.h);
        alwan_scalar ds = ALWAN_ABS(hsy_map[i * 3 + 1] - hsy_pixel.s);
        alwan_scalar dy = ALWAN_ABS(hsy_map[i * 3 + 2] - hsy_pixel.y);
        if (dh > tol || ds > tol || dy > tol) {
            printf("  HSY map/pixel mismatch at %zu: diff [%e, %e, %e]\n", i, dh, ds, dy);
            TEST_ASSERT(0, "HSY map vs per-pixel mismatch");
        }
    }

    printf("  Tested %zu pixels\n", N);
    TEST_PASS("hsy_map_consistency");
}

/* Test HSY null pointer rejection */
static int test_hsy_null_rejection(void) {
    alwan_rgb rgb = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)};
    alwan_hsy hsy = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.5)};

    TEST_ASSERT(alwan_rgb_to_hsy(NULL, &rgb) == ALWAN_E_INVALID, "hsy null out");
    TEST_ASSERT(alwan_rgb_to_hsy(&hsy, NULL) == ALWAN_E_INVALID, "hsy null in");
    TEST_ASSERT(alwan_hsy_to_rgb(NULL, &hsy) == ALWAN_E_INVALID, "hsy->rgb null out");
    TEST_ASSERT(alwan_hsy_to_rgb(&rgb, NULL) == ALWAN_E_INVALID, "hsy->rgb null in");

    TEST_PASS("hsy_null_rejection");
}

/* Test HSP H/S match HSV H/S */
static int test_hsp_hs_matches_hsv(void) {
    alwan_rgb test_colors[] = {
        {ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.5)},
        {ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.9), ALWAN_LITERAL(0.4)},
        {ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.7)},
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},
    };
    size_t const num_colors = sizeof(test_colors) / sizeof(test_colors[0]);

    for (size_t i = 0; i < num_colors; i++) {
        alwan_hsv hsv;
        alwan_hsp hsp;
        alwan_rgb_to_hsv(&hsv, &test_colors[i]);
        alwan_rgb_to_hsp(&hsp, &test_colors[i]);

        TEST_ASSERT(ALWAN_ABS(hsp.h - hsv.h) < ALWAN_TEST_TOLERANCE, "HSP H matches HSV H");
        TEST_ASSERT(ALWAN_ABS(hsp.s - hsv.s) < ALWAN_TEST_TOLERANCE, "HSP S matches HSV S");
    }

    TEST_PASS("hsp_hs_matches_hsv");
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
    {"hsp_forward_known_values", test_hsp_forward_known_values},
    {"hsp_roundtrip", test_hsp_roundtrip},
    {"hsp_map_consistency", test_hsp_map_consistency},
    {"hsp_null_rejection", test_hsp_null_rejection},
    {"hsp_hs_matches_hsv", test_hsp_hs_matches_hsv},
    {"hsy_forward_known_values", test_hsy_forward_known_values},
    {"hsy_roundtrip", test_hsy_roundtrip},
    {"hsy_luma_preservation", test_hsy_luma_preservation},
    {"hsy_map_consistency", test_hsy_map_consistency},
    {"hsy_null_rejection", test_hsy_null_rejection},
};

int test_73_hsp_hsy_main(void) {
    printf("=== HSP/HSY Color Model Tests ===\n");

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
