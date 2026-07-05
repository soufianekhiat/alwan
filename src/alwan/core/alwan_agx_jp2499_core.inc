/* No include guard -- included once per precision (f32/f64) from
 * alwan_agx_jp2499_core.h (and once, single precision, for the GPU backends).
 *
 * JP2499 picture formation (Juan Pablo Zambrano's "2499" DRT) -- per-pixel
 * core. A purely analytical display-rendering transform: a Michaelis-Menten
 * (Siragusano) ratio-preserving tonescale with per-primary hue-flight /
 * chroma-attenuation / purity controls. Source: jedypod/JP2499 (Jp-DRT.dctl).
 *
 * Header-only (ALWAN_INLINE) so both the scalar apply (alwan_agx_jp2499.c) and
 * the tiled map kernels (alwan_agx_jp2499_map_kernels.inc) share one worker;
 * that also makes the map path byte-exact with the scalar path in fast and
 * deterministic builds.
 *
 * Pointer-free by design: every function takes and returns values
 * (ALWAN_CORE_VEC3 / ALWAN_CORE_MAT3X3 / small structs), so the whole file is
 * legal on the GPU backends (HLSL/GLSL/Halide have no raw pointers). The C
 * codegen is equivalent to the old pointer form, so scalar-apply == tiled-map
 * stays byte-exact.
 */

/* ---- Per-call setup types (produced once per apply()/map call) ---- */

/* Michaelis-Menten tonescale parameters. */
typedef struct {
    ALWAN_CORE_T m, s, c, fl;
} ALWAN_CORE_FNLIT(jp2499_ts);

/* Rotated/scaled "moment" chromaticities: 3 primaries (xy pairs) + white. */
typedef struct {
    ALWAN_CORE_T prim[6];   /* rx,ry, gx,gy, bx,by */
    ALWAN_CORE_T white[2];
} ALWAN_CORE_FNLIT(jp2499_chr);

/* Hue geometry: the inset and inverse-outset matrices applied in the render. */
typedef struct {
    ALWAN_CORE_MAT3X3 inset;
    ALWAN_CORE_MAT3X3 outset_inv;
} ALWAN_CORE_FNLIT(jp2499_geom);

/* Michaelis-Menten tonescale params from peak luminance (per Jp-DRT.dctl).
 * log(x)/log(base) == log2(x)/log2(base), so the natural logs become log2
 * (ALWAN_CORE has no natural LOG on every backend). */
ALWAN_INLINE ALWAN_CORE_FNLIT(jp2499_ts) ALWAN_CORE_FN(jp2499_tonescale_params)(ALWAN_CORE_T Lp) {
    ALWAN_CORE_T const C = ALWAN_CORE_LITERAL(1.15), FL = ALWAN_CORE_LITERAL(0.041), gb = ALWAN_CORE_LITERAL(0.12);
    ALWAN_CORE_T px = ALWAN_CORE_LITERAL(256.0) * ALWAN_CORE_LOG2(Lp) / ALWAN_CORE_LOG2(ALWAN_CORE_LITERAL(100.0)) - ALWAN_CORE_LITERAL(128.0);
    ALWAN_CORE_T py = Lp / ALWAN_CORE_LITERAL(100.0);
    ALWAN_CORE_T gx = ALWAN_CORE_LITERAL(0.18);
    ALWAN_CORE_T gy = ALWAN_CORE_LITERAL(0.1) * (ALWAN_CORE_ONE + gb * ALWAN_CORE_LOG2(py));
    ALWAN_CORE_T s0 = (gy + ALWAN_CORE_SQRT(gy * (ALWAN_CORE_LITERAL(4.0) * FL + gy))) / ALWAN_CORE_LITERAL(2.0);
    ALWAN_CORE_T m0 = (py + ALWAN_CORE_SQRT(py * (ALWAN_CORE_LITERAL(4.0) * FL + py))) / ALWAN_CORE_LITERAL(2.0);
    ALWAN_CORE_T ip = ALWAN_CORE_ONE / C;
    ALWAN_CORE_T S = (px * gx * (ALWAN_CORE_POW(m0, ip) - ALWAN_CORE_POW(s0, ip)))
                   / (px * ALWAN_CORE_POW(s0, ip) - gx * ALWAN_CORE_POW(m0, ip));
    ALWAN_CORE_T M = ALWAN_CORE_POW(m0, ip) * (S + px) / px;
    ALWAN_CORE_FNLIT(jp2499_ts) ts;
    ts.m = M; ts.s = S; ts.c = C; ts.fl = FL;
    return ts;
}

/* Hexagonal boundary. asin(x) == pi/2 - acos(x) (ALWAN_CORE has no ASIN). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FN(jp2499_triangle)(ALWAN_CORE_T t) {
    ALWAN_CORE_T const pi = ALWAN_CORE_LITERAL(3.14159265358979323846);
    ALWAN_CORE_T const halfpi = ALWAN_CORE_LITERAL(1.57079632679489661923);
    ALWAN_CORE_T asin_c = halfpi - ALWAN_CORE_ACOS(ALWAN_CORE_COS(ALWAN_CORE_LITERAL(3.0) * t));
    return ALWAN_CORE_COS(pi / ALWAN_CORE_LITERAL(3.0))
         / ALWAN_CORE_COS((ALWAN_CORE_LITERAL(2.0) * asin_c + pi) / ALWAN_CORE_LITERAL(6.0))
         * ALWAN_CORE_LITERAL(0.3);
}

/* Rotated/scaled "moment" chromaticities around white (1/3, 1/3). */
ALWAN_INLINE ALWAN_CORE_FNLIT(jp2499_chr) ALWAN_CORE_FN(jp2499_inset_chr)(
        ALWAN_CORE_VEC3 rot, ALWAN_CORE_VEC3 scale) {
    ALWAN_CORE_T const pi = ALWAN_CORE_LITERAL(3.14159265358979323846);
    ALWAN_CORE_T const third = ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0);
    ALWAN_CORE_FNLIT(jp2499_chr) out_chr;
    for (int i = 0; i < 3; i++) {
        ALWAN_CORE_T off = (ALWAN_CORE_T)i * ALWAN_CORE_LITERAL(2.0) * pi / ALWAN_CORE_LITERAL(3.0);
        ALWAN_CORE_T a = rot.v[i] + off;
        ALWAN_CORE_T tv = ALWAN_CORE_FN(jp2499_triangle)(a) * (ALWAN_CORE_ONE - scale.v[i]);
        out_chr.prim[i*2+0] = ALWAN_CORE_COS(a) * tv + third;
        out_chr.prim[i*2+1] = ALWAN_CORE_SIN(a) * tv + third;
    }
    out_chr.white[0] = third; out_chr.white[1] = third;
    return out_chr;
}

ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FN(jp2499_invert3)(ALWAN_CORE_MAT3X3 a) {
    ALWAN_CORE_T A = a.m[4]*a.m[8]-a.m[5]*a.m[7], B = -(a.m[3]*a.m[8]-a.m[5]*a.m[6]), C = a.m[3]*a.m[7]-a.m[4]*a.m[6];
    ALWAN_CORE_T det = a.m[0]*A + a.m[1]*B + a.m[2]*C;
    ALWAN_CORE_T id = ALWAN_CORE_SELECT(det != ALWAN_CORE_ZERO, ALWAN_CORE_ONE / det, ALWAN_CORE_ZERO);
    ALWAN_CORE_MAT3X3 o;
    o.m[0]=A*id; o.m[1]=(a.m[2]*a.m[7]-a.m[1]*a.m[8])*id; o.m[2]=(a.m[1]*a.m[5]-a.m[2]*a.m[4])*id;
    o.m[3]=B*id; o.m[4]=(a.m[0]*a.m[8]-a.m[2]*a.m[6])*id; o.m[5]=(a.m[2]*a.m[3]-a.m[0]*a.m[5])*id;
    o.m[6]=C*id; o.m[7]=(a.m[1]*a.m[6]-a.m[0]*a.m[7])*id; o.m[8]=(a.m[0]*a.m[4]-a.m[1]*a.m[3])*id;
    return o;
}

ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FN(jp2499_matmul3)(ALWAN_CORE_MAT3X3 a, ALWAN_CORE_MAT3X3 b) {
    ALWAN_CORE_MAT3X3 o;
    for (int r = 0; r < 3; r++)
        for (int cc = 0; cc < 3; cc++)
            o.m[r*3+cc] = a.m[r*3+0]*b.m[0*3+cc] + a.m[r*3+1]*b.m[1*3+cc] + a.m[r*3+2]*b.m[2*3+cc];
    return o;
}

/* Normalized primary matrix (RGB -> XYZ) from chromaticities + white. */
ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FN(jp2499_npm)(ALWAN_CORE_FNLIT(jp2499_chr) c) {
    ALWAN_CORE_MAT3X3 M;
    for (int cc = 0; cc < 3; cc++) {
        ALWAN_CORE_T x = c.prim[cc*2+0], y = c.prim[cc*2+1];
        M.m[0*3+cc] = x / y; M.m[1*3+cc] = ALWAN_CORE_ONE; M.m[2*3+cc] = (ALWAN_CORE_ONE - x - y) / y;
    }
    ALWAN_CORE_T W0 = c.white[0]/c.white[1];
    ALWAN_CORE_T W1 = ALWAN_CORE_ONE;
    ALWAN_CORE_T W2 = (ALWAN_CORE_ONE - c.white[0] - c.white[1]) / c.white[1];
    ALWAN_CORE_MAT3X3 Minv = ALWAN_CORE_FN(jp2499_invert3)(M);
    ALWAN_CORE_T S0 = Minv.m[0]*W0 + Minv.m[1]*W1 + Minv.m[2]*W2;
    ALWAN_CORE_T S1 = Minv.m[3]*W0 + Minv.m[4]*W1 + Minv.m[5]*W2;
    ALWAN_CORE_T S2 = Minv.m[6]*W0 + Minv.m[7]*W1 + Minv.m[8]*W2;
    ALWAN_CORE_MAT3X3 npm;
    for (int r = 0; r < 3; r++) {
        npm.m[r*3+0] = M.m[r*3+0] * S0;
        npm.m[r*3+1] = M.m[r*3+1] * S1;
        npm.m[r*3+2] = M.m[r*3+2] * S2;
    }
    return npm;
}

/* rgb_to_rgb(src, dst) = inv(npm(dst)) @ npm(src). */
ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FN(jp2499_rgb_to_rgb)(
        ALWAN_CORE_FNLIT(jp2499_chr) a, ALWAN_CORE_FNLIT(jp2499_chr) b) {
    ALWAN_CORE_MAT3X3 na = ALWAN_CORE_FN(jp2499_npm)(a);
    ALWAN_CORE_MAT3X3 nb = ALWAN_CORE_FN(jp2499_npm)(b);
    ALWAN_CORE_MAT3X3 nbi = ALWAN_CORE_FN(jp2499_invert3)(nb);
    return ALWAN_CORE_FN(jp2499_matmul3)(nbi, na);
}

/* inset = rgb_to_rgb(hue+attenuation, unity); outset_inv = inv(rgb_to_rgb(hue+purity, unity)). */
ALWAN_INLINE ALWAN_CORE_FNLIT(jp2499_geom) ALWAN_CORE_FN(jp2499_geometry)(
        ALWAN_CORE_VEC3 hue_flight, ALWAN_CORE_VEC3 chroma_attenuation, ALWAN_CORE_VEC3 purity) {
    ALWAN_CORE_VEC3 zero;
    zero.v[0] = ALWAN_CORE_ZERO; zero.v[1] = ALWAN_CORE_ZERO; zero.v[2] = ALWAN_CORE_ZERO;
    ALWAN_CORE_FNLIT(jp2499_chr) unity = ALWAN_CORE_FN(jp2499_inset_chr)(zero, zero);
    ALWAN_CORE_FNLIT(jp2499_geom) g;
    g.inset = ALWAN_CORE_FN(jp2499_rgb_to_rgb)(
        ALWAN_CORE_FN(jp2499_inset_chr)(hue_flight, chroma_attenuation), unity);
    g.outset_inv = ALWAN_CORE_FN(jp2499_invert3)(
        ALWAN_CORE_FN(jp2499_rgb_to_rgb)(
            ALWAN_CORE_FN(jp2499_inset_chr)(hue_flight, purity), unity));
    return g;
}

/* Safe divide: 0 when the denominator is 0 (matches Jp-DRT sdivf). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FN(jp2499_sdiv)(ALWAN_CORE_T a, ALWAN_CORE_T b) {
    return ALWAN_CORE_SELECT(b == ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, a / b);
}

/* Michaelis-Menten constrained tonescale (forward): (m*x/(x+s))^c. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FN(jp2499_tonescale)(ALWAN_CORE_T x,
        ALWAN_CORE_T m, ALWAN_CORE_T s, ALWAN_CORE_T c) {
    if (x <= ALWAN_CORE_ZERO) return x;
    return ALWAN_CORE_POW(m * x / (x + s), c);
}

/* Parabolic toe (forward): x^2/(x+fl). Compresses near zero without clipping. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FN(jp2499_flare)(ALWAN_CORE_T x, ALWAN_CORE_T fl) {
    if (x <= ALWAN_CORE_ZERO) return x;
    return x * x / (x + fl);
}

/* Power toe on a channel ratio: leaves x>=t0 unchanged, compresses below. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FN(jp2499_powerptoe)(ALWAN_CORE_T x,
        ALWAN_CORE_T p, ALWAN_CORE_T mm, ALWAN_CORE_T t0) {
    if (x > t0) return x;
    return (x - t0) * ALWAN_CORE_POW(
               ALWAN_CORE_POW((t0 - x) / (t0 - mm), ALWAN_CORE_ONE / p) + ALWAN_CORE_ONE,
               -p) + t0;
}

/* One ratio pass: normalise by the weighted norm, toe the ratios, recombine. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FN(jp2499_ratio_toe)(ALWAN_CORE_VEC3 rgb) {
    ALWAN_CORE_T const w0 = ALWAN_CORE_LITERAL(0.25), w1 = ALWAN_CORE_LITERAL(0.45), w2 = ALWAN_CORE_LITERAL(0.3);
    ALWAN_CORE_T lum = w0 * ALWAN_CORE_MAX(ALWAN_CORE_ZERO, rgb.v[0])
                     + w1 * ALWAN_CORE_MAX(ALWAN_CORE_ZERO, rgb.v[1])
                     + w2 * ALWAN_CORE_MAX(ALWAN_CORE_ZERO, rgb.v[2]);
    ALWAN_CORE_T const pp_p = ALWAN_CORE_LITERAL(0.05);
    ALWAN_CORE_T rr = ALWAN_CORE_FN(jp2499_powerptoe)(ALWAN_CORE_FN(jp2499_sdiv)(rgb.v[0], lum), pp_p, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
    ALWAN_CORE_T rg = ALWAN_CORE_FN(jp2499_powerptoe)(ALWAN_CORE_FN(jp2499_sdiv)(rgb.v[1], lum), pp_p, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
    ALWAN_CORE_T rb = ALWAN_CORE_FN(jp2499_powerptoe)(ALWAN_CORE_FN(jp2499_sdiv)(rgb.v[2], lum), pp_p, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
    ALWAN_CORE_VEC3 o;
    o.v[0] = rr * lum; o.v[1] = rg * lum; o.v[2] = rb * lum;
    return o;
}

/* Full JP2499 render for one pixel (linear Rec.709 -> display-linear Rec.709).
 * Tonescale params m/s/c/fl and display scale ds, plus the inset and inverse-
 * outset matrices (hue geometry), are precomputed per call. Param is named
 * 'rgb' and the result 'result' -- 'in'/'out' are HLSL keywords. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FN(jp2499_render)(ALWAN_CORE_VEC3 rgb,
        ALWAN_CORE_T m, ALWAN_CORE_T s, ALWAN_CORE_T c, ALWAN_CORE_T fl, ALWAN_CORE_T ds,
        ALWAN_CORE_MAT3X3 inset, ALWAN_CORE_MAT3X3 outset_inv,
        ALWAN_CORE_VEC3 white_tip, ALWAN_CORE_VEC3 black_tip) {
    ALWAN_CORE_T r = rgb.v[0], g = rgb.v[1], b = rgb.v[2];
    ALWAN_CORE_T mx = ALWAN_CORE_MAX(r, ALWAN_CORE_MAX(g, b));

    /* Ratio pass 1. */
    {
        ALWAN_CORE_VEC3 t; t.v[0] = r; t.v[1] = g; t.v[2] = b;
        t = ALWAN_CORE_FN(jp2499_ratio_toe)(t);
        r = t.v[0]; g = t.v[1]; b = t.v[2];
    }

    /* Inset (hue-flight / chroma-attenuation). */
    {
        ALWAN_CORE_T ir = inset.m[0]*r + inset.m[1]*g + inset.m[2]*b;
        ALWAN_CORE_T ig = inset.m[3]*r + inset.m[4]*g + inset.m[5]*b;
        ALWAN_CORE_T ib = inset.m[6]*r + inset.m[7]*g + inset.m[8]*b;
        r = ir; g = ig; b = ib;
    }

    /* Per-channel tonescale + flare, and the max-channel tonescale for recombine. */
    r = ALWAN_CORE_FN(jp2499_flare)(ALWAN_CORE_FN(jp2499_tonescale)(r, m, s, c), fl);
    g = ALWAN_CORE_FN(jp2499_flare)(ALWAN_CORE_FN(jp2499_tonescale)(g, m, s, c), fl);
    b = ALWAN_CORE_FN(jp2499_flare)(ALWAN_CORE_FN(jp2499_tonescale)(b, m, s, c), fl);
    ALWAN_CORE_T ts = ALWAN_CORE_FN(jp2499_flare)(ALWAN_CORE_FN(jp2499_tonescale)(mx, m, s, c), fl);

    /* Outset (purity restore), applied as the inverse matrix. */
    {
        ALWAN_CORE_T or_ = outset_inv.m[0]*r + outset_inv.m[1]*g + outset_inv.m[2]*b;
        ALWAN_CORE_T og  = outset_inv.m[3]*r + outset_inv.m[4]*g + outset_inv.m[5]*b;
        ALWAN_CORE_T ob  = outset_inv.m[6]*r + outset_inv.m[7]*g + outset_inv.m[8]*b;
        r = or_; g = og; b = ob;
    }

    /* Ratio pass 2, then recombine ratios with the tonescaled max. */
    ALWAN_CORE_T omx = ALWAN_CORE_MAX(r, ALWAN_CORE_MAX(g, b));
    {
        ALWAN_CORE_VEC3 t; t.v[0] = r; t.v[1] = g; t.v[2] = b;
        t = ALWAN_CORE_FN(jp2499_ratio_toe)(t);
        r = t.v[0]; g = t.v[1]; b = t.v[2];
    }
    r = ALWAN_CORE_FN(jp2499_sdiv)(r * ts, omx);
    g = ALWAN_CORE_FN(jp2499_sdiv)(g * ts, omx);
    b = ALWAN_CORE_FN(jp2499_sdiv)(b * ts, omx);

    /* Cube-tip split-tone: tint toward black_tip in shadows, white_tip in
     * highlights (weighted by the tonescaled luminance). Zero tips = no-op. */
    {
        ALWAN_CORE_T lum = ALWAN_CORE_LITERAL(0.25) * r
                         + ALWAN_CORE_LITERAL(0.45) * g
                         + ALWAN_CORE_LITERAL(0.30) * b;
        ALWAN_CORE_T shadow = ALWAN_CORE_ONE - lum;
        r += black_tip.v[0] * shadow + white_tip.v[0] * lum;
        g += black_tip.v[1] * shadow + white_tip.v[1] * lum;
        b += black_tip.v[2] * shadow + white_tip.v[2] * lum;
    }

    ALWAN_CORE_VEC3 result;
    result.v[0] = r * ds; result.v[1] = g * ds; result.v[2] = b * ds;
    return result;
}
