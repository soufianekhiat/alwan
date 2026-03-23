/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for ATD95 Color Vision Model
 */

#include "test_common.h"
#include <stdlib.h>

/* Test data from CSV: XYZ_in (3), XYZ_w (3), Y_0, k1, k2, sigma, H, C, Br
 * Format: 13 values per row */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const test_data[] = {
#include "reference_values/atd95.csv"
};
ALWAN_DIAG_POP

static size_t const num_test_cases = sizeof(test_data) / sizeof(test_data[0]) / 13;

/* ----------------------------------------------------------------
 * Test: ATD95 Forward Transform
 * ---------------------------------------------------------------- */
static int test_atd95_forward(void) {
    printf("\n=== Testing ATD95 Forward Transform ===\n");

    int failed = 0;

    for (size_t i = 0; i < num_test_cases; i++) {
        size_t offset = i * 13;
        alwan_vec3 xyz_in = {test_data[offset + 0], test_data[offset + 1], test_data[offset + 2]};
        alwan_vec3 xyz_w = {test_data[offset + 3], test_data[offset + 4], test_data[offset + 5]};
        alwan_scalar Y_0 = test_data[offset + 6];
        alwan_scalar k1 = test_data[offset + 7];
        alwan_scalar k2 = test_data[offset + 8];
        alwan_scalar sigma = test_data[offset + 9];

        alwan_atd95_viewing_conditions vc;
        ALWAN_MEMCPY(&vc.white_xyz, &xyz_w, sizeof(alwan_xyz));
        vc.Y_0 = Y_0;
        vc.k1 = k1;
        vc.k2 = k2;
        vc.sigma = sigma;

        alwan_xyz xyz_typed;
        ALWAN_MEMCPY(&xyz_typed, &xyz_in, sizeof(alwan_xyz));
        alwan_atd95_correlates result;
        int status = alwan_atd95_forward(&result, &xyz_typed, &vc);

        if (status != ALWAN_OK) {
            printf("  Test %zu: FAILED - Status %d\n", i + 1, status);
            failed++;
            continue;
        }

        printf("  Test %zu: H=%.1f, C=%.3f, Br=%.3f\n",
               i + 1, result.H, result.C, result.Br);
    }

    if (failed == 0) {
        printf("\n[OK] All %zu ATD95 forward transform tests passed!\n", num_test_cases);
    } else {
        printf("\n[FAIL] %d/%zu ATD95 forward transform tests failed\n",
               failed, num_test_cases);
    }

    return failed;
}

/* ----------------------------------------------------------------
 * Test: ATD95 Adaptation Parameters
 * ---------------------------------------------------------------- */
static int test_atd95_adaptation(void) {
    printf("\n=== Testing ATD95 Adaptation Parameters ===\n");

    int failed = 0;

    /* D65 illuminant */
    alwan_vec3 d65 = {ALWAN_LITERAL(95.05),
                      ALWAN_LITERAL(100.0),
                      ALWAN_LITERAL(108.88)};

    /* Test color: mid-gray */
    alwan_vec3 xyz = {ALWAN_LITERAL(50.0),
                      ALWAN_LITERAL(50.0),
                      ALWAN_LITERAL(50.0)};

    alwan_atd95_viewing_conditions vc;
    ALWAN_MEMCPY(&vc.white_xyz, &d65, sizeof(alwan_xyz));
    vc.Y_0 = ALWAN_LITERAL(318.31);
    vc.sigma = ALWAN_LITERAL(300.0);

    alwan_atd95_correlates corr_unrelated, corr_related;

    /* Test unrelated colors (k1=1, k2=0) */
    vc.k1 = ALWAN_LITERAL(1.0);
    vc.k2 = ALWAN_LITERAL(0.0);
    alwan_xyz xyz_typed;
    ALWAN_MEMCPY(&xyz_typed, &xyz, sizeof(alwan_xyz));
    int status = alwan_atd95_forward(&corr_unrelated, &xyz_typed, &vc);
    if (status != ALWAN_OK) {
        printf("  Unrelated colors: FAILED (status %d)\n", status);
        failed++;
    } else {
        printf("  Unrelated (k1=1, k2=0): Br=%.3f, C=%.3f\n",
               corr_unrelated.Br, corr_unrelated.C);
    }

    /* Test related colors (k1=0, k2=50) */
    vc.k1 = ALWAN_LITERAL(0.0);
    vc.k2 = ALWAN_LITERAL(50.0);
    status = alwan_atd95_forward(&corr_related, &xyz_typed, &vc);
    if (status != ALWAN_OK) {
        printf("  Related colors: FAILED (status %d)\n", status);
        failed++;
    } else {
        printf("  Related (k1=0, k2=50): Br=%.3f, C=%.3f\n",
               corr_related.Br, corr_related.C);
    }

    if (failed == 0) {
        printf("\n[OK] All adaptation parameter tests passed!\n");
    } else {
        printf("\n[FAIL] %d adaptation parameter tests failed\n", failed);
    }

    return failed;
}

/* ----------------------------------------------------------------
 * Main Test Runner
 * ---------------------------------------------------------------- */
int test_48_atd95_main(void) {
    int failed = 0;

    printf("\n========================================\n");
    printf("ATD95 Color Vision Model Tests\n");
    printf("========================================\n");

    failed += test_atd95_forward();
    failed += test_atd95_adaptation();

    if (failed == 0) {
        printf("\n========================================\n");
        printf("All ATD95 tests PASSED!\n");
        printf("========================================\n");
        return 0;
    } else {
        printf("\n========================================\n");
        printf("%d ATD95 tests FAILED!\n", failed);
        printf("========================================\n");
        return 1;
    }
}
