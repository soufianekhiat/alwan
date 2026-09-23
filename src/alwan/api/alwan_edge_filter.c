/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_edge_filter_{T}: the edge-aware smoothing filters behind one entry point. Each
 * method's worker lives with its own references in api/alwan_guided_filter.c,
 * alwan_bilateral.c, alwan_domain_transform.c, alwan_fast_global_smoother.c and
 * alwan_l0_smooth.c; this file
 * resolves the defaults of alwan_edge_filter_params_{T} and routes.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>

typedef struct {
    size_t radius, iterations;
    double eps, sigma_color, sigma_space, lambda, attenuation, kappa;
    int start_from_source;
} alwan_ef_resolved;

static double alwan_ef_or(double v, double def) {
    return v == 0.0 ? def : v;
}

static alwan_status alwan_ef_route(void *out, size_t out_rs, void const *src, size_t src_rs, size_t sch,
                                   void const *guide, size_t guide_rs, size_t gch, size_t w, size_t h,
                                   alwan_edge_filter_method method, alwan_ef_resolved const *p, int is_f32) {
    if (!guide) {
        guide = src;
        guide_rs = src_rs;
        gch = sch;
    }
    switch (method) {
    case ALWAN_EDGE_FILTER_GUIDED:
        return alwan__gf_run(out, out_rs, src, src_rs, sch, guide, guide_rs, gch, w, h,
                             p->radius == 0 ? 4 : p->radius, alwan_ef_or(p->eps, 0.01), is_f32);
    case ALWAN_EDGE_FILTER_JOINT_BILATERAL:
    case ALWAN_EDGE_FILTER_ROLLING_GUIDANCE: {
        double const ss = alwan_ef_or(p->sigma_space, 3.0), sc = alwan_ef_or(p->sigma_color, 0.1);
        size_t const r = p->radius != 0 ? p->radius : (size_t)floor(1.5 * ss + 0.5);
        if (method == ALWAN_EDGE_FILTER_JOINT_BILATERAL) {
            return alwan__bf_run(out, out_rs, src, src_rs, sch, guide, guide_rs, gch, w, h, r, sc, ss, is_f32);
        }
        return alwan__rgf_run(out, out_rs, src, src_rs, sch, w, h, r, sc, ss, p->iterations == 0 ? 4 : p->iterations,
                              p->start_from_source ? 0 : 1, is_f32);
    }
    case ALWAN_EDGE_FILTER_DOMAIN_TRANSFORM_NC:
    case ALWAN_EDGE_FILTER_DOMAIN_TRANSFORM_RF:
        return alwan__dt_run(out, out_rs, src, src_rs, sch, guide, guide_rs, gch, w, h, alwan_ef_or(p->sigma_space, 60.0),
                             alwan_ef_or(p->sigma_color, 0.4), method == ALWAN_EDGE_FILTER_DOMAIN_TRANSFORM_NC ? 0 : 2,
                             p->iterations == 0 ? 3 : p->iterations, is_f32);
    case ALWAN_EDGE_FILTER_FAST_GLOBAL_SMOOTHER:
        return alwan__fgs_run(out, out_rs, src, src_rs, sch, guide, guide_rs, gch, w, h, alwan_ef_or(p->lambda, 900.0),
                              alwan_ef_or(p->sigma_color, 0.03), alwan_ef_or(p->attenuation, 0.25),
                              p->iterations == 0 ? 3 : p->iterations, is_f32);
    case ALWAN_EDGE_FILTER_L0_SMOOTH: /* the guide is not used */
        return alwan__l0_run(out, out_rs, src, src_rs, sch, w, h, alwan_ef_or(p->lambda, 0.02), alwan_ef_or(p->kappa, 2.0),
                             is_f32);
    default:
        return ALWAN_E_INVALID;
    }
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_edge_filter_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride,
                                   size_t src_channels, alwan_f64 const *guide, size_t guide_row_stride,
                                   size_t guide_channels, size_t width, size_t height, alwan_edge_filter_method method,
                                   alwan_edge_filter_params_f64 const *params) {
    alwan_ef_resolved r = { 0 };
    if (params) {
        r.radius = params->radius;
        r.iterations = params->iterations;
        r.eps = (double)params->eps;
        r.sigma_color = (double)params->sigma_color;
        r.sigma_space = (double)params->sigma_space;
        r.lambda = (double)params->lambda;
        r.attenuation = (double)params->lambda_attenuation;
        r.start_from_source = params->start_from_source;
        r.kappa = (double)params->kappa;
    }
    return alwan_ef_route(out, out_row_stride, src, src_row_stride, src_channels, guide, guide_row_stride,
                          guide_channels, width, height, method, &r, 0);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_edge_filter_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride,
                                   size_t src_channels, alwan_f32 const *guide, size_t guide_row_stride,
                                   size_t guide_channels, size_t width, size_t height, alwan_edge_filter_method method,
                                   alwan_edge_filter_params_f32 const *params) {
    alwan_ef_resolved r = { 0 };
    if (params) {
        r.radius = params->radius;
        r.iterations = params->iterations;
        r.eps = (double)params->eps;
        r.sigma_color = (double)params->sigma_color;
        r.sigma_space = (double)params->sigma_space;
        r.lambda = (double)params->lambda;
        r.attenuation = (double)params->lambda_attenuation;
        r.start_from_source = params->start_from_source;
        r.kappa = (double)params->kappa;
    }
    return alwan_ef_route(out, out_row_stride, src, src_row_stride, src_channels, guide, guide_row_stride,
                          guide_channels, width, height, method, &r, 1);
}
#endif
