/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * Test 21: Color difference (ΔE) metrics
 */

#include "../../src/alwan/alwan.h"
#include "../../src/alwan/alwan_internal.h"
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

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_delta_e_76(void) {
    /* Load Lab test pairs and expected ΔE76 values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static Scalar const lab1_data[] = {
#include "../../data/fixtures/m2_delta_e_lab1.csv"
    };
    static Scalar const lab2_data[] = {
#include "../../data/fixtures/m2_delta_e_lab2.csv"
    };
    static Scalar const de76_data[] = {
#include "../../data/fixtures/m2_delta_e_76.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(lab1_data) / (3 * sizeof(Scalar));
#if ALWAN_SCALAR_IS_FLOAT
    Scalar const tolerance = ALWAN_LITERAL(1e-6);
#else
    Scalar const tolerance = ALWAN_LITERAL(1e-12);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 lab1 = {{lab1_data[i * 3 + 0], lab1_data[i * 3 + 1], lab1_data[i * 3 + 2]}};
        alwan_vec3 lab2 = {{lab2_data[i * 3 + 0], lab2_data[i * 3 + 1], lab2_data[i * 3 + 2]}};
        Scalar expected = de76_data[i];

        Scalar result = alwan_delta_e_76(&lab1, &lab2);
        Scalar diff = ALWAN_FABS(result - expected);

        TEST_ASSERT(diff < tolerance, "ΔE*76 mismatch");
    }

    TEST_PASS("ΔE*76");
}

static int test_delta_e_94(void) {
    /* Load Lab test pairs and expected ΔE94 values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static Scalar const lab1_data[] = {
#include "../../data/fixtures/m2_delta_e_lab1.csv"
    };
    static Scalar const lab2_data[] = {
#include "../../data/fixtures/m2_delta_e_lab2.csv"
    };
    static Scalar const de94_data[] = {
#include "../../data/fixtures/m2_delta_e_94.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(lab1_data) / (3 * sizeof(Scalar));
#if ALWAN_SCALAR_IS_FLOAT
    Scalar const tolerance = ALWAN_LITERAL(1e-6);
#else
    Scalar const tolerance = ALWAN_LITERAL(1e-12);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 lab1 = {{lab1_data[i * 3 + 0], lab1_data[i * 3 + 1], lab1_data[i * 3 + 2]}};
        alwan_vec3 lab2 = {{lab2_data[i * 3 + 0], lab2_data[i * 3 + 1], lab2_data[i * 3 + 2]}};
        Scalar expected = de94_data[i];

        Scalar result = alwan_delta_e_94(&lab1, &lab2);
        Scalar diff = ALWAN_FABS(result - expected);

        TEST_ASSERT(diff < tolerance, "ΔE*94 mismatch");
    }

    TEST_PASS("ΔE*94");
}

static int test_delta_e_cmc(void) {
    /* Load Lab test pairs and expected ΔE CMC values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static Scalar const lab1_data[] = {
#include "../../data/fixtures/m2_delta_e_lab1.csv"
    };
    static Scalar const lab2_data[] = {
#include "../../data/fixtures/m2_delta_e_lab2.csv"
    };
    static Scalar const de_cmc_data[] = {
#include "../../data/fixtures/m2_delta_e_cmc.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(lab1_data) / (3 * sizeof(Scalar));
#if ALWAN_SCALAR_IS_FLOAT
    Scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    Scalar const tolerance = ALWAN_LITERAL(1e-12);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 lab1 = {{lab1_data[i * 3 + 0], lab1_data[i * 3 + 1], lab1_data[i * 3 + 2]}};
        alwan_vec3 lab2 = {{lab2_data[i * 3 + 0], lab2_data[i * 3 + 1], lab2_data[i * 3 + 2]}};
        Scalar expected = de_cmc_data[i];

        /* Use default l=2, c=1 (acceptability) */
        Scalar result = alwan_delta_e_cmc(&lab1, &lab2, ALWAN_LITERAL(2.0), ALWAN_LITERAL(1.0));
        Scalar diff = ALWAN_FABS(result - expected);

        TEST_ASSERT(diff < tolerance, "ΔE CMC(2:1) mismatch");
    }

    TEST_PASS("ΔE CMC(2:1)");
}

static int test_delta_e_2000(void) {
    /* Load Lab test pairs and expected ΔE2000 values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static Scalar const lab1_data[] = {
#include "../../data/fixtures/m2_delta_e_lab1.csv"
    };
    static Scalar const lab2_data[] = {
#include "../../data/fixtures/m2_delta_e_lab2.csv"
    };
    static Scalar const de2000_data[] = {
#include "../../data/fixtures/m2_delta_e_2000.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(lab1_data) / (3 * sizeof(Scalar));
#if ALWAN_SCALAR_IS_FLOAT
    Scalar const tolerance = ALWAN_LITERAL(1e-6);
#else
    Scalar const tolerance = ALWAN_LITERAL(1e-12);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 lab1 = {{lab1_data[i * 3 + 0], lab1_data[i * 3 + 1], lab1_data[i * 3 + 2]}};
        alwan_vec3 lab2 = {{lab2_data[i * 3 + 0], lab2_data[i * 3 + 1], lab2_data[i * 3 + 2]}};
        Scalar expected = de2000_data[i];

        Scalar result = alwan_delta_e_2000(&lab1, &lab2);
        Scalar diff = ALWAN_FABS(result - expected);

        TEST_ASSERT(diff < tolerance, "ΔE*00 mismatch");
    }

    TEST_PASS("ΔE*00 (CIEDE2000)");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_21_delta_e_main(void) {
    int failures = 0;

    failures += test_delta_e_76();
    failures += test_delta_e_94();
    failures += test_delta_e_cmc();
    failures += test_delta_e_2000();

    if (failures == 0) {
        printf("\n=== All ΔE metric tests passed ===\n");
        return 0;
    } else {
        fprintf(stderr, "\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
