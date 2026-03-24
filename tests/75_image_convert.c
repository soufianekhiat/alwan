/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test suite 75: Image Convert
 * Validates alwan_image_convert across pixel formats, same/different white points,
 * and row-stride layouts.
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>

/* Reference: manually apply EOTF -> rgb_convert -> OETF to match alwan_image_convert.
 * alwan_rgb_convert operates on linear RGB (no TF applied), while alwan_image_convert
 * handles the full encoded -> decoded -> matrix -> encoded pipeline. */
static int ref_image_pixel(alwan_rgb *out, alwan_ctx *ctx,
                           alwan_rgb_space_desc const *src_space,
                           alwan_rgb_space_desc const *dst_space,
                           alwan_rgb const *in) {
    /* Decode source: EOTF (encoded -> linear) */
    alwan_f64 linear_src[3];
    alwan_eotf_apply(linear_src, src_space->eotf,
                     (alwan_f64 const *)in, 3,
                     sizeof(alwan_f64), sizeof(alwan_f64));

    /* Convert linear src -> linear dst */
    alwan_rgb lin_in = {linear_src[0], linear_src[1], linear_src[2]};
    alwan_rgb lin_out;
    int status = alwan_rgb_convert(&lin_out, ctx, src_space, dst_space, &lin_in);
    if (status != ALWAN_OK) return status;

    /* Encode destination: OETF (linear -> encoded) */
    alwan_f64 linear_dst[3] = {lin_out.r, lin_out.g, lin_out.b};
    alwan_f64 encoded_dst[3];
    alwan_oetf_apply(encoded_dst, dst_space->oetf,
                     linear_dst, 3,
                     sizeof(alwan_f64), sizeof(alwan_f64));

    out->r = encoded_dst[0];
    out->g = encoded_dst[1];
    out->b = encoded_dst[2];
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Test: sRGB -> Display P3 (same white point D65, F64 -> F64)
 * ---------------------------------------------------------------- */
static int test_image_convert_same_wp(void) {
    TEST_START("image_convert sRGB -> Display P3 (same white point)");

    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) TEST_FAIL("failed to create context");

    alwan_rgb_space_desc srgb = {0};
    alwan_rgb_space_desc p3 = {0};
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_P3_D65);

    /* 4x2 image */
    size_t const W = 4, H = 2;
    alwan_f64 src[4 * 2 * 3];
    alwan_f64 dst[4 * 2 * 3];

    /* Fill with known sRGB-encoded values */
    alwan_f64 const colors[][3] = {
        {0.0, 0.0, 0.0},
        {1.0, 1.0, 1.0},
        {0.8, 0.2, 0.1},
        {0.2, 0.6, 0.4},
        {0.5, 0.5, 0.5},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0},
    };
    for (size_t i = 0; i < W * H; i++) {
        src[i * 3 + 0] = colors[i][0];
        src[i * 3 + 1] = colors[i][1];
        src[i * 3 + 2] = colors[i][2];
    }

    size_t row_stride = W * 3 * sizeof(alwan_f64);

    int status = alwan_image_convert(
        dst, ALWAN_PIXEL_F64, row_stride,
        src, ALWAN_PIXEL_F64, row_stride,
        W, H, ctx, &srgb, &p3);

    TEST_ASSERT(status == ALWAN_OK, "alwan_image_convert returned error");

    /* Verify against reference: EOTF -> rgb_convert -> OETF */
    for (size_t i = 0; i < W * H; i++) {
        alwan_rgb rgb_in = {colors[i][0], colors[i][1], colors[i][2]};
        alwan_rgb rgb_ref;
        ref_image_pixel(&rgb_ref, ctx, &srgb, &p3, &rgb_in);

        char msg[128];
        snprintf(msg, sizeof(msg), "pixel %zu R", i);
        TEST_ASSERT_NEAR(dst[i * 3 + 0], rgb_ref.r, ALWAN_TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "pixel %zu G", i);
        TEST_ASSERT_NEAR(dst[i * 3 + 1], rgb_ref.g, ALWAN_TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "pixel %zu B", i);
        TEST_ASSERT_NEAR(dst[i * 3 + 2], rgb_ref.b, ALWAN_TEST_TOLERANCE, msg);
    }

    alwan_destroy(ctx);
    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * Test: sRGB -> ACEScg (different white points: D65 -> D60)
 * ---------------------------------------------------------------- */
static int test_image_convert_diff_wp(void) {
    TEST_START("image_convert sRGB -> ACEScg (chromatic adaptation)");

    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) TEST_FAIL("failed to create context");

    alwan_rgb_space_desc srgb = {0};
    alwan_rgb_space_desc acescg = {0};
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&acescg, ctx, ALWAN_RGB_SPACE_ACESCG);

    size_t const W = 3, H = 1;
    alwan_f64 src[3 * 3];
    alwan_f64 dst[3 * 3];

    alwan_f64 const colors[][3] = {
        {0.5, 0.3, 0.2},
        {0.1, 0.9, 0.5},
        {0.8, 0.8, 0.8},
    };
    for (size_t i = 0; i < W * H; i++) {
        src[i * 3 + 0] = colors[i][0];
        src[i * 3 + 1] = colors[i][1];
        src[i * 3 + 2] = colors[i][2];
    }

    size_t row_stride = W * 3 * sizeof(alwan_f64);

    int status = alwan_image_convert(
        dst, ALWAN_PIXEL_F64, row_stride,
        src, ALWAN_PIXEL_F64, row_stride,
        W, H, ctx, &srgb, &acescg);

    TEST_ASSERT(status == ALWAN_OK, "alwan_image_convert returned error");

    for (size_t i = 0; i < W * H; i++) {
        alwan_rgb rgb_in = {colors[i][0], colors[i][1], colors[i][2]};
        alwan_rgb rgb_ref;
        ref_image_pixel(&rgb_ref, ctx, &srgb, &acescg, &rgb_in);

        char msg[128];
        snprintf(msg, sizeof(msg), "CAT pixel %zu R", i);
        TEST_ASSERT_NEAR(dst[i * 3 + 0], rgb_ref.r, ALWAN_TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "CAT pixel %zu G", i);
        TEST_ASSERT_NEAR(dst[i * 3 + 1], rgb_ref.g, ALWAN_TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "CAT pixel %zu B", i);
        TEST_ASSERT_NEAR(dst[i * 3 + 2], rgb_ref.b, ALWAN_TEST_TOLERANCE, msg);
    }

    alwan_destroy(ctx);
    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * Test: U8 -> F32 format conversion
 * ---------------------------------------------------------------- */
static int test_image_convert_u8_to_f32(void) {
    TEST_START("image_convert U8 -> F32 format conversion");

    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) TEST_FAIL("failed to create context");

    alwan_rgb_space_desc srgb = {0};
    alwan_rgb_space_desc bt2020 = {0};
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&bt2020, ctx, ALWAN_RGB_SPACE_BT2020);

    size_t const W = 3, H = 2;

    /* U8 source image */
    uint8_t src_u8[3 * 2 * 3];
    uint8_t const pixels[][3] = {
        {0, 0, 0},
        {255, 255, 255},
        {204, 51, 26},
        {128, 128, 128},
        {255, 0, 0},
        {0, 255, 0},
    };
    for (size_t i = 0; i < W * H; i++) {
        src_u8[i * 3 + 0] = pixels[i][0];
        src_u8[i * 3 + 1] = pixels[i][1];
        src_u8[i * 3 + 2] = pixels[i][2];
    }

    /* F32 destination */
    float dst_f32[3 * 2 * 3];

    size_t src_row = W * 3 * sizeof(uint8_t);
    size_t dst_row = W * 3 * sizeof(float);

    int status = alwan_image_convert(
        dst_f32, ALWAN_PIXEL_F32, dst_row,
        src_u8, ALWAN_PIXEL_U8, src_row,
        W, H, ctx, &srgb, &bt2020);

    TEST_ASSERT(status == ALWAN_OK, "alwan_image_convert U8->F32 returned error");

    /* Verify against reference: normalize U8, EOTF, convert, OETF */
    alwan_f64 const f32_tol = ALWAN_LITERAL(1e-5);
    for (size_t i = 0; i < W * H; i++) {
        alwan_rgb rgb_in = {
            (alwan_f64)pixels[i][0] / ALWAN_LITERAL(255.0),
            (alwan_f64)pixels[i][1] / ALWAN_LITERAL(255.0),
            (alwan_f64)pixels[i][2] / ALWAN_LITERAL(255.0)
        };
        alwan_rgb rgb_ref;
        ref_image_pixel(&rgb_ref, ctx, &srgb, &bt2020, &rgb_in);

        char msg[128];
        snprintf(msg, sizeof(msg), "U8->F32 pixel %zu R", i);
        TEST_ASSERT_NEAR((alwan_f64)dst_f32[i * 3 + 0], rgb_ref.r, f32_tol, msg);
        snprintf(msg, sizeof(msg), "U8->F32 pixel %zu G", i);
        TEST_ASSERT_NEAR((alwan_f64)dst_f32[i * 3 + 1], rgb_ref.g, f32_tol, msg);
        snprintf(msg, sizeof(msg), "U8->F32 pixel %zu B", i);
        TEST_ASSERT_NEAR((alwan_f64)dst_f32[i * 3 + 2], rgb_ref.b, f32_tol, msg);
    }

    alwan_destroy(ctx);
    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * Test: Row stride with padding
 * ---------------------------------------------------------------- */
static int test_image_convert_row_stride(void) {
    TEST_START("image_convert with padded row stride");

    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) TEST_FAIL("failed to create context");

    alwan_rgb_space_desc srgb = {0};
    alwan_rgb_space_desc p3 = {0};
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_P3_D65);

    /* 2x3 image with padded rows (32-byte aligned) */
    size_t const W = 2, H = 3;
    size_t const padded_row = 32;  /* 32 bytes (natural is 24 for 2 * 3 * sizeof(float)) */

    uint8_t *src_buf = (uint8_t *)calloc(padded_row * H, 1);
    uint8_t *dst_buf = (uint8_t *)calloc(padded_row * H, 1);
    if (!src_buf || !dst_buf) TEST_FAIL("allocation failed");

    /* Fill source pixels */
    float const test_colors[][3] = {
        {0.8f, 0.2f, 0.1f}, {0.3f, 0.7f, 0.5f},
        {0.1f, 0.1f, 0.9f}, {0.9f, 0.9f, 0.1f},
        {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 0.0f},
    };
    for (size_t y = 0; y < H; y++) {
        float *row = (float *)(src_buf + y * padded_row);
        for (size_t x = 0; x < W; x++) {
            row[x * 3 + 0] = test_colors[y * W + x][0];
            row[x * 3 + 1] = test_colors[y * W + x][1];
            row[x * 3 + 2] = test_colors[y * W + x][2];
        }
    }

    int status = alwan_image_convert(
        dst_buf, ALWAN_PIXEL_F32, padded_row,
        src_buf, ALWAN_PIXEL_F32, padded_row,
        W, H, ctx, &srgb, &p3);

    TEST_ASSERT(status == ALWAN_OK, "alwan_image_convert padded returned error");

    /* Verify each pixel */
    alwan_f64 const f32_tol = ALWAN_LITERAL(1e-5);
    for (size_t y = 0; y < H; y++) {
        float const *dst_row = (float const *)(dst_buf + y * padded_row);
        for (size_t x = 0; x < W; x++) {
            size_t idx = y * W + x;
            alwan_rgb rgb_in = {
                (alwan_f64)test_colors[idx][0],
                (alwan_f64)test_colors[idx][1],
                (alwan_f64)test_colors[idx][2]
            };
            alwan_rgb rgb_ref;
            ref_image_pixel(&rgb_ref, ctx, &srgb, &p3, &rgb_in);

            char msg[128];
            snprintf(msg, sizeof(msg), "padded [%zu,%zu] R", y, x);
            TEST_ASSERT_NEAR((alwan_f64)dst_row[x * 3 + 0], rgb_ref.r, f32_tol, msg);
            snprintf(msg, sizeof(msg), "padded [%zu,%zu] G", y, x);
            TEST_ASSERT_NEAR((alwan_f64)dst_row[x * 3 + 1], rgb_ref.g, f32_tol, msg);
            snprintf(msg, sizeof(msg), "padded [%zu,%zu] B", y, x);
            TEST_ASSERT_NEAR((alwan_f64)dst_row[x * 3 + 2], rgb_ref.b, f32_tol, msg);
        }
    }

    free(src_buf);
    free(dst_buf);
    alwan_destroy(ctx);
    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * Test: In-place conversion (src == dst)
 * ---------------------------------------------------------------- */
static int test_image_convert_in_place(void) {
    TEST_START("image_convert in-place (src == dst)");

    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) TEST_FAIL("failed to create context");

    alwan_rgb_space_desc srgb = {0};
    alwan_rgb_space_desc p3 = {0};
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_P3_D65);

    size_t const W = 3, H = 1;
    alwan_f64 buf[3 * 3];
    alwan_f64 const colors[][3] = {
        {0.8, 0.2, 0.1},
        {0.3, 0.7, 0.5},
        {0.5, 0.5, 0.5},
    };

    /* Compute reference first */
    alwan_f64 ref[3 * 3];
    for (size_t i = 0; i < W; i++) {
        alwan_rgb rgb_in = {colors[i][0], colors[i][1], colors[i][2]};
        alwan_rgb rgb_ref;
        ref_image_pixel(&rgb_ref, ctx, &srgb, &p3, &rgb_in);
        ref[i * 3 + 0] = rgb_ref.r;
        ref[i * 3 + 1] = rgb_ref.g;
        ref[i * 3 + 2] = rgb_ref.b;
    }

    /* Fill buffer and convert in-place */
    for (size_t i = 0; i < W; i++) {
        buf[i * 3 + 0] = colors[i][0];
        buf[i * 3 + 1] = colors[i][1];
        buf[i * 3 + 2] = colors[i][2];
    }

    size_t row_stride = W * 3 * sizeof(alwan_f64);
    int status = alwan_image_convert(
        buf, ALWAN_PIXEL_F64, row_stride,
        buf, ALWAN_PIXEL_F64, row_stride,
        W, H, ctx, &srgb, &p3);

    TEST_ASSERT(status == ALWAN_OK, "alwan_image_convert in-place returned error");

    for (size_t i = 0; i < W; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "in-place pixel %zu R", i);
        TEST_ASSERT_NEAR(buf[i * 3 + 0], ref[i * 3 + 0], ALWAN_TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "in-place pixel %zu G", i);
        TEST_ASSERT_NEAR(buf[i * 3 + 1], ref[i * 3 + 1], ALWAN_TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "in-place pixel %zu B", i);
        TEST_ASSERT_NEAR(buf[i * 3 + 2], ref[i * 3 + 2], ALWAN_TEST_TOLERANCE, msg);
    }

    alwan_destroy(ctx);
    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * Test: Error handling
 * ---------------------------------------------------------------- */
static int test_image_convert_errors(void) {
    TEST_START("image_convert error handling");

    alwan_rgb_space_desc srgb = {0};
    alwan_f64 buf[9];

    /* NULL dst */
    TEST_ASSERT(alwan_image_convert(NULL, ALWAN_PIXEL_F64, 24,
        buf, ALWAN_PIXEL_F64, 24, 1, 1, NULL, &srgb, &srgb) == ALWAN_E_INVALID,
        "NULL dst should fail");

    /* NULL src */
    TEST_ASSERT(alwan_image_convert(buf, ALWAN_PIXEL_F64, 24,
        NULL, ALWAN_PIXEL_F64, 24, 1, 1, NULL, &srgb, &srgb) == ALWAN_E_INVALID,
        "NULL src should fail");

    /* zero width */
    TEST_ASSERT(alwan_image_convert(buf, ALWAN_PIXEL_F64, 24,
        buf, ALWAN_PIXEL_F64, 24, 0, 1, NULL, &srgb, &srgb) == ALWAN_E_INVALID,
        "zero width should fail");

    /* zero height */
    TEST_ASSERT(alwan_image_convert(buf, ALWAN_PIXEL_F64, 24,
        buf, ALWAN_PIXEL_F64, 24, 1, 0, NULL, &srgb, &srgb) == ALWAN_E_INVALID,
        "zero height should fail");

    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * Test: RGBA straight alpha pass-through
 * ---------------------------------------------------------------- */
static int test_image_convert_rgba_straight(void) {
    TEST_START("image_convert_rgba straight alpha (pass-through)");

    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) TEST_FAIL("failed to create context");

    alwan_rgb_space_desc srgb = {0};
    alwan_rgb_space_desc p3 = {0};
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_P3_D65);

    size_t const W = 3, H = 1;
    alwan_f64 src[3 * 4]; /* 3 pixels x 4 channels */
    alwan_f64 dst[3 * 4];

    /* RGBA pixels with varying alpha */
    alwan_f64 const pixels[][4] = {
        {0.8, 0.2, 0.1, 1.0},   /* fully opaque */
        {0.3, 0.7, 0.5, 0.5},   /* semi-transparent */
        {0.5, 0.5, 0.5, 0.0},   /* fully transparent */
    };
    for (size_t i = 0; i < W; i++) {
        src[i * 4 + 0] = pixels[i][0];
        src[i * 4 + 1] = pixels[i][1];
        src[i * 4 + 2] = pixels[i][2];
        src[i * 4 + 3] = pixels[i][3];
    }

    size_t row_stride = W * 4 * sizeof(alwan_f64);
    int status = alwan_image_convert_rgba(
        dst, ALWAN_PIXEL_F64, row_stride,
        src, ALWAN_PIXEL_F64, row_stride,
        W, H, ctx, &srgb, &p3, ALWAN_ALPHA_STRAIGHT);

    TEST_ASSERT(status == ALWAN_OK, "alwan_image_convert_rgba returned error");

    /* Verify RGB matches 3-channel reference, alpha preserved */
    for (size_t i = 0; i < W; i++) {
        alwan_rgb rgb_in = {pixels[i][0], pixels[i][1], pixels[i][2]};
        alwan_rgb rgb_ref;
        ref_image_pixel(&rgb_ref, ctx, &srgb, &p3, &rgb_in);

        char msg[128];
        snprintf(msg, sizeof(msg), "straight pixel %zu R", i);
        TEST_ASSERT_NEAR(dst[i * 4 + 0], rgb_ref.r, ALWAN_TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "straight pixel %zu G", i);
        TEST_ASSERT_NEAR(dst[i * 4 + 1], rgb_ref.g, ALWAN_TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "straight pixel %zu B", i);
        TEST_ASSERT_NEAR(dst[i * 4 + 2], rgb_ref.b, ALWAN_TEST_TOLERANCE, msg);

        /* Alpha must be exactly preserved */
        snprintf(msg, sizeof(msg), "straight pixel %zu A", i);
        TEST_ASSERT_NEAR(dst[i * 4 + 3], pixels[i][3], ALWAN_LITERAL(1e-15), msg);
    }

    alwan_destroy(ctx);
    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * Test: RGBA premultiplied alpha
 * ---------------------------------------------------------------- */
static int test_image_convert_rgba_premul(void) {
    TEST_START("image_convert_rgba premultiplied alpha");

    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) TEST_FAIL("failed to create context");

    alwan_rgb_space_desc srgb = {0};
    alwan_rgb_space_desc p3 = {0};
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_P3_D65);

    size_t const W = 3, H = 1;
    alwan_f64 src[3 * 4];
    alwan_f64 dst[3 * 4];

    /* Straight RGBA values to build premultiplied from */
    alwan_f64 const straight[][4] = {
        {0.8, 0.2, 0.1, 1.0},   /* opaque: premul == straight */
        {0.4, 0.8, 0.6, 0.5},   /* semi: premul = straight * 0.5 */
        {0.0, 0.0, 0.0, 0.0},   /* zero alpha: RGB must be 0 */
    };

    /* Store as premultiplied */
    for (size_t i = 0; i < W; i++) {
        alwan_f64 a = straight[i][3];
        src[i * 4 + 0] = straight[i][0] * a;
        src[i * 4 + 1] = straight[i][1] * a;
        src[i * 4 + 2] = straight[i][2] * a;
        src[i * 4 + 3] = a;
    }

    size_t row_stride = W * 4 * sizeof(alwan_f64);
    int status = alwan_image_convert_rgba(
        dst, ALWAN_PIXEL_F64, row_stride,
        src, ALWAN_PIXEL_F64, row_stride,
        W, H, ctx, &srgb, &p3, ALWAN_ALPHA_PREMULTIPLIED);

    TEST_ASSERT(status == ALWAN_OK, "alwan_image_convert_rgba premul returned error");

    /* For each pixel: unpremul -> convert -> repremul.
     * Reference: convert the straight RGB, then repremultiply. */
    for (size_t i = 0; i < W; i++) {
        alwan_f64 a = straight[i][3];
        char msg[128];

        if (a > ALWAN_LITERAL(0.0)) {
            alwan_rgb rgb_in = {straight[i][0], straight[i][1], straight[i][2]};
            alwan_rgb rgb_ref;
            ref_image_pixel(&rgb_ref, ctx, &srgb, &p3, &rgb_in);

            /* Result should be premultiplied: converted_rgb * alpha */
            snprintf(msg, sizeof(msg), "premul pixel %zu R", i);
            TEST_ASSERT_NEAR(dst[i * 4 + 0], rgb_ref.r * a, ALWAN_TEST_TOLERANCE, msg);
            snprintf(msg, sizeof(msg), "premul pixel %zu G", i);
            TEST_ASSERT_NEAR(dst[i * 4 + 1], rgb_ref.g * a, ALWAN_TEST_TOLERANCE, msg);
            snprintf(msg, sizeof(msg), "premul pixel %zu B", i);
            TEST_ASSERT_NEAR(dst[i * 4 + 2], rgb_ref.b * a, ALWAN_TEST_TOLERANCE, msg);
        } else {
            /* Zero alpha: RGB should remain zero (premul * 0 = 0) */
            snprintf(msg, sizeof(msg), "premul pixel %zu R (zero alpha)", i);
            TEST_ASSERT_NEAR(dst[i * 4 + 0], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1e-15), msg);
            snprintf(msg, sizeof(msg), "premul pixel %zu G (zero alpha)", i);
            TEST_ASSERT_NEAR(dst[i * 4 + 1], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1e-15), msg);
            snprintf(msg, sizeof(msg), "premul pixel %zu B (zero alpha)", i);
            TEST_ASSERT_NEAR(dst[i * 4 + 2], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1e-15), msg);
        }

        /* Alpha preserved */
        snprintf(msg, sizeof(msg), "premul pixel %zu A", i);
        TEST_ASSERT_NEAR(dst[i * 4 + 3], a, ALWAN_LITERAL(1e-15), msg);
    }

    alwan_destroy(ctx);
    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * Test: RGBA U8 -> F32 format conversion with alpha
 * ---------------------------------------------------------------- */
static int test_image_convert_rgba_u8_f32(void) {
    TEST_START("image_convert_rgba U8 -> F32 with alpha");

    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) TEST_FAIL("failed to create context");

    alwan_rgb_space_desc srgb = {0};
    alwan_rgb_space_desc p3 = {0};
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_P3_D65);

    size_t const W = 2, H = 1;
    uint8_t src_u8[2 * 4];
    float   dst_f32[2 * 4];

    /* U8 RGBA pixels */
    uint8_t const u8_pixels[][4] = {
        {204, 51, 26, 255},  /* fully opaque */
        {128, 200, 100, 128}, /* semi-transparent */
    };
    for (size_t i = 0; i < W; i++) {
        src_u8[i * 4 + 0] = u8_pixels[i][0];
        src_u8[i * 4 + 1] = u8_pixels[i][1];
        src_u8[i * 4 + 2] = u8_pixels[i][2];
        src_u8[i * 4 + 3] = u8_pixels[i][3];
    }

    size_t src_row = W * 4 * sizeof(uint8_t);
    size_t dst_row = W * 4 * sizeof(float);

    int status = alwan_image_convert_rgba(
        dst_f32, ALWAN_PIXEL_F32, dst_row,
        src_u8, ALWAN_PIXEL_U8, src_row,
        W, H, ctx, &srgb, &p3, ALWAN_ALPHA_STRAIGHT);

    TEST_ASSERT(status == ALWAN_OK, "rgba U8->F32 returned error");

    alwan_f64 const f32_tol = ALWAN_LITERAL(1e-5);
    for (size_t i = 0; i < W; i++) {
        alwan_rgb rgb_in = {
            (alwan_f64)u8_pixels[i][0] / ALWAN_LITERAL(255.0),
            (alwan_f64)u8_pixels[i][1] / ALWAN_LITERAL(255.0),
            (alwan_f64)u8_pixels[i][2] / ALWAN_LITERAL(255.0)
        };
        alwan_rgb rgb_ref;
        ref_image_pixel(&rgb_ref, ctx, &srgb, &p3, &rgb_in);

        alwan_f64 expected_alpha = (alwan_f64)u8_pixels[i][3] / ALWAN_LITERAL(255.0);

        char msg[128];
        snprintf(msg, sizeof(msg), "rgba U8->F32 pixel %zu R", i);
        TEST_ASSERT_NEAR((alwan_f64)dst_f32[i * 4 + 0], rgb_ref.r, f32_tol, msg);
        snprintf(msg, sizeof(msg), "rgba U8->F32 pixel %zu G", i);
        TEST_ASSERT_NEAR((alwan_f64)dst_f32[i * 4 + 1], rgb_ref.g, f32_tol, msg);
        snprintf(msg, sizeof(msg), "rgba U8->F32 pixel %zu B", i);
        TEST_ASSERT_NEAR((alwan_f64)dst_f32[i * 4 + 2], rgb_ref.b, f32_tol, msg);
        snprintf(msg, sizeof(msg), "rgba U8->F32 pixel %zu A", i);
        TEST_ASSERT_NEAR((alwan_f64)dst_f32[i * 4 + 3], expected_alpha, f32_tol, msg);
    }

    alwan_destroy(ctx);
    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * Test: RGBA in-place conversion
 * ---------------------------------------------------------------- */
static int test_image_convert_rgba_in_place(void) {
    TEST_START("image_convert_rgba in-place (src == dst)");

    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) TEST_FAIL("failed to create context");

    alwan_rgb_space_desc srgb = {0};
    alwan_rgb_space_desc p3 = {0};
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_P3_D65);

    size_t const W = 2, H = 1;
    alwan_f64 buf[2 * 4];

    alwan_f64 const pixels[][4] = {
        {0.8, 0.2, 0.1, 0.75},
        {0.3, 0.7, 0.5, 1.0},
    };

    /* Compute reference first */
    alwan_f64 ref[2 * 4];
    for (size_t i = 0; i < W; i++) {
        alwan_rgb rgb_in = {pixels[i][0], pixels[i][1], pixels[i][2]};
        alwan_rgb rgb_ref;
        ref_image_pixel(&rgb_ref, ctx, &srgb, &p3, &rgb_in);
        ref[i * 4 + 0] = rgb_ref.r;
        ref[i * 4 + 1] = rgb_ref.g;
        ref[i * 4 + 2] = rgb_ref.b;
        ref[i * 4 + 3] = pixels[i][3]; /* alpha preserved */
    }

    /* Fill and convert in-place */
    for (size_t i = 0; i < W; i++) {
        buf[i * 4 + 0] = pixels[i][0];
        buf[i * 4 + 1] = pixels[i][1];
        buf[i * 4 + 2] = pixels[i][2];
        buf[i * 4 + 3] = pixels[i][3];
    }

    size_t row_stride = W * 4 * sizeof(alwan_f64);
    int status = alwan_image_convert_rgba(
        buf, ALWAN_PIXEL_F64, row_stride,
        buf, ALWAN_PIXEL_F64, row_stride,
        W, H, ctx, &srgb, &p3, ALWAN_ALPHA_STRAIGHT);

    TEST_ASSERT(status == ALWAN_OK, "rgba in-place returned error");

    for (size_t i = 0; i < W; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "rgba in-place pixel %zu R", i);
        TEST_ASSERT_NEAR(buf[i * 4 + 0], ref[i * 4 + 0], ALWAN_TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "rgba in-place pixel %zu G", i);
        TEST_ASSERT_NEAR(buf[i * 4 + 1], ref[i * 4 + 1], ALWAN_TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "rgba in-place pixel %zu B", i);
        TEST_ASSERT_NEAR(buf[i * 4 + 2], ref[i * 4 + 2], ALWAN_TEST_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "rgba in-place pixel %zu A", i);
        TEST_ASSERT_NEAR(buf[i * 4 + 3], ref[i * 4 + 3], ALWAN_LITERAL(1e-15), msg);
    }

    alwan_destroy(ctx);
    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * SIMD coverage tests for alwan_image_convert_rgba
 *
 * Use MIN_SIMD_PIXELS = 33 pixels so the SIMD inner loop executes
 * (not just the scalar tail).  Compare against ref_image_pixel
 * which processes 3 values at a time — always scalar on any target.
 * ---------------------------------------------------------------- */

static int test_image_convert_rgba_simd_straight(void) {
    TEST_START("image_convert_rgba SIMD straight alpha");

    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) TEST_FAIL("failed to create context");

    alwan_rgb_space_desc srgb = {0};
    alwan_rgb_space_desc p3   = {0};
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3,   ctx, ALWAN_RGB_SPACE_P3_D65);

    size_t const W = MIN_SIMD_PIXELS, H = 1;
    alwan_f64 src[MIN_SIMD_PIXELS * 4];
    alwan_f64 dst[MIN_SIMD_PIXELS * 4];

    /* Fill with a structured ramp: R/G/B in [0,1], alpha varying */
    for (size_t i = 0; i < W; i++) {
        alwan_f64 t = (alwan_f64)i / (alwan_f64)(W - 1);
        src[i * 4 + 0] = t;
        src[i * 4 + 1] = ALWAN_LITERAL(1.0) - t;
        src[i * 4 + 2] = t * ALWAN_LITERAL(0.5) + ALWAN_LITERAL(0.1);
        src[i * 4 + 3] = (i % 4 == 0) ? ALWAN_LITERAL(0.0) : t; /* include alpha=0 */
    }

    size_t const row_stride = W * 4 * sizeof(alwan_f64);
    int status = alwan_image_convert_rgba(
        dst, ALWAN_PIXEL_F64, row_stride,
        src, ALWAN_PIXEL_F64, row_stride,
        W, H, ctx, &srgb, &p3, ALWAN_ALPHA_STRAIGHT);
    TEST_ASSERT(status == ALWAN_OK, "rgba SIMD straight failed");

    for (size_t i = 0; i < W; i++) {
        alwan_rgb rgb_in = {src[i * 4 + 0], src[i * 4 + 1], src[i * 4 + 2]};
        alwan_rgb ref;
        ref_image_pixel(&ref, ctx, &srgb, &p3, &rgb_in);

        char msg[128];
        snprintf(msg, sizeof(msg), "SIMD straight px %zu R", i);
        TEST_ASSERT_NEAR(dst[i * 4 + 0], ref.r, ALWAN_SIMD_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "SIMD straight px %zu G", i);
        TEST_ASSERT_NEAR(dst[i * 4 + 1], ref.g, ALWAN_SIMD_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "SIMD straight px %zu B", i);
        TEST_ASSERT_NEAR(dst[i * 4 + 2], ref.b, ALWAN_SIMD_TOLERANCE, msg);

        /* Alpha must be bit-exact pass-through */
        snprintf(msg, sizeof(msg), "SIMD straight px %zu A", i);
        TEST_ASSERT_NEAR(dst[i * 4 + 3], src[i * 4 + 3], ALWAN_LITERAL(1e-15), msg);
    }

    alwan_destroy(ctx);
    TEST_PASS_MSG();
    return 0;
}

static int test_image_convert_rgba_simd_premul(void) {
    TEST_START("image_convert_rgba SIMD premultiplied alpha");

    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) TEST_FAIL("failed to create context");

    alwan_rgb_space_desc srgb = {0};
    alwan_rgb_space_desc p3   = {0};
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3,   ctx, ALWAN_RGB_SPACE_P3_D65);

    size_t const W = MIN_SIMD_PIXELS, H = 1;
    alwan_f64 src[MIN_SIMD_PIXELS * 4];
    alwan_f64 dst[MIN_SIMD_PIXELS * 4];

    /* Build premultiplied source from straight values */
    for (size_t i = 0; i < W; i++) {
        alwan_f64 t = (alwan_f64)(i + 1) / (alwan_f64)W;
        alwan_f64 a = (i == W / 2) ? ALWAN_LITERAL(0.0) : t; /* one zero-alpha pixel */
        alwan_f64 r = t * ALWAN_LITERAL(0.8);
        alwan_f64 g = (ALWAN_LITERAL(1.0) - t) * ALWAN_LITERAL(0.6);
        alwan_f64 b = t * ALWAN_LITERAL(0.4) + ALWAN_LITERAL(0.05);
        src[i * 4 + 0] = r * a;
        src[i * 4 + 1] = g * a;
        src[i * 4 + 2] = b * a;
        src[i * 4 + 3] = a;
    }

    size_t const row_stride = W * 4 * sizeof(alwan_f64);
    int status = alwan_image_convert_rgba(
        dst, ALWAN_PIXEL_F64, row_stride,
        src, ALWAN_PIXEL_F64, row_stride,
        W, H, ctx, &srgb, &p3, ALWAN_ALPHA_PREMULTIPLIED);
    TEST_ASSERT(status == ALWAN_OK, "rgba SIMD premul failed");

    for (size_t i = 0; i < W; i++) {
        alwan_f64 a = src[i * 4 + 3];

        /* Reference: unpremultiply -> convert -> repremultiply */
        alwan_rgb straight_in;
        if (a > ALWAN_LITERAL(0.0)) {
            alwan_f64 inv_a = ALWAN_LITERAL(1.0) / a;
            straight_in.r = src[i * 4 + 0] * inv_a;
            straight_in.g = src[i * 4 + 1] * inv_a;
            straight_in.b = src[i * 4 + 2] * inv_a;
        } else {
            straight_in.r = straight_in.g = straight_in.b = ALWAN_LITERAL(0.0);
        }
        alwan_rgb ref_straight;
        ref_image_pixel(&ref_straight, ctx, &srgb, &p3, &straight_in);

        alwan_f64 ref_r = ref_straight.r * a;
        alwan_f64 ref_g = ref_straight.g * a;
        alwan_f64 ref_b = ref_straight.b * a;

        char msg[128];
        snprintf(msg, sizeof(msg), "SIMD premul px %zu R", i);
        TEST_ASSERT_NEAR(dst[i * 4 + 0], ref_r, ALWAN_SIMD_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "SIMD premul px %zu G", i);
        TEST_ASSERT_NEAR(dst[i * 4 + 1], ref_g, ALWAN_SIMD_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "SIMD premul px %zu B", i);
        TEST_ASSERT_NEAR(dst[i * 4 + 2], ref_b, ALWAN_SIMD_TOLERANCE, msg);
        snprintf(msg, sizeof(msg), "SIMD premul px %zu A", i);
        TEST_ASSERT_NEAR(dst[i * 4 + 3], a, ALWAN_LITERAL(1e-15), msg);
    }

    alwan_destroy(ctx);
    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */
int test_75_image_convert_main(void) {
    test_count = 0;
    test_passed = 0;
    test_failed = 0;

    int result = 0;
    result |= test_image_convert_same_wp();
    result |= test_image_convert_diff_wp();
    result |= test_image_convert_u8_to_f32();
    result |= test_image_convert_row_stride();
    result |= test_image_convert_in_place();
    result |= test_image_convert_errors();
    result |= test_image_convert_rgba_straight();
    result |= test_image_convert_rgba_premul();
    result |= test_image_convert_rgba_u8_f32();
    result |= test_image_convert_rgba_in_place();
    result |= test_image_convert_rgba_simd_straight();
    result |= test_image_convert_rgba_simd_premul();

    printf("\n  Summary: %d/%d passed\n", test_passed, test_count);
    return result;
}
