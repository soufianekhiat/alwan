/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for ACES 2.0 components:
 *   - RGB_to_JMh20 and JMh_to_RGB20 (roundtrip)
 *   - TonescaleCompress20
 *   - GamutCompress20
 * Reference data generated from OpenColorIO 2.5.0
 */

#include "test_common.h"
#include <stdlib.h>

/* ============================================================================
 * RGB to JMh20 Test Data (from OCIO BuiltinTransform)
 * ============================================================================ */

/* RGB input values (AP1 primaries) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const rgb_to_jmh_input[][3] = {
#include "reference_values/aces_ff_test_rgb_input.csv"
};
ALWAN_DIAG_POP
#define NUM_RGB_TO_JMH (sizeof(rgb_to_jmh_input) / sizeof(rgb_to_jmh_input[0]))

/* Expected JMh values from OCIO */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const rgb_to_jmh_expected[][3] = {
#include "reference_values/aces_rgb_to_jmh20_output.csv"
};
ALWAN_DIAG_POP

/* ============================================================================
 * TonescaleCompress20 Test Data at 1000 nits
 * ============================================================================ */

/* Same RGB input as above */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const tonescale_input[][3] = {
#include "reference_values/aces_ff_test_rgb_input.csv"
};
ALWAN_DIAG_POP
#define NUM_TONESCALE (sizeof(tonescale_input) / sizeof(tonescale_input[0]))

/* Expected tonescale output from OCIO at 1000 nits */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const tonescale_expected_1000[][3] = {
#include "reference_values/aces_tonescale20_1000_output.csv"
};
ALWAN_DIAG_POP

/* ============================================================================
 * GamutCompress20 Test Data at 1000 nits
 * ============================================================================ */

/* Expected GamutCompress output from OCIO at 1000 nits (RGB domain) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const gamut_compress_expected_1000[][3] = {
#include "reference_values/aces_gamut_compress20_1000_output.csv"
};
ALWAN_DIAG_POP
#define NUM_GAMUT_COMPRESS (sizeof(gamut_compress_expected_1000) / sizeof(gamut_compress_expected_1000[0]))

/* ============================================================================
 * RGB to JMh20 Tests
 * ============================================================================ */

static int test_rgb_to_jmh20(void) {
    printf("  Testing RGB to JMh20...\n");

    /* Initialize AP1 primaries */
    alwan_aces_primaries primaries;
    alwan_aces_primaries_ap1_default(&primaries);

    int passed = 0;
    int skipped = 0;

    for (size_t i = 0; i < NUM_RGB_TO_JMH; i++) {
        alwan_rgb rgb_in = {rgb_to_jmh_input[i][0], rgb_to_jmh_input[i][1], rgb_to_jmh_input[i][2]};
        alwan_vec3 jmh_out;

        int result = alwan_aces_rgb_to_jmh20(&jmh_out, &rgb_in, &primaries);
        if (result != 0) {
            printf("FAIL: alwan_aces_rgb_to_jmh20 returned error %d for input [%zu]\n", result, i);
            return 1;
        }

        /* Skip NaN expected values (black point edge cases) */
        if (rgb_to_jmh_expected[i][0] == 0.0 && rgb_to_jmh_expected[i][1] == 0.0 &&
            rgb_to_jmh_expected[i][2] == 0.0 && rgb_in.r == 0.0 && rgb_in.g == 0.0 && rgb_in.b == 0.0) {
            /* Black input should give black output */
            if (jmh_out.v[0] == 0.0 && jmh_out.v[1] == 0.0 && jmh_out.v[2] == 0.0) {
                passed++;
                continue;
            }
        }

        /* Check J component */
        char msg[256];
        snprintf(msg, sizeof(msg), "RGB_to_JMh20 [%zu] J: RGB=(%.3f, %.3f, %.3f)",
                 i, (alwan_scalar)rgb_in.r, (alwan_scalar)rgb_in.g, (alwan_scalar)rgb_in.b);

        /* Use absolute tolerance for small values, relative for large */
        if (ALWAN_ABS(rgb_to_jmh_expected[i][0]) > ALWAN_LITERAL(1.0)) {
            TEST_ASSERT_REL(jmh_out.v[0], rgb_to_jmh_expected[i][0], ALWAN_TEST_TOLERANCE, msg);
        } else {
            TEST_ASSERT_ABS(jmh_out.v[0], rgb_to_jmh_expected[i][0], ALWAN_TEST_TOLERANCE, msg);
        }

        /* Check M component */
        snprintf(msg, sizeof(msg), "RGB_to_JMh20 [%zu] M: RGB=(%.3f, %.3f, %.3f)",
                 i, (alwan_scalar)rgb_in.r, (alwan_scalar)rgb_in.g, (alwan_scalar)rgb_in.b);
        if (ALWAN_ABS(rgb_to_jmh_expected[i][1]) > ALWAN_LITERAL(1.0)) {
            TEST_ASSERT_REL(jmh_out.v[1], rgb_to_jmh_expected[i][1], ALWAN_TEST_TOLERANCE, msg);
        } else {
            TEST_ASSERT_ABS(jmh_out.v[1], rgb_to_jmh_expected[i][1], ALWAN_TEST_TOLERANCE, msg);
        }

        /* Check h component (hue in degrees) */
        /* Skip hue check if M is near zero (achromatic - hue is undefined) */
        if (rgb_to_jmh_expected[i][1] > ALWAN_LITERAL(0.01)) {
            snprintf(msg, sizeof(msg), "RGB_to_JMh20 [%zu] h: RGB=(%.3f, %.3f, %.3f)",
                     i, (alwan_scalar)rgb_in.r, (alwan_scalar)rgb_in.g, (alwan_scalar)rgb_in.b);
            /* Handle hue wraparound at 0/360 boundary */
            alwan_scalar h_diff = ALWAN_ABS(jmh_out.v[2] - rgb_to_jmh_expected[i][2]);
            if (h_diff > ALWAN_LITERAL(180.0)) {
                h_diff = ALWAN_LITERAL(360.0) - h_diff;
            }
            if (h_diff > ALWAN_TEST_TOLERANCE) {
                printf("FAIL: %s\n  Expected: %.2f, Got: %.2f, Diff: %.2f\n",
                       msg, (alwan_scalar)rgb_to_jmh_expected[i][2], (alwan_scalar)jmh_out.v[2], (alwan_scalar)h_diff);
                return 1;
            }
        }

        passed++;
    }

    printf("    PASS: %d RGB_to_JMh20 tests (skipped %d)\n", passed, skipped);
    return 0;
}

/* ============================================================================
 * TonescaleCompress20 Tests
 *
 * Tests the complete RGB -> JMh -> TonescaleCompress -> JMh -> RGB pipeline.
 * Reference data is generated from OCIO 2.5.0 using the chained transforms:
 *   ACES_RGB_TO_JMH_20 -> ACES_TONESCALE_COMPRESS_20 -> ACES_RGB_TO_JMH_20^-1
 *
 * ============================================================================ */

static int test_tonescale_compress20(void) {
    printf("  Testing TonescaleCompress20 at 1000 nits...\n");

    alwan_scalar const PEAK_LUMINANCE = 1000.0;
    int passed = 0;
    int failed = 0;

    for (size_t i = 0; i < NUM_TONESCALE; i++) {
        alwan_rgb rgb_in = {tonescale_input[i][0], tonescale_input[i][1], tonescale_input[i][2]};
        alwan_rgb rgb_out;

        int result = alwan_aces_tonescale_compress20(&rgb_out, &rgb_in, PEAK_LUMINANCE);
        if (result != 0) {
            printf("FAIL: alwan_aces_tonescale_compress20 returned error %d for input [%zu]\n", result, i);
            failed++;
            continue;
        }

        /* Compare against OCIO reference */
        alwan_scalar exp_r = tonescale_expected_1000[i][0];
        alwan_scalar exp_g = tonescale_expected_1000[i][1];
        alwan_scalar exp_b = tonescale_expected_1000[i][2];

        alwan_scalar diff_r = ALWAN_ABS(rgb_out.r - exp_r);
        alwan_scalar diff_g = ALWAN_ABS(rgb_out.g - exp_g);
        alwan_scalar diff_b = ALWAN_ABS(rgb_out.b - exp_b);

        if (diff_r > ALWAN_TEST_TOLERANCE || diff_g > ALWAN_TEST_TOLERANCE || diff_b > ALWAN_TEST_TOLERANCE) {
            printf("FAIL [%zu]: in=(%.4f,%.4f,%.4f) exp=(%.4f,%.4f,%.4f) got=(%.4f,%.4f,%.4f)\n",
                   i, (alwan_scalar)rgb_in.r, (alwan_scalar)rgb_in.g, (alwan_scalar)rgb_in.b,
                   (alwan_scalar)exp_r, (alwan_scalar)exp_g, (alwan_scalar)exp_b,
                   (alwan_scalar)rgb_out.r, (alwan_scalar)rgb_out.g, (alwan_scalar)rgb_out.b);
            failed++;
        } else {
            passed++;
        }
    }

    printf("    TonescaleCompress20: %d passed, %d failed\n", passed, failed);
    return (failed > 0) ? 1 : 0;
}

/* ============================================================================
 * JMh Roundtrip Tests (RGB -> JMh -> RGB)
 *
 * Tests that JMh_to_RGB20 is the proper inverse of RGB_to_JMh20.
 * ============================================================================ */


static int test_jmh_roundtrip(void) {
    printf("  Testing JMh roundtrip (RGB -> JMh -> RGB)...\n");

    alwan_aces_primaries primaries;
    alwan_aces_primaries_ap1_default(&primaries);

    int passed = 0;
    int failed = 0;

    for (size_t i = 0; i < NUM_RGB_TO_JMH; i++) {
        alwan_rgb rgb_in = {rgb_to_jmh_input[i][0], rgb_to_jmh_input[i][1], rgb_to_jmh_input[i][2]};
        alwan_vec3 jmh;
        alwan_rgb rgb_out;

        /* Forward: RGB -> JMh */
        int result = alwan_aces_rgb_to_jmh20(&jmh, &rgb_in, &primaries);
        if (result != 0) {
            printf("FAIL: rgb_to_jmh20 error %d for input [%zu]\n", result, i);
            failed++;
            continue;
        }

        /* Inverse: JMh -> RGB */
        result = alwan_aces_jmh_to_rgb20(&rgb_out, &jmh, &primaries);
        if (result != 0) {
            printf("FAIL: jmh_to_rgb20 error %d for input [%zu]\n", result, i);
            failed++;
            continue;
        }

        /* Compare roundtrip result with original */
        alwan_scalar diff_r = ALWAN_ABS(rgb_out.r - rgb_in.r);
        alwan_scalar diff_g = ALWAN_ABS(rgb_out.g - rgb_in.g);
        alwan_scalar diff_b = ALWAN_ABS(rgb_out.b - rgb_in.b);

        if (diff_r > ALWAN_TEST_TOLERANCE || diff_g > ALWAN_TEST_TOLERANCE || diff_b > ALWAN_TEST_TOLERANCE) {
            printf("FAIL [%zu]: in=(%.6f,%.6f,%.6f) out=(%.6f,%.6f,%.6f) diff=(%.2e,%.2e,%.2e)\n",
                   i, (alwan_scalar)rgb_in.r, (alwan_scalar)rgb_in.g, (alwan_scalar)rgb_in.b,
                   (alwan_scalar)rgb_out.r, (alwan_scalar)rgb_out.g, (alwan_scalar)rgb_out.b,
                   (alwan_scalar)diff_r, (alwan_scalar)diff_g, (alwan_scalar)diff_b);
            failed++;
        } else {
            passed++;
        }
    }

    printf("    JMh roundtrip: %d passed, %d failed\n", passed, failed);
    return (failed > 0) ? 1 : 0;
}

/* ============================================================================
 * GamutCompress20 Tests
 *
 * Tests the complete RGB -> JMh -> GamutCompress -> JMh -> RGB pipeline.
 * For AP1 limit primaries (wide gamut), colors pass through unchanged.
 * ============================================================================ */


static int test_gamut_compress20(void) {
    printf("  Testing GamutCompress20 at 1000 nits...\n");

    alwan_scalar const PEAK_LUMINANCE = 1000.0;
    alwan_aces_primaries primaries;
    alwan_aces_primaries_ap1_default(&primaries);

    int passed = 0;
    int failed = 0;

    for (size_t i = 0; i < NUM_GAMUT_COMPRESS; i++) {
        alwan_rgb rgb_in = {rgb_to_jmh_input[i][0], rgb_to_jmh_input[i][1], rgb_to_jmh_input[i][2]};

        /* Convert RGB -> JMh */
        alwan_vec3 jmh_in;
        int result = alwan_aces_rgb_to_jmh20(&jmh_in, &rgb_in, &primaries);
        if (result != 0) {
            printf("FAIL: rgb_to_jmh20 error %d for input [%zu]\n", result, i);
            failed++;
            continue;
        }

        /* Apply GamutCompress */
        alwan_vec3 jmh_out;
        result = alwan_aces_gamut_compress20(&jmh_out, &jmh_in, PEAK_LUMINANCE, &primaries);
        if (result != 0) {
            printf("FAIL: gamut_compress20 error %d for input [%zu]\n", result, i);
            failed++;
            continue;
        }

        /* Convert JMh -> RGB */
        alwan_rgb rgb_out;
        result = alwan_aces_jmh_to_rgb20(&rgb_out, &jmh_out, &primaries);
        if (result != 0) {
            printf("FAIL: jmh_to_rgb20 error %d for input [%zu]\n", result, i);
            failed++;
            continue;
        }

        /* Compare against OCIO reference */
        alwan_scalar exp_r = gamut_compress_expected_1000[i][0];
        alwan_scalar exp_g = gamut_compress_expected_1000[i][1];
        alwan_scalar exp_b = gamut_compress_expected_1000[i][2];

        alwan_scalar diff_r = ALWAN_ABS(rgb_out.r - exp_r);
        alwan_scalar diff_g = ALWAN_ABS(rgb_out.g - exp_g);
        alwan_scalar diff_b = ALWAN_ABS(rgb_out.b - exp_b);

        if (diff_r > ALWAN_TEST_TOLERANCE || diff_g > ALWAN_TEST_TOLERANCE || diff_b > ALWAN_TEST_TOLERANCE) {
            printf("FAIL [%zu]: in=(%.4f,%.4f,%.4f) exp=(%.4f,%.4f,%.4f) got=(%.4f,%.4f,%.4f)\n",
                   i, (alwan_scalar)rgb_in.r, (alwan_scalar)rgb_in.g, (alwan_scalar)rgb_in.b,
                   (alwan_scalar)exp_r, (alwan_scalar)exp_g, (alwan_scalar)exp_b,
                   (alwan_scalar)rgb_out.r, (alwan_scalar)rgb_out.g, (alwan_scalar)rgb_out.b);
            failed++;
        } else {
            passed++;
        }
    }

    printf("    GamutCompress20: %d passed, %d failed\n", passed, failed);
    return (failed > 0) ? 1 : 0;
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int test_54_aces20_main(void) {
    printf("ACES 2.0 Component Tests\n");
    int failures = 0;

    if (test_rgb_to_jmh20() != 0) failures++;
    if (test_jmh_roundtrip() != 0) failures++;
    if (test_tonescale_compress20() != 0) failures++;
    if (test_gamut_compress20() != 0) failures++;

    if (failures == 0) {
        printf("All ACES 2.0 tests PASSED!\n");
    } else {
        printf("ACES 2.0 tests: %d test(s) FAILED\n", failures);
    }

    return failures;
}
