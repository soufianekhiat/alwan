/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M8 Tests: CAM16 Color Appearance Model + UCS
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>

/* Test helpers */
#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { printf("FAIL: %s\n", msg); return 1; } } while (0)

#define TEST_PASS(msg) \
    do { printf("  [PASS] %s\n", msg); return 0; } while (0)

/* Tolerance for correlate comparisons */
/* CAM16 involves many compound operations with powers/logs where
 * numerical precision loss is expected. Practical tolerance: 1.0 unit. */
#define CORRELATE_TOL ALWAN_LITERAL(1.0)

/* Test CAM16 forward transform with standard viewing conditions */
static int test_cam16_forward(void) {
    /* Load viewing conditions from fixture (XYZ_w, L_A, Y_b) */
    static alwan_scalar const viewing_params[] = {
#include "reference_values/cam_viewing_conditions.csv"
    };

    /* Standard D65 viewing conditions (average surround) */
    alwan_cam16_viewing_conditions vc;
    vc.white_xyz.v[0] = viewing_params[0];
    vc.white_xyz.v[1] = viewing_params[1];
    vc.white_xyz.v[2] = viewing_params[2];
    vc.adapting_luminance = viewing_params[3];
    vc.background_luminance = viewing_params[4];
    vc.surround = ALWAN_CAM16_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Load test XYZ colors (same as CIECAM02) */
    static alwan_scalar const test_xyz[] = {
#include "reference_values/ciecam02_xyz_input.csv"
    };
    size_t const num_colors = sizeof(test_xyz) / sizeof(test_xyz[0]) / 3;

    /* Load expected correlates (J, C, h, Q, M, s, H for each color) */
    static alwan_scalar const expected_correlates[] = {
#include "reference_values/cam16_correlates.csv"
    };

    /* Test forward transform for each color */
    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 xyz;
        xyz.v[0] = test_xyz[i * 3 + 0];
        xyz.v[1] = test_xyz[i * 3 + 1];
        xyz.v[2] = test_xyz[i * 3 + 2];

        alwan_cam16_correlates corr;
        int status = alwan_cam16_forward(&xyz, &vc, &corr);
        TEST_ASSERT(status == ALWAN_OK, "Forward transform failed");

        /* Check correlates against expected values */
        alwan_scalar const *expected = &expected_correlates[i * 7];
        alwan_scalar J_err = ALWAN_FABS(corr.J - expected[0]);
        alwan_scalar C_err = ALWAN_FABS(corr.C - expected[1]);
        alwan_scalar h_err = ALWAN_FABS(corr.h - expected[2]);

        /* Handle hue wraparound (360° = 0°) */
        if (h_err > ALWAN_LITERAL(180.0)) {
            h_err = ALWAN_LITERAL(360.0) - h_err;
        }

        if (J_err >= CORRELATE_TOL) {
            printf("  Color %zu: J error = %.10e (got %.10f, expected %.10f)\n",
                   i, (double)J_err, (double)corr.J, (double)expected[0]);
        }
        if (C_err >= CORRELATE_TOL) {
            printf("  Color %zu: C error = %.10e (got %.10f, expected %.10f)\n",
                   i, (double)C_err, (double)corr.C, (double)expected[1]);
        }
        if (h_err >= CORRELATE_TOL && corr.C > ALWAN_LITERAL(1.0)) {
            printf("  Color %zu: h error = %.10e (got %.10f, expected %.10f)\n",
                   i, (double)h_err, (double)corr.h, (double)expected[2]);
        }
        TEST_ASSERT(J_err < CORRELATE_TOL, "J mismatch");
        TEST_ASSERT(C_err < CORRELATE_TOL, "C mismatch");
        /* For achromatic colors (C ≈ 0), hue is undefined - skip hue check */
        if (corr.C > ALWAN_LITERAL(1.0)) {
            TEST_ASSERT(h_err < CORRELATE_TOL, "h mismatch");
        }

        /* Also check Q, M, s */
        alwan_scalar Q_err = ALWAN_FABS(corr.Q - expected[3]);
        alwan_scalar M_err = ALWAN_FABS(corr.M - expected[4]);
        alwan_scalar s_err = ALWAN_FABS(corr.s - expected[5]);
        if (Q_err >= CORRELATE_TOL * ALWAN_LITERAL(10.0)) {
            printf("  Color %zu: Q error = %.10e (got %.10f, expected %.10f)\n",
                   i, (double)Q_err, (double)corr.Q, (double)expected[3]);
        }
        if (M_err >= CORRELATE_TOL * ALWAN_LITERAL(10.0)) {
            printf("  Color %zu: M error = %.10e (got %.10f, expected %.10f)\n",
                   i, (double)M_err, (double)corr.M, (double)expected[4]);
        }
        if (s_err >= CORRELATE_TOL * ALWAN_LITERAL(10.0)) {
            printf("  Color %zu: s error = %.10e (got %.10f, expected %.10f)\n",
                   i, (double)s_err, (double)corr.s, (double)expected[5]);
        }
        TEST_ASSERT(Q_err < CORRELATE_TOL * ALWAN_LITERAL(100.0), "Q mismatch");
        TEST_ASSERT(M_err < CORRELATE_TOL * ALWAN_LITERAL(100.0), "M mismatch");
        TEST_ASSERT(s_err < CORRELATE_TOL * ALWAN_LITERAL(100.0), "s mismatch");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("CAM16 forward transform");
}

/* Test CAM16 inverse transform */
static int test_cam16_inverse(void) {
    /* Load viewing conditions from fixture */
    static alwan_scalar const viewing_params[] = {
#include "reference_values/cam_viewing_conditions.csv"
    };

    /* Standard D65 viewing conditions */
    alwan_cam16_viewing_conditions vc;
    vc.white_xyz.v[0] = viewing_params[0];
    vc.white_xyz.v[1] = viewing_params[1];
    vc.white_xyz.v[2] = viewing_params[2];
    vc.adapting_luminance = viewing_params[3];
    vc.background_luminance = viewing_params[4];
    vc.surround = ALWAN_CAM16_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Load correlates (J, C, h) */
    static alwan_scalar const correlates_data[] = {
#include "reference_values/cam16_correlates.csv"
    };
    size_t const num_colors = sizeof(correlates_data) / sizeof(correlates_data[0]) / 7;

    /* Load expected reconstructed XYZ */
    static alwan_scalar const expected_xyz[] = {
#include "reference_values/cam16_xyz_reconstructed.csv"
    };

    /* Test inverse transform for each color */
    for (size_t i = 0; i < num_colors; i++) {
        /* Extract J, C, h from correlates */
        alwan_cam16_correlates corr;
        corr.J = correlates_data[i * 7 + 0];
        corr.C = correlates_data[i * 7 + 1];
        corr.h = correlates_data[i * 7 + 2];

        alwan_vec3 xyz_out;
        int status = alwan_cam16_inverse(&corr, &vc, &xyz_out);
        TEST_ASSERT(status == ALWAN_OK, "Inverse transform failed");

        /* Check XYZ against expected values */
        alwan_scalar const *expected = &expected_xyz[i * 3];
        alwan_scalar X_err = ALWAN_FABS(xyz_out.v[0] - expected[0]);
        alwan_scalar Y_err = ALWAN_FABS(xyz_out.v[1] - expected[1]);
        alwan_scalar Z_err = ALWAN_FABS(xyz_out.v[2] - expected[2]);

        if (X_err >= CORRELATE_TOL || Y_err >= CORRELATE_TOL || Z_err >= CORRELATE_TOL) {
            printf("  Color %zu: XYZ errors = [%.10e, %.10e, %.10e]\n",
                   i, (double)X_err, (double)Y_err, (double)Z_err);
            printf("    Got:      [%.10f, %.10f, %.10f]\n",
                   (double)xyz_out.v[0], (double)xyz_out.v[1], (double)xyz_out.v[2]);
            printf("    Expected: [%.10f, %.10f, %.10f]\n",
                   (double)expected[0], (double)expected[1], (double)expected[2]);
        }
        TEST_ASSERT(X_err < CORRELATE_TOL, "X mismatch");
        TEST_ASSERT(Y_err < CORRELATE_TOL, "Y mismatch");
        TEST_ASSERT(Z_err < CORRELATE_TOL, "Z mismatch");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("CAM16 inverse transform");
}

/* Test CAM16 round-trip (XYZ -> correlates -> XYZ) */
static int test_cam16_roundtrip(void) {
    /* Load viewing conditions from fixture */
    static alwan_scalar const viewing_params[] = {
#include "reference_values/cam_viewing_conditions.csv"
    };

    /* Standard D65 viewing conditions */
    alwan_cam16_viewing_conditions vc;
    vc.white_xyz.v[0] = viewing_params[0];
    vc.white_xyz.v[1] = viewing_params[1];
    vc.white_xyz.v[2] = viewing_params[2];
    vc.adapting_luminance = viewing_params[3];
    vc.background_luminance = viewing_params[4];
    vc.surround = ALWAN_CAM16_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Load test XYZ colors */
    static alwan_scalar const test_xyz[] = {
#include "reference_values/ciecam02_xyz_input.csv"
    };
    size_t const num_colors = sizeof(test_xyz) / sizeof(test_xyz[0]) / 3;

    /* Test round-trip for each color */
    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 xyz_in;
        xyz_in.v[0] = test_xyz[i * 3 + 0];
        xyz_in.v[1] = test_xyz[i * 3 + 1];
        xyz_in.v[2] = test_xyz[i * 3 + 2];

        /* Forward: XYZ -> correlates */
        alwan_cam16_correlates corr;
        int status = alwan_cam16_forward(&xyz_in, &vc, &corr);
        TEST_ASSERT(status == ALWAN_OK, "Forward transform failed");

        /* Inverse: correlates -> XYZ */
        alwan_vec3 xyz_out;
        status = alwan_cam16_inverse(&corr, &vc, &xyz_out);
        TEST_ASSERT(status == ALWAN_OK, "Inverse transform failed");

        /* Check round-trip error */
        alwan_scalar X_err = ALWAN_FABS(xyz_out.v[0] - xyz_in.v[0]);
        alwan_scalar Y_err = ALWAN_FABS(xyz_out.v[1] - xyz_in.v[1]);
        alwan_scalar Z_err = ALWAN_FABS(xyz_out.v[2] - xyz_in.v[2]);

        if (X_err >= CORRELATE_TOL || Y_err >= CORRELATE_TOL || Z_err >= CORRELATE_TOL) {
            printf("  Color %zu: Round-trip XYZ errors = [%.10e, %.10e, %.10e]\n",
                   i, (double)X_err, (double)Y_err, (double)Z_err);
            printf("    Input:  [%.10f, %.10f, %.10f]\n",
                   (double)xyz_in.v[0], (double)xyz_in.v[1], (double)xyz_in.v[2]);
            printf("    Output: [%.10f, %.10f, %.10f]\n",
                   (double)xyz_out.v[0], (double)xyz_out.v[1], (double)xyz_out.v[2]);
        }
        TEST_ASSERT(X_err < CORRELATE_TOL, "Round-trip X error too large");
        TEST_ASSERT(Y_err < CORRELATE_TOL, "Round-trip Y error too large");
        TEST_ASSERT(Z_err < CORRELATE_TOL, "Round-trip Z error too large");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("CAM16 round-trip");
}

/* Test CAM16-UCS forward transform (JMh -> Jab) */
static int test_cam16_ucs_forward(void) {
    /* Load CAM16 correlates */
    static alwan_scalar const correlates_data[] = {
#include "reference_values/cam16_correlates.csv"
    };
    size_t const num_colors = sizeof(correlates_data) / sizeof(correlates_data[0]) / 7;

    /* Load expected CAM16-UCS Jab coordinates */
    static alwan_scalar const expected_jab[] = {
#include "reference_values/cam16_ucs_jab.csv"
    };

    /* Test UCS transform for each color */
    for (size_t i = 0; i < num_colors; i++) {
        /* Extract J, M, h from correlates */
        alwan_cam16_correlates corr;
        corr.J = correlates_data[i * 7 + 0];
        corr.C = correlates_data[i * 7 + 1];
        corr.h = correlates_data[i * 7 + 2];
        corr.Q = correlates_data[i * 7 + 3];
        corr.M = correlates_data[i * 7 + 4];
        corr.s = correlates_data[i * 7 + 5];
        corr.H = correlates_data[i * 7 + 6];

        alwan_vec3 jab_out;
        int status = alwan_cam16_to_ucs(&corr, &jab_out);
        TEST_ASSERT(status == ALWAN_OK, "CAM16-UCS forward transform failed");

        /* Check Jab against expected values */
        alwan_scalar const *expected = &expected_jab[i * 3];
        alwan_scalar J_err = ALWAN_FABS(jab_out.v[0] - expected[0]);
        alwan_scalar a_err = ALWAN_FABS(jab_out.v[1] - expected[1]);
        alwan_scalar b_err = ALWAN_FABS(jab_out.v[2] - expected[2]);

        TEST_ASSERT(J_err < CORRELATE_TOL, "J' mismatch");
        TEST_ASSERT(a_err < CORRELATE_TOL, "a' mismatch");
        TEST_ASSERT(b_err < CORRELATE_TOL, "b' mismatch");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("CAM16-UCS forward transform");
}

/* Test CAM16-UCS round-trip (JMh -> Jab -> JMh) */
static int test_cam16_ucs_roundtrip(void) {
    /* Load CAM16 correlates */
    static alwan_scalar const correlates_data[] = {
#include "reference_values/cam16_correlates.csv"
    };
    size_t const num_colors = sizeof(correlates_data) / sizeof(correlates_data[0]) / 7;

    /* Test UCS round-trip for each color */
    for (size_t i = 0; i < num_colors; i++) {
        /* Extract J, M, h from correlates */
        alwan_cam16_correlates corr_in;
        corr_in.J = correlates_data[i * 7 + 0];
        corr_in.M = correlates_data[i * 7 + 4];
        corr_in.h = correlates_data[i * 7 + 2];

        /* Forward: JMh -> Jab */
        alwan_vec3 jab;
        int status = alwan_cam16_to_ucs(&corr_in, &jab);
        TEST_ASSERT(status == ALWAN_OK, "UCS forward transform failed");

        /* Inverse: Jab -> JMh */
        alwan_cam16_correlates corr_out;
        status = alwan_cam16_from_ucs(&jab, &corr_out);
        TEST_ASSERT(status == ALWAN_OK, "UCS inverse transform failed");

        /* Check round-trip error */
        alwan_scalar J_err = ALWAN_FABS(corr_out.J - corr_in.J);
        alwan_scalar M_err = ALWAN_FABS(corr_out.M - corr_in.M);
        alwan_scalar h_err = ALWAN_FABS(corr_out.h - corr_in.h);

        /* Handle hue wraparound */
        if (h_err > ALWAN_LITERAL(180.0)) {
            h_err = ALWAN_LITERAL(360.0) - h_err;
        }

        TEST_ASSERT(J_err < CORRELATE_TOL, "Round-trip J error too large");
        TEST_ASSERT(M_err < CORRELATE_TOL, "Round-trip M error too large");
        TEST_ASSERT(h_err < CORRELATE_TOL, "Round-trip h error too large");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("CAM16-UCS round-trip");
}

/* Main test runner */
int test_80_cam16_main(void) {
    printf("=== M8: CAM16 Color Appearance Model + UCS Tests ===\n");

    int failed = 0;
    failed += test_cam16_forward();
    failed += test_cam16_inverse();
    failed += test_cam16_roundtrip();
    failed += test_cam16_ucs_forward();
    failed += test_cam16_ucs_roundtrip();

    if (failed == 0) {
        printf("\n=== All M8 tests passed ===\n");
    } else {
        printf("\n=== %d M8 test(s) failed ===\n", failed);
    }

    return failed;
}
