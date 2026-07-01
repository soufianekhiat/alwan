/* No include guard -- included once per precision (f32/f64) from
 * alwan_agx_jp2499_core.h.
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
 * STAGE 1: the ratio-preserving tonescale core (linear Rec.709 -> display-
 * linear Rec.709). Neutral and per-channel behaviour are exact; the inset/
 * outset hue-geometry (hue_flight / chroma_attenuation / purity) is identity
 * here and wired in stage 2. The caller applies the display OETF.
 */

/* ---- Per-call setup (native precision): tonescale params + hue geometry ----
 * Computed once per apply()/map call in the working precision (no hardcoded
 * f64), then handed to the per-pixel render below. */

/* Michaelis-Menten tonescale params from peak luminance (per Jp-DRT.dctl).
 * log(x)/log(base) == log2(x)/log2(base), so the natural logs become log2
 * (ALWAN_CORE has no natural LOG). */
ALWAN_INLINE void ALWAN_CORE_FN(jp2499_tonescale_params)(ALWAN_CORE_T Lp,
        ALWAN_CORE_T *m, ALWAN_CORE_T *s, ALWAN_CORE_T *c, ALWAN_CORE_T *fl) {
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
    *m = M; *s = S; *c = C; *fl = FL;
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

/* Rotated/scaled "moment" chromaticities around white (1/3, 1/3).
 * prim is flat: [rx,ry, gx,gy, bx,by]. */
ALWAN_INLINE void ALWAN_CORE_FN(jp2499_inset_chr)(ALWAN_CORE_T const rot[3], ALWAN_CORE_T const scale[3],
                                                  ALWAN_CORE_T prim[6], ALWAN_CORE_T white[2]) {
    ALWAN_CORE_T const pi = ALWAN_CORE_LITERAL(3.14159265358979323846);
    ALWAN_CORE_T const third = ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0);
    ALWAN_CORE_T const off[3] = {ALWAN_CORE_ZERO,
                                 ALWAN_CORE_LITERAL(2.0) * pi / ALWAN_CORE_LITERAL(3.0),
                                 ALWAN_CORE_LITERAL(4.0) * pi / ALWAN_CORE_LITERAL(3.0)};
    for (int i = 0; i < 3; i++) {
        ALWAN_CORE_T a = rot[i] + off[i];
        ALWAN_CORE_T tv = ALWAN_CORE_FN(jp2499_triangle)(a) * (ALWAN_CORE_ONE - scale[i]);
        prim[i*2+0] = ALWAN_CORE_COS(a) * tv + third;
        prim[i*2+1] = ALWAN_CORE_SIN(a) * tv + third;
    }
    white[0] = third; white[1] = third;
}

ALWAN_INLINE void ALWAN_CORE_FN(jp2499_invert3)(ALWAN_CORE_T const m[9], ALWAN_CORE_T out[9]) {
    ALWAN_CORE_T A = m[4]*m[8]-m[5]*m[7], B = -(m[3]*m[8]-m[5]*m[6]), C = m[3]*m[7]-m[4]*m[6];
    ALWAN_CORE_T det = m[0]*A + m[1]*B + m[2]*C;
    ALWAN_CORE_T id = det != ALWAN_CORE_ZERO ? ALWAN_CORE_ONE / det : ALWAN_CORE_ZERO;
    out[0]=A*id; out[1]=(m[2]*m[7]-m[1]*m[8])*id; out[2]=(m[1]*m[5]-m[2]*m[4])*id;
    out[3]=B*id; out[4]=(m[0]*m[8]-m[2]*m[6])*id; out[5]=(m[2]*m[3]-m[0]*m[5])*id;
    out[6]=C*id; out[7]=(m[1]*m[6]-m[0]*m[7])*id; out[8]=(m[0]*m[4]-m[1]*m[3])*id;
}

ALWAN_INLINE void ALWAN_CORE_FN(jp2499_matmul3)(ALWAN_CORE_T const a[9], ALWAN_CORE_T const b[9], ALWAN_CORE_T out[9]) {
    for (int r = 0; r < 3; r++)
        for (int cc = 0; cc < 3; cc++)
            out[r*3+cc] = a[r*3+0]*b[0*3+cc] + a[r*3+1]*b[1*3+cc] + a[r*3+2]*b[2*3+cc];
}

/* Normalized primary matrix (RGB -> XYZ) from chromaticities + white. */
ALWAN_INLINE void ALWAN_CORE_FN(jp2499_npm)(ALWAN_CORE_T const prim[6], ALWAN_CORE_T const white[2], ALWAN_CORE_T npm[9]) {
    ALWAN_CORE_T M[9];
    for (int cc = 0; cc < 3; cc++) {
        ALWAN_CORE_T x = prim[cc*2+0], y = prim[cc*2+1];
        M[0*3+cc] = x / y; M[1*3+cc] = ALWAN_CORE_ONE; M[2*3+cc] = (ALWAN_CORE_ONE - x - y) / y;
    }
    ALWAN_CORE_T W[3] = {white[0]/white[1], ALWAN_CORE_ONE, (ALWAN_CORE_ONE - white[0] - white[1]) / white[1]};
    ALWAN_CORE_T Minv[9]; ALWAN_CORE_FN(jp2499_invert3)(M, Minv);
    ALWAN_CORE_T S[3];
    for (int r = 0; r < 3; r++) S[r] = Minv[r*3+0]*W[0] + Minv[r*3+1]*W[1] + Minv[r*3+2]*W[2];
    for (int r = 0; r < 3; r++) for (int cc = 0; cc < 3; cc++) npm[r*3+cc] = M[r*3+cc] * S[cc];
}

/* rgb_to_rgb(src, dst) = inv(npm(dst)) @ npm(src). Flat prim arrays. */
ALWAN_INLINE void ALWAN_CORE_FN(jp2499_rgb_to_rgb)(ALWAN_CORE_T const pa[6], ALWAN_CORE_T const wa[2],
                                                   ALWAN_CORE_T const pb[6], ALWAN_CORE_T const wb[2], ALWAN_CORE_T out[9]) {
    ALWAN_CORE_T na[9], nb[9], nbi[9];
    ALWAN_CORE_FN(jp2499_npm)(pa, wa, na);
    ALWAN_CORE_FN(jp2499_npm)(pb, wb, nb);
    ALWAN_CORE_FN(jp2499_invert3)(nb, nbi);
    ALWAN_CORE_FN(jp2499_matmul3)(nbi, na, out);
}

/* inset = rgb_to_rgb(hue+attenuation, unity); outset_inv = inv(rgb_to_rgb(hue+purity, unity)). */
ALWAN_INLINE void ALWAN_CORE_FN(jp2499_geometry)(ALWAN_CORE_T const hue_flight[3],
        ALWAN_CORE_T const chroma_attenuation[3], ALWAN_CORE_T const purity[3],
        ALWAN_CORE_T inset[9], ALWAN_CORE_T outset_inv[9]) {
    ALWAN_CORE_T const zero[3] = {ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ALWAN_CORE_ZERO};
    ALWAN_CORE_T ur[6], uw[2], pr[6], pw[2], outset[9];
    ALWAN_CORE_FN(jp2499_inset_chr)(zero, zero, ur, uw);                 /* unity */
    ALWAN_CORE_FN(jp2499_inset_chr)(hue_flight, chroma_attenuation, pr, pw);
    ALWAN_CORE_FN(jp2499_rgb_to_rgb)(pr, pw, ur, uw, inset);
    ALWAN_CORE_FN(jp2499_inset_chr)(hue_flight, purity, pr, pw);
    ALWAN_CORE_FN(jp2499_rgb_to_rgb)(pr, pw, ur, uw, outset);
    ALWAN_CORE_FN(jp2499_invert3)(outset, outset_inv);
}

/* Safe divide: 0 when the denominator is 0 (matches Jp-DRT sdivf). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FN(jp2499_sdiv)(ALWAN_CORE_T a, ALWAN_CORE_T b) {
    return b == ALWAN_CORE_ZERO ? ALWAN_CORE_ZERO : a / b;
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
ALWAN_INLINE void ALWAN_CORE_FN(jp2499_ratio_toe)(ALWAN_CORE_T *r, ALWAN_CORE_T *g, ALWAN_CORE_T *b) {
    ALWAN_CORE_T const w0 = ALWAN_CORE_LITERAL(0.25), w1 = ALWAN_CORE_LITERAL(0.45), w2 = ALWAN_CORE_LITERAL(0.3);
    ALWAN_CORE_T lum = w0 * ALWAN_CORE_MAX(ALWAN_CORE_ZERO, *r)
                     + w1 * ALWAN_CORE_MAX(ALWAN_CORE_ZERO, *g)
                     + w2 * ALWAN_CORE_MAX(ALWAN_CORE_ZERO, *b);
    ALWAN_CORE_T const pp_p = ALWAN_CORE_LITERAL(0.05);
    ALWAN_CORE_T rr = ALWAN_CORE_FN(jp2499_powerptoe)(ALWAN_CORE_FN(jp2499_sdiv)(*r, lum), pp_p, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
    ALWAN_CORE_T rg = ALWAN_CORE_FN(jp2499_powerptoe)(ALWAN_CORE_FN(jp2499_sdiv)(*g, lum), pp_p, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
    ALWAN_CORE_T rb = ALWAN_CORE_FN(jp2499_powerptoe)(ALWAN_CORE_FN(jp2499_sdiv)(*b, lum), pp_p, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
    *r = rr * lum; *g = rg * lum; *b = rb * lum;
}

/* Full JP2499 render for one pixel (linear Rec.709 -> display-linear Rec.709).
 * Tonescale params m/s/c/fl and display scale ds, plus the inset and inverse-
 * outset 3x3 matrices (hue geometry), are precomputed per call. */
ALWAN_INLINE void ALWAN_CORE_FN(jp2499_render)(ALWAN_CORE_T const *in, ALWAN_CORE_T *out,
        ALWAN_CORE_T m, ALWAN_CORE_T s, ALWAN_CORE_T c, ALWAN_CORE_T fl, ALWAN_CORE_T ds,
        ALWAN_CORE_T const *inset, ALWAN_CORE_T const *outset_inv,
        ALWAN_CORE_T const *white_tip, ALWAN_CORE_T const *black_tip) {
    ALWAN_CORE_T r = in[0], g = in[1], b = in[2];
    ALWAN_CORE_T mx = ALWAN_CORE_MAX(r, ALWAN_CORE_MAX(g, b));

    /* Ratio pass 1. */
    ALWAN_CORE_FN(jp2499_ratio_toe)(&r, &g, &b);

    /* Inset (hue-flight / chroma-attenuation). */
    {
        ALWAN_CORE_T ir = inset[0]*r + inset[1]*g + inset[2]*b;
        ALWAN_CORE_T ig = inset[3]*r + inset[4]*g + inset[5]*b;
        ALWAN_CORE_T ib = inset[6]*r + inset[7]*g + inset[8]*b;
        r = ir; g = ig; b = ib;
    }

    /* Per-channel tonescale + flare, and the max-channel tonescale for recombine. */
    r = ALWAN_CORE_FN(jp2499_flare)(ALWAN_CORE_FN(jp2499_tonescale)(r, m, s, c), fl);
    g = ALWAN_CORE_FN(jp2499_flare)(ALWAN_CORE_FN(jp2499_tonescale)(g, m, s, c), fl);
    b = ALWAN_CORE_FN(jp2499_flare)(ALWAN_CORE_FN(jp2499_tonescale)(b, m, s, c), fl);
    ALWAN_CORE_T ts = ALWAN_CORE_FN(jp2499_flare)(ALWAN_CORE_FN(jp2499_tonescale)(mx, m, s, c), fl);

    /* Outset (purity restore), applied as the inverse matrix. */
    {
        ALWAN_CORE_T or_ = outset_inv[0]*r + outset_inv[1]*g + outset_inv[2]*b;
        ALWAN_CORE_T og  = outset_inv[3]*r + outset_inv[4]*g + outset_inv[5]*b;
        ALWAN_CORE_T ob  = outset_inv[6]*r + outset_inv[7]*g + outset_inv[8]*b;
        r = or_; g = og; b = ob;
    }

    /* Ratio pass 2, then recombine ratios with the tonescaled max. */
    ALWAN_CORE_T omx = ALWAN_CORE_MAX(r, ALWAN_CORE_MAX(g, b));
    ALWAN_CORE_FN(jp2499_ratio_toe)(&r, &g, &b);
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
        r += black_tip[0] * shadow + white_tip[0] * lum;
        g += black_tip[1] * shadow + white_tip[1] * lum;
        b += black_tip[2] * shadow + white_tip[2] * lum;
    }

    out[0] = r * ds; out[1] = g * ds; out[2] = b * ds;
}
