/* No include guard — included twice (f32 and f64 passes).
 *
 * Native dual-precision view transforms. Shared constant data
 * (ap1_to_ap0, agx_inset_matrix, agx_outset_matrix and the AgX contrast
 * LUT) is defined once at f64 precision in alwan_view.c and read here
 * with per-element casts to ALWAN_CORE_T, so every arithmetic operation
 * runs at the selected precision — a genuine f32 path, not a
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

    /* Output includes sRGB OETF — undo to get linear for the chain. */
    rgb_out[0] = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(out.r);
    rgb_out[1] = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(out.g);
    rgb_out[2] = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(out.b);
}

/* ----------------------------------------------------------------
 * AgX View Transform (inset -> log encode -> contrast LUT -> linearize)
 * ---------------------------------------------------------------- */
static ALWAN_CORE_T ALWAN_CORE_FN(agx_lut_eval)(ALWAN_CORE_T t) {
    /* Linear interpolation in the 4096-entry contrast LUT. */
    t = ALWAN_CORE_SATURATE(t);
    ALWAN_CORE_T fi = t * (ALWAN_CORE_T)(AGX_LUT_SIZE - 1);
    int i0 = (int)fi;
    int i1 = i0 + 1;
    if (i1 >= AGX_LUT_SIZE) i1 = AGX_LUT_SIZE - 1;
    ALWAN_CORE_T frac = fi - (ALWAN_CORE_T)i0;
    return ALWAN_CORE_FNLIT(agx_default_contrast_lut)[i0]
         + frac * (ALWAN_CORE_FNLIT(agx_default_contrast_lut)[i1] - ALWAN_CORE_FNLIT(agx_default_contrast_lut)[i0]);
}

static void ALWAN_CORE_FN(agx_base_transform)(ALWAN_CORE_T const *rgb_in, ALWAN_CORE_T *rgb_out) {
    /* Clamp negatives to 0 (matches OCIO RangeTransform before inset). */
    ALWAN_CORE_T r_in = rgb_in[0] > ALWAN_CORE_ZERO ? rgb_in[0] : ALWAN_CORE_ZERO;
    ALWAN_CORE_T g_in = rgb_in[1] > ALWAN_CORE_ZERO ? rgb_in[1] : ALWAN_CORE_ZERO;
    ALWAN_CORE_T b_in = rgb_in[2] > ALWAN_CORE_ZERO ? rgb_in[2] : ALWAN_CORE_ZERO;

    ALWAN_CORE_T inset_r = ALWAN_CORE_FNLIT(agx_inset_matrix)[0]*r_in + ALWAN_CORE_FNLIT(agx_inset_matrix)[1]*g_in + ALWAN_CORE_FNLIT(agx_inset_matrix)[2]*b_in;
    ALWAN_CORE_T inset_g = ALWAN_CORE_FNLIT(agx_inset_matrix)[3]*r_in + ALWAN_CORE_FNLIT(agx_inset_matrix)[4]*g_in + ALWAN_CORE_FNLIT(agx_inset_matrix)[5]*b_in;
    ALWAN_CORE_T inset_b = ALWAN_CORE_FNLIT(agx_inset_matrix)[6]*r_in + ALWAN_CORE_FNLIT(agx_inset_matrix)[7]*g_in + ALWAN_CORE_FNLIT(agx_inset_matrix)[8]*b_in;

    ALWAN_CORE_T const agx_min = ALWAN_CORE_LITERAL(-12.47393);
    ALWAN_CORE_T const agx_max = ALWAN_CORE_LITERAL(4.026069);

    ALWAN_CORE_T log_r = ALWAN_CORE_FNVLIT(alwan_agx_log_encode)(inset_r, agx_min, agx_max);
    ALWAN_CORE_T log_g = ALWAN_CORE_FNVLIT(alwan_agx_log_encode)(inset_g, agx_min, agx_max);
    ALWAN_CORE_T log_b = ALWAN_CORE_FNVLIT(alwan_agx_log_encode)(inset_b, agx_min, agx_max);

    rgb_out[0] = ALWAN_CORE_FN(agx_lut_eval)(log_r);
    rgb_out[1] = ALWAN_CORE_FN(agx_lut_eval)(log_g);
    rgb_out[2] = ALWAN_CORE_FN(agx_lut_eval)(log_b);
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
 * View Transform API (native per-precision dispatch + bulk loop)
 * ---------------------------------------------------------------- */
int ALWAN_CORE_FNLIT(alwan_view_transform_apply)(ALWAN_CORE_T *rgb_out, size_t out_stride, ALWAN_CORE_T const *rgb_in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx) {
    (void)ctx;  /* Unused for stateless transforms */

    if (!rgb_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    void (*transform_fn)(ALWAN_CORE_T const *, ALWAN_CORE_T *) = NULL;

    switch (vt) {
        case ALWAN_VIEW_ACES_REC709:        transform_fn = ALWAN_CORE_FN(aces_rec709_transform); break;
        case ALWAN_VIEW_AGX:                transform_fn = ALWAN_CORE_FN(agx_base_view); break;
        case ALWAN_VIEW_AGX_PUNCHY:         transform_fn = ALWAN_CORE_FN(agx_punchy_transform); break;
        case ALWAN_VIEW_AGX_GOLDEN:         transform_fn = ALWAN_CORE_FN(agx_golden_transform); break;
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

    /* Apply view transform to RGB triplets (strides are in bytes). */
    for (size_t i = 0; i < count; i++) {
        ALWAN_CORE_T const *in_ptr = (ALWAN_CORE_T const *)((char const *)rgb_in + i * in_stride);
        ALWAN_CORE_T *out_ptr = (ALWAN_CORE_T *)((char *)rgb_out + i * out_stride);
        transform_fn(in_ptr, out_ptr);
    }

    return ALWAN_OK;
}
