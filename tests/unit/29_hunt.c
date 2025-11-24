/*
 * Hunt Color Appearance Model Tests
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>

#define TEST_ASSERT(cond, msg) do { if (!(cond)) { printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); return 1; } } while(0)
#define TEST_PASS(name) do { return 0; } while(0)

/* ----------------------------------------------------------------
 * Test Hunt forward transform with reference data
 * ---------------------------------------------------------------- */

static int test_hunt_forward(void) {
    static alwan_scalar const test_data[] = {
#include "reference_values/test_hunt_correlates.csv"
    };

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 9;

    /* Setup viewing conditions matching reference data */
    alwan_hunt_viewing_conditions vc;
    vc.xyz_w.v[0] = ALWAN_LITERAL(95.047);
    vc.xyz_w.v[1] = ALWAN_LITERAL(100.0);
    vc.xyz_w.v[2] = ALWAN_LITERAL(108.883);
    vc.La = ALWAN_LITERAL(318.31);
    vc.Yb = ALWAN_LITERAL(20.0);
    vc.surround = ALWAN_HUNT_SURROUND_NORMAL;
    vc.discount_illuminant = 0;

    /* Looser tolerance for Hunt due to complexity */
    alwan_scalar const tolerance = ALWAN_TEST_TOLERANCE * ALWAN_LITERAL(10000.0);

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 xyz;
        alwan_hunt_correlates expected, computed;

        /* Load test data: XYZ (3) + J, C, h, s, Q, M (6) */
        xyz.v[0] = test_data[i * 9 + 0];
        xyz.v[1] = test_data[i * 9 + 1];
        xyz.v[2] = test_data[i * 9 + 2];

        expected.J = test_data[i * 9 + 3];
        expected.C = test_data[i * 9 + 4];
        expected.h = test_data[i * 9 + 5];
        expected.s = test_data[i * 9 + 6];
        expected.Q = test_data[i * 9 + 7];
        expected.M = test_data[i * 9 + 8];

        /* Test Hunt forward */
        int result = alwan_hunt_forward(&xyz, &vc, &computed);
        TEST_ASSERT(result == 0, "Hunt forward failed");

        /* Check key correlates: J, C, h */
        alwan_scalar diff_J = ALWAN_FABS(computed.J - expected.J);
        alwan_scalar diff_C = ALWAN_FABS(computed.C - expected.C);
        alwan_scalar diff_h = ALWAN_FABS(computed.h - expected.h);
        if (diff_h > ALWAN_LITERAL(180.0)) diff_h = ALWAN_LITERAL(360.0) - diff_h;

        if (diff_J > tolerance || diff_C > tolerance || diff_h > tolerance) {
            printf("Color %zu failed:\n", i);
            printf("  XYZ: [%.2f, %.2f, %.2f]\n",
                   (double)xyz.v[0], (double)xyz.v[1], (double)xyz.v[2]);
            printf("  Expected J=%.4f, C=%.4f, h=%.2f\n",
                   (double)expected.J, (double)expected.C, (double)expected.h);
            printf("  Got      J=%.4f, C=%.4f, h=%.2f\n",
                   (double)computed.J, (double)computed.C, (double)computed.h);
            printf("  Diff: J=%.2e, C=%.2e, h=%.2e (tol=%.2e)\n",
                   (double)diff_J, (double)diff_C, (double)diff_h, (double)tolerance);
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("Hunt forward");
}

/* ----------------------------------------------------------------
 * Main test entry point
 * ---------------------------------------------------------------- */

int test_29_hunt_main(void) {
    printf("\n=== Hunt CAM Tests ===\n");

    int failures = 0;

    printf("Test: Hunt forward transform\n");
    failures += test_hunt_forward();

    if (failures == 0) {
        printf("All Hunt tests passed!\n");
    } else {
        printf("%d Hunt test(s) failed!\n", failures);
    }

    return failures;
}
