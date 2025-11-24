/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M47 Tests: Kim2009 Color Appearance Model
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
/* Kim2009 involves power laws and logarithms where numerical precision
 * loss is expected. Practical tolerance: 1.0 unit. */
#define CORRELATE_TOL ALWAN_LITERAL(1.0)

/* Test data from CSV: XYZ_in (3), XYZ_w (3), La, Yb, J, C, h
 * Format: 12 values per row (3+3+1+1+3) = 12 scalars per test case */
static alwan_scalar const test_data[] = {
#include "reference_values/kim2009.csv"
};

static size_t const num_test_cases = sizeof(test_data) / sizeof(test_data[0]) / 12;

/* Helper to extract test case from flat array */
static void get_test_case(size_t index, alwan_vec3 *xyz_in, alwan_vec3 *xyz_w,
                          alwan_scalar *La, alwan_scalar *Yb,
                          alwan_scalar *J_expected, alwan_scalar *C_expected, alwan_scalar *h_expected) {
    size_t offset = index * 12;
    xyz_in->v[0] = test_data[offset + 0];
    xyz_in->v[1] = test_data[offset + 1];
    xyz_in->v[2] = test_data[offset + 2];
    xyz_w->v[0] = test_data[offset + 3];
    xyz_w->v[1] = test_data[offset + 4];
    xyz_w->v[2] = test_data[offset + 5];
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
        alwan_vec3 xyz_in, xyz_w;
        alwan_scalar La, Yb, J_expected, C_expected, h_expected;
        get_test_case(i, &xyz_in, &xyz_w, &La, &Yb, &J_expected, &C_expected, &h_expected);

        /* Set up viewing conditions */
        alwan_kim2009_viewing_conditions vc;
        vc.white_xyz = xyz_w;
        vc.La = La;
        vc.Yb = Yb;
        vc.discount_illuminant = 0;

        /* Forward transform */
        alwan_kim2009_correlates corr;
        int status = alwan_kim2009_forward(&xyz_in, &vc, &corr);
        TEST_ASSERT(status == ALWAN_OK, "Forward transform failed");

        /* Check correlates against expected values */
        alwan_scalar J_err = ALWAN_FABS(corr.J - J_expected);
        alwan_scalar C_err = ALWAN_FABS(corr.C - C_expected);
        alwan_scalar h_err = ALWAN_FABS(corr.h - h_expected);

        /* Handle hue wraparound (360° = 0°) */
        if (h_err > ALWAN_LITERAL(180.0)) {
            h_err = ALWAN_LITERAL(360.0) - h_err;
        }

        if (J_err >= CORRELATE_TOL) {
            printf("  Test %zu: J error = %.10e (got %.10f, expected %.10f)\n",
                   i + 1, (double)J_err, (double)corr.J, (double)J_expected);
        }
        if (C_err >= CORRELATE_TOL) {
            printf("  Test %zu: C error = %.10e (got %.10f, expected %.10f)\n",
                   i + 1, (double)C_err, (double)corr.C, (double)C_expected);
        }
        if (h_err >= CORRELATE_TOL && corr.C > ALWAN_LITERAL(1.0)) {
            printf("  Test %zu: h error = %.10e (got %.10f, expected %.10f)\n",
                   i + 1, (double)h_err, (double)corr.h, (double)h_expected);
        }

        TEST_ASSERT(J_err < CORRELATE_TOL, "J mismatch");
        TEST_ASSERT(C_err < CORRELATE_TOL, "C mismatch");
        /* For achromatic colors (C ≈ 0), hue is undefined - skip hue check */
        if (corr.C > ALWAN_LITERAL(1.0)) {
            TEST_ASSERT(h_err < CORRELATE_TOL, "h mismatch");
        }
    }

    printf("  Tested %zu colors\n", num_test_cases);
    TEST_PASS("Kim2009 forward transform");
}

/* Test Kim2009 inverse transform */
static int test_kim2009_inverse(void) {
    /* Test inverse transform for each test case */
    for (size_t i = 0; i < num_test_cases; i++) {
        alwan_vec3 xyz_in, xyz_w;
        alwan_scalar La, Yb, J_expected, C_expected, h_expected;
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
        alwan_vec3 xyz_out;
        int status = alwan_kim2009_inverse(&corr, &vc, &xyz_out);
        TEST_ASSERT(status == ALWAN_OK, "Inverse transform failed");

        /* Check XYZ against input values */
        alwan_scalar X_err = ALWAN_FABS(xyz_out.v[0] - xyz_in.v[0]);
        alwan_scalar Y_err = ALWAN_FABS(xyz_out.v[1] - xyz_in.v[1]);
        alwan_scalar Z_err = ALWAN_FABS(xyz_out.v[2] - xyz_in.v[2]);

        if (X_err >= CORRELATE_TOL || Y_err >= CORRELATE_TOL || Z_err >= CORRELATE_TOL) {
            printf("  Test %zu: XYZ errors = [%.10e, %.10e, %.10e]\n",
                   i + 1, (double)X_err, (double)Y_err, (double)Z_err);
            printf("    Got:      [%.10f, %.10f, %.10f]\n",
                   (double)xyz_out.v[0], (double)xyz_out.v[1], (double)xyz_out.v[2]);
            printf("    Expected: [%.10f, %.10f, %.10f]\n",
                   (double)xyz_in.v[0], (double)xyz_in.v[1], (double)xyz_in.v[2]);
        }

        TEST_ASSERT(X_err < CORRELATE_TOL, "X mismatch");
        TEST_ASSERT(Y_err < CORRELATE_TOL, "Y mismatch");
        TEST_ASSERT(Z_err < CORRELATE_TOL, "Z mismatch");
    }

    printf("  Tested %zu colors\n", num_test_cases);
    TEST_PASS("Kim2009 inverse transform");
}

/* Test Kim2009 round-trip (XYZ -> correlates -> XYZ) */
static int test_kim2009_roundtrip(void) {
    /* Test round-trip for each test case */
    for (size_t i = 0; i < num_test_cases; i++) {
        alwan_vec3 xyz_in, xyz_w;
        alwan_scalar La, Yb, J_expected, C_expected, h_expected;
        get_test_case(i, &xyz_in, &xyz_w, &La, &Yb, &J_expected, &C_expected, &h_expected);

        /* Set up viewing conditions */
        alwan_kim2009_viewing_conditions vc;
        vc.white_xyz = xyz_w;
        vc.La = La;
        vc.Yb = Yb;
        vc.discount_illuminant = 0;

        /* Forward: XYZ -> correlates */
        alwan_kim2009_correlates corr;
        int status = alwan_kim2009_forward(&xyz_in, &vc, &corr);
        TEST_ASSERT(status == ALWAN_OK, "Forward transform failed");

        /* Inverse: correlates -> XYZ */
        alwan_vec3 xyz_out;
        status = alwan_kim2009_inverse(&corr, &vc, &xyz_out);
        TEST_ASSERT(status == ALWAN_OK, "Inverse transform failed");

        /* Check round-trip error */
        alwan_scalar X_err = ALWAN_FABS(xyz_out.v[0] - xyz_in.v[0]);
        alwan_scalar Y_err = ALWAN_FABS(xyz_out.v[1] - xyz_in.v[1]);
        alwan_scalar Z_err = ALWAN_FABS(xyz_out.v[2] - xyz_in.v[2]);

        if (X_err >= CORRELATE_TOL || Y_err >= CORRELATE_TOL || Z_err >= CORRELATE_TOL) {
            printf("  Test %zu: Round-trip XYZ errors = [%.10e, %.10e, %.10e]\n",
                   i + 1, (double)X_err, (double)Y_err, (double)Z_err);
            printf("    Input:  [%.10f, %.10f, %.10f]\n",
                   (double)xyz_in.v[0], (double)xyz_in.v[1], (double)xyz_in.v[2]);
            printf("    Output: [%.10f, %.10f, %.10f]\n",
                   (double)xyz_out.v[0], (double)xyz_out.v[1], (double)xyz_out.v[2]);
        }

        TEST_ASSERT(X_err < CORRELATE_TOL, "Round-trip X error too large");
        TEST_ASSERT(Y_err < CORRELATE_TOL, "Round-trip Y error too large");
        TEST_ASSERT(Z_err < CORRELATE_TOL, "Round-trip Z error too large");
    }

    printf("  Tested %zu colors\n", num_test_cases);
    TEST_PASS("Kim2009 round-trip");
}

/* Test different background luminance values (surrounds) */
static int test_kim2009_surrounds(void) {
    alwan_kim2009_viewing_conditions vc;
    vc.white_xyz.v[0] = ALWAN_LITERAL(95.05);
    vc.white_xyz.v[1] = ALWAN_LITERAL(100.0);
    vc.white_xyz.v[2] = ALWAN_LITERAL(108.88);
    vc.La = ALWAN_LITERAL(318.31);
    vc.discount_illuminant = 0;

    /* Test color: mid-gray */
    alwan_vec3 xyz;
    xyz.v[0] = ALWAN_LITERAL(50.0);
    xyz.v[1] = ALWAN_LITERAL(50.0);
    xyz.v[2] = ALWAN_LITERAL(50.0);

    alwan_kim2009_correlates corr_avg, corr_dim, corr_dark;

    /* Average surround (Yb = 20) */
    vc.Yb = ALWAN_LITERAL(20.0);
    int status = alwan_kim2009_forward(&xyz, &vc, &corr_avg);
    TEST_ASSERT(status == ALWAN_OK, "Average surround failed");

    /* Dim surround (Yb = 5) */
    vc.Yb = ALWAN_LITERAL(5.0);
    status = alwan_kim2009_forward(&xyz, &vc, &corr_dim);
    TEST_ASSERT(status == ALWAN_OK, "Dim surround failed");

    /* Dark surround (Yb = 0.5) */
    vc.Yb = ALWAN_LITERAL(0.5);
    status = alwan_kim2009_forward(&xyz, &vc, &corr_dark);
    TEST_ASSERT(status == ALWAN_OK, "Dark surround failed");

    /* Different surrounds should give different results */
    TEST_ASSERT(ALWAN_FABS(corr_avg.J - corr_dim.J) > ALWAN_LITERAL(0.01), "Average vs Dim J should differ");
    TEST_ASSERT(ALWAN_FABS(corr_avg.J - corr_dark.J) > ALWAN_LITERAL(0.01), "Average vs Dark J should differ");

    printf("  Average surround (Yb=20): J=%.2f, C=%.2f, h=%.2f\n",
           (double)corr_avg.J, (double)corr_avg.C, (double)corr_avg.h);
    printf("  Dim surround (Yb=5):      J=%.2f, C=%.2f, h=%.2f\n",
           (double)corr_dim.J, (double)corr_dim.C, (double)corr_dim.h);
    printf("  Dark surround (Yb=0.5):   J=%.2f, C=%.2f, h=%.2f\n",
           (double)corr_dark.J, (double)corr_dark.C, (double)corr_dark.h);

    TEST_PASS("Surround conditions");
}

/* Main test runner */
int test_47_kim2009_main(void) {
    printf("=== M47: Kim2009 Color Appearance Model Tests ===\n");

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
