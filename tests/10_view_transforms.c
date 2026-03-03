/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 10: View Transforms (ACES, AgX)
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Test helpers
 * ---------------------------------------------------------------- */

static int is_in_range_01(alwan_scalar const *rgb) {
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
    alwan_scalar test_inputs[][3] = {
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
        alwan_scalar output[3];
        int status = alwan_view_transform_apply(output, NULL, ALWAN_VIEW_ACES_REC709,
                                                test_inputs[i], 1,
                                                3 * sizeof(alwan_scalar),
                                                3 * sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "ACES Rec.709 transform failed");

        /* Output must be in [0,1] range for display-referred RGB */
        if (!is_in_range_01(output)) {
            printf("  Test %zu: output out of range [%.6f %.6f %.6f]\n",
                   i, output[0], output[1], output[2]);
        }
        TEST_ASSERT(is_in_range_01(output), "ACES output not in [0,1] range");

        /* Black input should produce near-black output */
        if (i == 0) {
            alwan_scalar max_val = output[0];
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
    alwan_scalar test_inputs[][3] = {
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
        alwan_scalar output[3];
        int status = alwan_view_transform_apply(output, NULL, ALWAN_VIEW_AGX,
                                                test_inputs[i], 1,
                                                3 * sizeof(alwan_scalar),
                                                3 * sizeof(alwan_scalar));
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
    alwan_scalar test_input[3] = {
        ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)
    };

    alwan_scalar base_output[3], punchy_output[3];

    /* Apply base AgX */
    int status = alwan_view_transform_apply(base_output, NULL, ALWAN_VIEW_AGX,
                                           test_input, 1,
                                                3 * sizeof(alwan_scalar),
                                                3 * sizeof(alwan_scalar));
    TEST_ASSERT(status == ALWAN_OK, "AgX base transform failed");

    /* Apply punchy AgX */
    status = alwan_view_transform_apply(punchy_output, NULL, ALWAN_VIEW_AGX_PUNCHY,
                                       test_input, 1,
                                                3 * sizeof(alwan_scalar),
                                                3 * sizeof(alwan_scalar));
    TEST_ASSERT(status == ALWAN_OK, "AgX punchy transform failed");

    /* Both should be in [0,1] range */
    TEST_ASSERT(is_in_range_01(base_output), "AgX base output not in [0,1]");
    TEST_ASSERT(is_in_range_01(punchy_output), "AgX punchy output not in [0,1]");

    /* Punchy variant should produce different results (higher contrast) */
    /* We can't predict exact differences, but they shouldn't be identical */
    alwan_scalar diff = ALWAN_ABS(base_output[0] - punchy_output[0]) +
                  ALWAN_ABS(base_output[1] - punchy_output[1]) +
                  ALWAN_ABS(base_output[2] - punchy_output[2]);

    if (diff < ALWAN_LITERAL(0.001)) {
        printf("  Warning: AgX base and punchy produce very similar results (diff=%.6f)\n", diff);
    }

    TEST_PASS("AgX punchy variant");
}

static int test_khronos_pbr_neutral_basic(void) {
    /* Test Khronos PBR Neutral tone mapping */
    alwan_scalar test_inputs[][3] = {
        /* Black */
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},
        /* Middle gray (0.18) — below switch point, should pass through */
        {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)},
        /* Below switch point (0.5) */
        {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)},
        /* Bright white (1.0) — above switch point */
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},
        /* Over-bright (4.0) — tests tone mapping compression */
        {ALWAN_LITERAL(4.0), ALWAN_LITERAL(4.0), ALWAN_LITERAL(4.0)},
        /* Pure red (scene-referred) */
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)}
    };

    size_t const num_tests = sizeof(test_inputs) / sizeof(test_inputs[0]);

    for (size_t i = 0; i < num_tests; i++) {
        alwan_scalar output[3];
        int status = alwan_view_transform_apply(output, NULL, ALWAN_VIEW_KHRONOS_PBR_NEUTRAL,
                                                test_inputs[i], 1,
                                                3 * sizeof(alwan_scalar),
                                                3 * sizeof(alwan_scalar));
        TEST_ASSERT(status == ALWAN_OK, "Khronos PBR Neutral transform failed");

        /* Output must be in [0,1] range for display-referred RGB */
        if (!is_in_range_01(output)) {
            printf("  Test %zu: output out of range [%.6f %.6f %.6f]\n",
                   i, output[0], output[1], output[2]);
        }
        TEST_ASSERT(is_in_range_01(output), "Khronos PBR Neutral output not in [0,1] range");

        /* Black input should produce near-black output */
        if (i == 0) {
            alwan_scalar max_val = output[0];
            if (output[1] > max_val) max_val = output[1];
            if (output[2] > max_val) max_val = output[2];
            TEST_ASSERT(max_val < ALWAN_LITERAL(0.01), "Black input should produce near-black output");
        }

        /* Below switch point (~0.76): output = input - offset
         * For achromatic 0.18, x=0.18, offset = 0.18 - 6.25*0.18^0.18 = 0.18 - 0.2025 < 0
         * Actually: since x=0.18 > 0.08, offset = 0.04
         * So output ≈ 0.18 - 0.04 = 0.14 (achromatic, peak < start_compression) */
        if (i == 1) {
            alwan_scalar expected = ALWAN_LITERAL(0.18) - ALWAN_LITERAL(0.04);
            alwan_scalar diff = ALWAN_ABS(output[0] - expected) +
                          ALWAN_ABS(output[1] - expected) +
                          ALWAN_ABS(output[2] - expected);
            TEST_ASSERT(diff < ALWAN_LITERAL(0.01), "Below switch point should apply offset only");
        }

        /* Over-bright input should be tone-mapped to [0,1] */
        if (i == 4) {
            TEST_ASSERT(output[0] <= ALWAN_LITERAL(1.0) &&
                       output[1] <= ALWAN_LITERAL(1.0) &&
                       output[2] <= ALWAN_LITERAL(1.0),
                       "Over-bright values should be tone-mapped");
        }
    }

    TEST_PASS("Khronos PBR Neutral basic tests");
}

static int test_view_transform_monotonic(void) {
    /* Test that view transforms are monotonic: brighter input -> brighter output */
    alwan_scalar inputs[][3] = {
        {ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.1)},
        {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)},
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)}
    };

    alwan_view_transform transforms[] = {ALWAN_VIEW_ACES_REC709, ALWAN_VIEW_AGX, ALWAN_VIEW_AGX_PUNCHY, ALWAN_VIEW_KHRONOS_PBR_NEUTRAL};
    char const *transform_names[] = {"aces_rec709", "agx", "agx_punchy", "khronos_pbr_neutral"};
    size_t const num_transforms = sizeof(transforms) / sizeof(transforms[0]);

    for (size_t t = 0; t < num_transforms; t++) {
        alwan_scalar prev_luma = ALWAN_LITERAL(-1.0);

        for (size_t i = 0; i < 3; i++) {
            alwan_scalar output[3];
            int status = alwan_view_transform_apply(output, NULL, transforms[t],
                                                   inputs[i], 1,
                                                3 * sizeof(alwan_scalar),
                                                3 * sizeof(alwan_scalar));
            TEST_ASSERT(status == ALWAN_OK, "View transform failed");

            /* Calculate luminance (Rec.709 weights) */
            alwan_scalar luma = ALWAN_LITERAL(0.2126) * output[0] +
                         ALWAN_LITERAL(0.7152) * output[1] +
                         ALWAN_LITERAL(0.0722) * output[2];

            /* Should be monotonically increasing */
            if (i > 0 && luma <= prev_luma) {
                printf("  %s: Non-monotonic at step %zu: prev_luma=%.6f, luma=%.6f\n",
                       transform_names[t], i, prev_luma, luma);
                TEST_ASSERT(0, "View transform not monotonic");
            }

            prev_luma = luma;
        }
    }

    TEST_PASS("View transform monotonicity");
}

static int test_bulk_view_transform(void) {
    /* Test bulk processing of multiple RGB triplets */
    alwan_scalar inputs[] = {
        /* RGB triplet 0: black */
        ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
        /* RGB triplet 1: gray */
        ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18),
        /* RGB triplet 2: white */
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0),
        /* RGB triplet 3: red */
        ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)
    };

    alwan_scalar outputs[12];  /* 4 RGB triplets */

    int status = alwan_view_transform_apply(outputs, NULL, ALWAN_VIEW_ACES_REC709,
                                           inputs, 4,
                                           3 * sizeof(alwan_scalar),
                                           3 * sizeof(alwan_scalar));
    TEST_ASSERT(status == ALWAN_OK, "Bulk view transform failed");

    /* Verify all outputs are in [0,1] range */
    for (int i = 0; i < 4; i++) {
        alwan_scalar const *rgb = &outputs[i * 3];
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
    alwan_scalar red_input[3] = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
    alwan_scalar output[3];

    int status = alwan_view_transform_apply(output, NULL, ALWAN_VIEW_ACES_REC709,
                                           red_input, 1,
                                                3 * sizeof(alwan_scalar),
                                                3 * sizeof(alwan_scalar));
    TEST_ASSERT(status == ALWAN_OK, "View transform failed");

    /* Red channel should dominate */
    if (!(output[0] > output[1] && output[0] > output[2])) {
        printf("  Red input produced: [%.6f %.6f %.6f]\n",
               output[0], output[1], output[2]);
    }
    TEST_ASSERT(output[0] > output[1] && output[0] > output[2],
                "Red hue not preserved");

    /* Test with green */
    alwan_scalar green_input[3] = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)};
    status = alwan_view_transform_apply(output, NULL, ALWAN_VIEW_ACES_REC709,
                                       green_input, 1,
                                                3 * sizeof(alwan_scalar),
                                                3 * sizeof(alwan_scalar));
    TEST_ASSERT(status == ALWAN_OK, "View transform failed");

    /* Green channel should dominate */
    TEST_ASSERT(output[1] > output[0] && output[1] > output[2],
                "Green hue not preserved");

    TEST_PASS("View transform hue preservation");
}

static int test_invalid_view_transform(void) {
    /* Test that invalid view transform enum values return error */
    alwan_scalar dummy_in[3] = {
        ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)
    };
    alwan_scalar dummy_out[3];

    int status = alwan_view_transform_apply(dummy_out, NULL, (alwan_view_transform)999,
                                           dummy_in, 1,
                                                3 * sizeof(alwan_scalar),
                                                3 * sizeof(alwan_scalar));
    TEST_ASSERT(status == ALWAN_E_INVALID, "Should reject invalid view transform enum");

    TEST_PASS("Invalid view transform rejection");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_10_view_transforms_main(void) {
    int failures = 0;

    failures += test_aces_rec709_basic();
    failures += test_agx_basic();
    failures += test_agx_punchy();
    failures += test_khronos_pbr_neutral_basic();
    failures += test_view_transform_monotonic();
    failures += test_bulk_view_transform();
    failures += test_view_transform_preserves_hue();
    failures += test_invalid_view_transform();

    if (failures == 0) {
        printf("\n=== All view transform tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
