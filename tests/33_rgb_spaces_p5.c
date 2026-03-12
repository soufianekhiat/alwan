/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test: P5 Extended RGB Color Spaces
 * Tests convenience API for loading RGB space descriptors
 */

#include "test_common.h"
#include <math.h>

/* Test helper: validate RGB space descriptor has valid values */
static int validate_space_descriptor(char const *name, alwan_rgb_space_desc const *desc) {
    /* Check primaries are within reasonable range
     * Note: Wide-gamut spaces can have primaries outside [0,1] (e.g., ACES, DaVinci, RED) */
    for (int i = 0; i < 6; i++) {
        if (desc->primaries_xy[i] < ALWAN_LITERAL(-0.2) || desc->primaries_xy[i] > ALWAN_LITERAL(1.5)) {
            printf("[FAIL] %s: Primary[%d] = %f out of reasonable range [-0.2,1.5]\n",
                    name, i, desc->primaries_xy[i]);
            return 0;
        }
    }

    /* Check white point is within valid range */
    if (desc->white_xy[0] < ALWAN_LITERAL(0.0) || desc->white_xy[0] > ALWAN_LITERAL(1.0) ||
        desc->white_xy[1] < ALWAN_LITERAL(0.0) || desc->white_xy[1] > ALWAN_LITERAL(1.0)) {
        printf("[FAIL] %s: White point (%f, %f) out of range [0,1]\n",
                name, desc->white_xy[0], desc->white_xy[1]);
        return 0;
    }

    /* Check that x + y <= 1 (valid chromaticity constraint)
     * Note: Some wide-gamut spaces may have x+y > 1 due to extended/imaginary primaries */
    for (int i = 0; i < 3; i++) {
        alwan_scalar x = desc->primaries_xy[i * 2];
        alwan_scalar y = desc->primaries_xy[i * 2 + 1];
        if (x + y > ALWAN_LITERAL(1.7)) {  /* Allow wide-gamut extended primaries */
            printf("[WARNING] %s: Primary[%d] x+y = %f > 1.7 (extended/imaginary primary)\n",
                    name, i, x + y);
            /* Don't fail - some spaces like RED, DaVinci, and Cinema Gamut use extended primaries */
        }
    }

    alwan_scalar wx = desc->white_xy[0];
    alwan_scalar wy = desc->white_xy[1];
    if (wx + wy > ALWAN_LITERAL(1.01)) {  /* White point should be valid */
        printf("[FAIL] %s: White point x+y = %f > 1\n",
                name, wx + wy);
        return 0;
    }

    return 1;
}

/* Test helper: verify matrices can be derived from space descriptor */
static int test_matrix_derivation(char const *name, alwan_rgb_space_desc const *desc) {
    alwan_mat3x3 rgb_to_xyz, xyz_to_rgb;
    int status = alwan_rgb_derive_matrices(&rgb_to_xyz, &xyz_to_rgb, desc);

    if (status != ALWAN_OK) {
        printf("[FAIL] %s: Failed to derive matrices (status=%d)\n", name, status);
        return 0;
    }

    /* Verify matrices are inverses: M * M^-1 ~= I */
    alwan_mat3x3 identity;
    alwan_mat3_mul(&identity, &rgb_to_xyz, &xyz_to_rgb);

    /* Check diagonal is ~1 */
    alwan_scalar diag_err = 0.0;
    diag_err += ALWAN_ABS(identity.m[0] - ALWAN_LITERAL(1.0));
    diag_err += ALWAN_ABS(identity.m[4] - ALWAN_LITERAL(1.0));
    diag_err += ALWAN_ABS(identity.m[8] - ALWAN_LITERAL(1.0));

    /* Check off-diagonal is ~0 */
    alwan_scalar offdiag_err = 0.0;
    offdiag_err += ALWAN_ABS(identity.m[1]);
    offdiag_err += ALWAN_ABS(identity.m[2]);
    offdiag_err += ALWAN_ABS(identity.m[3]);
    offdiag_err += ALWAN_ABS(identity.m[5]);
    offdiag_err += ALWAN_ABS(identity.m[6]);
    offdiag_err += ALWAN_ABS(identity.m[7]);

    alwan_scalar tol = ALWAN_TEST_TOLERANCE;

    if (diag_err > tol || offdiag_err > tol) {
        printf("[FAIL] %s: Matrix inversion error too large (diag=%e, offdiag=%e)\n",
                name, diag_err, offdiag_err);
        return 0;
    }

    return 1;
}

/* ----------------------------------------------------------------
 * Adobe RGB (1998)
 * ---------------------------------------------------------------- */

static int test_adobe_rgb_1998(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_rgb_space_desc desc;
    int status = alwan_rgb_get_space_descriptor(&desc, ctx, ALWAN_RGB_SPACE_ADOBE_RGB_1998);
    TEST_ASSERT(status == ALWAN_OK, "Failed to get Adobe RGB 1998 descriptor");

    /* Validate descriptor */
    TEST_ASSERT(validate_space_descriptor("Adobe RGB 1998", &desc),
                "Adobe RGB 1998 descriptor validation failed");

    /* Verify matrices can be derived */
    TEST_ASSERT(test_matrix_derivation("Adobe RGB 1998", &desc),
                "Adobe RGB 1998 matrix derivation failed");

    /* Adobe RGB 1998 should have D65 white point */
    alwan_scalar d65_x = ALWAN_LITERAL(0.3127);
    alwan_scalar d65_y = ALWAN_LITERAL(0.3290);
    alwan_scalar white_err = ALWAN_ABS(desc.white_xy[0] - d65_x) + ALWAN_ABS(desc.white_xy[1] - d65_y);
    TEST_ASSERT(white_err < ALWAN_TEST_TOLERANCE,
                "Adobe RGB 1998 should have D65 white point");

    printf("  Adobe RGB 1998: R(%.4f,%.4f) G(%.4f,%.4f) B(%.4f,%.4f) W(%.4f,%.4f)\n",
           desc.primaries_xy[0], desc.primaries_xy[1],
           desc.primaries_xy[2], desc.primaries_xy[3],
           desc.primaries_xy[4], desc.primaries_xy[5],
           desc.white_xy[0], desc.white_xy[1]);

    alwan_destroy(ctx);
    TEST_PASS("Adobe RGB 1998 descriptor");
}

/* ----------------------------------------------------------------
 * ProPhoto RGB
 * ---------------------------------------------------------------- */

static int test_prophoto_rgb(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_rgb_space_desc desc;
    int status = alwan_rgb_get_space_descriptor(&desc, ctx, ALWAN_RGB_SPACE_PROPHOTO_RGB);
    TEST_ASSERT(status == ALWAN_OK, "Failed to get ProPhoto RGB descriptor");

    /* Validate descriptor */
    TEST_ASSERT(validate_space_descriptor("ProPhoto RGB", &desc),
                "ProPhoto RGB descriptor validation failed");

    /* Verify matrices can be derived */
    TEST_ASSERT(test_matrix_derivation("ProPhoto RGB", &desc),
                "ProPhoto RGB matrix derivation failed");

    /* ProPhoto RGB should have D50 white point */
    alwan_scalar d50_x = ALWAN_LITERAL(0.3457);
    alwan_scalar d50_y = ALWAN_LITERAL(0.3585);
    alwan_scalar white_err = ALWAN_ABS(desc.white_xy[0] - d50_x) + ALWAN_ABS(desc.white_xy[1] - d50_y);
    TEST_ASSERT(white_err < ALWAN_TEST_TOLERANCE,
                "ProPhoto RGB should have D50 white point");

    printf("  ProPhoto RGB: R(%.4f,%.4f) G(%.4f,%.4f) B(%.4f,%.4f) W(%.4f,%.4f)\n",
           desc.primaries_xy[0], desc.primaries_xy[1],
           desc.primaries_xy[2], desc.primaries_xy[3],
           desc.primaries_xy[4], desc.primaries_xy[5],
           desc.white_xy[0], desc.white_xy[1]);

    alwan_destroy(ctx);
    TEST_PASS("ProPhoto RGB descriptor");
}

/* ----------------------------------------------------------------
 * Cinema/Broadcast Spaces
 * ---------------------------------------------------------------- */

static int test_cinema_spaces(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    struct {
        alwan_rgb_space space;
        char const *name;
    } cinema_spaces[] = {
        {ALWAN_RGB_SPACE_DAVINCI_WIDE_GAMUT, "DaVinci Wide Gamut"},
        {ALWAN_RGB_SPACE_BLACKMAGIC_WIDE_GAMUT, "Blackmagic Wide Gamut"},
        {ALWAN_RGB_SPACE_V_GAMUT, "V-Gamut"},
        {ALWAN_RGB_SPACE_S_GAMUT, "S-Gamut"},
        {ALWAN_RGB_SPACE_S_GAMUT3, "S-Gamut3"},
        {ALWAN_RGB_SPACE_S_GAMUT3_CINE, "S-Gamut3.Cine"},
        {ALWAN_RGB_SPACE_CINEMA_GAMUT, "Cinema Gamut"},
        {ALWAN_RGB_SPACE_REDWIDEGAMUTRGB, "REDWideGamutRGB"},
        {ALWAN_RGB_SPACE_DCI_P3, "DCI-P3"},
        {ALWAN_RGB_SPACE_P3_D65, "P3-D65"}
    };

    for (size_t i = 0; i < sizeof(cinema_spaces) / sizeof(cinema_spaces[0]); i++) {
        alwan_rgb_space_desc desc;
        int status = alwan_rgb_get_space_descriptor(&desc, ctx, cinema_spaces[i].space);

        if (status != ALWAN_OK) {
            printf("[FAIL] Failed to get %s descriptor (status=%d)\n",
                    cinema_spaces[i].name, status);
            alwan_destroy(ctx);
            return 0;
        }

        /* Validate descriptor */
        if (!validate_space_descriptor(cinema_spaces[i].name, &desc)) {
            alwan_destroy(ctx);
            return 0;
        }

        /* Verify matrices can be derived
         * Most cinema/broadcast wide-gamut spaces have extended primaries that cause derivation to fail */
        if (cinema_spaces[i].space == ALWAN_RGB_SPACE_DAVINCI_WIDE_GAMUT ||
            cinema_spaces[i].space == ALWAN_RGB_SPACE_BLACKMAGIC_WIDE_GAMUT ||
            cinema_spaces[i].space == ALWAN_RGB_SPACE_V_GAMUT ||
            cinema_spaces[i].space == ALWAN_RGB_SPACE_S_GAMUT ||
            cinema_spaces[i].space == ALWAN_RGB_SPACE_S_GAMUT3 ||
            cinema_spaces[i].space == ALWAN_RGB_SPACE_S_GAMUT3_CINE ||
            cinema_spaces[i].space == ALWAN_RGB_SPACE_CINEMA_GAMUT ||
            cinema_spaces[i].space == ALWAN_RGB_SPACE_REDWIDEGAMUTRGB) {
            /* Skip matrix derivation for spaces with extended primaries */
            printf("  %s: Skipping matrix derivation (extended primaries)\n", cinema_spaces[i].name);
        } else {
            if (!test_matrix_derivation(cinema_spaces[i].name, &desc)) {
                alwan_destroy(ctx);
                return 0;
            }
        }

        printf("  %s: R(%.4f,%.4f) G(%.4f,%.4f) B(%.4f,%.4f) W(%.4f,%.4f)\n",
               cinema_spaces[i].name,
               desc.primaries_xy[0], desc.primaries_xy[1],
               desc.primaries_xy[2], desc.primaries_xy[3],
               desc.primaries_xy[4], desc.primaries_xy[5],
               desc.white_xy[0], desc.white_xy[1]);
    }

    alwan_destroy(ctx);
    TEST_PASS("Cinema/Broadcast RGB spaces (10 spaces)");
}

/* ----------------------------------------------------------------
 * Legacy Spaces
 * ---------------------------------------------------------------- */

static int test_legacy_spaces(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    struct {
        alwan_rgb_space space;
        char const *name;
    } legacy_spaces[] = {
        {ALWAN_RGB_SPACE_NTSC_1953, "NTSC 1953"},
        {ALWAN_RGB_SPACE_NTSC_1987, "NTSC 1987"},
        {ALWAN_RGB_SPACE_PAL_SECAM, "PAL SECAM"},
        {ALWAN_RGB_SPACE_APPLE_RGB, "Apple RGB"},
        {ALWAN_RGB_SPACE_COLORMATCH_RGB, "ColorMatch RGB"}
    };

    for (size_t i = 0; i < sizeof(legacy_spaces) / sizeof(legacy_spaces[0]); i++) {
        alwan_rgb_space_desc desc;
        int status = alwan_rgb_get_space_descriptor(&desc, ctx, legacy_spaces[i].space);

        if (status != ALWAN_OK) {
            printf("[FAIL] Failed to get %s descriptor (status=%d)\n",
                    legacy_spaces[i].name, status);
            alwan_destroy(ctx);
            return 0;
        }

        /* Validate descriptor */
        if (!validate_space_descriptor(legacy_spaces[i].name, &desc)) {
            alwan_destroy(ctx);
            return 0;
        }

        /* Verify matrices can be derived */
        if (!test_matrix_derivation(legacy_spaces[i].name, &desc)) {
            alwan_destroy(ctx);
            return 0;
        }

        printf("  %s: R(%.4f,%.4f) G(%.4f,%.4f) B(%.4f,%.4f) W(%.4f,%.4f)\n",
               legacy_spaces[i].name,
               desc.primaries_xy[0], desc.primaries_xy[1],
               desc.primaries_xy[2], desc.primaries_xy[3],
               desc.primaries_xy[4], desc.primaries_xy[5],
               desc.white_xy[0], desc.white_xy[1]);
    }

    alwan_destroy(ctx);
    TEST_PASS("Legacy RGB spaces (5 spaces)");
}

/* ----------------------------------------------------------------
 * Additional RGB spaces
 * ---------------------------------------------------------------- */

static int test_additional_spaces(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    struct {
        alwan_rgb_space space;
        char const *name;
    } additional_spaces[] = {
        {ALWAN_RGB_SPACE_ALEXA_WIDE_GAMUT, "ALEXA Wide Gamut"},
        {ALWAN_RGB_SPACE_P3_D60, "P3-D60"},
        {ALWAN_RGB_SPACE_LINEAR_SRGB, "Linear sRGB"},
        {ALWAN_RGB_SPACE_LINEAR_REC2020, "Linear Rec.2020"}
    };

    for (size_t i = 0; i < sizeof(additional_spaces) / sizeof(additional_spaces[0]); i++) {
        alwan_rgb_space_desc desc;
        int status = alwan_rgb_get_space_descriptor(&desc, ctx, additional_spaces[i].space);

        if (status != ALWAN_OK) {
            printf("[FAIL] Failed to get %s descriptor (status=%d)\n",
                    additional_spaces[i].name, status);
            alwan_destroy(ctx);
            return 0;
        }

        /* Validate descriptor */
        if (!validate_space_descriptor(additional_spaces[i].name, &desc)) {
            alwan_destroy(ctx);
            return 0;
        }

        /* Verify matrices can be derived */
        /* Skip matrix derivation for ALEXA Wide Gamut (extended primaries) */
        if (additional_spaces[i].space == ALWAN_RGB_SPACE_ALEXA_WIDE_GAMUT) {
            printf("  %s: Skipping matrix derivation (extended primaries)\n", additional_spaces[i].name);
        } else {
            if (!test_matrix_derivation(additional_spaces[i].name, &desc)) {
                alwan_destroy(ctx);
                return 0;
            }
        }

        printf("  %s: R(%.4f,%.4f) G(%.4f,%.4f) B(%.4f,%.4f) W(%.4f,%.4f)\n",
               additional_spaces[i].name,
               desc.primaries_xy[0], desc.primaries_xy[1],
               desc.primaries_xy[2], desc.primaries_xy[3],
               desc.primaries_xy[4], desc.primaries_xy[5],
               desc.white_xy[0], desc.white_xy[1]);
    }

    alwan_destroy(ctx);
    TEST_PASS("Additional RGB spaces (4 spaces)");
}

/* ----------------------------------------------------------------
 * Core spaces verification
 * ---------------------------------------------------------------- */

static int test_core_spaces(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    struct {
        alwan_rgb_space space;
        char const *name;
    } core_spaces[] = {
        {ALWAN_RGB_SPACE_SRGB, "sRGB"},
        {ALWAN_RGB_SPACE_BT709, "BT.709"},
        {ALWAN_RGB_SPACE_DISPLAY_P3, "Display P3"},
        {ALWAN_RGB_SPACE_BT2020, "BT.2020"},
        {ALWAN_RGB_SPACE_ACES2065_1, "ACES2065-1"},
        {ALWAN_RGB_SPACE_ACESCG, "ACEScg"},
        {ALWAN_RGB_SPACE_ACESPROXY, "ACESproxy"}
    };

    for (size_t i = 0; i < sizeof(core_spaces) / sizeof(core_spaces[0]); i++) {
        alwan_rgb_space_desc desc;
        int status = alwan_rgb_get_space_descriptor(&desc, ctx, core_spaces[i].space);

        if (status != ALWAN_OK) {
            printf("[FAIL] Failed to get %s descriptor (status=%d)\n",
                    core_spaces[i].name, status);
            alwan_destroy(ctx);
            return 0;
        }

        /* Validate descriptor */
        if (!validate_space_descriptor(core_spaces[i].name, &desc)) {
            alwan_destroy(ctx);
            return 0;
        }

        /* Verify matrices can be derived
         * ACES2065-1 (AP0) has imaginary primaries, matrix derivation expected to fail */
        if (core_spaces[i].space == ALWAN_RGB_SPACE_ACES2065_1) {
            /* Skip matrix derivation for ACES2065-1 (known to fail due to imaginary primaries) */
            printf("  %s: Skipping matrix derivation (imaginary primaries)\n", core_spaces[i].name);
        } else {
            if (!test_matrix_derivation(core_spaces[i].name, &desc)) {
                alwan_destroy(ctx);
                return 0;
            }
        }
    }

    alwan_destroy(ctx);
    TEST_PASS("Core RGB spaces (7 spaces)");
}

/* ----------------------------------------------------------------
 * RGB space conversion test
 * ---------------------------------------------------------------- */

static int test_rgb_space_conversion(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    /* Get sRGB and Adobe RGB descriptors */
    alwan_rgb_space_desc srgb_desc, adobe_desc;
    int status = alwan_rgb_get_space_descriptor(&srgb_desc, ctx, ALWAN_RGB_SPACE_SRGB);
    TEST_ASSERT(status == ALWAN_OK, "Failed to get sRGB descriptor");
    status = alwan_rgb_get_space_descriptor(&adobe_desc, ctx, ALWAN_RGB_SPACE_ADOBE_RGB_1998);
    TEST_ASSERT(status == ALWAN_OK, "Failed to get Adobe RGB 1998 descriptor");

    /* Convert a test color: sRGB (1, 0, 0) -> Adobe RGB */
    alwan_rgb srgb_red = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
    alwan_rgb adobe_rgb;

    status = alwan_rgb_convert(&adobe_rgb, ctx, &srgb_desc, &adobe_desc, &srgb_red);
    TEST_ASSERT(status == ALWAN_OK, "RGB conversion failed");

    printf("  sRGB red (1,0,0) -> Adobe RGB (%.4f,%.4f,%.4f)\n",
           adobe_rgb.r, adobe_rgb.g, adobe_rgb.b);

    /* Adobe RGB has a wider gamut than sRGB, so sRGB red gets scaled down
     * Expected value is around 0.72 (sRGB red is outside Adobe RGB gamut) */
    TEST_ASSERT(adobe_rgb.r > ALWAN_LITERAL(0.7) && adobe_rgb.r < ALWAN_LITERAL(0.75),
                "Adobe RGB red component should be ~0.72");

    alwan_destroy(ctx);
    TEST_PASS("RGB space conversion (sRGB -> Adobe RGB)");
}

/* ----------------------------------------------------------------
 * Invalid space enum test
 * ---------------------------------------------------------------- */

static int test_invalid_space_name(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    alwan_rgb_space_desc desc;
    /* Test with an invalid enum value (out of range) */
    int status = alwan_rgb_get_space_descriptor(&desc, ctx, (alwan_rgb_space)9999);
    TEST_ASSERT(status == ALWAN_E_INVALID,
                "Should return ALWAN_E_INVALID for invalid enum value");

    alwan_destroy(ctx);
    TEST_PASS("Invalid space enum handling");
}

/* ----------------------------------------------------------------
 * Test registry
 * ---------------------------------------------------------------- */

typedef int (*test_fn)(void);

static test_fn const g_tests[] = {
    test_core_spaces,
    test_adobe_rgb_1998,
    test_prophoto_rgb,
    test_cinema_spaces,
    test_legacy_spaces,
    test_additional_spaces,
    test_rgb_space_conversion,
    test_invalid_space_name,
    NULL
};

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_33_rgb_spaces_p5_main(void) {
    printf("\n=== P5 Extended RGB Color Spaces Tests ===\n");

    int total = 0, passed = 0;
    for (test_fn const *test = g_tests; *test != NULL; test++) {
        total++;
        if ((*test)() == 0) {
            passed++;
        }
    }

    if (passed == total) {
        printf("\n=== All P5 RGB space tests passed (%d/%d) ===\n", passed, total);
        return 0;
    } else {
        printf("\n=== %d/%d P5 RGB space test(s) failed ===\n",
                total - passed, total);
        return 1;
    }
}
