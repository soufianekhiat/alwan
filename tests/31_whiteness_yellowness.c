/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 31: Whiteness & Yellowness Indices (P4.6)
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_yellowness_astm_e313(void) {
    /* Load test XYZ samples and expected yellowness values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const xyz_data[] = {
#include "reference_values/whiteness_test_xyz.csv"
    };
    static alwan_scalar const yi_c_2deg_data[] = {
#include "reference_values/yellowness_c_2deg.csv"
    };
    static alwan_scalar const yi_d65_2deg_data[] = {
#include "reference_values/yellowness_d65_2deg.csv"
    };
    static alwan_scalar const yi_c_10deg_data[] = {
#include "reference_values/yellowness_c_10deg.csv"
    };
    static alwan_scalar const yi_d65_10deg_data[] = {
#include "reference_values/yellowness_d65_10deg.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(xyz_data) / (3 * sizeof(alwan_scalar));
    alwan_scalar const tolerance = ALWAN_TEST_TOLERANCE;

    /* Test all illuminant/observer combinations */
    for (int i = 0; i < num_tests; i++) {
        alwan_xyz xyz = {xyz_data[i * 3 + 0], xyz_data[i * 3 + 1], xyz_data[i * 3 + 2]};

        /* C/2-deg */
        alwan_scalar expected_c_2 = yi_c_2deg_data[i];
        alwan_scalar result_c_2 = alwan_yellowness_astm_e313(&xyz, ALWAN_ASTM_E313_C_2DEG);
        alwan_scalar diff_c_2 = ALWAN_ABS(result_c_2 - expected_c_2);
        if (diff_c_2 >= tolerance) {
            printf("Sample %d: Expected %.15f, Got %.15f, Diff %.15e\n", i, expected_c_2, result_c_2, diff_c_2);
        }
        TEST_ASSERT(diff_c_2 < tolerance, "Yellowness C/2-deg mismatch");

        /* D65/2-deg */
        alwan_scalar expected_d65_2 = yi_d65_2deg_data[i];
        alwan_scalar result_d65_2 = alwan_yellowness_astm_e313(&xyz, ALWAN_ASTM_E313_D65_2DEG);
        alwan_scalar diff_d65_2 = ALWAN_ABS(result_d65_2 - expected_d65_2);
        TEST_ASSERT(diff_d65_2 < tolerance, "Yellowness D65/2-deg mismatch");

        /* C/10-deg */
        alwan_scalar expected_c_10 = yi_c_10deg_data[i];
        alwan_scalar result_c_10 = alwan_yellowness_astm_e313(&xyz, ALWAN_ASTM_E313_C_10DEG);
        alwan_scalar diff_c_10 = ALWAN_ABS(result_c_10 - expected_c_10);
        TEST_ASSERT(diff_c_10 < tolerance, "Yellowness C/10-deg mismatch");

        /* D65/10-deg */
        alwan_scalar expected_d65_10 = yi_d65_10deg_data[i];
        alwan_scalar result_d65_10 = alwan_yellowness_astm_e313(&xyz, ALWAN_ASTM_E313_D65_10DEG);
        alwan_scalar diff_d65_10 = ALWAN_ABS(result_d65_10 - expected_d65_10);
        TEST_ASSERT(diff_d65_10 < tolerance, "Yellowness D65/10-deg mismatch");
    }

    TEST_PASS("ASTM E313 Yellowness Index");
}

static int test_whiteness_astm_e313(void) {
    /* Load test XYZ samples and expected whiteness values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const xyz_data[] = {
#include "reference_values/whiteness_test_xyz.csv"
    };
    static alwan_scalar const wi_c_2deg_data[] = {
#include "reference_values/whiteness_c_2deg.csv"
    };
    static alwan_scalar const wi_d65_2deg_data[] = {
#include "reference_values/whiteness_d65_2deg.csv"
    };
    static alwan_scalar const wi_c_10deg_data[] = {
#include "reference_values/whiteness_c_10deg.csv"
    };
    static alwan_scalar const wi_d65_10deg_data[] = {
#include "reference_values/whiteness_d65_10deg.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(xyz_data) / (3 * sizeof(alwan_scalar));
    alwan_scalar const tolerance = ALWAN_TEST_TOLERANCE;

    /* Test all illuminant/observer combinations */
    for (int i = 0; i < num_tests; i++) {
        alwan_xyz xyz = {xyz_data[i * 3 + 0], xyz_data[i * 3 + 1], xyz_data[i * 3 + 2]};

        /* C/2-deg */
        alwan_scalar expected_c_2 = wi_c_2deg_data[i];
        alwan_scalar result_c_2 = alwan_whiteness_astm_e313(&xyz, ALWAN_ASTM_E313_C_2DEG);
        alwan_scalar diff_c_2 = ALWAN_ABS(result_c_2 - expected_c_2);
        if (diff_c_2 >= tolerance) {
            printf("Sample %d: Expected %.15f, Got %.15f, Diff %.15e\n", i, expected_c_2, result_c_2, diff_c_2);
        }
        TEST_ASSERT(diff_c_2 < tolerance, "Whiteness C/2-deg mismatch");

        /* D65/2-deg */
        alwan_scalar expected_d65_2 = wi_d65_2deg_data[i];
        alwan_scalar result_d65_2 = alwan_whiteness_astm_e313(&xyz, ALWAN_ASTM_E313_D65_2DEG);
        alwan_scalar diff_d65_2 = ALWAN_ABS(result_d65_2 - expected_d65_2);
        TEST_ASSERT(diff_d65_2 < tolerance, "Whiteness D65/2-deg mismatch");

        /* C/10-deg */
        alwan_scalar expected_c_10 = wi_c_10deg_data[i];
        alwan_scalar result_c_10 = alwan_whiteness_astm_e313(&xyz, ALWAN_ASTM_E313_C_10DEG);
        alwan_scalar diff_c_10 = ALWAN_ABS(result_c_10 - expected_c_10);
        TEST_ASSERT(diff_c_10 < tolerance, "Whiteness C/10-deg mismatch");

        /* D65/10-deg */
        alwan_scalar expected_d65_10 = wi_d65_10deg_data[i];
        alwan_scalar result_d65_10 = alwan_whiteness_astm_e313(&xyz, ALWAN_ASTM_E313_D65_10DEG);
        alwan_scalar diff_d65_10 = ALWAN_ABS(result_d65_10 - expected_d65_10);
        TEST_ASSERT(diff_d65_10 < tolerance, "Whiteness D65/10-deg mismatch");
    }

    TEST_PASS("ASTM E313 Whiteness Index");
}

static int test_whiteness_cie2004(void) {
    /* Load test XYZ samples and expected CIE 2004 whiteness values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const xyz_data[] = {
#include "reference_values/whiteness_test_xyz.csv"
    };
    static alwan_scalar const wi_cie2004_data[] = {
#include "reference_values/whiteness_cie2004.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(xyz_data) / (3 * sizeof(alwan_scalar));
    alwan_scalar const tolerance = ALWAN_TEST_TOLERANCE;

    /* D65/2-deg reference white for CIE 2004 */
    alwan_vec2 xy_n = {ALWAN_LITERAL(0.3127), ALWAN_LITERAL(0.3290)};

    for (int i = 0; i < num_tests; i++) {
        alwan_xyz xyz = {xyz_data[i * 3 + 0], xyz_data[i * 3 + 1], xyz_data[i * 3 + 2]};
        alwan_scalar expected = wi_cie2004_data[i];

        /* Convert XYZ to xy */
        alwan_scalar sum = xyz.x + xyz.y + xyz.z;
        alwan_vec2 xy = {xyz.x / sum, xyz.y / sum};
        alwan_scalar Y = xyz.y;

        alwan_scalar result = alwan_whiteness_cie2004(&xy, Y, &xy_n);
        alwan_scalar diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < tolerance, "CIE 2004 Whiteness mismatch");
    }

    TEST_PASS("CIE 2004 Whiteness Index");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_31_whiteness_yellowness_main(void) {
    int failures = 0;

    failures += test_yellowness_astm_e313();
    failures += test_whiteness_astm_e313();
    failures += test_whiteness_cie2004();

    if (failures == 0) {
        printf("\n=== All whiteness & yellowness tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
