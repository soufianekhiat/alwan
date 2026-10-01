/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Environment-map sampling (parameterized). No include guard: included once
 * per precision pass. The pyramid readers live in alwan_env_sampling_reader.inc.
 *
 * THE MAP. A position (x, y) in [0, 1]^2 of an equirectangular map is the
 * direction at azimuth phi = 2 pi x and polar angle theta = pi y from +Y:
 *     (sin theta cos phi, cos theta, sin theta sin phi)
 * so the map's top row looks up (+Y) and its middle row along the horizon. A
 * map position's density p_xy is a solid-angle density p_xy / (2 pi^2 sin theta).
 */

#define ALWAN_ENV_LOBE_KIND_COSINE 0
#define ALWAN_ENV_LOBE_KIND_PHONG  1
#define ALWAN_ENV_LOBE_KIND_NONE   (-1)

/* The cell of n that holds v, v in cell units: the floor, clamped to [0, n - 1],
 * NaN to 0. Every float that becomes an index goes through here. */
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_env_cell)(ALWAN_CORE_T v, int n) {
    if (!(v > ALWAN_CORE_LITERAL(0.0))) return 0;
    if (v >= (ALWAN_CORE_T)n) return n - 1;
    return (int)v;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_env_direction)(ALWAN_CORE_T x, ALWAN_CORE_T y) {
    ALWAN_CORE_VEC3 d;
    const ALWAN_CORE_T phi = ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI * x;
    const ALWAN_CORE_T theta = ALWAN_CORE_PI * y;
    const ALWAN_CORE_T st = ALWAN_CORE_SIN(theta);
    d.v[0] = st * ALWAN_CORE_COS(phi);
    d.v[1] = ALWAN_CORE_COS(theta);
    d.v[2] = st * ALWAN_CORE_SIN(phi);
    return d;
}

/* The map position of a direction (need not be unit length, must not be 0). */
ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_CORE_FNVLIT(alwan_env_position)(ALWAN_CORE_T dx, ALWAN_CORE_T dy, ALWAN_CORE_T dz) {
    ALWAN_CORE_VEC2 p;
    const ALWAN_CORE_T len = ALWAN_CORE_SQRT(dx * dx + dy * dy + dz * dz);
    ALWAN_CORE_T c = dy / len;
    ALWAN_CORE_T phi = ALWAN_CORE_ATAN2(dz, dx);
    if (c > ALWAN_CORE_LITERAL(1.0)) c = ALWAN_CORE_LITERAL(1.0);
    if (c < ALWAN_CORE_LITERAL(-1.0)) c = ALWAN_CORE_LITERAL(-1.0);
    if (phi < ALWAN_CORE_LITERAL(0.0)) phi = phi + ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI;
    p.v[0] = phi / (ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI);
    p.v[1] = ALWAN_CORE_ACOS(c) / ALWAN_CORE_PI;
    if (p.v[0] >= ALWAN_CORE_LITERAL(1.0)) p.v[0] = ALWAN_CORE_LITERAL(0.0);
    return p;
}

/* An upper bound of the lobe over the map cell [xa, xb] x [ya, yb], positive
 * wherever the lobe is positive somewhere in the cell. The lobe is
 * max(0, axis . w), raised to exponent for PHONG (axis unit length). The cell
 * lies inside the cone around its centre direction of half-angle
 *     a = (theta_b - theta_a) / 2 + max sin theta over the cell x (phi_b - phi_a) / 2
 * (down the meridian, then along the parallel, which is longer than the great
 * circle), so the lobe's largest value is at angle max(0, angle to the axis - a). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_env_lobe_bound)(ALWAN_CORE_T xa, ALWAN_CORE_T xb, ALWAN_CORE_T ya,
                                                                 ALWAN_CORE_T yb, int kind, ALWAN_CORE_T ax,
                                                                 ALWAN_CORE_T ay, ALWAN_CORE_T az, ALWAN_CORE_T e) {
    const ALWAN_CORE_T ta = ALWAN_CORE_PI * ya;
    const ALWAN_CORE_T tb = ALWAN_CORE_PI * yb;
    const ALWAN_CORE_T half_pi = ALWAN_CORE_LITERAL(0.5) * ALWAN_CORE_PI;
    ALWAN_CORE_T smax, a, cd, g, b;
    ALWAN_CORE_VEC3 dc;
    if (kind == ALWAN_ENV_LOBE_KIND_NONE) return ALWAN_CORE_LITERAL(1.0);
    if (ta <= half_pi && tb >= half_pi) {
        smax = ALWAN_CORE_LITERAL(1.0);
    } else {
        const ALWAN_CORE_T sa = ALWAN_CORE_SIN(ta);
        const ALWAN_CORE_T sb = ALWAN_CORE_SIN(tb);
        smax = sa > sb ? sa : sb;
    }
    a = ALWAN_CORE_LITERAL(0.5) * (tb - ta) + smax * ALWAN_CORE_PI * (xb - xa);
    if (a >= ALWAN_CORE_PI) return ALWAN_CORE_LITERAL(1.0);
    dc = ALWAN_CORE_FNVLIT(alwan_env_direction)(ALWAN_CORE_LITERAL(0.5) * (xa + xb), ALWAN_CORE_LITERAL(0.5) * (ya + yb));
    cd = ax * dc.v[0] + ay * dc.v[1] + az * dc.v[2];
    if (cd > ALWAN_CORE_LITERAL(1.0)) cd = ALWAN_CORE_LITERAL(1.0);
    if (cd < ALWAN_CORE_LITERAL(-1.0)) cd = ALWAN_CORE_LITERAL(-1.0);
    g = ALWAN_CORE_ACOS(cd) - a;
    if (g <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(1.0);
    if (g >= half_pi) return ALWAN_CORE_LITERAL(0.0);
    b = ALWAN_CORE_COS(g);
    if (kind == ALWAN_ENV_LOBE_KIND_PHONG && e != ALWAN_CORE_LITERAL(1.0)) b = ALWAN_CORE_POW(b, e);
    return b;
}

/* What a cell weighs in the sampler: the lobe's mean over the cell, by solid
 * angle, from a 3 x 3 midpoint rule in (phi, theta), but never less than 1/64
 * of the bound above. The mean shapes where samples go (Clarberg's hierarchy
 * weighs each cell by light x lobe over it); the floor keeps every cell the
 * lobe reaches positive, which is what keeps estimates unbiased, since the
 * mean's few points can miss a lobe's sliver of the cell. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_env_lobe_weight)(ALWAN_CORE_T xa, ALWAN_CORE_T xb, ALWAN_CORE_T ya,
                                                                  ALWAN_CORE_T yb, int kind, ALWAN_CORE_T ax,
                                                                  ALWAN_CORE_T ay, ALWAN_CORE_T az, ALWAN_CORE_T e) {
    ALWAN_CORE_T bound, num = ALWAN_CORE_LITERAL(0.0), den = ALWAN_CORE_LITERAL(0.0), mean, floor_w;
    int a, b;
    if (kind == ALWAN_ENV_LOBE_KIND_NONE) return ALWAN_CORE_LITERAL(1.0);
    bound = ALWAN_CORE_FNVLIT(alwan_env_lobe_bound)(xa, xb, ya, yb, kind, ax, ay, az, e);
    if (!(bound > ALWAN_CORE_LITERAL(0.0))) return ALWAN_CORE_LITERAL(0.0);
    for (b = 0; b < 3; b++) {
        const ALWAN_CORE_T fy = (ALWAN_CORE_T)(2 * b + 1) / ALWAN_CORE_LITERAL(6.0);
        const ALWAN_CORE_T theta = ALWAN_CORE_PI * (ya + fy * (yb - ya));
        const ALWAN_CORE_T st = ALWAN_CORE_SIN(theta);
        const ALWAN_CORE_T ct = ALWAN_CORE_COS(theta);
        for (a = 0; a < 3; a++) {
            const ALWAN_CORE_T fx = (ALWAN_CORE_T)(2 * a + 1) / ALWAN_CORE_LITERAL(6.0);
            const ALWAN_CORE_T phi = ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI * (xa + fx * (xb - xa));
            ALWAN_CORE_T c = ax * st * ALWAN_CORE_COS(phi) + ay * ct + az * st * ALWAN_CORE_SIN(phi);
            if (!(c > ALWAN_CORE_LITERAL(0.0))) c = ALWAN_CORE_LITERAL(0.0);
            else if (kind == ALWAN_ENV_LOBE_KIND_PHONG && e != ALWAN_CORE_LITERAL(1.0)) c = ALWAN_CORE_POW(c, e);
            num = num + c * st;
            den = den + st;
        }
    }
    mean = den > ALWAN_CORE_LITERAL(0.0) ? num / den : ALWAN_CORE_LITERAL(0.0);
    floor_w = bound * ALWAN_CORE_LITERAL(0.015625);
    return mean > floor_w ? mean : floor_w;
}

/* The lobe itself at a direction (unit axis): max(0, axis . w)^e. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_env_lobe_value)(int kind, ALWAN_CORE_T ax, ALWAN_CORE_T ay, ALWAN_CORE_T az,
                                                                 ALWAN_CORE_T e, ALWAN_CORE_T wx, ALWAN_CORE_T wy,
                                                                 ALWAN_CORE_T wz) {
    ALWAN_CORE_T c;
    if (kind == ALWAN_ENV_LOBE_KIND_NONE) return ALWAN_CORE_LITERAL(1.0);
    c = ax * wx + ay * wy + az * wz;
    if (!(c > ALWAN_CORE_LITERAL(0.0))) return ALWAN_CORE_LITERAL(0.0);
    if (kind == ALWAN_ENV_LOBE_KIND_PHONG && e != ALWAN_CORE_LITERAL(1.0)) c = ALWAN_CORE_POW(c, e);
    return c;
}

/* ----------------------------------------------------------------
 * THE PYRAMID. The leaf level is the map padded with zero weights to 2^la x
 * 2^lb cells; level l halves each side still longer than 1 cell, up to level
 * max(la, lb), one cell. Levels are stored leaf first, row by row. Cell (i, j)
 * of level l holds the sum of the leaves under it.
 * ---------------------------------------------------------------- */
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_envp_dim)(int lg, int l) { return l >= lg ? 1 : (1 << (lg - l)); }
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_envp_span)(int lg, int l) { return l >= lg ? (1 << lg) : (1 << l); }
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_envp_level_off)(int la, int lb, int l) {
    int off = 0, k;
    for (k = 0; k < l; k++) off += ALWAN_CORE_FNVLIT(alwan_envp_dim)(la, k) * ALWAN_CORE_FNVLIT(alwan_envp_dim)(lb, k);
    return off;
}

#if ALWAN_HAS_POINTERS
#define ALWAN_ENVP_READER_POINTER 1
#include "alwan_env_sampling_reader.inc"
#endif
