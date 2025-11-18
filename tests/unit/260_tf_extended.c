/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 260: P6 Extended Transfer Functions (Log curves, gamma variants)
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>

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
 * Sony S-Log Family Tests
 * ---------------------------------------------------------------- */

static int test_slog_roundtrip(void) {
    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.001),
        ALWAN_LITERAL(0.01),
        ALWAN_LITERAL(0.1),
        ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(1.0)
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar linear = test_values[i];
        alwan_scalar encoded, roundtrip;

        int status = alwan_oetf_apply(ALWAN_TF_SLOG, &linear, 1, sizeof(alwan_scalar), &encoded, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "S-Log OETF failed");

        status = alwan_eotf_apply(ALWAN_TF_SLOG, &encoded, 1, sizeof(alwan_scalar), &roundtrip, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "S-Log EOTF failed");

        alwan_scalar diff = ALWAN_FABS(roundtrip - linear);
        if (diff >= tolerance) {
            printf("  S-Log round-trip test %zu: linear=%.6f, encoded=%.6f, roundtrip=%.6f, diff=%.2e\n",
                   i, linear, encoded, roundtrip, diff);
        }
        TEST_ASSERT(diff < tolerance, "S-Log round-trip mismatch");
    }

    TEST_PASS("S-Log round-trip");
}

static int test_slog2_roundtrip(void) {
    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.001),
        ALWAN_LITERAL(0.01),
        ALWAN_LITERAL(0.1),
        ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(1.0)
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar linear = test_values[i];
        alwan_scalar encoded, roundtrip;

        int status = alwan_oetf_apply(ALWAN_TF_SLOG2, &linear, 1, sizeof(alwan_scalar), &encoded, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "S-Log2 OETF failed");

        status = alwan_eotf_apply(ALWAN_TF_SLOG2, &encoded, 1, sizeof(alwan_scalar), &roundtrip, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "S-Log2 EOTF failed");

        alwan_scalar diff = ALWAN_FABS(roundtrip - linear);
        TEST_ASSERT(diff < tolerance, "S-Log2 round-trip mismatch");
    }

    TEST_PASS("S-Log2 round-trip");
}

static int test_slog3_roundtrip(void) {
    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.001),
        ALWAN_LITERAL(0.01),
        ALWAN_LITERAL(0.1),
        ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(1.0)
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar linear = test_values[i];
        alwan_scalar encoded, roundtrip;

        int status = alwan_oetf_apply(ALWAN_TF_SLOG3, &linear, 1, sizeof(alwan_scalar), &encoded, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "S-Log3 OETF failed");

        status = alwan_eotf_apply(ALWAN_TF_SLOG3, &encoded, 1, sizeof(alwan_scalar), &roundtrip, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "S-Log3 EOTF failed");

        alwan_scalar diff = ALWAN_FABS(roundtrip - linear);
        TEST_ASSERT(diff < tolerance, "S-Log3 round-trip mismatch");
    }

    TEST_PASS("S-Log3 round-trip");
}

/* ----------------------------------------------------------------
 * Canon C-Log Family Tests
 * ---------------------------------------------------------------- */

static int test_clog_roundtrip(void) {
    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.001),
        ALWAN_LITERAL(0.01),
        ALWAN_LITERAL(0.18),
        ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(1.0)
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar linear = test_values[i];
        alwan_scalar encoded, roundtrip;

        int status = alwan_oetf_apply(ALWAN_TF_CLOG, &linear, 1, sizeof(alwan_scalar), &encoded, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "C-Log OETF failed");

        status = alwan_eotf_apply(ALWAN_TF_CLOG, &encoded, 1, sizeof(alwan_scalar), &roundtrip, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "C-Log EOTF failed");

        alwan_scalar diff = ALWAN_FABS(roundtrip - linear);
        TEST_ASSERT(diff < tolerance, "C-Log round-trip mismatch");
    }

    TEST_PASS("C-Log round-trip");
}

static int test_clog2_roundtrip(void) {
    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.001),
        ALWAN_LITERAL(0.01),
        ALWAN_LITERAL(0.18),
        ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(1.0)
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar linear = test_values[i];
        alwan_scalar encoded, roundtrip;

        int status = alwan_oetf_apply(ALWAN_TF_CLOG2, &linear, 1, sizeof(alwan_scalar), &encoded, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "C-Log2 OETF failed");

        status = alwan_eotf_apply(ALWAN_TF_CLOG2, &encoded, 1, sizeof(alwan_scalar), &roundtrip, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "C-Log2 EOTF failed");

        alwan_scalar diff = ALWAN_FABS(roundtrip - linear);
        TEST_ASSERT(diff < tolerance, "C-Log2 round-trip mismatch");
    }

    TEST_PASS("C-Log2 round-trip");
}

static int test_clog3_roundtrip(void) {
    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.001),
        ALWAN_LITERAL(0.01),
        ALWAN_LITERAL(0.018),
        ALWAN_LITERAL(0.18),
        ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(1.0)
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar linear = test_values[i];
        alwan_scalar encoded, roundtrip;

        int status = alwan_oetf_apply(ALWAN_TF_CLOG3, &linear, 1, sizeof(alwan_scalar), &encoded, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "C-Log3 OETF failed");

        status = alwan_eotf_apply(ALWAN_TF_CLOG3, &encoded, 1, sizeof(alwan_scalar), &roundtrip, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "C-Log3 EOTF failed");

        alwan_scalar diff = ALWAN_FABS(roundtrip - linear);
        TEST_ASSERT(diff < tolerance, "C-Log3 round-trip mismatch");
    }

    TEST_PASS("C-Log3 round-trip");
}

/* ----------------------------------------------------------------
 * Panasonic V-Log Tests
 * ---------------------------------------------------------------- */

static int test_vlog_roundtrip(void) {
    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.001),
        ALWAN_LITERAL(0.01),
        ALWAN_LITERAL(0.1),
        ALWAN_LITERAL(0.18),
        ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(1.0)
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar linear = test_values[i];
        alwan_scalar encoded, roundtrip;

        int status = alwan_oetf_apply(ALWAN_TF_VLOG, &linear, 1, sizeof(alwan_scalar), &encoded, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "V-Log OETF failed");

        status = alwan_eotf_apply(ALWAN_TF_VLOG, &encoded, 1, sizeof(alwan_scalar), &roundtrip, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "V-Log EOTF failed");

        alwan_scalar diff = ALWAN_FABS(roundtrip - linear);
        TEST_ASSERT(diff < tolerance, "V-Log round-trip mismatch");
    }

    TEST_PASS("V-Log round-trip");
}

/* ----------------------------------------------------------------
 * Standard Gamma Variants Tests
 * ---------------------------------------------------------------- */

static int test_gamma22_roundtrip(void) {
    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.1),
        ALWAN_LITERAL(0.18),
        ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(1.0)
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-12);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar linear = test_values[i];
        alwan_scalar encoded, roundtrip;

        int status = alwan_oetf_apply(ALWAN_TF_GAMMA22, &linear, 1, sizeof(alwan_scalar), &encoded, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "Gamma 2.2 OETF failed");

        status = alwan_eotf_apply(ALWAN_TF_GAMMA22, &encoded, 1, sizeof(alwan_scalar), &roundtrip, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "Gamma 2.2 EOTF failed");

        alwan_scalar diff = ALWAN_FABS(roundtrip - linear);
        TEST_ASSERT(diff < tolerance, "Gamma 2.2 round-trip mismatch");
    }

    TEST_PASS("Gamma 2.2 round-trip");
}

static int test_gamma24_roundtrip(void) {
    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.1),
        ALWAN_LITERAL(0.18),
        ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(1.0)
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-12);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar linear = test_values[i];
        alwan_scalar encoded, roundtrip;

        int status = alwan_oetf_apply(ALWAN_TF_GAMMA24, &linear, 1, sizeof(alwan_scalar), &encoded, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "Gamma 2.4 OETF failed");

        status = alwan_eotf_apply(ALWAN_TF_GAMMA24, &encoded, 1, sizeof(alwan_scalar), &roundtrip, sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "Gamma 2.4 EOTF failed");

        alwan_scalar diff = ALWAN_FABS(roundtrip - linear);
        TEST_ASSERT(diff < tolerance, "Gamma 2.4 round-trip mismatch");
    }

    TEST_PASS("Gamma 2.4 round-trip");
}

static int test_gamma_known_values(void) {
    /* Test gamma 2.2: 0.5^(1/2.2) ≈ 0.72974 */
    alwan_scalar linear = ALWAN_LITERAL(0.5);
    alwan_scalar expected_encoded = ALWAN_LITERAL(0.72974);
    alwan_scalar encoded;

    int status = alwan_oetf_apply(ALWAN_TF_GAMMA22, &linear, 1, sizeof(alwan_scalar), &encoded, sizeof(alwan_scalar));
    TEST_ASSERT(status == ALWAN_OK, "Gamma 2.2 OETF failed");

    alwan_scalar diff = ALWAN_FABS(encoded - expected_encoded);
    TEST_ASSERT(diff < ALWAN_LITERAL(0.0001), "Gamma 2.2 known value mismatch");

    TEST_PASS("Gamma known values");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_260_tf_extended_main(void) {
    int failures = 0;

    /* Sony S-Log Family */
    failures += test_slog_roundtrip();
    failures += test_slog2_roundtrip();
    failures += test_slog3_roundtrip();

    /* Canon C-Log Family */
    failures += test_clog_roundtrip();
    failures += test_clog2_roundtrip();
    failures += test_clog3_roundtrip();

    /* Panasonic V-Log */
    failures += test_vlog_roundtrip();

    /* Standard Gamma Variants */
    failures += test_gamma22_roundtrip();
    failures += test_gamma24_roundtrip();
    failures += test_gamma_known_values();

    if (failures == 0) {
        printf("\n=== All P6 extended transfer function tests passed ===\n");
        return 0;
    } else {
        fprintf(stderr, "\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
