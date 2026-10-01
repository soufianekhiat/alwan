/* No include guard -- included twice (f32 and f64 passes) */

/* Every sum here is formed in double whatever the pass, through the f64 core, and the
 * result rounded to the pass's precision once: the f32 forms take f32 maps and hand back
 * f32 tables without losing the accumulation to single precision. */

#define ALWAN_IBL_VEC3 ALWAN_CORE_VEC3

/* The basis values of every (l, m) up to band at the unit direction d, in double. */
static void ALWAN_CORE_FNLIT(alwan__sh_basis_all)(double *y, int band, double dx, double dy, double dz) {
    int l, m;
    for (l = 0; l <= band; l++) {
        for (m = -l; m <= l; m++) y[l * l + l + m] = alwan_sh_basis_f64_v(l, m, dx, dy, dz);
    }
}

alwan_status ALWAN_CORE_FNLIT(alwan_sh_project_equirect)(ALWAN_CORE_T *coeffs_out, int band, ALWAN_CORE_T const *map,
                                                        size_t row_stride, size_t width, size_t height, size_t channels) {
    double acc[ALWAN__SH_MAX_COEFFS * ALWAN__SH_MAX_CHANNELS];
    double s0[ALWAN__SH_MAX_CHANNELS];
    double sc[(ALWAN_SH_MAX_BAND + 1) * ALWAN__SH_MAX_CHANNELS], ss[(ALWAN_SH_MAX_BAND + 1) * ALWAN__SH_MAX_CHANNELS];
    double const two_pi = 6.283185307179586476925;
    double const pi = 3.141592653589793238463;
    size_t n, i, j, c;
    int l, m;
    if (!coeffs_out || !map || band < 0 || band > ALWAN_SH_MAX_BAND || width == 0 || height == 0 || channels == 0 ||
        channels > ALWAN__SH_MAX_CHANNELS) return ALWAN_E_INVALID;
    if (row_stride < width * channels * sizeof(ALWAN_CORE_T)) return ALWAN_E_INVALID;
    n = (size_t)(band + 1) * (size_t)(band + 1);
    memset(acc, 0, sizeof(acc));
    for (j = 0; j < height; j++) {
        ALWAN_CORE_T const *row = (ALWAN_CORE_T const *)((unsigned char const *)map + j * row_stride);
        double const theta = pi * ((double)j + 0.5) / (double)height;
        double const ct = ALWAN_COS(theta), st = ALWAN_SIN(theta);
        /* the row's exact solid angle per pixel: (2 pi / width)(cos theta_top - cos theta_bottom) */
        double const omega = two_pi / (double)width *
                             (ALWAN_COS(pi * (double)j / (double)height) - ALWAN_COS(pi * (double)(j + 1) / (double)height));
        double stm;
        memset(s0, 0, sizeof(s0));
        memset(sc, 0, sizeof(sc));
        memset(ss, 0, sizeof(ss));
        for (i = 0; i < width; i++) {
            double const phi = two_pi * ((double)i + 0.5) / (double)width;
            double const cp = ALWAN_COS(phi), sp = ALWAN_SIN(phi);
            double cm = 1.0, sm = 0.0;     /* cos(m phi), sin(m phi), by rotation */
            for (c = 0; c < channels; c++) {
                double const v = (double)row[i * channels + c];
                if (!(v >= -1.7976931348623157e308 && v <= 1.7976931348623157e308)) return ALWAN_E_INVALID;
                s0[c] += v;
            }
            for (m = 1; m <= band; m++) {
                double const t = cm * cp - sm * sp;
                sm = sm * cp + cm * sp;
                cm = t;
                for (c = 0; c < channels; c++) {
                    double const v = (double)row[i * channels + c];
                    sc[(size_t)m * channels + c] += v * cm;
                    ss[(size_t)m * channels + c] += v * sm;
                }
            }
        }
        stm = 1.0;
        for (m = 0; m <= band; m++) {
            for (l = m; l <= band; l++) {
                double const f = alwan_sh_theta_part_f64_v(l, m, ct) * stm * omega;
                for (c = 0; c < channels; c++) {
                    if (m == 0) {
                        acc[(size_t)(l * l + l) * channels + c] += f * s0[c];
                    } else {
                        acc[(size_t)(l * l + l + m) * channels + c] += f * sc[(size_t)m * channels + c];
                        acc[(size_t)(l * l + l - m) * channels + c] += f * ss[(size_t)m * channels + c];
                    }
                }
            }
            stm *= st;
        }
    }
    for (i = 0; i < n * channels; i++) {
        if (!(acc[i] >= -1.7976931348623157e308 && acc[i] <= 1.7976931348623157e308)) return ALWAN_E_RANGE;
        coeffs_out[i] = (ALWAN_CORE_T)acc[i];
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_sh_evaluate)(ALWAN_CORE_T *out, ALWAN_IBL_VEC3 const *dir, ALWAN_CORE_T const *coeffs,
                                                int band, size_t channels) {
    double y[ALWAN__SH_MAX_COEFFS];
    double dx, dy, dz, len;
    size_t c, k, n;
    if (!out || !dir || !coeffs || band < 0 || band > ALWAN_SH_MAX_BAND || channels == 0) return ALWAN_E_INVALID;
    dx = (double)dir->v[0]; dy = (double)dir->v[1]; dz = (double)dir->v[2];
    len = ALWAN_SQRT(dx * dx + dy * dy + dz * dz);
    if (!(len > 0.0 && len <= 1.7976931348623157e308)) return ALWAN_E_INVALID;
    ALWAN_CORE_FNLIT(alwan__sh_basis_all)(y, band, dx / len, dy / len, dz / len);
    n = (size_t)(band + 1) * (size_t)(band + 1);
    for (c = 0; c < channels; c++) {
        double s = 0.0;
        for (k = 0; k < n; k++) s += (double)coeffs[k * channels + c] * y[k];
        out[c] = (ALWAN_CORE_T)s;
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_sh_render_equirect)(ALWAN_CORE_T *map_out, size_t row_stride, size_t width,
                                                       size_t height, ALWAN_CORE_T const *coeffs, int band,
                                                       size_t channels) {
    double y[ALWAN__SH_MAX_COEFFS];
    double const two_pi = 6.283185307179586476925;
    double const pi = 3.141592653589793238463;
    size_t i, j, c, k, n;
    if (!map_out || !coeffs || band < 0 || band > ALWAN_SH_MAX_BAND || width == 0 || height == 0 || channels == 0)
        return ALWAN_E_INVALID;
    if (row_stride < width * channels * sizeof(ALWAN_CORE_T)) return ALWAN_E_INVALID;
    n = (size_t)(band + 1) * (size_t)(band + 1);
    for (j = 0; j < height; j++) {
        ALWAN_CORE_T *row = (ALWAN_CORE_T *)((unsigned char *)map_out + j * row_stride);
        double const theta = pi * ((double)j + 0.5) / (double)height;
        double const ct = ALWAN_COS(theta), st = ALWAN_SIN(theta);
        for (i = 0; i < width; i++) {
            double const phi = two_pi * ((double)i + 0.5) / (double)width;
            ALWAN_CORE_FNLIT(alwan__sh_basis_all)(y, band, st * ALWAN_COS(phi), ct, st * ALWAN_SIN(phi));
            for (c = 0; c < channels; c++) {
                double s = 0.0;
                for (k = 0; k < n; k++) s += (double)coeffs[k * channels + c] * y[k];
                row[i * channels + c] = (ALWAN_CORE_T)s;
            }
        }
    }
    return ALWAN_OK;
}

/* The window width a caller asked for: 0 for band + 1, else positive and finite. */
static int ALWAN_CORE_FNLIT(alwan__sh_window_ok)(alwan_sh_window window, double width) {
    if (window != ALWAN_SH_WINDOW_NONE && window != ALWAN_SH_WINDOW_HANNING && window != ALWAN_SH_WINDOW_LANCZOS) return 0;
    return width == 0.0 || (width > 0.0 && width <= 1.0e6);
}

alwan_status ALWAN_CORE_FNLIT(alwan_sh_irradiance_coefficients)(ALWAN_CORE_T *out, ALWAN_CORE_T const *coeffs, int band,
                                                               size_t channels, alwan_sh_window window,
                                                               ALWAN_CORE_T window_width) {
    double w;
    size_t c;
    int l, m;
    if (!out || !coeffs || band < 0 || band > ALWAN_SH_MAX_BAND || channels == 0) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FNLIT(alwan__sh_window_ok)(window, (double)window_width)) return ALWAN_E_INVALID;
    w = window_width == ALWAN_CORE_LITERAL(0.0) ? (double)(band + 1) : (double)window_width;
    for (l = 0; l <= band; l++) {
        double const f = alwan_sh_cosine_lobe_f64_v(l) * alwan_sh_window_f64_v(l, (int)window, w);
        for (m = -l; m <= l; m++) {
            size_t const k = (size_t)(l * l + l + m);
            for (c = 0; c < channels; c++) out[k * channels + c] = (ALWAN_CORE_T)(f * (double)coeffs[k * channels + c]);
        }
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_sh_irradiance_matrix)(ALWAN_CORE_T *out, ALWAN_CORE_T const *coeffs, size_t channels,
                                                         alwan_sh_window window, ALWAN_CORE_T window_width) {
    double const pi = 3.141592653589793238463;
    double k0, k1, ka, kb, kc, w, a0, a1, a2;
    size_t c;
    if (!out || !coeffs || channels == 0) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FNLIT(alwan__sh_window_ok)(window, (double)window_width)) return ALWAN_E_INVALID;
    w = window_width == ALWAN_CORE_LITERAL(0.0) ? 3.0 : (double)window_width;
    /* the basis constants: Y_00 = k0, Y_1m = k1 (d.z, d.y, d.x), Y_2,-2 = ka d.x d.z,
     * Y_2,-1 = ka d.z d.y, Y_20 = kc (3 d.y^2 - 1), Y_21 = ka d.x d.y, Y_22 = kb (d.x^2 - d.z^2) */
    k0 = 0.5 / ALWAN_SQRT(pi);
    k1 = ALWAN_SQRT(3.0 / (4.0 * pi));
    ka = 0.5 * ALWAN_SQRT(15.0 / pi);
    kb = 0.25 * ALWAN_SQRT(15.0 / pi);
    kc = 0.25 * ALWAN_SQRT(5.0 / pi);
    a0 = alwan_sh_cosine_lobe_f64_v(0) * alwan_sh_window_f64_v(0, (int)window, w);
    a1 = alwan_sh_cosine_lobe_f64_v(1) * alwan_sh_window_f64_v(1, (int)window, w);
    a2 = alwan_sh_cosine_lobe_f64_v(2) * alwan_sh_window_f64_v(2, (int)window, w);
    for (c = 0; c < channels; c++) {
        double L[9];
        double M[16];
        int k;
        for (k = 0; k < 9; k++) L[k] = (double)coeffs[(size_t)k * channels + c];
        M[0] = a2 * kb * L[8];                  /* X X */
        M[5] = 3.0 * a2 * kc * L[6];            /* Y Y */
        M[10] = -a2 * kb * L[8];                /* Z Z */
        M[1] = M[4] = 0.5 * a2 * ka * L[7];     /* X Y */
        M[2] = M[8] = 0.5 * a2 * ka * L[4];     /* X Z */
        M[6] = M[9] = 0.5 * a2 * ka * L[5];     /* Y Z */
        M[3] = M[12] = 0.5 * a1 * k1 * L[3];    /* X 1 */
        M[7] = M[13] = 0.5 * a1 * k1 * L[2];    /* Y 1 */
        M[11] = M[14] = 0.5 * a1 * k1 * L[1];   /* Z 1 */
        M[15] = a0 * k0 * L[0] - a2 * kc * L[6];
        for (k = 0; k < 16; k++) out[c * 16 + (size_t)k] = (ALWAN_CORE_T)M[k];
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * GGX prefiltering and the split-sum tables.
 * ---------------------------------------------------------------- */

/* The radical inverse of i in base 2 (van der Corput), the second Hammersley coordinate. */
static double ALWAN_CORE_FNLIT(alwan__ibl_radical_inverse)(uint32_t i) {
    i = (i << 16) | (i >> 16);
    i = ((i & 0x55555555u) << 1) | ((i & 0xAAAAAAAAu) >> 1);
    i = ((i & 0x33333333u) << 2) | ((i & 0xCCCCCCCCu) >> 2);
    i = ((i & 0x0F0F0F0Fu) << 4) | ((i & 0xF0F0F0F0u) >> 4);
    i = ((i & 0x00FF00FFu) << 8) | ((i & 0xFF00FF00u) >> 8);
    return (double)i * 2.3283064365386962890625e-10;
}

/* One level of the box pyramid: w x h x ch doubles. */
typedef struct {
    double *px;
    size_t w, h;
} ALWAN_CORE_FNLIT(alwan__ibl_level);

/* Level l of a pyramid at map position (x, y): bilinear, x wrapping round the map, y
 * clamped to the first and last rows (a lookup does not cross a pole). */
static void ALWAN_CORE_FNLIT(alwan__ibl_bilinear)(double *out, ALWAN_CORE_FNLIT(alwan__ibl_level) const *lv, size_t ch,
                                                 double x, double y) {
    double const wd = (double)lv->w, hd = (double)lv->h;
    double px = x * wd - 0.5;
    double py = y * hd - 0.5;
    double fx, fy;
    size_t i0, i1, j0, j1;
    size_t c;
    /* x wraps: px into [0, w), then through the table gate (a NaN to texel 0) to the
     * lower texel; the last texel blends with the first */
    px = px - ALWAN_FLOOR(px / wd) * wd;
    i0 = (size_t)ALWAN_FLOOR(alwan_table_coord_unit_f64_v(px, (int)lv->w));
    fx = px - (double)i0;
    i1 = i0 + 1 < lv->w ? i0 + 1 : 0;
    /* y holds to the first and last rows */
    py = alwan_table_coord_unit_f64_v(py, (int)lv->h);
    j0 = (size_t)ALWAN_FLOOR(py);
    fy = py - (double)j0;
    j1 = j0 + 1 < lv->h ? j0 + 1 : j0;
    (void)hd;
    for (c = 0; c < ch; c++) {
        double const a = lv->px[((size_t)j0 * lv->w + (size_t)i0) * ch + c];
        double const b = lv->px[((size_t)j0 * lv->w + (size_t)i1) * ch + c];
        double const d = lv->px[((size_t)j1 * lv->w + (size_t)i0) * ch + c];
        double const e = lv->px[((size_t)j1 * lv->w + (size_t)i1) * ch + c];
        double const top = a + (b - a) * fx, bot = d + (e - d) * fx;
        out[c] = top + (bot - top) * fy;
    }
}

alwan_status ALWAN_CORE_FNLIT(alwan_ibl_prefilter_ggx)(ALWAN_CORE_T *out, size_t out_row_stride, size_t out_width,
                                                      size_t out_height, ALWAN_CORE_T roughness, ALWAN_CORE_T const *map,
                                                      size_t row_stride, size_t width, size_t height, size_t channels,
                                                      alwan_ibl_prefilter_params const *params, alwan_ctx *ctx) {
    ALWAN_CORE_FNLIT(alwan__ibl_level) lv[40];
    double const two_pi = 6.283185307179586476925;
    double const pi = 3.141592653589793238463;
    double *samples = NULL;     /* per sample: hx, hy, hz, n.l, its lod before the texel's term */
    double acc[ALWAN__SH_MAX_CHANNELS], tap[ALWAN__SH_MAX_CHANNELS], tap2[ALWAN__SH_MAX_CHANNELS];
    size_t count, levels = 0, i, j, c, s;
    int filtered;
    double alpha, r;
    alwan_status status = ALWAN_OK;
    (void)ctx;
    if (!out || !map || out_width == 0 || out_height == 0 || width == 0 || height == 0 || channels == 0 ||
        channels > ALWAN__SH_MAX_CHANNELS) return ALWAN_E_INVALID;
    if (row_stride < width * channels * sizeof(ALWAN_CORE_T) ||
        out_row_stride < out_width * channels * sizeof(ALWAN_CORE_T)) return ALWAN_E_INVALID;
    r = (double)roughness;
    if (!(r >= 0.0 && r <= 1.0)) return ALWAN_E_RANGE;
    count = params && params->sample_count ? params->sample_count : 256;
    if (count > 0x7fffffffu) return ALWAN_E_RANGE;
    filtered = !(params && params->no_mip_filtering);
    alpha = r * r;

    /* the box pyramid of the map, level 0 the map itself in double */
    memset(lv, 0, sizeof(lv));
    {
        size_t w = width, h = height;
        for (;;) {
            lv[levels].w = w;
            lv[levels].h = h;
            lv[levels].px = (double *)ALWAN_ALLOC(alwan_safe_array_size(alwan_safe_array_size(w, h), channels * sizeof(double)),
                                                  sizeof(double));
            if (!lv[levels].px) { status = ALWAN_E_NOMEM; goto done; }
            levels++;
            if ((w == 1 && h == 1) || !filtered || levels == 40) break;
            w = w > 1 ? (w + 1) / 2 : 1;
            h = h > 1 ? (h + 1) / 2 : 1;
        }
    }
    for (j = 0; j < height; j++) {
        ALWAN_CORE_T const *row = (ALWAN_CORE_T const *)((unsigned char const *)map + j * row_stride);
        for (i = 0; i < width * channels; i++) {
            double const v = (double)row[i];
            if (!(v >= -1.7976931348623157e308 && v <= 1.7976931348623157e308)) { status = ALWAN_E_INVALID; goto done; }
            lv[0].px[j * width * channels + i] = v;
        }
    }
    for (s = 1; s < levels; s++) {
        ALWAN_CORE_FNLIT(alwan__ibl_level) const *a = &lv[s - 1];
        ALWAN_CORE_FNLIT(alwan__ibl_level) *b = &lv[s];
        for (j = 0; j < b->h; j++) {
            size_t const y0 = a->h > 1 ? 2 * j : 0, y1 = a->h > 1 && 2 * j + 1 < a->h ? 2 * j + 1 : y0;
            for (i = 0; i < b->w; i++) {
                size_t const x0 = a->w > 1 ? 2 * i : 0, x1 = a->w > 1 && 2 * i + 1 < a->w ? 2 * i + 1 : x0;
                for (c = 0; c < channels; c++) {
                    b->px[(j * b->w + i) * channels + c] =
                        0.25 * (a->px[(y0 * a->w + x0) * channels + c] + a->px[(y0 * a->w + x1) * channels + c] +
                                a->px[(y1 * a->w + x0) * channels + c] + a->px[(y1 * a->w + x1) * channels + c]);
                }
            }
        }
    }

    if (alpha > 0.0) {
        samples = (double *)ALWAN_ALLOC(alwan_safe_array_size(count, 5 * sizeof(double)), sizeof(double));
        if (!samples) { status = ALWAN_E_NOMEM; goto done; }
        for (s = 0; s < count; s++) {
            alwan_vec3_f64 const hv = alwan_ibl_ggx_sample_f64_v((double)s / (double)count,
                                                                 ALWAN_CORE_FNLIT(alwan__ibl_radical_inverse)((uint32_t)s), alpha);
            double const nol = 2.0 * hv.v[2] * hv.v[2] - 1.0;
            double const pdf = alwan_ibl_ggx_d_f64_v(hv.v[2], alpha) * 0.25;   /* v = n: the density of l */
            samples[5 * s + 0] = hv.v[0];
            samples[5 * s + 1] = hv.v[1];
            samples[5 * s + 2] = hv.v[2];
            samples[5 * s + 3] = nol;
            /* 0.5 log2(solid angle of a sample) + 1, Karis 2013's bias; the texel's term comes per lookup */
            samples[5 * s + 4] = pdf > 0.0 ? 0.5 * ALWAN_LOG2(1.0 / ((double)count * pdf)) + 1.0 : 0.0;
        }
    }

    for (j = 0; j < out_height; j++) {
        ALWAN_CORE_T *orow = (ALWAN_CORE_T *)((unsigned char *)out + j * out_row_stride);
        double const theta = pi * ((double)j + 0.5) / (double)out_height;
        double const ct = ALWAN_COS(theta), st = ALWAN_SIN(theta);
        for (i = 0; i < out_width; i++) {
            double const phi = two_pi * ((double)i + 0.5) / (double)out_width;
            double const n0 = st * ALWAN_COS(phi), n1 = ct, n2 = st * ALWAN_SIN(phi);
            double wsum = 0.0;
            if (!(alpha > 0.0)) {
                ALWAN_CORE_FNLIT(alwan__ibl_bilinear)(acc, &lv[0], channels, ((double)i + 0.5) / (double)out_width,
                                                     ((double)j + 0.5) / (double)out_height);
                for (c = 0; c < channels; c++) orow[i * channels + c] = (ALWAN_CORE_T)acc[c];
                continue;
            }
            {
                /* the tangent frame of n: t = normalize(up x n), b = n x t */
                double ux = 0.0, uy = 1.0, uz = 0.0, tx, ty, tz, bx, by, bz, tl;
                if (n1 > 0.999 || n1 < -0.999) { ux = 1.0; uy = 0.0; }
                tx = uy * n2 - uz * n1;
                ty = uz * n0 - ux * n2;
                tz = ux * n1 - uy * n0;
                tl = ALWAN_SQRT(tx * tx + ty * ty + tz * tz);
                tx /= tl; ty /= tl; tz /= tl;
                bx = n1 * tz - n2 * ty;
                by = n2 * tx - n0 * tz;
                bz = n0 * ty - n1 * tx;
                for (c = 0; c < channels; c++) acc[c] = 0.0;
                for (s = 0; s < count; s++) {
                    double const *sm = samples + 5 * s;
                    double hx, hy, hz, lx, ly, lz, lp, lt, xm, ym, sl;
                    if (!(sm[3] > 0.0)) continue;
                    hx = sm[0] * tx + sm[1] * bx + sm[2] * n0;
                    hy = sm[0] * ty + sm[1] * by + sm[2] * n1;
                    hz = sm[0] * tz + sm[1] * bz + sm[2] * n2;
                    /* l = 2 (v . h) h - v with v = n, and v . h = n . h */
                    lx = 2.0 * sm[2] * hx - n0;
                    ly = 2.0 * sm[2] * hy - n1;
                    lz = 2.0 * sm[2] * hz - n2;
                    if (ly > 1.0) ly = 1.0;
                    if (ly < -1.0) ly = -1.0;
                    lt = ALWAN_ACOS(ly);
                    lp = ALWAN_ATAN2(lz, lx);
                    if (lp < 0.0) lp += two_pi;
                    xm = lp / two_pi;
                    ym = lt / pi;
                    if (filtered) {
                        /* the texel's solid angle at level 0 where l lands */
                        double sn = ALWAN_SIN(lt), lod, f;
                        size_t k;
                        if (sn < 1e-6) sn = 1e-6;
                        sl = two_pi / (double)width * pi / (double)height * sn;
                        lod = alwan_table_coord_unit_f64_v(sm[4] - 0.5 * ALWAN_LOG2(sl), (int)levels);
                        k = (size_t)ALWAN_FLOOR(lod);
                        if (k > levels - 1) k = levels - 1;
                        f = lod - (double)k;
                        ALWAN_CORE_FNLIT(alwan__ibl_bilinear)(tap, &lv[k], channels, xm, ym);
                        if (f > 0.0 && k + 1 < levels) {
                            ALWAN_CORE_FNLIT(alwan__ibl_bilinear)(tap2, &lv[k + 1], channels, xm, ym);
                            for (c = 0; c < channels; c++) tap[c] += (tap2[c] - tap[c]) * f;
                        }
                    } else {
                        ALWAN_CORE_FNLIT(alwan__ibl_bilinear)(tap, &lv[0], channels, xm, ym);
                    }
                    for (c = 0; c < channels; c++) acc[c] += tap[c] * sm[3];
                    wsum += sm[3];
                }
            }
            for (c = 0; c < channels; c++) orow[i * channels + c] = (ALWAN_CORE_T)(wsum > 0.0 ? acc[c] / wsum : 0.0);
        }
    }
done:
    for (s = 0; s < levels; s++) ALWAN_FREE(lv[s].px);
    if (levels < 40 && lv[levels].px) ALWAN_FREE(lv[levels].px);
    ALWAN_FREE(samples);
    return status;
}

/* The split-sum pair at one (n . v, roughness), in double. */
static void ALWAN_CORE_FNLIT(alwan__ibl_brdf)(double *a_out, double *b_out, double nov, double roughness, size_t count) {
    double const alpha = roughness * roughness;
    double const vx = ALWAN_SQRT(1.0 - nov * nov), vz = nov;
    double a = 0.0, b = 0.0;
    size_t s;
    for (s = 0; s < count; s++) {
        alwan_vec3_f64 const h = alwan_ibl_ggx_sample_f64_v((double)s / (double)count,
                                                            ALWAN_CORE_FNLIT(alwan__ibl_radical_inverse)((uint32_t)s), alpha);
        double const voh = vx * h.v[0] + vz * h.v[2];
        double const lz = 2.0 * voh * h.v[2] - vz;
        if (lz > 0.0 && voh > 0.0) {
            double const g = alwan_ibl_smith_g2_f64_v(nov, lz, alpha);
            double const gv = g * voh / (h.v[2] * nov);
            double const fc1 = 1.0 - voh;
            double const fc = fc1 * fc1 * fc1 * fc1 * fc1;
            a += (1.0 - fc) * gv;
            b += fc * gv;
        }
    }
    *a_out = a / (double)count;
    *b_out = b / (double)count;
}

alwan_status ALWAN_CORE_FNLIT(alwan_ibl_brdf_integrate)(ALWAN_CORE_T *scale_out, ALWAN_CORE_T *bias_out, ALWAN_CORE_T nov,
                                                       ALWAN_CORE_T roughness, size_t sample_count) {
    double a, b;
    size_t const count = sample_count ? sample_count : 1024;
    if (!scale_out || !bias_out) return ALWAN_E_INVALID;
    if (!((double)nov > 0.0 && (double)nov <= 1.0) || !((double)roughness >= 0.0 && (double)roughness <= 1.0) ||
        count > 0x7fffffffu) return ALWAN_E_RANGE;
    ALWAN_CORE_FNLIT(alwan__ibl_brdf)(&a, &b, (double)nov, (double)roughness, count);
    *scale_out = (ALWAN_CORE_T)a;
    *bias_out = (ALWAN_CORE_T)b;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_ibl_brdf_lut)(ALWAN_CORE_T *out, size_t row_stride, size_t width, size_t height,
                                                 size_t sample_count) {
    size_t const count = sample_count ? sample_count : 1024;
    size_t i, j;
    if (!out || width == 0 || height == 0) return ALWAN_E_INVALID;
    if (row_stride < width * 2 * sizeof(ALWAN_CORE_T)) return ALWAN_E_INVALID;
    if (count > 0x7fffffffu) return ALWAN_E_RANGE;
    for (j = 0; j < height; j++) {
        ALWAN_CORE_T *row = (ALWAN_CORE_T *)((unsigned char *)out + j * row_stride);
        double const roughness = ((double)j + 0.5) / (double)height;
        for (i = 0; i < width; i++) {
            double a, b;
            ALWAN_CORE_FNLIT(alwan__ibl_brdf)(&a, &b, ((double)i + 0.5) / (double)width, roughness, count);
            row[2 * i] = (ALWAN_CORE_T)a;
            row[2 * i + 1] = (ALWAN_CORE_T)b;
        }
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_ibl_energy_average)(ALWAN_CORE_T *out, ALWAN_CORE_T const *lut, size_t row_stride,
                                                       size_t width, size_t height) {
    size_t i, j;
    if (!out || !lut || width == 0 || height == 0) return ALWAN_E_INVALID;
    if (row_stride < width * 2 * sizeof(ALWAN_CORE_T)) return ALWAN_E_INVALID;
    for (j = 0; j < height; j++) {
        ALWAN_CORE_T const *row = (ALWAN_CORE_T const *)((unsigned char const *)lut + j * row_stride);
        double s = 0.0;
        for (i = 0; i < width; i++) {
            double const mu = ((double)i + 0.5) / (double)width;
            s += ((double)row[2 * i] + (double)row[2 * i + 1]) * mu;
        }
        out[j] = (ALWAN_CORE_T)(2.0 * s / (double)width);
    }
    return ALWAN_OK;
}

#undef ALWAN_IBL_VEC3
