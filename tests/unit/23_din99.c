/*
 * DIN99 Family Tests
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>

#define TEST_ASSERT(cond, msg) do { if (!(cond)) { printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); return 1; } } while(0)
#define TEST_PASS(name) do { return 0; } while(0)

/* ----------------------------------------------------------------
 * Embedded test data
 * ---------------------------------------------------------------- */

/* DIN99 variant test data - embedded at compile time */
static alwan_scalar const g_din99_test_data[] = {
#include "reference_values/test_lab_din99_pairs.csv"
};

static alwan_scalar const g_din99b_test_data[] = {
#include "reference_values/test_lab_din99b_pairs.csv"
};

static alwan_scalar const g_din99c_test_data[] = {
#include "reference_values/test_lab_din99c_pairs.csv"
};

static alwan_scalar const g_din99d_test_data[] = {
#include "reference_values/test_lab_din99d_pairs.csv"
};

/* ----------------------------------------------------------------
 * Test Lab <-> DIN99 conversions
 * ---------------------------------------------------------------- */

static int test_din99_variant(int variant, char const *variant_name, alwan_scalar const *test_data, size_t data_count) {
    size_t const num_colors = data_count / 6;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_xyz xyz;
        alwan_lab lab;
        alwan_din99 din99_expected, din99_computed;
        alwan_lab lab_out;

        /* Load test data */
        xyz.x = test_data[i * 6 + 0];
        xyz.y = test_data[i * 6 + 1];
        xyz.z = test_data[i * 6 + 2];
        din99_expected.L99 = test_data[i * 6 + 3];
        din99_expected.a99 = test_data[i * 6 + 4];
        din99_expected.b99 = test_data[i * 6 + 5];

        /* Convert XYZ to Lab first (D65) */
        alwan_xyz D65 = {ALWAN_D65_X, ALWAN_D65_Y, ALWAN_D65_Z};
        alwan_xyz_to_lab(&xyz, &D65, &lab);

        /* Test Lab -> DIN99 */
        alwan_lab_to_din99(&lab, &din99_computed, variant);

        /* Tolerance for trig operations */
#if ALWAN_SCALAR_IS_FLOAT
        alwan_scalar const din99_tol = ALWAN_LITERAL(1e-4);
#else
        alwan_scalar const din99_tol = ALWAN_LITERAL(1e-7);
#endif
        alwan_scalar din99_comp_arr[3] = {din99_computed.L99, din99_computed.a99, din99_computed.b99};
        alwan_scalar din99_exp_arr[3] = {din99_expected.L99, din99_expected.a99, din99_expected.b99};
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_FABS(din99_comp_arr[j] - din99_exp_arr[j]);
            if (diff > din99_tol) {
                printf("Color %zu channel %d failed (%s):\n", i, j, variant_name);
                printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz.x, (double)xyz.y, (double)xyz.z);
                printf("  Lab: [%.6f, %.6f, %.6f]\n",
                       (double)lab.L, (double)lab.a, (double)lab.b);
                printf("  Expected DIN99: [%.10f, %.10f, %.10f]\n",
                       (double)din99_exp_arr[0], (double)din99_exp_arr[1], (double)din99_exp_arr[2]);
                printf("  Got DIN99: [%.10f, %.10f, %.10f]\n",
                       (double)din99_comp_arr[0], (double)din99_comp_arr[1], (double)din99_comp_arr[2]);
                printf("  Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "DIN99 values don't match");
            }
        }

        /* Test round-trip: DIN99 -> Lab */
        alwan_din99_to_lab(&din99_computed, &lab_out, variant);

#if ALWAN_SCALAR_IS_FLOAT
        alwan_scalar const roundtrip_tol = ALWAN_LITERAL(1e-4);
#else
        alwan_scalar const roundtrip_tol = ALWAN_LITERAL(1e-8);
#endif

        alwan_scalar lab_arr[3] = {lab.L, lab.a, lab.b};
        alwan_scalar lab_out_arr[3] = {lab_out.L, lab_out.a, lab_out.b};
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_FABS(lab_out_arr[j] - lab_arr[j]);
            if (diff > roundtrip_tol) {
                printf("Round-trip color %zu channel %d failed (%s):\n", i, j, variant_name);
                printf("  Original Lab: [%.6f, %.6f, %.6f]\n",
                       (double)lab_arr[0], (double)lab_arr[1], (double)lab_arr[2]);
                printf("  Round-trip Lab: [%.6f, %.6f, %.6f]\n",
                       (double)lab_out_arr[0], (double)lab_out_arr[1], (double)lab_out_arr[2]);
                printf("  Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "Lab round-trip failed");
            }
        }
    }

    printf("  Tested %zu colors for %s\n", num_colors, variant_name);
    TEST_PASS(variant_name);
}

/* ----------------------------------------------------------------
 * Main test entry point
 * ---------------------------------------------------------------- */

int test_23_din99_main(void) {
    printf("\n=== DIN99 Family Tests ===\n");

    int failures = 0;

    printf("Test: Lab <-> DIN99 (variant 0)\n");
    failures += test_din99_variant(0, "DIN99", g_din99_test_data,
                                    sizeof(g_din99_test_data) / sizeof(g_din99_test_data[0]));

    printf("Test: Lab <-> DIN99b (variant 1)\n");
    failures += test_din99_variant(1, "DIN99b", g_din99b_test_data,
                                    sizeof(g_din99b_test_data) / sizeof(g_din99b_test_data[0]));

    printf("Test: Lab <-> DIN99c (variant 2)\n");
    failures += test_din99_variant(2, "DIN99c", g_din99c_test_data,
                                    sizeof(g_din99c_test_data) / sizeof(g_din99c_test_data[0]));

    printf("Test: Lab <-> DIN99d (variant 3)\n");
    failures += test_din99_variant(3, "DIN99d", g_din99d_test_data,
                                    sizeof(g_din99d_test_data) / sizeof(g_din99d_test_data[0]));

    if (failures == 0) {
        printf("All DIN99 tests passed!\n");
    } else {
        printf("%d DIN99 test(s) failed!\n", failures);
    }

    return failures;
}
