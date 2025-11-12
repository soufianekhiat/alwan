/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * Test 31: Chromatic Adaptation round-trip tests
 */

#include "../../src/alwan/alwan.h"
#include "../../src/alwan/alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Test helpers
 * ---------------------------------------------------------------- */

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } \
} while(0)

#define TEST_PASS(name) do { \
    printf("[PASS] %s\n", name); \
    return 0; \
} while(0)

static Scalar vec3_max_diff(alwan_vec3 const *a, alwan_vec3 const *b) {
    Scalar max_diff = 0;
    for (int i = 0; i < 3; i++) {
        Scalar diff = ALWAN_FABS(a->v[i] - b->v[i]);
        if (diff > max_diff) max_diff = diff;
    }
    return max_diff;
}

static void vec3_print(char const *name, alwan_vec3 const *v) {
    printf("%s: [%12.8f %12.8f %12.8f]\n", name, v->v[0], v->v[1], v->v[2]);
}

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_adapt_d65_to_d50_bradford(void) {
    /* Load white points and test colors */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static Scalar const d65_xyz_data[] = {
#include "../../data/fixtures/m3_d65_xyz.csv"
    };
    static Scalar const d50_xyz_data[] = {
#include "../../data/fixtures/m3_d50_xyz.csv"
    };
    static Scalar const test_colors_data[] = {
#include "../../data/fixtures/m3_test_xyz_colors.csv"
    };
    static Scalar const expected_adapted_data[] = {
#include "../../data/fixtures/m3_adapted_d65_to_d50_bradford.csv"
    };
    ALWAN_DIAG_POP

    alwan_vec3 d65_xyz = {{d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]}};
    alwan_vec3 d50_xyz = {{d50_xyz_data[0], d50_xyz_data[1], d50_xyz_data[2]}};

    int const num_tests = sizeof(test_colors_data) / (3 * sizeof(Scalar));
#if ALWAN_SCALAR_IS_FLOAT
    Scalar const tolerance = ALWAN_LITERAL(5e-5);
#else
    Scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 input_xyz = {{test_colors_data[i * 3 + 0],
                                  test_colors_data[i * 3 + 1],
                                  test_colors_data[i * 3 + 2]}};
        alwan_vec3 expected = {{expected_adapted_data[i * 3 + 0],
                                 expected_adapted_data[i * 3 + 1],
                                 expected_adapted_data[i * 3 + 2]}};

        /* Adapt using bulk function */
        alwan_vec3 adapted;
        int status = alwan_xyz_adapt(input_xyz.v, 1, 3,
                                     &d65_xyz, &d50_xyz,
                                     ALWAN_CAT_BRADFORD,
                                     adapted.v, 3);
        TEST_ASSERT(status == ALWAN_OK, "Adaptation failed");

        Scalar diff = vec3_max_diff(&adapted, &expected);
        if (diff >= tolerance) {
            printf("Test %d: diff=%e (tol=%e)\n", i, diff, tolerance);
            vec3_print("  Input", &input_xyz);
            vec3_print("  Computed", &adapted);
            vec3_print("  Expected", &expected);
        }
        TEST_ASSERT(diff < tolerance, "D65→D50 adaptation mismatch");
    }

    TEST_PASS("Adapt D65→D50 (Bradford)");
}

static int test_adapt_a_to_d65_bradford(void) {
    /* Load white points and test colors */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static Scalar const d65_xyz_data[] = {
#include "../../data/fixtures/m3_d65_xyz.csv"
    };
    static Scalar const a_xyz_data[] = {
#include "../../data/fixtures/m3_a_xyz.csv"
    };
    static Scalar const test_colors_data[] = {
#include "../../data/fixtures/m3_test_xyz_colors.csv"
    };
    static Scalar const expected_adapted_data[] = {
#include "../../data/fixtures/m3_adapted_a_to_d65_bradford.csv"
    };
    ALWAN_DIAG_POP

    alwan_vec3 d65_xyz = {{d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]}};
    alwan_vec3 a_xyz = {{a_xyz_data[0], a_xyz_data[1], a_xyz_data[2]}};

    int const num_tests = sizeof(test_colors_data) / (3 * sizeof(Scalar));
#if ALWAN_SCALAR_IS_FLOAT
    Scalar const tolerance = ALWAN_LITERAL(5e-5);
#else
    Scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 input_xyz = {{test_colors_data[i * 3 + 0],
                                  test_colors_data[i * 3 + 1],
                                  test_colors_data[i * 3 + 2]}};
        alwan_vec3 expected = {{expected_adapted_data[i * 3 + 0],
                                 expected_adapted_data[i * 3 + 1],
                                 expected_adapted_data[i * 3 + 2]}};

        /* Adapt using bulk function */
        alwan_vec3 adapted;
        int status = alwan_xyz_adapt(input_xyz.v, 1, 3,
                                     &a_xyz, &d65_xyz,
                                     ALWAN_CAT_BRADFORD,
                                     adapted.v, 3);
        TEST_ASSERT(status == ALWAN_OK, "Adaptation failed");

        Scalar diff = vec3_max_diff(&adapted, &expected);
        TEST_ASSERT(diff < tolerance, "A→D65 adaptation mismatch");
    }

    TEST_PASS("Adapt A→D65 (Bradford)");
}

static int test_roundtrip_d65_d50_d65(void) {
    /* Load white points */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static Scalar const d65_xyz_data[] = {
#include "../../data/fixtures/m3_d65_xyz.csv"
    };
    static Scalar const d50_xyz_data[] = {
#include "../../data/fixtures/m3_d50_xyz.csv"
    };
    static Scalar const test_colors_data[] = {
#include "../../data/fixtures/m3_test_xyz_colors.csv"
    };
    ALWAN_DIAG_POP

    alwan_vec3 d65_xyz = {{d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]}};
    alwan_vec3 d50_xyz = {{d50_xyz_data[0], d50_xyz_data[1], d50_xyz_data[2]}};

    int const num_tests = sizeof(test_colors_data) / (3 * sizeof(Scalar));
#if ALWAN_SCALAR_IS_FLOAT
    Scalar const tolerance = ALWAN_LITERAL(5e-5);
#else
    Scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 original = {{test_colors_data[i * 3 + 0],
                                 test_colors_data[i * 3 + 1],
                                 test_colors_data[i * 3 + 2]}};

        /* D65 → D50 */
        alwan_vec3 adapted_to_d50;
        int status = alwan_xyz_adapt(original.v, 1, 3,
                                     &d65_xyz, &d50_xyz,
                                     ALWAN_CAT_BRADFORD,
                                     adapted_to_d50.v, 3);
        TEST_ASSERT(status == ALWAN_OK, "D65→D50 adaptation failed");

        /* D50 → D65 (back) */
        alwan_vec3 roundtrip;
        status = alwan_xyz_adapt(adapted_to_d50.v, 1, 3,
                                &d50_xyz, &d65_xyz,
                                ALWAN_CAT_BRADFORD,
                                roundtrip.v, 3);
        TEST_ASSERT(status == ALWAN_OK, "D50→D65 adaptation failed");

        Scalar diff = vec3_max_diff(&original, &roundtrip);
        if (diff >= tolerance) {
            printf("Round-trip test %d: diff=%e (tol=%e)\n", i, diff, tolerance);
            vec3_print("  Original", &original);
            vec3_print("  Round-trip", &roundtrip);
        }
        TEST_ASSERT(diff < tolerance, "D65→D50→D65 round-trip mismatch");
    }

    TEST_PASS("Round-trip D65→D50→D65 (Bradford)");
}

static int test_roundtrip_all_methods(void) {
    /* Load white points */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static Scalar const d65_xyz_data[] = {
#include "../../data/fixtures/m3_d65_xyz.csv"
    };
    static Scalar const d50_xyz_data[] = {
#include "../../data/fixtures/m3_d50_xyz.csv"
    };
    ALWAN_DIAG_POP

    alwan_vec3 d65_xyz = {{d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]}};
    alwan_vec3 d50_xyz = {{d50_xyz_data[0], d50_xyz_data[1], d50_xyz_data[2]}};

    /* Test color: sRGB red in D65 */
    alwan_vec3 original = {{ALWAN_LITERAL(0.412456), ALWAN_LITERAL(0.212673), ALWAN_LITERAL(0.019334)}};

#if ALWAN_SCALAR_IS_FLOAT
    Scalar const tolerance = ALWAN_LITERAL(5e-5);
#else
    Scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    /* Test all methods */
    alwan_cat_method methods[] = {
        ALWAN_CAT_BRADFORD,
        ALWAN_CAT_CAT02,
        ALWAN_CAT_CAT16,
        ALWAN_CAT_XYZ_SCALING
    };
    char const *method_names[] = {
        "Bradford",
        "CAT02",
        "CAT16",
        "XYZ Scaling"
    };

    for (int m = 0; m < 4; m++) {
        /* Forward */
        alwan_vec3 adapted;
        int status = alwan_xyz_adapt(original.v, 1, 3,
                                     &d65_xyz, &d50_xyz,
                                     methods[m],
                                     adapted.v, 3);
        TEST_ASSERT(status == ALWAN_OK, "Forward adaptation failed");

        /* Backward */
        alwan_vec3 roundtrip;
        status = alwan_xyz_adapt(adapted.v, 1, 3,
                                &d50_xyz, &d65_xyz,
                                methods[m],
                                roundtrip.v, 3);
        TEST_ASSERT(status == ALWAN_OK, "Backward adaptation failed");

        Scalar diff = vec3_max_diff(&original, &roundtrip);
        if (diff >= tolerance) {
            printf("Round-trip %s: diff=%e (tol=%e)\n", method_names[m], diff, tolerance);
            vec3_print("  Original", &original);
            vec3_print("  Round-trip", &roundtrip);
        }
        TEST_ASSERT(diff < tolerance, "Round-trip mismatch");
    }

    TEST_PASS("Round-trip all methods");
}

static int test_bulk_adaptation(void) {
    /* Load white points */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static Scalar const d65_xyz_data[] = {
#include "../../data/fixtures/m3_d65_xyz.csv"
    };
    static Scalar const d50_xyz_data[] = {
#include "../../data/fixtures/m3_d50_xyz.csv"
    };
    static Scalar const test_colors_data[] = {
#include "../../data/fixtures/m3_test_xyz_colors.csv"
    };
    static Scalar const expected_adapted_data[] = {
#include "../../data/fixtures/m3_adapted_d65_to_d50_bradford.csv"
    };
    ALWAN_DIAG_POP

    alwan_vec3 d65_xyz = {{d65_xyz_data[0], d65_xyz_data[1], d65_xyz_data[2]}};
    alwan_vec3 d50_xyz = {{d50_xyz_data[0], d50_xyz_data[1], d50_xyz_data[2]}};

    int const num_tests = sizeof(test_colors_data) / (3 * sizeof(Scalar));

    /* Adapt all colors at once */
    Scalar adapted_data[15];  /* 5 colors * 3 components */
    int status = alwan_xyz_adapt(test_colors_data, num_tests, 3,
                                 &d65_xyz, &d50_xyz,
                                 ALWAN_CAT_BRADFORD,
                                 adapted_data, 3);
    TEST_ASSERT(status == ALWAN_OK, "Bulk adaptation failed");

#if ALWAN_SCALAR_IS_FLOAT
    Scalar const tolerance = ALWAN_LITERAL(5e-5);
#else
    Scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    /* Verify all results */
    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 computed = {{adapted_data[i * 3 + 0],
                                 adapted_data[i * 3 + 1],
                                 adapted_data[i * 3 + 2]}};
        alwan_vec3 expected = {{expected_adapted_data[i * 3 + 0],
                                 expected_adapted_data[i * 3 + 1],
                                 expected_adapted_data[i * 3 + 2]}};

        Scalar diff = vec3_max_diff(&computed, &expected);
        TEST_ASSERT(diff < tolerance, "Bulk adaptation result mismatch");
    }

    TEST_PASS("Bulk adaptation");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_31_cat_roundtrip_main(void) {
    int failures = 0;

    failures += test_adapt_d65_to_d50_bradford();
    failures += test_adapt_a_to_d65_bradford();
    failures += test_roundtrip_d65_d50_d65();
    failures += test_roundtrip_all_methods();
    failures += test_bulk_adaptation();

    if (failures == 0) {
        printf("\n=== All CAT round-trip tests passed ===\n");
        return 0;
    } else {
        fprintf(stderr, "\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
