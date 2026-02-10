/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 37: P8.1 RGB to Spectrum Conversion (Spectral Upsampling)
 *
 * Tests the spectral upsampling methods (Smits1999, Mallett2019, and Jakob2019)
 * Reference values generated from colour-science Python library
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>

static alwan_scalar vec3_max_diff(alwan_xyz const *a, alwan_xyz const *b) {
    alwan_scalar max_diff = 0;
    alwan_scalar diff_x = ALWAN_ABS(a->x - b->x);
    alwan_scalar diff_y = ALWAN_ABS(a->y - b->y);
    alwan_scalar diff_z = ALWAN_ABS(a->z - b->z);

    if (diff_x > max_diff) max_diff = diff_x;
    if (diff_y > max_diff) max_diff = diff_y;
    if (diff_z > max_diff) max_diff = diff_z;

    return max_diff;
}

static void vec3_print(char const *name, alwan_xyz const *v) {
    printf("%s: [%12.8f %12.8f %12.8f]\n", name, v->x, v->y, v->z);
}

static void rgb_print(char const *name, alwan_rgb const *v) {
    printf("%s: [%12.8f %12.8f %12.8f]\n", name, v->r, v->g, v->b);
}

/* ----------------------------------------------------------------
 * Reference Value Loading
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* Smits1999 test data */
static alwan_scalar const smits1999_white_xyz_recovered[] = {
#include "reference_values/smits1999_white_xyz_recovered.csv"
};

static alwan_scalar const smits1999_white_xyz_expected[] = {
#include "reference_values/smits1999_white_xyz_expected.csv"
};

static alwan_scalar const smits1999_red_xyz_recovered[] = {
#include "reference_values/smits1999_red_xyz_recovered.csv"
};

static alwan_scalar const smits1999_red_xyz_expected[] = {
#include "reference_values/smits1999_red_xyz_expected.csv"
};

static alwan_scalar const smits1999_green_xyz_recovered[] = {
#include "reference_values/smits1999_green_xyz_recovered.csv"
};

static alwan_scalar const smits1999_green_xyz_expected[] = {
#include "reference_values/smits1999_green_xyz_expected.csv"
};

static alwan_scalar const smits1999_blue_xyz_recovered[] = {
#include "reference_values/smits1999_blue_xyz_recovered.csv"
};

static alwan_scalar const smits1999_blue_xyz_expected[] = {
#include "reference_values/smits1999_blue_xyz_expected.csv"
};

static alwan_scalar const smits1999_gray50_xyz_recovered[] = {
#include "reference_values/smits1999_gray50_xyz_recovered.csv"
};

static alwan_scalar const smits1999_gray50_xyz_expected[] = {
#include "reference_values/smits1999_gray50_xyz_expected.csv"
};

/* Mallett2019 test data */
static alwan_scalar const mallett2019_white_xyz_recovered[] = {
#include "reference_values/mallett2019_white_xyz_recovered.csv"
};

static alwan_scalar const mallett2019_white_xyz_expected[] = {
#include "reference_values/mallett2019_white_xyz_expected.csv"
};

static alwan_scalar const mallett2019_red_xyz_recovered[] = {
#include "reference_values/mallett2019_red_xyz_recovered.csv"
};

static alwan_scalar const mallett2019_red_xyz_expected[] = {
#include "reference_values/mallett2019_red_xyz_expected.csv"
};

static alwan_scalar const mallett2019_green_xyz_recovered[] = {
#include "reference_values/mallett2019_green_xyz_recovered.csv"
};

static alwan_scalar const mallett2019_green_xyz_expected[] = {
#include "reference_values/mallett2019_green_xyz_expected.csv"
};

static alwan_scalar const mallett2019_blue_xyz_recovered[] = {
#include "reference_values/mallett2019_blue_xyz_recovered.csv"
};

static alwan_scalar const mallett2019_blue_xyz_expected[] = {
#include "reference_values/mallett2019_blue_xyz_expected.csv"
};

static alwan_scalar const mallett2019_gray50_xyz_recovered[] = {
#include "reference_values/mallett2019_gray50_xyz_recovered.csv"
};

static alwan_scalar const mallett2019_gray50_xyz_expected[] = {
#include "reference_values/mallett2019_gray50_xyz_expected.csv"
};

/* Note: Jakob2019 reference values not generated (colour-science requires slow optimization).
 * Jakob2019 tests only validate structure and basic functionality. */

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Test: Smits1999 RGB to Spectrum Round-trip
 * ---------------------------------------------------------------- */

static int test_smits1999_round_trip(char const *color_name,
                                      alwan_scalar r, alwan_scalar g, alwan_scalar b,
                                      alwan_scalar const *expected_xyz) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    alwan_rgb rgb = {r, g, b};
    alwan_xyz expected = {expected_xyz[0], expected_xyz[1], expected_xyz[2]};
    alwan_spd spectrum = {0};
    alwan_spd illuminant_d65 = {0};
    alwan_xyz xyz_recovered;

    /* Convert RGB to spectrum using Smits1999 */
    int status = alwan_rgb_to_spectrum_smits1999(&spectrum, ctx, &rgb);
    TEST_ASSERT(status == ALWAN_OK, "alwan_rgb_to_spectrum_smits1999 failed");

    /* Get D65 illuminant SPD */
    status = alwan_spd_illuminant(&illuminant_d65, ctx, ALWAN_ILLUMINANT_D65);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &spectrum);
        alwan_destroy(ctx);
        TEST_ASSERT(0, "Failed to load D65 illuminant");
    }

    /* Convert spectrum back to XYZ using D65 illuminant */
    status = alwan_xyz_from_spd(&xyz_recovered, ctx, &spectrum, &illuminant_d65,
                                 ALWAN_OBSERVER_CIE_1931_2DEG,
                                 ALWAN_INTEGRATE_TRAPEZOID,
                                 ALWAN_LITERAL(0.0));

    TEST_ASSERT(status == ALWAN_OK, "alwan_xyz_from_spd failed");

    /* Normalize XYZ: alwan_xyz_from_spd returns unnormalized values
     * We need to divide by the normalization constant K = integral illuminant(lambda) * y_bar(lambda) dlambda
     * For D65 with CIE 1931 2°, this is approximately 10600 */
    alwan_scalar const K_D65 = ALWAN_LITERAL(10599.3675);  /* Normalization constant for D65 */
    xyz_recovered.x /= K_D65;
    xyz_recovered.y /= K_D65;
    xyz_recovered.z /= K_D65;

    /* Clean up */
    alwan_spd_destroy(ctx, &spectrum);
    alwan_spd_destroy(ctx, &illuminant_d65);

    /* Compare recovered XYZ with expected
     * Note: Spectral upsampling is approximate, so we use a larger tolerance */
    alwan_scalar tolerance = ALWAN_LITERAL(0.08);  /* 8% tolerance for Smits1999 round-trip */
    alwan_scalar diff = vec3_max_diff(&xyz_recovered, &expected);

    if (diff >= tolerance) {
        fprintf(stderr, "Round-trip error too large: max_diff = %.6f (tolerance = %.6f)\n",
                diff, tolerance);
        rgb_print("  RGB input", &rgb);
        vec3_print("  XYZ expected", &expected);
        vec3_print("  XYZ recovered", &xyz_recovered);
        alwan_destroy(ctx);
        return 1;
    }

    alwan_destroy(ctx);
    char test_name[256];
    snprintf(test_name, sizeof(test_name), "Smits1999: %s RGB->Spectrum->XYZ", color_name);
    TEST_PASS(test_name);
}

/* ----------------------------------------------------------------
 * Test: Mallett2019 RGB to Spectrum Round-trip
 * ---------------------------------------------------------------- */

static int test_mallett2019_round_trip(char const *color_name,
                                        alwan_scalar r, alwan_scalar g, alwan_scalar b,
                                        alwan_scalar const *expected_xyz) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    alwan_rgb rgb = {r, g, b};
    alwan_xyz expected = {expected_xyz[0], expected_xyz[1], expected_xyz[2]};
    alwan_spd spectrum = {0};
    alwan_spd illuminant_d65 = {0};
    alwan_xyz xyz_recovered;

    /* Convert RGB to spectrum using Mallett2019 */
    int status = alwan_rgb_to_spectrum_mallett2019(&spectrum, ctx, &rgb);
    TEST_ASSERT(status == ALWAN_OK, "alwan_rgb_to_spectrum_mallett2019 failed");

    /* Get D65 illuminant SPD */
    status = alwan_spd_illuminant(&illuminant_d65, ctx, ALWAN_ILLUMINANT_D65);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &spectrum);
        alwan_destroy(ctx);
        TEST_ASSERT(0, "Failed to load D65 illuminant");
    }

    /* Convert spectrum back to XYZ using D65 illuminant */
    status = alwan_xyz_from_spd(&xyz_recovered, ctx, &spectrum, &illuminant_d65,
                                 ALWAN_OBSERVER_CIE_1931_2DEG,
                                 ALWAN_INTEGRATE_TRAPEZOID,
                                 ALWAN_LITERAL(0.0));

    TEST_ASSERT(status == ALWAN_OK, "alwan_xyz_from_spd failed");

    /* Normalize XYZ: alwan_xyz_from_spd returns unnormalized values
     * We need to divide by the normalization constant K = integral illuminant(lambda) * y_bar(lambda) dlambda
     * For D65 with CIE 1931 2°, this is approximately 10600 */
    alwan_scalar const K_D65 = ALWAN_LITERAL(10567.2678);  /* Normalization constant for D65 (Mallett 5nm spacing) */
    xyz_recovered.x /= K_D65;
    xyz_recovered.y /= K_D65;
    xyz_recovered.z /= K_D65;

    /* Clean up */
    alwan_spd_destroy(ctx, &spectrum);
    alwan_spd_destroy(ctx, &illuminant_d65);

    /* Compare recovered XYZ with expected
     * Mallett2019 should have better accuracy than Smits1999 */
    alwan_scalar tolerance = ALWAN_LITERAL(0.01);  /* 1% tolerance for Mallett2019 */
    alwan_scalar diff = vec3_max_diff(&xyz_recovered, &expected);

    if (diff >= tolerance) {
        fprintf(stderr, "Round-trip error too large: max_diff = %.6f (tolerance = %.6f)\n",
                diff, tolerance);
        rgb_print("  RGB input", &rgb);
        vec3_print("  XYZ expected", &expected);
        vec3_print("  XYZ recovered", &xyz_recovered);
        alwan_destroy(ctx);
        return 1;
    }

    alwan_destroy(ctx);
    char test_name[256];
    snprintf(test_name, sizeof(test_name), "Mallett2019: %s RGB->Spectrum->XYZ", color_name);
    TEST_PASS(test_name);
}

/* ----------------------------------------------------------------
 * Test: Jakob2019 RGB to Spectrum Round-trip
 * ---------------------------------------------------------------- */

static int test_jakob2019_structure(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    alwan_rgb rgb = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)};
    alwan_spd spectrum = {0};

    /* Convert RGB to spectrum using Jakob2019 with sRGB gamut */
    int status = alwan_rgb_to_spectrum_jakob2019(&spectrum, ctx, ALWAN_JAKOB2019_SRGB, &rgb);
    TEST_ASSERT(status == ALWAN_OK, "alwan_rgb_to_spectrum_jakob2019 failed");

    /* Validate spectrum structure */
    TEST_ASSERT(spectrum.count >= 36, "Jakob2019 spectrum has too few samples");
    TEST_ASSERT(spectrum.wavelength_min == ALWAN_LITERAL(360.0), "Jakob2019 min wavelength incorrect");
    TEST_ASSERT(spectrum.wavelength_max == ALWAN_LITERAL(780.0), "Jakob2019 max wavelength incorrect");

    /* Validate all spectrum values are non-negative */
    for (size_t i = 0; i < spectrum.count; i++) {
        TEST_ASSERT(spectrum.values[i] >= ALWAN_LITERAL(0.0), "Jakob2019 spectrum has negative value");
    }

    /* Clean up */
    alwan_spd_destroy(ctx, &spectrum);
    alwan_destroy(ctx);

    TEST_PASS("Jakob2019: spectrum structure validation");
}

/* ----------------------------------------------------------------
 * Test: Spectrum Structure Validation
 * ---------------------------------------------------------------- */

static int test_spectrum_structure_smits1999(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    alwan_rgb rgb = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};  /* Red */
    alwan_spd spectrum = {0};

    int status = alwan_rgb_to_spectrum_smits1999(&spectrum, ctx, &rgb);
    TEST_ASSERT(status == ALWAN_OK, "alwan_rgb_to_spectrum_smits1999 failed");

    /* Validate spectrum structure */
    TEST_ASSERT(spectrum.values != NULL, "Spectrum values NULL");
    TEST_ASSERT(spectrum.count == 10, "Smits1999 should have 10 samples");
    TEST_ASSERT(spectrum.wavelength_min == ALWAN_LITERAL(380.0), "Wrong wavelength_min");
    TEST_ASSERT(spectrum.wavelength_max == ALWAN_LITERAL(720.0), "Wrong wavelength_max");

    /* Validate that spectrum values are reasonable (non-negative, bounded) */
    for (size_t i = 0; i < spectrum.count; i++) {
        TEST_ASSERT(spectrum.values[i] >= ALWAN_LITERAL(0.0), "Negative reflectance value");
        TEST_ASSERT(spectrum.values[i] <= ALWAN_LITERAL(1.1), "Reflectance > 1.1 (unreasonable)");
    }

    alwan_spd_destroy(ctx, &spectrum);
    alwan_destroy(ctx);
    TEST_PASS("Smits1999: spectrum structure validation");
}

static int test_spectrum_structure_mallett2019(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    alwan_rgb rgb = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)};  /* Green */
    alwan_spd spectrum = {0};

    int status = alwan_rgb_to_spectrum_mallett2019(&spectrum, ctx, &rgb);
    TEST_ASSERT(status == ALWAN_OK, "alwan_rgb_to_spectrum_mallett2019 failed");

    /* Validate spectrum structure */
    TEST_ASSERT(spectrum.values != NULL, "Spectrum values NULL");
    TEST_ASSERT(spectrum.count == 81, "Mallett2019 should have 81 samples");
    TEST_ASSERT(spectrum.wavelength_min == ALWAN_LITERAL(380.0), "Wrong wavelength_min");
    TEST_ASSERT(spectrum.wavelength_max == ALWAN_LITERAL(780.0), "Wrong wavelength_max");

    /* Validate that spectrum values are reasonable (allow negative due to basis functions) */
    int has_positive = 0;
    for (size_t i = 0; i < spectrum.count; i++) {
        if (spectrum.values[i] > ALWAN_LITERAL(0.0)) {
            has_positive = 1;
        }
        /* Mallett2019 can have small negative values, but not too large */
        TEST_ASSERT(spectrum.values[i] >= ALWAN_LITERAL(-0.1), "Too negative reflectance");
        TEST_ASSERT(spectrum.values[i] <= ALWAN_LITERAL(1.1), "Reflectance > 1.1 (unreasonable)");
    }
    TEST_ASSERT(has_positive, "Spectrum should have some positive values");

    alwan_spd_destroy(ctx, &spectrum);
    alwan_destroy(ctx);
    TEST_PASS("Mallett2019: spectrum structure validation");
}

static int test_spectrum_structure_jakob2019(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    alwan_rgb rgb = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)};  /* Blue */
    alwan_spd spectrum = {0};

    int status = alwan_rgb_to_spectrum_jakob2019(&spectrum, ctx, ALWAN_JAKOB2019_SRGB, &rgb);
    TEST_ASSERT(status == ALWAN_OK, "alwan_rgb_to_spectrum_jakob2019 failed");

    /* Validate spectrum structure */
    TEST_ASSERT(spectrum.values != NULL, "Spectrum values NULL");
    TEST_ASSERT(spectrum.count == 85, "Jakob2019 should have 85 samples");
    TEST_ASSERT(spectrum.wavelength_min == ALWAN_LITERAL(360.0), "Wrong wavelength_min");
    TEST_ASSERT(spectrum.wavelength_max == ALWAN_LITERAL(780.0), "Wrong wavelength_max");

    /* Validate that spectrum values are reasonable (non-negative, bounded) */
    for (size_t i = 0; i < spectrum.count; i++) {
        TEST_ASSERT(spectrum.values[i] >= ALWAN_LITERAL(0.0), "Negative reflectance value");
        TEST_ASSERT(spectrum.values[i] <= ALWAN_LITERAL(1.1), "Reflectance > 1.1 (unreasonable)");
    }

    alwan_spd_destroy(ctx, &spectrum);
    alwan_destroy(ctx);
    TEST_PASS("Jakob2019: spectrum structure validation");
}

/* ----------------------------------------------------------------
 * Test Suite
 * ---------------------------------------------------------------- */

int test_37_rgb_to_spectrum_main(void) {
    int failed = 0;

    /* Spectrum structure validation */
    failed += test_spectrum_structure_smits1999();
    failed += test_spectrum_structure_mallett2019();
    failed += test_spectrum_structure_jakob2019();

    /* Smits1999 round-trip tests */
    failed += test_smits1999_round_trip("white", 1.0, 1.0, 1.0, smits1999_white_xyz_recovered);
    failed += test_smits1999_round_trip("red", 1.0, 0.0, 0.0, smits1999_red_xyz_recovered);
    failed += test_smits1999_round_trip("green", 0.0, 1.0, 0.0, smits1999_green_xyz_recovered);
    failed += test_smits1999_round_trip("blue", 0.0, 0.0, 1.0, smits1999_blue_xyz_recovered);
    failed += test_smits1999_round_trip("gray50", 0.5, 0.5, 0.5, smits1999_gray50_xyz_recovered);

    /* Mallett2019 round-trip tests */
    failed += test_mallett2019_round_trip("white", 1.0, 1.0, 1.0, mallett2019_white_xyz_recovered);
    failed += test_mallett2019_round_trip("red", 1.0, 0.0, 0.0, mallett2019_red_xyz_recovered);
    failed += test_mallett2019_round_trip("green", 0.0, 1.0, 0.0, mallett2019_green_xyz_recovered);
    failed += test_mallett2019_round_trip("blue", 0.0, 0.0, 1.0, mallett2019_blue_xyz_recovered);
    failed += test_mallett2019_round_trip("gray50", 0.5, 0.5, 0.5, mallett2019_gray50_xyz_recovered);

    /* Jakob2019 tests - only structure validation (no reference values) */
    failed += test_jakob2019_structure();

    return failed;
}
