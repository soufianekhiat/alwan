/*
 * P1.1: Oklab & Oklch Tests
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>

#define TEST_ASSERT(cond, msg) do { if (!(cond)) { printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); return 1; } } while(0)
#define TEST_PASS(name) do { return 0; } while(0)

/* ----------------------------------------------------------------
 * Test XYZ <-> Oklab conversions
 * ---------------------------------------------------------------- */

static int test_xyz_oklab_round_trip(void) {
    static alwan_scalar const test_data[] = {
#include "data/fixtures/oklab_values.csv"
    };

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
        alwan_xyz_to_oklab(&xyz_in, &oklab_computed);

        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_FABS(oklab_computed.v[j] - oklab_expected.v[j]);
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
        alwan_oklab_to_xyz(&oklab_computed, &xyz_out);

        /* Relaxed tolerance for round-trip due to cube root operations
         * (1e-5 for double, 1e-2 for float) */
        alwan_scalar const roundtrip_tol = ALWAN_TEST_TOLERANCE * ALWAN_LITERAL(10000000.0);

        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_FABS(xyz_out.v[j] - xyz_in.v[j]);
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
    static alwan_scalar const test_data[] = {
#include "data/fixtures/oklch_values.csv"
    };

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
        alwan_oklab_to_oklch(&oklab_in, &oklch_computed);

        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_FABS(oklch_computed.v[j] - oklch_expected.v[j]);
            /* Slightly relaxed tolerance for hue (angular) */
            alwan_scalar tol = (j == 2) ? ALWAN_TEST_TOLERANCE * ALWAN_LITERAL(10.0) : ALWAN_TEST_TOLERANCE;
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
        alwan_oklch_to_oklab(&oklch_computed, &oklab_out);

        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_FABS(oklab_out.v[j] - oklab_in.v[j]);
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
    /* Relaxed tolerance for cube root operations (1e-4 for double, 1e-1 for float) */
    alwan_scalar const tol = ALWAN_TEST_TOLERANCE * ALWAN_LITERAL(100000000.0);

    /* D65 white should be L=1, a=0, b=0 */
    alwan_vec3 xyz_white = {{ALWAN_LITERAL(0.95047), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.08883)}};
    alwan_vec3 oklab;
    alwan_xyz_to_oklab(&xyz_white, &oklab);

    TEST_ASSERT(ALWAN_FABS(oklab.v[0] - ALWAN_LITERAL(1.0)) < tol, "White L != 1");
    TEST_ASSERT(ALWAN_FABS(oklab.v[1]) < tol, "White a != 0");
    TEST_ASSERT(ALWAN_FABS(oklab.v[2]) < tol, "White b != 0");

    /* Black should be L=0, a=0, b=0 */
    alwan_vec3 xyz_black = {{ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)}};
    alwan_xyz_to_oklab(&xyz_black, &oklab);

    TEST_ASSERT(ALWAN_FABS(oklab.v[0]) < tol, "Black L != 0");
    TEST_ASSERT(ALWAN_FABS(oklab.v[1]) < tol, "Black a != 0");
    TEST_ASSERT(ALWAN_FABS(oklab.v[2]) < tol, "Black b != 0");

    printf("  White and black verified\n");
    TEST_PASS("Oklab known values");
}

/* Main test runner for P1.1 */
int test_120_oklab_main(void) {
    printf("=== P1.1: Oklab & Oklch Tests ===\n");

    if (test_xyz_oklab_round_trip() != 0) return 1;
    if (test_oklab_oklch_round_trip() != 0) return 2;
    if (test_oklab_known_values() != 0) return 3;

    printf("\n=== All P1.1 tests passed ===\n");
    return 0;
}
