/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 05: XYZ/xyY/Lab/Luv/LCh color space conversions
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Test helpers
 * ---------------------------------------------------------------- */

static alwan_scalar vec3_max_diff(alwan_vec3 const *a, alwan_vec3 const *b) {
    alwan_scalar max_diff = 0;
    for (int i = 0; i < 3; i++) {
        alwan_scalar diff = ALWAN_ABS(a->v[i] - b->v[i]);
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
    alwan_scalar const tolerance = ALWAN_TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_xyz xyz;
        xyz.x = xyz_data[i * 3 + 0];
        xyz.y = xyz_data[i * 3 + 1];
        xyz.z = xyz_data[i * 3 + 2];
        alwan_xyy xyy_expected;
        xyy_expected.x = xyy_data[i * 3 + 0];
        xyy_expected.y = xyy_data[i * 3 + 1];
        xyy_expected.Y = xyy_data[i * 3 + 2];

        /* XYZ -> xyY */
        alwan_xyy xyy;
        alwan_xyz_to_xyy(&xyy, &xyz);
        alwan_vec3 xyy_vec, xyy_expected_vec;
        ALWAN_MEMCPY(&xyy_vec, &xyy, sizeof(alwan_vec3));
        ALWAN_MEMCPY(&xyy_expected_vec, &xyy_expected, sizeof(alwan_vec3));
        alwan_scalar diff_forward = vec3_max_diff(&xyy_vec, &xyy_expected_vec);
        if (diff_forward >= tolerance) {
            printf("Test %d: diff=%e (tol=%e)\n", i, diff_forward, tolerance);
            vec3_print("  Computed", &xyy_vec);
            vec3_print("  Expected", &xyy_expected_vec);
        }
        TEST_ASSERT(diff_forward < tolerance, "XYZ->xyY mismatch");

        /* xyY -> XYZ (round-trip) */
        alwan_xyz xyz_roundtrip;
        alwan_xyy_to_xyz(&xyz_roundtrip, &xyy);
        alwan_vec3 xyz_vec, xyz_roundtrip_vec;
        ALWAN_MEMCPY(&xyz_vec, &xyz, sizeof(alwan_vec3));
        ALWAN_MEMCPY(&xyz_roundtrip_vec, &xyz_roundtrip, sizeof(alwan_vec3));
        alwan_scalar diff_roundtrip = vec3_max_diff(&xyz_vec, &xyz_roundtrip_vec);
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

    alwan_xyz white_xyz;
    white_xyz.x = d65_xyz_data[0];
    white_xyz.y = d65_xyz_data[1];
    white_xyz.z = d65_xyz_data[2];

    int const num_tests = sizeof(xyz_data) / (3 * sizeof(alwan_scalar));
    alwan_scalar const tolerance = ALWAN_TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_xyz xyz;
        xyz.x = xyz_data[i * 3 + 0];
        xyz.y = xyz_data[i * 3 + 1];
        xyz.z = xyz_data[i * 3 + 2];
        alwan_lab lab_expected;
        lab_expected.L = lab_data[i * 3 + 0];
        lab_expected.a = lab_data[i * 3 + 1];
        lab_expected.b = lab_data[i * 3 + 2];

        /* XYZ -> Lab */
        alwan_lab lab;
        alwan_xyz_to_lab(&lab, &xyz, &white_xyz);
        alwan_vec3 lab_vec, lab_expected_vec;
        ALWAN_MEMCPY(&lab_vec, &lab, sizeof(alwan_vec3));
        ALWAN_MEMCPY(&lab_expected_vec, &lab_expected, sizeof(alwan_vec3));
        alwan_scalar diff_forward = vec3_max_diff(&lab_vec, &lab_expected_vec);
        if (diff_forward >= tolerance) {
            printf("Lab D65 test %d: diff=%e (tol=%e)\n", i, diff_forward, tolerance);
            vec3_print("  Computed", &lab_vec);
            vec3_print("  Expected", &lab_expected_vec);
        }
        TEST_ASSERT(diff_forward < tolerance, "XYZ->Lab mismatch (D65)");

        /* Lab -> XYZ (round-trip) */
        alwan_xyz xyz_roundtrip;
        alwan_lab_to_xyz(&xyz_roundtrip, &lab, &white_xyz);
        alwan_vec3 xyz_vec, xyz_roundtrip_vec;
        ALWAN_MEMCPY(&xyz_vec, &xyz, sizeof(alwan_vec3));
        ALWAN_MEMCPY(&xyz_roundtrip_vec, &xyz_roundtrip, sizeof(alwan_vec3));
        alwan_scalar diff_roundtrip = vec3_max_diff(&xyz_vec, &xyz_roundtrip_vec);
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

    alwan_xyz white_xyz;
    white_xyz.x = d50_xyz_data[0];
    white_xyz.y = d50_xyz_data[1];
    white_xyz.z = d50_xyz_data[2];

    int const num_tests = sizeof(xyz_data) / (3 * sizeof(alwan_scalar));
    alwan_scalar const tolerance = ALWAN_TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_xyz xyz;
        xyz.x = xyz_data[i * 3 + 0];
        xyz.y = xyz_data[i * 3 + 1];
        xyz.z = xyz_data[i * 3 + 2];
        alwan_lab lab_expected;
        lab_expected.L = lab_data[i * 3 + 0];
        lab_expected.a = lab_data[i * 3 + 1];
        lab_expected.b = lab_data[i * 3 + 2];

        /* XYZ -> Lab */
        alwan_lab lab;
        alwan_xyz_to_lab(&lab, &xyz, &white_xyz);
        alwan_vec3 lab_vec, lab_expected_vec;
        ALWAN_MEMCPY(&lab_vec, &lab, sizeof(alwan_vec3));
        ALWAN_MEMCPY(&lab_expected_vec, &lab_expected, sizeof(alwan_vec3));
        alwan_scalar diff_forward = vec3_max_diff(&lab_vec, &lab_expected_vec);
        TEST_ASSERT(diff_forward < tolerance, "XYZ->Lab mismatch (D50)");

        /* Lab -> XYZ (round-trip) */
        alwan_xyz xyz_roundtrip;
        alwan_lab_to_xyz(&xyz_roundtrip, &lab, &white_xyz);
        alwan_vec3 xyz_vec, xyz_roundtrip_vec;
        ALWAN_MEMCPY(&xyz_vec, &xyz, sizeof(alwan_vec3));
        ALWAN_MEMCPY(&xyz_roundtrip_vec, &xyz_roundtrip, sizeof(alwan_vec3));
        alwan_scalar diff_roundtrip = vec3_max_diff(&xyz_vec, &xyz_roundtrip_vec);
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

    alwan_xyz white_xyz;
    white_xyz.x = d65_xyz_data[0];
    white_xyz.y = d65_xyz_data[1];
    white_xyz.z = d65_xyz_data[2];

    int const num_tests = sizeof(xyz_data) / (3 * sizeof(alwan_scalar));
    alwan_scalar const tolerance = ALWAN_TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_xyz xyz;
        xyz.x = xyz_data[i * 3 + 0];
        xyz.y = xyz_data[i * 3 + 1];
        xyz.z = xyz_data[i * 3 + 2];
        alwan_luv luv_expected;
        luv_expected.L = luv_data[i * 3 + 0];
        luv_expected.u = luv_data[i * 3 + 1];
        luv_expected.v = luv_data[i * 3 + 2];

        /* XYZ -> Luv */
        alwan_luv luv;
        alwan_xyz_to_luv(&luv, &xyz, &white_xyz);
        alwan_vec3 luv_vec, luv_expected_vec;
        ALWAN_MEMCPY(&luv_vec, &luv, sizeof(alwan_vec3));
        ALWAN_MEMCPY(&luv_expected_vec, &luv_expected, sizeof(alwan_vec3));
        alwan_scalar diff_forward = vec3_max_diff(&luv_vec, &luv_expected_vec);
        TEST_ASSERT(diff_forward < tolerance, "XYZ->Luv mismatch (D65)");

        /* Luv -> XYZ (round-trip) */
        alwan_xyz xyz_roundtrip;
        alwan_luv_to_xyz(&xyz_roundtrip, &luv, &white_xyz);
        alwan_vec3 xyz_vec, xyz_roundtrip_vec;
        ALWAN_MEMCPY(&xyz_vec, &xyz, sizeof(alwan_vec3));
        ALWAN_MEMCPY(&xyz_roundtrip_vec, &xyz_roundtrip, sizeof(alwan_vec3));
        alwan_scalar diff_roundtrip = vec3_max_diff(&xyz_vec, &xyz_roundtrip_vec);
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
    alwan_scalar const tolerance = ALWAN_TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_lab lab;
        lab.L = lab_data[i * 3 + 0];
        lab.a = lab_data[i * 3 + 1];
        lab.b = lab_data[i * 3 + 2];
        alwan_lch lch_expected;
        lch_expected.L = lch_data[i * 3 + 0];
        lch_expected.C = lch_data[i * 3 + 1];
        lch_expected.h = lch_data[i * 3 + 2];

        /* Lab -> LCh */
        alwan_lch lch;
        alwan_lab_to_lch(&lch, &lab);

        /* For achromatic colors (C ≈ 0), hue is undefined - only check L and C */
        alwan_scalar L_err = ALWAN_ABS(lch.L - lch_expected.L);
        alwan_scalar C_err = ALWAN_ABS(lch.C - lch_expected.C);
        TEST_ASSERT(L_err < tolerance, "Lab->LCh L mismatch");
        TEST_ASSERT(C_err < tolerance, "Lab->LCh C mismatch");

        /* Only check hue for chromatic colors */
        if (lch.C > ALWAN_LITERAL(1.0)) {
            alwan_scalar h_err = ALWAN_ABS(lch.h - lch_expected.h);
            /* Handle hue wraparound */
            if (h_err > ALWAN_LITERAL(180.0)) {
                h_err = ALWAN_LITERAL(360.0) - h_err;
            }
            TEST_ASSERT(h_err < tolerance, "Lab->LCh h mismatch");
        }

        /* LCh -> Lab (round-trip) */
        alwan_lab lab_roundtrip;
        alwan_lch_to_lab(&lab_roundtrip, &lch);
        alwan_vec3 lab_vec, lab_roundtrip_vec;
        ALWAN_MEMCPY(&lab_vec, &lab, sizeof(alwan_vec3));
        ALWAN_MEMCPY(&lab_roundtrip_vec, &lab_roundtrip, sizeof(alwan_vec3));
        alwan_scalar diff_roundtrip = vec3_max_diff(&lab_vec, &lab_roundtrip_vec);
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
    alwan_scalar const tolerance = ALWAN_TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_luv luv;
        luv.L = luv_data[i * 3 + 0];
        luv.u = luv_data[i * 3 + 1];
        luv.v = luv_data[i * 3 + 2];
        alwan_lchuv lchuv_expected;
        lchuv_expected.L = lchuv_data[i * 3 + 0];
        lchuv_expected.C = lchuv_data[i * 3 + 1];
        lchuv_expected.h = lchuv_data[i * 3 + 2];

        /* Luv -> LCh(uv) */
        alwan_lchuv lchuv;
        alwan_luv_to_lchuv(&lchuv, &luv);

        /* For achromatic colors (C ≈ 0), hue is undefined - only check L and C */
        alwan_scalar L_err = ALWAN_ABS(lchuv.L - lchuv_expected.L);
        alwan_scalar C_err = ALWAN_ABS(lchuv.C - lchuv_expected.C);
        TEST_ASSERT(L_err < tolerance, "Luv->LChuv L mismatch");
        TEST_ASSERT(C_err < tolerance, "Luv->LChuv C mismatch");

        /* Only check hue for chromatic colors */
        if (lchuv.C > ALWAN_LITERAL(1.0)) {
            alwan_scalar h_err = ALWAN_ABS(lchuv.h - lchuv_expected.h);
            /* Handle hue wraparound */
            if (h_err > ALWAN_LITERAL(180.0)) {
                h_err = ALWAN_LITERAL(360.0) - h_err;
            }
            TEST_ASSERT(h_err < tolerance, "Luv->LChuv h mismatch");
        }

        /* LCh(uv) -> Luv (round-trip) */
        alwan_luv luv_roundtrip;
        alwan_lchuv_to_luv(&luv_roundtrip, &lchuv);
        alwan_vec3 luv_vec, luv_roundtrip_vec;
        ALWAN_MEMCPY(&luv_vec, &luv, sizeof(alwan_vec3));
        ALWAN_MEMCPY(&luv_roundtrip_vec, &luv_roundtrip, sizeof(alwan_vec3));
        alwan_scalar diff_roundtrip = vec3_max_diff(&luv_vec, &luv_roundtrip_vec);
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
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
