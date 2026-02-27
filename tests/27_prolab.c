/*
 * ProLab Color Space Tests
 */

#include "test_common.h"

/* ----------------------------------------------------------------
 * Test XYZ <-> ProLab conversions
 * ---------------------------------------------------------------- */

static int test_xyz_prolab_round_trip(void) {
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const test_data[] = {
#include "reference_values/test_xyz_prolab_pairs.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    alwan_scalar const prolab_tolerance = TEST_TOLERANCE;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_xyz xyz_in, xyz_out;
        alwan_prolab prolab_expected, prolab_computed;

        /* Load test data */
        xyz_in.x = test_data[i * 6 + 0];
        xyz_in.y = test_data[i * 6 + 1];
        xyz_in.z = test_data[i * 6 + 2];
        prolab_expected.L = test_data[i * 6 + 3];
        prolab_expected.a = test_data[i * 6 + 4];
        prolab_expected.b = test_data[i * 6 + 5];

        /* Skip black (0,0,0) - projective transforms are undefined at origin */
        if (xyz_in.x < ALWAN_LITERAL(0.01) &&
            xyz_in.y < ALWAN_LITERAL(0.01) &&
            xyz_in.z < ALWAN_LITERAL(0.01)) {
            continue;
        }

        /* Test XYZ -> ProLab */
        alwan_xyz_to_prolab(&prolab_computed, &xyz_in);

        alwan_scalar prolab_comp_arr[3] = {prolab_computed.L, prolab_computed.a, prolab_computed.b};
        alwan_scalar prolab_exp_arr[3] = {prolab_expected.L, prolab_expected.a, prolab_expected.b};
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_ABS(prolab_comp_arr[j] - prolab_exp_arr[j]);
            if (diff > prolab_tolerance) {
                printf("Color %zu channel %d failed:\n", i, j);
                printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
                printf("  Expected ProLab: [%.10f, %.10f, %.10f]\n",
                       (double)prolab_exp_arr[0], (double)prolab_exp_arr[1], (double)prolab_exp_arr[2]);
                printf("  Got ProLab: [%.10f, %.10f, %.10f]\n",
                       (double)prolab_comp_arr[0], (double)prolab_comp_arr[1], (double)prolab_comp_arr[2]);
                printf("  Diff: %.6e (tolerance: %.6e)\n", (double)diff, (double)prolab_tolerance);
                TEST_ASSERT(0, "ProLab values don't match");
            }
        }

        /* Test round-trip: ProLab -> XYZ */
        alwan_prolab_to_xyz(&xyz_out, &prolab_computed);

        alwan_scalar const roundtrip_tol = TEST_TOLERANCE;

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
    TEST_PASS("XYZ <-> ProLab round-trip");
}

/* ----------------------------------------------------------------
 * Main test entry point
 * ---------------------------------------------------------------- */

int test_27_prolab_main(void) {
    printf("\n=== ProLab Color Space Tests ===\n");

    int failures = 0;

    printf("Test: XYZ <-> ProLab round-trip\n");
    failures += test_xyz_prolab_round_trip();

    if (failures == 0) {
        printf("All ProLab tests passed!\n");
    } else {
        printf("%d ProLab test(s) failed!\n", failures);
    }

    return failures;
}
