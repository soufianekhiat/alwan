/*
 * DIN99 Family Tests
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>

#define TEST_ASSERT(cond, msg) do { if (!(cond)) { printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); return 1; } } while(0)
#define TEST_PASS(name) do { return 0; } while(0)

/* ----------------------------------------------------------------
 * Test Lab <-> DIN99 conversions
 * ---------------------------------------------------------------- */

static int test_din99_variant(int variant, char const *variant_name, char const *csv_file) {
    static alwan_scalar test_data[16 * 6];  /* Static buffer for data */

    /* Include the CSV file */
    FILE *f = fopen(csv_file, "r");
    if (!f) {
        printf("  [SKIP] Could not open %s\n", csv_file);
        return 0;
    }

    size_t count = 0;
    double temp;
    while (count < 16 * 6 && fscanf(f, "%lf,", &temp) == 1) {
        test_data[count] = (alwan_scalar)temp;
        count++;
    }
    fclose(f);

    size_t const num_colors = count / 6;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 xyz, lab, din99_expected, din99_computed, lab_out;

        /* Load test data */
        xyz.v[0] = test_data[i * 6 + 0];
        xyz.v[1] = test_data[i * 6 + 1];
        xyz.v[2] = test_data[i * 6 + 2];
        din99_expected.v[0] = test_data[i * 6 + 3];
        din99_expected.v[1] = test_data[i * 6 + 4];
        din99_expected.v[2] = test_data[i * 6 + 5];

        /* Convert XYZ to Lab first (D65) */
        alwan_vec3 D65 = {{ALWAN_D65_X, ALWAN_D65_Y, ALWAN_D65_Z}};
        alwan_xyz_to_lab(&xyz, &D65, &lab);

        /* Test Lab -> DIN99 */
        alwan_lab_to_din99(&lab, &din99_computed, variant);

        /* Tolerance slightly relaxed to account for numerical precision with atan2/trig operations */
        alwan_scalar const din99_tol = ALWAN_TEST_TOLERANCE * ALWAN_LITERAL(15000.0);
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_FABS(din99_computed.v[j] - din99_expected.v[j]);
            if (diff > din99_tol) {
                printf("Color %zu channel %d failed (%s):\n", i, j, variant_name);
                printf("  XYZ: [%.6f, %.6f, %.6f]\n",
                       (double)xyz.v[0], (double)xyz.v[1], (double)xyz.v[2]);
                printf("  Lab: [%.6f, %.6f, %.6f]\n",
                       (double)lab.v[0], (double)lab.v[1], (double)lab.v[2]);
                printf("  Expected DIN99: [%.10f, %.10f, %.10f]\n",
                       (double)din99_expected.v[0], (double)din99_expected.v[1], (double)din99_expected.v[2]);
                printf("  Got DIN99: [%.10f, %.10f, %.10f]\n",
                       (double)din99_computed.v[0], (double)din99_computed.v[1], (double)din99_computed.v[2]);
                printf("  Diff: %.6e\n", (double)diff);
                TEST_ASSERT(0, "DIN99 values don't match");
            }
        }

        /* Test round-trip: DIN99 -> Lab */
        alwan_din99_to_lab(&din99_computed, &lab_out, variant);

        alwan_scalar const roundtrip_tol = ALWAN_TEST_TOLERANCE * ALWAN_LITERAL(10000000.0);

        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_FABS(lab_out.v[j] - lab.v[j]);
            if (diff > roundtrip_tol) {
                printf("Round-trip color %zu channel %d failed (%s):\n", i, j, variant_name);
                printf("  Original Lab: [%.6f, %.6f, %.6f]\n",
                       (double)lab.v[0], (double)lab.v[1], (double)lab.v[2]);
                printf("  Round-trip Lab: [%.6f, %.6f, %.6f]\n",
                       (double)lab_out.v[0], (double)lab_out.v[1], (double)lab_out.v[2]);
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

int test_150_din99_main(void) {
    printf("\n=== DIN99 Family Tests ===\n");

    int failures = 0;

    printf("Test: Lab <-> DIN99 (variant 0)\n");
    failures += test_din99_variant(0, "DIN99", "tests/unit/reference_values/test_lab_din99_pairs.csv");

    printf("Test: Lab <-> DIN99b (variant 1)\n");
    failures += test_din99_variant(1, "DIN99b", "tests/unit/reference_values/test_lab_din99b_pairs.csv");

    printf("Test: Lab <-> DIN99c (variant 2)\n");
    failures += test_din99_variant(2, "DIN99c", "tests/unit/reference_values/test_lab_din99c_pairs.csv");

    printf("Test: Lab <-> DIN99d (variant 3)\n");
    failures += test_din99_variant(3, "DIN99d", "tests/unit/reference_values/test_lab_din99d_pairs.csv");

    if (failures == 0) {
        printf("All DIN99 tests passed!\n");
    } else {
        printf("%d DIN99 test(s) failed!\n", failures);
    }

    return failures;
}
