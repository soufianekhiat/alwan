/* ================================================================
 * Alwan - P10.1: Color Blindness Simulation
 * ================================================================
 * Implements color vision deficiency (CVD) simulation based on
 * Brettel, Viénot & Mollon (1997)
 * ================================================================ */

#include "alwan.h"
#include "alwan_internal.h"
#include <math.h>

/* ================================================================
 * P10.1: Color Vision Deficiency (CVD) Simulation
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

int alwan_simulate_cvd(alwan_vec3 const *rgb_in,
                        alwan_cvd_type cvd_type,
                        alwan_scalar severity,
                        alwan_vec3 *rgb_out) {
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
    mat3_mulv(RGB_TO_LMS, rgb_in, &lms);

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
    mat3_mulv(LMS_TO_RGB, &lms_cvd, rgb_out);

    /* Clamp to valid RGB range [0, 1] */
    if (rgb_out->v[0] < ALWAN_LITERAL(0.0)) rgb_out->v[0] = ALWAN_LITERAL(0.0);
    if (rgb_out->v[0] > ALWAN_LITERAL(1.0)) rgb_out->v[0] = ALWAN_LITERAL(1.0);
    if (rgb_out->v[1] < ALWAN_LITERAL(0.0)) rgb_out->v[1] = ALWAN_LITERAL(0.0);
    if (rgb_out->v[1] > ALWAN_LITERAL(1.0)) rgb_out->v[1] = ALWAN_LITERAL(1.0);
    if (rgb_out->v[2] < ALWAN_LITERAL(0.0)) rgb_out->v[2] = ALWAN_LITERAL(0.0);
    if (rgb_out->v[2] > ALWAN_LITERAL(1.0)) rgb_out->v[2] = ALWAN_LITERAL(1.0);

    return ALWAN_OK;
}

/* ================================================================
 * P10.2 & P10.3: Stub implementations (to be completed later)
 * ================================================================ */

alwan_scalar alwan_luminous_efficiency(alwan_scalar wavelength, alwan_vision_type vision_type) {
    (void)wavelength;
    (void)vision_type;
    /* TODO: Implement luminous efficiency function */
    return ALWAN_LITERAL(-1.0);
}

alwan_scalar alwan_photopic_luminance(alwan_ctx *ctx, alwan_spd const *spd) {
    (void)ctx;
    (void)spd;
    /* TODO: Implement photopic luminance */
    return ALWAN_LITERAL(-1.0);
}

alwan_scalar alwan_scotopic_luminance(alwan_ctx *ctx, alwan_spd const *spd) {
    (void)ctx;
    (void)spd;
    /* TODO: Implement scotopic luminance */
    return ALWAN_LITERAL(-1.0);
}

alwan_scalar alwan_mesopic_luminance(alwan_ctx *ctx,
                                      alwan_spd const *spd,
                                      alwan_scalar adaptation_level) {
    (void)ctx;
    (void)spd;
    (void)adaptation_level;
    /* TODO: Implement mesopic luminance */
    return ALWAN_LITERAL(-1.0);
}

alwan_scalar alwan_csf(alwan_scalar spatial_frequency, alwan_scalar luminance) {
    (void)spatial_frequency;
    (void)luminance;
    /* TODO: Implement contrast sensitivity function */
    return ALWAN_LITERAL(-1.0);
}
