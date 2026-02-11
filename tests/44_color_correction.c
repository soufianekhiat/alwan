/* Test suite for Color Correction & Grading Tools */

#include "test_common.h"
#include <string.h>

/* ================================================================
 * Lift/Gamma/Gain Tests
 * ================================================================ */

static int test_lgg_neutral(void)
{
    printf("  TEST: LGG with neutral values (no change)\n");

    alwan_rgb rgb_in = {0.5, 0.5, 0.5};
    alwan_rgb lift = {0.0, 0.0, 0.0};    /* No lift */
    alwan_rgb gamma = {1.0, 1.0, 1.0};   /* No gamma change */
    alwan_rgb gain = {1.0, 1.0, 1.0};    /* No gain */
    alwan_rgb rgb_out;

    int status = alwan_lgg_apply(&rgb_out, &rgb_in, &lift, &gamma, &gain);
    TEST_ASSERT(status == ALWAN_OK, "LGG apply failed");

    /* With neutral values, output should match input */
    TEST_ASSERT_NEAR(rgb_out.r, ALWAN_LITERAL(0.5), TEST_TOLERANCE, "Red channel should be unchanged");
    TEST_ASSERT_NEAR(rgb_out.g, ALWAN_LITERAL(0.5), TEST_TOLERANCE, "Green channel should be unchanged");
    TEST_ASSERT_NEAR(rgb_out.b, ALWAN_LITERAL(0.5), TEST_TOLERANCE, "Blue channel should be unchanged");

    return 0;
}

static int test_lgg_lift(void)
{
    printf("  TEST: LGG lift adjustment (shadows)\n");

    alwan_rgb rgb_in = {0.2, 0.3, 0.4};
    alwan_rgb lift = {0.1, 0.0, -0.1};   /* Lift red, neutral green, lower blue */
    alwan_rgb gamma = {1.0, 1.0, 1.0};
    alwan_rgb gain = {1.0, 1.0, 1.0};
    alwan_rgb rgb_out;

    int status = alwan_lgg_apply(&rgb_out, &rgb_in, &lift, &gamma, &gain);
    TEST_ASSERT(status == ALWAN_OK, "LGG apply failed");

    /* Lift adds to input: (0.2 + 0.1) = 0.3 for red */
    TEST_ASSERT_NEAR(rgb_out.r, ALWAN_LITERAL(0.3), TEST_TOLERANCE, "Red lifted by 0.1");
    TEST_ASSERT_NEAR(rgb_out.g, ALWAN_LITERAL(0.3), TEST_TOLERANCE, "Green unchanged");
    TEST_ASSERT_NEAR(rgb_out.b, ALWAN_LITERAL(0.3), TEST_TOLERANCE, "Blue lowered by 0.1");

    return 0;
}

static int test_lgg_gamma(void)
{
    printf("  TEST: LGG gamma adjustment (midtones)\n");

    alwan_rgb rgb_in = {0.5, 0.5, 0.5};
    alwan_rgb lift = {0.0, 0.0, 0.0};
    alwan_rgb gamma = {2.0, 0.5, 1.0};   /* Darken red, brighten green, neutral blue */
    alwan_rgb gain = {1.0, 1.0, 1.0};
    alwan_rgb rgb_out;

    int status = alwan_lgg_apply(&rgb_out, &rgb_in, &lift, &gamma, &gain);
    TEST_ASSERT(status == ALWAN_OK, "LGG apply failed");

    /* Gamma 2.0: 0.5^(1/2) = 0.707... */
    TEST_ASSERT_NEAR(rgb_out.r, ALWAN_LITERAL(0.707106781), ALWAN_LITERAL(0.0001), "Red darkened (gamma 2.0)");
    /* Gamma 0.5: 0.5^(1/0.5) = 0.5^2 = 0.25 */
    TEST_ASSERT_NEAR(rgb_out.g, ALWAN_LITERAL(0.25), TEST_TOLERANCE, "Green brightened (gamma 0.5)");
    /* Gamma 1.0: no change */
    TEST_ASSERT_NEAR(rgb_out.b, ALWAN_LITERAL(0.5), TEST_TOLERANCE, "Blue unchanged");

    return 0;
}

static int test_lgg_gain(void)
{
    printf("  TEST: LGG gain adjustment (highlights)\n");

    alwan_rgb rgb_in = {0.5, 0.5, 0.5};
    alwan_rgb lift = {0.0, 0.0, 0.0};
    alwan_rgb gamma = {1.0, 1.0, 1.0};
    alwan_rgb gain = {2.0, 0.5, 1.0};    /* Double red, halve green, neutral blue */
    alwan_rgb rgb_out;

    int status = alwan_lgg_apply(&rgb_out, &rgb_in, &lift, &gamma, &gain);
    TEST_ASSERT(status == ALWAN_OK, "LGG apply failed");

    TEST_ASSERT_NEAR(rgb_out.r, ALWAN_LITERAL(1.0), TEST_TOLERANCE, "Red gain doubled");
    TEST_ASSERT_NEAR(rgb_out.g, ALWAN_LITERAL(0.25), TEST_TOLERANCE, "Green gain halved");
    TEST_ASSERT_NEAR(rgb_out.b, ALWAN_LITERAL(0.5), TEST_TOLERANCE, "Blue unchanged");

    return 0;
}

static int test_lgg_combined(void)
{
    printf("  TEST: LGG combined adjustments\n");

    /* Reference values from colour-science (generate_data_tests.ps1) */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const ref_lgg_combined[] = {
#include "reference_values/lgg_combined.csv"
    };
    ALWAN_DIAG_POP

    alwan_rgb rgb_in = {0.3, 0.5, 0.7};
    alwan_rgb lift = {0.1, 0.0, -0.1};
    alwan_rgb gamma = {1.2, 1.0, 0.8};
    alwan_rgb gain = {1.1, 1.0, 0.9};
    alwan_rgb rgb_out;

    int status = alwan_lgg_apply(&rgb_out, &rgb_in, &lift, &gamma, &gain);
    TEST_ASSERT(status == ALWAN_OK, "LGG apply failed");

    /* Compare with reference values from colour-science */
    TEST_ASSERT_NEAR(rgb_out.r, ref_lgg_combined[0], TEST_TOLERANCE, "Red combined adjustment");
    TEST_ASSERT_NEAR(rgb_out.g, ref_lgg_combined[1], TEST_TOLERANCE, "Green combined adjustment");
    TEST_ASSERT_NEAR(rgb_out.b, ref_lgg_combined[2], TEST_TOLERANCE, "Blue combined adjustment");

    return 0;
}

/* ================================================================
 * Color Matrix Tests
 * ================================================================ */

static int test_color_matrix_identity(void)
{
    printf("  TEST: Color matrix with identity matrix\n");

    alwan_rgb rgb_in = {0.5, 0.6, 0.7};
    alwan_mat3x3 identity;
    alwan_mat3_identity(&identity);
    alwan_rgb rgb_out;

    int status = alwan_color_matrix_apply(&rgb_out, &rgb_in, &identity);
    TEST_ASSERT(status == ALWAN_OK, "Color matrix apply failed");

    TEST_ASSERT_NEAR(rgb_out.r, ALWAN_LITERAL(0.5), TEST_TOLERANCE, "Red unchanged");
    TEST_ASSERT_NEAR(rgb_out.g, ALWAN_LITERAL(0.6), TEST_TOLERANCE, "Green unchanged");
    TEST_ASSERT_NEAR(rgb_out.b, ALWAN_LITERAL(0.7), TEST_TOLERANCE, "Blue unchanged");

    return 0;
}

static int test_color_matrix_sepia(void)
{
    printf("  TEST: Sepia color matrix preset\n");

    /* Reference values from colour-science (generate_data_tests.ps1) */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const ref_sepia[] = {
#include "reference_values/color_matrix_sepia.csv"
    };
    ALWAN_DIAG_POP

    alwan_mat3x3 sepia_matrix;
    int status = alwan_color_matrix_get_preset(&sepia_matrix, ALWAN_COLOR_MATRIX_SEPIA);
    TEST_ASSERT(status == ALWAN_OK, "Get sepia preset failed");

    /* Test with mid-gray */
    alwan_rgb rgb_in = {0.5, 0.5, 0.5};
    alwan_rgb rgb_out;

    status = alwan_color_matrix_apply(&rgb_out, &rgb_in, &sepia_matrix);
    TEST_ASSERT(status == ALWAN_OK, "Apply sepia failed");

    /* Compare with reference values from colour-science */
    TEST_ASSERT_NEAR(rgb_out.r, ref_sepia[0], TEST_TOLERANCE, "Sepia red channel");
    TEST_ASSERT_NEAR(rgb_out.g, ref_sepia[1], TEST_TOLERANCE, "Sepia green channel");
    TEST_ASSERT_NEAR(rgb_out.b, ref_sepia[2], TEST_TOLERANCE, "Sepia blue channel");

    /* Sepia should produce warm brownish tones */
    TEST_ASSERT(rgb_out.r > rgb_out.g, "Sepia red > green");
    TEST_ASSERT(rgb_out.g > rgb_out.b, "Sepia green > blue");

    return 0;
}

static int test_color_matrix_monochrome(void)
{
    printf("  TEST: Monochrome color matrix preset\n");

    alwan_mat3x3 mono_matrix;
    int status = alwan_color_matrix_get_preset(&mono_matrix, ALWAN_COLOR_MATRIX_MONOCHROME);
    TEST_ASSERT(status == ALWAN_OK, "Get monochrome preset failed");

    /* Test with colored input */
    alwan_rgb rgb_in = {0.8, 0.4, 0.2};
    alwan_rgb rgb_out;

    status = alwan_color_matrix_apply(&rgb_out, &rgb_in, &mono_matrix);
    TEST_ASSERT(status == ALWAN_OK, "Apply monochrome failed");

    /* Monochrome should make all channels equal (luminance)
     * Luma = 0.299*R + 0.587*G + 0.114*B = 0.299*0.8 + 0.587*0.4 + 0.114*0.2 ≈ 0.497 */
    alwan_scalar expected_luma = 0.299 * 0.8 + 0.587 * 0.4 + 0.114 * 0.2;
    TEST_ASSERT_NEAR(rgb_out.r, expected_luma, TEST_TOLERANCE, "Monochrome red = luma");
    TEST_ASSERT_NEAR(rgb_out.g, expected_luma, TEST_TOLERANCE, "Monochrome green = luma");
    TEST_ASSERT_NEAR(rgb_out.b, expected_luma, TEST_TOLERANCE, "Monochrome blue = luma");

    return 0;
}

static int test_color_matrix_all_presets(void)
{
    printf("  TEST: All color matrix presets valid\n");

    alwan_mat3x3 matrix;
    alwan_rgb rgb_in = {0.5, 0.5, 0.5};
    alwan_rgb rgb_out;

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
        int status = alwan_color_matrix_get_preset(&matrix, presets[i]);
        TEST_ASSERT(status == ALWAN_OK, "Get preset failed");

        status = alwan_color_matrix_apply(&rgb_out, &rgb_in, &matrix);
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

    alwan_rgb rgb_in = {0.5, 0.6, 0.7};
    alwan_rgb rgb_out;

    /* Default lights (25) should not change the image */
    int status = alwan_printer_lights_apply(&rgb_out, &rgb_in, 25.0, 25.0, 25.0);
    TEST_ASSERT(status == ALWAN_OK, "Printer lights apply failed");

    TEST_ASSERT_NEAR(rgb_out.r, ALWAN_LITERAL(0.5), TEST_TOLERANCE, "Red unchanged at neutral");
    TEST_ASSERT_NEAR(rgb_out.g, ALWAN_LITERAL(0.6), TEST_TOLERANCE, "Green unchanged at neutral");
    TEST_ASSERT_NEAR(rgb_out.b, ALWAN_LITERAL(0.7), TEST_TOLERANCE, "Blue unchanged at neutral");

    return 0;
}

static int test_printer_lights_exposure(void)
{
    printf("  TEST: Printer lights exposure adjustment\n");

    alwan_rgb rgb_in = {0.5, 0.5, 0.5};
    alwan_rgb rgb_out;

    /* Reducing lights increases exposure (brightens)
     * 25 -> 15 = 10 light reduction = +0.25 log exposure = *1.778... */
    int status = alwan_printer_lights_apply(&rgb_out, &rgb_in, 15.0, 25.0, 35.0);
    TEST_ASSERT(status == ALWAN_OK, "Printer lights apply failed");

    /* Red (15 lights): brighter than input */
    TEST_ASSERT(rgb_out.r > ALWAN_LITERAL(0.5), "Red brighter with fewer lights");
    /* Green (25 lights): unchanged */
    TEST_ASSERT_NEAR(rgb_out.g, ALWAN_LITERAL(0.5), TEST_TOLERANCE, "Green unchanged");
    /* Blue (35 lights): darker than input */
    TEST_ASSERT(rgb_out.b < ALWAN_LITERAL(0.5), "Blue darker with more lights");

    return 0;
}

static int test_printer_lights_per_channel(void)
{
    printf("  TEST: Printer lights per-channel control\n");

    /* Reference values from colour-science (generate_data_tests.ps1) */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const ref_printer_lights[] = {
#include "reference_values/printer_lights_per_channel.csv"
    };
    ALWAN_DIAG_POP

    alwan_rgb rgb_in = {0.3, 0.5, 0.7};
    alwan_rgb rgb_out;

    /* Different lights for each channel */
    int status = alwan_printer_lights_apply(&rgb_out, &rgb_in, 20.0, 25.0, 30.0);
    TEST_ASSERT(status == ALWAN_OK, "Printer lights apply failed");

    /* Compare with reference values from colour-science */
    TEST_ASSERT_NEAR(rgb_out.r, ref_printer_lights[0], TEST_TOLERANCE, "Red exposure correct");
    TEST_ASSERT_NEAR(rgb_out.g, ref_printer_lights[1], TEST_TOLERANCE, "Green unchanged");
    TEST_ASSERT_NEAR(rgb_out.b, ref_printer_lights[2], TEST_TOLERANCE, "Blue exposure correct");

    return 0;
}

/* ================================================================
 * Cheung 2004 Polynomial Expansion Tests
 * ================================================================ */

static int test_cheung2004_expand_basic(void)
{
    printf("  TEST: Cheung 2004 polynomial expansion (basic terms)\n");

    /* Reference values from colour-science */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const ref_input[] = {
#include "reference_values/cheung2004_input_rgb.csv"
    };
    static alwan_scalar const ref_expand_3[] = {
#include "reference_values/cheung2004_expand_3.csv"
    };
    static alwan_scalar const ref_expand_7[] = {
#include "reference_values/cheung2004_expand_7.csv"
    };
    static alwan_scalar const ref_expand_11[] = {
#include "reference_values/cheung2004_expand_11.csv"
    };
    ALWAN_DIAG_POP

    int num_samples = 6;
    alwan_scalar expanded[35];

    /* Test 3-term expansion */
    for (int i = 0; i < num_samples; i++) {
        alwan_rgb rgb = {ref_input[i*3], ref_input[i*3+1], ref_input[i*3+2]};
        int status = alwan_poly_expand_cheung2004(expanded, &rgb, ALWAN_POLY_CHEUNG_3);
        TEST_ASSERT(status == ALWAN_OK, "Cheung2004 expand failed");
        for (int j = 0; j < 3; j++) {
            TEST_ASSERT_NEAR(expanded[j], ref_expand_3[i*3+j], TEST_TOLERANCE,
                             "Cheung2004 3-term mismatch");
        }
    }

    /* Test 7-term expansion */
    for (int i = 0; i < num_samples; i++) {
        alwan_rgb rgb = {ref_input[i*3], ref_input[i*3+1], ref_input[i*3+2]};
        int status = alwan_poly_expand_cheung2004(expanded, &rgb, ALWAN_POLY_CHEUNG_7);
        TEST_ASSERT(status == ALWAN_OK, "Cheung2004 expand failed");
        for (int j = 0; j < 7; j++) {
            TEST_ASSERT_NEAR(expanded[j], ref_expand_7[i*7+j], TEST_TOLERANCE,
                             "Cheung2004 7-term mismatch");
        }
    }

    /* Test 11-term expansion */
    for (int i = 0; i < num_samples; i++) {
        alwan_rgb rgb = {ref_input[i*3], ref_input[i*3+1], ref_input[i*3+2]};
        int status = alwan_poly_expand_cheung2004(expanded, &rgb, ALWAN_POLY_CHEUNG_11);
        TEST_ASSERT(status == ALWAN_OK, "Cheung2004 expand failed");
        for (int j = 0; j < 11; j++) {
            TEST_ASSERT_NEAR(expanded[j], ref_expand_11[i*11+j], TEST_TOLERANCE,
                             "Cheung2004 11-term mismatch");
        }
    }

    return 0;
}

static int test_cheung2004_expand_full(void)
{
    printf("  TEST: Cheung 2004 polynomial expansion (35-term)\n");

    /* Reference values from colour-science */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const ref_input[] = {
#include "reference_values/cheung2004_input_rgb.csv"
    };
    static alwan_scalar const ref_expand_35[] = {
#include "reference_values/cheung2004_expand_35.csv"
    };
    ALWAN_DIAG_POP

    int num_samples = 6;
    alwan_scalar expanded[35];

    for (int i = 0; i < num_samples; i++) {
        alwan_rgb rgb = {ref_input[i*3], ref_input[i*3+1], ref_input[i*3+2]};
        int status = alwan_poly_expand_cheung2004(expanded, &rgb, ALWAN_POLY_CHEUNG_35);
        TEST_ASSERT(status == ALWAN_OK, "Cheung2004 35-term expand failed");
        for (int j = 0; j < 35; j++) {
            TEST_ASSERT_NEAR(expanded[j], ref_expand_35[i*35+j], TEST_TOLERANCE,
                             "Cheung2004 35-term mismatch");
        }
    }

    return 0;
}

/* ================================================================
 * Finlayson 2015 Polynomial Expansion Tests
 * ================================================================ */

static int test_finlayson2015_expand_standard(void)
{
    printf("  TEST: Finlayson 2015 standard polynomial expansion\n");

    /* Reference values from colour-science */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const ref_input[] = {
#include "reference_values/finlayson2015_input_rgb.csv"
    };
    static alwan_scalar const ref_deg2[] = {
#include "reference_values/finlayson2015_std_deg2.csv"
    };
    static alwan_scalar const ref_deg3[] = {
#include "reference_values/finlayson2015_std_deg3.csv"
    };
    ALWAN_DIAG_POP

    int num_samples = 4;
    alwan_scalar expanded[34];
    int out_size;

    /* Test degree 2 standard expansion (size=9) */
    for (int i = 0; i < num_samples; i++) {
        alwan_rgb rgb = {ref_input[i*3], ref_input[i*3+1], ref_input[i*3+2]};
        int status = alwan_poly_expand_finlayson2015(expanded, &out_size, &rgb, 2, 0);
        TEST_ASSERT(status == ALWAN_OK, "Finlayson2015 expand failed");
        TEST_ASSERT(out_size == 9, "Finlayson2015 degree 2 should have 9 terms");
        for (int j = 0; j < 9; j++) {
            TEST_ASSERT_NEAR(expanded[j], ref_deg2[i*9+j], TEST_TOLERANCE,
                             "Finlayson2015 std deg2 mismatch");
        }
    }

    /* Test degree 3 standard expansion (size=19) */
    for (int i = 0; i < num_samples; i++) {
        alwan_rgb rgb = {ref_input[i*3], ref_input[i*3+1], ref_input[i*3+2]};
        int status = alwan_poly_expand_finlayson2015(expanded, &out_size, &rgb, 3, 0);
        TEST_ASSERT(status == ALWAN_OK, "Finlayson2015 expand failed");
        TEST_ASSERT(out_size == 19, "Finlayson2015 degree 3 should have 19 terms");
        for (int j = 0; j < 19; j++) {
            TEST_ASSERT_NEAR(expanded[j], ref_deg3[i*19+j], TEST_TOLERANCE,
                             "Finlayson2015 std deg3 mismatch");
        }
    }

    return 0;
}

static int test_finlayson2015_expand_root(void)
{
    printf("  TEST: Finlayson 2015 root-polynomial expansion\n");

    /* Reference values from colour-science */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const ref_input[] = {
#include "reference_values/finlayson2015_input_rgb.csv"
    };
    static alwan_scalar const ref_root_deg2[] = {
#include "reference_values/finlayson2015_root_deg2.csv"
    };
    static alwan_scalar const ref_root_deg3[] = {
#include "reference_values/finlayson2015_root_deg3.csv"
    };
    ALWAN_DIAG_POP

    int num_samples = 4;
    alwan_scalar expanded[34];
    int out_size;

    /* Test degree 2 root-polynomial expansion (size=6) */
    for (int i = 0; i < num_samples; i++) {
        alwan_rgb rgb = {ref_input[i*3], ref_input[i*3+1], ref_input[i*3+2]};
        int status = alwan_poly_expand_finlayson2015(expanded, &out_size, &rgb, 2, 1);
        TEST_ASSERT(status == ALWAN_OK, "Finlayson2015 root expand failed");
        TEST_ASSERT(out_size == 6, "Finlayson2015 root degree 2 should have 6 terms");
        for (int j = 0; j < 6; j++) {
            TEST_ASSERT_NEAR(expanded[j], ref_root_deg2[i*6+j], TEST_TOLERANCE,
                             "Finlayson2015 root deg2 mismatch");
        }
    }

    /* Test degree 3 root-polynomial expansion (size=13) */
    for (int i = 0; i < num_samples; i++) {
        alwan_rgb rgb = {ref_input[i*3], ref_input[i*3+1], ref_input[i*3+2]};
        int status = alwan_poly_expand_finlayson2015(expanded, &out_size, &rgb, 3, 1);
        TEST_ASSERT(status == ALWAN_OK, "Finlayson2015 root expand failed");
        TEST_ASSERT(out_size == 13, "Finlayson2015 root degree 3 should have 13 terms");
        for (int j = 0; j < 13; j++) {
            TEST_ASSERT_NEAR(expanded[j], ref_root_deg3[i*13+j], TEST_TOLERANCE,
                             "Finlayson2015 root deg3 mismatch");
        }
    }

    return 0;
}

/* ================================================================
 * Vandermonde Expansion Tests
 * ================================================================ */

static int test_vandermonde_expand(void)
{
    printf("  TEST: Vandermonde polynomial expansion\n");

    /* Reference values from colour-science */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const ref_input[] = {
#include "reference_values/vandermonde_input.csv"
    };
    static alwan_scalar const ref_deg2[] = {
#include "reference_values/vandermonde_deg2.csv"
    };
    static alwan_scalar const ref_deg3[] = {
#include "reference_values/vandermonde_deg3.csv"
    };
    ALWAN_DIAG_POP

    int num_samples = 3;
    alwan_scalar expanded[20];
    int out_size;

    /* Test degree 2 (size = 3*2 + 1 = 7) */
    for (int i = 0; i < num_samples; i++) {
        int status = alwan_poly_expand_vandermonde(expanded, &out_size, &ref_input[i*3], 3, 2);
        TEST_ASSERT(status == ALWAN_OK, "Vandermonde expand failed");
        TEST_ASSERT(out_size == 7, "Vandermonde degree 2 should have 7 terms");
        for (int j = 0; j < 7; j++) {
            TEST_ASSERT_NEAR(expanded[j], ref_deg2[i*7+j], TEST_TOLERANCE,
                             "Vandermonde deg2 mismatch");
        }
    }

    /* Test degree 3 (size = 3*3 + 1 = 10) */
    for (int i = 0; i < num_samples; i++) {
        int status = alwan_poly_expand_vandermonde(expanded, &out_size, &ref_input[i*3], 3, 3);
        TEST_ASSERT(status == ALWAN_OK, "Vandermonde expand failed");
        TEST_ASSERT(out_size == 10, "Vandermonde degree 3 should have 10 terms");
        for (int j = 0; j < 10; j++) {
            TEST_ASSERT_NEAR(expanded[j], ref_deg3[i*10+j], TEST_TOLERANCE,
                             "Vandermonde deg3 mismatch");
        }
    }

    return 0;
}

/* ================================================================
 * White Balance Tests
 * ================================================================ */

static int test_white_balance(void)
{
    printf("  TEST: White balance from gray card\n");

    /* Reference values from colour-science */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const ref_grays[] = {
#include "reference_values/white_balance_input_gray.csv"
    };
    static alwan_scalar const ref_multipliers[] = {
#include "reference_values/white_balance_multipliers.csv"
    };
    ALWAN_DIAG_POP

    int num_samples = 5;

    for (int i = 0; i < num_samples; i++) {
        alwan_rgb measured_gray = {ref_grays[i*3], ref_grays[i*3+1], ref_grays[i*3+2]};
        alwan_rgb multipliers;

        int status = alwan_white_balance_from_gray(&multipliers, &measured_gray);
        TEST_ASSERT(status == ALWAN_OK, "White balance computation failed");

        TEST_ASSERT_NEAR(multipliers.r, ref_multipliers[i*3], TEST_TOLERANCE,
                         "White balance R multiplier mismatch");
        TEST_ASSERT_NEAR(multipliers.g, ref_multipliers[i*3+1], TEST_TOLERANCE,
                         "White balance G multiplier mismatch");
        TEST_ASSERT_NEAR(multipliers.b, ref_multipliers[i*3+2], TEST_TOLERANCE,
                         "White balance B multiplier mismatch");
    }

    return 0;
}

static int test_white_balance_apply(void)
{
    printf("  TEST: White balance application\n");

    /* Test that applying white balance to the measured gray produces neutral */
    alwan_rgb measured_gray = {0.5, 0.45, 0.55};
    alwan_rgb multipliers;
    alwan_rgb result;

    int status = alwan_white_balance_from_gray(&multipliers, &measured_gray);
    TEST_ASSERT(status == ALWAN_OK, "White balance computation failed");

    status = alwan_white_balance_apply(&result, &measured_gray, &multipliers);
    TEST_ASSERT(status == ALWAN_OK, "White balance apply failed");

    /* After applying white balance, all channels should be equal (neutral) */
    TEST_ASSERT_NEAR(result.r, result.g, TEST_TOLERANCE, "WB result should be neutral (R=G)");
    TEST_ASSERT_NEAR(result.g, result.b, TEST_TOLERANCE, "WB result should be neutral (G=B)");

    return 0;
}

/* ================================================================
 * Main Test Runner
 * ================================================================ */

int test_44_color_correction_main(void)
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

    /* Cheung 2004 polynomial expansion tests */
    if (test_cheung2004_expand_basic()) return 1;
    if (test_cheung2004_expand_full()) return 1;

    /* Finlayson 2015 polynomial expansion tests */
    if (test_finlayson2015_expand_standard()) return 1;
    if (test_finlayson2015_expand_root()) return 1;

    /* Vandermonde expansion tests */
    if (test_vandermonde_expand()) return 1;

    /* White balance tests */
    if (test_white_balance()) return 1;
    if (test_white_balance_apply()) return 1;

    printf("Test Results: %d/%d passed\n", test_passed, test_count);
    if (test_failed > 0) {
        printf("=== %d test(s) failed ===\n", test_failed);
        return 1;
    }

    printf("All color correction tests passed!\n");
    return 0;
}
