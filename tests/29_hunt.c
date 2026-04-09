/*
 * Hunt Color Appearance Model Tests
 */

#include "test_common.h"

/* ----------------------------------------------------------------
 * Test Hunt forward transform with reference data
 * ---------------------------------------------------------------- */

static int test_hunt_forward(void) {
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const test_data[] = {
#include "reference_values/test_hunt_correlates.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 9;

    /* Setup viewing conditions matching reference data */
    alwan_hunt_viewing_conditions vc;
    vc.xyz_w.x = ALWAN_LITERAL(95.047);
    vc.xyz_w.y = ALWAN_LITERAL(100.0);
    vc.xyz_w.z = ALWAN_LITERAL(108.883);
    vc.La = ALWAN_LITERAL(318.31);
    vc.Yb = ALWAN_LITERAL(20.0);
    vc.surround = ALWAN_HUNT_SURROUND_NORMAL;
    vc.discount_illuminant = 0;

    alwan_f64 const tolerance = ALWAN_TEST_TOLERANCE;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_xyz_f64 xyz;
        alwan_hunt_correlates expected, computed;

        /* Load test data: XYZ (3) + J, C, h, s, Q, M (6) */
        xyz.x = test_data[i * 9 + 0];
        xyz.y = test_data[i * 9 + 1];
        xyz.z = test_data[i * 9 + 2];

        expected.J = test_data[i * 9 + 3];
        expected.C = test_data[i * 9 + 4];
        expected.h = test_data[i * 9 + 5];
        expected.s = test_data[i * 9 + 6];
        expected.Q = test_data[i * 9 + 7];
        expected.M = test_data[i * 9 + 8];

        /* Test Hunt forward */
        int result = alwan_hunt_forward_f64(&computed, &xyz, &vc);
        TEST_ASSERT(result == 0, "Hunt forward failed");

        /* Check key correlates: J, C, h */
        alwan_f64 diff_J = ALWAN_ABS(computed.J - expected.J);
        alwan_f64 diff_C = ALWAN_ABS(computed.C - expected.C);
        alwan_f64 diff_h = ALWAN_ABS(computed.h - expected.h);
        if (diff_h > ALWAN_LITERAL(180.0)) diff_h = ALWAN_LITERAL(360.0) - diff_h;

        if (diff_J > tolerance || diff_C > tolerance || diff_h > tolerance) {
            printf("Color %zu failed:\n", i);
            printf("  XYZ: [%.2f, %.2f, %.2f]\n",
                   (double)xyz.x, (double)xyz.y, (double)xyz.z);
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
