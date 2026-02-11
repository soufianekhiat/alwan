/*
 * ZCAM (HDR Color Appearance Model) Tests
 */

#include "test_common.h"

/* ----------------------------------------------------------------
 * Test ZCAM forward transform with reference data
 * ---------------------------------------------------------------- */

static int test_zcam_forward(void) {
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const test_data[] = {
#include "reference_values/test_zcam_correlates.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 9;

    /* Setup viewing conditions matching reference data */
    alwan_zcam_viewing_conditions vc;
    vc.xyz_w.x = ALWAN_LITERAL(95.047);
    vc.xyz_w.y = ALWAN_LITERAL(100.0);
    vc.xyz_w.z = ALWAN_LITERAL(108.883);
    vc.La = ALWAN_LITERAL(100.0);
    vc.Yb = ALWAN_LITERAL(20.0);
    vc.surround = ALWAN_ZCAM_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Tolerance for ZCAM */
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-4);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-8);
#endif

    for (size_t i = 0; i < num_colors; i++) {
        alwan_xyz xyz;
        alwan_zcam_correlates expected, computed;

        /* Load test data: XYZ (3) + J, C, h, Q, M, s (6) */
        xyz.x = test_data[i * 9 + 0];
        xyz.y = test_data[i * 9 + 1];
        xyz.z = test_data[i * 9 + 2];

        expected.Jz = test_data[i * 9 + 3];
        expected.Cz = test_data[i * 9 + 4];
        expected.hz = test_data[i * 9 + 5];
        expected.Qz = test_data[i * 9 + 6];
        expected.Mz = test_data[i * 9 + 7];
        expected.Sz = test_data[i * 9 + 8];

        /* Test ZCAM forward */
        int result = alwan_zcam_forward(&computed, &xyz, &vc);
        TEST_ASSERT(result == 0, "ZCAM forward failed");

        /* Check key correlates: Jz, Cz, hz */
        alwan_scalar diff_J = ALWAN_ABS(computed.Jz - expected.Jz);
        alwan_scalar diff_C = ALWAN_ABS(computed.Cz - expected.Cz);
        alwan_scalar diff_h = ALWAN_ABS(computed.hz - expected.hz);
        if (diff_h > ALWAN_LITERAL(180.0)) diff_h = ALWAN_LITERAL(360.0) - diff_h;

        if (diff_J > tolerance || diff_C > tolerance || diff_h > tolerance) {
            printf("Color %zu failed:\n", i);
            printf("  XYZ: [%.2f, %.2f, %.2f]\n",
                   (double)xyz.x, (double)xyz.y, (double)xyz.z);
            printf("  Expected Jz=%.4f, Cz=%.4f, hz=%.2f\n",
                   (double)expected.Jz, (double)expected.Cz, (double)expected.hz);
            printf("  Got      Jz=%.4f, Cz=%.4f, hz=%.2f\n",
                   (double)computed.Jz, (double)computed.Cz, (double)computed.hz);
            printf("  Diff: J=%.2e, C=%.2e, h=%.2e (tol=%.2e)\n",
                   (double)diff_J, (double)diff_C, (double)diff_h, (double)tolerance);
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("ZCAM forward");
}

/* ----------------------------------------------------------------
 * Main test entry point
 * ---------------------------------------------------------------- */

int test_28_zcam_main(void) {
    printf("\n=== ZCAM (HDR CAM) Tests ===\n");

    int failures = 0;

    printf("Test: ZCAM forward transform\n");
    failures += test_zcam_forward();

    if (failures == 0) {
        printf("All ZCAM tests passed!\n");
    } else {
        printf("%d ZCAM test(s) failed!\n", failures);
    }

    return failures;
}
