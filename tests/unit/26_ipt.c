/*
 * IPT Color Space Tests
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>

#define TEST_ASSERT(cond, msg) do { if (!(cond)) { printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); return 1; } } while(0)
#define TEST_PASS(name) do { return 0; } while(0)

/* ----------------------------------------------------------------
 * Test XYZ <-> IPT conversions
 * ---------------------------------------------------------------- */

static int test_xyz_ipt_round_trip(void) {
    static alwan_scalar const test_data[] = {
#include "reference_values/test_xyz_ipt_pairs.csv"
    };

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 xyz_in, ipt_expected, ipt_computed, xyz_out;

        /* Load test data */
        xyz_in.v[0] = test_data[i * 6 + 0];
        xyz_in.v[1] = test_data[i * 6 + 1];
        xyz_in.v[2] = test_data[i * 6 + 2];
        ipt_expected.v[0] = test_data[i * 6 + 3];
        ipt_expected.v[1] = test_data[i * 6 + 4];
        ipt_expected.v[2] = test_data[i * 6 + 5];

        /* Test XYZ -> IPT */
        alwan_xyz_to_ipt((alwan_xyz const *)&xyz_in, (alwan_ipt *)&ipt_computed);

#if ALWAN_SCALAR_IS_FLOAT
        alwan_scalar const ipt_tol = ALWAN_LITERAL(1e-4);
#else
        alwan_scalar const ipt_tol = ALWAN_LITERAL(1e-8);
#endif
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_FABS(ipt_computed.v[j] - ipt_expected.v[j]);
            if (diff > ipt_tol) {
                printf("Color %zu channel %d failed:\n", i, j);
                printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_in.v[0], (double)xyz_in.v[1], (double)xyz_in.v[2]);
                printf("  Expected IPT: [%.10f, %.10f, %.10f]\n",
                       (double)ipt_expected.v[0], (double)ipt_expected.v[1], (double)ipt_expected.v[2]);
                printf("  Got IPT: [%.10f, %.10f, %.10f]\n",
                       (double)ipt_computed.v[0], (double)ipt_computed.v[1], (double)ipt_computed.v[2]);
                printf("  Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "IPT values don't match");
            }
        }

        /* Test round-trip: IPT -> XYZ */
        alwan_ipt_to_xyz((alwan_ipt const *)&ipt_computed, (alwan_xyz *)&xyz_out);

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
    TEST_PASS("XYZ <-> IPT round-trip");
}

/* ----------------------------------------------------------------
 * Test IPT <-> IPTch conversions
 * ---------------------------------------------------------------- */

static int test_ipt_iptch_round_trip(void) {
    alwan_vec3 ipt, iptch, ipt_out;

    /* Test 1: Neutral (no chroma) */
    ipt.v[0] = ALWAN_LITERAL(0.5);
    ipt.v[1] = ALWAN_LITERAL(0.0);
    ipt.v[2] = ALWAN_LITERAL(0.0);

    alwan_ipt_to_iptch((alwan_ipt const *)&ipt, &iptch);
    TEST_ASSERT(ALWAN_FABS(iptch.v[0] - ALWAN_LITERAL(0.5)) < ALWAN_TEST_TOLERANCE, "I mismatch");
    TEST_ASSERT(ALWAN_FABS(iptch.v[1]) < ALWAN_TEST_TOLERANCE, "C should be 0");

    alwan_iptch_to_ipt(&iptch, (alwan_ipt *)&ipt_out);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(ALWAN_FABS(ipt_out.v[i] - ipt.v[i]) < ALWAN_TEST_TOLERANCE,
                    "IPT round-trip failed");
    }

    /* Test 2: Chromatic color */
    ipt.v[0] = ALWAN_LITERAL(0.6);
    ipt.v[1] = ALWAN_LITERAL(0.1);
    ipt.v[2] = ALWAN_LITERAL(0.05);

    alwan_ipt_to_iptch((alwan_ipt const *)&ipt, &iptch);
    alwan_iptch_to_ipt(&iptch, (alwan_ipt *)&ipt_out);

    for (int i = 0; i < 3; i++) {
        alwan_scalar diff = ALWAN_FABS(ipt_out.v[i] - ipt.v[i]);
        TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "IPT round-trip failed");
    }

    TEST_PASS("IPT <-> IPTch round-trip");
}

/* ----------------------------------------------------------------
 * Main test entry point
 * ---------------------------------------------------------------- */

int test_26_ipt_main(void) {
    printf("\n=== IPT Color Space Tests ===\n");

    int failures = 0;

    printf("Test: XYZ <-> IPT round-trip\n");
    failures += test_xyz_ipt_round_trip();

    printf("Test: IPT <-> IPTch round-trip\n");
    failures += test_ipt_iptch_round_trip();

    if (failures == 0) {
        printf("All IPT tests passed!\n");
    } else {
        printf("%d IPT test(s) failed!\n", failures);
    }

    return failures;
}
