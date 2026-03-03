/*
 * IPT Color Space Tests
 */

#include "test_common.h"

/* ----------------------------------------------------------------
 * Test XYZ <-> IPT conversions
 * ---------------------------------------------------------------- */

static int test_xyz_ipt_round_trip(void) {
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const test_data[] = {
#include "reference_values/test_xyz_ipt_pairs.csv"
    };
ALWAN_DIAG_POP

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_xyz xyz_in, xyz_out;
        alwan_ipt ipt_expected, ipt_computed;

        /* Load test data */
        xyz_in.x = test_data[i * 6 + 0];
        xyz_in.y = test_data[i * 6 + 1];
        xyz_in.z = test_data[i * 6 + 2];
        ipt_expected.I = test_data[i * 6 + 3];
        ipt_expected.P = test_data[i * 6 + 4];
        ipt_expected.T = test_data[i * 6 + 5];

        /* Test XYZ -> IPT */
        alwan_xyz_to_ipt(&ipt_computed, &xyz_in);

        alwan_scalar const ipt_tol = TEST_TOLERANCE;
        alwan_scalar ipt_comp_arr[3] = {ipt_computed.I, ipt_computed.P, ipt_computed.T};
        alwan_scalar ipt_exp_arr[3] = {ipt_expected.I, ipt_expected.P, ipt_expected.T};
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_ABS(ipt_comp_arr[j] - ipt_exp_arr[j]);
            if (diff > ipt_tol) {
                printf("Color %zu channel %d failed:\n", i, j);
                printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_in.x, (double)xyz_in.y, (double)xyz_in.z);
                printf("  Expected IPT: [%.10f, %.10f, %.10f]\n",
                       (double)ipt_exp_arr[0], (double)ipt_exp_arr[1], (double)ipt_exp_arr[2]);
                printf("  Got IPT: [%.10f, %.10f, %.10f]\n",
                       (double)ipt_comp_arr[0], (double)ipt_comp_arr[1], (double)ipt_comp_arr[2]);
                printf("  Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "IPT values don't match");
            }
        }

        /* Test round-trip: IPT -> XYZ */
        alwan_ipt_to_xyz(&xyz_out, &ipt_computed);

        alwan_scalar const roundtrip_tol = TEST_TOLERANCE;

        alwan_scalar xyz_in_arr[3] = {xyz_in.x, xyz_in.y, xyz_in.z};
        alwan_scalar xyz_out_arr[3] = {xyz_out.x, xyz_out.y, xyz_out.z};
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_ABS(xyz_out_arr[j] - xyz_in_arr[j]);
            if (diff > roundtrip_tol) {
                printf("Round-trip color %zu channel %d failed:\n", i, j);
                printf("  Original XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_in_arr[0], (double)xyz_in_arr[1], (double)xyz_in_arr[2]);
                printf("  Round-trip XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz_out_arr[0], (double)xyz_out_arr[1], (double)xyz_out_arr[2]);
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
    alwan_ipt ipt, ipt_out;
    alwan_iptch iptch;

    /* Test 1: Neutral (no chroma) */
    ipt.I = ALWAN_LITERAL(0.5);
    ipt.P = ALWAN_LITERAL(0.0);
    ipt.T = ALWAN_LITERAL(0.0);

    alwan_ipt_to_iptch(&iptch, &ipt);
    TEST_ASSERT(ALWAN_ABS(iptch.I - ALWAN_LITERAL(0.5)) < TEST_TOLERANCE, "I mismatch");
    TEST_ASSERT(ALWAN_ABS(iptch.C) < TEST_TOLERANCE, "C should be 0");

    alwan_iptch_to_ipt(&ipt_out, &iptch);
    alwan_scalar ipt_arr[3] = {ipt.I, ipt.P, ipt.T};
    alwan_scalar ipt_out_arr[3] = {ipt_out.I, ipt_out.P, ipt_out.T};
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(ALWAN_ABS(ipt_out_arr[i] - ipt_arr[i]) < TEST_TOLERANCE,
                    "IPT round-trip failed");
    }

    /* Test 2: Chromatic color */
    ipt.I = ALWAN_LITERAL(0.6);
    ipt.P = ALWAN_LITERAL(0.1);
    ipt.T = ALWAN_LITERAL(0.05);

    alwan_ipt_to_iptch(&iptch, &ipt);
    alwan_iptch_to_ipt(&ipt_out, &iptch);

    ipt_arr[0] = ipt.I; ipt_arr[1] = ipt.P; ipt_arr[2] = ipt.T;
    ipt_out_arr[0] = ipt_out.I; ipt_out_arr[1] = ipt_out.P; ipt_out_arr[2] = ipt_out.T;
    for (int i = 0; i < 3; i++) {
        alwan_scalar diff = ALWAN_ABS(ipt_out_arr[i] - ipt_arr[i]);
        TEST_ASSERT(diff < TEST_TOLERANCE, "IPT round-trip failed");
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
