/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 02: Data embedding - CSV inclusion with diagnostic guards
 */

#include "test_common.h"

/* ----------------------------------------------------------------
 * Embed CSV data with diagnostic guards
 * ---------------------------------------------------------------- */

/* D65 white point (x, y) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const g_d65_xy[] = {
#include "data/illuminants_xy/d65_xy.csv"
};
ALWAN_DIAG_POP

/* sRGB primaries (rx, ry, gx, gy, bx, by) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const g_srgb_primaries[] = {
#include "data/srgb_primaries_only.csv"
};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_d65_data(void) {
    /* Verify D65 white point data loads correctly from embedded CSV */

    /* Expected values from colour-science (data/illuminants_xy/d65_xy.csv) */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const expected_d65[] = {
#include "data/illuminants_xy/d65_xy.csv"
    };
    ALWAN_DIAG_POP

    alwan_scalar diff_x = ALWAN_ABS(g_d65_xy[0] - expected_d65[0]);
    alwan_scalar diff_y = ALWAN_ABS(g_d65_xy[1] - expected_d65[1]);

    printf("  D65 x: %.17g (diff %e)\n", g_d65_xy[0], diff_x);
    printf("  D65 y: %.17g (diff %e)\n", g_d65_xy[1], diff_y);

    TEST_ASSERT(diff_x < TEST_TOLERANCE, "D65 x value mismatch");
    TEST_ASSERT(diff_y < TEST_TOLERANCE, "D65 y value mismatch");

    TEST_PASS("test_d65_data");
}

static int test_srgb_primaries(void) {
    /* Verify sRGB primary chromaticities (ITU-R BT.709) */
    alwan_scalar const expected[] = {
        ALWAN_LITERAL(0.64), ALWAN_LITERAL(0.33),  /* Red */
        ALWAN_LITERAL(0.30), ALWAN_LITERAL(0.60),  /* Green */
        ALWAN_LITERAL(0.15), ALWAN_LITERAL(0.06)   /* Blue */
    };

    printf("  sRGB primaries (r, g, b):\n");
    for (int i = 0; i < 3; i++) {
        printf("    [%f, %f]\n", g_srgb_primaries[i * 2], g_srgb_primaries[i * 2 + 1]);
    }

    for (int i = 0; i < 6; i++) {
        alwan_scalar diff = ALWAN_ABS(g_srgb_primaries[i] - expected[i]);
        if (diff > 1e-5) {
            fprintf(stderr, "  Primary [%d] mismatch: %f vs %f (diff %e)\n",
                    i, g_srgb_primaries[i], expected[i], diff);
            TEST_ASSERT(0, "sRGB primary value mismatch");
        }
    }

    TEST_PASS("test_srgb_primaries");
}

static int test_array_sizes(void) {
    /* Verify expected array sizes */
    size_t d65_size = sizeof(g_d65_xy) / sizeof(g_d65_xy[0]);
    size_t srgb_size = sizeof(g_srgb_primaries) / sizeof(g_srgb_primaries[0]);

    printf("  D65 array size: %zu (expected 2)\n", d65_size);
    printf("  sRGB primaries size: %zu (expected 6)\n", srgb_size);

    TEST_ASSERT(d65_size == 2, "D65 array size incorrect");
    TEST_ASSERT(srgb_size == 6, "sRGB primaries array size incorrect");

    TEST_PASS("test_array_sizes");
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
    {"d65_data", test_d65_data},
    {"srgb_primaries", test_srgb_primaries},
    {"array_sizes", test_array_sizes},
};

int test_02_data_embed_main(void) {
    printf("Running data embedding tests...\n");
    printf("alwan_scalar type: %s\n", sizeof(alwan_scalar) == 4 ? "float" : "double");

    int failed = 0;
    int passed = 0;
    size_t const num_tests = sizeof(tests) / sizeof(tests[0]);

    for (size_t i = 0; i < num_tests; i++) {
        int result = tests[i].fn();
        if (result == 0) {
            passed++;
        } else {
            failed++;
            fprintf(stderr, "[FAIL] Test '%s' failed\n", tests[i].name);
        }
    }

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed (out of %zu)\n", passed, failed, num_tests);
    printf("========================================\n");

    return (failed > 0) ? 1 : 0;
}
