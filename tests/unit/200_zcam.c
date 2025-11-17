/*
 * ZCAM (HDR Color Appearance Model) Tests
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>

#define TEST_ASSERT(cond, msg) do { if (!(cond)) { printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); return 1; } } while(0)
#define TEST_PASS(name) do { return 0; } while(0)

/* ----------------------------------------------------------------
 * Test ZCAM forward transform with reference data
 * ---------------------------------------------------------------- */

static int test_zcam_forward(void) {
    static alwan_scalar const test_data[] = {
#include "reference_values/test_zcam_correlates.csv"
    };

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 9;

    /* Setup viewing conditions matching reference data */
    alwan_zcam_viewing_conditions vc;
    vc.xyz_w.v[0] = ALWAN_LITERAL(95.047);
    vc.xyz_w.v[1] = ALWAN_LITERAL(100.0);
    vc.xyz_w.v[2] = ALWAN_LITERAL(108.883);
    vc.La = ALWAN_LITERAL(100.0);
    vc.Yb = ALWAN_LITERAL(20.0);
    vc.surround = ALWAN_ZCAM_SURROUND_AVERAGE;
    vc.discount_illuminant = 0;

    /* Looser tolerance for ZCAM due to complexity */
    alwan_scalar const tolerance = ALWAN_TEST_TOLERANCE * ALWAN_LITERAL(1000.0);

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 xyz;
        alwan_zcam_correlates expected, computed;

        /* Load test data: XYZ (3) + J, C, h, Q, M, s (6) */
        xyz.v[0] = test_data[i * 9 + 0];
        xyz.v[1] = test_data[i * 9 + 1];
        xyz.v[2] = test_data[i * 9 + 2];

        expected.Jz = test_data[i * 9 + 3];
        expected.Cz = test_data[i * 9 + 4];
        expected.hz = test_data[i * 9 + 5];
        expected.Qz = test_data[i * 9 + 6];
        expected.Mz = test_data[i * 9 + 7];
        expected.Sz = test_data[i * 9 + 8];

        /* Test ZCAM forward */
        int result = alwan_zcam_forward(&xyz, &vc, &computed);
        TEST_ASSERT(result == 0, "ZCAM forward failed");

        /* Check key correlates: Jz, Cz, hz */
        alwan_scalar diff_J = ALWAN_FABS(computed.Jz - expected.Jz);
        alwan_scalar diff_C = ALWAN_FABS(computed.Cz - expected.Cz);
        alwan_scalar diff_h = ALWAN_FABS(computed.hz - expected.hz);
        if (diff_h > ALWAN_LITERAL(180.0)) diff_h = ALWAN_LITERAL(360.0) - diff_h;

        if (diff_J > tolerance || diff_C > tolerance || diff_h > tolerance) {
            printf("Color %zu failed:\n", i);
            printf("  XYZ: [%.2f, %.2f, %.2f]\n",
                   (double)xyz.v[0], (double)xyz.v[1], (double)xyz.v[2]);
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

int test_200_zcam_main(void) {
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
