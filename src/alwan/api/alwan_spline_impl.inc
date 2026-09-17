/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Cubic spline interpolation, templated per precision.
 *
 * Included once per precision from alwan_spline.c between
 * alwan_api_{f32,f64}_setup.h and alwan_api_teardown.h.
 *
 * The spline is C2 and global: every node slope depends on every sample, so the
 * slopes come from one tridiagonal solve before any output point is evaluated.
 * That solve needs O(count_in) scratch, which is why this lives in its own unit
 * rather than beside alwan_interpolate, whose methods each read a fixed stencil
 * and allocate nothing.
 *
 * The system is the one scipy.interpolate.CubicSpline builds, solved for the
 * first derivative s[i] at each node:
 *
 *   interior    dx[i] s[i-1] + 2 (dx[i-1] + dx[i]) s[i] + dx[i-1] s[i+1]
 *                 = 3 (dx[i] m[i-1] + dx[i-1] m[i])
 *
 * with m the interval slopes, and two end rows chosen by the boundary condition.
 * Both end rows touch only two unknowns, so the whole system stays tridiagonal
 * and is solved by the Thomas algorithm without pivoting. The not-a-knot end rows
 * are not diagonally dominant; checked against scipy's pivoted banded solver at
 * spacing ratios up to 1e12, with pivots down to 3e-13, the unpivoted result stays
 * within 2.2e-15 relative, because a small pivot here always comes with the small
 * interval that produced it.
 *
 * Each interval is then the cubic Hermite polynomial on its two samples and two
 * slopes, the same basis ALWAN_INTERP_PCHIP evaluates.
 */

/* Index k with x[k] <= v < x[k + 1], for x[0] < v < x[n - 1]. Binary search. */
static size_t ALWAN_CORE_FN(spline_interval)(ALWAN_CORE_T const *x, size_t n, ALWAN_CORE_T v) {
    size_t lo = 0, hi = n - 1;
    while (hi - lo > 1) {
        size_t const mid = lo + (hi - lo) / 2;
        if (x[mid] <= v) lo = mid; else hi = mid;
    }
    return lo;
}

/* Row i of the slope system: sub, diag, super and right-hand side. */
static void ALWAN_CORE_FN(spline_row)(ALWAN_CORE_T const *x, ALWAN_CORE_T const *y, size_t n, size_t i,
                                      alwan_spline_boundary boundary,
                                      ALWAN_CORE_T *sub, ALWAN_CORE_T *diag, ALWAN_CORE_T *sup,
                                      ALWAN_CORE_T *rhs) {
    ALWAN_CORE_T const two = ALWAN_CORE_LITERAL(2.0), three = ALWAN_CORE_LITERAL(3.0);
    *sub = *sup = ALWAN_CORE_LITERAL(0.0);

    if (i > 0 && i < n - 1) {
        ALWAN_CORE_T const h0 = x[i] - x[i - 1], h1 = x[i + 1] - x[i];
        ALWAN_CORE_T const m0 = (y[i] - y[i - 1]) / h0, m1 = (y[i + 1] - y[i]) / h1;
        *sub = h1;
        *diag = two * (h0 + h1);
        *sup = h0;
        *rhs = three * (h1 * m0 + h0 * m1);
        return;
    }

    if (i == 0) {
        ALWAN_CORE_T const h0 = x[1] - x[0], m0 = (y[1] - y[0]) / h0;
        if (boundary == ALWAN_SPLINE_NATURAL) {
            *diag = two; *sup = ALWAN_CORE_LITERAL(1.0); *rhs = three * m0;
        } else if (n == 3) {
            /* Not-a-knot on three samples states one condition twice, so scipy fits
             * the parabola through them instead: s0 + s1 = 2 m0. */
            *diag = ALWAN_CORE_LITERAL(1.0); *sup = ALWAN_CORE_LITERAL(1.0); *rhs = two * m0;
        } else {
            ALWAN_CORE_T const h1 = x[2] - x[1], m1 = (y[2] - y[1]) / h1, d = x[2] - x[0];
            *diag = h1;
            *sup = d;
            *rhs = ((h0 + two * d) * h1 * m0 + h0 * h0 * m1) / d;
        }
        return;
    }

    /* i == n - 1 */
    {
        ALWAN_CORE_T const hl = x[n - 1] - x[n - 2], ml = (y[n - 1] - y[n - 2]) / hl;
        if (boundary == ALWAN_SPLINE_NATURAL) {
            *sub = ALWAN_CORE_LITERAL(1.0); *diag = two; *rhs = three * ml;
        } else if (n == 3) {
            *sub = ALWAN_CORE_LITERAL(1.0); *diag = ALWAN_CORE_LITERAL(1.0); *rhs = two * ml;
        } else {
            ALWAN_CORE_T const hp = x[n - 2] - x[n - 3], mp = (y[n - 2] - y[n - 3]) / hp;
            ALWAN_CORE_T const d = x[n - 1] - x[n - 3];
            *sub = d;
            *diag = hp;
            *rhs = (hl * hl * mp + (two * d + hl) * hp * ml) / d;
        }
    }
}

alwan_status ALWAN_CORE_FNLIT(alwan_interpolate_cubic_spline)(ALWAN_CORE_T const *x_in, ALWAN_CORE_T const *y_in,
                                                              size_t count_in,
                                                              ALWAN_CORE_T const *x_out, ALWAN_CORE_T *y_out,
                                                              size_t count_out,
                                                              alwan_spline_boundary boundary, alwan_ctx *ctx) {
    size_t i, bytes;
    ALWAN_CORE_T *scratch, *cp, *s;

    if (!x_in || !y_in || !x_out || !y_out || count_in < 2 || count_out == 0) return ALWAN_E_INVALID;
    if (boundary != ALWAN_SPLINE_NOT_A_KNOT && boundary != ALWAN_SPLINE_NATURAL) return ALWAN_E_INVALID;
    /* Strictly increasing. Written as !(>) so a NaN abscissa is refused too, where a
     * repeated one would otherwise divide by a zero interval. */
    for (i = 1; i < count_in; i++) {
        if (!(x_in[i] > x_in[i - 1])) return ALWAN_E_INVALID;
    }

    bytes = alwan_safe_array_size(count_in, 2 * sizeof(ALWAN_CORE_T));
    if (bytes == 0) return ALWAN_E_NOMEM;
    scratch = (ALWAN_CORE_T *)(ctx ? ctx->alloc_fn(bytes, sizeof(ALWAN_CORE_T))
                                   : ALWAN_ALLOC(bytes, sizeof(ALWAN_CORE_T)));
    if (!scratch) return ALWAN_E_NOMEM;
    cp = scratch;              /* normalised super-diagonal */
    s = scratch + count_in;    /* normalised right-hand side, then the slopes */

    if (count_in == 2) {
        /* One interval: both boundary conditions reduce to the straight line. */
        ALWAN_CORE_T const m = (y_in[1] - y_in[0]) / (x_in[1] - x_in[0]);
        s[0] = s[1] = m;
    } else {
        ALWAN_CORE_T sub, diag, sup, rhs;
        ALWAN_CORE_FN(spline_row)(x_in, y_in, count_in, 0, boundary, &sub, &diag, &sup, &rhs);
        cp[0] = sup / diag;
        s[0] = rhs / diag;
        for (i = 1; i < count_in; i++) {
            ALWAN_CORE_T denom;
            ALWAN_CORE_FN(spline_row)(x_in, y_in, count_in, i, boundary, &sub, &diag, &sup, &rhs);
            denom = diag - sub * cp[i - 1];
            cp[i] = sup / denom;
            s[i] = (rhs - sub * s[i - 1]) / denom;
        }
        for (i = count_in - 1; i-- > 0;) {
            s[i] -= cp[i] * s[i + 1];
        }
    }

    for (i = 0; i < count_out; i++) {
        ALWAN_CORE_T const v = x_out[i];
        size_t k;
        ALWAN_CORE_T h, t, t2, t3;
        /* Held flat outside the samples, as alwan_interpolate does. */
        if (!(v > x_in[0])) { y_out[i] = y_in[0]; continue; }
        if (v >= x_in[count_in - 1]) { y_out[i] = y_in[count_in - 1]; continue; }
        k = ALWAN_CORE_FN(spline_interval)(x_in, count_in, v);
        h = x_in[k + 1] - x_in[k];
        t = (v - x_in[k]) / h;
        t2 = t * t;
        t3 = t2 * t;
        y_out[i] = (ALWAN_CORE_LITERAL(2.0) * t3 - ALWAN_CORE_LITERAL(3.0) * t2 + ALWAN_CORE_LITERAL(1.0)) * y_in[k]
                 + (t3 - ALWAN_CORE_LITERAL(2.0) * t2 + t) * h * s[k]
                 + (ALWAN_CORE_LITERAL(3.0) * t2 - ALWAN_CORE_LITERAL(2.0) * t3) * y_in[k + 1]
                 + (t3 - t2) * h * s[k + 1];
    }

    if (ctx) ctx->free_fn(scratch); else ALWAN_FREE(scratch);
    return ALWAN_OK;
}
