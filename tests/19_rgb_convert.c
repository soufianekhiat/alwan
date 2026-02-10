/*
 * RGB-to-RGB Conversion Tests
 */

#include "test_common.h"

/* Test RGB space descriptors */
static alwan_rgb_space_desc get_srgb_desc(void) {
    alwan_rgb_space_desc desc;
    /* sRGB primaries: R(0.64, 0.33), G(0.30, 0.60), B(0.15, 0.06) */
    desc.primaries_xy[0] = ALWAN_LITERAL(0.64);
    desc.primaries_xy[1] = ALWAN_LITERAL(0.33);
    desc.primaries_xy[2] = ALWAN_LITERAL(0.30);
    desc.primaries_xy[3] = ALWAN_LITERAL(0.60);
    desc.primaries_xy[4] = ALWAN_LITERAL(0.15);
    desc.primaries_xy[5] = ALWAN_LITERAL(0.06);
    /* D65 white point */
    desc.white_xy[0] = ALWAN_LITERAL(0.3127);
    desc.white_xy[1] = ALWAN_LITERAL(0.3290);
    desc.oetf = ALWAN_TF_SRGB;
    desc.eotf = ALWAN_TF_SRGB;
    return desc;
}

static alwan_rgb_space_desc get_display_p3_desc(void) {
    alwan_rgb_space_desc desc;
    /* Display P3 primaries: R(0.68, 0.32), G(0.265, 0.69), B(0.15, 0.06) */
    desc.primaries_xy[0] = ALWAN_LITERAL(0.68);
    desc.primaries_xy[1] = ALWAN_LITERAL(0.32);
    desc.primaries_xy[2] = ALWAN_LITERAL(0.265);
    desc.primaries_xy[3] = ALWAN_LITERAL(0.69);
    desc.primaries_xy[4] = ALWAN_LITERAL(0.15);
    desc.primaries_xy[5] = ALWAN_LITERAL(0.06);
    /* D65 white point */
    desc.white_xy[0] = ALWAN_LITERAL(0.3127);
    desc.white_xy[1] = ALWAN_LITERAL(0.3290);
    desc.oetf = ALWAN_TF_LINEAR;
    desc.eotf = ALWAN_TF_LINEAR;
    return desc;
}

static alwan_rgb_space_desc get_bt2020_desc(void) {
    alwan_rgb_space_desc desc;
    /* BT.2020 primaries: R(0.708, 0.292), G(0.170, 0.797), B(0.131, 0.046) */
    desc.primaries_xy[0] = ALWAN_LITERAL(0.708);
    desc.primaries_xy[1] = ALWAN_LITERAL(0.292);
    desc.primaries_xy[2] = ALWAN_LITERAL(0.170);
    desc.primaries_xy[3] = ALWAN_LITERAL(0.797);
    desc.primaries_xy[4] = ALWAN_LITERAL(0.131);
    desc.primaries_xy[5] = ALWAN_LITERAL(0.046);
    /* D65 white point */
    desc.white_xy[0] = ALWAN_LITERAL(0.3127);
    desc.white_xy[1] = ALWAN_LITERAL(0.3290);
    desc.oetf = ALWAN_TF_LINEAR;
    desc.eotf = ALWAN_TF_LINEAR;
    return desc;
}

static alwan_rgb_space_desc get_acescg_desc(void) {
    alwan_rgb_space_desc desc;
    /* ACEScg (AP1) primaries: R(0.713, 0.293), G(0.165, 0.830), B(0.128, 0.044) */
    desc.primaries_xy[0] = ALWAN_LITERAL(0.713);
    desc.primaries_xy[1] = ALWAN_LITERAL(0.293);
    desc.primaries_xy[2] = ALWAN_LITERAL(0.165);
    desc.primaries_xy[3] = ALWAN_LITERAL(0.830);
    desc.primaries_xy[4] = ALWAN_LITERAL(0.128);
    desc.primaries_xy[5] = ALWAN_LITERAL(0.044);
    /* D60 white point */
    desc.white_xy[0] = ALWAN_LITERAL(0.32168);
    desc.white_xy[1] = ALWAN_LITERAL(0.33767);
    desc.oetf = ALWAN_TF_LINEAR;
    desc.eotf = ALWAN_TF_LINEAR;
    return desc;
}

/* Test sRGB to Display P3 conversion (same white point) */
static int test_srgb_to_p3(alwan_ctx *ctx) {
    static alwan_scalar const test_data[] = {
#include "data/fixtures/rgb_convert_srgb_to_p3.csv"
    };

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;  /* 6 values per color (src + dst RGB) */

    alwan_rgb_space_desc srgb = get_srgb_desc();
    alwan_rgb_space_desc p3 = get_display_p3_desc();

    for (size_t i = 0; i < num_colors; i++) {
        alwan_rgb src_rgb, expected_rgb, result_rgb;
        src_rgb.r = test_data[i * 6 + 0];
        src_rgb.g = test_data[i * 6 + 1];
        src_rgb.b = test_data[i * 6 + 2];
        expected_rgb.r = test_data[i * 6 + 3];
        expected_rgb.g = test_data[i * 6 + 4];
        expected_rgb.b = test_data[i * 6 + 5];

        int status = alwan_rgb_convert(&result_rgb, ctx, &srgb, &p3, &src_rgb);
        TEST_ASSERT(status == ALWAN_OK, "Conversion failed");

        alwan_scalar diff_r = ALWAN_ABS(result_rgb.r - expected_rgb.r);
        alwan_scalar diff_g = ALWAN_ABS(result_rgb.g - expected_rgb.g);
        alwan_scalar diff_b = ALWAN_ABS(result_rgb.b - expected_rgb.b);

        if (diff_r > TEST_TOLERANCE || diff_g > TEST_TOLERANCE || diff_b > TEST_TOLERANCE) {
            printf("sRGB->P3 color %zu failed:\n", i);
            printf("  Source: [%.6f, %.6f, %.6f]\n", src_rgb.r, src_rgb.g, src_rgb.b);
            printf("  Expected: [%.6f, %.6f, %.6f]\n", expected_rgb.r, expected_rgb.g, expected_rgb.b);
            printf("  Got:      [%.6f, %.6f, %.6f]\n", result_rgb.r, result_rgb.g, result_rgb.b);
            printf("  Diff:     [%e, %e, %e]\n", diff_r, diff_g, diff_b);
            TEST_ASSERT(0, "RGB conversion values don't match");
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("sRGB to Display P3");
}

/* Test sRGB to BT.2020 conversion (same white point, wider gamut) */
static int test_srgb_to_bt2020(alwan_ctx *ctx) {
    static alwan_scalar const test_data[] = {
#include "data/fixtures/rgb_convert_srgb_to_bt2020.csv"
    };

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    alwan_rgb_space_desc srgb = get_srgb_desc();
    alwan_rgb_space_desc bt2020 = get_bt2020_desc();

    for (size_t i = 0; i < num_colors; i++) {
        alwan_rgb src_rgb, expected_rgb, result_rgb;
        src_rgb.r = test_data[i * 6 + 0];
        src_rgb.g = test_data[i * 6 + 1];
        src_rgb.b = test_data[i * 6 + 2];
        expected_rgb.r = test_data[i * 6 + 3];
        expected_rgb.g = test_data[i * 6 + 4];
        expected_rgb.b = test_data[i * 6 + 5];

        int status = alwan_rgb_convert(&result_rgb, ctx, &srgb, &bt2020, &src_rgb);
        TEST_ASSERT(status == ALWAN_OK, "Conversion failed");

        alwan_scalar diff_r = ALWAN_ABS(result_rgb.r - expected_rgb.r);
        alwan_scalar diff_g = ALWAN_ABS(result_rgb.g - expected_rgb.g);
        alwan_scalar diff_b = ALWAN_ABS(result_rgb.b - expected_rgb.b);

        if (diff_r > TEST_TOLERANCE || diff_g > TEST_TOLERANCE || diff_b > TEST_TOLERANCE) {
            printf("sRGB->BT2020 color %zu failed:\n", i);
            printf("  Source: [%.6f, %.6f, %.6f]\n", src_rgb.r, src_rgb.g, src_rgb.b);
            printf("  Expected: [%.6f, %.6f, %.6f]\n", expected_rgb.r, expected_rgb.g, expected_rgb.b);
            printf("  Got:      [%.6f, %.6f, %.6f]\n", result_rgb.r, result_rgb.g, result_rgb.b);
            printf("  Diff:     [%e, %e, %e]\n", diff_r, diff_g, diff_b);
            TEST_ASSERT(0, "RGB conversion values don't match");
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("sRGB to BT.2020");
}

/* Test sRGB to ACEScg conversion (different white point, needs CAT) */
static int test_srgb_to_acescg(alwan_ctx *ctx) {
    static alwan_scalar const test_data[] = {
#include "data/fixtures/rgb_convert_srgb_to_acescg.csv"
    };

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    alwan_rgb_space_desc srgb = get_srgb_desc();
    alwan_rgb_space_desc acescg = get_acescg_desc();

    for (size_t i = 0; i < num_colors; i++) {
        alwan_rgb src_rgb, expected_rgb, result_rgb;
        src_rgb.r = test_data[i * 6 + 0];
        src_rgb.g = test_data[i * 6 + 1];
        src_rgb.b = test_data[i * 6 + 2];
        expected_rgb.r = test_data[i * 6 + 3];
        expected_rgb.g = test_data[i * 6 + 4];
        expected_rgb.b = test_data[i * 6 + 5];

        int status = alwan_rgb_convert(&result_rgb, ctx, &srgb, &acescg, &src_rgb);
        TEST_ASSERT(status == ALWAN_OK, "Conversion failed");

        alwan_scalar diff_r = ALWAN_ABS(result_rgb.r - expected_rgb.r);
        alwan_scalar diff_g = ALWAN_ABS(result_rgb.g - expected_rgb.g);
        alwan_scalar diff_b = ALWAN_ABS(result_rgb.b - expected_rgb.b);

        if (diff_r > TEST_TOLERANCE || diff_g > TEST_TOLERANCE || diff_b > TEST_TOLERANCE) {
            printf("sRGB->ACEScg color %zu failed:\n", i);
            printf("  Source: [%.6f, %.6f, %.6f]\n", src_rgb.r, src_rgb.g, src_rgb.b);
            printf("  Expected: [%.6f, %.6f, %.6f]\n", expected_rgb.r, expected_rgb.g, expected_rgb.b);
            printf("  Got:      [%.6f, %.6f, %.6f]\n", result_rgb.r, result_rgb.g, result_rgb.b);
            printf("  Diff:     [%e, %e, %e]\n", diff_r, diff_g, diff_b);
            TEST_ASSERT(0, "RGB conversion values don't match");
        }
    }

    printf("  Tested %zu colors (with chromatic adaptation)\n", num_colors);
    TEST_PASS("sRGB to ACEScg");
}

/* Test bulk conversion */
static int test_bulk_conversion(alwan_ctx *ctx) {
    alwan_rgb_space_desc srgb = get_srgb_desc();
    alwan_rgb_space_desc p3 = get_display_p3_desc();

    /* Test with 5 colors */
    alwan_rgb src_colors[5] = {
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},  /* Red */
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)},  /* Green */
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)},  /* Blue */
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},  /* White */
        {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)}   /* Gray */
    };
    alwan_rgb dst_colors_bulk[5];
    alwan_rgb dst_colors_single[5];

    /* Convert using bulk function */
    int status = alwan_rgb_convert_bulk(dst_colors_bulk, ctx, &srgb, &p3, src_colors, 5);
    TEST_ASSERT(status == ALWAN_OK, "Bulk conversion failed");

    /* Convert using single function for comparison */
    for (size_t i = 0; i < 5; i++) {
        status = alwan_rgb_convert(&dst_colors_single[i], ctx, &srgb, &p3, &src_colors[i]);
        TEST_ASSERT(status == ALWAN_OK, "Single conversion failed");
    }

    /* Compare results */
    for (size_t i = 0; i < 5; i++) {
        alwan_scalar diff_r = ALWAN_ABS(dst_colors_bulk[i].r - dst_colors_single[i].r);
        alwan_scalar diff_g = ALWAN_ABS(dst_colors_bulk[i].g - dst_colors_single[i].g);
        alwan_scalar diff_b = ALWAN_ABS(dst_colors_bulk[i].b - dst_colors_single[i].b);

        if (diff_r > TEST_TOLERANCE || diff_g > TEST_TOLERANCE || diff_b > TEST_TOLERANCE) {
            printf("Bulk conversion mismatch for color %zu:\n", i);
            printf("  Bulk:   [%.6f, %.6f, %.6f]\n",
                   dst_colors_bulk[i].r, dst_colors_bulk[i].g, dst_colors_bulk[i].b);
            printf("  Single: [%.6f, %.6f, %.6f]\n",
                   dst_colors_single[i].r, dst_colors_single[i].g, dst_colors_single[i].b);
            TEST_ASSERT(0, "Bulk and single conversion results differ");
        }
    }

    printf("  Tested 5 colors in bulk\n");
    TEST_PASS("Bulk conversion");
}

/* Main test runner for RGB convert */
int test_19_rgb_convert_main(void) {
    printf("=== RGB-to-RGB Conversion Tests ===\n");

    alwan_ctx *ctx = alwan_create(NULL);
    if (!ctx) {
        printf("[FAIL] Failed to create context\n");
        return 1;
    }

    int failures = 0;

    failures += test_srgb_to_p3(ctx);
    failures += test_srgb_to_bt2020(ctx);
    failures += test_srgb_to_acescg(ctx);
    failures += test_bulk_conversion(ctx);

    alwan_destroy(ctx);

    if (failures == 0) {
        printf("\n=== All M11 tests passed ===\n");
    }

    return failures;
}
