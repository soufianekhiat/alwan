/* No include guard -- included twice (f32 and f64 passes).
 *
 * Native dual-precision RGB->spectrum upsampling. The basis spectra and
 * Jakob2019 coefficient LUTs are dual-declared (f32 + f64 twins from the
 * same CSV) in alwan_spectrum_upsample.c and read here via
 * ALWAN_CORE_FNLIT(NAME), so the f32 pass reads native float data and
 * every operation runs at the selected precision -- a genuine f32 path
 * rather than a widen-to-f64 facade.
 */

static inline ALWAN_CORE_T ALWAN_CORE_FN(spu_clamp)(ALWAN_CORE_T x, ALWAN_CORE_T lo, ALWAN_CORE_T hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* ----------------------------------------------------------------
 * Smits1999: RGB to Spectrum
 * ---------------------------------------------------------------- */
alwan_status ALWAN_CORE_FNLIT(alwan_rgb_to_spectrum_smits1999)(
        ALWAN_CORE_SPD *out_spd, ALWAN_CORE_RGB const *rgb, alwan_ctx *ctx) {
    if (!rgb || !out_spd) return ALWAN_E_INVALID;

    int status = ALWAN_CORE_FNLIT(alwan_spd_create)(out_spd,
        (ALWAN_CORE_T)SMITS1999_WAVELENGTH_MIN, (ALWAN_CORE_T)SMITS1999_WAVELENGTH_MAX,
        SMITS1999_WAVELENGTH_COUNT, ctx);
    if (status != ALWAN_OK) return status;

    ALWAN_CORE_T r = ALWAN_CORE_FN(spu_clamp)(rgb->r, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T g = ALWAN_CORE_FN(spu_clamp)(rgb->g, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T b = ALWAN_CORE_FN(spu_clamp)(rgb->b, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));

    if (r <= g && r <= b) {
        for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
            out_spd->values[i] = ALWAN_CORE_FNLIT(smits1999_white)[i] * r;
        if (g <= b) {
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNLIT(smits1999_cyan)[i] * (g - r);
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNLIT(smits1999_blue)[i] * (b - g);
        } else {
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNLIT(smits1999_cyan)[i] * (b - r);
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNLIT(smits1999_green)[i] * (g - b);
        }
    } else if (g <= r && g <= b) {
        for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
            out_spd->values[i] = ALWAN_CORE_FNLIT(smits1999_white)[i] * g;
        if (r <= b) {
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNLIT(smits1999_magenta)[i] * (r - g);
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNLIT(smits1999_blue)[i] * (b - r);
        } else {
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNLIT(smits1999_magenta)[i] * (b - g);
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNLIT(smits1999_red)[i] * (r - b);
        }
    } else {
        for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
            out_spd->values[i] = ALWAN_CORE_FNLIT(smits1999_white)[i] * b;
        if (r <= g) {
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNLIT(smits1999_yellow)[i] * (r - b);
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNLIT(smits1999_green)[i] * (g - r);
        } else {
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNLIT(smits1999_yellow)[i] * (g - b);
            for (size_t i = 0; i < SMITS1999_WAVELENGTH_COUNT; i++)
                out_spd->values[i] += ALWAN_CORE_FNLIT(smits1999_red)[i] * (r - g);
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Mallett2019: RGB to Spectrum (linear combination of basis functions)
 * ---------------------------------------------------------------- */
alwan_status ALWAN_CORE_FNLIT(alwan_rgb_to_spectrum_mallett2019)(
        ALWAN_CORE_SPD *out_spd, ALWAN_CORE_RGB const *rgb, alwan_ctx *ctx) {
    if (!rgb || !out_spd) return ALWAN_E_INVALID;

    int status = ALWAN_CORE_FNLIT(alwan_spd_create)(out_spd,
        (ALWAN_CORE_T)MALLETT2019_WAVELENGTH_MIN, (ALWAN_CORE_T)MALLETT2019_WAVELENGTH_MAX,
        MALLETT2019_WAVELENGTH_COUNT, ctx);
    if (status != ALWAN_OK) return status;

    ALWAN_CORE_T r = rgb->r, g = rgb->g, b = rgb->b;

    for (size_t i = 0; i < MALLETT2019_WAVELENGTH_COUNT; i++) {
        out_spd->values[i] = r * ALWAN_CORE_FNLIT(mallett2019_red)[i] +
                             g * ALWAN_CORE_FNLIT(mallett2019_green)[i] +
                             b * ALWAN_CORE_FNLIT(mallett2019_blue)[i];
    }
    for (size_t i = 0; i < MALLETT2019_WAVELENGTH_COUNT; i++) {
        if (out_spd->values[i] < ALWAN_CORE_LITERAL(0.0))
            out_spd->values[i] = ALWAN_CORE_LITERAL(0.0);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Jakob2019: trilinear LUT sample + polynomial evaluation
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(jakob2019_lut_sample)(
        ALWAN_CORE_T const *lut_c0, ALWAN_CORE_T const *lut_c1, ALWAN_CORE_T const *lut_c2,
        ALWAN_CORE_T r, ALWAN_CORE_T g, ALWAN_CORE_T b,
        ALWAN_CORE_T *out_c0, ALWAN_CORE_T *out_c1, ALWAN_CORE_T *out_c2) {
    r = ALWAN_CORE_FN(spu_clamp)(r, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
    g = ALWAN_CORE_FN(spu_clamp)(g, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
    b = ALWAN_CORE_FN(spu_clamp)(b, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));

    ALWAN_CORE_T const res_f = (ALWAN_CORE_T)(JAKOB2019_LUT_RES - 1);
    ALWAN_CORE_T const rf = r * res_f;
    ALWAN_CORE_T const gf = g * res_f;
    ALWAN_CORE_T const bf = b * res_f;

    int const r0 = (int)rf;
    int const g0 = (int)gf;
    int const b0 = (int)bf;

    int const r1 = (r0 + 1 < JAKOB2019_LUT_RES) ? (r0 + 1) : r0;
    int const g1 = (g0 + 1 < JAKOB2019_LUT_RES) ? (g0 + 1) : g0;
    int const b1 = (b0 + 1 < JAKOB2019_LUT_RES) ? (b0 + 1) : b0;

    ALWAN_CORE_T const fr = rf - (ALWAN_CORE_T)r0;
    ALWAN_CORE_T const fg = gf - (ALWAN_CORE_T)g0;
    ALWAN_CORE_T const fb = bf - (ALWAN_CORE_T)b0;

#define LUT_INDEX(ri, gi, bi) ((ri) * JAKOB2019_LUT_RES * JAKOB2019_LUT_RES + (gi) * JAKOB2019_LUT_RES + (bi))
    int const i000 = LUT_INDEX(r0, g0, b0);
    int const i001 = LUT_INDEX(r0, g0, b1);
    int const i010 = LUT_INDEX(r0, g1, b0);
    int const i011 = LUT_INDEX(r0, g1, b1);
    int const i100 = LUT_INDEX(r1, g0, b0);
    int const i101 = LUT_INDEX(r1, g0, b1);
    int const i110 = LUT_INDEX(r1, g1, b0);
    int const i111 = LUT_INDEX(r1, g1, b1);
#undef LUT_INDEX

    ALWAN_CORE_T const one = ALWAN_CORE_LITERAL(1.0);

    ALWAN_CORE_T const c0_00 = lut_c0[i000] * (one - fb) + lut_c0[i001] * fb;
    ALWAN_CORE_T const c0_01 = lut_c0[i010] * (one - fb) + lut_c0[i011] * fb;
    ALWAN_CORE_T const c0_10 = lut_c0[i100] * (one - fb) + lut_c0[i101] * fb;
    ALWAN_CORE_T const c0_11 = lut_c0[i110] * (one - fb) + lut_c0[i111] * fb;
    ALWAN_CORE_T const c0_0 = c0_00 * (one - fg) + c0_01 * fg;
    ALWAN_CORE_T const c0_1 = c0_10 * (one - fg) + c0_11 * fg;
    *out_c0 = c0_0 * (one - fr) + c0_1 * fr;

    ALWAN_CORE_T const c1_00 = lut_c1[i000] * (one - fb) + lut_c1[i001] * fb;
    ALWAN_CORE_T const c1_01 = lut_c1[i010] * (one - fb) + lut_c1[i011] * fb;
    ALWAN_CORE_T const c1_10 = lut_c1[i100] * (one - fb) + lut_c1[i101] * fb;
    ALWAN_CORE_T const c1_11 = lut_c1[i110] * (one - fb) + lut_c1[i111] * fb;
    ALWAN_CORE_T const c1_0 = c1_00 * (one - fg) + c1_01 * fg;
    ALWAN_CORE_T const c1_1 = c1_10 * (one - fg) + c1_11 * fg;
    *out_c1 = c1_0 * (one - fr) + c1_1 * fr;

    ALWAN_CORE_T const c2_00 = lut_c2[i000] * (one - fb) + lut_c2[i001] * fb;
    ALWAN_CORE_T const c2_01 = lut_c2[i010] * (one - fb) + lut_c2[i011] * fb;
    ALWAN_CORE_T const c2_10 = lut_c2[i100] * (one - fb) + lut_c2[i101] * fb;
    ALWAN_CORE_T const c2_11 = lut_c2[i110] * (one - fb) + lut_c2[i111] * fb;
    ALWAN_CORE_T const c2_0 = c2_00 * (one - fg) + c2_01 * fg;
    ALWAN_CORE_T const c2_1 = c2_10 * (one - fg) + c2_11 * fg;
    *out_c2 = c2_0 * (one - fr) + c2_1 * fr;
}

static inline ALWAN_CORE_T ALWAN_CORE_FN(jakob2019_eval_poly)(
        ALWAN_CORE_T c0, ALWAN_CORE_T c1, ALWAN_CORE_T c2, ALWAN_CORE_T wavelength) {
    ALWAN_CORE_T const U = c0 * wavelength * wavelength + c1 * wavelength + c2;
    ALWAN_CORE_T const U_sq = U * U;
    ALWAN_CORE_T const denom = ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(1.0) + U_sq);
    ALWAN_CORE_T const R = ALWAN_CORE_LITERAL(0.5) + U / denom;
    return ALWAN_CORE_FN(spu_clamp)(R, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
}

alwan_status ALWAN_CORE_FNLIT(alwan_rgb_to_spectrum_jakob2019)(
        ALWAN_CORE_SPD *out_spd, alwan_jakob2019_gamut gamut,
        ALWAN_CORE_RGB const *rgb, alwan_ctx *ctx) {
    if (!rgb || !out_spd) return ALWAN_E_INVALID;

    ALWAN_CORE_T const *lut_c0, *lut_c1, *lut_c2;
    switch (gamut) {
        case ALWAN_JAKOB2019_SRGB:
            lut_c0 = ALWAN_CORE_FNLIT(jakob2019_srgb_lut_c0); lut_c1 = ALWAN_CORE_FNLIT(jakob2019_srgb_lut_c1); lut_c2 = ALWAN_CORE_FNLIT(jakob2019_srgb_lut_c2); break;
        case ALWAN_JAKOB2019_PROPHOTO_RGB:
            lut_c0 = ALWAN_CORE_FNLIT(jakob2019_prophoto_lut_c0); lut_c1 = ALWAN_CORE_FNLIT(jakob2019_prophoto_lut_c1); lut_c2 = ALWAN_CORE_FNLIT(jakob2019_prophoto_lut_c2); break;
        case ALWAN_JAKOB2019_ACES2065_1:
            lut_c0 = ALWAN_CORE_FNLIT(jakob2019_aces_lut_c0); lut_c1 = ALWAN_CORE_FNLIT(jakob2019_aces_lut_c1); lut_c2 = ALWAN_CORE_FNLIT(jakob2019_aces_lut_c2); break;
        case ALWAN_JAKOB2019_REC2020:
            lut_c0 = ALWAN_CORE_FNLIT(jakob2019_rec2020_lut_c0); lut_c1 = ALWAN_CORE_FNLIT(jakob2019_rec2020_lut_c1); lut_c2 = ALWAN_CORE_FNLIT(jakob2019_rec2020_lut_c2); break;
        case ALWAN_JAKOB2019_ERGB:
            lut_c0 = ALWAN_CORE_FNLIT(jakob2019_ergb_lut_c0); lut_c1 = ALWAN_CORE_FNLIT(jakob2019_ergb_lut_c1); lut_c2 = ALWAN_CORE_FNLIT(jakob2019_ergb_lut_c2); break;
        case ALWAN_JAKOB2019_XYZ:
            lut_c0 = ALWAN_CORE_FNLIT(jakob2019_xyz_lut_c0); lut_c1 = ALWAN_CORE_FNLIT(jakob2019_xyz_lut_c1); lut_c2 = ALWAN_CORE_FNLIT(jakob2019_xyz_lut_c2); break;
        default:
            return ALWAN_E_INVALID;
    }

    int status = ALWAN_CORE_FNLIT(alwan_spd_create)(out_spd,
        (ALWAN_CORE_T)JAKOB2019_WAVELENGTH_MIN, (ALWAN_CORE_T)JAKOB2019_WAVELENGTH_MAX,
        JAKOB2019_WAVELENGTH_COUNT, ctx);
    if (status != ALWAN_OK) return status;

    ALWAN_CORE_T c0, c1, c2;
    ALWAN_CORE_FN(jakob2019_lut_sample)(lut_c0, lut_c1, lut_c2, rgb->r, rgb->g, rgb->b, &c0, &c1, &c2);

    ALWAN_CORE_T const wl_min = (ALWAN_CORE_T)JAKOB2019_WAVELENGTH_MIN;
    ALWAN_CORE_T const wl_step = ((ALWAN_CORE_T)JAKOB2019_WAVELENGTH_MAX - (ALWAN_CORE_T)JAKOB2019_WAVELENGTH_MIN)
                               / (ALWAN_CORE_T)(JAKOB2019_WAVELENGTH_COUNT - 1);

    for (size_t i = 0; i < JAKOB2019_WAVELENGTH_COUNT; i++) {
        ALWAN_CORE_T const wavelength = wl_min + (ALWAN_CORE_T)i * wl_step;
        out_spd->values[i] = ALWAN_CORE_FN(jakob2019_eval_poly)(c0, c1, c2, wavelength);
    }

    return ALWAN_OK;
}
