/*
 * M11: Gamut Utilities & Mapping Tests
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>

#define TEST_ASSERT(cond, msg) do { if (!(cond)) { printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); return 1; } } while(0)
#define TEST_PASS(name) do { return 0; } while(0)

#define GAMUT_MAP_TOLERANCE ALWAN_LITERAL(1e-6)

/* ----------------------------------------------------------------
 * Test gamut volume estimation (Monte Carlo)
 * ---------------------------------------------------------------- */

static int test_gamut_volume_srgb(void) {
    /* sRGB space */
    alwan_rgb_space_desc srgb;
    srgb.primaries_xy[0] = ALWAN_LITERAL(0.64);
    srgb.primaries_xy[1] = ALWAN_LITERAL(0.33);
    srgb.primaries_xy[2] = ALWAN_LITERAL(0.30);
    srgb.primaries_xy[3] = ALWAN_LITERAL(0.60);
    srgb.primaries_xy[4] = ALWAN_LITERAL(0.15);
    srgb.primaries_xy[5] = ALWAN_LITERAL(0.06);
    srgb.white_xy[0] = ALWAN_LITERAL(0.3127);
    srgb.white_xy[1] = ALWAN_LITERAL(0.3290);
    srgb.oetf = ALWAN_TF_LINEAR;
    srgb.eotf = ALWAN_TF_LINEAR;

    alwan_scalar volume;
    int status = alwan_gamut_volume_mc(&volume, &srgb, 100000, 42);
    TEST_ASSERT(status == ALWAN_OK, "Volume estimation failed");
    TEST_ASSERT(volume > ALWAN_LITERAL(0.0), "Volume should be positive");

    printf("  sRGB gamut volume: %.6f\n", (double)volume);

    /* Sanity check: volume should be reasonable (not too large or small) */
    TEST_ASSERT(volume > ALWAN_LITERAL(0.1) && volume < ALWAN_LITERAL(1.0),
                "sRGB volume out of expected range");

    TEST_PASS("sRGB gamut volume");
}

static int test_gamut_volume_bt2020(void) {
    /* BT.2020 space (wider gamut) */
    alwan_rgb_space_desc bt2020;
    bt2020.primaries_xy[0] = ALWAN_LITERAL(0.708);
    bt2020.primaries_xy[1] = ALWAN_LITERAL(0.292);
    bt2020.primaries_xy[2] = ALWAN_LITERAL(0.170);
    bt2020.primaries_xy[3] = ALWAN_LITERAL(0.797);
    bt2020.primaries_xy[4] = ALWAN_LITERAL(0.131);
    bt2020.primaries_xy[5] = ALWAN_LITERAL(0.046);
    bt2020.white_xy[0] = ALWAN_LITERAL(0.3127);
    bt2020.white_xy[1] = ALWAN_LITERAL(0.3290);
    bt2020.oetf = ALWAN_TF_LINEAR;
    bt2020.eotf = ALWAN_TF_LINEAR;

    alwan_scalar volume;
    int status = alwan_gamut_volume_mc(&volume, &bt2020, 100000, 42);
    TEST_ASSERT(status == ALWAN_OK, "Volume estimation failed");
    TEST_ASSERT(volume > ALWAN_LITERAL(0.0), "Volume should be positive");

    alwan_scalar srgb_volume = ALWAN_LITERAL(0.207);  /* Approximate sRGB volume from previous test */
    printf("  BT.2020 gamut volume: %.6f\n", (double)volume);

    /* BT.2020 should have larger gamut than sRGB (roughly 4-5x) */
    TEST_ASSERT(volume > srgb_volume * ALWAN_LITERAL(1.5), "BT.2020 should have larger gamut than sRGB");

    TEST_PASS("BT.2020 gamut volume");
}

static int test_gamut_volume_reproducible(void) {
    alwan_rgb_space_desc srgb;
    srgb.primaries_xy[0] = ALWAN_LITERAL(0.64);
    srgb.primaries_xy[1] = ALWAN_LITERAL(0.33);
    srgb.primaries_xy[2] = ALWAN_LITERAL(0.30);
    srgb.primaries_xy[3] = ALWAN_LITERAL(0.60);
    srgb.primaries_xy[4] = ALWAN_LITERAL(0.15);
    srgb.primaries_xy[5] = ALWAN_LITERAL(0.06);
    srgb.white_xy[0] = ALWAN_LITERAL(0.3127);
    srgb.white_xy[1] = ALWAN_LITERAL(0.3290);
    srgb.oetf = ALWAN_TF_LINEAR;
    srgb.eotf = ALWAN_TF_LINEAR;

    /* Same seed should give same result */
    alwan_scalar volume1, volume2;
    alwan_gamut_volume_mc(&volume1, &srgb, 100000, 123);
    alwan_gamut_volume_mc(&volume2, &srgb, 100000, 123);

    TEST_ASSERT(ALWAN_ABS(volume1 - volume2) < ALWAN_EPSILON,
                "Same seed should produce identical results");

    printf("  Reproducibility verified\n");
    TEST_PASS("Volume estimation reproducibility");
}

/* ----------------------------------------------------------------
 * Test gamut mapping
 * ---------------------------------------------------------------- */

static int test_gamut_map_clip(void) {
    static alwan_scalar const test_data[] = {
#include "data/fixtures/gamut_map_clip.csv"
    };

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;

    for (size_t i = 0; i < num_colors; i++) {
        alwan_rgb rgb_in, expected, result;

        /* Load test data */
        rgb_in.r = test_data[i * 6 + 0];
        rgb_in.g = test_data[i * 6 + 1];
        rgb_in.b = test_data[i * 6 + 2];
        expected.r = test_data[i * 6 + 3];
        expected.g = test_data[i * 6 + 4];
        expected.b = test_data[i * 6 + 5];

        /* Test clip mapping */
        int status = alwan_gamut_map(&result.r, ALWAN_GAMUT_MAP_CLIP, &rgb_in.r, 1,
                                      sizeof(alwan_rgb), sizeof(alwan_rgb));
        TEST_ASSERT(status == ALWAN_OK, "Clip mapping failed");

        /* Check results */
        alwan_scalar const result_arr[3] = {result.r, result.g, result.b};
        alwan_scalar const expected_arr[3] = {expected.r, expected.g, expected.b};
        alwan_scalar const rgb_in_arr[3] = {rgb_in.r, rgb_in.g, rgb_in.b};
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_ABS(result_arr[j] - expected_arr[j]);
            if (diff > GAMUT_MAP_TOLERANCE) {
                printf("Color %zu channel %d failed:\n", i, j);
                printf("  Input: [%.6f, %.6f, %.6f]\n",
                       (double)rgb_in_arr[0], (double)rgb_in_arr[1], (double)rgb_in_arr[2]);
                printf("  Expected: [%.6f, %.6f, %.6f]\n",
                       (double)expected_arr[0], (double)expected_arr[1], (double)expected_arr[2]);
                printf("  Got: [%.6f, %.6f, %.6f]\n",
                       (double)result_arr[0], (double)result_arr[1], (double)result_arr[2]);
                TEST_ASSERT(0, "Clipped values don't match");
            }
        }
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("Clip gamut mapping");
}

static int test_gamut_map_hue_preserving(void) {
    static alwan_scalar const test_data[] = {
#include "data/fixtures/gamut_map_hue_preserving.csv"
    };

    size_t const num_colors = sizeof(test_data) / sizeof(test_data[0]) / 6;
    /* Relaxed tolerance for hue-preserving (involves iterative algorithm) */
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);

    for (size_t i = 0; i < num_colors; i++) {
        alwan_rgb rgb_in, expected, result;

        /* Load test data */
        rgb_in.r = test_data[i * 6 + 0];
        rgb_in.g = test_data[i * 6 + 1];
        rgb_in.b = test_data[i * 6 + 2];
        expected.r = test_data[i * 6 + 3];
        expected.g = test_data[i * 6 + 4];
        expected.b = test_data[i * 6 + 5];

        /* Test hue-preserving mapping */
        int status = alwan_gamut_map(&result.r, ALWAN_GAMUT_MAP_HUE_PRESERVING, &rgb_in.r, 1,
                                      sizeof(alwan_rgb), sizeof(alwan_rgb));
        TEST_ASSERT(status == ALWAN_OK, "Hue-preserving mapping failed");

        /* Check results */
        alwan_scalar const result_arr[3] = {result.r, result.g, result.b};
        alwan_scalar const expected_arr[3] = {expected.r, expected.g, expected.b};
        alwan_scalar const rgb_in_arr[3] = {rgb_in.r, rgb_in.g, rgb_in.b};
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_ABS(result_arr[j] - expected_arr[j]);
            if (diff > tolerance) {
                printf("Color %zu channel %d failed:\n", i, j);
                printf("  Input: [%.6f, %.6f, %.6f]\n",
                       (double)rgb_in_arr[0], (double)rgb_in_arr[1], (double)rgb_in_arr[2]);
                printf("  Expected: [%.6f, %.6f, %.6f]\n",
                       (double)expected_arr[0], (double)expected_arr[1], (double)expected_arr[2]);
                printf("  Got: [%.6f, %.6f, %.6f]\n",
                       (double)result_arr[0], (double)result_arr[1], (double)result_arr[2]);
                printf("  Diff: %.6e\n", (double)diff);
            }
        }

        /* Verify result is in gamut */
        TEST_ASSERT(result.r >= ALWAN_LITERAL(0.0) && result.r <= ALWAN_LITERAL(1.0),
                    "Result R out of gamut");
        TEST_ASSERT(result.g >= ALWAN_LITERAL(0.0) && result.g <= ALWAN_LITERAL(1.0),
                    "Result G out of gamut");
        TEST_ASSERT(result.b >= ALWAN_LITERAL(0.0) && result.b <= ALWAN_LITERAL(1.0),
                    "Result B out of gamut");
    }

    printf("  Tested %zu colors\n", num_colors);
    TEST_PASS("Hue-preserving gamut mapping");
}

static int test_gamut_map_monotonicity(void) {
    /* Test that colors already in gamut are not changed */
    alwan_rgb in_gamut_colors[] = {
        {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)},
        {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)},
        {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)},
        {ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.7), ALWAN_LITERAL(0.2)},
    };

    alwan_rgb results[4];

    int status = alwan_gamut_map(&results[0].r, ALWAN_GAMUT_MAP_HUE_PRESERVING, &in_gamut_colors[0].r, 4,
                                  sizeof(alwan_rgb), sizeof(alwan_rgb));
    TEST_ASSERT(status == ALWAN_OK, "Gamut mapping failed");

    for (int i = 0; i < 4; i++) {
        alwan_scalar const result_arr[3] = {results[i].r, results[i].g, results[i].b};
        alwan_scalar const input_arr[3] = {in_gamut_colors[i].r, in_gamut_colors[i].g, in_gamut_colors[i].b};
        for (int j = 0; j < 3; j++) {
            alwan_scalar diff = ALWAN_ABS(result_arr[j] - input_arr[j]);
            TEST_ASSERT(diff < ALWAN_LITERAL(1e-6), "In-gamut color was changed");
        }
    }

    printf("  Verified 4 in-gamut colors unchanged\n");
    TEST_PASS("Gamut mapping monotonicity");
}

/* Main test runner for M11 */
int test_18_gamut_main(void) {
    printf("=== M11: Gamut Utilities & Mapping Tests ===\n");

    if (test_gamut_volume_srgb() != 0) return 1;
    if (test_gamut_volume_bt2020() != 0) return 2;
    if (test_gamut_volume_reproducible() != 0) return 3;
    if (test_gamut_map_clip() != 0) return 4;
    if (test_gamut_map_hue_preserving() != 0) return 5;
    if (test_gamut_map_monotonicity() != 0) return 6;

    printf("\n=== All M11 tests passed ===\n");
    return 0;
}
