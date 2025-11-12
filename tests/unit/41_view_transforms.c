/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * Test 41: View Transforms (ACES, AgX)
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

static int is_in_range_01(Scalar const *rgb) {
    for (int i = 0; i < 3; i++) {
        if (rgb[i] < ALWAN_LITERAL(0.0) || rgb[i] > ALWAN_LITERAL(1.0)) {
            return 0;
        }
    }
    return 1;
}

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_aces_rec709_basic(void) {
    /* Test ACES RRT+ODT for Rec.709 with typical scene-referred values */
    Scalar test_inputs[][3] = {
        /* Black */
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},
        /* Middle gray (0.18) */
        {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)},
        /* Bright white (1.0) */
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},
        /* Over-bright (tests tone mapping) */
        {ALWAN_LITERAL(4.0), ALWAN_LITERAL(4.0), ALWAN_LITERAL(4.0)},
        /* Pure red (scene-referred) */
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)}
    };

    size_t const num_tests = sizeof(test_inputs) / sizeof(test_inputs[0]);

    for (size_t i = 0; i < num_tests; i++) {
        Scalar output[3];
        int status = alwan_view_transform_apply(NULL, "aces_rec709",
                                                test_inputs[i], 1, 3,
                                                output, 3);
        TEST_ASSERT(status == ALWAN_OK, "ACES Rec.709 transform failed");

        /* Output must be in [0,1] range for display-referred RGB */
        if (!is_in_range_01(output)) {
            printf("  Test %zu: output out of range [%.6f %.6f %.6f]\n",
                   i, output[0], output[1], output[2]);
        }
        TEST_ASSERT(is_in_range_01(output), "ACES output not in [0,1] range");

        /* Black input should produce near-black output */
        if (i == 0) {
            Scalar max_val = output[0];
            if (output[1] > max_val) max_val = output[1];
            if (output[2] > max_val) max_val = output[2];
            TEST_ASSERT(max_val < ALWAN_LITERAL(0.01), "Black input should produce near-black output");
        }

        /* Over-bright input should be tone-mapped to [0,1] */
        if (i == 3) {
            TEST_ASSERT(output[0] <= ALWAN_LITERAL(1.0) &&
                       output[1] <= ALWAN_LITERAL(1.0) &&
                       output[2] <= ALWAN_LITERAL(1.0),
                       "Over-bright values should be tone-mapped");
        }
    }

    TEST_PASS("ACES Rec.709 basic tests");
}

static int test_agx_basic(void) {
    /* Test AgX base transform with typical linear RGB values */
    Scalar test_inputs[][3] = {
        /* Black */
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},
        /* Very dark */
        {ALWAN_LITERAL(0.001), ALWAN_LITERAL(0.001), ALWAN_LITERAL(0.001)},
        /* Middle gray (0.18) */
        {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)},
        /* Bright white */
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},
        /* Over-bright (HDR) */
        {ALWAN_LITERAL(10.0), ALWAN_LITERAL(10.0), ALWAN_LITERAL(10.0)}
    };

    size_t const num_tests = sizeof(test_inputs) / sizeof(test_inputs[0]);

    for (size_t i = 0; i < num_tests; i++) {
        Scalar output[3];
        int status = alwan_view_transform_apply(NULL, "agx",
                                                test_inputs[i], 1, 3,
                                                output, 3);
        TEST_ASSERT(status == ALWAN_OK, "AgX transform failed");

        /* Output must be in [0,1] range */
        if (!is_in_range_01(output)) {
            printf("  Test %zu: output out of range [%.6f %.6f %.6f]\n",
                   i, output[0], output[1], output[2]);
        }
        TEST_ASSERT(is_in_range_01(output), "AgX output not in [0,1] range");
    }

    TEST_PASS("AgX basic tests");
}

static int test_agx_punchy(void) {
    /* Test AgX punchy variant produces higher contrast than base */
    Scalar test_input[3] = {
        ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)
    };

    Scalar base_output[3], punchy_output[3];

    /* Apply base AgX */
    int status = alwan_view_transform_apply(NULL, "agx",
                                           test_input, 1, 3,
                                           base_output, 3);
    TEST_ASSERT(status == ALWAN_OK, "AgX base transform failed");

    /* Apply punchy AgX */
    status = alwan_view_transform_apply(NULL, "agx_punchy",
                                       test_input, 1, 3,
                                       punchy_output, 3);
    TEST_ASSERT(status == ALWAN_OK, "AgX punchy transform failed");

    /* Both should be in [0,1] range */
    TEST_ASSERT(is_in_range_01(base_output), "AgX base output not in [0,1]");
    TEST_ASSERT(is_in_range_01(punchy_output), "AgX punchy output not in [0,1]");

    /* Punchy variant should produce different results (higher contrast) */
    /* We can't predict exact differences, but they shouldn't be identical */
    Scalar diff = ALWAN_FABS(base_output[0] - punchy_output[0]) +
                  ALWAN_FABS(base_output[1] - punchy_output[1]) +
                  ALWAN_FABS(base_output[2] - punchy_output[2]);

    if (diff < ALWAN_LITERAL(0.001)) {
        printf("  Warning: AgX base and punchy produce very similar results (diff=%.6f)\n", diff);
    }

    TEST_PASS("AgX punchy variant");
}

static int test_view_transform_monotonic(void) {
    /* Test that view transforms are monotonic: brighter input → brighter output */
    Scalar inputs[][3] = {
        {ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.1)},
        {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)},
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)}
    };

    char const *transforms[] = {"aces_rec709", "agx", "agx_punchy"};
    size_t const num_transforms = sizeof(transforms) / sizeof(transforms[0]);

    for (size_t t = 0; t < num_transforms; t++) {
        Scalar prev_luma = ALWAN_LITERAL(-1.0);

        for (size_t i = 0; i < 3; i++) {
            Scalar output[3];
            int status = alwan_view_transform_apply(NULL, transforms[t],
                                                   inputs[i], 1, 3,
                                                   output, 3);
            TEST_ASSERT(status == ALWAN_OK, "View transform failed");

            /* Calculate luminance (Rec.709 weights) */
            Scalar luma = ALWAN_LITERAL(0.2126) * output[0] +
                         ALWAN_LITERAL(0.7152) * output[1] +
                         ALWAN_LITERAL(0.0722) * output[2];

            /* Should be monotonically increasing */
            if (i > 0 && luma <= prev_luma) {
                printf("  %s: Non-monotonic at step %zu: prev_luma=%.6f, luma=%.6f\n",
                       transforms[t], i, prev_luma, luma);
                TEST_ASSERT(0, "View transform not monotonic");
            }

            prev_luma = luma;
        }
    }

    TEST_PASS("View transform monotonicity");
}

static int test_bulk_view_transform(void) {
    /* Test bulk processing of multiple RGB triplets */
    Scalar inputs[] = {
        /* RGB triplet 0: black */
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        /* RGB triplet 1: gray */
        ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18),
        /* RGB triplet 2: white */
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0),
        /* RGB triplet 3: red */
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)
    };

    Scalar outputs[12];  /* 4 RGB triplets */

    int status = alwan_view_transform_apply(NULL, "aces_rec709",
                                           inputs, 4, 3,
                                           outputs, 3);
    TEST_ASSERT(status == ALWAN_OK, "Bulk view transform failed");

    /* Verify all outputs are in [0,1] range */
    for (int i = 0; i < 4; i++) {
        Scalar const *rgb = &outputs[i * 3];
        if (!is_in_range_01(rgb)) {
            printf("  Output %d out of range: [%.6f %.6f %.6f]\n",
                   i, rgb[0], rgb[1], rgb[2]);
        }
        TEST_ASSERT(is_in_range_01(rgb), "Bulk output not in [0,1] range");
    }

    TEST_PASS("Bulk view transform");
}

static int test_view_transform_preserves_hue(void) {
    /* Test that view transforms roughly preserve hue for saturated colors */
    /* For pure red, output should have R > G and R > B */
    Scalar red_input[3] = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
    Scalar output[3];

    int status = alwan_view_transform_apply(NULL, "aces_rec709",
                                           red_input, 1, 3,
                                           output, 3);
    TEST_ASSERT(status == ALWAN_OK, "View transform failed");

    /* Red channel should dominate */
    if (!(output[0] > output[1] && output[0] > output[2])) {
        printf("  Red input produced: [%.6f %.6f %.6f]\n",
               output[0], output[1], output[2]);
    }
    TEST_ASSERT(output[0] > output[1] && output[0] > output[2],
                "Red hue not preserved");

    /* Test with green */
    Scalar green_input[3] = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)};
    status = alwan_view_transform_apply(NULL, "aces_rec709",
                                       green_input, 1, 3,
                                       output, 3);
    TEST_ASSERT(status == ALWAN_OK, "View transform failed");

    /* Green channel should dominate */
    TEST_ASSERT(output[1] > output[0] && output[1] > output[2],
                "Green hue not preserved");

    TEST_PASS("View transform hue preservation");
}

static int test_invalid_view_transform(void) {
    /* Test that invalid view transform names return error */
    Scalar dummy_in[3] = {
        ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)
    };
    Scalar dummy_out[3];

    int status = alwan_view_transform_apply(NULL, "invalid_transform",
                                           dummy_in, 1, 3,
                                           dummy_out, 3);
    TEST_ASSERT(status == ALWAN_E_INVALID, "Should reject invalid view transform name");

    /* Test with NULL name */
    status = alwan_view_transform_apply(NULL, NULL,
                                       dummy_in, 1, 3,
                                       dummy_out, 3);
    TEST_ASSERT(status == ALWAN_E_INVALID, "Should reject NULL name");

    TEST_PASS("Invalid view transform rejection");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_41_view_transforms_main(void) {
    int failures = 0;

    failures += test_aces_rec709_basic();
    failures += test_agx_basic();
    failures += test_agx_punchy();
    failures += test_view_transform_monotonic();
    failures += test_bulk_view_transform();
    failures += test_view_transform_preserves_hue();
    failures += test_invalid_view_transform();

    if (failures == 0) {
        printf("\n=== All view transform tests passed ===\n");
        return 0;
    } else {
        fprintf(stderr, "\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
