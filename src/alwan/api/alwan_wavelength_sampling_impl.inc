/* No include guard -- included twice (f32 and f64 passes) */

#define ALWAN_WLS_CAT2_(a, b) a##b
#define ALWAN_WLS_CAT_(a, b)  ALWAN_WLS_CAT2_(a, b)
#define ALWAN_WLS_S        struct ALWAN_WLS_CAT_(ALWAN_CORE_FNLIT(alwan_wavelength_sampler), _s)
#define ALWAN_WLS_PARAMS   ALWAN_CORE_FNLIT(alwan_wavelength_sampler_params)
#define ALWAN_WLS_IS2D     ALWAN_CORE_FNLIT(alwan_importance_sampling_2d)
#define ALWAN_WLS_IS2D_P   ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_params)

/* The density, its range and the constants each density reads. TABULATED keeps a 2D
 * sampler laid out as one column, height weight_count, its y axis the wavelength and its x
 * axis [0, 1]: the 2D sampler's marginal is then the wavelength's distribution, which every
 * one of its modes draws from u1 alone (ALIAS places the point inside the picked cell with
 * what is left of u1), so u1 = u and u0 = 1/2 for all three. */
ALWAN_WLS_S {
    alwan_wavelength_pdf pdf;
    ALWAN_CORE_T lo, hi;
    ALWAN_CORE_T a, b, t0, t1;
    ALWAN_WLS_IS2D *table;
    alwan_importance_sampling_2d_mode mode;
};

static int ALWAN_CORE_FNLIT(alwan__wls_finite)(ALWAN_CORE_T v) {
    return v == v && v - v == v - v;   /* not NaN, not infinite */
}

alwan_status ALWAN_CORE_FNLIT(alwan_wavelength_sampler_create)(ALWAN_CORE_FNLIT(alwan_wavelength_sampler) **out,
                                                               alwan_wavelength_pdf pdf, ALWAN_WLS_PARAMS const *params,
                                                               alwan_ctx *ctx) {
    ALWAN_WLS_S *s;
    ALWAN_CORE_T lo = ALWAN_CORE_LITERAL(360.0), hi = ALWAN_CORE_LITERAL(830.0);
    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(0.0072), b = ALWAN_CORE_LITERAL(538.0);
    if (!out) return ALWAN_E_INVALID;
    *out = NULL;
    if ((int)pdf < (int)ALWAN_WAVELENGTH_PDF_UNIFORM || (int)pdf > (int)ALWAN_WAVELENGTH_PDF_TABULATED) return ALWAN_E_INVALID;
    if (params) {
        if (!(params->lambda_min == ALWAN_CORE_ZERO && params->lambda_max == ALWAN_CORE_ZERO)) {
            lo = params->lambda_min;
            hi = params->lambda_max;
            if (!ALWAN_CORE_FNLIT(alwan__wls_finite)(lo) || !ALWAN_CORE_FNLIT(alwan__wls_finite)(hi) || !(hi > lo)
                || lo < ALWAN_CORE_ZERO) {
                return ALWAN_E_INVALID;
            }
        }
        if (params->visible_a != ALWAN_CORE_ZERO) a = params->visible_a;
        if (params->visible_b != ALWAN_CORE_ZERO) b = params->visible_b;
        if (!ALWAN_CORE_FNLIT(alwan__wls_finite)(a) || !(a > ALWAN_CORE_ZERO) || !ALWAN_CORE_FNLIT(alwan__wls_finite)(b)) {
            return ALWAN_E_INVALID;
        }
    }
    if (pdf == ALWAN_WAVELENGTH_PDF_TABULATED && (!params || !params->weights || params->weight_count == 0)) {
        return ALWAN_E_INVALID;
    }
    s = (ALWAN_WLS_S *)ALWAN_ALLOC(sizeof(*s), sizeof(double));
    if (!s) return ALWAN_E_NOMEM;
    memset(s, 0, sizeof(*s));
    s->pdf = pdf;
    s->lo = lo;
    s->hi = hi;
    s->a = a;
    s->b = b;
    s->t0 = ALWAN_CORE_TANH(a * (lo - b));
    s->t1 = ALWAN_CORE_TANH(a * (hi - b));
    if (pdf == ALWAN_WAVELENGTH_PDF_VISIBLE && !(s->t1 > s->t0)) {
        ALWAN_FREE(s);
        return ALWAN_E_INVALID;   /* a range so far out that the tanh is flat in this precision */
    }
    if (pdf == ALWAN_WAVELENGTH_PDF_TABULATED) {
        ALWAN_WLS_IS2D_P p;
        alwan_status st;
        memset(&p, 0, sizeof(p));
        p.domain_min_x = ALWAN_CORE_ZERO;
        p.domain_max_x = ALWAN_CORE_ONE;
        p.domain_min_y = lo;
        p.domain_max_y = hi;
        p.resolution_x = 1;
        p.resolution_y = params->tabulated_resolution;
        s->mode = params->tabulated_mode;
        st = ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_prepare)(&s->table, params->weights, params->weight_stride, 1,
                                                                    params->weight_count, params->tabulated_mode, &p, ctx);
        if (st != ALWAN_OK) {
            ALWAN_FREE(s);
            return st;
        }
    }
    *out = (ALWAN_CORE_FNLIT(alwan_wavelength_sampler) *)s;
    return ALWAN_OK;
}

void ALWAN_CORE_FNLIT(alwan_wavelength_sampler_destroy)(ALWAN_CORE_FNLIT(alwan_wavelength_sampler) *sampler, alwan_ctx *ctx) {
    ALWAN_WLS_S *s = (ALWAN_WLS_S *)sampler;
    if (!s) return;
    if (s->table) ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_destroy)(s->table, ctx);
    ALWAN_FREE(s);
}

/* The density at lambda, whatever the kind. */
static alwan_status ALWAN_CORE_FNLIT(alwan__wls_pdf)(ALWAN_CORE_T *pdf_out, ALWAN_CORE_T lambda, ALWAN_WLS_S const *s) {
    if (s->pdf == ALWAN_WAVELENGTH_PDF_UNIFORM) {
        *pdf_out = ALWAN_CORE_FNVLIT(alwan_wavelength_uniform_pdf)(lambda, s->lo, s->hi);
    } else if (s->pdf == ALWAN_WAVELENGTH_PDF_VISIBLE) {
        *pdf_out = ALWAN_CORE_FNVLIT(alwan_wavelength_visible_pdf)(lambda, s->lo, s->hi, s->a, s->b, s->t0, s->t1);
    } else {
        ALWAN_CORE_VEC2 xy;
        xy.v[0] = ALWAN_CORE_LITERAL(0.5);
        xy.v[1] = lambda;
        return ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_pdf)(pdf_out, &xy, s->table);
    }
    return ALWAN_OK;
}

static alwan_status ALWAN_CORE_FNLIT(alwan__wls_sample)(ALWAN_CORE_T *lambda_out, ALWAN_CORE_T *pdf_out, ALWAN_CORE_T u,
                                                        ALWAN_WLS_S const *s) {
    if (!(u >= ALWAN_CORE_ZERO && u <= ALWAN_CORE_ONE)) return ALWAN_E_RANGE;
    if (s->pdf == ALWAN_WAVELENGTH_PDF_UNIFORM) {
        *lambda_out = ALWAN_CORE_FNVLIT(alwan_wavelength_uniform_sample)(u, s->lo, s->hi);
        if (pdf_out) *pdf_out = ALWAN_CORE_ONE / (s->hi - s->lo);
    } else if (s->pdf == ALWAN_WAVELENGTH_PDF_VISIBLE) {
        ALWAN_CORE_T const l = ALWAN_CORE_FNVLIT(alwan_wavelength_visible_sample)(u, s->lo, s->hi, s->a, s->b, s->t0, s->t1);
        *lambda_out = l;
        if (pdf_out) *pdf_out = ALWAN_CORE_FNVLIT(alwan_wavelength_visible_pdf)(l, s->lo, s->hi, s->a, s->b, s->t0, s->t1);
    } else {
        ALWAN_CORE_VEC2 uu, xy;
        ALWAN_CORE_T pdf = ALWAN_CORE_ZERO;
        alwan_status st;
        uu.v[0] = ALWAN_CORE_LITERAL(0.5);
        uu.v[1] = u;
        st = ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_sample)(&xy, &pdf, NULL, &uu, s->table);
        if (st != ALWAN_OK) return st;
        *lambda_out = xy.v[1];
        if (pdf_out) *pdf_out = pdf;   /* per area of [0, 1] x [lo, hi], so per nm */
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_wavelength_sample)(ALWAN_CORE_T *lambda_out, ALWAN_CORE_T *pdf_out, ALWAN_CORE_T u,
                                                       ALWAN_CORE_FNLIT(alwan_wavelength_sampler) const *sampler) {
    if (!lambda_out || !sampler) return ALWAN_E_INVALID;
    return ALWAN_CORE_FNLIT(alwan__wls_sample)(lambda_out, pdf_out, u, (ALWAN_WLS_S const *)sampler);
}

alwan_status ALWAN_CORE_FNLIT_MAP(alwan_wavelength_sample)(ALWAN_CORE_T *out, size_t out_stride, ALWAN_CORE_T const *in,
                                                           size_t in_stride, size_t count,
                                                           ALWAN_CORE_FNLIT(alwan_wavelength_sampler) const *sampler) {
    size_t i;
    if (!out || !in || !sampler) return ALWAN_E_INVALID;
    if (out_stride == 0) out_stride = 2 * sizeof(ALWAN_CORE_T);
    if (in_stride == 0) in_stride = sizeof(ALWAN_CORE_T);
    for (i = 0; i < count; i++) {
        ALWAN_CORE_T const u = *(ALWAN_CORE_T const *)((char const *)in + i * in_stride);
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((char *)out + i * out_stride);
        alwan_status const st = ALWAN_CORE_FNLIT(alwan__wls_sample)(&o[0], &o[1], u, (ALWAN_WLS_S const *)sampler);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_wavelength_pdf)(ALWAN_CORE_T *pdf_out, ALWAN_CORE_T lambda,
                                                    ALWAN_CORE_FNLIT(alwan_wavelength_sampler) const *sampler) {
    if (!pdf_out || !sampler) return ALWAN_E_INVALID;
    return ALWAN_CORE_FNLIT(alwan__wls_pdf)(pdf_out, lambda, (ALWAN_WLS_S const *)sampler);
}

alwan_status ALWAN_CORE_FNLIT(alwan_wavelength_invert)(ALWAN_CORE_T *u_out, ALWAN_CORE_T lambda,
                                                       ALWAN_CORE_FNLIT(alwan_wavelength_sampler) const *sampler) {
    ALWAN_WLS_S const *s = (ALWAN_WLS_S const *)sampler;
    if (!u_out || !s) return ALWAN_E_INVALID;
    if (!(lambda >= s->lo && lambda <= s->hi)) return ALWAN_E_RANGE;
    if (s->pdf == ALWAN_WAVELENGTH_PDF_UNIFORM) {
        *u_out = (lambda - s->lo) / (s->hi - s->lo);
    } else if (s->pdf == ALWAN_WAVELENGTH_PDF_VISIBLE) {
        *u_out = ALWAN_CORE_FNVLIT(alwan_wavelength_visible_cdf)(lambda, s->a, s->b, s->t0, s->t1);
    } else {
        ALWAN_CORE_VEC2 xy, uu;
        alwan_status st;
        xy.v[0] = ALWAN_CORE_LITERAL(0.5);
        xy.v[1] = lambda;
        st = ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_invert)(&uu, &xy, s->table);
        if (st != ALWAN_OK) return st;
        *u_out = uu.v[1];
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_wavelength_sample_hero)(ALWAN_CORE_T *lambda_out, ALWAN_CORE_T *pdf_out,
                                                            ALWAN_CORE_T *mis_out, size_t count, ALWAN_CORE_T u,
                                                            ALWAN_CORE_FNLIT(alwan_wavelength_sampler) const *sampler) {
    ALWAN_WLS_S const *s = (ALWAN_WLS_S const *)sampler;
    ALWAN_CORE_T hero = ALWAN_CORE_ZERO, sum = ALWAN_CORE_ZERO;
    alwan_status st;
    size_t j;
    if (!lambda_out || !s || count == 0 || count > 0x7fffffff) return ALWAN_E_INVALID;
    st = ALWAN_CORE_FNLIT(alwan__wls_sample)(&hero, NULL, u, s);
    if (st != ALWAN_OK) return st;
    for (j = 0; j < count; j++) {
        ALWAN_CORE_T p = ALWAN_CORE_ZERO;
        ALWAN_CORE_T const l = j == 0 ? hero
                             : ALWAN_CORE_FNVLIT(alwan_wavelength_hero_rotate)(hero, s->lo, s->hi, (int)j, (int)count);
        lambda_out[j] = l;
        st = ALWAN_CORE_FNLIT(alwan__wls_pdf)(&p, l, s);
        if (st != ALWAN_OK) return st;
        if (pdf_out) pdf_out[j] = p;
        sum += p;
    }
    if (mis_out) {
        /* the hero's own density is positive (it was drawn), so the sum is too */
        for (j = 0; j < count; j++) {
            ALWAN_CORE_T p = ALWAN_CORE_ZERO;
            if (pdf_out) {
                p = pdf_out[j];
            } else {
                st = ALWAN_CORE_FNLIT(alwan__wls_pdf)(&p, lambda_out[j], s);
                if (st != ALWAN_OK) return st;
            }
            mis_out[j] = sum > ALWAN_CORE_ZERO ? p / sum : ALWAN_CORE_ONE / (ALWAN_CORE_T)count;
        }
    }
    return ALWAN_OK;
}

#undef ALWAN_WLS_CAT2_
#undef ALWAN_WLS_CAT_
#undef ALWAN_WLS_S
#undef ALWAN_WLS_PARAMS
#undef ALWAN_WLS_IS2D
#undef ALWAN_WLS_IS2D_P
