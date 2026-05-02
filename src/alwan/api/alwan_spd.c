/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Spectral Power Distributions (SPD)
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_spd_core.h"
#include <string.h>

/* ----------------------------------------------------------------
 * SPD Creation and Destruction
 * ---------------------------------------------------------------- */

int alwan_spd_create_f64(alwan_spd_f64 *out, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, alwan_ctx *ctx) {
    if (!out || count == 0 || wavelength_min >= wavelength_max) {
        return ALWAN_E_INVALID;
    }

    /* Check for allocation size overflow */
    size_t alloc_size = alwan_safe_array_size(count, sizeof(alwan_f64));
    if (alloc_size == 0) {
        return ALWAN_E_NOMEM;  /* Overflow would occur */
    }

    /* Allocate values array */
    alwan_f64 *values = (alwan_f64 *)ALWAN_ALLOC(alloc_size, sizeof(alwan_f64));
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

    (void)ctx;
    return ALWAN_OK;
}

void alwan_spd_destroy_f64(alwan_spd_f64 *spd, alwan_ctx *ctx) {
    if (spd && spd->values) {
        ALWAN_FREE(spd->values);
        spd->values = NULL;
        spd->count = 0;
    }
    (void)ctx;
}

/* ----------------------------------------------------------------
 * Get wavelength at index
 * ---------------------------------------------------------------- */

static inline alwan_f64 spd_wavelength_at(alwan_spd_f64 const *spd, size_t index) {
    if (spd->count <= 1) {
        return spd->wavelength_min;
    }
    alwan_f64 t = (alwan_f64)index / (alwan_f64)(spd->count - 1);
    return spd->wavelength_min + t * (spd->wavelength_max - spd->wavelength_min);
}

/* Interpolate SPD value at wavelength with extrapolation */
static alwan_f64 spd_interpolate(alwan_spd_f64 const *spd, alwan_f64 wavelength,
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
            alwan_f64 wl0 = spd_wavelength_at(spd, 0);
            alwan_f64 wl1 = spd_wavelength_at(spd, 1);
            alwan_f64 slope = (spd->values[1] - spd->values[0]) / (wl1 - wl0);
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
            alwan_f64 wl_n2 = spd_wavelength_at(spd, n - 2);
            alwan_f64 wl_n1 = spd_wavelength_at(spd, n - 1);
            alwan_f64 slope = (spd->values[n - 1] - spd->values[n - 2]) / (wl_n1 - wl_n2);
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
    alwan_f64 span = spd->wavelength_max - spd->wavelength_min;
    alwan_f64 t = (wavelength - spd->wavelength_min) / span;
    alwan_f64 f_index = t * (alwan_f64)(spd->count - 1);

    /* Clamp to valid range */
    if (f_index <= ALWAN_LITERAL(0.0)) {
        return spd->values[0];
    }
    if (f_index >= (alwan_f64)(spd->count - 1)) {
        return spd->values[spd->count - 1];
    }

    size_t i0 = (size_t)f_index;
    alwan_f64 frac = f_index - (alwan_f64)i0;

    if (method == ALWAN_RESAMPLE_LINEAR) {
        /* Linear interpolation */
        return spd->values[i0] * (ALWAN_LITERAL(1.0) - frac) + spd->values[i0 + 1] * frac;
    } else {
        /* Catmull-Rom spline interpolation */
        alwan_f64 p0 = (i0 > 0) ? spd->values[i0 - 1] : spd->values[i0];
        alwan_f64 p1 = spd->values[i0];
        alwan_f64 p2 = spd->values[i0 + 1];
        alwan_f64 p3 = (i0 + 2 < spd->count) ? spd->values[i0 + 2] : spd->values[i0 + 1];

        alwan_f64 t2 = frac * frac;
        alwan_f64 t3 = t2 * frac;

        alwan_f64 result = ALWAN_LITERAL(0.5) * (
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

int alwan_spd_resample_f64(alwan_spd_f64 *dst, alwan_spd_f64 const *src, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, alwan_resample_method method, alwan_extrapolate_mode extrapolate, alwan_ctx *ctx) {
    if (!src || !dst || count == 0 || wavelength_min >= wavelength_max) {
        return ALWAN_E_INVALID;
    }

    /* Create destination SPD */
    int status = alwan_spd_create_f64(dst, wavelength_min, wavelength_max, count, ctx);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Resample each point */
    for (size_t i = 0; i < count; i++) {
        alwan_f64 wavelength = spd_wavelength_at(dst, i);
        dst->values[i] = spd_interpolate(src, wavelength, method, extrapolate);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Illuminant Loading
 * ---------------------------------------------------------------- */

int alwan_spd_illuminant_f64(alwan_spd_f64 *out, alwan_illuminant ill, alwan_ctx *ctx) {
    if (!out) {
        return ALWAN_E_INVALID;
    }

    /* Create SPD structure (360-830nm, 1nm steps = 471 samples) */
    int status = alwan_spd_create_f64(out, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471, ctx);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Load illuminant SPD data from embedded CSV */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV

    switch (ill) {
        case ALWAN_ILLUMINANT_A: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/A_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D50: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/D50_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D55: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/D55_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D65: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/D65_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_E: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/E_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F1: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/F1_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F2: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/F2_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F3: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/F3_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F4: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/F4_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F5: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/F5_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F6: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/F6_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F7: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/F7_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F8: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/F8_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F9: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/F9_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F10: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/F10_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F11: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/F11_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_F12: {
            static alwan_f64 const data[] = {
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
            static alwan_f64 const data[] = {
#include "../data/illuminants/B_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_C: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/C_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D60: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/D60_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D75: {
            static alwan_f64 const data[] = {
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
            static alwan_f64 const data[] = {
#include "../data/illuminants/D40_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D45: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/D45_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_D93: {
            static alwan_f64 const data[] = {
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
            static alwan_f64 const data[] = {
#include "../data/illuminants/LED-B1_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_B2: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/LED-B2_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_B3: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/LED-B3_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_B4: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/LED-B4_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_B5: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/LED-B5_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_BH1: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/LED-BH1_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_RGB1: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/LED-RGB1_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_V1: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/LED-V1_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_LED_V2: {
            static alwan_f64 const data[] = {
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
            static alwan_f64 const data[] = {
#include "../data/illuminants/HP1_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_HP2: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/HP2_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_HP3: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/HP3_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_HP4: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/HP4_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }
        case ALWAN_ILLUMINANT_HP5: {
            static alwan_f64 const data[] = {
#include "../data/illuminants/HP5_360_830_1nm.csv"
            };
            size_t const n = sizeof(data) / sizeof(data[0]);
            for (size_t i = 0; i < n && i < out->count; i++) {
                out->values[i] = data[i];
            }
            break;
        }

        default:
            alwan_spd_destroy_f64(out, ctx);
            return ALWAN_E_INVALID;
    }

    ALWAN_DIAG_POP

    (void)ctx;
    return ALWAN_OK;
}

/* Generate blackbody (Planckian) SPD using Planck's law
 * Spectral radiance: L(lambda,T) = c1 / (lambda^5 * (exp(c2/(lambda*T)) - 1))
 * where c1 = 3.741771e-16 W*m^2, c2 = 1.4388e-2 m*K */
int alwan_spd_blackbody_f64(alwan_spd_f64 *out, alwan_f64 temperature_K, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, alwan_ctx *ctx) {
    if (!out || count == 0) {
        return ALWAN_E_INVALID;
    }

    /* Validate temperature range (1000-25000K typical for lighting) */
    if (temperature_K < ALWAN_LITERAL(1000.0) || temperature_K > ALWAN_LITERAL(25000.0)) {
        return ALWAN_E_INVALID;
    }

    /* Create SPD structure */
    int status = alwan_spd_create_f64(out, wavelength_min, wavelength_max, count, ctx);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Calculate wavelength step */
    alwan_f64 const step = (wavelength_max - wavelength_min) / (alwan_f64)(count - 1);

    /* Compute Planckian spectral radiance for each wavelength */
    for (size_t i = 0; i < count; i++) {
        alwan_f64 wavelength_nm = wavelength_min + (alwan_f64)i * step;
        alwan_f64 wavelength_m = wavelength_nm * ALWAN_LITERAL(1e-9);  /* Convert nm to meters */
        out->values[i] = spd_planck_radiance_f64_v(wavelength_m, temperature_K);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Observer CMF Loading
 * ---------------------------------------------------------------- */

static int load_observer_cmf(alwan_ctx *ctx,
                              alwan_observer_type observer,
                              alwan_spd_f64 *x_bar,
                              alwan_spd_f64 *y_bar,
                              alwan_spd_f64 *z_bar) {
    /* Determine wavelength range and sample count based on observer */
    alwan_f64 wl_min, wl_max;
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

    status = alwan_spd_create_f64(x_bar, wl_min, wl_max, count, ctx);
    if (status != ALWAN_OK) return status;

    status = alwan_spd_create_f64(y_bar, wl_min, wl_max, count, ctx);
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(x_bar, ctx);
        return status;
    }

    status = alwan_spd_create_f64(z_bar, wl_min, wl_max, count, ctx);
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(x_bar, ctx);
        alwan_spd_destroy_f64(y_bar, ctx);
        return status;
    }

    /* Load CMF data from embedded CSV */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV

    if (observer == ALWAN_OBSERVER_CIE_1931_2DEG) {
        /* CIE 1931 2° Standard Observer */
        static alwan_f64 const x_data[] = {
#include "../data/cmf/cie_1931_2deg_x_360_830_1nm.csv"
        };
        static alwan_f64 const y_data[] = {
#include "../data/cmf/cie_1931_2deg_y_360_830_1nm.csv"
        };
        static alwan_f64 const z_data[] = {
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
        static alwan_f64 const x_data[] = {
#include "../data/cmf/cie_1964_10deg_x_360_830_1nm.csv"
        };
        static alwan_f64 const y_data[] = {
#include "../data/cmf/cie_1964_10deg_y_360_830_1nm.csv"
        };
        static alwan_f64 const z_data[] = {
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
        static alwan_f64 const x_data[] = {
#include "../data/cmf/cie_2012_2deg_x_360_830_1nm.csv"
        };
        static alwan_f64 const y_data[] = {
#include "../data/cmf/cie_2012_2deg_y_360_830_1nm.csv"
        };
        static alwan_f64 const z_data[] = {
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
        static alwan_f64 const x_data[] = {
#include "../data/cmf/cie_2012_10deg_x_360_830_1nm.csv"
        };
        static alwan_f64 const y_data[] = {
#include "../data/cmf/cie_2012_10deg_y_360_830_1nm.csv"
        };
        static alwan_f64 const z_data[] = {
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
        static alwan_f64 const x_data[] = {
#include "../data/cmf/stockman_sharpe_2deg_x_360_830_1nm.csv"
        };
        static alwan_f64 const y_data[] = {
#include "../data/cmf/stockman_sharpe_2deg_y_360_830_1nm.csv"
        };
        static alwan_f64 const z_data[] = {
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
        static alwan_f64 const x_data[] = {
#include "../data/cmf/cie_2015_2deg_x_360_830_1nm.csv"
        };
        static alwan_f64 const y_data[] = {
#include "../data/cmf/cie_2015_2deg_y_360_830_1nm.csv"
        };
        static alwan_f64 const z_data[] = {
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
        static alwan_f64 const x_data[] = {
#include "../data/cmf/cie_2015_10deg_x_360_830_1nm.csv"
        };
        static alwan_f64 const y_data[] = {
#include "../data/cmf/cie_2015_10deg_y_360_830_1nm.csv"
        };
        static alwan_f64 const z_data[] = {
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
        static alwan_f64 const r_data[] = {
#include "../data/cmf/wright_guild_1931_r_360_830_1nm.csv"
        };
        static alwan_f64 const g_data[] = {
#include "../data/cmf/wright_guild_1931_g_360_830_1nm.csv"
        };
        static alwan_f64 const b_data[] = {
#include "../data/cmf/wright_guild_1931_b_360_830_1nm.csv"
        };

        size_t const n = sizeof(r_data) / sizeof(r_data[0]);
        for (size_t i = 0; i < n && i < x_bar->count; i++) {
            x_bar->values[i] = r_data[i];
            y_bar->values[i] = g_data[i];
            z_bar->values[i] = b_data[i];
        }
    } else {
        alwan_spd_destroy_f64(x_bar, ctx);
        alwan_spd_destroy_f64(y_bar, ctx);
        alwan_spd_destroy_f64(z_bar, ctx);
        return ALWAN_E_INVALID;
    }

    ALWAN_DIAG_POP
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ Integration from SPD
 * ---------------------------------------------------------------- */

/* Trapezoidal integration */
static alwan_f64 integrate_trapezoid(alwan_f64 const *values, size_t count, alwan_f64 dx) {
    if (count == 0) return ALWAN_LITERAL(0.0);
    if (count == 1) return values[0] * dx;

    alwan_f64 sum = ALWAN_LITERAL(0.5) * (values[0] + values[count - 1]);
    for (size_t i = 1; i < count - 1; i++) {
        sum += values[i];
    }
    return sum * dx;
}

/* Simpson's 1/3 rule integration */
static alwan_f64 integrate_simpson(alwan_f64 const *values, size_t count, alwan_f64 dx) {
    if (count == 0) return ALWAN_LITERAL(0.0);
    if (count == 1) return values[0] * dx;
    if (count == 2) return ALWAN_LITERAL(0.5) * (values[0] + values[1]) * dx;

    /* Simpson's rule requires odd number of points */
    size_t n = count;
    int use_trapezoid_last = (count % 2 == 0);
    if (use_trapezoid_last) n--;

    alwan_f64 sum = values[0] + values[n - 1];
    for (size_t i = 1; i < n - 1; i += 2) {
        sum += ALWAN_LITERAL(4.0) * values[i];
    }
    for (size_t i = 2; i < n - 1; i += 2) {
        sum += ALWAN_LITERAL(2.0) * values[i];
    }
    alwan_f64 result = sum * dx / ALWAN_LITERAL(3.0);

    /* Add last interval with trapezoid if needed */
    if (use_trapezoid_last) {
        result += ALWAN_LITERAL(0.5) * (values[n - 1] + values[n]) * dx;
    }

    return result;
}

int alwan_xyz_from_spd_f64(alwan_xyz_f64 *xyz_out, alwan_spd_f64 const *spd, alwan_spd_f64 const *illuminant, alwan_observer_type observer, alwan_integrate_method method, alwan_f64 bandpass_nm, alwan_ctx *ctx) {
    if (!spd || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    /* Load observer CMFs */
    alwan_spd_f64 x_bar, y_bar, z_bar;
    int status = load_observer_cmf(ctx, observer, &x_bar, &y_bar, &z_bar);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Resample CMFs to match SPD wavelength range (use constant extrapolation for smooth CMFs) */
    alwan_spd_f64 x_bar_resampled, y_bar_resampled, z_bar_resampled;
    status = alwan_spd_resample_f64(&x_bar_resampled, &x_bar, spd->wavelength_min, spd->wavelength_max, spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT, ctx);
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&x_bar, ctx);
        alwan_spd_destroy_f64(&y_bar, ctx);
        alwan_spd_destroy_f64(&z_bar, ctx);
        return status;
    }

    status = alwan_spd_resample_f64(&y_bar_resampled, &y_bar, spd->wavelength_min, spd->wavelength_max, spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT, ctx);
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&x_bar, ctx);
        alwan_spd_destroy_f64(&y_bar, ctx);
        alwan_spd_destroy_f64(&z_bar, ctx);
        alwan_spd_destroy_f64(&x_bar_resampled, ctx);
        return status;
    }

    status = alwan_spd_resample_f64(&z_bar_resampled, &z_bar, spd->wavelength_min, spd->wavelength_max, spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT, ctx);
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&x_bar, ctx);
        alwan_spd_destroy_f64(&y_bar, ctx);
        alwan_spd_destroy_f64(&z_bar, ctx);
        alwan_spd_destroy_f64(&x_bar_resampled, ctx);
        alwan_spd_destroy_f64(&y_bar_resampled, ctx);
        return status;
    }

    /* Allocate temporary arrays for products (with overflow protection) */
    size_t alloc_size = alwan_safe_array_size(spd->count, sizeof(alwan_f64));
    if (alloc_size == 0) {
        alwan_spd_destroy_f64(&x_bar, ctx);
        alwan_spd_destroy_f64(&y_bar, ctx);
        alwan_spd_destroy_f64(&z_bar, ctx);
        alwan_spd_destroy_f64(&x_bar_resampled, ctx);
        alwan_spd_destroy_f64(&y_bar_resampled, ctx);
        alwan_spd_destroy_f64(&z_bar_resampled, ctx);
        return ALWAN_E_NOMEM;
    }
    alwan_f64 *prod_x = (alwan_f64 *)ALWAN_ALLOC(alloc_size, sizeof(alwan_f64));
    alwan_f64 *prod_y = (alwan_f64 *)ALWAN_ALLOC(alloc_size, sizeof(alwan_f64));
    alwan_f64 *prod_z = (alwan_f64 *)ALWAN_ALLOC(alloc_size, sizeof(alwan_f64));

    if (!prod_x || !prod_y || !prod_z) {
        if (prod_x) ALWAN_FREE(prod_x);
        if (prod_y) ALWAN_FREE(prod_y);
        if (prod_z) ALWAN_FREE(prod_z);
        alwan_spd_destroy_f64(&x_bar, ctx);
        alwan_spd_destroy_f64(&y_bar, ctx);
        alwan_spd_destroy_f64(&z_bar, ctx);
        alwan_spd_destroy_f64(&x_bar_resampled, ctx);
        alwan_spd_destroy_f64(&y_bar_resampled, ctx);
        alwan_spd_destroy_f64(&z_bar_resampled, ctx);
        return ALWAN_E_NOMEM;
    }

    /* Compute products: SPD * illuminant * CMF */
    for (size_t i = 0; i < spd->count; i++) {
        alwan_f64 spd_value = spd->values[i];

        /* If illuminant provided, multiply by it */
        if (illuminant) {
            alwan_f64 wavelength = spd_wavelength_at(spd, i);
            alwan_f64 illum_value = spd_interpolate(illuminant, wavelength, ALWAN_RESAMPLE_LINEAR,
                                                  ALWAN_EXTRAPOLATE_ZERO);
            spd_value *= illum_value;
        }

        prod_x[i] = spd_value * x_bar_resampled.values[i];
        prod_y[i] = spd_value * y_bar_resampled.values[i];
        prod_z[i] = spd_value * z_bar_resampled.values[i];
    }

    /* Bandpass correction (Stearns & Stearns 1988) not implemented; parameter reserved. */
    (void)bandpass_nm;

    /* Integrate to get XYZ */
    alwan_f64 dx = (spd->wavelength_max - spd->wavelength_min) / (alwan_f64)(spd->count - 1);
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
    alwan_spd_destroy_f64(&x_bar, ctx);
    alwan_spd_destroy_f64(&y_bar, ctx);
    alwan_spd_destroy_f64(&z_bar, ctx);
    alwan_spd_destroy_f64(&x_bar_resampled, ctx);
    alwan_spd_destroy_f64(&y_bar_resampled, ctx);
    alwan_spd_destroy_f64(&z_bar_resampled, ctx);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Camera Sensitivity Functions
 * ---------------------------------------------------------------- */

int alwan_spd_camera_sensitivity_f64(alwan_spd_f64 *spd_r, alwan_spd_f64 *spd_g, alwan_spd_f64 *spd_b, alwan_camera_sensitivity camera, alwan_ctx *ctx) {
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
            static alwan_f64 const data_r[] = {
#include "../data/camera_sensitivities/nikon_5100_r.csv"
            };
            static alwan_f64 const data_g[] = {
#include "../data/camera_sensitivities/nikon_5100_g.csv"
            };
            static alwan_f64 const data_b[] = {
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
            static alwan_f64 const data_r[] = {
#include "../data/camera_sensitivities/sigma_sdmerill_r.csv"
            };
            static alwan_f64 const data_g[] = {
#include "../data/camera_sensitivities/sigma_sdmerill_g.csv"
            };
            static alwan_f64 const data_b[] = {
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

    (void)ctx;
    return ALWAN_OK;
}

int alwan_xyz_from_spd_camera_f64(alwan_xyz_f64 *xyz_out, alwan_spd_f64 const *spd, alwan_spd_f64 const *illuminant, alwan_camera_sensitivity camera, alwan_integrate_method method, alwan_ctx *ctx) {
    if (!spd || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    /* Load camera RGB sensitivities */
    alwan_spd_f64 r_sens, g_sens, b_sens;
    int status = alwan_spd_create_f64(&r_sens, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471, ctx);
    if (status != ALWAN_OK) {
        return status;
    }
    status = alwan_spd_create_f64(&g_sens, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471, ctx);
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&r_sens, ctx);
        return status;
    }
    status = alwan_spd_create_f64(&b_sens, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471, ctx);
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&r_sens, ctx);
        alwan_spd_destroy_f64(&g_sens, ctx);
        return status;
    }

    status = alwan_spd_camera_sensitivity_f64(&r_sens, &g_sens, &b_sens, camera, ctx);
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&r_sens, ctx);
        alwan_spd_destroy_f64(&g_sens, ctx);
        alwan_spd_destroy_f64(&b_sens, ctx);
        return status;
    }

    /* Resample sensitivities to match SPD wavelength range */
    alwan_spd_f64 r_resampled, g_resampled, b_resampled;
    status = alwan_spd_resample_f64(&r_resampled, &r_sens, spd->wavelength_min, spd->wavelength_max, spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO, ctx);
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&r_sens, ctx);
        alwan_spd_destroy_f64(&g_sens, ctx);
        alwan_spd_destroy_f64(&b_sens, ctx);
        return status;
    }

    status = alwan_spd_resample_f64(&g_resampled, &g_sens, spd->wavelength_min, spd->wavelength_max, spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO, ctx);
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&r_sens, ctx);
        alwan_spd_destroy_f64(&g_sens, ctx);
        alwan_spd_destroy_f64(&b_sens, ctx);
        alwan_spd_destroy_f64(&r_resampled, ctx);
        return status;
    }

    status = alwan_spd_resample_f64(&b_resampled, &b_sens, spd->wavelength_min, spd->wavelength_max, spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO, ctx);
    if (status != ALWAN_OK) {
        alwan_spd_destroy_f64(&r_sens, ctx);
        alwan_spd_destroy_f64(&g_sens, ctx);
        alwan_spd_destroy_f64(&b_sens, ctx);
        alwan_spd_destroy_f64(&r_resampled, ctx);
        alwan_spd_destroy_f64(&g_resampled, ctx);
        return status;
    }

    /* Allocate temporary arrays for products (with overflow protection) */
    size_t alloc_size = alwan_safe_array_size(spd->count, sizeof(alwan_f64));
    if (alloc_size == 0) {
        alwan_spd_destroy_f64(&r_sens, ctx);
        alwan_spd_destroy_f64(&g_sens, ctx);
        alwan_spd_destroy_f64(&b_sens, ctx);
        alwan_spd_destroy_f64(&r_resampled, ctx);
        alwan_spd_destroy_f64(&g_resampled, ctx);
        alwan_spd_destroy_f64(&b_resampled, ctx);
        return ALWAN_E_NOMEM;
    }
    alwan_f64 *prod_r = (alwan_f64 *)ALWAN_ALLOC(alloc_size, sizeof(alwan_f64));
    alwan_f64 *prod_g = (alwan_f64 *)ALWAN_ALLOC(alloc_size, sizeof(alwan_f64));
    alwan_f64 *prod_b = (alwan_f64 *)ALWAN_ALLOC(alloc_size, sizeof(alwan_f64));

    if (!prod_r || !prod_g || !prod_b) {
        if (prod_r) ALWAN_FREE(prod_r);
        if (prod_g) ALWAN_FREE(prod_g);
        if (prod_b) ALWAN_FREE(prod_b);
        alwan_spd_destroy_f64(&r_sens, ctx);
        alwan_spd_destroy_f64(&g_sens, ctx);
        alwan_spd_destroy_f64(&b_sens, ctx);
        alwan_spd_destroy_f64(&r_resampled, ctx);
        alwan_spd_destroy_f64(&g_resampled, ctx);
        alwan_spd_destroy_f64(&b_resampled, ctx);
        return ALWAN_E_NOMEM;
    }

    /* Compute products: SPD * illuminant * sensitivity */
    for (size_t i = 0; i < spd->count; i++) {
        alwan_f64 spd_value = spd->values[i];

        /* If illuminant provided, multiply by it */
        if (illuminant) {
            alwan_f64 wavelength = spd_wavelength_at(spd, i);
            alwan_f64 illum_value = spd_interpolate(illuminant, wavelength, ALWAN_RESAMPLE_LINEAR,
                                                  ALWAN_EXTRAPOLATE_ZERO);
            spd_value *= illum_value;
        }

        prod_r[i] = spd_value * r_resampled.values[i];
        prod_g[i] = spd_value * g_resampled.values[i];
        prod_b[i] = spd_value * b_resampled.values[i];
    }

    /* Integrate to get RGB (stored in XYZ output) */
    alwan_f64 dx = (spd->wavelength_max - spd->wavelength_min) / (alwan_f64)(spd->count - 1);
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
    alwan_spd_destroy_f64(&r_sens, ctx);
    alwan_spd_destroy_f64(&g_sens, ctx);
    alwan_spd_destroy_f64(&b_sens, ctx);
    alwan_spd_destroy_f64(&r_resampled, ctx);
    alwan_spd_destroy_f64(&g_resampled, ctx);
    alwan_spd_destroy_f64(&b_resampled, ctx);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Spectral Shape Descriptors
 * ---------------------------------------------------------------- */

int alwan_spd_analyze_shape_f64(alwan_spd_shape_f64 *shape_out, alwan_spd_f64 const *spd) {
    if (!spd || !shape_out || spd->count == 0) {
        return ALWAN_E_INVALID;
    }

    /* Find peak wavelength and value */
    size_t peak_idx = 0;
    alwan_f64 peak_val = spd->values[0];
    for (size_t i = 1; i < spd->count; i++) {
        if (spd->values[i] > peak_val) {
            peak_val = spd->values[i];
            peak_idx = i;
        }
    }

    shape_out->peak_value = peak_val;
    shape_out->peak_wavelength = spd_wavelength_at(spd, peak_idx);

    /* Compute FWHM (Full Width at Half Maximum) */
    alwan_f64 half_max = peak_val * ALWAN_LITERAL(0.5);

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
    alwan_f64 left_wl = spd_wavelength_at(spd, left_idx);
    if (left_idx < peak_idx && spd->values[left_idx + 1] != spd->values[left_idx]) {
        alwan_f64 t = (half_max - spd->values[left_idx]) /
                         (spd->values[left_idx + 1] - spd->values[left_idx]);
        left_wl = spd_wavelength_at(spd, left_idx) +
                  t * (spd_wavelength_at(spd, left_idx + 1) - spd_wavelength_at(spd, left_idx));
    }

    alwan_f64 right_wl = spd_wavelength_at(spd, right_idx);
    if (right_idx > peak_idx && right_idx > 0 && spd->values[right_idx - 1] != spd->values[right_idx]) {
        alwan_f64 t = (half_max - spd->values[right_idx]) /
                         (spd->values[right_idx - 1] - spd->values[right_idx]);
        right_wl = spd_wavelength_at(spd, right_idx) +
                   t * (spd_wavelength_at(spd, right_idx - 1) - spd_wavelength_at(spd, right_idx));
    }

    shape_out->fwhm = right_wl - left_wl;
    if (shape_out->fwhm < ALWAN_LITERAL(0.0)) {
        shape_out->fwhm = ALWAN_LITERAL(0.0);
    }

    /* Compute centroid (weighted mean wavelength) */
    alwan_f64 sum_weighted = ALWAN_LITERAL(0.0);
    alwan_f64 sum_values = ALWAN_LITERAL(0.0);

    for (size_t i = 0; i < spd->count; i++) {
        alwan_f64 wl = spd_wavelength_at(spd, i);
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

/* ================================================================
 * f32 wrappers
 *
 * SPD operations route through f64 primitives because the CMF tables,
 * integrators (Simpson/Trapezoid), and scientific constants are all
 * stored in f64. The wrappers allocate a temporary f64 SPD, call the
 * f64 function, and convert values to f32.
 * ================================================================ */

int alwan_spd_create_f32(alwan_spd_f32 *out, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, alwan_ctx *ctx) {
    if (!out || count == 0 || wavelength_min >= wavelength_max) {
        return ALWAN_E_INVALID;
    }

    size_t alloc_size = alwan_safe_array_size(count, sizeof(alwan_f32));
    if (alloc_size == 0) return ALWAN_E_NOMEM;

    alwan_f32 *values = (alwan_f32 *)ALWAN_ALLOC(alloc_size, sizeof(alwan_f32));
    if (!values) return ALWAN_E_NOMEM;

    for (size_t i = 0; i < count; i++) values[i] = 0.0f;

    out->values = values;
    out->wavelength_min = wavelength_min;
    out->wavelength_max = wavelength_max;
    out->count = count;

    (void)ctx;
    return ALWAN_OK;
}

void alwan_spd_destroy_f32(alwan_spd_f32 *spd, alwan_ctx *ctx) {
    if (spd && spd->values) {
        ALWAN_FREE(spd->values);
        spd->values = NULL;
        spd->count = 0;
    }
    (void)ctx;
}

/* Helper: convert f32 spd shell to f64 (views the same wavelength range and count,
 * allocates and copies values). Caller must destroy with alwan_spd_destroy_f64. */
static int spd_f32_to_f64(alwan_spd_f64 *out, alwan_ctx *ctx, alwan_spd_f32 const *in) {
    int rc = alwan_spd_create_f64(out, (alwan_f64)in->wavelength_min, (alwan_f64)in->wavelength_max, in->count, ctx);
    if (rc != ALWAN_OK) return rc;
    for (size_t i = 0; i < in->count; i++) {
        out->values[i] = (alwan_f64)in->values[i];
    }
    return ALWAN_OK;
}

/* Copy values from f64 to existing f32 SPD (shell fields updated, caller must
 * have allocated f32 values array to match count). */
static void spd_f64_to_f32_values(alwan_spd_f32 *out, alwan_spd_f64 const *in) {
    out->wavelength_min = (alwan_f32)in->wavelength_min;
    out->wavelength_max = (alwan_f32)in->wavelength_max;
    /* count is already set; copy values */
    for (size_t i = 0; i < in->count && i < out->count; i++) {
        out->values[i] = (alwan_f32)in->values[i];
    }
}

int alwan_spd_illuminant_f32(alwan_spd_f32 *out, alwan_illuminant ill, alwan_ctx *ctx) {
    if (!out) return ALWAN_E_INVALID;
    alwan_spd_f64 tmp;
    int rc = alwan_spd_illuminant_f64(&tmp, ill, ctx);
    if (rc != ALWAN_OK) return rc;
    rc = alwan_spd_create_f32(out, (alwan_f32)tmp.wavelength_min, (alwan_f32)tmp.wavelength_max, tmp.count, ctx);
    if (rc == ALWAN_OK) spd_f64_to_f32_values(out, &tmp);
    alwan_spd_destroy_f64(&tmp, ctx);
    return rc;
}

int alwan_spd_blackbody_f32(alwan_spd_f32 *out, alwan_f32 temperature_K, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, alwan_ctx *ctx) {
    if (!out) return ALWAN_E_INVALID;
    alwan_spd_f64 tmp;
    int rc = alwan_spd_blackbody_f64(&tmp, (alwan_f64)temperature_K, (alwan_f64)wavelength_min, (alwan_f64)wavelength_max, count, ctx);
    if (rc != ALWAN_OK) return rc;
    rc = alwan_spd_create_f32(out, wavelength_min, wavelength_max, count, ctx);
    if (rc == ALWAN_OK) spd_f64_to_f32_values(out, &tmp);
    alwan_spd_destroy_f64(&tmp, ctx);
    return rc;
}

int alwan_spd_resample_f32(alwan_spd_f32 *dst, alwan_spd_f32 const *src, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, alwan_resample_method method, alwan_extrapolate_mode extrapolate, alwan_ctx *ctx) {
    if (!src || !dst) return ALWAN_E_INVALID;

    alwan_spd_f64 src_f64;
    int rc = spd_f32_to_f64(&src_f64, ctx, src);
    if (rc != ALWAN_OK) return rc;

    alwan_spd_f64 dst_f64;
    rc = alwan_spd_resample_f64(&dst_f64, &src_f64, (alwan_f64)wavelength_min, (alwan_f64)wavelength_max, count, method, extrapolate, ctx);
    alwan_spd_destroy_f64(&src_f64, ctx);
    if (rc != ALWAN_OK) return rc;

    rc = alwan_spd_create_f32(dst, wavelength_min, wavelength_max, count, ctx);
    if (rc == ALWAN_OK) spd_f64_to_f32_values(dst, &dst_f64);
    alwan_spd_destroy_f64(&dst_f64, ctx);
    return rc;
}

int alwan_spd_camera_sensitivity_f32(alwan_spd_f32 *spd_r, alwan_spd_f32 *spd_g, alwan_spd_f32 *spd_b, alwan_camera_sensitivity camera, alwan_ctx *ctx) {
    if (!spd_r || !spd_g || !spd_b) return ALWAN_E_INVALID;

    alwan_spd_f64 r64, g64, b64;
    int rc = alwan_spd_create_f64(&r64, 360.0, 830.0, 471, ctx);
    if (rc != ALWAN_OK) return rc;
    rc = alwan_spd_create_f64(&g64, 360.0, 830.0, 471, ctx);
    if (rc != ALWAN_OK) { alwan_spd_destroy_f64(&r64, ctx); return rc; }
    rc = alwan_spd_create_f64(&b64, 360.0, 830.0, 471, ctx);
    if (rc != ALWAN_OK) { alwan_spd_destroy_f64(&r64, ctx); alwan_spd_destroy_f64(&g64, ctx); return rc; }

    rc = alwan_spd_camera_sensitivity_f64(&r64, &g64, &b64, camera, ctx);
    if (rc == ALWAN_OK) {
        rc = alwan_spd_create_f32(spd_r, (alwan_f32)r64.wavelength_min, (alwan_f32)r64.wavelength_max, r64.count, ctx);
    }
    if (rc == ALWAN_OK) {
        rc = alwan_spd_create_f32(spd_g, (alwan_f32)g64.wavelength_min, (alwan_f32)g64.wavelength_max, g64.count, ctx);
    }
    if (rc == ALWAN_OK) {
        rc = alwan_spd_create_f32(spd_b, (alwan_f32)b64.wavelength_min, (alwan_f32)b64.wavelength_max, b64.count, ctx);
    }
    if (rc == ALWAN_OK) {
        spd_f64_to_f32_values(spd_r, &r64);
        spd_f64_to_f32_values(spd_g, &g64);
        spd_f64_to_f32_values(spd_b, &b64);
    }

    alwan_spd_destroy_f64(&r64, ctx);
    alwan_spd_destroy_f64(&g64, ctx);
    alwan_spd_destroy_f64(&b64, ctx);
    return rc;
}

int alwan_xyz_from_spd_f32(alwan_xyz_f32 *xyz_out, alwan_spd_f32 const *spd, alwan_spd_f32 const *illuminant, alwan_observer_type observer, alwan_integrate_method method, alwan_f32 bandpass_nm, alwan_ctx *ctx) {
    if (!spd || !xyz_out) return ALWAN_E_INVALID;

    alwan_spd_f64 spd_f64;
    int rc = spd_f32_to_f64(&spd_f64, ctx, spd);
    if (rc != ALWAN_OK) return rc;

    alwan_spd_f64 illum_f64;
    int have_illum = 0;
    if (illuminant) {
        rc = spd_f32_to_f64(&illum_f64, ctx, illuminant);
        if (rc != ALWAN_OK) { alwan_spd_destroy_f64(&spd_f64, ctx); return rc; }
        have_illum = 1;
    }

    alwan_xyz_f64 xyz_f64;
    rc = alwan_xyz_from_spd_f64(&xyz_f64, &spd_f64, have_illum ? &illum_f64 : NULL, observer, method, (alwan_f64)bandpass_nm, ctx);
    if (rc == ALWAN_OK) {
        xyz_out->x = (alwan_f32)xyz_f64.x;
        xyz_out->y = (alwan_f32)xyz_f64.y;
        xyz_out->z = (alwan_f32)xyz_f64.z;
    }

    alwan_spd_destroy_f64(&spd_f64, ctx);
    if (have_illum) alwan_spd_destroy_f64(&illum_f64, ctx);
    return rc;
}

int alwan_xyz_from_spd_camera_f32(alwan_xyz_f32 *xyz_out, alwan_spd_f32 const *spd, alwan_spd_f32 const *illuminant, alwan_camera_sensitivity camera, alwan_integrate_method method, alwan_ctx *ctx) {
    if (!spd || !xyz_out) return ALWAN_E_INVALID;

    alwan_spd_f64 spd_f64;
    int rc = spd_f32_to_f64(&spd_f64, ctx, spd);
    if (rc != ALWAN_OK) return rc;

    alwan_spd_f64 illum_f64;
    int have_illum = 0;
    if (illuminant) {
        rc = spd_f32_to_f64(&illum_f64, ctx, illuminant);
        if (rc != ALWAN_OK) { alwan_spd_destroy_f64(&spd_f64, ctx); return rc; }
        have_illum = 1;
    }

    alwan_xyz_f64 xyz_f64;
    rc = alwan_xyz_from_spd_camera_f64(&xyz_f64, &spd_f64, have_illum ? &illum_f64 : NULL, camera, method, ctx);
    if (rc == ALWAN_OK) {
        xyz_out->x = (alwan_f32)xyz_f64.x;
        xyz_out->y = (alwan_f32)xyz_f64.y;
        xyz_out->z = (alwan_f32)xyz_f64.z;
    }

    alwan_spd_destroy_f64(&spd_f64, ctx);
    if (have_illum) alwan_spd_destroy_f64(&illum_f64, ctx);
    return rc;
}

int alwan_spd_analyze_shape_f32(alwan_spd_shape_f32 *shape_out, alwan_spd_f32 const *spd) {
    if (!spd || !shape_out || spd->count == 0) return ALWAN_E_INVALID;

    alwan_spd_f64 spd_f64;
    int rc = spd_f32_to_f64(&spd_f64, NULL, spd);
    if (rc != ALWAN_OK) return rc;

    alwan_spd_shape_f64 shape_f64;
    rc = alwan_spd_analyze_shape_f64(&shape_f64, &spd_f64);
    if (rc == ALWAN_OK) {
        shape_out->peak_wavelength = (alwan_f32)shape_f64.peak_wavelength;
        shape_out->peak_value = (alwan_f32)shape_f64.peak_value;
        shape_out->fwhm = (alwan_f32)shape_f64.fwhm;
        shape_out->centroid = (alwan_f32)shape_f64.centroid;
        shape_out->bandwidth = (alwan_f32)shape_f64.bandwidth;
    }

    alwan_spd_destroy_f64(&spd_f64, NULL);
    return rc;
}
