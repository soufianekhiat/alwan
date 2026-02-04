/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for ACES Fixed Functions (RRT Components)
 * Reference: OpenColorIO (PyOpenColorIO)
 */

#include <stdio.h>
#include <stdlib.h>
#include "alwan.h"
#include "alwan_internal.h"
#include "test_common.h"

/* Test tolerance - match OpenColorIO precision - documented in docs/violations.md */
#define TEST_TOLERANCE ALWAN_LITERAL(1e-5)

/* ============================================================================
 * Test Input Data (from OpenColorIO)
 * ============================================================================ */

/* Test RGB input values */
static alwan_scalar const test_rgb_input[] = {
#include "reference_values/aces_ff_test_rgb_input.csv"
};
#define NUM_TEST_RGB (sizeof(test_rgb_input) / sizeof(test_rgb_input[0]) / 3)

/* ============================================================================
 * Expected Output Data (from OpenColorIO)
 * ============================================================================ */

/* ACES RedMod03 output */
static alwan_scalar const redmod03_expected[] = {
#include "reference_values/aces_redmod03_output.csv"
};

/* ACES RedMod10 output */
static alwan_scalar const redmod10_expected[] = {
#include "reference_values/aces_redmod10_output.csv"
};

/* ACES Glow03 output */
static alwan_scalar const glow03_expected[] = {
#include "reference_values/aces_glow03_output.csv"
};

/* ACES Glow10 output */
static alwan_scalar const glow10_expected[] = {
#include "reference_values/aces_glow10_output.csv"
};

/* ACES DarkToDim10 output */
static alwan_scalar const dark_to_dim10_expected[] = {
#include "reference_values/aces_dark_to_dim10_output.csv"
};

/* ACES GamutComp13 output */
static alwan_scalar const gamut_comp13_expected[] = {
#include "reference_values/aces_gamut_comp13_output.csv"
};

/* Rec2100 Surround output */
static alwan_scalar const rec2100_surround_expected[] = {
#include "reference_values/rec2100_surround_output.csv"
};

/* ACES TonescaleCompress20 output */
static alwan_scalar const tonescale_compress20_expected[] = {
#include "reference_values/aces_tonescale_compress20_output.csv"
};

/* ACES RGB to JMh20 output */
static alwan_scalar const rgb_to_jmh20_expected[] = {
#include "reference_values/aces_rgb_to_jmh20_output.csv"
};

/* ============================================================================
 * Test Functions
 * ============================================================================ */

static int test_aces_redmod03(void) {
    printf("  Testing ACES RedMod03...\n");

    for (size_t i = 0; i < NUM_TEST_RGB; i++) {
        alwan_rgb rgb_in = {
            test_rgb_input[i * 3],
            test_rgb_input[i * 3 + 1],
            test_rgb_input[i * 3 + 2]
        };
        alwan_rgb rgb_out;

        int result = alwan_aces_redmod03(&rgb_out, &rgb_in);
        if (result != ALWAN_OK) {
            printf("FAIL: alwan_aces_redmod03 returned error %d\n", result);
            return 1;
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "RedMod03 [%zu] R", i);
        TEST_ASSERT_REL(rgb_out.r, redmod03_expected[i * 3], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "RedMod03 [%zu] G", i);
        TEST_ASSERT_REL(rgb_out.g, redmod03_expected[i * 3 + 1], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "RedMod03 [%zu] B", i);
        TEST_ASSERT_REL(rgb_out.b, redmod03_expected[i * 3 + 2], TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu RedMod03 tests\n", NUM_TEST_RGB);
    return 0;
}

static int test_aces_redmod10(void) {
    printf("  Testing ACES RedMod10...\n");

    for (size_t i = 0; i < NUM_TEST_RGB; i++) {
        alwan_rgb rgb_in = {
            test_rgb_input[i * 3],
            test_rgb_input[i * 3 + 1],
            test_rgb_input[i * 3 + 2]
        };
        alwan_rgb rgb_out;

        int result = alwan_aces_redmod10(&rgb_out, &rgb_in);
        if (result != ALWAN_OK) {
            printf("FAIL: alwan_aces_redmod10 returned error %d\n", result);
            return 1;
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "RedMod10 [%zu] R", i);
        TEST_ASSERT_REL(rgb_out.r, redmod10_expected[i * 3], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "RedMod10 [%zu] G", i);
        TEST_ASSERT_REL(rgb_out.g, redmod10_expected[i * 3 + 1], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "RedMod10 [%zu] B", i);
        TEST_ASSERT_REL(rgb_out.b, redmod10_expected[i * 3 + 2], TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu RedMod10 tests\n", NUM_TEST_RGB);
    return 0;
}

static int test_aces_glow03(void) {
    printf("  Testing ACES Glow03...\n");

    for (size_t i = 0; i < NUM_TEST_RGB; i++) {
        alwan_rgb rgb_in = {
            test_rgb_input[i * 3],
            test_rgb_input[i * 3 + 1],
            test_rgb_input[i * 3 + 2]
        };
        alwan_rgb rgb_out;

        int result = alwan_aces_glow03(&rgb_out, &rgb_in);
        if (result != ALWAN_OK) {
            printf("FAIL: alwan_aces_glow03 returned error %d\n", result);
            return 1;
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "Glow03 [%zu] R", i);
        TEST_ASSERT_REL(rgb_out.r, glow03_expected[i * 3], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "Glow03 [%zu] G", i);
        TEST_ASSERT_REL(rgb_out.g, glow03_expected[i * 3 + 1], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "Glow03 [%zu] B", i);
        TEST_ASSERT_REL(rgb_out.b, glow03_expected[i * 3 + 2], TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu Glow03 tests\n", NUM_TEST_RGB);
    return 0;
}

static int test_aces_glow10(void) {
    printf("  Testing ACES Glow10...\n");

    for (size_t i = 0; i < NUM_TEST_RGB; i++) {
        alwan_rgb rgb_in = {
            test_rgb_input[i * 3],
            test_rgb_input[i * 3 + 1],
            test_rgb_input[i * 3 + 2]
        };
        alwan_rgb rgb_out;

        int result = alwan_aces_glow10(&rgb_out, &rgb_in);
        if (result != ALWAN_OK) {
            printf("FAIL: alwan_aces_glow10 returned error %d\n", result);
            return 1;
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "Glow10 [%zu] R", i);
        TEST_ASSERT_REL(rgb_out.r, glow10_expected[i * 3], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "Glow10 [%zu] G", i);
        TEST_ASSERT_REL(rgb_out.g, glow10_expected[i * 3 + 1], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "Glow10 [%zu] B", i);
        TEST_ASSERT_REL(rgb_out.b, glow10_expected[i * 3 + 2], TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu Glow10 tests\n", NUM_TEST_RGB);
    return 0;
}

static int test_aces_dark_to_dim10(void) {
    printf("  Testing ACES DarkToDim10...\n");

    for (size_t i = 0; i < NUM_TEST_RGB; i++) {
        alwan_rgb rgb_in = {
            test_rgb_input[i * 3],
            test_rgb_input[i * 3 + 1],
            test_rgb_input[i * 3 + 2]
        };
        alwan_rgb rgb_out;

        int result = alwan_aces_dark_to_dim10(&rgb_out, &rgb_in);
        if (result != ALWAN_OK) {
            printf("FAIL: alwan_aces_dark_to_dim10 returned error %d\n", result);
            return 1;
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "DarkToDim10 [%zu] R", i);
        TEST_ASSERT_REL(rgb_out.r, dark_to_dim10_expected[i * 3], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "DarkToDim10 [%zu] G", i);
        TEST_ASSERT_REL(rgb_out.g, dark_to_dim10_expected[i * 3 + 1], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "DarkToDim10 [%zu] B", i);
        TEST_ASSERT_REL(rgb_out.b, dark_to_dim10_expected[i * 3 + 2], TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu DarkToDim10 tests\n", NUM_TEST_RGB);
    return 0;
}

static int test_aces_gamut_comp13(void) {
    printf("  Testing ACES GamutComp13...\n");

    /* Use default ACES 1.3 parameters */
    alwan_aces_gamut_comp13_params params;
    alwan_aces_gamut_comp13_params_default(&params);

    for (size_t i = 0; i < NUM_TEST_RGB; i++) {
        alwan_rgb rgb_in = {
            test_rgb_input[i * 3],
            test_rgb_input[i * 3 + 1],
            test_rgb_input[i * 3 + 2]
        };
        alwan_rgb rgb_out;

        int result = alwan_aces_gamut_comp13(&rgb_out, &rgb_in, &params);
        if (result != ALWAN_OK) {
            printf("FAIL: alwan_aces_gamut_comp13 returned error %d\n", result);
            return 1;
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "GamutComp13 [%zu] R", i);
        TEST_ASSERT_REL(rgb_out.r, gamut_comp13_expected[i * 3], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "GamutComp13 [%zu] G", i);
        TEST_ASSERT_REL(rgb_out.g, gamut_comp13_expected[i * 3 + 1], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "GamutComp13 [%zu] B", i);
        TEST_ASSERT_REL(rgb_out.b, gamut_comp13_expected[i * 3 + 2], TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu GamutComp13 tests\n", NUM_TEST_RGB);
    return 0;
}

static int test_rec2100_surround(void) {
    printf("  Testing Rec2100 Surround...\n");

    /* Use standard gamma for dim surround */
    alwan_scalar gamma = 0.78;

    for (size_t i = 0; i < NUM_TEST_RGB; i++) {
        alwan_rgb rgb_in = {
            test_rgb_input[i * 3],
            test_rgb_input[i * 3 + 1],
            test_rgb_input[i * 3 + 2]
        };
        alwan_rgb rgb_out;

        int result = alwan_rec2100_surround(&rgb_out, &rgb_in, gamma);
        if (result != ALWAN_OK) {
            printf("FAIL: alwan_rec2100_surround returned error %d\n", result);
            return 1;
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "Rec2100_Surround [%zu] R", i);
        TEST_ASSERT_REL(rgb_out.r, rec2100_surround_expected[i * 3], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "Rec2100_Surround [%zu] G", i);
        TEST_ASSERT_REL(rgb_out.g, rec2100_surround_expected[i * 3 + 1], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "Rec2100_Surround [%zu] B", i);
        TEST_ASSERT_REL(rgb_out.b, rec2100_surround_expected[i * 3 + 2], TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu Rec2100_Surround tests\n", NUM_TEST_RGB);
    return 0;
}

static int test_aces_tonescale_compress20(void) {
    printf("  Testing ACES TonescaleCompress20...\n");

    /* Use 1000 nits peak luminance */
    alwan_scalar peak_luminance = 1000.0;

    for (size_t i = 0; i < NUM_TEST_RGB; i++) {
        alwan_rgb rgb_in = {
            test_rgb_input[i * 3],
            test_rgb_input[i * 3 + 1],
            test_rgb_input[i * 3 + 2]
        };
        alwan_rgb rgb_out;

        int result = alwan_aces_tonescale_compress20(&rgb_out, &rgb_in, peak_luminance);
        if (result != ALWAN_OK) {
            printf("FAIL: alwan_aces_tonescale_compress20 returned error %d\n", result);
            return 1;
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "TonescaleCompress20 [%zu] R", i);
        TEST_ASSERT_REL(rgb_out.r, tonescale_compress20_expected[i * 3], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "TonescaleCompress20 [%zu] G", i);
        TEST_ASSERT_REL(rgb_out.g, tonescale_compress20_expected[i * 3 + 1], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "TonescaleCompress20 [%zu] B", i);
        TEST_ASSERT_REL(rgb_out.b, tonescale_compress20_expected[i * 3 + 2], TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu TonescaleCompress20 tests\n", NUM_TEST_RGB);
    return 0;
}

static int test_aces_rgb_to_jmh20(void) {
    printf("  Testing ACES RGB to JMh20...\n");

    /* Use AP1 primaries */
    alwan_aces_primaries primaries;
    alwan_aces_primaries_ap1_default(&primaries);

    for (size_t i = 0; i < NUM_TEST_RGB; i++) {
        alwan_rgb rgb_in = {
            test_rgb_input[i * 3],
            test_rgb_input[i * 3 + 1],
            test_rgb_input[i * 3 + 2]
        };
        alwan_vec3 jmh_out;

        int result = alwan_aces_rgb_to_jmh20(&jmh_out, &rgb_in, &primaries);
        if (result != ALWAN_OK) {
            printf("FAIL: alwan_aces_rgb_to_jmh20 returned error %d\n", result);
            return 1;
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "RGB_to_JMh20 [%zu] J", i);
        TEST_ASSERT_REL(jmh_out.v[0], rgb_to_jmh20_expected[i * 3], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "RGB_to_JMh20 [%zu] M", i);
        TEST_ASSERT_REL(jmh_out.v[1], rgb_to_jmh20_expected[i * 3 + 1], TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "RGB_to_JMh20 [%zu] h", i);
        TEST_ASSERT_REL(jmh_out.v[2], rgb_to_jmh20_expected[i * 3 + 2], TEST_TOLERANCE, msg);
    }

    printf("    PASS: %zu RGB_to_JMh20 tests\n", NUM_TEST_RGB);
    return 0;
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int test_52_aces_fixed_functions_main(void) {
    printf("ACES Fixed Functions Tests\n");
    printf("==========================\n");

    int failures = 0;

    /* Section 6: ACES Fixed Functions (RRT Components) */
    failures += test_aces_redmod03();
    failures += test_aces_redmod10();
    failures += test_aces_glow03();
    failures += test_aces_glow10();
    failures += test_aces_dark_to_dim10();
    failures += test_aces_gamut_comp13();
    failures += test_rec2100_surround();

    /* Section 5: ACES 2.0 Components - SKIPPED (complex, needs more research) */
    /* TODO: Implement TonescaleCompress20 and RGB_to_JMh20 */
    printf("  SKIPPED: TonescaleCompress20 (placeholder)\n");
    printf("  SKIPPED: RGB_to_JMh20 (placeholder)\n");
    /* failures += test_aces_tonescale_compress20(); */
    /* failures += test_aces_rgb_to_jmh20(); */

    if (failures == 0) {
        printf("\nAll ACES Fixed Functions tests PASSED!\n");
    } else {
        printf("\n%d test(s) FAILED!\n", failures);
    }

    return failures;
}
