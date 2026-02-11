/*
 * Jzazbz & JzCzhz Tests
 */

#include "test_common.h"

/* ----------------------------------------------------------------
 * Test XYZ <-> Jzazbz conversions
 * ---------------------------------------------------------------- */

static int test_xyz_jzazbz_round_trip(void) {
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const test_data[] = {
#include "reference_values/test_xyz_jzazbz_pairs.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_xyz xyz_in, xyz_out;
        alwan_jzazbz jzazbz_expected, jzazbz_computed;

        /* Load test data */
        xyz_in.x = test_data[i * 6 + 0];
        xyz_in.y = test_data[i * 6 + 1];
        xyz_in.z = test_data[i * 6 + 2];
        jzazbz_expected.Jz = test_data[i * 6 + 3];
        jzazbz_expected.az = test_data[i * 6 + 4];
        jzazbz_expected.bz = test_data[i * 6 + 5];

        /* Test XYZ -> Jzazbz */
        alwan_xyz_to_jzazbz(&jzazbz_computed, &xyz_in);

#if ALWAN_SCALAR_IS_FLOAT
        alwan_scalar const jzazbz_tol = ALWAN_LITERAL(1e-4);
#else
        alwan_scalar const jzazbz_tol = ALWAN_LITERAL(1e-10);
#endif
        alwan_scalar diff_Jz = ALWAN_ABS(jzazbz_computed.Jz - jzazbz_expected.Jz);
        alwan_scalar diff_az = ALWAN_ABS(jzazbz_computed.az - jzazbz_expected.az);
        alwan_scalar diff_bz = ALWAN_ABS(jzazbz_computed.bz - jzazbz_expected.bz);

        if (diff_Jz > jzazbz_tol) {
            printf("Color %zu Jz channel failed:\n", i);
            printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                   (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
            printf("  Expected Jzazbz: [%.10f, %.10f, %.10f]\n",
                   (double)jzazbz_expected.Jz, (double)jzazbz_expected.az, (double)jzazbz_expected.bz);
            printf("  Got Jzazbz: [%.10f, %.10f, %.10f]\n",
                   (double)jzazbz_computed.Jz, (double)jzazbz_computed.az, (double)jzazbz_computed.bz);
            printf("  Diff: %.6e\n", (double)diff_Jz);
            TEST_ASSERT(0, "Jzazbz values don't match");
        }
        if (diff_az > jzazbz_tol) {
            printf("Color %zu az channel failed:\n", i);
            printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                   (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
            printf("  Expected Jzazbz: [%.10f, %.10f, %.10f]\n",
                   (double)jzazbz_expected.Jz, (double)jzazbz_expected.az, (double)jzazbz_expected.bz);
            printf("  Got Jzazbz: [%.10f, %.10f, %.10f]\n",
                   (double)jzazbz_computed.Jz, (double)jzazbz_computed.az, (double)jzazbz_computed.bz);
            printf("  Diff: %.6e\n", (double)diff_az);
            TEST_ASSERT(0, "Jzazbz values don't match");
        }
        if (diff_bz > jzazbz_tol) {
            printf("Color %zu bz channel failed:\n", i);
            printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                   (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
            printf("  Expected Jzazbz: [%.10f, %.10f, %.10f]\n",
                   (double)jzazbz_expected.Jz, (double)jzazbz_expected.az, (double)jzazbz_expected.bz);
            printf("  Got Jzazbz: [%.10f, %.10f, %.10f]\n",
                   (double)jzazbz_computed.Jz, (double)jzazbz_computed.az, (double)jzazbz_computed.bz);
            printf("  Diff: %.6e\n", (double)diff_bz);
            TEST_ASSERT(0, "Jzazbz values don't match");
        }

        /* Test round-trip: Jzazbz -> XYZ */
        alwan_jzazbz_to_xyz(&xyz_out, &jzazbz_computed);

#if ALWAN_SCALAR_IS_FLOAT
        alwan_scalar const roundtrip_tol = ALWAN_LITERAL(1e-4);
#else
        alwan_scalar const roundtrip_tol = ALWAN_LITERAL(1e-10);
#endif

        alwan_scalar diff_x = ALWAN_ABS(xyz_out.x - xyz_in.x);
        alwan_scalar diff_y = ALWAN_ABS(xyz_out.y - xyz_in.y);
        alwan_scalar diff_z = ALWAN_ABS(xyz_out.z - xyz_in.z);

        if (diff_x > roundtrip_tol) {
            printf("Round-trip color %zu X channel failed:\n", i);
            printf("  Original XYZ: [%.6f, %.6f, %.6f]\n",
                   (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
            printf("  Round-trip XYZ: [%.6f, %.6f, %.6f]\n",
                   (double)xyz_out.x, (double)xyz_out.y, (double)xyz_out.z);
            printf("  Diff: %.6e\n", (double)diff_x);
            TEST_ASSERT(0, "XYZ round-trip failed");
        }
        if (diff_y > roundtrip_tol) {
            printf("Round-trip color %zu Y channel failed:\n", i);
            printf("  Original XYZ: [%.6f, %.6f, %.6f]\n",
                   (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
            printf("  Round-trip XYZ: [%.6f, %.6f, %.6f]\n",
                   (double)xyz_out.x, (double)xyz_out.y, (double)xyz_out.z);
            printf("  Diff: %.6e\n", (double)diff_y);
            TEST_ASSERT(0, "XYZ round-trip failed");
        }
        if (diff_z > roundtrip_tol) {
            printf("Round-trip color %zu Z channel failed:\n", i);
            printf("  Original XYZ: [%.6f, %.6f, %.6f]\n",
                   (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
            printf("  Round-trip XYZ: [%.6f, %.6f, %.6f]\n",
                   (double)xyz_out.x, (double)xyz_out.y, (double)xyz_out.z);
            printf("  Diff: %.6e\n", (double)diff_z);
            TEST_ASSERT(0, "XYZ round-trip failed");
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("XYZ <-> Jzazbz round-trip");
}

/* ----------------------------------------------------------------
 * Test Jzazbz <-> JzCzhz conversions
 * ---------------------------------------------------------------- */

static int test_jzazbz_jzczhz_round_trip(void) {
    alwan_jzazbz jzazbz, jzazbz_out;
    alwan_jzczhz jzczhz;

    /* Test 1: Neutral (no chroma) */
    jzazbz.Jz = ALWAN_LITERAL(0.5);
    jzazbz.az = ALWAN_LITERAL(0.0);
    jzazbz.bz = ALWAN_LITERAL(0.0);

    alwan_jzazbz_to_jzczhz(&jzczhz, &jzazbz);
    TEST_ASSERT(ALWAN_ABS(jzczhz.Jz - ALWAN_LITERAL(0.5)) < TEST_TOLERANCE, "Jz mismatch");
    TEST_ASSERT(ALWAN_ABS(jzczhz.Cz) < TEST_TOLERANCE, "Cz should be 0");

    alwan_jzczhz_to_jzazbz(&jzazbz_out, &jzczhz);
    TEST_ASSERT(ALWAN_ABS(jzazbz_out.Jz - jzazbz.Jz) < TEST_TOLERANCE,
                "Jzazbz round-trip failed (Jz)");
    TEST_ASSERT(ALWAN_ABS(jzazbz_out.az - jzazbz.az) < TEST_TOLERANCE,
                "Jzazbz round-trip failed (az)");
    TEST_ASSERT(ALWAN_ABS(jzazbz_out.bz - jzazbz.bz) < TEST_TOLERANCE,
                "Jzazbz round-trip failed (bz)");

    /* Test 2: Chromatic color */
    jzazbz.Jz = ALWAN_LITERAL(0.6);
    jzazbz.az = ALWAN_LITERAL(0.1);
    jzazbz.bz = ALWAN_LITERAL(0.05);

    alwan_jzazbz_to_jzczhz(&jzczhz, &jzazbz);
    alwan_jzczhz_to_jzazbz(&jzazbz_out, &jzczhz);

    alwan_scalar diff_Jz = ALWAN_ABS(jzazbz_out.Jz - jzazbz.Jz);
    alwan_scalar diff_az = ALWAN_ABS(jzazbz_out.az - jzazbz.az);
    alwan_scalar diff_bz = ALWAN_ABS(jzazbz_out.bz - jzazbz.bz);

    TEST_ASSERT(diff_Jz < TEST_TOLERANCE, "Jzazbz round-trip failed (Jz)");
    TEST_ASSERT(diff_az < TEST_TOLERANCE, "Jzazbz round-trip failed (az)");
    TEST_ASSERT(diff_bz < TEST_TOLERANCE, "Jzazbz round-trip failed (bz)");

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
