/*
 * Lightness, luminance, Munsell value, whiteness and yellowness, instantiated per
 * precision by alwan_lightness.c. Each formula is colour-science's, in its order of
 * operations.
 */

#define ALWAN__LP ALWAN_CORE_FNLIT(alwan_lightness_params)
#define ALWAN__T  ALWAN_CORE_T
#define ALWAN__L(x) ALWAN_CORE_LITERAL(x)

/* colour-science's spow: the power of |x|, with x's sign. */
static ALWAN__T ALWAN_CORE_FNLIT(alwan__lt_spow)(ALWAN__T x, ALWAN__T p) {
    ALWAN__T const m = ALWAN_CORE_ABS(x);
    ALWAN__T const r = m == ALWAN__L(0.0) ? ALWAN__L(0.0) : ALWAN_CORE_POW(m, p);
    return x < ALWAN__L(0.0) ? -r : r;
}
#define ALWAN__SPOW ALWAN_CORE_FNLIT(alwan__lt_spow)

/* The parameters, defaults applied; ALWAN_E_INVALID for a negative one. */
static alwan_status ALWAN_CORE_FNLIT(alwan__lt_params)(ALWAN__T *Y_n, ALWAN__T *eps, ALWAN__LP const *p, int method) {
    ALWAN__T const yn = p ? p->Y_n : ALWAN__L(0.0), e = p ? p->epsilon : ALWAN__L(0.0);
    if (yn < ALWAN__L(0.0) || e < ALWAN__L(0.0)) return ALWAN_E_INVALID;
    *Y_n = yn == ALWAN__L(0.0) ? ALWAN__L(100.0) : yn;
    *eps = e != ALWAN__L(0.0) ? e : (method == (int)ALWAN_LIGHTNESS_FAIRCHILD2010 ? ALWAN__L(1.836) : ALWAN__L(0.474));
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_lightness)(ALWAN__T *L_out, ALWAN__T Y, alwan_lightness_method method,
                                               ALWAN__LP const *params) {
    ALWAN__T Y_n, eps, t, S;
    int const m = (int)method;
    if (!L_out || m < (int)ALWAN_LIGHTNESS_CIE1976 || m > (int)ALWAN_LIGHTNESS_ABEBE2017_STEVENS) return ALWAN_E_INVALID;
    if (ALWAN_CORE_FNLIT(alwan__lt_params)(&Y_n, &eps, params, m) != ALWAN_OK) return ALWAN_E_INVALID;
    switch (m) {
    case ALWAN_LIGHTNESS_CIE1976:
        t = Y / Y_n;
        t = t > (ALWAN__L(24.0) / ALWAN__L(116.0)) * (ALWAN__L(24.0) / ALWAN__L(116.0)) * (ALWAN__L(24.0) / ALWAN__L(116.0))
                ? ALWAN__SPOW(t, ALWAN__L(1.0) / ALWAN__L(3.0))
                : (ALWAN__L(841.0) / ALWAN__L(108.0)) * t + ALWAN__L(16.0) / ALWAN__L(116.0);
        *L_out = ALWAN__L(116.0) * t - ALWAN__L(16.0);
        break;
    case ALWAN_LIGHTNESS_GLASSER1958:
        *L_out = ALWAN__L(25.29) * ALWAN__SPOW(Y, ALWAN__L(1.0) / ALWAN__L(3.0)) - ALWAN__L(18.38);
        break;
    case ALWAN_LIGHTNESS_WYSZECKI1963:
        *L_out = ALWAN__L(25.0) * ALWAN__SPOW(Y, ALWAN__L(1.0) / ALWAN__L(3.0)) - ALWAN__L(17.0);
        break;
    case ALWAN_LIGHTNESS_FAIRCHILD2010:
        S = ALWAN__SPOW(Y, eps);
        *L_out = (ALWAN__L(100.0) * S) / (ALWAN__SPOW(ALWAN__L(0.184), eps) + S) + ALWAN__L(0.02);
        break;
    case ALWAN_LIGHTNESS_FAIRCHILD2011_CIELAB:
    case ALWAN_LIGHTNESS_FAIRCHILD2011_IPT:
        S = ALWAN__SPOW(Y, eps);
        t = m == (int)ALWAN_LIGHTNESS_FAIRCHILD2011_CIELAB ? ALWAN__L(247.0) : ALWAN__L(246.0);
        *L_out = (t * S) / (ALWAN__SPOW(ALWAN__L(2.0), eps) + S) + ALWAN__L(0.02);
        break;
    case ALWAN_LIGHTNESS_ABEBE2017_MICHAELIS_MENTEN:
        t = Y / Y_n;
        if (Y_n <= ALWAN__L(100.0)) {
            S = ALWAN__SPOW(t, ALWAN__L(0.582));
            *L_out = (ALWAN__L(1.448) * S) / (ALWAN__L(0.813) * S + ALWAN__L(0.635));
        } else {
            S = ALWAN__SPOW(t, ALWAN__L(0.293));
            *L_out = (ALWAN__L(1.680) * S) / (ALWAN__L(0.096) * S + ALWAN__L(1.584));
        }
        break;
    default: /* ALWAN_LIGHTNESS_ABEBE2017_STEVENS */
        t = Y / Y_n;
        *L_out = Y_n <= ALWAN__L(100.0) ? ALWAN__L(1.226) * ALWAN__SPOW(t, ALWAN__L(0.266)) - ALWAN__L(0.226)
                                        : ALWAN__L(1.127) * ALWAN__SPOW(t, ALWAN__L(0.230)) - ALWAN__L(0.127);
        break;
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_luminance_from_lightness)(ALWAN__T *Y_out, ALWAN__T L, alwan_lightness_method method,
                                                              ALWAN__LP const *params) {
    ALWAN__T Y_n, eps, f, v, S, vmax;
    int const m = (int)method;
    if (!Y_out || m < (int)ALWAN_LIGHTNESS_CIE1976 || m > (int)ALWAN_LIGHTNESS_ABEBE2017_STEVENS) return ALWAN_E_INVALID;
    if (ALWAN_CORE_FNLIT(alwan__lt_params)(&Y_n, &eps, params, m) != ALWAN_OK) return ALWAN_E_INVALID;
    switch (m) {
    case ALWAN_LIGHTNESS_CIE1976:
        f = (L + ALWAN__L(16.0)) / ALWAN__L(116.0);
        *Y_out = f > ALWAN__L(24.0) / ALWAN__L(116.0)
                     ? Y_n * f * f * f
                     : Y_n * (f - ALWAN__L(16.0) / ALWAN__L(116.0)) * (ALWAN__L(108.0) / ALWAN__L(841.0));
        break;
    case ALWAN_LIGHTNESS_GLASSER1958:
        f = (L + ALWAN__L(18.38)) / ALWAN__L(25.29);
        *Y_out = ALWAN__SPOW(f, ALWAN__L(3.0));
        break;
    case ALWAN_LIGHTNESS_WYSZECKI1963:
        f = (L + ALWAN__L(17.0)) / ALWAN__L(25.0);
        *Y_out = ALWAN__SPOW(f, ALWAN__L(3.0));
        break;
    case ALWAN_LIGHTNESS_FAIRCHILD2010:
    case ALWAN_LIGHTNESS_FAIRCHILD2011_CIELAB:
    case ALWAN_LIGHTNESS_FAIRCHILD2011_IPT: {
        ALWAN__T const K = ALWAN__SPOW(m == (int)ALWAN_LIGHTNESS_FAIRCHILD2010 ? ALWAN__L(0.184) : ALWAN__L(2.0), eps);
        vmax = m == (int)ALWAN_LIGHTNESS_FAIRCHILD2010 ? ALWAN__L(100.0)
             : (m == (int)ALWAN_LIGHTNESS_FAIRCHILD2011_CIELAB ? ALWAN__L(247.0) : ALWAN__L(246.0));
        v = L - ALWAN__L(0.02);
        S = (v * K) / (vmax - v);
        *Y_out = ALWAN_CORE_EXP(ALWAN_CORE_LN(S) / eps);
        break;
    }
    case ALWAN_LIGHTNESS_ABEBE2017_MICHAELIS_MENTEN:
        if (Y_n <= ALWAN__L(100.0)) {
            S = (L * ALWAN__L(0.635)) / (ALWAN__L(1.448) - ALWAN__L(0.813) * L);
            *Y_out = ALWAN__SPOW(S, ALWAN__L(1.0) / ALWAN__L(0.582)) * Y_n;
        } else {
            S = (L * ALWAN__L(1.584)) / (ALWAN__L(1.680) - ALWAN__L(0.096) * L);
            *Y_out = ALWAN__SPOW(S, ALWAN__L(1.0) / ALWAN__L(0.293)) * Y_n;
        }
        break;
    default: /* ALWAN_LIGHTNESS_ABEBE2017_STEVENS */
        *Y_out = (Y_n <= ALWAN__L(100.0)
                      ? ALWAN__SPOW((L + ALWAN__L(0.226)) / ALWAN__L(1.226), ALWAN__L(1.0) / ALWAN__L(0.266))
                      : ALWAN__SPOW((L + ALWAN__L(0.127)) / ALWAN__L(1.127), ALWAN__L(1.0) / ALWAN__L(0.230))) *
                 Y_n;
        break;
    }
    return ALWAN_OK;
}

/* ASTM D1535's quintic, and Newhall, Nickerson and Judd's. */
static ALWAN__T ALWAN_CORE_FNLIT(alwan__lt_d1535)(ALWAN__T V) {
    return ALWAN__L(1.1914) * V - ALWAN__L(0.22533) * ALWAN__SPOW(V, ALWAN__L(2.0)) +
           ALWAN__L(0.23352) * ALWAN__SPOW(V, ALWAN__L(3.0)) - ALWAN__L(0.020484) * ALWAN__SPOW(V, ALWAN__L(4.0)) +
           ALWAN__L(0.00081939) * ALWAN__SPOW(V, ALWAN__L(5.0));
}

alwan_status ALWAN_CORE_FNLIT(alwan_luminance_from_munsell_value)(ALWAN__T *Y_out, ALWAN__T V,
                                                                 alwan_munsell_luminance_method method) {
    if (!Y_out) return ALWAN_E_INVALID;
    switch ((int)method) {
    case ALWAN_MUNSELL_LUMINANCE_ASTM_D1535:
        *Y_out = ALWAN_CORE_FNLIT(alwan__lt_d1535)(V);
        return ALWAN_OK;
    case ALWAN_MUNSELL_LUMINANCE_NEWHALL1943:
        *Y_out = ALWAN__L(1.2219) * V - ALWAN__L(0.23111) * (V * V) + ALWAN__L(0.23951) * ALWAN__SPOW(V, ALWAN__L(3.0)) -
                 ALWAN__L(0.021009) * ALWAN__SPOW(V, ALWAN__L(4.0)) + ALWAN__L(0.0008404) * ALWAN__SPOW(V, ALWAN__L(5.0));
        return ALWAN_OK;
    default:
        return ALWAN_E_INVALID;
    }
}

alwan_status ALWAN_CORE_FNLIT(alwan_munsell_value)(ALWAN__T *V_out, ALWAN__T Y, alwan_munsell_value_method method) {
    ALWAN__T const c13 = ALWAN__L(1.0) / ALWAN__L(3.0);
    if (!V_out) return ALWAN_E_INVALID;
    switch ((int)method) {
    case ALWAN_MUNSELL_VALUE_ASTM_D1535: {
        /* The quintic rises on the whole of its use, so Newton's method from Priest's
         * value converges to the one root; colour-science interpolates a 0.001 grid. */
        ALWAN__T V = ALWAN__L(10.0) * ALWAN_CORE_SQRT(ALWAN_CORE_ABS(Y) / ALWAN__L(100.0));
        int i;
        for (i = 0; i < 40; i++) {
            ALWAN__T const d = ALWAN__L(1.1914) - ALWAN__L(2.0) * ALWAN__L(0.22533) * V +
                               ALWAN__L(3.0) * ALWAN__L(0.23352) * V * V - ALWAN__L(4.0) * ALWAN__L(0.020484) * V * V * V +
                               ALWAN__L(5.0) * ALWAN__L(0.00081939) * V * V * V * V;
            V = V - (ALWAN_CORE_FNLIT(alwan__lt_d1535)(V) - Y) / d;
        }
        *V_out = V;
        return ALWAN_OK;
    }
    case ALWAN_MUNSELL_VALUE_PRIEST1920:
        *V_out = ALWAN__L(10.0) * ALWAN_CORE_SQRT(Y / ALWAN__L(100.0));
        return ALWAN_OK;
    case ALWAN_MUNSELL_VALUE_MUNSELL1933:
        *V_out = ALWAN_CORE_SQRT(ALWAN__L(1.4742) * Y - ALWAN__L(0.004743) * (Y * Y));
        return ALWAN_OK;
    case ALWAN_MUNSELL_VALUE_MOON1943:
        *V_out = ALWAN__L(1.4) * ALWAN__SPOW(Y, ALWAN__L(0.426));
        return ALWAN_OK;
    case ALWAN_MUNSELL_VALUE_SAUNDERSON1944:
        *V_out = ALWAN__L(2.357) * ALWAN__SPOW(Y, ALWAN__L(0.343)) - ALWAN__L(1.52);
        return ALWAN_OK;
    case ALWAN_MUNSELL_VALUE_LADD1955:
        *V_out = ALWAN__L(2.468) * ALWAN__SPOW(Y, c13) - ALWAN__L(1.636);
        return ALWAN_OK;
    case ALWAN_MUNSELL_VALUE_MCCAMY1987: {
        ALWAN__T const q = ALWAN__L(0.1073) * Y - ALWAN__L(3.084);
        if (Y <= ALWAN__L(0.9)) {
            *V_out = ALWAN__L(0.87445) * ALWAN__SPOW(Y, ALWAN__L(0.9967));
        } else {
            *V_out = ALWAN__L(2.49268) * ALWAN__SPOW(Y, c13) - ALWAN__L(1.5614) - (ALWAN__L(0.985) / ((q * q) + ALWAN__L(7.54))) +
                     ALWAN__L(0.0133) / ALWAN__SPOW(Y, ALWAN__L(2.3)) +
                     ALWAN__L(0.0084) * ALWAN_CORE_SIN(ALWAN__L(4.1) * ALWAN__SPOW(Y, c13) + ALWAN__L(1.0)) +
                     (ALWAN__L(0.0221) / Y) * ALWAN_CORE_SIN(ALWAN__L(0.39) * (Y - ALWAN__L(2.0))) -
                     (ALWAN__L(0.0037) / (ALWAN__L(0.44) * Y)) * ALWAN_CORE_SIN(ALWAN__L(1.28) * (Y - ALWAN__L(0.53)));
        }
        return ALWAN_OK;
    }
    default:
        return ALWAN_E_INVALID;
    }
}

alwan_status ALWAN_CORE_FNLIT(alwan_whiteness_berger1959)(ALWAN__T *W_out, ALWAN_CORE_XYZ const *xyz,
                                                          ALWAN_CORE_XYZ const *xyz_0) {
    if (!W_out || !xyz || !xyz_0 || xyz_0->x == ALWAN__L(0.0) || xyz_0->z == ALWAN__L(0.0)) return ALWAN_E_INVALID;
    *W_out = ALWAN__L(0.333) * xyz->y + ALWAN__L(125.0) * (xyz->z / xyz_0->z) - ALWAN__L(125.0) * (xyz->x / xyz_0->x);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_whiteness_taube1960)(ALWAN__T *W_out, ALWAN_CORE_XYZ const *xyz,
                                                         ALWAN_CORE_XYZ const *xyz_0) {
    if (!W_out || !xyz || !xyz_0 || xyz_0->z == ALWAN__L(0.0)) return ALWAN_E_INVALID;
    *W_out = ALWAN__L(400.0) * (xyz->z / xyz_0->z) - ALWAN__L(3.0) * xyz->y;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_whiteness_stensby1968)(ALWAN__T *W_out, ALWAN_CORE_LAB const *lab) {
    if (!W_out || !lab) return ALWAN_E_INVALID;
    *W_out = lab->L - ALWAN__L(3.0) * lab->b + ALWAN__L(3.0) * lab->a;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_whiteness_ganz1979)(ALWAN__T *W_out, ALWAN__T *T_out, ALWAN_CORE_VEC2 const *xy,
                                                        ALWAN__T Y) {
    if (!W_out || !T_out || !xy) return ALWAN_E_INVALID;
    *W_out = Y - ALWAN__L(1868.322) * xy->v[0] - ALWAN__L(3695.690) * xy->v[1] + ALWAN__L(1809.441);
    *T_out = ALWAN__L(-1001.223) * xy->v[0] + ALWAN__L(748.366) * xy->v[1] + ALWAN__L(68.261);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_yellowness_astm_d1925)(ALWAN__T *YI_out, ALWAN_CORE_XYZ const *xyz) {
    if (!YI_out || !xyz || xyz->y == ALWAN__L(0.0)) return ALWAN_E_INVALID;
    *YI_out = (ALWAN__L(100.0) * (ALWAN__L(1.28) * xyz->x - ALWAN__L(1.06) * xyz->z)) / xyz->y;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_yellowness_astm_e313_alternative)(ALWAN__T *YI_out, ALWAN_CORE_XYZ const *xyz) {
    if (!YI_out || !xyz || xyz->y == ALWAN__L(0.0)) return ALWAN_E_INVALID;
    *YI_out = ALWAN__L(100.0) * (ALWAN__L(1.0) - (ALWAN__L(0.847) * xyz->z) / xyz->y);
    return ALWAN_OK;
}

#undef ALWAN__SPOW
#undef ALWAN__L
#undef ALWAN__T
#undef ALWAN__LP
