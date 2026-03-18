/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test suite 78: HDR Ecosystem
 * ST.2086 metadata, BT.2446 Methods B/C, BT.2390 EETF,
 * exposure/Reinhard calibrated tone mapping, gamut mapping,
 * display characterization.
 */

#include "test_common.h"
#include "core/alwan_hdr_core.h"
#include "core/alwan_jzazbz_core.h"

/* ----------------------------------------------------------------
 * ST.2086 Metadata
 * ---------------------------------------------------------------- */

static int test_st2086_metadata(void) {
    /* Initialize with BT.2020 primaries + D65 white point */
    alwan_st2086_metadata meta;
    alwan_f64 primaries[6] = {
        ALWAN_LITERAL(0.708), ALWAN_LITERAL(0.292),  /* R */
        ALWAN_LITERAL(0.170), ALWAN_LITERAL(0.797),  /* G */
        ALWAN_LITERAL(0.131), ALWAN_LITERAL(0.046)   /* B */
    };
    alwan_f64 wp[2] = {ALWAN_LITERAL(0.3127), ALWAN_LITERAL(0.3290)};

    int status = alwan_st2086_init(&meta, primaries, wp,
                                    ALWAN_LITERAL(1000.0),
                                    ALWAN_LITERAL(0.001));
    TEST_ASSERT(status == ALWAN_OK, "st2086 init status");
    TEST_ASSERT_NEAR(meta.display_primaries_xy[0], ALWAN_LITERAL(0.708),
                     ALWAN_LITERAL(1e-10), "st2086 rx");
    TEST_ASSERT_NEAR(meta.white_point_xy[0], ALWAN_LITERAL(0.3127),
                     ALWAN_LITERAL(1e-10), "st2086 wx");
    TEST_ASSERT_NEAR(meta.max_luminance, ALWAN_LITERAL(1000.0),
                     ALWAN_LITERAL(1e-10), "st2086 max lum");
    TEST_ASSERT_NEAR(meta.min_luminance, ALWAN_LITERAL(0.001),
                     ALWAN_LITERAL(1e-10), "st2086 min lum");

    /* NULL checks */
    TEST_ASSERT(alwan_st2086_init(NULL, primaries, wp, 1000, 0.001) == ALWAN_E_INVALID,
                "st2086 null meta");

    TEST_PASS("ST.2086 metadata");
}

/* ----------------------------------------------------------------
 * Content Light Level
 * ---------------------------------------------------------------- */

static int test_content_light_level(void) {
    alwan_f64 pixels[] = {
        ALWAN_LITERAL(100.0), ALWAN_LITERAL(200.0), ALWAN_LITERAL(300.0),
        ALWAN_LITERAL(500.0), ALWAN_LITERAL(800.0), ALWAN_LITERAL(100.0),
        ALWAN_LITERAL(900.0), ALWAN_LITERAL(100.0), ALWAN_LITERAL(400.0),
    };
    alwan_content_light_level cll;
    int status = alwan_content_light_level_compute(&cll, pixels, 3,
                                                     3 * sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "cll compute status");
    TEST_ASSERT_NEAR(cll.max_cll, ALWAN_LITERAL(900.0), ALWAN_LITERAL(1e-10),
                     "cll maxcll");
    /* FALL = avg of per-pixel max: (300 + 800 + 900) / 3 = 666.667 */
    TEST_ASSERT_NEAR(cll.max_fall, ALWAN_LITERAL(2000.0) / ALWAN_LITERAL(3.0),
                     ALWAN_LITERAL(1e-6), "cll maxfall");

    TEST_PASS("content light level");
}

/* ----------------------------------------------------------------
 * BT.2446 Method B: SDR to HDR
 * ---------------------------------------------------------------- */

static int test_bt2446b(void) {
    /* Black maps to black */
    {
        alwan_f64 y_hdr = alwan_bt2446b_forward_f64_v(
            ALWAN_LITERAL(0.0), ALWAN_LITERAL(1000.0), ALWAN_LITERAL(100.0));
        TEST_ASSERT_NEAR(y_hdr, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1e-6),
                         "bt2446b: black -> black");
    }

    /* Monotonicity: brighter SDR -> brighter HDR */
    {
        alwan_f64 prev = ALWAN_LITERAL(-1.0);
        for (int i = 0; i <= 10; i++) {
            alwan_f64 x = (alwan_f64)i / ALWAN_LITERAL(10.0);
            alwan_f64 y = alwan_bt2446b_forward_f64_v(
                x, ALWAN_LITERAL(1000.0), ALWAN_LITERAL(100.0));
            TEST_ASSERT(y >= prev, "bt2446b: monotonic");
            prev = y;
        }
    }

    /* API wrapper */
    {
        alwan_f64 y_hdr;
        int status = alwan_bt2446b_forward(&y_hdr, ALWAN_LITERAL(0.5),
                                             ALWAN_LITERAL(1000.0),
                                             ALWAN_LITERAL(100.0));
        TEST_ASSERT(status == ALWAN_OK, "bt2446b api status");
        TEST_ASSERT(y_hdr > ALWAN_LITERAL(0.0), "bt2446b api > 0");
    }

    TEST_PASS("BT.2446 Method B (SDR to HDR)");
}

/* ----------------------------------------------------------------
 * BT.2446 Method C: HDR to SDR
 * ---------------------------------------------------------------- */

static int test_bt2446c(void) {
    /* Black maps to black */
    {
        alwan_f64 y_sdr = alwan_bt2446c_forward_f64_v(
            ALWAN_LITERAL(0.0), ALWAN_LITERAL(1000.0), ALWAN_LITERAL(100.0));
        TEST_ASSERT_NEAR(y_sdr, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1e-6),
                         "bt2446c: black -> black");
    }

    /* Monotonicity */
    {
        alwan_f64 prev = ALWAN_LITERAL(-1.0);
        for (int i = 0; i <= 10; i++) {
            alwan_f64 x = (alwan_f64)i / ALWAN_LITERAL(10.0);
            alwan_f64 y = alwan_bt2446c_forward_f64_v(
                x, ALWAN_LITERAL(1000.0), ALWAN_LITERAL(100.0));
            TEST_ASSERT(y >= prev, "bt2446c: monotonic");
            prev = y;
        }
    }

    /* Output should be in [0,1] */
    {
        alwan_f64 y = alwan_bt2446c_forward_f64_v(
            ALWAN_LITERAL(1.0), ALWAN_LITERAL(1000.0), ALWAN_LITERAL(100.0));
        TEST_ASSERT(y >= ALWAN_LITERAL(0.0) && y <= ALWAN_LITERAL(1.0),
                    "bt2446c: output in [0,1]");
    }

    /* API wrapper */
    {
        alwan_f64 y_sdr;
        int status = alwan_bt2446c_forward(&y_sdr, ALWAN_LITERAL(0.5),
                                             ALWAN_LITERAL(1000.0),
                                             ALWAN_LITERAL(100.0));
        TEST_ASSERT(status == ALWAN_OK, "bt2446c api status");
    }

    TEST_PASS("BT.2446 Method C (HDR to SDR)");
}

/* ----------------------------------------------------------------
 * BT.2390 EETF
 * ---------------------------------------------------------------- */

static int test_bt2390_eetf(void) {
    /* Identity: source range == target range */
    {
        alwan_f64 y = alwan_bt2390_eetf_f64_v(
            ALWAN_LITERAL(0.5),
            ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
            ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
        TEST_ASSERT_NEAR(y, ALWAN_LITERAL(0.5), ALWAN_LITERAL(1e-4),
                         "bt2390: identity");
    }

    /* Black preserved */
    {
        alwan_f64 y = alwan_bt2390_eetf_f64_v(
            ALWAN_LITERAL(0.0),
            ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
            ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.5));
        TEST_ASSERT_NEAR(y, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1e-6),
                         "bt2390: black preserved");
    }

    /* Monotonicity */
    {
        alwan_f64 LW_src = alwan_pq_oetf_f64(ALWAN_LITERAL(10000.0));
        alwan_f64 LW_tgt = alwan_pq_oetf_f64(ALWAN_LITERAL(1000.0));
        alwan_f64 prev = ALWAN_LITERAL(-1.0);
        for (int i = 0; i <= 20; i++) {
            alwan_f64 x = (alwan_f64)i / ALWAN_LITERAL(20.0) * LW_src;
            alwan_f64 y = alwan_bt2390_eetf_f64_v(x,
                ALWAN_LITERAL(0.0), LW_src,
                ALWAN_LITERAL(0.0), LW_tgt);
            TEST_ASSERT(y >= prev, "bt2390: monotonic");
            prev = y;
        }
    }

    /* Luminance convenience wrapper */
    {
        alwan_f64 y;
        int status = alwan_bt2390_eetf_luminance(&y, ALWAN_LITERAL(0.5),
                                                   ALWAN_LITERAL(10000.0),
                                                   ALWAN_LITERAL(1000.0));
        TEST_ASSERT(status == ALWAN_OK, "bt2390 luminance api status");
    }

    TEST_PASS("BT.2390 EETF");
}

/* ----------------------------------------------------------------
 * Exposure-Based Tone Mapping
 * ---------------------------------------------------------------- */

static int test_exposure_tonemap(void) {
    /* Zero exposure: 1 - exp(-L) */
    {
        alwan_f64 y = alwan_exposure_tonemap_f64_v(ALWAN_LITERAL(1.0), ALWAN_ZERO);
        alwan_f64 expected = ALWAN_ONE - ALWAN_EXP(-ALWAN_ONE);
        TEST_ASSERT_NEAR(y, expected, ALWAN_LITERAL(1e-10),
                         "exposure: 0 EV at L=1");
    }

    /* +1 EV: 1 - exp(-2*L) */
    {
        alwan_f64 y = alwan_exposure_tonemap_f64_v(ALWAN_LITERAL(1.0), ALWAN_ONE);
        alwan_f64 expected = ALWAN_ONE - ALWAN_EXP(-ALWAN_LITERAL(2.0));
        TEST_ASSERT_NEAR(y, expected, ALWAN_LITERAL(1e-10),
                         "exposure: +1 EV at L=1");
    }

    /* Black preserved */
    {
        alwan_f64 y = alwan_exposure_tonemap_f64_v(ALWAN_ZERO, ALWAN_ZERO);
        TEST_ASSERT_NEAR(y, ALWAN_ZERO, ALWAN_LITERAL(1e-10),
                         "exposure: black preserved");
    }

    /* Monotonicity */
    {
        alwan_f64 prev = ALWAN_LITERAL(-1.0);
        for (int i = 0; i <= 10; i++) {
            alwan_f64 L = (alwan_f64)i / ALWAN_LITERAL(10.0) * ALWAN_LITERAL(5.0);
            alwan_f64 y = alwan_exposure_tonemap_f64_v(L, ALWAN_ZERO);
            TEST_ASSERT(y >= prev, "exposure: monotonic");
            prev = y;
        }
    }

    /* RGB version */
    {
        alwan_vec3 in = {{ALWAN_LITERAL(0.5), ALWAN_LITERAL(1.0), ALWAN_LITERAL(2.0)}};
        alwan_vec3 out = alwan_exposure_tonemap_rgb_f64_v(in, ALWAN_ZERO);
        for (int c = 0; c < 3; c++) {
            TEST_ASSERT(out.v[c] >= ALWAN_ZERO && out.v[c] <= ALWAN_ONE,
                        "exposure rgb: output in [0,1]");
        }
    }

    /* API wrapper */
    {
        alwan_f64 y;
        int status = alwan_exposure_tonemap(&y, ALWAN_LITERAL(0.5), ALWAN_ZERO);
        TEST_ASSERT(status == ALWAN_OK, "exposure api status");
    }

    TEST_PASS("exposure-based tone mapping");
}

/* ----------------------------------------------------------------
 * Reinhard Calibrated
 * ---------------------------------------------------------------- */

static int test_reinhard_calibrated(void) {
    /* Black preserved */
    {
        alwan_f64 y = alwan_reinhard_calibrated_f64_v(
            ALWAN_ZERO, ALWAN_LITERAL(0.18),
            ALWAN_LITERAL(0.18), ALWAN_LITERAL(4.0));
        TEST_ASSERT_NEAR(y, ALWAN_ZERO, ALWAN_LITERAL(1e-10),
                         "reinhard cal: black preserved");
    }

    /* Monotonicity */
    {
        alwan_f64 prev = ALWAN_LITERAL(-1.0);
        for (int i = 0; i <= 10; i++) {
            alwan_f64 L = (alwan_f64)i / ALWAN_LITERAL(10.0) * ALWAN_LITERAL(5.0);
            alwan_f64 y = alwan_reinhard_calibrated_f64_v(
                L, ALWAN_LITERAL(0.18),
                ALWAN_LITERAL(0.18), ALWAN_LITERAL(4.0));
            TEST_ASSERT(y >= prev, "reinhard cal: monotonic");
            prev = y;
        }
    }

    /* L_white acts as hard clip: L >= L_white maps to ~1 */
    {
        alwan_f64 y = alwan_reinhard_calibrated_f64_v(
            ALWAN_LITERAL(100.0), ALWAN_LITERAL(0.18),
            ALWAN_LITERAL(0.18), ALWAN_LITERAL(4.0));
        TEST_ASSERT(y > ALWAN_LITERAL(0.9),
                    "reinhard cal: high input near 1.0");
    }

    /* API wrapper */
    {
        alwan_f64 y;
        int status = alwan_reinhard_calibrated(&y, ALWAN_LITERAL(0.5),
                                                ALWAN_LITERAL(0.18),
                                                ALWAN_LITERAL(0.18),
                                                ALWAN_LITERAL(4.0));
        TEST_ASSERT(status == ALWAN_OK, "reinhard cal api status");
    }

    TEST_PASS("Reinhard calibrated tone mapping");
}

/* ----------------------------------------------------------------
 * HDR Gamut Mapping (JzCzhz chroma compression)
 * ---------------------------------------------------------------- */

static int test_hdr_gamut_map(void) {
    /* In-gamut: no compression */
    {
        alwan_jzczhz in = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.1), ALWAN_LITERAL(180.0)};
        alwan_jzczhz out = alwan_hdr_gamut_map_jzczhz_f64_v(in, ALWAN_LITERAL(0.2));
        TEST_ASSERT_NEAR(out.Cz, in.Cz, ALWAN_LITERAL(1e-10),
                         "gamut map: in-gamut preserved");
        TEST_ASSERT_NEAR(out.Jz, in.Jz, ALWAN_LITERAL(1e-10),
                         "gamut map: Jz preserved");
        TEST_ASSERT_NEAR(out.hz, in.hz, ALWAN_LITERAL(1e-10),
                         "gamut map: hz preserved");
    }

    /* Out-of-gamut: compressed */
    {
        alwan_jzczhz in = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.3), ALWAN_LITERAL(90.0)};
        alwan_jzczhz out = alwan_hdr_gamut_map_jzczhz_f64_v(in, ALWAN_LITERAL(0.15));
        TEST_ASSERT(out.Cz < in.Cz, "gamut map: chroma compressed");
        TEST_ASSERT_NEAR(out.Jz, in.Jz, ALWAN_LITERAL(1e-10),
                         "gamut map: Jz preserved when compressed");
        TEST_ASSERT_NEAR(out.hz, in.hz, ALWAN_LITERAL(1e-10),
                         "gamut map: hz preserved when compressed");
    }

    /* API wrapper */
    {
        alwan_jzczhz in = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.3), ALWAN_LITERAL(90.0)};
        alwan_jzczhz out;
        alwan_hdr_gamut_map_jzczhz(&out, &in, ALWAN_LITERAL(0.15));
    }

    TEST_PASS("HDR gamut mapping (JzCzhz)");
}

/* ----------------------------------------------------------------
 * PQ Peak Luminance Normalization
 * ---------------------------------------------------------------- */

static int test_pq_normalize(void) {
    /* PQ value below peak: pass through */
    {
        alwan_f64 pq_100 = alwan_pq_oetf_f64(100.0);
        alwan_f64 out = alwan_pq_normalize_peak_f64_v(pq_100, 1000.0);
        TEST_ASSERT_NEAR(out, pq_100, ALWAN_LITERAL(1e-6),
                         "pq normalize: below peak unchanged");
    }

    /* PQ value at peak: clipped */
    {
        alwan_f64 pq_max = alwan_pq_oetf_f64(ALWAN_LITERAL(10000.0));
        alwan_f64 out = alwan_pq_normalize_peak_f64_v(pq_max, ALWAN_LITERAL(1000.0));
        alwan_f64 expected = alwan_pq_oetf_f64(ALWAN_LITERAL(1000.0));
        TEST_ASSERT_NEAR(out, expected, ALWAN_LITERAL(1e-4),
                         "pq normalize: clipped at peak");
    }

    /* Black preserved */
    {
        alwan_f64 out = alwan_pq_normalize_peak_f64_v(ALWAN_ZERO, ALWAN_LITERAL(1000.0));
        TEST_ASSERT_NEAR(out, ALWAN_ZERO, ALWAN_LITERAL(1e-6),
                         "pq normalize: black preserved");
    }

    /* API wrapper */
    {
        alwan_f64 out;
        int status = alwan_pq_normalize_peak(&out, ALWAN_LITERAL(0.5),
                                               ALWAN_LITERAL(1000.0));
        TEST_ASSERT(status == ALWAN_OK, "pq normalize api status");
    }

    TEST_PASS("PQ peak luminance normalization");
}

/* ----------------------------------------------------------------
 * View Transform Dispatch (BT.2446 B/C, BT.2390, Reinhard Cal, Exposure)
 * ---------------------------------------------------------------- */

static int test_hdr_view_transforms(void) {
    alwan_view_transform hdr_vts[] = {
        ALWAN_VIEW_BT2446B_SDR_TO_HDR,
        ALWAN_VIEW_BT2446C_HDR_TO_SDR,
        ALWAN_VIEW_BT2390_HDR_TO_SDR,
        ALWAN_VIEW_REINHARD_CALIBRATED,
        ALWAN_VIEW_EXPOSURE
    };
    char const *names[] = {
        "bt2446b", "bt2446c", "bt2390", "reinhard_cal", "exposure"
    };
    size_t const n = sizeof(hdr_vts) / sizeof(hdr_vts[0]);

    for (size_t t = 0; t < n; t++) {
        /* Basic: dispatch should succeed */
        alwan_f64 input[3] = {
            ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.2)
        };
        alwan_f64 output[3];
        int status = alwan_view_transform_apply(output, NULL, hdr_vts[t],
                                                 input, 1,
                                                 3 * sizeof(alwan_f64),
                                                 3 * sizeof(alwan_f64));
        char msg[128];
        snprintf(msg, sizeof(msg), "%s: dispatch ok", names[t]);
        TEST_ASSERT(status == ALWAN_OK, msg);

        /* Output should not be NaN */
        snprintf(msg, sizeof(msg), "%s: no NaN", names[t]);
        TEST_ASSERT(output[0] == output[0] && output[1] == output[1] &&
                    output[2] == output[2], msg);
    }

    TEST_PASS("HDR view transforms dispatch");
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

int test_78_hdr_ecosystem_main(void) {
    int failures = 0;

    failures += test_st2086_metadata();
    failures += test_content_light_level();
    failures += test_bt2446b();
    failures += test_bt2446c();
    failures += test_bt2390_eetf();
    failures += test_exposure_tonemap();
    failures += test_reinhard_calibrated();
    failures += test_hdr_gamut_map();
    failures += test_pq_normalize();
    failures += test_hdr_view_transforms();

    if (failures == 0) {
        printf("\n=== All HDR ecosystem tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
