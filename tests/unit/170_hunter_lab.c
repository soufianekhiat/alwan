/*
 * P1.6: Hunter Lab Tests
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>

#define TEST_ASSERT(cond, msg) do { if (!(cond)) { printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); return 1; } } while(0)
#define TEST_PASS(name) do { return 0; } while(0)

/* ----------------------------------------------------------------
 * Test XYZ <-> Hunter Lab conversions
 * ---------------------------------------------------------------- */

static int test_xyz_hunter_lab_round_trip(void) {
    static alwan_scalar const test_data[] = {
#include "reference_values/test_xyz_hunter_lab_pairs.csv"
    };

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 xyz_in, hunter_expected, hunter_computed, xyz_out;

        /* Load test data */
        xyz_in.v[0] = test_data[i * 6 + 0];
        xyz_in.v[1] = test_data[i * 6 + 1];
        xyz_in.v[2] = test_data[i * 6 + 2];
        hunter_expected.v[0] = test_data[i * 6 + 3];
        hunter_expected.v[1] = test_data[i * 6 + 4];
        hunter_expected.v[2] = test_data[i * 6 + 5];

        /* Skip black (0,0,0) as it has division by zero issues */
        if (xyz_in.v[0] < ALWAN_LITERAL(0.01) &&
            xyz_in.v[1] < ALWAN_LITERAL(0.01) &&
            xyz_in.v[2] < ALWAN_LITERAL(0.01)) {
            continue;
        }

        /* Skip black (0,0,0) due to numerical issues with hue */
        if (xyz_in.v[0] < ALWAN_LITERAL(0.01) && xyz_in.v[1] < ALWAN_LITERAL(0.01) && xyz_in.v[2] < ALWAN_LITERAL(0.01)) {
            continue;
        }

        /* Test XYZ -> Hunter Lab */
        alwan_xyz_to_hunter_lab(&xyz_in, &hunter_computed);

        alwan_scalar const hunter_tol = ALWAN_TEST_TOLERANCE * ALWAN_LITERAL(100000.0);
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_FABS(hunter_computed.v[j] - hunter_expected.v[j]);
            if (diff > hunter_tol) {
                printf("Color %zu channel %d failed:\n", i, j);
                printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_in.v[0], (double)xyz_in.v[1], (double)xyz_in.v[2]);
                printf("  Expected Hunter Lab: [%.10f, %.10f, %.10f]\n",
                       (double)hunter_expected.v[0], (double)hunter_expected.v[1], (double)hunter_expected.v[2]);
                printf("  Got Hunter Lab: [%.10f, %.10f, %.10f]\n",
                       (double)hunter_computed.v[0], (double)hunter_computed.v[1], (double)hunter_computed.v[2]);
                printf("  Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "Hunter Lab values don't match");
            }
        }

        /* Test round-trip: Hunter Lab -> XYZ */
        alwan_hunter_lab_to_xyz(&hunter_computed, &xyz_out);

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
    TEST_PASS("XYZ <-> Hunter Lab round-trip");
}

/* ----------------------------------------------------------------
 * Main test entry point
 * ---------------------------------------------------------------- */

int test_170_hunter_lab_main(void) {
    printf("\n=== P1.6: Hunter Lab Tests ===\n");

    int failures = 0;

    printf("Test: XYZ <-> Hunter Lab round-trip\n");
    failures += test_xyz_hunter_lab_round_trip();

    if (failures == 0) {
        printf("All Hunter Lab tests passed!\n");
    } else {
        printf("%d Hunter Lab test(s) failed!\n", failures);
    }

    return failures;
}
