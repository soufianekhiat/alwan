/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * Main test runner - runs all unit tests consecutively
 */

#include <stdio.h>
#include <stdlib.h>

/* Forward declarations of test main functions */
extern int test_00_context_main(void);
extern int test_01_mat3_ops_main(void);
extern int test_02_data_embed_main(void);
extern int test_10_rgb_matrices_main(void);
extern int test_11_srgb_tf_main(void);
extern int test_20_xyz_lab_luv_main(void);
extern int test_21_delta_e_main(void);
extern int test_30_cat_matrices_main(void);
extern int test_31_cat_roundtrip_main(void);
extern int test_40_tf_hdr_main(void);
extern int test_41_view_transforms_main(void);
extern int test_50_spd_to_xyz_main(void);
extern int test_60_bandpass_2012_main(void);
extern int test_70_ciecam02_main(void);
extern int test_80_cam16_main(void);

/* Test registry */
typedef struct {
    char const *name;
    int (*test_fn)(void);
} test_suite;

static test_suite const g_test_suites[] = {
    {"00_context", test_00_context_main},
    {"01_mat3_ops", test_01_mat3_ops_main},
    {"02_data_embed", test_02_data_embed_main},
    {"10_rgb_matrices", test_10_rgb_matrices_main},
    {"11_srgb_tf", test_11_srgb_tf_main},
    {"20_xyz_lab_luv", test_20_xyz_lab_luv_main},
    {"21_delta_e", test_21_delta_e_main},
    {"30_cat_matrices", test_30_cat_matrices_main},
    {"31_cat_roundtrip", test_31_cat_roundtrip_main},
    {"40_tf_hdr", test_40_tf_hdr_main},
    {"41_view_transforms", test_41_view_transforms_main},
    {"50_spd_to_xyz", test_50_spd_to_xyz_main},
    {"60_bandpass_2012", test_60_bandpass_2012_main},
    {"70_ciecam02", test_70_ciecam02_main},
    {"80_cam16", test_80_cam16_main},
};

int main(void) {
    printf("========================================\n");
    printf("Alwan Unit Test Runner\n");
    printf("========================================\n\n");

    int total_failed = 0;
    int total_passed = 0;
    size_t const num_suites = sizeof(g_test_suites) / sizeof(g_test_suites[0]);

    for (size_t i = 0; i < num_suites; i++) {
        printf("\n");
        printf("========================================\n");
        printf("Running test suite: %s\n", g_test_suites[i].name);
        printf("========================================\n");

        int result = g_test_suites[i].test_fn();

        if (result == 0) {
            total_passed++;
            printf("[PASS] Test suite '%s' passed\n", g_test_suites[i].name);
        } else {
            total_failed++;
            printf("[FAIL] Test suite '%s' failed with code %d\n",
                   g_test_suites[i].name, result);
        }
    }

    printf("\n");
    printf("========================================\n");
    printf("Overall Results\n");
    printf("========================================\n");
    printf("Test suites passed: %d\n", total_passed);
    printf("Test suites failed: %d\n", total_failed);
    printf("Total test suites:  %zu\n", num_suites);
    printf("========================================\n");

    if (total_failed > 0) {
        printf("\nSome tests FAILED!\n");
        return 1;
    } else {
        printf("\nAll tests PASSED!\n");
        return 0;
    }
}
