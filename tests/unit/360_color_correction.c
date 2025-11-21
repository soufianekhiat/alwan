/* Test suite for Color Correction & Grading Tools */

#include "../../src/alwan/alwan.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#define TEST_TOLERANCE 1e-5

/* Math helper macros */
#if ALWAN_SCALAR_IS_FLOAT
  #define TEST_FABS(x) fabsf(x)
  #define TEST_POW(x, y) powf(x, y)
#else
  #define TEST_FABS(x) fabs(x)
  #define TEST_POW(x, y) pow(x, y)
#endif

static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    test_count++; \
    if (!(cond)) { \
        printf("[FAIL] %s\n", msg); \
        test_failed++; \
        return 1; \
    } \
    test_passed++; \
} while(0)

#define TEST_ASSERT_NEAR(a, b, tol, msg) do { \
    test_count++; \
    alwan_scalar diff = TEST_FABS((a) - (b)); \
    if (diff > (tol)) { \
        printf("[FAIL] %s: expected %.10f, got %.10f (diff=%.10f)\n", msg, (double)(b), (double)(a), (double)diff); \
        test_failed++; \
        return 1; \
    } \
    test_passed++; \
} while(0)

/* ================================================================
 * Lift/Gamma/Gain Tests
 * ================================================================ */

static int test_lgg_neutral(void)
{
    printf("  TEST: LGG with neutral values (no change)\n");

    alwan_vec3 rgb_in = {{0.5, 0.5, 0.5}};
    alwan_vec3 lift = {{0.0, 0.0, 0.0}};    /* No lift */
    alwan_vec3 gamma = {{1.0, 1.0, 1.0}};   /* No gamma change */
    alwan_vec3 gain = {{1.0, 1.0, 1.0}};    /* No gain */
    alwan_vec3 rgb_out;

    int status = alwan_lgg_apply(&rgb_in, &lift, &gamma, &gain, &rgb_out);
    TEST_ASSERT(status == ALWAN_OK, "LGG apply failed");

    /* With neutral values, output should match input */
    TEST_ASSERT_NEAR(rgb_out.v[0], 0.5, TEST_TOLERANCE, "Red channel should be unchanged");
    TEST_ASSERT_NEAR(rgb_out.v[1], 0.5, TEST_TOLERANCE, "Green channel should be unchanged");
    TEST_ASSERT_NEAR(rgb_out.v[2], 0.5, TEST_TOLERANCE, "Blue channel should be unchanged");

    return 0;
}

static int test_lgg_lift(void)
{
    printf("  TEST: LGG lift adjustment (shadows)\n");

    alwan_vec3 rgb_in = {{0.2, 0.3, 0.4}};
    alwan_vec3 lift = {{0.1, 0.0, -0.1}};   /* Lift red, neutral green, lower blue */
    alwan_vec3 gamma = {{1.0, 1.0, 1.0}};
    alwan_vec3 gain = {{1.0, 1.0, 1.0}};
    alwan_vec3 rgb_out;

    int status = alwan_lgg_apply(&rgb_in, &lift, &gamma, &gain, &rgb_out);
    TEST_ASSERT(status == ALWAN_OK, "LGG apply failed");

    /* Lift adds to input: (0.2 + 0.1) = 0.3 for red */
    TEST_ASSERT_NEAR(rgb_out.v[0], 0.3, TEST_TOLERANCE, "Red lifted by 0.1");
    TEST_ASSERT_NEAR(rgb_out.v[1], 0.3, TEST_TOLERANCE, "Green unchanged");
    TEST_ASSERT_NEAR(rgb_out.v[2], 0.3, TEST_TOLERANCE, "Blue lowered by 0.1");

    return 0;
}

static int test_lgg_gamma(void)
{
    printf("  TEST: LGG gamma adjustment (midtones)\n");

    alwan_vec3 rgb_in = {{0.5, 0.5, 0.5}};
    alwan_vec3 lift = {{0.0, 0.0, 0.0}};
    alwan_vec3 gamma = {{2.0, 0.5, 1.0}};   /* Darken red, brighten green, neutral blue */
    alwan_vec3 gain = {{1.0, 1.0, 1.0}};
    alwan_vec3 rgb_out;

    int status = alwan_lgg_apply(&rgb_in, &lift, &gamma, &gain, &rgb_out);
    TEST_ASSERT(status == ALWAN_OK, "LGG apply failed");

    /* Gamma 2.0: 0.5^(1/2) = 0.707... */
    TEST_ASSERT_NEAR(rgb_out.v[0], 0.707106781, 0.0001, "Red darkened (gamma 2.0)");
    /* Gamma 0.5: 0.5^(1/0.5) = 0.5^2 = 0.25 */
    TEST_ASSERT_NEAR(rgb_out.v[1], 0.25, TEST_TOLERANCE, "Green brightened (gamma 0.5)");
    /* Gamma 1.0: no change */
    TEST_ASSERT_NEAR(rgb_out.v[2], 0.5, TEST_TOLERANCE, "Blue unchanged");

    return 0;
}

static int test_lgg_gain(void)
{
    printf("  TEST: LGG gain adjustment (highlights)\n");

    alwan_vec3 rgb_in = {{0.5, 0.5, 0.5}};
    alwan_vec3 lift = {{0.0, 0.0, 0.0}};
    alwan_vec3 gamma = {{1.0, 1.0, 1.0}};
    alwan_vec3 gain = {{2.0, 0.5, 1.0}};    /* Double red, halve green, neutral blue */
    alwan_vec3 rgb_out;

    int status = alwan_lgg_apply(&rgb_in, &lift, &gamma, &gain, &rgb_out);
    TEST_ASSERT(status == ALWAN_OK, "LGG apply failed");

    TEST_ASSERT_NEAR(rgb_out.v[0], 1.0, TEST_TOLERANCE, "Red gain doubled");
    TEST_ASSERT_NEAR(rgb_out.v[1], 0.25, TEST_TOLERANCE, "Green gain halved");
    TEST_ASSERT_NEAR(rgb_out.v[2], 0.5, TEST_TOLERANCE, "Blue unchanged");

    return 0;
}

static int test_lgg_combined(void)
{
    printf("  TEST: LGG combined adjustments\n");

    /* Reference values from colour-science (generate_data_tests.ps1) */
    static alwan_scalar const ref_lgg_combined[] = {
#include "reference_values/lgg_combined.csv"
    };

    alwan_vec3 rgb_in = {{0.3, 0.5, 0.7}};
    alwan_vec3 lift = {{0.1, 0.0, -0.1}};
    alwan_vec3 gamma = {{1.2, 1.0, 0.8}};
    alwan_vec3 gain = {{1.1, 1.0, 0.9}};
    alwan_vec3 rgb_out;

    int status = alwan_lgg_apply(&rgb_in, &lift, &gamma, &gain, &rgb_out);
    TEST_ASSERT(status == ALWAN_OK, "LGG apply failed");

    /* Compare with reference values from colour-science */
    TEST_ASSERT_NEAR(rgb_out.v[0], ref_lgg_combined[0], TEST_TOLERANCE, "Red combined adjustment");
    TEST_ASSERT_NEAR(rgb_out.v[1], ref_lgg_combined[1], TEST_TOLERANCE, "Green combined adjustment");
    TEST_ASSERT_NEAR(rgb_out.v[2], ref_lgg_combined[2], TEST_TOLERANCE, "Blue combined adjustment");

    return 0;
}

/* ================================================================
 * Color Matrix Tests
 * ================================================================ */

static int test_color_matrix_identity(void)
{
    printf("  TEST: Color matrix with identity matrix\n");

    alwan_vec3 rgb_in = {{0.5, 0.6, 0.7}};
    alwan_mat3x3 identity;
    alwan_mat3_identity(&identity);
    alwan_vec3 rgb_out;

    int status = alwan_color_matrix_apply(&rgb_in, &identity, &rgb_out);
    TEST_ASSERT(status == ALWAN_OK, "Color matrix apply failed");

    TEST_ASSERT_NEAR(rgb_out.v[0], 0.5, TEST_TOLERANCE, "Red unchanged");
    TEST_ASSERT_NEAR(rgb_out.v[1], 0.6, TEST_TOLERANCE, "Green unchanged");
    TEST_ASSERT_NEAR(rgb_out.v[2], 0.7, TEST_TOLERANCE, "Blue unchanged");

    return 0;
}

static int test_color_matrix_sepia(void)
{
    printf("  TEST: Sepia color matrix preset\n");

    /* Reference values from colour-science (generate_data_tests.ps1) */
    static alwan_scalar const ref_sepia[] = {
#include "reference_values/color_matrix_sepia.csv"
    };

    alwan_mat3x3 sepia_matrix;
    int status = alwan_color_matrix_get_preset(ALWAN_COLOR_MATRIX_SEPIA, &sepia_matrix);
    TEST_ASSERT(status == ALWAN_OK, "Get sepia preset failed");

    /* Test with mid-gray */
    alwan_vec3 rgb_in = {{0.5, 0.5, 0.5}};
    alwan_vec3 rgb_out;

    status = alwan_color_matrix_apply(&rgb_in, &sepia_matrix, &rgb_out);
    TEST_ASSERT(status == ALWAN_OK, "Apply sepia failed");

    /* Compare with reference values from colour-science */
    TEST_ASSERT_NEAR(rgb_out.v[0], ref_sepia[0], TEST_TOLERANCE, "Sepia red channel");
    TEST_ASSERT_NEAR(rgb_out.v[1], ref_sepia[1], TEST_TOLERANCE, "Sepia green channel");
    TEST_ASSERT_NEAR(rgb_out.v[2], ref_sepia[2], TEST_TOLERANCE, "Sepia blue channel");

    /* Sepia should produce warm brownish tones */
    TEST_ASSERT(rgb_out.v[0] > rgb_out.v[1], "Sepia red > green");
    TEST_ASSERT(rgb_out.v[1] > rgb_out.v[2], "Sepia green > blue");

    return 0;
}

static int test_color_matrix_monochrome(void)
{
    printf("  TEST: Monochrome color matrix preset\n");

    alwan_mat3x3 mono_matrix;
    int status = alwan_color_matrix_get_preset(ALWAN_COLOR_MATRIX_MONOCHROME, &mono_matrix);
    TEST_ASSERT(status == ALWAN_OK, "Get monochrome preset failed");

    /* Test with colored input */
    alwan_vec3 rgb_in = {{0.8, 0.4, 0.2}};
    alwan_vec3 rgb_out;

    status = alwan_color_matrix_apply(&rgb_in, &mono_matrix, &rgb_out);
    TEST_ASSERT(status == ALWAN_OK, "Apply monochrome failed");

    /* Monochrome should make all channels equal (luminance)
     * Luma = 0.299*R + 0.587*G + 0.114*B = 0.299*0.8 + 0.587*0.4 + 0.114*0.2 ≈ 0.497 */
    alwan_scalar expected_luma = 0.299 * 0.8 + 0.587 * 0.4 + 0.114 * 0.2;
    TEST_ASSERT_NEAR(rgb_out.v[0], expected_luma, TEST_TOLERANCE, "Monochrome red = luma");
    TEST_ASSERT_NEAR(rgb_out.v[1], expected_luma, TEST_TOLERANCE, "Monochrome green = luma");
    TEST_ASSERT_NEAR(rgb_out.v[2], expected_luma, TEST_TOLERANCE, "Monochrome blue = luma");

    return 0;
}

static int test_color_matrix_all_presets(void)
{
    printf("  TEST: All color matrix presets valid\n");

    alwan_mat3x3 matrix;
    alwan_vec3 rgb_in = {{0.5, 0.5, 0.5}};
    alwan_vec3 rgb_out;

    /* Test all presets can be retrieved and applied */
    alwan_color_matrix_preset presets[] = {
        ALWAN_COLOR_MATRIX_SEPIA,
        ALWAN_COLOR_MATRIX_VINTAGE,
        ALWAN_COLOR_MATRIX_BLEACH_BYPASS,
        ALWAN_COLOR_MATRIX_COOL,
        ALWAN_COLOR_MATRIX_WARM,
        ALWAN_COLOR_MATRIX_MONOCHROME,
        ALWAN_COLOR_MATRIX_NIGHT_VISION
    };

    for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
        int status = alwan_color_matrix_get_preset(presets[i], &matrix);
        TEST_ASSERT(status == ALWAN_OK, "Get preset failed");

        status = alwan_color_matrix_apply(&rgb_in, &matrix, &rgb_out);
        TEST_ASSERT(status == ALWAN_OK, "Apply preset failed");
    }

    return 0;
}

/* ================================================================
 * Printer Lights Tests
 * ================================================================ */

static int test_printer_lights_neutral(void)
{
    printf("  TEST: Printer lights with neutral values (25, 25, 25)\n");

    alwan_vec3 rgb_in = {{0.5, 0.6, 0.7}};
    alwan_vec3 rgb_out;

    /* Default lights (25) should not change the image */
    int status = alwan_printer_lights_apply(&rgb_in, 25.0, 25.0, 25.0, &rgb_out);
    TEST_ASSERT(status == ALWAN_OK, "Printer lights apply failed");

    TEST_ASSERT_NEAR(rgb_out.v[0], 0.5, TEST_TOLERANCE, "Red unchanged at neutral");
    TEST_ASSERT_NEAR(rgb_out.v[1], 0.6, TEST_TOLERANCE, "Green unchanged at neutral");
    TEST_ASSERT_NEAR(rgb_out.v[2], 0.7, TEST_TOLERANCE, "Blue unchanged at neutral");

    return 0;
}

static int test_printer_lights_exposure(void)
{
    printf("  TEST: Printer lights exposure adjustment\n");

    alwan_vec3 rgb_in = {{0.5, 0.5, 0.5}};
    alwan_vec3 rgb_out;

    /* Reducing lights increases exposure (brightens)
     * 25 -> 15 = 10 light reduction = +0.25 log exposure = *1.778... */
    int status = alwan_printer_lights_apply(&rgb_in, 15.0, 25.0, 35.0, &rgb_out);
    TEST_ASSERT(status == ALWAN_OK, "Printer lights apply failed");

    /* Red (15 lights): brighter than input */
    TEST_ASSERT(rgb_out.v[0] > 0.5, "Red brighter with fewer lights");
    /* Green (25 lights): unchanged */
    TEST_ASSERT_NEAR(rgb_out.v[1], 0.5, TEST_TOLERANCE, "Green unchanged");
    /* Blue (35 lights): darker than input */
    TEST_ASSERT(rgb_out.v[2] < 0.5, "Blue darker with more lights");

    return 0;
}

static int test_printer_lights_per_channel(void)
{
    printf("  TEST: Printer lights per-channel control\n");

    /* Reference values from colour-science (generate_data_tests.ps1) */
    static alwan_scalar const ref_printer_lights[] = {
#include "reference_values/printer_lights_per_channel.csv"
    };

    alwan_vec3 rgb_in = {{0.3, 0.5, 0.7}};
    alwan_vec3 rgb_out;

    /* Different lights for each channel */
    int status = alwan_printer_lights_apply(&rgb_in, 20.0, 25.0, 30.0, &rgb_out);
    TEST_ASSERT(status == ALWAN_OK, "Printer lights apply failed");

    /* Compare with reference values from colour-science */
    TEST_ASSERT_NEAR(rgb_out.v[0], ref_printer_lights[0], TEST_TOLERANCE, "Red exposure correct");
    TEST_ASSERT_NEAR(rgb_out.v[1], ref_printer_lights[1], TEST_TOLERANCE, "Green unchanged");
    TEST_ASSERT_NEAR(rgb_out.v[2], ref_printer_lights[2], TEST_TOLERANCE, "Blue exposure correct");

    return 0;
}

/* ================================================================
 * Main Test Runner
 * ================================================================ */

int test_360_color_correction_main(void)
{
    printf("\nRunning test suite: 360_color_correction\n");
    test_count = 0;
    test_passed = 0;
    test_failed = 0;

    /* LGG tests */
    if (test_lgg_neutral()) return 1;
    if (test_lgg_lift()) return 1;
    if (test_lgg_gamma()) return 1;
    if (test_lgg_gain()) return 1;
    if (test_lgg_combined()) return 1;

    /* Color matrix tests */
    if (test_color_matrix_identity()) return 1;
    if (test_color_matrix_sepia()) return 1;
    if (test_color_matrix_monochrome()) return 1;
    if (test_color_matrix_all_presets()) return 1;

    /* Printer lights tests */
    if (test_printer_lights_neutral()) return 1;
    if (test_printer_lights_exposure()) return 1;
    if (test_printer_lights_per_channel()) return 1;

    printf("Test Results: %d/%d passed\n", test_passed, test_count);
    if (test_failed > 0) {
        printf("=== %d test(s) failed ===\n", test_failed);
        return 1;
    }

    printf("All color correction tests passed!\n");
    return 0;
}
