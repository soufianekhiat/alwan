/* No include guard -- included twice (f32 and f64 passes).
 *
 * Native dual-precision Spectral Power Distribution (SPD) implementation.
 * Every function and file-static helper is templated over ALWAN_CORE_T, so
 * the f32 pass integrates in float throughout while the f64 pass is
 * byte-for-byte identical to the former hand-written double implementation.
 *
 * The illuminant, observer CMF and camera-sensitivity tables are not embedded
 * here. They are declared in data/alwan_data_tables.h, defined once per
 * precision in data/alwan_data_tables_api.c, and read through
 * alwan_table1d_row in core/alwan_table_core.
 *
 * Spectral primitives (Planck's law) live in core/alwan_spd_core.inc and are
 * reused here via ALWAN_CORE_FNVLIT(spd_planck_radiance).
 */

/* ----------------------------------------------------------------
 * SPD Creation and Destruction
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_spd_create)(ALWAN_CORE_SPD *out, ALWAN_CORE_T wavelength_min, ALWAN_CORE_T wavelength_max, size_t count, alwan_ctx *ctx) {
    if (!out || count == 0 || wavelength_min >= wavelength_max) {
        return ALWAN_E_INVALID;
    }

    /* Check for allocation size overflow */
    size_t alloc_size = alwan_safe_array_size(count, sizeof(ALWAN_CORE_T));
    if (alloc_size == 0) {
        return ALWAN_E_NOMEM;  /* Overflow would occur */
    }

    /* Allocate values array */
    ALWAN_CORE_T *values = (ALWAN_CORE_T *)ALWAN_ALLOC(alloc_size, sizeof(ALWAN_CORE_T));
    if (!values) {
        return ALWAN_E_NOMEM;
    }

    /* Initialize to zero */
    for (size_t i = 0; i < count; i++) {
        values[i] = ALWAN_CORE_LITERAL(0.0);
    }

    out->values = values;
    out->wavelength_min = wavelength_min;
    out->wavelength_max = wavelength_max;
    out->count = count;

    (void)ctx;
    return ALWAN_OK;
}

void ALWAN_CORE_FNLIT(alwan_spd_destroy)(ALWAN_CORE_SPD *spd, alwan_ctx *ctx) {
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

static inline ALWAN_CORE_T ALWAN_CORE_FN(spd_wavelength_at)(ALWAN_CORE_SPD const *spd, size_t index) {
    if (spd->count <= 1) {
        return spd->wavelength_min;
    }
    ALWAN_CORE_T t = (ALWAN_CORE_T)index / (ALWAN_CORE_T)(spd->count - 1);
    return spd->wavelength_min + t * (spd->wavelength_max - spd->wavelength_min);
}

/* Interpolate SPD value at wavelength with extrapolation */
static ALWAN_CORE_T ALWAN_CORE_FN(spd_interpolate)(ALWAN_CORE_SPD const *spd, ALWAN_CORE_T wavelength,
                               alwan_resample_method method,
                               alwan_extrapolate_mode extrapolate) {
    /* Handle out-of-range wavelengths based on extrapolation mode */
    if (wavelength < spd->wavelength_min) {
        if (extrapolate == ALWAN_EXTRAPOLATE_ZERO) {
            return ALWAN_CORE_LITERAL(0.0);
        } else if (extrapolate == ALWAN_EXTRAPOLATE_CONSTANT) {
            return spd->values[0];
        } else /* ALWAN_EXTRAPOLATE_LINEAR */ {
            /* Linear extrapolation using first two points */
            if (spd->count < 2) {
                return spd->values[0];
            }
            ALWAN_CORE_T wl0 = ALWAN_CORE_FN(spd_wavelength_at)(spd, 0);
            ALWAN_CORE_T wl1 = ALWAN_CORE_FN(spd_wavelength_at)(spd, 1);
            ALWAN_CORE_T slope = (spd->values[1] - spd->values[0]) / (wl1 - wl0);
            return spd->values[0] + slope * (wavelength - wl0);
        }
    }

    if (wavelength > spd->wavelength_max) {
        if (extrapolate == ALWAN_EXTRAPOLATE_ZERO) {
            return ALWAN_CORE_LITERAL(0.0);
        } else if (extrapolate == ALWAN_EXTRAPOLATE_CONSTANT) {
            return spd->values[spd->count - 1];
        } else /* ALWAN_EXTRAPOLATE_LINEAR */ {
            /* Linear extrapolation using last two points */
            if (spd->count < 2) {
                return spd->values[spd->count - 1];
            }
            size_t n = spd->count;
            ALWAN_CORE_T wl_n2 = ALWAN_CORE_FN(spd_wavelength_at)(spd, n - 2);
            ALWAN_CORE_T wl_n1 = ALWAN_CORE_FN(spd_wavelength_at)(spd, n - 1);
            ALWAN_CORE_T slope = (spd->values[n - 1] - spd->values[n - 2]) / (wl_n1 - wl_n2);
            return spd->values[n - 1] + slope * (wavelength - wl_n1);
        }
    }

    if (spd->count == 0) {
        return ALWAN_CORE_LITERAL(0.0);
    }

    if (spd->count == 1) {
        return spd->values[0];
    }

    /* Find fractional index */
    ALWAN_CORE_T span = spd->wavelength_max - spd->wavelength_min;
    ALWAN_CORE_T t = (wavelength - spd->wavelength_min) / span;
    ALWAN_CORE_T f_index = t * (ALWAN_CORE_T)(spd->count - 1);

    /* Clamp to valid range */
    if (f_index <= ALWAN_CORE_LITERAL(0.0)) {
        return spd->values[0];
    }
    if (f_index >= (ALWAN_CORE_T)(spd->count - 1)) {
        return spd->values[spd->count - 1];
    }
    /* Both range tests above are false for NaN, so a NaN index reaches the
     * cast, and (size_t)NaN is undefined behaviour. Resolve it to the low edge,
     * matching the addressing gate in core/alwan_table_core. */
    if (!(f_index >= ALWAN_CORE_LITERAL(0.0))) {
        return spd->values[0];
    }

    size_t i0 = (size_t)f_index;
    ALWAN_CORE_T frac = f_index - (ALWAN_CORE_T)i0;

    if (method == ALWAN_RESAMPLE_LINEAR) {
        /* Linear interpolation */
        return spd->values[i0] * (ALWAN_CORE_LITERAL(1.0) - frac) + spd->values[i0 + 1] * frac;
    } else {
        /* Catmull-Rom spline interpolation */
        ALWAN_CORE_T p0 = (i0 > 0) ? spd->values[i0 - 1] : spd->values[i0];
        ALWAN_CORE_T p1 = spd->values[i0];
        ALWAN_CORE_T p2 = spd->values[i0 + 1];
        ALWAN_CORE_T p3 = (i0 + 2 < spd->count) ? spd->values[i0 + 2] : spd->values[i0 + 1];

        ALWAN_CORE_T t2 = frac * frac;
        ALWAN_CORE_T t3 = t2 * frac;

        ALWAN_CORE_T result = ALWAN_CORE_LITERAL(0.5) * (
            (ALWAN_CORE_LITERAL(2.0) * p1) +
            (-p0 + p2) * frac +
            (ALWAN_CORE_LITERAL(2.0) * p0 - ALWAN_CORE_LITERAL(5.0) * p1 + ALWAN_CORE_LITERAL(4.0) * p2 - p3) * t2 +
            (-p0 + ALWAN_CORE_LITERAL(3.0) * p1 - ALWAN_CORE_LITERAL(3.0) * p2 + p3) * t3
        );

        return result;
    }
}

/* ----------------------------------------------------------------
 * SPD Resampling
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_spd_resample)(ALWAN_CORE_SPD *dst, ALWAN_CORE_SPD const *src, ALWAN_CORE_T wavelength_min, ALWAN_CORE_T wavelength_max, size_t count, alwan_resample_method method, alwan_extrapolate_mode extrapolate, alwan_ctx *ctx) {
    if (!src || !dst || count == 0 || wavelength_min >= wavelength_max) {
        return ALWAN_E_INVALID;
    }

    /* Create destination SPD */
    int status = ALWAN_CORE_FNLIT(alwan_spd_create)(dst, wavelength_min, wavelength_max, count, ctx);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Resample each point */
    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T wavelength = ALWAN_CORE_FN(spd_wavelength_at)(dst, i);
        dst->values[i] = ALWAN_CORE_FN(spd_interpolate)(src, wavelength, method, extrapolate);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Registry table -> SPD copy
 * ---------------------------------------------------------------- */

/* Copy one embedded table from data/alwan_data_tables.h into an SPD.
 * The extent is fixed at ALWAN_TABLE_SPD_360_830_1NM_SIZE rather than passed in.
 * Taking it as a parameter meant the loop bound and the reader's clamp both came
 * from the caller, so a caller that passed the wrong constant would read past the
 * end of the table and the gate would clamp to the wrong number rather than catch
 * it. Every table this helper is used with is on the 360-830 nm 1 nm grid; one
 * that is not needs its own copy loop, and the compiler now says so.
 *
 * Every element goes through alwan_table1d_row so the read is governed by
 * ALWAN_READ_DATA_NO_BOUND_CHECK like every other table read in the library.
 * The loop is a plain copy with no arithmetic, so the values that land in the
 * SPD are the CSV bytes, unchanged. */
static void ALWAN_CORE_FN(spd_copy_table)(ALWAN_CORE_SPD *dst,
                                          ALWAN_CORE_T const *table) {
    size_t const n = (size_t)ALWAN_TABLE_SPD_360_830_1NM_SIZE;
    for (size_t i = 0; i < n && i < dst->count; i++) {
        dst->values[i] = ALWAN_CORE_FNVLIT(alwan_table1d_row)(
            table, ALWAN_TABLE_SPD_360_830_1NM_SIZE, (int)i);
    }
}

/* ----------------------------------------------------------------
 * Illuminant Loading
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_spd_illuminant)(ALWAN_CORE_SPD *out, alwan_illuminant ill, alwan_ctx *ctx) {
    if (!out) {
        return ALWAN_E_INVALID;
    }

    /* Create SPD structure (360-830nm, 1nm steps = 471 samples) */
    int status = ALWAN_CORE_FNLIT(alwan_spd_create)(out, ALWAN_CORE_LITERAL(360.0), ALWAN_CORE_LITERAL(830.0), 471, ctx);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Copy the illuminant's registry table into the SPD */
    switch (ill) {
        case ALWAN_ILLUMINANT_A:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_a));
            break;
        case ALWAN_ILLUMINANT_D50:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d50));
            break;
        case ALWAN_ILLUMINANT_D55:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d55));
            break;
        case ALWAN_ILLUMINANT_D65:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d65));
            break;
        case ALWAN_ILLUMINANT_E:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_e));
            break;
        case ALWAN_ILLUMINANT_F1:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f1));
            break;
        case ALWAN_ILLUMINANT_F2:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f2));
            break;
        case ALWAN_ILLUMINANT_F3:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f3));
            break;
        case ALWAN_ILLUMINANT_F4:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f4));
            break;
        case ALWAN_ILLUMINANT_F5:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f5));
            break;
        case ALWAN_ILLUMINANT_F6:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f6));
            break;
        case ALWAN_ILLUMINANT_F7:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f7));
            break;
        case ALWAN_ILLUMINANT_F8:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f8));
            break;
        case ALWAN_ILLUMINANT_F9:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f9));
            break;
        case ALWAN_ILLUMINANT_F10:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f10));
            break;
        case ALWAN_ILLUMINANT_F11:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f11));
            break;
        case ALWAN_ILLUMINANT_F12:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f12));
            break;

        /* Extended illuminants */
        case ALWAN_ILLUMINANT_B:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_b));
            break;
        case ALWAN_ILLUMINANT_C:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_c));
            break;
        case ALWAN_ILLUMINANT_D60:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d60));
            break;
        case ALWAN_ILLUMINANT_D75:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d75));
            break;

        /* Additional D-series illuminants */
        case ALWAN_ILLUMINANT_D40:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d40));
            break;
        case ALWAN_ILLUMINANT_D45:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d45));
            break;
        case ALWAN_ILLUMINANT_D93:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d93));
            break;

        /* LED illuminants */
        case ALWAN_ILLUMINANT_LED_B1:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_b1));
            break;
        case ALWAN_ILLUMINANT_LED_B2:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_b2));
            break;
        case ALWAN_ILLUMINANT_LED_B3:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_b3));
            break;
        case ALWAN_ILLUMINANT_LED_B4:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_b4));
            break;
        case ALWAN_ILLUMINANT_LED_B5:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_b5));
            break;
        case ALWAN_ILLUMINANT_LED_BH1:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_bh1));
            break;
        case ALWAN_ILLUMINANT_LED_RGB1:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_rgb1));
            break;
        case ALWAN_ILLUMINANT_LED_V1:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_v1));
            break;
        case ALWAN_ILLUMINANT_LED_V2:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_v2));
            break;

        /* High Pressure illuminants */
        case ALWAN_ILLUMINANT_HP1:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_hp1));
            break;
        case ALWAN_ILLUMINANT_HP2:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_hp2));
            break;
        case ALWAN_ILLUMINANT_HP3:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_hp3));
            break;
        case ALWAN_ILLUMINANT_HP4:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_hp4));
            break;
        case ALWAN_ILLUMINANT_HP5:
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_hp5));
            break;

        default:
            ALWAN_CORE_FNLIT(alwan_spd_destroy)(out, ctx);
            return ALWAN_E_INVALID;
    }

    (void)ctx;
    return ALWAN_OK;
}

/* Generate blackbody (Planckian) SPD using Planck's law
 * Spectral radiance: L(lambda,T) = c1 / (lambda^5 * (exp(c2/(lambda*T)) - 1))
 * where c1 = 3.741771e-16 W*m^2, c2 = 1.4388e-2 m*K */
alwan_status ALWAN_CORE_FNLIT(alwan_spd_blackbody)(ALWAN_CORE_SPD *out, ALWAN_CORE_T temperature_K, ALWAN_CORE_T wavelength_min, ALWAN_CORE_T wavelength_max, size_t count, alwan_ctx *ctx) {
    if (!out || count == 0) {
        return ALWAN_E_INVALID;
    }

    /* Validate temperature range (1000-25000K typical for lighting) */
    if (temperature_K < ALWAN_CORE_LITERAL(1000.0) || temperature_K > ALWAN_CORE_LITERAL(25000.0)) {
        return ALWAN_E_INVALID;
    }

    /* Create SPD structure */
    int status = ALWAN_CORE_FNLIT(alwan_spd_create)(out, wavelength_min, wavelength_max, count, ctx);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Calculate wavelength step */
    ALWAN_CORE_T const step = (wavelength_max - wavelength_min) / (ALWAN_CORE_T)(count - 1);

    /* Compute Planckian spectral radiance for each wavelength */
    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T wavelength_nm = wavelength_min + (ALWAN_CORE_T)i * step;
        ALWAN_CORE_T wavelength_m = wavelength_nm * ALWAN_CORE_LITERAL(1e-9);  /* Convert nm to meters */
        out->values[i] = ALWAN_CORE_FNVLIT(spd_planck_radiance)(wavelength_m, temperature_K);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Observer CMF Loading
 * ---------------------------------------------------------------- */

static int ALWAN_CORE_FN(load_observer_cmf)(alwan_ctx *ctx,
                              alwan_observer_type observer,
                              ALWAN_CORE_SPD *x_bar,
                              ALWAN_CORE_SPD *y_bar,
                              ALWAN_CORE_SPD *z_bar) {
    /* Determine wavelength range and sample count based on observer */
    ALWAN_CORE_T wl_min, wl_max;
    size_t count;

    if (observer == ALWAN_OBSERVER_CIE_1931_2DEG || observer == ALWAN_OBSERVER_CIE_1964_10DEG) {
        /* CIE 1931/1964: 360-830nm, 1nm steps = 471 samples */
        wl_min = ALWAN_CORE_LITERAL(360.0);
        wl_max = ALWAN_CORE_LITERAL(830.0);
        count = 471;
    } else {
        /* CIE 2012/2015, Stockman & Sharpe: 360-830nm, 1nm steps = 471 samples
         * CSVs cover 360-830nm with leading zeros before 390nm. */
        wl_min = ALWAN_CORE_LITERAL(360.0);
        wl_max = ALWAN_CORE_LITERAL(830.0);
        count = 471;
    }

    int status;

    status = ALWAN_CORE_FNLIT(alwan_spd_create)(x_bar, wl_min, wl_max, count, ctx);
    if (status != ALWAN_OK) return status;

    status = ALWAN_CORE_FNLIT(alwan_spd_create)(y_bar, wl_min, wl_max, count, ctx);
    if (status != ALWAN_OK) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(x_bar, ctx);
        return status;
    }

    status = ALWAN_CORE_FNLIT(alwan_spd_create)(z_bar, wl_min, wl_max, count, ctx);
    if (status != ALWAN_OK) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(x_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(y_bar, ctx);
        return status;
    }

    /* Copy the observer's registry tables into the three SPDs */
    if (observer == ALWAN_OBSERVER_CIE_1931_2DEG) {
        /* CIE 1931 2 deg Standard Observer */
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1931_2deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1931_2deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1931_2deg_z));
    } else if (observer == ALWAN_OBSERVER_CIE_1964_10DEG) {
        /* CIE 1964 10 deg Standard Observer */
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1964_10deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1964_10deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1964_10deg_z));
    } else if (observer == ALWAN_OBSERVER_CIE_2012_2DEG) {
        /* CIE 2012/2015 2 deg Standard Observer (390-830nm) */
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2012_2deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2012_2deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2012_2deg_z));
    } else if (observer == ALWAN_OBSERVER_CIE_2012_10DEG) {
        /* CIE 2012/2015 10 deg Standard Observer (390-830nm) */
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2012_10deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2012_10deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2012_10deg_z));
    } else if (observer == ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG) {
        /* Stockman & Sharpe 2000 2 deg Cone Fundamentals */
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_stockman_sharpe_2deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_stockman_sharpe_2deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_stockman_sharpe_2deg_z));
    } else if (observer == ALWAN_OBSERVER_CIE_2015_2DEG) {
        /* CIE 2015 2 deg Cone Fundamental Observer */
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2015_2deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2015_2deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2015_2deg_z));
    } else if (observer == ALWAN_OBSERVER_CIE_2015_10DEG) {
        /* CIE 2015 10 deg Cone Fundamental Observer */
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2015_10deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2015_10deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2015_10deg_z));
    } else if (observer == ALWAN_OBSERVER_WRIGHT_GUILD_1931) {
        /* Wright & Guild 1931 2 deg RGB CMFs (historical) */
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_wright_guild_1931_r));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_wright_guild_1931_g));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_wright_guild_1931_b));
    } else {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(x_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(y_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(z_bar, ctx);
        return ALWAN_E_INVALID;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ Integration from SPD
 * ---------------------------------------------------------------- */

/* Trapezoidal integration */
static ALWAN_CORE_T ALWAN_CORE_FN(integrate_trapezoid)(ALWAN_CORE_T const *values, size_t count, ALWAN_CORE_T dx) {
    if (count == 0) return ALWAN_CORE_LITERAL(0.0);
    if (count == 1) return values[0] * dx;

    ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.5) * (values[0] + values[count - 1]);
    for (size_t i = 1; i < count - 1; i++) {
        sum += values[i];
    }
    return sum * dx;
}

/* Simpson's 1/3 rule integration */
static ALWAN_CORE_T ALWAN_CORE_FN(integrate_simpson)(ALWAN_CORE_T const *values, size_t count, ALWAN_CORE_T dx) {
    if (count == 0) return ALWAN_CORE_LITERAL(0.0);
    if (count == 1) return values[0] * dx;
    if (count == 2) return ALWAN_CORE_LITERAL(0.5) * (values[0] + values[1]) * dx;

    /* Simpson's rule requires odd number of points */
    size_t n = count;
    int use_trapezoid_last = (count % 2 == 0);
    if (use_trapezoid_last) n--;

    ALWAN_CORE_T sum = values[0] + values[n - 1];
    for (size_t i = 1; i < n - 1; i += 2) {
        sum += ALWAN_CORE_LITERAL(4.0) * values[i];
    }
    for (size_t i = 2; i < n - 1; i += 2) {
        sum += ALWAN_CORE_LITERAL(2.0) * values[i];
    }
    ALWAN_CORE_T result = sum * dx / ALWAN_CORE_LITERAL(3.0);

    /* Add last interval with trapezoid if needed */
    if (use_trapezoid_last) {
        result += ALWAN_CORE_LITERAL(0.5) * (values[n - 1] + values[n]) * dx;
    }

    return result;
}

alwan_status ALWAN_CORE_FNLIT(alwan_xyz_from_spd)(ALWAN_CORE_XYZ *xyz_out, ALWAN_CORE_SPD const *spd, ALWAN_CORE_SPD const *illuminant, alwan_observer_type observer, alwan_integrate_method method, ALWAN_CORE_T bandpass_nm, alwan_ctx *ctx) {
    if (!spd || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    /* Load observer CMFs */
    ALWAN_CORE_SPD x_bar, y_bar, z_bar;
    int status = ALWAN_CORE_FN(load_observer_cmf)(ctx, observer, &x_bar, &y_bar, &z_bar);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Resample CMFs to match SPD wavelength range (use constant extrapolation for smooth CMFs) */
    ALWAN_CORE_SPD x_bar_resampled, y_bar_resampled, z_bar_resampled;
    status = ALWAN_CORE_FNLIT(alwan_spd_resample)(&x_bar_resampled, &x_bar, spd->wavelength_min, spd->wavelength_max, spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT, ctx);
    if (status != ALWAN_OK) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&y_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&z_bar, ctx);
        return status;
    }

    status = ALWAN_CORE_FNLIT(alwan_spd_resample)(&y_bar_resampled, &y_bar, spd->wavelength_min, spd->wavelength_max, spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT, ctx);
    if (status != ALWAN_OK) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&y_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&z_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar_resampled, ctx);
        return status;
    }

    status = ALWAN_CORE_FNLIT(alwan_spd_resample)(&z_bar_resampled, &z_bar, spd->wavelength_min, spd->wavelength_max, spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT, ctx);
    if (status != ALWAN_OK) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&y_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&z_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar_resampled, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&y_bar_resampled, ctx);
        return status;
    }

    /* Allocate temporary arrays for products (with overflow protection) */
    size_t alloc_size = alwan_safe_array_size(spd->count, sizeof(ALWAN_CORE_T));
    if (alloc_size == 0) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&y_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&z_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar_resampled, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&y_bar_resampled, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&z_bar_resampled, ctx);
        return ALWAN_E_NOMEM;
    }
    ALWAN_CORE_T *prod_x = (ALWAN_CORE_T *)ALWAN_ALLOC(alloc_size, sizeof(ALWAN_CORE_T));
    ALWAN_CORE_T *prod_y = (ALWAN_CORE_T *)ALWAN_ALLOC(alloc_size, sizeof(ALWAN_CORE_T));
    ALWAN_CORE_T *prod_z = (ALWAN_CORE_T *)ALWAN_ALLOC(alloc_size, sizeof(ALWAN_CORE_T));
    /* Holds the bandpass-corrected SPD when the caller asks for one. */
    ALWAN_CORE_T *spd_corr = (ALWAN_CORE_T *)ALWAN_ALLOC(alloc_size, sizeof(ALWAN_CORE_T));

    if (!prod_x || !prod_y || !prod_z || !spd_corr) {
        if (prod_x) ALWAN_FREE(prod_x);
        if (prod_y) ALWAN_FREE(prod_y);
        if (prod_z) ALWAN_FREE(prod_z);
        if (spd_corr) ALWAN_FREE(spd_corr);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&y_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&z_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar_resampled, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&y_bar_resampled, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&z_bar_resampled, ctx);
        return ALWAN_E_NOMEM;
    }

    /* Stearns & Stearns (1988) bandpass correction, applied to the measured SPD
     * before it meets the CMFs.
     *
     * alpha = 0.083 is the paper's value and is not a free parameter: the
     * correction is derived for a triangular passband whose width equals the
     * sampling interval. bandpass_nm therefore asserts that interval rather than
     * selecting a strength, and a value that disagrees with the SPD's own
     * spacing is rejected instead of silently corrected for the wrong width.
     * Pass 0 to skip the correction, which is the default and what every caller
     * got before, since the parameter used to be discarded by a (void) cast
     * while the header, README and API docs all said it was applied. */
    {
        ALWAN_CORE_T const interval = (spd->wavelength_max - spd->wavelength_min)
                                    / (ALWAN_CORE_T)(spd->count - 1);
        int const want_bandpass = (spd->count > 1) && (bandpass_nm > ALWAN_CORE_LITERAL(0.0));

        if (want_bandpass) {
            ALWAN_CORE_T const alpha = ALWAN_CORE_LITERAL(0.083);
            size_t i;

            if (spd->count < 3 ||
                ALWAN_CORE_ABS(bandpass_nm - interval) > ALWAN_CORE_LITERAL(1e-6) * interval) {
                ALWAN_FREE(prod_x); ALWAN_FREE(prod_y); ALWAN_FREE(prod_z); ALWAN_FREE(spd_corr);
                ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar, ctx);
                ALWAN_CORE_FNLIT(alwan_spd_destroy)(&y_bar, ctx);
                ALWAN_CORE_FNLIT(alwan_spd_destroy)(&z_bar, ctx);
                ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar_resampled, ctx);
                ALWAN_CORE_FNLIT(alwan_spd_destroy)(&y_bar_resampled, ctx);
                ALWAN_CORE_FNLIT(alwan_spd_destroy)(&z_bar_resampled, ctx);
                return ALWAN_E_INVALID;
            }

            spd_corr[0] = (ALWAN_CORE_LITERAL(1.0) + alpha) * spd->values[0]
                        - alpha * spd->values[1];
            spd_corr[spd->count - 1] = (ALWAN_CORE_LITERAL(1.0) + alpha) * spd->values[spd->count - 1]
                                     - alpha * spd->values[spd->count - 2];
            for (i = 1; i + 1 < spd->count; i++) {
                spd_corr[i] = -alpha * spd->values[i - 1]
                            + (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(2.0) * alpha) * spd->values[i]
                            - alpha * spd->values[i + 1];
            }
        } else {
            size_t i;
            for (i = 0; i < spd->count; i++) {
                spd_corr[i] = spd->values[i];
            }
        }
    }

    /* Compute products: SPD * illuminant * CMF */
    for (size_t i = 0; i < spd->count; i++) {
        ALWAN_CORE_T spd_value = spd_corr[i];

        /* If illuminant provided, multiply by it */
        if (illuminant) {
            ALWAN_CORE_T wavelength = ALWAN_CORE_FN(spd_wavelength_at)(spd, i);
            ALWAN_CORE_T illum_value = ALWAN_CORE_FN(spd_interpolate)(illuminant, wavelength, ALWAN_RESAMPLE_LINEAR,
                                                  ALWAN_EXTRAPOLATE_ZERO);
            spd_value *= illum_value;
        }

        prod_x[i] = spd_value * x_bar_resampled.values[i];
        prod_y[i] = spd_value * y_bar_resampled.values[i];
        prod_z[i] = spd_value * z_bar_resampled.values[i];
    }

    /* Integrate to get XYZ */
    ALWAN_CORE_T dx = (spd->wavelength_max - spd->wavelength_min) / (ALWAN_CORE_T)(spd->count - 1);
    if (spd->count == 1) dx = ALWAN_CORE_LITERAL(1.0);

    if (method == ALWAN_INTEGRATE_TRAPEZOID) {
        xyz_out->x = ALWAN_CORE_FN(integrate_trapezoid)(prod_x, spd->count, dx);
        xyz_out->y = ALWAN_CORE_FN(integrate_trapezoid)(prod_y, spd->count, dx);
        xyz_out->z = ALWAN_CORE_FN(integrate_trapezoid)(prod_z, spd->count, dx);
    } else {
        xyz_out->x = ALWAN_CORE_FN(integrate_simpson)(prod_x, spd->count, dx);
        xyz_out->y = ALWAN_CORE_FN(integrate_simpson)(prod_y, spd->count, dx);
        xyz_out->z = ALWAN_CORE_FN(integrate_simpson)(prod_z, spd->count, dx);
    }

    /* Cleanup */
    ALWAN_FREE(prod_x);
    ALWAN_FREE(prod_y);
    ALWAN_FREE(prod_z);
    ALWAN_FREE(spd_corr);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&y_bar, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&z_bar, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar_resampled, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&y_bar_resampled, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&z_bar_resampled, ctx);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Camera Sensitivity Functions
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_spd_camera_sensitivity)(ALWAN_CORE_SPD *spd_r, ALWAN_CORE_SPD *spd_g, ALWAN_CORE_SPD *spd_b, alwan_camera_sensitivity camera, alwan_ctx *ctx) {
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

    /* Copy the camera's registry tables into the three SPDs */
    switch (camera) {
        case ALWAN_CAMERA_NIKON_5100: {
            ALWAN_CORE_FN(spd_copy_table)(spd_r, ALWAN_CORE_FNLIT(alwan_table_camera_nikon_5100_r));
            ALWAN_CORE_FN(spd_copy_table)(spd_g, ALWAN_CORE_FNLIT(alwan_table_camera_nikon_5100_g));
            ALWAN_CORE_FN(spd_copy_table)(spd_b, ALWAN_CORE_FNLIT(alwan_table_camera_nikon_5100_b));
            break;
        }
        case ALWAN_CAMERA_SIGMA_SDMERILL: {
            ALWAN_CORE_FN(spd_copy_table)(spd_r, ALWAN_CORE_FNLIT(alwan_table_camera_sigma_sdmerill_r));
            ALWAN_CORE_FN(spd_copy_table)(spd_g, ALWAN_CORE_FNLIT(alwan_table_camera_sigma_sdmerill_g));
            ALWAN_CORE_FN(spd_copy_table)(spd_b, ALWAN_CORE_FNLIT(alwan_table_camera_sigma_sdmerill_b));
            break;
        }
        default:
            return ALWAN_E_INVALID;
    }

    (void)ctx;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_xyz_from_spd_camera)(ALWAN_CORE_XYZ *xyz_out, ALWAN_CORE_SPD const *spd, ALWAN_CORE_SPD const *illuminant, alwan_camera_sensitivity camera, alwan_integrate_method method, alwan_ctx *ctx) {
    if (!spd || !xyz_out) {
        return ALWAN_E_INVALID;
    }

    /* Load camera RGB sensitivities */
    ALWAN_CORE_SPD r_sens, g_sens, b_sens;
    int status = ALWAN_CORE_FNLIT(alwan_spd_create)(&r_sens, ALWAN_CORE_LITERAL(360.0), ALWAN_CORE_LITERAL(830.0), 471, ctx);
    if (status != ALWAN_OK) {
        return status;
    }
    status = ALWAN_CORE_FNLIT(alwan_spd_create)(&g_sens, ALWAN_CORE_LITERAL(360.0), ALWAN_CORE_LITERAL(830.0), 471, ctx);
    if (status != ALWAN_OK) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_sens, ctx);
        return status;
    }
    status = ALWAN_CORE_FNLIT(alwan_spd_create)(&b_sens, ALWAN_CORE_LITERAL(360.0), ALWAN_CORE_LITERAL(830.0), 471, ctx);
    if (status != ALWAN_OK) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&g_sens, ctx);
        return status;
    }

    status = ALWAN_CORE_FNLIT(alwan_spd_camera_sensitivity)(&r_sens, &g_sens, &b_sens, camera, ctx);
    if (status != ALWAN_OK) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&g_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&b_sens, ctx);
        return status;
    }

    /* Resample sensitivities to match SPD wavelength range */
    ALWAN_CORE_SPD r_resampled, g_resampled, b_resampled;
    status = ALWAN_CORE_FNLIT(alwan_spd_resample)(&r_resampled, &r_sens, spd->wavelength_min, spd->wavelength_max, spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO, ctx);
    if (status != ALWAN_OK) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&g_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&b_sens, ctx);
        return status;
    }

    status = ALWAN_CORE_FNLIT(alwan_spd_resample)(&g_resampled, &g_sens, spd->wavelength_min, spd->wavelength_max, spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO, ctx);
    if (status != ALWAN_OK) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&g_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&b_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_resampled, ctx);
        return status;
    }

    status = ALWAN_CORE_FNLIT(alwan_spd_resample)(&b_resampled, &b_sens, spd->wavelength_min, spd->wavelength_max, spd->count, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO, ctx);
    if (status != ALWAN_OK) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&g_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&b_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_resampled, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&g_resampled, ctx);
        return status;
    }

    /* Allocate temporary arrays for products (with overflow protection) */
    size_t alloc_size = alwan_safe_array_size(spd->count, sizeof(ALWAN_CORE_T));
    if (alloc_size == 0) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&g_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&b_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_resampled, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&g_resampled, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&b_resampled, ctx);
        return ALWAN_E_NOMEM;
    }
    ALWAN_CORE_T *prod_r = (ALWAN_CORE_T *)ALWAN_ALLOC(alloc_size, sizeof(ALWAN_CORE_T));
    ALWAN_CORE_T *prod_g = (ALWAN_CORE_T *)ALWAN_ALLOC(alloc_size, sizeof(ALWAN_CORE_T));
    ALWAN_CORE_T *prod_b = (ALWAN_CORE_T *)ALWAN_ALLOC(alloc_size, sizeof(ALWAN_CORE_T));

    if (!prod_r || !prod_g || !prod_b) {
        if (prod_r) ALWAN_FREE(prod_r);
        if (prod_g) ALWAN_FREE(prod_g);
        if (prod_b) ALWAN_FREE(prod_b);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&g_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&b_sens, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_resampled, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&g_resampled, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(&b_resampled, ctx);
        return ALWAN_E_NOMEM;
    }

    /* Compute products: SPD * illuminant * sensitivity */
    for (size_t i = 0; i < spd->count; i++) {
        ALWAN_CORE_T spd_value = spd->values[i];

        /* If illuminant provided, multiply by it */
        if (illuminant) {
            ALWAN_CORE_T wavelength = ALWAN_CORE_FN(spd_wavelength_at)(spd, i);
            ALWAN_CORE_T illum_value = ALWAN_CORE_FN(spd_interpolate)(illuminant, wavelength, ALWAN_RESAMPLE_LINEAR,
                                                  ALWAN_EXTRAPOLATE_ZERO);
            spd_value *= illum_value;
        }

        prod_r[i] = spd_value * r_resampled.values[i];
        prod_g[i] = spd_value * g_resampled.values[i];
        prod_b[i] = spd_value * b_resampled.values[i];
    }

    /* Integrate to get RGB (stored in XYZ output) */
    ALWAN_CORE_T dx = (spd->wavelength_max - spd->wavelength_min) / (ALWAN_CORE_T)(spd->count - 1);
    if (spd->count == 1) dx = ALWAN_CORE_LITERAL(1.0);

    if (method == ALWAN_INTEGRATE_TRAPEZOID) {
        xyz_out->x = ALWAN_CORE_FN(integrate_trapezoid)(prod_r, spd->count, dx);
        xyz_out->y = ALWAN_CORE_FN(integrate_trapezoid)(prod_g, spd->count, dx);
        xyz_out->z = ALWAN_CORE_FN(integrate_trapezoid)(prod_b, spd->count, dx);
    } else {
        xyz_out->x = ALWAN_CORE_FN(integrate_simpson)(prod_r, spd->count, dx);
        xyz_out->y = ALWAN_CORE_FN(integrate_simpson)(prod_g, spd->count, dx);
        xyz_out->z = ALWAN_CORE_FN(integrate_simpson)(prod_b, spd->count, dx);
    }

    /* Cleanup */
    ALWAN_FREE(prod_r);
    ALWAN_FREE(prod_g);
    ALWAN_FREE(prod_b);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_sens, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&g_sens, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&b_sens, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&r_resampled, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&g_resampled, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&b_resampled, ctx);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Spectral Shape Descriptors
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_spd_analyze_shape)(ALWAN_CORE_SPD_SHAPE *shape_out, ALWAN_CORE_SPD const *spd) {
    if (!spd || !shape_out || spd->count == 0) {
        return ALWAN_E_INVALID;
    }

    /* Find peak wavelength and value */
    size_t peak_idx = 0;
    ALWAN_CORE_T peak_val = spd->values[0];
    for (size_t i = 1; i < spd->count; i++) {
        if (spd->values[i] > peak_val) {
            peak_val = spd->values[i];
            peak_idx = i;
        }
    }

    shape_out->peak_value = peak_val;
    shape_out->peak_wavelength = ALWAN_CORE_FN(spd_wavelength_at)(spd, peak_idx);

    /* Compute FWHM (Full Width at Half Maximum) */
    ALWAN_CORE_T half_max = peak_val * ALWAN_CORE_LITERAL(0.5);

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
    ALWAN_CORE_T left_wl = ALWAN_CORE_FN(spd_wavelength_at)(spd, left_idx);
    if (left_idx < peak_idx && spd->values[left_idx + 1] != spd->values[left_idx]) {
        ALWAN_CORE_T t = (half_max - spd->values[left_idx]) /
                         (spd->values[left_idx + 1] - spd->values[left_idx]);
        left_wl = ALWAN_CORE_FN(spd_wavelength_at)(spd, left_idx) +
                  t * (ALWAN_CORE_FN(spd_wavelength_at)(spd, left_idx + 1) - ALWAN_CORE_FN(spd_wavelength_at)(spd, left_idx));
    }

    ALWAN_CORE_T right_wl = ALWAN_CORE_FN(spd_wavelength_at)(spd, right_idx);
    if (right_idx > peak_idx && right_idx > 0 && spd->values[right_idx - 1] != spd->values[right_idx]) {
        ALWAN_CORE_T t = (half_max - spd->values[right_idx]) /
                         (spd->values[right_idx - 1] - spd->values[right_idx]);
        right_wl = ALWAN_CORE_FN(spd_wavelength_at)(spd, right_idx) +
                   t * (ALWAN_CORE_FN(spd_wavelength_at)(spd, right_idx - 1) - ALWAN_CORE_FN(spd_wavelength_at)(spd, right_idx));
    }

    shape_out->fwhm = right_wl - left_wl;
    if (shape_out->fwhm < ALWAN_CORE_LITERAL(0.0)) {
        shape_out->fwhm = ALWAN_CORE_LITERAL(0.0);
    }

    /* Compute centroid (weighted mean wavelength) */
    ALWAN_CORE_T sum_weighted = ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T sum_values = ALWAN_CORE_LITERAL(0.0);

    for (size_t i = 0; i < spd->count; i++) {
        ALWAN_CORE_T wl = ALWAN_CORE_FN(spd_wavelength_at)(spd, i);
        sum_weighted += wl * spd->values[i];
        sum_values += spd->values[i];
    }

    if (sum_values > ALWAN_CORE_LITERAL(0.0)) {
        shape_out->centroid = sum_weighted / sum_values;
    } else {
        shape_out->centroid = (spd->wavelength_min + spd->wavelength_max) * ALWAN_CORE_LITERAL(0.5);
    }

    /* Compute bandwidth (total wavelength range) */
    shape_out->bandwidth = spd->wavelength_max - spd->wavelength_min;

    return ALWAN_OK;
}
