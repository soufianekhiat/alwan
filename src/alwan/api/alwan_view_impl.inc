/* No include guard -- included twice (f32 and f64 passes).
 *
 * Native dual-precision view transforms. Shared constant data (ap1_to_ap0,
 * the AgX inset matrices + contrast LUTs, the Blender 3D LUT) is defined
 * natively at f32 and f64 in alwan_view.c and selected here via
 * ALWAN_CORE_FNLIT, so every arithmetic operation
 * runs at the selected precision -- a genuine f32 path, not a
 * widen-to-f64 facade. The core _v helpers resolve to the matching
 * precision via ALWAN_CORE_FNVLIT / ALWAN_CORE_FNLIT.
 */

/* ----------------------------------------------------------------
 * ACES RRT+ODT (Simplified Approximation for Rec.709)
 * Input: ACEScg (AP1) linear. Output: sRGB linear (pre display EOTF).
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(aces_rec709_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_RGB ap0;
    ap0.r = ALWAN_CORE_FNLIT(ap1_to_ap0)[0]*rgb_in[0] + ALWAN_CORE_FNLIT(ap1_to_ap0)[1]*rgb_in[1] + ALWAN_CORE_FNLIT(ap1_to_ap0)[2]*rgb_in[2];
    ap0.g = ALWAN_CORE_FNLIT(ap1_to_ap0)[3]*rgb_in[0] + ALWAN_CORE_FNLIT(ap1_to_ap0)[4]*rgb_in[1] + ALWAN_CORE_FNLIT(ap1_to_ap0)[5]*rgb_in[2];
    ap0.b = ALWAN_CORE_FNLIT(ap1_to_ap0)[6]*rgb_in[0] + ALWAN_CORE_FNLIT(ap1_to_ap0)[7]*rgb_in[1] + ALWAN_CORE_FNLIT(ap1_to_ap0)[8]*rgb_in[2];

    ALWAN_CORE_RGB out;
    ALWAN_CORE_FNLIT(alwan_aces1_output_transform)(&out, &ap0, ALWAN_ACES1_OUT_SRGB_100NIT);

    /* Output includes sRGB OETF -- undo to get linear for the chain. */
    rgb_out[0] = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(out.r);
    rgb_out[1] = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(out.g);
    rgb_out[2] = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(out.b);
}

/* ----------------------------------------------------------------
 * AgX View Transform (inset -> log encode -> contrast LUT -> linearize)
 * ---------------------------------------------------------------- */
static ALWAN_CORE_T ALWAN_CORE_FN(agx_lut_eval)(ALWAN_CORE_T t, ALWAN_CORE_T const *lut) {
    /* Linear interpolation in the 4096-entry contrast LUT. */
    t = ALWAN_CORE_SATURATE(t);
    ALWAN_CORE_T fi = t * (ALWAN_CORE_T)(AGX_LUT_SIZE - 1);
    /* SATURATE cannot remove a NaN (both compares fail), and (int)NaN is
     * undefined behaviour -- INT_MIN on x86 -- which indexes the LUT far out of
     * bounds and segfaults. A NaN coordinate is mapped to 0 here: the >= test
     * is false for NaN, so this also re-clamps any stray out-of-range value. */
    fi = (fi >= ALWAN_CORE_ZERO) ? fi : ALWAN_CORE_ZERO;
    int i0 = (int)fi;
    if (i0 > AGX_LUT_SIZE - 1) i0 = AGX_LUT_SIZE - 1;
    int i1 = i0 + 1;
    if (i1 >= AGX_LUT_SIZE) i1 = AGX_LUT_SIZE - 1;
    ALWAN_CORE_T frac = fi - (ALWAN_CORE_T)i0;
    return lut[i0] + frac * (lut[i1] - lut[i0]);
}

/* Parameterized AgX picture formation: clamp -> inset matrix -> log2 encode
 * over [log2_min, log2_max] -> contrast LUT. The named experiments differ only
 * in their inset matrix, log2 range and contrast LUT. */
static void ALWAN_CORE_FN(agx_transform_impl)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out,
        ALWAN_CORE_T const *inset, ALWAN_CORE_T log2_min, ALWAN_CORE_T log2_max,
        ALWAN_CORE_T const *lut) {
    /* Clamp negatives to 0 (matches OCIO RangeTransform before inset). */
    ALWAN_CORE_T r_in = rgb_in[0] > ALWAN_CORE_ZERO ? rgb_in[0] : ALWAN_CORE_ZERO;
    ALWAN_CORE_T g_in = rgb_in[1] > ALWAN_CORE_ZERO ? rgb_in[1] : ALWAN_CORE_ZERO;
    ALWAN_CORE_T b_in = rgb_in[2] > ALWAN_CORE_ZERO ? rgb_in[2] : ALWAN_CORE_ZERO;

    ALWAN_CORE_T inset_r = inset[0]*r_in + inset[1]*g_in + inset[2]*b_in;
    ALWAN_CORE_T inset_g = inset[3]*r_in + inset[4]*g_in + inset[5]*b_in;
    ALWAN_CORE_T inset_b = inset[6]*r_in + inset[7]*g_in + inset[8]*b_in;

    ALWAN_CORE_T log_r = ALWAN_CORE_FNVLIT(alwan_agx_log_encode)(inset_r, log2_min, log2_max);
    ALWAN_CORE_T log_g = ALWAN_CORE_FNVLIT(alwan_agx_log_encode)(inset_g, log2_min, log2_max);
    ALWAN_CORE_T log_b = ALWAN_CORE_FNVLIT(alwan_agx_log_encode)(inset_b, log2_min, log2_max);

    rgb_out[0] = ALWAN_CORE_FN(agx_lut_eval)(log_r, lut);
    rgb_out[1] = ALWAN_CORE_FN(agx_lut_eval)(log_g, lut);
    rgb_out[2] = ALWAN_CORE_FN(agx_lut_eval)(log_b, lut);
}

static void ALWAN_CORE_FN(agx_base_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_FN(agx_transform_impl)(rgb_in, rgb_out,
        ALWAN_CORE_FNLIT(agx_inset_matrix),
        ALWAN_CORE_LITERAL(-12.47393), ALWAN_CORE_LITERAL(4.026069),
        ALWAN_CORE_FNLIT(agx_default_contrast_lut));
}

/* SB2383: Sobotka's newer AgX experiment -- own inset + contrast LUT. Its
 * -10/+6.5 EV range around mid-grey 0.18 is, in absolute log2 bounds (what
 * alwan_agx_log_encode takes), log2(0.18)+[-10,+6.5] = [-12.47393, 4.026069]
 * -- the same framing as the original; SB2383 differs in the inset + curve. */
static void ALWAN_CORE_FN(agx_sb2383_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_FN(agx_transform_impl)(rgb_in, rgb_out,
        ALWAN_CORE_FNLIT(agx_sb2383_inset_matrix),
        ALWAN_CORE_LITERAL(-12.47393), ALWAN_CORE_LITERAL(4.026069),
        ALWAN_CORE_FNLIT(agx_sb2383_contrast_lut));
}

/* AgX LUT output is sRGB-encoded; sRGB EOTF linearizes it. */
static void ALWAN_CORE_FN(agx_encoded_to_linear)(ALWAN_CORE_T *rgb) {
    rgb[0] = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(ALWAN_CORE_SATURATE(rgb[0]));
    rgb[1] = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(ALWAN_CORE_SATURATE(rgb[1]));
    rgb[2] = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(ALWAN_CORE_SATURATE(rgb[2]));
}

static void ALWAN_CORE_FN(agx_base_view)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_FN(agx_base_transform)(rgb_in, rgb_out);
    ALWAN_CORE_FN(agx_encoded_to_linear)(rgb_out);
}

static void ALWAN_CORE_FN(agx_sb2383_view)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_FN(agx_sb2383_transform)(rgb_in, rgb_out);
    ALWAN_CORE_FN(agx_encoded_to_linear)(rgb_out);
}

/* Blender AgX (EaryChow): a baked 57^3 3D LUT (guard-rail gamut compression
 * cannot be decomposed to matrix+curve). Tetrahedral interpolation of the f32
 * LUT in the working precision, matching OCIO's tetrahedral sampling (and
 * keeping the neutral axis symmetric). */
static void ALWAN_CORE_FN(agx_blender_lut3d_sample)(ALWAN_CORE_T r, ALWAN_CORE_T g, ALWAN_CORE_T b, ALWAN_CORE_T *out) {
    int const n = AGX_BLENDER_LUT_SIZE;
    ALWAN_CORE_T const mi = (ALWAN_CORE_T)(n - 1);
    r = ALWAN_CORE_SATURATE(r) * mi;
    g = ALWAN_CORE_SATURATE(g) * mi;
    b = ALWAN_CORE_SATURATE(b) * mi;
    /* Same NaN guard as agx_lut_eval: SATURATE leaves a NaN intact and
     * (int)NaN is UB (INT_MIN), which reads far outside the 57^3 cube. A NaN
     * channel resolves to index 0 rather than crashing. */
    r = (r >= ALWAN_CORE_ZERO) ? r : ALWAN_CORE_ZERO;
    g = (g >= ALWAN_CORE_ZERO) ? g : ALWAN_CORE_ZERO;
    b = (b >= ALWAN_CORE_ZERO) ? b : ALWAN_CORE_ZERO;
    int r0 = (int)r, g0 = (int)g, b0 = (int)b;
    if (r0 > n - 1) r0 = n - 1;
    if (g0 > n - 1) g0 = n - 1;
    if (b0 > n - 1) b0 = n - 1;
    int r1 = r0 + 1 < n ? r0 + 1 : n - 1;
    int g1 = g0 + 1 < n ? g0 + 1 : n - 1;
    int b1 = b0 + 1 < n ? b0 + 1 : n - 1;
    ALWAN_CORE_T fr = r - (ALWAN_CORE_T)r0, fg = g - (ALWAN_CORE_T)g0, fb = b - (ALWAN_CORE_T)b0;
    #define ALWAN_BL_AT(rr,gg,bb) (ALWAN_CORE_FNLIT(agx_blender_lut3d) + ((size_t)(bb)*(size_t)n*(size_t)n + (size_t)(gg)*(size_t)n + (size_t)(rr))*3)
    ALWAN_CORE_T const *c000 = ALWAN_BL_AT(r0,g0,b0), *c111 = ALWAN_BL_AT(r1,g1,b1);
    /* Pick the tetrahedron by the ordering of (fr,fg,fb): two intermediate
     * corners cP,cQ and sorted weights w1>=w2>=w3. */
    ALWAN_CORE_T const *cP, *cQ;
    ALWAN_CORE_T w1, w2, w3;
    if (fr >= fg) {
        if (fg >= fb)      { cP = ALWAN_BL_AT(r1,g0,b0); cQ = ALWAN_BL_AT(r1,g1,b0); w1 = fr; w2 = fg; w3 = fb; }
        else if (fr >= fb) { cP = ALWAN_BL_AT(r1,g0,b0); cQ = ALWAN_BL_AT(r1,g0,b1); w1 = fr; w2 = fb; w3 = fg; }
        else               { cP = ALWAN_BL_AT(r0,g0,b1); cQ = ALWAN_BL_AT(r1,g0,b1); w1 = fb; w2 = fr; w3 = fg; }
    } else {
        if (fb >= fg)      { cP = ALWAN_BL_AT(r0,g0,b1); cQ = ALWAN_BL_AT(r0,g1,b1); w1 = fb; w2 = fg; w3 = fr; }
        else if (fb >= fr) { cP = ALWAN_BL_AT(r0,g1,b0); cQ = ALWAN_BL_AT(r0,g1,b1); w1 = fg; w2 = fb; w3 = fr; }
        else               { cP = ALWAN_BL_AT(r0,g1,b0); cQ = ALWAN_BL_AT(r1,g1,b0); w1 = fg; w2 = fr; w3 = fb; }
    }
    #undef ALWAN_BL_AT
    for (int ch = 0; ch < 3; ch++) {
        ALWAN_CORE_T v0 = c000[ch];
        out[ch] = v0
                + w1 * (cP[ch]  - v0)
                + w2 * (cQ[ch]  - cP[ch])
                + w3 * (c111[ch] - cQ[ch]);
    }
}

static void ALWAN_CORE_FN(agx_blender_view)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_T const *M = ALWAN_CORE_FNLIT(agx_blender_rec709_to_egamut);
    ALWAN_CORE_T er = M[0]*rgb_in[0] + M[1]*rgb_in[1] + M[2]*rgb_in[2];
    ALWAN_CORE_T eg = M[3]*rgb_in[0] + M[4]*rgb_in[1] + M[5]*rgb_in[2];
    ALWAN_CORE_T eb = M[6]*rgb_in[0] + M[7]*rgb_in[1] + M[8]*rgb_in[2];
    /* log2 allocation shaper: -10..+15 stops around 0.18 (matches Blender). */
    ALWAN_CORE_T sr = ALWAN_CORE_FNVLIT(alwan_agx_log_encode)(er, ALWAN_CORE_LITERAL(-12.47393), ALWAN_CORE_LITERAL(12.5260688117));
    ALWAN_CORE_T sg = ALWAN_CORE_FNVLIT(alwan_agx_log_encode)(eg, ALWAN_CORE_LITERAL(-12.47393), ALWAN_CORE_LITERAL(12.5260688117));
    ALWAN_CORE_T sb = ALWAN_CORE_FNVLIT(alwan_agx_log_encode)(eb, ALWAN_CORE_LITERAL(-12.47393), ALWAN_CORE_LITERAL(12.5260688117));
    ALWAN_CORE_T lut[3];
    ALWAN_CORE_FN(agx_blender_lut3d_sample)(sr, sg, sb, lut);
    /* Cube output is power-2.4 encoded; decode to display-linear (caller OETFs). */
    rgb_out[0] = ALWAN_CORE_POW(lut[0], ALWAN_CORE_LITERAL(2.4));
    rgb_out[1] = ALWAN_CORE_POW(lut[1], ALWAN_CORE_LITERAL(2.4));
    rgb_out[2] = ALWAN_CORE_POW(lut[2], ALWAN_CORE_LITERAL(2.4));
}

static void ALWAN_CORE_FN(agx_punchy_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_FN(agx_base_transform)(rgb_in, rgb_out);
    ALWAN_CORE_VEC3 enc;
    enc.v[0] = rgb_out[0]; enc.v[1] = rgb_out[1]; enc.v[2] = rgb_out[2];
    ALWAN_CORE_VEC3 graded = ALWAN_CORE_FNVLIT(alwan_agx_punchy_grade)(enc);
    rgb_out[0] = graded.v[0]; rgb_out[1] = graded.v[1]; rgb_out[2] = graded.v[2];
    ALWAN_CORE_FN(agx_encoded_to_linear)(rgb_out);
}

static void ALWAN_CORE_FN(agx_golden_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_FN(agx_base_transform)(rgb_in, rgb_out);
    ALWAN_CORE_VEC3 enc;
    enc.v[0] = rgb_out[0]; enc.v[1] = rgb_out[1]; enc.v[2] = rgb_out[2];
    ALWAN_CORE_VEC3 graded = ALWAN_CORE_FNVLIT(alwan_agx_golden_grade)(enc);
    rgb_out[0] = graded.v[0]; rgb_out[1] = graded.v[1]; rgb_out[2] = graded.v[2];
    ALWAN_CORE_FN(agx_encoded_to_linear)(rgb_out);
}

/* ----------------------------------------------------------------
 * BT.2446 Method A HDR<->SDR
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(bt2446a_hdr_to_sdr_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_T L_hdr = ALWAN_CORE_LITERAL(1000.0);
    ALWAN_CORE_T L_sdr = ALWAN_CORE_LITERAL(100.0);
    for (int i = 0; i < 3; i++) {
        rgb_out[i] = ALWAN_CORE_FNVLIT(alwan_bt2446a_forward)(rgb_in[i], L_hdr, L_sdr);
    }
}

static void ALWAN_CORE_FN(bt2446a_sdr_to_hdr_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_T L_hdr = ALWAN_CORE_LITERAL(1000.0);
    ALWAN_CORE_T L_sdr = ALWAN_CORE_LITERAL(100.0);
    for (int i = 0; i < 3; i++) {
        rgb_out[i] = ALWAN_CORE_FNVLIT(alwan_bt2446a_inverse)(rgb_in[i], L_hdr, L_sdr);
    }
}

/* ----------------------------------------------------------------
 * Khronos PBR Neutral Tone Mapping
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(khronos_pbr_neutral_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_VEC3 in_v = {{rgb_in[0], rgb_in[1], rgb_in[2]}};
    ALWAN_CORE_VEC3 out_v = ALWAN_CORE_FNVLIT(alwan_khronos_pbr_neutral)(in_v);
    rgb_out[0] = out_v.v[0];
    rgb_out[1] = out_v.v[1];
    rgb_out[2] = out_v.v[2];
}

/* ----------------------------------------------------------------
 * Reinhard Extended (Luminance-based)
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(reinhard_ext_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_VEC3 in_v = {{rgb_in[0], rgb_in[1], rgb_in[2]}};
    ALWAN_CORE_VEC3 out_v = ALWAN_CORE_FNVLIT(alwan_reinhard_extended_luma)(in_v, ALWAN_CORE_LITERAL(4.0));
    rgb_out[0] = ALWAN_CORE_SATURATE(out_v.v[0]);
    rgb_out[1] = ALWAN_CORE_SATURATE(out_v.v[1]);
    rgb_out[2] = ALWAN_CORE_SATURATE(out_v.v[2]);
}

/* ----------------------------------------------------------------
 * Uchimura / Gran Turismo
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(uchimura_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    rgb_out[0] = ALWAN_CORE_SATURATE(ALWAN_CORE_FNVLIT(alwan_uchimura_default)(ALWAN_CORE_MAX(rgb_in[0], ALWAN_CORE_ZERO)));
    rgb_out[1] = ALWAN_CORE_SATURATE(ALWAN_CORE_FNVLIT(alwan_uchimura_default)(ALWAN_CORE_MAX(rgb_in[1], ALWAN_CORE_ZERO)));
    rgb_out[2] = ALWAN_CORE_SATURATE(ALWAN_CORE_FNVLIT(alwan_uchimura_default)(ALWAN_CORE_MAX(rgb_in[2], ALWAN_CORE_ZERO)));
}

/* ----------------------------------------------------------------
 * Lottes / AMD Cauldron
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(lottes_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_VEC3 in_v = {{
        ALWAN_CORE_MAX(rgb_in[0], ALWAN_CORE_ZERO),
        ALWAN_CORE_MAX(rgb_in[1], ALWAN_CORE_ZERO),
        ALWAN_CORE_MAX(rgb_in[2], ALWAN_CORE_ZERO)
    }};
    ALWAN_CORE_VEC3 out_v = ALWAN_CORE_FNVLIT(alwan_lottes_default)(in_v);
    rgb_out[0] = ALWAN_CORE_SATURATE(out_v.v[0]);
    rgb_out[1] = ALWAN_CORE_SATURATE(out_v.v[1]);
    rgb_out[2] = ALWAN_CORE_SATURATE(out_v.v[2]);
}

/* ----------------------------------------------------------------
 * Tony McMapface (Somewhat Boring Display Transform)
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(tony_mcmapface_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_VEC3 in_v = {{
        ALWAN_CORE_MAX(rgb_in[0], ALWAN_CORE_ZERO),
        ALWAN_CORE_MAX(rgb_in[1], ALWAN_CORE_ZERO),
        ALWAN_CORE_MAX(rgb_in[2], ALWAN_CORE_ZERO)
    }};
    ALWAN_CORE_VEC3 out_v = ALWAN_CORE_FNVLIT(alwan_tony_mcmapface)(in_v);
    rgb_out[0] = ALWAN_CORE_SATURATE(out_v.v[0]);
    rgb_out[1] = ALWAN_CORE_SATURATE(out_v.v[1]);
    rgb_out[2] = ALWAN_CORE_SATURATE(out_v.v[2]);
}

/* ----------------------------------------------------------------
 * BT.2446 Method B: SDR to HDR (1000 nits HDR, 100 nits SDR)
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(bt2446b_sdr_to_hdr_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    for (int i = 0; i < 3; i++) {
        rgb_out[i] = ALWAN_CORE_FNVLIT(alwan_bt2446b_forward)(ALWAN_CORE_SATURATE(rgb_in[i]),
                        ALWAN_CORE_LITERAL(1000.0), ALWAN_CORE_LITERAL(100.0));
    }
}

/* ----------------------------------------------------------------
 * BT.2446 Method C: HDR to SDR (1000 nits HDR, 100 nits SDR)
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(bt2446c_hdr_to_sdr_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    for (int i = 0; i < 3; i++) {
        rgb_out[i] = ALWAN_CORE_FNVLIT(alwan_bt2446c_forward)(ALWAN_CORE_SATURATE(rgb_in[i]),
                        ALWAN_CORE_LITERAL(1000.0), ALWAN_CORE_LITERAL(100.0));
    }
}

/* ----------------------------------------------------------------
 * BT.2390 EETF: HDR to SDR (10000 nits -> 100 nits)
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(bt2390_hdr_to_sdr_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    for (int i = 0; i < 3; i++) {
        rgb_out[i] = ALWAN_CORE_FNVLIT(alwan_bt2390_eetf_luminance)(ALWAN_CORE_SATURATE(rgb_in[i]),
                        ALWAN_CORE_LITERAL(10000.0), ALWAN_CORE_LITERAL(100.0));
    }
}

/* ----------------------------------------------------------------
 * Reinhard Calibrated (key=0.18, L_avg=0.18, L_white=4.0)
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(reinhard_calibrated_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    for (int i = 0; i < 3; i++) {
        rgb_out[i] = ALWAN_CORE_SATURATE(ALWAN_CORE_FNVLIT(alwan_reinhard_calibrated)(
                        ALWAN_CORE_MAX(rgb_in[i], ALWAN_CORE_ZERO),
                        ALWAN_CORE_LITERAL(0.18), ALWAN_CORE_LITERAL(0.18), ALWAN_CORE_LITERAL(4.0)));
    }
}

/* ----------------------------------------------------------------
 * Exposure-Based (exposure = 0 EV)
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(exposure_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_VEC3 in_v = {{
        ALWAN_CORE_MAX(rgb_in[0], ALWAN_CORE_ZERO),
        ALWAN_CORE_MAX(rgb_in[1], ALWAN_CORE_ZERO),
        ALWAN_CORE_MAX(rgb_in[2], ALWAN_CORE_ZERO)
    }};
    ALWAN_CORE_VEC3 out_v = ALWAN_CORE_FNVLIT(alwan_exposure_tonemap_rgb)(in_v, ALWAN_CORE_ZERO);
    rgb_out[0] = ALWAN_CORE_SATURATE(out_v.v[0]);
    rgb_out[1] = ALWAN_CORE_SATURATE(out_v.v[1]);
    rgb_out[2] = ALWAN_CORE_SATURATE(out_v.v[2]);
}


/* ----------------------------------------------------------------
 * Unclamped twins of the per-channel tone-mapping views. The raw operator
 * output is returned without the display [0,1] SATURATE and without the
 * negative-input clip (except where the operator's math requires it --
 * Uchimura's toe is pow-based, so MAX(0) stays as a numerical guard).
 * Used by alwan_view_transform_apply_unclamped.
 * ---------------------------------------------------------------- */
static void ALWAN_CORE_FN(reinhard_ext_transform_unclamped)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_VEC3 in_v = {{rgb_in[0], rgb_in[1], rgb_in[2]}};
    ALWAN_CORE_VEC3 out_v = ALWAN_CORE_FNVLIT(alwan_reinhard_extended_luma)(in_v, ALWAN_CORE_LITERAL(4.0));
    rgb_out[0] = out_v.v[0];
    rgb_out[1] = out_v.v[1];
    rgb_out[2] = out_v.v[2];
}

static void ALWAN_CORE_FN(uchimura_transform_unclamped)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    /* MAX(0) retained: the toe segment is POW-based and NaNs on negatives. */
    rgb_out[0] = ALWAN_CORE_FNVLIT(alwan_uchimura_default)(ALWAN_CORE_MAX(rgb_in[0], ALWAN_CORE_ZERO));
    rgb_out[1] = ALWAN_CORE_FNVLIT(alwan_uchimura_default)(ALWAN_CORE_MAX(rgb_in[1], ALWAN_CORE_ZERO));
    rgb_out[2] = ALWAN_CORE_FNVLIT(alwan_uchimura_default)(ALWAN_CORE_MAX(rgb_in[2], ALWAN_CORE_ZERO));
}

static void ALWAN_CORE_FN(lottes_transform_unclamped)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_VEC3 in_v = {{rgb_in[0], rgb_in[1], rgb_in[2]}};
    ALWAN_CORE_VEC3 out_v = ALWAN_CORE_FNVLIT(alwan_lottes_default)(in_v);
    rgb_out[0] = out_v.v[0];
    rgb_out[1] = out_v.v[1];
    rgb_out[2] = out_v.v[2];
}

static void ALWAN_CORE_FN(tony_mcmapface_transform_unclamped)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_VEC3 in_v = {{rgb_in[0], rgb_in[1], rgb_in[2]}};
    ALWAN_CORE_VEC3 out_v = ALWAN_CORE_FNVLIT(alwan_tony_mcmapface)(in_v);
    rgb_out[0] = out_v.v[0];
    rgb_out[1] = out_v.v[1];
    rgb_out[2] = out_v.v[2];
}

static void ALWAN_CORE_FN(reinhard_calibrated_transform_unclamped)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    for (int i = 0; i < 3; i++) {
        rgb_out[i] = ALWAN_CORE_FNVLIT(alwan_reinhard_calibrated)(rgb_in[i],
                        ALWAN_CORE_LITERAL(0.18), ALWAN_CORE_LITERAL(0.18), ALWAN_CORE_LITERAL(4.0));
    }
}

static void ALWAN_CORE_FN(exposure_transform_unclamped)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    ALWAN_CORE_VEC3 in_v = {{rgb_in[0], rgb_in[1], rgb_in[2]}};
    ALWAN_CORE_VEC3 out_v = ALWAN_CORE_FNVLIT(alwan_exposure_tonemap_rgb)(in_v, ALWAN_CORE_ZERO);
    rgb_out[0] = out_v.v[0];
    rgb_out[1] = out_v.v[1];
    rgb_out[2] = out_v.v[2];
}

/* ----------------------------------------------------------------
 * View Transform API (native per-precision dispatch + bulk loop)
 * ---------------------------------------------------------------- */
static int ALWAN_CORE_FN(alwan__view_apply_impl)(ALWAN_CORE_T *rgb_out, size_t out_stride, ALWAN_CORE_T const *rgb_in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx, int unclamped) {
    (void)ctx;  /* Unused for stateless transforms */

    if (!rgb_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    void (*transform_fn)(ALWAN_CORE_T const *, ALWAN_CORE_T *) = NULL;

    switch (vt) {
        case ALWAN_VIEW_ACES_REC709:        transform_fn = ALWAN_CORE_FN(aces_rec709_transform); break;
        case ALWAN_VIEW_AGX_ORIGINAL:                transform_fn = ALWAN_CORE_FN(agx_base_view); break;
        case ALWAN_VIEW_AGX_PUNCHY:         transform_fn = ALWAN_CORE_FN(agx_punchy_transform); break;
        case ALWAN_VIEW_AGX_GOLDEN:         transform_fn = ALWAN_CORE_FN(agx_golden_transform); break;
        case ALWAN_VIEW_AGX_SB2383:         transform_fn = ALWAN_CORE_FN(agx_sb2383_view); break;
        case ALWAN_VIEW_AGX_BLENDER:        transform_fn = ALWAN_CORE_FN(agx_blender_view); break;
        case ALWAN_VIEW_BT2446A_HDR_TO_SDR: transform_fn = ALWAN_CORE_FN(bt2446a_hdr_to_sdr_transform); break;
        case ALWAN_VIEW_BT2446A_SDR_TO_HDR: transform_fn = ALWAN_CORE_FN(bt2446a_sdr_to_hdr_transform); break;
        case ALWAN_VIEW_KHRONOS_PBR_NEUTRAL: transform_fn = ALWAN_CORE_FN(khronos_pbr_neutral_transform); break;
        case ALWAN_VIEW_REINHARD_EXT:       transform_fn = ALWAN_CORE_FN(reinhard_ext_transform); break;
        case ALWAN_VIEW_UCHIMURA:           transform_fn = ALWAN_CORE_FN(uchimura_transform); break;
        case ALWAN_VIEW_LOTTES:             transform_fn = ALWAN_CORE_FN(lottes_transform); break;
        case ALWAN_VIEW_TONY_MCMAPFACE:     transform_fn = ALWAN_CORE_FN(tony_mcmapface_transform); break;
        case ALWAN_VIEW_BT2446B_SDR_TO_HDR: transform_fn = ALWAN_CORE_FN(bt2446b_sdr_to_hdr_transform); break;
        case ALWAN_VIEW_BT2446C_HDR_TO_SDR: transform_fn = ALWAN_CORE_FN(bt2446c_hdr_to_sdr_transform); break;
        case ALWAN_VIEW_BT2390_HDR_TO_SDR:  transform_fn = ALWAN_CORE_FN(bt2390_hdr_to_sdr_transform); break;
        case ALWAN_VIEW_REINHARD_CALIBRATED: transform_fn = ALWAN_CORE_FN(reinhard_calibrated_transform); break;
        case ALWAN_VIEW_EXPOSURE:           transform_fn = ALWAN_CORE_FN(exposure_transform); break;
        default:
            return ALWAN_E_INVALID;
    }

    /* Unclamped entry: swap in the raw twins for the per-channel tone mappers.
     * Transforms whose [0,1] output is definitional (AgX picture formation,
     * ACES display encode, the BT.2446/2390 normalized-signal EETFs, PBR
     * Neutral) keep their standard implementation -- for those the two entry
     * points return identical results by design. */
    if (unclamped) {
        switch (vt) {
            case ALWAN_VIEW_REINHARD_EXT:        transform_fn = ALWAN_CORE_FN(reinhard_ext_transform_unclamped); break;
            case ALWAN_VIEW_UCHIMURA:            transform_fn = ALWAN_CORE_FN(uchimura_transform_unclamped); break;
            case ALWAN_VIEW_LOTTES:              transform_fn = ALWAN_CORE_FN(lottes_transform_unclamped); break;
            case ALWAN_VIEW_TONY_MCMAPFACE:      transform_fn = ALWAN_CORE_FN(tony_mcmapface_transform_unclamped); break;
            case ALWAN_VIEW_REINHARD_CALIBRATED: transform_fn = ALWAN_CORE_FN(reinhard_calibrated_transform_unclamped); break;
            case ALWAN_VIEW_EXPOSURE:            transform_fn = ALWAN_CORE_FN(exposure_transform_unclamped); break;
            default: break;
        }
    }

    /* Apply view transform to RGB triplets (strides are in bytes). */
    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T const *in_ptr = (ALWAN_CORE_T const *)((char const *)rgb_in + i * in_stride);
        ALWAN_CORE_T *out_ptr = (ALWAN_CORE_T *)((char *)rgb_out + i * out_stride);
        transform_fn(in_ptr, out_ptr);
    }

    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_view_transform_apply)(ALWAN_CORE_T *rgb_out, size_t out_stride, ALWAN_CORE_T const *rgb_in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx) {
    return ALWAN_CORE_FN(alwan__view_apply_impl)(rgb_out, out_stride, rgb_in, in_stride, count, vt, ctx, 0);
}

alwan_status ALWAN_CORE_FNLIT(alwan_view_transform_apply_unclamped)(ALWAN_CORE_T *rgb_out, size_t out_stride, ALWAN_CORE_T const *rgb_in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx) {
    return ALWAN_CORE_FN(alwan__view_apply_impl)(rgb_out, out_stride, rgb_in, in_stride, count, vt, ctx, 1);
}
