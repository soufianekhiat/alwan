/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 63: CAM18sl - Color Appearance Model for Self-Luminous Stimuli
 */

#include "test_common.h"
#include <stdlib.h>

#include "core/alwan_cam18sl_core.h"

/* ----------------------------------------------------------------
 * CAM18sl forward/inverse roundtrip (core)
 * ---------------------------------------------------------------- */

static int test_cam18sl_roundtrip(void) {
    alwan_xyz xyz_in = { ALWAN_LITERAL(50.0), ALWAN_LITERAL(50.0), ALWAN_LITERAL(50.0) };
    alwan_f64 Y_b = ALWAN_LITERAL(20.0);

    alwan_cam18sl_v_correlates_f64 fwd = alwan_cam18sl_forward_f64_v(xyz_in, Y_b);

    /* Sanity: brightness should be positive */
    TEST_ASSERT(fwd.Q > ALWAN_ZERO, "cam18sl Q > 0");

    alwan_xyz xyz_rt = alwan_cam18sl_inverse_f64_v(fwd, Y_b);

    TEST_ASSERT_NEAR(xyz_rt.x, xyz_in.x, ALWAN_LITERAL(1e-6), "cam18sl rt X");
    TEST_ASSERT_NEAR(xyz_rt.y, xyz_in.y, ALWAN_LITERAL(1e-6), "cam18sl rt Y");
    TEST_ASSERT_NEAR(xyz_rt.z, xyz_in.z, ALWAN_LITERAL(1e-6), "cam18sl rt Z");

    TEST_PASS("cam18sl roundtrip");
}

/* ----------------------------------------------------------------
 * CAM18sl achromatic input (neutral)
 * ---------------------------------------------------------------- */

static int test_cam18sl_achromatic(void) {
    alwan_xyz xyz_in = { ALWAN_LITERAL(95.047), ALWAN_LITERAL(100.0), ALWAN_LITERAL(108.883) };
    alwan_f64 Y_b = ALWAN_LITERAL(20.0);

    alwan_cam18sl_v_correlates_f64 fwd = alwan_cam18sl_forward_f64_v(xyz_in, Y_b);

    /* D65 white should have very low colorfulness */
    TEST_ASSERT(fwd.C < ALWAN_LITERAL(5.0), "cam18sl achromatic C should be low");
    TEST_ASSERT(fwd.Q > ALWAN_ZERO, "cam18sl achromatic Q > 0");

    TEST_PASS("cam18sl achromatic");
}

/* ----------------------------------------------------------------
 * CAM18sl chromatic input
 * ---------------------------------------------------------------- */

static int test_cam18sl_chromatic(void) {
    /* A saturated red */
    alwan_xyz xyz_in = { ALWAN_LITERAL(40.0), ALWAN_LITERAL(20.0), ALWAN_LITERAL(5.0) };
    alwan_f64 Y_b = ALWAN_LITERAL(20.0);

    alwan_cam18sl_v_correlates_f64 fwd = alwan_cam18sl_forward_f64_v(xyz_in, Y_b);

    /* Should have significant colorfulness */
    TEST_ASSERT(fwd.C > ALWAN_LITERAL(0.5), "cam18sl chromatic C > 0.5");
    /* Hue angle should be in [0, 360) */
    TEST_ASSERT(fwd.h >= ALWAN_ZERO && fwd.h < ALWAN_LITERAL(360.0),
                "cam18sl hue in [0,360)");

    TEST_PASS("cam18sl chromatic");
}

/* ----------------------------------------------------------------
 * CAM18sl API wrappers
 * ---------------------------------------------------------------- */

static int test_cam18sl_api(void) {
    alwan_xyz xyz_in = { ALWAN_LITERAL(50.0), ALWAN_LITERAL(50.0), ALWAN_LITERAL(50.0) };
    alwan_f64 Y_b = ALWAN_LITERAL(20.0);
    alwan_cam18sl_correlates out;

    alwan_cam18sl_forward(&out, &xyz_in, Y_b);
    TEST_ASSERT(out.Q > ALWAN_ZERO, "cam18sl api Q > 0");

    alwan_xyz xyz_rt;
    alwan_cam18sl_inverse(&xyz_rt, &out, Y_b);
    TEST_ASSERT_NEAR(xyz_rt.x, xyz_in.x, ALWAN_LITERAL(1e-6), "cam18sl api rt X");

    TEST_PASS("cam18sl api");
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

int test_63_cam18sl_main(void) {
    int failures = 0;

    failures += test_cam18sl_roundtrip();
    failures += test_cam18sl_achromatic();
    failures += test_cam18sl_chromatic();
    failures += test_cam18sl_api();

    if (failures == 0) {
        printf("\n=== All CAM18sl tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
