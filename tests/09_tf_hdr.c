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

static int test_pq_forward_inverse(void) {
    /* Test PQ (ST.2084) OETF and EOTF against reference values.
     * Reference: SMPTE ST 2084, computed with double precision Python. */
    alwan_f64 const tolerance = ALWAN_TEST_TOLERANCE;

    /* Test OETF: linear cd/m^2 -> PQ code value */
    {
        static alwan_f64 const oetf_input[] = {
            ALWAN_LITERAL(0.0),
            ALWAN_LITERAL(100.0),
            ALWAN_LITERAL(1000.0),
            ALWAN_LITERAL(4000.0),
            ALWAN_LITERAL(10000.0)
        };
        static alwan_f64 const oetf_expected[] = {
            ALWAN_LITERAL(7.30955902578396645900e-07),
            ALWAN_LITERAL(5.08078421517399014817e-01),
            ALWAN_LITERAL(7.51827096247041026800e-01),
            ALWAN_LITERAL(9.02572393310937304278e-01),
            ALWAN_LITERAL(1.0)
        };
        size_t const n = sizeof(oetf_input) / sizeof(oetf_input[0]);

        for (size_t i = 0; i < n; i++) {
            alwan_f64 encoded;
            int status = alwan_oetf_apply(&encoded, ALWAN_TF_PQ, &oetf_input[i], 1, 1, 1);
            TEST_ASSERT(status == ALWAN_OK, "PQ OETF failed");
            alwan_f64 diff = ALWAN_ABS(encoded - oetf_expected[i]);
            TEST_ASSERT(diff < tolerance, "PQ OETF value mismatch");
        }
    }

    /* Test EOTF: PQ code value -> linear cd/m^2 */
    {
        static alwan_f64 const eotf_input[] = {
            ALWAN_LITERAL(0.0),
            ALWAN_LITERAL(0.25),
            ALWAN_LITERAL(0.5),
            ALWAN_LITERAL(0.75),
            ALWAN_LITERAL(1.0)
        };
        static alwan_f64 const eotf_expected[] = {
            ALWAN_LITERAL(0.0),
            ALWAN_LITERAL(5.15417600983300694395e+00),
            ALWAN_LITERAL(9.22457089940652679161e+01),
            ALWAN_LITERAL(9.83377855587027511319e+02),
            ALWAN_LITERAL(1.0e+04)
        };
        size_t const n = sizeof(eotf_input) / sizeof(eotf_input[0]);

        for (size_t i = 0; i < n; i++) {
            alwan_f64 decoded;
            int status = alwan_eotf_apply(&decoded, ALWAN_TF_PQ, &eotf_input[i], 1, 1, 1);
            TEST_ASSERT(status == ALWAN_OK, "PQ EOTF failed");
            alwan_f64 diff = ALWAN_ABS(decoded - eotf_expected[i]);
            /* EOTF output ranges 0..10000 cd/m^2; use relative tolerance for large values */
            alwan_f64 eotf_tol = eotf_expected[i] > ALWAN_LITERAL(1.0)
                ? ALWAN_ABS(eotf_expected[i]) * ALWAN_LITERAL(1e-12)
                : tolerance;
            TEST_ASSERT(diff < eotf_tol, "PQ EOTF value mismatch");
        }
    }

    TEST_PASS("PQ forward/inverse");
}

static int test_hlg_roundtrip(void) {
    /* Test HLG round-trip: scene linear -> HLG -> display linear */
    alwan_f64 test_values[] = {
        ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.01),
        ALWAN_LITERAL(0.05),
        ALWAN_LITERAL(0.1),
        ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(1.0)
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

    for (size_t i = 0; i < num_tests; i++) {
        alwan_f64 scene_linear = test_values[i];
        alwan_f64 encoded;

        /* Forward: scene linear -> HLG */
        int status = alwan_oetf_apply(&encoded, ALWAN_TF_HLG, &scene_linear, 1, 1, 1);
        TEST_ASSERT(status == ALWAN_OK, "HLG OETF failed");

        /* Check encoded is in valid range [0,1] */
        TEST_ASSERT(encoded >= ALWAN_LITERAL(0.0) && encoded <= ALWAN_LITERAL(1.0),
                    "HLG encoded value out of range");

        /* Note: HLG EOTF produces display-referred linear, not scene-referred
         * So we don't test exact round-trip, just that it works */
        alwan_f64 display_linear;
        status = alwan_eotf_apply(&display_linear, ALWAN_TF_HLG, &encoded, 1, 1, 1);
        TEST_ASSERT(status == ALWAN_OK, "HLG EOTF failed");

        TEST_ASSERT(display_linear >= ALWAN_LITERAL(0.0), "HLG EOTF produced negative value");
    }

    TEST_PASS("HLG encode/decode");
}

static int test_bt1886_eotf(void) {
    /* Test BT.1886 EOTF (simple gamma 2.4) */
    alwan_f64 test_pairs[][2] = {
        {ALWAN_LITERAL(0.0),  ALWAN_LITERAL(0.0)},
        {ALWAN_LITERAL(0.5),  ALWAN_LITERAL(1.89464570813799776383e-01)},  /* 0.5^2.4 */
        {ALWAN_LITERAL(1.0),  ALWAN_LITERAL(1.0)}
    };

    size_t const num_tests = sizeof(test_pairs) / sizeof(test_pairs[0]);

    alwan_f64 const tolerance = ALWAN_TEST_TOLERANCE;

    for (size_t i = 0; i < num_tests; i++) {
        alwan_f64 encoded = test_pairs[i][0];
        alwan_f64 expected = test_pairs[i][1];
        alwan_f64 result;

        int status = alwan_eotf_apply(&result, ALWAN_TF_BT1886, &encoded, 1, 1, 1);
        TEST_ASSERT(status == ALWAN_OK, "BT.1886 EOTF failed");

        alwan_f64 diff = ALWAN_ABS(result - expected);
        TEST_ASSERT(diff < tolerance, "BT.1886 result mismatch");
    }

    TEST_PASS("BT.1886 EOTF");
}

static int test_acesproxy_roundtrip(void) {
    /* Test ACESproxy round-trip: ACES linear -> ACESproxy -> ACES linear */
    alwan_f64 test_values[] = {
        ALWAN_LITERAL(0.001),   /* Very dark */
        ALWAN_LITERAL(0.18),    /* Middle gray */
        ALWAN_LITERAL(1.0),     /* Bright */
        ALWAN_LITERAL(4.0)      /* Very bright */
    };

    size_t const num_tests = sizeof(test_values) / sizeof(test_values[0]);

    alwan_f64 const tolerance = ALWAN_TEST_TOLERANCE;

    for (size_t i = 0; i < num_tests; i++) {
        alwan_f64 linear = test_values[i];
        alwan_f64 encoded, roundtrip;

        /* Forward: ACES linear -> ACESproxy */
        int status = alwan_oetf_apply(&encoded, ALWAN_TF_ACESPROXY, &linear, 1, 1, 1);
        TEST_ASSERT(status == ALWAN_OK, "ACESproxy OETF failed");

        /* Backward: ACESproxy -> ACES linear */
        status = alwan_eotf_apply(&roundtrip, ALWAN_TF_ACESPROXY, &encoded, 1, 1, 1);
        TEST_ASSERT(status == ALWAN_OK, "ACESproxy EOTF failed");

        /* Check round-trip (relative error for log encoding) */
        alwan_f64 rel_diff = ALWAN_ABS((roundtrip - linear) / linear);
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
    alwan_f64 linear = ALWAN_LITERAL(1000.0);  /* 1000 cd/m^2 */
    alwan_f64 encoded_pq, encoded_st2084;

    int status = alwan_oetf_apply(&encoded_pq, ALWAN_TF_PQ, &linear, 1, 1, 1);
    TEST_ASSERT(status == ALWAN_OK, "PQ OETF failed");

    status = alwan_oetf_apply(&encoded_st2084, ALWAN_TF_ST2084, &linear, 1, 1, 1);
    TEST_ASSERT(status == ALWAN_OK, "ST.2084 OETF failed");

    /* Should produce identical results */
    alwan_f64 diff = ALWAN_ABS(encoded_pq - encoded_st2084);
    TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "PQ and ST.2084 should be aliases");

    TEST_PASS("PQ/ST.2084 alias");
}

static int test_invalid_tf_name(void) {
    /* Test that invalid transfer function enum values return error */
    alwan_f64 dummy_in = ALWAN_LITERAL(0.5);
    alwan_f64 dummy_out;

    int status = alwan_oetf_apply(&dummy_out, (alwan_transfer_function)999, &dummy_in, 1, 1, 1);
    TEST_ASSERT(status == ALWAN_E_INVALID, "Should reject invalid OETF enum");

    status = alwan_eotf_apply(&dummy_out, (alwan_transfer_function)999, &dummy_in, 1, 1, 1);
    TEST_ASSERT(status == ALWAN_E_INVALID, "Should reject invalid EOTF enum");

    TEST_PASS("Invalid TF enum rejection");
}

/* ----------------------------------------------------------------
 * SIMD vs scalar parity tests
 *
 * These run each TF in two ways:
 *  - bulk (count = MIN_SIMD_PIXELS, contiguous strides) -> exercises SIMD loop
 *  - scalar (count = 1 per element)                     -> exercises scalar tail
 * Then compare element-by-element to catch any vectorization divergence.
 * ---------------------------------------------------------------- */

static int test_pq_simd_vs_scalar(void) {
    TEST_START("PQ OETF/EOTF SIMD vs scalar parity");

    alwan_f64 oetf_in[MIN_SIMD_PIXELS];
    alwan_f64 simd_out[MIN_SIMD_PIXELS];
    alwan_f64 ref_out[MIN_SIMD_PIXELS];

    /* Ramp from 0 to 10 000 cd/m² (full PQ range) */
    for (int i = 0; i < MIN_SIMD_PIXELS; i++)
        oetf_in[i] = (alwan_f64)i / (alwan_f64)(MIN_SIMD_PIXELS - 1) * ALWAN_LITERAL(10000.0);

    /* Bulk SIMD path */
    int status = alwan_oetf_apply(simd_out, ALWAN_TF_PQ,
                                  oetf_in, MIN_SIMD_PIXELS,
                                  sizeof(alwan_f64), sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "PQ OETF bulk failed");

    /* Per-element scalar reference (count=1 stays below any SIMD width) */
    for (int i = 0; i < MIN_SIMD_PIXELS; i++) {
        status = alwan_oetf_apply(&ref_out[i], ALWAN_TF_PQ,
                                  &oetf_in[i], 1,
                                  sizeof(alwan_f64), sizeof(alwan_f64));
        TEST_ASSERT(status == ALWAN_OK, "PQ OETF scalar ref failed");
    }

    for (int i = 0; i < MIN_SIMD_PIXELS; i++) {
        alwan_f64 diff = ALWAN_ABS(simd_out[i] - ref_out[i]);
        if (diff > ALWAN_SIMD_PQ_TOLERANCE) {
            printf("  PQ OETF SIMD/scalar mismatch at [%d]: simd=%.15g ref=%.15g diff=%e\n",
                   i, simd_out[i], ref_out[i], diff);
            TEST_ASSERT(0, "PQ OETF SIMD/scalar mismatch");
        }
    }

    /* EOTF: ramp 0..1 encoded -> linear cd/m² */
    alwan_f64 eotf_in[MIN_SIMD_PIXELS];
    alwan_f64 eotf_simd[MIN_SIMD_PIXELS];
    alwan_f64 eotf_ref[MIN_SIMD_PIXELS];

    for (int i = 0; i < MIN_SIMD_PIXELS; i++)
        eotf_in[i] = (alwan_f64)i / (alwan_f64)(MIN_SIMD_PIXELS - 1);

    status = alwan_eotf_apply(eotf_simd, ALWAN_TF_PQ,
                              eotf_in, MIN_SIMD_PIXELS,
                              sizeof(alwan_f64), sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "PQ EOTF bulk failed");

    for (int i = 0; i < MIN_SIMD_PIXELS; i++) {
        status = alwan_eotf_apply(&eotf_ref[i], ALWAN_TF_PQ,
                                  &eotf_in[i], 1,
                                  sizeof(alwan_f64), sizeof(alwan_f64));
        TEST_ASSERT(status == ALWAN_OK, "PQ EOTF scalar ref failed");
    }

    for (int i = 0; i < MIN_SIMD_PIXELS; i++) {
        /* Use relative tolerance for large cd/m² values */
        alwan_f64 tol = eotf_ref[i] > ALWAN_LITERAL(1.0)
            ? ALWAN_ABS(eotf_ref[i]) * ALWAN_SIMD_PQ_TOLERANCE
            : ALWAN_SIMD_PQ_TOLERANCE;
        alwan_f64 diff = ALWAN_ABS(eotf_simd[i] - eotf_ref[i]);
        if (diff > tol) {
            printf("  PQ EOTF SIMD/scalar mismatch at [%d]: simd=%.15g ref=%.15g diff=%e\n",
                   i, eotf_simd[i], eotf_ref[i], diff);
            TEST_ASSERT(0, "PQ EOTF SIMD/scalar mismatch");
        }
    }

    TEST_PASS_MSG();
    return 0;
}

static int test_hlg_simd_vs_scalar(void) {
    TEST_START("HLG OETF/EOTF SIMD vs scalar parity");

    alwan_f64 oetf_in[MIN_SIMD_PIXELS];
    alwan_f64 simd_out[MIN_SIMD_PIXELS];
    alwan_f64 ref_out[MIN_SIMD_PIXELS];

    /* Ramp over HLG scene-linear range [0, 1] */
    for (int i = 0; i < MIN_SIMD_PIXELS; i++)
        oetf_in[i] = (alwan_f64)i / (alwan_f64)(MIN_SIMD_PIXELS - 1);

    int status = alwan_oetf_apply(simd_out, ALWAN_TF_HLG,
                                  oetf_in, MIN_SIMD_PIXELS,
                                  sizeof(alwan_f64), sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "HLG OETF bulk failed");

    for (int i = 0; i < MIN_SIMD_PIXELS; i++) {
        status = alwan_oetf_apply(&ref_out[i], ALWAN_TF_HLG,
                                  &oetf_in[i], 1,
                                  sizeof(alwan_f64), sizeof(alwan_f64));
        TEST_ASSERT(status == ALWAN_OK, "HLG OETF scalar ref failed");
    }

    for (int i = 0; i < MIN_SIMD_PIXELS; i++) {
        alwan_f64 diff = ALWAN_ABS(simd_out[i] - ref_out[i]);
        if (diff > ALWAN_SIMD_TOLERANCE) {
            printf("  HLG OETF SIMD/scalar mismatch at [%d]: simd=%.15g ref=%.15g diff=%e\n",
                   i, simd_out[i], ref_out[i], diff);
            TEST_ASSERT(0, "HLG OETF SIMD/scalar mismatch");
        }
    }

    /* EOTF: ramp 0..1 */
    alwan_f64 eotf_simd[MIN_SIMD_PIXELS];
    alwan_f64 eotf_ref[MIN_SIMD_PIXELS];

    status = alwan_eotf_apply(eotf_simd, ALWAN_TF_HLG,
                              oetf_in, MIN_SIMD_PIXELS,
                              sizeof(alwan_f64), sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "HLG EOTF bulk failed");

    for (int i = 0; i < MIN_SIMD_PIXELS; i++) {
        status = alwan_eotf_apply(&eotf_ref[i], ALWAN_TF_HLG,
                                  &oetf_in[i], 1,
                                  sizeof(alwan_f64), sizeof(alwan_f64));
        TEST_ASSERT(status == ALWAN_OK, "HLG EOTF scalar ref failed");
    }

    for (int i = 0; i < MIN_SIMD_PIXELS; i++) {
        alwan_f64 diff = ALWAN_ABS(eotf_simd[i] - eotf_ref[i]);
        if (diff > ALWAN_SIMD_TOLERANCE) {
            printf("  HLG EOTF SIMD/scalar mismatch at [%d]: simd=%.15g ref=%.15g diff=%e\n",
                   i, eotf_simd[i], eotf_ref[i], diff);
            TEST_ASSERT(0, "HLG EOTF SIMD/scalar mismatch");
        }
    }

    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_09_tf_hdr_main(void) {
    int failures = 0;

    failures += test_pq_forward_inverse();
    failures += test_hlg_roundtrip();
    failures += test_bt1886_eotf();
    failures += test_acesproxy_roundtrip();
    failures += test_pq_st2084_alias();
    failures += test_invalid_tf_name();
    failures += test_pq_simd_vs_scalar();
    failures += test_hlg_simd_vs_scalar();

    if (failures == 0) {
        printf("\n=== All HDR transfer function tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
