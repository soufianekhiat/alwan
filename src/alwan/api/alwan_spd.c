/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Spectral Power Distributions (SPD)
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * SPD Creation and Destruction
 * ---------------------------------------------------------------- */

int alwan_spd_create(alwan_spd *out,
                     alwan_ctx *ctx,
                     alwan_scalar wavelength_min,
                     alwan_scalar wavelength_max,
                     size_t count) {
    if (!out || count == 0 || wavelength_min >= wavelength_max) {
        return ALWAN_E_INVALID;
    }

    /* Check for allocation size overflow */
    size_t alloc_size = alwan_safe_array_size(count, sizeof(alwan_scalar));
    if (alloc_size == 0) {
        return ALWAN_E_NOMEM;  /* Overflow would occur */
    }

    /* Allocate values array */
    alwan_scalar *values = (alwan_scalar *)ALWAN_ALLOC(alloc_size, sizeof(alwan_scalar));
    if (!values) {
        return ALWAN_E_NOMEM;
    }

    /* Initialize to zero */
    for (size_t i = 0; i < count; i++) {
        values[i] = ALWAN_LITERAL(0.0);
    }

    out->values = values;
    out->wavelength_min = wavelength_min;
    out->wavelength_max = wavelength_max;
    out->count = count;

    (void)ctx;  /* Unused for now, reserved for future use */
    return ALWAN_OK;
}

void alwan_spd_destroy(alwan_ctx *ctx, alwan_spd *spd) {
    if (spd && spd->values) {
        ALWAN_FREE(spd->values);
        spd->values = NULL;
        spd->count = 0;
    }
    (void)ctx;
}

/* ----------------------------------------------------------------
 * Helper: Get wavelength at index
 * ---------------------------------------------------------------- */

static inline alwan_scalar spd_wavelength_at(alwan_spd const *spd, size_t index) {
    if (spd->count <= 1) {
        return spd->wavelength_min;
    }
    alwan_scalar t = (alwan_scalar)index / (alwan_scalar)(spd->count - 1);
    return spd->wavelength_min + t * (spd->wavelength_max - spd->wavelength_min);
}

/* Helper: Interpolate SPD value at wavelength with extrapolation */
static alwan_scalar spd_interpolate(alwan_spd const *spd, alwan_scalar wavelength,
                               alwan_resample_method method,
                               alwan_extrapolate_mode extrapolate) {
    /* Handle out-of-range wavelengths based on extrapolation mode */
    if (wavelength < spd->wavelength_min) {
        if (extrapolate == ALWAN_EXTRAPOLATE_ZERO) {
            return ALWAN_LITERAL(0.0);
        } else if (extrapolate == ALWAN_EXTRAPOLATE_CONSTANT) {
            return spd->values[0];
        } else /* ALWAN_EXTRAPOLATE_LINEAR */ {
            /* Linear extrapolation using first two points */
            if (spd->count < 2) {
                return spd->values[0];
            }
            alwan_scalar wl0 = spd_wavelength_at(spd, 0);
            alwan_scalar wl1 = spd_wavelength_at(spd, 1);
            alwan_scalar slope = (spd->values[1] - spd->values[0]) / (wl1 - wl0);
            return spd->values[0] + slope * (wavelength - wl0);
        }
    }

    if (wavelength > spd->wavelength_max) {
        if (extrapolate == ALWAN_EXTRAPOLATE_ZERO) {
            return ALWAN_LITERAL(0.0);
        } else if (extrapolate == ALWAN_EXTRAPOLATE_CONSTANT) {
            return spd->values[spd->count - 1];
        } else /* ALWAN_EXTRAPOLATE_LINEAR */ {
            /* Linear extrapolation using last two points */
            if (spd->count < 2) {
                return spd->values[spd->count - 1];
            }
            size_t n = spd->count;
            alwan_scalar wl_n2 = spd_wavelength_at(spd, n - 2);
            alwan_scalar wl_n1 = spd_wavelength_at(spd, n - 1);
            alwan_scalar slope = (spd->values[n - 1] - spd->values[n - 2]) / (wl_n1 - wl_n2);
            return spd->values[n - 1] + slope * (wavelength - wl_n1);
        }
    }

    if (spd->count == 0) {
        return ALWAN_LITERAL(0.0);
    }

    if (spd->count == 1) {
        return spd->values[0];
    }

    /* Find fractional index */
    alwan_scalar span = spd->wavelength_max - spd->wavelength_min;
    alwan_scalar t = (wavelength - spd->wavelength_min) / span;
    alwan_scalar f_index = t * (alwan_scalar)(spd->count - 1);

    /* Clamp to valid range */
    if (f_index <= ALWAN_LITERAL(0.0)) {
        return spd->values[0];
    }
    if (f_index >= (alwan_scalar)(spd->count - 1)) {
        return spd->values[spd->count - 1];
    }

    size_t i0 = (size_t)f_index;
    alwan_scalar frac = f_index - (alwan_scalar)i0;

    if (method == ALWAN_RESAMPLE_LINEAR) {
        /* Linear interpolation */
        return spd->values[i0] * (ALWAN_LITERAL(1.0) - frac) + spd->values[i0 + 1] * frac;
    } else {
        /* Catmull-Rom spline interpolation */
        alwan_scalar p0 = (i0 > 0) ? spd->values[i0 - 1] : spd->values[i0];
        alwan_scalar p1 = spd->values[i0];
        alwan_scalar p2 = spd->values[i0 + 1];
        alwan_scalar p3 = (i0 + 2 < spd->count) ? spd->values[i0 + 2] : spd->values[i0 + 1];

        alwan_scalar t2 = frac * frac;
        alwan_scalar t3 = t2 * frac;

        alwan_scalar result = ALWAN_LITERAL(0.5) * (
            (ALWAN_LITERAL(2.0) * p1) +
            (-p0 + p2) * frac +
            (ALWAN_LITERAL(2.0) * p0 - ALWAN_LITERAL(5.0) * p1 + ALWAN_LITERAL(4.0) * p2 - p3) * t2 +
            (-p0 + ALWAN_LITERAL(3.0) * p1 - ALWAN_LITERAL(3.0) * p2 + p3) * t3
        );

        return result;
    }
}

/* ----------------------------------------------------------------
 * SPD Resampling
 * ---------------------------------------------------------------- */

int alwan_spd_resample(alwan_spd *dst,
                       alwan_ctx *ctx,
                       alwan_spd const *src,
                       alwan_scalar wavelength_min,
                       alwan_scalar wavelength_max,
                       size_t count,
                       alwan_resample_method method,
                       alwan_extrapolate_mode extrapolate) {
    if (!src || !dst || count == 0 || wavelength_min >= wavelength_max) {
        return ALWAN_E_INVALID;
    }

    /* Create destination SPD */
    int status = alwan_spd_create(dst, ctx, wavelength_min, wavelength_max, count);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Resample each point */
    for (size_t i = 0; i < count; i++) {
        alwan_scalar wavelength = spd_wavelength_at(dst, i);
        dst->values[i] = spd_interpolate(src, wavelength, method, extrapolate);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Illuminant Loading
 * ---------------------------------------------------------------- */

int alwan_spd_illuminant(alwan_spd *out, alwan_ctx *ctx, alwan_illuminant ill) {
    if (!out) {
        return ALWAN_E_INVALID;
    }

    /* Create SPD structure (360-830nm, 1nm steps = 471 samples) */
    int status = alwan_spd_create(out, ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Load illuminant SPD data from embedded CSV */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV

    switch (ill) {
        case ALWAN_ILLUMINANT_A: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/A_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D50: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/D50_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D55: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/D55_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D65: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/D65_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_E: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/E_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F1: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/F1_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F2: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/F2_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F3: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/F3_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F4: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/F4_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F5: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/F5_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F6: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/F6_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F7: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/F7_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F8: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/F8_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F9: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/F9_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F10: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/F10_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F11: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/F11_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F12: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/F12_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }

        /* Extended illuminants */
        case ALWAN_ILLUMINANT_B: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/B_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_C: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/C_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D60: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/D60_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D75: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/D75_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }

        /* Additional D-series illuminants */
        case ALWAN_ILLUMINANT_D40: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/D40_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D45: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/D45_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D93: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/D93_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }

        /* LED illuminants */
        case ALWAN_ILLUMINANT_LED_B1: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/LED-B1_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_B2: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/LED-B2_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_B3: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/LED-B3_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_B4: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/LED-B4_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_B5: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/LED-B5_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_BH1: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/LED-BH1_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_RGB1: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/LED-RGB1_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_V1: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/LED-V1_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_V2: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/LED-V2_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }

        /* High Pressure illuminants */
        case ALWAN_ILLUMINANT_HP1: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/HP1_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_HP2: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/HP2_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_HP3: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/HP3_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_HP4: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/HP4_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_HP5: {
            static alwan_scalar const data[] = {
#include "../data/illuminants/HP5_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }

        default:
            alwan_spd_destroy(ctx, out);
            return ALWAN_E_INVALID;
    }

    ALWAN_DIAG_POP

    (void)ctx;
    return ALWAN_OK;
}

/* Generate blackbody (Planckian) SPD using Planck's law
 * Spectral radiance: L(λ,T) = c1 / (λ^5 * (exp(c2/(λ*T)) - 1))
 * where c1 = 3.741771e-16 W⋅m², c2 = 1.4388e-2 m⋅K */
int alwan_spd_blackbody(alwan_spd *out,
                        alwan_ctx *ctx,
                        alwan_scalar temperature_K,
                        alwan_scalar wavelength_min,
                        alwan_scalar wavelength_max,
                        size_t count) {
    if (!out || count == 0) {
        return ALWAN_E_INVALID;
    }

    /* Validate temperature range (1000-25000K typical for lighting) */
    if (temperature_K < ALWAN_LITERAL(1000.0) || temperature_K > ALWAN_LITERAL(25000.0)) {
        return ALWAN_E_INVALID;
    }

    /* Create SPD structure */
    int status = alwan_spd_create(out, ctx, wavelength_min, wavelength_max, count);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Planck's law constants */
    const alwan_scalar c1 = ALWAN_LITERAL(3.741771e-16);  /* W⋅m² (first radiation constant) */
    const alwan_scalar c2 = ALWAN_LITERAL(1.4388e-2);     /* m⋅K (second radiation constant) */

    /* Calculate wavelength step */
    alwan_scalar const step = (wavelength_max - wavelength_min) / (alwan_scalar)(count - 1);

    /* Compute Planckian spectral radiance for each wavelength */
    for (size_t i = 0; i < count; i++) {
        alwan_scalar wavelength_nm = wavelength_min + (alwan_scalar)i * step;
        alwan_scalar wavelength_m = wavelength_nm * ALWAN_LITERAL(1e-9);  /* Convert nm to meters */

        /* Planck's law: L(λ,T) = c1 / (λ^5 * (exp(c2/(λ*T)) - 1)) */
        alwan_scalar lambda5 = wavelength_m * wavelength_m * wavelength_m * wavelength_m * wavelength_m;
        alwan_scalar exponent = c2 / (wavelength_m * temperature_K);
        alwan_scalar denominator = lambda5 * (ALWAN_EXP(exponent) - ALWAN_LITERAL(1.0));

        /* Avoid division by zero (though should never happen with valid inputs)
         * Use a much smaller threshold since Planck's law naturally produces tiny denominators
         * (typically 1e-30 to 1e-28 for visible wavelengths) */
        if (ALWAN_ABS(denominator) < ALWAN_LITERAL(1e-100)) {
            out->values[i] = ALWAN_LITERAL(0.0);
        } else {
            out->values[i] = c1 / denominator;
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Observer CMF Loading
 * ---------------------------------------------------------------- */

static int load_observer_cmf(alwan_ctx *ctx,
                              alwan_observer_type observer,
                              alwan_spd *x_bar,
                              alwan_spd *y_bar,
                              alwan_spd *z_bar) {
    /* Determine wavelength range and sample count based on observer */
    alwan_scalar wl_min, wl_max;
    size_t count;

    if (observer == ALWAN_OBSERVER_CIE_1931_2DEG || observer == ALWAN_OBSERVER_CIE_1964_10DEG) {
        /* CIE 1931/1964: 360-830nm, 1nm steps = 471 samples */
        wl_min = ALWAN_LITERAL(360.0);
        wl_max = ALWAN_LITERAL(830.0);
        count = 471;
    } else {
        /* CIE 2012/2015, Stockman & Sharpe: 360-830nm, 1nm steps = 471 samples
         * CSVs cover 360-830nm with leading zeros before 390nm. */
        wl_min = ALWAN_LITERAL(360.0);
        wl_max = ALWAN_LITERAL(830.0);
        count = 471;
    }

    int status;

    status = alwan_spd_create(x_bar, ctx, wl_min, wl_max, count);
    if (status != ALWAN_OK) return status;

    status = alwan_spd_create(y_bar, ctx, wl_min, wl_max, count);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, x_bar);
        return status;
    }

    status = alwan_spd_create(z_bar, ctx, wl_min, wl_max, count);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, x_bar);
        alwan_spd_destroy(ctx, y_bar);
        return status;
    }

    /* Load CMF data from embedded CSV */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV

    if (observer == ALWAN_OBSERVER_CIE_1931_2DEG) {
        /* CIE 1931 2° Standard Observer */
        static alwan_scalar const x_data[] = {
#include "../data/cmf/cie_1931_2deg_x_360_830_1nm.csv"
        };
        static alwan_scalar const y_data[] = {
#include "../data/cmf/cie_1931_2deg_y_360_830_1nm.csv"
        };
        static alwan_scalar const z_data[] = {
#include "../data/cmf/cie_1931_2deg_z_360_830_1nm.csv"
        };

        size_t const n = sizeof(x_data) / sizeof(x_data[0]);
        for (size_t i = 0; i < n && i < x_bar->count; i++) {
            x_bar->values[i] = x_data[i];
            y_bar->values[i] = y_data[i];
            z_bar->values[i] = z_data[i];
        }
    } else if (observer == ALWAN_OBSERVER_CIE_1964_10DEG) {
        /* CIE 1964 10° Standard Observer */
        static alwan_scalar const x_data[] = {
#include "../data/cmf/cie_1964_10deg_x_360_830_1nm.csv"
        };
        static alwan_scalar const y_data[] = {
#include "../data/cmf/cie_1964_10deg_y_360_830_1nm.csv"
        };
        static alwan_scalar const z_data[] = {
#include "../data/cmf/cie_1964_10deg_z_360_830_1nm.csv"
        };

        size_t const n = sizeof(x_data) / sizeof(x_data[0]);
        for (size_t i = 0; i < n && i < x_bar->count; i++) {
            x_bar->values[i] = x_data[i];
            y_bar->values[i] = y_data[i];
            z_bar->values[i] = z_data[i];
        }
    } else if (observer == ALWAN_OBSERVER_CIE_2012_2DEG) {
        /* CIE 2012/2015 2° Standard Observer (390-830nm) */
        static alwan_scalar const x_data[] = {
#include "../data/cmf/cie_2012_2deg_x_360_830_1nm.csv"
        };
        static alwan_scalar const y_data[] = {
#include "../data/cmf/cie_2012_2deg_y_360_830_1nm.csv"
        };
        static alwan_scalar const z_data[] = {
#include "../data/cmf/cie_2012_2deg_z_360_830_1nm.csv"
        };

        size_t const n = sizeof(x_data) / sizeof(x_data[0]);
        for (size_t i = 0; i < n && i < x_bar->count; i++) {
            x_bar->values[i] = x_data[i];
            y_bar->values[i] = y_data[i];
            z_bar->values[i] = z_data[i];
        }
    } else if (observer == ALWAN_OBSERVER_CIE_2012_10DEG) {
        /* CIE 2012/2015 10° Standard Observer (390-830nm) */
        static alwan_scalar const x_data[] = {
#include "../data/cmf/cie_2012_10deg_x_360_830_1nm.csv"
        };
        static alwan_scalar const y_data[] = {
#include "../data/cmf/cie_2012_10deg_y_360_830_1nm.csv"
        };
        static alwan_scalar const z_data[] = {
#include "../data/cmf/cie_2012_10deg_z_360_830_1nm.csv"
        };

        size_t const n = sizeof(x_data) / sizeof(x_data[0]);
        for (size_t i = 0; i < n && i < x_bar->count; i++) {
            x_bar->values[i] = x_data[i];
            y_bar->values[i] = y_data[i];
            z_bar->values[i] = z_data[i];
        }
    } else if (observer == ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG) {
        /* Stockman & Sharpe 2000 2° Cone Fundamentals */
        static alwan_scalar const x_data[] = {
#include "../data/cmf/stockman_sharpe_2deg_x_360_830_1nm.csv"
        };
        static alwan_scalar const y_data[] = {
#include "../data/cmf/stockman_sharpe_2deg_y_360_830_1nm.csv"
        };
        static alwan_scalar const z_data[] = {
#include "../data/cmf/stockman_sharpe_2deg_z_360_830_1nm.csv"
        };

        size_t const n = sizeof(x_data) / sizeof(x_data[0]);
        for (size_t i = 0; i < n && i < x_bar->count; i++) {
            x_bar->values[i] = x_data[i];
            y_bar->values[i] = y_data[i];
            z_bar->values[i] = z_data[i];
        }
    } else if (observer == ALWAN_OBSERVER_CIE_2015_2DEG) {
        /* CIE 2015 2° Cone Fundamental Observer */
        static alwan_scalar const x_data[] = {
#include "../data/cmf/cie_2015_2deg_x_360_830_1nm.csv"
        };
        static alwan_scalar const y_data[] = {
#include "../data/cmf/cie_2015_2deg_y_360_830_1nm.csv"
        };
        static alwan_scalar const z_data[] = {
#include "../data/cmf/cie_2015_2deg_z_360_830_1nm.csv"
        };

        size_t const n = sizeof(x_data) / sizeof(x_data[0]);
        for (size_t i = 0; i < n && i < x_bar->count; i++) {
            x_bar->values[i] = x_data[i];
            y_bar->values[i] = y_data[i];
            z_bar->values[i] = z_data[i];
        }
    } else if (observer == ALWAN_OBSERVER_CIE_2015_10DEG) {
        /* CIE 2015 10° Cone Fundamental Observer */
        static alwan_scalar const x_data[] = {
#include "../data/cmf/cie_2015_10deg_x_360_830_1nm.csv"
        };
        static alwan_scalar const y_data[] = {
#include "../data/cmf/cie_2015_10deg_y_360_830_1nm.csv"
        };
        static alwan_scalar const z_data[] = {
#include "../data/cmf/cie_2015_10deg_z_360_830_1nm.csv"
        };

        size_t const n = sizeof(x_data) / sizeof(x_data[0]);
        for (size_t i = 0; i < n && i < x_bar->count; i++) {
            x_bar->values[i] = x_data[i];
            y_bar->values[i] = y_data[i];
            z_bar->values[i] = z_data[i];
        }
    } else if (observer == ALWAN_OBSERVER_WRIGHT_GUILD_1931) {
        /* Wright & Guild 1931 2° RGB CMFs (historical) */
        static alwan_scalar const r_data[] = {
#include "../data/cmf/wright_guild_1931_r_360_830_1nm.csv"
        };
        static alwan_scalar const g_data[] = {
#include "../data/cmf/wright_guild_1931_g_360_830_1nm.csv"
        };
        static alwan_scalar const b_data[] = {
#include "../data/cmf/wright_guild_1931_b_360_830_1nm.csv"
        };

        size_t const n = sizeof(r_data) / sizeof(r_data[0]);
        for (size_t i = 0; i < n && i < x_bar->count; i++) {
            x_bar->values[i] = r_data[i];
            y_bar->values[i] = g_data[i];
            z_bar->values[i] = b_data[i];
        }
    } else {
        alwan_spd_destroy(ctx, x_bar);
        alwan_spd_destroy(ctx, y_bar);
        alwan_spd_destroy(ctx, z_bar);
        return ALWAN_E_INVALID;
    }

    ALWAN_DIAG_POP
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ Integration from SPD
 * ---------------------------------------------------------------- */

/* Trapezoidal integration */
static alwan_scalar integrate_trapezoid(alwan_scalar const *values, size_t count, alwan_scalar dx) {
    if (count == 0) return ALWAN_LITERAL(0.0);
    if (count == 1) return values[0] * dx;

    alwan_scalar sum = ALWAN_LITERAL(0.5) * (values[0] + values[count - 1]);
    for (size_t i = 1; i < count - 1; i++) {
        sum += values[i];
    }
    return sum * dx;
}

/* Simpson's 1/3 rule integration */
static alwan_scalar integrate_simpson(alwan_scalar const *values, size_t count, alwan_scalar dx) {
    if (count == 0) return ALWAN_LITERAL(0.0);
    if (count == 1) return values[0] * dx;
    if (count == 2) return ALWAN_LITERAL(0.5) * (values[0] + values[1]) * dx;

    /* Simpson's rule requires odd number of points */
    size_t n = count;
    int use_trapezoid_last = (count % 2 == 0);
    if (use_trapezoid_last) n--;

    alwan_scalar sum = values[0] + values[n - 1];
    for (size_t i = 1; i < n - 1; i += 2) {
        sum += ALWAN_LITERAL(4.0) * values[i];
    }
    for (size_t i = 2; i < n - 1; i += 2) {
        sum += ALWAN_LITERAL(2.0) * values[i];
    }
    alwan_scalar result = sum * dx / ALWAN_LITERAL(3.0);

    /* Add last interval with trapezoid if needed */
    if (use_trapezoid_last) {
        result += ALWAN_LITERAL(0.5) * (values[n - 1] + values[n]) * dx;
    }

    return result;
}

int alwan_xyz_from_spd(alwan_xyz *xyz_out,
                       alwan_ctx *ctx,
                       alwan_spd const *spd,
                       alwan_spd const *illuminant,
                       alwan_observer_type observer,
                       alwan_integrate_method method,
                       alwan_scalar bandpass_nm) {
    if (!spd || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    /* Load observer CMFs */
    alwan_spd x_bar, y_bar, z_bar;
    int status = load_observer_cmf(ctx, observer, &x_bar, &y_bar, &z_bar);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Resample CMFs to match SPD wavelength range (use constant extrapolation for smooth CMFs) */
    alwan_spd x_bar_resampled, y_bar_resampled, z_bar_resampled;
    status = alwan_spd_resample(&x_bar_resampled, ctx, &x_bar, spd->wavelength_min, spd->wavelength_max,
                                spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &x_bar);
        alwan_spd_destroy(ctx, &y_bar);
        alwan_spd_destroy(ctx, &z_bar);
        return status;
    }

    status = alwan_spd_resample(&y_bar_resampled, ctx, &y_bar, spd->wavelength_min, spd->wavelength_max,
                                spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &x_bar);
        alwan_spd_destroy(ctx, &y_bar);
        alwan_spd_destroy(ctx, &z_bar);
        alwan_spd_destroy(ctx, &x_bar_resampled);
        return status;
    }

    status = alwan_spd_resample(&z_bar_resampled, ctx, &z_bar, spd->wavelength_min, spd->wavelength_max,
                                spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &x_bar);
        alwan_spd_destroy(ctx, &y_bar);
        alwan_spd_destroy(ctx, &z_bar);
        alwan_spd_destroy(ctx, &x_bar_resampled);
        alwan_spd_destroy(ctx, &y_bar_resampled);
        return status;
    }

    /* Allocate temporary arrays for products (with overflow protection) */
    size_t alloc_size = alwan_safe_array_size(spd->count, sizeof(alwan_scalar));
    if (alloc_size == 0) {
        alwan_spd_destroy(ctx, &x_bar);
        alwan_spd_destroy(ctx, &y_bar);
        alwan_spd_destroy(ctx, &z_bar);
        alwan_spd_destroy(ctx, &x_bar_resampled);
        alwan_spd_destroy(ctx, &y_bar_resampled);
        alwan_spd_destroy(ctx, &z_bar_resampled);
        return ALWAN_E_NOMEM;
    }
    alwan_scalar *prod_x = (alwan_scalar *)ALWAN_ALLOC(alloc_size, sizeof(alwan_scalar));
    alwan_scalar *prod_y = (alwan_scalar *)ALWAN_ALLOC(alloc_size, sizeof(alwan_scalar));
    alwan_scalar *prod_z = (alwan_scalar *)ALWAN_ALLOC(alloc_size, sizeof(alwan_scalar));

    if (!prod_x || !prod_y || !prod_z) {
        if (prod_x) ALWAN_FREE(prod_x);
        if (prod_y) ALWAN_FREE(prod_y);
        if (prod_z) ALWAN_FREE(prod_z);
        alwan_spd_destroy(ctx, &x_bar);
        alwan_spd_destroy(ctx, &y_bar);
        alwan_spd_destroy(ctx, &z_bar);
        alwan_spd_destroy(ctx, &x_bar_resampled);
        alwan_spd_destroy(ctx, &y_bar_resampled);
        alwan_spd_destroy(ctx, &z_bar_resampled);
        return ALWAN_E_NOMEM;
    }

    /* Compute products: SPD * illuminant * CMF */
    for (size_t i = 0; i < spd->count; i++) {
        alwan_scalar spd_value = spd->values[i];

        /* If illuminant provided, multiply by it */
        if (illuminant) {
            alwan_scalar wavelength = spd_wavelength_at(spd, i);
            alwan_scalar illum_value = spd_interpolate(illuminant, wavelength, ALWAN_RESAMPLE_LINEAR,
                                                  ALWAN_EXTRAPOLATE_ZERO);
            spd_value *= illum_value;
        }

        prod_x[i] = spd_value * x_bar_resampled.values[i];
        prod_y[i] = spd_value * y_bar_resampled.values[i];
        prod_z[i] = spd_value * z_bar_resampled.values[i];
    }

    /* Bandpass correction (Stearns & Stearns 1988) not yet implemented.
     * For now, bandpass_nm parameter is accepted but not used. */
    (void)bandpass_nm;

    /* Integrate to get XYZ */
    alwan_scalar dx = (spd->wavelength_max - spd->wavelength_min) / (alwan_scalar)(spd->count - 1);
    if (spd->count == 1) dx = ALWAN_LITERAL(1.0);

    if (method == ALWAN_INTEGRATE_TRAPEZOID) {
        xyz_out->x = integrate_trapezoid(prod_x, spd->count, dx);
        xyz_out->y = integrate_trapezoid(prod_y, spd->count, dx);
        xyz_out->z = integrate_trapezoid(prod_z, spd->count, dx);
    } else {
        xyz_out->x = integrate_simpson(prod_x, spd->count, dx);
        xyz_out->y = integrate_simpson(prod_y, spd->count, dx);
        xyz_out->z = integrate_simpson(prod_z, spd->count, dx);
    }

    /* Cleanup */
    ALWAN_FREE(prod_x);
    ALWAN_FREE(prod_y);
    ALWAN_FREE(prod_z);
    alwan_spd_destroy(ctx, &x_bar);
    alwan_spd_destroy(ctx, &y_bar);
    alwan_spd_destroy(ctx, &z_bar);
    alwan_spd_destroy(ctx, &x_bar_resampled);
    alwan_spd_destroy(ctx, &y_bar_resampled);
    alwan_spd_destroy(ctx, &z_bar_resampled);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Camera Sensitivity Functions
 * ---------------------------------------------------------------- */

int alwan_spd_camera_sensitivity(alwan_spd *spd_r,
                                   alwan_spd *spd_g,
                                   alwan_spd *spd_b,
                                   alwan_ctx *ctx,
                                   alwan_camera_sensitivity camera) {
    if (!spd_r || !spd_g || !spd_b) {
        return ALWAN_E_INVALID;
    }

    /* Validate that all SPDs have same wavelength range and count */
    if (spd_r->wavelength_min != spd_g->wavelength_min || spd_r->wavelength_min != spd_b->wavelength_min ||
        spd_r->wavelength_max != spd_g->wavelength_max || spd_r->wavelength_max != spd_b->wavelength_max ||
        spd_r->count != spd_g->count || spd_r->count != spd_b->count) {
        return ALWAN_E_INVALID;
    }

    /* Expected wavelength range: 360-830nm, 1nm steps = 471 samples */
    if (spd_r->count != 471) {
        return ALWAN_E_INVALID;
    }

    /* Load camera sensitivity data from embedded CSV */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV

    switch (camera) {
        case ALWAN_CAMERA_NIKON_5100: {
            static alwan_scalar const data_r[] = {
#include "../data/camera_sensitivities/nikon_5100_r.csv"
            };
            static alwan_scalar const data_g[] = {
#include "../data/camera_sensitivities/nikon_5100_g.csv"
            };
            static alwan_scalar const data_b[] = {
#include "../data/camera_sensitivities/nikon_5100_b.csv"
            };
            size_t const n = sizeof(data_r) / sizeof(data_r[0]);
            for (size_t i = 0; i < n && i < spd_r->count; i++) {
                spd_r->values[i] = data_r[i];
                spd_g->values[i] = data_g[i];
                spd_b->values[i] = data_b[i];
            }
            break;
        }
        case ALWAN_CAMERA_SIGMA_SDMERILL: {
            static alwan_scalar const data_r[] = {
#include "../data/camera_sensitivities/sigma_sdmerill_r.csv"
            };
            static alwan_scalar const data_g[] = {
#include "../data/camera_sensitivities/sigma_sdmerill_g.csv"
            };
            static alwan_scalar const data_b[] = {
#include "../data/camera_sensitivities/sigma_sdmerill_b.csv"
            };
            size_t const n = sizeof(data_r) / sizeof(data_r[0]);
            for (size_t i = 0; i < n && i < spd_r->count; i++) {
                spd_r->values[i] = data_r[i];
                spd_g->values[i] = data_g[i];
                spd_b->values[i] = data_b[i];
            }
            break;
        }
        default:
            return ALWAN_E_INVALID;
    }

    ALWAN_DIAG_POP

    (void)ctx;  /* Unused for now */
    return ALWAN_OK;
}

int alwan_xyz_from_spd_camera(alwan_xyz *xyz_out,
                               alwan_ctx *ctx,
                               alwan_spd const *spd,
                               alwan_spd const *illuminant,
                               alwan_camera_sensitivity camera,
                               alwan_integrate_method method) {
    if (!spd || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    /* Load camera RGB sensitivities */
    alwan_spd r_sens, g_sens, b_sens;
    int status = alwan_spd_create(&r_sens, ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471);
    if (status != ALWAN_OK) {
        return status;
    }
    status = alwan_spd_create(&g_sens, ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &r_sens);
        return status;
    }
    status = alwan_spd_create(&b_sens, ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &r_sens);
        alwan_spd_destroy(ctx, &g_sens);
        return status;
    }

    status = alwan_spd_camera_sensitivity(&r_sens, &g_sens, &b_sens, ctx, camera);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &r_sens);
        alwan_spd_destroy(ctx, &g_sens);
        alwan_spd_destroy(ctx, &b_sens);
        return status;
    }

    /* Resample sensitivities to match SPD wavelength range */
    alwan_spd r_resampled, g_resampled, b_resampled;
    status = alwan_spd_resample(&r_resampled, ctx, &r_sens, spd->wavelength_min, spd->wavelength_max,
                                spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &r_sens);
        alwan_spd_destroy(ctx, &g_sens);
        alwan_spd_destroy(ctx, &b_sens);
        return status;
    }

    status = alwan_spd_resample(&g_resampled, ctx, &g_sens, spd->wavelength_min, spd->wavelength_max,
                                spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &r_sens);
        alwan_spd_destroy(ctx, &g_sens);
        alwan_spd_destroy(ctx, &b_sens);
        alwan_spd_destroy(ctx, &r_resampled);
        return status;
    }

    status = alwan_spd_resample(&b_resampled, ctx, &b_sens, spd->wavelength_min, spd->wavelength_max,
                                spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &r_sens);
        alwan_spd_destroy(ctx, &g_sens);
        alwan_spd_destroy(ctx, &b_sens);
        alwan_spd_destroy(ctx, &r_resampled);
        alwan_spd_destroy(ctx, &g_resampled);
        return status;
    }

    /* Allocate temporary arrays for products (with overflow protection) */
    size_t alloc_size = alwan_safe_array_size(spd->count, sizeof(alwan_scalar));
    if (alloc_size == 0) {
        alwan_spd_destroy(ctx, &r_sens);
        alwan_spd_destroy(ctx, &g_sens);
        alwan_spd_destroy(ctx, &b_sens);
        alwan_spd_destroy(ctx, &r_resampled);
        alwan_spd_destroy(ctx, &g_resampled);
        alwan_spd_destroy(ctx, &b_resampled);
        return ALWAN_E_NOMEM;
    }
    alwan_scalar *prod_r = (alwan_scalar *)ALWAN_ALLOC(alloc_size, sizeof(alwan_scalar));
    alwan_scalar *prod_g = (alwan_scalar *)ALWAN_ALLOC(alloc_size, sizeof(alwan_scalar));
    alwan_scalar *prod_b = (alwan_scalar *)ALWAN_ALLOC(alloc_size, sizeof(alwan_scalar));

    if (!prod_r || !prod_g || !prod_b) {
        if (prod_r) ALWAN_FREE(prod_r);
        if (prod_g) ALWAN_FREE(prod_g);
        if (prod_b) ALWAN_FREE(prod_b);
        alwan_spd_destroy(ctx, &r_sens);
        alwan_spd_destroy(ctx, &g_sens);
        alwan_spd_destroy(ctx, &b_sens);
        alwan_spd_destroy(ctx, &r_resampled);
        alwan_spd_destroy(ctx, &g_resampled);
        alwan_spd_destroy(ctx, &b_resampled);
        return ALWAN_E_NOMEM;
    }

    /* Compute products: SPD * illuminant * sensitivity */
    for (size_t i = 0; i < spd->count; i++) {
        alwan_scalar spd_value = spd->values[i];

        /* If illuminant provided, multiply by it */
        if (illuminant) {
            alwan_scalar wavelength = spd_wavelength_at(spd, i);
            alwan_scalar illum_value = spd_interpolate(illuminant, wavelength, ALWAN_RESAMPLE_LINEAR,
                                                  ALWAN_EXTRAPOLATE_ZERO);
            spd_value *= illum_value;
        }

        prod_r[i] = spd_value * r_resampled.values[i];
        prod_g[i] = spd_value * g_resampled.values[i];
        prod_b[i] = spd_value * b_resampled.values[i];
    }

    /* Integrate to get RGB (stored in XYZ output) */
    alwan_scalar dx = (spd->wavelength_max - spd->wavelength_min) / (alwan_scalar)(spd->count - 1);
    if (spd->count == 1) dx = ALWAN_LITERAL(1.0);

    if (method == ALWAN_INTEGRATE_TRAPEZOID) {
        xyz_out->x = integrate_trapezoid(prod_r, spd->count, dx);
        xyz_out->y = integrate_trapezoid(prod_g, spd->count, dx);
        xyz_out->z = integrate_trapezoid(prod_b, spd->count, dx);
    } else {
        xyz_out->x = integrate_simpson(prod_r, spd->count, dx);
        xyz_out->y = integrate_simpson(prod_g, spd->count, dx);
        xyz_out->z = integrate_simpson(prod_b, spd->count, dx);
    }

    /* Cleanup */
    ALWAN_FREE(prod_r);
    ALWAN_FREE(prod_g);
    ALWAN_FREE(prod_b);
    alwan_spd_destroy(ctx, &r_sens);
    alwan_spd_destroy(ctx, &g_sens);
    alwan_spd_destroy(ctx, &b_sens);
    alwan_spd_destroy(ctx, &r_resampled);
    alwan_spd_destroy(ctx, &g_resampled);
    alwan_spd_destroy(ctx, &b_resampled);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Spectral Shape Descriptors
 * ---------------------------------------------------------------- */

int alwan_spd_analyze_shape(alwan_spd_shape *shape_out, alwan_spd const *spd) {
    if (!spd || !shape_out || spd->count == 0) {
        return ALWAN_E_INVALID;
    }

    /* Find peak wavelength and value */
    size_t peak_idx = 0;
    alwan_scalar peak_val = spd->values[0];
    for (size_t i = 1; i < spd->count; i++) {
        if (spd->values[i] > peak_val) {
            peak_val = spd->values[i];
            peak_idx = i;
        }
    }

    shape_out->peak_value = peak_val;
    shape_out->peak_wavelength = spd_wavelength_at(spd, peak_idx);

    /* Compute FWHM (Full Width at Half Maximum) */
    alwan_scalar half_max = peak_val * ALWAN_LITERAL(0.5);

    /* Find left edge (below peak) */
    size_t left_idx = peak_idx;
    while (left_idx > 0 && spd->values[left_idx] >= half_max) {
        left_idx--;
    }

    /* Find right edge (above peak) */
    size_t right_idx = peak_idx;
    while (right_idx < spd->count - 1 && spd->values[right_idx] >= half_max) {
        right_idx++;
    }

    /* Linear interpolation for more accurate FWHM edges */
    alwan_scalar left_wl = spd_wavelength_at(spd, left_idx);
    if (left_idx < peak_idx && spd->values[left_idx + 1] != spd->values[left_idx]) {
        alwan_scalar t = (half_max - spd->values[left_idx]) /
                         (spd->values[left_idx + 1] - spd->values[left_idx]);
        left_wl = spd_wavelength_at(spd, left_idx) +
                  t * (spd_wavelength_at(spd, left_idx + 1) - spd_wavelength_at(spd, left_idx));
    }

    alwan_scalar right_wl = spd_wavelength_at(spd, right_idx);
    if (right_idx > peak_idx && right_idx > 0 && spd->values[right_idx - 1] != spd->values[right_idx]) {
        alwan_scalar t = (half_max - spd->values[right_idx]) /
                         (spd->values[right_idx - 1] - spd->values[right_idx]);
        right_wl = spd_wavelength_at(spd, right_idx) +
                   t * (spd_wavelength_at(spd, right_idx - 1) - spd_wavelength_at(spd, right_idx));
    }

    shape_out->fwhm = right_wl - left_wl;
    if (shape_out->fwhm < ALWAN_LITERAL(0.0)) {
        shape_out->fwhm = ALWAN_LITERAL(0.0);
    }

    /* Compute centroid (weighted mean wavelength) */
    alwan_scalar sum_weighted = ALWAN_LITERAL(0.0);
    alwan_scalar sum_values = ALWAN_LITERAL(0.0);

    for (size_t i = 0; i < spd->count; i++) {
        alwan_scalar wl = spd_wavelength_at(spd, i);
        sum_weighted += wl * spd->values[i];
        sum_values += spd->values[i];
    }

    if (sum_values > ALWAN_LITERAL(0.0)) {
        shape_out->centroid = sum_weighted / sum_values;
    } else {
        shape_out->centroid = (spd->wavelength_min + spd->wavelength_max) * ALWAN_LITERAL(0.5);
    }

    /* Compute bandwidth (total wavelength range) */
    shape_out->bandwidth = spd->wavelength_max - spd->wavelength_min;

    return ALWAN_OK;
}
