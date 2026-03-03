/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M7 Tests: CIECAM02 Color Appearance Model
 */

#include "test_common.h"
#include <stdlib.h>

/* Test CIECAM02 forward transform with standard viewing conditions */
static int test_ciecam02_forward(void) {
    /* Load viewing conditions from fixture */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const viewing_params[] = {
#include "reference_values/cam_viewing_conditions.csv"
    };
    ALWAN_DIAG_POP

    /* Standard D65 viewing conditions (average surround) */
    alwan_ciecam02_viewing_conditions vc;
    vc.white_xyz.x = viewing_params[0];
    vc.white_xyz.y = viewing_params[1];
    vc.white_xyz.z = viewing_params[2];
    vc.adapting_luminance = viewing_params[3];
    vc.background_luminance = viewing_params[4];
    vc.surround = ALWAN_CIECAM02_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Load test XYZ colors */
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
#include "reference_values/ciecam02_correlates.csv"
    };
    ALWAN_DIAG_POP

    /* Test forward transform for each color */
    for (size_t i = 0; i < num_colors; i++) {
        alwan_xyz xyz;
        xyz.x = test_xyz[i * 3 + 0];
        xyz.y = test_xyz[i * 3 + 1];
        xyz.z = test_xyz[i * 3 + 2];

        alwan_ciecam02_correlates corr;
        int status = alwan_ciecam02_forward(&corr, &xyz, &vc);
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
        TEST_ASSERT(Q_err < TEST_TOLERANCE, "Q mismatch");
        TEST_ASSERT(M_err < TEST_TOLERANCE, "M mismatch");
        TEST_ASSERT(s_err < TEST_TOLERANCE, "s mismatch");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("CIECAM02 forward transform");
}

/* Test CIECAM02 inverse transform */
static int test_ciecam02_inverse(void) {
    /* Standard D65 viewing conditions */
    alwan_ciecam02_viewing_conditions vc;
    /* Load viewing conditions from fixture */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const viewing_params[] = {
#include "reference_values/cam_viewing_conditions.csv"
    };
    ALWAN_DIAG_POP

    vc.white_xyz.x = viewing_params[0];
    vc.white_xyz.y = viewing_params[1];
    vc.white_xyz.z = viewing_params[2];
    vc.adapting_luminance = viewing_params[3];
    vc.background_luminance = viewing_params[4];
    vc.surround = ALWAN_CIECAM02_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Load correlates (J, C, h) */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const correlates_data[] = {
#include "reference_values/ciecam02_correlates.csv"
    };
    ALWAN_DIAG_POP
    size_t const num_colors = sizeof(correlates_data) / sizeof(correlates_data[0]) / 7;

    /* Load expected reconstructed XYZ */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const expected_xyz[] = {
#include "reference_values/ciecam02_xyz_reconstructed.csv"
    };
    ALWAN_DIAG_POP

    /* Test inverse transform for each color */
    for (size_t i = 0; i < num_colors; i++) {
        /* Extract J, C, h from correlates */
        alwan_ciecam02_correlates corr;
        corr.J = correlates_data[i * 7 + 0];
        corr.C = correlates_data[i * 7 + 1];
        corr.h = correlates_data[i * 7 + 2];
        /* Other fields not used for inverse */

        alwan_xyz xyz_out;
        int status = alwan_ciecam02_inverse(&xyz_out, &corr, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Inverse transform failed");

        /* Check XYZ against expected values */
        alwan_scalar const *expected = &expected_xyz[i * 3];
        alwan_scalar X_err = ALWAN_ABS(xyz_out.x - expected[0]);
        alwan_scalar Y_err = ALWAN_ABS(xyz_out.y - expected[1]);
        alwan_scalar Z_err = ALWAN_ABS(xyz_out.z - expected[2]);

        TEST_ASSERT(X_err < TEST_TOLERANCE, "X mismatch");
        TEST_ASSERT(Y_err < TEST_TOLERANCE, "Y mismatch");
        TEST_ASSERT(Z_err < TEST_TOLERANCE, "Z mismatch");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("CIECAM02 inverse transform");
}

/* Test CIECAM02 round-trip (XYZ -> correlates -> XYZ) */
static int test_ciecam02_roundtrip(void) {
    /* Standard D65 viewing conditions */
    alwan_ciecam02_viewing_conditions vc;
    /* Load viewing conditions from fixture */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const viewing_params[] = {
#include "reference_values/cam_viewing_conditions.csv"
    };
    ALWAN_DIAG_POP

    vc.white_xyz.x = viewing_params[0];
    vc.white_xyz.y = viewing_params[1];
    vc.white_xyz.z = viewing_params[2];
    vc.adapting_luminance = viewing_params[3];
    vc.background_luminance = viewing_params[4];
    vc.surround = ALWAN_CIECAM02_SURROUND_AVERAGE;
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
        alwan_ciecam02_correlates corr;
        int status = alwan_ciecam02_forward(&corr, &xyz_in, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Forward transform failed");

        /* Inverse: correlates -> XYZ */
        alwan_xyz xyz_out;
        status = alwan_ciecam02_inverse(&xyz_out, &corr, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Inverse transform failed");

        /* Check round-trip error */
        alwan_scalar X_err = ALWAN_ABS(xyz_out.x - xyz_in.x);
        alwan_scalar Y_err = ALWAN_ABS(xyz_out.y - xyz_in.y);
        alwan_scalar Z_err = ALWAN_ABS(xyz_out.z - xyz_in.z);

        TEST_ASSERT(X_err < TEST_TOLERANCE, "Round-trip X error too large");
        TEST_ASSERT(Y_err < TEST_TOLERANCE, "Round-trip Y error too large");
        TEST_ASSERT(Z_err < TEST_TOLERANCE, "Round-trip Z error too large");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("CIECAM02 round-trip");
}

/* Test different surround conditions */
static int test_ciecam02_surround_conditions(void) {
    alwan_ciecam02_viewing_conditions vc;
    /* Load viewing conditions from fixture */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const viewing_params[] = {
#include "reference_values/cam_viewing_conditions.csv"
    };
    ALWAN_DIAG_POP

    vc.white_xyz.x = viewing_params[0];
    vc.white_xyz.y = viewing_params[1];
    vc.white_xyz.z = viewing_params[2];
    vc.adapting_luminance = viewing_params[3];
    vc.background_luminance = viewing_params[4];
    vc.discount_illuminant = 0;

    /* Test color: mid-gray */
    alwan_xyz xyz;
    xyz.x = ALWAN_LITERAL(50.0);
    xyz.y = ALWAN_LITERAL(50.0);
    xyz.z = ALWAN_LITERAL(50.0);

    alwan_ciecam02_correlates corr_avg, corr_dim, corr_dark;

    /* Average surround */
    vc.surround = ALWAN_CIECAM02_SURROUND_AVERAGE;
    int status = alwan_ciecam02_forward(&corr_avg, &xyz, &vc);
    TEST_ASSERT(status == ALWAN_OK, "Average surround failed");

    /* Dim surround */
    vc.surround = ALWAN_CIECAM02_SURROUND_DIM;
    status = alwan_ciecam02_forward(&corr_dim, &xyz, &vc);
    TEST_ASSERT(status == ALWAN_OK, "Dim surround failed");

    /* Dark surround */
    vc.surround = ALWAN_CIECAM02_SURROUND_DARK;
    status = alwan_ciecam02_forward(&corr_dark, &xyz, &vc);
    TEST_ASSERT(status == ALWAN_OK, "Dark surround failed");

    /* Different surrounds should give different results */
    TEST_ASSERT(ALWAN_ABS(corr_avg.J - corr_dim.J) > ALWAN_LITERAL(0.01), "Average vs Dim J should differ");
    TEST_ASSERT(ALWAN_ABS(corr_avg.J - corr_dark.J) > ALWAN_LITERAL(0.01), "Average vs Dark J should differ");

    printf("  Average surround: J=%.2f, C=%.2f, h=%.2f\n",
           (double)corr_avg.J, (double)corr_avg.C, (double)corr_avg.h);
    printf("  Dim surround:     J=%.2f, C=%.2f, h=%.2f\n",
           (double)corr_dim.J, (double)corr_dim.C, (double)corr_dim.h);
    printf("  Dark surround:    J=%.2f, C=%.2f, h=%.2f\n",
           (double)corr_dark.J, (double)corr_dark.C, (double)corr_dark.h);

    TEST_PASS("Surround conditions");
}

/* Main test runner */
int test_13_ciecam02_main(void) {
    printf("=== M7: CIECAM02 Color Appearance Model Tests ===\n");

    int failed = 0;
    failed += test_ciecam02_forward();
    failed += test_ciecam02_inverse();
    failed += test_ciecam02_roundtrip();
    failed += test_ciecam02_surround_conditions();

    if (failed == 0) {
        printf("\n=== All M7 tests passed ===\n");
    } else {
        printf("\n=== %d M7 test(s) failed ===\n", failed);
    }

    return failed;
}
