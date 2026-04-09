/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 60: Validate embedded RGB<>XYZ matrices via conversions
 *
 * For each RGB space, compares RGB->XYZ and XYZ->RGB conversions using:
 *   1. Precomputed (embedded/generated) matrices from gendata
 *   2. Matrices derived at runtime via alwan_rgb_derive_matrices
 * This validates that the generated CSV matrices match the C derivation.
 */

#include "test_common.h"
#include <string.h>

/* ----------------------------------------------------------------
 * Space names for diagnostics (order MUST match alwan_rgb_space enum)
 * ---------------------------------------------------------------- */

static char const * const g_space_names[] = {
    "sRGB",
    "BT.709",
    "Display P3",
    "BT.2020",
    "ACES2065-1",
    "ACEScg",
    "ACESproxy",
    "ACEScc",
    "ACEScct",
    "ARRI Wide Gamut 3",
    "ARRI Wide Gamut 4",
    "ARRI LogC3",
    "ARRI LogC4",
    "REDcolor",
    "REDcolor2",
    "REDcolor3",
    "REDcolor4",
    "DRAGONcolor",
    "DRAGONcolor2",
    "REDLog",
    "Venice S-Gamut3",
    "Venice S-Gamut3.Cine",
    "S-Log",
    "S-Log2",
    "S-Log3",
    "CIE RGB",
    "Adobe Wide Gamut RGB",
    "ROMM RGB",
    "RIMM RGB",
    "ERIMM RGB",
    "FilmLight E-Gamut",
    "FilmLight T-Log",
    "F-Gamut",
    "Fujifilm F-Log",
    "N-Gamut",
    "N-Log",
    "DJI D-Gamut",
    "Protune Native",
    "ITU-R BT.470-525",
    "ITU-R BT.470-625",
    "SMPTE 240M",
    "SMPTE C",
    "DCDM XYZ",
    "Best RGB",
    "Beta RGB",
    "Don RGB 4",
    "Ekta Space PS 5",
    "Max RGB",
    "Russell RGB",
    "Sharp RGB",
    "ECI RGB v2",
    "Adobe RGB 1998",
    "ProPhoto RGB",
    "DaVinci Wide Gamut",
    "DaVinci Intermediate",
    "Blackmagic Wide Gamut",
    "Blackmagic Film",
    "Blackmagic Film Gen5",
    "V-Gamut",
    "V-Log",
    "S-Gamut",
    "S-Gamut3",
    "S-Gamut3.Cine",
    "Cinema Gamut",
    "Canon Log",
    "REDWideGamutRGB",
    "DCI-P3",
    "DCI-P3+",
    "P3-D65",
    "NTSC 1953",
    "NTSC 1987",
    "PAL/SECAM",
    "EBU Tech. 3213-E",
    "Apple RGB",
    "ColorMatch RGB",
    "ALEXA Wide Gamut",
    "P3-D60",
    "Xtreme RGB",
    "Linear Rec.709",
    "Linear Rec.2020",
    "Linear Adobe RGB 1998",
    "Linear P3-D65",
    "Linear Display P3",
    "Linear ProPhoto RGB",
    "Linear DCI-P3",
    "Linear Adobe Wide Gamut RGB",
    "Linear Apple RGB",
    "Linear ColorMatch RGB",
    "Linear P3-D60",
    "Linear BT.470-525",
    "Linear BT.470-625",
    "Linear SMPTE 240M",
    "ITU-T H.273 Unspecified",
    "ITU-T H.273 Generic Film",
    "PLASA ANSI E1.54",
    "Gamma 2.2 Rec.709",
    "Gamma 2.2 Adobe RGB",
    "Gamma 2.2 P3-D65",
    "Gamma 2.2 AP1",
    "Gamma 1.8 Rec.709",
    "Rec.1886 Rec.709",
    "Rec.2100 PQ",
    "Rec.2100 HLG",
    "Display P3 HDR",
};

#define NUM_SPACES (ALWAN_RGB_SPACE_DISPLAY_P3_HDR + 1)

_Static_assert(
    sizeof(g_space_names) / sizeof(g_space_names[0]) == NUM_SPACES,
    "g_space_names[] size must match number of RGB spaces"
);

/* ----------------------------------------------------------------
 * Test RGB values (linear, in [0,1] range)
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

static alwan_rgb_f64 const g_test_rgb[] = {
    {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},
    {ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)},
    {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)},
    {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},
    {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},
    {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)},
    {ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18)},
    {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.25)},
    {ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.6)},
};

ALWAN_DIAG_POP

#define NUM_TEST_COLORS (sizeof(g_test_rgb) / sizeof(g_test_rgb[0]))

/* ----------------------------------------------------------------
 * Test: embedded vs derived RGB->XYZ conversions
 *
 * For each space, converts the same RGB values using:
 *   - The embedded (precomputed) matrices
 *   - Matrices derived from primaries via alwan_rgb_derive_matrices
 * and compares the resulting XYZ values.
 * ---------------------------------------------------------------- */

static int test_rgb_to_xyz_embedded_vs_derived(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    int spaces_tested = 0;

    for (int s = 0; s < NUM_SPACES; s++) {
        /* Get descriptor with embedded matrices */
        alwan_rgb_space_desc embedded;
        int status = alwan_rgb_get_space_descriptor(&embedded, ctx, (alwan_rgb_space)s);
        if (status != ALWAN_OK) {
            printf("  [SKIP] %s: get_space_descriptor failed (%d)\n", g_space_names[s], status);
            continue;
        }

        /* Create descriptor without embedded matrices (derive from primaries) */
        alwan_rgb_space_desc derived;
        memcpy(derived.primaries_xy, embedded.primaries_xy, sizeof(derived.primaries_xy));
        memcpy(derived.white_xy, embedded.white_xy, sizeof(derived.white_xy));
        derived.oetf = ALWAN_TF_LINEAR;
        derived.eotf = ALWAN_TF_LINEAR;
        derived.has_matrices = 0;

        for (size_t c = 0; c < NUM_TEST_COLORS; c++) {
            alwan_xyz_f64 xyz_embed, xyz_derive;

            status = alwan_rgb_to_xyz_f64(&xyz_embed, &embedded, &g_test_rgb[c]);
            TEST_ASSERT(status == ALWAN_OK, "alwan_rgb_to_xyz (embedded) failed");

            status = alwan_rgb_to_xyz_f64(&xyz_derive, &derived, &g_test_rgb[c]);
            TEST_ASSERT(status == ALWAN_OK, "alwan_rgb_to_xyz (derived) failed");

            alwan_f64 dx = ALWAN_ABS(xyz_embed.x - xyz_derive.x);
            alwan_f64 dy = ALWAN_ABS(xyz_embed.y - xyz_derive.y);
            alwan_f64 dz = ALWAN_ABS(xyz_embed.z - xyz_derive.z);

            if (dx > ALWAN_TEST_TOLERANCE || dy > ALWAN_TEST_TOLERANCE || dz > ALWAN_TEST_TOLERANCE) {
                printf("[FAIL] %s RGB->XYZ color %zu:\n", g_space_names[s], c);
                printf("  Input RGB: [%.6f, %.6f, %.6f]\n",
                       g_test_rgb[c].r, g_test_rgb[c].g, g_test_rgb[c].b);
                printf("  Embedded XYZ: [%.10e, %.10e, %.10e]\n",
                       xyz_embed.x, xyz_embed.y, xyz_embed.z);
                printf("  Derived  XYZ: [%.10e, %.10e, %.10e]\n",
                       xyz_derive.x, xyz_derive.y, xyz_derive.z);
                printf("  Diff:         [%e, %e, %e]\n", dx, dy, dz);
                alwan_destroy(ctx);
                TEST_ASSERT(0, "Embedded vs derived RGB->XYZ mismatch");
            }
        }

        spaces_tested++;
    }

    printf("  RGB->XYZ embedded vs derived: %d spaces x %zu colors OK\n",
           spaces_tested, NUM_TEST_COLORS);

    alwan_destroy(ctx);
    TEST_PASS("test_rgb_to_xyz_embedded_vs_derived");
}

/* ----------------------------------------------------------------
 * Test: embedded vs derived XYZ->RGB conversions
 *
 * Converts test XYZ values (obtained from the embedded path)
 * back to RGB using both embedded and derived matrices.
 * ---------------------------------------------------------------- */

static int test_xyz_to_rgb_embedded_vs_derived(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    int spaces_tested = 0;

    for (int s = 0; s < NUM_SPACES; s++) {
        alwan_rgb_space_desc embedded;
        int status = alwan_rgb_get_space_descriptor(&embedded, ctx, (alwan_rgb_space)s);
        if (status != ALWAN_OK) continue;

        alwan_rgb_space_desc derived;
        memcpy(derived.primaries_xy, embedded.primaries_xy, sizeof(derived.primaries_xy));
        memcpy(derived.white_xy, embedded.white_xy, sizeof(derived.white_xy));
        derived.oetf = ALWAN_TF_LINEAR;
        derived.eotf = ALWAN_TF_LINEAR;
        derived.has_matrices = 0;

        for (size_t c = 0; c < NUM_TEST_COLORS; c++) {
            /* Get an XYZ value by converting a known RGB through embedded */
            alwan_xyz_f64 xyz;
            status = alwan_rgb_to_xyz_f64(&xyz, &embedded, &g_test_rgb[c]);
            if (status != ALWAN_OK) continue;

            alwan_rgb_f64 rgb_embed, rgb_derive;

            status = alwan_xyz_to_rgb_f64(&rgb_embed, &embedded, &xyz);
            TEST_ASSERT(status == ALWAN_OK, "alwan_xyz_to_rgb (embedded) failed");

            status = alwan_xyz_to_rgb_f64(&rgb_derive, &derived, &xyz);
            TEST_ASSERT(status == ALWAN_OK, "alwan_xyz_to_rgb (derived) failed");

            alwan_f64 dr = ALWAN_ABS(rgb_embed.r - rgb_derive.r);
            alwan_f64 dg = ALWAN_ABS(rgb_embed.g - rgb_derive.g);
            alwan_f64 db = ALWAN_ABS(rgb_embed.b - rgb_derive.b);

            if (dr > ALWAN_TEST_TOLERANCE || dg > ALWAN_TEST_TOLERANCE || db > ALWAN_TEST_TOLERANCE) {
                printf("[FAIL] %s XYZ->RGB color %zu:\n", g_space_names[s], c);
                printf("  Input XYZ: [%.10e, %.10e, %.10e]\n", xyz.x, xyz.y, xyz.z);
                printf("  Embedded RGB: [%.6f, %.6f, %.6f]\n",
                       rgb_embed.r, rgb_embed.g, rgb_embed.b);
                printf("  Derived  RGB: [%.6f, %.6f, %.6f]\n",
                       rgb_derive.r, rgb_derive.g, rgb_derive.b);
                printf("  Diff:         [%e, %e, %e]\n", dr, dg, db);
                alwan_destroy(ctx);
                TEST_ASSERT(0, "Embedded vs derived XYZ->RGB mismatch");
            }
        }

        spaces_tested++;
    }

    printf("  XYZ->RGB embedded vs derived: %d spaces x %zu colors OK\n",
           spaces_tested, NUM_TEST_COLORS);

    alwan_destroy(ctx);
    TEST_PASS("test_xyz_to_rgb_embedded_vs_derived");
}

/* ----------------------------------------------------------------
 * Test: RGB->XYZ->RGB round-trip with embedded matrices
 *
 * Validates that converting RGB->XYZ->RGB using embedded matrices
 * recovers the original RGB values within tolerance.
 * ---------------------------------------------------------------- */

static int test_rgb_xyz_roundtrip(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Failed to create context");

    int spaces_tested = 0;

    for (int s = 0; s < NUM_SPACES; s++) {
        alwan_rgb_space_desc desc;
        int status = alwan_rgb_get_space_descriptor(&desc, ctx, (alwan_rgb_space)s);
        if (status != ALWAN_OK) continue;

        for (size_t c = 0; c < NUM_TEST_COLORS; c++) {
            alwan_xyz_f64 xyz;
            alwan_rgb_f64 rgb_back;

            status = alwan_rgb_to_xyz_f64(&xyz, &desc, &g_test_rgb[c]);
            TEST_ASSERT(status == ALWAN_OK, "alwan_rgb_to_xyz failed");

            status = alwan_xyz_to_rgb_f64(&rgb_back, &desc, &xyz);
            TEST_ASSERT(status == ALWAN_OK, "alwan_xyz_to_rgb failed");

            alwan_f64 dr = ALWAN_ABS(rgb_back.r - g_test_rgb[c].r);
            alwan_f64 dg = ALWAN_ABS(rgb_back.g - g_test_rgb[c].g);
            alwan_f64 db = ALWAN_ABS(rgb_back.b - g_test_rgb[c].b);

            if (dr > ALWAN_TEST_TOLERANCE || dg > ALWAN_TEST_TOLERANCE || db > ALWAN_TEST_TOLERANCE) {
                printf("[FAIL] %s round-trip color %zu:\n", g_space_names[s], c);
                printf("  Original: [%.6f, %.6f, %.6f]\n",
                       g_test_rgb[c].r, g_test_rgb[c].g, g_test_rgb[c].b);
                printf("  XYZ:      [%.10e, %.10e, %.10e]\n", xyz.x, xyz.y, xyz.z);
                printf("  Back:     [%.6f, %.6f, %.6f]\n",
                       rgb_back.r, rgb_back.g, rgb_back.b);
                printf("  Diff:     [%e, %e, %e]\n", dr, dg, db);
                alwan_destroy(ctx);
                TEST_ASSERT(0, "RGB->XYZ->RGB round-trip failed");
            }
        }

        spaces_tested++;
    }

    printf("  RGB->XYZ->RGB round-trip: %d spaces x %zu colors OK\n",
           spaces_tested, NUM_TEST_COLORS);

    alwan_destroy(ctx);
    TEST_PASS("test_rgb_xyz_roundtrip");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_60_rgb_xyz_embedded_main(void) {
    printf("=== Embedded RGB<>XYZ Matrix Validation Tests ===\n");

    int failures = 0;

    failures += test_rgb_to_xyz_embedded_vs_derived();
    failures += test_xyz_to_rgb_embedded_vs_derived();
    failures += test_rgb_xyz_roundtrip();

    if (failures == 0) {
        printf("\n=== All embedded RGB<>XYZ tests passed ===\n");
    }

    return failures;
}
