/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * Test 40: HDR Transfer Functions (PQ, HLG, BT.1886, ACESproxy)
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

static int test_pq_roundtrip(void) {
    /* Test PQ (ST.2084) round-trip: linear → PQ → linear */
    Scalar test_values[] = {
        ALWAN_LITERAL(0.0),       /* Black */
        ALWAN_LITERAL(100.0),     /* SDR white (100 cd/m²) */
        ALWAN_LITERAL(1000.0),    /* HDR mid */
        ALWAN_LITERAL(10000.0)    /* Peak white */
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

#if ALWAN_SCALAR_IS_FLOAT
    Scalar const tolerance = ALWAN_LITERAL(0.1);  /* cd/m² tolerance for float */
#else
    Scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        Scalar linear = test_values[i];
        Scalar encoded, roundtrip;

        /* Forward: linear → PQ */
        int status = alwan_oetf_apply("pq", &linear, 1, 1, &encoded, 1);
        TEST_ASSERT(status == ALWAN_OK, "PQ OETF failed");

        /* Backward: PQ → linear */
        status = alwan_eotf_apply("pq", &encoded, 1, 1, &roundtrip, 1);
        TEST_ASSERT(status == ALWAN_OK, "PQ EOTF failed");

        /* Check round-trip */
        Scalar diff = ALWAN_FABS(roundtrip - linear);
        if (diff >= tolerance) {
            printf("  PQ round-trip test %zu: linear=%.2f, encoded=%.6f, roundtrip=%.2f, diff=%.2e\n",
                   i, linear, encoded, roundtrip, diff);
        }
        TEST_ASSERT(diff < tolerance, "PQ round-trip mismatch");
    }

    TEST_PASS("PQ round-trip");
}

static int test_hlg_roundtrip(void) {
    /* Test HLG round-trip: scene linear → HLG → display linear */
    Scalar test_values[] = {
        ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.01),
        ALWAN_LITERAL(0.05),
        ALWAN_LITERAL(0.1),
        ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(1.0)
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (size_t i = 0; i < num_tests; i++) {
        Scalar scene_linear = test_values[i];
        Scalar encoded;

        /* Forward: scene linear → HLG */
        int status = alwan_oetf_apply("hlg", &scene_linear, 1, 1, &encoded, 1);
        TEST_ASSERT(status == ALWAN_OK, "HLG OETF failed");

        /* Check encoded is in valid range [0,1] */
        TEST_ASSERT(encoded >= ALWAN_LITERAL(0.0) && encoded <= ALWAN_LITERAL(1.0),
                    "HLG encoded value out of range");

        /* Note: HLG EOTF produces display-referred linear, not scene-referred
         * So we don't test exact round-trip, just that it works */
        Scalar display_linear;
        status = alwan_eotf_apply("hlg", &encoded, 1, 1, &display_linear, 1);
        TEST_ASSERT(status == ALWAN_OK, "HLG EOTF failed");

        TEST_ASSERT(display_linear >= ALWAN_LITERAL(0.0), "HLG EOTF produced negative value");
    }

    TEST_PASS("HLG encode/decode");
}

static int test_bt1886_eotf(void) {
    /* Test BT.1886 EOTF (simple gamma 2.4) */
    Scalar test_pairs[][2] = {
        {ALWAN_LITERAL(0.0),  ALWAN_LITERAL(0.0)},
        {ALWAN_LITERAL(0.5),  ALWAN_LITERAL(0.18946)},  /* 0.5^2.4 ≈ 0.18946 */
        {ALWAN_LITERAL(1.0),  ALWAN_LITERAL(1.0)}
    };

    size_t const num_tests = sizeof(test_pairs) / sizeof(test_pairs[0]);

#if ALWAN_SCALAR_IS_FLOAT
    Scalar const tolerance = ALWAN_LITERAL(1e-4);
#else
    Scalar const tolerance = ALWAN_LITERAL(1e-5);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        Scalar encoded = test_pairs[i][0];
        Scalar expected = test_pairs[i][1];
        Scalar result;

        int status = alwan_eotf_apply("bt1886", &encoded, 1, 1, &result, 1);
        TEST_ASSERT(status == ALWAN_OK, "BT.1886 EOTF failed");

        Scalar diff = ALWAN_FABS(result - expected);
        TEST_ASSERT(diff < tolerance, "BT.1886 result mismatch");
    }

    TEST_PASS("BT.1886 EOTF");
}

static int test_acesproxy_roundtrip(void) {
    /* Test ACESproxy round-trip: ACES linear → ACESproxy → ACES linear */
    Scalar test_values[] = {
        ALWAN_LITERAL(0.001),   /* Very dark */
        ALWAN_LITERAL(0.18),    /* Middle gray */
        ALWAN_LITERAL(1.0),     /* Bright */
        ALWAN_LITERAL(4.0)      /* Very bright */
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

#if ALWAN_SCALAR_IS_FLOAT
    Scalar const tolerance = ALWAN_LITERAL(1e-4);  /* Relative tolerance for log encoding */
#else
    Scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        Scalar linear = test_values[i];
        Scalar encoded, roundtrip;

        /* Forward: ACES linear → ACESproxy */
        int status = alwan_oetf_apply("acesproxy", &linear, 1, 1, &encoded, 1);
        TEST_ASSERT(status == ALWAN_OK, "ACESproxy OETF failed");

        /* Backward: ACESproxy → ACES linear */
        status = alwan_eotf_apply("acesproxy", &encoded, 1, 1, &roundtrip, 1);
        TEST_ASSERT(status == ALWAN_OK, "ACESproxy EOTF failed");

        /* Check round-trip (relative error for log encoding) */
        Scalar rel_diff = ALWAN_FABS((roundtrip - linear) / linear);
        if (rel_diff >= tolerance) {
            printf("  ACESproxy round-trip test %zu: linear=%.4f, roundtrip=%.4f, rel_diff=%.2e\n",
                   i, linear, roundtrip, rel_diff);
        }
        TEST_ASSERT(rel_diff < tolerance, "ACESproxy round-trip mismatch");
    }

    TEST_PASS("ACESproxy round-trip");
}

static int test_pq_st2084_alias(void) {
    /* Test that "pq" and "st2084" are aliases */
    Scalar linear = ALWAN_LITERAL(1000.0);  /* 1000 cd/m² */
    Scalar encoded_pq, encoded_st2084;

    int status = alwan_oetf_apply("pq", &linear, 1, 1, &encoded_pq, 1);
    TEST_ASSERT(status == ALWAN_OK, "PQ OETF failed");

    status = alwan_oetf_apply("st2084", &linear, 1, 1, &encoded_st2084, 1);
    TEST_ASSERT(status == ALWAN_OK, "ST.2084 OETF failed");

    /* Should produce identical results */
    Scalar diff = ALWAN_FABS(encoded_pq - encoded_st2084);
    TEST_ASSERT(diff < ALWAN_EPSILON, "PQ and ST.2084 should be aliases");

    TEST_PASS("PQ/ST.2084 alias");
}

static int test_invalid_tf_name(void) {
    /* Test that invalid transfer function names return error */
    Scalar dummy_in = ALWAN_LITERAL(0.5);
    Scalar dummy_out;

    int status = alwan_oetf_apply("invalid_tf", &dummy_in, 1, 1, &dummy_out, 1);
    TEST_ASSERT(status == ALWAN_E_INVALID, "Should reject invalid OETF name");

    status = alwan_eotf_apply("invalid_tf", &dummy_in, 1, 1, &dummy_out, 1);
    TEST_ASSERT(status == ALWAN_E_INVALID, "Should reject invalid EOTF name");

    TEST_PASS("Invalid TF name rejection");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_40_tf_hdr_main(void) {
    int failures = 0;

    failures += test_pq_roundtrip();
    failures += test_hlg_roundtrip();
    failures += test_bt1886_eotf();
    failures += test_acesproxy_roundtrip();
    failures += test_pq_st2084_alias();
    failures += test_invalid_tf_name();

    if (failures == 0) {
        printf("\n=== All HDR transfer function tests passed ===\n");
        return 0;
    } else {
        fprintf(stderr, "\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
