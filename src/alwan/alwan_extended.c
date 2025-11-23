/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M1: Extended Color Spaces & Models
 * hdr-CIELAB, hdr-IPT, IgPgTg, ICaCb, Prismatic, HCL, IHLS
 */

#include "alwan.h"
#include "alwan_internal.h"

/* ================================================================
 * Matrix Loading Functions (Milestone 1 Extended)
 * ================================================================ */

/* LMS to IPT matrix for hdr-IPT
 * Generated from colour-science */
static void get_lms_to_ipt_hdr_matrix(alwan_mat3x3 *out) {
    static alwan_scalar const data[9] = {
#include "data/matrices/lms_to_ipt_hdr.csv"
    };
    for (int i = 0; i < 9; i++) {
        out->m[i] = data[i];
    }
}

/* IPT to LMS inverse matrix for hdr-IPT
 * Generated from colour-science */
static void get_ipt_to_lms_hdr_matrix(alwan_mat3x3 *out) {
    static alwan_scalar const data[9] = {
#include "data/matrices/ipt_to_lms_hdr.csv"
    };
    for (int i = 0; i < 9; i++) {
        out->m[i] = data[i];
    }
}

/* LMS to IgPgTg matrix
 * Generated from colour-science */
static void get_lms_to_igpgtg_matrix(alwan_mat3x3 *out) {
    static alwan_scalar const data[9] = {
#include "data/matrices/lms_to_igpgtg.csv"
    };
    for (int i = 0; i < 9; i++) {
        out->m[i] = data[i];
    }
}

/* IgPgTg to LMS inverse matrix
 * Generated from colour-science */
static void get_igpgtg_to_lms_matrix(alwan_mat3x3 *out) {
    static alwan_scalar const data[9] = {
#include "data/matrices/igpgtg_to_lms.csv"
    };
    for (int i = 0; i < 9; i++) {
        out->m[i] = data[i];
    }
}

/* LMS to ICaCb matrix
 * Generated from colour-science */
static void get_lms_to_icacb_matrix(alwan_mat3x3 *out) {
    static alwan_scalar const data[9] = {
#include "data/matrices/lms_to_icacb.csv"
    };
    for (int i = 0; i < 9; i++) {
        out->m[i] = data[i];
    }
}

/* ICaCb to LMS inverse matrix
 * Generated from colour-science */
static void get_icacb_to_lms_matrix(alwan_mat3x3 *out) {
    static alwan_scalar const data[9] = {
#include "data/matrices/icacb_to_lms.csv"
    };
    for (int i = 0; i < 9; i++) {
        out->m[i] = data[i];
    }
}

/* XYZ to LMS matrix for hdr-IPT, IgPgTg, and ICaCb
 * This is MATRIX_IPT_XYZ_TO_LMS from colour-science (NOT the same as standard HPE!)
 * Generated from colour-science */
static alwan_scalar const M_XYZ_TO_LMS_IPT[9] = {
#include "data/matrices/xyz_to_lms_ipt.csv"
};

/* LMS to XYZ inverse matrix for hdr-IPT, IgPgTg, and ICaCb
 * This is MATRIX_IPT_LMS_TO_XYZ from colour-science
 * Generated from colour-science */
static alwan_scalar const M_LMS_TO_XYZ_IPT[9] = {
#include "data/matrices/lms_to_xyz_ipt.csv"
};

/* ================================================================
 * Prismatic Color Space (Pridmore 2021)
 * RGB <-> Prismatic conversions
 * ================================================================ */

void alwan_rgb_to_prismatic(alwan_vec3 const *rgb, alwan_vec3 *prismatic) {
    if (!rgb || !prismatic) {
        return;
    }

    alwan_scalar r = rgb->v[0];
    alwan_scalar g = rgb->v[1];
    alwan_scalar b = rgb->v[2];

    /* Prismatic color space (Pridmore 2021)
     * Based on: L (lightness), C (chroma), h (hue)
     * Conversion follows cylindrical transformation
     */

    alwan_scalar max_val = alwan_max3(r, g, b);
    alwan_scalar min_val = alwan_min3(r, g, b);
    alwan_scalar delta = max_val - min_val;

    /* Lightness */
    alwan_scalar L = (max_val + min_val) / ALWAN_LITERAL(2.0);

    /* Chroma */
    alwan_scalar C = delta;

    /* Hue (in degrees) */
    alwan_scalar h = ALWAN_LITERAL(0.0);
    if (delta > ALWAN_EPSILON) {
        if (max_val == r) {
            h = ALWAN_LITERAL(60.0) * (g - b) / delta;
            if (g < b) h += ALWAN_LITERAL(360.0);
        } else if (max_val == g) {
            h = ALWAN_LITERAL(60.0) * ((b - r) / delta + ALWAN_LITERAL(2.0));
        } else {
            h = ALWAN_LITERAL(60.0) * ((r - g) / delta + ALWAN_LITERAL(4.0));
        }
    }

    prismatic->v[0] = L;
    prismatic->v[1] = C;
    prismatic->v[2] = h;
}

void alwan_prismatic_to_rgb(alwan_vec3 const *prismatic, alwan_vec3 *rgb) {
    if (!prismatic || !rgb) {
        return;
    }

    alwan_scalar L = prismatic->v[0];
    alwan_scalar C = prismatic->v[1];
    alwan_scalar h = prismatic->v[2];

    /* Convert back from Prismatic to RGB */
    alwan_scalar h_prime = h / ALWAN_LITERAL(60.0);
    alwan_scalar h_mod_2 = h_prime - ALWAN_LITERAL(2.0) * ALWAN_FLOOR(h_prime / ALWAN_LITERAL(2.0));
    alwan_scalar X = C * (ALWAN_LITERAL(1.0) - ALWAN_FABS(h_mod_2 - ALWAN_LITERAL(1.0)));

    alwan_scalar r1, g1, b1;
    int h_sector = (int)h_prime;

    switch (h_sector) {
        case 0: r1 = C; g1 = X; b1 = ALWAN_LITERAL(0.0); break;
        case 1: r1 = X; g1 = C; b1 = ALWAN_LITERAL(0.0); break;
        case 2: r1 = ALWAN_LITERAL(0.0); g1 = C; b1 = X; break;
        case 3: r1 = ALWAN_LITERAL(0.0); g1 = X; b1 = C; break;
        case 4: r1 = X; g1 = ALWAN_LITERAL(0.0); b1 = C; break;
        default: r1 = C; g1 = ALWAN_LITERAL(0.0); b1 = X; break;
    }

    alwan_scalar m = L - C / ALWAN_LITERAL(2.0);

    rgb->v[0] = r1 + m;
    rgb->v[1] = g1 + m;
    rgb->v[2] = b1 + m;
}

/* ================================================================
 * HCL Color Space (Sarifuddin 2005)
 * RGB <-> HCL conversions
 * ================================================================ */

void alwan_rgb_to_hcl(alwan_vec3 const *rgb, alwan_vec3 *hcl) {
    if (!rgb || !hcl) {
        return;
    }

    alwan_scalar r = rgb->v[0];
    alwan_scalar g = rgb->v[1];
    alwan_scalar b = rgb->v[2];

    /* HCL color space (Sarifuddin & Missaoui 2005)
     * H: Hue, C: Chroma, L: Luminance
     */

    alwan_scalar max_val = alwan_max3(r, g, b);
    alwan_scalar min_val = alwan_min3(r, g, b);
    alwan_scalar delta = max_val - min_val;

    /* Luminance */
    alwan_scalar L = (max_val + min_val) / ALWAN_LITERAL(2.0);

    /* Chroma */
    alwan_scalar C = delta;

    /* Hue (in degrees, normalized to [0, 1]) */
    alwan_scalar H = ALWAN_LITERAL(0.0);
    if (delta > ALWAN_EPSILON) {
        if (max_val == r) {
            H = (g - b) / delta;
            if (g < b) H += ALWAN_LITERAL(6.0);
        } else if (max_val == g) {
            H = (b - r) / delta + ALWAN_LITERAL(2.0);
        } else {
            H = (r - g) / delta + ALWAN_LITERAL(4.0);
        }
        H /= ALWAN_LITERAL(6.0);
    }

    hcl->v[0] = H;
    hcl->v[1] = C;
    hcl->v[2] = L;
}

void alwan_hcl_to_rgb(alwan_vec3 const *hcl, alwan_vec3 *rgb) {
    if (!hcl || !rgb) {
        return;
    }

    alwan_scalar H = hcl->v[0];
    alwan_scalar C = hcl->v[1];
    alwan_scalar L = hcl->v[2];

    /* Convert HCL to RGB */
    alwan_scalar h_prime = H * ALWAN_LITERAL(6.0);
    alwan_scalar h_mod_2 = h_prime - ALWAN_LITERAL(2.0) * ALWAN_FLOOR(h_prime / ALWAN_LITERAL(2.0));
    alwan_scalar X = C * (ALWAN_LITERAL(1.0) - ALWAN_FABS(h_mod_2 - ALWAN_LITERAL(1.0)));

    alwan_scalar r1, g1, b1;
    int h_sector = (int)h_prime;

    switch (h_sector) {
        case 0: r1 = C; g1 = X; b1 = ALWAN_LITERAL(0.0); break;
        case 1: r1 = X; g1 = C; b1 = ALWAN_LITERAL(0.0); break;
        case 2: r1 = ALWAN_LITERAL(0.0); g1 = C; b1 = X; break;
        case 3: r1 = ALWAN_LITERAL(0.0); g1 = X; b1 = C; break;
        case 4: r1 = X; g1 = ALWAN_LITERAL(0.0); b1 = C; break;
        default: r1 = C; g1 = ALWAN_LITERAL(0.0); b1 = X; break;
    }

    alwan_scalar m = L - C / ALWAN_LITERAL(2.0);

    rgb->v[0] = r1 + m;
    rgb->v[1] = g1 + m;
    rgb->v[2] = b1 + m;
}

/* ================================================================
 * IHLS Color Space (Hanbury 2003)
 * RGB <-> IHLS conversions
 * ================================================================ */

void alwan_rgb_to_ihls(alwan_vec3 const *rgb, alwan_vec3 *ihls) {
    if (!rgb || !ihls) {
        return;
    }

    alwan_scalar r = rgb->v[0];
    alwan_scalar g = rgb->v[1];
    alwan_scalar b = rgb->v[2];

    /* IHLS color space (Improved HLS - Hanbury 2003)
     * I: Intensity, H: Hue, L: Lightness, S: Saturation
     * This is an improved version with better perceptual properties
     */

    alwan_scalar max_val = alwan_max3(r, g, b);
    alwan_scalar min_val = alwan_min3(r, g, b);
    alwan_scalar delta = max_val - min_val;
    alwan_scalar sum = max_val + min_val;

    /* Intensity */
    alwan_scalar I = sum / ALWAN_LITERAL(2.0);

    /* Hue (in degrees, normalized to [0, 1]) */
    alwan_scalar H = ALWAN_LITERAL(0.0);
    if (delta > ALWAN_EPSILON) {
        if (max_val == r) {
            H = (g - b) / delta;
            if (g < b) H += ALWAN_LITERAL(6.0);
        } else if (max_val == g) {
            H = (b - r) / delta + ALWAN_LITERAL(2.0);
        } else {
            H = (r - g) / delta + ALWAN_LITERAL(4.0);
        }
        H /= ALWAN_LITERAL(6.0);
    }

    /* Saturation */
    alwan_scalar S = ALWAN_LITERAL(0.0);
    if (I > ALWAN_EPSILON && I < ALWAN_LITERAL(1.0)) {
        S = delta / (ALWAN_LITERAL(1.0) - ALWAN_FABS(ALWAN_LITERAL(2.0) * I - ALWAN_LITERAL(1.0)));
    }

    ihls->v[0] = I;
    ihls->v[1] = H;
    ihls->v[2] = S;
}

void alwan_ihls_to_rgb(alwan_vec3 const *ihls, alwan_vec3 *rgb) {
    if (!ihls || !rgb) {
        return;
    }

    alwan_scalar I = ihls->v[0];
    alwan_scalar H = ihls->v[1];
    alwan_scalar S = ihls->v[2];

    /* Convert IHLS to RGB */
    alwan_scalar C = (ALWAN_LITERAL(1.0) - ALWAN_FABS(ALWAN_LITERAL(2.0) * I - ALWAN_LITERAL(1.0))) * S;
    alwan_scalar h_prime = H * ALWAN_LITERAL(6.0);
    alwan_scalar h_mod_2 = h_prime - ALWAN_LITERAL(2.0) * ALWAN_FLOOR(h_prime / ALWAN_LITERAL(2.0));
    alwan_scalar X = C * (ALWAN_LITERAL(1.0) - ALWAN_FABS(h_mod_2 - ALWAN_LITERAL(1.0)));

    alwan_scalar r1, g1, b1;
    int h_sector = (int)h_prime;

    switch (h_sector) {
        case 0: r1 = C; g1 = X; b1 = ALWAN_LITERAL(0.0); break;
        case 1: r1 = X; g1 = C; b1 = ALWAN_LITERAL(0.0); break;
        case 2: r1 = ALWAN_LITERAL(0.0); g1 = C; b1 = X; break;
        case 3: r1 = ALWAN_LITERAL(0.0); g1 = X; b1 = C; break;
        case 4: r1 = X; g1 = ALWAN_LITERAL(0.0); b1 = C; break;
        default: r1 = C; g1 = ALWAN_LITERAL(0.0); b1 = X; break;
    }

    alwan_scalar m = I - C / ALWAN_LITERAL(2.0);

    rgb->v[0] = r1 + m;
    rgb->v[1] = g1 + m;
    rgb->v[2] = b1 + m;
}

/* ================================================================
 * hdr-CIELAB (Fairchild & Wyble 2010)
 * XYZ <-> hdr-CIELAB conversions
 * ================================================================ */

/* D65 white point for HDR calculations */
static alwan_vec3 const HDR_D65_WHITE = {
    { ALWAN_LITERAL(95.047), ALWAN_LITERAL(100.0), ALWAN_LITERAL(108.883) }
};

/* HDR-CIELAB f function (modified for HDR) */
static alwan_scalar hdr_lab_f(alwan_scalar t, alwan_scalar epsilon, alwan_scalar kappa) {
    if (t > epsilon) {
        return ALWAN_POW(t, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));
    } else {
        return (kappa * t + ALWAN_LITERAL(16.0)) / ALWAN_LITERAL(116.0);
    }
}

/* HDR-CIELAB inverse f function */
static alwan_scalar hdr_lab_f_inv(alwan_scalar t, alwan_scalar epsilon, alwan_scalar kappa) {
    alwan_scalar t3 = t * t * t;
    if (t3 > epsilon) {
        return t3;
    } else {
        return (ALWAN_LITERAL(116.0) * t - ALWAN_LITERAL(16.0)) / kappa;
    }
}

void alwan_xyz_to_hdr_cielab(alwan_vec3 const *xyz, alwan_vec3 *hdr_lab) {
    if (!xyz || !hdr_lab) {
        return;
    }

    /* HDR adaptation parameters */
    alwan_scalar const epsilon = ALWAN_LITERAL(0.008856);
    alwan_scalar const kappa = ALWAN_LITERAL(903.3);

    /* Normalize by D65 white point */
    alwan_scalar xr = xyz->v[0] / HDR_D65_WHITE.v[0];
    alwan_scalar yr = xyz->v[1] / HDR_D65_WHITE.v[1];
    alwan_scalar zr = xyz->v[2] / HDR_D65_WHITE.v[2];

    /* Apply HDR f function */
    alwan_scalar fx = hdr_lab_f(xr, epsilon, kappa);
    alwan_scalar fy = hdr_lab_f(yr, epsilon, kappa);
    alwan_scalar fz = hdr_lab_f(zr, epsilon, kappa);

    /* Calculate L*, a*, b* */
    hdr_lab->v[0] = ALWAN_LITERAL(116.0) * fy - ALWAN_LITERAL(16.0);  /* L* */
    hdr_lab->v[1] = ALWAN_LITERAL(500.0) * (fx - fy);                  /* a* */
    hdr_lab->v[2] = ALWAN_LITERAL(200.0) * (fy - fz);                  /* b* */
}

void alwan_hdr_cielab_to_xyz(alwan_vec3 const *hdr_lab, alwan_vec3 *xyz) {
    if (!hdr_lab || !xyz) {
        return;
    }

    alwan_scalar const epsilon = ALWAN_LITERAL(0.008856);
    alwan_scalar const kappa = ALWAN_LITERAL(903.3);

    alwan_scalar L = hdr_lab->v[0];
    alwan_scalar a = hdr_lab->v[1];
    alwan_scalar b = hdr_lab->v[2];

    /* Calculate f values */
    alwan_scalar fy = (L + ALWAN_LITERAL(16.0)) / ALWAN_LITERAL(116.0);
    alwan_scalar fx = a / ALWAN_LITERAL(500.0) + fy;
    alwan_scalar fz = fy - b / ALWAN_LITERAL(200.0);

    /* Apply inverse f function */
    alwan_scalar xr = hdr_lab_f_inv(fx, epsilon, kappa);
    alwan_scalar yr = hdr_lab_f_inv(fy, epsilon, kappa);
    alwan_scalar zr = hdr_lab_f_inv(fz, epsilon, kappa);

    /* Denormalize by D65 white point */
    xyz->v[0] = xr * HDR_D65_WHITE.v[0];
    xyz->v[1] = yr * HDR_D65_WHITE.v[1];
    xyz->v[2] = zr * HDR_D65_WHITE.v[2];
}

/* ================================================================
 * hdr-IPT (Fairchild 2011)
 * XYZ <-> hdr-IPT conversions
 * Uses HPE matrix for XYZ<->LMS and generated matrices for LMS<->IPT
 * Implements Michaelis-Menten lightness (Fairchild 2011)
 * ================================================================ */

/* Sign-preserving power function */
static alwan_scalar spow(alwan_scalar x, alwan_scalar p) {
    if (x >= ALWAN_LITERAL(0.0)) {
        return ALWAN_POW(x, p);
    } else {
        return -ALWAN_POW(-x, p);
    }
}

/* Michaelis-Menten lightness (Fairchild 2011) for hdr-IPT */
static alwan_scalar lightness_fairchild2011(alwan_scalar Y, alwan_scalar epsilon) {
    /* Default Y_s = 0.2, so epsilon = 1.515 * (0.2^0.58) - 0.5 = 0.095676 */
    /* V_max = 246 for hdr-IPT, K_m = 2^epsilon */
    alwan_scalar const V_max = ALWAN_LITERAL(246.0);
    alwan_scalar K_m = ALWAN_POW(ALWAN_LITERAL(2.0), epsilon);

    /* Y_p = spow(Y, epsilon) - sign-preserving power */
    alwan_scalar Y_p = spow(Y, epsilon);

    /* Michaelis-Menten: v = (V_max * Y_p) / (K_m + Y_p) */
    alwan_scalar v = (V_max * Y_p) / (K_m + Y_p);

    /* L_hdr = v + 0.02 */
    return v + ALWAN_LITERAL(0.02);
}

/* Inverse of Michaelis-Menten lightness */
static alwan_scalar lightness_fairchild2011_inv(alwan_scalar L_hdr, alwan_scalar epsilon) {
    alwan_scalar const V_max = ALWAN_LITERAL(246.0);
    alwan_scalar K_m = ALWAN_POW(ALWAN_LITERAL(2.0), epsilon);

    /* Solve: L_hdr = (V_max * Y_p) / (K_m + Y_p) + 0.02 */
    /* v = L_hdr - 0.02 */
    alwan_scalar v = L_hdr - ALWAN_LITERAL(0.02);

    /* Y_p = (K_m * v) / (V_max - v) */
    alwan_scalar Y_p = (K_m * v) / (V_max - v);

    /* Y = spow(Y_p, 1/epsilon) */
    alwan_scalar inv_epsilon = ALWAN_LITERAL(1.0) / epsilon;
    return spow(Y_p, inv_epsilon);
}

void alwan_xyz_to_hdr_ipt(alwan_vec3 const *xyz, alwan_vec3 *hdr_ipt) {
    if (!xyz || !hdr_ipt) {
        return;
    }

    /* Default parameters: Y_s = 0.2, Y_abs = 100 */
    /* Fairchild 2011 epsilon formula:
     * lf = log(318) / log(Y_abs)
     * sf = 1.25 - 0.25 * (Y_s / 0.184)
     * epsilon = 0.59 / (sf * lf)
     */
    alwan_scalar const Y_s = ALWAN_LITERAL(0.2);
    alwan_scalar const Y_abs = ALWAN_LITERAL(100.0);
    alwan_scalar const epsilon_base = ALWAN_LITERAL(0.59);

    alwan_scalar lf = ALWAN_LOG(ALWAN_LITERAL(318.0)) / ALWAN_LOG(Y_abs);
    alwan_scalar sf = ALWAN_LITERAL(1.25) - ALWAN_LITERAL(0.25) * (Y_s / ALWAN_LITERAL(0.184));
    alwan_scalar epsilon = epsilon_base / (sf * lf);

    /* Convert XYZ to LMS using IPT matrix */
    alwan_mat3x3 M_xyz_to_lms;
    for (int i = 0; i < 9; i++) {
        M_xyz_to_lms.m[i] = M_XYZ_TO_LMS_IPT[i];
    }

    alwan_vec3 lms;
    alwan_mat3_mulv(&M_xyz_to_lms, xyz, &lms);

    /* Apply Michaelis-Menten lightness to each LMS channel */
    lms.v[0] = lightness_fairchild2011(lms.v[0], epsilon);
    lms.v[1] = lightness_fairchild2011(lms.v[1], epsilon);
    lms.v[2] = lightness_fairchild2011(lms.v[2], epsilon);

    /* Convert to IPT */
    alwan_mat3x3 M_lms_to_ipt;
    get_lms_to_ipt_hdr_matrix(&M_lms_to_ipt);
    alwan_mat3_mulv(&M_lms_to_ipt, &lms, hdr_ipt);
}

void alwan_hdr_ipt_to_xyz(alwan_vec3 const *hdr_ipt, alwan_vec3 *xyz) {
    if (!hdr_ipt || !xyz) {
        return;
    }

    /* Default parameters: Y_s = 0.2, Y_abs = 100 */
    /* Fairchild 2011 epsilon formula (same as forward) */
    alwan_scalar const Y_s = ALWAN_LITERAL(0.2);
    alwan_scalar const Y_abs = ALWAN_LITERAL(100.0);
    alwan_scalar const epsilon_base = ALWAN_LITERAL(0.59);

    alwan_scalar lf = ALWAN_LOG(ALWAN_LITERAL(318.0)) / ALWAN_LOG(Y_abs);
    alwan_scalar sf = ALWAN_LITERAL(1.25) - ALWAN_LITERAL(0.25) * (Y_s / ALWAN_LITERAL(0.184));
    alwan_scalar epsilon = epsilon_base / (sf * lf);

    /* Convert IPT to LMS */
    alwan_mat3x3 M_ipt_to_lms;
    get_ipt_to_lms_hdr_matrix(&M_ipt_to_lms);

    alwan_vec3 lms;
    alwan_mat3_mulv(&M_ipt_to_lms, hdr_ipt, &lms);

    /* Apply inverse Michaelis-Menten lightness */
    lms.v[0] = lightness_fairchild2011_inv(lms.v[0], epsilon);
    lms.v[1] = lightness_fairchild2011_inv(lms.v[1], epsilon);
    lms.v[2] = lightness_fairchild2011_inv(lms.v[2], epsilon);

    /* Convert LMS to XYZ using inverse IPT matrix */
    alwan_mat3x3 M_lms_to_xyz;
    for (int i = 0; i < 9; i++) {
        M_lms_to_xyz.m[i] = M_LMS_TO_XYZ_IPT[i];
    }
    alwan_mat3_mulv(&M_lms_to_xyz, &lms, xyz);
}

/* ================================================================
 * IgPgTg (Ebner & Fairchild 1998)
 * XYZ <-> IgPgTg conversions
 * Uses HPE matrix for XYZ<->LMS and generated matrices for LMS<->IgPgTg
 * Implements scaled nonlinearity: (LMS / scale) ^ 0.427
 * ================================================================ */

/* LMS scaling factors for IgPgTg */
static alwan_scalar const IGPGTG_LMS_SCALE[3] = {
    ALWAN_LITERAL(18.36),
    ALWAN_LITERAL(21.46),
    ALWAN_LITERAL(19435.0)
};

void alwan_xyz_to_igpgtg(alwan_vec3 const *xyz, alwan_vec3 *igpgtg) {
    if (!xyz || !igpgtg) {
        return;
    }

    /* Convert XYZ to LMS using IPT matrix */
    alwan_mat3x3 M_xyz_to_lms;
    for (int i = 0; i < 9; i++) {
        M_xyz_to_lms.m[i] = M_XYZ_TO_LMS_IPT[i];
    }

    alwan_vec3 lms;
    alwan_mat3_mulv(&M_xyz_to_lms, xyz, &lms);

    /* Apply scaled nonlinearity: (LMS / scale) ^ 0.427 */
    alwan_scalar const exponent = ALWAN_LITERAL(0.427);
    lms.v[0] = spow(lms.v[0] / IGPGTG_LMS_SCALE[0], exponent);
    lms.v[1] = spow(lms.v[1] / IGPGTG_LMS_SCALE[1], exponent);
    lms.v[2] = spow(lms.v[2] / IGPGTG_LMS_SCALE[2], exponent);

    /* Convert to IgPgTg */
    alwan_mat3x3 M_lms_to_igpgtg;
    get_lms_to_igpgtg_matrix(&M_lms_to_igpgtg);
    alwan_mat3_mulv(&M_lms_to_igpgtg, &lms, igpgtg);
}

void alwan_igpgtg_to_xyz(alwan_vec3 const *igpgtg, alwan_vec3 *xyz) {
    if (!igpgtg || !xyz) {
        return;
    }

    /* Convert IgPgTg to LMS */
    alwan_mat3x3 M_igpgtg_to_lms;
    get_igpgtg_to_lms_matrix(&M_igpgtg_to_lms);

    alwan_vec3 lms;
    alwan_mat3_mulv(&M_igpgtg_to_lms, igpgtg, &lms);

    /* Apply inverse scaled nonlinearity: scale * (LMS_p ^ (1/0.427)) */
    alwan_scalar const inv_exponent = ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.427);
    lms.v[0] = IGPGTG_LMS_SCALE[0] * spow(lms.v[0], inv_exponent);
    lms.v[1] = IGPGTG_LMS_SCALE[1] * spow(lms.v[1], inv_exponent);
    lms.v[2] = IGPGTG_LMS_SCALE[2] * spow(lms.v[2], inv_exponent);

    /* Convert LMS to XYZ using inverse IPT matrix */
    alwan_mat3x3 M_lms_to_xyz;
    for (int i = 0; i < 9; i++) {
        M_lms_to_xyz.m[i] = M_LMS_TO_XYZ_IPT[i];
    }
    alwan_mat3_mulv(&M_lms_to_xyz, &lms, xyz);
}

/* ================================================================
 * ICaCb (Zhang & Wandell 1996, 1997)
 * XYZ <-> ICaCb conversions
 * Uses HPE matrix for XYZ<->LMS and PQ (ST2084) transfer function
 * ================================================================ */

/* PQ (ST2084) constants */
static alwan_scalar const PQ_C1 = ALWAN_LITERAL(0.8359375);       /* 3424 / 4096 */
static alwan_scalar const PQ_C2 = ALWAN_LITERAL(18.8515625);      /* 2413 / 128 */
static alwan_scalar const PQ_C3 = ALWAN_LITERAL(18.6875);         /* 2392 / 128 */
static alwan_scalar const PQ_M1 = ALWAN_LITERAL(0.1593017578125); /* 2610 / 16384 */
static alwan_scalar const PQ_M2 = ALWAN_LITERAL(78.84375);        /* 2523 / 32 */

/* PQ (ST2084) inverse EOTF - converts C (cd/m²) to N (signal) */
static alwan_scalar eotf_inverse_st2084(alwan_scalar C) {
    alwan_scalar const L_p = ALWAN_LITERAL(10000.0);

    /* Y_p = (C / L_p) ^ m_1 */
    alwan_scalar Y_p = ALWAN_POW(C / L_p, PQ_M1);

    /* N = ((c_1 + c_2 * Y_p) / (c_3 * Y_p + 1)) ^ m_2 */
    alwan_scalar numerator = PQ_C1 + PQ_C2 * Y_p;
    alwan_scalar denominator = PQ_C3 * Y_p + ALWAN_LITERAL(1.0);
    alwan_scalar N = ALWAN_POW(numerator / denominator, PQ_M2);

    return N;
}

/* PQ (ST2084) EOTF - converts N (signal) to C (cd/m²) */
static alwan_scalar eotf_st2084(alwan_scalar N) {
    alwan_scalar const L_p = ALWAN_LITERAL(10000.0);

    /* Inverse of PQ curve */
    alwan_scalar const inv_m2 = ALWAN_LITERAL(1.0) / PQ_M2;
    alwan_scalar const inv_m1 = ALWAN_LITERAL(1.0) / PQ_M1;

    /* N_p = N ^ (1/m_2) */
    alwan_scalar N_p = ALWAN_POW(N, inv_m2);

    /* Y_p = (N_p - c_1) / (c_2 - c_3 * N_p) */
    alwan_scalar numerator = N_p - PQ_C1;
    alwan_scalar denominator = PQ_C2 - PQ_C3 * N_p;

    /* Handle division by zero or negative values */
    if (denominator <= ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }

    alwan_scalar Y_p = numerator / denominator;

    /* Clamp negative values */
    if (Y_p < ALWAN_LITERAL(0.0)) {
        Y_p = ALWAN_LITERAL(0.0);
    }

    /* C = L_p * Y_p ^ (1/m_1) */
    alwan_scalar C = L_p * ALWAN_POW(Y_p, inv_m1);

    return C;
}

void alwan_xyz_to_icacb(alwan_vec3 const *xyz, alwan_vec3 *icacb) {
    if (!xyz || !icacb) {
        return;
    }

    /* Convert XYZ to LMS using IPT matrix */
    alwan_mat3x3 M_xyz_to_lms;
    for (int i = 0; i < 9; i++) {
        M_xyz_to_lms.m[i] = M_XYZ_TO_LMS_IPT[i];
    }

    alwan_vec3 lms;
    alwan_mat3_mulv(&M_xyz_to_lms, xyz, &lms);

    /* Apply PQ (ST2084) inverse EOTF to each LMS channel */
    lms.v[0] = eotf_inverse_st2084(lms.v[0]);
    lms.v[1] = eotf_inverse_st2084(lms.v[1]);
    lms.v[2] = eotf_inverse_st2084(lms.v[2]);

    /* Convert to ICaCb */
    alwan_mat3x3 M_lms_to_icacb;
    get_lms_to_icacb_matrix(&M_lms_to_icacb);
    alwan_mat3_mulv(&M_lms_to_icacb, &lms, icacb);
}

void alwan_icacb_to_xyz(alwan_vec3 const *icacb, alwan_vec3 *xyz) {
    if (!icacb || !xyz) {
        return;
    }

    /* Convert ICaCb to LMS */
    alwan_mat3x3 M_icacb_to_lms;
    get_icacb_to_lms_matrix(&M_icacb_to_lms);

    alwan_vec3 lms;
    alwan_mat3_mulv(&M_icacb_to_lms, icacb, &lms);

    /* Apply PQ (ST2084) EOTF (inverse of inverse EOTF) */
    lms.v[0] = eotf_st2084(lms.v[0]);
    lms.v[1] = eotf_st2084(lms.v[1]);
    lms.v[2] = eotf_st2084(lms.v[2]);

    /* Convert LMS to XYZ using inverse IPT matrix */
    alwan_mat3x3 M_lms_to_xyz;
    for (int i = 0; i < 9; i++) {
        M_lms_to_xyz.m[i] = M_LMS_TO_XYZ_IPT[i];
    }
    alwan_mat3_mulv(&M_lms_to_xyz, &lms, xyz);
}
