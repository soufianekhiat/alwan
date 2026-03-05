/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Hero Wavelength Spectral Sampling
 * Thin wrapper -- per-sample math in alwan_hero_wavelength_core.h
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_hero_wavelength_core.h"

/* ----------------------------------------------------------------
 * Single Sample
 * ---------------------------------------------------------------- */

int alwan_hero_wavelength_sample(alwan_scalar *lambda_out, alwan_scalar u) {
    if (!lambda_out) return ALWAN_E_INVALID;
    *lambda_out = alwan_hero_wavelength_sample_v(u);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Single wavelength -> XYZ
 * ---------------------------------------------------------------- */

int alwan_hero_wavelength_to_xyz(alwan_xyz *xyz_out, alwan_scalar lambda) {
    if (!xyz_out) return ALWAN_E_INVALID;
    *xyz_out = alwan_hero_wavelength_to_xyz_v(lambda);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Batch sampling with stratification
 * ---------------------------------------------------------------- */

int alwan_hero_wavelength_batch(alwan_scalar *lambda_out,
                                 alwan_xyz *xyz_weights,
                                 size_t count,
                                 alwan_scalar seed) {
    if (!lambda_out || count == 0) return ALWAN_E_INVALID;

    /* Generate hero wavelength from seed */
    alwan_scalar hero = alwan_hero_wavelength_sample_v(seed);

    for (size_t i = 0; i < count; i++) {
        alwan_scalar lambda = alwan_hero_wavelength_stratified_v(hero, (int)i, (int)count);
        lambda_out[i] = lambda;

        if (xyz_weights) {
            alwan_scalar pdf = alwan_hero_wavelength_pdf_v(lambda);
            alwan_xyz cmf = alwan_hero_wavelength_to_xyz_v(lambda);
            /* Weight = CMF / pdf for importance sampling */
            alwan_scalar inv_pdf = ALWAN_LITERAL(1.0) / pdf;
            xyz_weights[i].x = cmf.x * inv_pdf;
            xyz_weights[i].y = cmf.y * inv_pdf;
            xyz_weights[i].z = cmf.z * inv_pdf;
        }
    }

    return ALWAN_OK;
}
