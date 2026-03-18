/* ================================================================
 * Alwan - Color Vision & Perception
 *
 * Enum resolution and LUT-based spectral lookups live here;
 * per-pixel math in alwan_vision_core.h.
 * ================================================================ */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_vision_core.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_vision_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_vision_impl.inc"
#include "alwan_api_teardown.h"

/* ================================================================
 * Color Vision Deficiency (CVD) Simulation
 * ================================================================ */

int alwan_simulate_cvd(alwan_rgb *rgb_out,
                        alwan_rgb const *rgb_in,
                        alwan_cvd_type cvd_type,
                        alwan_f64 severity) {
    if (!rgb_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    alwan_rgb result;

    switch (cvd_type) {
        case ALWAN_CVD_PROTANOPIA:
        case ALWAN_CVD_PROTANOMALY:
            result = alwan_simulate_protanopia_f64_v(*rgb_in, severity);
            break;

        case ALWAN_CVD_DEUTERANOPIA:
        case ALWAN_CVD_DEUTERANOMALY:
            result = alwan_simulate_deuteranopia_f64_v(*rgb_in, severity);
            break;

        case ALWAN_CVD_TRITANOPIA:
        case ALWAN_CVD_TRITANOMALY:
            result = alwan_simulate_tritanopia_f64_v(*rgb_in, severity);
            break;

        default:
            return ALWAN_E_INVALID;
    }

    *rgb_out = result;
    return ALWAN_OK;
}

/* ================================================================
 * Machado 2009 CVD Simulation
 * ================================================================ */

int alwan_simulate_cvd_machado(alwan_rgb *rgb_out,
                                alwan_rgb const *rgb_in,
                                alwan_cvd_type cvd_type,
                                alwan_f64 severity) {
    if (!rgb_in || !rgb_out) return ALWAN_E_INVALID;

    alwan_rgb result;

    switch (cvd_type) {
        case ALWAN_CVD_PROTANOPIA:
        case ALWAN_CVD_PROTANOMALY:
            result = alwan_simulate_machado_protan_f64_v(*rgb_in, severity);
            break;
        case ALWAN_CVD_DEUTERANOPIA:
        case ALWAN_CVD_DEUTERANOMALY:
            result = alwan_simulate_machado_deutan_f64_v(*rgb_in, severity);
            break;
        case ALWAN_CVD_TRITANOPIA:
        case ALWAN_CVD_TRITANOMALY:
            result = alwan_simulate_machado_tritan_f64_v(*rgb_in, severity);
            break;
        default:
            return ALWAN_E_INVALID;
    }

    *rgb_out = result;
    return ALWAN_OK;
}

int alwan_simulate_cvd_ex(alwan_rgb *rgb_out,
                           alwan_rgb const *rgb_in,
                           alwan_cvd_type cvd_type,
                           alwan_f64 severity,
                           alwan_cvd_model model) {
    switch (model) {
        case ALWAN_CVD_MODEL_BRETTEL:
            return alwan_simulate_cvd(rgb_out, rgb_in, cvd_type, severity);
        case ALWAN_CVD_MODEL_MACHADO:
            return alwan_simulate_cvd_machado(rgb_out, rgb_in, cvd_type, severity);
        default:
            return ALWAN_E_INVALID;
    }
}

/* ================================================================
 * Luminous Efficiency Functions
 * Luminous Efficiency Functions (LUT-based)
 * ================================================================ */

/* CIE 1924 Photopic V(lambda) - interleaved {wavelength, value} pairs
 * 42 samples, 380-780 nm (10 nm step + peak at 555 nm)
 * Generated from colour-science SDS_LEFS['CIE 1924 Photopic Standard Observer'] */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const PHOTOPIC_V_DATA[] = {
#include "../data/vision/photopic_v_lambda.csv"
};
ALWAN_DIAG_POP
#define PHOTOPIC_V_COUNT 42

/* CIE 1951 Scotopic V'(lambda) - interleaved {wavelength, value} pairs
 * 42 samples, 380-780 nm (10 nm step + peak at 507 nm)
 * Generated from colour-science SDS_LEFS['CIE 1951 Scotopic Standard Observer'] */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const SCOTOPIC_VP_DATA[] = {
#include "../data/vision/scotopic_vp_lambda.csv"
};
ALWAN_DIAG_POP
#define SCOTOPIC_VP_COUNT 42

/* Interpolate interleaved {wavelength, value} pairs (stride 2) */
static alwan_f64 interpolate_lut(alwan_f64 const *data,
                                      int count,
                                      alwan_f64 wavelength) {
    if (wavelength <= data[0]) {
        return data[1];
    }
    if (wavelength >= data[(count - 1) * 2]) {
        return data[(count - 1) * 2 + 1];
    }

    int i;
    for (i = 0; i < count - 1; i++) {
        alwan_f64 wl_lo = data[i * 2];
        alwan_f64 wl_hi = data[(i + 1) * 2];
        if (wavelength >= wl_lo && wavelength <= wl_hi) {
            alwan_f64 t = (wavelength - wl_lo) / (wl_hi - wl_lo);
            return data[i * 2 + 1] + t * (data[(i + 1) * 2 + 1] - data[i * 2 + 1]);
        }
    }

    return ALWAN_LITERAL(0.0);
}

alwan_f64 alwan_luminous_efficiency(alwan_f64 wavelength, alwan_vision_type vision_type) {
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

alwan_f64 alwan_photopic_luminance(alwan_ctx *ctx, alwan_spd const *spd) {
    (void)ctx; (void)spd;
    return ALWAN_LITERAL(-1.0);  /* Not yet implemented */
}

alwan_f64 alwan_scotopic_luminance(alwan_ctx *ctx, alwan_spd const *spd) {
    (void)ctx; (void)spd;
    return ALWAN_LITERAL(-1.0);  /* Not yet implemented */
}

alwan_f64 alwan_mesopic_luminance(alwan_ctx *ctx, alwan_spd const *spd,
                                      alwan_f64 adaptation_level) {
    (void)ctx; (void)spd; (void)adaptation_level;
    return ALWAN_LITERAL(-1.0);  /* Not yet implemented */
}

/* ================================================================
 * Contrast Sensitivity Function (CSF)
 * ================================================================ */

alwan_f64 alwan_csf(alwan_f64 spatial_frequency, alwan_f64 luminance) {
    if (spatial_frequency < ALWAN_LITERAL(0.1) || spatial_frequency > ALWAN_LITERAL(60.0)) {
        return ALWAN_LITERAL(-1.0);
    }
    if (luminance < ALWAN_LITERAL(0.01) || luminance > ALWAN_LITERAL(10000.0)) {
        return ALWAN_LITERAL(-1.0);
    }

    return alwan_csf_simple_f64_v(spatial_frequency, luminance);
}

alwan_f64 alwan_pupil_diameter_barten1999(alwan_f64 L,
                                              alwan_f64 X_0,
                                              alwan_f64 Y_0) {
    return alwan_pupil_diameter_barten1999_f64_v(L, X_0, Y_0);
}

alwan_f64 alwan_retinal_illuminance_barten1999(alwan_f64 L,
                                                   alwan_f64 d,
                                                   int apply_stiles_crawford) {
    return alwan_retinal_illuminance_barten1999_f64_v(L, d, (alwan_f64)apply_stiles_crawford);
}

alwan_f64 alwan_optical_mtf_barten1999(alwan_f64 u, alwan_f64 sigma) {
    return alwan_optical_mtf_barten1999_f64_v(u, sigma);
}

alwan_f64 alwan_sigma_barten1999(alwan_f64 sigma_0,
                                     alwan_f64 C_ab,
                                     alwan_f64 d) {
    return alwan_sigma_barten1999_f64_v(sigma_0, C_ab, d);
}

alwan_f64 alwan_maximum_angular_size_barten1999(alwan_f64 u,
                                                    alwan_f64 X_0,
                                                    alwan_f64 X_max,
                                                    alwan_f64 N_max) {
    return alwan_maximum_angular_size_barten1999_f64_v(u, X_0, X_max, N_max);
}

void alwan_csf_barten1999_params_default(alwan_csf_barten1999_params *params) {
    if (!params) return;

    params->sigma = alwan_sigma_barten1999_f64_v(
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
    params->E = alwan_retinal_illuminance_barten1999_f64_v(
        ALWAN_LITERAL(20.0), ALWAN_LITERAL(2.1), ALWAN_ONE);
    params->phi_0 = ALWAN_LITERAL(3.0e-8);
    params->u_0 = ALWAN_LITERAL(7.0);
}

alwan_f64 alwan_csf_barten1999(alwan_f64 u,
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
    alwan_csf_barten1999_v_params_f64 vp;
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

    return alwan_csf_barten1999_f64_v(u, vp);
}

/* ================================================================
 * Accessibility Contrast Metrics
 * ================================================================ */

int alwan_wcag_contrast_ratio(alwan_f64 *result, alwan_f64 Y1, alwan_f64 Y2) {
    if (!result) return ALWAN_E_INVALID;
    *result = alwan_wcag_contrast_ratio_f64_v(Y1, Y2);
    return ALWAN_OK;
}

/* alwan_apca_contrast_f32 / alwan_apca_contrast_f64 generated via alwan_vision_impl.inc */
