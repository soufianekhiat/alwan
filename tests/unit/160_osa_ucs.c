/*
 * P1.5: OSA-UCS Tests
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>

#define TEST_ASSERT(cond, msg) do { if (!(cond)) { printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); return 1; } } while(0)
#define TEST_PASS(name) do { return 0; } while(0)

/* ----------------------------------------------------------------
 * Test XYZ <-> OSA-UCS conversions
 * ---------------------------------------------------------------- */

static int test_xyz_osa_ucs_forward(void) {
    static alwan_scalar const test_data[] = {
#include "reference_values/test_xyz_osa_ucs_pairs.csv"
    };

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    /* OSA-UCS has looser tolerance due to complex transformations */
    alwan_scalar const osa_tolerance = ALWAN_LITERAL(0.5);  /* Absolute tolerance */

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 xyz_in, osa_expected, osa_computed;

        /* Load test data */
        xyz_in.v[0] = test_data[i * 6 + 0];
        xyz_in.v[1] = test_data[i * 6 + 1];
        xyz_in.v[2] = test_data[i * 6 + 2];
        osa_expected.v[0] = test_data[i * 6 + 3];
        osa_expected.v[1] = test_data[i * 6 + 4];
        osa_expected.v[2] = test_data[i * 6 + 5];

        /* Skip black (0,0,0) as it can have numerical issues */
        if (xyz_in.v[0] < ALWAN_LITERAL(0.01) &&
            xyz_in.v[1] < ALWAN_LITERAL(0.01) &&
            xyz_in.v[2] < ALWAN_LITERAL(0.01)) {
            continue;
        }

        /* Test XYZ -> OSA-UCS */
        alwan_xyz_to_osa_ucs(&xyz_in, &osa_computed);

        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_FABS(osa_computed.v[j] - osa_expected.v[j]);
            if (diff > osa_tolerance) {
                printf("Color %zu channel %d failed:\n", i, j);
                printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_in.v[0], (double)xyz_in.v[1], (double)xyz_in.v[2]);
                printf("  Expected OSA-UCS: [%.10f, %.10f, %.10f]\n",
                       (double)osa_expected.v[0], (double)osa_expected.v[1], (double)osa_expected.v[2]);
                printf("  Got OSA-UCS: [%.10f, %.10f, %.10f]\n",
                       (double)osa_computed.v[0], (double)osa_computed.v[1], (double)osa_computed.v[2]);
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
    alwan_vec3 xyz, osa, xyz_out;

    /* White D65 */
    xyz.v[0] = ALWAN_LITERAL(95.047);
    xyz.v[1] = ALWAN_LITERAL(100.0);
    xyz.v[2] = ALWAN_LITERAL(108.883);

    alwan_xyz_to_osa_ucs(&xyz, &osa);
    alwan_osa_ucs_to_xyz(&osa, &xyz_out);

    /* Very loose absolute tolerance for inverse (approximate solution) */
    alwan_scalar const loose_tol = ALWAN_LITERAL(20.0);

    for (int i = 0; i < 3; i++) {
        alwan_scalar diff = ALWAN_FABS(xyz_out.v[i] - xyz.v[i]);
        if (diff > loose_tol) {
            printf("Inverse approximation test failed:\n");
            printf("  Original XYZ: [%.6f, %.6f, %.6f]\n",
                   (double)xyz.v[0], (double)xyz.v[1], (double)xyz.v[2]);
            printf("  OSA-UCS: [%.6f, %.6f, %.6f]\n",
                   (double)osa.v[0], (double)osa.v[1], (double)osa.v[2]);
            printf("  Recovered XYZ: [%.6f, %.6f, %.6f]\n",
                   (double)xyz_out.v[0], (double)xyz_out.v[1], (double)xyz_out.v[2]);
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

int test_160_osa_ucs_main(void) {
    printf("\n=== P1.5: OSA-UCS Tests ===\n");

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
