/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * ICtCp (ITU-R BT.2100 HDR) Tests
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>

/* Test helpers */
#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); return 1; } } while(0)

#define TEST_PASS(name) \
    do { printf("  [PASS] %s\n", name); return 0; } while(0)

/* Tolerance for ICtCp tests
 * Per P1.2 acceptance: precision <= 1e-6
 * PQ and HLG involve complex transfer functions, so we use relaxed tolerance */
#if ALWAN_SCALAR_IS_FLOAT
    #define ICTCP_TOL ALWAN_LITERAL(1e-5)      /* Float: relaxed due to precision */
    #define ROUNDTRIP_TOL ALWAN_LITERAL(1e-4)  /* Round-trip: more relaxed for float */
#else
    #define ICTCP_TOL ALWAN_LITERAL(1e-6)      /* Double: meets P1.2 spec */
    #define ROUNDTRIP_TOL ALWAN_LITERAL(1e-5)  /* Round-trip: slightly relaxed */
#endif

/* ----------------------------------------------------------------
 * Test RGB (BT.2020) <-> ICtCp with PQ
 * ---------------------------------------------------------------- */

static int test_rgb_to_ictcp_pq(void) {
    /* Load test RGB colors and expected ICtCp values */
    static alwan_scalar const test_rgb[] = {
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),      /* Black */
        ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18),  /* 18% gray */
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0),      /* SDR white */
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),      /* SDR red */
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0),      /* SDR green */
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),      /* SDR blue */
        ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0),      /* HDR white */
        ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0)       /* HDR bright */
    };

    static alwan_scalar const expected_ictcp[] = {
#include "reference_values/ictcp_pq_from_rgb.csv"
    };

    size_t const num_colors = sizeof(test_rgb) / sizeof(test_rgb[0]) / 3;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 rgb, ictcp_computed;

        /* Load input RGB */
        rgb.v[0] = test_rgb[i * 3 + 0];
        rgb.v[1] = test_rgb[i * 3 + 1];
        rgb.v[2] = test_rgb[i * 3 + 2];

        /* Convert RGB -> ICtCp (PQ) */
        alwan_rgb_to_ictcp(&rgb, &ictcp_computed, 1);

        /* Check against expected values */
        for (int j = 0; j < 3; j++) {
            alwan_scalar expected = expected_ictcp[i * 3 + j];
            alwan_scalar diff = ALWAN_FABS(ictcp_computed.v[j] - expected);

            if (diff >= ICTCP_TOL) {
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
    static alwan_scalar const test_rgb[] = {
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0),
        ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0)
    };

    static alwan_scalar const expected_rgb_reconstructed[] = {
#include "reference_values/rgb_from_ictcp_pq.csv"
    };

    size_t const num_colors = sizeof(test_rgb) / sizeof(test_rgb[0]) / 3;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 rgb_in, ictcp, rgb_out;

        /* Load input RGB */
        rgb_in.v[0] = test_rgb[i * 3 + 0];
        rgb_in.v[1] = test_rgb[i * 3 + 1];
        rgb_in.v[2] = test_rgb[i * 3 + 2];

        /* Forward: RGB -> ICtCp */
        alwan_rgb_to_ictcp(&rgb_in, &ictcp, 1);

        /* Inverse: ICtCp -> RGB */
        alwan_ictcp_to_rgb(&ictcp, &rgb_out, 1);

        /* Check round-trip against colour-science reconstructed values */
        for (int j = 0; j < 3; j++) {
            alwan_scalar expected = expected_rgb_reconstructed[i * 3 + j];
            alwan_scalar diff = ALWAN_FABS(rgb_out.v[j] - expected);

            if (diff >= ROUNDTRIP_TOL) {
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
    static alwan_scalar const test_rgb[] = {
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0),
        ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0)
    };

    static alwan_scalar const expected_ictcp[] = {
#include "reference_values/ictcp_hlg_from_rgb.csv"
    };

    size_t const num_colors = sizeof(test_rgb) / sizeof(test_rgb[0]) / 3;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 rgb, ictcp_computed;

        /* Load input RGB */
        rgb.v[0] = test_rgb[i * 3 + 0];
        rgb.v[1] = test_rgb[i * 3 + 1];
        rgb.v[2] = test_rgb[i * 3 + 2];

        /* Convert RGB -> ICtCp (HLG) */
        alwan_rgb_to_ictcp(&rgb, &ictcp_computed, 0);

        /* Check against expected values */
        for (int j = 0; j < 3; j++) {
            alwan_scalar expected = expected_ictcp[i * 3 + j];
            alwan_scalar diff = ALWAN_FABS(ictcp_computed.v[j] - expected);

            if (diff >= ICTCP_TOL) {
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
    static alwan_scalar const test_rgb[] = {
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0), ALWAN_LITERAL(2.0),
        ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0), ALWAN_LITERAL(5.0)
    };

    static alwan_scalar const expected_rgb_reconstructed[] = {
#include "reference_values/rgb_from_ictcp_hlg.csv"
    };

    size_t const num_colors = sizeof(test_rgb) / sizeof(test_rgb[0]) / 3;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 rgb_in, ictcp, rgb_out;

        /* Load input RGB */
        rgb_in.v[0] = test_rgb[i * 3 + 0];
        rgb_in.v[1] = test_rgb[i * 3 + 1];
        rgb_in.v[2] = test_rgb[i * 3 + 2];

        /* Forward: RGB -> ICtCp */
        alwan_rgb_to_ictcp(&rgb_in, &ictcp, 0);

        /* Inverse: ICtCp -> RGB */
        alwan_ictcp_to_rgb(&ictcp, &rgb_out, 0);

        /* Check round-trip against colour-science reconstructed values */
        for (int j = 0; j < 3; j++) {
            alwan_scalar expected = expected_rgb_reconstructed[i * 3 + j];
            alwan_scalar diff = ALWAN_FABS(rgb_out.v[j] - expected);

            if (diff >= ROUNDTRIP_TOL) {
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
    static alwan_scalar const test_xyz[] = {
#include "reference_values/test_xyz_colors.csv"
    };

    static alwan_scalar const expected_ictcp[] = {
#include "reference_values/ictcp_pq_from_xyz.csv"
    };

    size_t const num_colors = sizeof(test_xyz) / sizeof(test_xyz[0]) / 3;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 xyz, ictcp_computed;

        /* Load input XYZ */
        xyz.v[0] = test_xyz[i * 3 + 0];
        xyz.v[1] = test_xyz[i * 3 + 1];
        xyz.v[2] = test_xyz[i * 3 + 2];

        /* Convert XYZ -> ICtCp (PQ) */
        alwan_xyz_to_ictcp(&xyz, &ictcp_computed, 1);

        /* Check against expected values */
        for (int j = 0; j < 3; j++) {
            alwan_scalar expected = expected_ictcp[i * 3 + j];
            alwan_scalar diff = ALWAN_FABS(ictcp_computed.v[j] - expected);

            if (diff >= ICTCP_TOL) {
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
    static alwan_scalar const test_xyz[] = {
#include "reference_values/test_xyz_colors.csv"
    };

    static alwan_scalar const expected_ictcp[] = {
#include "reference_values/ictcp_hlg_from_xyz.csv"
    };

    size_t const num_colors = sizeof(test_xyz) / sizeof(test_xyz[0]) / 3;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 xyz, ictcp_computed;

        /* Load input XYZ */
        xyz.v[0] = test_xyz[i * 3 + 0];
        xyz.v[1] = test_xyz[i * 3 + 1];
        xyz.v[2] = test_xyz[i * 3 + 2];

        /* Convert XYZ -> ICtCp (HLG) */
        alwan_xyz_to_ictcp(&xyz, &ictcp_computed, 0);

        /* Check against expected values */
        for (int j = 0; j < 3; j++) {
            alwan_scalar expected = expected_ictcp[i * 3 + j];
            alwan_scalar diff = ALWAN_FABS(ictcp_computed.v[j] - expected);

            if (diff >= ICTCP_TOL) {
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
