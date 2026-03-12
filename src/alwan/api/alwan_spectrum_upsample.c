/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Spectral Upsampling - RGB to Spectrum Conversion
 * Implements reflectance recovery methods based on colour-science
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>
#include <math.h>

/* ----------------------------------------------------------------
 * Smits1999 Basis Spectra Data
 * Based on: Smits, Brian. "An RGB to Spectrum Conversion for Reflectances" (1999)
 * White point: CIE Illuminant E (equal energy)
 * RGB colorspace: sRGB primaries
 * Wavelength range: 380-720nm, 10 samples
 * ---------------------------------------------------------------- */

#define SMITS1999_WAVELENGTH_COUNT 10
#define SMITS1999_WAVELENGTH_MIN ALWAN_LITERAL(380.0)
#define SMITS1999_WAVELENGTH_MAX ALWAN_LITERAL(720.0)

/* Wavelengths (shared across all basis spectra) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const smits1999_wavelengths[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/wavelengths.csv"
};
ALWAN_DIAG_POP

/* Basis spectra: white, cyan, magenta, yellow, red, green, blue */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const smits1999_white[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/white.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const smits1999_cyan[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/cyan.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const smits1999_magenta[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/magenta.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const smits1999_yellow[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/yellow.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const smits1999_red[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/red.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const smits1999_green[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/green.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const smits1999_blue[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/blue.csv"
};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Mallett2019 Basis Functions Data
 * Based on: Mallett & Yuksel. "Spectral Primary Decomposition for Rendering with sRGB Reflectance" (2019)
 * RGB colorspace: sRGB
 * Wavelength range: 380-780nm, 81 samples (5nm intervals)
 * ---------------------------------------------------------------- */

#define MALLETT2019_WAVELENGTH_COUNT 81
#define MALLETT2019_WAVELENGTH_MIN ALWAN_LITERAL(380.0)
#define MALLETT2019_WAVELENGTH_MAX ALWAN_LITERAL(780.0)

/* Wavelengths (shared across all basis functions) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const mallett2019_wavelengths[MALLETT2019_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/mallett2019/wavelengths.csv"
};
ALWAN_DIAG_POP

/* Basis functions: red, green, blue */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const mallett2019_red[MALLETT2019_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/mallett2019/red.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const mallett2019_green[MALLETT2019_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/mallett2019/green.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const mallett2019_blue[MALLETT2019_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/mallett2019/blue.csv"
};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Jakob2019 Polynomial LUT Data
 * Based on: Jakob & Hanika. "A Low-Dimensional Function Space for Efficient Spectral Upsampling" (2019)
 * RGB colorspace: sRGB
 * Wavelength range: 360-780nm, 85 samples (5nm intervals)
 * LUT resolution: 16x16x16 (4,096 entries)
 * ---------------------------------------------------------------- */

#define JAKOB2019_WAVELENGTH_COUNT 85
#define JAKOB2019_WAVELENGTH_MIN ALWAN_LITERAL(360.0)
#define JAKOB2019_WAVELENGTH_MAX ALWAN_LITERAL(780.0)

/* Jakob2019 polynomial coefficient LUT (64x64x64 resolution for sRGB) */
#define JAKOB2019_LUT_RES 64
#define JAKOB2019_LUT_SIZE (JAKOB2019_LUT_RES * JAKOB2019_LUT_RES * JAKOB2019_LUT_RES)

/* sRGB LUT (default) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_srgb_lut_c0[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_srgb_lut_c1[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_srgb_lut_c2[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2.csv"
};
ALWAN_DIAG_POP

/* ProPhoto RGB LUT */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_prophoto_lut_c0[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_prophotorgb.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_prophoto_lut_c1[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_prophotorgb.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_prophoto_lut_c2[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_prophotorgb.csv"
};
ALWAN_DIAG_POP

/* ACES2065-1 LUT */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_aces_lut_c0[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_aces2065_1.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_aces_lut_c1[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_aces2065_1.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_aces_lut_c2[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_aces2065_1.csv"
};
ALWAN_DIAG_POP

/* Rec.2020 LUT */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_rec2020_lut_c0[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_rec2020.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_rec2020_lut_c1[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_rec2020.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_rec2020_lut_c2[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_rec2020.csv"
};
ALWAN_DIAG_POP

/* eRGB LUT */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_ergb_lut_c0[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_ergb.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_ergb_lut_c1[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_ergb.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_ergb_lut_c2[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_ergb.csv"
};
ALWAN_DIAG_POP

/* CIE XYZ LUT */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_xyz_lut_c0[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_xyz.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_xyz_lut_c1[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_xyz.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const jakob2019_xyz_lut_c2[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_xyz.csv"
};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Helper Functions
 * ---------------------------------------------------------------- */

/* Clamp scalar value to [min, max] range */
static inline alwan_scalar clamp(alwan_scalar x, alwan_scalar min, alwan_scalar max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

/* ----------------------------------------------------------------
 * Smits1999: RGB to Spectrum Conversion
 * Algorithm:
 * 1. Find minimum RGB component
 * 2. Start with white spectrum scaled by minimum
 * 3. Add appropriate complementary colors based on remaining components
 * ---------------------------------------------------------------- */

int alwan_rgb_to_spectrum_smits1999(alwan_spd *out_spd,
                                     alwan_ctx *ctx,
                                     alwan_rgb const *rgb) {
    if (!rgb || !out_spd) {
        return ALWAN_E_INVALID;
    }

    /* Create output SPD with Smits1999 wavelength range */
    int status = alwan_spd_create(out_spd,
                                   ctx,
                                   SMITS1999_WAVELENGTH_MIN,
                                   SMITS1999_WAVELENGTH_MAX,
                                   SMITS1999_WAVELENGTH_COUNT);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Extract RGB components (clamped to [0, 1]) */
    alwan_scalar r = clamp(rgb->r, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    alwan_scalar g = clamp(rgb->g, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    alwan_scalar b = clamp(rgb->b, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

    /* Smits1999 algorithm: build spectrum from basis spectra */
    /* Based on colour-science implementation */

    if (r <= g && r <= b) {
        /* R is minimum: start with white * R */
        for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
            out_spd->values[i] = smits1999_white[i] * r;
        }

        if (g <= b) {
            /* Add cyan for (G - R) */
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += smits1999_cyan[i] * (g - r);
            }
            /* Add blue for (B - G) */
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += smits1999_blue[i] * (b - g);
            }
        } else {
            /* Add cyan for (B - R) */
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += smits1999_cyan[i] * (b - r);
            }
            /* Add green for (G - B) */
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += smits1999_green[i] * (g - b);
            }
        }
    } else if (g <= r && g <= b) {
        /* G is minimum: start with white * G */
        for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
            out_spd->values[i] = smits1999_white[i] * g;
        }

        if (r <= b) {
            /* Add magenta for (R - G) */
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += smits1999_magenta[i] * (r - g);
            }
            /* Add blue for (B - R) */
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += smits1999_blue[i] * (b - r);
            }
        } else {
            /* Add magenta for (B - G) */
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += smits1999_magenta[i] * (b - g);
            }
            /* Add red for (R - B) */
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += smits1999_red[i] * (r - b);
            }
        }
    } else {
        /* B is minimum: start with white * B */
        for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
            out_spd->values[i] = smits1999_white[i] * b;
        }

        if (r <= g) {
            /* Add yellow for (R - B) */
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += smits1999_yellow[i] * (r - b);
            }
            /* Add green for (G - R) */
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += smits1999_green[i] * (g - r);
            }
        } else {
            /* Add yellow for (G - B) */
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += smits1999_yellow[i] * (g - b);
            }
            /* Add red for (R - G) */
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += smits1999_red[i] * (r - g);
            }
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Mallett2019: RGB to Spectrum Conversion
 * Algorithm: Linear combination of basis functions
 * recovered_spectrum = R * basis_red + G * basis_green + B * basis_blue
 * ---------------------------------------------------------------- */

int alwan_rgb_to_spectrum_mallett2019(alwan_spd *out_spd,
                                       alwan_ctx *ctx,
                                       alwan_rgb const *rgb) {
    if (!rgb || !out_spd) {
        return ALWAN_E_INVALID;
    }

    /* Create output SPD with Mallett2019 wavelength range */
    int status = alwan_spd_create(out_spd,
                                   ctx,
                                   MALLETT2019_WAVELENGTH_MIN,
                                   MALLETT2019_WAVELENGTH_MAX,
                                   MALLETT2019_WAVELENGTH_COUNT);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Extract RGB components */
    alwan_scalar r = rgb->r;
    alwan_scalar g = rgb->g;
    alwan_scalar b = rgb->b;

    /* Linear combination: spectrum = R * basis_red + G * basis_green + B * basis_blue */
    for (size_t i = 0; i < MALLETT2019_WAVELENGTH_COUNT; i++) {
        out_spd->values[i] = r * mallett2019_red[i] +
                             g * mallett2019_green[i] +
                             b * mallett2019_blue[i];
    }

    /* Optional: clamp negative values to zero for physical validity */
    for (size_t i = 0; i < MALLETT2019_WAVELENGTH_COUNT; i++) {
        if (out_spd->values[i] < ALWAN_LITERAL(0.0)) {
            out_spd->values[i] = ALWAN_LITERAL(0.0);
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Jakob2019: RGB to Spectrum Conversion
 * Uses polynomial LUT-based approach from the original paper
 * ---------------------------------------------------------------- */

/* Trilinear interpolation of Jakob2019 LUT */
static void jakob2019_lut_sample(alwan_scalar const *lut_c0,
                                  alwan_scalar const *lut_c1,
                                  alwan_scalar const *lut_c2,
                                  alwan_scalar r, alwan_scalar g, alwan_scalar b,
                                  alwan_scalar *out_c0,
                                  alwan_scalar *out_c1,
                                  alwan_scalar *out_c2) {
    /* Clamp inputs to [0, 1] */
    r = clamp(r, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    g = clamp(g, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    b = clamp(b, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

    /* Scale to LUT resolution */
    alwan_scalar const res_f = (alwan_scalar)(JAKOB2019_LUT_RES - 1);
    alwan_scalar const rf = r * res_f;
    alwan_scalar const gf = g * res_f;
    alwan_scalar const bf = b * res_f;

    /* Integer indices */
    int const r0 = (int)rf;
    int const g0 = (int)gf;
    int const b0 = (int)bf;

    /* Clamp to valid range */
    int const r1 = (r0 + 1 < JAKOB2019_LUT_RES) ? (r0 + 1) : r0;
    int const g1 = (g0 + 1 < JAKOB2019_LUT_RES) ? (g0 + 1) : g0;
    int const b1 = (b0 + 1 < JAKOB2019_LUT_RES) ? (b0 + 1) : b0;

    /* Fractional parts */
    alwan_scalar const fr = rf - (alwan_scalar)r0;
    alwan_scalar const fg = gf - (alwan_scalar)g0;
    alwan_scalar const fb = bf - (alwan_scalar)b0;

    /* LUT indexing: index = r * RES^2 + g * RES + b */
#define LUT_INDEX(ri, gi, bi) ((ri) * JAKOB2019_LUT_RES * JAKOB2019_LUT_RES + (gi) * JAKOB2019_LUT_RES + (bi))

    /* Sample 8 corners of the cube */
    int const i000 = LUT_INDEX(r0, g0, b0);
    int const i001 = LUT_INDEX(r0, g0, b1);
    int const i010 = LUT_INDEX(r0, g1, b0);
    int const i011 = LUT_INDEX(r0, g1, b1);
    int const i100 = LUT_INDEX(r1, g0, b0);
    int const i101 = LUT_INDEX(r1, g0, b1);
    int const i110 = LUT_INDEX(r1, g1, b0);
    int const i111 = LUT_INDEX(r1, g1, b1);

#undef LUT_INDEX

    /* Trilinear interpolation for c0 */
    alwan_scalar const c0_00 = lut_c0[i000] * (ALWAN_LITERAL(1.0) - fb) + lut_c0[i001] * fb;
    alwan_scalar const c0_01 = lut_c0[i010] * (ALWAN_LITERAL(1.0) - fb) + lut_c0[i011] * fb;
    alwan_scalar const c0_10 = lut_c0[i100] * (ALWAN_LITERAL(1.0) - fb) + lut_c0[i101] * fb;
    alwan_scalar const c0_11 = lut_c0[i110] * (ALWAN_LITERAL(1.0) - fb) + lut_c0[i111] * fb;
    alwan_scalar const c0_0 = c0_00 * (ALWAN_LITERAL(1.0) - fg) + c0_01 * fg;
    alwan_scalar const c0_1 = c0_10 * (ALWAN_LITERAL(1.0) - fg) + c0_11 * fg;
    *out_c0 = c0_0 * (ALWAN_LITERAL(1.0) - fr) + c0_1 * fr;

    /* Trilinear interpolation for c1 */
    alwan_scalar const c1_00 = lut_c1[i000] * (ALWAN_LITERAL(1.0) - fb) + lut_c1[i001] * fb;
    alwan_scalar const c1_01 = lut_c1[i010] * (ALWAN_LITERAL(1.0) - fb) + lut_c1[i011] * fb;
    alwan_scalar const c1_10 = lut_c1[i100] * (ALWAN_LITERAL(1.0) - fb) + lut_c1[i101] * fb;
    alwan_scalar const c1_11 = lut_c1[i110] * (ALWAN_LITERAL(1.0) - fb) + lut_c1[i111] * fb;
    alwan_scalar const c1_0 = c1_00 * (ALWAN_LITERAL(1.0) - fg) + c1_01 * fg;
    alwan_scalar const c1_1 = c1_10 * (ALWAN_LITERAL(1.0) - fg) + c1_11 * fg;
    *out_c1 = c1_0 * (ALWAN_LITERAL(1.0) - fr) + c1_1 * fr;

    /* Trilinear interpolation for c2 */
    alwan_scalar const c2_00 = lut_c2[i000] * (ALWAN_LITERAL(1.0) - fb) + lut_c2[i001] * fb;
    alwan_scalar const c2_01 = lut_c2[i010] * (ALWAN_LITERAL(1.0) - fb) + lut_c2[i011] * fb;
    alwan_scalar const c2_10 = lut_c2[i100] * (ALWAN_LITERAL(1.0) - fb) + lut_c2[i101] * fb;
    alwan_scalar const c2_11 = lut_c2[i110] * (ALWAN_LITERAL(1.0) - fb) + lut_c2[i111] * fb;
    alwan_scalar const c2_0 = c2_00 * (ALWAN_LITERAL(1.0) - fg) + c2_01 * fg;
    alwan_scalar const c2_1 = c2_10 * (ALWAN_LITERAL(1.0) - fg) + c2_11 * fg;
    *out_c2 = c2_0 * (ALWAN_LITERAL(1.0) - fr) + c2_1 * fr;
}

/* Evaluate Jakob2019 polynomial to generate spectrum from coefficients */
static inline alwan_scalar jakob2019_eval_poly(alwan_scalar c0, alwan_scalar c1, alwan_scalar c2, alwan_scalar wavelength) {
    /* Polynomial: U = c0 * wavelength^2 + c1 * wavelength + c2 */
    alwan_scalar const U = c0 * wavelength * wavelength + c1 * wavelength + c2;

    /* Reflectance: R = 0.5 + U / (2 * sqrt(1 + U^2)) */
    alwan_scalar const U_sq = U * U;
    alwan_scalar const denom = ALWAN_LITERAL(2.0) * ALWAN_SQRT(ALWAN_LITERAL(1.0) + U_sq);
    alwan_scalar const R = ALWAN_LITERAL(0.5) + U / denom;

    /* Clamp to [0, 1] */
    return clamp(R, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
}

int alwan_rgb_to_spectrum_jakob2019(alwan_spd *out_spd,
                                      alwan_ctx *ctx,
                                      alwan_jakob2019_gamut gamut,
                                      alwan_rgb const *rgb) {
    if (!rgb || !out_spd) {
        return ALWAN_E_INVALID;
    }

    /* Select LUT based on gamut */
    alwan_scalar const *lut_c0, *lut_c1, *lut_c2;

    switch (gamut) {
        case ALWAN_JAKOB2019_SRGB:
            lut_c0 = jakob2019_srgb_lut_c0;
            lut_c1 = jakob2019_srgb_lut_c1;
            lut_c2 = jakob2019_srgb_lut_c2;
            break;

        case ALWAN_JAKOB2019_PROPHOTO_RGB:
            lut_c0 = jakob2019_prophoto_lut_c0;
            lut_c1 = jakob2019_prophoto_lut_c1;
            lut_c2 = jakob2019_prophoto_lut_c2;
            break;

        case ALWAN_JAKOB2019_ACES2065_1:
            lut_c0 = jakob2019_aces_lut_c0;
            lut_c1 = jakob2019_aces_lut_c1;
            lut_c2 = jakob2019_aces_lut_c2;
            break;

        case ALWAN_JAKOB2019_REC2020:
            lut_c0 = jakob2019_rec2020_lut_c0;
            lut_c1 = jakob2019_rec2020_lut_c1;
            lut_c2 = jakob2019_rec2020_lut_c2;
            break;

        case ALWAN_JAKOB2019_ERGB:
            lut_c0 = jakob2019_ergb_lut_c0;
            lut_c1 = jakob2019_ergb_lut_c1;
            lut_c2 = jakob2019_ergb_lut_c2;
            break;

        case ALWAN_JAKOB2019_XYZ:
            lut_c0 = jakob2019_xyz_lut_c0;
            lut_c1 = jakob2019_xyz_lut_c1;
            lut_c2 = jakob2019_xyz_lut_c2;
            break;

        default:
            return ALWAN_E_INVALID;
    }

    /* Create output SPD with Jakob2019 wavelength range */
    int status = alwan_spd_create(out_spd,
                                   ctx,
                                   JAKOB2019_WAVELENGTH_MIN,
                                   JAKOB2019_WAVELENGTH_MAX,
                                   JAKOB2019_WAVELENGTH_COUNT);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Sample LUT to get polynomial coefficients via trilinear interpolation */
    alwan_scalar c0, c1, c2;
    jakob2019_lut_sample(lut_c0, lut_c1, lut_c2, rgb->r, rgb->g, rgb->b, &c0, &c1, &c2);

    /* Evaluate polynomial for each wavelength */
    alwan_scalar const wl_min = JAKOB2019_WAVELENGTH_MIN;
    alwan_scalar const wl_step = (JAKOB2019_WAVELENGTH_MAX - JAKOB2019_WAVELENGTH_MIN) / (alwan_scalar)(JAKOB2019_WAVELENGTH_COUNT - 1);

    for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
        alwan_scalar const wavelength = wl_min + (alwan_scalar)i * wl_step;
        out_spd->values[i] = jakob2019_eval_poly(c0, c1, c2, wavelength);
    }

    return ALWAN_OK;
}