/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * Spectral Power Distributions (SPD)
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * SPD Creation and Destruction
 * ---------------------------------------------------------------- */

int alwan_spd_create(alwan_ctx *ctx,
                     Scalar wavelength_min,
                     Scalar wavelength_max,
                     size_t count,
                     alwan_spd *out) {
    if (!out || count == 0 || wavelength_min >= wavelength_max) {
        return ALWAN_E_INVALID;
    }

    /* Allocate values array */
    Scalar *values = (Scalar *)ALWAN_ALLOC(count * sizeof(Scalar), sizeof(Scalar));
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

static inline Scalar spd_wavelength_at(alwan_spd const *spd, size_t index) {
    if (spd->count <= 1) {
        return spd->wavelength_min;
    }
    Scalar t = (Scalar)index / (Scalar)(spd->count - 1);
    return spd->wavelength_min + t * (spd->wavelength_max - spd->wavelength_min);
}

/* Helper: Interpolate SPD value at wavelength with extrapolation */
static Scalar spd_interpolate(alwan_spd const *spd, Scalar wavelength,
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
            Scalar wl0 = spd_wavelength_at(spd, 0);
            Scalar wl1 = spd_wavelength_at(spd, 1);
            Scalar slope = (spd->values[1] - spd->values[0]) / (wl1 - wl0);
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
            Scalar wl_n2 = spd_wavelength_at(spd, n - 2);
            Scalar wl_n1 = spd_wavelength_at(spd, n - 1);
            Scalar slope = (spd->values[n - 1] - spd->values[n - 2]) / (wl_n1 - wl_n2);
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
    Scalar span = spd->wavelength_max - spd->wavelength_min;
    Scalar t = (wavelength - spd->wavelength_min) / span;
    Scalar f_index = t * (Scalar)(spd->count - 1);

    /* Clamp to valid range */
    if (f_index <= ALWAN_LITERAL(0.0)) {
        return spd->values[0];
    }
    if (f_index >= (Scalar)(spd->count - 1)) {
        return spd->values[spd->count - 1];
    }

    size_t i0 = (size_t)f_index;
    Scalar frac = f_index - (Scalar)i0;

    if (method == ALWAN_RESAMPLE_LINEAR) {
        /* Linear interpolation */
        return spd->values[i0] * (ALWAN_LITERAL(1.0) - frac) + spd->values[i0 + 1] * frac;
    } else {
        /* Catmull-Rom spline interpolation */
        Scalar p0 = (i0 > 0) ? spd->values[i0 - 1] : spd->values[i0];
        Scalar p1 = spd->values[i0];
        Scalar p2 = spd->values[i0 + 1];
        Scalar p3 = (i0 + 2 < spd->count) ? spd->values[i0 + 2] : spd->values[i0 + 1];

        Scalar t2 = frac * frac;
        Scalar t3 = t2 * frac;

        Scalar result = ALWAN_LITERAL(0.5) * (
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

int alwan_spd_resample(alwan_ctx *ctx,
                       alwan_spd const *src,
                       Scalar wavelength_min,
                       Scalar wavelength_max,
                       size_t count,
                       alwan_resample_method method,
                       alwan_extrapolate_mode extrapolate,
                       alwan_spd *dst) {
    if (!src || !dst || count == 0 || wavelength_min >= wavelength_max) {
        return ALWAN_E_INVALID;
    }

    /* Create destination SPD */
    int status = alwan_spd_create(ctx, wavelength_min, wavelength_max, count, dst);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Resample each point */
    for (size_t i = 0; i < count; i++) {
        Scalar wavelength = spd_wavelength_at(dst, i);
        dst->values[i] = spd_interpolate(src, wavelength, method, extrapolate);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Illuminant Loading
 * ---------------------------------------------------------------- */

int alwan_spd_illuminant(alwan_ctx *ctx, char const *name, alwan_spd *out) {
    if (!name || !out) {
        return ALWAN_E_INVALID;
    }

    /* Create SPD structure (360-830nm, 1nm steps = 471 samples) */
    int status = alwan_spd_create(ctx, ALWAN_LITERAL(360.0), ALWAN_LITERAL(830.0), 471, out);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Load illuminant SPD data from embedded CSV */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV

    if (strcmp(name, "A") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/A_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "D50") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/D50_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "D55") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/D55_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "D65") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/D65_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "E") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/E_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "F1") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/F1_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "F2") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/F2_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "F3") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/F3_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "F4") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/F4_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "F5") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/F5_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "F6") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/F6_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "F7") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/F7_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "F8") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/F8_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "F9") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/F9_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "F10") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/F10_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "F11") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/F11_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else if (strcmp(name, "F12") == 0) {
        static Scalar const data[] = {
#include "../../data/illuminants/F12_360_830_1nm.csv"
        };
        size_t const n = sizeof(data) / sizeof(data[0]);
        for (size_t i = 0; i < n && i < out->count; i++) {
            out->values[i] = data[i];
        }
    } else {
        alwan_spd_destroy(ctx, out);
        return ALWAN_E_INVALID;
    }

    ALWAN_DIAG_POP

    (void)ctx;
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
    Scalar wl_min, wl_max;
    size_t count;

    if (observer == ALWAN_OBSERVER_CIE_1931_2DEG || observer == ALWAN_OBSERVER_CIE_1964_10DEG) {
        /* CIE 1931/1964: 360-830nm, 1nm steps = 471 samples */
        wl_min = ALWAN_LITERAL(360.0);
        wl_max = ALWAN_LITERAL(830.0);
        count = 471;
    } else {
        /* CIE 2012/2015: 390-830nm, 1nm steps = 441 samples */
        wl_min = ALWAN_LITERAL(390.0);
        wl_max = ALWAN_LITERAL(830.0);
        count = 441;
    }

    int status;

    status = alwan_spd_create(ctx, wl_min, wl_max, count, x_bar);
    if (status != ALWAN_OK) return status;

    status = alwan_spd_create(ctx, wl_min, wl_max, count, y_bar);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, x_bar);
        return status;
    }

    status = alwan_spd_create(ctx, wl_min, wl_max, count, z_bar);
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
        static Scalar const x_data[] = {
#include "../../data/cmf/cie_1931_2deg_x_360_830_1nm.csv"
        };
        static Scalar const y_data[] = {
#include "../../data/cmf/cie_1931_2deg_y_360_830_1nm.csv"
        };
        static Scalar const z_data[] = {
#include "../../data/cmf/cie_1931_2deg_z_360_830_1nm.csv"
        };

        size_t const n = sizeof(x_data) / sizeof(x_data[0]);
        for (size_t i = 0; i < n && i < x_bar->count; i++) {
            x_bar->values[i] = x_data[i];
            y_bar->values[i] = y_data[i];
            z_bar->values[i] = z_data[i];
        }
    } else if (observer == ALWAN_OBSERVER_CIE_1964_10DEG) {
        /* CIE 1964 10° Standard Observer */
        static Scalar const x_data[] = {
#include "../../data/cmf/cie_1964_10deg_x_360_830_1nm.csv"
        };
        static Scalar const y_data[] = {
#include "../../data/cmf/cie_1964_10deg_y_360_830_1nm.csv"
        };
        static Scalar const z_data[] = {
#include "../../data/cmf/cie_1964_10deg_z_360_830_1nm.csv"
        };

        size_t const n = sizeof(x_data) / sizeof(x_data[0]);
        for (size_t i = 0; i < n && i < x_bar->count; i++) {
            x_bar->values[i] = x_data[i];
            y_bar->values[i] = y_data[i];
            z_bar->values[i] = z_data[i];
        }
    } else if (observer == ALWAN_OBSERVER_CIE_2012_2DEG) {
        /* CIE 2012/2015 2° Standard Observer (390-830nm) */
        static Scalar const x_data[] = {
#include "../../data/cmf/cie_2012_2deg_x_360_830_1nm.csv"
        };
        static Scalar const y_data[] = {
#include "../../data/cmf/cie_2012_2deg_y_360_830_1nm.csv"
        };
        static Scalar const z_data[] = {
#include "../../data/cmf/cie_2012_2deg_z_360_830_1nm.csv"
        };

        size_t const n = sizeof(x_data) / sizeof(x_data[0]);
        for (size_t i = 0; i < n && i < x_bar->count; i++) {
            x_bar->values[i] = x_data[i];
            y_bar->values[i] = y_data[i];
            z_bar->values[i] = z_data[i];
        }
    } else if (observer == ALWAN_OBSERVER_CIE_2012_10DEG) {
        /* CIE 2012/2015 10° Standard Observer (390-830nm) */
        static Scalar const x_data[] = {
#include "../../data/cmf/cie_2012_10deg_x_360_830_1nm.csv"
        };
        static Scalar const y_data[] = {
#include "../../data/cmf/cie_2012_10deg_y_360_830_1nm.csv"
        };
        static Scalar const z_data[] = {
#include "../../data/cmf/cie_2012_10deg_z_360_830_1nm.csv"
        };

        size_t const n = sizeof(x_data) / sizeof(x_data[0]);
        for (size_t i = 0; i < n && i < x_bar->count; i++) {
            x_bar->values[i] = x_data[i];
            y_bar->values[i] = y_data[i];
            z_bar->values[i] = z_data[i];
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
static Scalar integrate_trapezoid(Scalar const *values, size_t count, Scalar dx) {
    if (count == 0) return ALWAN_LITERAL(0.0);
    if (count == 1) return values[0] * dx;

    Scalar sum = ALWAN_LITERAL(0.5) * (values[0] + values[count - 1]);
    for (size_t i = 1; i < count - 1; i++) {
        sum += values[i];
    }
    return sum * dx;
}

/* Simpson's 1/3 rule integration */
static Scalar integrate_simpson(Scalar const *values, size_t count, Scalar dx) {
    if (count == 0) return ALWAN_LITERAL(0.0);
    if (count == 1) return values[0] * dx;
    if (count == 2) return ALWAN_LITERAL(0.5) * (values[0] + values[1]) * dx;

    /* Simpson's rule requires odd number of points */
    size_t n = count;
    int use_trapezoid_last = (count % 2 == 0);
    if (use_trapezoid_last) n--;

    Scalar sum = values[0] + values[n - 1];
    for (size_t i = 1; i < n - 1; i += 2) {
        sum += ALWAN_LITERAL(4.0) * values[i];
    }
    for (size_t i = 2; i < n - 1; i += 2) {
        sum += ALWAN_LITERAL(2.0) * values[i];
    }
    Scalar result = sum * dx / ALWAN_LITERAL(3.0);

    /* Add last interval with trapezoid if needed */
    if (use_trapezoid_last) {
        result += ALWAN_LITERAL(0.5) * (values[n - 1] + values[n]) * dx;
    }

    return result;
}

int alwan_xyz_from_spd(alwan_ctx *ctx,
                       alwan_spd const *spd,
                       alwan_spd const *illuminant,
                       alwan_observer_type observer,
                       alwan_integrate_method method,
                       Scalar bandpass_nm,
                       alwan_vec3 *xyz_out) {
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
    status = alwan_spd_resample(ctx, &x_bar, spd->wavelength_min, spd->wavelength_max,
                                spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT,
                                &x_bar_resampled);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &x_bar);
        alwan_spd_destroy(ctx, &y_bar);
        alwan_spd_destroy(ctx, &z_bar);
        return status;
    }

    status = alwan_spd_resample(ctx, &y_bar, spd->wavelength_min, spd->wavelength_max,
                                spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT,
                                &y_bar_resampled);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &x_bar);
        alwan_spd_destroy(ctx, &y_bar);
        alwan_spd_destroy(ctx, &z_bar);
        alwan_spd_destroy(ctx, &x_bar_resampled);
        return status;
    }

    status = alwan_spd_resample(ctx, &z_bar, spd->wavelength_min, spd->wavelength_max,
                                spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT,
                                &z_bar_resampled);
    if (status != ALWAN_OK) {
        alwan_spd_destroy(ctx, &x_bar);
        alwan_spd_destroy(ctx, &y_bar);
        alwan_spd_destroy(ctx, &z_bar);
        alwan_spd_destroy(ctx, &x_bar_resampled);
        alwan_spd_destroy(ctx, &y_bar_resampled);
        return status;
    }

    /* Allocate temporary arrays for products */
    Scalar *prod_x = (Scalar *)ALWAN_ALLOC(spd->count * sizeof(Scalar), sizeof(Scalar));
    Scalar *prod_y = (Scalar *)ALWAN_ALLOC(spd->count * sizeof(Scalar), sizeof(Scalar));
    Scalar *prod_z = (Scalar *)ALWAN_ALLOC(spd->count * sizeof(Scalar), sizeof(Scalar));

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
        Scalar spd_value = spd->values[i];

        /* If illuminant provided, multiply by it */
        if (illuminant) {
            Scalar wavelength = spd_wavelength_at(spd, i);
            Scalar illum_value = spd_interpolate(illuminant, wavelength, ALWAN_RESAMPLE_LINEAR,
                                                  ALWAN_EXTRAPOLATE_ZERO);
            spd_value *= illum_value;
        }

        prod_x[i] = spd_value * x_bar_resampled.values[i];
        prod_y[i] = spd_value * y_bar_resampled.values[i];
        prod_z[i] = spd_value * z_bar_resampled.values[i];
    }

    /* Apply bandpass correction if requested (Stearns & Stearns 1988) */
    /* TODO(M6): Implement full Stearns & Stearns bandpass correction algorithm
     * For now, bandpass_nm parameter is accepted but not used.
     * Correction would involve:
     * 1. Computing second derivatives of reflectance spectrum
     * 2. Applying correction factor based on bandpass width
     * 3. Adjusting XYZ values accordingly */
    (void)bandpass_nm;  /* Unused for now */

    /* Integrate to get XYZ */
    Scalar dx = (spd->wavelength_max - spd->wavelength_min) / (Scalar)(spd->count - 1);
    if (spd->count == 1) dx = ALWAN_LITERAL(1.0);

    if (method == ALWAN_INTEGRATE_TRAPEZOID) {
        xyz_out->v[0] = integrate_trapezoid(prod_x, spd->count, dx);
        xyz_out->v[1] = integrate_trapezoid(prod_y, spd->count, dx);
        xyz_out->v[2] = integrate_trapezoid(prod_z, spd->count, dx);
    } else {
        xyz_out->v[0] = integrate_simpson(prod_x, spd->count, dx);
        xyz_out->v[1] = integrate_simpson(prod_y, spd->count, dx);
        xyz_out->v[2] = integrate_simpson(prod_z, spd->count, dx);
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
