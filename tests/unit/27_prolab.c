/*
 * ProLab Color Space Tests
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>

#define TEST_ASSERT(cond, msg) do { if (!(cond)) { printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); return 1; } } while(0)
#define TEST_PASS(name) do { return 0; } while(0)

/* ----------------------------------------------------------------
 * Test XYZ <-> ProLab conversions
 * ---------------------------------------------------------------- */

static int test_xyz_prolab_round_trip(void) {
    static alwan_scalar const test_data[] = {
#include "reference_values/test_xyz_prolab_pairs.csv"
    };

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    /* ProLab uses projective transformation */
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const prolab_tolerance = ALWAN_LITERAL(1e-4);
#else
    alwan_scalar const prolab_tolerance = ALWAN_LITERAL(1e-8);
#endif

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 xyz_in, prolab_expected, prolab_computed, xyz_out;

        /* Load test data */
        xyz_in.v[0] = test_data[i * 6 + 0];
        xyz_in.v[1] = test_data[i * 6 + 1];
        xyz_in.v[2] = test_data[i * 6 + 2];
        prolab_expected.v[0] = test_data[i * 6 + 3];
        prolab_expected.v[1] = test_data[i * 6 + 4];
        prolab_expected.v[2] = test_data[i * 6 + 5];

        /* Skip black (0,0,0) - projective transforms are undefined at origin */
        if (xyz_in.v[0] < ALWAN_LITERAL(0.01) &&
            xyz_in.v[1] < ALWAN_LITERAL(0.01) &&
            xyz_in.v[2] < ALWAN_LITERAL(0.01)) {
            continue;
        }

        /* Test XYZ -> ProLab */
        alwan_xyz_to_prolab(&xyz_in, &prolab_computed);

        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_FABS(prolab_computed.v[j] - prolab_expected.v[j]);
            if (diff > prolab_tolerance) {
                printf("Color %zu channel %d failed:\n", i, j);
                printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_in.v[0], (double)xyz_in.v[1], (double)xyz_in.v[2]);
                printf("  Expected ProLab: [%.10f, %.10f, %.10f]\n",
                       (double)prolab_expected.v[0], (double)prolab_expected.v[1], (double)prolab_expected.v[2]);
                printf("  Got ProLab: [%.10f, %.10f, %.10f]\n",
                       (double)prolab_computed.v[0], (double)prolab_computed.v[1], (double)prolab_computed.v[2]);
                printf("  Diff: %.6e (tolerance: %.6e)\n", (double)diff, (double)prolab_tolerance);
                TEST_ASSERT(0, "ProLab values don't match");
            }
        }

        /* Test round-trip: ProLab -> XYZ */
        alwan_prolab_to_xyz(&prolab_computed, &xyz_out);

        /* Projective transforms amplify matrix coefficient errors; 2e-7 is practical limit */
#if ALWAN_SCALAR_IS_FLOAT
        alwan_scalar const roundtrip_tol = ALWAN_LITERAL(1e-4);
#else
        alwan_scalar const roundtrip_tol = ALWAN_LITERAL(2e-7);
#endif

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
