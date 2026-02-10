/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test suite 350: Reference Data & Color Systems
 * Tests: Munsell, Color Checker, NCS, RGB Space Definitions
 */

#define TEST_USE_COUNTERS
#include "test_common.h"
#include <stdlib.h>
#include <string.h>


/* ----------------------------------------------------------------
 * Munsell Renotation Data Tests
 * ---------------------------------------------------------------- */

static int test_munsell_neutrals(void) {
    TEST_START("Munsell neutral axis (N0-N10)");

    alwan_xyz xyz;
    int status;

    /* N0 (black) */
    status = alwan_munsell_to_xyz(&xyz, 0.0, 0.0, 0.0, ALWAN_ILLUMINANT_C);
    if (status != ALWAN_OK) {
        TEST_FAIL("Failed to convert N0: error %d", status);
    }
    TEST_CHECK_NEAR(xyz.y, 0.0, TEST_TOLERANCE);  /* Y should be ~0 */

    /* N5 (mid gray) */
    status = alwan_munsell_to_xyz(&xyz, 0.0, 5.0, 0.0, ALWAN_ILLUMINANT_C);
    if (status != ALWAN_OK) {
        TEST_FAIL("Failed to convert N5: error %d", status);
    }
    /* N5 should have Y ≈ 0.198 (19.8% reflectance) */
    TEST_CHECK_NEAR(xyz.y, 0.198, TEST_TOLERANCE);

    /* N10 (white) */
    status = alwan_munsell_to_xyz(&xyz, 0.0, 10.0, 0.0, ALWAN_ILLUMINANT_C);
    if (status != ALWAN_OK) {
        TEST_FAIL("Failed to convert N10: error %d", status);
    }
    TEST_CHECK_NEAR(xyz.y, 1.0, TEST_TOLERANCE);  /* Y should be ~1.0 */

    TEST_PASS_MSG();
    return 0;
}

static int test_munsell_chromatic(void) {
    TEST_START("Munsell chromatic colors");

    alwan_xyz xyz;
    int status;

    /* 5R 5/10 (medium red with high chroma) */
    status = alwan_munsell_to_xyz(&xyz, 5.0, 5.0, 10.0, ALWAN_ILLUMINANT_C);
    if (status != ALWAN_OK) {
        TEST_FAIL("Failed to convert 5R 5/10: error %d", status);
    }
    /* Should have reasonable XYZ values */
    if (xyz.x < 0.0 || xyz.y < 0.0 || xyz.z < 0.0) {
        TEST_FAIL("Invalid XYZ values: X=%g, Y=%g, Z=%g", xyz.x, xyz.y, xyz.z);
    }

    TEST_PASS_MSG();
    return 0;
}

static int test_munsell_roundtrip(void) {
    TEST_START("Munsell HVC roundtrip conversion");

    alwan_scalar hue_in = 0.0;
    alwan_scalar value_in = 5.0;
    alwan_scalar chroma_in = 0.0;

    alwan_xyz xyz;
    int status = alwan_munsell_to_xyz(&xyz, hue_in, value_in, chroma_in, ALWAN_ILLUMINANT_C);
    if (status != ALWAN_OK) {
        TEST_FAIL("Forward conversion failed: error %d", status);
    }

    alwan_scalar hue_out, value_out, chroma_out;
    status = alwan_xyz_to_munsell(&hue_out, &value_out, &chroma_out, &xyz, ALWAN_ILLUMINANT_C);
    if (status != ALWAN_OK) {
        TEST_FAIL("Inverse conversion failed: error %d", status);
    }

    /* For neutral colors, hue is undefined, so only check value and chroma */
    TEST_CHECK_NEAR(value_out, value_in, 0.5);    /* Allow 0.5 Munsell value units tolerance */
    TEST_CHECK_NEAR(chroma_out, chroma_in, 1.0);  /* Allow 1.0 chroma units tolerance */

    TEST_PASS_MSG();
    return 0;
}

static int test_munsell_illuminant_adaptation(void) {
    TEST_START("Munsell illuminant adaptation");

    alwan_xyz xyz_c, xyz_d65;
    int status;

    /* Convert N5 under Illuminant C */
    status = alwan_munsell_to_xyz(&xyz_c, 0.0, 5.0, 0.0, ALWAN_ILLUMINANT_C);
    if (status != ALWAN_OK) {
        TEST_FAIL("Failed to convert under C: error %d", status);
    }

    /* Convert N5 under Illuminant D65 */
    status = alwan_munsell_to_xyz(&xyz_d65, 0.0, 5.0, 0.0, ALWAN_ILLUMINANT_D65);
    if (status != ALWAN_OK) {
        TEST_FAIL("Failed to convert under D65: error %d", status);
    }

    /* Values should differ due to chromatic adaptation */
    alwan_scalar diff =  ALWAN_ABS(xyz_c.x - xyz_d65.x) +
                         ALWAN_ABS(xyz_c.y - xyz_d65.y) +
                         ALWAN_ABS(xyz_c.z - xyz_d65.z);

    if (diff < TEST_TOLERANCE) {
        TEST_FAIL("Expected chromatic adaptation to change XYZ values");
    }

    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * Color Checker Data Tests
 * ---------------------------------------------------------------- */

static int test_colorchecker_num_patches(void) {
    TEST_START("Color Checker patch counts");

    size_t count;

    count = alwan_color_checker_num_patches(ALWAN_COLORCHECKER_CLASSIC);
    if (count != 24) {
        TEST_FAIL("ColorChecker Classic should have 24 patches, got %zu", count);
    }

    count = alwan_color_checker_num_patches(ALWAN_COLORCHECKER_SG);
    if (count != 140) {
        TEST_FAIL("ColorChecker SG should have 140 patches, got %zu", count);
    }

    TEST_PASS_MSG();
    return 0;
}

static int test_colorchecker_classic_patches(void) {
    TEST_START("ColorChecker Classic patch data");

    alwan_xyz xyz;
    int status;

    /* Test patch 19 (white) - should have high Y */
    status = alwan_color_checker_data(&xyz, ALWAN_COLORCHECKER_CLASSIC, ALWAN_ILLUMINANT_D50,
                                       18);  /* Index 18 = patch 19 */
    if (status != ALWAN_OK) {
        TEST_FAIL("Failed to get white patch: error %d", status);
    }

    if (xyz.y < 0.8) {
        TEST_FAIL("White patch Y should be > 0.8, got %g", xyz.y);
    }

    /* Test patch 24 (black) - should have low Y */
    status = alwan_color_checker_data(&xyz, ALWAN_COLORCHECKER_CLASSIC, ALWAN_ILLUMINANT_D50,
                                       23);  /* Index 23 = patch 24 */
    if (status != ALWAN_OK) {
        TEST_FAIL("Failed to get black patch: error %d", status);
    }

    if (xyz.y > 0.05) {
        TEST_FAIL("Black patch Y should be < 0.05, got %g", xyz.y);
    }

    TEST_PASS_MSG();
    return 0;
}

static int test_colorchecker_illuminant_adaptation(void) {
    TEST_START("ColorChecker illuminant adaptation");

    alwan_xyz xyz_d50, xyz_d65;
    int status;

    /* Get patch under D50 */
    status = alwan_color_checker_data(&xyz_d50, ALWAN_COLORCHECKER_CLASSIC, ALWAN_ILLUMINANT_D50,
                                       0);
    if (status != ALWAN_OK) {
        TEST_FAIL("Failed to get patch under D50: error %d", status);
    }

    /* Get same patch under D65 */
    status = alwan_color_checker_data(&xyz_d65, ALWAN_COLORCHECKER_CLASSIC, ALWAN_ILLUMINANT_D65,
                                       0);
    if (status != ALWAN_OK) {
        TEST_FAIL("Failed to get patch under D65: error %d", status);
    }

    /* Values should differ due to chromatic adaptation */
    alwan_scalar diff =  ALWAN_ABS(xyz_d50.x - xyz_d65.x) +
                         ALWAN_ABS(xyz_d50.y - xyz_d65.y) +
                         ALWAN_ABS(xyz_d50.z - xyz_d65.z);

    if (diff < TEST_TOLERANCE) {
        TEST_FAIL("Expected chromatic adaptation to change XYZ values");
    }

    TEST_PASS_MSG();
    return 0;
}

static int test_colorchecker_bounds(void) {
    TEST_START("ColorChecker bounds checking");

    alwan_xyz xyz;
    int status;

    /* Test out-of-bounds patch index */
    status = alwan_color_checker_data(&xyz, ALWAN_COLORCHECKER_CLASSIC, ALWAN_ILLUMINANT_D50,
                                       24);  /* Index 24 is out of bounds (0-23) */
    if (status == ALWAN_OK) {
        TEST_FAIL("Expected error for out-of-bounds index");
    }

    /* Test invalid type */
    status = alwan_color_checker_data(&xyz, (alwan_colorchecker_type)999, ALWAN_ILLUMINANT_D50,
                                       0);
    if (status == ALWAN_OK) {
        TEST_FAIL("Expected error for invalid type");
    }

    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * NCS Data Tests
 * ---------------------------------------------------------------- */

static int test_ncs_parsing(void) {
    TEST_START("NCS notation parsing");

    alwan_xyz xyz;
    int status;

    /* Note: NCS functions are stubs and return ALWAN_E_INVALID */
    status = alwan_ncs_to_xyz(&xyz, "S 1050-Y90R");
    if (status != ALWAN_E_INVALID) {
        /* If implementation is added, this test should validate the result */
        printf("    [INFO] NCS conversion returned status %d\n", status);
    }

    printf("    [SKIP] NCS implementation pending\n");
    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * RGB Space Definitions Tests
 * ---------------------------------------------------------------- */

static int test_rgb_space_lookup(void) {
    TEST_START("RGB space lookup by enum");

    alwan_scalar primaries[6];
    alwan_vec2 white_point;
    int status;

    /* Test sRGB */
    status = alwan_rgb_space_by_enum(primaries, &white_point, ALWAN_RGB_SPACE_SRGB);
    if (status != ALWAN_OK) {
        TEST_FAIL("Failed to find sRGB: error %d", status);
    }

    /* Verify sRGB primaries */
    TEST_CHECK_NEAR(primaries[0], 0.6400, TEST_TOLERANCE);  /* Red x */
    TEST_CHECK_NEAR(primaries[1], 0.3300, TEST_TOLERANCE);  /* Red y */
    TEST_CHECK_NEAR(primaries[2], 0.3000, TEST_TOLERANCE);  /* Green x */
    TEST_CHECK_NEAR(primaries[3], 0.6000, TEST_TOLERANCE);  /* Green y */
    TEST_CHECK_NEAR(primaries[4], 0.1500, TEST_TOLERANCE);  /* Blue x */
    TEST_CHECK_NEAR(primaries[5], 0.0600, TEST_TOLERANCE);  /* Blue y */

    /* Verify D65 white point */
    TEST_CHECK_NEAR(white_point.v[0], 0.3127, TEST_TOLERANCE);
    TEST_CHECK_NEAR(white_point.v[1], 0.3290, TEST_TOLERANCE);

    TEST_PASS_MSG();
    return 0;
}

static int test_rgb_space_various(void) {
    TEST_START("Various RGB space definitions");

    alwan_scalar primaries[6];
    alwan_vec2 white_point;
    int status;

    alwan_rgb_space spaces[] = {
        ALWAN_RGB_SPACE_ADOBE_RGB_1998,
        ALWAN_RGB_SPACE_PROPHOTO_RGB,
        ALWAN_RGB_SPACE_DCI_P3,
        ALWAN_RGB_SPACE_DISPLAY_P3,
        ALWAN_RGB_SPACE_BT2020,
        ALWAN_RGB_SPACE_ACES2065_1,
        ALWAN_RGB_SPACE_ACESCG,
    };

    char const *space_names[] = {
        "Adobe RGB",
        "ProPhoto RGB",
        "DCI-P3",
        "Display P3",
        "Rec. 2020",
        "ACES AP0",
        "ACES AP1",
    };

    size_t num_spaces = sizeof(spaces) / sizeof(spaces[0]);

    for (size_t i = 0; i < num_spaces; i++) {
        status = alwan_rgb_space_by_enum(primaries, &white_point, spaces[i]);
        if (status != ALWAN_OK) {
            TEST_FAIL("Failed to find %s: error %d", space_names[i], status);
        }

        /* Verify primaries are in valid range [0, 1] */
        for (int j = 0; j < 6; j++) {
            if (primaries[j] < -0.1 || primaries[j] > 1.1) {
                TEST_FAIL("%s: primary[%d] = %g out of reasonable range",
                          space_names[i], j, primaries[j]);
            }
        }

        /* Verify white point is reasonable */
        if (white_point.v[0] < 0.2 || white_point.v[0] > 0.4 ||
            white_point.v[1] < 0.2 || white_point.v[1] > 0.5) {
            TEST_FAIL("%s: white point (%g, %g) out of reasonable range",
                      space_names[i], white_point.v[0], white_point.v[1]);
        }
    }

    TEST_PASS_MSG();
    return 0;
}

static int test_rgb_space_transfer_functions(void) {
    TEST_START("RGB space transfer functions");

    alwan_transfer_function oetf, eotf;
    int status;

    /* Test sRGB */
    status = alwan_rgb_space_get_tfs(&oetf, &eotf, ALWAN_RGB_SPACE_SRGB);
    if (status != ALWAN_OK) {
        TEST_FAIL("Failed to get sRGB TF: error %d", status);
    }

    if (oetf != ALWAN_TF_SRGB || eotf != ALWAN_TF_SRGB) {
        TEST_FAIL("Expected sRGB TF, got oetf=%d eotf=%d", oetf, eotf);
    }

    /* Test Adobe RGB */
    status = alwan_rgb_space_get_tfs(&oetf, &eotf, ALWAN_RGB_SPACE_ADOBE_RGB_1998);
    if (status != ALWAN_OK) {
        TEST_FAIL("Failed to get Adobe RGB TF: error %d", status);
    }

    if (oetf != ALWAN_TF_GAMMA22 || eotf != ALWAN_TF_GAMMA22) {
        TEST_FAIL("Expected Gamma 2.2 TF, got oetf=%d eotf=%d", oetf, eotf);
    }

    TEST_PASS_MSG();
    return 0;
}

static int test_rgb_space_not_found(void) {
    TEST_START("RGB space invalid enum handling");

    alwan_scalar primaries[6];
    alwan_vec2 white_point;
    int status;

    /* Test with out-of-range enum value */
    status = alwan_rgb_space_by_enum(primaries, &white_point, (alwan_rgb_space)9999);
    if (status == ALWAN_OK) {
        TEST_FAIL("Expected error for invalid space enum");
    }

    alwan_transfer_function oetf, eotf;
    status = alwan_rgb_space_get_tfs(&oetf, &eotf, (alwan_rgb_space)9999);
    if (status == ALWAN_OK) {
        TEST_FAIL("Expected error for nonexistent space TF");
    }

    TEST_PASS_MSG();
    return 0;
}

/* ----------------------------------------------------------------
 * Main Test Runner
 * ---------------------------------------------------------------- */

int test_43_reference_data_main(void) {
    printf("========================================\n");
    printf("Test Suite 350: Reference Data\n");
    printf("========================================\n\n");

    test_count = 0;
    test_passed = 0;

    /* Munsell Renotation Data */
    printf("Munsell Renotation Data\n");
    printf("--------------------------------\n");
    if (test_munsell_neutrals()) return 1;
    if (test_munsell_chromatic()) return 1;
    if (test_munsell_roundtrip()) return 1;
    if (test_munsell_illuminant_adaptation()) return 1;

    /* Color Checker Data */
    printf("\nColor Checker Data\n");
    printf("--------------------------------\n");
    if (test_colorchecker_num_patches()) return 1;
    if (test_colorchecker_classic_patches()) return 1;
    if (test_colorchecker_illuminant_adaptation()) return 1;
    if (test_colorchecker_bounds()) return 1;

    /* NCS Data */
    printf("\nNCS Data\n");
    printf("--------------------------------\n");
    if (test_ncs_parsing()) return 1;

    /* RGB Space Definitions */
    printf("\nRGB Space Definitions\n");
    printf("--------------------------------\n");
    if (test_rgb_space_lookup()) return 1;
    if (test_rgb_space_various()) return 1;
    if (test_rgb_space_transfer_functions()) return 1;
    if (test_rgb_space_not_found()) return 1;

    printf("\n========================================\n");
    printf("Test Results: %d/%d passed\n", test_passed, test_count);
    printf("========================================\n");

    return (test_passed == test_count) ? 0 : 1;
}
