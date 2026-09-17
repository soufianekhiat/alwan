/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Film halation, templated per precision. Included once per precision from
 * experimental/alwan_film_halation.c.
 *
 * The kernel is built in double whatever the precision, because it is a few
 * thousand values computed once, and stored in the precision of the image. The
 * convolution then runs in the image's precision.
 */

/* Unpolarised Fresnel reflectance for light inside a medium of index n meeting
 * air at internal angle theta; 1 past the critical angle. Hecht, Optics, 4.6. */
static double ALWAN_CORE_FN(halation_fresnel)(double n, double cos_i, double sin_i) {
    double const sin_t = n * sin_i;
    double cos_t, rs, rp;
    if (sin_t >= 1.0) return 1.0;
    cos_t = sqrt(1.0 - sin_t * sin_t);
    rs = (n * cos_i - cos_t) / (n * cos_i + cos_t);
    rp = (cos_i - n * cos_t) / (cos_i + n * cos_t);
    return 0.5 * (rs * rs + rp * rp);
}

/* K(r) before normalisation: R(theta) cos^4(theta), with tan(theta) = r / 2d and
 * 2d = r_c sqrt(n^2 - 1), so the rim lands at r_c whatever the thickness. */
static double ALWAN_CORE_FN(halation_profile)(double r, double rim, double n) {
    double const two_d = rim * sqrt(n * n - 1.0);
    double const t = r / two_d;
    double const cos_i = 1.0 / sqrt(1.0 + t * t);
    double const sin_i = t * cos_i;
    double const c2 = cos_i * cos_i;
    return ALWAN_CORE_FN(halation_fresnel)(n, cos_i, sin_i) * c2 * c2;
}

static int ALWAN_CORE_FN(halation_params_ok)(alwan_film_halation_params const *p) {
    return p && p->rim_radius > 0.0 && p->rim_radius == p->rim_radius && p->index > 1.0 && p->index < 4.0 &&
           p->reach >= 1.0 && p->reach <= 16.0 && p->strength[0] >= 0.0 && p->strength[1] >= 0.0 &&
           p->strength[2] >= 0.0;
}

static size_t ALWAN_CORE_FN(halation_radius)(alwan_film_halation_params const *p) {
    double const r = ceil(p->reach * p->rim_radius);
    return r < 1.0 ? 1 : (size_t)r;
}

static void *ALWAN_CORE_FN(halation_alloc)(alwan_ctx *ctx, size_t count, size_t elem) {
    size_t const bytes = alwan_safe_array_size(count, elem);
    if (bytes == 0) return NULL;
    return ctx ? ctx->alloc_fn(bytes, sizeof(ALWAN_CORE_T)) : ALWAN_ALLOC(bytes, sizeof(ALWAN_CORE_T));
}

static void ALWAN_CORE_FN(halation_free)(alwan_ctx *ctx, void *p) {
    if (ctx) ctx->free_fn(p); else ALWAN_FREE(p);
}

alwan_status ALWAN_CORE_FNLIT(alwan_film_halation_kernel)(ALWAN_CORE_T *kernel_out, size_t *radius_out,
                                                          alwan_film_halation_params const *params) {
    size_t R, n, i, j;
    double sum = 0.0;
    if (!ALWAN_CORE_FN(halation_params_ok)(params)) return ALWAN_E_INVALID;
    R = ALWAN_CORE_FN(halation_radius)(params);
    if (radius_out) *radius_out = R;
    if (!kernel_out) return ALWAN_OK;
    n = 2 * R + 1;
    for (j = 0; j < n; j++) {
        for (i = 0; i < n; i++) {
            double const dx = (double)i - (double)R, dy = (double)j - (double)R;
            double const v = ALWAN_CORE_FN(halation_profile)(sqrt(dx * dx + dy * dy), params->rim_radius, params->index);
            kernel_out[j * n + i] = (ALWAN_CORE_T)v;
            sum += v;
        }
    }
    if (!(sum > 0.0)) return ALWAN_E_RANGE;
    for (i = 0; i < n * n; i++) kernel_out[i] = (ALWAN_CORE_T)((double)kernel_out[i] / sum);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_film_halation)(ALWAN_CORE_T *exposure, size_t row_stride, size_t width, size_t height,
                                                   alwan_film_halation_params const *params, alwan_ctx *ctx) {
    size_t R, n, x, y, i, j;
    ALWAN_CORE_T *kernel, *src;
    alwan_status st;
    int c;
    if (!exposure || width == 0 || height == 0 || !ALWAN_CORE_FN(halation_params_ok)(params)) return ALWAN_E_INVALID;
    if (row_stride < width * 3 * sizeof(ALWAN_CORE_T)) return ALWAN_E_INVALID;
    R = ALWAN_CORE_FN(halation_radius)(params);
    n = 2 * R + 1;
    kernel = (ALWAN_CORE_T *)ALWAN_CORE_FN(halation_alloc)(ctx, n * n, sizeof(ALWAN_CORE_T));
    if (!kernel) return ALWAN_E_NOMEM;
    st = ALWAN_CORE_FNLIT(alwan_film_halation_kernel)(kernel, NULL, params);
    if (st != ALWAN_OK) {
        ALWAN_CORE_FN(halation_free)(ctx, kernel);
        return st;
    }
    /* The convolution reads the exposure before any of it was changed. */
    src = (ALWAN_CORE_T *)ALWAN_CORE_FN(halation_alloc)(ctx, width * height * 3, sizeof(ALWAN_CORE_T));
    if (!src) {
        ALWAN_CORE_FN(halation_free)(ctx, kernel);
        return ALWAN_E_NOMEM;
    }
    for (y = 0; y < height; y++) {
        ALWAN_CORE_T const *row = (ALWAN_CORE_T const *)((char const *)exposure + y * row_stride);
        for (x = 0; x < width * 3; x++) src[y * width * 3 + x] = row[x];
    }
    for (y = 0; y < height; y++) {
        ALWAN_CORE_T *row = (ALWAN_CORE_T *)((char *)exposure + y * row_stride);
        for (x = 0; x < width; x++) {
            ALWAN_CORE_T acc[3] = { ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0) };
            for (j = 0; j < n; j++) {
                /* Edges replicated: the tap's row and column are clamped into the image. */
                long sy = (long)y + (long)j - (long)R;
                ALWAN_CORE_T const *srow;
                if (sy < 0) sy = 0;
                if (sy >= (long)height) sy = (long)height - 1;
                srow = src + (size_t)sy * width * 3;
                for (i = 0; i < n; i++) {
                    long sx = (long)x + (long)i - (long)R;
                    ALWAN_CORE_T const w = kernel[j * n + i];
                    if (sx < 0) sx = 0;
                    if (sx >= (long)width) sx = (long)width - 1;
                    acc[0] += w * srow[(size_t)sx * 3 + 0];
                    acc[1] += w * srow[(size_t)sx * 3 + 1];
                    acc[2] += w * srow[(size_t)sx * 3 + 2];
                }
            }
            for (c = 0; c < 3; c++) row[x * 3 + c] += (ALWAN_CORE_T)params->strength[c] * acc[c];
        }
    }
    ALWAN_CORE_FN(halation_free)(ctx, src);
    ALWAN_CORE_FN(halation_free)(ctx, kernel);
    return ALWAN_OK;
}
