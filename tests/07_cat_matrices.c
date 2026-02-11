/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 07: Chromatic Adaptation Transform (CAT) matrices
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Test helpers
 * ---------------------------------------------------------------- */

static alwan_scalar mat3_max_diff(alwan_mat3x3 const *a, alwan_mat3x3 const *b) {
    alwan_scalar max_diff = 0;
    for (int i = 0; i < 9; i++) {
        alwan_scalar diff = ALWAN_ABS(a->m[i] - b->m[i]);
        if (diff > max_diff) max_diff = diff;
    }
    return max_diff;
}

static void mat3_print(char const *name, alwan_mat3x3 const *m) {
    printf("%s:\n", name);
    for (int row = 0; row < 3; row++) {
        printf("  [%12.8f %12.8f %12.8f]\n",
               m->m[row * 3 + 0], m->m[row * 3 + 1], m->m[row * 3 + 2]);
    }
}

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_cat_d65_to_d50_bradford(void) {
    /* Load white points and expected matrix */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const d65_xyz_data[] = {
#include "reference_values/test_d65_white.csv"
    };
    static alwan_scalar const d50_xyz_data[] = {
#include "reference_values/test_d50_white.csv"
    };
    static alwan_scalar const expected_matrix_data[] = {
#include "reference_values/cat_d65_to_d50_bradford.csv"
    };
    ALWAN_DIAG_POP

    alwan_xyz d65_xyz = {d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]};
    alwan_xyz d50_xyz = {d50_xyz_data[0], d50_xyz_data[1], d50_xyz_data[2]};
    alwan_mat3x3 expected_matrix;
    memcpy(expected_matrix.m, expected_matrix_data, sizeof(expected_matrix_data));

    /* Compute CAT matrix */
    alwan_mat3x3 computed_matrix;
    int status = alwan_cat_matrix(&computed_matrix, &d65_xyz, &d50_xyz, ALWAN_CAT_BRADFORD);
    TEST_ASSERT(status == ALWAN_OK, "CAT matrix computation failed");

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-12);
#endif

    alwan_scalar diff = mat3_max_diff(&computed_matrix, &expected_matrix);
    if (diff >= tolerance) {
        printf("  Max diff: %e (tolerance: %e)\n", diff, tolerance);
        mat3_print("  Computed", &computed_matrix);
        mat3_print("  Expected", &expected_matrix);
    }
    TEST_ASSERT(diff < tolerance, "D65->D50 Bradford matrix mismatch");

    TEST_PASS("CAT D65->D50 (Bradford)");
}

static int test_cat_d50_to_d65_bradford(void) {
    /* Load white points and expected matrix */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const d65_xyz_data[] = {
#include "reference_values/test_d65_white.csv"
    };
    static alwan_scalar const d50_xyz_data[] = {
#include "reference_values/test_d50_white.csv"
    };
    static alwan_scalar const expected_matrix_data[] = {
#include "reference_values/cat_d50_to_d65_bradford.csv"
    };
    ALWAN_DIAG_POP

    alwan_xyz d65_xyz = {d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]};
    alwan_xyz d50_xyz = {d50_xyz_data[0], d50_xyz_data[1], d50_xyz_data[2]};
    alwan_mat3x3 expected_matrix;
    memcpy(expected_matrix.m, expected_matrix_data, sizeof(expected_matrix_data));

    /* Compute CAT matrix (reverse direction) */
    alwan_mat3x3 computed_matrix;
    int status = alwan_cat_matrix(&computed_matrix, &d50_xyz, &d65_xyz, ALWAN_CAT_BRADFORD);
    TEST_ASSERT(status == ALWAN_OK, "CAT matrix computation failed");

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-12);
#endif

    alwan_scalar diff = mat3_max_diff(&computed_matrix, &expected_matrix);
    TEST_ASSERT(diff < tolerance, "D50->D65 Bradford matrix mismatch");

    TEST_PASS("CAT D50->D65 (Bradford)");
}

static int test_cat_a_to_d65_bradford(void) {
    /* Load white points and expected matrix */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const d65_xyz_data[] = {
#include "reference_values/test_d65_white.csv"
    };
    static alwan_scalar const a_xyz_data[] = {
#include "reference_values/a_xyz.csv"
    };
    static alwan_scalar const expected_matrix_data[] = {
#include "reference_values/cat_a_to_d65_bradford.csv"
    };
    ALWAN_DIAG_POP

    alwan_xyz d65_xyz = {d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]};
    alwan_xyz a_xyz = {a_xyz_data[0], a_xyz_data[1], a_xyz_data[2]};
    alwan_mat3x3 expected_matrix;
    memcpy(expected_matrix.m, expected_matrix_data, sizeof(expected_matrix_data));

    /* Compute CAT matrix */
    alwan_mat3x3 computed_matrix;
    int status = alwan_cat_matrix(&computed_matrix, &a_xyz, &d65_xyz, ALWAN_CAT_BRADFORD);
    TEST_ASSERT(status == ALWAN_OK, "CAT matrix computation failed");

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-12);
#endif

    alwan_scalar diff = mat3_max_diff(&computed_matrix, &expected_matrix);
    TEST_ASSERT(diff < tolerance, "A->D65 Bradford matrix mismatch");

    TEST_PASS("CAT A->D65 (Bradford)");
}

static int test_cat_d65_to_d60_bradford(void) {
    /* Load white points and expected matrix */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const d65_xyz_data[] = {
#include "reference_values/test_d65_white.csv"
    };
    static alwan_scalar const d60_xyz_data[] = {
#include "reference_values/d60_xyz.csv"
    };
    static alwan_scalar const expected_matrix_data[] = {
#include "reference_values/cat_d65_to_d60_bradford.csv"
    };
    ALWAN_DIAG_POP

    alwan_xyz d65_xyz = {d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]};
    alwan_xyz d60_xyz = {d60_xyz_data[0], d60_xyz_data[1], d60_xyz_data[2]};
    alwan_mat3x3 expected_matrix;
    memcpy(expected_matrix.m, expected_matrix_data, sizeof(expected_matrix_data));

    /* Compute CAT matrix */
    alwan_mat3x3 computed_matrix;
    int status = alwan_cat_matrix(&computed_matrix, &d65_xyz, &d60_xyz, ALWAN_CAT_BRADFORD);
    TEST_ASSERT(status == ALWAN_OK, "CAT matrix computation failed");

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-12);
#endif

    alwan_scalar diff = mat3_max_diff(&computed_matrix, &expected_matrix);
    TEST_ASSERT(diff < tolerance, "D65->D60 Bradford matrix mismatch");

    TEST_PASS("CAT D65->D60 (Bradford)");
}

static int test_cat_d65_to_d50_cat02(void) {
    /* Load white points and expected matrix */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const d65_xyz_data[] = {
#include "reference_values/test_d65_white.csv"
    };
    static alwan_scalar const d50_xyz_data[] = {
#include "reference_values/test_d50_white.csv"
    };
    static alwan_scalar const expected_matrix_data[] = {
#include "reference_values/cat_d65_to_d50_cat02.csv"
    };
    ALWAN_DIAG_POP

    alwan_xyz d65_xyz = {d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]};
    alwan_xyz d50_xyz = {d50_xyz_data[0], d50_xyz_data[1], d50_xyz_data[2]};
    alwan_mat3x3 expected_matrix;
    memcpy(expected_matrix.m, expected_matrix_data, sizeof(expected_matrix_data));

    /* Compute CAT matrix */
    alwan_mat3x3 computed_matrix;
    int status = alwan_cat_matrix(&computed_matrix, &d65_xyz, &d50_xyz, ALWAN_CAT_CAT02);
    TEST_ASSERT(status == ALWAN_OK, "CAT matrix computation failed");

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-12);
#endif

    alwan_scalar diff = mat3_max_diff(&computed_matrix, &expected_matrix);
    TEST_ASSERT(diff < tolerance, "D65->D50 CAT02 matrix mismatch");

    TEST_PASS("CAT D65->D50 (CAT02)");
}

static int test_cat_d65_to_d50_cat16(void) {
    /* Load white points and expected matrix */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const d65_xyz_data[] = {
#include "reference_values/test_d65_white.csv"
    };
    static alwan_scalar const d50_xyz_data[] = {
#include "reference_values/test_d50_white.csv"
    };
    static alwan_scalar const expected_matrix_data[] = {
#include "reference_values/cat_d65_to_d50_cat16.csv"
    };
    ALWAN_DIAG_POP

    alwan_xyz d65_xyz = {d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]};
    alwan_xyz d50_xyz = {d50_xyz_data[0], d50_xyz_data[1], d50_xyz_data[2]};
    alwan_mat3x3 expected_matrix;
    memcpy(expected_matrix.m, expected_matrix_data, sizeof(expected_matrix_data));

    /* Compute CAT matrix */
    alwan_mat3x3 computed_matrix;
    int status = alwan_cat_matrix(&computed_matrix, &d65_xyz, &d50_xyz, ALWAN_CAT_CAT16);
    TEST_ASSERT(status == ALWAN_OK, "CAT matrix computation failed");

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-12);
#endif

    alwan_scalar diff = mat3_max_diff(&computed_matrix, &expected_matrix);
    TEST_ASSERT(diff < tolerance, "D65->D50 CAT16 matrix mismatch");

    TEST_PASS("CAT D65->D50 (CAT16)");
}

static int test_cat_d65_to_d50_xyz_scaling(void) {
    /* Load white points and expected matrix */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const d65_xyz_data[] = {
#include "reference_values/test_d65_white.csv"
    };
    static alwan_scalar const d50_xyz_data[] = {
#include "reference_values/test_d50_white.csv"
    };
    static alwan_scalar const expected_matrix_data[] = {
#include "reference_values/cat_d65_to_d50_xyz_scaling.csv"
    };
    ALWAN_DIAG_POP

    alwan_xyz d65_xyz = {d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]};
    alwan_xyz d50_xyz = {d50_xyz_data[0], d50_xyz_data[1], d50_xyz_data[2]};
    alwan_mat3x3 expected_matrix;
    memcpy(expected_matrix.m, expected_matrix_data, sizeof(expected_matrix_data));

    /* Compute CAT matrix */
    alwan_mat3x3 computed_matrix;
    int status = alwan_cat_matrix(&computed_matrix, &d65_xyz, &d50_xyz, ALWAN_CAT_XYZ_SCALING);
    TEST_ASSERT(status == ALWAN_OK, "CAT matrix computation failed");

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-12);
#endif

    alwan_scalar diff = mat3_max_diff(&computed_matrix, &expected_matrix);
    TEST_ASSERT(diff < tolerance, "D65->D50 XYZ Scaling matrix mismatch");

    TEST_PASS("CAT D65->D50 (XYZ Scaling)");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_07_cat_matrices_main(void) {
    int failures = 0;

    failures += test_cat_d65_to_d50_bradford();
    failures += test_cat_d50_to_d65_bradford();
    failures += test_cat_a_to_d65_bradford();
    failures += test_cat_d65_to_d60_bradford();
    failures += test_cat_d65_to_d50_cat02();
    failures += test_cat_d65_to_d50_cat16();
    failures += test_cat_d65_to_d50_xyz_scaling();

    if (failures == 0) {
        printf("\n=== All CAT matrix tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
