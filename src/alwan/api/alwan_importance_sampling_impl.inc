/* No include guard -- included twice (f32 and f64 passes) */

#define ALWAN_IS2D_CAT2_(a, b) a##b
#define ALWAN_IS2D_CAT_(a, b)  ALWAN_IS2D_CAT2_(a, b)
#define ALWAN_IS2D_S       struct ALWAN_IS2D_CAT_(ALWAN_CORE_FNLIT(alwan_importance_sampling_2d), _s)
#define ALWAN_IS2D_PARAMS  ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_params)
#define ALWAN_IS2D_LAYOUT  ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_layout)

/* What prepare builds and sample reads: the mode, the domain, the image's size,
 * and one table laid out as core/alwan_importance_sampling_core.inc says. */
ALWAN_IS2D_S {
    ALWAN_CORE_T *table;
    size_t count;
    int nu, nv, ru, rv;
    alwan_importance_sampling_2d_mode mode;
    ALWAN_CORE_T x0, y0, x1, y1;
    ALWAN_CORE_T integral;
};

/* The CDF of n weights at w, written to c[0..n]: the running sum over the total, so
 * c[0] = 0 and c[n] = 1 exactly. A row whose weights are all 0 gets an even CDF.
 * Returns the sum. */
static ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan__is2d_cdf)(ALWAN_CORE_T const *w, ptrdiff_t w_step, ALWAN_CORE_T *c, int n) {
    ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
    int i;
    c[0] = ALWAN_CORE_LITERAL(0.0);
    for (i = 0; i < n; i++) {
        sum += w[i * w_step];
        c[i + 1] = sum;
    }
    if (sum > ALWAN_CORE_LITERAL(0.0)) {
        for (i = 1; i < n; i++) c[i] = c[i] / sum;
    } else {
        for (i = 1; i < n; i++) c[i] = (ALWAN_CORE_T)i / (ALWAN_CORE_T)n;
    }
    c[n] = ALWAN_CORE_LITERAL(1.0);
    return sum;
}

/* The inverse table of the CDF at cdf[c_base], n cells: r + 1 positions, the
 * exact inverse at u = k / r. */
static void ALWAN_CORE_FNLIT(alwan__is2d_inverse)(ALWAN_CORE_T *dst, ALWAN_CORE_T const *cdf, int c_base, int n, int r) {
    int k;
    for (k = 0; k <= r; k++) {
        ALWAN_CORE_T const u = (ALWAN_CORE_T)k / (ALWAN_CORE_T)r;
        ALWAN_CORE_VEC2 const t = ALWAN_CORE_FNVLIT(alwan_is2d_search_1d)(cdf, c_base, n, u);
        dst[k] = t.v[0];
    }
}

alwan_status ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_prepare)(ALWAN_CORE_FNLIT(alwan_importance_sampling_2d) **out,
                                                                   ALWAN_CORE_T const *image, size_t row_stride,
                                                                   size_t width, size_t height,
                                                                   alwan_importance_sampling_2d_mode mode,
                                                                   ALWAN_IS2D_PARAMS const *params, alwan_ctx *ctx) {
    ALWAN_IS2D_S *s;
    ALWAN_CORE_T *work;
    ALWAN_CORE_T x0 = ALWAN_CORE_LITERAL(0.0), y0 = ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T x1 = ALWAN_CORE_LITERAL(1.0), y1 = ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_T total = ALWAN_CORE_LITERAL(0.0);
    size_t ru = width, rv = height, search_count, count, i, j;
    int nu, nv;
    (void)ctx;
    if (!out) return ALWAN_E_INVALID;
    *out = NULL;
    if (!image || width == 0 || height == 0) return ALWAN_E_INVALID;
    if (mode != ALWAN_IMPORTANCE_SAMPLING_2D_DIRECT && mode != ALWAN_IMPORTANCE_SAMPLING_2D_SEARCH) return ALWAN_E_INVALID;
    if (width > 0x7fffffffu / sizeof(ALWAN_CORE_T)) return ALWAN_E_RANGE;
    if (row_stride == 0) row_stride = width * sizeof(ALWAN_CORE_T);
    if (row_stride < width * sizeof(ALWAN_CORE_T)) return ALWAN_E_INVALID;
    if (params) {
        if (params->domain_min_x != ALWAN_CORE_LITERAL(0.0) || params->domain_min_y != ALWAN_CORE_LITERAL(0.0) ||
            params->domain_max_x != ALWAN_CORE_LITERAL(0.0) || params->domain_max_y != ALWAN_CORE_LITERAL(0.0)) {
            x0 = params->domain_min_x; y0 = params->domain_min_y;
            x1 = params->domain_max_x; y1 = params->domain_max_y;
            if (!(x1 > x0) || !(y1 > y0)) return ALWAN_E_INVALID;
            if (!((double)x0 >= -1.7976931348623157e308 && (double)x1 <= 1.7976931348623157e308 &&
                  (double)y0 >= -1.7976931348623157e308 && (double)y1 <= 1.7976931348623157e308)) {
                return ALWAN_E_INVALID;
            }
        }
        if (params->resolution_x) ru = params->resolution_x;
        if (params->resolution_y) rv = params->resolution_y;
    }

    /* every offset is an int, for a shader: the tables stay below 2^31 elements */
    if (height > 0x7fffffffu || ru > 0x7fffffffu || rv > 0x7fffffffu) return ALWAN_E_RANGE;
    if (width * height / width != height) return ALWAN_E_RANGE;
    search_count = width * height + height * (width + 1) + height + 1 + 1;
    if (search_count > 0x7fffffffu) return ALWAN_E_RANGE;
    if (mode == ALWAN_IMPORTANCE_SAMPLING_2D_DIRECT) {
        if ((ru + 1) > 0x7fffffffu / height) return ALWAN_E_RANGE;
        count = height * (ru + 1) + rv + 1;
        if (count > 0x7fffffffu) return ALWAN_E_RANGE;
    } else {
        count = search_count;
    }
    nu = (int)width;
    nv = (int)height;

    /* the CDFs, in the search layout: the search mode keeps them, the direct mode
     * builds its inverse tables from them and lets them go */
    work = (ALWAN_CORE_T *)ALWAN_ALLOC(alwan_safe_array_size(search_count, sizeof(ALWAN_CORE_T)), sizeof(ALWAN_CORE_T));
    if (!work) return ALWAN_E_NOMEM;
    for (j = 0; j < height; j++) {
        ALWAN_CORE_T const *row = (ALWAN_CORE_T const *)((unsigned char const *)image + j * row_stride);
        for (i = 0; i < width; i++) {
            ALWAN_CORE_T const w = row[i];
            /* non-negative and finite: a bound rather than isfinite, so the test
             * survives a compiler assuming finite math */
            if (!(w >= ALWAN_CORE_LITERAL(0.0) && (double)w <= 1.7976931348623157e308)) {
                ALWAN_FREE(work);
                return ALWAN_E_INVALID;
            }
            work[j * width + i] = w;
        }
    }
    {
        int const c_off = ALWAN_CORE_FNVLIT(alwan_is2d_off_cdf)(nu, nv);
        int const m_off = ALWAN_CORE_FNVLIT(alwan_is2d_off_mcdf)(nu, nv);
        ALWAN_CORE_T *rowsum = (ALWAN_CORE_T *)ALWAN_ALLOC(alwan_safe_array_size(height, sizeof(ALWAN_CORE_T)), sizeof(ALWAN_CORE_T));
        if (!rowsum) {
            ALWAN_FREE(work);
            return ALWAN_E_NOMEM;
        }
        for (j = 0; j < height; j++) {
            rowsum[j] = ALWAN_CORE_FNLIT(alwan__is2d_cdf)(work + j * width, 1, work + c_off + j * (width + 1), nu);
        }
        total = ALWAN_CORE_FNLIT(alwan__is2d_cdf)(rowsum, 1, work + m_off, nv);
        ALWAN_FREE(rowsum);
        if (!(total > ALWAN_CORE_LITERAL(0.0)) || !((double)total <= 1.7976931348623157e308)) {
            /* nothing to sample, or a sum past the precision's range */
            ALWAN_FREE(work);
            return total > ALWAN_CORE_LITERAL(0.0) ? ALWAN_E_RANGE : ALWAN_E_INVALID;
        }
        /* density per unit weight: 1 / (sum x cell area) */
        work[ALWAN_CORE_FNVLIT(alwan_is2d_off_norm)(nu, nv)] =
            ALWAN_CORE_LITERAL(1.0) / (total * ((x1 - x0) / (ALWAN_CORE_T)nu) * ((y1 - y0) / (ALWAN_CORE_T)nv));
    }

    s = (ALWAN_IS2D_S *)ALWAN_ALLOC(sizeof(*s), sizeof(double));
    if (!s) {
        ALWAN_FREE(work);
        return ALWAN_E_NOMEM;
    }
    memset(s, 0, sizeof(*s));
    s->nu = nu; s->nv = nv;
    s->mode = mode;
    s->x0 = x0; s->y0 = y0; s->x1 = x1; s->y1 = y1;
    s->integral = total * ((x1 - x0) / (ALWAN_CORE_T)nu) * ((y1 - y0) / (ALWAN_CORE_T)nv);
    s->count = count;
    if (mode == ALWAN_IMPORTANCE_SAMPLING_2D_SEARCH) {
        s->table = work;
    } else {
        int const c_off = ALWAN_CORE_FNVLIT(alwan_is2d_off_cdf)(nu, nv);
        int r;
        s->ru = (int)ru;
        s->rv = (int)rv;
        s->table = (ALWAN_CORE_T *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(ALWAN_CORE_T)), sizeof(ALWAN_CORE_T));
        if (!s->table) {
            ALWAN_FREE(work);
            ALWAN_FREE(s);
            return ALWAN_E_NOMEM;
        }
        for (r = 0; r < nv; r++) {
            ALWAN_CORE_FNLIT(alwan__is2d_inverse)(s->table + (size_t)r * (ru + 1), work, c_off + r * (nu + 1), nu, s->ru);
        }
        ALWAN_CORE_FNLIT(alwan__is2d_inverse)(s->table + ALWAN_CORE_FNVLIT(alwan_is2d_off_minv)(nv, s->ru), work,
                                              ALWAN_CORE_FNVLIT(alwan_is2d_off_mcdf)(nu, nv), nv, s->rv);
        ALWAN_FREE(work);
    }
    *out = (ALWAN_CORE_FNLIT(alwan_importance_sampling_2d) *)s;
    return ALWAN_OK;
}

void ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_destroy)(ALWAN_CORE_FNLIT(alwan_importance_sampling_2d) *sampler,
                                                            alwan_ctx *ctx) {
    ALWAN_IS2D_S *s = (ALWAN_IS2D_S *)sampler;
    (void)ctx;
    if (!s) return;
    ALWAN_FREE(s->table);
    ALWAN_FREE(s);
}

/* One sample, the cell it fell in written to off[0], off[1]. */
static void ALWAN_CORE_FNLIT(alwan__is2d_sample_one)(ALWAN_CORE_T *x, ALWAN_CORE_T *y, ALWAN_CORE_T *pdf, size_t *off,
                                                     ALWAN_IS2D_S const *s, ALWAN_CORE_T u0, ALWAN_CORE_T u1) {
    ALWAN_CORE_VEC3 r;
    if (s->mode == ALWAN_IMPORTANCE_SAMPLING_2D_SEARCH) {
        r = ALWAN_CORE_FNVLIT(alwan_is2d_sample_2d_search)(s->table, s->nu, s->nv, s->x0, s->y0, s->x1, s->y1, u0, u1);
    } else {
        r = ALWAN_CORE_FNVLIT(alwan_is2d_sample_2d_direct)(s->table, s->nu, s->nv, s->ru, s->rv, s->x0, s->y0, s->x1, s->y1,
                                                           u0, u1);
    }
    if (off) {
        /* the cells the sampler chose, recovered the way it chose them */
        if (s->mode == ALWAN_IMPORTANCE_SAMPLING_2D_SEARCH) {
            ALWAN_CORE_VEC2 const sy = ALWAN_CORE_FNVLIT(alwan_is2d_search_1d)(s->table, ALWAN_CORE_FNVLIT(alwan_is2d_off_mcdf)(s->nu, s->nv),
                                                                               s->nv, u1);
            int const iv = ALWAN_CORE_FNVLIT(alwan_is2d_cell)(sy.v[1], s->nv);
            ALWAN_CORE_VEC2 const sx = ALWAN_CORE_FNVLIT(alwan_is2d_search_1d)(
                s->table, ALWAN_CORE_FNVLIT(alwan_is2d_off_cdf)(s->nu, s->nv) + iv * (s->nu + 1), s->nu, u0);
            off[0] = (size_t)ALWAN_CORE_FNVLIT(alwan_is2d_cell)(sx.v[1], s->nu);
            off[1] = (size_t)iv;
        } else {
            ALWAN_CORE_VEC2 const sy = ALWAN_CORE_FNVLIT(alwan_is2d_direct_1d)(s->table, ALWAN_CORE_FNVLIT(alwan_is2d_off_minv)(s->nv, s->ru),
                                                                               s->rv, u1, s->y1 - s->y0);
            int const iv = ALWAN_CORE_FNVLIT(alwan_is2d_cell)(sy.v[0] * (ALWAN_CORE_T)s->nv, s->nv);
            ALWAN_CORE_VEC2 const sx = ALWAN_CORE_FNVLIT(alwan_is2d_direct_1d)(s->table, iv * (s->ru + 1), s->ru, u0, s->x1 - s->x0);
            off[0] = (size_t)ALWAN_CORE_FNVLIT(alwan_is2d_cell)(sx.v[0] * (ALWAN_CORE_T)s->nu, s->nu);
            off[1] = (size_t)iv;
        }
    }
    *x = r.v[0];
    *y = r.v[1];
    *pdf = r.v[2];
}

static int ALWAN_CORE_FNLIT(alwan__is2d_u_ok)(ALWAN_CORE_T u) {
    return u >= ALWAN_CORE_LITERAL(0.0) && u <= ALWAN_CORE_LITERAL(1.0);
}

alwan_status ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_sample)(ALWAN_CORE_VEC2 *xy_out, ALWAN_CORE_T *pdf_out,
                                                                  size_t *offset_out, ALWAN_CORE_VEC2 const *u,
                                                                  ALWAN_CORE_FNLIT(alwan_importance_sampling_2d) const *sampler) {
    ALWAN_IS2D_S const *s = (ALWAN_IS2D_S const *)sampler;
    ALWAN_CORE_T x, y, pdf;
    if (!xy_out || !u || !s) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FNLIT(alwan__is2d_u_ok)(u->v[0]) || !ALWAN_CORE_FNLIT(alwan__is2d_u_ok)(u->v[1])) return ALWAN_E_RANGE;
    ALWAN_CORE_FNLIT(alwan__is2d_sample_one)(&x, &y, &pdf, offset_out, s, u->v[0], u->v[1]);
    xy_out->v[0] = x;
    xy_out->v[1] = y;
    if (pdf_out) *pdf_out = pdf;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT_MAP(alwan_importance_sampling_2d_sample)(ALWAN_CORE_T *out, size_t out_stride,
                                                                      ALWAN_CORE_T const *in, size_t in_stride, size_t count,
                                                                      ALWAN_CORE_FNLIT(alwan_importance_sampling_2d) const *sampler) {
    ALWAN_IS2D_S const *s = (ALWAN_IS2D_S const *)sampler;
    size_t k;
    if (!s || (count > 0 && (!out || !in))) return ALWAN_E_INVALID;
    if (out_stride == 0) out_stride = 3 * sizeof(ALWAN_CORE_T);
    if (in_stride == 0) in_stride = 2 * sizeof(ALWAN_CORE_T);
    for (k = 0; k < count; k++) {
        ALWAN_CORE_T const *u = (ALWAN_CORE_T const *)((unsigned char const *)in + k * in_stride);
        ALWAN_CORE_T *o = (ALWAN_CORE_T *)((unsigned char *)out + k * out_stride);
        if (!ALWAN_CORE_FNLIT(alwan__is2d_u_ok)(u[0]) || !ALWAN_CORE_FNLIT(alwan__is2d_u_ok)(u[1])) return ALWAN_E_RANGE;
        ALWAN_CORE_FNLIT(alwan__is2d_sample_one)(&o[0], &o[1], &o[2], NULL, s, u[0], u[1]);
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_pdf)(ALWAN_CORE_T *pdf_out, ALWAN_CORE_VEC2 const *xy,
                                                               ALWAN_CORE_FNLIT(alwan_importance_sampling_2d) const *sampler) {
    ALWAN_IS2D_S const *s = (ALWAN_IS2D_S const *)sampler;
    if (!pdf_out || !xy || !s) return ALWAN_E_INVALID;
    if (s->mode == ALWAN_IMPORTANCE_SAMPLING_2D_SEARCH) {
        *pdf_out = ALWAN_CORE_FNVLIT(alwan_is2d_pdf_2d_search)(s->table, s->nu, s->nv, s->x0, s->y0, s->x1, s->y1,
                                                               xy->v[0], xy->v[1]);
    } else {
        *pdf_out = ALWAN_CORE_FNVLIT(alwan_is2d_pdf_2d_direct)(s->table, s->nu, s->nv, s->ru, s->rv, s->x0, s->y0, s->x1,
                                                               s->y1, xy->v[0], xy->v[1]);
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_invert)(ALWAN_CORE_VEC2 *u_out, ALWAN_CORE_VEC2 const *xy,
                                                                  ALWAN_CORE_FNLIT(alwan_importance_sampling_2d) const *sampler) {
    ALWAN_IS2D_S const *s = (ALWAN_IS2D_S const *)sampler;
    ALWAN_CORE_VEC2 r;
    if (!u_out || !xy || !s) return ALWAN_E_INVALID;
    if (s->mode == ALWAN_IMPORTANCE_SAMPLING_2D_SEARCH) {
        r = ALWAN_CORE_FNVLIT(alwan_is2d_invert_2d_search)(s->table, s->nu, s->nv, s->x0, s->y0, s->x1, s->y1,
                                                           xy->v[0], xy->v[1]);
    } else {
        r = ALWAN_CORE_FNVLIT(alwan_is2d_invert_2d_direct)(s->table, s->nu, s->nv, s->ru, s->rv, s->x0, s->y0, s->x1,
                                                           s->y1, xy->v[0], xy->v[1]);
    }
    if (r.v[0] < ALWAN_CORE_LITERAL(0.0) || r.v[1] < ALWAN_CORE_LITERAL(0.0)) return ALWAN_E_RANGE;
    *u_out = r;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_importance_sampling_2d_get_layout)(ALWAN_IS2D_LAYOUT *out,
                                                                      ALWAN_CORE_FNLIT(alwan_importance_sampling_2d) const *sampler) {
    ALWAN_IS2D_S const *s = (ALWAN_IS2D_S const *)sampler;
    if (!out || !s) return ALWAN_E_INVALID;
    out->table = s->table;
    out->table_count = s->count;
    out->width = s->nu;
    out->height = s->nv;
    out->resolution_x = s->ru;
    out->resolution_y = s->rv;
    out->mode = s->mode;
    out->domain_min_x = s->x0;
    out->domain_min_y = s->y0;
    out->domain_max_x = s->x1;
    out->domain_max_y = s->y1;
    out->integral = s->integral;
    return ALWAN_OK;
}

#undef ALWAN_IS2D_S
#undef ALWAN_IS2D_CAT_
#undef ALWAN_IS2D_CAT2_
#undef ALWAN_IS2D_PARAMS
#undef ALWAN_IS2D_LAYOUT
