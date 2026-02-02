/*
 * Hunter Lab Tests
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
        alwan_xyz xyz_in, xyz_out;
        alwan_hunter_lab hunter_expected, hunter_computed;

        /* Load test data */
        xyz_in.x = test_data[i * 6 + 0];
        xyz_in.y = test_data[i * 6 + 1];
        xyz_in.z = test_data[i * 6 + 2];
        hunter_expected.L = test_data[i * 6 + 3];
        hunter_expected.a = test_data[i * 6 + 4];
        hunter_expected.b = test_data[i * 6 + 5];

        /* Skip black (0,0,0) as it has division by zero issues */
        if (xyz_in.x < ALWAN_LITERAL(0.01) &&
            xyz_in.y < ALWAN_LITERAL(0.01) &&
            xyz_in.z < ALWAN_LITERAL(0.01)) {
            continue;
        }

        /* Skip black (0,0,0) due to numerical issues with hue */
        if (xyz_in.x < ALWAN_LITERAL(0.01) && xyz_in.y < ALWAN_LITERAL(0.01) && xyz_in.z < ALWAN_LITERAL(0.01)) {
            continue;
        }

        /* Test XYZ -> Hunter Lab */
        alwan_xyz_to_hunter_lab(&hunter_computed, &xyz_in);

#if ALWAN_SCALAR_IS_FLOAT
        alwan_scalar const hunter_tol = ALWAN_LITERAL(1e-3);
#else
        alwan_scalar const hunter_tol = ALWAN_LITERAL(1e-8);
#endif
        alwan_scalar hunter_comp_arr[3] = {hunter_computed.L, hunter_computed.a, hunter_computed.b};
        alwan_scalar hunter_exp_arr[3] = {hunter_expected.L, hunter_expected.a, hunter_expected.b};
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_ABS(hunter_comp_arr[j] - hunter_exp_arr[j]);
            if (diff > hunter_tol) {
                printf("Color %zu channel %d failed:\n", i, j);
                printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
                printf("  Expected Hunter Lab: [%.10f, %.10f, %.10f]\n",
                       (double)hunter_exp_arr[0], (double)hunter_exp_arr[1], (double)hunter_exp_arr[2]);
                printf("  Got Hunter Lab: [%.10f, %.10f, %.10f]\n",
                       (double)hunter_comp_arr[0], (double)hunter_comp_arr[1], (double)hunter_comp_arr[2]);
                printf("  Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "Hunter Lab values don't match");
            }
        }

        /* Test round-trip: Hunter Lab -> XYZ */
        alwan_hunter_lab_to_xyz(&xyz_out, &hunter_computed);

#if ALWAN_SCALAR_IS_FLOAT
        alwan_scalar const roundtrip_tol = ALWAN_LITERAL(1e-4);
#else
        alwan_scalar const roundtrip_tol = ALWAN_LITERAL(1e-10);
#endif

        alwan_scalar xyz_in_arr[3] = {xyz_in.x, xyz_in.y, xyz_in.z};
        alwan_scalar xyz_out_arr[3] = {xyz_out.x, xyz_out.y, xyz_out.z};
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_ABS(xyz_out_arr[j] - xyz_in_arr[j]);
            if (diff > roundtrip_tol) {
                printf("Round-trip color %zu channel %d failed:\n", i, j);
                printf("  Original XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_in_arr[0], (double)xyz_in_arr[1], (double)xyz_in_arr[2]);
                printf("  Round-trip XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_out_arr[0], (double)xyz_out_arr[1], (double)xyz_out_arr[2]);
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

int test_25_hunter_lab_main(void) {
    printf("\n=== Hunter Lab Tests ===\n");

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
