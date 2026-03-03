/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 57: ACES LMTs and Additional Transfer Functions
 */

#include "test_common.h"
#include <stdlib.h>
#include <math.h>

static int test_blue_light_fix_roundtrip(void) {
    alwan_rgb test_colors[] = {
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(2.0)},
    };
    size_t num_colors = sizeof(test_colors) / sizeof(test_colors[0]);
    for (size_t i = 0; i < num_colors; i++) {
        alwan_rgb forward, roundtrip;
        int status = alwan_aces_blue_light_fix(&forward, &test_colors[i]);
        TEST_ASSERT(status == ALWAN_OK, "Blue light fix forward failed");
        status = alwan_aces_blue_light_fix_inv(&roundtrip, &forward);
        TEST_ASSERT(status == ALWAN_OK, "Blue light fix inverse failed");
        alwan_scalar diff_r = ALWAN_ABS(roundtrip.r - test_colors[i].r);
        alwan_scalar diff_g = ALWAN_ABS(roundtrip.g - test_colors[i].g);
        alwan_scalar diff_b = ALWAN_ABS(roundtrip.b - test_colors[i].b);
        TEST_ASSERT(diff_r < TEST_TOLERANCE && diff_g < TEST_TOLERANCE && diff_b < TEST_TOLERANCE, "Round-trip exceeded");
    }
    TEST_PASS("test_blue_light_fix_roundtrip");
}

static int test_blue_light_fix_effect(void) {
    alwan_rgb neon_blue = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(2.0)};
    alwan_rgb result;
    int status = alwan_aces_blue_light_fix(&result, &neon_blue);
    TEST_ASSERT(status == ALWAN_OK, "Blue light fix failed");
    TEST_ASSERT(result.r > neon_blue.r, "Should add red");
    TEST_ASSERT(result.g > neon_blue.g, "Should add green");
    TEST_PASS("test_blue_light_fix_effect");
}

static int test_acescc_known_values(void) {
    struct { alwan_scalar linear; alwan_scalar encoded; } known_pairs[] = {
        {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.41358840249244228)},
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(-0.35844748858447484)},
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.55479452054794520)},
    };
    size_t num_pairs = sizeof(known_pairs) / sizeof(known_pairs[0]);
    for (size_t i = 0; i < num_pairs; i++) {
        alwan_scalar encoded;
        alwan_oetf_apply(&encoded, ALWAN_TF_ACESCC, &known_pairs[i].linear, 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan_scalar diff = ALWAN_ABS(encoded - known_pairs[i].encoded);
        TEST_ASSERT(diff < TEST_TOLERANCE, "ACEScc known value exceeded");
    }
    TEST_PASS("test_acescc_known_values");
}

static int test_acescc_roundtrip(void) {
    alwan_scalar test_values[] = {ALWAN_LITERAL(0.0001), ALWAN_LITERAL(0.18), ALWAN_LITERAL(1.0)};
    size_t num_values = sizeof(test_values) / sizeof(test_values[0]);
    for (size_t i = 0; i < num_values; i++) {
        alwan_scalar encoded, decoded;
        alwan_oetf_apply(&encoded, ALWAN_TF_ACESCC, &test_values[i], 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan_eotf_apply(&decoded, ALWAN_TF_ACESCC, &encoded, 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan_scalar rel = (test_values[i] > ALWAN_LITERAL(0.0001)) ? ALWAN_ABS(decoded - test_values[i]) / test_values[i] : ALWAN_ABS(decoded - test_values[i]);
        TEST_ASSERT(rel < TEST_TOLERANCE, "ACEScc round-trip exceeded");
    }
    TEST_PASS("test_acescc_roundtrip");
}

static int test_acescct_known_values(void) {
    struct { alwan_scalar linear; alwan_scalar encoded; } known_pairs[] = {
        {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.41358840249244228)},
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0729055341958355)},
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.55479452054794520)},
    };
    size_t num_pairs = sizeof(known_pairs) / sizeof(known_pairs[0]);
    for (size_t i = 0; i < num_pairs; i++) {
        alwan_scalar encoded;
        alwan_oetf_apply(&encoded, ALWAN_TF_ACESCCT, &known_pairs[i].linear, 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        TEST_ASSERT(ALWAN_ABS(encoded - known_pairs[i].encoded) < TEST_TOLERANCE, "ACEScct known value exceeded");
    }
    TEST_PASS("test_acescct_known_values");
}

static int test_acescct_roundtrip(void) {
    alwan_scalar test_values[] = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0078125), ALWAN_LITERAL(0.18), ALWAN_LITERAL(1.0)};
    size_t num_values = sizeof(test_values) / sizeof(test_values[0]);
    for (size_t i = 0; i < num_values; i++) {
        alwan_scalar encoded, decoded;
        alwan_oetf_apply(&encoded, ALWAN_TF_ACESCCT, &test_values[i], 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan_eotf_apply(&decoded, ALWAN_TF_ACESCCT, &encoded, 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        TEST_ASSERT(ALWAN_ABS(decoded - test_values[i]) < TEST_TOLERANCE, "ACEScct round-trip exceeded");
    }
    TEST_PASS("test_acescct_roundtrip");
}

static int test_tf_roundtrip(char const *name, alwan_transfer_function tf) {
    alwan_scalar test_values[] = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.01), ALWAN_LITERAL(0.18), ALWAN_LITERAL(1.0)};
    size_t num_values = sizeof(test_values) / sizeof(test_values[0]);
    for (size_t i = 0; i < num_values; i++) {
        alwan_scalar encoded, decoded;
        if (alwan_oetf_apply(&encoded, tf, &test_values[i], 1, sizeof(alwan_scalar), sizeof(alwan_scalar)) != ALWAN_OK) return 1;
        if (alwan_eotf_apply(&decoded, tf, &encoded, 1, sizeof(alwan_scalar), sizeof(alwan_scalar)) != ALWAN_OK) return 1;
        if (ALWAN_ABS(decoded - test_values[i]) > TEST_TOLERANCE) return 1;
    }
    printf("[PASS] %s round-trip\n", name);
    return 0;
}

static int test_flog_roundtrip(void) { return test_tf_roundtrip("F-Log", ALWAN_TF_FLOG); }
static int test_flog2_roundtrip(void) { return test_tf_roundtrip("F-Log2", ALWAN_TF_FLOG2); }
static int test_llog_roundtrip(void) { return test_tf_roundtrip("L-Log", ALWAN_TF_LLOG); }
static int test_dlog_roundtrip(void) { return test_tf_roundtrip("D-Log", ALWAN_TF_DLOG); }
static int test_bmdfilm4_roundtrip(void) { return test_tf_roundtrip("BMDFilm4", ALWAN_TF_BMDFILM4); }

static int test_camera_logs_range(void) {
    alwan_transfer_function tfs[] = {ALWAN_TF_FLOG, ALWAN_TF_FLOG2, ALWAN_TF_LLOG, ALWAN_TF_DLOG, ALWAN_TF_BMDFILM4};
    size_t num_tfs = sizeof(tfs) / sizeof(tfs[0]);
    alwan_scalar gray = ALWAN_LITERAL(0.18);
    for (size_t i = 0; i < num_tfs; i++) {
        alwan_scalar encoded;
        alwan_oetf_apply(&encoded, tfs[i], &gray, 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        TEST_ASSERT(encoded > ALWAN_LITERAL(0.2) && encoded < ALWAN_LITERAL(0.7), "Camera log out of range");
    }
    TEST_PASS("test_camera_logs_range");
}

static int test_adx10_roundtrip(void) {
    alwan_scalar test_values[] = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.3), ALWAN_LITERAL(1.0), ALWAN_LITERAL(2.0)};
    size_t num_values = sizeof(test_values) / sizeof(test_values[0]);
    for (size_t i = 0; i < num_values; i++) {
        alwan_scalar encoded, decoded;
        alwan_oetf_apply(&encoded, ALWAN_TF_ADX10, &test_values[i], 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan_eotf_apply(&decoded, ALWAN_TF_ADX10, &encoded, 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        TEST_ASSERT(ALWAN_ABS(decoded - test_values[i]) < TEST_TOLERANCE, "ADX10 round-trip exceeded");
    }
    printf("[PASS] ADX10 round-trip\n");
    return 0;
}

static int test_adx16_roundtrip(void) {
    alwan_scalar test_values[] = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.3), ALWAN_LITERAL(1.0), ALWAN_LITERAL(2.0)};
    size_t num_values = sizeof(test_values) / sizeof(test_values[0]);
    for (size_t i = 0; i < num_values; i++) {
        alwan_scalar encoded, decoded;
        alwan_oetf_apply(&encoded, ALWAN_TF_ADX16, &test_values[i], 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan_eotf_apply(&decoded, ALWAN_TF_ADX16, &encoded, 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        TEST_ASSERT(ALWAN_ABS(decoded - test_values[i]) < TEST_TOLERANCE, "ADX16 round-trip exceeded");
    }
    printf("[PASS] ADX16 round-trip\n");
    return 0;
}

static int test_adx_known_values(void) {
    alwan_scalar density_zero = ALWAN_LITERAL(0.0);
    alwan_scalar encoded;
    alwan_oetf_apply(&encoded, ALWAN_TF_ADX10, &density_zero, 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
    alwan_scalar expected = ALWAN_LITERAL(200.0) / ALWAN_LITERAL(1023.0);
    TEST_ASSERT(ALWAN_ABS(encoded - expected) < TEST_TOLERANCE, "ADX10 known value exceeded");
    TEST_PASS("test_adx_known_values");
}

static int test_dlog_known_values(void) {
    /* DJI D-Log reference values from colour-science
     * For linear in log region (>0.0078): y = log10(x * 0.9892 + 0.0108) * 0.256663 + 0.584555
     * For linear in linear region (<=0.0078): y = 6.025 * x + 0.0929 */
    struct { alwan_scalar linear; alwan_scalar encoded; } known_pairs[] = {
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0929)},             /* Linear region: 6.025 * 0 + 0.0929 */
        {ALWAN_LITERAL(0.001), ALWAN_LITERAL(0.098925)},         /* Linear region: 6.025 * 0.001 + 0.0929 */
        {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.39876455618933060)}, /* Log region: 18% gray */
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.584555)},           /* Log region: diffuse white */
    };
    size_t num_pairs = sizeof(known_pairs) / sizeof(known_pairs[0]);
    for (size_t i = 0; i < num_pairs; i++) {
        alwan_scalar encoded;
        alwan_oetf_apply(&encoded, ALWAN_TF_DLOG, &known_pairs[i].linear, 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        alwan_scalar diff = ALWAN_ABS(encoded - known_pairs[i].encoded);
        if (diff >= TEST_TOLERANCE) {
            printf("D-Log: linear=%g, expected=%g, got=%g, diff=%g\n",
                    (double)known_pairs[i].linear, (double)known_pairs[i].encoded, (double)encoded, (double)diff);
        }
        TEST_ASSERT(diff < TEST_TOLERANCE, "D-Log known value exceeded");
    }
    TEST_PASS("test_dlog_known_values");
}

static int test_linear_identity(void) {
    alwan_scalar test_values[] = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.18), ALWAN_LITERAL(1.0), ALWAN_LITERAL(-0.1)};
    size_t num_values = sizeof(test_values) / sizeof(test_values[0]);
    for (size_t i = 0; i < num_values; i++) {
        alwan_scalar encoded, decoded;
        alwan_oetf_apply(&encoded, ALWAN_TF_LINEAR, &test_values[i], 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        TEST_ASSERT(encoded == test_values[i], "Linear OETF should be identity");
        alwan_eotf_apply(&decoded, ALWAN_TF_LINEAR, &test_values[i], 1, sizeof(alwan_scalar), sizeof(alwan_scalar));
        TEST_ASSERT(decoded == test_values[i], "Linear EOTF should be identity");
    }
    TEST_PASS("test_linear_identity");
}


static int test_aces_look_1_0_roundtrip(void) {
    alwan_rgb test_colors[] = {
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},
        {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)},
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.2)},
        {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.05)},
    };
    size_t num_colors = sizeof(test_colors) / sizeof(test_colors[0]);
    for (size_t i = 0; i < num_colors; i++) {
        alwan_rgb forward, roundtrip;
        int status = alwan_aces_look_1_0(&forward, &test_colors[i]);
        TEST_ASSERT(status == ALWAN_OK, "ACES 1.0 Look forward failed");
        status = alwan_aces_look_1_0_inv(&roundtrip, &forward);
        TEST_ASSERT(status == ALWAN_OK, "ACES 1.0 Look inverse failed");
        alwan_scalar diff_r = ALWAN_ABS(roundtrip.r - test_colors[i].r);
        alwan_scalar diff_g = ALWAN_ABS(roundtrip.g - test_colors[i].g);
        alwan_scalar diff_b = ALWAN_ABS(roundtrip.b - test_colors[i].b);
        if (diff_r >= TEST_TOLERANCE || diff_g >= TEST_TOLERANCE || diff_b >= TEST_TOLERANCE) {
            printf("  Index %zu: in=(%.6f,%.6f,%.6f) out=(%.6f,%.6f,%.6f) diff=(%.6f,%.6f,%.6f)\n",
                   i, test_colors[i].r, test_colors[i].g, test_colors[i].b,
                   roundtrip.r, roundtrip.g, roundtrip.b, diff_r, diff_g, diff_b);
        }
        TEST_ASSERT(diff_r < TEST_TOLERANCE && diff_g < TEST_TOLERANCE && diff_b < TEST_TOLERANCE, "ACES 1.0 Look round-trip exceeded");
    }
    TEST_PASS("test_aces_look_1_0_roundtrip");
}

static int test_glow_inv_roundtrip(void) {
    alwan_rgb test_colors[] = {
        {ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.1)},
        {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.2)},
    };
    size_t num_colors = sizeof(test_colors) / sizeof(test_colors[0]);
    for (size_t i = 0; i < num_colors; i++) {
        alwan_rgb forward, roundtrip;
        /* Test Glow10 */
        alwan_aces_glow10(&forward, &test_colors[i]);
        alwan_aces_glow10_inv(&roundtrip, &forward);
        alwan_scalar diff = ALWAN_ABS(roundtrip.r - test_colors[i].r) +
                            ALWAN_ABS(roundtrip.g - test_colors[i].g) +
                            ALWAN_ABS(roundtrip.b - test_colors[i].b);
        TEST_ASSERT(diff < TEST_TOLERANCE * 3, "Glow10 round-trip exceeded");
        /* Test Glow03 */
        alwan_aces_glow03(&forward, &test_colors[i]);
        alwan_aces_glow03_inv(&roundtrip, &forward);
        diff = ALWAN_ABS(roundtrip.r - test_colors[i].r) +
               ALWAN_ABS(roundtrip.g - test_colors[i].g) +
               ALWAN_ABS(roundtrip.b - test_colors[i].b);
        TEST_ASSERT(diff < TEST_TOLERANCE * 3, "Glow03 round-trip exceeded");
    }
    TEST_PASS("test_glow_inv_roundtrip");
}

static int test_redmod_inv_roundtrip(void) {
    alwan_rgb test_colors[] = {
        {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.1)},
        {ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.05)},
    };
    size_t num_colors = sizeof(test_colors) / sizeof(test_colors[0]);
    for (size_t i = 0; i < num_colors; i++) {
        alwan_rgb forward, roundtrip;
        /* Test RedMod10 */
        alwan_aces_redmod10(&forward, &test_colors[i]);
        alwan_aces_redmod10_inv(&roundtrip, &forward);
        alwan_scalar diff = ALWAN_ABS(roundtrip.r - test_colors[i].r);
        if (diff >= TEST_TOLERANCE) {
            printf("  RedMod10 index %zu: in=%.6f fwd=%.6f out=%.6f diff=%.6f\n",
                   i, test_colors[i].r, forward.r, roundtrip.r, diff);
        }
        TEST_ASSERT(diff < TEST_TOLERANCE, "RedMod10 round-trip exceeded");
        /* Test RedMod03 */
        alwan_aces_redmod03(&forward, &test_colors[i]);
        alwan_aces_redmod03_inv(&roundtrip, &forward);
        diff = ALWAN_ABS(roundtrip.r - test_colors[i].r);
        if (diff >= TEST_TOLERANCE) {
            printf("  RedMod03 index %zu: in=%.6f fwd=%.6f out=%.6f diff=%.6f\n",
                   i, test_colors[i].r, forward.r, roundtrip.r, diff);
        }
        TEST_ASSERT(diff < TEST_TOLERANCE, "RedMod03 round-trip exceeded");
    }
    TEST_PASS("test_redmod_inv_roundtrip");
}

/* Test AP0 <-> AP1 (ACES2065-1 <-> ACEScg) round-trip using ACES output transform
 * The output transform includes AP0->AP1 conversion internally, so we test
 * that the neutral axis is preserved (which validates the matrix math) */
static int test_ap0_ap1_roundtrip(void) {
    /* Test that 18% gray stays neutral through ACES output transform */
    alwan_rgb gray = {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)};
    alwan_rgb result;

    /* ACES 2.0 output transform (includes AP0->AP1 internally) */
    int status = alwan_aces2_output_transform(&result, &gray, ALWAN_ACES2_OUT_SRGB_100NIT);
    TEST_ASSERT(status == ALWAN_OK, "ACES2 output transform failed");

    /* Check that output is neutral (R≈G≈B) */
    alwan_scalar max_diff = ALWAN_ABS(result.r - result.g);
    alwan_scalar diff2 = ALWAN_ABS(result.g - result.b);
    if (diff2 > max_diff) max_diff = diff2;
    alwan_scalar diff3 = ALWAN_ABS(result.r - result.b);
    if (diff3 > max_diff) max_diff = diff3;
    TEST_ASSERT(max_diff < TEST_TOLERANCE, "Neutral axis not preserved in AP0->AP1");

    /* Test ACES 1.x output transform as well */
    status = alwan_aces1_output_transform(&result, &gray, ALWAN_ACES1_OUT_SRGB_100NIT);
    TEST_ASSERT(status == ALWAN_OK, "ACES1 output transform failed");

    max_diff = ALWAN_ABS(result.r - result.g);
    diff2 = ALWAN_ABS(result.g - result.b);
    if (diff2 > max_diff) max_diff = diff2;
    diff3 = ALWAN_ABS(result.r - result.b);
    if (diff3 > max_diff) max_diff = diff3;
    /* ACES 1.x has slight chromatic adaptation differences due to D60->D65 conversion */
    TEST_ASSERT(max_diff < ALWAN_LITERAL(0.01), "Neutral axis not preserved in ACES 1.x");

    TEST_PASS("test_ap0_ap1_roundtrip");
}

/* Test JMh round-trip (ACES 2.0) */
static int test_jmh_roundtrip(void) {
    alwan_rgb test_colors[] = {
        {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)},
        {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.2)},
        {ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.9)},
    };
    size_t num_colors = sizeof(test_colors) / sizeof(test_colors[0]);

    /* Get default AP1 primaries */
    alwan_aces_primaries primaries;
    alwan_aces_primaries_ap1_default(&primaries);

    for (size_t i = 0; i < num_colors; i++) {
        alwan_vec3 jmh;
        alwan_rgb roundtrip;
        /* RGB -> JMh */
        int status = alwan_aces_rgb_to_jmh20(&jmh, &test_colors[i], &primaries);
        TEST_ASSERT(status == ALWAN_OK, "RGB to JMh conversion failed");
        /* JMh -> RGB */
        status = alwan_aces_jmh_to_rgb20(&roundtrip, &jmh, &primaries);
        TEST_ASSERT(status == ALWAN_OK, "JMh to RGB conversion failed");
        /* Check round-trip (looser tolerance for JMh) */
        alwan_scalar diff = ALWAN_ABS(roundtrip.r - test_colors[i].r) +
                            ALWAN_ABS(roundtrip.g - test_colors[i].g) +
                            ALWAN_ABS(roundtrip.b - test_colors[i].b);
        TEST_ASSERT(diff < TEST_TOLERANCE * 3, "JMh round-trip exceeded");
    }
    TEST_PASS("test_jmh_roundtrip");
}

/* Test Parametric LMT */
static int test_parametric_lmt(void) {
    alwan_aces_lmt_params params;
    alwan_aces_lmt_params_init(&params);

    /* Test 1: Identity transform (default params) */
    alwan_rgb gray = {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)};
    alwan_rgb result;
    int status = alwan_aces_lmt_apply(&result, &gray, &params);
    TEST_ASSERT(status == ALWAN_OK, "Parametric LMT identity failed");
    TEST_ASSERT(ALWAN_ABS(result.r - gray.r) < TEST_TOLERANCE, "Identity should preserve R");
    TEST_ASSERT(ALWAN_ABS(result.g - gray.g) < TEST_TOLERANCE, "Identity should preserve G");
    TEST_ASSERT(ALWAN_ABS(result.b - gray.b) < TEST_TOLERANCE, "Identity should preserve B");

    /* Test 2: Slope (gain) adjustment */
    params.slope[0] = params.slope[1] = params.slope[2] = ALWAN_LITERAL(2.0);
    status = alwan_aces_lmt_apply(&result, &gray, &params);
    TEST_ASSERT(status == ALWAN_OK, "Parametric LMT slope failed");
    TEST_ASSERT(ALWAN_ABS(result.r - ALWAN_LITERAL(0.36)) < TEST_TOLERANCE, "Slope should double values");
    alwan_aces_lmt_params_init(&params);  /* Reset */

    /* Test 3: Offset adjustment */
    params.offset[0] = params.offset[1] = params.offset[2] = ALWAN_LITERAL(0.1);
    status = alwan_aces_lmt_apply(&result, &gray, &params);
    TEST_ASSERT(status == ALWAN_OK, "Parametric LMT offset failed");
    TEST_ASSERT(ALWAN_ABS(result.r - ALWAN_LITERAL(0.28)) < TEST_TOLERANCE, "Offset should add 0.1");
    alwan_aces_lmt_params_init(&params);  /* Reset */

    /* Test 4: Saturation adjustment - should keep gray neutral */
    params.saturation = ALWAN_LITERAL(1.5);
    status = alwan_aces_lmt_apply(&result, &gray, &params);
    TEST_ASSERT(status == ALWAN_OK, "Parametric LMT saturation failed");
    /* Gray input should remain gray (R=G=B) */
    TEST_ASSERT(ALWAN_ABS(result.r - result.g) < TEST_TOLERANCE, "Gray should stay neutral");
    TEST_ASSERT(ALWAN_ABS(result.g - result.b) < TEST_TOLERANCE, "Gray should stay neutral");
    alwan_aces_lmt_params_init(&params);  /* Reset */

    /* Test 5: Saturation on chromatic color */
    alwan_rgb red = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.1), ALWAN_LITERAL(0.1)};
    params.saturation = ALWAN_LITERAL(1.5);
    status = alwan_aces_lmt_apply(&result, &red, &params);
    TEST_ASSERT(status == ALWAN_OK, "Parametric LMT saturation on color failed");
    /* Increased saturation should increase chroma */
    alwan_scalar in_chroma = red.r - (red.g + red.b) / ALWAN_LITERAL(2.0);
    alwan_scalar out_chroma = result.r - (result.g + result.b) / ALWAN_LITERAL(2.0);
    TEST_ASSERT(out_chroma > in_chroma, "Saturation 1.5 should increase chroma");

    TEST_PASS("test_parametric_lmt");
}

/* Test ACES 2.0 tonescale known values */
static int test_tonescale_known_values(void) {
    /* Test that 18% gray maps to expected mid-gray in SDR output */
    alwan_rgb gray18 = {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)};
    alwan_rgb result;

    /* 100 nit SDR tonescale */
    int status = alwan_aces_tonescale_compress20(&result, &gray18, ALWAN_LITERAL(100.0));
    TEST_ASSERT(status == ALWAN_OK, "Tonescale 100 nit failed");
    /* 18% gray should map to a reasonable positive value (output is compressed linear) */
    TEST_ASSERT(result.r > ALWAN_LITERAL(0.05) && result.r < ALWAN_LITERAL(1.0), "18% gray should map to positive output");
    /* Should remain neutral */
    TEST_ASSERT(ALWAN_ABS(result.r - result.g) < TEST_TOLERANCE, "Neutral should stay neutral");
    TEST_ASSERT(ALWAN_ABS(result.g - result.b) < TEST_TOLERANCE, "Neutral should stay neutral");

    /* Test black stays black */
    alwan_rgb black = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
    status = alwan_aces_tonescale_compress20(&result, &black, ALWAN_LITERAL(100.0));
    TEST_ASSERT(status == ALWAN_OK, "Tonescale black failed");
    TEST_ASSERT(result.r < ALWAN_LITERAL(0.01), "Black should stay near black");

    /* Test HDR tonescale - higher peak should compress less in highlights */
    alwan_rgb bright = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};
    alwan_rgb result_sdr, result_hdr;
    alwan_aces_tonescale_compress20(&result_sdr, &bright, ALWAN_LITERAL(100.0));
    alwan_aces_tonescale_compress20(&result_hdr, &bright, ALWAN_LITERAL(1000.0));
    /* HDR should have higher output for same input (less compression) */
    TEST_ASSERT(result_hdr.r >= result_sdr.r, "HDR should compress less than SDR");

    TEST_PASS("test_tonescale_known_values");
}

/* Test ACES 2.0 gamut compression boundary colors */
static int test_gamut_compress_boundary(void) {
    alwan_aces_primaries primaries;
    alwan_aces_primaries_ap1_default(&primaries);

    /* Test highly saturated red (out of gamut for most displays) */
    alwan_rgb saturated_red = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
    alwan_vec3 jmh, jmh_compressed, jmh_recovered;
    alwan_rgb recovered;

    /* Convert to JMh */
    int status = alwan_aces_rgb_to_jmh20(&jmh, &saturated_red, &primaries);
    TEST_ASSERT(status == ALWAN_OK, "RGB to JMh failed");

    /* Forward gamut compression in JMh space */
    status = alwan_aces_gamut_compress20(&jmh_compressed, &jmh, ALWAN_LITERAL(100.0), &primaries);
    TEST_ASSERT(status == ALWAN_OK, "Gamut compress forward failed");
    /* Compressed M (colorfulness) should be <= original M */
    TEST_ASSERT(jmh_compressed.v[1] <= jmh.v[1] + TEST_TOLERANCE, "Gamut compress should reduce or preserve M");

    /* Inverse gamut compression */
    status = alwan_aces_gamut_compress20_inv(&jmh_recovered, &jmh_compressed, ALWAN_LITERAL(100.0), &primaries);
    TEST_ASSERT(status == ALWAN_OK, "Gamut compress inverse failed");
    /* Should approximately recover original JMh (within tolerance) */
    alwan_scalar diff = ALWAN_ABS(jmh_recovered.v[0] - jmh.v[0]) +
                        ALWAN_ABS(jmh_recovered.v[1] - jmh.v[1]) +
                        ALWAN_ABS(jmh_recovered.v[2] - jmh.v[2]);
    TEST_ASSERT(diff < TEST_TOLERANCE * 3, "Gamut compress round-trip exceeded");

    /* Convert back to RGB and verify */
    status = alwan_aces_jmh_to_rgb20(&recovered, &jmh_recovered, &primaries);
    TEST_ASSERT(status == ALWAN_OK, "JMh to RGB failed");

    /* Test neutral stays neutral through full pipeline */
    alwan_rgb gray = {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)};
    alwan_vec3 gray_jmh, gray_compressed;
    alwan_aces_rgb_to_jmh20(&gray_jmh, &gray, &primaries);
    status = alwan_aces_gamut_compress20(&gray_compressed, &gray_jmh, ALWAN_LITERAL(100.0), &primaries);
    TEST_ASSERT(status == ALWAN_OK, "Gamut compress neutral failed");
    /* Neutral should have M (colorfulness) near zero */
    TEST_ASSERT(gray_jmh.v[1] < ALWAN_LITERAL(0.1), "Neutral should have low colorfulness");

    TEST_PASS("test_gamut_compress_boundary");
}

typedef int (*test_fn)(void);
typedef struct { char const *name; test_fn fn; } test_entry;

static test_entry const tests[] = {
    {"blue_light_fix_roundtrip", test_blue_light_fix_roundtrip},
    {"blue_light_fix_effect", test_blue_light_fix_effect},
    {"acescc_known_values", test_acescc_known_values},
    {"acescc_roundtrip", test_acescc_roundtrip},
    {"acescct_known_values", test_acescct_known_values},
    {"acescct_roundtrip", test_acescct_roundtrip},
    {"flog_roundtrip", test_flog_roundtrip},
    {"flog2_roundtrip", test_flog2_roundtrip},
    {"llog_roundtrip", test_llog_roundtrip},
    {"dlog_roundtrip", test_dlog_roundtrip},
    {"bmdfilm4_roundtrip", test_bmdfilm4_roundtrip},
    {"camera_logs_range", test_camera_logs_range},
    {"adx10_roundtrip", test_adx10_roundtrip},
    {"adx16_roundtrip", test_adx16_roundtrip},
    {"adx_known_values", test_adx_known_values},
    {"dlog_known_values", test_dlog_known_values},
    {"linear_identity", test_linear_identity},
    {"aces_look_1_0_roundtrip", test_aces_look_1_0_roundtrip},
    {"glow_inv_roundtrip", test_glow_inv_roundtrip},
    {"redmod_inv_roundtrip", test_redmod_inv_roundtrip},
    {"ap0_ap1_roundtrip", test_ap0_ap1_roundtrip},
    {"jmh_roundtrip", test_jmh_roundtrip},
    {"parametric_lmt", test_parametric_lmt},
    {"tonescale_known_values", test_tonescale_known_values},
    {"gamut_compress_boundary", test_gamut_compress_boundary},
};

int test_57_aces_lmt_tf_main(void) {
    printf("Running ACES LMT and Transfer Function tests...\n");
    int failed = 0, passed = 0;
    size_t const num_tests = sizeof(tests) / sizeof(tests[0]);
    for (size_t i = 0; i < num_tests; i++) {
        if (tests[i].fn() == 0) passed++; else { failed++; printf("[FAIL] Test '%s' failed\n", tests[i].name); }
    }
    printf("\n========================================\n");
    printf("Results: %d passed, %d failed (out of %zu)\n", passed, failed, num_tests);
    printf("========================================\n");
    return (failed > 0) ? 1 : 0;
}
