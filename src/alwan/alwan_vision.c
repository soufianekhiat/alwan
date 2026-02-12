/* ================================================================
 * Alwan - Color Vision & Perception
 * Thin wrapper — logic in alwan_vision_core.h
 *
 * Only enum resolution and LUT-based spectral lookups live here;
 * all per-pixel math delegated to core.
 * ================================================================ */

#include "alwan.h"
#include "alwan_internal.h"
#include "alwan_vision_core.h"

/* ================================================================
 * Color Vision Deficiency (CVD) Simulation
 * ================================================================ */

int alwan_simulate_cvd(alwan_rgb *rgb_out,
                        alwan_rgb const *rgb_in,
                        alwan_cvd_type cvd_type,
                        alwan_scalar severity) {
    if (!rgb_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    alwan_rgb result;

    switch (cvd_type) {
        case ALWAN_CVD_PROTANOPIA:
        case ALWAN_CVD_PROTANOMALY:
            result = alwan_simulate_protanopia_v(*rgb_in, severity);
            break;

        case ALWAN_CVD_DEUTERANOPIA:
        case ALWAN_CVD_DEUTERANOMALY:
            result = alwan_simulate_deuteranopia_v(*rgb_in, severity);
            break;

        case ALWAN_CVD_TRITANOPIA:
        case ALWAN_CVD_TRITANOMALY:
            result = alwan_simulate_tritanopia_v(*rgb_in, severity);
            break;

        default:
            return ALWAN_E_INVALID;
    }

    *rgb_out = result;
    return ALWAN_OK;
}

/* ================================================================
 * Luminous Efficiency Functions
 * LUT-based spectral lookup — uses linear search loop, stays in .c
 * ================================================================ */

/* CIE 1924 Photopic V(lambda) - interleaved {wavelength, value} pairs
 * 42 samples, 380-780 nm (10 nm step + peak at 555 nm)
 * Generated from colour-science SDS_LEFS['CIE 1924 Photopic Standard Observer'] */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const PHOTOPIC_V_DATA[] = {
#include "data/vision/photopic_v_lambda.csv"
};
ALWAN_DIAG_POP
#define PHOTOPIC_V_COUNT 42

/* CIE 1951 Scotopic V'(lambda) - interleaved {wavelength, value} pairs
 * 42 samples, 380-780 nm (10 nm step + peak at 507 nm)
 * Generated from colour-science SDS_LEFS['CIE 1951 Scotopic Standard Observer'] */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const SCOTOPIC_VP_DATA[] = {
#include "data/vision/scotopic_vp_lambda.csv"
};
ALWAN_DIAG_POP
#define SCOTOPIC_VP_COUNT 42

/* Interpolate interleaved {wavelength, value} pairs (stride 2) */
static alwan_scalar interpolate_lut(alwan_scalar const *data,
                                      int count,
                                      alwan_scalar wavelength) {
    if (wavelength <= data[0]) {
        return data[1];
    }
    if (wavelength >= data[(count - 1) * 2]) {
        return data[(count - 1) * 2 + 1];
    }

    int i;
    for (i = 0; i < count - 1; i++) {
        alwan_scalar wl_lo = data[i * 2];
        alwan_scalar wl_hi = data[(i + 1) * 2];
        if (wavelength >= wl_lo && wavelength <= wl_hi) {
            alwan_scalar t = (wavelength - wl_lo) / (wl_hi - wl_lo);
            return data[i * 2 + 1] + t * (data[(i + 1) * 2 + 1] - data[i * 2 + 1]);
        }
    }

    return ALWAN_LITERAL(0.0);
}

alwan_scalar alwan_luminous_efficiency(alwan_scalar wavelength, alwan_vision_type vision_type) {
    if (wavelength < ALWAN_LITERAL(360.0) || wavelength > ALWAN_LITERAL(830.0)) {
        return ALWAN_LITERAL(-1.0);
    }

    switch (vision_type) {
        case ALWAN_VISION_PHOTOPIC:
            return interpolate_lut(PHOTOPIC_V_DATA, PHOTOPIC_V_COUNT, wavelength);
        case ALWAN_VISION_SCOTOPIC:
            return interpolate_lut(SCOTOPIC_VP_DATA, SCOTOPIC_VP_COUNT, wavelength);
        case ALWAN_VISION_MESOPIC:
            return interpolate_lut(PHOTOPIC_V_DATA, PHOTOPIC_V_COUNT, wavelength);
        default:
            return ALWAN_LITERAL(-1.0);
    }
}

alwan_scalar alwan_photopic_luminance(alwan_ctx *ctx, alwan_spd const *spd) {
    (void)ctx; (void)spd;
    return ALWAN_LITERAL(-1.0);  /* Not yet implemented */
}

alwan_scalar alwan_scotopic_luminance(alwan_ctx *ctx, alwan_spd const *spd) {
    (void)ctx; (void)spd;
    return ALWAN_LITERAL(-1.0);  /* Not yet implemented */
}

alwan_scalar alwan_mesopic_luminance(alwan_ctx *ctx, alwan_spd const *spd,
                                      alwan_scalar adaptation_level) {
    (void)ctx; (void)spd; (void)adaptation_level;
    return ALWAN_LITERAL(-1.0);  /* Not yet implemented */
}

/* ================================================================
 * Contrast Sensitivity Function (CSF)
 * Thin wrappers — math in alwan_vision_core.h
 * ================================================================ */

alwan_scalar alwan_csf(alwan_scalar spatial_frequency, alwan_scalar luminance) {
    if (spatial_frequency < ALWAN_LITERAL(0.1) || spatial_frequency > ALWAN_LITERAL(60.0)) {
        return ALWAN_LITERAL(-1.0);
    }
    if (luminance < ALWAN_LITERAL(0.01) || luminance > ALWAN_LITERAL(10000.0)) {
        return ALWAN_LITERAL(-1.0);
    }

    return alwan_csf_simple_v(spatial_frequency, luminance);
}

alwan_scalar alwan_pupil_diameter_barten1999(alwan_scalar L,
                                              alwan_scalar X_0,
                                              alwan_scalar Y_0) {
    return alwan_pupil_diameter_barten1999_v(L, X_0, Y_0);
}

alwan_scalar alwan_retinal_illuminance_barten1999(alwan_scalar L,
                                                   alwan_scalar d,
                                                   int apply_stiles_crawford) {
    return alwan_retinal_illuminance_barten1999_v(L, d, (alwan_scalar)apply_stiles_crawford);
}

alwan_scalar alwan_optical_mtf_barten1999(alwan_scalar u, alwan_scalar sigma) {
    return alwan_optical_mtf_barten1999_v(u, sigma);
}

alwan_scalar alwan_sigma_barten1999(alwan_scalar sigma_0,
                                     alwan_scalar C_ab,
                                     alwan_scalar d) {
    return alwan_sigma_barten1999_v(sigma_0, C_ab, d);
}

alwan_scalar alwan_maximum_angular_size_barten1999(alwan_scalar u,
                                                    alwan_scalar X_0,
                                                    alwan_scalar X_max,
                                                    alwan_scalar N_max) {
    return alwan_maximum_angular_size_barten1999_v(u, X_0, X_max, N_max);
}

void alwan_csf_barten1999_params_default(alwan_csf_barten1999_params *params) {
    if (!params) return;

    params->sigma = alwan_sigma_barten1999_v(
        ALWAN_LITERAL(0.5) / ALWAN_LITERAL(60.0),
        ALWAN_LITERAL(0.08) / ALWAN_LITERAL(60.0),
        ALWAN_LITERAL(2.1));
    params->k = ALWAN_LITERAL(3.0);
    params->T = ALWAN_LITERAL(0.1);
    params->X_0 = ALWAN_LITERAL(60.0);
    params->Y_0 = ALWAN_LITERAL(-1.0);
    params->X_max = ALWAN_LITERAL(12.0);
    params->Y_max = ALWAN_LITERAL(-1.0);
    params->N_max = ALWAN_LITERAL(15.0);
    params->n = ALWAN_LITERAL(0.03);
    params->p = ALWAN_LITERAL(1.2274e6);
    params->E = alwan_retinal_illuminance_barten1999_v(
        ALWAN_LITERAL(20.0), ALWAN_LITERAL(2.1), ALWAN_ONE);
    params->phi_0 = ALWAN_LITERAL(3.0e-8);
    params->u_0 = ALWAN_LITERAL(7.0);
}

alwan_scalar alwan_csf_barten1999(alwan_scalar u,
                                   alwan_csf_barten1999_params const *params) {
    alwan_csf_barten1999_params defaults;
    alwan_csf_barten1999_params const *p;

    if (params) {
        p = params;
    } else {
        alwan_csf_barten1999_params_default(&defaults);
        p = &defaults;
    }

    /* Map public struct to core struct */
    alwan_csf_barten1999_v_params vp;
    vp.sigma = p->sigma;
    vp.k     = p->k;
    vp.T     = p->T;
    vp.X_0   = p->X_0;
    vp.Y_0   = p->Y_0;
    vp.X_max = p->X_max;
    vp.Y_max = p->Y_max;
    vp.N_max = p->N_max;
    vp.n     = p->n;
    vp.p     = p->p;
    vp.E     = p->E;
    vp.phi_0 = p->phi_0;
    vp.u_0   = p->u_0;

    return alwan_csf_barten1999_v(u, vp);
}
