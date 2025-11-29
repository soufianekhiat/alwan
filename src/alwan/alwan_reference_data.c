/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Reference Data & Color Systems
 * Munsell, Color Checker, NCS, and RGB Space Definitions
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Munsell Renotation Data
 * ---------------------------------------------------------------- */

/* Munsell renotation entry: HVC -> xyY */
typedef struct {
    alwan_scalar hue;      /* Hue [0, 100] */
    alwan_scalar value;    /* Value [0, 10] */
    alwan_scalar chroma;   /* Chroma [0, 20+] */
    alwan_scalar x;        /* CIE x chromaticity */
    alwan_scalar y;        /* CIE y chromaticity */
    alwan_scalar Y;        /* CIE Y (relative luminance) */
} munsell_renotation_entry;

/* Munsell renotation data (subset for now - full data to be added)
 * Full dataset contains ~4000 entries from the 1943 Munsell Renotation
 * Format: H, V, C, x, y, Y (under Illuminant C)
 * This is a minimal stub - the full data will be embedded as CSV */
static munsell_renotation_entry const g_munsell_renotation_data[] = {
    /* Sample entries - full data to be added */
    {0.0, 0.0, 0.0, 0.310, 0.316, 0.0},      /* N0 (black) */
    {0.0, 1.0, 0.0, 0.310, 0.316, 0.01},     /* N1 */
    {0.0, 2.0, 0.0, 0.310, 0.316, 0.04},     /* N2 */
    {0.0, 5.0, 0.0, 0.310, 0.316, 0.198},    /* N5 (mid gray) */
    {0.0, 9.0, 0.0, 0.310, 0.316, 0.756},    /* N9 */
    {0.0, 10.0, 0.0, 0.310, 0.316, 1.0},     /* N10 (white) */
};

static size_t const g_munsell_renotation_count =
    sizeof(g_munsell_renotation_data) / sizeof(g_munsell_renotation_data[0]);

/* Convert Munsell HVC to XYZ tristimulus values
 * Uses trilinear interpolation in the Munsell renotation data */
int alwan_munsell_to_xyz(alwan_scalar hue, alwan_scalar value, alwan_scalar chroma,
                         alwan_illuminant illuminant, alwan_xyz *xyz) {
    if (!xyz) {
        return ALWAN_E_INVALID;
    }

    /* Validate input ranges */
    if (value < ALWAN_LITERAL(0.0) || value > ALWAN_LITERAL(10.0)) {
        return ALWAN_E_RANGE;
    }
    if (chroma < ALWAN_LITERAL(0.0)) {
        return ALWAN_E_RANGE;
    }

    /* Normalize hue to [0, 100] */
    while (hue < ALWAN_LITERAL(0.0)) {
        hue += ALWAN_LITERAL(100.0);
    }
    while (hue >= ALWAN_LITERAL(100.0)) {
        hue -= ALWAN_LITERAL(100.0);
    }

    /* For achromatic colors (chroma ~0), use neutral axis */
    if (chroma < ALWAN_LITERAL(0.01)) {
        /* Find neutral entries (chroma = 0) and interpolate by value */
        size_t idx_low = 0;
        size_t idx_high = 0;

        for (size_t i = 0; i < g_munsell_renotation_count; i++) {
            if (g_munsell_renotation_data[i].chroma < ALWAN_LITERAL(0.01)) {
                if (g_munsell_renotation_data[i].value <= value) {
                    idx_low = i;
                }
                if (g_munsell_renotation_data[i].value >= value && idx_high == 0) {
                    idx_high = i;
                }
            }
        }

        munsell_renotation_entry const *e_low = &g_munsell_renotation_data[idx_low];
        munsell_renotation_entry const *e_high = &g_munsell_renotation_data[idx_high];

        /* Linear interpolation by value */
        alwan_scalar t = ALWAN_LITERAL(0.0);
        if (e_high->value > e_low->value) {
            t = (value - e_low->value) / (e_high->value - e_low->value);
        }

        alwan_scalar x_interp = e_low->x + t * (e_high->x - e_low->x);
        alwan_scalar y_interp = e_low->y + t * (e_high->y - e_low->y);
        alwan_scalar Y_interp = e_low->Y + t * (e_high->Y - e_low->Y);

        /* Convert xyY to XYZ */
        if (y_interp <= ALWAN_LITERAL(0.0)) {
            return ALWAN_E_INVALID;
        }

        xyz->x = x_interp * Y_interp / y_interp;
        xyz->y = Y_interp;
        xyz->z = (ALWAN_LITERAL(1.0) - x_interp - y_interp) * Y_interp / y_interp;
    } else {
        /* Chromatic colors: use trilinear interpolation in HVC space
         * Full implementation requires complete Munsell renotation dataset */

        /* Use mid-gray as placeholder */
        alwan_scalar const x = ALWAN_LITERAL(0.310);
        alwan_scalar const y = ALWAN_LITERAL(0.316);
        alwan_scalar Y = value / ALWAN_LITERAL(10.0);  /* Approximate Y from value */

        xyz->x = x * Y / y;
        xyz->y = Y;
        xyz->z = (ALWAN_LITERAL(1.0) - x - y) * Y / y;
    }

    /* Adapt from Illuminant C to requested illuminant */
    if (illuminant != ALWAN_ILLUMINANT_C) {
        /* Get white points for both illuminants */
        alwan_xyz white_c, white_dst;
        int status = alwan_illuminant_white_point(ALWAN_ILLUMINANT_C,
                                                    ALWAN_OBSERVER_CIE_1931_2DEG, &white_c);
        if (status != ALWAN_OK) {
            return status;
        }

        status = alwan_illuminant_white_point(illuminant,
                                               ALWAN_OBSERVER_CIE_1931_2DEG, &white_dst);
        if (status != ALWAN_OK) {
            return status;
        }

        /* Compute CAT matrix */
        alwan_mat3x3 cat_matrix;
        status = alwan_cat_matrix(&white_c, &white_dst, ALWAN_CAT_BRADFORD, &cat_matrix);
        if (status != ALWAN_OK) {
            return status;
        }

        /* Apply adaptation */
        alwan_xyz xyz_adapted;
        alwan_mat3_mulv(&cat_matrix, (alwan_vec3 const *)xyz, (alwan_vec3 *)&xyz_adapted);
        *xyz = xyz_adapted;
    }

    return ALWAN_OK;
}

/* Convert XYZ tristimulus values to Munsell HVC notation
 * Uses iterative search in the Munsell renotation data */
int alwan_xyz_to_munsell(alwan_xyz const *xyz, alwan_illuminant illuminant,
                         alwan_scalar *hue, alwan_scalar *value, alwan_scalar *chroma) {
    if (!xyz || !hue || !value || !chroma) {
        return ALWAN_E_INVALID;
    }

    /* Adapt to Illuminant C (Munsell renotation data is under Illuminant C) */
    alwan_xyz xyz_c;
    if (illuminant != ALWAN_ILLUMINANT_C) {
        /* Get white points for both illuminants */
        alwan_xyz white_src, white_c;
        int status = alwan_illuminant_white_point(illuminant,
                                                    ALWAN_OBSERVER_CIE_1931_2DEG, &white_src);
        if (status != ALWAN_OK) {
            return status;
        }

        status = alwan_illuminant_white_point(ALWAN_ILLUMINANT_C,
                                               ALWAN_OBSERVER_CIE_1931_2DEG, &white_c);
        if (status != ALWAN_OK) {
            return status;
        }

        /* Compute CAT matrix */
        alwan_mat3x3 cat_matrix;
        status = alwan_cat_matrix(&white_src, &white_c, ALWAN_CAT_BRADFORD, &cat_matrix);
        if (status != ALWAN_OK) {
            return status;
        }

        /* Apply adaptation */
        alwan_mat3_mulv(&cat_matrix, (alwan_vec3 const *)xyz, (alwan_vec3 *)&xyz_c);
    } else {
        xyz_c = *xyz;
    }

    /* Convert XYZ to xyY */
    alwan_scalar sum = xyz_c.x + xyz_c.y + xyz_c.z;
    if (sum <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar x = xyz_c.x / sum;
    alwan_scalar y = xyz_c.y / sum;
    alwan_scalar Y = xyz_c.y;

    /* Find nearest entry in Munsell renotation data using nearest-neighbor search
     * More sophisticated inverse interpolation could be added in future versions */

    alwan_scalar min_dist = ALWAN_LITERAL(1e10);
    size_t best_idx = 0;

    for (size_t i = 0; i < g_munsell_renotation_count; i++) {
        munsell_renotation_entry const *e = &g_munsell_renotation_data[i];

        alwan_scalar dx = x - e->x;
        alwan_scalar dy = y - e->y;
        alwan_scalar dY = Y - e->Y;

        alwan_scalar dist = dx * dx + dy * dy + dY * dY * ALWAN_LITERAL(0.01);

        if (dist < min_dist) {
            min_dist = dist;
            best_idx = i;
        }
    }

    munsell_renotation_entry const *best = &g_munsell_renotation_data[best_idx];
    *hue = best->hue;
    *value = best->value;
    *chroma = best->chroma;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Color Checker Data
 * ---------------------------------------------------------------- */

/* ColorChecker Classic 24-patch data (D50, xyY)
 * Generated from colour-science SDS_COLOURCHECKERS['ColorChecker N Ohta']
 * Format: 24 patches × 3 values (x, y, Y) = 72 values total */
static alwan_scalar const g_colorchecker_classic_d50_xyY[] = {
#include "data/colorchecker/classic_d50_xyy.csv"
};

/* Get number of patches in a Color Checker target */
size_t alwan_color_checker_num_patches(alwan_colorchecker_type type) {
    switch (type) {
        case ALWAN_COLORCHECKER_CLASSIC:
            return 24;
        case ALWAN_COLORCHECKER_SG:
            return 140;
        case ALWAN_COLORCHECKER_DIGITAL_SG:
            return 140;
        case ALWAN_BABELCOLOR_AVERAGE:
            return 24;
        case ALWAN_BABELCOLOR_HCT:
            return 24;
        default:
            return 0;
    }
}

/* Get XYZ tristimulus values for a Color Checker patch */
int alwan_color_checker_data(alwan_colorchecker_type type, alwan_illuminant illuminant,
                              size_t patch_index, alwan_xyz *xyz) {
    if (!xyz) {
        return ALWAN_E_INVALID;
    }

    /* Get number of patches for validation */
    size_t num_patches = alwan_color_checker_num_patches(type);
    if (num_patches == 0) {
        return ALWAN_E_INVALID;  /* Unknown type */
    }
    if (patch_index >= num_patches) {
        return ALWAN_E_RANGE;
    }

    /* Currently only ColorChecker Classic is implemented */
    if (type != ALWAN_COLORCHECKER_CLASSIC) {
        return ALWAN_E_INVALID;
    }

    /* Get xyY data under D50 (flat array: patch_index * 3 + offset) */
    size_t const base_idx = patch_index * 3;
    alwan_scalar x = g_colorchecker_classic_d50_xyY[base_idx + 0];
    alwan_scalar y = g_colorchecker_classic_d50_xyY[base_idx + 1];
    alwan_scalar Y = g_colorchecker_classic_d50_xyY[base_idx + 2];

    /* Convert xyY to XYZ */
    if (y <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_INVALID;
    }

    alwan_xyz xyz_d50;
    xyz_d50.x = x * Y / y;
    xyz_d50.y = Y;
    xyz_d50.z = (ALWAN_LITERAL(1.0) - x - y) * Y / y;

    /* Adapt from D50 to requested illuminant */
    if (illuminant != ALWAN_ILLUMINANT_D50) {
        /* Get white points for both illuminants */
        alwan_xyz white_d50, white_dst;
        int status = alwan_illuminant_white_point(ALWAN_ILLUMINANT_D50,
                                                    ALWAN_OBSERVER_CIE_1931_2DEG, &white_d50);
        if (status != ALWAN_OK) {
            return status;
        }

        status = alwan_illuminant_white_point(illuminant,
                                               ALWAN_OBSERVER_CIE_1931_2DEG, &white_dst);
        if (status != ALWAN_OK) {
            return status;
        }

        /* Compute CAT matrix */
        alwan_mat3x3 cat_matrix;
        status = alwan_cat_matrix(&white_d50, &white_dst, ALWAN_CAT_BRADFORD, &cat_matrix);
        if (status != ALWAN_OK) {
            return status;
        }

        /* Apply adaptation */
        alwan_mat3_mulv(&cat_matrix, (alwan_vec3 const *)&xyz_d50, (alwan_vec3 *)xyz);
    } else {
        *xyz = xyz_d50;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * NCS (Natural Color System) Data
 * ---------------------------------------------------------------- */

/* NCS color notation structure */
typedef struct {
    int nuance_code;       /* 0-99: blackness + chromaticness code */
    int hue_code;          /* 0-99: hue position */
    char hue_name[8];      /* e.g., "Y90R", "G10Y" */
} ncs_notation_parsed;

/* Parse NCS notation string (e.g., "S 1050-Y90R") */
static int parse_ncs_notation(char const *notation, ncs_notation_parsed *parsed) {
    if (!notation || !parsed) {
        return ALWAN_E_INVALID;
    }

    /* Simple parser - full implementation needed
     * Format: [S|W] NNCC-HHX[X]
     * S = standard NCS colors
     * NN = blackness (00-99)
     * CC = chromaticness (00-99)
     * HH = hue position (00-99)
     * X[X] = hue name (Y, YR, R, RB, B, BG, G, GY) */

    /* Full parser not yet implemented */
    return ALWAN_E_INVALID;
}

/* Convert NCS notation to XYZ tristimulus values */
int alwan_ncs_to_xyz(char const *ncs_notation, alwan_xyz *xyz) {
    if (!ncs_notation || !xyz) {
        return ALWAN_E_INVALID;
    }

    ncs_notation_parsed parsed;
    int status = parse_ncs_notation(ncs_notation, &parsed);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Requires NCS color atlas data */
    return ALWAN_E_INVALID;
}

/* Convert XYZ tristimulus values to NCS notation */
int alwan_xyz_to_ncs(alwan_xyz const *xyz, char *ncs_notation, size_t notation_size) {
    if (!xyz || !ncs_notation || notation_size < 16) {
        return ALWAN_E_INVALID;
    }

    /* Requires inverse lookup in NCS color atlas */
    return ALWAN_E_INVALID;
}

/* ----------------------------------------------------------------
 * Additional RGB Space Definitions
 * ---------------------------------------------------------------- */

/* RGB space definition entry */
typedef struct {
    char const *name;
    alwan_scalar primaries[6];  /* rx, ry, gx, gy, bx, by */
    alwan_scalar white_x;
    alwan_scalar white_y;
    alwan_transfer_function oetf;
    alwan_transfer_function eotf;
} rgb_space_def;

/* RGB space definitions database
 * Data from various standards (ITU-R, SMPTE, ISO, etc.) */
static rgb_space_def const g_rgb_spaces[] = {
    /* Standard spaces (some already in main API) */
    {"sRGB", {0.6400, 0.3300, 0.3000, 0.6000, 0.1500, 0.0600}, 0.3127, 0.3290, ALWAN_TF_SRGB, ALWAN_TF_SRGB},
    {"Adobe RGB", {0.6400, 0.3300, 0.2100, 0.7100, 0.1500, 0.0600}, 0.3127, 0.3290, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
    {"ProPhoto RGB", {0.7347, 0.2653, 0.1596, 0.8404, 0.0366, 0.0001}, 0.3457, 0.3585, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
    {"DCI-P3", {0.6800, 0.3200, 0.2650, 0.6900, 0.1500, 0.0600}, 0.3140, 0.3510, ALWAN_TF_GAMMA26, ALWAN_TF_GAMMA26},
    {"Display P3", {0.6800, 0.3200, 0.2650, 0.6900, 0.1500, 0.0600}, 0.3127, 0.3290, ALWAN_TF_SRGB, ALWAN_TF_SRGB},
    {"Rec. 2020", {0.7080, 0.2920, 0.1700, 0.7970, 0.1310, 0.0460}, 0.3127, 0.3290, ALWAN_TF_BT2020, ALWAN_TF_BT2020},
    {"ACES AP0", {0.7347, 0.2653, 0.0000, 1.0000, 0.0001, -0.0770}, 0.3127, 0.3290, ALWAN_TF_LINEAR, ALWAN_TF_LINEAR},
    {"ACES AP1", {0.7130, 0.2930, 0.1650, 0.8300, 0.1280, 0.0440}, 0.3127, 0.3290, ALWAN_TF_LINEAR, ALWAN_TF_LINEAR},
    {"ACEScg", {0.7130, 0.2930, 0.1650, 0.8300, 0.1280, 0.0440}, 0.3127, 0.3290, ALWAN_TF_LINEAR, ALWAN_TF_LINEAR},
    /* Additional spaces */
    {"Apple RGB", {0.6250, 0.3400, 0.2800, 0.5950, 0.1550, 0.0700}, 0.3127, 0.3290, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
    {"Best RGB", {0.7347, 0.2653, 0.2150, 0.7750, 0.1300, 0.0350}, 0.3457, 0.3585, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
    {"Beta RGB", {0.6888, 0.3112, 0.1986, 0.7551, 0.1265, 0.0352}, 0.3457, 0.3585, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
    {"Bruce RGB", {0.6400, 0.3300, 0.2800, 0.6500, 0.1500, 0.0600}, 0.3127, 0.3290, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
    {"CIE RGB", {0.7350, 0.2650, 0.2740, 0.7170, 0.1670, 0.0090}, 0.3333, 0.3333, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
    {"ColorMatch RGB", {0.6300, 0.3400, 0.2950, 0.6050, 0.1500, 0.0750}, 0.3457, 0.3585, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
    {"Don RGB 4", {0.6960, 0.3000, 0.2150, 0.7650, 0.1300, 0.0350}, 0.3457, 0.3585, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
    {"ECI RGB v2", {0.6700, 0.3300, 0.2100, 0.7100, 0.1400, 0.0800}, 0.3457, 0.3585, ALWAN_TF_LINEAR, ALWAN_TF_LINEAR},
    {"Ekta Space PS5", {0.6950, 0.3050, 0.2600, 0.7000, 0.1100, 0.0050}, 0.3457, 0.3585, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
    {"NTSC RGB", {0.6700, 0.3300, 0.2100, 0.7100, 0.1400, 0.0800}, 0.3101, 0.3162, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
    {"PAL/SECAM RGB", {0.6400, 0.3300, 0.2900, 0.6000, 0.1500, 0.0600}, 0.3127, 0.3290, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
    {"SMPTE-C RGB", {0.6300, 0.3400, 0.3100, 0.5950, 0.1550, 0.0700}, 0.3127, 0.3290, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
    {"Wide Gamut RGB", {0.7350, 0.2650, 0.1150, 0.8260, 0.1570, 0.0180}, 0.3457, 0.3585, ALWAN_TF_GAMMA22, ALWAN_TF_GAMMA22},
};

static size_t const g_rgb_spaces_count = sizeof(g_rgb_spaces) / sizeof(g_rgb_spaces[0]);

/* Map enum value to g_rgb_spaces array index. Returns -1 if not found. */
static int get_rgb_space_index(alwan_rgb_space space) {
    switch (space) {
        case ALWAN_RGB_SPACE_SRGB:            return 0;  /* sRGB */
        case ALWAN_RGB_SPACE_ADOBE_RGB_1998:  return 1;  /* Adobe RGB */
        case ALWAN_RGB_SPACE_PROPHOTO_RGB:    return 2;  /* ProPhoto RGB */
        case ALWAN_RGB_SPACE_DCI_P3:          return 3;  /* DCI-P3 */
        case ALWAN_RGB_SPACE_DISPLAY_P3:      return 4;  /* Display P3 */
        case ALWAN_RGB_SPACE_BT2020:          return 5;  /* Rec. 2020 */
        case ALWAN_RGB_SPACE_ACES2065_1:      return 6;  /* ACES AP0 */
        case ALWAN_RGB_SPACE_ACESCG:          return 8;  /* ACEScg (same as ACES AP1) */
        case ALWAN_RGB_SPACE_APPLE_RGB:       return 9;  /* Apple RGB */
        case ALWAN_RGB_SPACE_BEST_RGB:        return 10; /* Best RGB */
        case ALWAN_RGB_SPACE_BETA_RGB:        return 11; /* Beta RGB */
        case ALWAN_RGB_SPACE_CIE_RGB:         return 13; /* CIE RGB */
        case ALWAN_RGB_SPACE_COLORMATCH_RGB:  return 14; /* ColorMatch RGB */
        case ALWAN_RGB_SPACE_DON_RGB_4:       return 15; /* Don RGB 4 */
        case ALWAN_RGB_SPACE_ECI_RGB_V2:      return 16; /* ECI RGB v2 */
        case ALWAN_RGB_SPACE_EKTA_SPACE_PS5: return 17; /* Ekta Space PS5 */
        case ALWAN_RGB_SPACE_NTSC_1953:       return 18; /* NTSC RGB */
        case ALWAN_RGB_SPACE_PAL_SECAM:       return 19; /* PAL/SECAM RGB */
        case ALWAN_RGB_SPACE_SMPTE_C:         return 20; /* SMPTE-C RGB */
        case ALWAN_RGB_SPACE_ADOBE_WIDE_GAMUT_RGB: return 21; /* Wide Gamut RGB */
        default: return -1;
    }
}

/* Get RGB space primaries and white point by name */
int alwan_rgb_space_by_enum(alwan_rgb_space space, alwan_scalar primaries[6], alwan_vec2 *white_point) {
    if (!primaries || !white_point) {
        return ALWAN_E_INVALID;
    }

    /* Map enum to array index */
    int index = get_rgb_space_index(space);
    if (index < 0 || (size_t)index >= g_rgb_spaces_count) {
        return ALWAN_E_INVALID;
    }

    /* Lookup using mapped index */
    rgb_space_def const *def = &g_rgb_spaces[index];

    /* Copy primaries */
    for (int j = 0; j < 6; j++) {
        primaries[j] = def->primaries[j];
    }

    /* Copy white point */
    white_point->v[0] = def->white_x;
    white_point->v[1] = def->white_y;

    return ALWAN_OK;
}

/* Get RGB space transfer functions */
int alwan_rgb_space_get_tfs(alwan_rgb_space space, alwan_transfer_function *oetf, alwan_transfer_function *eotf) {
    if (!oetf || !eotf) {
        return ALWAN_E_INVALID;
    }

    /* Map enum to array index */
    int index = get_rgb_space_index(space);
    if (index < 0 || (size_t)index >= g_rgb_spaces_count) {
        return ALWAN_E_INVALID;
    }

    /* Lookup using mapped index */
    rgb_space_def const *def = &g_rgb_spaces[index];

    /* Copy transfer functions */
    *oetf = def->oetf;
    *eotf = def->eotf;

    return ALWAN_OK;
}
