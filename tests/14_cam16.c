/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M8 Tests: CAM16 Color Appearance Model + UCS
 */

#include "test_common.h"
#include <stdlib.h>

/* Test CAM16 forward transform with standard viewing conditions */
static int test_cam16_forward(void) {
    /* Load viewing conditions from fixture (XYZ_w, L_A, Y_b) */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const viewing_params[] = {
#include "reference_values/cam_viewing_conditions.csv"
    };
    ALWAN_DIAG_POP

    /* Standard D65 viewing conditions (average surround) */
    alwan_cam16_viewing_conditions vc;
    vc.white_xyz.x = viewing_params[0];
    vc.white_xyz.y = viewing_params[1];
    vc.white_xyz.z = viewing_params[2];
    vc.adapting_luminance = viewing_params[3];
    vc.background_luminance = viewing_params[4];
    vc.surround = ALWAN_CAM16_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Load test XYZ colors (same as CIECAM02) */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const test_xyz[] = {
#include "reference_values/ciecam02_xyz_input.csv"
    };
    ALWAN_DIAG_POP
    size_t const num_colors = sizeof(test_xyz) / sizeof(test_xyz[0]) / 3;

    /* Load expected correlates (J, C, h, Q, M, s, H for each color) */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const expected_correlates[] = {
#include "reference_values/cam16_correlates.csv"
    };
    ALWAN_DIAG_POP

    /* Test forward transform for each color */
    for (size_t i = 0; i < num_colors; i++) {
        alwan_xyz xyz;
        xyz.x = test_xyz[i * 3 + 0];
        xyz.y = test_xyz[i * 3 + 1];
        xyz.z = test_xyz[i * 3 + 2];

        alwan_cam16_correlates corr;
        int status = alwan_cam16_forward(&corr, &xyz, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Forward transform failed");

        /* Check correlates against expected values */
        alwan_scalar const *expected = &expected_correlates[i * 7];
        alwan_scalar J_err = ALWAN_ABS(corr.J - expected[0]);
        alwan_scalar C_err = ALWAN_ABS(corr.C - expected[1]);
        alwan_scalar h_err = ALWAN_ABS(corr.h - expected[2]);

        /* Handle hue wraparound (360° = 0°) */
        if (h_err > ALWAN_LITERAL(180.0)) {
            h_err = ALWAN_LITERAL(360.0) - h_err;
        }

        if (J_err >= TEST_TOLERANCE) {
            printf("  Color %zu: J error = %.10e (got %.10f, expected %.10f)\n",
                   i, (double)J_err, (double)corr.J, (double)expected[0]);
        }
        if (C_err >= TEST_TOLERANCE) {
            printf("  Color %zu: C error = %.10e (got %.10f, expected %.10f)\n",
                   i, (double)C_err, (double)corr.C, (double)expected[1]);
        }
        if (h_err >= TEST_TOLERANCE && corr.C > ALWAN_LITERAL(2.0)) {
            printf("  Color %zu: h error = %.10e (got %.10f, expected %.10f)\n",
                   i, (double)h_err, (double)corr.h, (double)expected[2]);
        }
        TEST_ASSERT(J_err < TEST_TOLERANCE, "J mismatch");
        TEST_ASSERT(C_err < TEST_TOLERANCE, "C mismatch");
        /* For near-achromatic colors (C < 2), hue is ill-conditioned:
         * atan2 amplifies ULP errors by ~1/sqrt(a²+b²), making 1e-12
         * precision unachievable at double precision when C is small. */
        if (corr.C > ALWAN_LITERAL(2.0)) {
            TEST_ASSERT(h_err < TEST_TOLERANCE, "h mismatch");
        }

        /* Also check Q, M, s */
        alwan_scalar Q_err = ALWAN_ABS(corr.Q - expected[3]);
        alwan_scalar M_err = ALWAN_ABS(corr.M - expected[4]);
        alwan_scalar s_err = ALWAN_ABS(corr.s - expected[5]);
        if (Q_err >= TEST_TOLERANCE) {
            printf("  Color %zu: Q error = %.10e (got %.10f, expected %.10f)\n",
                   i, (double)Q_err, (double)corr.Q, (double)expected[3]);
        }
        if (M_err >= TEST_TOLERANCE) {
            printf("  Color %zu: M error = %.10e (got %.10f, expected %.10f)\n",
                   i, (double)M_err, (double)corr.M, (double)expected[4]);
        }
        if (s_err >= TEST_TOLERANCE) {
            printf("  Color %zu: s error = %.10e (got %.10f, expected %.10f)\n",
                   i, (double)s_err, (double)corr.s, (double)expected[5]);
        }
        TEST_ASSERT(Q_err < TEST_TOLERANCE, "Q mismatch");
        TEST_ASSERT(M_err < TEST_TOLERANCE, "M mismatch");
        TEST_ASSERT(s_err < TEST_TOLERANCE, "s mismatch");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("CAM16 forward transform");
}

/* Test CAM16 inverse transform */
static int test_cam16_inverse(void) {
    /* Load viewing conditions from fixture */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const viewing_params[] = {
#include "reference_values/cam_viewing_conditions.csv"
    };
    ALWAN_DIAG_POP

    /* Standard D65 viewing conditions */
    alwan_cam16_viewing_conditions vc;
    vc.white_xyz.x = viewing_params[0];
    vc.white_xyz.y = viewing_params[1];
    vc.white_xyz.z = viewing_params[2];
    vc.adapting_luminance = viewing_params[3];
    vc.background_luminance = viewing_params[4];
    vc.surround = ALWAN_CAM16_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Load correlates (J, C, h) */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const correlates_data[] = {
#include "reference_values/cam16_correlates.csv"
    };
    ALWAN_DIAG_POP
    size_t const num_colors = sizeof(correlates_data) / sizeof(correlates_data[0]) / 7;

    /* Load expected reconstructed XYZ */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const expected_xyz[] = {
#include "reference_values/cam16_xyz_reconstructed.csv"
    };
    ALWAN_DIAG_POP

    /* Test inverse transform for each color */
    for (size_t i = 0; i < num_colors; i++) {
        /* Extract J, C, h from correlates */
        alwan_cam16_correlates corr;
        corr.J = correlates_data[i * 7 + 0];
        corr.C = correlates_data[i * 7 + 1];
        corr.h = correlates_data[i * 7 + 2];

        alwan_xyz xyz_out;
        int status = alwan_cam16_inverse(&xyz_out, &corr, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Inverse transform failed");

        /* Check XYZ against expected values */
        alwan_scalar const *expected = &expected_xyz[i * 3];
        alwan_scalar X_err = ALWAN_ABS(xyz_out.x - expected[0]);
        alwan_scalar Y_err = ALWAN_ABS(xyz_out.y - expected[1]);
        alwan_scalar Z_err = ALWAN_ABS(xyz_out.z - expected[2]);

        if (X_err >= TEST_TOLERANCE || Y_err >= TEST_TOLERANCE || Z_err >= TEST_TOLERANCE) {
            printf("  Color %zu: XYZ errors = [%.10e, %.10e, %.10e]\n",
                   i, (double)X_err, (double)Y_err, (double)Z_err);
            printf("    Got:      [%.10f, %.10f, %.10f]\n",
                   (double)xyz_out.x, (double)xyz_out.y, (double)xyz_out.z);
            printf("    Expected: [%.10f, %.10f, %.10f]\n",
                   (double)expected[0], (double)expected[1], (double)expected[2]);
        }
        TEST_ASSERT(X_err < TEST_TOLERANCE, "X mismatch");
        TEST_ASSERT(Y_err < TEST_TOLERANCE, "Y mismatch");
        TEST_ASSERT(Z_err < TEST_TOLERANCE, "Z mismatch");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("CAM16 inverse transform");
}

/* Test CAM16 round-trip (XYZ -> correlates -> XYZ) */
static int test_cam16_roundtrip(void) {
    /* Load viewing conditions from fixture */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const viewing_params[] = {
#include "reference_values/cam_viewing_conditions.csv"
    };
    ALWAN_DIAG_POP

    /* Standard D65 viewing conditions */
    alwan_cam16_viewing_conditions vc;
    vc.white_xyz.x = viewing_params[0];
    vc.white_xyz.y = viewing_params[1];
    vc.white_xyz.z = viewing_params[2];
    vc.adapting_luminance = viewing_params[3];
    vc.background_luminance = viewing_params[4];
    vc.surround = ALWAN_CAM16_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Load test XYZ colors */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const test_xyz[] = {
#include "reference_values/ciecam02_xyz_input.csv"
    };
    ALWAN_DIAG_POP
    size_t const num_colors = sizeof(test_xyz) / sizeof(test_xyz[0]) / 3;

    /* Test round-trip for each color */
    for (size_t i = 0; i < num_colors; i++) {
        alwan_xyz xyz_in;
        xyz_in.x = test_xyz[i * 3 + 0];
        xyz_in.y = test_xyz[i * 3 + 1];
        xyz_in.z = test_xyz[i * 3 + 2];

        /* Forward: XYZ -> correlates */
        alwan_cam16_correlates corr;
        int status = alwan_cam16_forward(&corr, &xyz_in, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Forward transform failed");

        /* Inverse: correlates -> XYZ */
        alwan_xyz xyz_out;
        status = alwan_cam16_inverse(&xyz_out, &corr, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Inverse transform failed");

        /* Check round-trip error */
        alwan_scalar X_err = ALWAN_ABS(xyz_out.x - xyz_in.x);
        alwan_scalar Y_err = ALWAN_ABS(xyz_out.y - xyz_in.y);
        alwan_scalar Z_err = ALWAN_ABS(xyz_out.z - xyz_in.z);

        if (X_err >= TEST_TOLERANCE || Y_err >= TEST_TOLERANCE || Z_err >= TEST_TOLERANCE) {
            printf("  Color %zu: Round-trip XYZ errors = [%.10e, %.10e, %.10e]\n",
                   i, (double)X_err, (double)Y_err, (double)Z_err);
            printf("    Input:  [%.10f, %.10f, %.10f]\n",
                   (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
            printf("    Output: [%.10f, %.10f, %.10f]\n",
                   (double)xyz_out.x, (double)xyz_out.y, (double)xyz_out.z);
        }
        TEST_ASSERT(X_err < TEST_TOLERANCE, "Round-trip X error too large");
        TEST_ASSERT(Y_err < TEST_TOLERANCE, "Round-trip Y error too large");
        TEST_ASSERT(Z_err < TEST_TOLERANCE, "Round-trip Z error too large");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("CAM16 round-trip");
}

/* Test CAM16-UCS forward transform (JMh -> Jab) */
static int test_cam16_ucs_forward(void) {
    /* Load CAM16 correlates */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const correlates_data[] = {
#include "reference_values/cam16_correlates.csv"
    };
    ALWAN_DIAG_POP
    size_t const num_colors = sizeof(correlates_data) / sizeof(correlates_data[0]) / 7;

    /* Load expected CAM16-UCS Jab coordinates */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const expected_jab[] = {
#include "reference_values/cam16_ucs_jab.csv"
    };
    ALWAN_DIAG_POP

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

        alwan_cam_jab jab_out;
        int status = alwan_cam16_to_ucs(&jab_out, &corr);
        TEST_ASSERT(status == ALWAN_OK, "CAM16-UCS forward transform failed");

        /* Check Jab against expected values */
        alwan_scalar const *expected = &expected_jab[i * 3];
        alwan_scalar J_err = ALWAN_ABS(jab_out.J - expected[0]);
        alwan_scalar a_err = ALWAN_ABS(jab_out.a - expected[1]);
        alwan_scalar b_err = ALWAN_ABS(jab_out.b - expected[2]);

        TEST_ASSERT(J_err < TEST_TOLERANCE, "J' mismatch");
        TEST_ASSERT(a_err < TEST_TOLERANCE, "a' mismatch");
        TEST_ASSERT(b_err < TEST_TOLERANCE, "b' mismatch");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("CAM16-UCS forward transform");
}

/* Test CAM16-UCS round-trip (JMh -> Jab -> JMh) */
static int test_cam16_ucs_roundtrip(void) {
    /* Load CAM16 correlates */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const correlates_data[] = {
#include "reference_values/cam16_correlates.csv"
    };
    ALWAN_DIAG_POP
    size_t const num_colors = sizeof(correlates_data) / sizeof(correlates_data[0]) / 7;

    /* Test UCS round-trip for each color */
    for (size_t i = 0; i < num_colors; i++) {
        /* Extract J, M, h from correlates */
        alwan_cam16_correlates corr_in;
        corr_in.J = correlates_data[i * 7 + 0];
        corr_in.M = correlates_data[i * 7 + 4];
        corr_in.h = correlates_data[i * 7 + 2];

        /* Forward: JMh -> Jab */
        alwan_cam_jab jab;
        int status = alwan_cam16_to_ucs(&jab, &corr_in);
        TEST_ASSERT(status == ALWAN_OK, "UCS forward transform failed");

        /* Inverse: Jab -> JMh */
        alwan_cam16_correlates corr_out;
        status = alwan_cam16_from_ucs(&corr_out, &jab);
        TEST_ASSERT(status == ALWAN_OK, "UCS inverse transform failed");

        /* Check round-trip error */
        alwan_scalar J_err = ALWAN_ABS(corr_out.J - corr_in.J);
        alwan_scalar M_err = ALWAN_ABS(corr_out.M - corr_in.M);
        alwan_scalar h_err = ALWAN_ABS(corr_out.h - corr_in.h);

        /* Handle hue wraparound */
        if (h_err > ALWAN_LITERAL(180.0)) {
            h_err = ALWAN_LITERAL(360.0) - h_err;
        }

        TEST_ASSERT(J_err < TEST_TOLERANCE, "Round-trip J error too large");
        TEST_ASSERT(M_err < TEST_TOLERANCE, "Round-trip M error too large");
        TEST_ASSERT(h_err < TEST_TOLERANCE, "Round-trip h error too large");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("CAM16-UCS round-trip");
}

/* Main test runner */
int test_14_cam16_main(void) {
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
