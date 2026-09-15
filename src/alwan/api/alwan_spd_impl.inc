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

    /* Copy the illuminant's registry table into the SPD. A valid illuminant whose
     * table was compiled out (data/alwan_data_tables_config.h) sets `missing` and
     * reports ALWAN_E_NODATA; an out-of-range enum is still ALWAN_E_INVALID. */
    int missing = 0;
    switch (ill) {
        case ALWAN_ILLUMINANT_A:
#if ALWAN_TABLE_SPD_ILLUMINANT_A
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_a));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_D50:
#if ALWAN_TABLE_SPD_ILLUMINANT_D50
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d50));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_D55:
#if ALWAN_TABLE_SPD_ILLUMINANT_D55
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d55));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_D65:
#if ALWAN_TABLE_SPD_ILLUMINANT_D65
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d65));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_E:
#if ALWAN_TABLE_SPD_ILLUMINANT_E
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_e));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_F1:
#if ALWAN_TABLE_SPD_ILLUMINANT_F1
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f1));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_F2:
#if ALWAN_TABLE_SPD_ILLUMINANT_F2
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f2));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_F3:
#if ALWAN_TABLE_SPD_ILLUMINANT_F3
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f3));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_F4:
#if ALWAN_TABLE_SPD_ILLUMINANT_F4
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f4));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_F5:
#if ALWAN_TABLE_SPD_ILLUMINANT_F5
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f5));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_F6:
#if ALWAN_TABLE_SPD_ILLUMINANT_F6
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f6));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_F7:
#if ALWAN_TABLE_SPD_ILLUMINANT_F7
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f7));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_F8:
#if ALWAN_TABLE_SPD_ILLUMINANT_F8
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f8));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_F9:
#if ALWAN_TABLE_SPD_ILLUMINANT_F9
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f9));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_F10:
#if ALWAN_TABLE_SPD_ILLUMINANT_F10
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f10));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_F11:
#if ALWAN_TABLE_SPD_ILLUMINANT_F11
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f11));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_F12:
#if ALWAN_TABLE_SPD_ILLUMINANT_F12
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_f12));
#else
            missing = 1;
#endif
            break;

        /* Extended illuminants */
        case ALWAN_ILLUMINANT_B:
#if ALWAN_TABLE_SPD_ILLUMINANT_B
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_b));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_C:
#if ALWAN_TABLE_SPD_ILLUMINANT_C
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_c));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_D60:
#if ALWAN_TABLE_SPD_ILLUMINANT_D60
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d60));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_D75:
#if ALWAN_TABLE_SPD_ILLUMINANT_D75
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d75));
#else
            missing = 1;
#endif
            break;

        /* Additional D-series illuminants */
        case ALWAN_ILLUMINANT_D40:
#if ALWAN_TABLE_SPD_ILLUMINANT_D40
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d40));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_D45:
#if ALWAN_TABLE_SPD_ILLUMINANT_D45
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d45));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_D93:
#if ALWAN_TABLE_SPD_ILLUMINANT_D93
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_d93));
#else
            missing = 1;
#endif
            break;

        /* LED illuminants */
        case ALWAN_ILLUMINANT_LED_B1:
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B1
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_b1));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_LED_B2:
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B2
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_b2));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_LED_B3:
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B3
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_b3));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_LED_B4:
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B4
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_b4));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_LED_B5:
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B5
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_b5));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_LED_BH1:
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_BH1
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_bh1));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_LED_RGB1:
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_RGB1
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_rgb1));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_LED_V1:
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_V1
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_v1));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_LED_V2:
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_V2
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_led_v2));
#else
            missing = 1;
#endif
            break;

        /* High Pressure illuminants */
        case ALWAN_ILLUMINANT_HP1:
#if ALWAN_TABLE_SPD_ILLUMINANT_HP1
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_hp1));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_HP2:
#if ALWAN_TABLE_SPD_ILLUMINANT_HP2
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_hp2));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_HP3:
#if ALWAN_TABLE_SPD_ILLUMINANT_HP3
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_hp3));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_HP4:
#if ALWAN_TABLE_SPD_ILLUMINANT_HP4
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_hp4));
#else
            missing = 1;
#endif
            break;
        case ALWAN_ILLUMINANT_HP5:
#if ALWAN_TABLE_SPD_ILLUMINANT_HP5
            ALWAN_CORE_FN(spd_copy_table)(out,
                ALWAN_CORE_FNLIT(alwan_table_spd_illuminant_hp5));
#else
            missing = 1;
#endif
            break;

        default:
            ALWAN_CORE_FNLIT(alwan_spd_destroy)(out, ctx);
            return ALWAN_E_INVALID;
    }
    if (missing) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(out, ctx);
        return ALWAN_E_NODATA;
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

    /* Copy the observer's registry tables into the three SPDs. A table compiled
     * out sets `missing`: ALWAN_E_NODATA, as for the illuminants above. */
    int missing = 0;
    if (observer == ALWAN_OBSERVER_CIE_1931_2DEG) {
        /* CIE 1931 2 deg Standard Observer */
#if ALWAN_TABLE_CMF_CIE_1931_2DEG
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1931_2deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1931_2deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1931_2deg_z));
#else
        missing = 1;
#endif
    } else if (observer == ALWAN_OBSERVER_CIE_1964_10DEG) {
        /* CIE 1964 10 deg Standard Observer */
#if ALWAN_TABLE_CMF_CIE_1964_10DEG
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1964_10deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1964_10deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_1964_10deg_z));
#else
        missing = 1;
#endif
    } else if (observer == ALWAN_OBSERVER_CIE_2012_2DEG) {
        /* CIE 2012/2015 2 deg Standard Observer (390-830nm) */
#if ALWAN_TABLE_CMF_CIE_2012_2DEG
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2012_2deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2012_2deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2012_2deg_z));
#else
        missing = 1;
#endif
    } else if (observer == ALWAN_OBSERVER_CIE_2012_10DEG) {
        /* CIE 2012/2015 10 deg Standard Observer (390-830nm) */
#if ALWAN_TABLE_CMF_CIE_2012_10DEG
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2012_10deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2012_10deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2012_10deg_z));
#else
        missing = 1;
#endif
    } else if (observer == ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG) {
        /* Stockman & Sharpe 2000 2 deg Cone Fundamentals */
#if ALWAN_TABLE_CMF_STOCKMAN_SHARPE_2DEG
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_stockman_sharpe_2deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_stockman_sharpe_2deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_stockman_sharpe_2deg_z));
#else
        missing = 1;
#endif
    } else if (observer == ALWAN_OBSERVER_CIE_2015_2DEG) {
        /* CIE 2015 2 deg Cone Fundamental Observer */
#if ALWAN_TABLE_CMF_CIE_2015_2DEG
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2015_2deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2015_2deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2015_2deg_z));
#else
        missing = 1;
#endif
    } else if (observer == ALWAN_OBSERVER_CIE_2015_10DEG) {
        /* CIE 2015 10 deg Cone Fundamental Observer */
#if ALWAN_TABLE_CMF_CIE_2015_10DEG
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2015_10deg_x));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2015_10deg_y));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_cie_2015_10deg_z));
#else
        missing = 1;
#endif
    } else if (observer == ALWAN_OBSERVER_WRIGHT_GUILD_1931) {
        /* Wright & Guild 1931 2 deg RGB CMFs (historical) */
#if ALWAN_TABLE_CMF_WRIGHT_GUILD_1931
        ALWAN_CORE_FN(spd_copy_table)(x_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_wright_guild_1931_r));
        ALWAN_CORE_FN(spd_copy_table)(y_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_wright_guild_1931_g));
        ALWAN_CORE_FN(spd_copy_table)(z_bar, ALWAN_CORE_FNLIT(alwan_table_cmf_wright_guild_1931_b));
#else
        missing = 1;
#endif
    } else {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(x_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(y_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(z_bar, ctx);
        return ALWAN_E_INVALID;
    }
    if (missing) {
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(x_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(y_bar, ctx);
        ALWAN_CORE_FNLIT(alwan_spd_destroy)(z_bar, ctx);
        return ALWAN_E_NODATA;
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
    /* Anything but the two rules used to fall through to Simpson. */
    if ((int)method != (int)ALWAN_INTEGRATE_TRAPEZOID && (int)method != (int)ALWAN_INTEGRATE_SIMPSON) {
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
 * ASTM E308 and E2022: XYZ as industrial colorimetry reports it
 *
 * E2022 builds tables of tristimulus weighting factors from the 1 nm observer and
 * illuminant: each 1 nm product S(l) cmf(l) is spread onto the nodes of the
 * measurement interval by Lagrange coefficients, cubic over the first and last
 * intervals and over four nodes elsewhere, and the table is scaled so its Y weights
 * sum to 100. E308 folds the weights of nodes a spectrum does not reach onto its first
 * and last node. Data at 1 nm, and at 5 nm by the omission method, are summed on
 * their own samples instead, and data at 20 nm are interpolated to 10 nm first. This
 * follows colour-science's sd_to_XYZ_ASTME308 loop for loop, which the tests hold it
 * to. Computes in double in both precisions.
 * ---------------------------------------------------------------- */

#ifndef ALWAN__ASTM_E308_ONCE
#define ALWAN__ASTM_E308_ONCE

#define ALWAN__E308_START        360   /* the observers' first sample, nm */
#define ALWAN__E308_PRACTICE_END 780   /* E308's practice range */
#define ALWAN__E308_FULL_END     830   /* the observers' last sample */
#define ALWAN__E308_MAX_NODES    96    /* (830 - 360) / 5 + 1 */

/* A wavelength as a whole number of nanometres, when it is one. */
static int alwan__e308_whole(double v, long *out) {
    long const n = (long)(v + 0.5);
    if (!(v > 0.0) || ALWAN_ABS(v - (double)n) > 1e-9 * v) return 0;
    *out = n;
    return 1;
}

/* colour.algebra.lagrange_coefficients: the Lagrange basis on nodes 0 .. n - 1 at r. */
static void alwan__e308_lagrange(double *out, double r, int n) {
    int i, j;
    for (j = 0; j < n; j++) {
        double v = 1.0;
        for (i = 0; i < n; i++) {
            if (i != j) v *= (r - (double)i) / ((double)j - (double)i);
        }
        out[j] = v;
    }
}

/* E2022: S and Y (w_c x 3) at 1 nm from the range's start, interval d in {5, 10, 20};
 * W receives (w_c - 1) / d + 1 nodes x 3, scaled so the Y weights sum to 100. The
 * loops are colour-science's, in its order, so every weight gathers its terms alike. */
static void alwan__e2022(double *W, double const *S, double const *Y, int w_c, int d) {
    int const i_c = (w_c - 1) / d + 1, i_cm = i_c - 1, r_c = d - 1;
    int const w_lif = w_c - (w_c - 1) % d - 1 - r_c;
    double const lo = 1.0 / (double)d, hi = 1.0 - 1.0 / (double)d;
    double const step = r_c > 1 ? (hi - lo) / (double)(r_c - 1) : 0.0;
    double cc[19 * 3], cb[19 * 4];
    double sum_y = 0.0, scale;
    int h, g, i, k;
    for (h = 0; h < r_c; h++) {
        double const r = (r_c > 1 && h == r_c - 1) ? hi : (double)h * step + lo;   /* numpy.linspace */
        alwan__e308_lagrange(cc + 3 * h, r, 3);
        alwan__e308_lagrange(cb + 4 * h, r + 1.0, 4);
    }
    for (k = 0; k < i_c; k++) {
        for (i = 0; i < 3; i++) W[k * 3 + i] = S[k * d] * Y[(k * d) * 3 + i];
    }
    for (i = 0; i < 3; i++) {
        for (h = 0; h < r_c; h++) {                                  /* the first interval */
            for (g = 0; g < 3; g++) W[g * 3 + i] = W[g * 3 + i] + cc[h * 3 + g] * S[h + 1] * Y[(h + 1) * 3 + i];
        }
        for (h = 0; h < r_c; h++) {                                  /* the last interval */
            for (g = i_cm; g > i_cm - 3; g--) {
                W[g * 3 + i] = W[g * 3 + i]
                             + cc[(r_c - h - 1) * 3 + (i_cm - g)] * S[h + w_lif] * Y[(h + w_lif) * 3 + i];
            }
        }
        for (h = 0; h < i_c - 3; h++) {                              /* the intervals between */
            for (g = 0; g < r_c; g++) {
                int const w_i = (r_c + 1) * (h + 1) + 1 + g;
                W[h * 3 + i] = W[h * 3 + i] + cb[g * 4 + 0] * S[w_i] * Y[w_i * 3 + i];
                W[(h + 1) * 3 + i] = W[(h + 1) * 3 + i] + cb[g * 4 + 1] * S[w_i] * Y[w_i * 3 + i];
                W[(h + 2) * 3 + i] = W[(h + 2) * 3 + i] + cb[g * 4 + 2] * S[w_i] * Y[w_i * 3 + i];
                W[(h + 3) * 3 + i] = W[(h + 3) * 3 + i] + cb[g * 4 + 3] * S[w_i] * Y[w_i * 3 + i];
            }
        }
        for (h = w_c - (w_c - 1) % d; h < w_c; h++) {               /* an incomplete last interval */
            W[i_cm * 3 + i] = W[i_cm * 3 + i] + S[h] * Y[h * 3 + i];
        }
    }
    for (k = 0; k < i_c; k++) sum_y += W[k * 3 + 1];
    scale = 100.0 / sum_y;
    for (k = 0; k < 3 * i_c; k++) W[k] *= scale;
}

/* E308's adjustment and sum: W's nodes a .. b (inclusive), the weights below a folded
 * onto a and those above b onto b, as colour-science folds them; R holds b - a + 1
 * values. */
static void alwan__e308_weighted(double xyz[3], double const *W, long i_c, long a, long b, double const *R) {
    double adj[ALWAN__E308_MAX_NODES * 3];
    long const n = b - a + 1;
    long k;
    int c;
    for (k = 0; k < 3 * n; k++) adj[k] = W[a * 3 + k];
    for (k = 0; k < a; k++) {
        for (c = 0; c < 3; c++) adj[c] += W[k * 3 + c];
    }
    for (k = 0; k < i_c - 1 - b; k++) {
        for (c = 0; c < 3; c++) adj[(n - 1) * 3 + c] += W[(i_c - 1 - k) * 3 + c];
    }
    for (c = 0; c < 3; c++) {
        double acc = 0.0;
        for (k = 0; k < n; k++) acc += adj[k * 3 + c] * R[k];
        xyz[c] = acc;
    }
}

#endif /* ALWAN__ASTM_E308_ONCE */

/* S and Y at every whole nanometre of the range, w_c of each. */
static alwan_status ALWAN_CORE_FN(e308_tables)(double *S, double *Y, int w_c, ALWAN_CORE_SPD const *illuminant,
                                                alwan_observer_type observer, alwan_ctx *ctx) {
    ALWAN_CORE_SPD xb, yb, zb;
    int j;
    int const st = ALWAN_CORE_FN(load_observer_cmf)(ctx, observer, &xb, &yb, &zb);
    if (st != ALWAN_OK) return (alwan_status)st;
    for (j = 0; j < w_c; j++) {
        ALWAN_CORE_T const wl = (ALWAN_CORE_T)(ALWAN__E308_START + j);
        S[j] = illuminant ? (double)ALWAN_CORE_FN(spd_interpolate)(illuminant, wl, ALWAN_RESAMPLE_LINEAR,
                                                                   ALWAN_EXTRAPOLATE_CONSTANT)
                          : 1.0;
        Y[j * 3 + 0] = (double)xb.values[j];
        Y[j * 3 + 1] = (double)yb.values[j];
        Y[j * 3 + 2] = (double)zb.values[j];
    }
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&xb, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&yb, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&zb, ctx);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_astm_e2022_weights)(ALWAN_CORE_T *weights_out, size_t *node_count,
                                                        ALWAN_CORE_SPD const *illuminant, alwan_observer_type observer,
                                                        int interval_nm, int observer_range, alwan_ctx *ctx) {
    int const end = observer_range ? ALWAN__E308_FULL_END : ALWAN__E308_PRACTICE_END;
    int const w_c = end - ALWAN__E308_START + 1;
    double *buf;
    alwan_status st;
    int i_c, k;
    if (!node_count || (interval_nm != 5 && interval_nm != 10 && interval_nm != 20)) return ALWAN_E_INVALID;
    i_c = (w_c - 1) / interval_nm + 1;
    *node_count = (size_t)i_c;
    if (!weights_out) return ALWAN_OK;
    buf = (double *)ALWAN_ALLOC((size_t)(4 * w_c + 3 * i_c) * sizeof(double), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    st = ALWAN_CORE_FN(e308_tables)(buf, buf + w_c, w_c, illuminant, observer, ctx);
    if (st == ALWAN_OK) {
        double *W = buf + 4 * w_c;
        alwan__e2022(W, buf, buf + w_c, w_c, interval_nm);
        for (k = 0; k < 3 * i_c; k++) weights_out[k] = (ALWAN_CORE_T)W[k];
    }
    ALWAN_FREE(buf);
    return st;
}

alwan_status ALWAN_CORE_FNLIT(alwan_xyz_from_spd_astm_e308)(ALWAN_CORE_XYZ *xyz_out, ALWAN_CORE_SPD const *spd,
                                                           ALWAN_CORE_SPD const *illuminant,
                                                           alwan_observer_type observer,
                                                           alwan_astm_e308_params const *params, alwan_ctx *ctx) {
    alwan_astm_e308_params const zero = { 0, 0, 0 };
    alwan_astm_e308_params const *p = params ? params : &zero;
    long const end = p->observer_range ? ALWAN__E308_FULL_END : ALWAN__E308_PRACTICE_END;
    int const w_c = (int)(end - ALWAN__E308_START) + 1;
    long lo = 0, hi = 0, d, grid, a, b, k;
    size_t const n_spd = spd ? spd->count : 0;
    double *buf, *S, *Y, *W, *R, xyz[3] = { 0.0, 0.0, 0.0 };
    alwan_status st;

    if (!xyz_out || !spd || !spd->values || n_spd < 2) return ALWAN_E_INVALID;
    if (!alwan__e308_whole((double)spd->wavelength_min, &lo) || !alwan__e308_whole((double)spd->wavelength_max, &hi)
        || hi <= lo || (hi - lo) % (long)(n_spd - 1) != 0) {
        return ALWAN_E_INVALID;
    }
    d = (hi - lo) / (long)(n_spd - 1);
    if (d != 1 && d != 5 && d != 10 && d != 20) return ALWAN_E_INVALID;
    grid = (d == 20 && !p->tables_at_20nm) ? 10 : d;
    if (lo % grid != 0 || hi % grid != 0) return ALWAN_E_INVALID;
    /* The spectrum's first and last sample inside the range. */
    a = lo >= ALWAN__E308_START ? lo : lo + ((ALWAN__E308_START - lo + d - 1) / d) * d;
    b = hi <= end ? hi : hi - ((hi - end + d - 1) / d) * d;
    if (a > b) return ALWAN_E_INVALID;

    buf = (double *)ALWAN_ALLOC((size_t)(4 * w_c + 3 * ALWAN__E308_MAX_NODES + 2 * (long)n_spd + w_c + 8)
                                * sizeof(double), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    S = buf;
    Y = S + w_c;
    W = Y + 3 * w_c;
    R = W + 3 * ALWAN__E308_MAX_NODES;
    st = ALWAN_CORE_FN(e308_tables)(S, Y, w_c, illuminant, observer, ctx);

    if (st == ALWAN_OK && (d == 1 || (d == 5 && !p->tables_at_5nm))) {
        /* Summed on the spectrum's own grid over the range, held at its end values
         * beyond it: sd_to_XYZ_integration after colour aligns the spectrum. */
        double sum_y = 0.0, dot[3] = { 0.0, 0.0, 0.0 }, kk;
        for (k = ALWAN__E308_START; k <= end; k += d) {
            long const at = k < lo ? 0 : (k > hi ? (long)n_spd - 1 : (k - lo) / d);
            double const r = (double)spd->values[at];
            double const s = S[k - ALWAN__E308_START];
            double const *y = Y + 3 * (k - ALWAN__E308_START);
            sum_y += y[1] * s;
            dot[0] += r * s * y[0];
            dot[1] += r * s * y[1];
            dot[2] += r * s * y[2];
        }
        kk = 100.0 / (sum_y * (double)d);
        xyz[0] = kk * dot[0] * (double)d;
        xyz[1] = kk * dot[1] * (double)d;
        xyz[2] = kk * dot[2] * (double)d;
    } else if (st == ALWAN_OK && d == 20 && !p->tables_at_20nm) {
        /* 20 nm to 10 nm: padded 20 nm beyond each end by cubic extrapolation, each
         * midpoint from the four 20 nm samples around it, then the 10 nm tables. */
        long const n20 = (b - a) / 20 + 1;
        long const L = 2 * (n20 - 1) + 5;
        double *P = R;
        if (n20 < 3) {
            st = ALWAN_E_INVALID;
        } else {
            for (k = 0; k < n20; k++) P[2 + 2 * k] = (double)spd->values[(a - lo) / 20 + k];
            P[0] = 3.0 * P[2] - 3.0 * P[4] + P[6];
            P[L - 1] = P[L - 7] - 3.0 * P[L - 5] + 3.0 * P[L - 3];
            for (k = 3; k < L - 3; k += 2) {
                P[k] = -0.0625 * P[k - 3] + 0.5625 * P[k - 1] + 0.5625 * P[k + 1] - 0.0625 * P[k + 3];
            }
            alwan__e2022(W, S, Y, w_c, 10);
            alwan__e308_weighted(xyz, W, (long)((w_c - 1) / 10 + 1), (a - ALWAN__E308_START) / 10,
                                 (b - ALWAN__E308_START) / 10, P + 2);
        }
    } else if (st == ALWAN_OK) {
        /* E2022 tables at the spectrum's own interval, folded to its extent. */
        long const i_c = (long)((w_c - 1) / (int)d + 1);
        long const last = ALWAN__E308_START + d * (i_c - 1);
        if (b > last) {
            st = ALWAN_E_INVALID;          /* past the table's last node */
        } else {
            for (k = 0; k <= (b - a) / d; k++) R[k] = (double)spd->values[(a - lo) / d + k];
            alwan__e2022(W, S, Y, w_c, (int)d);
            alwan__e308_weighted(xyz, W, i_c, (a - ALWAN__E308_START) / d, (b - ALWAN__E308_START) / d, R);
        }
    }
    if (st == ALWAN_OK) {
        xyz_out->x = (ALWAN_CORE_T)xyz[0];
        xyz_out->y = (ALWAN_CORE_T)xyz[1];
        xyz_out->z = (ALWAN_CORE_T)xyz[2];
    }
    ALWAN_FREE(buf);
    return st;
}

/* ----------------------------------------------------------------
 * Planck 1900 locus
 *
 * Planck's law summed against the observer's 1 nm CMFs, then CIE 1960 uv, as
 * colour-science's CCT_to_uv_Planck1900. c1, pi and the integration step multiply
 * X, Y and Z alike and cancel in uv.
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_cct_to_uv_planck1900)(ALWAN_CORE_VEC2 *uv_out, ALWAN_CORE_T cct,
                                                          alwan_observer_type observer, alwan_ctx *ctx) {
    ALWAN_CORE_SPD xb, yb, zb;
    double X = 0.0, Y = 0.0, Z = 0.0, U, V, W, s;
    size_t i;
    int st;
    if (!uv_out || !(cct > ALWAN_CORE_LITERAL(0.0))) return ALWAN_E_INVALID;
    st = ALWAN_CORE_FN(load_observer_cmf)(ctx, observer, &xb, &yb, &zb);
    if (st != ALWAN_OK) return (alwan_status)st;
    for (i = 0; i < xb.count; i++) {
        double const l = ((double)xb.wavelength_min + (double)i) * 1e-9;
        double const p = (1.0 / (l * l * l * l * l)) / (ALWAN_EXP_F64(1.4388e-2 / (l * (double)cct)) - 1.0);
        X += p * (double)xb.values[i];
        Y += p * (double)yb.values[i];
        Z += p * (double)zb.values[i];
    }
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&xb, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&yb, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&zb, ctx);
    U = 2.0 / 3.0 * X;
    V = Y;
    W = 1.0 / 2.0 * (-X + 3.0 * Y + Z);
    s = U + V + W;
    uv_out->v[0] = (ALWAN_CORE_T)(U / s);
    uv_out->v[1] = (ALWAN_CORE_T)(V / s);
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

    /* Copy the camera's registry tables into the three SPDs. A table compiled out
     * sets `missing`: ALWAN_E_NODATA. */
    int missing = 0;
    switch (camera) {
        case ALWAN_CAMERA_NIKON_5100: {
#if ALWAN_TABLE_CAMERA_NIKON_5100
            ALWAN_CORE_FN(spd_copy_table)(spd_r, ALWAN_CORE_FNLIT(alwan_table_camera_nikon_5100_r));
            ALWAN_CORE_FN(spd_copy_table)(spd_g, ALWAN_CORE_FNLIT(alwan_table_camera_nikon_5100_g));
            ALWAN_CORE_FN(spd_copy_table)(spd_b, ALWAN_CORE_FNLIT(alwan_table_camera_nikon_5100_b));
#else
            missing = 1;
#endif
            break;
        }
        case ALWAN_CAMERA_SIGMA_SDMERILL: {
#if ALWAN_TABLE_CAMERA_SIGMA_SDMERILL
            ALWAN_CORE_FN(spd_copy_table)(spd_r, ALWAN_CORE_FNLIT(alwan_table_camera_sigma_sdmerill_r));
            ALWAN_CORE_FN(spd_copy_table)(spd_g, ALWAN_CORE_FNLIT(alwan_table_camera_sigma_sdmerill_g));
            ALWAN_CORE_FN(spd_copy_table)(spd_b, ALWAN_CORE_FNLIT(alwan_table_camera_sigma_sdmerill_b));
#else
            missing = 1;
#endif
            break;
        }
        default:
            return ALWAN_E_INVALID;
    }
    if (missing) {
        return ALWAN_E_NODATA;
    }

    (void)ctx;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_xyz_from_spd_camera)(ALWAN_CORE_XYZ *xyz_out, ALWAN_CORE_SPD const *spd, ALWAN_CORE_SPD const *illuminant, alwan_camera_sensitivity camera, alwan_integrate_method method, alwan_ctx *ctx) {
    if (!spd || !xyz_out) {
        return ALWAN_E_INVALID;
    }
    /* Anything but the two rules used to fall through to Simpson. */
    if ((int)method != (int)ALWAN_INTEGRATE_TRAPEZOID && (int)method != (int)ALWAN_INTEGRATE_SIMPSON) {
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

/* ================================================================
 * Spectral foundation: observers as SPDs, SPD generators, and the
 * weights that integrate a whole multispectral image at once.
 * ================================================================ */

/* The observer's three functions as SPDs on the 360-830 nm 1 nm grid. */
alwan_status ALWAN_CORE_FNLIT(alwan_spd_observer)(ALWAN_CORE_SPD *x_bar, ALWAN_CORE_SPD *y_bar,
                                                  ALWAN_CORE_SPD *z_bar, alwan_observer_type observer,
                                                  alwan_ctx *ctx) {
    if (!x_bar || !y_bar || !z_bar) {
        return ALWAN_E_INVALID;
    }
    return (alwan_status)ALWAN_CORE_FN(load_observer_cmf)(ctx, observer, x_bar, y_bar, z_bar);
}

/* Round half to even at three decimals, which is numpy.around(v, 3) and so what
 * colour-science applies to M1 and M2: scale, round to the nearest integer with
 * ties to even, unscale. */
static ALWAN_CORE_T ALWAN_CORE_FN(spd_round3)(ALWAN_CORE_T v) {
    ALWAN_CORE_T const s = v * ALWAN_CORE_LITERAL(1000.0);
    ALWAN_CORE_T const f = ALWAN_CORE_FLOOR(s);
    ALWAN_CORE_T const d = s - f;
    ALWAN_CORE_T r;
    if (d > ALWAN_CORE_LITERAL(0.5)) {
        r = f + ALWAN_CORE_LITERAL(1.0);
    } else if (d < ALWAN_CORE_LITERAL(0.5)) {
        r = f;
    } else {
        r = (ALWAN_CORE_FLOOR(f * ALWAN_CORE_LITERAL(0.5)) * ALWAN_CORE_LITERAL(2.0) == f)
            ? f : f + ALWAN_CORE_LITERAL(1.0);
    }
    return r / ALWAN_CORE_LITERAL(1000.0);
}

/* CIE daylight at a chromaticity, CIE 015:2004. The basis S0/S1/S2 is the table the
 * TM-30 and CRI reference illuminants already use, on 360-830 nm at 5 nm; the SPD is
 * formed at the basis wavelengths and interpolated linearly between them, which is
 * colour.sd_CIE_illuminant_D_series followed by a linear align. */
alwan_status ALWAN_CORE_FNLIT(alwan_spd_cie_daylight)(ALWAN_CORE_SPD *out, ALWAN_CORE_VEC2 const *xy,
        int round_m1_m2, ALWAN_CORE_T wavelength_min, ALWAN_CORE_T wavelength_max, size_t count,
        alwan_ctx *ctx) {
    if (!out || !xy) {
        return ALWAN_E_INVALID;
    }
    if (!(wavelength_min >= ALWAN_CORE_LITERAL(360.0)) || !(wavelength_max <= ALWAN_CORE_LITERAL(830.0))) {
        return ALWAN_E_RANGE;
    }
#if !ALWAN_TABLE_DAYLIGHT_BASIS
    (void)round_m1_m2; (void)count; (void)ctx;
    return ALWAN_E_NODATA;
#else
    {
        ALWAN_CORE_T const x = xy->v[0];
        ALWAN_CORE_T const y = xy->v[1];
        ALWAN_CORE_T const m = ALWAN_CORE_LITERAL(0.0241) + ALWAN_CORE_LITERAL(0.2562) * x
                             - ALWAN_CORE_LITERAL(0.7341) * y;
        ALWAN_CORE_T m1, m2;
        alwan_status st;
        size_t i;
        if (!(ALWAN_CORE_ABS(m) > ALWAN_CORE_LITERAL(1e-12))) {
            return ALWAN_E_RANGE;
        }
        m1 = (ALWAN_CORE_LITERAL(-1.3515) - ALWAN_CORE_LITERAL(1.7703) * x
              + ALWAN_CORE_LITERAL(5.9114) * y) / m;
        m2 = (ALWAN_CORE_LITERAL(0.0300) - ALWAN_CORE_LITERAL(31.4424) * x
              + ALWAN_CORE_LITERAL(30.0717) * y) / m;
        if (round_m1_m2) {
            m1 = ALWAN_CORE_FN(spd_round3)(m1);
            m2 = ALWAN_CORE_FN(spd_round3)(m2);
        }
        st = ALWAN_CORE_FNLIT(alwan_spd_create)(out, wavelength_min, wavelength_max, count, ctx);
        if (st != ALWAN_OK) {
            return st;
        }
        for (i = 0; i < count; i++) {
            ALWAN_CORE_T const wl = ALWAN_CORE_FN(spd_wavelength_at)(out, i);
            ALWAN_CORE_T const p = (wl - ALWAN_CORE_LITERAL(360.0)) / ALWAN_CORE_LITERAL(5.0);
            int lo = (int)ALWAN_CORE_FLOOR(p);
            ALWAN_CORE_T f, s_lo, s_hi;
            if (lo < 0) lo = 0;
            if (lo > ALWAN_TABLE_QUALITY_SPECTRUM - 2) lo = ALWAN_TABLE_QUALITY_SPECTRUM - 2;
            f = p - (ALWAN_CORE_T)lo;
            s_lo = (ALWAN_CORE_T)alwan_table2d_row_at_f64_v(alwan_table_daylight_basis_f64,
                       ALWAN_TABLE_DAYLIGHT_BASIS_ROWS, ALWAN_TABLE_QUALITY_SPECTRUM, 0, lo)
                 + m1 * (ALWAN_CORE_T)alwan_table2d_row_at_f64_v(alwan_table_daylight_basis_f64,
                       ALWAN_TABLE_DAYLIGHT_BASIS_ROWS, ALWAN_TABLE_QUALITY_SPECTRUM, 1, lo)
                 + m2 * (ALWAN_CORE_T)alwan_table2d_row_at_f64_v(alwan_table_daylight_basis_f64,
                       ALWAN_TABLE_DAYLIGHT_BASIS_ROWS, ALWAN_TABLE_QUALITY_SPECTRUM, 2, lo);
            s_hi = (ALWAN_CORE_T)alwan_table2d_row_at_f64_v(alwan_table_daylight_basis_f64,
                       ALWAN_TABLE_DAYLIGHT_BASIS_ROWS, ALWAN_TABLE_QUALITY_SPECTRUM, 0, lo + 1)
                 + m1 * (ALWAN_CORE_T)alwan_table2d_row_at_f64_v(alwan_table_daylight_basis_f64,
                       ALWAN_TABLE_DAYLIGHT_BASIS_ROWS, ALWAN_TABLE_QUALITY_SPECTRUM, 1, lo + 1)
                 + m2 * (ALWAN_CORE_T)alwan_table2d_row_at_f64_v(alwan_table_daylight_basis_f64,
                       ALWAN_TABLE_DAYLIGHT_BASIS_ROWS, ALWAN_TABLE_QUALITY_SPECTRUM, 2, lo + 1);
            out->values[i] = s_lo + f * (s_hi - s_lo);
        }
        return ALWAN_OK;
    }
#endif
}

/* A Gaussian source by peak and full width at half maximum. 2 sqrt(2 ln 2) is the
 * FWHM of a unit-sigma Gaussian. */
alwan_status ALWAN_CORE_FNLIT(alwan_spd_gaussian)(ALWAN_CORE_SPD *out, ALWAN_CORE_T peak_nm,
        ALWAN_CORE_T fwhm_nm, ALWAN_CORE_T wavelength_min, ALWAN_CORE_T wavelength_max, size_t count,
        alwan_ctx *ctx) {
    alwan_status st;
    ALWAN_CORE_T sigma, two_sigma_sq;
    size_t i;
    if (!out || !(fwhm_nm > ALWAN_CORE_LITERAL(0.0))) {
        return ALWAN_E_INVALID;
    }
    st = ALWAN_CORE_FNLIT(alwan_spd_create)(out, wavelength_min, wavelength_max, count, ctx);
    if (st != ALWAN_OK) {
        return st;
    }
    sigma = fwhm_nm / ALWAN_CORE_LITERAL(2.3548200450309493);
    two_sigma_sq = ALWAN_CORE_LITERAL(2.0) * sigma * sigma;
    for (i = 0; i < count; i++) {
        ALWAN_CORE_T const d = ALWAN_CORE_FN(spd_wavelength_at)(out, i) - peak_nm;
        out->values[i] = ALWAN_CORE_EXP(-(d * d) / two_sigma_sq);
    }
    return ALWAN_OK;
}

/* Ohno 2005 LED model, one or several emitters: each is (g + 2 g^5) / 3 with
 * g = exp(-((lambda - peak) / half_width)^2), weighted by its peak power. */
alwan_status ALWAN_CORE_FNLIT(alwan_spd_led_ohno2005)(ALWAN_CORE_SPD *out, ALWAN_CORE_T const *peak_nm,
        ALWAN_CORE_T const *half_width_nm, ALWAN_CORE_T const *peak_power, size_t led_count,
        ALWAN_CORE_T wavelength_min, ALWAN_CORE_T wavelength_max, size_t count, alwan_ctx *ctx) {
    alwan_status st;
    size_t i, k;
    if (!out || !peak_nm || !half_width_nm || led_count == 0) {
        return ALWAN_E_INVALID;
    }
    for (k = 0; k < led_count; k++) {
        if (!(half_width_nm[k] > ALWAN_CORE_LITERAL(0.0))) {
            return ALWAN_E_INVALID;
        }
    }
    st = ALWAN_CORE_FNLIT(alwan_spd_create)(out, wavelength_min, wavelength_max, count, ctx);
    if (st != ALWAN_OK) {
        return st;
    }
    for (i = 0; i < count; i++) {
        ALWAN_CORE_T const wl = ALWAN_CORE_FN(spd_wavelength_at)(out, i);
        ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
        for (k = 0; k < led_count; k++) {
            ALWAN_CORE_T const u = (wl - peak_nm[k]) / half_width_nm[k];
            ALWAN_CORE_T const g = ALWAN_CORE_EXP(-(u * u));
            ALWAN_CORE_T const g2 = g * g;
            ALWAN_CORE_T const v = (g + ALWAN_CORE_LITERAL(2.0) * (g2 * g2 * g)) / ALWAN_CORE_LITERAL(3.0);
            sum += v * (peak_power ? peak_power[k] : ALWAN_CORE_LITERAL(1.0));
        }
        out->values[i] = sum;
    }
    return ALWAN_OK;
}

/* The coefficient each sample carries in alwan_xyz_from_spd's integrators, so that
 * the weights below reproduce it: trapezoid, or Simpson's 1/3 rule with a closing
 * trapezoid when the sample count is even. Multiply by the sample interval. */
static ALWAN_CORE_T ALWAN_CORE_FN(integration_coefficient)(size_t i, size_t n, alwan_integrate_method method) {
    if (n == 1) return ALWAN_CORE_LITERAL(1.0);
    if (method == ALWAN_INTEGRATE_TRAPEZOID || n == 2) {
        return (i == 0 || i == n - 1) ? ALWAN_CORE_LITERAL(0.5) : ALWAN_CORE_LITERAL(1.0);
    } else {
        size_t const m = (n % 2 == 0) ? n - 1 : n;   /* points covered by Simpson */
        ALWAN_CORE_T c = ALWAN_CORE_LITERAL(0.0);
        if (i < m) {
            if (i == 0 || i == m - 1) c = ALWAN_CORE_LITERAL(1.0);
            else c = (i % 2 == 1) ? ALWAN_CORE_LITERAL(4.0) : ALWAN_CORE_LITERAL(2.0);
            c /= ALWAN_CORE_LITERAL(3.0);
        }
        if (m != n) {
            if (i == m - 1) c += ALWAN_CORE_LITERAL(0.5);
            if (i == n - 1) c = ALWAN_CORE_LITERAL(0.5);
        }
        return c;
    }
}

/* Weights that integrate a band_count-sample spectrum against three responses. */
alwan_status ALWAN_CORE_FNLIT(alwan_spectral_weights)(ALWAN_CORE_T *weights_out, size_t band_count,
        ALWAN_CORE_T wavelength_min, ALWAN_CORE_T wavelength_max, ALWAN_CORE_SPD const *illuminant,
        ALWAN_CORE_SPD const *response_0, ALWAN_CORE_SPD const *response_1, ALWAN_CORE_SPD const *response_2,
        alwan_integrate_method method, int normalize) {
    ALWAN_CORE_SPD const *resp[3];
    ALWAN_CORE_SPD grid;
    ALWAN_CORE_T dx;
    size_t i, c;
    if (!weights_out || !response_0 || !response_1 || !response_2 || band_count == 0) {
        return ALWAN_E_INVALID;
    }
    if (band_count > 1 && !(wavelength_max > wavelength_min)) {
        return ALWAN_E_INVALID;
    }
    if (method != ALWAN_INTEGRATE_TRAPEZOID && method != ALWAN_INTEGRATE_SIMPSON) {
        return ALWAN_E_INVALID;
    }
    resp[0] = response_0; resp[1] = response_1; resp[2] = response_2;
    for (c = 0; c < 3; c++) {
        if (!resp[c]->values || resp[c]->count == 0) return ALWAN_E_INVALID;
    }
    if (illuminant && (!illuminant->values || illuminant->count == 0)) {
        return ALWAN_E_INVALID;
    }
    /* A view of the band grid, used only for its wavelengths. */
    grid.values = NULL;
    grid.wavelength_min = wavelength_min;
    grid.wavelength_max = wavelength_max;
    grid.count = band_count;
    dx = (band_count > 1)
       ? (wavelength_max - wavelength_min) / (ALWAN_CORE_T)(band_count - 1)
       : ALWAN_CORE_LITERAL(1.0);
    for (i = 0; i < band_count; i++) {
        ALWAN_CORE_T const wl = ALWAN_CORE_FN(spd_wavelength_at)(&grid, i);
        ALWAN_CORE_T const e = illuminant
            ? ALWAN_CORE_FN(spd_interpolate)(illuminant, wl, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_ZERO)
            : ALWAN_CORE_LITERAL(1.0);
        ALWAN_CORE_T const k = ALWAN_CORE_FN(integration_coefficient)(i, band_count, method) * dx;
        for (c = 0; c < 3; c++) {
            weights_out[c * band_count + i] = k * e * ALWAN_CORE_FN(spd_interpolate)(
                resp[c], wl, ALWAN_RESAMPLE_LINEAR, ALWAN_EXTRAPOLATE_CONSTANT);
        }
    }
    if (normalize) {
        ALWAN_CORE_T white = ALWAN_CORE_LITERAL(0.0);
        for (i = 0; i < band_count; i++) white += weights_out[band_count + i];
        if (!(white > ALWAN_CORE_LITERAL(0.0))) {
            return ALWAN_E_RANGE;
        }
        for (i = 0; i < 3 * band_count; i++) weights_out[i] /= white;
    }
    return ALWAN_OK;
}

/* The same weights for a standard observer, whose functions this loads. */
alwan_status ALWAN_CORE_FNLIT(alwan_spectral_weights_observer)(ALWAN_CORE_T *weights_out, size_t band_count,
        ALWAN_CORE_T wavelength_min, ALWAN_CORE_T wavelength_max, ALWAN_CORE_SPD const *illuminant,
        alwan_observer_type observer, alwan_integrate_method method, int normalize, alwan_ctx *ctx) {
    ALWAN_CORE_SPD x_bar, y_bar, z_bar;
    alwan_status st;
    if (!weights_out || band_count == 0) {
        return ALWAN_E_INVALID;
    }
    st = (alwan_status)ALWAN_CORE_FN(load_observer_cmf)(ctx, observer, &x_bar, &y_bar, &z_bar);
    if (st != ALWAN_OK) {
        return st;
    }
    st = ALWAN_CORE_FNLIT(alwan_spectral_weights)(weights_out, band_count, wavelength_min, wavelength_max,
                                                  illuminant, &x_bar, &y_bar, &z_bar, method, normalize);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&x_bar, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&y_bar, ctx);
    ALWAN_CORE_FNLIT(alwan_spd_destroy)(&z_bar, ctx);
    return st;
}

#ifndef ALWAN_SPD_MAPNAME
#  define ALWAN_SPD_MAPNAME_(base, sfx) base##sfx##_map_interleave
#  define ALWAN_SPD_MAPNAME(base, sfx)  ALWAN_SPD_MAPNAME_(base, sfx)
#endif

/* A multispectral buffer to three channels: out_c = sum_i weights[c][i] * sample_i,
 * summed left to right so the result is the same in every build. */
alwan_status ALWAN_SPD_MAPNAME(alwan_spectral_to_tristimulus, ALWAN_CORE_SUFFIX)(
        ALWAN_CORE_T *out, size_t out_stride, ALWAN_CORE_T const *in, size_t in_stride, size_t count,
        size_t band_count, ALWAN_CORE_T const *weights) {
    size_t p, i;
    if (!out || !in || !weights || band_count == 0) {
        return ALWAN_E_INVALID;
    }
    for (p = 0; p < count; p++) {
        ALWAN_CORE_T const *s = (ALWAN_CORE_T const *)((char const *)in + p * in_stride);
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)out + p * out_stride);
        ALWAN_CORE_T a0 = ALWAN_CORE_LITERAL(0.0), a1 = ALWAN_CORE_LITERAL(0.0), a2 = ALWAN_CORE_LITERAL(0.0);
        for (i = 0; i < band_count; i++) {
            ALWAN_CORE_T const v = s[i];
            a0 += weights[i] * v;
            a1 += weights[band_count + i] * v;
            a2 += weights[2 * band_count + i] * v;
        }
        o[0] = a0; o[1] = a1; o[2] = a2;
    }
    return ALWAN_OK;
}

/* Camera RGB from an SPD, under its own name: alwan_xyz_from_spd_camera returns the
 * same three numbers in an alwan_xyz. */
alwan_status ALWAN_CORE_FNLIT(alwan_camera_rgb_from_spd)(ALWAN_CORE_RGB *rgb_out, ALWAN_CORE_SPD const *spd,
        ALWAN_CORE_SPD const *illuminant, alwan_camera_sensitivity camera, alwan_integrate_method method,
        alwan_ctx *ctx) {
    ALWAN_CORE_XYZ t;
    alwan_status st;
    if (!rgb_out) {
        return ALWAN_E_INVALID;
    }
    st = ALWAN_CORE_FNLIT(alwan_xyz_from_spd_camera)(&t, spd, illuminant, camera, method, ctx);
    if (st == ALWAN_OK) {
        rgb_out->r = t.x;
        rgb_out->g = t.y;
        rgb_out->b = t.z;
    }
    return st;
}
