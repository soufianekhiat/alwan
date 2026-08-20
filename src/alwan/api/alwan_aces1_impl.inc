/* No include guard. Native single-precision ACES 1.x output transform.
 *
 * The f64 path remains hand-written in alwan_aces_ff.c (the OCIO-validated
 * reference); this template generates the matching NATIVE f32 path so the
 * public f32 entry points compute in float rather than widening to double.
 * Shared constant data (AP0/AP1/XYZ matrices, C9 spline params, OCIO curve
 * knots) lives in the enclosing TU at f64 precision and is read here with
 * casts to ALWAN_CORE_T.
 *
 * TODO(cleanup): migrate the f64 path in alwan_aces_ff.c into this template
 * (include twice) to remove the f64/f32 duplication. Kept separate for now
 * to avoid disturbing the validated f64 reference.
 *
 * Requires in the enclosing TU (precision-independent):
 *   the f64 data arrays and aces1_c9_params struct, the global g_aces_interp,
 *   ACES1_RRT_GLOW_GAIN / REDMOD10_* / REDMOD_NOISE_LIMIT macros, and the
 *   templated cores aces1_*_v / aces_*_oetf_v / aces_*_eotf_v / alwan_mat3_mulv_v.
 */

/* CORE matrix * CORE rgb -> CORE rgb (native precision data + multiply). */
static ALWAN_CORE_RGB ALWAN_CORE_FN(aces1_mat3)(ALWAN_CORE_T const *m, ALWAN_CORE_RGB v) {
    ALWAN_CORE_RGB o;
    o.r = m[0]*v.r + m[1]*v.g + m[2]*v.b;
    o.g = m[3]*v.r + m[4]*v.g + m[5]*v.b;
    o.b = m[6]*v.r + m[7]*v.g + m[8]*v.b;
    return o;
}

static ALWAN_CORE_T ALWAN_CORE_FN(aces1_hermite_quad)(ALWAN_CORE_T x,
        ALWAN_CORE_T const *px, ALWAN_CORE_T const *py, ALWAN_CORE_T const *ps, int n) {
    if (x <= px[0])
        return py[0] + ps[0] * (x - px[0]);
    if (x >= px[n-1]) {
        ALWAN_CORE_T dx = px[n-1] - px[n-2];
        ALWAN_CORE_T A = (ps[n-1] - ps[n-2]) / (ALWAN_CORE_LITERAL(2.0) * dx);
        ALWAN_CORE_T end_slope = ALWAN_CORE_LITERAL(2.0) * A * dx + ps[n-2];
        return py[n-1] + end_slope * (x - px[n-1]);
    }
    int i = 0;
    while (i < n - 2 && x >= px[i + 1]) i++;
    ALWAN_CORE_T dx = px[i+1] - px[i];
    ALWAN_CORE_T t = x - px[i];
    ALWAN_CORE_T A = (ps[i+1] - ps[i]) / (ALWAN_CORE_LITERAL(2.0) * dx);
    ALWAN_CORE_T B = ps[i];
    ALWAN_CORE_T C = py[i];
    return (A * t + B) * t + C;
}

static ALWAN_CORE_T ALWAN_CORE_FN(aces1_rrt_hermite)(ALWAN_CORE_T x) {
    static const ALWAN_CORE_T px[7] = { -5.26017743, -3.75502745, -2.24987747, -0.74472749, 1.06145248, 2.86763245, 4.67381243 };
    static const ALWAN_CORE_T py[7] = { -4.0, -3.57868829, -1.82131329, 0.68124124, 2.87457742, 3.83406206, 4.0 };
    static const ALWAN_CORE_T ps[7] = { 0.0, 0.55982688, 1.77532247, 1.55, 0.8787017, 0.18374463, 0.0 };
    ALWAN_CORE_T lx = ALWAN_CORE_LOG10(ALWAN_CORE_MAX(x, ALWAN_CORE_LITERAL(1e-10)));
    ALWAN_CORE_T ly = ALWAN_CORE_FN(aces1_hermite_quad)(lx, px, py, ps, 7);
    return ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), ly);
}

static ALWAN_CORE_T ALWAN_CORE_FN(aces1_odt48_hermite)(ALWAN_CORE_T x) {
    static const ALWAN_CORE_T px[15] = { -2.54062362, -2.08035721, -1.6200908, -1.15982439, -0.69955799, -0.23929158, 0.22097483, 0.68124124, 1.01284632, 1.3444514, 1.67605648, 2.00766156, 2.33926665, 2.67087173, 3.00247681 };
    static const ALWAN_CORE_T py[15] = { -1.69897, -1.588435, -1.3535, -1.04695, -0.6564, -0.22141, 0.22814402, 0.68124124, 0.99142189, 1.258, 1.44995, 1.5591, 1.6226, 1.66065457, 1.68124124 };
    static const ALWAN_CORE_T ps[15] = { 0.0, 0.4803088, 0.5405565, 0.79149813, 0.9055625, 0.98460368, 0.96884766, 1.0, 0.87078346, 0.73702127, 0.42068113, 0.23763206, 0.14535362, 0.08416378, 0.04 };
    ALWAN_CORE_T lx = ALWAN_CORE_LOG10(ALWAN_CORE_MAX(x, ALWAN_CORE_LITERAL(1e-10)));
    ALWAN_CORE_T ly = ALWAN_CORE_FN(aces1_hermite_quad)(lx, px, py, ps, 15);
    ALWAN_CORE_T nits = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), ly);
    return (nits - ALWAN_CORE_LITERAL(0.02)) / (ALWAN_CORE_LITERAL(48.0) - ALWAN_CORE_LITERAL(0.02));
}

static ALWAN_CORE_T ALWAN_CORE_FN(aces1_c9_raw)(ALWAN_CORE_T oces, const ALWAN_CORE_FNLIT(aces1_c9_params) *p) {
    ALWAN_CORE_T lx = ALWAN_CORE_LOG10(ALWAN_CORE_MAX(oces, ALWAN_CORE_LITERAL(1e-10)));
    ALWAN_CORE_T log_min = ALWAN_CORE_LOG10(p->min_x);
    ALWAN_CORE_T log_mid = ALWAN_CORE_LOG10(p->mid_x);
    ALWAN_CORE_T log_max = ALWAN_CORE_LOG10(p->max_x);
    ALWAN_CORE_T ly;
    if (lx <= log_min) {
        ly = ALWAN_CORE_LOG10(p->min_y) + p->slope_low * (lx - log_min);
    } else if (lx >= log_max) {
        ly = ALWAN_CORE_LOG10(p->max_y) + p->slope_high * (lx - log_max);
    } else {
        ALWAN_CORE_T const *co; ALWAN_CORE_T ks, ke; int nk = 8;
        if (lx < log_mid) { co = p->coefsLow;  ks = log_min; ke = log_mid; }
        else              { co = p->coefsHigh; ks = log_mid; ke = log_max; }
        ALWAN_CORE_T kc = (ALWAN_CORE_T)(nk - 1) * (lx - ks) / (ke - ks);
        int jj = (int)kc;
        if (jj < 0) jj = 0;
        if (jj > nk - 2) jj = nk - 2;
        ALWAN_CORE_T tt = kc - (ALWAN_CORE_T)jj;
        ALWAN_CORE_T f0 = co[jj], f1 = co[jj+1], f2 = co[jj+2];
        ly = ALWAN_CORE_LITERAL(0.5) * tt * tt * (f0 - ALWAN_CORE_LITERAL(2.0) * f1 + f2)
           + tt * (-f0 + f1) + ALWAN_CORE_LITERAL(0.5) * (f0 + f1);
    }
    return ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), ly);
}

static ALWAN_CORE_T ALWAN_CORE_FN(aces1_ocio_curve)(ALWAN_CORE_T log_in, const ALWAN_CORE_T *knots, int n_knots) {
    if (log_in <= knots[0]) return knots[1];
    if (log_in >= knots[(n_knots - 1) * 3]) return knots[(n_knots - 1) * 3 + 1];
    int seg = 0;
    for (int i = 1; i < n_knots; i++) {
        if (log_in < knots[i * 3]) { seg = i - 1; break; }
    }
    ALWAN_CORE_T x0 = knots[seg*3],     y0 = knots[seg*3+1],     s0 = knots[seg*3+2];
    ALWAN_CORE_T x1 = knots[(seg+1)*3], y1 = knots[(seg+1)*3+1], s1 = knots[(seg+1)*3+2];
    ALWAN_CORE_T dx = x1 - x0;
    ALWAN_CORE_T t = (log_in - x0) / dx;
    ALWAN_CORE_T t2 = t * t, t3 = t2 * t;
    ALWAN_CORE_T h00 = ALWAN_CORE_LITERAL(2.0)*t3 - ALWAN_CORE_LITERAL(3.0)*t2 + ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_T h10 = t3 - ALWAN_CORE_LITERAL(2.0)*t2 + t;
    ALWAN_CORE_T h01 = ALWAN_CORE_LITERAL(-2.0)*t3 + ALWAN_CORE_LITERAL(3.0)*t2;
    ALWAN_CORE_T h11 = t3 - t2;
    return h00 * y0 + h10 * (s0 * dx) + h01 * y1 + h11 * (s1 * dx);
}

static ALWAN_CORE_T ALWAN_CORE_FN(aces1_ocio_sdr)(ALWAN_CORE_T scene_val) {
    ALWAN_CORE_T log_scene = ALWAN_CORE_LOG10(ALWAN_CORE_MAX(scene_val, ALWAN_CORE_LITERAL(1e-10)));
    ALWAN_CORE_T log_oces = ALWAN_CORE_FN(aces1_ocio_curve)(log_scene, ALWAN_CORE_FNLIT(ocio_sdr_c5), 7);
    ALWAN_CORE_T log_display = ALWAN_CORE_FN(aces1_ocio_curve)(log_oces, ALWAN_CORE_FNLIT(ocio_sdr_c9), 15);
    ALWAN_CORE_T display_nits = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), log_display);
    return (display_nits - ALWAN_CORE_LITERAL(0.02)) / (ALWAN_CORE_LITERAL(48.0) - ALWAN_CORE_LITERAL(0.02));
}

static ALWAN_CORE_T ALWAN_CORE_FN(aces1_ocio_hdr1000)(ALWAN_CORE_T scene_val) {
    ALWAN_CORE_T log_scene = ALWAN_CORE_LOG10(ALWAN_CORE_MAX(scene_val, ALWAN_CORE_LITERAL(1e-10)));
    ALWAN_CORE_T log_display = ALWAN_CORE_FN(aces1_ocio_curve)(log_scene, ALWAN_CORE_FNLIT(ocio_hdr1000), 7);
    ALWAN_CORE_T display_nits = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), log_display);
    return (display_nits - ALWAN_CORE_LITERAL(0.0001)) / (ALWAN_CORE_LITERAL(1000.0) - ALWAN_CORE_LITERAL(0.0001));
}

static ALWAN_CORE_T ALWAN_CORE_FN(aces1_spline_c9)(ALWAN_CORE_T oces, const ALWAN_CORE_FNLIT(aces1_c9_params) *p) {
    ALWAN_CORE_T lx = ALWAN_CORE_LOG10(ALWAN_CORE_MAX(oces, ALWAN_CORE_LITERAL(1e-10)));
    ALWAN_CORE_T log_min = ALWAN_CORE_LOG10(p->min_x);
    ALWAN_CORE_T log_mid = ALWAN_CORE_LOG10(p->mid_x);
    ALWAN_CORE_T log_max = ALWAN_CORE_LOG10(p->max_x);
    ALWAN_CORE_T ly;
    if (lx <= log_min) {
        ly = ALWAN_CORE_LOG10(p->min_y) + p->slope_low * (lx - log_min);
    } else if (lx >= log_max) {
        ly = ALWAN_CORE_LOG10(p->max_y) + p->slope_high * (lx - log_max);
    } else {
        ALWAN_CORE_T const *co; ALWAN_CORE_T ks, ke; int nk = 8;
        if (lx < log_mid) { co = p->coefsLow;  ks = log_min; ke = log_mid; }
        else              { co = p->coefsHigh; ks = log_mid; ke = log_max; }
        ALWAN_CORE_T kc = (ALWAN_CORE_T)(nk - 1) * (lx - ks) / (ke - ks);
        int jj = (int)kc;
        if (jj < 0) jj = 0;
        if (jj > nk - 2) jj = nk - 2;
        ALWAN_CORE_T tt = kc - (ALWAN_CORE_T)jj;
        ALWAN_CORE_T f0 = co[jj], f1 = co[jj+1], f2 = co[jj+2];
        ly = ALWAN_CORE_LITERAL(0.5) * tt * tt * (f0 - ALWAN_CORE_LITERAL(2.0) * f1 + f2)
           + tt * (-f0 + f1) + ALWAN_CORE_LITERAL(0.5) * (f0 + f1);
    }
    ALWAN_CORE_T display_nits = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), ly);
    return (display_nits - p->min_y) / (p->max_y - p->min_y);
}

/* ---- Forward ---- */
alwan_status ALWAN_CORE_FNLIT(alwan_aces1_output_transform)(ALWAN_CORE_RGB *rgb_out,
        ALWAN_CORE_RGB const *rgb_in, alwan_aces1_output output) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    if (output < 0 || output >= ALWAN_ACES1_OUT_COUNT) return ALWAN_E_INVALID;

    ALWAN_CORE_RGB ap0_mod = *rgb_in, ap1, rrt, xyz, d65, display;

    /* Glow10 (AP0) */
    {
        ALWAN_CORE_T sat = ALWAN_CORE_FNVLIT(aces1_saturation)(ap0_mod.r, ap0_mod.g, ap0_mod.b);
        ALWAN_CORE_T chroma = ALWAN_CORE_SQRT(ALWAN_CORE_ABS(
            ap0_mod.b * (ap0_mod.b - ap0_mod.g) +
            ap0_mod.g * (ap0_mod.g - ap0_mod.r) +
            ap0_mod.r * (ap0_mod.r - ap0_mod.b)));
        ALWAN_CORE_T ycIn = (ap0_mod.r + ap0_mod.g + ap0_mod.b + ALWAN_CORE_LITERAL(1.75) * chroma) / ALWAN_CORE_LITERAL(3.0);
        ALWAN_CORE_T s = ALWAN_CORE_FNVLIT(aces_sigmoid_shaper)((sat - ALWAN_CORE_LITERAL(0.4)) / ALWAN_CORE_LITERAL(0.2));
        ALWAN_CORE_T glowGainIn = (ALWAN_CORE_T)ACES1_RRT_GLOW_GAIN * s;
        ALWAN_CORE_T glowMid = ALWAN_CORE_LITERAL(0.08);
        ALWAN_CORE_T glow;
        if (ycIn <= ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(3.0) * glowMid) glow = glowGainIn;
        else if (ycIn >= ALWAN_CORE_LITERAL(2.0) * glowMid) glow = ALWAN_CORE_LITERAL(0.0);
        else glow = glowGainIn * (glowMid / ycIn - ALWAN_CORE_LITERAL(0.5));
        ap0_mod.r *= (ALWAN_CORE_LITERAL(1.0) + glow);
        ap0_mod.g *= (ALWAN_CORE_LITERAL(1.0) + glow);
        ap0_mod.b *= (ALWAN_CORE_LITERAL(1.0) + glow);
    }
    /* RedMod10 (AP0) */
    {
        ALWAN_CORE_T hueWeight = ALWAN_CORE_FNVLIT(aces_calc_hue_weight)(ap0_mod.r, ap0_mod.g, ap0_mod.b, (ALWAN_CORE_T)REDMOD10_INV_WIDTH);
        if (hueWeight > ALWAN_CORE_LITERAL(0.0)) {
            ALWAN_CORE_T f_S = ALWAN_CORE_FNVLIT(aces_calc_sat_weight)(ap0_mod.r, ap0_mod.g, ap0_mod.b, (ALWAN_CORE_T)REDMOD_NOISE_LIMIT);
            ap0_mod.r = ap0_mod.r + hueWeight * f_S * ((ALWAN_CORE_T)REDMOD10_PIVOT - ap0_mod.r) * (ALWAN_CORE_LITERAL(1.0) - (ALWAN_CORE_T)REDMOD10_SCALE);
        }
    }
    ap0_mod.r = ALWAN_CORE_MAX(ap0_mod.r, ALWAN_CORE_LITERAL(0.0));
    ap0_mod.g = ALWAN_CORE_MAX(ap0_mod.g, ALWAN_CORE_LITERAL(0.0));
    ap0_mod.b = ALWAN_CORE_MAX(ap0_mod.b, ALWAN_CORE_LITERAL(0.0));

    ap1 = ALWAN_CORE_FN(aces1_mat3)(ALWAN_CORE_FNLIT(ACES1_AP0_TO_AP1), ap0_mod);
    ap1.r = ALWAN_CORE_MAX(ap1.r, ALWAN_CORE_LITERAL(0.0));
    ap1.g = ALWAN_CORE_MAX(ap1.g, ALWAN_CORE_LITERAL(0.0));
    ap1.b = ALWAN_CORE_MAX(ap1.b, ALWAN_CORE_LITERAL(0.0));

    /* RRT desat + tone curve */
    {
        ALWAN_CORE_T rl = ALWAN_CORE_LITERAL(0.2722287168), gl = ALWAN_CORE_LITERAL(0.6740817658), bl = ALWAN_CORE_LITERAL(0.0536895174);
        ALWAN_CORE_T luma = rl*ap1.r + gl*ap1.g + bl*ap1.b;
        rrt.r = luma + ALWAN_CORE_LITERAL(0.96) * (ap1.r - luma);
        rrt.g = luma + ALWAN_CORE_LITERAL(0.96) * (ap1.g - luma);
        rrt.b = luma + ALWAN_CORE_LITERAL(0.96) * (ap1.b - luma);

        int is_hdr_1000 = (output == ALWAN_ACES1_OUT_REC2020_1000NIT_PQ);
        if (g_aces_interp == ALWAN_ACES_INTERP_OCIO) {
            if (is_hdr_1000) {
                rrt.r = ALWAN_CORE_FN(aces1_ocio_hdr1000)(rrt.r);
                rrt.g = ALWAN_CORE_FN(aces1_ocio_hdr1000)(rrt.g);
                rrt.b = ALWAN_CORE_FN(aces1_ocio_hdr1000)(rrt.b);
            } else {
                rrt.r = ALWAN_CORE_FN(aces1_ocio_sdr)(rrt.r);
                rrt.g = ALWAN_CORE_FN(aces1_ocio_sdr)(rrt.g);
                rrt.b = ALWAN_CORE_FN(aces1_ocio_sdr)(rrt.b);
            }
        } else {
            if (g_aces_interp == ALWAN_ACES_INTERP_HERMITE) {
                rrt.r = ALWAN_CORE_FN(aces1_rrt_hermite)(rrt.r);
                rrt.g = ALWAN_CORE_FN(aces1_rrt_hermite)(rrt.g);
                rrt.b = ALWAN_CORE_FN(aces1_rrt_hermite)(rrt.b);
            } else {
                rrt.r = ALWAN_CORE_FNVLIT(aces1_segmented_spline_c5)(rrt.r);
                rrt.g = ALWAN_CORE_FNVLIT(aces1_segmented_spline_c5)(rrt.g);
                rrt.b = ALWAN_CORE_FNVLIT(aces1_segmented_spline_c5)(rrt.b);
            }
            const ALWAN_CORE_FNLIT(aces1_c9_params) *c9p;
            switch (output) {
                case ALWAN_ACES1_OUT_REC2020_1000NIT_PQ: c9p = &ALWAN_CORE_FNLIT(c9_1000nit); break;
                case ALWAN_ACES1_OUT_REC2020_2000NIT_PQ: c9p = &ALWAN_CORE_FNLIT(c9_2000nit); break;
                case ALWAN_ACES1_OUT_REC2020_4000NIT_PQ: c9p = &ALWAN_CORE_FNLIT(c9_4000nit); break;
                default: c9p = &ALWAN_CORE_FNLIT(c9_48nit); break;
            }
            if (g_aces_interp == ALWAN_ACES_INTERP_HERMITE && c9p == &ALWAN_CORE_FNLIT(c9_48nit)) {
                rrt.r = ALWAN_CORE_FN(aces1_odt48_hermite)(rrt.r);
                rrt.g = ALWAN_CORE_FN(aces1_odt48_hermite)(rrt.g);
                rrt.b = ALWAN_CORE_FN(aces1_odt48_hermite)(rrt.b);
            } else {
                rrt.r = ALWAN_CORE_FN(aces1_spline_c9)(rrt.r, c9p);
                rrt.g = ALWAN_CORE_FN(aces1_spline_c9)(rrt.g, c9p);
                rrt.b = ALWAN_CORE_FN(aces1_spline_c9)(rrt.b, c9p);
            }
        }
    }

    /* dimSurround + ODT desat (100-nit video only) */
    {
        int is_cinema = (output == ALWAN_ACES1_OUT_P3DCI_48NIT || output == ALWAN_ACES1_OUT_P3D60_48NIT ||
                         output == ALWAN_ACES1_OUT_P3D65_48NIT || output == ALWAN_ACES1_OUT_DCDM_48NIT);
        int is_hdr_pq = (output == ALWAN_ACES1_OUT_REC2020_1000NIT_PQ || output == ALWAN_ACES1_OUT_REC2020_2000NIT_PQ ||
                         output == ALWAN_ACES1_OUT_REC2020_4000NIT_PQ);
        if (!is_cinema && !is_hdr_pq) {
            ALWAN_CORE_T rl = ALWAN_CORE_LITERAL(0.2722287168), gl = ALWAN_CORE_LITERAL(0.6740817658), bl = ALWAN_CORE_LITERAL(0.0536895174);
            ALWAN_CORE_T Y = rl * rrt.r + gl * rrt.g + bl * rrt.b;
            if (Y > ALWAN_CORE_LITERAL(1e-10)) {
                ALWAN_CORE_T scale = ALWAN_CORE_POW(Y, ALWAN_CORE_LITERAL(-0.0189));
                rrt.r *= scale; rrt.g *= scale; rrt.b *= scale;
            }
            ALWAN_CORE_T s = ALWAN_CORE_LITERAL(0.93);
            ALWAN_CORE_T odt_luma = rl * rrt.r + gl * rrt.g + bl * rrt.b;
            rrt.r = odt_luma + s * (rrt.r - odt_luma);
            rrt.g = odt_luma + s * (rrt.g - odt_luma);
            rrt.b = odt_luma + s * (rrt.b - odt_luma);
        }
    }

    xyz = ALWAN_CORE_FN(aces1_mat3)(ALWAN_CORE_FNLIT(ACES1_AP1_TO_XYZ_D60), rrt);
    int needs_d65 = (output != ALWAN_ACES1_OUT_SRGB_D60_100NIT && output != ALWAN_ACES1_OUT_P3D60_48NIT);
    d65 = needs_d65 ? ALWAN_CORE_FN(aces1_mat3)(ALWAN_CORE_FNLIT(ACES1_D60_TO_D65), xyz) : xyz;

    ALWAN_CORE_T const *xyz_to_display = NULL;
    ALWAN_CORE_T peak_nits = ALWAN_CORE_LITERAL(100.0);
    int use_pq = 0, use_srgb = 0, use_gamma26 = 0;
    switch (output) {
        case ALWAN_ACES1_OUT_REC709_100NIT:      xyz_to_display = ALWAN_CORE_FNLIT(ACES1_XYZ_TO_REC709); break;
        case ALWAN_ACES1_OUT_SRGB_100NIT:
        case ALWAN_ACES1_OUT_SRGB_D60_100NIT:    xyz_to_display = ALWAN_CORE_FNLIT(ACES1_XYZ_TO_REC709); use_srgb = 1; break;
        case ALWAN_ACES1_OUT_P3DCI_48NIT:
        case ALWAN_ACES1_OUT_P3D60_48NIT:
        case ALWAN_ACES1_OUT_P3D65_48NIT:        xyz_to_display = ALWAN_CORE_FNLIT(ACES1_XYZ_TO_P3D65); use_gamma26 = 1; break;
        case ALWAN_ACES1_OUT_P3D65_100NIT:       xyz_to_display = ALWAN_CORE_FNLIT(ACES1_XYZ_TO_P3D65); use_srgb = 1; break;
        case ALWAN_ACES1_OUT_REC2020_100NIT:     xyz_to_display = ALWAN_CORE_FNLIT(ACES1_XYZ_TO_REC2020); break;
        case ALWAN_ACES1_OUT_REC2020_1000NIT_PQ: xyz_to_display = ALWAN_CORE_FNLIT(ACES1_XYZ_TO_REC2020); use_pq = 1; peak_nits = ALWAN_CORE_LITERAL(1000.0); break;
        case ALWAN_ACES1_OUT_REC2020_2000NIT_PQ: xyz_to_display = ALWAN_CORE_FNLIT(ACES1_XYZ_TO_REC2020); use_pq = 1; peak_nits = ALWAN_CORE_LITERAL(2000.0); break;
        case ALWAN_ACES1_OUT_REC2020_4000NIT_PQ: xyz_to_display = ALWAN_CORE_FNLIT(ACES1_XYZ_TO_REC2020); use_pq = 1; peak_nits = ALWAN_CORE_LITERAL(4000.0); break;
        case ALWAN_ACES1_OUT_DCDM_48NIT:
            display = d65;
            display.r = ALWAN_CORE_FNVLIT(aces_gamma26_oetf)(display.r * ALWAN_CORE_LITERAL(48.0) / ALWAN_CORE_LITERAL(52.37));
            display.g = ALWAN_CORE_FNVLIT(aces_gamma26_oetf)(display.g * ALWAN_CORE_LITERAL(48.0) / ALWAN_CORE_LITERAL(52.37));
            display.b = ALWAN_CORE_FNVLIT(aces_gamma26_oetf)(display.b * ALWAN_CORE_LITERAL(48.0) / ALWAN_CORE_LITERAL(52.37));
            *rgb_out = display;
            return ALWAN_OK;
        default: return ALWAN_E_INVALID;
    }

    display = ALWAN_CORE_FN(aces1_mat3)(xyz_to_display, d65);
    display.r = ALWAN_CORE_MIN(ALWAN_CORE_MAX(display.r, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(1.0));
    display.g = ALWAN_CORE_MIN(ALWAN_CORE_MAX(display.g, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(1.0));
    display.b = ALWAN_CORE_MIN(ALWAN_CORE_MAX(display.b, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(1.0));

    if (use_pq) {
        rgb_out->r = ALWAN_CORE_FNVLIT(aces_pq_oetf)(display.r, peak_nits);
        rgb_out->g = ALWAN_CORE_FNVLIT(aces_pq_oetf)(display.g, peak_nits);
        rgb_out->b = ALWAN_CORE_FNVLIT(aces_pq_oetf)(display.b, peak_nits);
    } else if (use_srgb) {
        rgb_out->r = ALWAN_CORE_FNVLIT(aces_srgb_oetf)(display.r);
        rgb_out->g = ALWAN_CORE_FNVLIT(aces_srgb_oetf)(display.g);
        rgb_out->b = ALWAN_CORE_FNVLIT(aces_srgb_oetf)(display.b);
    } else if (use_gamma26) {
        rgb_out->r = ALWAN_CORE_FNVLIT(aces_gamma26_oetf)(display.r);
        rgb_out->g = ALWAN_CORE_FNVLIT(aces_gamma26_oetf)(display.g);
        rgb_out->b = ALWAN_CORE_FNVLIT(aces_gamma26_oetf)(display.b);
    } else {
        rgb_out->r = ALWAN_CORE_FNVLIT(aces_bt1886_oetf)(display.r);
        rgb_out->g = ALWAN_CORE_FNVLIT(aces_bt1886_oetf)(display.g);
        rgb_out->b = ALWAN_CORE_FNVLIT(aces_bt1886_oetf)(display.b);
    }
    return ALWAN_OK;
}
