/* ================================================================
 * Alwan - Color Blindness Simulation
 * ================================================================
 * Implements color vision deficiency (CVD) simulation based on
 * Brettel, Viénot & Mollon (1997)
 * ================================================================ */

#include "alwan.h"
#include "alwan_internal.h"
#include <math.h>

/* ================================================================
 * Color Vision Deficiency (CVD) Simulation
 * ================================================================ */

/* CVD transformation matrices as flat 9-element arrays
 * Row-major format: [r0c0, r0c1, r0c2, r1c0, r1c1, r1c2, r2c0, r2c1, r2c2] */

/* Protanopia (L-cone absent) - red-blind */
static alwan_scalar const PROTANOPIA_MATRIX[9] = {
    ALWAN_LITERAL(0.0), ALWAN_LITERAL(2.02344), ALWAN_LITERAL(-2.52581),
    ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0),
    ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)
};

/* Deuteranopia (M-cone absent) - green-blind */
static alwan_scalar const DEUTERANOPIA_MATRIX[9] = {
    ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
    ALWAN_LITERAL(0.494207), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.24827),
    ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)
};

/* Tritanopia (S-cone absent) - blue-blind */
static alwan_scalar const TRITANOPIA_MATRIX[9] = {
    ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
    ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0),
    ALWAN_LITERAL(-0.395913), ALWAN_LITERAL(0.801109), ALWAN_LITERAL(0.0)
};

/* sRGB to LMS matrix */
static alwan_scalar const RGB_TO_LMS[9] = {
    ALWAN_LITERAL(0.31399022), ALWAN_LITERAL(0.63951294), ALWAN_LITERAL(0.04649755),
    ALWAN_LITERAL(0.15537241), ALWAN_LITERAL(0.75789446), ALWAN_LITERAL(0.08670142),
    ALWAN_LITERAL(0.01775239), ALWAN_LITERAL(0.10944209), ALWAN_LITERAL(0.87256922)
};

/* LMS to sRGB matrix */
static alwan_scalar const LMS_TO_RGB[9] = {
    ALWAN_LITERAL(5.47221206), ALWAN_LITERAL(-4.6419601), ALWAN_LITERAL(0.16963708),
    ALWAN_LITERAL(-1.1252419), ALWAN_LITERAL(2.29317094), ALWAN_LITERAL(-0.1678952),
    ALWAN_LITERAL(0.02980165), ALWAN_LITERAL(-0.19318073), ALWAN_LITERAL(1.16364789)
};

/* Helper: multiply 3x3 matrix by vector */
static void mat3_mulv(alwan_scalar const *M, alwan_vec3 const *v, alwan_vec3 *out) {
    alwan_scalar x = M[0] * v->v[0] + M[1] * v->v[1] + M[2] * v->v[2];
    alwan_scalar y = M[3] * v->v[0] + M[4] * v->v[1] + M[5] * v->v[2];
    alwan_scalar z = M[6] * v->v[0] + M[7] * v->v[1] + M[8] * v->v[2];
    out->v[0] = x;
    out->v[1] = y;
    out->v[2] = z;
}

int alwan_simulate_cvd(alwan_rgb *rgb_out,
                        alwan_rgb const *rgb_in,
                        alwan_cvd_type cvd_type,
                        alwan_scalar severity) {
    if (!rgb_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* Clamp severity to [0, 1] */
    if (severity < ALWAN_LITERAL(0.0)) severity = ALWAN_LITERAL(0.0);
    if (severity > ALWAN_LITERAL(1.0)) severity = ALWAN_LITERAL(1.0);

    /* For normal vision (severity = 0), return input unchanged */
    if (severity < ALWAN_LITERAL(0.001)) {
        *rgb_out = *rgb_in;
        return ALWAN_OK;
    }

    /* Convert RGB to LMS */
    alwan_vec3 lms;
    mat3_mulv(RGB_TO_LMS, (alwan_vec3 const *)rgb_in, &lms);

    /* Apply CVD transformation based on type */
    alwan_vec3 lms_cvd;
    alwan_scalar const *cvd_matrix;

    switch (cvd_type) {
        case ALWAN_CVD_PROTANOPIA:
        case ALWAN_CVD_PROTANOMALY:
            cvd_matrix = PROTANOPIA_MATRIX;
            break;

        case ALWAN_CVD_DEUTERANOPIA:
        case ALWAN_CVD_DEUTERANOMALY:
            cvd_matrix = DEUTERANOPIA_MATRIX;
            break;

        case ALWAN_CVD_TRITANOPIA:
        case ALWAN_CVD_TRITANOMALY:
            cvd_matrix = TRITANOPIA_MATRIX;
            break;

        default:
            return ALWAN_E_INVALID;
    }

    /* Apply confusion line transformation */
    mat3_mulv(cvd_matrix, &lms, &lms_cvd);

    /* For anomalous trichromacy, blend between normal and dichromatic vision */
    if (cvd_type >= ALWAN_CVD_PROTANOMALY) {
        /* Interpolate: lms_final = (1-severity)*lms + severity*lms_cvd */
        lms_cvd.v[0] = (ALWAN_LITERAL(1.0) - severity) * lms.v[0] + severity * lms_cvd.v[0];
        lms_cvd.v[1] = (ALWAN_LITERAL(1.0) - severity) * lms.v[1] + severity * lms_cvd.v[1];
        lms_cvd.v[2] = (ALWAN_LITERAL(1.0) - severity) * lms.v[2] + severity * lms_cvd.v[2];
    }

    /* Convert LMS back to RGB */
    mat3_mulv(LMS_TO_RGB, &lms_cvd, (alwan_vec3 *)rgb_out);

    /* Clamp to valid RGB range [0, 1] */
    if (rgb_out->r < ALWAN_LITERAL(0.0)) rgb_out->r = ALWAN_LITERAL(0.0);
    if (rgb_out->r > ALWAN_LITERAL(1.0)) rgb_out->r = ALWAN_LITERAL(1.0);
    if (rgb_out->g < ALWAN_LITERAL(0.0)) rgb_out->g = ALWAN_LITERAL(0.0);
    if (rgb_out->g > ALWAN_LITERAL(1.0)) rgb_out->g = ALWAN_LITERAL(1.0);
    if (rgb_out->b < ALWAN_LITERAL(0.0)) rgb_out->b = ALWAN_LITERAL(0.0);
    if (rgb_out->b > ALWAN_LITERAL(1.0)) rgb_out->b = ALWAN_LITERAL(1.0);

    return ALWAN_OK;
}

/* ================================================================
 * Luminous Efficiency Functions
 * ================================================================ */

/* CIE Photopic Luminous Efficiency V(λ) - 1924/1988
 * Peak sensitivity at 555nm (normalized to 1.0)
 * Data from CIE 1988 2° Standard Observer */
static alwan_scalar const PHOTOPIC_V_LAMBDA_WL[] = {
    ALWAN_LITERAL(380.0), ALWAN_LITERAL(390.0), ALWAN_LITERAL(400.0), ALWAN_LITERAL(410.0),
    ALWAN_LITERAL(420.0), ALWAN_LITERAL(430.0), ALWAN_LITERAL(440.0), ALWAN_LITERAL(450.0),
    ALWAN_LITERAL(460.0), ALWAN_LITERAL(470.0), ALWAN_LITERAL(480.0), ALWAN_LITERAL(490.0),
    ALWAN_LITERAL(500.0), ALWAN_LITERAL(510.0), ALWAN_LITERAL(520.0), ALWAN_LITERAL(530.0),
    ALWAN_LITERAL(540.0), ALWAN_LITERAL(550.0), ALWAN_LITERAL(555.0), ALWAN_LITERAL(560.0),
    ALWAN_LITERAL(570.0), ALWAN_LITERAL(580.0), ALWAN_LITERAL(590.0), ALWAN_LITERAL(600.0),
    ALWAN_LITERAL(610.0), ALWAN_LITERAL(620.0), ALWAN_LITERAL(630.0), ALWAN_LITERAL(640.0),
    ALWAN_LITERAL(650.0), ALWAN_LITERAL(660.0), ALWAN_LITERAL(670.0), ALWAN_LITERAL(680.0),
    ALWAN_LITERAL(690.0), ALWAN_LITERAL(700.0), ALWAN_LITERAL(710.0), ALWAN_LITERAL(720.0),
    ALWAN_LITERAL(730.0), ALWAN_LITERAL(740.0), ALWAN_LITERAL(750.0), ALWAN_LITERAL(760.0),
    ALWAN_LITERAL(770.0), ALWAN_LITERAL(780.0)
};

static alwan_scalar const PHOTOPIC_V_LAMBDA[] = {
    ALWAN_LITERAL(0.00004), ALWAN_LITERAL(0.00012), ALWAN_LITERAL(0.00040), ALWAN_LITERAL(0.00116),
    ALWAN_LITERAL(0.00400), ALWAN_LITERAL(0.01160), ALWAN_LITERAL(0.02300), ALWAN_LITERAL(0.03800),
    ALWAN_LITERAL(0.06000), ALWAN_LITERAL(0.09098), ALWAN_LITERAL(0.13902), ALWAN_LITERAL(0.20802),
    ALWAN_LITERAL(0.32300), ALWAN_LITERAL(0.50300), ALWAN_LITERAL(0.71000), ALWAN_LITERAL(0.86200),
    ALWAN_LITERAL(0.95400), ALWAN_LITERAL(0.99495), ALWAN_LITERAL(1.00000), ALWAN_LITERAL(0.99500),
    ALWAN_LITERAL(0.95200), ALWAN_LITERAL(0.87000), ALWAN_LITERAL(0.75700), ALWAN_LITERAL(0.63100),
    ALWAN_LITERAL(0.50300), ALWAN_LITERAL(0.38100), ALWAN_LITERAL(0.26500), ALWAN_LITERAL(0.17500),
    ALWAN_LITERAL(0.10700), ALWAN_LITERAL(0.06100), ALWAN_LITERAL(0.03200), ALWAN_LITERAL(0.01700),
    ALWAN_LITERAL(0.00821), ALWAN_LITERAL(0.00410), ALWAN_LITERAL(0.00209), ALWAN_LITERAL(0.00105),
    ALWAN_LITERAL(0.00052), ALWAN_LITERAL(0.00025), ALWAN_LITERAL(0.00012), ALWAN_LITERAL(0.00006),
    ALWAN_LITERAL(0.00003), ALWAN_LITERAL(0.000015)
};

#define PHOTOPIC_V_COUNT (sizeof(PHOTOPIC_V_LAMBDA_WL) / sizeof(PHOTOPIC_V_LAMBDA_WL[0]))

/* CIE Scotopic Luminous Efficiency V'(λ) - 1951
 * Peak sensitivity around 507nm (normalized to 1.0)
 * Data from CIE 1951 Scotopic Observer */
static alwan_scalar const SCOTOPIC_VP_LAMBDA_WL[] = {
    ALWAN_LITERAL(380.0), ALWAN_LITERAL(390.0), ALWAN_LITERAL(400.0), ALWAN_LITERAL(410.0),
    ALWAN_LITERAL(420.0), ALWAN_LITERAL(430.0), ALWAN_LITERAL(440.0), ALWAN_LITERAL(450.0),
    ALWAN_LITERAL(460.0), ALWAN_LITERAL(470.0), ALWAN_LITERAL(480.0), ALWAN_LITERAL(490.0),
    ALWAN_LITERAL(500.0), ALWAN_LITERAL(507.0), ALWAN_LITERAL(510.0), ALWAN_LITERAL(520.0),
    ALWAN_LITERAL(530.0), ALWAN_LITERAL(540.0), ALWAN_LITERAL(550.0), ALWAN_LITERAL(560.0),
    ALWAN_LITERAL(570.0), ALWAN_LITERAL(580.0), ALWAN_LITERAL(590.0), ALWAN_LITERAL(600.0),
    ALWAN_LITERAL(610.0), ALWAN_LITERAL(620.0), ALWAN_LITERAL(630.0), ALWAN_LITERAL(640.0),
    ALWAN_LITERAL(650.0), ALWAN_LITERAL(660.0), ALWAN_LITERAL(670.0), ALWAN_LITERAL(680.0),
    ALWAN_LITERAL(690.0), ALWAN_LITERAL(700.0), ALWAN_LITERAL(710.0), ALWAN_LITERAL(720.0),
    ALWAN_LITERAL(730.0), ALWAN_LITERAL(740.0), ALWAN_LITERAL(750.0), ALWAN_LITERAL(760.0),
    ALWAN_LITERAL(770.0), ALWAN_LITERAL(780.0)
};

static alwan_scalar const SCOTOPIC_VP_LAMBDA[] = {
    ALWAN_LITERAL(0.00059), ALWAN_LITERAL(0.00221), ALWAN_LITERAL(0.00929), ALWAN_LITERAL(0.03484),
    ALWAN_LITERAL(0.09660), ALWAN_LITERAL(0.19840), ALWAN_LITERAL(0.32800), ALWAN_LITERAL(0.45500),
    ALWAN_LITERAL(0.56700), ALWAN_LITERAL(0.67600), ALWAN_LITERAL(0.79300), ALWAN_LITERAL(0.90400),
    ALWAN_LITERAL(0.98200), ALWAN_LITERAL(1.00000), ALWAN_LITERAL(0.99700), ALWAN_LITERAL(0.93500),
    ALWAN_LITERAL(0.81100), ALWAN_LITERAL(0.65000), ALWAN_LITERAL(0.48100), ALWAN_LITERAL(0.32800),
    ALWAN_LITERAL(0.20700), ALWAN_LITERAL(0.12100), ALWAN_LITERAL(0.06550), ALWAN_LITERAL(0.03315),
    ALWAN_LITERAL(0.01593), ALWAN_LITERAL(0.00737), ALWAN_LITERAL(0.00334), ALWAN_LITERAL(0.00150),
    ALWAN_LITERAL(0.00068), ALWAN_LITERAL(0.00031), ALWAN_LITERAL(0.00015), ALWAN_LITERAL(0.00007),
    ALWAN_LITERAL(0.00004), ALWAN_LITERAL(0.00002), ALWAN_LITERAL(0.00001), ALWAN_LITERAL(0.000005),
    ALWAN_LITERAL(0.000003), ALWAN_LITERAL(0.000001), ALWAN_LITERAL(0.0000005), ALWAN_LITERAL(0.0000003),
    ALWAN_LITERAL(0.0000001), ALWAN_LITERAL(0.00000005)
};

#define SCOTOPIC_VP_COUNT (sizeof(SCOTOPIC_VP_LAMBDA_WL) / sizeof(SCOTOPIC_VP_LAMBDA_WL[0]))

/* Helper: linear interpolation in lookup table */
static alwan_scalar interpolate_lut(alwan_scalar const *wl_table,
                                      alwan_scalar const *value_table,
                                      int count,
                                      alwan_scalar wavelength) {
    /* Check bounds */
    if (wavelength <= wl_table[0]) {
        return value_table[0];
    }
    if (wavelength >= wl_table[count - 1]) {
        return value_table[count - 1];
    }

    /* Find bracketing indices */
    int i;
    for (i = 0; i < count - 1; i++) {
        if (wavelength >= wl_table[i] && wavelength <= wl_table[i + 1]) {
            /* Linear interpolation */
            alwan_scalar t = (wavelength - wl_table[i]) / (wl_table[i + 1] - wl_table[i]);
            return value_table[i] + t * (value_table[i + 1] - value_table[i]);
        }
    }

    /* Should not reach here */
    return ALWAN_LITERAL(0.0);
}

alwan_scalar alwan_luminous_efficiency(alwan_scalar wavelength, alwan_vision_type vision_type) {
    /* Validate wavelength range [360, 830] */
    if (wavelength < ALWAN_LITERAL(360.0) || wavelength > ALWAN_LITERAL(830.0)) {
        return ALWAN_LITERAL(-1.0);
    }

    switch (vision_type) {
        case ALWAN_VISION_PHOTOPIC:
            return interpolate_lut(PHOTOPIC_V_LAMBDA_WL, PHOTOPIC_V_LAMBDA,
                                   (int)PHOTOPIC_V_COUNT, wavelength);

        case ALWAN_VISION_SCOTOPIC:
            return interpolate_lut(SCOTOPIC_VP_LAMBDA_WL, SCOTOPIC_VP_LAMBDA,
                                   (int)SCOTOPIC_VP_COUNT, wavelength);

        case ALWAN_VISION_MESOPIC:
            /* For mesopic, we could blend photopic and scotopic
             * For now, return photopic as a simple approximation */
            return interpolate_lut(PHOTOPIC_V_LAMBDA_WL, PHOTOPIC_V_LAMBDA,
                                   (int)PHOTOPIC_V_COUNT, wavelength);

        default:
            return ALWAN_LITERAL(-1.0);
    }
}

alwan_scalar alwan_photopic_luminance(alwan_ctx *ctx, alwan_spd const *spd) {
    (void)ctx;
    (void)spd;
    /* Not yet implemented */
    return ALWAN_LITERAL(-1.0);
}

alwan_scalar alwan_scotopic_luminance(alwan_ctx *ctx, alwan_spd const *spd) {
    (void)ctx;
    (void)spd;
    /* Not yet implemented */
    return ALWAN_LITERAL(-1.0);
}

alwan_scalar alwan_mesopic_luminance(alwan_ctx *ctx,
                                      alwan_spd const *spd,
                                      alwan_scalar adaptation_level) {
    (void)ctx;
    (void)spd;
    (void)adaptation_level;
    /* Not yet implemented */
    return ALWAN_LITERAL(-1.0);
}

/* ================================================================
 * Contrast Sensitivity Function (CSF)
 * ================================================================ */

/* Barten CSF model (1999)
 * Computes contrast sensitivity as a function of spatial frequency and luminance
 * Reference: Barten, P. G. J. (1999). Contrast sensitivity of the human eye
 *            and its effects on image quality. SPIE Press. */

alwan_scalar alwan_csf(alwan_scalar spatial_frequency, alwan_scalar luminance) {
    /* Validate input ranges */
    if (spatial_frequency < ALWAN_LITERAL(0.1) || spatial_frequency > ALWAN_LITERAL(60.0)) {
        return ALWAN_LITERAL(-1.0);
    }
    if (luminance < ALWAN_LITERAL(0.01) || luminance > ALWAN_LITERAL(10000.0)) {
        return ALWAN_LITERAL(-1.0);
    }

    /* Barten CSF model parameters */
    alwan_scalar const k = ALWAN_LITERAL(3.0);         /* Neural noise factor */
    alwan_scalar const phi_0 = ALWAN_LITERAL(3.0e-8);  /* Photon noise constant */

    alwan_scalar f = spatial_frequency;
    alwan_scalar L = luminance;

    /* Simplified Barten CSF model
     * Returns contrast sensitivity (1/threshold) as a function of
     * spatial frequency and luminance */

    /* Calculate retinal illuminance E (trolands) from luminance
     * E ≈ L * pupil_area, where pupil diameter depends on luminance
     * Simplified model: pupil diameter d ≈ 5 - 3*tanh(0.4*log10(L)) */
    alwan_scalar log_L = ALWAN_LOG10(L);
    alwan_scalar d = ALWAN_LITERAL(5.0) - ALWAN_LITERAL(3.0) * ALWAN_TANH(ALWAN_LITERAL(0.4) * log_L);
    alwan_scalar pupil_area = ALWAN_LITERAL(3.14159265359) * d * d / ALWAN_LITERAL(4.0);
    alwan_scalar E = L * pupil_area;

    /* Band-pass filter centered around 4-6 cpd
     * Models lateral inhibition and neural filtering */
    alwan_scalar low_freq_atten = f / (f + ALWAN_LITERAL(0.5));  /* Low-frequency roll-off */
    alwan_scalar high_freq_atten = ALWAN_EXP(-ALWAN_LITERAL(0.005) * f * f);  /* High-frequency roll-off */
    alwan_scalar M_opt = low_freq_atten * high_freq_atten;

    /* Photon noise (inversely proportional to retinal illuminance) */
    alwan_scalar noise_photon = phi_0 / (E + ALWAN_LITERAL(1e-10));

    /* Neural noise (constant baseline) */
    alwan_scalar noise_neural = ALWAN_LITERAL(1.0) / k;

    /* Total noise */
    alwan_scalar noise_total = ALWAN_SQRT(noise_photon * noise_photon + noise_neural * noise_neural);

    /* Contrast sensitivity = signal / noise */
    alwan_scalar S = (M_opt * E) / (noise_total + ALWAN_LITERAL(1e-10));

    /* Scale to reasonable values (typical CSF peak ~100-500) */
    S = S * ALWAN_LITERAL(10.0);

    return S;
}

/* ================================================================
 * Barten 1999 Full Model Implementation
 * Reference: Barten (1999), colour-science implementation
 * All formulas from colour-science 0.4.6
 * ================================================================ */

/* Pupil diameter using Barten (1999) method
 * Formula: d = 5 - 3 * tanh(0.4 * log10(L * X_0 * Y_0 / 40^2)) */
alwan_scalar alwan_pupil_diameter_barten1999(alwan_scalar L,
                                              alwan_scalar X_0,
                                              alwan_scalar Y_0)
{
    alwan_scalar Y = (Y_0 < ALWAN_LITERAL(0.0)) ? X_0 : Y_0;
    alwan_scalar arg = ALWAN_LITERAL(0.4) * ALWAN_LOG10(L * X_0 * Y / ALWAN_LITERAL(1600.0));
    return ALWAN_LITERAL(5.0) - ALWAN_LITERAL(3.0) * ALWAN_TANH(arg);
}

/* Retinal illuminance using Barten (1999) method
 * Formula: E = (pi * d^2 / 4) * L * [Stiles-Crawford correction]
 * Stiles-Crawford: 1 - (d/9.7)^2 + (d/12.4)^4 */
alwan_scalar alwan_retinal_illuminance_barten1999(alwan_scalar L,
                                                   alwan_scalar d,
                                                   int apply_stiles_crawford)
{
    alwan_scalar E = (ALWAN_PI * d * d / ALWAN_LITERAL(4.0)) * L;

    if (apply_stiles_crawford) {
        alwan_scalar d_97 = d / ALWAN_LITERAL(9.7);
        alwan_scalar d_124 = d / ALWAN_LITERAL(12.4);
        E *= ALWAN_LITERAL(1.0) - d_97 * d_97 + d_124 * d_124 * d_124 * d_124;
    }

    return E;
}

/* Optical MTF using Barten (1999) method
 * Formula: M_opt = exp(-2 * pi^2 * sigma^2 * u^2) */
alwan_scalar alwan_optical_mtf_barten1999(alwan_scalar u, alwan_scalar sigma)
{
    return ALWAN_EXP(ALWAN_LITERAL(-2.0) * ALWAN_PI * ALWAN_PI * sigma * sigma * u * u);
}

/* Standard deviation of line-spread function using Barten (1999) method
 * Formula: sigma = sqrt(sigma_0^2 + (C_ab * d)^2) = hypot(sigma_0, C_ab * d) */
alwan_scalar alwan_sigma_barten1999(alwan_scalar sigma_0,
                                     alwan_scalar C_ab,
                                     alwan_scalar d)
{
    alwan_scalar Cab_d = C_ab * d;
    return ALWAN_SQRT(sigma_0 * sigma_0 + Cab_d * Cab_d);
}

/* Maximum angular size using Barten (1999) method
 * Formula: X = (1/X_0^2 + 1/X_max^2 + u^2/N_max^2)^(-0.5) */
alwan_scalar alwan_maximum_angular_size_barten1999(alwan_scalar u,
                                                    alwan_scalar X_0,
                                                    alwan_scalar X_max,
                                                    alwan_scalar N_max)
{
    alwan_scalar term1 = ALWAN_LITERAL(1.0) / (X_0 * X_0);
    alwan_scalar term2 = ALWAN_LITERAL(1.0) / (X_max * X_max);
    alwan_scalar term3 = (u * u) / (N_max * N_max);
    return ALWAN_POW(term1 + term2 + term3, ALWAN_LITERAL(-0.5));
}

/* Initialize CSF parameters with defaults from colour-science */
void alwan_csf_barten1999_params_default(alwan_csf_barten1999_params *params)
{
    if (!params) return;

    /* Default sigma using sigma_0=0.5/60, C_ab=0.08/60, d=2.1 */
    params->sigma = alwan_sigma_barten1999(
        ALWAN_LITERAL(0.5) / ALWAN_LITERAL(60.0),
        ALWAN_LITERAL(0.08) / ALWAN_LITERAL(60.0),
        ALWAN_LITERAL(2.1)
    );
    params->k = ALWAN_LITERAL(3.0);
    params->T = ALWAN_LITERAL(0.1);
    params->X_0 = ALWAN_LITERAL(60.0);
    params->Y_0 = ALWAN_LITERAL(-1.0);  /* -1 means use X_0 */
    params->X_max = ALWAN_LITERAL(12.0);
    params->Y_max = ALWAN_LITERAL(-1.0);  /* -1 means use X_max */
    params->N_max = ALWAN_LITERAL(15.0);
    params->n = ALWAN_LITERAL(0.03);
    params->p = ALWAN_LITERAL(1.2274e6);
    /* Default E using L=20, d=2.1, Stiles-Crawford=true */
    params->E = alwan_retinal_illuminance_barten1999(
        ALWAN_LITERAL(20.0),
        ALWAN_LITERAL(2.1),
        1
    );
    params->phi_0 = ALWAN_LITERAL(3.0e-8);
    params->u_0 = ALWAN_LITERAL(7.0);
}

/* Full Barten (1999) CSF
 * Formula: S = (M_opt / k) / sqrt(2/T * M_as * (1/(n*p*E) + phi_0/(1 - exp(-(u/u_0)^2)))) */
alwan_scalar alwan_csf_barten1999(alwan_scalar u,
                                   alwan_csf_barten1999_params const *params)
{
    alwan_csf_barten1999_params defaults;
    alwan_csf_barten1999_params const *p;

    if (params) {
        p = params;
    } else {
        alwan_csf_barten1999_params_default(&defaults);
        p = &defaults;
    }

    /* Get Y values (use X if Y is -1) */
    alwan_scalar Y_0 = (p->Y_0 < ALWAN_LITERAL(0.0)) ? p->X_0 : p->Y_0;
    alwan_scalar Y_max = (p->Y_max < ALWAN_LITERAL(0.0)) ? p->X_max : p->Y_max;

    /* Optical MTF */
    alwan_scalar M_opt = alwan_optical_mtf_barten1999(u, p->sigma);

    /* Maximum angular size product M_as = 1/(X*Y) */
    alwan_scalar X = alwan_maximum_angular_size_barten1999(u, p->X_0, p->X_max, p->N_max);
    alwan_scalar Y = alwan_maximum_angular_size_barten1999(u, Y_0, Y_max, p->N_max);
    alwan_scalar M_as = ALWAN_LITERAL(1.0) / (X * Y);

    /* Photon noise term */
    alwan_scalar photon_term = ALWAN_LITERAL(1.0) / (p->n * p->p * p->E);

    /* Neural noise term with lateral inhibition cutoff */
    alwan_scalar u_ratio = u / p->u_0;
    alwan_scalar neural_term = p->phi_0 / (ALWAN_LITERAL(1.0) - ALWAN_EXP(-(u_ratio * u_ratio)));

    /* Total noise under square root */
    alwan_scalar noise = (ALWAN_LITERAL(2.0) / p->T) * M_as * (photon_term + neural_term);

    /* Contrast sensitivity */
    alwan_scalar S = (M_opt / p->k) / ALWAN_SQRT(noise);

    return S;
}
