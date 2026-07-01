/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ACES 2.0 batch output transform -- parameterized pixel loop.
 * No include guard -- included twice (f32 pass, f64 pass).
 *
 * Requires before inclusion (via alwan_api_f32/f64_setup.h):
 *   ALWAN_CORE_T, ALWAN_CORE_FNLIT, ALWAN_CORE_FNVLIT, ALWAN_CORE_LITERAL, ALWAN_CORE_LN, ...
 *
 * Requires in enclosing TU:
 *   ChromaCompressParams_f32 / ChromaCompressParams_f64
 *   init_JMhParams_f32 / init_JMhParams_f64
 *   init_TSParams_f32  / init_TSParams_f64
 *   init_ChromaCompressParams_f32 / init_ChromaCompressParams_f64
 *   init_GamutCompressParams_f32  / init_GamutCompressParams_f64
 *   init_hue_dependent_params_f32 / init_hue_dependent_params_f64
 *   get_output_config, primaries_are_ap1  (precision-independent)
 */

/* ----------------------------------------------------------------
 * Reach-M lookup for chroma compression (per-precision)
 * ---------------------------------------------------------------- */
static ALWAN_CORE_T ALWAN_CORE_FNLIT(lookup_reach_m_cc)(
        ALWAN_CORE_T h_deg,
        ALWAN_CORE_FNLIT(ChromaCompressParams) const *cp) {
    /* Wrap hue into [0,360). Guard NaN/+-Inf/huge first: the naive while-subtract
     * never terminates for +Inf or for |h| so large that h - 360 == h in floating
     * point (observed to hang when an intermediate hue overflows). After the guard
     * the wrap is bounded and bit-identical to the original for valid hues. */
    if (!(h_deg > ALWAN_CORE_LITERAL(-1.0e6) && h_deg < ALWAN_CORE_LITERAL(1.0e6))) h_deg = ALWAN_CORE_LITERAL(0.0);
    while (h_deg < ALWAN_CORE_LITERAL(0.0)) h_deg += ALWAN_CORE_LITERAL(360.0);
    while (h_deg >= ALWAN_CORE_LITERAL(360.0)) h_deg -= ALWAN_CORE_LITERAL(360.0);
    int idx0 = (int)h_deg;
    int idx1 = (idx0 + 1) % ACES2_REACH_TABLE_SIZE;
    ALWAN_CORE_T t = h_deg - (ALWAN_CORE_T)idx0;
    return cp->reach_m_table[idx0] * (ALWAN_CORE_LITERAL(1.0) - t)
         + cp->reach_m_table[idx1] * t;
}

/* ----------------------------------------------------------------
 * Chroma compression forward (per-precision)
 * ---------------------------------------------------------------- */
static ALWAN_CORE_T ALWAN_CORE_FNLIT(chroma_compress_fwd_impl)(
        ALWAN_CORE_T J, ALWAN_CORE_T M, ALWAN_CORE_T h_deg,
        ALWAN_CORE_T J_ts,
        ALWAN_CORE_FNLIT(ChromaCompressParams) const *cp) {
    if (J <= ALWAN_CORE_LITERAL(0.0) || M <= ALWAN_CORE_LITERAL(0.0))
        return ALWAN_CORE_LITERAL(0.0);

    ALWAN_CORE_T h_rad = h_deg * ALWAN_CORE_LITERAL(3.14159265358979323846)
                       / ALWAN_CORE_LITERAL(180.0);
    ALWAN_CORE_T Mnorm = ALWAN_CORE_FNVLIT(aces_chroma_compress_norm)(h_rad, cp->chroma_compress_scale);
    ALWAN_CORE_T M_cp  = M * ALWAN_CORE_POW(J_ts / J, cp->model_gamma_inv);
    M_cp = M_cp / Mnorm;

    ALWAN_CORE_T nJ    = J_ts / cp->limit_J_max;
    ALWAN_CORE_T snJ   = (ALWAN_CORE_LITERAL(1.0) - nJ > ALWAN_CORE_LITERAL(0.0))
                       ? (ALWAN_CORE_LITERAL(1.0) - nJ) : ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T reachMaxM = ALWAN_CORE_FNLIT(lookup_reach_m_cc)(h_deg, cp);
    ALWAN_CORE_T limit = ALWAN_CORE_POW(nJ, cp->model_gamma_inv) * reachMaxM / Mnorm;

    ALWAN_CORE_T sat_limit = limit - ALWAN_CORE_LITERAL(0.001);
    if (sat_limit < ALWAN_CORE_LITERAL(0.001)) sat_limit = ALWAN_CORE_LITERAL(0.001);
    M_cp = limit - ALWAN_CORE_FNVLIT(aces_toe_fwd)(
               limit - M_cp, sat_limit,
               snJ * cp->sat,
               ALWAN_CORE_SQRT(nJ * nJ + cp->sat_thr));
    M_cp = ALWAN_CORE_FNVLIT(aces_toe_fwd)(
               M_cp, limit, nJ * cp->compr, snJ);
    M_cp = M_cp * Mnorm;
    return M_cp;
}

/* ----------------------------------------------------------------
 * ACES 2.0 batch output transform implementation (per-precision)
 * ---------------------------------------------------------------- */
static int ALWAN_CORE_FNLIT(aces2_ot_map_impl)(
        ALWAN_CORE_T *out, ALWAN_CORE_T const *in,
        alwan_aces2_output output, size_t count,
        size_t in_stride, size_t out_stride) {
    if (!out || !in || count == 0) return ALWAN_E_INVALID;
    if (output < 0 || output >= ALWAN_ACES2_OUT_COUNT) return ALWAN_E_INVALID;

    aces2_output_config config;
    int status = get_output_config(output, &config);
    if (status != ALWAN_OK) return status;

    /* Convert f64 config primaries to the precision used in this pass */
    ALWAN_CORE_PRIM limit_prim;
    limit_prim.red_x    = (ALWAN_CORE_T)config.primaries.red_x;
    limit_prim.red_y    = (ALWAN_CORE_T)config.primaries.red_y;
    limit_prim.green_x  = (ALWAN_CORE_T)config.primaries.green_x;
    limit_prim.green_y  = (ALWAN_CORE_T)config.primaries.green_y;
    limit_prim.blue_x   = (ALWAN_CORE_T)config.primaries.blue_x;
    limit_prim.blue_y   = (ALWAN_CORE_T)config.primaries.blue_y;
    limit_prim.white_x  = (ALWAN_CORE_T)config.primaries.white_x;
    limit_prim.white_y  = (ALWAN_CORE_T)config.primaries.white_y;

    ALWAN_CORE_PRIM ap1;
    ALWAN_CORE_FNLIT(alwan_aces_primaries_ap1_default)(&ap1);

    ALWAN_CORE_FNLIT(aces2_JMhParams) jmh_params_storage; ALWAN_CORE_FNLIT(init_JMhParams)(&ap1, &jmh_params_storage);
    ALWAN_CORE_FNLIT(aces2_JMhParams) const *jmh_params = &jmh_params_storage;

    ALWAN_CORE_FNLIT(aces2_TSParams) ts;
    ALWAN_CORE_FNLIT(init_TSParams)((ALWAN_CORE_T)config.peak_luminance, &ts);

    ALWAN_CORE_FNLIT(ChromaCompressParams) chroma_params;
    ALWAN_CORE_FNLIT(init_ChromaCompressParams)(
        (ALWAN_CORE_T)config.peak_luminance, jmh_params, &chroma_params);

    int skip_gamut = primaries_are_ap1(&config.primaries);
    ALWAN_CORE_FNLIT(aces2_GamutCompressParams) gcp;
    ALWAN_CORE_T limit_J_max = ALWAN_CORE_LITERAL(0.0);
    if (!skip_gamut) { ALWAN_CORE_FNLIT(init_GamutCompressParams)(
            (ALWAN_CORE_T)config.peak_luminance, &limit_prim, jmh_params, &gcp);
        limit_J_max = ALWAN_CORE_FNVLIT(aces2_y_to_j)(
            (ALWAN_CORE_T)config.peak_luminance, jmh_params);
    }

    int is_pq = (config.eotf == ALWAN_TF_PQ);
    int is_hlg = (config.eotf == ALWAN_TF_HLG);
    ALWAN_CORE_T peak = (ALWAN_CORE_T)config.peak_luminance;
    /* HLG: display-linear is in n_r=100 units; normalise to [0,1] by the
     * display peak, then apply the inverse OOTF (display->scene) so the basic
     * HLG OETF produces the signal. System gamma per BT.2100:
     * gamma = 1.2 + 0.42*log10(peak/1000) (= 1.2 at 1000 nits). */
    ALWAN_CORE_T hlg_sc = ALWAN_CORE_LITERAL(100.0) / peak;
    ALWAN_CORE_T hlg_gamma = ALWAN_CORE_LITERAL(1.2)
        + ALWAN_CORE_LITERAL(0.42) * ALWAN_CORE_LOG10(peak / ALWAN_CORE_LITERAL(1000.0));
    ALWAN_CORE_T hlg_exp = (ALWAN_CORE_LITERAL(1.0) - hlg_gamma) / hlg_gamma;

    ALWAN_CORE_T r_hit_ap1 =
        ALWAN_CORE_LITERAL(128.0)
        + ALWAN_CORE_LITERAL(768.0)
          * (ALWAN_CORE_LN(peak / ALWAN_CORE_LITERAL(100.0))
             / ALWAN_CORE_LN(ALWAN_CORE_LITERAL(100.0)));
    ALWAN_CORE_T ap1_clamp_max = r_hit_ap1 * ALWAN_CORE_LITERAL(8.0);

    /* Output stage matrix: the OCIO-validated scalar path converts the
     * gamut-compressed JMh back to RGB in AP1, then applies AP1->limit
     * primaries and (for D65 displays) a D60->D65 Bradford CAT in XYZ.
     * All linear, so combine into one 3x3 computed once in f64 and applied
     * per-pixel. (The previous code went JMh->RGB via the limit-primary
     * JMhParams directly, skipping the matrix + CAT -- diverged from OCIO.) */
    alwan_f64 M_out_f64[9];
    {
        alwan_f64 ap1_to_limit[9];
        compute_ap1_to_limit_matrix_f64(&config.primaries, ap1_to_limit);
        int needs_cat = (ALWAN_ABS(config.primaries.white_x - ALWAN_D65_x) < ALWAN_LITERAL(0.01) &&
                         ALWAN_ABS(config.primaries.white_y - ALWAN_D65_y) < ALWAN_LITERAL(0.01));
        if (needs_cat) {
            alwan_f64 limit_to_xyz[9], xyz_to_limit[9], tmp[9], cat_limit[9];
            primaries_to_rgb_to_xyz_f64(
                config.primaries.red_x, config.primaries.red_y,
                config.primaries.green_x, config.primaries.green_y,
                config.primaries.blue_x, config.primaries.blue_y,
                config.primaries.white_x, config.primaries.white_y,
                ALWAN_LITERAL(1.0), limit_to_xyz);
            invert_mat3(limit_to_xyz, xyz_to_limit);
            mult_mat3(g_d60_to_d65_bradford, limit_to_xyz, tmp);
            mult_mat3(xyz_to_limit, tmp, cat_limit);
            mult_mat3(cat_limit, ap1_to_limit, M_out_f64);
        } else {
            for (int mi = 0; mi < 9; mi++) M_out_f64[mi] = ap1_to_limit[mi];
        }
    }

    size_t in_step  = in_stride  / sizeof(ALWAN_CORE_T);
    size_t out_step = out_stride / sizeof(ALWAN_CORE_T);

    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T const *px_in  = in  + i * in_step;
        ALWAN_CORE_T       *px_out = out + i * out_step;

        /* Clamp AP1 input to [0, ap1_clamp_max] -- matches OCIO pre-clamp.
         * Use >= 0 test (not < 0) so NaN (which fails all comparisons) maps to 0
         * rather than passing through, avoiding (int)NaN UB in hue table lookup. */
        ALWAN_CORE_T ri = px_in[0], gi = px_in[1], bi = px_in[2];
        if (!(ri >= 0)) ri = 0; else if (ri > ap1_clamp_max) ri = ap1_clamp_max;
        if (!(gi >= 0)) gi = 0; else if (gi > ap1_clamp_max) gi = ap1_clamp_max;
        if (!(bi >= 0)) bi = 0; else if (bi > ap1_clamp_max) bi = ap1_clamp_max;

        /* RGB -> Aab -> JMh */
        ALWAN_CORE_VEC3 rgb_in_v; rgb_in_v.v[0] = ri; rgb_in_v.v[1] = gi; rgb_in_v.v[2] = bi;
        ALWAN_CORE_VEC3 aab = ALWAN_CORE_FNVLIT(aces2_rgb_to_aab)(rgb_in_v, jmh_params);
        ALWAN_CORE_VEC3 jmh = ALWAN_CORE_FNVLIT(aces2_aab_to_jmh)(aab, jmh_params);

        /* Tonescale compress */
        ALWAN_CORE_T Y_in = ALWAN_CORE_FNVLIT(aces2_j_to_y)(jmh.v[0], jmh_params);
        ALWAN_CORE_T Y_out = ALWAN_CORE_FNVLIT(aces2_tonescale_fwd)(Y_in, &ts);
        ALWAN_CORE_T J_ts  = ALWAN_CORE_FNVLIT(aces2_y_to_j)(Y_out, jmh_params);

        /* Chroma compress */
        ALWAN_CORE_T M_cc = ALWAN_CORE_FNLIT(chroma_compress_fwd_impl)(
            jmh.v[0], jmh.v[1], jmh.v[2], J_ts, &chroma_params);

        /* Gamut compress (skipped when limit primaries == AP1) */
        ALWAN_CORE_T J_gc = J_ts, M_gc = M_cc;
        if (!skip_gamut && J_gc > ALWAN_CORE_LITERAL(0.0) && M_gc > ALWAN_CORE_LITERAL(0.0)) {
            if (J_gc > limit_J_max) {
                /* Above the limit: drop colourfulness but KEEP J (matches the
                 * scalar gamut_compress20 / OCIO; the old code clamped J). */
                M_gc = ALWAN_CORE_LITERAL(0.0);
            } else {
                ALWAN_CORE_FNLIT(aces2_HueDependentGamutParams) hdp;
                ALWAN_CORE_FNLIT(init_hue_dependent_params)(jmh.v[2], &gcp, &hdp);
                ALWAN_CORE_VEC2 gc = ALWAN_CORE_FNVLIT(aces2_compress_gamut_fwd)(
                    J_gc, M_gc, jmh.v[2], &gcp, &hdp); J_gc = gc.v[0];
                M_gc = gc.v[1];
            }
        }

        /* JMh -> Aab -> RGB in AP1, then the combined AP1->limit (+CAT) matrix.
         * This is the OCIO-validated scalar output path. */
        ALWAN_CORE_VEC3 jmh_gc; jmh_gc.v[0] = J_gc; jmh_gc.v[1] = M_gc; jmh_gc.v[2] = jmh.v[2];
        ALWAN_CORE_VEC3 aab_out  = ALWAN_CORE_FNVLIT(aces2_jmh_to_aab)(jmh_gc, jmh_params);
        ALWAN_CORE_VEC3 rgb_ap1  = ALWAN_CORE_FNVLIT(aces2_aab_to_rgb)(aab_out, jmh_params);
        ALWAN_CORE_T r = (ALWAN_CORE_T)M_out_f64[0]*rgb_ap1.v[0] + (ALWAN_CORE_T)M_out_f64[1]*rgb_ap1.v[1] + (ALWAN_CORE_T)M_out_f64[2]*rgb_ap1.v[2];
        ALWAN_CORE_T g = (ALWAN_CORE_T)M_out_f64[3]*rgb_ap1.v[0] + (ALWAN_CORE_T)M_out_f64[4]*rgb_ap1.v[1] + (ALWAN_CORE_T)M_out_f64[5]*rgb_ap1.v[2];
        ALWAN_CORE_T b = (ALWAN_CORE_T)M_out_f64[6]*rgb_ap1.v[0] + (ALWAN_CORE_T)M_out_f64[7]*rgb_ap1.v[1] + (ALWAN_CORE_T)M_out_f64[8]*rgb_ap1.v[2];

        /* Clamp + encode exactly as the scalar output transform: clamp the
         * normalized display-linear RGB to [0,1]; for PQ scale to absolute
         * nits [0,peak]; then apply the display OETF via the same path the
         * scalar uses (alwan_oetf_apply). */
        ALWAN_CORE_T lin[3];
        ALWAN_CORE_T const out_rgb[3] = { r, g, b };
        if (is_hlg) {
            /* Normalise display-linear (n_r=100 units) to [0,1] by peak, then
             * inverse OOTF: E = Yd^((1-g)/g) * Fd (Lw=1). The OETF below
             * (the basic HLG scene curve) then yields the signal. */
            ALWAN_CORE_T fr = out_rgb[0] * hlg_sc, fg = out_rgb[1] * hlg_sc, fb = out_rgb[2] * hlg_sc;
            fr = fr < ALWAN_CORE_LITERAL(0.0) ? ALWAN_CORE_LITERAL(0.0) : (fr > ALWAN_CORE_LITERAL(1.0) ? ALWAN_CORE_LITERAL(1.0) : fr);
            fg = fg < ALWAN_CORE_LITERAL(0.0) ? ALWAN_CORE_LITERAL(0.0) : (fg > ALWAN_CORE_LITERAL(1.0) ? ALWAN_CORE_LITERAL(1.0) : fg);
            fb = fb < ALWAN_CORE_LITERAL(0.0) ? ALWAN_CORE_LITERAL(0.0) : (fb > ALWAN_CORE_LITERAL(1.0) ? ALWAN_CORE_LITERAL(1.0) : fb);
            ALWAN_CORE_T Yd = ALWAN_CORE_LITERAL(0.2627) * fr + ALWAN_CORE_LITERAL(0.6780) * fg + ALWAN_CORE_LITERAL(0.0593) * fb;
            if (Yd < ALWAN_CORE_LITERAL(1e-12)) Yd = ALWAN_CORE_LITERAL(1e-12);
            ALWAN_CORE_T factor = ALWAN_CORE_POW(Yd, hlg_exp);
            lin[0] = factor * fr; lin[1] = factor * fg; lin[2] = factor * fb;
        } else {
            for (int k = 0; k < 3; k++) {
                ALWAN_CORE_T x = out_rgb[k];
                if (is_pq) {
                    /* x is display-linear normalised to the ACES2 reference
                     * luminance n_r = 100 nits (NOT to the display peak).
                     * Convert to absolute nits with n_r, then clamp to the
                     * display peak; the PQ OETF normalises by 10000. Scaling by
                     * `peak` over-brightened every non-100-nit output by
                     * peak/100 (e.g. 10x for the 1000-nit PQ outputs). */
                    ALWAN_CORE_T nits = x * ALWAN_CORE_LITERAL(100.0);
                    lin[k] = nits < ALWAN_CORE_LITERAL(0.0) ? ALWAN_CORE_LITERAL(0.0)
                           : (nits > peak ? peak : nits);
                } else {
                    lin[k] = x < ALWAN_CORE_LITERAL(0.0) ? ALWAN_CORE_LITERAL(0.0)
                           : (x > ALWAN_CORE_LITERAL(1.0) ? ALWAN_CORE_LITERAL(1.0) : x);
                }
            }
        }
        ALWAN_CORE_FNLIT(alwan_oetf_apply)(lin, sizeof(ALWAN_CORE_T), lin, sizeof(ALWAN_CORE_T), 3, config.eotf);
        px_out[0] = lin[0];
        px_out[1] = lin[1];
        px_out[2] = lin[2];
    }

    return ALWAN_OK;
}
