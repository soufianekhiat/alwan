/* No include guard -- included twice (f32 and f64 passes) */

void ALWAN_CORE_FNLIT(alwan_mat3_identity)(ALWAN_CORE_MAT3X3 *out) {
    *out = ALWAN_CORE_FNVLIT(alwan_mat3_identity)();
}

void ALWAN_CORE_FNLIT(alwan_mat3_mul)(ALWAN_CORE_MAT3X3 *out, ALWAN_CORE_MAT3X3 const *a, ALWAN_CORE_MAT3X3 const *b) {
    *out = ALWAN_CORE_FNVLIT(alwan_mat3_mul)(*a, *b);
}

void ALWAN_CORE_FNLIT(alwan_mat3_mulv)(ALWAN_CORE_VEC3 *out, ALWAN_CORE_MAT3X3 const *m, ALWAN_CORE_VEC3 const *v) {
    *out = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(*m, *v);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_mat3_det)(ALWAN_CORE_MAT3X3 const *m) {
    return ALWAN_CORE_FNVLIT(alwan_mat3_det)(*m);
}

alwan_status ALWAN_CORE_FNLIT(alwan_mat3_inv)(ALWAN_CORE_MAT3X3 *out, ALWAN_CORE_MAT3X3 const *m) {
    ALWAN_CORE_T det = ALWAN_CORE_FNVLIT(alwan_mat3_det)(*m);
    if (ALWAN_CORE_ABS(det) < ALWAN_CORE_EPSILON) {
        return ALWAN_E_RANGE;
    }
    *out = ALWAN_CORE_FNVLIT(alwan_mat3_inv)(*m);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Find interval (binary search for interpolation)
 * ---------------------------------------------------------------- */
static size_t ALWAN_CORE_FNLIT(alwan_math_find_interval)(ALWAN_CORE_T const *x_in, size_t count, ALWAN_CORE_T x) {
    if (x <= x_in[0]) return 0;
    if (x >= x_in[count - 1]) return count - 2;

    size_t left = 0, right = count - 1;
    while (right - left > 1) {
        size_t mid = (left + right) / 2;
        if (x_in[mid] <= x) {
            left = mid;
        } else {
            right = mid;
        }
    }
    return left;
}

/* ----------------------------------------------------------------
 * PCHIP, Fritsch and Carlson's monotone cubic
 *
 * The node derivatives are a local rule, which is the whole reason this method fits
 * here: evaluating the interval [k, k+1] needs only nodes k-1 to k+2, the same stencil
 * Catmull-Rom already reads, so no pass over the data and no scratch. scipy builds every
 * derivative up front, but the formula it uses is local and the two agree: checked
 * against PchipInterpolator over uniform and non-uniform grids, worst 8.9e-16, the
 * endpoints included.
 * ---------------------------------------------------------------- */

static ALWAN_CORE_T ALWAN_CORE_FN(pchip_sign)(ALWAN_CORE_T v) {
    if (v > ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(1.0);
    if (v < ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(-1.0);
    return ALWAN_CORE_LITERAL(0.0);
}

/* The one-sided three-point rule scipy uses at the two ends, clipped so the edge cannot
 * overshoot the slope beside it or turn the other way from it. */
static ALWAN_CORE_T ALWAN_CORE_FN(pchip_edge)(ALWAN_CORE_T h0, ALWAN_CORE_T h1,
                                              ALWAN_CORE_T d0, ALWAN_CORE_T d1) {
    ALWAN_CORE_T const v = ((ALWAN_CORE_LITERAL(2.0) * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
    if (ALWAN_CORE_FN(pchip_sign)(v) != ALWAN_CORE_FN(pchip_sign)(d0)) {
        return ALWAN_CORE_LITERAL(0.0);
    }
    if (ALWAN_CORE_FN(pchip_sign)(d0) != ALWAN_CORE_FN(pchip_sign)(d1)
        && ALWAN_ABS(v) > ALWAN_CORE_LITERAL(3.0) * ALWAN_ABS(d0)) {
        return ALWAN_CORE_LITERAL(3.0) * d0;
    }
    return v;
}

/* The derivative at node k, from the local stencil alone. Zero wherever the data turns,
 * which is what keeps the interpolant monotone on monotone data. */
static ALWAN_CORE_T ALWAN_CORE_FN(pchip_deriv)(ALWAN_CORE_T const *x, ALWAN_CORE_T const *y,
                                               size_t n, size_t k) {
    if (n < 3) return (y[1] - y[0]) / (x[1] - x[0]);
    if (k == 0) {
        ALWAN_CORE_T const h0 = x[1] - x[0], h1 = x[2] - x[1];
        return ALWAN_CORE_FN(pchip_edge)(h0, h1, (y[1] - y[0]) / h0, (y[2] - y[1]) / h1);
    }
    if (k == n - 1) {
        ALWAN_CORE_T const h0 = x[n - 1] - x[n - 2], h1 = x[n - 2] - x[n - 3];
        return ALWAN_CORE_FN(pchip_edge)(h0, h1, (y[n - 1] - y[n - 2]) / h0,
                                         (y[n - 2] - y[n - 3]) / h1);
    }
    {
        ALWAN_CORE_T const hm = x[k] - x[k - 1], hk = x[k + 1] - x[k];
        ALWAN_CORE_T const dm = (y[k] - y[k - 1]) / hm, dk = (y[k + 1] - y[k]) / hk;
        if (dm * dk <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);
        {
            ALWAN_CORE_T const w1 = ALWAN_CORE_LITERAL(2.0) * hk + hm;
            ALWAN_CORE_T const w2 = hk + ALWAN_CORE_LITERAL(2.0) * hm;
            return (w1 + w2) / (w1 / dm + w2 / dk);
        }
    }
}

/* ----------------------------------------------------------------
 * Advanced Interpolation Methods
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * Sprague (1880), as colour-science's SpragueInterpolator
 *
 * The data are padded with two points beyond each end, extrapolated by Sprague's own
 * coefficients over the six nearest samples (divided by 209), and every interval is the
 * quintic through the six samples around it. The padding sits one smallest-interval step
 * apart, as colour's interval(x)[0] takes it. Until 2026-09-25 alwan evaluated a Taylor
 * form in finite differences instead, 0.24 from colour on a smooth spectrum.
 * ---------------------------------------------------------------- */

static ALWAN_CORE_T ALWAN_CORE_FN(sprague_padded)(ALWAN_CORE_T const *y, size_t n, long k) {
    static int const c[4][6] = { { 884, -1960, 3033, -2648, 1080, -180 },
                                 { 508, -540, 488, -367, 144, -24 },
                                 { -24, 144, -367, 488, -540, 508 },
                                 { -180, 1080, -2648, 3033, -1960, 884 } };
    int row, j;
    ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T const *src;
    if (k >= 0 && k < (long)n) return y[k];
    if (k == -2) row = 0;
    else if (k == -1) row = 1;
    else if (k == (long)n) row = 2;
    else row = 3;
    src = row < 2 ? y : y + (n - 6);
    for (j = 0; j < 6; j++) sum += (ALWAN_CORE_T)c[row][j] * src[j];
    return sum / ALWAN_CORE_LITERAL(209.0);
}

static ALWAN_CORE_T ALWAN_CORE_FN(sprague_eval)(ALWAN_CORE_T const *x, ALWAN_CORE_T const *y, size_t n,
                                                size_t idx, ALWAN_CORE_T xv) {
    static int const w[5][6] = { { 2, -16, 0, 16, -2, 0 },
                                 { -1, 16, -30, 16, -1, 0 },
                                 { -9, 39, -70, 66, -33, 7 },
                                 { 13, -64, 126, -124, 61, -12 },
                                 { -5, 25, -50, 50, -25, 5 } };
    ALWAN_CORE_T r[6], a[5], X, Xk, out;
    int k, j;
    for (j = 0; j < 6; j++) r[j] = ALWAN_CORE_FN(sprague_padded)(y, n, (long)idx - 2 + j);
    X = (xv - x[idx]) / (x[idx + 1] - x[idx]);
    for (k = 0; k < 5; k++) {
        ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
        for (j = 0; j < 6; j++) sum += (ALWAN_CORE_T)w[k][j] * r[j];
        a[k] = sum / ALWAN_CORE_LITERAL(24.0);
    }
    out = ALWAN_CORE_LITERAL(0.0);
    Xk = X;
    for (k = 0; k < 5; k++) {
        out += a[k] * Xk;
        Xk *= X;
    }
    return r[2] + out;
}

/* ----------------------------------------------------------------
 * Akima (1970), as scipy's Akima1DInterpolator (method "akima")
 *
 * Secant slopes extended by two linear extrapolations at each end; the slope at a node
 * is Akima's weighted mean of the secants beside it, or their plain mean where the
 * weights vanish, the vanishing judged against the largest weight over the whole data
 * (scipy's f12 > 1e-9 max(f12)). Evaluated as scipy's PPoly evaluates a Hermite cubic.
 * Until 2026-09-25 alwan used one slope for both ends of an interval, 0.17 from scipy.
 * ---------------------------------------------------------------- */

/* scipy's m[j], j = 0 .. n + 2; m[2 .. n] are the n - 1 secants. */
static ALWAN_CORE_T ALWAN_CORE_FN(akima_m)(ALWAN_CORE_T const *x, ALWAN_CORE_T const *y, size_t n, long j) {
    long const last = (long)n;   /* m[n] is the last secant */
    if (j >= 2 && j <= last) return (y[j - 1] - y[j - 2]) / (x[j - 1] - x[j - 2]);
    if (j == 1) return ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_FN(akima_m)(x, y, n, 2) - ALWAN_CORE_FN(akima_m)(x, y, n, 3);
    if (j == 0) return ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_FN(akima_m)(x, y, n, 1) - ALWAN_CORE_FN(akima_m)(x, y, n, 2);
    if (j == last + 1) return ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_FN(akima_m)(x, y, n, last) - ALWAN_CORE_FN(akima_m)(x, y, n, last - 1);
    return ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_FN(akima_m)(x, y, n, last + 1) - ALWAN_CORE_FN(akima_m)(x, y, n, last);
}

/* makima adds |m1 + m0| / 2 to each weight, as scipy's method="makima" does:
 * f1 = dm[2:] + 0.5 * pm[2:], with dm and pm the absolute differences and sums. */
static ALWAN_CORE_T ALWAN_CORE_FN(akima_f12)(ALWAN_CORE_T const *x, ALWAN_CORE_T const *y, size_t n, size_t i,
                                             int makima, ALWAN_CORE_T *f1, ALWAN_CORE_T *f2) {
    long const k = (long)i;
    ALWAN_CORE_T const m0 = ALWAN_CORE_FN(akima_m)(x, y, n, k), m1 = ALWAN_CORE_FN(akima_m)(x, y, n, k + 1);
    ALWAN_CORE_T const m2 = ALWAN_CORE_FN(akima_m)(x, y, n, k + 2), m3 = ALWAN_CORE_FN(akima_m)(x, y, n, k + 3);
    *f1 = ALWAN_CORE_ABS(m3 - m2);
    *f2 = ALWAN_CORE_ABS(m1 - m0);
    if (makima) {
        *f1 = *f1 + ALWAN_CORE_LITERAL(0.5) * ALWAN_CORE_ABS(m3 + m2);
        *f2 = *f2 + ALWAN_CORE_LITERAL(0.5) * ALWAN_CORE_ABS(m1 + m0);
    }
    return *f1 + *f2;
}

static ALWAN_CORE_T ALWAN_CORE_FN(akima_t)(ALWAN_CORE_T const *x, ALWAN_CORE_T const *y, size_t n, size_t i,
                                           int makima, ALWAN_CORE_T f12_max) {
    long const k = (long)i;
    ALWAN_CORE_T f1, f2;
    ALWAN_CORE_T const f12 = ALWAN_CORE_FN(akima_f12)(x, y, n, i, makima, &f1, &f2);
    if (f12 > ALWAN_CORE_LITERAL(1e-9) * f12_max) {
        return (f1 * ALWAN_CORE_FN(akima_m)(x, y, n, k + 1) + f2 * ALWAN_CORE_FN(akima_m)(x, y, n, k + 2)) / f12;
    }
    return ALWAN_CORE_LITERAL(0.5) * (ALWAN_CORE_FN(akima_m)(x, y, n, k + 3) + ALWAN_CORE_FN(akima_m)(x, y, n, k));
}

alwan_status ALWAN_CORE_FNLIT(alwan_interpolate)(ALWAN_CORE_T const *x_in, ALWAN_CORE_T const *y_in, size_t count_in,
                       ALWAN_CORE_T const *x_out, ALWAN_CORE_T *y_out, size_t count_out,
                       alwan_interp_method method) {
    if (!x_in || !y_in || !x_out || !y_out || count_in < 2 || count_out == 0) {
        return ALWAN_E_RANGE;
    }

    /* The two global splines solve for every node slope at once; they live in
     * alwan_interpolate_cubic_spline, which holds the ends flat outside the samples as
     * this function does. */
    if (method == ALWAN_INTERP_NATURAL_SPLINE || method == ALWAN_INTERP_CLAMPED_SPLINE) {
        alwan_status const st = ALWAN_CORE_FNLIT(alwan_interpolate_cubic_spline)(
            x_in, y_in, count_in, x_out, y_out, count_out,
            method == ALWAN_INTERP_NATURAL_SPLINE ? ALWAN_SPLINE_NATURAL : ALWAN_SPLINE_CLAMPED, NULL);
        return st == ALWAN_E_INVALID ? ALWAN_E_RANGE : st;
    }

    int const makima = method == ALWAN_INTERP_MAKIMA;
    ALWAN_CORE_T akima_f12_max = ALWAN_CORE_LITERAL(0.0);
    if ((method == ALWAN_INTERP_AKIMA || makima) && count_in >= 3) {
        /* scipy judges a vanishing Akima weight against the largest one in the data */
        size_t k;
        ALWAN_CORE_T f1, f2;
        akima_f12_max = ALWAN_CORE_FN(akima_f12)(x_in, y_in, count_in, 0, makima, &f1, &f2);
        for (k = 1; k < count_in; k++) {
            ALWAN_CORE_T const f = ALWAN_CORE_FN(akima_f12)(x_in, y_in, count_in, k, makima, &f1, &f2);
            if (f > akima_f12_max) akima_f12_max = f;
        }
    }

    for (size_t i = 0; i < count_out; i++) {
        ALWAN_CORE_T x = x_out[i];

        /* Handle boundary cases */
        if (x <= x_in[0]) {
            y_out[i] = y_in[0];
            continue;
        }
        if (x >= x_in[count_in - 1]) {
            y_out[i] = y_in[count_in - 1];
            continue;
        }

        size_t idx = ALWAN_CORE_FNLIT(alwan_math_find_interval)(x_in, count_in, x);
        ALWAN_CORE_T x0 = x_in[idx];
        ALWAN_CORE_T x1 = x_in[idx + 1];
        ALWAN_CORE_T t = (x - x0) / (x1 - x0);

        switch (method) {
            case ALWAN_INTERP_LINEAR: {
                y_out[i] = y_in[idx] * (ALWAN_CORE_LITERAL(1.0) - t) + y_in[idx + 1] * t;
                break;
            }

            case ALWAN_INTERP_CUBIC: {
                ALWAN_CORE_T y0 = (idx > 0) ? y_in[idx - 1] : y_in[idx];
                ALWAN_CORE_T y1 = y_in[idx];
                ALWAN_CORE_T y2 = y_in[idx + 1];
                ALWAN_CORE_T y3 = (idx + 2 < count_in) ? y_in[idx + 2] : y_in[idx + 1];

                y_out[i] = ALWAN_CORE_FNVLIT(alwan_catmull_rom)(y0, y1, y2, y3, t);
                break;
            }

            case ALWAN_INTERP_PCHIP: {
                ALWAN_CORE_T const h = x1 - x0;
                ALWAN_CORE_T const d0 = ALWAN_CORE_FN(pchip_deriv)(x_in, y_in, count_in, idx);
                ALWAN_CORE_T const d1 = ALWAN_CORE_FN(pchip_deriv)(x_in, y_in, count_in, idx + 1);
                ALWAN_CORE_T const t2 = t * t, t3 = t2 * t;
                ALWAN_CORE_T const b00 = ALWAN_CORE_LITERAL(2.0) * t3 - ALWAN_CORE_LITERAL(3.0) * t2
                                       + ALWAN_CORE_LITERAL(1.0);
                ALWAN_CORE_T const b10 = t3 - ALWAN_CORE_LITERAL(2.0) * t2 + t;
                ALWAN_CORE_T const b01 = -ALWAN_CORE_LITERAL(2.0) * t3 + ALWAN_CORE_LITERAL(3.0) * t2;
                ALWAN_CORE_T const b11 = t3 - t2;

                y_out[i] = b00 * y_in[idx] + b10 * h * d0 + b01 * y_in[idx + 1] + b11 * h * d1;
                break;
            }

            case ALWAN_INTERP_LANCZOS: {
                int const a = 3;
                ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
                ALWAN_CORE_T weight_sum = ALWAN_CORE_LITERAL(0.0);

                for (int j = -a + 1; j <= a; j++) {
                    int k = (int)idx + j;
                    if (k < 0 || k >= (int)count_in) continue;

                    ALWAN_CORE_T dx = x - x_in[k];
                    ALWAN_CORE_T normalized_dx = dx / (x1 - x0);
                    ALWAN_CORE_T weight = ALWAN_CORE_FNVLIT(alwan_lanczos_kernel)(normalized_dx, (ALWAN_CORE_T)a);

                    sum += y_in[k] * weight;
                    weight_sum += weight;
                }

                y_out[i] = (weight_sum > ALWAN_CORE_LITERAL(0.0)) ? (sum / weight_sum) : y_in[idx];
                break;
            }

            case ALWAN_INTERP_SPRAGUE: {
                if (count_in < 6) {
                    /* Sprague needs six samples; colour refuses fewer, alwan falls back to
                     * Catmull-Rom as it always has. */
                    ALWAN_CORE_T y0 = (idx > 0) ? y_in[idx - 1] : y_in[idx];
                    ALWAN_CORE_T y1 = y_in[idx];
                    ALWAN_CORE_T y2 = y_in[idx + 1];
                    ALWAN_CORE_T y3 = (idx + 2 < count_in) ? y_in[idx + 2] : y_in[idx + 1];

                    y_out[i] = ALWAN_CORE_FNVLIT(alwan_catmull_rom)(y0, y1, y2, y3, t);
                } else {
                    y_out[i] = ALWAN_CORE_FN(sprague_eval)(x_in, y_in, count_in, idx, x);
                }
                break;
            }

            case ALWAN_INTERP_LAGRANGE: {
                ALWAN_CORE_T y[4];
                ALWAN_CORE_T x_vals[4];

                int start = (int)idx - 1;
                if (start < 0) start = 0;
                if (start + 4 > (int)count_in) start = (int)count_in - 4;

                for (int j = 0; j < 4; j++) {
                    y[j] = y_in[start + j];
                    x_vals[j] = x_in[start + j];
                }

                ALWAN_CORE_T result = ALWAN_CORE_LITERAL(0.0);
                for (int j = 0; j < 4; j++) {
                    ALWAN_CORE_T term = y[j];
                    for (int k = 0; k < 4; k++) {
                        if (j != k) {
                            term *= (x - x_vals[k]) / (x_vals[j] - x_vals[k]);
                        }
                    }
                    result += term;
                }

                y_out[i] = result;
                break;
            }

            case ALWAN_INTERP_NEAREST: {
                /* scipy's midpoint (x0 + x1) / 2, a tie going to the lower sample */
                ALWAN_CORE_T const mid = (x0 + x1) / ALWAN_CORE_LITERAL(2.0);
                y_out[i] = (x <= mid) ? y_in[idx] : y_in[idx + 1];
                break;
            }

            case ALWAN_INTERP_PREVIOUS: {
                y_out[i] = y_in[idx];   /* x_in[idx] <= x < x_in[idx + 1] */
                break;
            }

            case ALWAN_INTERP_AKIMA:
            case ALWAN_INTERP_MAKIMA: {
                if (count_in < 3) {
                    y_out[i] = y_in[idx] * (ALWAN_CORE_LITERAL(1.0) - t) + y_in[idx + 1] * t;
                } else {
                    ALWAN_CORE_T const h = x1 - x0;
                    ALWAN_CORE_T const d0 = ALWAN_CORE_FN(akima_t)(x_in, y_in, count_in, idx, makima, akima_f12_max);
                    ALWAN_CORE_T const d1 = ALWAN_CORE_FN(akima_t)(x_in, y_in, count_in, idx + 1, makima, akima_f12_max);
                    ALWAN_CORE_T const slope = (y_in[idx + 1] - y_in[idx]) / h;
                    ALWAN_CORE_T const tt = (d0 + d1 - ALWAN_CORE_LITERAL(2.0) * slope) / h;
                    ALWAN_CORE_T const c0 = tt / h;
                    ALWAN_CORE_T const c1 = (slope - d0) / h - tt;
                    ALWAN_CORE_T const sx = x - x0;
                    y_out[i] = ((c0 * sx + c1) * sx + d0) * sx + y_in[idx];
                }
                break;
            }

            default:
                return ALWAN_E_RANGE;
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Enhanced Extrapolation Methods
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_extrapolate)(ALWAN_CORE_T const *x_in, ALWAN_CORE_T const *y_in, size_t count_in,
                       ALWAN_CORE_T const *x_out, ALWAN_CORE_T *y_out, size_t count_out,
                       alwan_extrap_method method) {
    if (!x_in || !y_in || !x_out || !y_out || count_in < 2 || count_out == 0) {
        return ALWAN_E_RANGE;
    }

    for (size_t i = 0; i < count_out; i++) {
        ALWAN_CORE_T x = x_out[i];
        int extrapolating = 0;
        int use_left = 0;

        if (x < x_in[0]) {
            extrapolating = 1;
            use_left = 1;
        } else if (x > x_in[count_in - 1]) {
            extrapolating = 1;
            use_left = 0;
        }

        if (!extrapolating) {
            size_t idx = ALWAN_CORE_FNLIT(alwan_math_find_interval)(x_in, count_in, x);
            ALWAN_CORE_T t = (x - x_in[idx]) / (x_in[idx + 1] - x_in[idx]);
            y_out[i] = y_in[idx] * (ALWAN_CORE_LITERAL(1.0) - t) + y_in[idx + 1] * t;
            continue;
        }

        switch (method) {
            case ALWAN_EXTRAP_CONSTANT: {
                y_out[i] = use_left ? y_in[0] : y_in[count_in - 1];
                break;
            }

            case ALWAN_EXTRAP_LINEAR: {
                if (use_left) {
                    ALWAN_CORE_T slope = (y_in[1] - y_in[0]) / (x_in[1] - x_in[0]);
                    y_out[i] = y_in[0] + slope * (x - x_in[0]);
                } else {
                    ALWAN_CORE_T slope = (y_in[count_in - 1] - y_in[count_in - 2]) /
                                        (x_in[count_in - 1] - x_in[count_in - 2]);
                    y_out[i] = y_in[count_in - 1] + slope * (x - x_in[count_in - 1]);
                }
                break;
            }

            case ALWAN_EXTRAP_POLYNOMIAL: {
                if (count_in < 3) {
                    if (use_left) {
                        ALWAN_CORE_T slope = (y_in[1] - y_in[0]) / (x_in[1] - x_in[0]);
                        y_out[i] = y_in[0] + slope * (x - x_in[0]);
                    } else {
                        ALWAN_CORE_T slope = (y_in[count_in - 1] - y_in[count_in - 2]) /
                                            (x_in[count_in - 1] - x_in[count_in - 2]);
                        y_out[i] = y_in[count_in - 1] + slope * (x - x_in[count_in - 1]);
                    }
                } else {
                    ALWAN_CORE_T x0, x1, x2, y0, y1, y2;
                    if (use_left) {
                        x0 = x_in[0]; y0 = y_in[0];
                        x1 = x_in[1]; y1 = y_in[1];
                        x2 = x_in[2]; y2 = y_in[2];
                    } else {
                        x0 = x_in[count_in - 3]; y0 = y_in[count_in - 3];
                        x1 = x_in[count_in - 2]; y1 = y_in[count_in - 2];
                        x2 = x_in[count_in - 1]; y2 = y_in[count_in - 1];
                    }

                    y_out[i] = y0 * ((x - x1) * (x - x2)) / ((x0 - x1) * (x0 - x2)) +
                               y1 * ((x - x0) * (x - x2)) / ((x1 - x0) * (x1 - x2)) +
                               y2 * ((x - x0) * (x - x1)) / ((x2 - x0) * (x2 - x1));
                }
                break;
            }

            case ALWAN_EXTRAP_EXPONENTIAL: {
                if (use_left) {
                    if (y_in[0] > ALWAN_CORE_LITERAL(0.0) && y_in[1] > ALWAN_CORE_LITERAL(0.0)) {
                        ALWAN_CORE_T ratio = y_in[1] / y_in[0];
                        ALWAN_CORE_T lambda = ALWAN_CORE_LN(ratio) / (x_in[1] - x_in[0]);
                        y_out[i] = y_in[0] * ALWAN_CORE_EXP(lambda * (x - x_in[0]));
                    } else {
                        y_out[i] = y_in[0];
                    }
                } else {
                    if (y_in[count_in - 1] > ALWAN_CORE_LITERAL(0.0) && y_in[count_in - 2] > ALWAN_CORE_LITERAL(0.0)) {
                        ALWAN_CORE_T ratio = y_in[count_in - 1] / y_in[count_in - 2];
                        ALWAN_CORE_T lambda = ALWAN_CORE_LN(ratio) / (x_in[count_in - 1] - x_in[count_in - 2]);
                        y_out[i] = y_in[count_in - 1] * ALWAN_CORE_EXP(lambda * (x - x_in[count_in - 1]));
                    } else {
                        y_out[i] = y_in[count_in - 1];
                    }
                }

                if (y_out[i] < ALWAN_CORE_LITERAL(0.0)) y_out[i] = ALWAN_CORE_LITERAL(0.0);
                break;
            }

            case ALWAN_EXTRAP_REFLECT: {
                ALWAN_CORE_T x_reflected;

                if (use_left) {
                    ALWAN_CORE_T dist = x_in[0] - x;
                    x_reflected = x_in[0] + dist;
                } else {
                    ALWAN_CORE_T dist = x - x_in[count_in - 1];
                    x_reflected = x_in[count_in - 1] - dist;
                }

                if (x_reflected < x_in[0]) x_reflected = x_in[0];
                if (x_reflected > x_in[count_in - 1]) x_reflected = x_in[count_in - 1];

                size_t idx = ALWAN_CORE_FNLIT(alwan_math_find_interval)(x_in, count_in, x_reflected);
                ALWAN_CORE_T t = (x_reflected - x_in[idx]) / (x_in[idx + 1] - x_in[idx]);
                y_out[i] = y_in[idx] * (ALWAN_CORE_LITERAL(1.0) - t) + y_in[idx + 1] * t;
                break;
            }

            case ALWAN_EXTRAP_NATURAL: {
                y_out[i] = use_left ? y_in[0] : y_in[count_in - 1];
                break;
            }

            default:
                return ALWAN_E_RANGE;
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 1D Table Interpolation
 * ---------------------------------------------------------------- */

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_table_interp_1d)(ALWAN_CORE_T const *table, size_t size,
                                    ALWAN_CORE_T x, alwan_interp_method method) {
    if (!table || size < 2) {
        return ALWAN_CORE_LITERAL(0.0);
    }

    /* Clamp x to [0, 1]. Written as !(x > 0) rather than x <= 0 so a NaN takes
     * this branch too: NaN fails every compare, and letting it reach the
     * (size_t) cast below is undefined behaviour. Low edge matches the
     * addressing gate in core/alwan_table_core. */
    if (!(x > ALWAN_CORE_LITERAL(0.0))) return table[0];
    if (x >= ALWAN_CORE_LITERAL(1.0)) return table[size - 1];

    /* Map x to table index */
    ALWAN_CORE_T pos = x * (ALWAN_CORE_T)(size - 1);
    size_t idx = (size_t)pos;
    ALWAN_CORE_T t = pos - (ALWAN_CORE_T)idx;

    if (idx >= size - 1) {
        return table[size - 1];
    }

    switch (method) {
        case ALWAN_INTERP_LINEAR: {
            return table[idx] * (ALWAN_CORE_LITERAL(1.0) - t) + table[idx + 1] * t;
        }

        case ALWAN_INTERP_CUBIC: {
            ALWAN_CORE_T y0 = (idx > 0) ? table[idx - 1] : table[idx];
            ALWAN_CORE_T y1 = table[idx];
            ALWAN_CORE_T y2 = table[idx + 1];
            ALWAN_CORE_T y3 = (idx + 2 < size) ? table[idx + 2] : table[idx + 1];

            return ALWAN_CORE_FNVLIT(alwan_catmull_rom)(y0, y1, y2, y3, t);
        }

        default:
            return table[idx] * (ALWAN_CORE_LITERAL(1.0) - t) + table[idx + 1] * t;
    }
}

/* ----------------------------------------------------------------
 * CCT and Duv Optimization
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_cct_duv_optimize)(ALWAN_CORE_T *cct_out, ALWAN_CORE_T *duv_out, ALWAN_CORE_VEC2 const *xy) {
    /* duv_out is documented as optional; the previous code rejected NULL,
     * contradicting the header. ALWAN_E_INVALID is both what the header
     * promises and the house convention for a null argument. */
    if (!xy || !cct_out) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_T x = xy->v[0];
    ALWAN_CORE_T y = xy->v[1];

    /* Solved in mired (reciprocal megakelvin), not in kelvin.
     *
     * The Planckian locus is close to uniform in mired, so the objective
     * Duv(T)^2 has roughly constant curvature across the whole range. In kelvin
     * it does not: d(u,v)/dT falls off as 1/T^2, so the curvature of the
     * objective drops to ~2e-12 at 25000 K. The previous implementation ran
     * Newton-Raphson in kelvin behind an absolute guard
     * `if (fabs(second_deriv) < 1e-10) break;`, which therefore fired on
     * iteration 0 for every CCT above roughly 7300 K and returned the raw
     * McCamy seed unchanged: 5496 K of error at 25000 K, against a documented
     * bound of 1 K.
     *
     * Golden-section over a coarse bracket replaces it. It has no
     * scale-dependent threshold to get wrong, needs no derivatives of a
     * quantity whose natural magnitude is 1e-12, runs a fixed number of
     * iterations (so it is deterministic and has no convergence cliff), and the
     * coarse scan means a global minimum is still found if the objective is not
     * unimodal for a far-off-locus input. */

    /* Search only where the locus model is defined. Kang 2002 is published for
     * 1667-25000 K; below 1667 K its polynomial is pure extrapolation and dives
     * away from the real locus, which puts a spurious global minimum down
     * there. A search over the old [1000, 25000] clamp finds that false
     * minimum and returns ~1000 K for a genuine 1700 K input. Inputs whose
     * nearest locus point lies outside this range clamp to the nearest end. */
    ALWAN_CORE_T const mired_min = ALWAN_CORE_LITERAL(40.0);            /* 25000 K */
    ALWAN_CORE_T const mired_max = ALWAN_CORE_LITERAL(599.88002399520); /*  1667 K */

    int const scan_steps = 64;
    ALWAN_CORE_T best_m = mired_min;
    ALWAN_CORE_T best_f = ALWAN_CORE_LITERAL(-1.0);

    for (int i = 0; i <= scan_steps; i++) {
        ALWAN_CORE_T m = mired_min + (mired_max - mired_min) *
                         ((ALWAN_CORE_T)i / (ALWAN_CORE_T)scan_steps);
        ALWAN_CORE_T d = ALWAN_CORE_FNVLIT(alwan_compute_duv)(x, y, ALWAN_CORE_LITERAL(1.0e6) / m);
        ALWAN_CORE_T f = d * d;
        if (best_f < ALWAN_CORE_LITERAL(0.0) || f < best_f) {
            best_f = f;
            best_m = m;
        }
    }

    /* Bracket the coarse winner by one scan step on each side, clamped. */
    ALWAN_CORE_T const step = (mired_max - mired_min) / (ALWAN_CORE_T)scan_steps;
    ALWAN_CORE_T a = best_m - step;
    ALWAN_CORE_T b = best_m + step;
    if (a < mired_min) a = mired_min;
    if (b > mired_max) b = mired_max;

    /* Golden-section search. 48 iterations shrink a 30-mired bracket by
     * 0.618^48 ~ 5e-11, far below the 1e-4 mired (0.06 K at 25000 K) needed to
     * hold the documented 1 K bound. */
    ALWAN_CORE_T const inv_phi = ALWAN_CORE_LITERAL(0.61803398874989484820);
    ALWAN_CORE_T c = b - (b - a) * inv_phi;
    ALWAN_CORE_T d_pt = a + (b - a) * inv_phi;
    ALWAN_CORE_T dc = ALWAN_CORE_FNVLIT(alwan_compute_duv)(x, y, ALWAN_CORE_LITERAL(1.0e6) / c);
    ALWAN_CORE_T dd = ALWAN_CORE_FNVLIT(alwan_compute_duv)(x, y, ALWAN_CORE_LITERAL(1.0e6) / d_pt);
    ALWAN_CORE_T fc = dc * dc;
    ALWAN_CORE_T fd = dd * dd;

    for (int i = 0; i < 48; i++) {
        if (fc < fd) {
            b = d_pt;
            d_pt = c; fd = fc;
            c = b - (b - a) * inv_phi;
            dc = ALWAN_CORE_FNVLIT(alwan_compute_duv)(x, y, ALWAN_CORE_LITERAL(1.0e6) / c);
            fc = dc * dc;
        } else {
            a = c;
            c = d_pt; fc = fd;
            d_pt = a + (b - a) * inv_phi;
            dd = ALWAN_CORE_FNVLIT(alwan_compute_duv)(x, y, ALWAN_CORE_LITERAL(1.0e6) / d_pt);
            fd = dd * dd;
        }
    }

    ALWAN_CORE_T m_star = (a + b) * ALWAN_CORE_LITERAL(0.5);
    ALWAN_CORE_T cct = ALWAN_CORE_LITERAL(1.0e6) / m_star;

    if (cct < ALWAN_CORE_LITERAL(1667.0)) cct = ALWAN_CORE_LITERAL(1667.0);
    if (cct > ALWAN_CORE_LITERAL(25000.0)) cct = ALWAN_CORE_LITERAL(25000.0);

    *cct_out = cct;
    if (duv_out) {
        /* Signed, per Ohno 2013 / ANSI C78.377: positive above the locus. */
        *duv_out = ALWAN_CORE_FNVLIT(alwan_compute_duv)(x, y, cct);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Tristimulus Optimization (Gaussian basis approximation)
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_optimize_spectrum_for_xyz)(ALWAN_CORE_SPD *spd_out,
                                      ALWAN_CORE_XYZ const *target_xyz,
                                      alwan_observer_type observer,
                                      alwan_ctx *ctx) {
    /* A weighted sum of seven Gaussians, solved so that the result MEETS the
     * target under the requested observer.
     *
     * What this did until 2026-09-22, and why every part of it is now
     * different:
     *
     *   - It rejected a NULL ctx, which it never used, so every caller that
     *     followed the library's own convention got ALWAN_E_RANGE. The
     *     determinism dump is such a caller: its five optimize_spectrum
     *     sections had never been written.
     *   - It ignored `observer`, with a (void) cast.
     *   - It ignored the target's X and Z. Each of the seven weights was
     *     target_xyz->y / 7, so the output was one fixed curve scaled by
     *     luminance, and two targets with the same Y produced the same
     *     "match". Nothing was optimised.
     *   - It forced wavelength_min 380, max 780 and count 81 over whatever the
     *     caller had allocated, so a caller who pre-allocated the range the
     *     header tells it to pre-allocate, at any other count, was written
     *     past the end of its buffer.
     *
     * The solve. XYZ is a linear functional of the SPD, so with A the 3x7
     * matrix whose column j is the XYZ of basis function j at unit weight,
     * every reachable target is A w for some weight vector w. Three equations
     * in seven unknowns: take the minimum-norm solution,
     * w = A^T (A A^T)^-1 xyz, which is exact rather than a fit.
     *
     * A is built by putting each basis function through alwan_xyz_from_spd on
     * the caller's own grid, so the identity holds for the grid the answer is
     * written on rather than for an ideal one.
     *
     * A NEGATIVE weight means the target is not a non-negative mixture of this
     * basis, which is to say there is no SPD of this shape with the power it
     * asks for. That is ALWAN_E_RANGE, not an SPD that misses the target: of
     * D65 white, a neutral and the three sRGB primaries, the first two are
     * reachable and match to 4e-14, and the three saturated primaries are not
     * reachable at all. A caller that wants a spectrum for a saturated colour
     * wants alwan_xyz_to_spectrum_otsu2018 or the RGB upsamplers, which solve
     * a different and better-posed problem.
     */
    size_t const num_gaussians = 7;
    ALWAN_CORE_T const centers[7] = {
        ALWAN_CORE_LITERAL(420.0), ALWAN_CORE_LITERAL(470.0), ALWAN_CORE_LITERAL(520.0),
        ALWAN_CORE_LITERAL(570.0), ALWAN_CORE_LITERAL(600.0), ALWAN_CORE_LITERAL(630.0),
        ALWAN_CORE_LITERAL(680.0)
    };
    ALWAN_CORE_T const width = ALWAN_CORE_LITERAL(40.0);
    ALWAN_CORE_T A[3][7];
    ALWAN_CORE_T w[7];
    ALWAN_CORE_MAT3X3 gram, gram_inv;
    ALWAN_CORE_T step, y0, y1, y2;
    size_t i, j;
    int r, c;
    alwan_status st;

    if (!target_xyz || !spd_out || !spd_out->values) {
        return ALWAN_E_INVALID;
    }
    /* count == 0 means "use the default grid", which is what this function has
     * always done to every caller: 380-780 nm at 81 samples. values[] must then
     * have room for 81. A caller that fills in a count gets THAT grid, which is
     * what the header asks it to pre-allocate and what this used to overwrite. */
    if (spd_out->count == 0) {
        spd_out->wavelength_min = ALWAN_CORE_LITERAL(380.0);
        spd_out->wavelength_max = ALWAN_CORE_LITERAL(780.0);
        spd_out->count = 81;
    }
    if (spd_out->count < 2 || !(spd_out->wavelength_max > spd_out->wavelength_min)) {
        return ALWAN_E_INVALID;
    }

    step = (spd_out->wavelength_max - spd_out->wavelength_min)
         / (ALWAN_CORE_T)(spd_out->count - 1);

    /* Column j of A: basis function j alone, through the library's own
     * integrator, on the caller's grid. spd_out is the scratch space for it,
     * which is why a failure below leaves it zeroed rather than half-written. */
    for (j = 0; j < num_gaussians; j++) {
        ALWAN_CORE_XYZ col;
        for (i = 0; i < spd_out->count; i++) {
            ALWAN_CORE_T const lambda = spd_out->wavelength_min + (ALWAN_CORE_T)i * step;
            ALWAN_CORE_T const dx = (lambda - centers[j]) / width;
            spd_out->values[i] = ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(-0.5) * dx * dx);
        }
        st = ALWAN_CORE_FNLIT(alwan_xyz_from_spd)(&col, spd_out, NULL, observer,
                                                  ALWAN_INTEGRATE_TRAPEZOID,
                                                  ALWAN_CORE_LITERAL(0.0), ctx);
        if (st != ALWAN_OK) {
            for (i = 0; i < spd_out->count; i++) spd_out->values[i] = ALWAN_CORE_LITERAL(0.0);
            return st;
        }
        A[0][j] = col.x;
        A[1][j] = col.y;
        A[2][j] = col.z;
    }

    /* A A^T, and the minimum-norm weights. */
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
            for (j = 0; j < num_gaussians; j++) sum += A[r][j] * A[c][j];
            gram.m[r * 3 + c] = sum;
        }
    }
    if (ALWAN_CORE_FNLIT(alwan_mat3_inv)(&gram_inv, &gram) != ALWAN_OK) {
        /* The basis does not span XYZ on this grid: too few samples, or a
         * range that misses the visible band. */
        for (i = 0; i < spd_out->count; i++) spd_out->values[i] = ALWAN_CORE_LITERAL(0.0);
        return ALWAN_E_RANGE;
    }
    y0 = gram_inv.m[0] * target_xyz->x + gram_inv.m[1] * target_xyz->y + gram_inv.m[2] * target_xyz->z;
    y1 = gram_inv.m[3] * target_xyz->x + gram_inv.m[4] * target_xyz->y + gram_inv.m[5] * target_xyz->z;
    y2 = gram_inv.m[6] * target_xyz->x + gram_inv.m[7] * target_xyz->y + gram_inv.m[8] * target_xyz->z;
    for (j = 0; j < num_gaussians; j++) {
        w[j] = A[0][j] * y0 + A[1][j] * y1 + A[2][j] * y2;
        if (w[j] < ALWAN_CORE_LITERAL(0.0)) {
            for (i = 0; i < spd_out->count; i++) spd_out->values[i] = ALWAN_CORE_LITERAL(0.0);
            return ALWAN_E_RANGE;
        }
    }

    /* The answer. Every weight and every basis value is non-negative, so no
     * clamp is needed and none is applied. */
    for (i = 0; i < spd_out->count; i++) {
        ALWAN_CORE_T const lambda = spd_out->wavelength_min + (ALWAN_CORE_T)i * step;
        ALWAN_CORE_T value = ALWAN_CORE_LITERAL(0.0);
        for (j = 0; j < num_gaussians; j++) {
            ALWAN_CORE_T const dx = (lambda - centers[j]) / width;
            value += w[j] * ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(-0.5) * dx * dx);
        }
        spd_out->values[i] = value;
    }

    return ALWAN_OK;
}
