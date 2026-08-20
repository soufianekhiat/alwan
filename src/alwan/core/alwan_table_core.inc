/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only table addressing gate and readers (parameterized)
 * No include guard - included twice via setup/teardown.
 */

/* ================================================================
 * WHY THIS FILE EXISTS
 *
 * alwan does not silently clamp COLOUR VALUES. That rule keeps a gamut
 * excursion visible instead of quietly flattened, and it is what the
 * raw/_gamut_safe twins encode. An ARRAY ADDRESS is not a colour: lut[-3] has
 * no unclamped meaning to preserve, and (int)NaN is undefined behaviour that
 * lands on INT_MIN under x86 and reads ~2 GB outside the table. Clamping an
 * index is not a colorimetric decision. It is the difference between a defined
 * result and a segfault.
 *
 * This file therefore clamps ADDRESSES and never colour values. Callers who
 * need to be told the coordinate was out of range, rather than served the edge
 * value, pass ALWAN_SAMPLE_STRICT to the API tier, which has an error channel.
 *
 * CONVENTION FOR KERNEL AUTHORS: hot loops call the mode-fixed readers
 * (_sample_linear, _sample_trilinear, _sample_tetrahedral, _sample_nearest)
 * directly. The dispatching _sample(..., mode) forms exist for callers that
 * select at runtime. A literal mode folds the dispatch away, but the
 * mode-fixed twin makes the fold guaranteed rather than left to the optimizer,
 * and this build sets no /GL and no /LTCG.
 * ================================================================ */

/* THE GATE. The one place a float coordinate becomes a table position.
 * Postcondition: the result is in [0, size-1] and is not NaN. Every reader
 * below depends on it, and no reader bounds-checks an index afterwards.
 *
 * Written as a ternary, not a max intrinsic. C guarantees NaN >= 0 is false on
 * every target, but the intrinsics disagree: x86 MAXSS returns its second
 * operand on NaN while ARMv8 FMAX (what vmaxq_f32 lowers to) PROPAGATES NaN,
 * so a max intrinsic would fix x86 and leave NEON crashing. ALWAN_CORE_SELECT
 * is already a plain ternary, which obliges FCSEL or FMAXNM on ARM and MAXSS
 * on x86. The library compiles /fp:precise in every configuration, so no
 * compiler may assume no-NaN and delete the guard.
 *
 * max first, then min: NaN resolves to the LOW edge, index 0. min first would
 * send it to the high edge, also in bounds but a different value, and scalar
 * and SIMD must agree byte for byte for the determinism contract. Index 0 also
 * matches the hand-written guard this replaces in the AgX view transform.
 *
 * The multiply stays AFTER the clamp, where the code it replaces put it, so
 * for a finite coordinate this is a pure operand-order swap and no rounding
 * can move. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_table_coord)(ALWAN_CORE_T coord, int size) {
#if ALWAN_READ_DATA_NO_BOUND_CHECK
    /* Caller asserts every coordinate is finite and in [0,1]. A NaN here is an
     * out-of-bounds read, not a wrong colour. See alwan_config.h. */
    return coord * (ALWAN_CORE_T)(size - 1);
#else
    ALWAN_CORE_T c = ALWAN_CORE_SELECT(coord >= ALWAN_CORE_ZERO, coord, ALWAN_CORE_ZERO);
    c = ALWAN_CORE_SELECT(c <= ALWAN_CORE_LITERAL(1.0), c, ALWAN_CORE_LITERAL(1.0));
    return c * (ALWAN_CORE_T)(size - 1);
#endif
}

/* Same gate for a coordinate already in TABLE UNITS (hue degrees, severity*10)
 * rather than normalized [0,1]. No rescale, same NaN-to-low-edge rule. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_table_coord_unit)(ALWAN_CORE_T pos, int size) {
#if ALWAN_READ_DATA_NO_BOUND_CHECK
    (void)size;
    return pos;
#else
    ALWAN_CORE_T const hi = (ALWAN_CORE_T)(size - 1);
    ALWAN_CORE_T c = ALWAN_CORE_SELECT(pos >= ALWAN_CORE_ZERO, pos, ALWAN_CORE_ZERO);
    return ALWAN_CORE_SELECT(c <= hi, c, hi);
#endif
}

/* Predicate backing ALWAN_SAMPLE_STRICT at the API tier. False for NaN,
 * because both compares fail, which is the whole point. */
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_table_coord_in_range)(ALWAN_CORE_T coord) {
    return (coord >= ALWAN_CORE_ZERO && coord <= ALWAN_CORE_LITERAL(1.0)) ? 1 : 0;
}

/* Resolve a normalized coordinate to a cell. Holding the i1 clamp and the frac
 * subtraction here means they exist once instead of once per reader. */
ALWAN_INLINE ALWAN_CORE_TABLE_CELL ALWAN_CORE_FNVLIT(alwan_table_cell)(ALWAN_CORE_T coord, int size) {
    ALWAN_CORE_T const p = ALWAN_CORE_FNVLIT(alwan_table_coord)(coord, size);
    ALWAN_CORE_TABLE_CELL r;
    r.i0 = (int)p;                                    /* gate proves [0, size-1] */
    r.i1 = (r.i0 + 1 < size) ? (r.i0 + 1) : (size - 1);
    r.frac = p - (ALWAN_CORE_T)r.i0;
    return r;
}

/* Cell from a coordinate already in table units. */
ALWAN_INLINE ALWAN_CORE_TABLE_CELL ALWAN_CORE_FNVLIT(alwan_table_cell_unit)(ALWAN_CORE_T pos, int size) {
    ALWAN_CORE_T const p = ALWAN_CORE_FNVLIT(alwan_table_coord_unit)(pos, size);
    ALWAN_CORE_TABLE_CELL r;
    r.i0 = (int)p;
    r.i1 = (r.i0 + 1 < size) ? (r.i0 + 1) : (size - 1);
    r.frac = p - (ALWAN_CORE_T)r.i0;
    return r;
}

/* ================================================================
 * Rank 1, scalar element
 * ================================================================ */

/* Weighted form, a*(1-f) + b*f. Matches alwan_lerp and the shipped
 * alwan_lut1d_sample bit for bit. This is ALWAN_SAMPLE_LINEAR. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_table1d_sample_linear)(
        ALWAN_CORE_T const *table, int size, ALWAN_CORE_T coord) {
    ALWAN_CORE_TABLE_CELL const c = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord, size);
    return table[c.i0] * (ALWAN_CORE_LITERAL(1.0) - c.frac) + table[c.i1] * c.frac;
}

/* Delta form, a + f*(b-a). Algebraically the same line, not the same bits: one
 * rounding lands differently. The AgX contrast LUT has always been read this
 * way and its output is pinned by the determinism MD5s, so the spelling is
 * preserved rather than silently re-rounded. Same gate, same bounds
 * guarantee -- only the blend differs. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_table1d_sample_linear_delta)(
        ALWAN_CORE_T const *table, int size, ALWAN_CORE_T coord) {
    ALWAN_CORE_TABLE_CELL const c = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord, size);
    return table[c.i0] + c.frac * (table[c.i1] - table[c.i0]);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_table1d_sample_nearest)(
        ALWAN_CORE_T const *table, int size, ALWAN_CORE_T coord) {
    ALWAN_CORE_TABLE_CELL const c = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord, size);
    return (c.frac < ALWAN_CORE_LITERAL(0.5)) ? table[c.i0] : table[c.i1];
}

/* Dispatch. The fallback returns the default mode for the rank: a core reader
 * has no status channel and must never leave the dispatch without a value, and
 * must stay free of error paths for the GPU backends. A mode the rank cannot
 * honour is rejected once, at the API tier, before any loop. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_table1d_sample)(
        ALWAN_CORE_T const *table, int size, ALWAN_CORE_T coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return ALWAN_CORE_FNVLIT(alwan_table1d_sample_nearest)(table, size, coord);
    return ALWAN_CORE_FNVLIT(alwan_table1d_sample_linear)(table, size, coord);
}

/* ================================================================
 * Rank 1, mat3x3 element (CVD severity ramps)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FNVLIT(alwan_table1d_mat3_sample_linear)(
        ALWAN_CORE_MAT3X3 const *table, int size, ALWAN_CORE_T coord) {
    ALWAN_CORE_TABLE_CELL const c = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord, size);
    ALWAN_CORE_MAT3X3 result;
    int i;
    for (i = 0; i < 9; i++) {
        result.m[i] = ALWAN_CORE_LERP(table[c.i0].m[i], table[c.i1].m[i], c.frac);
    }
    return result;
}

ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FNVLIT(alwan_table1d_mat3_sample_nearest)(
        ALWAN_CORE_MAT3X3 const *table, int size, ALWAN_CORE_T coord) {
    ALWAN_CORE_TABLE_CELL const c = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord, size);
    return (c.frac < ALWAN_CORE_LITERAL(0.5)) ? table[c.i0] : table[c.i1];
}

/* Not CATMULL_ROM: the reference CVD model defines linear severity
 * interpolation and a 4-tap kernel would change published output. */
ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FNVLIT(alwan_table1d_mat3_sample)(
        ALWAN_CORE_MAT3X3 const *table, int size, ALWAN_CORE_T coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return ALWAN_CORE_FNVLIT(alwan_table1d_mat3_sample_nearest)(table, size, coord);
    return ALWAN_CORE_FNVLIT(alwan_table1d_mat3_sample_linear)(table, size, coord);
}

/* ================================================================
 * Rank 3, interleaved RGB cube, R-fastest
 * index = ((b*size + g)*size + r)*3 + channel
 * ================================================================ */

#define ALWAN_TABLE3D_AT_(cube, size, rr, gg, bb) \
    ((cube) + ((size_t)(bb) * (size_t)(size) * (size_t)(size) + \
               (size_t)(gg) * (size_t)(size) + (size_t)(rr)) * 3)

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_table3d_sample_trilinear)(
        ALWAN_CORE_T const *cube, int size, ALWAN_CORE_VEC3 coord) {
    ALWAN_CORE_TABLE_CELL const cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    ALWAN_CORE_TABLE_CELL const cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    ALWAN_CORE_TABLE_CELL const cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);

    ALWAN_CORE_T const fr = cr.frac;
    ALWAN_CORE_T const fg = cg.frac;
    ALWAN_CORE_T const fb = cb.frac;

    ALWAN_CORE_T const *c000 = ALWAN_TABLE3D_AT_(cube, size, cr.i0, cg.i0, cb.i0);
    ALWAN_CORE_T const *c100 = ALWAN_TABLE3D_AT_(cube, size, cr.i1, cg.i0, cb.i0);
    ALWAN_CORE_T const *c010 = ALWAN_TABLE3D_AT_(cube, size, cr.i0, cg.i1, cb.i0);
    ALWAN_CORE_T const *c110 = ALWAN_TABLE3D_AT_(cube, size, cr.i1, cg.i1, cb.i0);
    ALWAN_CORE_T const *c001 = ALWAN_TABLE3D_AT_(cube, size, cr.i0, cg.i0, cb.i1);
    ALWAN_CORE_T const *c101 = ALWAN_TABLE3D_AT_(cube, size, cr.i1, cg.i0, cb.i1);
    ALWAN_CORE_T const *c011 = ALWAN_TABLE3D_AT_(cube, size, cr.i0, cg.i1, cb.i1);
    ALWAN_CORE_T const *c111 = ALWAN_TABLE3D_AT_(cube, size, cr.i1, cg.i1, cb.i1);

    ALWAN_CORE_VEC3 result;
    for (int ch = 0; ch < 3; ch++) {
        ALWAN_CORE_T c00 = c000[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c100[ch] * fr;
        ALWAN_CORE_T c01 = c001[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c101[ch] * fr;
        ALWAN_CORE_T c10 = c010[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c110[ch] * fr;
        ALWAN_CORE_T c11 = c011[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c111[ch] * fr;

        ALWAN_CORE_T c0 = c00 * (ALWAN_CORE_LITERAL(1.0) - fg) + c10 * fg;
        ALWAN_CORE_T c1 = c01 * (ALWAN_CORE_LITERAL(1.0) - fg) + c11 * fg;

        result.v[ch] = c0 * (ALWAN_CORE_LITERAL(1.0) - fb) + c1 * fb;
    }
    return result;
}

/* Tetrahedral: the ordering of (fr,fg,fb) picks the tetrahedron, giving two
 * intermediate corners and sorted weights w1 >= w2 >= w3. Matches OCIO, and
 * keeps the neutral axis symmetric, which trilinear does not. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_table3d_sample_tetrahedral)(
        ALWAN_CORE_T const *cube, int size, ALWAN_CORE_VEC3 coord) {
    ALWAN_CORE_TABLE_CELL const cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    ALWAN_CORE_TABLE_CELL const cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    ALWAN_CORE_TABLE_CELL const cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);

    int const r0 = cr.i0, g0 = cg.i0, b0 = cb.i0;
    int const r1 = cr.i1, g1 = cg.i1, b1 = cb.i1;
    ALWAN_CORE_T const fr = cr.frac;
    ALWAN_CORE_T const fg = cg.frac;
    ALWAN_CORE_T const fb = cb.frac;

    ALWAN_CORE_T const *c000 = ALWAN_TABLE3D_AT_(cube, size, r0, g0, b0);
    ALWAN_CORE_T const *c111 = ALWAN_TABLE3D_AT_(cube, size, r1, g1, b1);
    ALWAN_CORE_T const *cP;
    ALWAN_CORE_T const *cQ;
    ALWAN_CORE_T w1, w2, w3;
    if (fr >= fg) {
        if (fg >= fb)      { cP = ALWAN_TABLE3D_AT_(cube, size, r1, g0, b0); cQ = ALWAN_TABLE3D_AT_(cube, size, r1, g1, b0); w1 = fr; w2 = fg; w3 = fb; }
        else if (fr >= fb) { cP = ALWAN_TABLE3D_AT_(cube, size, r1, g0, b0); cQ = ALWAN_TABLE3D_AT_(cube, size, r1, g0, b1); w1 = fr; w2 = fb; w3 = fg; }
        else               { cP = ALWAN_TABLE3D_AT_(cube, size, r0, g0, b1); cQ = ALWAN_TABLE3D_AT_(cube, size, r1, g0, b1); w1 = fb; w2 = fr; w3 = fg; }
    } else {
        if (fb >= fg)      { cP = ALWAN_TABLE3D_AT_(cube, size, r0, g0, b1); cQ = ALWAN_TABLE3D_AT_(cube, size, r0, g1, b1); w1 = fb; w2 = fg; w3 = fr; }
        else if (fb >= fr) { cP = ALWAN_TABLE3D_AT_(cube, size, r0, g1, b0); cQ = ALWAN_TABLE3D_AT_(cube, size, r0, g1, b1); w1 = fg; w2 = fb; w3 = fr; }
        else               { cP = ALWAN_TABLE3D_AT_(cube, size, r0, g1, b0); cQ = ALWAN_TABLE3D_AT_(cube, size, r1, g1, b0); w1 = fg; w2 = fr; w3 = fb; }
    }

    ALWAN_CORE_VEC3 result;
    for (int ch = 0; ch < 3; ch++) {
        ALWAN_CORE_T v0 = c000[ch];
        result.v[ch] = v0
                     + w1 * (cP[ch]   - v0)
                     + w2 * (cQ[ch]   - cP[ch])
                     + w3 * (c111[ch] - cQ[ch]);
    }
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_table3d_sample_nearest)(
        ALWAN_CORE_T const *cube, int size, ALWAN_CORE_VEC3 coord) {
    ALWAN_CORE_TABLE_CELL const cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    ALWAN_CORE_TABLE_CELL const cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    ALWAN_CORE_TABLE_CELL const cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);
    int const ri = (cr.frac < ALWAN_CORE_LITERAL(0.5)) ? cr.i0 : cr.i1;
    int const gi = (cg.frac < ALWAN_CORE_LITERAL(0.5)) ? cg.i0 : cg.i1;
    int const bi = (cb.frac < ALWAN_CORE_LITERAL(0.5)) ? cb.i0 : cb.i1;
    ALWAN_CORE_T const *c = ALWAN_TABLE3D_AT_(cube, size, ri, gi, bi);
    ALWAN_CORE_VEC3 result;
    result.v[0] = c[0];
    result.v[1] = c[1];
    result.v[2] = c[2];
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_table3d_sample)(
        ALWAN_CORE_T const *cube, int size, ALWAN_CORE_VEC3 coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return ALWAN_CORE_FNVLIT(alwan_table3d_sample_nearest)(cube, size, coord);
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_TETRAHEDRAL)
        return ALWAN_CORE_FNVLIT(alwan_table3d_sample_tetrahedral)(cube, size, coord);
    return ALWAN_CORE_FNVLIT(alwan_table3d_sample_trilinear)(cube, size, coord);
}

/* ================================================================
 * Rank 2 strip: the same cube flattened to (size*size) x size.
 * index = (g*size*size + b*size + r)*3 + channel
 *
 * The strip IS a cube, so it is sampled trilinearly over r, g and b.
 * ALWAN_SAMPLE_BILINEAR is rejected at the API tier rather than quietly
 * treated as TRILINEAR: substituting a mode is how a colour pipeline ships the
 * wrong curve and nobody notices for two releases.
 * ================================================================ */

#define ALWAN_TABLE2D_AT_(strip, size, rr, gg, bb) \
    ((strip) + ((size_t)(gg) * (size_t)(size) * (size_t)(size) + \
                (size_t)(bb) * (size_t)(size) + (size_t)(rr)) * 3)

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_table2d_sample_trilinear)(
        ALWAN_CORE_T const *strip, int size, ALWAN_CORE_VEC3 coord) {
    ALWAN_CORE_TABLE_CELL const cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    ALWAN_CORE_TABLE_CELL const cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    ALWAN_CORE_TABLE_CELL const cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);

    ALWAN_CORE_T const fr = cr.frac;
    ALWAN_CORE_T const fg = cg.frac;
    ALWAN_CORE_T const fb = cb.frac;

    ALWAN_CORE_T const *c000 = ALWAN_TABLE2D_AT_(strip, size, cr.i0, cg.i0, cb.i0);
    ALWAN_CORE_T const *c100 = ALWAN_TABLE2D_AT_(strip, size, cr.i1, cg.i0, cb.i0);
    ALWAN_CORE_T const *c010 = ALWAN_TABLE2D_AT_(strip, size, cr.i0, cg.i1, cb.i0);
    ALWAN_CORE_T const *c110 = ALWAN_TABLE2D_AT_(strip, size, cr.i1, cg.i1, cb.i0);
    ALWAN_CORE_T const *c001 = ALWAN_TABLE2D_AT_(strip, size, cr.i0, cg.i0, cb.i1);
    ALWAN_CORE_T const *c101 = ALWAN_TABLE2D_AT_(strip, size, cr.i1, cg.i0, cb.i1);
    ALWAN_CORE_T const *c011 = ALWAN_TABLE2D_AT_(strip, size, cr.i0, cg.i1, cb.i1);
    ALWAN_CORE_T const *c111 = ALWAN_TABLE2D_AT_(strip, size, cr.i1, cg.i1, cb.i1);

    ALWAN_CORE_VEC3 result;
    for (int ch = 0; ch < 3; ch++) {
        ALWAN_CORE_T c00 = c000[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c100[ch] * fr;
        ALWAN_CORE_T c01 = c001[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c101[ch] * fr;
        ALWAN_CORE_T c10 = c010[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c110[ch] * fr;
        ALWAN_CORE_T c11 = c011[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c111[ch] * fr;

        ALWAN_CORE_T c0 = c00 * (ALWAN_CORE_LITERAL(1.0) - fg) + c10 * fg;
        ALWAN_CORE_T c1 = c01 * (ALWAN_CORE_LITERAL(1.0) - fg) + c11 * fg;

        result.v[ch] = c0 * (ALWAN_CORE_LITERAL(1.0) - fb) + c1 * fb;
    }
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_table2d_sample_nearest)(
        ALWAN_CORE_T const *strip, int size, ALWAN_CORE_VEC3 coord) {
    ALWAN_CORE_TABLE_CELL const cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    ALWAN_CORE_TABLE_CELL const cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    ALWAN_CORE_TABLE_CELL const cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);
    int const ri = (cr.frac < ALWAN_CORE_LITERAL(0.5)) ? cr.i0 : cr.i1;
    int const gi = (cg.frac < ALWAN_CORE_LITERAL(0.5)) ? cg.i0 : cg.i1;
    int const bi = (cb.frac < ALWAN_CORE_LITERAL(0.5)) ? cb.i0 : cb.i1;
    ALWAN_CORE_T const *c = ALWAN_TABLE2D_AT_(strip, size, ri, gi, bi);
    ALWAN_CORE_VEC3 result;
    result.v[0] = c[0];
    result.v[1] = c[1];
    result.v[2] = c[2];
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_table2d_sample)(
        ALWAN_CORE_T const *strip, int size, ALWAN_CORE_VEC3 coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return ALWAN_CORE_FNVLIT(alwan_table2d_sample_nearest)(strip, size, coord);
    return ALWAN_CORE_FNVLIT(alwan_table2d_sample_trilinear)(strip, size, coord);
}

/* ================================================================
 * Rank 3, three PLANAR scalar cubes sharing one coordinate, B-fastest
 * index = (r*size + g)*size + b
 *
 * This is the Jakob 2019 coefficient layout: three independent scalar fields,
 * not one interleaved RGB cube, and the fastest axis is b rather than r. A
 * separate reader rather than a flag on alwan_table3d_sample, because a
 * layout mismatch that compiles is exactly the trap this file exists to close.
 * ================================================================ */

#define ALWAN_TABLE3D_PLANAR_AT_(size, rr, gg, bb) \
    ((size_t)(rr) * (size_t)(size) * (size_t)(size) + \
     (size_t)(gg) * (size_t)(size) + (size_t)(bb))

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_table3d_planar3_sample_trilinear)(
        ALWAN_CORE_T const *cube0, ALWAN_CORE_T const *cube1, ALWAN_CORE_T const *cube2,
        int size, ALWAN_CORE_VEC3 coord) {
    ALWAN_CORE_TABLE_CELL const cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    ALWAN_CORE_TABLE_CELL const cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    ALWAN_CORE_TABLE_CELL const cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);

    ALWAN_CORE_T const fr = cr.frac;
    ALWAN_CORE_T const fg = cg.frac;
    ALWAN_CORE_T const fb = cb.frac;

    size_t const i000 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i0, cg.i0, cb.i0);
    size_t const i001 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i0, cg.i0, cb.i1);
    size_t const i010 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i0, cg.i1, cb.i0);
    size_t const i011 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i0, cg.i1, cb.i1);
    size_t const i100 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i1, cg.i0, cb.i0);
    size_t const i101 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i1, cg.i0, cb.i1);
    size_t const i110 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i1, cg.i1, cb.i0);
    size_t const i111 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i1, cg.i1, cb.i1);

    ALWAN_CORE_T const one = ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_VEC3 result;

    ALWAN_CORE_T const a_00 = cube0[i000] * (one - fb) + cube0[i001] * fb;
    ALWAN_CORE_T const a_01 = cube0[i010] * (one - fb) + cube0[i011] * fb;
    ALWAN_CORE_T const a_10 = cube0[i100] * (one - fb) + cube0[i101] * fb;
    ALWAN_CORE_T const a_11 = cube0[i110] * (one - fb) + cube0[i111] * fb;
    ALWAN_CORE_T const a_0 = a_00 * (one - fg) + a_01 * fg;
    ALWAN_CORE_T const a_1 = a_10 * (one - fg) + a_11 * fg;
    result.v[0] = a_0 * (one - fr) + a_1 * fr;

    ALWAN_CORE_T const b_00 = cube1[i000] * (one - fb) + cube1[i001] * fb;
    ALWAN_CORE_T const b_01 = cube1[i010] * (one - fb) + cube1[i011] * fb;
    ALWAN_CORE_T const b_10 = cube1[i100] * (one - fb) + cube1[i101] * fb;
    ALWAN_CORE_T const b_11 = cube1[i110] * (one - fb) + cube1[i111] * fb;
    ALWAN_CORE_T const b_0 = b_00 * (one - fg) + b_01 * fg;
    ALWAN_CORE_T const b_1 = b_10 * (one - fg) + b_11 * fg;
    result.v[1] = b_0 * (one - fr) + b_1 * fr;

    ALWAN_CORE_T const c_00 = cube2[i000] * (one - fb) + cube2[i001] * fb;
    ALWAN_CORE_T const c_01 = cube2[i010] * (one - fb) + cube2[i011] * fb;
    ALWAN_CORE_T const c_10 = cube2[i100] * (one - fb) + cube2[i101] * fb;
    ALWAN_CORE_T const c_11 = cube2[i110] * (one - fb) + cube2[i111] * fb;
    ALWAN_CORE_T const c_0 = c_00 * (one - fg) + c_01 * fg;
    ALWAN_CORE_T const c_1 = c_10 * (one - fg) + c_11 * fg;
    result.v[2] = c_0 * (one - fr) + c_1 * fr;

    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_table3d_planar3_sample_nearest)(
        ALWAN_CORE_T const *cube0, ALWAN_CORE_T const *cube1, ALWAN_CORE_T const *cube2,
        int size, ALWAN_CORE_VEC3 coord) {
    ALWAN_CORE_TABLE_CELL const cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    ALWAN_CORE_TABLE_CELL const cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    ALWAN_CORE_TABLE_CELL const cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);
    int const ri = (cr.frac < ALWAN_CORE_LITERAL(0.5)) ? cr.i0 : cr.i1;
    int const gi = (cg.frac < ALWAN_CORE_LITERAL(0.5)) ? cg.i0 : cg.i1;
    int const bi = (cb.frac < ALWAN_CORE_LITERAL(0.5)) ? cb.i0 : cb.i1;
    size_t const i = ALWAN_TABLE3D_PLANAR_AT_(size, ri, gi, bi);
    ALWAN_CORE_VEC3 result;
    result.v[0] = cube0[i];
    result.v[1] = cube1[i];
    result.v[2] = cube2[i];
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_table3d_planar3_sample)(
        ALWAN_CORE_T const *cube0, ALWAN_CORE_T const *cube1, ALWAN_CORE_T const *cube2,
        int size, ALWAN_CORE_VEC3 coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return ALWAN_CORE_FNVLIT(alwan_table3d_planar3_sample_nearest)(cube0, cube1, cube2, size, coord);
    return ALWAN_CORE_FNVLIT(alwan_table3d_planar3_sample_trilinear)(cube0, cube1, cube2, size, coord);
}
