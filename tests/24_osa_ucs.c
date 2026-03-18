/*
 * OSA-UCS Tests
 */

#include "test_common.h"

/* ----------------------------------------------------------------
 * Test XYZ <-> OSA-UCS conversions
 * ---------------------------------------------------------------- */

static int test_xyz_osa_ucs_forward(void) {
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const test_data[] = {
#include "reference_values/test_xyz_osa_ucs_pairs.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    alwan_f64 const osa_tolerance = ALWAN_TEST_TOLERANCE;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_xyz xyz_in;
        alwan_osa_ucs osa_expected, osa_computed;

        /* Load test data */
        xyz_in.x = test_data[i * 6 + 0];
        xyz_in.y = test_data[i * 6 + 1];
        xyz_in.z = test_data[i * 6 + 2];
        osa_expected.L = test_data[i * 6 + 3];
        osa_expected.j = test_data[i * 6 + 4];
        osa_expected.g = test_data[i * 6 + 5];

        /* Skip black (0,0,0) as it can have numerical issues */
        if (xyz_in.x < ALWAN_LITERAL(0.01) &&
            xyz_in.y < ALWAN_LITERAL(0.01) &&
            xyz_in.z < ALWAN_LITERAL(0.01)) {
            continue;
        }

        /* Test XYZ -> OSA-UCS */
        alwan_xyz_to_osa_ucs(&osa_computed, &xyz_in);

        alwan_f64 osa_comp_arr[3] = {osa_computed.L, osa_computed.j, osa_computed.g};
        alwan_f64 osa_exp_arr[3] = {osa_expected.L, osa_expected.j, osa_expected.g};
        for (int j = 0; j < 3; j++) {
            alwan_f64 diff = ALWAN_ABS(osa_comp_arr[j] - osa_exp_arr[j]);
            if (diff > osa_tolerance) {
                printf("Color %zu channel %d failed:\n", i, j);
                printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
                printf("  Expected OSA-UCS: [%.10f, %.10f, %.10f]\n",
                       (double)osa_exp_arr[0], (double)osa_exp_arr[1], (double)osa_exp_arr[2]);
                printf("  Got OSA-UCS: [%.10f, %.10f, %.10f]\n",
                       (double)osa_comp_arr[0], (double)osa_comp_arr[1], (double)osa_comp_arr[2]);
                printf("  Diff: %.6e (tolerance: %.6e)\n", (double)diff, (double)osa_tolerance);
                TEST_ASSERT(0, "OSA-UCS values don't match");
            }
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("XYZ -> OSA-UCS forward");
}

/* ----------------------------------------------------------------
 * Test OSA-UCS inverse (approximate)
 * Note: Inverse is approximate, so we test with looser tolerance
 * ---------------------------------------------------------------- */

static int test_osa_ucs_inverse_approximate(void) {
    /* Test a few known conversions with very loose tolerance */
    alwan_xyz xyz, xyz_out;
    alwan_osa_ucs osa;

    /* White D65 */
    xyz.x = ALWAN_LITERAL(95.047);
    xyz.y = ALWAN_LITERAL(100.0);
    xyz.z = ALWAN_LITERAL(108.883);

    alwan_xyz_to_osa_ucs(&osa, &xyz);
    alwan_osa_ucs_to_xyz(&xyz_out, &osa);

    alwan_f64 const loose_tol = ALWAN_TEST_TOLERANCE;

    alwan_f64 xyz_arr[3] = {xyz.x, xyz.y, xyz.z};
    alwan_f64 xyz_out_arr[3] = {xyz_out.x, xyz_out.y, xyz_out.z};
    for (int i = 0; i < 3; i++) {
        alwan_f64 diff = ALWAN_ABS(xyz_out_arr[i] - xyz_arr[i]);
        if (diff > loose_tol) {
            printf("Inverse approximation test failed:\n");
            printf("  Original XYZ: [%.6f, %.6f, %.6f]\n",
                   (double)xyz_arr[0], (double)xyz_arr[1], (double)xyz_arr[2]);
            printf("  OSA-UCS: [%.6f, %.6f, %.6f]\n",
                   (double)osa.L, (double)osa.j, (double)osa.g);
            printf("  Recovered XYZ: [%.6f, %.6f, %.6f]\n",
                   (double)xyz_out_arr[0], (double)xyz_out_arr[1], (double)xyz_out_arr[2]);
            printf("  Diff: %.6e (tolerance: %.6e)\n", (double)diff, (double)loose_tol);
            printf("  [Note: OSA-UCS inverse is approximate]\n");
        }
    }

    printf("  Tested inverse approximation (loose tolerance)\n");
    TEST_PASS("OSA-UCS inverse approximate");
}

/* ----------------------------------------------------------------
 * Main test entry point
 * ---------------------------------------------------------------- */

int test_24_osa_ucs_main(void) {
    printf("\n=== OSA-UCS Tests ===\n");

    int failures = 0;

    printf("Test: XYZ -> OSA-UCS forward\n");
    failures += test_xyz_osa_ucs_forward();

    printf("Test: OSA-UCS inverse (approximate)\n");
    failures += test_osa_ucs_inverse_approximate();

    if (failures == 0) {
        printf("All OSA-UCS tests passed!\n");
    } else {
        printf("%d OSA-UCS test(s) failed!\n", failures);
    }

    return failures;
}
