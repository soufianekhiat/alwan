/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M46 Tests: Kim2009 Color Appearance Model
 */

#include "test_common.h"
#include <stdlib.h>

/* Test data from CSV: XYZ_in (3), XYZ_w (3), La, Yb, J, C, h
 * Format: 11 values per row (3+3+1+1+3) = 11 scalars per test case */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const test_data[] = {
#include "reference_values/kim2009.csv"
};
ALWAN_DIAG_POP

static size_t const num_test_cases = sizeof(test_data) / sizeof(test_data[0]) / 11;

/* Helper to extract test case from flat array */
static void get_test_case(size_t index, alwan_xyz *xyz_in, alwan_xyz *xyz_w,
                          alwan_f64 *La, alwan_f64 *Yb,
                          alwan_f64 *J_expected, alwan_f64 *C_expected, alwan_f64 *h_expected) {
    size_t offset = index * 11;
    xyz_in->x = test_data[offset + 0];
    xyz_in->y = test_data[offset + 1];
    xyz_in->z = test_data[offset + 2];
    xyz_w->x = test_data[offset + 3];
    xyz_w->y = test_data[offset + 4];
    xyz_w->z = test_data[offset + 5];
    *La = test_data[offset + 6];
    *Yb = test_data[offset + 7];
    *J_expected = test_data[offset + 8];
    *C_expected = test_data[offset + 9];
    *h_expected = test_data[offset + 10];
}

/* Test Kim2009 forward transform */
static int test_kim2009_forward(void) {
    /* Test forward transform for each test case */
    for (size_t i = 0; i < num_test_cases; i++) {
        alwan_xyz xyz_in, xyz_w;
        alwan_f64 La, Yb, J_expected, C_expected, h_expected;
        get_test_case(i, &xyz_in, &xyz_w, &La, &Yb, &J_expected, &C_expected, &h_expected);

        /* Set up viewing conditions */
        alwan_kim2009_viewing_conditions vc;
        vc.white_xyz = xyz_w;
        vc.La = La;
        vc.Yb = Yb;
        vc.discount_illuminant = 0;

        /* Forward transform */
        alwan_kim2009_correlates corr;
        int status = alwan_kim2009_forward(&corr, &xyz_in, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Forward transform failed");

        /* Check correlates against expected values */
        alwan_f64 J_err = ALWAN_ABS(corr.J - J_expected);
        alwan_f64 C_err = ALWAN_ABS(corr.C - C_expected);
        alwan_f64 h_err = ALWAN_ABS(corr.h - h_expected);

        /* Handle hue wraparound (360° = 0°) */
        if (h_err > ALWAN_LITERAL(180.0)) {
            h_err = ALWAN_LITERAL(360.0) - h_err;
        }

        if (J_err >= ALWAN_TEST_TOLERANCE) {
            printf("  Test %zu: J error = %.10e (got %.10f, expected %.10f)\n",
                   i + 1, (double)J_err, (double)corr.J, (double)J_expected);
        }
        if (C_err >= ALWAN_TEST_TOLERANCE) {
            printf("  Test %zu: C error = %.10e (got %.10f, expected %.10f)\n",
                   i + 1, (double)C_err, (double)corr.C, (double)C_expected);
        }
        if (h_err >= ALWAN_TEST_TOLERANCE && corr.C > ALWAN_LITERAL(1.0)) {
            printf("  Test %zu: h error = %.10e (got %.10f, expected %.10f)\n",
                   i + 1, (double)h_err, (double)corr.h, (double)h_expected);
        }

        TEST_ASSERT(J_err < ALWAN_TEST_TOLERANCE, "J mismatch");
        TEST_ASSERT(C_err < ALWAN_TEST_TOLERANCE, "C mismatch");
        /* For achromatic colors (C ~= 0), hue is undefined - skip hue check.
         * Hue angles can be ~360°; use relative tolerance (1e-12 * |h|) */
        if (corr.C > ALWAN_LITERAL(1.0)) {
            alwan_f64 h_tol = ALWAN_ABS(h_expected) * ALWAN_LITERAL(1e-12);
            if (h_tol < ALWAN_TEST_TOLERANCE) h_tol = ALWAN_TEST_TOLERANCE;
            TEST_ASSERT(h_err < h_tol, "h mismatch");
        }
    }

    printf("  Tested %zu colors\n", num_test_cases);
    TEST_PASS("Kim2009 forward transform");
}

/* Test Kim2009 inverse transform */
static int test_kim2009_inverse(void) {
    /* Test inverse transform for each test case */
    for (size_t i = 0; i < num_test_cases; i++) {
        alwan_xyz xyz_in, xyz_w;
        alwan_f64 La, Yb, J_expected, C_expected, h_expected;
        get_test_case(i, &xyz_in, &xyz_w, &La, &Yb, &J_expected, &C_expected, &h_expected);

        /* Set up viewing conditions */
        alwan_kim2009_viewing_conditions vc;
        vc.white_xyz = xyz_w;
        vc.La = La;
        vc.Yb = Yb;
        vc.discount_illuminant = 0;

        /* Set up correlates from expected values */
        alwan_kim2009_correlates corr;
        corr.J = J_expected;
        corr.C = C_expected;
        corr.h = h_expected;

        /* Inverse transform */
        alwan_xyz xyz_out;
        int status = alwan_kim2009_inverse(&xyz_out, &corr, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Inverse transform failed");

        /* Check XYZ against input values */
        alwan_f64 X_err = ALWAN_ABS(xyz_out.x - xyz_in.x);
        alwan_f64 Y_err = ALWAN_ABS(xyz_out.y - xyz_in.y);
        alwan_f64 Z_err = ALWAN_ABS(xyz_out.z - xyz_in.z);

        if (X_err >= ALWAN_TEST_TOLERANCE || Y_err >= ALWAN_TEST_TOLERANCE || Z_err >= ALWAN_TEST_TOLERANCE) {
            printf("  Test %zu: XYZ errors = [%.10e, %.10e, %.10e]\n",
                   i + 1, (double)X_err, (double)Y_err, (double)Z_err);
            printf("    Got:      [%.10f, %.10f, %.10f]\n",
                   (double)xyz_out.x, (double)xyz_out.y, (double)xyz_out.z);
            printf("    Expected: [%.10f, %.10f, %.10f]\n",
                   (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
        }

        TEST_ASSERT(X_err < ALWAN_TEST_TOLERANCE, "X mismatch");
        TEST_ASSERT(Y_err < ALWAN_TEST_TOLERANCE, "Y mismatch");
        TEST_ASSERT(Z_err < ALWAN_TEST_TOLERANCE, "Z mismatch");
    }

    printf("  Tested %zu colors\n", num_test_cases);
    TEST_PASS("Kim2009 inverse transform");
}

/* Test Kim2009 round-trip (XYZ -> correlates -> XYZ) */
static int test_kim2009_roundtrip(void) {
    /* Test round-trip for each test case */
    for (size_t i = 0; i < num_test_cases; i++) {
        alwan_xyz xyz_in, xyz_w;
        alwan_f64 La, Yb, J_expected, C_expected, h_expected;
        get_test_case(i, &xyz_in, &xyz_w, &La, &Yb, &J_expected, &C_expected, &h_expected);

        /* Set up viewing conditions */
        alwan_kim2009_viewing_conditions vc;
        vc.white_xyz = xyz_w;
        vc.La = La;
        vc.Yb = Yb;
        vc.discount_illuminant = 0;

        /* Forward: XYZ -> correlates */
        alwan_kim2009_correlates corr;
        int status = alwan_kim2009_forward(&corr, &xyz_in, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Forward transform failed");

        /* Inverse: correlates -> XYZ */
        alwan_xyz xyz_out;
        status = alwan_kim2009_inverse(&xyz_out, &corr, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Inverse transform failed");

        /* Check round-trip error */
        alwan_f64 X_err = ALWAN_ABS(xyz_out.x - xyz_in.x);
        alwan_f64 Y_err = ALWAN_ABS(xyz_out.y - xyz_in.y);
        alwan_f64 Z_err = ALWAN_ABS(xyz_out.z - xyz_in.z);

        if (X_err >= ALWAN_TEST_TOLERANCE || Y_err >= ALWAN_TEST_TOLERANCE || Z_err >= ALWAN_TEST_TOLERANCE) {
            printf("  Test %zu: Round-trip XYZ errors = [%.10e, %.10e, %.10e]\n",
                   i + 1, (double)X_err, (double)Y_err, (double)Z_err);
            printf("    Input:  [%.10f, %.10f, %.10f]\n",
                   (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
            printf("    Output: [%.10f, %.10f, %.10f]\n",
                   (double)xyz_out.x, (double)xyz_out.y, (double)xyz_out.z);
        }

        TEST_ASSERT(X_err < ALWAN_TEST_TOLERANCE, "Round-trip X error too large");
        TEST_ASSERT(Y_err < ALWAN_TEST_TOLERANCE, "Round-trip Y error too large");
        TEST_ASSERT(Z_err < ALWAN_TEST_TOLERANCE, "Round-trip Z error too large");
    }

    printf("  Tested %zu colors\n", num_test_cases);
    TEST_PASS("Kim2009 round-trip");
}

/* Test different background luminance values (surrounds) */
static int test_kim2009_surrounds(void) {
    alwan_kim2009_viewing_conditions vc;
    vc.white_xyz.x = ALWAN_LITERAL(95.05);
    vc.white_xyz.y = ALWAN_LITERAL(100.0);
    vc.white_xyz.z = ALWAN_LITERAL(108.88);
    vc.La = ALWAN_LITERAL(318.31);
    vc.discount_illuminant = 0;

    /* Test color: mid-gray */
    alwan_xyz xyz;
    xyz.x = ALWAN_LITERAL(50.0);
    xyz.y = ALWAN_LITERAL(50.0);
    xyz.z = ALWAN_LITERAL(50.0);

    alwan_kim2009_correlates corr_avg, corr_dim, corr_dark;

    /* Average surround (Yb = 20) */
    vc.Yb = ALWAN_LITERAL(20.0);
    int status = alwan_kim2009_forward(&corr_avg, &xyz, &vc);
    TEST_ASSERT(status == ALWAN_OK, "Average surround failed");

    /* Dim surround (Yb = 5) */
    vc.Yb = ALWAN_LITERAL(5.0);
    status = alwan_kim2009_forward(&corr_dim, &xyz, &vc);
    TEST_ASSERT(status == ALWAN_OK, "Dim surround failed");

    /* Dark surround (Yb = 0.5) */
    vc.Yb = ALWAN_LITERAL(0.5);
    status = alwan_kim2009_forward(&corr_dark, &xyz, &vc);
    TEST_ASSERT(status == ALWAN_OK, "Dark surround failed");

    /* Print surround results (informational) */
    printf("  Average surround (Yb=20): J=%.2f, C=%.2f, h=%.2f\n",
           (double)corr_avg.J, (double)corr_avg.C, (double)corr_avg.h);
    printf("  Dim surround (Yb=5):      J=%.2f, C=%.2f, h=%.2f\n",
           (double)corr_dim.J, (double)corr_dim.C, (double)corr_dim.h);
    printf("  Dark surround (Yb=0.5):   J=%.2f, C=%.2f, h=%.2f\n",
           (double)corr_dark.J, (double)corr_dark.C, (double)corr_dark.h);

    /* Note: Surround effect on J is minimal in Kim2009 (~0.02 in colour-science).
     * This test is informational - different surrounds may produce nearly identical J. */
    alwan_f64 j_diff_avg_dim = ALWAN_ABS(corr_avg.J - corr_dim.J);
    alwan_f64 j_diff_avg_dark = ALWAN_ABS(corr_avg.J - corr_dark.J);
    printf("  J difference (Avg vs Dim):  %.4f\n", (double)j_diff_avg_dim);
    printf("  J difference (Avg vs Dark): %.4f\n", (double)j_diff_avg_dark);

    TEST_PASS("Surround conditions");
}

/* Main test runner */
int test_46_kim2009_main(void) {
    printf("=== M46: Kim2009 Color Appearance Model Tests ===\n");

    int failed = 0;
    failed += test_kim2009_forward();
    failed += test_kim2009_inverse();
    failed += test_kim2009_roundtrip();
    failed += test_kim2009_surrounds();

    if (failed == 0) {
        printf("\n=== All M47 tests passed ===\n");
    } else {
        printf("\n=== %d M47 test(s) failed ===\n", failed);
    }

    return failed;
}
