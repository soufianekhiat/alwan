/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 35: P7 Extended Chromatic Adaptation Transforms (CAT)
 *
 * Reference values generated from colour-science Python library
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>

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
 * Reference Value Loading
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* White points */
static alwan_scalar const d65_xyz_data[] = {
#include "reference_values/test_d65_white.csv"
};

static alwan_scalar const d50_xyz_data[] = {
#include "reference_values/test_d50_white.csv"
};

/* P7 CAT matrices D65->D50 */
static alwan_scalar const cat_sharp_data[] = {
#include "reference_values/cat_d65_to_d50_sharp.csv"
};

static alwan_scalar const cat_fairchild_data[] = {
#include "reference_values/cat_d65_to_d50_fairchild.csv"
};

static alwan_scalar const cat_cmccat97_data[] = {
#include "reference_values/cat_d65_to_d50_cmccat97.csv"
};

static alwan_scalar const cat_cmccat2000_data[] = {
#include "reference_values/cat_d65_to_d50_cmccat2000.csv"
};

static alwan_scalar const cat_cat02_brill_2008_data[] = {
#include "reference_values/cat_d65_to_d50_cat02_brill_2008.csv"
};

static alwan_scalar const cat_bianco_2010_data[] = {
#include "reference_values/cat_d65_to_d50_bianco_2010.csv"
};

static alwan_scalar const cat_bianco_pc_2010_data[] = {
#include "reference_values/cat_d65_to_d50_bianco_pc_2010.csv"
};

/* P7 adapted XYZ colors D65->D50 */
static alwan_scalar const adapted_sharp_data[] = {
#include "reference_values/adapted_d65_to_d50_sharp.csv"
};

static alwan_scalar const adapted_fairchild_data[] = {
#include "reference_values/adapted_d65_to_d50_fairchild.csv"
};

static alwan_scalar const adapted_cmccat97_data[] = {
#include "reference_values/adapted_d65_to_d50_cmccat97.csv"
};

static alwan_scalar const adapted_cmccat2000_data[] = {
#include "reference_values/adapted_d65_to_d50_cmccat2000.csv"
};

static alwan_scalar const adapted_cat02_brill_2008_data[] = {
#include "reference_values/adapted_d65_to_d50_cat02_brill_2008.csv"
};

static alwan_scalar const adapted_bianco_2010_data[] = {
#include "reference_values/adapted_d65_to_d50_bianco_2010.csv"
};

static alwan_scalar const adapted_bianco_pc_2010_data[] = {
#include "reference_values/adapted_d65_to_d50_bianco_pc_2010.csv"
};

/* Test XYZ colors (6 colors × 3 components) */
static alwan_scalar const test_xyz_data[] = {
#include "reference_values/test_xyz_colors.csv"
};

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Generic CAT Test Helper
 * ---------------------------------------------------------------- */

static int test_cat_method(
    char const *name,
    alwan_cat_method method,
    alwan_scalar const *expected_matrix_data,
    alwan_scalar const *expected_adapted_data)
{
    alwan_xyz d65_xyz = {d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]};
    alwan_xyz d50_xyz = {d50_xyz_data[0], d50_xyz_data[1], d50_xyz_data[2]};

    /* Test 1: CAT matrix D65->D50 */
    alwan_mat3x3 expected_matrix;
    memcpy(expected_matrix.m, expected_matrix_data, sizeof(expected_matrix.m));

    alwan_mat3x3 computed_matrix;
    int status = alwan_cat_matrix(&computed_matrix, &d65_xyz, &d50_xyz, method);
    TEST_ASSERT(status == ALWAN_OK, "CAT matrix computation failed");

    alwan_scalar const matrix_tolerance = ALWAN_TEST_TOLERANCE;
    alwan_scalar const adapted_tolerance = ALWAN_TEST_TOLERANCE;

    alwan_scalar diff = mat3_max_diff(&computed_matrix, &expected_matrix);
    if (diff >= matrix_tolerance) {
        printf("  %s matrix max diff: %e (tolerance: %e)\n", name, diff, matrix_tolerance);
        mat3_print("  Computed", &computed_matrix);
        mat3_print("  Expected", &expected_matrix);
    }
    TEST_ASSERT(diff < matrix_tolerance, "CAT matrix mismatch");

    /* Test 2: Adapted XYZ colors D65->D50 */
    size_t const num_test_colors = 8;
    alwan_scalar adapted_xyz[24];  /* 8 colors * 3 components */

    status = alwan_xyz_adapt(adapted_xyz,
                             &d65_xyz, &d50_xyz, method,
                             test_xyz_data, num_test_colors,
                             3 * sizeof(alwan_scalar),
                             3 * sizeof(alwan_scalar));
    TEST_ASSERT(status == ALWAN_OK, "alwan_xyz_adapt failed");

    int color_failures = 0;
    for (size_t i = 0; i < num_test_colors * 3; i++) {
        alwan_scalar color_diff = ALWAN_ABS(adapted_xyz[i] - expected_adapted_data[i]);
        if (color_diff >= adapted_tolerance) {
            if (color_failures == 0) {
                printf("  %s adapted color mismatches:\n", name);
            }
            printf("    [%zu]: expected=%.8f, got=%.8f, diff=%.2e\n",
                   i, expected_adapted_data[i], adapted_xyz[i], color_diff);
            color_failures++;
        }
    }
    TEST_ASSERT(color_failures == 0, "Adapted XYZ colors mismatch");

    TEST_PASS(name);
}

/* ----------------------------------------------------------------
 * Individual CAT Tests
 * ---------------------------------------------------------------- */

static int test_cat_sharp(void) {
    return test_cat_method("CAT Sharp D65->D50", ALWAN_CAT_SHARP,
                           cat_sharp_data, adapted_sharp_data);
}

static int test_cat_fairchild(void) {
    return test_cat_method("CAT Fairchild D65->D50", ALWAN_CAT_FAIRCHILD,
                           cat_fairchild_data, adapted_fairchild_data);
}

static int test_cat_cmccat97(void) {
    return test_cat_method("CAT CMCCAT97 D65->D50", ALWAN_CAT_CMCCAT97,
                           cat_cmccat97_data, adapted_cmccat97_data);
}

static int test_cat_cmccat2000(void) {
    return test_cat_method("CAT CMCCAT2000 D65->D50", ALWAN_CAT_CMCCAT2000,
                           cat_cmccat2000_data, adapted_cmccat2000_data);
}

static int test_cat_cat02_brill_2008(void) {
    return test_cat_method("CAT CAT02 Brill 2008 D65->D50", ALWAN_CAT_CAT02_BRILL_2008,
                           cat_cat02_brill_2008_data, adapted_cat02_brill_2008_data);
}

static int test_cat_bianco_2010(void) {
    return test_cat_method("CAT Bianco 2010 D65->D50", ALWAN_CAT_BIANCO_2010,
                           cat_bianco_2010_data, adapted_bianco_2010_data);
}

static int test_cat_bianco_pc_2010(void) {
    return test_cat_method("CAT Bianco PC 2010 D65->D50", ALWAN_CAT_BIANCO_PC_2010,
                           cat_bianco_pc_2010_data, adapted_bianco_pc_2010_data);
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_35_cat_extended_main(void) {
    printf("\n=== P7: Extended Chromatic Adaptation Transforms ===\n\n");

    int failures = 0;

    failures += test_cat_sharp();
    failures += test_cat_fairchild();
    failures += test_cat_cmccat97();
    failures += test_cat_cmccat2000();
    failures += test_cat_cat02_brill_2008();
    failures += test_cat_bianco_2010();
    failures += test_cat_bianco_pc_2010();

    if (failures == 0) {
        printf("\n=== All P7 CAT tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d P7 CAT test(s) failed ===\n", failures);
        return 1;
    }
}
