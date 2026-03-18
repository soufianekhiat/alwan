/*
 * Oklab & Oklch Tests
 */

#include "test_common.h"

/* ----------------------------------------------------------------
 * Test XYZ <-> Oklab conversions
 * ---------------------------------------------------------------- */

static int test_xyz_oklab_round_trip(void) {
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const test_data[] = {
#include "reference_values/test_xyz_oklab_pairs.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 xyz_in, oklab_expected, oklab_computed, xyz_out;

        /* Load test data */
        xyz_in.v[0] = test_data[i * 6 + 0];
        xyz_in.v[1] = test_data[i * 6 + 1];
        xyz_in.v[2] = test_data[i * 6 + 2];
        oklab_expected.v[0] = test_data[i * 6 + 3];
        oklab_expected.v[1] = test_data[i * 6 + 4];
        oklab_expected.v[2] = test_data[i * 6 + 5];

        /* Test XYZ -> Oklab */
        {
            alwan_xyz xyz_typed;
            alwan_oklab oklab_typed;
            ALWAN_MEMCPY(&xyz_typed, &xyz_in, sizeof(alwan_vec3));
            alwan_xyz_to_oklab(&oklab_typed, &xyz_typed);
            ALWAN_MEMCPY(&oklab_computed, &oklab_typed, sizeof(alwan_vec3));
        }

        for (int j = 0; j < 3; j++) {
            alwan_f64 diff = ALWAN_ABS(oklab_computed.v[j] - oklab_expected.v[j]);
            if (diff > ALWAN_TEST_TOLERANCE) {
                printf("Color %zu channel %d failed:\n", i, j);
                printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_in.v[0], (double)xyz_in.v[1], (double)xyz_in.v[2]);
                printf("  Expected Oklab: [%.6f, %.6f, %.6f]\n",
                       (double)oklab_expected.v[0], (double)oklab_expected.v[1], (double)oklab_expected.v[2]);
                printf("  Got Oklab: [%.6f, %.6f, %.6f]\n",
                       (double)oklab_computed.v[0], (double)oklab_computed.v[1], (double)oklab_computed.v[2]);
                printf("  Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "Oklab values don't match");
            }
        }

        /* Test round-trip: Oklab -> XYZ */
        {
            alwan_oklab oklab_typed;
            alwan_xyz xyz_typed;
            ALWAN_MEMCPY(&oklab_typed, &oklab_computed, sizeof(alwan_vec3));
            alwan_oklab_to_xyz(&xyz_typed, &oklab_typed);
            ALWAN_MEMCPY(&xyz_out, &xyz_typed, sizeof(alwan_vec3));
        }

        alwan_f64 const roundtrip_tol = ALWAN_TEST_TOLERANCE;

        for (int j = 0; j < 3; j++) {
            alwan_f64 diff = ALWAN_ABS(xyz_out.v[j] - xyz_in.v[j]);
            if (diff > roundtrip_tol) {
                printf("Round-trip color %zu channel %d failed:\n", i, j);
                printf("  Original XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_in.v[0], (double)xyz_in.v[1], (double)xyz_in.v[2]);
                printf("  Round-trip XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_out.v[0], (double)xyz_out.v[1], (double)xyz_out.v[2]);
                printf("  Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "XYZ round-trip failed");
            }
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("XYZ <-> Oklab round-trip");
}

/* ----------------------------------------------------------------
 * Test Oklab <-> Oklch conversions
 * ---------------------------------------------------------------- */

static int test_oklab_oklch_round_trip(void) {
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const test_data[] = {
#include "reference_values/test_oklab_oklch_pairs.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 oklab_in, oklch_expected, oklch_computed, oklab_out;

        /* Load test data */
        oklab_in.v[0] = test_data[i * 6 + 0];
        oklab_in.v[1] = test_data[i * 6 + 1];
        oklab_in.v[2] = test_data[i * 6 + 2];
        oklch_expected.v[0] = test_data[i * 6 + 3];
        oklch_expected.v[1] = test_data[i * 6 + 4];
        oklch_expected.v[2] = test_data[i * 6 + 5];

        /* Test Oklab -> Oklch */
        {
            alwan_oklab oklab_typed;
            alwan_oklch oklch_typed;
            ALWAN_MEMCPY(&oklab_typed, &oklab_in, sizeof(alwan_vec3));
            alwan_oklab_to_oklch(&oklch_typed, &oklab_typed);
            ALWAN_MEMCPY(&oklch_computed, &oklch_typed, sizeof(alwan_vec3));
        }

        for (int j = 0; j < 3; j++) {
            alwan_f64 diff = ALWAN_ABS(oklch_computed.v[j] - oklch_expected.v[j]);
            alwan_f64 tol = ALWAN_TEST_TOLERANCE;
            if (diff > tol) {
                printf("Color %zu channel %d failed:\n", i, j);
                printf("  Oklab: [%.6f, %.6f, %.6f]\n",
                       (double)oklab_in.v[0], (double)oklab_in.v[1], (double)oklab_in.v[2]);
                printf("  Expected Oklch: [%.6f, %.6f, %.6f]\n",
                       (double)oklch_expected.v[0], (double)oklch_expected.v[1], (double)oklch_expected.v[2]);
                printf("  Got Oklch: [%.6f, %.6f, %.6f]\n",
                       (double)oklch_computed.v[0], (double)oklch_computed.v[1], (double)oklch_computed.v[2]);
                printf("  Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "Oklch values don't match");
            }
        }

        /* Test round-trip: Oklch -> Oklab */
        {
            alwan_oklch oklch_typed;
            alwan_oklab oklab_typed;
            ALWAN_MEMCPY(&oklch_typed, &oklch_computed, sizeof(alwan_vec3));
            alwan_oklch_to_oklab(&oklab_typed, &oklch_typed);
            ALWAN_MEMCPY(&oklab_out, &oklab_typed, sizeof(alwan_vec3));
        }

        for (int j = 0; j < 3; j++) {
            alwan_f64 diff = ALWAN_ABS(oklab_out.v[j] - oklab_in.v[j]);
            if (diff > ALWAN_TEST_TOLERANCE) {
                printf("Round-trip color %zu channel %d failed:\n", i, j);
                printf("  Original Oklab: [%.6f, %.6f, %.6f]\n",
                       (double)oklab_in.v[0], (double)oklab_in.v[1], (double)oklab_in.v[2]);
                printf("  Round-trip Oklab: [%.6f, %.6f, %.6f]\n",
                       (double)oklab_out.v[0], (double)oklab_out.v[1], (double)oklab_out.v[2]);
                printf("  Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "Oklab round-trip failed");
            }
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("Oklab <-> Oklch round-trip");
}

/* ----------------------------------------------------------------
 * Test specific known values
 * ---------------------------------------------------------------- */

static int test_oklab_known_values(void) {
    alwan_f64 const tol = ALWAN_TEST_TOLERANCE;

    /* D65 white in Oklab: reference values computed from the Oklab M1/M2 matrices.
     * Note: Oklab matrices are designed for sRGB (1,1,1) -> L=1; an arbitrary D65
     * XYZ approximation won't give exactly (1, 0, 0) due to matrix coefficient precision. */
    alwan_vec3 xyz_white = {{ALWAN_LITERAL(0.95047), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.08883)}};
    alwan_vec3 oklab;
    {
        alwan_xyz xyz_typed;
        alwan_oklab oklab_typed;
        ALWAN_MEMCPY(&xyz_typed, &xyz_white, sizeof(alwan_vec3));
        alwan_xyz_to_oklab(&oklab_typed, &xyz_typed);
        ALWAN_MEMCPY(&oklab, &oklab_typed, sizeof(alwan_vec3));
    }

    TEST_ASSERT(ALWAN_ABS(oklab.v[0] - ALWAN_LITERAL(9.99999809520289439924e-01)) < tol, "White L mismatch");
    TEST_ASSERT(ALWAN_ABS(oklab.v[1] - ALWAN_LITERAL(-1.00917549680779039534e-05)) < tol, "White a mismatch");
    TEST_ASSERT(ALWAN_ABS(oklab.v[2] - ALWAN_LITERAL(-8.61087800451548757152e-05)) < tol, "White b mismatch");

    /* Black should be L=0, a=0, b=0 */
    alwan_vec3 xyz_black = {{ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)}};
    {
        alwan_xyz xyz_typed;
        alwan_oklab oklab_typed;
        ALWAN_MEMCPY(&xyz_typed, &xyz_black, sizeof(alwan_vec3));
        alwan_xyz_to_oklab(&oklab_typed, &xyz_typed);
        ALWAN_MEMCPY(&oklab, &oklab_typed, sizeof(alwan_vec3));
    }

    TEST_ASSERT(ALWAN_ABS(oklab.v[0]) < tol, "Black L != 0");
    TEST_ASSERT(ALWAN_ABS(oklab.v[1]) < tol, "Black a != 0");
    TEST_ASSERT(ALWAN_ABS(oklab.v[2]) < tol, "Black b != 0");

    printf("  White and black verified\n");
    TEST_PASS("Oklab known values");
}

/* Main test runner for P1.1 */
int test_20_oklab_main(void) {
    printf("=== Oklab & Oklch Tests ===\n");

    if (test_xyz_oklab_round_trip() != 0) return 1;
    if (test_oklab_oklch_round_trip() != 0) return 2;
    if (test_oklab_known_values() != 0) return 3;

    printf("\n=== All P1.1 tests passed ===\n");
    return 0;
}
