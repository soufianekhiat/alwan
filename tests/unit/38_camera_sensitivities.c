/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 38: P8.4 Camera Sensitivities
 *
 * Tests camera RGB sensitivity loading and XYZ computation
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Test helpers
 * ---------------------------------------------------------------- */

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } \
} while(0)

#define TEST_PASS(name) do { \
    printf("[PASS] %s\n", name); \
    return 0; \
} while(0)

/* ----------------------------------------------------------------
 * Test: Camera sensitivity loading
 * ---------------------------------------------------------------- */

static int test_camera_sensitivity_loading(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    /* Test Nikon 5100 */
    alwan_spd r_sens, g_sens, b_sens;
    int status = alwan_spd_create(ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471, &r_sens);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create R SPD");
    status = alwan_spd_create(ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471, &g_sens);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create G SPD");
    status = alwan_spd_create(ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471, &b_sens);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create B SPD");

    status = alwan_spd_camera_sensitivity(ctx, ALWAN_CAMERA_NIKON_5100, &r_sens, &g_sens, &b_sens);
    TEST_ASSERT(status == ALWAN_OK, "Failed to load Nikon 5100 sensitivities");

    /* Verify sensitivities are normalized (max value should be ~1.0) */
    alwan_scalar r_max = 0, g_max = 0, b_max = 0;
    for (size_t i = 0; i < r_sens.count; i++) {
        if (r_sens.values[i] > r_max) r_max = r_sens.values[i];
        if (g_sens.values[i] > g_max) g_max = g_sens.values[i];
        if (b_sens.values[i] > b_max) b_max = b_sens.values[i];
    }

    printf("  Nikon 5100 peak sensitivities: R=%.4f G=%.4f B=%.4f\n", r_max, g_max, b_max);
    TEST_ASSERT(r_max >= ALWAN_LITERAL(0.5) && r_max <= ALWAN_LITERAL(1.5), "R sensitivity out of range");
    TEST_ASSERT(g_max >= ALWAN_LITERAL(0.5) && g_max <= ALWAN_LITERAL(1.5), "G sensitivity out of range");
    TEST_ASSERT(b_max >= ALWAN_LITERAL(0.5) && b_max <= ALWAN_LITERAL(1.5), "B sensitivity out of range");

    alwan_spd_destroy(ctx, &r_sens);
    alwan_spd_destroy(ctx, &g_sens);
    alwan_spd_destroy(ctx, &b_sens);

    /* Test Sigma SD Merill */
    status = alwan_spd_create(ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471, &r_sens);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create R SPD");
    status = alwan_spd_create(ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471, &g_sens);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create G SPD");
    status = alwan_spd_create(ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471, &b_sens);
    TEST_ASSERT(status == ALWAN_OK, "Failed to create B SPD");

    status = alwan_spd_camera_sensitivity(ctx, ALWAN_CAMERA_SIGMA_SDMERILL, &r_sens, &g_sens, &b_sens);
    TEST_ASSERT(status == ALWAN_OK, "Failed to load Sigma SD Merill sensitivities");

    alwan_spd_destroy(ctx, &r_sens);
    alwan_spd_destroy(ctx, &g_sens);
    alwan_spd_destroy(ctx, &b_sens);

    alwan_destroy(ctx);
    TEST_PASS("Camera sensitivity loading");
}

/* ----------------------------------------------------------------
 * Test: XYZ from SPD using camera sensitivities
 * ---------------------------------------------------------------- */

static int test_xyz_from_spd_camera(void) {
    alwan_ctx *ctx = alwan_create(NULL);
    TEST_ASSERT(ctx != NULL, "Context creation failed");

    /* Load D65 illuminant */
    alwan_spd d65;
    int status = alwan_spd_illuminant(ctx, ALWAN_ILLUMINANT_D65, &d65);
    TEST_ASSERT(status == ALWAN_OK, "Failed to load D65 illuminant");

    /* Compute camera RGB for D65 */
    alwan_vec3 rgb_nikon, rgb_sigma;

    status = alwan_xyz_from_spd_camera(ctx, &d65, NULL, ALWAN_CAMERA_NIKON_5100,
                                        ALWAN_INTEGRATE_TRAPEZOID, &rgb_nikon);
    TEST_ASSERT(status == ALWAN_OK, "Failed to compute camera RGB for Nikon 5100");

    status = alwan_xyz_from_spd_camera(ctx, &d65, NULL, ALWAN_CAMERA_SIGMA_SDMERILL,
                                        ALWAN_INTEGRATE_TRAPEZOID, &rgb_sigma);
    TEST_ASSERT(status == ALWAN_OK, "Failed to compute camera RGB for Sigma SD Merill");

    printf("  D65 camera RGB (Nikon 5100): [%.4f, %.4f, %.4f]\n",
           rgb_nikon.v[0], rgb_nikon.v[1], rgb_nikon.v[2]);
    printf("  D65 camera RGB (Sigma SDM):   [%.4f, %.4f, %.4f]\n",
           rgb_sigma.v[0], rgb_sigma.v[1], rgb_sigma.v[2]);

    /* Verify RGB values are reasonable (all positive, non-zero) */
    TEST_ASSERT(rgb_nikon.v[0] > ALWAN_LITERAL(0.0), "Nikon R <= 0");
    TEST_ASSERT(rgb_nikon.v[1] > ALWAN_LITERAL(0.0), "Nikon G <= 0");
    TEST_ASSERT(rgb_nikon.v[2] > ALWAN_LITERAL(0.0), "Nikon B <= 0");

    TEST_ASSERT(rgb_sigma.v[0] > ALWAN_LITERAL(0.0), "Sigma R <= 0");
    TEST_ASSERT(rgb_sigma.v[1] > ALWAN_LITERAL(0.0), "Sigma G <= 0");
    TEST_ASSERT(rgb_sigma.v[2] > ALWAN_LITERAL(0.0), "Sigma B <= 0");

    alwan_spd_destroy(ctx, &d65);
    alwan_destroy(ctx);
    TEST_PASS("XYZ from SPD using camera sensitivities");
}

/* ----------------------------------------------------------------
 * Main test entry point
 * ---------------------------------------------------------------- */

int test_38_camera_sensitivities_main(void) {
    printf("\n========================================\n");
    printf("Test 38: Camera Sensitivities (P8.4)\n");
    printf("========================================\n\n");

    int result = 0;

    result = test_camera_sensitivity_loading();
    if (result != 0) return result;

    result = test_xyz_from_spd_camera();
    if (result != 0) return result;

    printf("\nAll camera sensitivity tests passed!\n");
    return 0;
}
