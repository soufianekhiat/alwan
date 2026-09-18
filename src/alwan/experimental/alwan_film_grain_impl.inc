/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Film grain, templated per precision. Included once per precision from
 * experimental/alwan_film_grain.c, which holds the generator.
 *
 * The rendering is the paper's: for each output pixel and each of `samples`
 * points, jitter the point by a Gaussian, find the input cells within the
 * largest disc radius of it, regenerate each cell's discs from the cell's own
 * seeded stream, and count the point covered if it falls inside any. The value
 * is covered over samples. Cells are regenerated per sample rather than stored,
 * which keeps memory at one copy of the image whatever the disc count.
 */

static int ALWAN_CORE_FN(grain_params_ok)(alwan_film_grain_params const *p) {
    return p && p->radius > 0.0 && p->radius == p->radius && p->radius <= 64.0 && p->radius_sigma >= 0.0 &&
           p->radius_sigma <= p->radius && p->filter_sigma >= 0.0 && p->filter_sigma <= 16.0 && p->samples >= 1 &&
           p->samples <= 65536;
}

/* Coverage of one continuous point (x, y) in input pixel units by the discs of the
 * cells around it. Cells past the edges exist, with the edge cell's value and a
 * stream of their own, so a border pixel sees as many discs as an interior one and
 * the field keeps its mean up to the edge. */
static int ALWAN_CORE_FN(grain_covered)(double x, double y, ALWAN_CORE_T const *src, size_t width, size_t height,
                                        int channels, int channel, alwan_film_grain_params const *p, double r_max,
                                        double inv_pi_er2, double ln_mu, double ln_sigma) {
    long const i0 = (long)floor(x - r_max), i1 = (long)floor(x + r_max);
    long const j0 = (long)floor(y - r_max), j1 = (long)floor(y + r_max);
    long i, j;
    for (j = j0; j <= j1; j++) {
        long const cj = j < 0 ? 0 : (j >= (long)height ? (long)height - 1 : j);
        for (i = i0; i <= i1; i++) {
            long const ci = i < 0 ? 0 : (i >= (long)width ? (long)width - 1 : i);
            alwan__grain_rng g;
            double u, lambda;
            unsigned n, k;
            u = (double)src[((size_t)cj * width + (size_t)ci) * (size_t)channels + (size_t)channel];
            if (u <= 0.0) continue;
            if (u > 0.9999) u = 0.9999;
            lambda = -log(1.0 - u) * inv_pi_er2;
            alwan__grain_seed(&g, (uint64_t)p->seed, i, j, (uint64_t)channel);
            n = alwan__grain_poisson(&g, lambda);
            for (k = 0; k < n; k++) {
                double const cx = (double)i + alwan__grain_uniform(&g);
                double const cy = (double)j + alwan__grain_uniform(&g);
                double r = p->radius;
                double dx, dy;
                if (p->radius_sigma > 0.0) r = exp(ln_mu + ln_sigma * alwan__grain_normal(&g));
                dx = x - cx;
                dy = y - cy;
                if (dx * dx + dy * dy <= r * r) return 1;
            }
        }
    }
    return 0;
}

alwan_status ALWAN_CORE_FNLIT(alwan_film_grain)(ALWAN_CORE_T *image, size_t row_stride, size_t width, size_t height,
                                                int channels, alwan_film_grain_params const *params, alwan_ctx *ctx) {
    ALWAN_CORE_T *src;
    size_t bytes, x, y;
    int c;
    double er2, inv_pi_er2, r_max, ln_mu = 0.0, ln_sigma = 0.0;
    if (!image || width == 0 || height == 0 || channels < 1 || channels > 4 || !ALWAN_CORE_FN(grain_params_ok)(params)) {
        return ALWAN_E_INVALID;
    }
    if (row_stride < width * (size_t)channels * sizeof(ALWAN_CORE_T)) return ALWAN_E_INVALID;
    bytes = alwan_safe_array_size(width * height, (size_t)channels * sizeof(ALWAN_CORE_T));
    if (bytes == 0) return ALWAN_E_NOMEM;
    src = (ALWAN_CORE_T *)(ctx ? ctx->alloc_fn(bytes, sizeof(ALWAN_CORE_T)) : ALWAN_ALLOC(bytes, sizeof(ALWAN_CORE_T)));
    if (!src) return ALWAN_E_NOMEM;
    for (y = 0; y < height; y++) {
        ALWAN_CORE_T const *row = (ALWAN_CORE_T const *)((char const *)image + y * row_stride);
        for (x = 0; x < width * (size_t)channels; x++) {
            ALWAN_CORE_T v = row[x];
            if (!(v > ALWAN_CORE_LITERAL(0.0))) v = ALWAN_CORE_LITERAL(0.0);
            if (v > ALWAN_CORE_LITERAL(1.0)) v = ALWAN_CORE_LITERAL(1.0);
            src[y * width * (size_t)channels + x] = v;
        }
    }
    /* E[R^2] of the radius law, and the largest radius a disc can have, three
     * standard deviations up the log-normal. */
    er2 = params->radius * params->radius + params->radius_sigma * params->radius_sigma;
    inv_pi_er2 = 1.0 / (3.141592653589793 * er2);
    r_max = params->radius;
    if (params->radius_sigma > 0.0) {
        double const s2 = log(1.0 + (params->radius_sigma / params->radius) * (params->radius_sigma / params->radius));
        ln_sigma = sqrt(s2);
        ln_mu = log(params->radius) - 0.5 * s2;
        r_max = exp(ln_mu + 3.0 * ln_sigma);
    }
    for (y = 0; y < height; y++) {
        ALWAN_CORE_T *row = (ALWAN_CORE_T *)((char *)image + y * row_stride);
        for (x = 0; x < width; x++) {
            for (c = 0; c < channels; c++) {
                alwan__grain_rng g;
                size_t k, covered = 0;
                /* The sample stream of this output pixel, its own so the field's cell
                 * streams are never consumed by the sampling. */
                alwan__grain_seed(&g, (uint64_t)params->seed ^ 0x5DEECE66Dull, (long)x, (long)y, (uint64_t)c + 16u);
                for (k = 0; k < params->samples; k++) {
                    double px = (double)x + 0.5, py = (double)y + 0.5;
                    if (params->filter_sigma > 0.0) {
                        px += params->filter_sigma * alwan__grain_normal(&g);
                        py += params->filter_sigma * alwan__grain_normal(&g);
                    }
                    covered += (size_t)ALWAN_CORE_FN(grain_covered)(px, py, src, width, height, channels, c, params, r_max,
                                                                    inv_pi_er2, ln_mu, ln_sigma);
                }
                row[x * (size_t)channels + (size_t)c] = (ALWAN_CORE_T)((double)covered / (double)params->samples);
            }
        }
    }
    if (ctx) ctx->free_fn(src); else ALWAN_FREE(src);
    return ALWAN_OK;
}

/* Selwyn grain on a negative's activations: the many-thin-layers limit of the disc
 * model, Gaussian on density with sigma_D = (rms / 1000) sqrt(D) sqrt(A_48 / pitch^2),
 * from a stream seeded by (seed, pixel, layer). The mean density is kept exactly and
 * a zero stays zero, which is what a spread proportional to sqrt(D) gives. */
alwan_status ALWAN_CORE_FNLIT(alwan_film_grain_density)(ALWAN_CORE_T *density, size_t row_stride, size_t width, size_t height,
                                                        alwan_film_grain_density_params const *params, alwan_ctx *ctx) {
    double const a48 = 3.141592653589793 * 24.0 * 24.0;   /* the 48 um aperture, um^2 */
    double scale;
    size_t x, y;
    int c;
    (void)ctx;
    if (!density || width == 0 || height == 0 || !params) return ALWAN_E_INVALID;
    if (!(params->rms_granularity > 0.0) || !(params->pixel_pitch > 0.0) || params->layer_scale[0] < 0.0 ||
        params->layer_scale[1] < 0.0 || params->layer_scale[2] < 0.0) {
        return ALWAN_E_INVALID;
    }
    if (row_stride < width * 3 * sizeof(ALWAN_CORE_T)) return ALWAN_E_INVALID;
    scale = (params->rms_granularity / 1000.0) * sqrt(a48 / (params->pixel_pitch * params->pixel_pitch));
    for (y = 0; y < height; y++) {
        ALWAN_CORE_T *row = (ALWAN_CORE_T *)((char *)density + y * row_stride);
        for (x = 0; x < width; x++) {
            for (c = 0; c < 3; c++) {
                alwan__grain_rng g;
                double d = (double)row[x * 3 + (size_t)c];
                double sigma;
                if (!(d > 0.0) || params->layer_scale[c] == 0.0) continue;
                alwan__grain_seed(&g, (uint64_t)params->seed, (long)x, (long)y, (uint64_t)c + 32u);
                sigma = scale * params->layer_scale[c] * sqrt(d);
                d += sigma * alwan__grain_normal(&g);
                row[x * 3 + (size_t)c] = (ALWAN_CORE_T)(d > 0.0 ? d : 0.0);
            }
        }
    }
    return ALWAN_OK;
}
