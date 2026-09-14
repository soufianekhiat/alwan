/* No include guard -- included twice (f32 and f64 passes) */

/*
 * Exposure and bracket merging, after colour-hdri (colour_hdri.exposure and
 * colour_hdri.generation), which the tests hold this to.
 *
 *   exposure   ISO 2720 reflected and incident light meter equations, ISO 12232
 *              focal plane exposure, saturation-based speed and exposure index,
 *              and Lagarde's 2014 photometric scale factor. EXIF numbers go in as
 *              plain values.
 *   merge      the weighted radiance merge of an exposure bracket, with the four
 *              weighting functions colour-hdri ships.
 */

/* The settings type of this pass: alwan_exposure_settings_f32 or _f64. */
#define ALWAN__EXPOSURE ALWAN_CORE_FNLIT(alwan_exposure_settings)
#define ALWAN__PI ALWAN_CORE_LITERAL(3.14159265358979323846)

/* ISO 12232 q: the share of scene luminance that reaches the focal plane, from the
 * lens transmittance T, the vignetting factor f_v and the angle off axis theta in
 * degrees. */
static ALWAN_CORE_T ALWAN_CORE_FN(exposure_q)(ALWAN_CORE_T T, ALWAN_CORE_T f_v, ALWAN_CORE_T theta) {
    ALWAN_CORE_T const c = ALWAN_CORE_COS(theta * (ALWAN__PI / ALWAN_CORE_LITERAL(180.0)));
    return ALWAN__PI / ALWAN_CORE_LITERAL(4.0) * T * f_v * (c * c * c * c);
}

static int ALWAN_CORE_FN(exposure_ok)(ALWAN_CORE_T f_number, ALWAN_CORE_T exposure_time, ALWAN_CORE_T iso) {
    return f_number > ALWAN_CORE_ZERO && exposure_time > ALWAN_CORE_ZERO && iso > ALWAN_CORE_ZERO;
}

/* A calibration constant: 0 takes the default, anything else must be positive. */
static int ALWAN_CORE_FN(exposure_constant)(ALWAN_CORE_T *out, ALWAN_CORE_T value, ALWAN_CORE_T fallback) {
    if (value == ALWAN_CORE_ZERO) {
        *out = fallback;
        return 1;
    }
    *out = value;
    return value > ALWAN_CORE_ZERO;
}

alwan_status ALWAN_CORE_FNLIT(alwan_average_luminance)(ALWAN_CORE_T *luminance_out, ALWAN_CORE_T f_number,
                                                       ALWAN_CORE_T exposure_time, ALWAN_CORE_T iso,
                                                       ALWAN_CORE_T k) {
    if (!luminance_out || !ALWAN_CORE_FN(exposure_ok)(f_number, exposure_time, iso)
        || !ALWAN_CORE_FN(exposure_constant)(&k, k, ALWAN_CORE_LITERAL(12.5))) {
        return ALWAN_E_INVALID;
    }
    *luminance_out = f_number * f_number / exposure_time / iso * k;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_average_illuminance)(ALWAN_CORE_T *illuminance_out, ALWAN_CORE_T f_number,
                                                         ALWAN_CORE_T exposure_time, ALWAN_CORE_T iso,
                                                         ALWAN_CORE_T c) {
    if (!illuminance_out || !ALWAN_CORE_FN(exposure_ok)(f_number, exposure_time, iso)
        || !ALWAN_CORE_FN(exposure_constant)(&c, c, ALWAN_CORE_LITERAL(250.0))) {
        return ALWAN_E_INVALID;
    }
    *illuminance_out = f_number * f_number / exposure_time / iso * c;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_luminance_to_exposure_value)(ALWAN_CORE_T *ev_out, ALWAN_CORE_T luminance,
                                                                 ALWAN_CORE_T iso, ALWAN_CORE_T k) {
    if (!ev_out || !(luminance > ALWAN_CORE_ZERO) || !(iso > ALWAN_CORE_ZERO)
        || !ALWAN_CORE_FN(exposure_constant)(&k, k, ALWAN_CORE_LITERAL(12.5))) {
        return ALWAN_E_INVALID;
    }
    *ev_out = ALWAN_CORE_LOG2(luminance * iso / k);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_illuminance_to_exposure_value)(ALWAN_CORE_T *ev_out, ALWAN_CORE_T illuminance,
                                                                   ALWAN_CORE_T iso, ALWAN_CORE_T c) {
    if (!ev_out || !(illuminance > ALWAN_CORE_ZERO) || !(iso > ALWAN_CORE_ZERO)
        || !ALWAN_CORE_FN(exposure_constant)(&c, c, ALWAN_CORE_LITERAL(250.0))) {
        return ALWAN_E_INVALID;
    }
    *ev_out = ALWAN_CORE_LOG2(illuminance * iso / c);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_exposure_value_100)(ALWAN_CORE_T *ev100_out, ALWAN_CORE_T f_number,
                                                        ALWAN_CORE_T exposure_time, ALWAN_CORE_T iso) {
    ALWAN_CORE_T L;
    alwan_status const st = ALWAN_CORE_FNLIT(alwan_average_luminance)(&L, f_number, exposure_time, iso,
                                                                      ALWAN_CORE_ZERO);
    if (!ev100_out || st != ALWAN_OK) {
        return ALWAN_E_INVALID;
    }
    return ALWAN_CORE_FNLIT(alwan_luminance_to_exposure_value)(ev100_out, L, ALWAN_CORE_LITERAL(100.0),
                                                               ALWAN_CORE_ZERO);
}

alwan_status ALWAN_CORE_FNLIT(alwan_focal_plane_exposure)(ALWAN_CORE_T *exposure_out, ALWAN_CORE_T luminance,
                                                          ALWAN_CORE_T f_number, ALWAN_CORE_T exposure_time,
                                                          ALWAN_CORE_T focal_length, ALWAN_CORE_T image_distance,
                                                          ALWAN_CORE_T flare, ALWAN_CORE_T transmittance,
                                                          ALWAN_CORE_T vignetting, ALWAN_CORE_T angle) {
    ALWAN_CORE_T q;
    if (!exposure_out || !(f_number > ALWAN_CORE_ZERO) || !(image_distance != ALWAN_CORE_ZERO)) {
        return ALWAN_E_INVALID;
    }
    q = ALWAN_CORE_FN(exposure_q)(transmittance, vignetting, angle);
    *exposure_out = q * (luminance * exposure_time * focal_length * focal_length)
                  / (f_number * f_number * image_distance * image_distance) + flare;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_saturation_based_speed_focal_plane_exposure)(
        ALWAN_CORE_T *exposure_out, ALWAN_CORE_T luminance, ALWAN_CORE_T f_number, ALWAN_CORE_T exposure_time,
        ALWAN_CORE_T iso, ALWAN_CORE_T focal_length, ALWAN_CORE_T image_distance, ALWAN_CORE_T flare,
        ALWAN_CORE_T transmittance, ALWAN_CORE_T vignetting, ALWAN_CORE_T angle) {
    ALWAN_CORE_T H;
    alwan_status const st = ALWAN_CORE_FNLIT(alwan_focal_plane_exposure)(&H, luminance, f_number, exposure_time,
                                                                         focal_length, image_distance, flare,
                                                                         transmittance, vignetting, angle);
    if (!exposure_out || st != ALWAN_OK) {
        return ALWAN_E_INVALID;
    }
    *exposure_out = H * iso / ALWAN_CORE_LITERAL(78.0);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_exposure_index)(ALWAN_CORE_T *index_out, ALWAN_CORE_T focal_plane_exposure) {
    if (!index_out || !(focal_plane_exposure > ALWAN_CORE_ZERO)) {
        return ALWAN_E_INVALID;
    }
    *index_out = ALWAN_CORE_LITERAL(10.0) / focal_plane_exposure;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_photometric_exposure_scale_factor_lagarde2014)(
        ALWAN_CORE_T *scale_out, ALWAN_CORE_T ev100, ALWAN_CORE_T transmittance, ALWAN_CORE_T vignetting,
        ALWAN_CORE_T angle) {
    ALWAN_CORE_T q;
    if (!scale_out) {
        return ALWAN_E_INVALID;
    }
    q = ALWAN_CORE_FN(exposure_q)(transmittance, vignetting, angle);
    if (!(q > ALWAN_CORE_ZERO)) {
        return ALWAN_E_INVALID;
    }
    *scale_out = ALWAN_CORE_ONE / (ALWAN_CORE_LITERAL(78.0) / (ALWAN_CORE_LITERAL(100.0) * q)
                                   * ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), ev100));
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Bracket merging
 * ---------------------------------------------------------------- */

/* colour-hdri's anchored sigmoid with its k = 2. */
static ALWAN_CORE_T ALWAN_CORE_FN(merge_sigmoid)(ALWAN_CORE_T x, ALWAN_CORE_T c, ALWAN_CORE_T d) {
    ALWAN_CORE_T const r = (x - c) / (d - c);
    ALWAN_CORE_T u;
    if (!(r > ALWAN_CORE_ZERO)) {
        return ALWAN_CORE_ZERO;
    }
    u = ALWAN_CORE_ONE / r - ALWAN_CORE_ONE;
    return ALWAN_CORE_ONE / (ALWAN_CORE_ONE + u * u);
}

static ALWAN_CORE_T ALWAN_CORE_FN(merge_weight)(ALWAN_CORE_T a, alwan_merge_weight fn) {
    switch (fn) {
    case ALWAN_MERGE_WEIGHT_HAT: {
        ALWAN_CORE_T const x = ALWAN_CORE_LITERAL(2.0) * a - ALWAN_CORE_ONE;
        ALWAN_CORE_T const x2 = x * x;
        ALWAN_CORE_T const x4 = x2 * x2;
        return ALWAN_CORE_ONE - x4 * x4 * x4;
    }
    case ALWAN_MERGE_WEIGHT_NORMAL: {
        ALWAN_CORE_T const d = a - ALWAN_CORE_LITERAL(0.5);
        return ALWAN_CORE_EXP(-(d * d) / (ALWAN_CORE_LITERAL(2.0) * (ALWAN_CORE_LITERAL(0.15) * ALWAN_CORE_LITERAL(0.15))));
    }
    case ALWAN_MERGE_WEIGHT_DOUBLE_SIGMOID:
        if (a <= ALWAN_CORE_LITERAL(0.025)) return ALWAN_CORE_ZERO;
        if (a <= ALWAN_CORE_LITERAL(0.2)) {
            return ALWAN_CORE_FN(merge_sigmoid)(a, ALWAN_CORE_LITERAL(0.025), ALWAN_CORE_LITERAL(0.2));
        }
        if (a < ALWAN_CORE_LITERAL(0.8)) return ALWAN_CORE_ONE;
        if (a < ALWAN_CORE_LITERAL(0.975)) {
            return ALWAN_CORE_ONE - ALWAN_CORE_FN(merge_sigmoid)(a, ALWAN_CORE_LITERAL(0.8), ALWAN_CORE_LITERAL(0.975));
        }
        return ALWAN_CORE_ZERO;
    default: {
        /* Debevec 1997: a triangle on [0.01, 0.99], normalised by its own peak.
         * colour-hdri divides by the largest weight in the image instead, which is
         * the same number whenever the image holds a value at the midpoint. */
        ALWAN_CORE_T const lo = ALWAN_CORE_LITERAL(0.01);
        ALWAN_CORE_T const hi = ALWAN_CORE_LITERAL(0.99);
        ALWAN_CORE_T const mid = (lo + hi) / ALWAN_CORE_LITERAL(2.0);
        ALWAN_CORE_T const w = (a <= mid ? a - lo : hi - a) / (mid - lo);
        return w < ALWAN_CORE_ZERO ? ALWAN_CORE_ZERO : w;
    }
    }
}

alwan_status ALWAN_CORE_FNLIT(alwan_hdr_merge_weight)(ALWAN_CORE_T *weight_out, ALWAN_CORE_T value,
                                                      alwan_merge_weight fn) {
    if (!weight_out || (int)fn < 0 || (int)fn > (int)ALWAN_MERGE_WEIGHT_DOUBLE_SIGMOID) {
        return ALWAN_E_INVALID;
    }
    *weight_out = ALWAN_CORE_FN(merge_weight)(value, fn);
    return ALWAN_OK;
}

#ifndef ALWAN_EXPOSURE_MAPNAME
#  define ALWAN_EXPOSURE_MAPNAME_(base, sfx) base##sfx##_map_interleave
#  define ALWAN_EXPOSURE_MAPNAME(base, sfx)  ALWAN_EXPOSURE_MAPNAME_(base, sfx)
#endif

alwan_status ALWAN_EXPOSURE_MAPNAME(alwan_hdr_merge, ALWAN_CORE_SUFFIX)(
        ALWAN_CORE_T *out, size_t out_stride,
        ALWAN_CORE_T const *const *images, size_t in_stride, size_t count,
        ALWAN__EXPOSURE const *settings, size_t image_count,
        alwan_merge_weight weight, ALWAN_CORE_T const *response, size_t response_size) {
    ALWAN_CORE_T const eps = ALWAN_CORE_LITERAL(2.220446049250313e-16);
    size_t i;
    if (!out || !images || !settings || image_count == 0
        || (int)weight < 0 || (int)weight > (int)ALWAN_MERGE_WEIGHT_DOUBLE_SIGMOID) {
        return ALWAN_E_INVALID;
    }
    if (response && (response_size < 2 || response_size > 1u << 24)) {
        return ALWAN_E_INVALID;
    }
    for (i = 0; i < image_count; i++) {
        if (!images[i] || !ALWAN_CORE_FN(exposure_ok)(settings[i].f_number, settings[i].exposure_time,
                                                      settings[i].iso)) {
            return ALWAN_E_INVALID;
        }
    }
    for (size_t p = 0; p < count; p++) {
        ALWAN_CORE_T num[3] = { ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ALWAN_CORE_ZERO };
        ALWAN_CORE_T den[3] = { ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ALWAN_CORE_ZERO };
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)out + p * out_stride);
        for (i = 0; i < image_count; i++) {
            ALWAN_CORE_T const *s = (ALWAN_CORE_T const *)((char const *)images[i] + p * in_stride);
            ALWAN_CORE_T const N = settings[i].f_number;
            /* colour-hdri's L: the reciprocal of the ISO 2720 average luminance. */
            ALWAN_CORE_T const L = ALWAN_CORE_ONE / (N * N / settings[i].exposure_time / settings[i].iso
                                                     * ALWAN_CORE_LITERAL(12.5));
            for (int c = 0; c < 3; c++) {
                ALWAN_CORE_T d = s[c];
                ALWAN_CORE_T w;
                /* The merge is defined on normalised sensor values: below the
                 * smallest positive value there is no signal, above 1 the
                 * photosite has saturated. */
                d = d < eps ? eps : (d > ALWAN_CORE_ONE ? ALWAN_CORE_ONE : d);
                w = ALWAN_CORE_FN(merge_weight)(d, weight);
                w = w < eps ? eps : (w > ALWAN_CORE_ONE ? ALWAN_CORE_ONE : w);
                /* The shortest exposure is trusted in the highlights, the longest
                 * in the shadows, whatever the weighting function says. */
                if (i == 0 && d >= ALWAN_CORE_LITERAL(0.5)) w = ALWAN_CORE_ONE;
                if (i == image_count - 1 && d <= ALWAN_CORE_LITERAL(0.5)) w = ALWAN_CORE_ONE;
                if (response) {
                    ALWAN_CORE_T v = d;
                    (void)ALWAN_CORE_FNLIT(alwan_table1d_sample)(&v, response + (size_t)c * response_size,
                                                                (int)response_size, d, ALWAN_SAMPLE_LINEAR);
                    d = v;
                }
                num[c] += w * d / L;
                den[c] += w;
            }
        }
        o[0] = num[0] / den[0];
        o[1] = num[1] / den[1];
        o[2] = num[2] / den[2];
    }
    return ALWAN_OK;
}

#undef ALWAN__EXPOSURE
#undef ALWAN__PI
