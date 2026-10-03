/* No include guard -- included twice (f32 and f64 passes) */

/* alwan's private piecewise-constant table for the wavelength sampler's TABULATED density:
 * the 2D importance sampler's prepare / sample / pdf / invert (that sampler is image
 * processing and lives in suwar), renamed and made static, so the results are the same
 * bit for bit. The private params and the opaque type are declared here. */

typedef struct {
    ALWAN_CORE_T domain_min_x, domain_min_y, domain_max_x, domain_max_y;
    size_t resolution_x, resolution_y;
} ALWAN_CORE_FNLIT(alwan__wlt_params);

#define ALWAN_WLT_CAT2_(a, b) a##b
#define ALWAN_WLT_CAT_(a, b)  ALWAN_WLT_CAT2_(a, b)
#define ALWAN_WLT_S       struct ALWAN_WLT_CAT_(ALWAN_CORE_FNLIT(alwan__wlt_table), _s)
#define ALWAN_WLT_PARAMS  ALWAN_CORE_FNLIT(alwan__wlt_params)
typedef ALWAN_WLT_S ALWAN_CORE_FNLIT(alwan__wlt_table);

/* What prepare builds and sample reads: the mode, the domain, the image's size,
 * and one table laid out as core/alwan_wavelength_table_core.inc says. */
ALWAN_WLT_S {
    ALWAN_CORE_T *table;
    size_t count;
    int nu, nv, ru, rv;
    alwan_wavelength_tabulated_mode mode;
    ALWAN_CORE_T x0, y0, x1, y1;
    ALWAN_CORE_T integral;
};

/* The CDF of n weights at w, written to c[0..n]: the running sum over the total, so
 * c[0] = 0 and c[n] = 1 exactly. A row whose weights are all 0 gets an even CDF.
 * Returns the sum. */
static ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan__wlt_cdf)(ALWAN_CORE_T const *w, ptrdiff_t w_step, ALWAN_CORE_T *c, int n) {
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
static void ALWAN_CORE_FNLIT(alwan__wlt_inverse)(ALWAN_CORE_T *dst, ALWAN_CORE_T const *cdf, int c_base, int n, int r) {
    int k;
    for (k = 0; k <= r; k++) {
        ALWAN_CORE_T const u = (ALWAN_CORE_T)k / (ALWAN_CORE_T)r;
        ALWAN_CORE_VEC2 const t = ALWAN_CORE_FNVLIT(alwan_wlt_search_1d)(cdf, c_base, n, u);
        dst[k] = t.v[0];
    }
}

/* Vose's alias table over the n weights at w (sum total), in double whatever the
 * precision: q_i = w_i n / total; each small q_i (below 1) is topped up from a large
 * one, which it names as its alias. What is left once one list empties is 1 up to
 * rounding and keeps its own cell. prob and idx are written as values. */
static alwan_status ALWAN_CORE_FNLIT(alwan__wlt_alias)(ALWAN_CORE_T *prob, ALWAN_CORE_T *idx, ALWAN_CORE_T const *w,
                                                        int n) {
    double *q = (double *)ALWAN_ALLOC(alwan_safe_array_size((size_t)n, sizeof(double)), sizeof(double));
    int *small = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)n, sizeof(int)), sizeof(int));
    int *large = (int *)ALWAN_ALLOC(alwan_safe_array_size((size_t)n, sizeof(int)), sizeof(int));
    double total = 0.0;
    int ns = 0, nl = 0, i;
    if (!q || !small || !large) {
        ALWAN_FREE(q); ALWAN_FREE(small); ALWAN_FREE(large);
        return ALWAN_E_NOMEM;
    }
    for (i = 0; i < n; i++) total += (double)w[i];
    for (i = 0; i < n; i++) {
        q[i] = (double)w[i] * (double)n / total;
        if (q[i] < 1.0) small[ns++] = i; else large[nl++] = i;
    }
    while (ns > 0 && nl > 0) {
        int const l = small[--ns];
        int const g = large[--nl];
        prob[l] = (ALWAN_CORE_T)q[l];
        idx[l] = (ALWAN_CORE_T)g;
        q[g] = (q[g] + q[l]) - 1.0;
        if (q[g] < 1.0) small[ns++] = g; else large[nl++] = g;
    }
    while (nl > 0) { int const g = large[--nl]; prob[g] = ALWAN_CORE_LITERAL(1.0); idx[g] = (ALWAN_CORE_T)g; }
    while (ns > 0) { int const l = small[--ns]; prob[l] = ALWAN_CORE_LITERAL(1.0); idx[l] = (ALWAN_CORE_T)l; }
    ALWAN_FREE(q); ALWAN_FREE(small); ALWAN_FREE(large);
    return ALWAN_OK;
}

static alwan_status ALWAN_CORE_FNLIT(alwan__wlt_prepare)(ALWAN_CORE_FNLIT(alwan__wlt_table) **out,
                                                                   ALWAN_CORE_T const *image, size_t row_stride,
                                                                   size_t width, size_t height,
                                                                   alwan_wavelength_tabulated_mode mode,
                                                                   ALWAN_WLT_PARAMS const *params, alwan_ctx *ctx) {
    ALWAN_WLT_S *s;
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
    if (mode != ALWAN_WAVELENGTH_TABULATED_DIRECT && mode != ALWAN_WAVELENGTH_TABULATED_SEARCH &&
        mode != ALWAN_WAVELENGTH_TABULATED_ALIAS) return ALWAN_E_INVALID;
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
    if (mode == ALWAN_WAVELENGTH_TABULATED_DIRECT) {
        if ((ru + 1) > 0x7fffffffu / height) return ALWAN_E_RANGE;
        count = height * (ru + 1) + rv + 1;
        if (count > 0x7fffffffu) return ALWAN_E_RANGE;
    } else if (mode == ALWAN_WAVELENGTH_TABULATED_ALIAS) {
        /* the alias of a cell is held as a value: exact below 2^24 in single precision */
        if (width * height > (sizeof(ALWAN_CORE_T) == 4 ? 16777216u : 0x7fffffffu / 3u)) return ALWAN_E_RANGE;
        count = 3 * width * height + 1;
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
        int const c_off = ALWAN_CORE_FNVLIT(alwan_wlt_off_cdf)(nu, nv);
        int const m_off = ALWAN_CORE_FNVLIT(alwan_wlt_off_mcdf)(nu, nv);
        ALWAN_CORE_T *rowsum = (ALWAN_CORE_T *)ALWAN_ALLOC(alwan_safe_array_size(height, sizeof(ALWAN_CORE_T)), sizeof(ALWAN_CORE_T));
        if (!rowsum) {
            ALWAN_FREE(work);
            return ALWAN_E_NOMEM;
        }
        for (j = 0; j < height; j++) {
            rowsum[j] = ALWAN_CORE_FNLIT(alwan__wlt_cdf)(work + j * width, 1, work + c_off + j * (width + 1), nu);
        }
        total = ALWAN_CORE_FNLIT(alwan__wlt_cdf)(rowsum, 1, work + m_off, nv);
        ALWAN_FREE(rowsum);
        if (!(total > ALWAN_CORE_LITERAL(0.0)) || !((double)total <= 1.7976931348623157e308)) {
            /* nothing to sample, or a sum past the precision's range */
            ALWAN_FREE(work);
            return total > ALWAN_CORE_LITERAL(0.0) ? ALWAN_E_RANGE : ALWAN_E_INVALID;
        }
        /* density per unit weight: 1 / (sum x cell area) */
        work[ALWAN_CORE_FNVLIT(alwan_wlt_off_norm)(nu, nv)] =
            ALWAN_CORE_LITERAL(1.0) / (total * ((x1 - x0) / (ALWAN_CORE_T)nu) * ((y1 - y0) / (ALWAN_CORE_T)nv));
    }

    s = (ALWAN_WLT_S *)ALWAN_ALLOC(sizeof(*s), sizeof(double));
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
    if (mode == ALWAN_WAVELENGTH_TABULATED_SEARCH) {
        s->table = work;
    } else if (mode == ALWAN_WAVELENGTH_TABULATED_ALIAS) {
        int const n = nu * nv;
        alwan_status st;
        s->table = (ALWAN_CORE_T *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(ALWAN_CORE_T)), sizeof(ALWAN_CORE_T));
        if (!s->table) {
            ALWAN_FREE(work);
            ALWAN_FREE(s);
            return ALWAN_E_NOMEM;
        }
        memcpy(s->table, work, (size_t)n * sizeof(ALWAN_CORE_T));
        s->table[ALWAN_CORE_FNVLIT(alwan_wlt_off_anorm)(n)] = work[ALWAN_CORE_FNVLIT(alwan_wlt_off_norm)(nu, nv)];
        st = ALWAN_CORE_FNLIT(alwan__wlt_alias)(s->table + ALWAN_CORE_FNVLIT(alwan_wlt_off_aprob)(n),
                                                 s->table + ALWAN_CORE_FNVLIT(alwan_wlt_off_aidx)(n), work, n);
        ALWAN_FREE(work);
        if (st != ALWAN_OK) {
            ALWAN_FREE(s->table);
            ALWAN_FREE(s);
            return st;
        }
    } else {
        int const c_off = ALWAN_CORE_FNVLIT(alwan_wlt_off_cdf)(nu, nv);
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
            ALWAN_CORE_FNLIT(alwan__wlt_inverse)(s->table + (size_t)r * (ru + 1), work, c_off + r * (nu + 1), nu, s->ru);
        }
        ALWAN_CORE_FNLIT(alwan__wlt_inverse)(s->table + ALWAN_CORE_FNVLIT(alwan_wlt_off_minv)(nv, s->ru), work,
                                              ALWAN_CORE_FNVLIT(alwan_wlt_off_mcdf)(nu, nv), nv, s->rv);
        ALWAN_FREE(work);
    }
    *out = (ALWAN_CORE_FNLIT(alwan__wlt_table) *)s;
    return ALWAN_OK;
}

static void ALWAN_CORE_FNLIT(alwan__wlt_destroy)(ALWAN_CORE_FNLIT(alwan__wlt_table) *sampler,
                                                            alwan_ctx *ctx) {
    ALWAN_WLT_S *s = (ALWAN_WLT_S *)sampler;
    (void)ctx;
    if (!s) return;
    ALWAN_FREE(s->table);
    ALWAN_FREE(s);
}

/* One sample, the cell it fell in written to off[0], off[1]. */
static void ALWAN_CORE_FNLIT(alwan__wlt_sample_one)(ALWAN_CORE_T *x, ALWAN_CORE_T *y, ALWAN_CORE_T *pdf, size_t *off,
                                                     ALWAN_WLT_S const *s, ALWAN_CORE_T u0, ALWAN_CORE_T u1) {
    ALWAN_CORE_VEC3 r;
    if (s->mode == ALWAN_WAVELENGTH_TABULATED_ALIAS) {
        r = ALWAN_CORE_FNVLIT(alwan_wlt_sample_2d_alias)(s->table, s->nu, s->nv, s->x0, s->y0, s->x1, s->y1, u0, u1);
        if (off) {
            ALWAN_CORE_VEC2 const pick = ALWAN_CORE_FNVLIT(alwan_wlt_alias_pick)(s->table, s->nu * s->nv, u1);
            int const c = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(pick.v[0], s->nu * s->nv);
            off[0] = (size_t)(c % s->nu);
            off[1] = (size_t)(c / s->nu);
        }
        *x = r.v[0];
        *y = r.v[1];
        *pdf = r.v[2];
        return;
    }
    if (s->mode == ALWAN_WAVELENGTH_TABULATED_SEARCH) {
        r = ALWAN_CORE_FNVLIT(alwan_wlt_sample_2d_search)(s->table, s->nu, s->nv, s->x0, s->y0, s->x1, s->y1, u0, u1);
    } else {
        r = ALWAN_CORE_FNVLIT(alwan_wlt_sample_2d_direct)(s->table, s->nu, s->nv, s->ru, s->rv, s->x0, s->y0, s->x1, s->y1,
                                                           u0, u1);
    }
    if (off) {
        /* the cells the sampler chose, recovered the way it chose them */
        if (s->mode == ALWAN_WAVELENGTH_TABULATED_SEARCH) {
            ALWAN_CORE_VEC2 const sy = ALWAN_CORE_FNVLIT(alwan_wlt_search_1d)(s->table, ALWAN_CORE_FNVLIT(alwan_wlt_off_mcdf)(s->nu, s->nv),
                                                                               s->nv, u1);
            int const iv = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(sy.v[1], s->nv);
            ALWAN_CORE_VEC2 const sx = ALWAN_CORE_FNVLIT(alwan_wlt_search_1d)(
                s->table, ALWAN_CORE_FNVLIT(alwan_wlt_off_cdf)(s->nu, s->nv) + iv * (s->nu + 1), s->nu, u0);
            off[0] = (size_t)ALWAN_CORE_FNVLIT(alwan_wlt_cell)(sx.v[1], s->nu);
            off[1] = (size_t)iv;
        } else {
            ALWAN_CORE_VEC2 const sy = ALWAN_CORE_FNVLIT(alwan_wlt_direct_1d)(s->table, ALWAN_CORE_FNVLIT(alwan_wlt_off_minv)(s->nv, s->ru),
                                                                               s->rv, u1, s->y1 - s->y0);
            int const iv = ALWAN_CORE_FNVLIT(alwan_wlt_cell)(sy.v[0] * (ALWAN_CORE_T)s->nv, s->nv);
            ALWAN_CORE_VEC2 const sx = ALWAN_CORE_FNVLIT(alwan_wlt_direct_1d)(s->table, iv * (s->ru + 1), s->ru, u0, s->x1 - s->x0);
            off[0] = (size_t)ALWAN_CORE_FNVLIT(alwan_wlt_cell)(sx.v[0] * (ALWAN_CORE_T)s->nu, s->nu);
            off[1] = (size_t)iv;
        }
    }
    *x = r.v[0];
    *y = r.v[1];
    *pdf = r.v[2];
}

static int ALWAN_CORE_FNLIT(alwan__wlt_u_ok)(ALWAN_CORE_T u) {
    return u >= ALWAN_CORE_LITERAL(0.0) && u <= ALWAN_CORE_LITERAL(1.0);
}

static alwan_status ALWAN_CORE_FNLIT(alwan__wlt_sample)(ALWAN_CORE_VEC2 *xy_out, ALWAN_CORE_T *pdf_out,
                                                                  size_t *offset_out, ALWAN_CORE_VEC2 const *u,
                                                                  ALWAN_CORE_FNLIT(alwan__wlt_table) const *sampler) {
    ALWAN_WLT_S const *s = (ALWAN_WLT_S const *)sampler;
    ALWAN_CORE_T x, y, pdf;
    if (!xy_out || !u || !s) return ALWAN_E_INVALID;
    if (!ALWAN_CORE_FNLIT(alwan__wlt_u_ok)(u->v[0]) || !ALWAN_CORE_FNLIT(alwan__wlt_u_ok)(u->v[1])) return ALWAN_E_RANGE;
    ALWAN_CORE_FNLIT(alwan__wlt_sample_one)(&x, &y, &pdf, offset_out, s, u->v[0], u->v[1]);
    xy_out->v[0] = x;
    xy_out->v[1] = y;
    if (pdf_out) *pdf_out = pdf;
    return ALWAN_OK;
}

static alwan_status ALWAN_CORE_FNLIT(alwan__wlt_pdf)(ALWAN_CORE_T *pdf_out, ALWAN_CORE_VEC2 const *xy,
                                                               ALWAN_CORE_FNLIT(alwan__wlt_table) const *sampler) {
    ALWAN_WLT_S const *s = (ALWAN_WLT_S const *)sampler;
    if (!pdf_out || !xy || !s) return ALWAN_E_INVALID;
    if (s->mode == ALWAN_WAVELENGTH_TABULATED_ALIAS) {
        *pdf_out = ALWAN_CORE_FNVLIT(alwan_wlt_pdf_2d_alias)(s->table, s->nu, s->nv, s->x0, s->y0, s->x1, s->y1,
                                                              xy->v[0], xy->v[1]);
    } else if (s->mode == ALWAN_WAVELENGTH_TABULATED_SEARCH) {
        *pdf_out = ALWAN_CORE_FNVLIT(alwan_wlt_pdf_2d_search)(s->table, s->nu, s->nv, s->x0, s->y0, s->x1, s->y1,
                                                               xy->v[0], xy->v[1]);
    } else {
        *pdf_out = ALWAN_CORE_FNVLIT(alwan_wlt_pdf_2d_direct)(s->table, s->nu, s->nv, s->ru, s->rv, s->x0, s->y0, s->x1,
                                                               s->y1, xy->v[0], xy->v[1]);
    }
    return ALWAN_OK;
}

static alwan_status ALWAN_CORE_FNLIT(alwan__wlt_invert)(ALWAN_CORE_VEC2 *u_out, ALWAN_CORE_VEC2 const *xy,
                                                                  ALWAN_CORE_FNLIT(alwan__wlt_table) const *sampler) {
    ALWAN_WLT_S const *s = (ALWAN_WLT_S const *)sampler;
    ALWAN_CORE_VEC2 r;
    if (!u_out || !xy || !s) return ALWAN_E_INVALID;
    /* the alias map scatters u: no monotone inverse to give */
    if (s->mode == ALWAN_WAVELENGTH_TABULATED_ALIAS) return ALWAN_E_INVALID;
    if (s->mode == ALWAN_WAVELENGTH_TABULATED_SEARCH) {
        r = ALWAN_CORE_FNVLIT(alwan_wlt_invert_2d_search)(s->table, s->nu, s->nv, s->x0, s->y0, s->x1, s->y1,
                                                           xy->v[0], xy->v[1]);
    } else {
        r = ALWAN_CORE_FNVLIT(alwan_wlt_invert_2d_direct)(s->table, s->nu, s->nv, s->ru, s->rv, s->x0, s->y0, s->x1,
                                                           s->y1, xy->v[0], xy->v[1]);
    }
    if (r.v[0] < ALWAN_CORE_LITERAL(0.0) || r.v[1] < ALWAN_CORE_LITERAL(0.0)) return ALWAN_E_RANGE;
    *u_out = r;
    return ALWAN_OK;
}

#undef ALWAN_WLT_S
#undef ALWAN_WLT_CAT_
#undef ALWAN_WLT_CAT2_
#undef ALWAN_WLT_PARAMS
