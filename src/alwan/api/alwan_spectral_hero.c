/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Hero Wavelength Spectral Sampling
 * Per-sample math in alwan_hero_wavelength_core.h
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_hero_wavelength_core.h"

/* ----------------------------------------------------------------
 * Single Sample
 * ---------------------------------------------------------------- */

int alwan_hero_wavelength_sample_f64(alwan_f64 *lambda_out, alwan_f64 u) {
    if (!lambda_out) return ALWAN_E_INVALID;
    *lambda_out = alwan_hero_wavelength_sample_f64_v(u);
    return ALWAN_OK;
}

int alwan_hero_wavelength_sample_f32(alwan_f32 *lambda_out, alwan_f32 u) {
    if (!lambda_out) return ALWAN_E_INVALID;
    *lambda_out = alwan_hero_wavelength_sample_f32_v(u);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Single wavelength -> XYZ
 * ---------------------------------------------------------------- */

void alwan_hero_wavelength_to_xyz_f32(alwan_xyz_f32 *xyz_out, alwan_f32 lambda) {
    if (!xyz_out) return;
    alwan_xyz_f32 result = alwan_hero_wavelength_to_xyz_f32_v(lambda);
    *xyz_out = result;
}

void alwan_hero_wavelength_to_xyz_f64(alwan_xyz_f64 *xyz_out, alwan_f64 lambda) {
    if (!xyz_out) return;
    *xyz_out = alwan_hero_wavelength_to_xyz_f64_v(lambda);
}

/* ----------------------------------------------------------------
 * Batch sampling with stratification
 * ---------------------------------------------------------------- */

int alwan_hero_wavelength_batch_f64(alwan_f64 *lambda_out,
                                 alwan_xyz_f64 *xyz_weights,
                                 size_t count,
                                 alwan_f64 seed) {
    if (!lambda_out || count == 0) return ALWAN_E_INVALID;

    /* Generate hero wavelength from seed */
    alwan_f64 hero = alwan_hero_wavelength_sample_f64_v(seed);

    for (size_t i = 0; i < count; i++) {
        alwan_f64 lambda = alwan_hero_wavelength_stratified_f64_v(hero, (int)i, (int)count);
        lambda_out[i] = lambda;

        if (xyz_weights) {
            alwan_f64 pdf = alwan_hero_wavelength_pdf_f64_v(lambda);
            alwan_xyz_f64 cmf = alwan_hero_wavelength_to_xyz_f64_v(lambda);
            /* Weight = CMF / pdf for importance sampling */
            alwan_f64 inv_pdf = ALWAN_LITERAL(1.0) / pdf;
            xyz_weights[i].x = cmf.x * inv_pdf;
            xyz_weights[i].y = cmf.y * inv_pdf;
            xyz_weights[i].z = cmf.z * inv_pdf;
        }
    }

    return ALWAN_OK;
}

int alwan_hero_wavelength_batch_f32(alwan_f32 *lambda_out,
                                 alwan_xyz_f32 *xyz_weights,
                                 size_t count,
                                 alwan_f32 seed) {
    if (!lambda_out || count == 0) return ALWAN_E_INVALID;

    alwan_f32 hero = alwan_hero_wavelength_sample_f32_v(seed);

    for (size_t i = 0; i < count; i++) {
        alwan_f32 lambda = alwan_hero_wavelength_stratified_f32_v(hero, (int)i, (int)count);
        lambda_out[i] = lambda;

        if (xyz_weights) {
            alwan_f32 pdf = alwan_hero_wavelength_pdf_f32_v(lambda);
            alwan_xyz_f32 cmf = alwan_hero_wavelength_to_xyz_f32_v(lambda);
            alwan_f32 inv_pdf = 1.0f / pdf;
            xyz_weights[i].x = cmf.x * inv_pdf;
            xyz_weights[i].y = cmf.y * inv_pdf;
            xyz_weights[i].z = cmf.z * inv_pdf;
        }
    }

    return ALWAN_OK;
}
