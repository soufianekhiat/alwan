/* No include guard -- included twice (f32 and f64 passes) */

#define ALWAN_ENVP_CAT2_(a, b) a##b
#define ALWAN_ENVP_CAT_(a, b)  ALWAN_ENVP_CAT2_(a, b)
#define ALWAN_ENVP_S       struct ALWAN_ENVP_CAT_(ALWAN_CORE_FNLIT(alwan_env_product_sampler), _s)
#define ALWAN_ENVP_LOBE    ALWAN_CORE_FNLIT(alwan_env_lobe)
#define ALWAN_ENVP_LAYOUT  ALWAN_CORE_FNLIT(alwan_env_product_sampler_layout)

/* What prepare builds: the map's size, its padding to powers of two, and the
 * pyramid of sums laid out as core/alwan_env_sampling_core.inc says. */
ALWAN_ENVP_S {
    ALWAN_CORE_T *table;
    size_t count;
    int w, h, la, lb;
};

/* The lobe a caller passes, checked and unit length: kind, axis, exponent. NULL
 * is no lobe (the map's light alone). */
static alwan_status ALWAN_CORE_FNLIT(alwan__env_lobe)(ALWAN_ENVP_LOBE const *lobe, int *kind, ALWAN_CORE_T *ax,
                                                      ALWAN_CORE_T *ay, ALWAN_CORE_T *az, ALWAN_CORE_T *e) {
    double len;
    if (!lobe) {
        *kind = ALWAN_ENV_LOBE_KIND_NONE;
        *ax = *ay = *az = ALWAN_CORE_LITERAL(0.0);
        *e = ALWAN_CORE_LITERAL(1.0);
        return ALWAN_OK;
    }
    if (lobe->kind != ALWAN_ENV_LOBE_COSINE && lobe->kind != ALWAN_ENV_LOBE_PHONG && lobe->kind != ALWAN_ENV_LOBE_GGX) {
        return ALWAN_E_INVALID;
    }
    len = ALWAN_SQRT((double)lobe->axis_x * lobe->axis_x + (double)lobe->axis_y * lobe->axis_y +
                     (double)lobe->axis_z * lobe->axis_z);
    if (!(len > 0.0 && len <= 1.7976931348623157e308)) return ALWAN_E_INVALID;
    *kind = lobe->kind == ALWAN_ENV_LOBE_PHONG ? ALWAN_ENV_LOBE_KIND_PHONG
          : lobe->kind == ALWAN_ENV_LOBE_GGX ? ALWAN_ENV_LOBE_KIND_GGX : ALWAN_ENV_LOBE_KIND_COSINE;
    *ax = (ALWAN_CORE_T)(lobe->axis_x / len);
    *ay = (ALWAN_CORE_T)(lobe->axis_y / len);
    *az = (ALWAN_CORE_T)(lobe->axis_z / len);
    *e = ALWAN_CORE_LITERAL(1.0);
    if (*kind == ALWAN_ENV_LOBE_KIND_PHONG && lobe->exponent != ALWAN_CORE_LITERAL(0.0)) {
        if (!(lobe->exponent > ALWAN_CORE_LITERAL(0.0) && (double)lobe->exponent <= 1.0e6)) return ALWAN_E_INVALID;
        *e = lobe->exponent;
    }
    if (*kind == ALWAN_ENV_LOBE_KIND_GGX) {
        /* alpha, 0 for 0.25; above 1 the distribution peaks at grazing, not at the axis */
        *e = ALWAN_CORE_LITERAL(0.25);
        if (lobe->alpha != ALWAN_CORE_LITERAL(0.0)) {
            if (!(lobe->alpha > ALWAN_CORE_LITERAL(0.0) && lobe->alpha <= ALWAN_CORE_LITERAL(1.0))) return ALWAN_E_INVALID;
            *e = lobe->alpha;
        }
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_env_equirect_direction)(ALWAN_CORE_VEC3 *dir_out, ALWAN_CORE_VEC2 const *xy) {
    if (!dir_out || !xy) return ALWAN_E_INVALID;
    if (!(xy->v[0] >= ALWAN_CORE_LITERAL(0.0) && xy->v[0] <= ALWAN_CORE_LITERAL(1.0) &&
          xy->v[1] >= ALWAN_CORE_LITERAL(0.0) && xy->v[1] <= ALWAN_CORE_LITERAL(1.0))) return ALWAN_E_RANGE;
    *dir_out = ALWAN_CORE_FNVLIT(alwan_env_direction)(xy->v[0], xy->v[1]);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_env_equirect_position)(ALWAN_CORE_VEC2 *xy_out, ALWAN_CORE_VEC3 const *dir) {
    double len2;
    if (!xy_out || !dir) return ALWAN_E_INVALID;
    len2 = (double)dir->v[0] * dir->v[0] + (double)dir->v[1] * dir->v[1] + (double)dir->v[2] * dir->v[2];
    if (!(len2 > 0.0 && len2 <= 1.7976931348623157e308)) return ALWAN_E_INVALID;
    *xy_out = ALWAN_CORE_FNVLIT(alwan_env_position)(dir->v[0], dir->v[1], dir->v[2]);
    return ALWAN_OK;
}

/* Pixel (i, j) of a w x h map: its weight, luminance x sin theta at its centre x
 * the lobe's bound over it. */
static ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan__env_pixel_weight)(ALWAN_CORE_T lum, size_t i, size_t j, size_t w, size_t h,
                                                              int kind, ALWAN_CORE_T ax, ALWAN_CORE_T ay,
                                                              ALWAN_CORE_T az, ALWAN_CORE_T e) {
    ALWAN_CORE_T const theta = ALWAN_CORE_PI * ((ALWAN_CORE_T)j + ALWAN_CORE_LITERAL(0.5)) / (ALWAN_CORE_T)h;
    ALWAN_CORE_T b;
    if (!(lum > ALWAN_CORE_LITERAL(0.0))) return ALWAN_CORE_LITERAL(0.0);
    b = ALWAN_CORE_FNVLIT(alwan_env_lobe_weight)((ALWAN_CORE_T)i / (ALWAN_CORE_T)w, (ALWAN_CORE_T)(i + 1) / (ALWAN_CORE_T)w,
                                                (ALWAN_CORE_T)j / (ALWAN_CORE_T)h, (ALWAN_CORE_T)(j + 1) / (ALWAN_CORE_T)h,
                                                kind, ax, ay, az, e);
    return lum * ALWAN_CORE_SIN(theta) * b;
}

alwan_status ALWAN_CORE_FNLIT(alwan_env_weight_equirect)(ALWAN_CORE_T *out, size_t out_row_stride,
                                                        ALWAN_CORE_T const *luminance, size_t row_stride, size_t width,
                                                        size_t height, ALWAN_ENVP_LOBE const *lobe) {
    int kind;
    ALWAN_CORE_T ax, ay, az, e;
    size_t i, j;
    alwan_status st;
    if (!out || !luminance || width == 0 || height == 0) return ALWAN_E_INVALID;
    st = ALWAN_CORE_FNLIT(alwan__env_lobe)(lobe, &kind, &ax, &ay, &az, &e);
    if (st != ALWAN_OK) return st;
    if (row_stride == 0) row_stride = width * sizeof(ALWAN_CORE_T);
    if (out_row_stride == 0) out_row_stride = width * sizeof(ALWAN_CORE_T);
    if (row_stride < width * sizeof(ALWAN_CORE_T) || out_row_stride < width * sizeof(ALWAN_CORE_T)) return ALWAN_E_INVALID;
    for (j = 0; j < height; j++) {
        ALWAN_CORE_T const *lr = (ALWAN_CORE_T const *)((unsigned char const *)luminance + j * row_stride);
        ALWAN_CORE_T *orow = (ALWAN_CORE_T *)((unsigned char *)out + j * out_row_stride);
        for (i = 0; i < width; i++) {
            ALWAN_CORE_T const v = lr[i];
            if (!(v >= ALWAN_CORE_LITERAL(0.0) && (double)v <= 1.7976931348623157e308)) return ALWAN_E_INVALID;
            orow[i] = ALWAN_CORE_FNLIT(alwan__env_pixel_weight)(v, i, j, width, height, kind, ax, ay, az, e);
        }
    }
    return ALWAN_OK;
}

static int ALWAN_CORE_FNLIT(alwan__env_log2_ceil)(size_t n) {
    int lg = 0;
    while (((size_t)1 << lg) < n) lg++;
    return lg;
}

alwan_status ALWAN_CORE_FNLIT(alwan_env_product_sampler_prepare)(ALWAN_CORE_FNLIT(alwan_env_product_sampler) **out,
                                                                ALWAN_CORE_T const *luminance, size_t row_stride,
                                                                size_t width, size_t height, alwan_ctx *ctx) {
    ALWAN_ENVP_S *s;
    int la, lb, top, l, w, h;
    size_t count = 0, i, j;
    double total = 0.0;
    (void)ctx;
    if (!out) return ALWAN_E_INVALID;
    *out = NULL;
    if (!luminance || width == 0 || height == 0) return ALWAN_E_INVALID;
    if (width > 32768 || height > 32768) return ALWAN_E_RANGE;
    if (row_stride == 0) row_stride = width * sizeof(ALWAN_CORE_T);
    if (row_stride < width * sizeof(ALWAN_CORE_T)) return ALWAN_E_INVALID;
    w = (int)width; h = (int)height;
    la = ALWAN_CORE_FNLIT(alwan__env_log2_ceil)(width);
    lb = ALWAN_CORE_FNLIT(alwan__env_log2_ceil)(height);
    top = la > lb ? la : lb;
    for (l = 0; l <= top; l++) {
        count += (size_t)ALWAN_CORE_FNVLIT(alwan_envp_dim)(la, l) * (size_t)ALWAN_CORE_FNVLIT(alwan_envp_dim)(lb, l);
    }
    if (count > 0x7fffffffu) return ALWAN_E_RANGE;

    s = (ALWAN_ENVP_S *)ALWAN_ALLOC(sizeof(*s), sizeof(double));
    if (!s) return ALWAN_E_NOMEM;
    memset(s, 0, sizeof(*s));
    s->table = (ALWAN_CORE_T *)ALWAN_ALLOC(alwan_safe_array_size(count, sizeof(ALWAN_CORE_T)), sizeof(ALWAN_CORE_T));
    if (!s->table) {
        ALWAN_FREE(s);
        return ALWAN_E_NOMEM;
    }
    memset(s->table, 0, count * sizeof(ALWAN_CORE_T));
    s->count = count;
    s->w = w; s->h = h; s->la = la; s->lb = lb;
    /* the leaves: luminance x sin theta, the padding 0 */
    {
        int const dw = ALWAN_CORE_FNVLIT(alwan_envp_dim)(la, 0);
        for (j = 0; j < height; j++) {
            ALWAN_CORE_T const *lr = (ALWAN_CORE_T const *)((unsigned char const *)luminance + j * row_stride);
            for (i = 0; i < width; i++) {
                ALWAN_CORE_T const v = lr[i];
                ALWAN_CORE_T wt;
                if (!(v >= ALWAN_CORE_LITERAL(0.0) && (double)v <= 1.7976931348623157e308)) {
                    ALWAN_FREE(s->table);
                    ALWAN_FREE(s);
                    return ALWAN_E_INVALID;
                }
                wt = ALWAN_CORE_FNLIT(alwan__env_pixel_weight)(v, i, j, width, height, ALWAN_ENV_LOBE_KIND_NONE,
                                                               ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0),
                                                               ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
                s->table[j * (size_t)dw + i] = wt;
                total += (double)wt;
            }
        }
    }
    if (!(total > 0.0) || !(total <= 1.7976931348623157e308)) {
        ALWAN_FREE(s->table);
        ALWAN_FREE(s);
        return total > 0.0 ? ALWAN_E_RANGE : ALWAN_E_INVALID;
    }
    /* each level the sums of the one below, added in double */
    for (l = 1; l <= top; l++) {
        int const dw = ALWAN_CORE_FNVLIT(alwan_envp_dim)(la, l), dh = ALWAN_CORE_FNVLIT(alwan_envp_dim)(lb, l);
        int const cw = ALWAN_CORE_FNVLIT(alwan_envp_dim)(la, l - 1), chh = ALWAN_CORE_FNVLIT(alwan_envp_dim)(lb, l - 1);
        int const fx = cw / dw, fy = chh / dh;
        ALWAN_CORE_T *dst = s->table + ALWAN_CORE_FNVLIT(alwan_envp_level_off)(la, lb, l);
        ALWAN_CORE_T const *src = s->table + ALWAN_CORE_FNVLIT(alwan_envp_level_off)(la, lb, l - 1);
        int x, y, a, b;
        for (y = 0; y < dh; y++) {
            for (x = 0; x < dw; x++) {
                double acc = 0.0;
                for (b = 0; b < fy; b++) {
                    for (a = 0; a < fx; a++) acc += (double)src[(size_t)(y * fy + b) * (size_t)cw + (size_t)(x * fx + a)];
                }
                dst[(size_t)y * (size_t)dw + (size_t)x] = (ALWAN_CORE_T)acc;
            }
        }
    }
    *out = (ALWAN_CORE_FNLIT(alwan_env_product_sampler) *)s;
    return ALWAN_OK;
}

void ALWAN_CORE_FNLIT(alwan_env_product_sampler_destroy)(ALWAN_CORE_FNLIT(alwan_env_product_sampler) *sampler,
                                                         alwan_ctx *ctx) {
    ALWAN_ENVP_S *s = (ALWAN_ENVP_S *)sampler;
    (void)ctx;
    if (!s) return;
    ALWAN_FREE(s->table);
    ALWAN_FREE(s);
}

/* A map-area density as a solid-angle one at polar position y. */
static ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan__env_solid_angle_pdf)(ALWAN_CORE_T pdf_xy, ALWAN_CORE_T y) {
    ALWAN_CORE_T st = ALWAN_CORE_SIN(ALWAN_CORE_PI * y);
    if (!(pdf_xy > ALWAN_CORE_LITERAL(0.0))) return ALWAN_CORE_LITERAL(0.0);
    if (st < ALWAN_CORE_LITERAL(1e-30)) st = ALWAN_CORE_LITERAL(1e-30);
    return pdf_xy / (ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI * ALWAN_CORE_PI * st);
}

alwan_status ALWAN_CORE_FNLIT(alwan_env_product_sampler_sample)(ALWAN_CORE_VEC3 *dir_out, ALWAN_CORE_VEC2 *xy_out,
                                                               ALWAN_CORE_T *pdf_out, ALWAN_CORE_VEC2 const *u,
                                                               ALWAN_ENVP_LOBE const *lobe,
                                                               ALWAN_CORE_FNLIT(alwan_env_product_sampler) const *sampler) {
    ALWAN_ENVP_S const *s = (ALWAN_ENVP_S const *)sampler;
    int kind;
    ALWAN_CORE_T ax, ay, az, e;
    ALWAN_CORE_VEC3 r;
    alwan_status st;
    if (!dir_out || !pdf_out || !u || !s) return ALWAN_E_INVALID;
    if (!(u->v[0] >= ALWAN_CORE_LITERAL(0.0) && u->v[0] <= ALWAN_CORE_LITERAL(1.0) &&
          u->v[1] >= ALWAN_CORE_LITERAL(0.0) && u->v[1] <= ALWAN_CORE_LITERAL(1.0))) return ALWAN_E_RANGE;
    st = ALWAN_CORE_FNLIT(alwan__env_lobe)(lobe, &kind, &ax, &ay, &az, &e);
    if (st != ALWAN_OK) return st;
    r = ALWAN_CORE_FNVLIT(alwan_envp_sample)(s->table, s->w, s->h, s->la, s->lb, kind, ax, ay, az, e, u->v[0], u->v[1]);
    if (xy_out) {
        xy_out->v[0] = r.v[0];
        xy_out->v[1] = r.v[1];
    }
    *dir_out = ALWAN_CORE_FNVLIT(alwan_env_direction)(r.v[0], r.v[1]);
    *pdf_out = ALWAN_CORE_FNLIT(alwan__env_solid_angle_pdf)(r.v[2], r.v[1]);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_env_product_sampler_pdf)(ALWAN_CORE_T *pdf_out, ALWAN_CORE_VEC3 const *dir,
                                                            ALWAN_ENVP_LOBE const *lobe,
                                                            ALWAN_CORE_FNLIT(alwan_env_product_sampler) const *sampler) {
    ALWAN_ENVP_S const *s = (ALWAN_ENVP_S const *)sampler;
    int kind;
    ALWAN_CORE_T ax, ay, az, e;
    ALWAN_CORE_VEC2 p;
    alwan_status st;
    if (!pdf_out || !dir || !s) return ALWAN_E_INVALID;
    st = ALWAN_CORE_FNLIT(alwan_env_equirect_position)(&p, dir);
    if (st != ALWAN_OK) return st;
    st = ALWAN_CORE_FNLIT(alwan__env_lobe)(lobe, &kind, &ax, &ay, &az, &e);
    if (st != ALWAN_OK) return st;
    *pdf_out = ALWAN_CORE_FNLIT(alwan__env_solid_angle_pdf)(
        ALWAN_CORE_FNVLIT(alwan_envp_pdf)(s->table, s->w, s->h, s->la, s->lb, kind, ax, ay, az, e, p.v[0], p.v[1]), p.v[1]);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_env_product_sampler_get_layout)(ALWAN_ENVP_LAYOUT *out,
                                                                   ALWAN_CORE_FNLIT(alwan_env_product_sampler) const *sampler) {
    ALWAN_ENVP_S const *s = (ALWAN_ENVP_S const *)sampler;
    if (!out || !s) return ALWAN_E_INVALID;
    out->table = s->table;
    out->table_count = s->count;
    out->width = s->w;
    out->height = s->h;
    out->log2_width = s->la;
    out->log2_height = s->lb;
    return ALWAN_OK;
}

#undef ALWAN_ENVP_S
#undef ALWAN_ENVP_CAT_
#undef ALWAN_ENVP_CAT2_
#undef ALWAN_ENVP_LOBE
#undef ALWAN_ENVP_LAYOUT
