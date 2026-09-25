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
 * The seven luminous efficiency functions as datasets
 *
 * CIE 1924 and CIE 1951 are the interleaved statics above, the same numbers
 * alwan_luminous_efficiency interpolates. The other five are registry tables,
 * bare rows on their own grids, 1 nm but for Judd 1951 at 10 nm; the dispatch
 * below is generated from the CSV file names by alwan_dev's splice, so the grid
 * each table sits on is read off the file, not assumed here.
 * ================================================================ */

/* Defined with the flux integrals further down; the flux under a canonical LEF
 * reads it so both names give one number. */
static double ALWAN_CORE_FNLIT(alwan__lef_or_zero)(double wl, alwan_vision_type vision);

/* A registry LEF's table, sample count, first wavelength and step. 1 for a
 * registry value, with table NULL when compiled out; 0 for anything else. */
static int ALWAN_CORE_FN(lef_registry_table)(alwan_lef lef, ALWAN_CORE_T const **table, size_t *count,
                                             ALWAN_CORE_T *wl_min, ALWAN_CORE_T *step) {
    *table = NULL;
    *count = 0;
    *wl_min = ALWAN_CORE_LITERAL(0.0);
    *step = ALWAN_CORE_LITERAL(1.0);
    switch (lef) {
        case ALWAN_LEF_JUDD_1951_PHOTOPIC:
#if ALWAN_TABLE_LEF_JUDD_1951_PHOTOPIC
            *table = ALWAN_CORE_FNLIT(alwan_table_lef_judd_1951_photopic);
            *count = ALWAN_TABLE_LEF_370_770_10NM_SIZE;
            *wl_min = ALWAN_CORE_LITERAL(370.0);
            *step = ALWAN_CORE_LITERAL(10.0);
#endif
            return 1;
        case ALWAN_LEF_JUDD_VOS_1978_PHOTOPIC:
#if ALWAN_TABLE_LEF_JUDD_VOS_1978_PHOTOPIC
            *table = ALWAN_CORE_FNLIT(alwan_table_lef_judd_vos_1978_photopic);
            *count = ALWAN_TABLE_LEF_380_780_1NM_SIZE;
            *wl_min = ALWAN_CORE_LITERAL(380.0);
            *step = ALWAN_CORE_LITERAL(1.0);
#endif
            return 1;
        case ALWAN_LEF_CIE_1964_PHOTOPIC_10DEG:
#if ALWAN_TABLE_LEF_CIE_1964_PHOTOPIC_10DEG
            *table = ALWAN_CORE_FNLIT(alwan_table_lef_cie_1964_photopic_10deg);
            *count = ALWAN_TABLE_LEF_360_830_1NM_SIZE;
            *wl_min = ALWAN_CORE_LITERAL(360.0);
            *step = ALWAN_CORE_LITERAL(1.0);
#endif
            return 1;
        case ALWAN_LEF_CIE_2008_PHOTOPIC_2DEG:
#if ALWAN_TABLE_LEF_CIE_2008_PHOTOPIC_2DEG
            *table = ALWAN_CORE_FNLIT(alwan_table_lef_cie_2008_photopic_2deg);
            *count = ALWAN_TABLE_LEF_390_830_1NM_SIZE;
            *wl_min = ALWAN_CORE_LITERAL(390.0);
            *step = ALWAN_CORE_LITERAL(1.0);
#endif
            return 1;
        case ALWAN_LEF_CIE_2008_PHOTOPIC_10DEG:
#if ALWAN_TABLE_LEF_CIE_2008_PHOTOPIC_10DEG
            *table = ALWAN_CORE_FNLIT(alwan_table_lef_cie_2008_photopic_10deg);
            *count = ALWAN_TABLE_LEF_390_830_1NM_SIZE;
            *wl_min = ALWAN_CORE_LITERAL(390.0);
            *step = ALWAN_CORE_LITERAL(1.0);
#endif
            return 1;
        default:
            return 0;
    }
}

alwan_status ALWAN_CORE_FNLIT(alwan_spd_lef)(ALWAN_CORE_SPD *out, alwan_lef lef, alwan_ctx *ctx) {
    ALWAN_CORE_T const *table;
    size_t count, i;
    ALWAN_CORE_T wl_min, step;
    alwan_status st;
    if (!out) {
        return ALWAN_E_INVALID;
    }
    if ((int)lef == (int)ALWAN_LEF_CIE_1924_PHOTOPIC || (int)lef == (int)ALWAN_LEF_CIE_1951_SCOTOPIC) {
        /* The interleaved statics: {wavelength, value} pairs. */
        int const scotopic = (int)lef == (int)ALWAN_LEF_CIE_1951_SCOTOPIC;
        ALWAN_CORE_T const *d = scotopic ? ALWAN_CORE_FNLIT(SCOTOPIC_VP_DATA) : ALWAN_CORE_FNLIT(PHOTOPIC_V_DATA);
        size_t const n = scotopic ? (size_t)SCOTOPIC_VP_COUNT : (size_t)PHOTOPIC_V_COUNT;
        st = ALWAN_CORE_FNLIT(alwan_spd_create)(out, d[0], d[(n - 1) * 2], n, ctx);
        if (st != ALWAN_OK) {
            return st;
        }
        for (i = 0; i < n; i++) {
            out->values[i] = d[i * 2 + 1];
        }
        return ALWAN_OK;
    }
    if (!ALWAN_CORE_FN(lef_registry_table)(lef, &table, &count, &wl_min, &step)) {
        return ALWAN_E_INVALID;
    }
    if (!table) {
        return ALWAN_E_NODATA;
    }
    st = ALWAN_CORE_FNLIT(alwan_spd_create)(out, wl_min, wl_min + step * (ALWAN_CORE_T)(count - 1), count, ctx);
    if (st != ALWAN_OK) {
        return st;
    }
    for (i = 0; i < count; i++) {
        out->values[i] = ALWAN_CORE_FNVLIT(alwan_table1d_row)(table, (int)count, (int)i);
    }
    return ALWAN_OK;
}

/* A registry LEF at a wavelength, linear between its nodes and 0 outside its data,
 * the counterpart of alwan__lef_or_zero for a bare row on a grid of any step. */
static double ALWAN_CORE_FN(lef_row_or_zero)(ALWAN_CORE_T const *table, size_t count, ALWAN_CORE_T wl_min,
                                             ALWAN_CORE_T step, double wl) {
    ALWAN_CORE_TABLE_CELL c;
    ALWAN_CORE_T v0, v1;
    if (wl < (double)wl_min || wl > (double)wl_min + (double)step * (double)(count - 1)) {
        return 0.0;
    }
    c = ALWAN_CORE_FNVLIT(alwan_table_cell_unit)(((ALWAN_CORE_T)wl - wl_min) / step, (int)count);
    v0 = ALWAN_CORE_FNVLIT(alwan_table1d_row)(table, (int)count, c.i0);
    v1 = ALWAN_CORE_FNVLIT(alwan_table1d_row)(table, (int)count, c.i1);
    return (double)(v0 + c.frac * (v1 - v0));
}

alwan_status ALWAN_CORE_FNLIT(alwan_spd_luminous_flux_lef)(ALWAN_CORE_T *flux_out, ALWAN_CORE_SPD const *spd,
                                                           alwan_lef lef, ALWAN_CORE_T K_m, alwan_ctx *ctx) {
    ALWAN_CORE_T const *table = NULL;
    size_t count = 0, i;
    ALWAN_CORE_T wl_min = ALWAN_CORE_LITERAL(0.0), step = ALWAN_CORE_LITERAL(1.0);
    int const canonical = (int)lef == (int)ALWAN_LEF_CIE_1924_PHOTOPIC || (int)lef == (int)ALWAN_LEF_CIE_1951_SCOTOPIC;
    int const scotopic = (int)lef == (int)ALWAN_LEF_CIE_1951_SCOTOPIC;
    double spd_step, w0, v0, vs, km;
    (void)ctx;
    if (!flux_out || !spd || !spd->values || spd->count < 2 || K_m < ALWAN_CORE_LITERAL(0.0)) {
        return ALWAN_E_INVALID;
    }
    if (!canonical) {
        if (!ALWAN_CORE_FN(lef_registry_table)(lef, &table, &count, &wl_min, &step)) {
            return ALWAN_E_INVALID;
        }
        if (!table) {
            return ALWAN_E_NODATA;
        }
    }
    /* The same trapezoid as alwan__luminous_integrals, summed in double, so the two
     * canonical functions give one number by either name. */
    spd_step = ((double)spd->wavelength_max - (double)spd->wavelength_min) / (double)(spd->count - 1);
    w0 = (double)spd->wavelength_min;
    v0 = (canonical ? ALWAN_CORE_FNLIT(alwan__lef_or_zero)(w0, scotopic ? ALWAN_VISION_SCOTOPIC : ALWAN_VISION_PHOTOPIC)
                    : ALWAN_CORE_FN(lef_row_or_zero)(table, count, wl_min, step, w0)) * (double)spd->values[0];
    vs = 0.0;
    for (i = 1; i < spd->count; i++) {
        double const w1 = (double)spd->wavelength_min + (double)i * spd_step;
        double const v1 = (canonical ? ALWAN_CORE_FNLIT(alwan__lef_or_zero)(w1, scotopic ? ALWAN_VISION_SCOTOPIC : ALWAN_VISION_PHOTOPIC)
                                     : ALWAN_CORE_FN(lef_row_or_zero)(table, count, wl_min, step, w1)) * (double)spd->values[i];
        vs += (w1 - w0) * (v1 + v0) / 2.0;
        w0 = w1;
        v0 = v1;
    }
    km = K_m != ALWAN_CORE_LITERAL(0.0) ? (double)K_m : (scotopic ? 1700.0 : 683.0);
    *flux_out = (ALWAN_CORE_T)(km * vs);
    return ALWAN_OK;
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
        ALWAN_CORE_T v = (lambda < ALWAN_CORE_FNLIT(PHOTOPIC_V_DATA)[0] || lambda > ALWAN_CORE_FNLIT(PHOTOPIC_V_DATA)[(PHOTOPIC_V_COUNT - 1) * 2])
                         ? ALWAN_CORE_LITERAL(0.0) : ALWAN_CORE_FNLIT(alwan_vision_interpolate_photopic_lut)(lambda);
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
        ALWAN_CORE_T vp = (lambda < ALWAN_CORE_FNLIT(SCOTOPIC_VP_DATA)[0] || lambda > ALWAN_CORE_FNLIT(SCOTOPIC_VP_DATA)[(SCOTOPIC_VP_COUNT - 1) * 2])
                         ? ALWAN_CORE_LITERAL(0.0) : ALWAN_CORE_FNLIT(alwan_vision_interpolate_scotopic_lut)(lambda);
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

/* CIE 191:2010 section 5 (the iteration CIE TN 007:2017 works through): from the photopic
 * and scotopic adaptation luminances, L_mes,n = (m L_p + (1 - m) L_s V'(lambda0)) /
 * (m + (1 - m) V'(lambda0)) with V'(lambda0) = 683 / 1699, m_n = 0.767 + 0.3334 log10(L_mes,n)
 * clipped to [0, 1], starting from m 0.5, until m stops changing. Worked in double. */
static void ALWAN_CORE_FNLIT(alwan__mesopic_iterate)(double *L_mes, double *m_out, double L_p, double S_P) {
    double const V0 = 683.0 / 1699.0;
    double const L_s = L_p * S_P;
    double m = 0.5;
    double L = (m * L_p + (1.0 - m) * L_s * V0) / (m + (1.0 - m) * V0);
    int i;
    for (i = 0; i < 200; i++) {
        double mn = L > 0.0 ? 0.767 + 0.3334 * log10(L) : 0.0;
        mn = mn < 0.0 ? 0.0 : (mn > 1.0 ? 1.0 : mn);
        L = (mn * L_p + (1.0 - mn) * L_s * V0) / (mn + (1.0 - mn) * V0);
        if (fabs(mn - m) <= 1e-15) { m = mn; break; }
        m = mn;
    }
    *L_mes = L;
    *m_out = m;
}

alwan_status ALWAN_CORE_FNLIT(alwan_mesopic_adaptation)(ALWAN_CORE_T *L_mes, ALWAN_CORE_T *m,
                                                        ALWAN_CORE_T L_p, ALWAN_CORE_T S_P) {
    double l, mm;
    if (!L_mes || !m) return ALWAN_E_INVALID;
    if (!(L_p > ALWAN_CORE_LITERAL(0.0)) || !(S_P >= ALWAN_CORE_LITERAL(0.0))) return ALWAN_E_INVALID;
    ALWAN_CORE_FNLIT(alwan__mesopic_iterate)(&l, &mm, (double)L_p, (double)S_P);
    *L_mes = (ALWAN_CORE_T)l;
    *m = (ALWAN_CORE_T)mm;
    return ALWAN_OK;
}

/* CIE 191:2010 mesopic luminance of a stimulus. The adaptation field is taken as lit by the
 * same source: photopic adaptation luminance adaptation_level, scotopic that times the SPD's
 * S/P ratio (CIE TN 007:2017's reading). With P and S the trapezoids of the SPD against V and
 * V' (0 outside their data), L_mes = 683 (m P + (1 - m) S) / (m + (1 - m) V'(lambda0)). */
ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_mesopic_luminance)(ALWAN_CORE_SPD const *spd, ALWAN_CORE_T adaptation_level,
                                                       alwan_ctx *ctx) {
    double P, S, total, L_a, m;
    double const V0 = 683.0 / 1699.0;
    (void)ctx;
    if (!(adaptation_level > ALWAN_CORE_LITERAL(0.0))) return ALWAN_CORE_LITERAL(-1.0);
    if (ALWAN_CORE_FNLIT(alwan__luminous_integrals)(&P, &total, spd, ALWAN_VISION_PHOTOPIC) != ALWAN_OK) return ALWAN_CORE_LITERAL(-1.0);
    if (ALWAN_CORE_FNLIT(alwan__luminous_integrals)(&S, &total, spd, ALWAN_VISION_SCOTOPIC) != ALWAN_OK) return ALWAN_CORE_LITERAL(-1.0);
    if (P == 0.0 && S == 0.0) return ALWAN_CORE_LITERAL(0.0);
    if (!(P > 0.0) || S < 0.0) return ALWAN_CORE_LITERAL(-1.0);
    ALWAN_CORE_FNLIT(alwan__mesopic_iterate)(&L_a, &m, (double)adaptation_level, 1699.0 * S / (683.0 * P));
    return (ALWAN_CORE_T)(683.0 * (m * P + (1.0 - m) * S) / (m + (1.0 - m) * V0));
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

/* ================================================================
 * Photometer V(lambda) mismatch: f1' and the spectral mismatch correction factor
 *
 * ISO/CIE 19476:2014 (formerly CIE S 023), as luxpy's f1prime and
 * get_spectral_mismatch_correction_factors compute it: sums over one grid, V(lambda) the
 * CIE 1924 table (0 outside it), every other curve read on that grid by linear
 * interpolation and 0 outside its own range. The calibration source defaults to CIE
 * illuminant A by its defining formula, which alwan evaluates rather than reads: the
 * tabulated A is the formula to six figures, and alwan_spd_illuminant holds the 780 nm
 * value flat above 780, where a detector's response still counts. Summed in double.
 * ================================================================ */

static double ALWAN_CORE_FNLIT(alwan__spd_at)(ALWAN_CORE_SPD const *s, double wl) {
    double const lo = (double)s->wavelength_min, hi = (double)s->wavelength_max;
    double t, step;
    size_t i;
    if (wl < lo || wl > hi) return 0.0;
    step = (hi - lo) / (double)(s->count - 1);
    t = (wl - lo) / step;
    i = (size_t)t;
    if (i >= s->count - 1) return (double)s->values[s->count - 1];
    t -= (double)i;
    return (double)s->values[i] + t * ((double)s->values[i + 1] - (double)s->values[i]);
}

/* CIE illuminant A, CIE 15:2018 equation 3.1, 100 at 560 nm, c2 = 1.435e7 nm K. */
static double ALWAN_CORE_FNLIT(alwan__illuminant_a_formula)(double wl) {
    double const c2 = 1.435e7;
    return 100.0 * pow(560.0 / wl, 5.0) * (exp(c2 / (2848.0 * 560.0)) - 1.0) / (exp(c2 / (2848.0 * wl)) - 1.0);
}

static int ALWAN_CORE_FNLIT(alwan__spd_usable)(ALWAN_CORE_SPD const *s) {
    return s && s->values && s->count >= 2 && s->wavelength_max > s->wavelength_min;
}

static double ALWAN_CORE_FNLIT(alwan__calibration_at)(ALWAN_CORE_SPD const *c, double wl) {
    return c ? ALWAN_CORE_FNLIT(alwan__spd_at)(c, wl) : ALWAN_CORE_FNLIT(alwan__illuminant_a_formula)(wl);
}

alwan_status ALWAN_CORE_FNLIT(alwan_photometer_f1_prime)(ALWAN_CORE_T *f1_prime, ALWAN_CORE_SPD const *detector,
                                                         ALWAN_CORE_SPD const *calibration) {
    double cv = 0.0, cs = 0.0, v_sum = 0.0, dev = 0.0, step, k;
    size_t i;
    if (!f1_prime || !ALWAN_CORE_FNLIT(alwan__spd_usable)(detector)) return ALWAN_E_INVALID;
    if (calibration && !ALWAN_CORE_FNLIT(alwan__spd_usable)(calibration)) return ALWAN_E_INVALID;
    step = ((double)detector->wavelength_max - (double)detector->wavelength_min) / (double)(detector->count - 1);
    for (i = 0; i < detector->count; i++) {
        double const wl = (double)detector->wavelength_min + (double)i * step;
        double const c = ALWAN_CORE_FNLIT(alwan__calibration_at)(calibration, wl);
        double const v = ALWAN_CORE_FNLIT(alwan__lef_or_zero)(wl, ALWAN_VISION_PHOTOPIC);
        cv += c * v;
        cs += c * (double)detector->values[i];
        v_sum += v;
    }
    if (cs == 0.0 || v_sum == 0.0) return ALWAN_E_RANGE;
    k = cv / cs;
    for (i = 0; i < detector->count; i++) {
        double const wl = (double)detector->wavelength_min + (double)i * step;
        double const v = ALWAN_CORE_FNLIT(alwan__lef_or_zero)(wl, ALWAN_VISION_PHOTOPIC);
        dev += fabs(k * (double)detector->values[i] - v);
    }
    *f1_prime = (ALWAN_CORE_T)(dev / v_sum);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_photometer_mismatch_correction)(ALWAN_CORE_T *factor, ALWAN_CORE_SPD const *detector,
                                                                    ALWAN_CORE_SPD const *test_source,
                                                                    ALWAN_CORE_SPD const *calibration) {
    double tv = 0.0, ts = 0.0, cv = 0.0, cs = 0.0, step;
    size_t i;
    if (!factor || !ALWAN_CORE_FNLIT(alwan__spd_usable)(detector) || !ALWAN_CORE_FNLIT(alwan__spd_usable)(test_source)) {
        return ALWAN_E_INVALID;
    }
    if (calibration && !ALWAN_CORE_FNLIT(alwan__spd_usable)(calibration)) return ALWAN_E_INVALID;
    step = ((double)test_source->wavelength_max - (double)test_source->wavelength_min) / (double)(test_source->count - 1);
    for (i = 0; i < test_source->count; i++) {
        double const wl = (double)test_source->wavelength_min + (double)i * step;
        double const t = (double)test_source->values[i];
        double const c = ALWAN_CORE_FNLIT(alwan__calibration_at)(calibration, wl);
        double const v = ALWAN_CORE_FNLIT(alwan__lef_or_zero)(wl, ALWAN_VISION_PHOTOPIC);
        double const s = ALWAN_CORE_FNLIT(alwan__spd_at)(detector, wl);
        tv += t * v;
        ts += t * s;
        cv += c * v;
        cs += c * s;
    }
    if (ts == 0.0 || cv == 0.0) return ALWAN_E_RANGE;
    *factor = (ALWAN_CORE_T)((tv * cs) / (ts * cv));
    return ALWAN_OK;
}
