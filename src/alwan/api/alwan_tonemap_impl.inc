/*
 * Global tone mapping operators, instantiated per precision by alwan_tonemap.c.
 * Luminance, the image statistics and the per-pixel maths run in double in both
 * precisions, as colour-hdri computes them in float64, so the f32 entry points differ
 * only by the rounding of their input and output. Each formula is written in
 * colour-hdri's order of operations.
 */

#ifndef ALWAN__TONEMAP_ONCE
#define ALWAN__TONEMAP_ONCE

/* colour-hdri's log_average adds numpy's float64 epsilon; Tumblin 1999 adds 2.3e-5. */
#define ALWAN__TM_EPS         DBL_EPSILON
#define ALWAN__TM_TUMBLIN_EPS 2.3e-5

typedef struct {
    double w[3];
    double p, q, k, gamma, ev;
    double l_da, c_max, l_max;
    double f, m, a, c;
    int automatic_m;
    double A, B, C, D, E, F, bias, white;
} alwan__tm_params;

/* A zero field is its default. */
static double alwan__tm_or(double v, double def) {
    return v == 0.0 ? def : v;
}

/* Hable 2010's curve, as colour-hdri writes it. */
static double alwan__tm_filmic(double x, alwan__tm_params const *t) {
    return ((x * (t->A * x + t->C * t->B) + t->D * t->E) / (x * (t->A * x + t->B) + t->D * t->F)) - t->E / t->F;
}

/* Tumblin 1999: the gamma of an eye adapted to luminance x. */
static double alwan__tm_tumblin_gamma(double x) {
    return x > 100.0 ? 2.655 : 1.855 + 0.4 * ALWAN_LOG10_F64(x + ALWAN__TM_TUMBLIN_EPS);
}

/* pow for a base that may be zero: pow(0, y > 0) is 0, which the deterministic pow,
 * defined for positive bases, need not return. */
static double alwan__tm_pow0(double x, double y) {
    return x == 0.0 ? 0.0 : ALWAN_POW_F64(x, y);
}

static int alwan__tm_uses_luminance(int op) {
    return op == (int)ALWAN_TONEMAP_NORMALIZATION ||
           (op >= (int)ALWAN_TONEMAP_LOGARITHMIC && op <= (int)ALWAN_TONEMAP_REINHARD2004);
}

/* The operators that scale RGB by L_d / L, and so map zero luminance to black. */
static int alwan__tm_scales_by_ratio(int op) {
    return op >= (int)ALWAN_TONEMAP_LOGARITHMIC && op <= (int)ALWAN_TONEMAP_TUMBLIN1999;
}

/* Each operator's ranges, checked after the defaults are in. */
static int alwan__tm_valid(alwan__tm_params const *t, int op) {
    switch (op) {
    case ALWAN_TONEMAP_GAMMA:
        return t->gamma > 0.0;
    case ALWAN_TONEMAP_LOGARITHMIC:
    case ALWAN_TONEMAP_EXPONENTIAL:
        return t->q >= 1.0 && t->k >= 1.0;
    case ALWAN_TONEMAP_LOGARITHMIC_MAPPING:
    case ALWAN_TONEMAP_EXPONENTIATION_MAPPING:
        return t->p > 0.0 && t->q > 0.0;
    case ALWAN_TONEMAP_SCHLICK1994:
        return t->p > 0.0;
    case ALWAN_TONEMAP_TUMBLIN1999:
        return t->l_da > 0.0 && t->c_max > 0.0 && t->l_max > 0.0;
    case ALWAN_TONEMAP_REINHARD2004:
        return t->m > 0.0 && t->a >= 0.0 && t->a <= 1.0 && t->c >= 0.0 && t->c <= 1.0;
    default:
        return 1;
    }
}

#endif /* ALWAN__TONEMAP_ONCE */

#define ALWAN__TM_PARAMS ALWAN_CORE_FNLIT(alwan_tonemap_params)

/* The public parameters in double, defaults applied. */
static alwan_status ALWAN_CORE_FNLIT(alwan__tm_read)(alwan__tm_params *t, ALWAN__TM_PARAMS const *in) {
    ALWAN__TM_PARAMS zero;
    int i;
    if (!in) {
        memset(&zero, 0, sizeof zero);
        in = &zero;
    }
    if (in->luminance_weights[0] == 0 && in->luminance_weights[1] == 0 && in->luminance_weights[2] == 0) {
        /* colour-hdri's default colourspace is sRGB: the Y row of its primary matrix. */
        ALWAN_CORE_RGB_SPACE_DESC desc;
        ALWAN_CORE_MAT3X3 to_xyz, from_xyz;
        if (ALWAN_CORE_FNLIT(alwan_rgb_get_space_descriptor)(&desc, ALWAN_RGB_SPACE_SRGB, NULL) != ALWAN_OK ||
            ALWAN_CORE_FNLIT(alwan_rgb_derive_matrices)(&to_xyz, &from_xyz, &desc) != ALWAN_OK) {
            return ALWAN_E_INVALID;
        }
        for (i = 0; i < 3; i++) t->w[i] = (double)to_xyz.m[3 + i];
    } else {
        for (i = 0; i < 3; i++) t->w[i] = (double)in->luminance_weights[i];
    }
    if (in->automatic_contrast && in->contrast != 0) return ALWAN_E_INVALID;
    t->p = alwan__tm_or((double)in->p, 1.0);
    t->q = alwan__tm_or((double)in->q, 1.0);
    t->k = alwan__tm_or((double)in->k, 1.0);
    t->gamma = alwan__tm_or((double)in->gamma, 1.0);
    t->ev = (double)in->ev;
    t->l_da = alwan__tm_or((double)in->display_adaptation, 20.0);
    t->c_max = alwan__tm_or((double)in->display_contrast, 100.0);
    t->l_max = alwan__tm_or((double)in->display_peak, 100.0);
    t->f = (double)in->intensity;
    t->automatic_m = in->automatic_contrast != 0;
    t->m = t->automatic_m ? 1.0 : alwan__tm_or((double)in->contrast, 0.3);   /* automatic: set from the image */
    t->a = (double)in->light_adaptation;
    t->c = (double)in->chromatic_adaptation;
    t->A = alwan__tm_or((double)in->shoulder_strength, 0.22);
    t->B = alwan__tm_or((double)in->linear_strength, 0.3);
    t->C = alwan__tm_or((double)in->linear_angle, 0.1);
    t->D = alwan__tm_or((double)in->toe_strength, 0.2);
    t->E = alwan__tm_or((double)in->toe_numerator, 0.01);
    t->F = alwan__tm_or((double)in->toe_denominator, 0.3);
    t->bias = alwan__tm_or((double)in->exposure_bias, 2.0);
    t->white = alwan__tm_or((double)in->linear_white, 11.2);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_tonemap_global)(ALWAN_CORE_T *rgb_out, size_t out_stride,
                                                    ALWAN_CORE_T const *rgb_in, size_t in_stride, size_t count,
                                                    alwan_tonemap_operator op, ALWAN__TM_PARAMS const *params) {
    alwan__tm_params t;
    int const o = (int)op;
    int const lum = alwan__tm_uses_luminance(o), ratio = alwan__tm_scales_by_ratio(o);
    double l_max = 0.0, l_min = DBL_MAX, s_log = 0.0, s_tum = 0.0, c_sum[3] = { 0.0, 0.0, 0.0 };
    double scale = 0.0, extra = 0.0, f_white = 1.0, ig[3] = { 0.0, 0.0, 0.0 };
    alwan_status st;
    size_t i;
    int ch;

    if (!rgb_out || !rgb_in || o < (int)ALWAN_TONEMAP_SIMPLE || o > (int)ALWAN_TONEMAP_FILMIC) {
        return ALWAN_E_INVALID;
    }
    st = ALWAN_CORE_FNLIT(alwan__tm_read)(&t, params);
    if (st != ALWAN_OK) return st;
    if (!alwan__tm_valid(&t, o)) return ALWAN_E_INVALID;
    if (o == (int)ALWAN_TONEMAP_FILMIC) {
        f_white = alwan__tm_filmic(t.white, &t);
        if (!(f_white > 0.0 || f_white < 0.0)) return ALWAN_E_INVALID;
    }
    if (count == 0) return ALWAN_OK;

    /* The statistics, over the whole image before any pixel is written, so out may be in. */
    if (lum) {
        for (i = 0; i < count; i++) {
            ALWAN_CORE_T const *s = (ALWAN_CORE_T const *)((char const *)rgb_in + i * in_stride);
            double const L = t.w[0] * (double)s[0] + t.w[1] * (double)s[1] + t.w[2] * (double)s[2];
            if (!(L >= 0.0) || L > DBL_MAX) return ALWAN_E_INVALID;
            l_max = L > l_max ? L : l_max;
            l_min = L < l_min ? L : l_min;
            if (o == (int)ALWAN_TONEMAP_EXPONENTIAL || o == (int)ALWAN_TONEMAP_REINHARD2004) {
                s_log += ALWAN_LN_F64(L + ALWAN__TM_EPS);
            }
            if (o == (int)ALWAN_TONEMAP_TUMBLIN1999) s_tum += ALWAN_LN_F64(L + ALWAN__TM_TUMBLIN_EPS);
            if (o == (int)ALWAN_TONEMAP_REINHARD2004) {
                for (ch = 0; ch < 3; ch++) {
                    /* With c > 0 a channel is its own adaptation level, which must not be negative. */
                    if (t.c > 0.0 && (double)s[ch] < 0.0) return ALWAN_E_INVALID;
                    c_sum[ch] += (double)s[ch];
                }
            }
        }
    }

    switch (o) {
    case ALWAN_TONEMAP_GAMMA:
        scale = ALWAN_POW_F64(2.0, t.ev);
        extra = 1.0 / t.gamma;
        break;
    case ALWAN_TONEMAP_LOGARITHMIC:
        scale = ALWAN_LOG10_F64(1.0 + l_max * t.k);
        break;
    case ALWAN_TONEMAP_EXPONENTIAL:
        scale = ALWAN_EXP_F64(s_log / (double)count) * t.k;
        break;
    case ALWAN_TONEMAP_LOGARITHMIC_MAPPING:
        scale = ALWAN_LN_F64(1.0 + t.p * l_max);
        break;
    case ALWAN_TONEMAP_TUMBLIN1999: {
        /* As colour-hdri computes it: the natural log of L_da in g_wd, base 10 in the gammas. */
        double const l_wa = ALWAN_EXP_F64(s_tum / (double)count);
        double const g_d = alwan__tm_tumblin_gamma(t.l_da), g_w = alwan__tm_tumblin_gamma(l_wa);
        double const g_wd = g_w / (1.855 + 0.4 * ALWAN_LN_F64(t.l_da));
        scale = ALWAN_POW_F64(ALWAN_SQRT_F64(t.c_max), g_wd - 1.0) * t.l_da;
        extra = g_w / g_d;
        l_min = l_wa;                                      /* the adaptation luminance, reused below */
        break;
    }
    case ALWAN_TONEMAP_REINHARD2004: {
        double const la = s_log / (double)count, l_lav = ALWAN_EXP_F64(la);
        if (t.automatic_m) {
            /* Reinhard and Devlin 2005: m = 0.3 + 0.7 k^1.4, k the key of the image in the log
             * domain. Every log is of luminance plus the epsilon of the log average, so k lies
             * in [0, 1]; a uniform image has k = 0. */
            double const lx = ALWAN_LN_F64(l_max + ALWAN__TM_EPS), ln = ALWAN_LN_F64(l_min + ALWAN__TM_EPS);
            double const kk = lx > ln ? (lx - la) / (lx - ln) : 0.0;
            t.m = 0.3 + 0.7 * alwan__tm_pow0(kk, 1.4);
        }
        scale = ALWAN_EXP_F64(-t.f);
        for (ch = 0; ch < 3; ch++) ig[ch] = t.c * (c_sum[ch] / (double)count) + (1.0 - t.c) * l_lav;
        break;
    }
    default:
        break;
    }

    for (i = 0; i < count; i++) {
        ALWAN_CORE_T const *s = (ALWAN_CORE_T const *)((char const *)rgb_in + i * in_stride);
        ALWAN_CORE_T *d = (ALWAN_CORE_T *)((char *)rgb_out + i * out_stride);
        double const x[3] = { (double)s[0], (double)s[1], (double)s[2] };
        double y[3] = { 0.0, 0.0, 0.0 }, L = 0.0, Ld = 0.0;
        if (lum) L = t.w[0] * x[0] + t.w[1] * x[1] + t.w[2] * x[2];
        if (ratio && L == 0.0) {
            d[0] = d[1] = d[2] = (ALWAN_CORE_T)0.0;
            continue;
        }
        switch (o) {
        case ALWAN_TONEMAP_SIMPLE:
            for (ch = 0; ch < 3; ch++) y[ch] = x[ch] / (x[ch] + 1.0);
            break;
        case ALWAN_TONEMAP_NORMALIZATION:
            /* An image whose peak luminance is zero maps to black. */
            for (ch = 0; ch < 3; ch++) y[ch] = l_max > 0.0 ? x[ch] / l_max : 0.0;
            break;
        case ALWAN_TONEMAP_GAMMA:
            for (ch = 0; ch < 3; ch++) y[ch] = alwan__tm_pow0(scale * x[ch], extra);
            break;
        case ALWAN_TONEMAP_LOGARITHMIC:
            Ld = ALWAN_LOG10_F64(1.0 + L * t.q) / scale;
            break;
        case ALWAN_TONEMAP_EXPONENTIAL:
            Ld = 1.0 - ALWAN_EXP_F64(-(L * t.q) / scale);
            break;
        case ALWAN_TONEMAP_LOGARITHMIC_MAPPING:
            Ld = ALWAN_POW_F64(ALWAN_LN_F64(1.0 + t.p * L) / scale, 1.0 / t.q);
            break;
        case ALWAN_TONEMAP_EXPONENTIATION_MAPPING:
            Ld = ALWAN_POW_F64(L / l_max, t.p / t.q);
            break;
        case ALWAN_TONEMAP_SCHLICK1994:
            Ld = (t.p * L) / (t.p * L - L + l_max);
            break;
        case ALWAN_TONEMAP_TUMBLIN1999:
            Ld = scale * ALWAN_POW_F64(L / l_min, extra);
            break;
        case ALWAN_TONEMAP_REINHARD2004:
            for (ch = 0; ch < 3; ch++) {
                double const il = t.c * x[ch] + (1.0 - t.c) * L;
                double const ia = t.a * il + (1.0 - t.a) * ig[ch];
                /* A zero channel stays zero: 0 / (0 + 0) where its adaptation level is 0 too. */
                y[ch] = x[ch] == 0.0 ? 0.0 : x[ch] / (x[ch] + alwan__tm_pow0(scale * ia, t.m));
            }
            break;
        default: /* ALWAN_TONEMAP_FILMIC */
            for (ch = 0; ch < 3; ch++) y[ch] = alwan__tm_filmic(x[ch] * t.bias, &t) * (1.0 / f_white);
            break;
        }
        if (ratio) {
            for (ch = 0; ch < 3; ch++) y[ch] = x[ch] * Ld / L;
            if (o == (int)ALWAN_TONEMAP_TUMBLIN1999) {
                for (ch = 0; ch < 3; ch++) y[ch] = y[ch] / t.l_max;
            }
        }
        for (ch = 0; ch < 3; ch++) d[ch] = (ALWAN_CORE_T)y[ch];
    }
    return ALWAN_OK;
}

#undef ALWAN__TM_PARAMS
