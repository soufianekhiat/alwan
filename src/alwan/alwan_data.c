/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Data loading: embedded vs runtime
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if ALWAN_EMBED_DATA

/* ----------------------------------------------------------------
 * Embedded data mode: compile-time data inclusion
 * ---------------------------------------------------------------- */

/* Disable float conversion warnings for embedded CSV data */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* Illuminant A (x, y) */
static alwan_scalar const g_a_xy[] = {
#include "data/illuminants_xy/a_xy.csv"
};

/* Illuminant D50 (x, y) */
static alwan_scalar const g_d50_xy[] = {
#include "data/illuminants_xy/d50_xy.csv"
};

/* Illuminant D55 (x, y) */
static alwan_scalar const g_d55_xy[] = {
#include "data/illuminants_xy/d55_xy.csv"
};

/* Illuminant D60 (x, y) */
static alwan_scalar const g_d60_xy[] = {
#include "data/illuminants_xy/d60_xy.csv"
};

/* Illuminant D65 (x, y) */
static alwan_scalar const g_d65_xy[] = {
#include "data/illuminants_xy/d65_xy.csv"
};

/* Illuminant E (x, y) */
static alwan_scalar const g_e_xy[] = {
#include "data/illuminants_xy/e_xy.csv"
};

/* Illuminant B (x, y) */
static alwan_scalar const g_b_xy[] = {
#include "data/illuminants_xy/b_xy.csv"
};

/* Illuminant C (x, y) */
static alwan_scalar const g_c_xy[] = {
#include "data/illuminants_xy/c_xy.csv"
};

/* Illuminant D75 (x, y) */
static alwan_scalar const g_d75_xy[] = {
#include "data/illuminants_xy/d75_xy.csv"
};

/* Additional D-series illuminants */
static alwan_scalar const g_d40_xy[] = {
#include "data/illuminants_xy/d40_xy.csv"
};

static alwan_scalar const g_d45_xy[] = {
#include "data/illuminants_xy/d45_xy.csv"
};

static alwan_scalar const g_d93_xy[] = {
#include "data/illuminants_xy/d93_xy.csv"
};

/* LED illuminants */
static alwan_scalar const g_led_b1_xy[] = {
#include "data/illuminants_xy/led-b1_xy.csv"
};

static alwan_scalar const g_led_b2_xy[] = {
#include "data/illuminants_xy/led-b2_xy.csv"
};

static alwan_scalar const g_led_b3_xy[] = {
#include "data/illuminants_xy/led-b3_xy.csv"
};

static alwan_scalar const g_led_b4_xy[] = {
#include "data/illuminants_xy/led-b4_xy.csv"
};

static alwan_scalar const g_led_b5_xy[] = {
#include "data/illuminants_xy/led-b5_xy.csv"
};

static alwan_scalar const g_led_bh1_xy[] = {
#include "data/illuminants_xy/led-bh1_xy.csv"
};

static alwan_scalar const g_led_rgb1_xy[] = {
#include "data/illuminants_xy/led-rgb1_xy.csv"
};

static alwan_scalar const g_led_v1_xy[] = {
#include "data/illuminants_xy/led-v1_xy.csv"
};

static alwan_scalar const g_led_v2_xy[] = {
#include "data/illuminants_xy/led-v2_xy.csv"
};

/* High Pressure illuminants */
static alwan_scalar const g_hp1_xy[] = {
#include "data/illuminants_xy/hp1_xy.csv"
};

static alwan_scalar const g_hp2_xy[] = {
#include "data/illuminants_xy/hp2_xy.csv"
};

static alwan_scalar const g_hp3_xy[] = {
#include "data/illuminants_xy/hp3_xy.csv"
};

static alwan_scalar const g_hp4_xy[] = {
#include "data/illuminants_xy/hp4_xy.csv"
};

static alwan_scalar const g_hp5_xy[] = {
#include "data/illuminants_xy/hp5_xy.csv"
};

/* sRGB primaries (rx, ry, gx, gy, bx, by) */
static alwan_scalar const g_srgb_primaries_3x2[] = {
#include "data/srgb_primaries_3x2.csv"
};

/* ----------------------------------------------------------------
 * CAT Matrices (Chromatic Adaptation Transform)
 * ---------------------------------------------------------------- */

/* Bradford CAT matrix (most common, used in ICC profiles) */
alwan_scalar const g_cat_bradford[9] = {
#include "data/matrices/cat_bradford.csv"
};

/* CAT02 matrix (from CIECAM02) */
alwan_scalar const g_cat_cat02[9] = {
#include "data/matrices/cat_cat02.csv"
};

/* CAT16 matrix (from CAM16) */
alwan_scalar const g_cat_cat16[9] = {
#include "data/matrices/cat_cat16.csv"
};

/* Sharp CAT matrix */
alwan_scalar const g_cat_sharp[9] = {
#include "data/matrices/cat_sharp.csv"
};

/* Fairchild 1990 CAT matrix */
alwan_scalar const g_cat_fairchild[9] = {
#include "data/matrices/cat_fairchild.csv"
};

/* CMCCAT97 matrix */
alwan_scalar const g_cat_cmccat97[9] = {
#include "data/matrices/cat_cmccat97.csv"
};

/* CMCCAT2000 matrix */
alwan_scalar const g_cat_cmccat2000[9] = {
#include "data/matrices/cat_cmccat2000.csv"
};

/* CAT02 Brill 2008 variant matrix */
alwan_scalar const g_cat_cat02_brill_2008[9] = {
#include "data/matrices/cat_cat02_brill_2008.csv"
};

/* Bianco 2010 CAT matrix */
alwan_scalar const g_cat_bianco_2010[9] = {
#include "data/matrices/cat_bianco_2010.csv"
};

/* Bianco PC 2010 CAT matrix */
alwan_scalar const g_cat_bianco_pc_2010[9] = {
#include "data/matrices/cat_bianco_pc_2010.csv"
};

/* ----------------------------------------------------------------
 * CAM Matrices (Color Appearance Models)
 * ---------------------------------------------------------------- */

/* Hunt-Pointer-Estevez matrix (XYZ to LMS cone response) */
alwan_scalar const g_hpe[9] = {
#include "data/matrices/hpe.csv"
};

/* Inverse Hunt-Pointer-Estevez matrix (LMS to XYZ) */
alwan_scalar const g_hpe_inv[9] = {
#include "data/matrices/hpe_inv.csv"
};

/* ----------------------------------------------------------------
 * ICtCp Matrices (ITU-R BT.2100 HDR)
 * ---------------------------------------------------------------- */

/* BT.2020 RGB to LMS cone response matrix */
alwan_scalar const g_ictcp_rgb_to_lms[9] = {
#include "data/ictcp_rgb_to_lms.csv"
};

/* LMS to BT.2020 RGB inverse matrix */
alwan_scalar const g_ictcp_lms_to_rgb[9] = {
#include "data/ictcp_lms_to_rgb.csv"
};

/* LMS' to ICtCp matrix for PQ */
alwan_scalar const g_ictcp_lms_p_to_ictcp_pq[9] = {
#include "data/ictcp_lms_p_to_ictcp_pq.csv"
};

/* ICtCp to LMS' inverse matrix for PQ */
alwan_scalar const g_ictcp_ictcp_to_lms_p_pq[9] = {
#include "data/ictcp_ictcp_to_lms_p_pq.csv"
};

/* LMS' to ICtCp matrix for HLG */
alwan_scalar const g_ictcp_lms_p_to_ictcp_hlg[9] = {
#include "data/ictcp_lms_p_to_ictcp_hlg.csv"
};

/* ICtCp to LMS' inverse matrix for HLG */
alwan_scalar const g_ictcp_ictcp_to_lms_p_hlg[9] = {
#include "data/ictcp_ictcp_to_lms_p_hlg.csv"
};

/* XYZ (D65) to BT.2020 RGB matrix */
alwan_scalar const g_ictcp_xyz_to_bt2020[9] = {
#include "data/ictcp_xyz_to_bt2020.csv"
};

/* BT.2020 RGB to XYZ (D65) matrix */
alwan_scalar const g_ictcp_bt2020_to_xyz[9] = {
#include "data/ictcp_bt2020_to_xyz.csv"
};

/* ----------------------------------------------------------------
 * IPT Matrices and Constants (Ebner & Fairchild 1998)
 * ---------------------------------------------------------------- */

/* IPT nonlinearity exponent */
alwan_scalar const g_ipt_exponent = {
#include "data/ipt_exponent.csv"
};

/* XYZ (D65) to LMS matrix for IPT */
alwan_scalar const g_ipt_xyz_to_lms[9] = {
#include "data/ipt_xyz_to_lms.csv"
};

/* LMS to XYZ inverse matrix for IPT */
alwan_scalar const g_ipt_lms_to_xyz[9] = {
#include "data/ipt_lms_to_xyz.csv"
};

/* LMS' to IPT matrix */
alwan_scalar const g_ipt_lms_p_to_ipt[9] = {
#include "data/ipt_lms_p_to_ipt.csv"
};

/* IPT to LMS' inverse matrix */
alwan_scalar const g_ipt_ipt_to_lms_p[9] = {
#include "data/ipt_ipt_to_lms_p.csv"
};

ALWAN_DIAG_POP

int alwan_data_get_illuminant_a(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;  /* Unused in embedded mode */
    *data = (alwan_scalar *)g_a_xy;
    *count = sizeof(g_a_xy) / sizeof(g_a_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d50(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_d50_xy;
    *count = sizeof(g_d50_xy) / sizeof(g_d50_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d55(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_d55_xy;
    *count = sizeof(g_d55_xy) / sizeof(g_d55_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d60(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_d60_xy;
    *count = sizeof(g_d60_xy) / sizeof(g_d60_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d65(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_d65_xy;
    *count = sizeof(g_d65_xy) / sizeof(g_d65_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_e(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_e_xy;
    *count = sizeof(g_e_xy) / sizeof(g_e_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_b(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_b_xy;
    *count = sizeof(g_b_xy) / sizeof(g_b_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_c(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_c_xy;
    *count = sizeof(g_c_xy) / sizeof(g_c_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d75(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_d75_xy;
    *count = sizeof(g_d75_xy) / sizeof(g_d75_xy[0]);
    return ALWAN_OK;
}

/* Additional D-series illuminants */
int alwan_data_get_illuminant_d40(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_d40_xy;
    *count = sizeof(g_d40_xy) / sizeof(g_d40_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d45(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_d45_xy;
    *count = sizeof(g_d45_xy) / sizeof(g_d45_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d93(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_d93_xy;
    *count = sizeof(g_d93_xy) / sizeof(g_d93_xy[0]);
    return ALWAN_OK;
}

/* LED illuminants */
int alwan_data_get_illuminant_led_b1(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_led_b1_xy;
    *count = sizeof(g_led_b1_xy) / sizeof(g_led_b1_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_b2(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_led_b2_xy;
    *count = sizeof(g_led_b2_xy) / sizeof(g_led_b2_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_b3(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_led_b3_xy;
    *count = sizeof(g_led_b3_xy) / sizeof(g_led_b3_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_b4(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_led_b4_xy;
    *count = sizeof(g_led_b4_xy) / sizeof(g_led_b4_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_b5(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_led_b5_xy;
    *count = sizeof(g_led_b5_xy) / sizeof(g_led_b5_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_bh1(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_led_bh1_xy;
    *count = sizeof(g_led_bh1_xy) / sizeof(g_led_bh1_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_rgb1(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_led_rgb1_xy;
    *count = sizeof(g_led_rgb1_xy) / sizeof(g_led_rgb1_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_v1(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_led_v1_xy;
    *count = sizeof(g_led_v1_xy) / sizeof(g_led_v1_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_v2(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_led_v2_xy;
    *count = sizeof(g_led_v2_xy) / sizeof(g_led_v2_xy[0]);
    return ALWAN_OK;
}

/* High Pressure illuminants */
int alwan_data_get_illuminant_hp1(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_hp1_xy;
    *count = sizeof(g_hp1_xy) / sizeof(g_hp1_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_hp2(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_hp2_xy;
    *count = sizeof(g_hp2_xy) / sizeof(g_hp2_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_hp3(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_hp3_xy;
    *count = sizeof(g_hp3_xy) / sizeof(g_hp3_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_hp4(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_hp4_xy;
    *count = sizeof(g_hp4_xy) / sizeof(g_hp4_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_hp5(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_hp5_xy;
    *count = sizeof(g_hp5_xy) / sizeof(g_hp5_xy[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_xy(alwan_scalar **data, size_t *count,
                                   alwan_ctx *ctx, alwan_illuminant illuminant) {
    switch (illuminant) {
        case ALWAN_ILLUMINANT_A:   return alwan_data_get_illuminant_a(data, count, ctx);
        case ALWAN_ILLUMINANT_D50: return alwan_data_get_illuminant_d50(data, count, ctx);
        case ALWAN_ILLUMINANT_D55: return alwan_data_get_illuminant_d55(data, count, ctx);
        case ALWAN_ILLUMINANT_D60: return alwan_data_get_illuminant_d60(data, count, ctx);
        case ALWAN_ILLUMINANT_D65: return alwan_data_get_illuminant_d65(data, count, ctx);
        case ALWAN_ILLUMINANT_E:   return alwan_data_get_illuminant_e(data, count, ctx);
        case ALWAN_ILLUMINANT_B:   return alwan_data_get_illuminant_b(data, count, ctx);
        case ALWAN_ILLUMINANT_C:   return alwan_data_get_illuminant_c(data, count, ctx);
        case ALWAN_ILLUMINANT_D75: return alwan_data_get_illuminant_d75(data, count, ctx);
        /* Additional illuminants */
        case ALWAN_ILLUMINANT_D40: return alwan_data_get_illuminant_d40(data, count, ctx);
        case ALWAN_ILLUMINANT_D45: return alwan_data_get_illuminant_d45(data, count, ctx);
        case ALWAN_ILLUMINANT_D93: return alwan_data_get_illuminant_d93(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_B1: return alwan_data_get_illuminant_led_b1(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_B2: return alwan_data_get_illuminant_led_b2(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_B3: return alwan_data_get_illuminant_led_b3(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_B4: return alwan_data_get_illuminant_led_b4(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_B5: return alwan_data_get_illuminant_led_b5(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_BH1: return alwan_data_get_illuminant_led_bh1(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_RGB1: return alwan_data_get_illuminant_led_rgb1(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_V1: return alwan_data_get_illuminant_led_v1(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_V2: return alwan_data_get_illuminant_led_v2(data, count, ctx);
        case ALWAN_ILLUMINANT_HP1: return alwan_data_get_illuminant_hp1(data, count, ctx);
        case ALWAN_ILLUMINANT_HP2: return alwan_data_get_illuminant_hp2(data, count, ctx);
        case ALWAN_ILLUMINANT_HP3: return alwan_data_get_illuminant_hp3(data, count, ctx);
        case ALWAN_ILLUMINANT_HP4: return alwan_data_get_illuminant_hp4(data, count, ctx);
        case ALWAN_ILLUMINANT_HP5: return alwan_data_get_illuminant_hp5(data, count, ctx);
        default:
            return ALWAN_E_INVALID;  /* Unsupported illuminant or no xy data */
    }
}

int alwan_data_get_srgb_primaries(alwan_scalar **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_scalar *)g_srgb_primaries_3x2;
    *count = sizeof(g_srgb_primaries_3x2) / sizeof(g_srgb_primaries_3x2[0]);
    return ALWAN_OK;
}

#else
    #error "Only ALWAN_EMBED_DATA mode is supported. Runtime CSV loading has been removed."
#endif /* ALWAN_EMBED_DATA */

/* ----------------------------------------------------------------
 * Illuminant White Point Calculation (works in both modes)
 * ---------------------------------------------------------------- */

int alwan_illuminant_white_point(alwan_xyz *out_xyz,
                                   alwan_illuminant illuminant,
                                   alwan_observer_type observer) {
    if (!out_xyz) {
        return ALWAN_E_INVALID;
    }

    /* For CIE 1931 2° observer, use pre-computed xy chromaticity values for efficiency */
    if (observer == ALWAN_OBSERVER_CIE_1931_2DEG) {
        /* Get xy chromaticity data for the illuminant */
        alwan_scalar *xy_data = NULL;
        size_t count = 0;
        int status = alwan_data_get_illuminant_xy(&xy_data, &count, NULL, illuminant);

        if (status != ALWAN_OK || count < 2) {
            return ALWAN_E_INVALID;
        }

        /* Extract x and y chromaticity coordinates */
        alwan_scalar x = xy_data[0];
        alwan_scalar y = xy_data[1];

        /* Convert xy to XYZ with Y = 1.0 (normalized)
         * Formula: X = x * Y / y
         *          Y = 1.0
         *          Z = (1 - x - y) * Y / y */
        alwan_scalar const Y = ALWAN_LITERAL(1.0);

        if (y <= ALWAN_LITERAL(0.0)) {
            return ALWAN_E_INVALID;  /* Invalid chromaticity */
        }

        out_xyz->x = x * Y / y;                    /* X */
        out_xyz->y = Y;                             /* Y */
        out_xyz->z = (ALWAN_LITERAL(1.0) - x - y) * Y / y;  /* Z */

        return ALWAN_OK;
    }

    /* For other observers, compute from illuminant SPD + observer CMF integration */
    alwan_spd illum_spd;
    int status = alwan_spd_illuminant(&illum_spd, NULL, illuminant);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Integrate illuminant SPD with observer CMFs to get XYZ */
    alwan_xyz xyz_unnormalized;
    status = alwan_xyz_from_spd(&xyz_unnormalized, NULL, &illum_spd, NULL, observer,
                                ALWAN_INTEGRATE_SIMPSON, ALWAN_LITERAL(0.0));

    alwan_spd_destroy(NULL, &illum_spd);

    if (status != ALWAN_OK) {
        return status;
    }

    /* Normalize to Y = 1.0 */
    if (xyz_unnormalized.y <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_INVALID;  /* Invalid Y value */
    }

    alwan_scalar norm_factor = ALWAN_LITERAL(1.0) / xyz_unnormalized.y;
    out_xyz->x = xyz_unnormalized.x * norm_factor;
    out_xyz->y = ALWAN_LITERAL(1.0);
    out_xyz->z = xyz_unnormalized.z * norm_factor;

    return ALWAN_OK;
}
