/* No include guard — included twice (f32 and f64 passes) */

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

int ALWAN_CORE_FNLIT(alwan_mat3_inv)(ALWAN_CORE_MAT3X3 *out, ALWAN_CORE_MAT3X3 const *m) {
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
 * Advanced Interpolation Methods
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_interpolate)(ALWAN_CORE_T const *x_in, ALWAN_CORE_T const *y_in, size_t count_in,
                       ALWAN_CORE_T const *x_out, ALWAN_CORE_T *y_out, size_t count_out,
                       alwan_interp_method method) {
    if (!x_in || !y_in || !x_out || !y_out || count_in < 2 || count_out == 0) {
        return ALWAN_E_RANGE;
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

            case ALWAN_INTERP_LANCZOS: {
                const int a = 3;
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
                    ALWAN_CORE_T y0 = (idx > 0) ? y_in[idx - 1] : y_in[idx];
                    ALWAN_CORE_T y1 = y_in[idx];
                    ALWAN_CORE_T y2 = y_in[idx + 1];
                    ALWAN_CORE_T y3 = (idx + 2 < count_in) ? y_in[idx + 2] : y_in[idx + 1];

                    y_out[i] = ALWAN_CORE_FNVLIT(alwan_catmull_rom)(y0, y1, y2, y3, t);
                } else {
                    ALWAN_CORE_T y[6];
                    for (int j = 0; j < 6; j++) {
                        int k = (int)idx + j - 2;
                        if (k < 0) k = 0;
                        if (k >= (int)count_in) k = (int)count_in - 1;
                        y[j] = y_in[k];
                    }

                    ALWAN_CORE_T t2 = t * t;
                    ALWAN_CORE_T t3 = t2 * t;
                    ALWAN_CORE_T t4 = t3 * t;
                    ALWAN_CORE_T t5 = t4 * t;

                    y_out[i] =
                        y[2] +
                        ALWAN_CORE_LITERAL(0.5) * t * (y[3] - y[1]) +
                        ALWAN_CORE_LITERAL(0.5) * t2 * (y[3] - ALWAN_CORE_LITERAL(2.0) * y[2] + y[1]) +
                        (ALWAN_CORE_LITERAL(1.0)/ALWAN_CORE_LITERAL(6.0)) * t3 * (y[4] - ALWAN_CORE_LITERAL(3.0) * y[3] + ALWAN_CORE_LITERAL(3.0) * y[1] - y[0]) +
                        (ALWAN_CORE_LITERAL(1.0)/ALWAN_CORE_LITERAL(24.0)) * t4 * (y[4] - ALWAN_CORE_LITERAL(4.0) * y[3] + ALWAN_CORE_LITERAL(6.0) * y[2] - ALWAN_CORE_LITERAL(4.0) * y[1] + y[0]) +
                        (ALWAN_CORE_LITERAL(1.0)/ALWAN_CORE_LITERAL(120.0)) * t5 * (y[5] - ALWAN_CORE_LITERAL(5.0) * y[4] + ALWAN_CORE_LITERAL(10.0) * y[3] - ALWAN_CORE_LITERAL(10.0) * y[2] + ALWAN_CORE_LITERAL(5.0) * y[1] - y[0]);
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

            case ALWAN_INTERP_AKIMA: {
                if (count_in < 4) {
                    y_out[i] = y_in[idx] * (ALWAN_CORE_LITERAL(1.0) - t) + y_in[idx + 1] * t;
                } else {
                    ALWAN_CORE_T m[5];
                    for (int j = -2; j <= 2; j++) {
                        int k = (int)idx + j;
                        if (k < 0) k = 0;
                        if (k >= (int)count_in - 1) k = (int)count_in - 2;
                        m[j + 2] = (y_in[k + 1] - y_in[k]) / (x_in[k + 1] - x_in[k]);
                    }

                    ALWAN_CORE_T w1 = ALWAN_CORE_ABS(m[3] - m[2]);
                    ALWAN_CORE_T w2 = ALWAN_CORE_ABS(m[1] - m[0]);
                    ALWAN_CORE_T slope = (w1 + w2 > ALWAN_CORE_LITERAL(0.0)) ?
                        ((w1 * m[1] + w2 * m[3]) / (w1 + w2)) :
                        (ALWAN_CORE_LITERAL(0.5) * (m[1] + m[3]));

                    ALWAN_CORE_T y1 = y_in[idx];
                    ALWAN_CORE_T y2 = y_in[idx + 1];
                    ALWAN_CORE_T dx = x1 - x0;

                    ALWAN_CORE_T h00 = (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(2.0) * t) * (ALWAN_CORE_LITERAL(1.0) - t) * (ALWAN_CORE_LITERAL(1.0) - t);
                    ALWAN_CORE_T h10 = t * (ALWAN_CORE_LITERAL(1.0) - t) * (ALWAN_CORE_LITERAL(1.0) - t);
                    ALWAN_CORE_T h01 = t * t * (ALWAN_CORE_LITERAL(3.0) - ALWAN_CORE_LITERAL(2.0) * t);
                    ALWAN_CORE_T h11 = t * t * (t - ALWAN_CORE_LITERAL(1.0));

                    y_out[i] = h00 * y1 + h10 * dx * slope + h01 * y2 + h11 * dx * slope;
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

int ALWAN_CORE_FNLIT(alwan_extrapolate)(ALWAN_CORE_T const *x_in, ALWAN_CORE_T const *y_in, size_t count_in,
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

    /* Clamp x to [0, 1] */
    if (x <= ALWAN_CORE_LITERAL(0.0)) return table[0];
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

int ALWAN_CORE_FNLIT(alwan_cct_duv_optimize)(ALWAN_CORE_T *cct_out, ALWAN_CORE_T *duv_out, ALWAN_CORE_VEC2 const *xy) {
    if (!xy || !cct_out || !duv_out) {
        return ALWAN_E_RANGE;
    }

    ALWAN_CORE_T x = xy->v[0];
    ALWAN_CORE_T y = xy->v[1];

    /* Initial CCT estimate using McCamy (1992) formula */
    ALWAN_CORE_T cct = ALWAN_CORE_FNVLIT(alwan_mccamy_cct)(x, y);

    /* Clamp to [1000, 25000] K */
    if (cct < ALWAN_CORE_LITERAL(1000.0)) cct = ALWAN_CORE_LITERAL(1000.0);
    if (cct > ALWAN_CORE_LITERAL(25000.0)) cct = ALWAN_CORE_LITERAL(25000.0);

    /* Newton-Raphson iteration to find minimum Duv */
    const int max_iter = 20;
    const ALWAN_CORE_T tol = ALWAN_CORE_LITERAL(0.01);
    const ALWAN_CORE_T h = ALWAN_CORE_LITERAL(1.0);

    for (int iter = 0; iter < max_iter; iter++) {
        ALWAN_CORE_T duv_minus = ALWAN_CORE_FNVLIT(alwan_compute_duv)(x, y, cct - h);
        ALWAN_CORE_T duv_current = ALWAN_CORE_FNVLIT(alwan_compute_duv)(x, y, cct);
        ALWAN_CORE_T duv_plus = ALWAN_CORE_FNVLIT(alwan_compute_duv)(x, y, cct + h);

        ALWAN_CORE_T f_minus = duv_minus * duv_minus;
        ALWAN_CORE_T f_current = duv_current * duv_current;
        ALWAN_CORE_T f_plus = duv_plus * duv_plus;

        ALWAN_CORE_T first_deriv = (f_plus - f_minus) / (ALWAN_CORE_LITERAL(2.0) * h);
        ALWAN_CORE_T second_deriv = (f_plus - ALWAN_CORE_LITERAL(2.0) * f_current + f_minus) / (h * h);

        if (ALWAN_CORE_ABS(first_deriv) < ALWAN_CORE_LITERAL(1e-10)) break;
        if (ALWAN_CORE_ABS(second_deriv) < ALWAN_CORE_LITERAL(1e-10)) break;

        ALWAN_CORE_T delta = -first_deriv / second_deriv;
        cct += delta;

        if (cct < ALWAN_CORE_LITERAL(1000.0)) cct = ALWAN_CORE_LITERAL(1000.0);
        if (cct > ALWAN_CORE_LITERAL(25000.0)) cct = ALWAN_CORE_LITERAL(25000.0);

        if (ALWAN_CORE_ABS(delta) < tol) break;
    }

    *cct_out = cct;
    *duv_out = ALWAN_CORE_FNVLIT(alwan_compute_duv)(x, y, cct);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Tristimulus Optimization (Gaussian basis approximation)
 * ---------------------------------------------------------------- */

int ALWAN_CORE_FNLIT(alwan_optimize_spectrum_for_xyz)(ALWAN_CORE_SPD *spd_out,
                                      ALWAN_CORE_XYZ const *target_xyz,
                                      alwan_observer_type observer,
                                      alwan_ctx *ctx) {
    (void)observer;

    if (!target_xyz || !ctx || !spd_out || !spd_out->values) {
        return ALWAN_E_RANGE;
    }

    const size_t num_gaussians = 7;
    const ALWAN_CORE_T centers[7] = {
        ALWAN_CORE_LITERAL(420.0), ALWAN_CORE_LITERAL(470.0), ALWAN_CORE_LITERAL(520.0),
        ALWAN_CORE_LITERAL(570.0), ALWAN_CORE_LITERAL(600.0), ALWAN_CORE_LITERAL(630.0),
        ALWAN_CORE_LITERAL(680.0)
    };
    const ALWAN_CORE_T widths[7] = {
        ALWAN_CORE_LITERAL(40.0), ALWAN_CORE_LITERAL(40.0), ALWAN_CORE_LITERAL(40.0),
        ALWAN_CORE_LITERAL(40.0), ALWAN_CORE_LITERAL(40.0), ALWAN_CORE_LITERAL(40.0),
        ALWAN_CORE_LITERAL(40.0)
    };

    spd_out->wavelength_min = ALWAN_CORE_LITERAL(380.0);
    spd_out->wavelength_max = ALWAN_CORE_LITERAL(780.0);
    spd_out->count = 81;

    ALWAN_CORE_T weights[7];
    ALWAN_CORE_T luminance = target_xyz->y;
    for (size_t i = 0; i < num_gaussians; i++) {
        weights[i] = luminance / (ALWAN_CORE_T)num_gaussians;
    }

    ALWAN_CORE_T wavelength_step = (spd_out->wavelength_max - spd_out->wavelength_min) / (ALWAN_CORE_T)(spd_out->count - 1);
    for (size_t i = 0; i < spd_out->count; i++) {
        ALWAN_CORE_T lambda = spd_out->wavelength_min + (ALWAN_CORE_T)i * wavelength_step;
        ALWAN_CORE_T value = ALWAN_CORE_LITERAL(0.0);

        for (size_t j = 0; j < num_gaussians; j++) {
            ALWAN_CORE_T dx = (lambda - centers[j]) / widths[j];
            value += weights[j] * ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(-0.5) * dx * dx);
        }

        spd_out->values[i] = (value > ALWAN_CORE_LITERAL(0.0)) ? value : ALWAN_CORE_LITERAL(0.0);
    }

    return ALWAN_OK;
}
