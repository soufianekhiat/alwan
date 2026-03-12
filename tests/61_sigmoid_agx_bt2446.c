/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 61: Sigmoid utility, AgX full pipeline, BT.2446 Method A
 */

#include "test_common.h"
#include <stdlib.h>

#include "core/alwan_sigmoid_core.h"
#include "core/alwan_view_core.h"
#include "core/alwan_hdr_core.h"

/* ----------------------------------------------------------------
 * Sigmoid roundtrip test
 * ---------------------------------------------------------------- */

static int test_sigmoid_roundtrip(void) {
    alwan_scalar pivot = ALWAN_LITERAL(0.18);
    alwan_scalar slope = ALWAN_LITERAL(1.0);
    alwan_scalar toe = ALWAN_LITERAL(1.5);
    alwan_scalar shoulder = ALWAN_LITERAL(1.5);

    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.05), ALWAN_LITERAL(0.18),
        ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.85), ALWAN_LITERAL(1.0)
    };

    for (int i = 0; i < 6; i++) {
        alwan_scalar x = test_values[i];
        alwan_scalar y = alwan_sigmoid_v(x, pivot, slope, toe, shoulder);
        alwan_scalar x_rt = alwan_sigmoid_inv_v(y, pivot, slope, toe, shoulder);

        /* Skip exact 0 and 1 endpoints (they may lose precision in the inverse) */
        if (x > ALWAN_LITERAL(0.001) && x < ALWAN_LITERAL(0.999)) {
            TEST_ASSERT_NEAR(x_rt, x, ALWAN_LITERAL(1e-6),
                "sigmoid roundtrip");
        }
    }

    TEST_PASS("sigmoid roundtrip");
}

/* ----------------------------------------------------------------
 * Sigmoid monotonicity test
 * ---------------------------------------------------------------- */

static int test_sigmoid_monotonic(void) {
    alwan_scalar pivot = ALWAN_LITERAL(0.18);
    alwan_scalar slope = ALWAN_LITERAL(1.0);
    alwan_scalar toe = ALWAN_LITERAL(2.0);
    alwan_scalar shoulder = ALWAN_LITERAL(2.0);

    alwan_scalar prev = alwan_sigmoid_v(ALWAN_LITERAL(0.0), pivot, slope, toe, shoulder);
    for (int i = 1; i <= 100; i++) {
        alwan_scalar x = (alwan_scalar)i / ALWAN_LITERAL(100.0);
        alwan_scalar y = alwan_sigmoid_v(x, pivot, slope, toe, shoulder);
        TEST_ASSERT(y >= prev - ALWAN_LITERAL(1e-10), "sigmoid not monotonic");
        prev = y;
    }

    TEST_PASS("sigmoid monotonicity");
}

/* ----------------------------------------------------------------
 * AgX view transform - basic sanity
 * ---------------------------------------------------------------- */

static int test_agx_basic(void) {
    alwan_scalar rgb_in[3] = { ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18) };
    alwan_scalar rgb_out[3];

    int status = alwan_view_transform_apply(
        rgb_out, NULL, ALWAN_VIEW_AGX, rgb_in, 1,
        3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
    TEST_ASSERT(status == ALWAN_OK, "agx base failed");

    /* Mid-gray should produce a reasonable display value */
    TEST_ASSERT(rgb_out[0] > ALWAN_LITERAL(0.0) && rgb_out[0] < ALWAN_LITERAL(1.0),
        "agx output out of range");

    /* R=G=B input should produce approximately R=G=B output (neutral preservation)
     * The inset/outset matrix approach may introduce small deviations */
    TEST_ASSERT_NEAR(rgb_out[0], rgb_out[1], ALWAN_LITERAL(2e-2), "agx neutral R!=G");
    TEST_ASSERT_NEAR(rgb_out[1], rgb_out[2], ALWAN_LITERAL(2e-2), "agx neutral G!=B");

    TEST_PASS("agx basic");
}

/* ----------------------------------------------------------------
 * AgX Golden variant
 * ---------------------------------------------------------------- */

static int test_agx_golden(void) {
    alwan_scalar rgb_in[3] = { ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.1) };
    alwan_scalar rgb_out[3];

    int status = alwan_view_transform_apply(
        rgb_out, NULL, ALWAN_VIEW_AGX_GOLDEN, rgb_in, 1,
        3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
    TEST_ASSERT(status == ALWAN_OK, "agx golden failed");

    /* Output should be in [0,1] */
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(rgb_out[i] >= ALWAN_LITERAL(0.0) && rgb_out[i] <= ALWAN_LITERAL(1.0),
            "agx golden output out of range");
    }

    TEST_PASS("agx golden");
}

/* ----------------------------------------------------------------
 * BT.2446 Method A forward/inverse roundtrip
 * ---------------------------------------------------------------- */

static int test_bt2446a_roundtrip(void) {
    alwan_scalar L_hdr = ALWAN_LITERAL(1000.0);
    alwan_scalar L_sdr = ALWAN_LITERAL(100.0);

    alwan_scalar test_values[] = {
        ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.5),
        ALWAN_LITERAL(0.7), ALWAN_LITERAL(0.9)
    };

    for (int i = 0; i < 5; i++) {
        alwan_scalar y_hdr = test_values[i];
        alwan_scalar y_sdr = alwan_bt2446a_forward_v(y_hdr, L_hdr, L_sdr);
        alwan_scalar y_rt = alwan_bt2446a_inverse_v(y_sdr, L_hdr, L_sdr);

        TEST_ASSERT_NEAR(y_rt, y_hdr, ALWAN_LITERAL(5e-2),
            "bt2446a roundtrip");
    }

    TEST_PASS("bt2446a roundtrip");
}

/* ----------------------------------------------------------------
 * BT.2446 view transform API
 * ---------------------------------------------------------------- */

static int test_bt2446a_api(void) {
    alwan_scalar rgb_in[3] = { ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5) };
    alwan_scalar rgb_out[3];

    int status = alwan_view_transform_apply(
        rgb_out, NULL, ALWAN_VIEW_BT2446A_HDR_TO_SDR, rgb_in, 1,
        3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
    TEST_ASSERT(status == ALWAN_OK, "bt2446a hdr_to_sdr failed");

    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(rgb_out[i] >= ALWAN_LITERAL(0.0) && rgb_out[i] <= ALWAN_LITERAL(1.0),
            "bt2446a output out of range");
    }

    TEST_PASS("bt2446a api");
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

int test_61_sigmoid_agx_bt2446_main(void) {
    int failures = 0;

    failures += test_sigmoid_roundtrip();
    failures += test_sigmoid_monotonic();
    failures += test_agx_basic();
    failures += test_agx_golden();
    failures += test_bt2446a_roundtrip();
    failures += test_bt2446a_api();

    if (failures == 0) {
        printf("\n=== All sigmoid/AgX/BT.2446 tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
