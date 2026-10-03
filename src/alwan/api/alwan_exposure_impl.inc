/* No include guard -- included twice (f32 and f64 passes) */

/*
 * Exposure and bracket merging, after colour-hdri (colour_hdri.exposure and
 * colour_hdri.generation), which the tests hold this to.
 *
 *   exposure   ISO 2720 reflected and incident light meter equations, ISO 12232
 *              focal plane exposure, saturation-based speed and exposure index,
 *              and Lagarde's 2014 photometric scale factor. EXIF numbers go in as
 *              plain values.
 * The bracket merge that used to sit here is image processing: alwan_hdr_merge_impl.inc,
 * kept apart so it moves to suwar as a file.
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

#undef ALWAN__EXPOSURE
#undef ALWAN__PI
