/*
 * Jzazbz & JzCzhz Tests
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>

#define TEST_ASSERT(cond, msg) do { if (!(cond)) { printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); return 1; } } while(0)
#define TEST_PASS(name) do { return 0; } while(0)

/* ----------------------------------------------------------------
 * Test XYZ <-> Jzazbz conversions
 * ---------------------------------------------------------------- */

static int test_xyz_jzazbz_round_trip(void) {
    static alwan_scalar const test_data[] = {
#include "reference_values/test_xyz_jzazbz_pairs.csv"
    };

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 xyz_in, jzazbz_expected, jzazbz_computed, xyz_out;

        /* Load test data */
        xyz_in.v[0] = test_data[i * 6 + 0];
        xyz_in.v[1] = test_data[i * 6 + 1];
        xyz_in.v[2] = test_data[i * 6 + 2];
        jzazbz_expected.v[0] = test_data[i * 6 + 3];
        jzazbz_expected.v[1] = test_data[i * 6 + 4];
        jzazbz_expected.v[2] = test_data[i * 6 + 5];

        /* Test XYZ -> Jzazbz */
        alwan_xyz_to_jzazbz((alwan_xyz *)&xyz_in, (alwan_jzazbz *)&jzazbz_computed);

#if ALWAN_SCALAR_IS_FLOAT
        alwan_scalar const jzazbz_tol = ALWAN_LITERAL(1e-4);
#else
        alwan_scalar const jzazbz_tol = ALWAN_LITERAL(1e-10);
#endif
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_FABS(jzazbz_computed.v[j] - jzazbz_expected.v[j]);
            if (diff > jzazbz_tol) {
                printf("Color %zu channel %d failed:\n", i, j);
                printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_in.v[0], (double)xyz_in.v[1], (double)xyz_in.v[2]);
                printf("  Expected Jzazbz: [%.10f, %.10f, %.10f]\n",
                       (double)jzazbz_expected.v[0], (double)jzazbz_expected.v[1], (double)jzazbz_expected.v[2]);
                printf("  Got Jzazbz: [%.10f, %.10f, %.10f]\n",
                       (double)jzazbz_computed.v[0], (double)jzazbz_computed.v[1], (double)jzazbz_computed.v[2]);
                printf("  Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "Jzazbz values don't match");
            }
        }

        /* Test round-trip: Jzazbz -> XYZ */
        alwan_jzazbz_to_xyz((alwan_jzazbz *)&jzazbz_computed, (alwan_xyz *)&xyz_out);

#if ALWAN_SCALAR_IS_FLOAT
        alwan_scalar const roundtrip_tol = ALWAN_LITERAL(1e-4);
#else
        alwan_scalar const roundtrip_tol = ALWAN_LITERAL(1e-10);
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
    TEST_PASS("XYZ <-> Jzazbz round-trip");
}

/* ----------------------------------------------------------------
 * Test Jzazbz <-> JzCzhz conversions
 * ---------------------------------------------------------------- */

static int test_jzazbz_jzczhz_round_trip(void) {
    alwan_vec3 jzazbz, jzczhz, jzazbz_out;

    /* Test 1: Neutral (no chroma) */
    jzazbz.v[0] = ALWAN_LITERAL(0.5);
    jzazbz.v[1] = ALWAN_LITERAL(0.0);
    jzazbz.v[2] = ALWAN_LITERAL(0.0);

    alwan_jzazbz_to_jzczhz((alwan_jzazbz *)&jzazbz, (alwan_jzczhz *)&jzczhz);
    TEST_ASSERT(ALWAN_FABS(jzczhz.v[0] - ALWAN_LITERAL(0.5)) < ALWAN_TEST_TOLERANCE, "Jz mismatch");
    TEST_ASSERT(ALWAN_FABS(jzczhz.v[1]) < ALWAN_TEST_TOLERANCE, "Cz should be 0");

    alwan_jzczhz_to_jzazbz((alwan_jzczhz *)&jzczhz, (alwan_jzazbz *)&jzazbz_out);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(ALWAN_FABS(jzazbz_out.v[i] - jzazbz.v[i]) < ALWAN_TEST_TOLERANCE,
                    "Jzazbz round-trip failed");
    }

    /* Test 2: Chromatic color */
    jzazbz.v[0] = ALWAN_LITERAL(0.6);
    jzazbz.v[1] = ALWAN_LITERAL(0.1);
    jzazbz.v[2] = ALWAN_LITERAL(0.05);

    alwan_jzazbz_to_jzczhz((alwan_jzazbz *)&jzazbz, (alwan_jzczhz *)&jzczhz);
    alwan_jzczhz_to_jzazbz((alwan_jzczhz *)&jzczhz, (alwan_jzazbz *)&jzazbz_out);

    for (int i = 0; i < 3; i++) {
        alwan_scalar diff = ALWAN_FABS(jzazbz_out.v[i] - jzazbz.v[i]);
        TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "Jzazbz round-trip failed");
    }

    TEST_PASS("Jzazbz <-> JzCzhz round-trip");
}

/* ----------------------------------------------------------------
 * Main test entry point
 * ---------------------------------------------------------------- */

int test_22_jzazbz_main(void) {
    printf("\n=== Jzazbz & JzCzhz Tests ===\n");

    int failures = 0;

    printf("Test: XYZ <-> Jzazbz round-trip\n");
    failures += test_xyz_jzazbz_round_trip();

    printf("Test: Jzazbz <-> JzCzhz round-trip\n");
    failures += test_jzazbz_jzczhz_round_trip();

    if (failures == 0) {
        printf("All Jzazbz tests passed!\n");
    } else {
        printf("%d Jzazbz test(s) failed!\n", failures);
    }

    return failures;
}
