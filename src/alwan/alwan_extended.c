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
 * Matrix Loading Functions
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

/* IMPORTANT: Each color space uses its OWN XYZ<->LMS matrix! */

/* XYZ to LMS matrix for hdr-IPT
 * This is MATRIX_IPT_XYZ_TO_LMS from colour-science (NOT the same as standard HPE!)
 * Generated from colour-science */
static alwan_scalar const M_XYZ_TO_LMS_IPT[9] = {
#include "data/matrices/xyz_to_lms_ipt.csv"
};

/* LMS to XYZ inverse matrix for hdr-IPT
 * This is MATRIX_IPT_LMS_TO_XYZ from colour-science
 * Generated from colour-science */
static alwan_scalar const M_LMS_TO_XYZ_IPT[9] = {
#include "data/matrices/lms_to_xyz_ipt.csv"
};

/* XYZ to LMS matrix for IgPgTg (DIFFERENT from hdr-IPT!)
 * This is MATRIX_IGPGTG_XYZ_TO_LMS from colour-science
 * Generated from colour-science */
static alwan_scalar const M_XYZ_TO_LMS_IGPGTG[9] = {
#include "data/matrices/xyz_to_lms_igpgtg.csv"
};

/* LMS to XYZ inverse matrix for IgPgTg
 * This is MATRIX_IGPGTG_LMS_TO_XYZ from colour-science
 * Generated from colour-science */
static alwan_scalar const M_LMS_TO_XYZ_IGPGTG[9] = {
#include "data/matrices/lms_to_xyz_igpgtg.csv"
};

/* XYZ to LMS matrix for ICaCb (DIFFERENT from hdr-IPT and IgPgTg!)
 * This is MATRIX_ICACB_XYZ_TO_LMS from colour-science
 * Generated from colour-science */
static alwan_scalar const M_XYZ_TO_LMS_ICACB[9] = {
#include "data/matrices/xyz_to_lms_icacb.csv"
};

/* LMS to XYZ inverse matrix for ICaCb
 * This is MATRIX_ICACB_LMS_TO_XYZ from colour-science
 * Generated from colour-science */
static alwan_scalar const M_LMS_TO_XYZ_ICACB[9] = {
#include "data/matrices/lms_to_xyz_icacb.csv"
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
     * Represents color as [L, P, Q] where:
     * - L = max(R,G,B) is the lightness
     * - P, Q are normalized color components (P = R/sum, Q = G/sum)
     * - R_component = B/sum = 1 - P - Q (not stored, can be computed)
     * Reference: colour.RGB_to_Prismatic
     */

    /* L is the maximum value */
    alwan_scalar L = alwan_max3(r, g, b);

    /* Normalize RGB to sum to 1 */
    alwan_scalar sum_rgb = r + g + b;
    alwan_scalar P, Q;

    if (sum_rgb < ALWAN_EPSILON) {
        /* Black point */
        P = ALWAN_LITERAL(0.0);
        Q = ALWAN_LITERAL(0.0);
    } else {
        P = r / sum_rgb;  /* Normalized R */
        Q = g / sum_rgb;  /* Normalized G */
        /* R_component = b / sum_rgb = 1 - P - Q */
    }

    prismatic->v[0] = L;
    prismatic->v[1] = P;
    prismatic->v[2] = Q;
}

void alwan_prismatic_to_rgb(alwan_vec3 const *prismatic, alwan_vec3 *rgb) {
    if (!prismatic || !rgb) {
        return;
    }

    alwan_scalar L = prismatic->v[0];
    alwan_scalar P = prismatic->v[1];
    alwan_scalar Q = prismatic->v[2];

    /* Convert back from Prismatic [L, P, Q] to RGB
     * R_component = 1 - P - Q (the normalized B value)
     * sum_rgb = L / max(P, Q, R_component)
     * RGB = [P, Q, R_component] * sum_rgb
     * Reference: colour.Prismatic_to_RGB
     */

    alwan_scalar R_comp = ALWAN_LITERAL(1.0) - P - Q;

    /* Find max of [P, Q, R_comp] */
    alwan_scalar max_pqr = alwan_max3(P, Q, R_comp);

    alwan_scalar sum_rgb;
    if (max_pqr < ALWAN_EPSILON) {
        /* Black point */
        sum_rgb = ALWAN_LITERAL(0.0);
    } else {
        sum_rgb = L / max_pqr;
    }

    /* Denormalize to get RGB */
    rgb->v[0] = P * sum_rgb;
    rgb->v[1] = Q * sum_rgb;
    rgb->v[2] = R_comp * sum_rgb;
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
     * Using the corrected formulas from Sarifuddin 2021
     */

    alwan_scalar const gamma = ALWAN_LITERAL(3.0);
    alwan_scalar const Y_0 = ALWAN_LITERAL(100.0);

    alwan_scalar max_val = alwan_max3(r, g, b);
    alwan_scalar min_val = alwan_min3(r, g, b);

    /* Q factor */
    alwan_scalar Q = ALWAN_LITERAL(1.0);
    if (max_val > ALWAN_EPSILON) {
        Q = ALWAN_EXP((min_val * gamma) / (max_val * Y_0));
    }

    /* Luminance */
    alwan_scalar L = (Q * max_val + (Q - ALWAN_LITERAL(1.0)) * min_val) / ALWAN_LITERAL(2.0);

    /* Chroma */
    alwan_scalar r_g = r - g;
    alwan_scalar g_b = g - b;
    alwan_scalar b_r = b - r;
    alwan_scalar C = Q * (ALWAN_FABS(r_g) + ALWAN_FABS(g_b) + ALWAN_FABS(b_r)) / ALWAN_LITERAL(3.0);

    /* Hue - use atan(G_B / R_G), not atan2! */
    alwan_scalar H = ALWAN_LITERAL(0.0);
    if (C > ALWAN_EPSILON) {
        alwan_scalar h_temp = ALWAN_ATAN(g_b / r_g);  /* arctan gives [-π/2, π/2] */
        alwan_scalar two_h_3 = ALWAN_LITERAL(2.0) * h_temp / ALWAN_LITERAL(3.0);
        alwan_scalar four_h_3 = ALWAN_LITERAL(4.0) * h_temp / ALWAN_LITERAL(3.0);

        /* Select H based on R-G and G-B signs */
        if (r_g >= ALWAN_LITERAL(0.0) && g_b >= ALWAN_LITERAL(0.0)) {
            H = two_h_3;
        } else if (r_g >= ALWAN_LITERAL(0.0) && g_b < ALWAN_LITERAL(0.0)) {
            H = four_h_3;
        } else if (r_g < ALWAN_LITERAL(0.0) && g_b >= ALWAN_LITERAL(0.0)) {
            H = ALWAN_PI + four_h_3;
        } else {
            H = two_h_3 - ALWAN_PI;
        }
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

    /* Convert HCL to RGB (Sarifuddin & Missaoui 2005, corrected 2021) */
    alwan_scalar const gamma = ALWAN_LITERAL(3.0);
    alwan_scalar const Y_0 = ALWAN_LITERAL(100.0);

    /* Compute Q, Min, Max */
    alwan_scalar Q = ALWAN_LITERAL(1.0);
    alwan_scalar Min = ALWAN_LITERAL(0.0);
    alwan_scalar Max = ALWAN_LITERAL(0.0);

    if (L > ALWAN_EPSILON) {
        Q = ALWAN_EXP((ALWAN_LITERAL(1.0) - (ALWAN_LITERAL(3.0) * C) / (ALWAN_LITERAL(4.0) * L)) * gamma / Y_0);
        alwan_scalar denom = ALWAN_LITERAL(4.0) * Q - ALWAN_LITERAL(2.0);
        if (ALWAN_FABS(denom) > ALWAN_EPSILON) {
            Min = (ALWAN_LITERAL(4.0) * L - ALWAN_LITERAL(3.0) * C) / denom;
        }
        if (Q > ALWAN_EPSILON) {
            Max = Min + (ALWAN_LITERAL(3.0) * C) / (ALWAN_LITERAL(2.0) * Q);
        }
    }

    /* Hue-based RGB computation */
    alwan_scalar r, g, b;
    alwan_scalar const r_p60 = ALWAN_PI / ALWAN_LITERAL(3.0);  /* 60 degrees */
    alwan_scalar const r_p120 = ALWAN_LITERAL(2.0) * ALWAN_PI / ALWAN_LITERAL(3.0);  /* 120 degrees */
    alwan_scalar const r_n60 = -ALWAN_PI / ALWAN_LITERAL(3.0);  /* -60 degrees */
    alwan_scalar const r_n120 = -ALWAN_LITERAL(2.0) * ALWAN_PI / ALWAN_LITERAL(3.0);  /* -120 degrees */

    if (H >= ALWAN_LITERAL(0.0) && H < r_p60) {
        /* 0° to 60° */
        alwan_scalar tan_val = ALWAN_TAN(ALWAN_LITERAL(3.0) * H / ALWAN_LITERAL(2.0));
        r = Max;
        g = (Max * tan_val + Min) / (ALWAN_LITERAL(1.0) + tan_val);
        b = Min;
    } else if (H >= r_p60 && H < r_p120) {
        /* 60° to 120° */
        alwan_scalar tan_val = ALWAN_TAN(ALWAN_LITERAL(3.0) * (H - ALWAN_PI) / ALWAN_LITERAL(4.0));
        if (ALWAN_FABS(tan_val) > ALWAN_EPSILON) {
            r = (Max * (ALWAN_LITERAL(1.0) + tan_val) - Min) / tan_val;
        } else {
            r = Max;
        }
        g = Max;
        b = Min;
    } else if (H >= r_p120 && H <= ALWAN_PI) {
        /* 120° to 180° */
        alwan_scalar tan_val = ALWAN_TAN(ALWAN_LITERAL(3.0) * (H - ALWAN_PI) / ALWAN_LITERAL(4.0));
        r = Min;
        g = Max;
        b = Max * (ALWAN_LITERAL(1.0) + tan_val) - Min * tan_val;
    } else if (H >= r_n60 && H < ALWAN_LITERAL(0.0)) {
        /* -60° to 0° */
        alwan_scalar tan_val = ALWAN_TAN(ALWAN_LITERAL(3.0) * H / ALWAN_LITERAL(4.0));
        r = Max;
        g = Min;
        b = Min * (ALWAN_LITERAL(1.0) + tan_val) - Max * tan_val;
    } else if (H >= r_n120 && H < r_n60) {
        /* -120° to -60° */
        alwan_scalar tan_val = ALWAN_TAN(ALWAN_LITERAL(3.0) * H / ALWAN_LITERAL(4.0));
        if (ALWAN_FABS(tan_val) > ALWAN_EPSILON) {
            r = (Min * (ALWAN_LITERAL(1.0) + tan_val) - Max) / tan_val;
        } else {
            r = Min;
        }
        g = Min;
        b = Max;
    } else {
        /* -180° to -120° */
        alwan_scalar tan_val = ALWAN_TAN(ALWAN_LITERAL(3.0) * (H + ALWAN_PI) / ALWAN_LITERAL(2.0));
        r = Min;
        g = (Min * tan_val + Max) / (ALWAN_LITERAL(1.0) + tan_val);
        b = Max;
    }

    rgb->v[0] = r;
    rgb->v[1] = g;
    rgb->v[2] = b;
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

    /* Transform RGB to YC₁C₂ using MATRIX_RGB_TO_YC_1_C_2
     * Matrix:
     *   [  0.2126     0.7152     0.0722  ]
     *   [  1.0      -0.5       -0.5     ]
     *   [  0.0      -√3/2       √3/2    ]
     * where √3/2 = 0.8660254037844386
     */
    alwan_scalar Y = ALWAN_LITERAL(0.2126) * r + ALWAN_LITERAL(0.7152) * g + ALWAN_LITERAL(0.0722) * b;
    alwan_scalar C_1 = r - ALWAN_LITERAL(0.5) * g - ALWAN_LITERAL(0.5) * b;
    alwan_scalar C_2 = -ALWAN_LITERAL(0.8660254037844386) * g + ALWAN_LITERAL(0.8660254037844386) * b;

    /* Compute C = √(C₁² + C₂²) */
    alwan_scalar C = ALWAN_SQRT(C_1 * C_1 + C_2 * C_2);

    /* Compute hue H using arccos(C₁/C) */
    alwan_scalar H = ALWAN_LITERAL(0.0);
    if (C > ALWAN_EPSILON) {
        alwan_scalar C_1_C = C_1 / C;
        /* Clamp to [-1, 1] to avoid numerical errors in arccos */
        if (C_1_C > ALWAN_LITERAL(1.0)) C_1_C = ALWAN_LITERAL(1.0);
        if (C_1_C < ALWAN_LITERAL(-1.0)) C_1_C = ALWAN_LITERAL(-1.0);

        alwan_scalar H_temp = ALWAN_ACOS(C_1_C);

        /* Adjust based on C₂ sign */
        if (C_2 <= ALWAN_LITERAL(0.0)) {
            H = H_temp;
        } else {
            H = ALWAN_LITERAL(2.0) * ALWAN_PI - H_temp;
        }
    }

    /* Saturation (simple chroma) */
    alwan_scalar S = delta;

    /* IHLS format is [H, Y, S] where Y is luminance-weighted intensity */
    ihls->v[0] = H;
    ihls->v[1] = Y;
    ihls->v[2] = S;
}

void alwan_ihls_to_rgb(alwan_vec3 const *ihls, alwan_vec3 *rgb) {
    if (!ihls || !rgb) {
        return;
    }

    /* IHLS format is [H, Y, S] */
    alwan_scalar H = ihls->v[0];
    alwan_scalar Y = ihls->v[1];
    alwan_scalar S = ihls->v[2];

    /* Convert IHLS to RGB via YC₁C₂
     * First compute C from S using: C = (√3 * S) / (2 * sin(2π/3 - H_s))
     * where H_s = H - floor(H/(π/3)) * (π/3)
     */
    alwan_scalar const pi_3 = ALWAN_PI / ALWAN_LITERAL(3.0);
    alwan_scalar const two_pi_3 = ALWAN_LITERAL(2.0) * ALWAN_PI / ALWAN_LITERAL(3.0);

    alwan_scalar k = ALWAN_FLOOR(H / pi_3);
    alwan_scalar H_s = H - k * pi_3;
    alwan_scalar C = (ALWAN_SQRT(ALWAN_LITERAL(3.0)) * S) / (ALWAN_LITERAL(2.0) * ALWAN_SIN(two_pi_3 - H_s));

    /* Compute C₁ and C₂ from C and H */
    alwan_scalar C_1 = C * ALWAN_COS(H);
    alwan_scalar C_2 = -C * ALWAN_SIN(H);

    /* Transform [Y, C₁, C₂] to RGB using MATRIX_YC_1_C_2_TO_RGB:
     *   [[ 1.0         0.787399999999999878      0.371236223088929396]
     *    [ 1.0        -0.212600000000000039     -0.206114046100696419]
     *    [ 1.0        -0.212600000000000011      0.948586492278555182]]
     */
    rgb->v[0] = Y + ALWAN_LITERAL(0.787399999999999878) * C_1 + ALWAN_LITERAL(0.371236223088929396) * C_2;
    rgb->v[1] = Y - ALWAN_LITERAL(0.212600000000000039) * C_1 - ALWAN_LITERAL(0.206114046100696419) * C_2;
    rgb->v[2] = Y - ALWAN_LITERAL(0.212600000000000011) * C_1 + ALWAN_LITERAL(0.948586492278555182) * C_2;
}

/* ================================================================
 * hdr-CIELAB (Fairchild & Wyble 2010)
 * XYZ <-> hdr-CIELAB conversions
 * ================================================================ */

/* D65 white point for HDR calculations (Y=1 scale, matches colour-science) */
static alwan_vec3 const HDR_D65_WHITE = {
    { ALWAN_LITERAL(0.95045593), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.08905775) }
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

static alwan_scalar hdr_lightness_fairchild2011(alwan_scalar Y, alwan_scalar epsilon) {
    /* Fairchild 2011 lightness function using Michaelis-Menten kinetics
     * L_hdr = (V_max * Y^epsilon) / (K_m + Y^epsilon) + 0.02
     * where V_max = 247, K_m = 2^epsilon
     */
    alwan_scalar const V_max = ALWAN_LITERAL(247.0);
    alwan_scalar Y_eps = ALWAN_POW(Y, epsilon);
    alwan_scalar K_m = ALWAN_POW(ALWAN_LITERAL(2.0), epsilon);
    alwan_scalar L_hdr = (V_max * Y_eps) / (K_m + Y_eps) + ALWAN_LITERAL(0.02);
    return L_hdr;
}

void alwan_xyz_to_hdr_cielab(alwan_vec3 const *xyz, alwan_vec3 *hdr_lab) {
    if (!xyz || !hdr_lab) {
        return;
    }

    /* hdr-CIELAB Fairchild 2011 parameters (defaults: Y_s=0.2, Y_abs=100) */
    alwan_scalar const Y_s = ALWAN_LITERAL(0.2);
    alwan_scalar const Y_abs = ALWAN_LITERAL(100.0);

    /* Compute epsilon exponent */
    alwan_scalar epsilon = ALWAN_LITERAL(0.58);
    alwan_scalar sf = ALWAN_LITERAL(1.25) - ALWAN_LITERAL(0.25) * (Y_s / ALWAN_LITERAL(0.184));
    alwan_scalar lf = ALWAN_LOG(ALWAN_LITERAL(318.0)) / ALWAN_LOG(Y_abs);
    epsilon /= sf * lf;

    /* Normalize by D65 white point */
    alwan_scalar xr = xyz->v[0] / HDR_D65_WHITE.v[0];
    alwan_scalar yr = xyz->v[1] / HDR_D65_WHITE.v[1];
    alwan_scalar zr = xyz->v[2] / HDR_D65_WHITE.v[2];

    /* Apply Fairchild 2011 lightness function */
    alwan_scalar L_hdr = hdr_lightness_fairchild2011(yr, epsilon);
    alwan_scalar fx = hdr_lightness_fairchild2011(xr, epsilon);
    alwan_scalar fz = hdr_lightness_fairchild2011(zr, epsilon);

    /* Calculate L*, a*, b* */
    hdr_lab->v[0] = L_hdr;  /* L* */
    hdr_lab->v[1] = ALWAN_LITERAL(5.0) * (fx - L_hdr);  /* a* */
    hdr_lab->v[2] = ALWAN_LITERAL(2.0) * (L_hdr - fz);  /* b* */
}

static alwan_scalar hdr_luminance_fairchild2011(alwan_scalar L_hdr, alwan_scalar epsilon) {
    /* Inverse Fairchild 2011 lightness function
     * S = ((L_hdr - 0.02) * K_m) / (V_max - (L_hdr - 0.02))
     * Y = S^(1/epsilon)
     * where V_max = 247, K_m = 2^epsilon
     */
    alwan_scalar const V_max = ALWAN_LITERAL(247.0);
    alwan_scalar K_m = ALWAN_POW(ALWAN_LITERAL(2.0), epsilon);
    alwan_scalar v = L_hdr - ALWAN_LITERAL(0.02);

    /* Avoid division by zero */
    if (ALWAN_FABS(V_max - v) < ALWAN_EPSILON) {
        return ALWAN_LITERAL(1.0);
    }

    alwan_scalar S = (v * K_m) / (V_max - v);
    alwan_scalar Y = ALWAN_POW(S, ALWAN_LITERAL(1.0) / epsilon);
    return Y;
}

void alwan_hdr_cielab_to_xyz(alwan_vec3 const *hdr_lab, alwan_vec3 *xyz) {
    if (!hdr_lab || !xyz) {
        return;
    }

    /* hdr-CIELAB Fairchild 2011 parameters (defaults: Y_s=0.2, Y_abs=100) */
    alwan_scalar const Y_s = ALWAN_LITERAL(0.2);
    alwan_scalar const Y_abs = ALWAN_LITERAL(100.0);

    /* Compute epsilon exponent */
    alwan_scalar epsilon = ALWAN_LITERAL(0.58);
    alwan_scalar sf = ALWAN_LITERAL(1.25) - ALWAN_LITERAL(0.25) * (Y_s / ALWAN_LITERAL(0.184));
    alwan_scalar lf = ALWAN_LOG(ALWAN_LITERAL(318.0)) / ALWAN_LOG(Y_abs);
    epsilon /= sf * lf;

    alwan_scalar L = hdr_lab->v[0];
    alwan_scalar a = hdr_lab->v[1];
    alwan_scalar b = hdr_lab->v[2];

    /* Calculate luminance values using inverse Fairchild 2011 */
    alwan_scalar yr = hdr_luminance_fairchild2011(L, epsilon);
    alwan_scalar xr = hdr_luminance_fairchild2011((a + ALWAN_LITERAL(5.0) * L) / ALWAN_LITERAL(5.0), epsilon);
    alwan_scalar zr = hdr_luminance_fairchild2011((-b + ALWAN_LITERAL(2.0) * L) / ALWAN_LITERAL(2.0), epsilon);

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
    /* XYZ_to_hdr_IPT uses hdr-CIELAB method by default, which uses V_max=247 */
    /* V_max = 247 for hdr-CIELAB (default in colour-science), K_m = 2^epsilon */
    alwan_scalar const V_max = ALWAN_LITERAL(247.0);
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
    alwan_scalar const V_max = ALWAN_LITERAL(247.0);
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

    /* Convert XYZ to LMS using IgPgTg-specific matrix (NOT the IPT matrix!) */
    alwan_mat3x3 M_xyz_to_lms;
    for (int i = 0; i < 9; i++) {
        M_xyz_to_lms.m[i] = M_XYZ_TO_LMS_IGPGTG[i];
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

    /* Convert LMS to XYZ using IgPgTg-specific inverse matrix */
    alwan_mat3x3 M_lms_to_xyz;
    for (int i = 0; i < 9; i++) {
        M_lms_to_xyz.m[i] = M_LMS_TO_XYZ_IGPGTG[i];
    }
    alwan_mat3_mulv(&M_lms_to_xyz, &lms, xyz);
}

/* ================================================================
 * ICaCb (Zhang & Wandell 1996, 1997)
 * XYZ <-> ICaCb conversions
 * Uses ICaCb-specific XYZ<->LMS matrix and PQ (ST2084) transfer function
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

    /* Convert XYZ to LMS using ICaCb-specific matrix (NOT the IPT matrix!) */
    alwan_mat3x3 M_xyz_to_lms;
    for (int i = 0; i < 9; i++) {
        M_xyz_to_lms.m[i] = M_XYZ_TO_LMS_ICACB[i];
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

    /* Convert LMS to XYZ using ICaCb-specific inverse matrix */
    alwan_mat3x3 M_lms_to_xyz;
    for (int i = 0; i < 9; i++) {
        M_lms_to_xyz.m[i] = M_LMS_TO_XYZ_ICACB[i];
    }
    alwan_mat3_mulv(&M_lms_to_xyz, &lms, xyz);
}
