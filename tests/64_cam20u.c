/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 64: CAM20u - Color Appearance Model for Unrelated Color
 */

#include "test_common.h"
#include <stdlib.h>

#include "core/alwan_cam20u_core.h"

/* ----------------------------------------------------------------
 * CAM20u forward/inverse roundtrip (core)
 * ---------------------------------------------------------------- */

static int test_cam20u_roundtrip(void) {
    alwan_xyz_f64 xyz_in = { ALWAN_LITERAL(50.0), ALWAN_LITERAL(50.0), ALWAN_LITERAL(50.0) };
    alwan_f64 Y_b = ALWAN_LITERAL(20.0);
    alwan_f64 L_a = ALWAN_LITERAL(64.0);

    alwan_cam20u_v_correlates_f64 fwd = alwan_cam20u_forward_f64_v(xyz_in, Y_b, L_a);

    /* Sanity: brightness should be positive */
    TEST_ASSERT(fwd.Q > ALWAN_ZERO, "cam20u Q > 0");

    alwan_xyz_f64 xyz_rt = alwan_cam20u_inverse_f64_v(fwd, Y_b, L_a);

    TEST_ASSERT_NEAR(xyz_rt.x, xyz_in.x, ALWAN_LITERAL(5e-1), "cam20u rt X");
    TEST_ASSERT_NEAR(xyz_rt.y, xyz_in.y, ALWAN_LITERAL(5e-1), "cam20u rt Y");
    TEST_ASSERT_NEAR(xyz_rt.z, xyz_in.z, ALWAN_LITERAL(5e-1), "cam20u rt Z");

    TEST_PASS("cam20u roundtrip");
}

/* ----------------------------------------------------------------
 * CAM20u achromatic input (neutral)
 * ---------------------------------------------------------------- */

static int test_cam20u_achromatic(void) {
    alwan_xyz_f64 xyz_in = { ALWAN_LITERAL(95.047), ALWAN_LITERAL(100.0), ALWAN_LITERAL(108.883) };
    alwan_f64 Y_b = ALWAN_LITERAL(20.0);
    alwan_f64 L_a = ALWAN_LITERAL(64.0);

    alwan_cam20u_v_correlates_f64 fwd = alwan_cam20u_forward_f64_v(xyz_in, Y_b, L_a);

    /* D65 white should have low colorfulness relative to achromatic */
    TEST_ASSERT(fwd.Q > ALWAN_ZERO, "cam20u achromatic Q > 0");

    TEST_PASS("cam20u achromatic");
}

/* ----------------------------------------------------------------
 * CAM20u chromatic input
 * ---------------------------------------------------------------- */

static int test_cam20u_chromatic(void) {
    /* A saturated red */
    alwan_xyz_f64 xyz_in = { ALWAN_LITERAL(40.0), ALWAN_LITERAL(20.0), ALWAN_LITERAL(5.0) };
    alwan_f64 Y_b = ALWAN_LITERAL(20.0);
    alwan_f64 L_a = ALWAN_LITERAL(64.0);

    alwan_cam20u_v_correlates_f64 fwd = alwan_cam20u_forward_f64_v(xyz_in, Y_b, L_a);

    /* Should have significant colorfulness */
    TEST_ASSERT(fwd.M > ALWAN_LITERAL(0.1), "cam20u chromatic M > 0.1");
    /* Hue angle should be in [0, 360) */
    TEST_ASSERT(fwd.h >= ALWAN_ZERO && fwd.h < ALWAN_LITERAL(360.0),
                "cam20u hue in [0,360)");

    TEST_PASS("cam20u chromatic");
}

/* ----------------------------------------------------------------
 * CAM20u API wrappers
 * ---------------------------------------------------------------- */

static int test_cam20u_api(void) {
    alwan_xyz_f64 xyz_in = { ALWAN_LITERAL(50.0), ALWAN_LITERAL(50.0), ALWAN_LITERAL(50.0) };
    alwan_f64 Y_b = ALWAN_LITERAL(20.0);
    alwan_f64 L_a = ALWAN_LITERAL(64.0);
    alwan_cam20u_correlates out;

    alwan_cam20u_forward_f64(&out, &xyz_in, Y_b, L_a);
    TEST_ASSERT(out.Q > ALWAN_ZERO, "cam20u api Q > 0");

    alwan_xyz_f64 xyz_rt;
    alwan_cam20u_inverse_f64(&xyz_rt, &out, Y_b, L_a);

    TEST_PASS("cam20u api");
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

int test_64_cam20u_main(void) {
    int failures = 0;

    failures += test_cam20u_roundtrip();
    failures += test_cam20u_achromatic();
    failures += test_cam20u_chromatic();
    failures += test_cam20u_api();

    if (failures == 0) {
        printf("\n=== All CAM20u tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
