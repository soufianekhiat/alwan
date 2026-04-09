/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 06: Color difference (dE) metrics
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_delta_e_76(void) {
    /* Load Lab test pairs and expected dE76 values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const lab1_data[] = {
#include "reference_values/delta_e_lab1.csv"
    };
    static alwan_f64 const lab2_data[] = {
#include "reference_values/delta_e_lab2.csv"
    };
    static alwan_f64 const de76_data[] = {
#include "reference_values/delta_e_76.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(lab1_data) / (3 * sizeof(alwan_f64));
    alwan_f64 const tolerance = ALWAN_TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_lab_f64 lab1 = {lab1_data[i * 3 + 0], lab1_data[i * 3 + 1], lab1_data[i * 3 + 2]};
        alwan_lab_f64 lab2 = {lab2_data[i * 3 + 0], lab2_data[i * 3 + 1], lab2_data[i * 3 + 2]};
        alwan_f64 expected = de76_data[i];

        alwan_f64 result = alwan_delta_e_76_f64(&lab1, &lab2);
        alwan_f64 diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < tolerance, "dE*76 mismatch");
    }

    TEST_PASS("dE*76");
}

static int test_delta_e_94(void) {
    /* Load Lab test pairs and expected dE94 values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const lab1_data[] = {
#include "reference_values/delta_e_lab1.csv"
    };
    static alwan_f64 const lab2_data[] = {
#include "reference_values/delta_e_lab2.csv"
    };
    static alwan_f64 const de94_data[] = {
#include "reference_values/delta_e_94.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(lab1_data) / (3 * sizeof(alwan_f64));
    alwan_f64 const tolerance = ALWAN_TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_lab_f64 lab1 = {lab1_data[i * 3 + 0], lab1_data[i * 3 + 1], lab1_data[i * 3 + 2]};
        alwan_lab_f64 lab2 = {lab2_data[i * 3 + 0], lab2_data[i * 3 + 1], lab2_data[i * 3 + 2]};
        alwan_f64 expected = de94_data[i];

        alwan_f64 result = alwan_delta_e_94_f64(&lab1, &lab2);
        alwan_f64 diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < tolerance, "dE*94 mismatch");
    }

    TEST_PASS("dE*94");
}

static int test_delta_e_cmc(void) {
    /* Load Lab test pairs and expected dE CMC values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const lab1_data[] = {
#include "reference_values/delta_e_lab1.csv"
    };
    static alwan_f64 const lab2_data[] = {
#include "reference_values/delta_e_lab2.csv"
    };
    static alwan_f64 const de_cmc_data[] = {
#include "reference_values/delta_e_cmc.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(lab1_data) / (3 * sizeof(alwan_f64));
    alwan_f64 const tolerance = ALWAN_TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_lab_f64 lab1 = {lab1_data[i * 3 + 0], lab1_data[i * 3 + 1], lab1_data[i * 3 + 2]};
        alwan_lab_f64 lab2 = {lab2_data[i * 3 + 0], lab2_data[i * 3 + 1], lab2_data[i * 3 + 2]};
        alwan_f64 expected = de_cmc_data[i];

        /* Use default l=2, c=1 (acceptability) */
        alwan_delta_e_cmc_params cmc_p; alwan_delta_e_cmc_params_default(&cmc_p);
        alwan_f64 result = alwan_delta_e_cmc_f64(&lab1, &lab2, &cmc_p);
        alwan_f64 diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < tolerance, "dE CMC(2:1) mismatch");
    }

    TEST_PASS("dE CMC(2:1)");
}

static int test_delta_e_2000(void) {
    /* Load Lab test pairs and expected dE2000 values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const lab1_data[] = {
#include "reference_values/delta_e_lab1.csv"
    };
    static alwan_f64 const lab2_data[] = {
#include "reference_values/delta_e_lab2.csv"
    };
    static alwan_f64 const de2000_data[] = {
#include "reference_values/delta_e_2000.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(lab1_data) / (3 * sizeof(alwan_f64));
    alwan_f64 const tolerance = ALWAN_TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_lab_f64 lab1 = {lab1_data[i * 3 + 0], lab1_data[i * 3 + 1], lab1_data[i * 3 + 2]};
        alwan_lab_f64 lab2 = {lab2_data[i * 3 + 0], lab2_data[i * 3 + 1], lab2_data[i * 3 + 2]};
        alwan_f64 expected = de2000_data[i];

        alwan_f64 result = alwan_delta_e_2000_f64(&lab1, &lab2);
        alwan_f64 diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < tolerance, "dE*00 mismatch");
    }

    TEST_PASS("dE*00 (CIEDE2000)");
}

static int test_delta_e_ok(void) {
    /* Test deltaEOK: Euclidean distance in Oklab space */

    /* Identical colors should give 0 */
    {
        alwan_oklab_f64 a = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.1), ALWAN_LITERAL(-0.05)};
        alwan_f64 result = alwan_delta_e_ok_f64(&a, &a);
        TEST_ASSERT_NEAR(result, ALWAN_LITERAL(0.0), ALWAN_TEST_TOLERANCE, "deltaEOK identical");
    }

    /* Known pair: dL=0.1, da=0.2, db=0.3 -> sqrt(0.01+0.04+0.09) = sqrt(0.14) */
    {
        alwan_oklab_f64 a = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
        alwan_oklab_f64 b = {ALWAN_LITERAL(0.6), ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.3)};
        alwan_f64 expected = ALWAN_SQRT(ALWAN_LITERAL(0.14));
        alwan_f64 result = alwan_delta_e_ok_f64(&a, &b);
        TEST_ASSERT_NEAR(result, expected, ALWAN_TEST_TOLERANCE, "deltaEOK known pair");
    }

    /* Symmetry: dE(a,b) == dE(b,a) */
    {
        alwan_oklab_f64 a = {ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.05), ALWAN_LITERAL(-0.1)};
        alwan_oklab_f64 b = {ALWAN_LITERAL(0.3), ALWAN_LITERAL(-0.02), ALWAN_LITERAL(0.15)};
        alwan_f64 d1 = alwan_delta_e_ok_f64(&a, &b);
        alwan_f64 d2 = alwan_delta_e_ok_f64(&b, &a);
        TEST_ASSERT_NEAR(d1, d2, ALWAN_TEST_TOLERANCE, "deltaEOK symmetry");
    }

    /* Non-negativity */
    {
        alwan_oklab_f64 a = {ALWAN_LITERAL(0.1), ALWAN_LITERAL(-0.3), ALWAN_LITERAL(0.2)};
        alwan_oklab_f64 b = {ALWAN_LITERAL(0.9), ALWAN_LITERAL(0.1), ALWAN_LITERAL(-0.1)};
        alwan_f64 result = alwan_delta_e_ok_f64(&a, &b);
        TEST_ASSERT(result >= ALWAN_LITERAL(0.0), "deltaEOK non-negative");
    }

    TEST_PASS("dE OK (Oklab Euclidean)");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_06_delta_e_main(void) {
    int failures = 0;

    failures += test_delta_e_76();
    failures += test_delta_e_94();
    failures += test_delta_e_cmc();
    failures += test_delta_e_2000();
    failures += test_delta_e_ok();

    if (failures == 0) {
        printf("\n=== All dE metric tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
