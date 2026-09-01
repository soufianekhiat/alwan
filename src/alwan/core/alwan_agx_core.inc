/* No include guard -- included once per precision (f32/f64) from alwan_agx_core.h.
 *
 * Analytic AgX picture formation -- a fully parameterized, geometric AgX engine
 * (Troy Sobotka's design: everything geometric except the single 1D sigmoid).
 * Per-pixel pipeline:
 *   clamp -> inset matrix -> log2 guard-rail encode -> Jed Smith tunable sigmoid
 *   (the only non-geometric stage) -> outset matrix (complementary-energy
 *   restore) -> cube-tip split-tone -> sRGB EOTF (display-linear out).
 *
 * The sigmoid is Jed Smith's calculate_sigmoid (AgXLib). With the SB2383
 * parameters it reproduces alwan's baked SB2383 contrast LUT to machine
 * precision -- that LUT was baked from this very curve. Header-only
 * (ALWAN_INLINE) so the scalar apply (api/alwan_agx.c) and the tiled map kernels
 * (map/alwan_agx_map_kernels.inc) share one worker, making the map path
 * byte-exact with the scalar path in both fast and deterministic builds.
 */

/* Jed Smith per-side scale (AgXLib equation_scale). Derived once per call from
 * pivot/slope/power. All POW arguments are positive by construction. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FN(agx_eq_scale)(ALWAN_CORE_T xp, ALWAN_CORE_T yp,
        ALWAN_CORE_T slope, ALWAN_CORE_T power) {
    ALWAN_CORE_T a = ALWAN_CORE_POW(slope * xp, -power);
    ALWAN_CORE_T b = ALWAN_CORE_POW(slope * (xp / yp), power) - ALWAN_CORE_ONE;
    return ALWAN_CORE_POW(a * b, -ALWAN_CORE_ONE / power);
}

/* Cube-tip sigmoid endpoints (Troy Sobotka's split-tone technique). Instead of
 * every channel's sigmoid terminating at a rigid 0 and 1, the upper (white) and
 * lower (black) termination points tilt per channel, while the middle fulcrum
 * (pivot_output py) is pinned identically for all channels -- so the three
 * sigmoids cross cleanly at mid-grey and the split tone stays clean. Each tip:
 *   angle  = hue on the wheel (0=R, 2pi/3=G, 4pi/3=B)
 *   force  = chromatic tilt. Upper: lower the *complementary* channels toward the
 *            hue (white slews toward it). Lower: raise the *hue* channels (black
 *            tints toward it).
 *   offset = achromatic wash: upper lowers all three whites (matte/haze), lower
 *            lifts all three blacks (film-like faded shadows).
 * The per-channel upper target w and lower target k feed agx_eq_scale (the pivot
 * stays py), giving per-channel toe/shoulder scales. All params zero -> w=1, k=0
 * -> the plain rigid sigmoid. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FN(agx_hue_weight)(ALWAN_CORE_T angle, ALWAN_CORE_T phase) {
    /* cosine wheel: 1 at the hue channel, 0 at its complements. */
    return ALWAN_CORE_LITERAL(0.5) + ALWAN_CORE_LITERAL(0.5) * ALWAN_CORE_COS(angle - phase);
}

/* Cube tips + middle tint. Beyond the upper/lower tips, the fulcrum itself can be
 * tinted (Troy's "third set of controls"): a per-channel pivot py_c so the axis
 * reads lower-bias -> middle-bias -> upper-bias, while the three channels still
 * converge at a single (now tinted) crossover -- so a split tone stays clean.
 * Middle tint is balanced (hue_mid - 0.5, zero-sum) so it shifts chroma, not
 * brightness. Outputs per-channel pivot py_out[3] and toe/shoulder scales.
 *
 * Host-only: this is per-call setup that produces the shader uniforms, and it
 * writes through pointer out-params, so it is excluded from the GPU pass (HLSL/
 * GLSL/Halide have no raw pointers). The per-pixel agx_render below is the
 * GPU-portable kernel; it consumes the scales this produces. */
#if ALWAN_BACKEND == ALWAN_BACKEND_C
ALWAN_INLINE void ALWAN_CORE_FN(agx_tip_scales)(
        ALWAN_CORE_T px, ALWAN_CORE_T py, ALWAN_CORE_T slope,
        ALWAN_CORE_T toe_p, ALWAN_CORE_T shoulder_p,
        ALWAN_CORE_T up_angle, ALWAN_CORE_T up_force, ALWAN_CORE_T up_offset,
        ALWAN_CORE_T lo_angle, ALWAN_CORE_T lo_force, ALWAN_CORE_T lo_offset,
        ALWAN_CORE_T mid_angle, ALWAN_CORE_T mid_force,
        ALWAN_CORE_T *py_out, ALWAN_CORE_T *scale_toe, ALWAN_CORE_T *scale_shoulder) {
    const ALWAN_CORE_T phase[3] = { ALWAN_CORE_ZERO,
                                    ALWAN_CORE_LITERAL(2.09439510239319549231),   /* 2pi/3 */
                                    ALWAN_CORE_LITERAL(4.18879020478639098462) }; /* 4pi/3 */
    const ALWAN_CORE_T eps = ALWAN_CORE_LITERAL(1e-4);
    for (int c = 0; c < 3; c++) {
        ALWAN_CORE_T hue_up  = ALWAN_CORE_FN(agx_hue_weight)(up_angle,  phase[c]);
        ALWAN_CORE_T hue_lo  = ALWAN_CORE_FN(agx_hue_weight)(lo_angle,  phase[c]);
        ALWAN_CORE_T hue_mid = ALWAN_CORE_FN(agx_hue_weight)(mid_angle, phase[c]);
        /* middle tint: per-channel pivot (balanced chroma shift of the fulcrum). */
        ALWAN_CORE_T py_c = py + mid_force * (hue_mid - ALWAN_CORE_LITERAL(0.5));
        py_c = ALWAN_CORE_SELECT(py_c > eps, py_c, eps);
        py_c = ALWAN_CORE_SELECT(py_c < ALWAN_CORE_ONE - eps, py_c, ALWAN_CORE_ONE - eps);
        /* upper: 1, washed down by offset, complements (1 - hue) lowered by force. */
        ALWAN_CORE_T w = ALWAN_CORE_ONE - up_offset - up_force * (ALWAN_CORE_ONE - hue_up);
        /* lower: 0, lifted by offset, hue channels raised by force. */
        ALWAN_CORE_T k = lo_offset + lo_force * hue_lo;
        /* keep each endpoint on its side of the (per-channel) pivot: monotone +
         * clean crossover; eq_scale needs a positive pivot-to-bound distance. */
        w = ALWAN_CORE_SELECT(w > py_c + eps, w, py_c + eps);
        k = ALWAN_CORE_SELECT(k < py_c - eps, k, py_c - eps);
        py_out[c]         = py_c;
        scale_shoulder[c] = ALWAN_CORE_FN(agx_eq_scale)(ALWAN_CORE_ONE - px, w - py_c, slope, shoulder_p);
        scale_toe[c]      = -ALWAN_CORE_FN(agx_eq_scale)(px, py_c - k, slope, toe_p);
    }
}
#endif /* ALWAN_BACKEND_C -- agx_tip_scales is host-only setup */

/* Jed Smith tunable sigmoid: log-encoded x in [0,1] -> display-encoded [0,1].
 * X = slope*(x - px)/scale is >= 0 on both sides (scale_toe is negative and
 * x < px there), so the POW arguments stay valid without an abs. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FN(agx_sigmoid)(ALWAN_CORE_T x,
        ALWAN_CORE_T px, ALWAN_CORE_T py, ALWAN_CORE_T slope,
        ALWAN_CORE_T toe_p, ALWAN_CORE_T shoulder_p,
        ALWAN_CORE_T scale_toe, ALWAN_CORE_T scale_shoulder) {
    ALWAN_CORE_T scale = ALWAN_CORE_SELECT(x < px, scale_toe, scale_shoulder);
    ALWAN_CORE_T power = ALWAN_CORE_SELECT(x < px, toe_p, shoulder_p);
    ALWAN_CORE_T X = slope * (x - px) / scale;
    ALWAN_CORE_T h = X / ALWAN_CORE_POW(ALWAN_CORE_ONE + ALWAN_CORE_POW(X, power),
                                        ALWAN_CORE_ONE / power);
    return ALWAN_CORE_SATURATE(h * scale + py);
}

/* log2 guard-rail encode: linear -> normalized [0,1] over absolute log2 bounds
 * [lmin, lmax] (identical to alwan_agx_log_encode; inlined to keep this core
 * self-contained and independent of view-core include order). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FN(agx_log_encode)(ALWAN_CORE_T x,
        ALWAN_CORE_T lmin, ALWAN_CORE_T lmax) {
    ALWAN_CORE_T c = ALWAN_CORE_SELECT(x < ALWAN_CORE_LITERAL(1e-10),
                                       ALWAN_CORE_LITERAL(1e-10), x);
    return ALWAN_CORE_SATURATE((ALWAN_CORE_LOG2(c) - lmin) / (lmax - lmin));
}

/* Full analytic AgX render for one pixel (linear Rec.709 -> display-linear).
 * The log-encoded pivot px and the per-channel cube-tip sigmoid scales
 * (scale_toe[3]/scale_shoulder[3]) are precomputed per call. The split-tone lives
 * entirely in those per-channel endpoints -- there is no post-sigmoid tint pass,
 * so the fulcrum (py) stays neutral. */
/* Struct-by-value signature (no pointers) so this per-pixel kernel is GPU-legal
 * (HLSL/GLSL/Halide) as well as C. inset/outset are ALWAN_CORE_MAT3X3 (row-major
 * .m[9]); in/py/scale_* are ALWAN_CORE_VEC3 (.v[3]); result is returned by value.
 * The C codegen is identical to the old pointer form, so scalar-apply == tiled-
 * map stays byte-exact. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FN(agx_render)(ALWAN_CORE_VEC3 rgb,
        ALWAN_CORE_MAT3X3 inset, ALWAN_CORE_T lmin, ALWAN_CORE_T lmax,
        ALWAN_CORE_T px, ALWAN_CORE_VEC3 py, ALWAN_CORE_T slope,
        ALWAN_CORE_T toe_p, ALWAN_CORE_T shoulder_p,
        ALWAN_CORE_VEC3 scale_toe, ALWAN_CORE_VEC3 scale_shoulder,
        ALWAN_CORE_MAT3X3 outset) {
    /* Clamp negatives (matches the AgX RangeTransform before the inset).
     * NB: param is named 'rgb' and the result 'result' -- 'in'/'out' are HLSL
     * parameter-modifier keywords. */
    ALWAN_CORE_T r = ALWAN_CORE_MAX(rgb.v[0], ALWAN_CORE_ZERO);
    ALWAN_CORE_T g = ALWAN_CORE_MAX(rgb.v[1], ALWAN_CORE_ZERO);
    ALWAN_CORE_T b = ALWAN_CORE_MAX(rgb.v[2], ALWAN_CORE_ZERO);

    /* Inset (geometric attenuation toward the achromatic axis). */
    {
        ALWAN_CORE_T ir = inset.m[0]*r + inset.m[1]*g + inset.m[2]*b;
        ALWAN_CORE_T ig = inset.m[3]*r + inset.m[4]*g + inset.m[5]*b;
        ALWAN_CORE_T ib = inset.m[6]*r + inset.m[7]*g + inset.m[8]*b;
        r = ir; g = ig; b = ib;
    }

    /* log2 guard-rail encode. */
    r = ALWAN_CORE_FN(agx_log_encode)(r, lmin, lmax);
    g = ALWAN_CORE_FN(agx_log_encode)(g, lmin, lmax);
    b = ALWAN_CORE_FN(agx_log_encode)(b, lmin, lmax);

    /* Per-channel tunable sigmoid with per-channel cube-tip endpoints and
     * per-channel pivot py.v[3] (the only non-geometric stage). py is shared
     * across channels unless the middle tint is used, so the mid-grey crossover
     * stays a single clean point (neutral by default, or a chosen tint). */
    r = ALWAN_CORE_FN(agx_sigmoid)(r, px, py.v[0], slope, toe_p, shoulder_p, scale_toe.v[0], scale_shoulder.v[0]);
    g = ALWAN_CORE_FN(agx_sigmoid)(g, px, py.v[1], slope, toe_p, shoulder_p, scale_toe.v[1], scale_shoulder.v[1]);
    b = ALWAN_CORE_FN(agx_sigmoid)(b, px, py.v[2], slope, toe_p, shoulder_p, scale_toe.v[2], scale_shoulder.v[2]);

    /* Outset (geometric complementary-energy restore; identity = none). */
    {
        ALWAN_CORE_T or_ = outset.m[0]*r + outset.m[1]*g + outset.m[2]*b;
        ALWAN_CORE_T og  = outset.m[3]*r + outset.m[4]*g + outset.m[5]*b;
        ALWAN_CORE_T ob  = outset.m[6]*r + outset.m[7]*g + outset.m[8]*b;
        r = or_; g = og; b = ob;
    }

    /* sRGB EOTF -> display-linear (det-aware macro; matches the SIMD map path). */
    r = ALWAN_CORE_SATURATE(r); g = ALWAN_CORE_SATURATE(g); b = ALWAN_CORE_SATURATE(b);
    ALWAN_CORE_VEC3 result;
    result.v[0] = ALWAN_CORE_SRGB_EOTF(r);
    result.v[1] = ALWAN_CORE_SRGB_EOTF(g);
    result.v[2] = ALWAN_CORE_SRGB_EOTF(b);
    return result;
}
