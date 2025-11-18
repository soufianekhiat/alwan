/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 260: Extended Transfer Functions (Log curves, gamma variants)
 *
 * Reference values generated from colour-science Python library
 * via generate_data_tests.ps1
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
 * Reference Value Loading
 * ---------------------------------------------------------------- */

/* Reference data format: triplets of (linear, encoded, decoded) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

static alwan_scalar const g_tf_slog[] = {
#include "reference_values/tf_slog.csv"
};

static alwan_scalar const g_tf_slog2[] = {
#include "reference_values/tf_slog2.csv"
};

static alwan_scalar const g_tf_slog3[] = {
#include "reference_values/tf_slog3.csv"
};

static alwan_scalar const g_tf_clog[] = {
#include "reference_values/tf_clog.csv"
};

static alwan_scalar const g_tf_clog2[] = {
#include "reference_values/tf_clog2.csv"
};

static alwan_scalar const g_tf_clog3[] = {
#include "reference_values/tf_clog3.csv"
};

static alwan_scalar const g_tf_vlog[] = {
#include "reference_values/tf_vlog.csv"
};

static alwan_scalar const g_tf_gamma22[] = {
#include "reference_values/tf_gamma22.csv"
};

static alwan_scalar const g_tf_gamma24[] = {
#include "reference_values/tf_gamma24.csv"
};

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Generic Transfer Function Test Helper
 * ---------------------------------------------------------------- */

static int test_transfer_function(
    char const *name,
    alwan_transfer_function tf,
    alwan_scalar const *ref_data,
    size_t num_triplets)
{
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-6);
#endif

    int failures = 0;

    for (size_t i = 0; i < num_triplets; i++) {
        size_t idx = i * 3;
        alwan_scalar linear_ref = ref_data[idx + 0];
        alwan_scalar encoded_ref = ref_data[idx + 1];
        alwan_scalar decoded_ref = ref_data[idx + 2];

        /* Test OETF: linear -> encoded */
        alwan_scalar encoded_actual;
        int status = alwan_oetf_apply(tf, &linear_ref, 1, sizeof(alwan_scalar),
                                      &encoded_actual, sizeof(alwan_scalar));

        if (status != ALWAN_OK) {
            fprintf(stderr, "  [%s] OETF test %zu: alwan_oetf_apply failed with status %d\n",
                   name, i, status);
            failures++;
            continue;
        }

        alwan_scalar encode_diff = ALWAN_FABS(encoded_actual - encoded_ref);
        if (encode_diff >= tolerance) {
            fprintf(stderr, "  [%s] OETF test %zu: linear=%.6f, expected=%.6f, got=%.6f, diff=%.2e\n",
                   name, i, linear_ref, encoded_ref, encoded_actual, encode_diff);
            failures++;
        }

        /* Test EOTF: encoded -> decoded */
        alwan_scalar decoded_actual;
        status = alwan_eotf_apply(tf, &encoded_ref, 1, sizeof(alwan_scalar),
                                  &decoded_actual, sizeof(alwan_scalar));

        if (status != ALWAN_OK) {
            fprintf(stderr, "  [%s] EOTF test %zu: alwan_eotf_apply failed with status %d\n",
                   name, i, status);
            failures++;
            continue;
        }

        alwan_scalar decode_diff = ALWAN_FABS(decoded_actual - decoded_ref);
        if (decode_diff >= tolerance) {
            fprintf(stderr, "  [%s] EOTF test %zu: encoded=%.6f, expected=%.6f, got=%.6f, diff=%.2e\n",
                   name, i, encoded_ref, decoded_ref, decoded_actual, decode_diff);
            failures++;
        }
    }

    if (failures == 0) {
        printf("[PASS] %s (%zu tests)\n", name, num_triplets * 2);
        return 0;
    } else {
        fprintf(stderr, "[FAIL] %s (%d/%zu tests failed)\n",
                name, failures, num_triplets * 2);
        return 1;
    }
}

/* ----------------------------------------------------------------
 * Sony S-Log Family Tests
 * ---------------------------------------------------------------- */

static int test_slog(void) {
    size_t num_triplets = sizeof(g_tf_slog) / sizeof(g_tf_slog[0]) / 3;
    return test_transfer_function("S-Log", ALWAN_TF_SLOG, g_tf_slog, num_triplets);
}

static int test_slog2(void) {
    size_t num_triplets = sizeof(g_tf_slog2) / sizeof(g_tf_slog2[0]) / 3;
    return test_transfer_function("S-Log2", ALWAN_TF_SLOG2, g_tf_slog2, num_triplets);
}

static int test_slog3(void) {
    size_t num_triplets = sizeof(g_tf_slog3) / sizeof(g_tf_slog3[0]) / 3;
    return test_transfer_function("S-Log3", ALWAN_TF_SLOG3, g_tf_slog3, num_triplets);
}

/* ----------------------------------------------------------------
 * Canon C-Log Family Tests
 * ---------------------------------------------------------------- */

static int test_clog(void) {
    size_t num_triplets = sizeof(g_tf_clog) / sizeof(g_tf_clog[0]) / 3;
    return test_transfer_function("C-Log", ALWAN_TF_CLOG, g_tf_clog, num_triplets);
}

static int test_clog2(void) {
    size_t num_triplets = sizeof(g_tf_clog2) / sizeof(g_tf_clog2[0]) / 3;
    return test_transfer_function("C-Log2", ALWAN_TF_CLOG2, g_tf_clog2, num_triplets);
}

static int test_clog3(void) {
    size_t num_triplets = sizeof(g_tf_clog3) / sizeof(g_tf_clog3[0]) / 3;
    return test_transfer_function("C-Log3", ALWAN_TF_CLOG3, g_tf_clog3, num_triplets);
}

/* ----------------------------------------------------------------
 * Panasonic V-Log Tests
 * ---------------------------------------------------------------- */

static int test_vlog(void) {
    size_t num_triplets = sizeof(g_tf_vlog) / sizeof(g_tf_vlog[0]) / 3;
    return test_transfer_function("V-Log", ALWAN_TF_VLOG, g_tf_vlog, num_triplets);
}

/* ----------------------------------------------------------------
 * Standard Gamma Variants Tests
 * ---------------------------------------------------------------- */

static int test_gamma22(void) {
    size_t num_triplets = sizeof(g_tf_gamma22) / sizeof(g_tf_gamma22[0]) / 3;
    return test_transfer_function("Gamma 2.2", ALWAN_TF_GAMMA22, g_tf_gamma22, num_triplets);
}

static int test_gamma24(void) {
    size_t num_triplets = sizeof(g_tf_gamma24) / sizeof(g_tf_gamma24[0]) / 3;
    return test_transfer_function("Gamma 2.4", ALWAN_TF_GAMMA24, g_tf_gamma24, num_triplets);
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_260_tf_extended_main(void) {
    printf("\n=== Extended Transfer Functions - colour-science validation ===\n\n");

    int failures = 0;

    /* Sony S-Log Family */
    printf("Sony S-Log Family:\n");
    failures += test_slog();
    failures += test_slog2();
    failures += test_slog3();

    /* Canon C-Log Family */
    printf("\nCanon C-Log Family:\n");
    failures += test_clog();
    failures += test_clog2();
    failures += test_clog3();

    /* Panasonic V-Log */
    printf("\nPanasonic V-Log:\n");
    failures += test_vlog();

    /* Standard Gamma Variants */
    printf("\nGamma Variants:\n");
    failures += test_gamma22();
    failures += test_gamma24();

    if (failures == 0) {
        printf("\n=== All extended transfer function tests passed ===\n");
        return 0;
    } else {
        fprintf(stderr, "\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
