/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 67: HWB roundtrip, Arbitrary Gamma TF, D-Series Illuminant
 */

#include "test_common.h"
#include <stdlib.h>

#include "core/alwan_convenience_core.h"
#include "core/alwan_rgb_core.h"
#include "core/alwan_quality_core.h"

/* ----------------------------------------------------------------
 * HWB <-> RGB roundtrip
 * ---------------------------------------------------------------- */

static int test_hwb_roundtrip(void) {
    alwan_rgb rgb_in = { ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.1) };
    alwan_hwb_f64 hwb = alwan_rgb_to_hwb_f64_v(rgb_in);
    alwan_rgb rgb_rt = alwan_hwb_to_rgb_f64_v(hwb);

    TEST_ASSERT_NEAR(rgb_rt.r, rgb_in.r, ALWAN_LITERAL(1e-6), "hwb rt R");
    TEST_ASSERT_NEAR(rgb_rt.g, rgb_in.g, ALWAN_LITERAL(1e-6), "hwb rt G");
    TEST_ASSERT_NEAR(rgb_rt.b, rgb_in.b, ALWAN_LITERAL(1e-6), "hwb rt B");

    TEST_PASS("hwb roundtrip");
}

/* ----------------------------------------------------------------
 * HWB: known values for pure colors
 * ---------------------------------------------------------------- */

static int test_hwb_pure_colors(void) {
    /* Pure red: H=0, W=0, B=0 (in HSV: H=0, S=1, V=1) */
    alwan_rgb red = { ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0) };
    alwan_hwb_f64 hwb_red = alwan_rgb_to_hwb_f64_v(red);
    TEST_ASSERT_NEAR(hwb_red.w, ALWAN_ZERO, ALWAN_LITERAL(1e-6), "hwb red W=0");
    TEST_ASSERT_NEAR(hwb_red.b, ALWAN_ZERO, ALWAN_LITERAL(1e-6), "hwb red B=0");

    /* White: H=undefined, W=1, B=0 */
    alwan_rgb white = { ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0) };
    alwan_hwb_f64 hwb_white = alwan_rgb_to_hwb_f64_v(white);
    TEST_ASSERT_NEAR(hwb_white.w, ALWAN_ONE, ALWAN_LITERAL(1e-6), "hwb white W=1");
    TEST_ASSERT_NEAR(hwb_white.b, ALWAN_ZERO, ALWAN_LITERAL(1e-6), "hwb white B=0");

    /* Black: H=undefined, W=0, B=1 */
    alwan_rgb black = { ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0) };
    alwan_hwb_f64 hwb_black = alwan_rgb_to_hwb_f64_v(black);
    TEST_ASSERT_NEAR(hwb_black.w, ALWAN_ZERO, ALWAN_LITERAL(1e-6), "hwb black W=0");
    TEST_ASSERT_NEAR(hwb_black.b, ALWAN_ONE, ALWAN_LITERAL(1e-6), "hwb black B=1");

    TEST_PASS("hwb pure colors");
}

/* ----------------------------------------------------------------
 * HWB API
 * ---------------------------------------------------------------- */

static int test_hwb_api(void) {
    alwan_rgb rgb_in = { ALWAN_LITERAL(0.6), ALWAN_LITERAL(0.4), ALWAN_LITERAL(0.2) };
    alwan_f64 hwb_out[3];

    alwan_rgb_to_hwb(hwb_out, &rgb_in);

    alwan_rgb rgb_rt;
    alwan_hwb_to_rgb(&rgb_rt, hwb_out);

    TEST_ASSERT_NEAR(rgb_rt.r, rgb_in.r, ALWAN_LITERAL(1e-6), "hwb api rt R");
    TEST_ASSERT_NEAR(rgb_rt.g, rgb_in.g, ALWAN_LITERAL(1e-6), "hwb api rt G");
    TEST_ASSERT_NEAR(rgb_rt.b, rgb_in.b, ALWAN_LITERAL(1e-6), "hwb api rt B");

    TEST_PASS("hwb api");
}

/* ----------------------------------------------------------------
 * Arbitrary Gamma TF roundtrip
 * ---------------------------------------------------------------- */

static int test_gamma_roundtrip(void) {
    alwan_f64 gammas[] = {
        ALWAN_LITERAL(1.8), ALWAN_LITERAL(2.2), ALWAN_LITERAL(2.6)
    };

    for (int g = 0; g < 3; g++) {
        alwan_f64 gamma = gammas[g];
        alwan_f64 test_values[] = {
            ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.5), ALWAN_LITERAL(1.0)
        };
        for (int i = 0; i < 4; i++) {
            alwan_f64 linear = test_values[i];
            alwan_f64 encoded = alwan_gamma_oetf_f64_v(linear, gamma);
            alwan_f64 decoded = alwan_gamma_eotf_f64_v(encoded, gamma);
            TEST_ASSERT_NEAR(decoded, linear, ALWAN_LITERAL(1e-6), "gamma roundtrip");
        }
    }

    TEST_PASS("gamma roundtrip");
}

/* ----------------------------------------------------------------
 * Gamma TF: known values
 * ---------------------------------------------------------------- */

static int test_gamma_known(void) {
    /* gamma=2.0: oetf(0.25) = sqrt(0.25) = 0.5 */
    alwan_f64 result = alwan_gamma_oetf_f64_v(ALWAN_LITERAL(0.25), ALWAN_LITERAL(2.0));
    TEST_ASSERT_NEAR(result, ALWAN_LITERAL(0.5), ALWAN_LITERAL(1e-6), "gamma 2.0 oetf(0.25)");

    /* gamma=2.0: eotf(0.5) = 0.5^2 = 0.25 */
    alwan_f64 result2 = alwan_gamma_eotf_f64_v(ALWAN_LITERAL(0.5), ALWAN_LITERAL(2.0));
    TEST_ASSERT_NEAR(result2, ALWAN_LITERAL(0.25), ALWAN_LITERAL(1e-6), "gamma 2.0 eotf(0.5)");

    TEST_PASS("gamma known");
}

/* ----------------------------------------------------------------
 * Gamma TF API
 * ---------------------------------------------------------------- */

static int test_gamma_api(void) {
    alwan_f64 in_val = ALWAN_LITERAL(0.25);
    alwan_f64 out_val;

    int status = alwan_gamma_oetf(&out_val, &in_val, ALWAN_LITERAL(2.0),
                                   1, sizeof(alwan_f64), sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "gamma oetf api failed");
    TEST_ASSERT_NEAR(out_val, ALWAN_LITERAL(0.5), ALWAN_LITERAL(1e-6), "gamma oetf api value");

    alwan_f64 decoded;
    status = alwan_gamma_eotf(&decoded, &out_val, ALWAN_LITERAL(2.0),
                               1, sizeof(alwan_f64), sizeof(alwan_f64));
    TEST_ASSERT(status == ALWAN_OK, "gamma eotf api failed");
    TEST_ASSERT_NEAR(decoded, ALWAN_LITERAL(0.25), ALWAN_LITERAL(1e-6), "gamma eotf api value");

    TEST_PASS("gamma api");
}

/* ----------------------------------------------------------------
 * D-Series Illuminant: known reference points
 * ---------------------------------------------------------------- */

static int test_d_series_known(void) {
    /* D65 at 6504K should give approximately x=0.3127, y=0.3290 */
    alwan_vec2 d65 = alwan_d_series_xy_f64_v(ALWAN_LITERAL(6504.0));
    TEST_ASSERT_NEAR(d65.v[0], ALWAN_LITERAL(0.3127), ALWAN_LITERAL(5e-3), "d65 x");
    TEST_ASSERT_NEAR(d65.v[1], ALWAN_LITERAL(0.3290), ALWAN_LITERAL(5e-3), "d65 y");

    /* D50 at ~5003K should give approximately x=0.3457, y=0.3585 */
    alwan_vec2 d50 = alwan_d_series_xy_f64_v(ALWAN_LITERAL(5003.0));
    TEST_ASSERT_NEAR(d50.v[0], ALWAN_LITERAL(0.3457), ALWAN_LITERAL(5e-3), "d50 x");
    TEST_ASSERT_NEAR(d50.v[1], ALWAN_LITERAL(0.3585), ALWAN_LITERAL(5e-3), "d50 y");

    TEST_PASS("d series known");
}

/* ----------------------------------------------------------------
 * D-Series Illuminant: monotonicity of x chromaticity
 * ---------------------------------------------------------------- */

static int test_d_series_range(void) {
    /* All D-series results should be reasonable xy chromaticity */
    alwan_f64 ccts[] = {
        ALWAN_LITERAL(4000.0), ALWAN_LITERAL(5000.0), ALWAN_LITERAL(6500.0),
        ALWAN_LITERAL(10000.0), ALWAN_LITERAL(15000.0), ALWAN_LITERAL(25000.0)
    };

    for (int i = 0; i < 6; i++) {
        alwan_vec2 xy = alwan_d_series_xy_f64_v(ccts[i]);
        TEST_ASSERT(xy.v[0] > ALWAN_LITERAL(0.2) && xy.v[0] < ALWAN_LITERAL(0.5),
                    "d series x in range");
        TEST_ASSERT(xy.v[1] > ALWAN_LITERAL(0.2) && xy.v[1] < ALWAN_LITERAL(0.4),
                    "d series y in range");
    }

    TEST_PASS("d series range");
}

/* ----------------------------------------------------------------
 * D-Series API
 * ---------------------------------------------------------------- */

static int test_d_series_api(void) {
    alwan_vec2 xy;
    int status = alwan_d_series_illuminant_xy(&xy, ALWAN_LITERAL(6504.0));
    TEST_ASSERT(status == ALWAN_OK, "d series api failed");
    TEST_ASSERT_NEAR(xy.v[0], ALWAN_LITERAL(0.3127), ALWAN_LITERAL(5e-3), "d series api x");

    TEST_PASS("d series api");
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

int test_67_hwb_gamma_dseries_main(void) {
    int failures = 0;

    failures += test_hwb_roundtrip();
    failures += test_hwb_pure_colors();
    failures += test_hwb_api();
    failures += test_gamma_roundtrip();
    failures += test_gamma_known();
    failures += test_gamma_api();
    failures += test_d_series_known();
    failures += test_d_series_range();
    failures += test_d_series_api();

    if (failures == 0) {
        printf("\n=== All HWB/gamma/D-series tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
