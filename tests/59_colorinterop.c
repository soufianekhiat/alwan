/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 59: ColorInterop compliance tests
 * - Display color space descriptors (4 new spaces)
 * - Transfer function assignments
 * - Integer-to-float normalization round-trips
 * - Backward compatibility alias
 */

#include "test_common.h"

/* ----------------------------------------------------------------
 * Test: New display space descriptors
 * ---------------------------------------------------------------- */

static int test_display_space_descriptors(void) {
    TEST_START("Display space descriptors");

    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    struct {
        alwan_rgb_space space;
        char const *name;
        alwan_transfer_function expected_oetf;
        alwan_transfer_function expected_eotf;
    } display_spaces[] = {
        { ALWAN_RGB_SPACE_REC1886_REC709,  "REC1886_REC709",  ALWAN_TF_BT709, ALWAN_TF_BT1886 },
        { ALWAN_RGB_SPACE_REC2100_PQ,      "REC2100_PQ",      ALWAN_TF_PQ,    ALWAN_TF_PQ     },
        { ALWAN_RGB_SPACE_REC2100_HLG,     "REC2100_HLG",     ALWAN_TF_HLG,   ALWAN_TF_HLG    },
        { ALWAN_RGB_SPACE_DISPLAY_P3_HDR,  "DISPLAY_P3_HDR",  ALWAN_TF_PQ,    ALWAN_TF_PQ     },
    };

    for (size_t i = 0; i < sizeof(display_spaces) / sizeof(display_spaces[0]); i++) {
        alwan_rgb_space_desc desc;
        int status = alwan_rgb_get_space_descriptor(&desc, ctx, display_spaces[i].space);

        if (status != ALWAN_OK) {
            printf("[FAIL] Failed to get %s descriptor (status=%d)\n",
                   display_spaces[i].name, status);
            alwan_destroy(ctx);
            return 1;
        }

        /* Verify primaries are non-zero (valid data loaded) */
        if (desc.primaries_xy[0] < ALWAN_EPSILON && desc.primaries_xy[1] < ALWAN_EPSILON) {
            printf("[FAIL] %s primaries are zero\n", display_spaces[i].name);
            alwan_destroy(ctx);
            return 1;
        }

        /* Verify OETF/EOTF match expected */
        if (desc.oetf != display_spaces[i].expected_oetf) {
            printf("[FAIL] %s OETF: expected %d, got %d\n",
                   display_spaces[i].name,
                   (int)display_spaces[i].expected_oetf, (int)desc.oetf);
            alwan_destroy(ctx);
            return 1;
        }
        if (desc.eotf != display_spaces[i].expected_eotf) {
            printf("[FAIL] %s EOTF: expected %d, got %d\n",
                   display_spaces[i].name,
                   (int)display_spaces[i].expected_eotf, (int)desc.eotf);
            alwan_destroy(ctx);
            return 1;
        }

        printf("  %s: OETF=%d EOTF=%d OK\n", display_spaces[i].name,
               (int)desc.oetf, (int)desc.eotf);
    }

    alwan_destroy(ctx);
    TEST_PASS("Display space descriptors");
}

/* ----------------------------------------------------------------
 * Test: TF assignment spot-checks for existing spaces
 * ---------------------------------------------------------------- */

static int test_tf_assignment(void) {
    TEST_START("TF assignment spot-checks");

    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    struct {
        alwan_rgb_space space;
        char const *name;
        alwan_transfer_function expected_oetf;
        alwan_transfer_function expected_eotf;
    } checks[] = {
        { ALWAN_RGB_SPACE_SRGB,         "sRGB",      ALWAN_TF_SRGB,    ALWAN_TF_SRGB    },
        { ALWAN_RGB_SPACE_BT709,        "BT.709",    ALWAN_TF_BT709,   ALWAN_TF_BT709   },
        { ALWAN_RGB_SPACE_BT2020,       "BT.2020",   ALWAN_TF_BT2020,  ALWAN_TF_BT2020  },
        { ALWAN_RGB_SPACE_ACES2065_1,   "ACES2065-1",ALWAN_TF_LINEAR,  ALWAN_TF_LINEAR  },
        { ALWAN_RGB_SPACE_ARRI_LOGC3,   "LogC3",     ALWAN_TF_LOGC3,   ALWAN_TF_LOGC3   },
        { ALWAN_RGB_SPACE_DISPLAY_P3,   "Display P3",ALWAN_TF_SRGB,    ALWAN_TF_SRGB    },
        { ALWAN_RGB_SPACE_ACESCG,       "ACEScg",    ALWAN_TF_LINEAR,  ALWAN_TF_LINEAR  },
        { ALWAN_RGB_SPACE_LINEAR_REC709,"Linear Rec.709", ALWAN_TF_LINEAR, ALWAN_TF_LINEAR },
        { ALWAN_RGB_SPACE_GAMMA22_REC709,"Gamma2.2 Rec.709", ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22 },
    };

    for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
        alwan_rgb_space_desc desc;
        int status = alwan_rgb_get_space_descriptor(&desc, ctx, checks[i].space);

        if (status != ALWAN_OK) {
            printf("[FAIL] Failed to get %s descriptor (status=%d)\n",
                   checks[i].name, status);
            alwan_destroy(ctx);
            return 1;
        }

        if (desc.oetf != checks[i].expected_oetf || desc.eotf != checks[i].expected_eotf) {
            printf("[FAIL] %s TF mismatch: expected {%d,%d}, got {%d,%d}\n",
                   checks[i].name,
                   (int)checks[i].expected_oetf, (int)checks[i].expected_eotf,
                   (int)desc.oetf, (int)desc.eotf);
            alwan_destroy(ctx);
            return 1;
        }
    }

    alwan_destroy(ctx);
    TEST_PASS("TF assignment spot-checks");
}

/* ----------------------------------------------------------------
 * Test: Integer normalization round-trip
 * ---------------------------------------------------------------- */

static int test_normalization_roundtrip(void) {
    TEST_START("Normalization round-trip");

    int bit_depths[] = { 8, 10, 12, 16 };
    alwan_uint16 max_vals[] = { 255, 1023, 4095, 65535 };

    for (int d = 0; d < 4; d++) {
        int bd = bit_depths[d];
        alwan_uint16 max_v = max_vals[d];

        /* Test 0 -> 0.0 */
        {
            alwan_uint16 in_val = 0;
            alwan_f64 out_f;
            int status = alwan_uint_to_float(&out_f, &in_val, bd, 1);
            TEST_ASSERT(status == ALWAN_OK, "uint_to_float failed");
            TEST_ASSERT_NEAR(out_f, ALWAN_ZERO, ALWAN_EPSILON, "0 -> 0.0");
        }

        /* Test max -> 1.0 */
        {
            alwan_uint16 in_val = max_v;
            alwan_f64 out_f;
            int status = alwan_uint_to_float(&out_f, &in_val, bd, 1);
            TEST_ASSERT(status == ALWAN_OK, "uint_to_float failed");
            TEST_ASSERT_NEAR(out_f, ALWAN_ONE, ALWAN_EPSILON, "max -> 1.0");
        }

        /* Round-trip: uint -> float -> uint */
        {
            alwan_uint16 in_vals[] = { 0, 1, (alwan_uint16)(max_v / 2), (alwan_uint16)(max_v - 1), max_v };
            size_t n = sizeof(in_vals) / sizeof(in_vals[0]);
            alwan_f64 floats[5];
            alwan_uint16 out_vals[5];

            int s1 = alwan_uint_to_float(floats, in_vals, bd, n);
            TEST_ASSERT(s1 == ALWAN_OK, "uint_to_float batch failed");

            int s2 = alwan_float_to_uint(out_vals, floats, bd, n);
            TEST_ASSERT(s2 == ALWAN_OK, "float_to_uint batch failed");

            for (size_t i = 0; i < n; i++) {
                if (out_vals[i] != in_vals[i]) {
                    printf("[FAIL] round-trip bd=%d: in=%u, out=%u\n",
                           bd, (unsigned)in_vals[i], (unsigned)out_vals[i]);
                    return 1;
                }
            }
        }
    }

    TEST_PASS("Normalization round-trip");
}

/* ----------------------------------------------------------------
 * Test: Normalization boundary values
 * ---------------------------------------------------------------- */

static int test_normalization_values(void) {
    TEST_START("Normalization values");

    /* 8-bit: 128/255 != 0.5 (verify it's not naive /256) */
    {
        alwan_uint16 in_val = 128;
        alwan_f64 out_f;
        alwan_uint_to_float(&out_f, &in_val, 8, 1);
        alwan_f64 expected = ALWAN_LITERAL(128.0) / ALWAN_LITERAL(255.0);
        TEST_ASSERT_NEAR(out_f, expected, ALWAN_EPSILON, "128/255");
        /* Must NOT equal 0.5 */
        alwan_f64 half = ALWAN_LITERAL(0.5);
        alwan_f64 diff = ALWAN_ABS(out_f - half);
        TEST_ASSERT(diff > ALWAN_LITERAL(0.001), "128/255 must not equal 0.5");
    }

    /* Clamping: values > 1.0 clamp to max */
    {
        alwan_f64 over = ALWAN_LITERAL(1.5);
        alwan_uint16 out_val;
        alwan_float_to_uint(&out_val, &over, 8, 1);
        TEST_ASSERT(out_val == 255, "1.5 -> 255 (clamped)");
    }

    /* Clamping: values < 0.0 clamp to 0 */
    {
        alwan_f64 under = ALWAN_LITERAL(-0.5);
        alwan_uint16 out_val;
        alwan_float_to_uint(&out_val, &under, 8, 1);
        TEST_ASSERT(out_val == 0, "-0.5 -> 0 (clamped)");
    }

    /* Invalid bit depth */
    {
        alwan_uint16 in_val = 0;
        alwan_f64 out_f;
        int status = alwan_uint_to_float(&out_f, &in_val, 7, 1);
        TEST_ASSERT(status == ALWAN_E_INVALID, "bit_depth=7 should fail");
    }

    TEST_PASS("Normalization values");
}

/* ----------------------------------------------------------------
 * Test: Backward compatibility alias
 * ---------------------------------------------------------------- */

static int test_backward_compat_alias(void) {
    TEST_START("Backward compat alias");

    /* ALWAN_RGB_SPACE_LINEAR_SRGB must still compile and equal LINEAR_REC709 */
    alwan_rgb_space s1 = ALWAN_RGB_SPACE_LINEAR_SRGB;
    alwan_rgb_space s2 = ALWAN_RGB_SPACE_LINEAR_REC709;
    TEST_ASSERT(s1 == s2, "LINEAR_SRGB alias must equal LINEAR_REC709");

    /* Verify descriptor can be retrieved via the alias */
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");
    alwan_rgb_space_desc desc;
    int status = alwan_rgb_get_space_descriptor(&desc, ctx, ALWAN_RGB_SPACE_LINEAR_SRGB);
    TEST_ASSERT(status == ALWAN_OK, "Descriptor via alias should succeed");
    alwan_destroy(ctx);

    TEST_PASS("Backward compat alias");
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

int test_59_colorinterop_main(void) {
    printf("Test 59: ColorInterop compliance\n");

    int failed = 0;
    failed += test_display_space_descriptors();
    failed += test_tf_assignment();
    failed += test_normalization_roundtrip();
    failed += test_normalization_values();
    failed += test_backward_compat_alias();

    printf("  Results: %d passed, %d failed\n", test_passed, test_failed);
    return failed;
}
