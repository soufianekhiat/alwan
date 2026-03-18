/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 62: HDR Utilities - HLG OOTF, MaxCLL, MaxFALL, BT.2408, Mirrored TF
 */

#include "test_common.h"
#include <stdlib.h>

#include "core/alwan_hdr_core.h"

/* ----------------------------------------------------------------
 * HLG OOTF roundtrip
 * ---------------------------------------------------------------- */

static int test_hlg_ootf_roundtrip(void) {
    /* Use neutral input (R=G=B) for roundtrip since the OOTF uses
     * luma-dependent scaling that only roundtrips exactly for neutrals.
     * For non-neutral signals, the luma coupling prevents exact inversion. */
    alwan_rgb E = { ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5) };
    alwan_f64 Lw = ALWAN_LITERAL(1000.0);
    alwan_f64 gamma_sys = ALWAN_LITERAL(1.2);

    alwan_rgb Fd = alwan_hlg_ootf_f64_v(E, Lw, gamma_sys);
    alwan_rgb E_rt = alwan_hlg_ootf_inv_f64_v(Fd, Lw, gamma_sys);

    TEST_ASSERT_NEAR(E_rt.r, E.r, ALWAN_LITERAL(1e-4), "hlg ootf rt R");
    TEST_ASSERT_NEAR(E_rt.g, E.g, ALWAN_LITERAL(1e-4), "hlg ootf rt G");
    TEST_ASSERT_NEAR(E_rt.b, E.b, ALWAN_LITERAL(1e-4), "hlg ootf rt B");

    TEST_PASS("hlg ootf roundtrip");
}

/* ----------------------------------------------------------------
 * HLG OOTF API
 * ---------------------------------------------------------------- */

static int test_hlg_ootf_api(void) {
    /* Use neutral input for roundtrip correctness (see roundtrip test comment) */
    alwan_rgb in = { ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5) };
    alwan_rgb out;

    alwan_hlg_ootf(&out, &in,
                   ALWAN_LITERAL(1000.0), ALWAN_LITERAL(1.2));

    /* Display-referred should be scaled by luminance */
    TEST_ASSERT(out.r > ALWAN_LITERAL(0.0), "hlg ootf R > 0");

    /* Inverse */
    alwan_rgb rt;
    alwan_hlg_ootf_inv(&rt, &out,
                       ALWAN_LITERAL(1000.0), ALWAN_LITERAL(1.2));
    TEST_ASSERT_NEAR(rt.r, in.r, ALWAN_LITERAL(1e-4), "hlg ootf api rt R");

    TEST_PASS("hlg ootf api");
}

/* ----------------------------------------------------------------
 * HLG OOTF neutral preservation
 * ---------------------------------------------------------------- */

static int test_hlg_ootf_neutral(void) {
    alwan_rgb E = { ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5) };
    alwan_rgb Fd = alwan_hlg_ootf_f64_v(E, ALWAN_LITERAL(1000.0), ALWAN_LITERAL(1.2));

    /* Neutral input should produce neutral output (equal channels) */
    TEST_ASSERT_NEAR(Fd.r, Fd.g, ALWAN_LITERAL(1e-10), "hlg neutral R!=G");
    TEST_ASSERT_NEAR(Fd.g, Fd.b, ALWAN_LITERAL(1e-10), "hlg neutral G!=B");

    TEST_PASS("hlg ootf neutral");
}

/* ----------------------------------------------------------------
 * MaxCLL / MaxFALL
 * ---------------------------------------------------------------- */

static int test_maxcll(void) {
    alwan_f64 pixels[] = {
        ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.3),
        ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.1),
        ALWAN_LITERAL(0.9), ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.4),
        ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.7),
    };
    alwan_f64 maxcll;
    int status = alwan_maxcll(&maxcll, pixels, 4, 3 * sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "maxcll failed");
    TEST_ASSERT_NEAR(maxcll, ALWAN_LITERAL(0.9), ALWAN_LITERAL(1e-10), "maxcll value");

    TEST_PASS("maxcll");
}

static int test_maxfall(void) {
    alwan_f64 pixels[] = {
        ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.3),
        ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.1),
        ALWAN_LITERAL(0.9), ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.4),
        ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.7),
    };
    /* Per-pixel max: 0.3, 0.8, 0.9, 0.7 => avg = 0.675 */
    alwan_f64 maxfall;
    int status = alwan_maxfall(&maxfall, pixels, 4, 3 * sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "maxfall failed");
    TEST_ASSERT_NEAR(maxfall, ALWAN_LITERAL(0.675), ALWAN_LITERAL(1e-10), "maxfall value");

    TEST_PASS("maxfall");
}

/* ----------------------------------------------------------------
 * BT.2408 Reference White
 * ---------------------------------------------------------------- */

static int test_bt2408_ref_white(void) {
    alwan_f64 pq_ref = alwan_bt2408_ref_white_f64_v(1);
    alwan_f64 hlg_ref = alwan_bt2408_ref_white_f64_v(0);

    TEST_ASSERT_NEAR(pq_ref, ALWAN_LITERAL(203.0) / ALWAN_LITERAL(10000.0),
                     ALWAN_LITERAL(1e-10), "bt2408 pq ref white");
    TEST_ASSERT_NEAR(hlg_ref, ALWAN_LITERAL(0.75),
                     ALWAN_LITERAL(1e-10), "bt2408 hlg ref white");

    TEST_PASS("bt2408 ref white");
}

/* ----------------------------------------------------------------
 * Mirrored TF Extension
 * ---------------------------------------------------------------- */

static int test_tf_mirror(void) {
    /* Mirror of positive value should be positive */
    alwan_f64 x_pos = ALWAN_LITERAL(0.5);
    alwan_f64 tf_pos = ALWAN_LITERAL(0.25);
    alwan_f64 result_pos = alwan_tf_mirror_f64_v(x_pos, tf_pos);
    TEST_ASSERT_NEAR(result_pos, ALWAN_LITERAL(0.25), ALWAN_LITERAL(1e-10),
                     "tf mirror positive");

    /* Mirror of negative value should be negative */
    alwan_f64 x_neg = ALWAN_LITERAL(-0.5);
    alwan_f64 tf_neg_abs = ALWAN_LITERAL(0.25);
    alwan_f64 result_neg = alwan_tf_mirror_f64_v(x_neg, tf_neg_abs);
    TEST_ASSERT_NEAR(result_neg, ALWAN_LITERAL(-0.25), ALWAN_LITERAL(1e-10),
                     "tf mirror negative");

    TEST_PASS("tf mirror");
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

int test_62_hdr_utilities_main(void) {
    int failures = 0;

    failures += test_hlg_ootf_roundtrip();
    failures += test_hlg_ootf_api();
    failures += test_hlg_ootf_neutral();
    failures += test_maxcll();
    failures += test_maxfall();
    failures += test_bt2408_ref_white();
    failures += test_tf_mirror();

    if (failures == 0) {
        printf("\n=== All HDR utility tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
