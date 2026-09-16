/* No include guard -- included twice (f32 and f64 passes) */

/* ================================================================
 * Accessibility Contrast Metrics
 * ================================================================ */

alwan_status ALWAN_CORE_FNLIT(alwan_apca_contrast)(ALWAN_CORE_T *Lc_out,
                                          ALWAN_CORE_RGB const *srgb_text,
                                          ALWAN_CORE_RGB const *srgb_bg) {
    if (!Lc_out || !srgb_text || !srgb_bg) return ALWAN_E_INVALID;
    *Lc_out = ALWAN_CORE_FNVLIT(alwan_apca_contrast)(*srgb_text, *srgb_bg);
    return ALWAN_OK;
}

/* ================================================================
 * Color Vision Deficiency (CVD) Simulation
 * ================================================================ */

alwan_status ALWAN_CORE_FNLIT(alwan_simulate_cvd)(ALWAN_CORE_RGB *rgb_out,
                        ALWAN_CORE_RGB const *rgb_in,
                        alwan_cvd_type cvd_type,
                        ALWAN_CORE_T severity) {
    if (!rgb_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_RGB result;

    switch (cvd_type) {
        case ALWAN_CVD_PROTANOPIA:
        case ALWAN_CVD_PROTANOMALY:
            result = ALWAN_CORE_FNVLIT(alwan_simulate_protanopia)(*rgb_in, severity);
            break;

        case ALWAN_CVD_DEUTERANOPIA:
        case ALWAN_CVD_DEUTERANOMALY:
            result = ALWAN_CORE_FNVLIT(alwan_simulate_deuteranopia)(*rgb_in, severity);
            break;

        case ALWAN_CVD_TRITANOPIA:
        case ALWAN_CVD_TRITANOMALY:
            result = ALWAN_CORE_FNVLIT(alwan_simulate_tritanopia)(*rgb_in, severity);
            break;

        default:
            return ALWAN_E_INVALID;
    }

    *rgb_out = result;
    return ALWAN_OK;
}

/* ================================================================
 * Machado 2009 CVD Simulation
 * ================================================================ */

alwan_status ALWAN_CORE_FNLIT(alwan_simulate_cvd_machado)(ALWAN_CORE_RGB *rgb_out,
                                ALWAN_CORE_RGB const *rgb_in,
                                alwan_cvd_type cvd_type,
                                ALWAN_CORE_T severity) {
    if (!rgb_in || !rgb_out) return ALWAN_E_INVALID;

    ALWAN_CORE_RGB result;

    switch (cvd_type) {
        case ALWAN_CVD_PROTANOPIA:
        case ALWAN_CVD_PROTANOMALY:
            result = ALWAN_CORE_FNVLIT(alwan_simulate_machado_protan)(*rgb_in, severity);
            break;
        case ALWAN_CVD_DEUTERANOPIA:
        case ALWAN_CVD_DEUTERANOMALY:
            result = ALWAN_CORE_FNVLIT(alwan_simulate_machado_deutan)(*rgb_in, severity);
            break;
        case ALWAN_CVD_TRITANOPIA:
        case ALWAN_CVD_TRITANOMALY:
            result = ALWAN_CORE_FNVLIT(alwan_simulate_machado_tritan)(*rgb_in, severity);
            break;
        default:
            return ALWAN_E_INVALID;
    }

    *rgb_out = result;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_simulate_cvd_ex)(ALWAN_CORE_RGB *rgb_out,
                           ALWAN_CORE_RGB const *rgb_in,
                           alwan_cvd_type cvd_type,
                           ALWAN_CORE_T severity,
                           alwan_cvd_model model) {
    switch (model) {
        case ALWAN_CVD_MODEL_BRETTEL:
            return ALWAN_CORE_FNLIT(alwan_simulate_cvd)(rgb_out, rgb_in, cvd_type, severity);
        case ALWAN_CVD_MODEL_MACHADO:
            return ALWAN_CORE_FNLIT(alwan_simulate_cvd_machado)(rgb_out, rgb_in, cvd_type, severity);
        default:
            return ALWAN_E_INVALID;
    }
}

/* ================================================================
 * Luminous Efficiency Functions
 * ================================================================ */

/* Interpolate interleaved {wavelength, value} pairs (stride 2) on the tables'
 * regular 1 nm grid, clamped to the end values. Templatized per precision so the
 * f32 path reads native float LUTs. */
static ALWAN_CORE_T ALWAN_CORE_FNLIT(interpolate_lut)(ALWAN_CORE_T const *data,
                                                      int count,
                                                      ALWAN_CORE_T wavelength) {
    if (wavelength <= data[0]) {
        return data[1];
    }
    if (wavelength >= data[(count - 1) * 2]) {
        return data[(count - 1) * 2 + 1];
    }

    ALWAN_CORE_TABLE_CELL const c =
        ALWAN_CORE_FNVLIT(alwan_table_cell_unit)((wavelength - data[0]) / (data[2] - data[0]), count);
    return data[c.i0 * 2 + 1] + c.frac * (data[c.i1 * 2 + 1] - data[c.i0 * 2 + 1]);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_vision_interpolate_photopic_lut)(ALWAN_CORE_T wavelength) {
    return ALWAN_CORE_FNLIT(interpolate_lut)(ALWAN_CORE_FNLIT(PHOTOPIC_V_DATA), PHOTOPIC_V_COUNT, wavelength);
}
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_vision_interpolate_scotopic_lut)(ALWAN_CORE_T wavelength) {
    return ALWAN_CORE_FNLIT(interpolate_lut)(ALWAN_CORE_FNLIT(SCOTOPIC_VP_DATA), SCOTOPIC_VP_COUNT, wavelength);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_luminous_efficiency)(ALWAN_CORE_T wavelength, alwan_vision_type vision_type) {
    if (wavelength < ALWAN_CORE_LITERAL(360.0) || wavelength > ALWAN_CORE_LITERAL(830.0)) {
        return ALWAN_CORE_LITERAL(-1.0);
    }

    switch (vision_type) {
        case ALWAN_VISION_PHOTOPIC:
            return ALWAN_CORE_FNLIT(alwan_vision_interpolate_photopic_lut)(wavelength);
        case ALWAN_VISION_SCOTOPIC:
            return ALWAN_CORE_FNLIT(alwan_vision_interpolate_scotopic_lut)(wavelength);
        case ALWAN_VISION_MESOPIC:
            return ALWAN_CORE_FNLIT(alwan_vision_interpolate_photopic_lut)(wavelength);
        default:
            return ALWAN_CORE_LITERAL(-1.0);
    }
}

/* ================================================================
 * Contrast Sensitivity Function (CSF)
 * ================================================================ */

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_csf)(ALWAN_CORE_T spatial_frequency, ALWAN_CORE_T luminance) {
    if (spatial_frequency < ALWAN_CORE_LITERAL(0.1) || spatial_frequency > ALWAN_CORE_LITERAL(60.0)) {
        return ALWAN_CORE_LITERAL(-1.0);
    }
    if (luminance < ALWAN_CORE_LITERAL(0.01) || luminance > ALWAN_CORE_LITERAL(10000.0)) {
        return ALWAN_CORE_LITERAL(-1.0);
    }

    return ALWAN_CORE_FNVLIT(alwan_csf_simple)(spatial_frequency, luminance);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_pupil_diameter_barten1999)(ALWAN_CORE_T L,
                                              ALWAN_CORE_T X_0,
                                              ALWAN_CORE_T Y_0) {
    return ALWAN_CORE_FNVLIT(alwan_pupil_diameter_barten1999)(L, X_0, Y_0);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_retinal_illuminance_barten1999)(ALWAN_CORE_T L,
                                                   ALWAN_CORE_T d,
                                                   int apply_stiles_crawford) {
    return ALWAN_CORE_FNVLIT(alwan_retinal_illuminance_barten1999)(L, d, (ALWAN_CORE_T)apply_stiles_crawford);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_optical_mtf_barten1999)(ALWAN_CORE_T u, ALWAN_CORE_T sigma) {
    return ALWAN_CORE_FNVLIT(alwan_optical_mtf_barten1999)(u, sigma);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_sigma_barten1999)(ALWAN_CORE_T sigma_0,
                                     ALWAN_CORE_T C_ab,
                                     ALWAN_CORE_T d) {
    return ALWAN_CORE_FNVLIT(alwan_sigma_barten1999)(sigma_0, C_ab, d);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_maximum_angular_size_barten1999)(ALWAN_CORE_T u,
                                                    ALWAN_CORE_T X_0,
                                                    ALWAN_CORE_T X_max,
                                                    ALWAN_CORE_T N_max) {
    return ALWAN_CORE_FNVLIT(alwan_maximum_angular_size_barten1999)(u, X_0, X_max, N_max);
}

void ALWAN_CORE_FNLIT(alwan_csf_barten1999_params_default)(ALWAN_CORE_FNLIT(alwan_csf_barten1999_params) *params) {
    if (!params) return;

    /* Default parameters for 1920x1080 display at ~3 m viewing distance.
     * Values from Barten (1999) "Contrast Sensitivity of the Human Eye and its
     * Effects on Image Quality", SPIE Press -- Table 3.2 reference observer. */
    params->sigma = ALWAN_CORE_FNVLIT(alwan_sigma_barten1999)(
        ALWAN_CORE_LITERAL(0.5) / ALWAN_CORE_LITERAL(60.0),
        ALWAN_CORE_LITERAL(0.08) / ALWAN_CORE_LITERAL(60.0),
        ALWAN_CORE_LITERAL(2.1));
    params->k = ALWAN_CORE_LITERAL(3.0);
    params->T = ALWAN_CORE_LITERAL(0.1);
    params->X_0 = ALWAN_CORE_LITERAL(60.0);
    params->Y_0 = ALWAN_CORE_LITERAL(-1.0);
    params->X_max = ALWAN_CORE_LITERAL(12.0);
    params->Y_max = ALWAN_CORE_LITERAL(-1.0);
    params->N_max = ALWAN_CORE_LITERAL(15.0);
    params->n = ALWAN_CORE_LITERAL(0.03);
    params->p = ALWAN_CORE_LITERAL(1.2274e6);
    params->E = ALWAN_CORE_FNVLIT(alwan_retinal_illuminance_barten1999)(
        ALWAN_CORE_LITERAL(20.0), ALWAN_CORE_LITERAL(2.1), ALWAN_CORE_ONE);
    params->phi_0 = ALWAN_CORE_LITERAL(3.0e-8);
    params->u_0 = ALWAN_CORE_LITERAL(7.0);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_csf_barten1999)(ALWAN_CORE_T u,
                                   ALWAN_CORE_FNLIT(alwan_csf_barten1999_params) const *params) {
    ALWAN_CORE_FNLIT(alwan_csf_barten1999_params) defaults;
    ALWAN_CORE_FNLIT(alwan_csf_barten1999_params) const *p;

    if (params) {
        p = params;
    } else {
        ALWAN_CORE_FNLIT(alwan_csf_barten1999_params_default)(&defaults);
        p = &defaults;
    }

    /* Map public struct to core struct */
    ALWAN_CORE_FNLIT(alwan_csf_barten1999_v_params) vp;
    vp.sigma = p->sigma;
    vp.k     = p->k;
    vp.T     = p->T;
    vp.X_0   = p->X_0;
    vp.Y_0   = p->Y_0;
    vp.X_max = p->X_max;
    vp.Y_max = p->Y_max;
    vp.N_max = p->N_max;
    vp.n     = p->n;
    vp.p     = p->p;
    vp.E     = p->E;
    vp.phi_0 = p->phi_0;
    vp.u_0   = p->u_0;

    return ALWAN_CORE_FNVLIT(alwan_csf_barten1999)(u, vp);
}

/* ================================================================
 * Accessibility Contrast Metrics
 * ================================================================ */

alwan_status ALWAN_CORE_FNLIT(alwan_wcag_contrast_ratio)(ALWAN_CORE_T *result, ALWAN_CORE_T Y1, ALWAN_CORE_T Y2) {
    if (!result) return ALWAN_E_INVALID;
    *result = ALWAN_CORE_FNVLIT(alwan_wcag_contrast_ratio)(Y1, Y2);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_weber_contrast)(ALWAN_CORE_T *result, ALWAN_CORE_T L_target, ALWAN_CORE_T L_bg) {
    if (!result) return ALWAN_E_INVALID;
    *result = ALWAN_CORE_FNVLIT(alwan_weber_contrast)(L_target, L_bg);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_michelson_contrast)(ALWAN_CORE_T *result, ALWAN_CORE_T L_max, ALWAN_CORE_T L_min) {
    if (!result) return ALWAN_E_INVALID;
    *result = ALWAN_CORE_FNVLIT(alwan_michelson_contrast)(L_max, L_min);
    return ALWAN_OK;
}

/* ================================================================
 * SPD-based luminance functions
 *
 * Use the per-precision LUTs via the ALWAN_CORE_FNLIT(interpolate_lut) helpers
 * defined above, so each pass reads its native float/double table.
 * ================================================================ */

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_photopic_luminance)(ALWAN_CORE_SPD const *spd, alwan_ctx *ctx) {
    (void)ctx;
    if (!spd || !spd->values || spd->count < 2) return ALWAN_CORE_LITERAL(-1.0);

    /* CIE 69:1987 / NIST SP 250-37 */
    ALWAN_CORE_T const Km = ALWAN_CORE_LITERAL(683.002);
    ALWAN_CORE_T step = ((ALWAN_CORE_T)spd->wavelength_max - (ALWAN_CORE_T)spd->wavelength_min)
                        / (ALWAN_CORE_T)(spd->count - 1);
    ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);

    for (size_t i = 0; i < spd->count; i++) {
        ALWAN_CORE_T lambda = (ALWAN_CORE_T)spd->wavelength_min + (ALWAN_CORE_T)i * step;
        ALWAN_CORE_T v = ALWAN_CORE_FNLIT(alwan_vision_interpolate_photopic_lut)(lambda);
        ALWAN_CORE_T w = (i == 0 || i == spd->count - 1)
                         ? ALWAN_CORE_LITERAL(0.5) : ALWAN_CORE_LITERAL(1.0);
        sum += w * (ALWAN_CORE_T)spd->values[i] * v;
    }

    return Km * sum * step;
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_scotopic_luminance)(ALWAN_CORE_SPD const *spd, alwan_ctx *ctx) {
    (void)ctx;
    if (!spd || !spd->values || spd->count < 2) return ALWAN_CORE_LITERAL(-1.0);

    /* CIE 69:1987 */
    ALWAN_CORE_T const Kmp = ALWAN_CORE_LITERAL(1699.998);
    ALWAN_CORE_T step = ((ALWAN_CORE_T)spd->wavelength_max - (ALWAN_CORE_T)spd->wavelength_min)
                        / (ALWAN_CORE_T)(spd->count - 1);
    ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);

    for (size_t i = 0; i < spd->count; i++) {
        ALWAN_CORE_T lambda = (ALWAN_CORE_T)spd->wavelength_min + (ALWAN_CORE_T)i * step;
        ALWAN_CORE_T vp = ALWAN_CORE_FNLIT(alwan_vision_interpolate_scotopic_lut)(lambda);
        ALWAN_CORE_T w = (i == 0 || i == spd->count - 1)
                         ? ALWAN_CORE_LITERAL(0.5) : ALWAN_CORE_LITERAL(1.0);
        sum += w * (ALWAN_CORE_T)spd->values[i] * vp;
    }

    return Kmp * sum * step;
}

/* ================================================================
 * Luminous flux, efficiency and efficacy
 *
 * colour-science's luminous_flux / luminous_efficiency / luminous_efficacy:
 * trapezoids over the SPD's own samples, V 0 outside its table. Summed in double
 * in both precisions.
 * ================================================================ */

static double ALWAN_CORE_FNLIT(alwan__lef_or_zero)(double wl, alwan_vision_type vision) {
    int const scotopic = (int)vision == (int)ALWAN_VISION_SCOTOPIC;
    ALWAN_CORE_T const *d = scotopic ? ALWAN_CORE_FNLIT(SCOTOPIC_VP_DATA) : ALWAN_CORE_FNLIT(PHOTOPIC_V_DATA);
    int const n = scotopic ? SCOTOPIC_VP_COUNT : PHOTOPIC_V_COUNT;
    if (wl < (double)d[0] || wl > (double)d[(n - 1) * 2]) return 0.0;
    return (double)ALWAN_CORE_FNLIT(interpolate_lut)(d, n, (ALWAN_CORE_T)wl);
}

static alwan_status ALWAN_CORE_FNLIT(alwan__luminous_integrals)(double *vs, double *s, ALWAN_CORE_SPD const *spd,
                                                               alwan_vision_type vision) {
    double step, w0, v0;
    size_t i;
    if (!spd || !spd->values || spd->count < 2) return ALWAN_E_INVALID;
    if ((int)vision != (int)ALWAN_VISION_PHOTOPIC && (int)vision != (int)ALWAN_VISION_SCOTOPIC) return ALWAN_E_INVALID;
    step = ((double)spd->wavelength_max - (double)spd->wavelength_min) / (double)(spd->count - 1);
    *vs = 0.0;
    *s = 0.0;
    w0 = (double)spd->wavelength_min;
    v0 = ALWAN_CORE_FNLIT(alwan__lef_or_zero)(w0, vision) * (double)spd->values[0];
    for (i = 1; i < spd->count; i++) {
        double const w1 = (double)spd->wavelength_min + (double)i * step;
        double const v1 = ALWAN_CORE_FNLIT(alwan__lef_or_zero)(w1, vision) * (double)spd->values[i];
        *vs += (w1 - w0) * (v1 + v0) / 2.0;
        *s += (w1 - w0) * ((double)spd->values[i] + (double)spd->values[i - 1]) / 2.0;
        w0 = w1;
        v0 = v1;
    }
    return ALWAN_OK;
}

static double ALWAN_CORE_FNLIT(alwan__luminous_km)(ALWAN_CORE_T K_m, alwan_vision_type vision) {
    return K_m != ALWAN_CORE_LITERAL(0.0) ? (double)K_m : ((int)vision == (int)ALWAN_VISION_SCOTOPIC ? 1700.0 : 683.0);
}

alwan_status ALWAN_CORE_FNLIT(alwan_spd_luminous_flux)(ALWAN_CORE_T *flux_out, ALWAN_CORE_SPD const *spd,
                                                       alwan_vision_type vision, ALWAN_CORE_T K_m) {
    double vs, s;
    if (!flux_out || K_m < ALWAN_CORE_LITERAL(0.0)) return ALWAN_E_INVALID;
    if (ALWAN_CORE_FNLIT(alwan__luminous_integrals)(&vs, &s, spd, vision) != ALWAN_OK) return ALWAN_E_INVALID;
    *flux_out = (ALWAN_CORE_T)(ALWAN_CORE_FNLIT(alwan__luminous_km)(K_m, vision) * vs);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_spd_luminous_efficiency)(ALWAN_CORE_T *efficiency_out, ALWAN_CORE_SPD const *spd,
                                                             alwan_vision_type vision) {
    double vs, s;
    if (!efficiency_out) return ALWAN_E_INVALID;
    if (ALWAN_CORE_FNLIT(alwan__luminous_integrals)(&vs, &s, spd, vision) != ALWAN_OK || s == 0.0) return ALWAN_E_INVALID;
    *efficiency_out = (ALWAN_CORE_T)(vs / s);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_spd_luminous_efficacy)(ALWAN_CORE_T *efficacy_out, ALWAN_CORE_SPD const *spd,
                                                           alwan_vision_type vision, ALWAN_CORE_T K_m) {
    double vs, s;
    if (!efficacy_out || K_m < ALWAN_CORE_LITERAL(0.0)) return ALWAN_E_INVALID;
    if (ALWAN_CORE_FNLIT(alwan__luminous_integrals)(&vs, &s, spd, vision) != ALWAN_OK || s == 0.0) return ALWAN_E_INVALID;
    *efficacy_out = (ALWAN_CORE_T)(ALWAN_CORE_FNLIT(alwan__luminous_km)(K_m, vision) * (vs / s));
    return ALWAN_OK;
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_mesopic_luminance)(ALWAN_CORE_SPD const *spd, ALWAN_CORE_T adaptation_level,
                                                       alwan_ctx *ctx) {
    if (!spd || !spd->values || spd->count < 2) return ALWAN_CORE_LITERAL(-1.0);
    if (adaptation_level <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(-1.0);

    /* CIE 191:2010 mesopic adaptation coefficient m */
    ALWAN_CORE_T m;
    if (adaptation_level >= ALWAN_CORE_LITERAL(5.0)) {
        m = ALWAN_CORE_LITERAL(1.0);
    } else {
        ALWAN_CORE_T const c = ALWAN_CORE_LITERAL(5.84738229067);
        m = c * adaptation_level / (ALWAN_CORE_LITERAL(1.0) + c * adaptation_level);
    }

    ALWAN_CORE_T L_p = ALWAN_CORE_FNLIT(alwan_photopic_luminance)(spd, ctx);
    if (L_p < ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(-1.0);

    ALWAN_CORE_T L_s = ALWAN_CORE_FNLIT(alwan_scotopic_luminance)(spd, ctx);
    if (L_s < ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(-1.0);

    ALWAN_CORE_T const Km_ratio = ALWAN_CORE_LITERAL(683.002) / ALWAN_CORE_LITERAL(1699.998);
    return m * L_p + (ALWAN_CORE_LITERAL(1.0) - m) * Km_ratio * L_s;
}

/* ================================================================
 * Helmholtz-Kohlrausch effect (Nayatani 1997)
 * ================================================================ */

/* The only thing the two variants disagree about. */
static ALWAN_CORE_T ALWAN_CORE_FN(hke_method_coefficient)(alwan_hke_nayatani1997_method method) {
    return (method == ALWAN_HKE_NAYATANI1997_VAC)
        ? ALWAN_CORE_LITERAL(-0.134)
        : ALWAN_CORE_LITERAL(-0.866);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_hke_object_nayatani1997)(
        ALWAN_CORE_VEC2 const *uv,
        ALWAN_CORE_VEC2 const *uv_c,
        ALWAN_CORE_T L_a,
        alwan_hke_nayatani1997_method method) {
    if (!uv || !uv_c) return ALWAN_CORE_LITERAL(-1.0);
    return ALWAN_CORE_FNVLIT(alwan_hke_object_nayatani1997)(
        uv->v[0], uv->v[1], uv_c->v[0], uv_c->v[1], L_a,
        ALWAN_CORE_FN(hke_method_coefficient)(method));
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_hke_luminous_nayatani1997)(
        ALWAN_CORE_VEC2 const *uv,
        ALWAN_CORE_VEC2 const *uv_c,
        ALWAN_CORE_T L_a,
        alwan_hke_nayatani1997_method method) {
    if (!uv || !uv_c) return ALWAN_CORE_LITERAL(-1.0);
    return ALWAN_CORE_FNVLIT(alwan_hke_luminous_nayatani1997)(
        uv->v[0], uv->v[1], uv_c->v[0], uv_c->v[1], L_a,
        ALWAN_CORE_FN(hke_method_coefficient)(method));
}
