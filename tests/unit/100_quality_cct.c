/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M10 Tests: Light Quality & CCT
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

/* Tolerance for CCT comparisons */
/* CCT methods have inherent approximation errors:
 * - McCamy: ~2% error above 2800K
 * - Robertson: generally <1% error
 * Using 5% tolerance to account for edge cases */
#define CCT_RELATIVE_TOL ALWAN_LITERAL(0.05)

/* Test CCT estimation using McCamy's approximation */
static int test_cct_mccamy(void) {
    /* Load test cases: x, y, expected_cct */
    static alwan_scalar const test_data[] = {
#include "data/fixtures/cct_test_cases.csv"
    };
    size_t const num_tests = sizeof(test_data) / sizeof(test_data[0]) / 3;

    for (size_t i = 0; i < num_tests; i++) {
        alwan_vec3 xy;
        xy.v[0] = test_data[i * 3 + 0];
        xy.v[1] = test_data[i * 3 + 1];
        alwan_scalar expected_cct = test_data[i * 3 + 2];

        alwan_scalar cct = alwan_cct_mccamy_xy(&xy);
        TEST_ASSERT(cct > 0, "McCamy CCT should be positive");

        alwan_scalar rel_err = ALWAN_FABS(cct - expected_cct) / expected_cct;

        if (rel_err > CCT_RELATIVE_TOL) {
            printf("  Test %zu: xy=[%.5f, %.5f]\n", i, xy.v[0], xy.v[1]);
            printf("    Expected CCT: %.1f K\n", expected_cct);
            printf("    Got CCT:      %.1f K\n", cct);
            printf("    Rel error:    %.2f%%\n", rel_err * 100.0);
            TEST_ASSERT(0, "McCamy CCT error too large");
        }
    }

    printf("  Tested %zu cases\n", num_tests);
    TEST_PASS("McCamy CCT estimation");
}

/* Test CCT estimation using Robertson's method */
static int test_cct_robertson(void) {
    /* Load test cases: x, y, expected_cct */
    static alwan_scalar const test_data[] = {
#include "data/fixtures/cct_test_cases.csv"
    };
    size_t const num_tests = sizeof(test_data) / sizeof(test_data[0]) / 3;

    for (size_t i = 0; i < num_tests; i++) {
        alwan_vec3 xy;
        xy.v[0] = test_data[i * 3 + 0];
        xy.v[1] = test_data[i * 3 + 1];
        alwan_scalar expected_cct = test_data[i * 3 + 2];

        alwan_scalar cct = alwan_cct_robertson_xy(&xy);
        TEST_ASSERT(cct > 0, "Robertson CCT should be positive");

        alwan_scalar rel_err = ALWAN_FABS(cct - expected_cct) / expected_cct;

        if (rel_err > CCT_RELATIVE_TOL) {
            printf("  Test %zu: xy=[%.5f, %.5f]\n", i, xy.v[0], xy.v[1]);
            printf("    Expected CCT: %.1f K\n", expected_cct);
            printf("    Got CCT:      %.1f K\n", cct);
            printf("    Rel error:    %.2f%%\n", rel_err * 100.0);
            TEST_ASSERT(0, "Robertson CCT error too large");
        }
    }

    printf("  Tested %zu cases\n", num_tests);
    TEST_PASS("Robertson CCT estimation");
}

/* Test CRI calculation (placeholder for now) */
static int test_cri(void) {
    /* CRI calculation is complex and requires full spectral data
     * For now, just verify the API doesn't crash */
    alwan_scalar result = alwan_cri_ra(NULL);
    TEST_ASSERT(result < 0, "CRI should return error for NULL input");

    printf("  CRI calculation not yet fully implemented\n");
    TEST_PASS("CRI API");
}

/* Main test runner for M10 */
int test_100_quality_cct_main(void) {
    printf("=== M10: Light Quality & CCT Tests ===\n");

    int failures = 0;

    failures += test_cct_mccamy();
    failures += test_cct_robertson();
    failures += test_cri();

    if (failures == 0) {
        printf("\n=== All M10 tests passed ===\n");
    } else {
        printf("\n=== %d M10 test(s) failed ===\n", failures);
    }

    return failures;
}
