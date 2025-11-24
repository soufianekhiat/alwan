/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 05: XYZ/xyY/Lab/Luv/LCh color space conversions
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Test helpers
 * ---------------------------------------------------------------- */

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } \
} while(0)

#define TEST_PASS(name) do { \
    printf("[PASS] %s\n", name); \
    return 0; \
} while(0)

static alwan_scalar vec3_max_diff(alwan_vec3 const *a, alwan_vec3 const *b) {
    alwan_scalar max_diff = 0;
    for (int i = 0; i < 3; i++) {
        alwan_scalar diff = ALWAN_FABS(a->v[i] - b->v[i]);
        if (diff > max_diff) max_diff = diff;
    }
    return max_diff;
}

static void vec3_print(char const *name, alwan_vec3 const *v) {
    printf("%s: [%12.8f %12.8f %12.8f]\n", name, v->v[0], v->v[1], v->v[2]);
}

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_xyz_xyy_roundtrip(void) {
    /* Load XYZ test values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const xyz_data[] = {
#include "reference_values/test_xyz_colors.csv"
    };
    static alwan_scalar const xyy_data[] = {
#include "reference_values/xyz_to_xyy.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(xyz_data) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(5e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{xyz_data[i * 3 + 0], xyz_data[i * 3 + 1], xyz_data[i * 3 + 2]}};
        alwan_vec3 xyy_expected = {{xyy_data[i * 3 + 0], xyy_data[i * 3 + 1], xyy_data[i * 3 + 2]}};

        /* XYZ -> xyY */
        alwan_vec3 xyy;
        alwan_xyz_to_xyy(&xyz, &xyy);
        alwan_scalar diff_forward = vec3_max_diff(&xyy, &xyy_expected);
        if (diff_forward >= tolerance) {
            printf("Test %d: diff=%e (tol=%e)\n", i, diff_forward, tolerance);
            vec3_print("  Computed", &xyy);
            vec3_print("  Expected", &xyy_expected);
        }
        TEST_ASSERT(diff_forward < tolerance, "XYZ->xyY mismatch");

        /* xyY -> XYZ (round-trip) */
        alwan_vec3 xyz_roundtrip;
        alwan_xyy_to_xyz(&xyy, &xyz_roundtrip);
        alwan_scalar diff_roundtrip = vec3_max_diff(&xyz, &xyz_roundtrip);
        TEST_ASSERT(diff_roundtrip < tolerance, "xyY->XYZ round-trip mismatch");
    }

    TEST_PASS("XYZ <-> xyY round-trip");
}

static int test_xyz_lab_d65_roundtrip(void) {
    /* Load white point and test values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const d65_xyz_data[] = {
#include "reference_values/test_d65_white.csv"
    };
    static alwan_scalar const xyz_data[] = {
#include "reference_values/test_xyz_colors.csv"
    };
    static alwan_scalar const lab_data[] = {
#include "reference_values/xyz_to_lab_d65.csv"
    };
    ALWAN_DIAG_POP

    alwan_vec3 white_xyz = {{d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]}};
    int const num_tests = sizeof(xyz_data) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(5e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{xyz_data[i * 3 + 0], xyz_data[i * 3 + 1], xyz_data[i * 3 + 2]}};
        alwan_vec3 lab_expected = {{lab_data[i * 3 + 0], lab_data[i * 3 + 1], lab_data[i * 3 + 2]}};

        /* XYZ -> Lab */
        alwan_vec3 lab;
        alwan_xyz_to_lab(&xyz, &white_xyz, &lab);
        alwan_scalar diff_forward = vec3_max_diff(&lab, &lab_expected);
        if (diff_forward >= tolerance) {
            printf("Lab D65 test %d: diff=%e (tol=%e)\n", i, diff_forward, tolerance);
            vec3_print("  Computed", &lab);
            vec3_print("  Expected", &lab_expected);
        }
        TEST_ASSERT(diff_forward < tolerance, "XYZ->Lab mismatch (D65)");

        /* Lab -> XYZ (round-trip) */
        alwan_vec3 xyz_roundtrip;
        alwan_lab_to_xyz(&lab, &white_xyz, &xyz_roundtrip);
        alwan_scalar diff_roundtrip = vec3_max_diff(&xyz, &xyz_roundtrip);
        TEST_ASSERT(diff_roundtrip < tolerance, "Lab->XYZ round-trip mismatch (D65)");
    }

    TEST_PASS("XYZ <-> Lab (D65) round-trip");
}

static int test_xyz_lab_d50_roundtrip(void) {
    /* Load white point and test values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const d50_xyz_data[] = {
#include "reference_values/test_d50_white.csv"
    };
    static alwan_scalar const xyz_data[] = {
#include "reference_values/test_xyz_colors.csv"
    };
    static alwan_scalar const lab_data[] = {
#include "reference_values/xyz_to_lab_d50.csv"
    };
    ALWAN_DIAG_POP

    alwan_vec3 white_xyz = {{d50_xyz_data[0], d50_xyz_data[1], d50_xyz_data[2]}};
    int const num_tests = sizeof(xyz_data) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(5e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{xyz_data[i * 3 + 0], xyz_data[i * 3 + 1], xyz_data[i * 3 + 2]}};
        alwan_vec3 lab_expected = {{lab_data[i * 3 + 0], lab_data[i * 3 + 1], lab_data[i * 3 + 2]}};

        /* XYZ -> Lab */
        alwan_vec3 lab;
        alwan_xyz_to_lab(&xyz, &white_xyz, &lab);
        alwan_scalar diff_forward = vec3_max_diff(&lab, &lab_expected);
        TEST_ASSERT(diff_forward < tolerance, "XYZ->Lab mismatch (D50)");

        /* Lab -> XYZ (round-trip) */
        alwan_vec3 xyz_roundtrip;
        alwan_lab_to_xyz(&lab, &white_xyz, &xyz_roundtrip);
        alwan_scalar diff_roundtrip = vec3_max_diff(&xyz, &xyz_roundtrip);
        TEST_ASSERT(diff_roundtrip < tolerance, "Lab->XYZ round-trip mismatch (D50)");
    }

    TEST_PASS("XYZ <-> Lab (D50) round-trip");
}

static int test_xyz_luv_d65_roundtrip(void) {
    /* Load white point and test values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const d65_xyz_data[] = {
#include "reference_values/test_d65_white.csv"
    };
    static alwan_scalar const xyz_data[] = {
#include "reference_values/test_xyz_colors.csv"
    };
    static alwan_scalar const luv_data[] = {
#include "reference_values/xyz_to_luv_d65.csv"
    };
    ALWAN_DIAG_POP

    alwan_vec3 white_xyz = {{d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]}};
    int const num_tests = sizeof(xyz_data) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(5e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{xyz_data[i * 3 + 0], xyz_data[i * 3 + 1], xyz_data[i * 3 + 2]}};
        alwan_vec3 luv_expected = {{luv_data[i * 3 + 0], luv_data[i * 3 + 1], luv_data[i * 3 + 2]}};

        /* XYZ -> Luv */
        alwan_vec3 luv;
        alwan_xyz_to_luv(&xyz, &white_xyz, &luv);
        alwan_scalar diff_forward = vec3_max_diff(&luv, &luv_expected);
        TEST_ASSERT(diff_forward < tolerance, "XYZ->Luv mismatch (D65)");

        /* Luv -> XYZ (round-trip) */
        alwan_vec3 xyz_roundtrip;
        alwan_luv_to_xyz(&luv, &white_xyz, &xyz_roundtrip);
        alwan_scalar diff_roundtrip = vec3_max_diff(&xyz, &xyz_roundtrip);
        TEST_ASSERT(diff_roundtrip < tolerance, "Luv->XYZ round-trip mismatch (D65)");
    }

    TEST_PASS("XYZ <-> Luv (D65) round-trip");
}

static int test_lab_lch_roundtrip(void) {
    /* Load Lab and LCh test values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const lab_data[] = {
#include "reference_values/xyz_to_lab_d65.csv"
    };
    static alwan_scalar const lch_data[] = {
#include "reference_values/lab_to_lch.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(lab_data) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(5e-5);
#else
    /* Relaxed tolerance for trigonometric functions (atan2) - different math libraries */
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-8);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 lab = {{lab_data[i * 3 + 0], lab_data[i * 3 + 1], lab_data[i * 3 + 2]}};
        alwan_vec3 lch_expected = {{lch_data[i * 3 + 0], lch_data[i * 3 + 1], lch_data[i * 3 + 2]}};

        /* Lab -> LCh */
        alwan_vec3 lch;
        alwan_lab_to_lch(&lab, &lch);

        /* For achromatic colors (C ≈ 0), hue is undefined - only check L and C */
        alwan_scalar L_err = ALWAN_FABS(lch.v[0] - lch_expected.v[0]);
        alwan_scalar C_err = ALWAN_FABS(lch.v[1] - lch_expected.v[1]);
        TEST_ASSERT(L_err < tolerance, "Lab->LCh L mismatch");
        TEST_ASSERT(C_err < tolerance, "Lab->LCh C mismatch");

        /* Only check hue for chromatic colors */
        if (lch.v[1] > ALWAN_LITERAL(1.0)) {
            alwan_scalar h_err = ALWAN_FABS(lch.v[2] - lch_expected.v[2]);
            /* Handle hue wraparound */
            if (h_err > ALWAN_LITERAL(180.0)) {
                h_err = ALWAN_LITERAL(360.0) - h_err;
            }
            TEST_ASSERT(h_err < tolerance, "Lab->LCh h mismatch");
        }

        /* LCh -> Lab (round-trip) */
        alwan_vec3 lab_roundtrip;
        alwan_lch_to_lab(&lch, &lab_roundtrip);
        alwan_scalar diff_roundtrip = vec3_max_diff(&lab, &lab_roundtrip);
        TEST_ASSERT(diff_roundtrip < tolerance, "LCh->Lab round-trip mismatch");
    }

    TEST_PASS("Lab <-> LCh round-trip");
}

static int test_luv_lchuv_roundtrip(void) {
    /* Load Luv and LCh(uv) test values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const luv_data[] = {
#include "reference_values/xyz_to_luv_d65.csv"
    };
    static alwan_scalar const lchuv_data[] = {
#include "reference_values/luv_to_lchuv.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(luv_data) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(5e-5);
#else
    /* Relaxed tolerance for trigonometric functions (atan2) - different math libraries */
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-8);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 luv = {{luv_data[i * 3 + 0], luv_data[i * 3 + 1], luv_data[i * 3 + 2]}};
        alwan_vec3 lchuv_expected = {{lchuv_data[i * 3 + 0], lchuv_data[i * 3 + 1], lchuv_data[i * 3 + 2]}};

        /* Luv -> LCh(uv) */
        alwan_vec3 lchuv;
        alwan_luv_to_lchuv(&luv, &lchuv);

        /* For achromatic colors (C ≈ 0), hue is undefined - only check L and C */
        alwan_scalar L_err = ALWAN_FABS(lchuv.v[0] - lchuv_expected.v[0]);
        alwan_scalar C_err = ALWAN_FABS(lchuv.v[1] - lchuv_expected.v[1]);
        TEST_ASSERT(L_err < tolerance, "Luv->LChuv L mismatch");
        TEST_ASSERT(C_err < tolerance, "Luv->LChuv C mismatch");

        /* Only check hue for chromatic colors */
        if (lchuv.v[1] > ALWAN_LITERAL(1.0)) {
            alwan_scalar h_err = ALWAN_FABS(lchuv.v[2] - lchuv_expected.v[2]);
            /* Handle hue wraparound */
            if (h_err > ALWAN_LITERAL(180.0)) {
                h_err = ALWAN_LITERAL(360.0) - h_err;
            }
            TEST_ASSERT(h_err < tolerance, "Luv->LChuv h mismatch");
        }

        /* LCh(uv) -> Luv (round-trip) */
        alwan_vec3 luv_roundtrip;
        alwan_lchuv_to_luv(&lchuv, &luv_roundtrip);
        alwan_scalar diff_roundtrip = vec3_max_diff(&luv, &luv_roundtrip);
        TEST_ASSERT(diff_roundtrip < tolerance, "LCh(uv)->Luv round-trip mismatch");
    }

    TEST_PASS("Luv <-> LCh(uv) round-trip");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_05_xyz_lab_luv_main(void) {
    int failures = 0;

    failures += test_xyz_xyy_roundtrip();
    failures += test_xyz_lab_d65_roundtrip();
    failures += test_xyz_lab_d50_roundtrip();
    failures += test_xyz_luv_d65_roundtrip();
    failures += test_lab_lch_roundtrip();
    failures += test_luv_lchuv_roundtrip();

    if (failures == 0) {
        printf("\n=== All color space conversion tests passed ===\n");
        return 0;
    } else {
        fprintf(stderr, "\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
