/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 09: HDR Transfer Functions (PQ, HLG, BT.1886, ACESproxy)
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>


/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_pq_roundtrip(void) {
    /* Test PQ (ST.2084) round-trip: linear -> PQ -> linear */
    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.0),       /* Black */
        ALWAN_LITERAL(100.0),     /* SDR white (100 cd/m²) */
        ALWAN_LITERAL(1000.0),    /* HDR mid */
        ALWAN_LITERAL(10000.0)    /* Peak white */
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(0.1);  /* cd/m² tolerance for float */
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar linear = test_values[i];
        alwan_scalar encoded, roundtrip;

        /* Forward: linear -> PQ */
        int status = alwan_oetf_apply(&encoded, ALWAN_TF_PQ, &linear, 1, 1, 1);
        TEST_ASSERT(status == ALWAN_OK, "PQ OETF failed");

        /* Backward: PQ -> linear */
        status = alwan_eotf_apply(&roundtrip, ALWAN_TF_PQ, &encoded, 1, 1, 1);
        TEST_ASSERT(status == ALWAN_OK, "PQ EOTF failed");

        /* Check round-trip */
        alwan_scalar diff = ALWAN_ABS(roundtrip - linear);
        if (diff >= tolerance) {
            printf("  PQ round-trip test %zu: linear=%.2f, encoded=%.6f, roundtrip=%.2f, diff=%.2e\n",
                   i, linear, encoded, roundtrip, diff);
        }
        TEST_ASSERT(diff < tolerance, "PQ round-trip mismatch");
    }

    TEST_PASS("PQ round-trip");
}

static int test_hlg_roundtrip(void) {
    /* Test HLG round-trip: scene linear -> HLG -> display linear */
    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.01),
        ALWAN_LITERAL(0.05),
        ALWAN_LITERAL(0.1),
        ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(1.0)
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar scene_linear = test_values[i];
        alwan_scalar encoded;

        /* Forward: scene linear -> HLG */
        int status = alwan_oetf_apply(&encoded, ALWAN_TF_HLG, &scene_linear, 1, 1, 1);
        TEST_ASSERT(status == ALWAN_OK, "HLG OETF failed");

        /* Check encoded is in valid range [0,1] */
        TEST_ASSERT(encoded >= ALWAN_LITERAL(0.0) && encoded <= ALWAN_LITERAL(1.0),
                    "HLG encoded value out of range");

        /* Note: HLG EOTF produces display-referred linear, not scene-referred
         * So we don't test exact round-trip, just that it works */
        alwan_scalar display_linear;
        status = alwan_eotf_apply(&display_linear, ALWAN_TF_HLG, &encoded, 1, 1, 1);
        TEST_ASSERT(status == ALWAN_OK, "HLG EOTF failed");

        TEST_ASSERT(display_linear >= ALWAN_LITERAL(0.0), "HLG EOTF produced negative value");
    }

    TEST_PASS("HLG encode/decode");
}

static int test_bt1886_eotf(void) {
    /* Test BT.1886 EOTF (simple gamma 2.4) */
    alwan_scalar test_pairs[][2] = {
        {ALWAN_LITERAL(0.0),  ALWAN_LITERAL(0.0)},
        {ALWAN_LITERAL(0.5),  ALWAN_LITERAL(0.18946)},  /* 0.5^2.4 ≈ 0.18946 */
        {ALWAN_LITERAL(1.0),  ALWAN_LITERAL(1.0)}
    };

    size_t const num_tests = sizeof(test_pairs) / sizeof(test_pairs[0]);

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-4);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar encoded = test_pairs[i][0];
        alwan_scalar expected = test_pairs[i][1];
        alwan_scalar result;

        int status = alwan_eotf_apply(&result, ALWAN_TF_BT1886, &encoded, 1, 1, 1);
        TEST_ASSERT(status == ALWAN_OK, "BT.1886 EOTF failed");

        alwan_scalar diff = ALWAN_ABS(result - expected);
        TEST_ASSERT(diff < tolerance, "BT.1886 result mismatch");
    }

    TEST_PASS("BT.1886 EOTF");
}

static int test_acesproxy_roundtrip(void) {
    /* Test ACESproxy round-trip: ACES linear -> ACESproxy -> ACES linear */
    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.001),   /* Very dark */
        ALWAN_LITERAL(0.18),    /* Middle gray */
        ALWAN_LITERAL(1.0),     /* Bright */
        ALWAN_LITERAL(4.0)      /* Very bright */
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-4);  /* Relative tolerance for log encoding */
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar linear = test_values[i];
        alwan_scalar encoded, roundtrip;

        /* Forward: ACES linear -> ACESproxy */
        int status = alwan_oetf_apply(&encoded, ALWAN_TF_ACESPROXY, &linear, 1, 1, 1);
        TEST_ASSERT(status == ALWAN_OK, "ACESproxy OETF failed");

        /* Backward: ACESproxy -> ACES linear */
        status = alwan_eotf_apply(&roundtrip, ALWAN_TF_ACESPROXY, &encoded, 1, 1, 1);
        TEST_ASSERT(status == ALWAN_OK, "ACESproxy EOTF failed");

        /* Check round-trip (relative error for log encoding) */
        alwan_scalar rel_diff = ALWAN_ABS((roundtrip - linear) / linear);
        if (rel_diff >= tolerance) {
            printf("  ACESproxy round-trip test %zu: linear=%.4f, roundtrip=%.4f, rel_diff=%.2e\n",
                   i, linear, roundtrip, rel_diff);
        }
        TEST_ASSERT(rel_diff < tolerance, "ACESproxy round-trip mismatch");
    }

    TEST_PASS("ACESproxy round-trip");
}

static int test_pq_st2084_alias(void) {
    /* Test that ALWAN_TF_PQ and ALWAN_TF_ST2084 are aliases */
    alwan_scalar linear = ALWAN_LITERAL(1000.0);  /* 1000 cd/m² */
    alwan_scalar encoded_pq, encoded_st2084;

    int status = alwan_oetf_apply(&encoded_pq, ALWAN_TF_PQ, &linear, 1, 1, 1);
    TEST_ASSERT(status == ALWAN_OK, "PQ OETF failed");

    status = alwan_oetf_apply(&encoded_st2084, ALWAN_TF_ST2084, &linear, 1, 1, 1);
    TEST_ASSERT(status == ALWAN_OK, "ST.2084 OETF failed");

    /* Should produce identical results */
    alwan_scalar diff = ALWAN_ABS(encoded_pq - encoded_st2084);
    TEST_ASSERT(diff < TEST_TOLERANCE, "PQ and ST.2084 should be aliases");

    TEST_PASS("PQ/ST.2084 alias");
}

static int test_invalid_tf_name(void) {
    /* Test that invalid transfer function enum values return error */
    alwan_scalar dummy_in = ALWAN_LITERAL(0.5);
    alwan_scalar dummy_out;

    int status = alwan_oetf_apply(&dummy_out, (alwan_transfer_function)999, &dummy_in, 1, 1, 1);
    TEST_ASSERT(status == ALWAN_E_INVALID, "Should reject invalid OETF enum");

    status = alwan_eotf_apply(&dummy_out, (alwan_transfer_function)999, &dummy_in, 1, 1, 1);
    TEST_ASSERT(status == ALWAN_E_INVALID, "Should reject invalid EOTF enum");

    TEST_PASS("Invalid TF enum rejection");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_09_tf_hdr_main(void) {
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
