/*
 * RGB-to-RGB Conversion Tests
 */

#include "test_common.h"

/* Test RGB space descriptors */
static alwan_rgb_space_desc get_srgb_desc(void) {
    alwan_rgb_space_desc desc;
    desc.primaries_xy[0] = ALWAN_BT709_RED_x;
    desc.primaries_xy[1] = ALWAN_BT709_RED_y;
    desc.primaries_xy[2] = ALWAN_BT709_GREEN_x;
    desc.primaries_xy[3] = ALWAN_BT709_GREEN_y;
    desc.primaries_xy[4] = ALWAN_BT709_BLUE_x;
    desc.primaries_xy[5] = ALWAN_BT709_BLUE_y;
    desc.white_xy[0] = ALWAN_LITERAL(0.3127);
    desc.white_xy[1] = ALWAN_LITERAL(0.3290);
    desc.oetf = ALWAN_TF_SRGB;
    desc.eotf = ALWAN_TF_SRGB;
    return desc;
}

static alwan_rgb_space_desc get_display_p3_desc(void) {
    alwan_rgb_space_desc desc;
    desc.primaries_xy[0] = ALWAN_P3_RED_x;
    desc.primaries_xy[1] = ALWAN_P3_RED_y;
    desc.primaries_xy[2] = ALWAN_P3_GREEN_x;
    desc.primaries_xy[3] = ALWAN_P3_GREEN_y;
    desc.primaries_xy[4] = ALWAN_P3_BLUE_x;
    desc.primaries_xy[5] = ALWAN_P3_BLUE_y;
    desc.white_xy[0] = ALWAN_LITERAL(0.3127);
    desc.white_xy[1] = ALWAN_LITERAL(0.3290);
    desc.oetf = ALWAN_TF_LINEAR;
    desc.eotf = ALWAN_TF_LINEAR;
    return desc;
}

static alwan_rgb_space_desc get_bt2020_desc(void) {
    alwan_rgb_space_desc desc;
    desc.primaries_xy[0] = ALWAN_BT2020_RED_x;
    desc.primaries_xy[1] = ALWAN_BT2020_RED_y;
    desc.primaries_xy[2] = ALWAN_BT2020_GREEN_x;
    desc.primaries_xy[3] = ALWAN_BT2020_GREEN_y;
    desc.primaries_xy[4] = ALWAN_BT2020_BLUE_x;
    desc.primaries_xy[5] = ALWAN_BT2020_BLUE_y;
    desc.white_xy[0] = ALWAN_LITERAL(0.3127);
    desc.white_xy[1] = ALWAN_LITERAL(0.3290);
    desc.oetf = ALWAN_TF_LINEAR;
    desc.eotf = ALWAN_TF_LINEAR;
    return desc;
}

static alwan_rgb_space_desc get_acescg_desc(void) {
    alwan_rgb_space_desc desc;
    desc.primaries_xy[0] = ALWAN_AP1_RED_x;
    desc.primaries_xy[1] = ALWAN_AP1_RED_y;
    desc.primaries_xy[2] = ALWAN_AP1_GREEN_x;
    desc.primaries_xy[3] = ALWAN_AP1_GREEN_y;
    desc.primaries_xy[4] = ALWAN_AP1_BLUE_x;
    desc.primaries_xy[5] = ALWAN_AP1_BLUE_y;
    /* D60 white point */
    desc.white_xy[0] = ALWAN_LITERAL(0.32168);
    desc.white_xy[1] = ALWAN_LITERAL(0.33767);
    desc.oetf = ALWAN_TF_LINEAR;
    desc.eotf = ALWAN_TF_LINEAR;
    return desc;
}

/* Test sRGB to Display P3 conversion (same white point) */
static int test_srgb_to_p3(alwan_ctx *ctx) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const test_data[] = {
#include "data/fixtures/rgb_convert_srgb_to_p3.csv"
    };
    ALWAN_DIAG_POP

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

        if (diff_r > ALWAN_TEST_TOLERANCE || diff_g > ALWAN_TEST_TOLERANCE || diff_b > ALWAN_TEST_TOLERANCE) {
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
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const test_data[] = {
#include "data/fixtures/rgb_convert_srgb_to_bt2020.csv"
    };
    ALWAN_DIAG_POP

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

        if (diff_r > ALWAN_TEST_TOLERANCE || diff_g > ALWAN_TEST_TOLERANCE || diff_b > ALWAN_TEST_TOLERANCE) {
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
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const test_data[] = {
#include "data/fixtures/rgb_convert_srgb_to_acescg.csv"
    };
    ALWAN_DIAG_POP

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

        if (diff_r > ALWAN_TEST_TOLERANCE || diff_g > ALWAN_TEST_TOLERANCE || diff_b > ALWAN_TEST_TOLERANCE) {
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

    alwan_destroy(ctx);

    if (failures == 0) {
        printf("\n=== All M11 tests passed ===\n");
    }

    return failures;
}
