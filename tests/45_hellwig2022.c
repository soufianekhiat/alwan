/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M45 Tests: Hellwig2022 Color Appearance Model
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
/* Hellwig2022 involves compound operations with powers/logs.
 * Implementation differences require ~1e-2 tolerance. */
#if ALWAN_SCALAR_IS_FLOAT
    #define CORRELATE_TOL ALWAN_LITERAL(1e-2)
#else
    #define CORRELATE_TOL ALWAN_LITERAL(1e-2)
#endif

/* Test data from CSV: XYZ_in (3), XYZ_w (3), La, Yb, surround_idx, J, C, h
 * Format: 12 values per row (3+3+1+1+1+3) = 12 scalars per test case */
static alwan_scalar const test_data[] = {
#include "reference_values/hellwig2022.csv"
};

static size_t const num_test_cases = sizeof(test_data) / sizeof(test_data[0]) / 12;

/* Helper to extract test case from flat array */
static void get_test_case(size_t index, alwan_xyz *xyz_in, alwan_xyz *xyz_w,
                          alwan_scalar *La, alwan_scalar *Yb, int *surround_idx,
                          alwan_scalar *J_expected, alwan_scalar *C_expected, alwan_scalar *h_expected) {
    size_t offset = index * 12;
    xyz_in->x = test_data[offset + 0];
    xyz_in->y = test_data[offset + 1];
    xyz_in->z = test_data[offset + 2];
    xyz_w->x = test_data[offset + 3];
    xyz_w->y = test_data[offset + 4];
    xyz_w->z = test_data[offset + 5];
    *La = test_data[offset + 6];
    *Yb = test_data[offset + 7];
    *surround_idx = (int)test_data[offset + 8];
    *J_expected = test_data[offset + 9];
    *C_expected = test_data[offset + 10];
    *h_expected = test_data[offset + 11];
}

/* Test Hellwig2022 forward transform with standard viewing conditions */
static int test_hellwig2022_forward(void) {
    /* Test forward transform for each test case */
    for (size_t i = 0; i < num_test_cases; i++) {
        alwan_xyz xyz_in, xyz_w;
        alwan_scalar La, Yb, J_expected, C_expected, h_expected;
        int surround_idx;
        get_test_case(i, &xyz_in, &xyz_w, &La, &Yb, &surround_idx, &J_expected, &C_expected, &h_expected);

        /* Set up viewing conditions */
        alwan_hellwig2022_viewing_conditions vc;
        vc.white_xyz = xyz_w;
        vc.adapting_luminance = La;
        vc.background_luminance = Yb;
        vc.surround = (alwan_hellwig2022_surround)surround_idx;
        vc.discount_illuminant = 0;

        /* Forward transform */
        alwan_hellwig2022_correlates corr;
        int status = alwan_hellwig2022_forward(&corr, &xyz_in, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Forward transform failed");

        /* Check correlates against expected values */
        alwan_scalar J_err = ALWAN_ABS(corr.J - J_expected);
        alwan_scalar C_err = ALWAN_ABS(corr.C - C_expected);
        alwan_scalar h_err = ALWAN_ABS(corr.h - h_expected);

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
    TEST_PASS("Hellwig2022 forward transform");
}

/* Test Hellwig2022 inverse transform */
static int test_hellwig2022_inverse(void) {
    /* Test inverse transform for each test case */
    for (size_t i = 0; i < num_test_cases; i++) {
        alwan_xyz xyz_in, xyz_w;
        alwan_scalar La, Yb, J_expected, C_expected, h_expected;
        int surround_idx;
        get_test_case(i, &xyz_in, &xyz_w, &La, &Yb, &surround_idx, &J_expected, &C_expected, &h_expected);

        /* Set up viewing conditions */
        alwan_hellwig2022_viewing_conditions vc;
        vc.white_xyz = xyz_w;
        vc.adapting_luminance = La;
        vc.background_luminance = Yb;
        vc.surround = (alwan_hellwig2022_surround)surround_idx;
        vc.discount_illuminant = 0;

        /* Set up correlates from expected values */
        alwan_hellwig2022_correlates corr;
        corr.J = J_expected;
        corr.C = C_expected;
        corr.h = h_expected;

        /* Inverse transform */
        alwan_xyz xyz_out;
        int status = alwan_hellwig2022_inverse(&xyz_out, &corr, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Inverse transform failed");

        /* Check XYZ against input values */
        alwan_scalar X_err = ALWAN_ABS(xyz_out.x - xyz_in.x);
        alwan_scalar Y_err = ALWAN_ABS(xyz_out.y - xyz_in.y);
        alwan_scalar Z_err = ALWAN_ABS(xyz_out.z - xyz_in.z);

        if (X_err >= CORRELATE_TOL || Y_err >= CORRELATE_TOL || Z_err >= CORRELATE_TOL) {
            printf("  Test %zu: XYZ errors = [%.10e, %.10e, %.10e]\n",
                   i + 1, (double)X_err, (double)Y_err, (double)Z_err);
            printf("    Got:      [%.10f, %.10f, %.10f]\n",
                   (double)xyz_out.x, (double)xyz_out.y, (double)xyz_out.z);
            printf("    Expected: [%.10f, %.10f, %.10f]\n",
                   (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
        }

        TEST_ASSERT(X_err < CORRELATE_TOL, "X mismatch");
        TEST_ASSERT(Y_err < CORRELATE_TOL, "Y mismatch");
        TEST_ASSERT(Z_err < CORRELATE_TOL, "Z mismatch");
    }

    printf("  Tested %zu colors\n", num_test_cases);
    TEST_PASS("Hellwig2022 inverse transform");
}

/* Test Hellwig2022 round-trip (XYZ -> correlates -> XYZ) */
static int test_hellwig2022_roundtrip(void) {
    /* Test round-trip for each test case */
    for (size_t i = 0; i < num_test_cases; i++) {
        alwan_xyz xyz_in, xyz_w;
        alwan_scalar La, Yb, J_expected, C_expected, h_expected;
        int surround_idx;
        get_test_case(i, &xyz_in, &xyz_w, &La, &Yb, &surround_idx, &J_expected, &C_expected, &h_expected);

        /* Set up viewing conditions */
        alwan_hellwig2022_viewing_conditions vc;
        vc.white_xyz = xyz_w;
        vc.adapting_luminance = La;
        vc.background_luminance = Yb;
        vc.surround = (alwan_hellwig2022_surround)surround_idx;
        vc.discount_illuminant = 0;

        /* Forward: XYZ -> correlates */
        alwan_hellwig2022_correlates corr;
        int status = alwan_hellwig2022_forward(&corr, &xyz_in, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Forward transform failed");

        /* Inverse: correlates -> XYZ */
        alwan_xyz xyz_out;
        status = alwan_hellwig2022_inverse(&xyz_out, &corr, &vc);
        TEST_ASSERT(status == ALWAN_OK, "Inverse transform failed");

        /* Check round-trip error */
        alwan_scalar X_err = ALWAN_ABS(xyz_out.x - xyz_in.x);
        alwan_scalar Y_err = ALWAN_ABS(xyz_out.y - xyz_in.y);
        alwan_scalar Z_err = ALWAN_ABS(xyz_out.z - xyz_in.z);

        if (X_err >= CORRELATE_TOL || Y_err >= CORRELATE_TOL || Z_err >= CORRELATE_TOL) {
            printf("  Test %zu: Round-trip XYZ errors = [%.10e, %.10e, %.10e]\n",
                   i + 1, (double)X_err, (double)Y_err, (double)Z_err);
            printf("    Input:  [%.10f, %.10f, %.10f]\n",
                   (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
            printf("    Output: [%.10f, %.10f, %.10f]\n",
                   (double)xyz_out.x, (double)xyz_out.y, (double)xyz_out.z);
        }

        TEST_ASSERT(X_err < CORRELATE_TOL, "Round-trip X error too large");
        TEST_ASSERT(Y_err < CORRELATE_TOL, "Round-trip Y error too large");
        TEST_ASSERT(Z_err < CORRELATE_TOL, "Round-trip Z error too large");
    }

    printf("  Tested %zu colors\n", num_test_cases);
    TEST_PASS("Hellwig2022 round-trip");
}

/* Test different surround conditions */
static int test_hellwig2022_surround_conditions(void) {
    alwan_hellwig2022_viewing_conditions vc;
    vc.white_xyz.x = ALWAN_LITERAL(95.05);
    vc.white_xyz.y = ALWAN_LITERAL(100.0);
    vc.white_xyz.z = ALWAN_LITERAL(108.88);
    vc.adapting_luminance = ALWAN_LITERAL(318.31);
    vc.background_luminance = ALWAN_LITERAL(20.0);
    vc.discount_illuminant = 0;

    /* Test color: mid-gray */
    alwan_xyz xyz;
    xyz.x = ALWAN_LITERAL(50.0);
    xyz.y = ALWAN_LITERAL(50.0);
    xyz.z = ALWAN_LITERAL(50.0);

    alwan_hellwig2022_correlates corr_avg, corr_dim, corr_dark;

    /* Average surround */
    vc.surround = ALWAN_HELLWIG2022_SURROUND_AVERAGE;
    int status = alwan_hellwig2022_forward(&corr_avg, &xyz, &vc);
    TEST_ASSERT(status == ALWAN_OK, "Average surround failed");

    /* Dim surround */
    vc.surround = ALWAN_HELLWIG2022_SURROUND_DIM;
    status = alwan_hellwig2022_forward(&corr_dim, &xyz, &vc);
    TEST_ASSERT(status == ALWAN_OK, "Dim surround failed");

    /* Dark surround */
    vc.surround = ALWAN_HELLWIG2022_SURROUND_DARK;
    status = alwan_hellwig2022_forward(&corr_dark, &xyz, &vc);
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
int test_45_hellwig2022_main(void) {
    printf("=== M45: Hellwig2022 Color Appearance Model Tests ===\n");

    int failed = 0;
    failed += test_hellwig2022_forward();
    failed += test_hellwig2022_inverse();
    failed += test_hellwig2022_roundtrip();
    failed += test_hellwig2022_surround_conditions();

    if (failed == 0) {
        printf("\n=== All M46 tests passed ===\n");
    } else {
        printf("\n=== %d M46 test(s) failed ===\n", failed);
    }

    return failed;
}
