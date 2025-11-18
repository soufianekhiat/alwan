/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * P8.1: Spectral Upsampling - RGB to Spectrum Conversion
 * Implements reflectance recovery methods based on colour-science
 */

#include "alwan.h"
#include "alwan_internal.h"
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
static alwan_scalar const smits1999_wavelengths[SMITS1999_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/smits1999/wavelengths.csv"
};

/* Basis spectra: white, cyan, magenta, yellow, red, green, blue */
static alwan_scalar const smits1999_white[SMITS1999_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/smits1999/white.csv"
};

static alwan_scalar const smits1999_cyan[SMITS1999_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/smits1999/cyan.csv"
};

static alwan_scalar const smits1999_magenta[SMITS1999_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/smits1999/magenta.csv"
};

static alwan_scalar const smits1999_yellow[SMITS1999_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/smits1999/yellow.csv"
};

static alwan_scalar const smits1999_red[SMITS1999_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/smits1999/red.csv"
};

static alwan_scalar const smits1999_green[SMITS1999_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/smits1999/green.csv"
};

static alwan_scalar const smits1999_blue[SMITS1999_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/smits1999/blue.csv"
};

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
static alwan_scalar const mallett2019_wavelengths[MALLETT2019_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/mallett2019/wavelengths.csv"
};

/* Basis functions: red, green, blue */
static alwan_scalar const mallett2019_red[MALLETT2019_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/mallett2019/red.csv"
};

static alwan_scalar const mallett2019_green[MALLETT2019_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/mallett2019/green.csv"
};

static alwan_scalar const mallett2019_blue[MALLETT2019_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/mallett2019/blue.csv"
};

/* ----------------------------------------------------------------
 * Jakob2019 Basis Spectra Data
 * Based on: Jakob & Hanika. "A Low-Dimensional Function Space for Efficient Spectral Upsampling" (2019)
 * RGB colorspace: sRGB
 * Wavelength range: 360-780nm, 85 samples (5nm intervals)
 * Note: Simplified implementation using pre-computed spectra from XYZ_to_sd_Jakob2019
 * ---------------------------------------------------------------- */

#define JAKOB2019_WAVELENGTH_COUNT 85
#define JAKOB2019_WAVELENGTH_MIN ALWAN_LITERAL(360.0)
#define JAKOB2019_WAVELENGTH_MAX ALWAN_LITERAL(780.0)

/* Wavelengths (shared across all basis spectra) */
static alwan_scalar const jakob2019_wavelengths[JAKOB2019_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/jakob2019/wavelengths.csv"
};

/* Basis spectra: white, cyan, magenta, yellow, red, green, blue */
static alwan_scalar const jakob2019_white[JAKOB2019_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/jakob2019/white.csv"
};

static alwan_scalar const jakob2019_cyan[JAKOB2019_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/jakob2019/cyan.csv"
};

static alwan_scalar const jakob2019_magenta[JAKOB2019_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/jakob2019/magenta.csv"
};

static alwan_scalar const jakob2019_yellow[JAKOB2019_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/jakob2019/yellow.csv"
};

static alwan_scalar const jakob2019_red[JAKOB2019_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/jakob2019/red.csv"
};

static alwan_scalar const jakob2019_green[JAKOB2019_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/jakob2019/green.csv"
};

static alwan_scalar const jakob2019_blue[JAKOB2019_WAVELENGTH_COUNT] = {
#include "data/spectral_basis/jakob2019/blue.csv"
};

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

int alwan_rgb_to_spectrum_smits1999(alwan_ctx *ctx,
                                     alwan_vec3 const *rgb,
                                     alwan_spd *out_spd) {
    if (!rgb || !out_spd) {
        return ALWAN_E_INVALID;
    }

    /* Create output SPD with Smits1999 wavelength range */
    int status = alwan_spd_create(ctx,
                                   SMITS1999_WAVELENGTH_MIN,
                                   SMITS1999_WAVELENGTH_MAX,
                                   SMITS1999_WAVELENGTH_COUNT,
                                   out_spd);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Extract RGB components (clamped to [0, 1]) */
    alwan_scalar r = clamp(rgb->v[0], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    alwan_scalar g = clamp(rgb->v[1], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    alwan_scalar b = clamp(rgb->v[2], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

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

int alwan_rgb_to_spectrum_mallett2019(alwan_ctx *ctx,
                                       alwan_vec3 const *rgb,
                                       alwan_spd *out_spd) {
    if (!rgb || !out_spd) {
        return ALWAN_E_INVALID;
    }

    /* Create output SPD with Mallett2019 wavelength range */
    int status = alwan_spd_create(ctx,
                                   MALLETT2019_WAVELENGTH_MIN,
                                   MALLETT2019_WAVELENGTH_MAX,
                                   MALLETT2019_WAVELENGTH_COUNT,
                                   out_spd);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Extract RGB components */
    alwan_scalar r = rgb->v[0];
    alwan_scalar g = rgb->v[1];
    alwan_scalar b = rgb->v[2];

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
 * Simplified implementation using Smits-style mixing with Jakob2019 basis spectra
 * Note: This is not the full polynomial LUT approach from the original paper
 * ---------------------------------------------------------------- */

int alwan_rgb_to_spectrum_jakob2019(alwan_ctx *ctx,
                                      alwan_vec3 const *rgb,
                                      alwan_spd *out_spd) {
    if (!rgb || !out_spd) {
        return ALWAN_E_INVALID;
    }

    /* Create output SPD with Jakob2019 wavelength range */
    int status = alwan_spd_create(ctx,
                                   JAKOB2019_WAVELENGTH_MIN,
                                   JAKOB2019_WAVELENGTH_MAX,
                                   JAKOB2019_WAVELENGTH_COUNT,
                                   out_spd);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Extract RGB components (clamped to [0, 1]) */
    alwan_scalar r = clamp(rgb->v[0], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    alwan_scalar g = clamp(rgb->v[1], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    alwan_scalar b = clamp(rgb->v[2], ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

    /* Use Smits-style algorithm with Jakob2019 basis spectra
     * This provides Jakob2019 quality spectra with Smits algorithm */

    if (r <= g && r <= b) {
        /* R is minimum: start with white * R */
        for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
            out_spd->values[i] = jakob2019_white[i] * r;
        }

        if (g <= b) {
            /* Add cyan for (G - R) */
            for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += jakob2019_cyan[i] * (g - r);
            }
            /* Add blue for (B - G) */
            for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += jakob2019_blue[i] * (b - g);
            }
        } else {
            /* Add cyan for (B - R) */
            for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += jakob2019_cyan[i] * (b - r);
            }
            /* Add green for (G - B) */
            for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += jakob2019_green[i] * (g - b);
            }
        }
    } else if (g <= r && g <= b) {
        /* G is minimum: start with white * G */
        for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
            out_spd->values[i] = jakob2019_white[i] * g;
        }

        if (r <= b) {
            /* Add magenta for (R - G) */
            for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += jakob2019_magenta[i] * (r - g);
            }
            /* Add blue for (B - R) */
            for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += jakob2019_blue[i] * (b - r);
            }
        } else {
            /* Add magenta for (B - G) */
            for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += jakob2019_magenta[i] * (b - g);
            }
            /* Add red for (R - B) */
            for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += jakob2019_red[i] * (r - b);
            }
        }
    } else {
        /* B is minimum: start with white * B */
        for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
            out_spd->values[i] = jakob2019_white[i] * b;
        }

        if (r <= g) {
            /* Add yellow for (R - B) */
            for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += jakob2019_yellow[i] * (r - b);
            }
            /* Add green for (G - R) */
            for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += jakob2019_green[i] * (g - r);
            }
        } else {
            /* Add yellow for (G - B) */
            for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += jakob2019_yellow[i] * (g - b);
            }
            /* Add red for (R - G) */
            for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
                out_spd->values[i] += jakob2019_red[i] * (r - g);
            }
        }
    }

    /* Clamp negative values */
    for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
        if (out_spd->values[i] < ALWAN_LITERAL(0.0)) {
            out_spd->values[i] = ALWAN_LITERAL(0.0);
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * TODO: Meng2015 - XYZ to Spectrum (Optimization-based)
 * ----------------------------------------------------------------
 *
 * Reference: Meng, J., Simon, F., Hanika, J., & Dachsbacher, C. (2015).
 * "Physically Meaningful Rendering using Tristimulus Colours."
 * Computer Graphics Forum, 34(4), 31-40.
 *
 * Algorithm overview:
 * - Uses constrained optimization to recover smooth reflectance spectrum from XYZ
 * - Iteratively adjusts spectrum to minimize difference between target and computed XYZ
 * - Enforces physical constraints (non-negative, bounded reflectance)
 *
 * Implementation requirements:
 * - Numerical optimization solver (e.g., L-BFGS, conjugate gradient)
 * - Objective function: minimize ||XYZ_target - XYZ_from_spectrum||²
 * - Constraints: 0 ≤ reflectance ≤ 1, smoothness regularization
 * - Parameters: wavelength interval, tolerance, max_iterations
 *
 * Prototype:
 * int alwan_xyz_to_spectrum_meng2015(alwan_ctx *ctx,
 *                                     alwan_vec3 const *xyz,
 *                                     alwan_scalar interval,
 *                                     alwan_scalar tolerance,
 *                                     int max_iterations,
 *                                     alwan_spd *out_spd);
 *
 * Status: NOT YET IMPLEMENTED
 * Reason: Requires external numerical optimization library
 */

/* ----------------------------------------------------------------
 * TODO: Jakob2019 - RGB to Spectrum (Polynomial LUT-based)
 * ----------------------------------------------------------------
 *
 * Reference: Jakob, W., & Hanika, J. (2019).
 * "A Low-Dimensional Function Space for Efficient Spectral Upsampling."
 * Computer Graphics Forum (Proceedings of Eurographics), 38(2).
 *
 * Algorithm overview:
 * - Pre-computed 3D lookup table maps RGB to polynomial coefficients
 * - Coefficients define smooth reflectance spectrum via polynomial basis
 * - Very fast: just trilinear interpolation + polynomial evaluation
 *
 * Implementation requirements:
 * - LUT3D coefficient data file (~64MB, available from Zenodo record 4050598)
 * - Trilinear interpolation in RGB cube
 * - Polynomial evaluation: spectrum(λ) = Σ c_i * basis_i(λ)
 * - Data loading infrastructure for coefficient tables
 *
 * Data source:
 * - Zenodo: https://zenodo.org/records/4050598
 * - Colour Science datasets repository
 * - Pre-computed for sRGB, Rec. 2020, and other color spaces
 *
 * Prototype:
 * int alwan_rgb_to_spectrum_jakob2019(alwan_ctx *ctx,
 *                                      alwan_vec3 const *rgb,
 *                                      alwan_spd *out_spd);
 *
 * Status: NOT YET IMPLEMENTED
 * Reason: Requires coefficient data files (need to download or generate)
 */
