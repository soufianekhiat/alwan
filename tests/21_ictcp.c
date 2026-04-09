/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * ICtCp (ITU-R BT.2100 HDR) Tests
 */

#include "test_common.h"

/* ----------------------------------------------------------------
 * Test RGB (BT.2020) <-> ICtCp with PQ
 * ---------------------------------------------------------------- */

static int test_rgb_to_ictcp_pq(void) {
    /* Load test RGB colors and expected ICtCp values */
    static alwan_f64 const test_rgb[] = {
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),      /* Black */
        ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18),  /* 18% gray */
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0),      /* SDR white */
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),      /* SDR red */
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0),      /* SDR green */
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),      /* SDR blue */
        ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0),      /* HDR white */
        ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0)       /* HDR bright */
    };

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const expected_ictcp[] = {
#include "reference_values/ictcp_pq_from_rgb.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_rgb) / sizeof(test_rgb[0]) / 3;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3_f64 rgb, ictcp_computed;

        /* Load input RGB */
        rgb.v[0] = test_rgb[i * 3 + 0];
        rgb.v[1] = test_rgb[i * 3 + 1];
        rgb.v[2] = test_rgb[i * 3 + 2];

        /* Convert RGB -> ICtCp (PQ) */
        alwan_rgb_f64 rgb_typed;
        alwan_ictcp_f64 ictcp_typed;
        ALWAN_MEMCPY(&rgb_typed, &rgb, sizeof(alwan_vec3_f64));
        alwan_rgb_to_ictcp_f64(&ictcp_typed, &rgb_typed, 1);
        ALWAN_MEMCPY(&ictcp_computed, &ictcp_typed, sizeof(alwan_vec3_f64));

        /* Check against expected values */
        for (int j = 0; j < 3; j++) {
            alwan_f64 expected = expected_ictcp[i * 3 + j];
            alwan_f64 diff = ALWAN_ABS(ictcp_computed.v[j] - expected);

            if (diff >= ALWAN_TEST_TOLERANCE) {
                printf("  Color %zu channel %d failed:\n", i, j);
                printf("    RGB: [%.6f, %.6f, %.6f]\n",
                       (double)rgb.v[0], (double)rgb.v[1], (double)rgb.v[2]);
                printf("    Expected ICtCp: [%.6f, %.6f, %.6f]\n",
                       (double)expected_ictcp[i * 3 + 0],
                       (double)expected_ictcp[i * 3 + 1],
                       (double)expected_ictcp[i * 3 + 2]);
                printf("    Got ICtCp: [%.6f, %.6f, %.6f]\n",
                       (double)ictcp_computed.v[0],
                       (double)ictcp_computed.v[1],
                       (double)ictcp_computed.v[2]);
                printf("    Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "ICtCp PQ values don't match");
            }
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("RGB -> ICtCp (PQ)");
}

static int test_ictcp_pq_to_rgb_roundtrip(void) {
    /* Load test RGB colors and reconstructed RGB */
    static alwan_f64 const test_rgb[] = {
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0),
        ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0)
    };

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const expected_rgb_reconstructed[] = {
#include "reference_values/rgb_from_ictcp_pq.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_rgb) / sizeof(test_rgb[0]) / 3;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3_f64 rgb_in, ictcp, rgb_out;

        /* Load input RGB */
        rgb_in.v[0] = test_rgb[i * 3 + 0];
        rgb_in.v[1] = test_rgb[i * 3 + 1];
        rgb_in.v[2] = test_rgb[i * 3 + 2];

        /* Forward: RGB -> ICtCp */
        alwan_rgb_f64 rgb_in_typed;
        alwan_ictcp_f64 ictcp_typed;
        ALWAN_MEMCPY(&rgb_in_typed, &rgb_in, sizeof(alwan_vec3_f64));
        alwan_rgb_to_ictcp_f64(&ictcp_typed, &rgb_in_typed, 1);
        ALWAN_MEMCPY(&ictcp, &ictcp_typed, sizeof(alwan_vec3_f64));

        /* Inverse: ICtCp -> RGB */
        alwan_rgb_f64 rgb_out_typed;
        ALWAN_MEMCPY(&ictcp_typed, &ictcp, sizeof(alwan_vec3_f64));
        alwan_ictcp_to_rgb_f64(&rgb_out_typed, &ictcp_typed, 1);
        ALWAN_MEMCPY(&rgb_out, &rgb_out_typed, sizeof(alwan_vec3_f64));

        /* Check round-trip against colour-science reconstructed values */
        for (int j = 0; j < 3; j++) {
            alwan_f64 expected = expected_rgb_reconstructed[i * 3 + j];
            alwan_f64 diff = ALWAN_ABS(rgb_out.v[j] - expected);

            if (diff >= ALWAN_TEST_TOLERANCE) {
                printf("  Color %zu channel %d failed:\n", i, j);
                printf("    Original RGB: [%.6f, %.6f, %.6f]\n",
                       (double)rgb_in.v[0], (double)rgb_in.v[1], (double)rgb_in.v[2]);
                printf("    Expected RGB: [%.6f, %.6f, %.6f]\n",
                       (double)expected_rgb_reconstructed[i * 3 + 0],
                       (double)expected_rgb_reconstructed[i * 3 + 1],
                       (double)expected_rgb_reconstructed[i * 3 + 2]);
                printf("    Got RGB: [%.6f, %.6f, %.6f]\n",
                       (double)rgb_out.v[0], (double)rgb_out.v[1], (double)rgb_out.v[2]);
                printf("    Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "ICtCp PQ round-trip failed");
            }
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("ICtCp (PQ) -> RGB round-trip");
}

/* ----------------------------------------------------------------
 * Test RGB (BT.2020) <-> ICtCp with HLG
 * ---------------------------------------------------------------- */

static int test_rgb_to_ictcp_hlg(void) {
    /* Load test RGB colors and expected ICtCp values */
    static alwan_f64 const test_rgb[] = {
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0),
        ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0)
    };

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const expected_ictcp[] = {
#include "reference_values/ictcp_hlg_from_rgb.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_rgb) / sizeof(test_rgb[0]) / 3;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3_f64 rgb, ictcp_computed;

        /* Load input RGB */
        rgb.v[0] = test_rgb[i * 3 + 0];
        rgb.v[1] = test_rgb[i * 3 + 1];
        rgb.v[2] = test_rgb[i * 3 + 2];

        /* Convert RGB -> ICtCp (HLG) */
        alwan_rgb_f64 rgb_typed;
        alwan_ictcp_f64 ictcp_typed;
        ALWAN_MEMCPY(&rgb_typed, &rgb, sizeof(alwan_vec3_f64));
        alwan_rgb_to_ictcp_f64(&ictcp_typed, &rgb_typed, 0);
        ALWAN_MEMCPY(&ictcp_computed, &ictcp_typed, sizeof(alwan_vec3_f64));

        /* Check against expected values */
        for (int j = 0; j < 3; j++) {
            alwan_f64 expected = expected_ictcp[i * 3 + j];
            alwan_f64 diff = ALWAN_ABS(ictcp_computed.v[j] - expected);

            if (diff >= ALWAN_TEST_TOLERANCE) {
                printf("  Color %zu channel %d failed:\n", i, j);
                printf("    RGB: [%.6f, %.6f, %.6f]\n",
                       (double)rgb.v[0], (double)rgb.v[1], (double)rgb.v[2]);
                printf("    Expected ICtCp: [%.6f, %.6f, %.6f]\n",
                       (double)expected_ictcp[i * 3 + 0],
                       (double)expected_ictcp[i * 3 + 1],
                       (double)expected_ictcp[i * 3 + 2]);
                printf("    Got ICtCp: [%.6f, %.6f, %.6f]\n",
                       (double)ictcp_computed.v[0],
                       (double)ictcp_computed.v[1],
                       (double)ictcp_computed.v[2]);
                printf("    Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "ICtCp HLG values don't match");
            }
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("RGB -> ICtCp (HLG)");
}

static int test_ictcp_hlg_to_rgb_roundtrip(void) {
    /* Load test RGB colors and reconstructed RGB */
    static alwan_f64 const test_rgb[] = {
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0),
        ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0)
    };

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const expected_rgb_reconstructed[] = {
#include "reference_values/rgb_from_ictcp_hlg.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_rgb) / sizeof(test_rgb[0]) / 3;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3_f64 rgb_in, ictcp, rgb_out;

        /* Load input RGB */
        rgb_in.v[0] = test_rgb[i * 3 + 0];
        rgb_in.v[1] = test_rgb[i * 3 + 1];
        rgb_in.v[2] = test_rgb[i * 3 + 2];

        /* Forward: RGB -> ICtCp */
        alwan_rgb_f64 rgb_in_typed;
        alwan_ictcp_f64 ictcp_typed;
        ALWAN_MEMCPY(&rgb_in_typed, &rgb_in, sizeof(alwan_vec3_f64));
        alwan_rgb_to_ictcp_f64(&ictcp_typed, &rgb_in_typed, 0);
        ALWAN_MEMCPY(&ictcp, &ictcp_typed, sizeof(alwan_vec3_f64));

        /* Inverse: ICtCp -> RGB */
        alwan_rgb_f64 rgb_out_typed;
        ALWAN_MEMCPY(&ictcp_typed, &ictcp, sizeof(alwan_vec3_f64));
        alwan_ictcp_to_rgb_f64(&rgb_out_typed, &ictcp_typed, 0);
        ALWAN_MEMCPY(&rgb_out, &rgb_out_typed, sizeof(alwan_vec3_f64));

        /* Check round-trip against colour-science reconstructed values */
        for (int j = 0; j < 3; j++) {
            alwan_f64 expected = expected_rgb_reconstructed[i * 3 + j];
            alwan_f64 diff = ALWAN_ABS(rgb_out.v[j] - expected);

            if (diff >= ALWAN_TEST_TOLERANCE) {
                printf("  Color %zu channel %d failed:\n", i, j);
                printf("    Original RGB: [%.6f, %.6f, %.6f]\n",
                       (double)rgb_in.v[0], (double)rgb_in.v[1], (double)rgb_in.v[2]);
                printf("    Expected RGB: [%.6f, %.6f, %.6f]\n",
                       (double)expected_rgb_reconstructed[i * 3 + 0],
                       (double)expected_rgb_reconstructed[i * 3 + 1],
                       (double)expected_rgb_reconstructed[i * 3 + 2]);
                printf("    Got RGB: [%.6f, %.6f, %.6f]\n",
                       (double)rgb_out.v[0], (double)rgb_out.v[1], (double)rgb_out.v[2]);
                printf("    Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "ICtCp HLG round-trip failed");
            }
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("ICtCp (HLG) -> RGB round-trip");
}

/* ----------------------------------------------------------------
 * Test XYZ (D65) <-> ICtCp
 * ---------------------------------------------------------------- */

static int test_xyz_to_ictcp_pq(void) {
    /* Load standard test XYZ colors */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const test_xyz[] = {
#include "reference_values/test_xyz_colors.csv"
    };
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const expected_ictcp[] = {
#include "reference_values/ictcp_pq_from_xyz.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_xyz) / sizeof(test_xyz[0]) / 3;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3_f64 xyz, ictcp_computed;

        /* Load input XYZ */
        xyz.v[0] = test_xyz[i * 3 + 0];
        xyz.v[1] = test_xyz[i * 3 + 1];
        xyz.v[2] = test_xyz[i * 3 + 2];

        /* Convert XYZ -> ICtCp (PQ) */
        alwan_xyz_f64 xyz_typed;
        alwan_ictcp_f64 ictcp_typed;
        ALWAN_MEMCPY(&xyz_typed, &xyz, sizeof(alwan_vec3_f64));
        alwan_xyz_to_ictcp_f64(&ictcp_typed, &xyz_typed, 1);
        ALWAN_MEMCPY(&ictcp_computed, &ictcp_typed, sizeof(alwan_vec3_f64));

        /* Check against expected values */
        for (int j = 0; j < 3; j++) {
            alwan_f64 expected = expected_ictcp[i * 3 + j];
            alwan_f64 diff = ALWAN_ABS(ictcp_computed.v[j] - expected);

            if (diff >= ALWAN_TEST_TOLERANCE) {
                printf("  Color %zu channel %d failed (diff: %.6e)\n", i, j, (double)diff);
                TEST_ASSERT(0, "XYZ -> ICtCp (PQ) failed");
            }
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("XYZ -> ICtCp (PQ)");
}

static int test_xyz_to_ictcp_hlg(void) {
    /* Load standard test XYZ colors */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const test_xyz[] = {
#include "reference_values/test_xyz_colors.csv"
    };
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const expected_ictcp[] = {
#include "reference_values/ictcp_hlg_from_xyz.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_xyz) / sizeof(test_xyz[0]) / 3;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3_f64 xyz, ictcp_computed;

        /* Load input XYZ */
        xyz.v[0] = test_xyz[i * 3 + 0];
        xyz.v[1] = test_xyz[i * 3 + 1];
        xyz.v[2] = test_xyz[i * 3 + 2];

        /* Convert XYZ -> ICtCp (HLG) */
        alwan_xyz_f64 xyz_typed;
        alwan_ictcp_f64 ictcp_typed;
        ALWAN_MEMCPY(&xyz_typed, &xyz, sizeof(alwan_vec3_f64));
        alwan_xyz_to_ictcp_f64(&ictcp_typed, &xyz_typed, 0);
        ALWAN_MEMCPY(&ictcp_computed, &ictcp_typed, sizeof(alwan_vec3_f64));

        /* Check against expected values */
        for (int j = 0; j < 3; j++) {
            alwan_f64 expected = expected_ictcp[i * 3 + j];
            alwan_f64 diff = ALWAN_ABS(ictcp_computed.v[j] - expected);

            if (diff >= ALWAN_TEST_TOLERANCE) {
                printf("  Color %zu channel %d failed (diff: %.6e)\n", i, j, (double)diff);
                TEST_ASSERT(0, "XYZ -> ICtCp (HLG) failed");
            }
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("XYZ -> ICtCp (HLG)");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_21_ictcp_main(void) {
    printf("=== ICtCp (BT.2100 HDR) Tests ===\n");

    int failures = 0;

    failures += test_rgb_to_ictcp_pq();
    failures += test_ictcp_pq_to_rgb_roundtrip();
    failures += test_rgb_to_ictcp_hlg();
    failures += test_ictcp_hlg_to_rgb_roundtrip();
    failures += test_xyz_to_ictcp_pq();
    failures += test_xyz_to_ictcp_hlg();

    if (failures == 0) {
        printf("\n=== All P1.2 tests passed ===\n");
    } else {
        printf("\n=== %d P1.2 test(s) failed ===\n", failures);
    }

    return failures;
}
