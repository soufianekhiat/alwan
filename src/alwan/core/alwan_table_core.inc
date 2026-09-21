/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only table addressing gate, blends and readers (parameterized)
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

/* Row gate for tables addressed by an INTEGER index rather than a float
 * coordinate. Same switch, same policy as alwan_table_coord above: the ADDRESS
 * is clamped so a bad index cannot read outside the array, while the VALUE is
 * never silently altered.
 *
 * This existed only for float coordinates until 2026-08-27, because the crash
 * class that motivated the gate was NaN reaching an (int) cast. An integer
 * index goes out of bounds just as easily: alwan_rgb_space_by_enum_* checked
 * `index < g_rgb_spaces_count` -- the length of the METADATA table -- and then
 * indexed the DATA table, which the generator had silently truncated by two
 * rows. Seven spaces returned another colourspace's primaries and two read 16
 * doubles past the end.
 *
 * Callers still validate and return ALWAN_E_INVALID for a genuinely bad index.
 * This is the backstop for when that validation is itself wrong, which is
 * exactly what happened. */
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_table_row)(int index, int count) {
#if ALWAN_READ_DATA_NO_BOUND_CHECK
    ALWAN_UNUSED(count);
    return index;
#else
    const int lo = index < 0 ? 0 : index;
    return lo >= count ? (count > 0 ? count - 1 : 0) : lo;
#endif
}

/* Same gate for a coordinate already in TABLE UNITS (hue degrees, severity*10)
 * rather than normalized [0,1]. No rescale, same NaN-to-low-edge rule. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_table_coord_unit)(ALWAN_CORE_T pos, int size) {
#if ALWAN_READ_DATA_NO_BOUND_CHECK
    ALWAN_UNUSED(size);
    return pos;
#else
    const ALWAN_CORE_T hi = (ALWAN_CORE_T)(size - 1);
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
    const ALWAN_CORE_T p = ALWAN_CORE_FNVLIT(alwan_table_coord)(coord, size);
    ALWAN_CORE_TABLE_CELL r;
    r.i0 = (int)p;                                    /* gate proves [0, size-1] */
    r.i1 = (r.i0 + 1 < size) ? (r.i0 + 1) : (size - 1);
    r.frac = p - (ALWAN_CORE_T)r.i0;
    return r;
}

/* Cell from a coordinate already in table units. */
ALWAN_INLINE ALWAN_CORE_TABLE_CELL ALWAN_CORE_FNVLIT(alwan_table_cell_unit)(ALWAN_CORE_T pos, int size) {
    const ALWAN_CORE_T p = ALWAN_CORE_FNVLIT(alwan_table_coord_unit)(pos, size);
    ALWAN_CORE_TABLE_CELL r;
    r.i0 = (int)p;
    r.i1 = (r.i0 + 1 < size) ? (r.i0 + 1) : (size - 1);
    r.frac = p - (ALWAN_CORE_T)r.i0;
    return r;
}

/* ================================================================
 * BLENDS AND ADDRESSES: the part of a reader that touches no table
 *
 * A reader is three steps: the gate above turns a coordinate into indices, the
 * table is READ at those indices, and the values are blended. Only the middle
 * step needs to know what a table is, and that is the one step a shading
 * language cannot share with C, because it has no pointer to pass. So the two
 * outer steps live here as plain value functions, and the readers in
 * alwan_table_reader.inc are built from them around an accessor.
 *
 * A caller that fetches its own elements, from a texture or a texel fetch,
 * uses these directly and gets the library's bits.
 *
 * THE BITS ARE THE SAME ON A GPU, in every build and not only a deterministic
 * one. A table read is data, and its value is pinned by the determinism MD5s,
 * so each blend computes into an ALWAN_DET_PRECISE local. That is `precise` in
 * HLSL and GLSL and nothing in C. It was measured rather than assumed: without
 * it dxc reassociates the Catmull-Rom and tetrahedral sums and lands one ULP
 * off on a third of the samples, while fxc and the two shorter blends happen to
 * agree. alwan_dev/hlsl_regression/run_table_parity.py holds every reader to
 * bit equality with C, on both compilers.
 *
 * For the same reason no blend here goes through alwan_lerp. On a shading
 * language that is the lerp or mix intrinsic, a + t*(b-a), which is one
 * rounding away from the weighted form C spells out.
 * ================================================================ */

/* Nearest index of a cell. Ties go UP, as they always have: frac < 0.5 picks
 * i0, anything else i1. An index rather than a value, so one spelling serves a
 * scalar, a matrix and a colour, and a shading language is never asked to run
 * a ternary over a struct, which legacy HLSL refuses. */
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(ALWAN_CORE_TABLE_CELL c) {
    return (c.frac < ALWAN_CORE_LITERAL(0.5)) ? c.i0 : c.i1;
}

/* Weighted form, a*(1-f) + b*f. Matches alwan_lerp bit for bit. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_table_blend)(
        ALWAN_CORE_T v0, ALWAN_CORE_T v1, ALWAN_CORE_T frac) {
    ALWAN_DET_PRECISE ALWAN_CORE_T r = v0 * (ALWAN_CORE_LITERAL(1.0) - frac) + v1 * frac;
    return r;
}

/* Delta form, a + f*(b-a). Not the same bits as the weighted form. See
 * alwan_table1d_sample_linear_delta for why both are kept. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_table_blend_delta)(
        ALWAN_CORE_T v0, ALWAN_CORE_T v1, ALWAN_CORE_T frac) {
    ALWAN_DET_PRECISE ALWAN_CORE_T r = v0 + frac * (v1 - v0);
    return r;
}

/* Catmull-Rom: the four-tap cubic through p0..p3, evaluated between p1 and p2.
 * C1 and interpolating, so it passes through every sample rather than
 * smoothing them. Worth having for LUT sampling, where linear leaves visible
 * facets on a coarse grid.
 *
 * Unlike the linear blend this can OVERSHOOT: a 4-tap cubic through a step
 * leaves the convex hull of its taps by up to about 1/8 of the step. Callers
 * sampling a table whose values must stay in a range have to clamp the result,
 * which is why this is not the default for any rank. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_table_blend_catmull_rom)(
        ALWAN_CORE_T p0, ALWAN_CORE_T p1, ALWAN_CORE_T p2, ALWAN_CORE_T p3,
        ALWAN_CORE_T t) {
    const ALWAN_CORE_T t2 = t * t;
    const ALWAN_CORE_T t3 = t2 * t;

    ALWAN_DET_PRECISE ALWAN_CORE_T r = ALWAN_CORE_LITERAL(0.5) * (
        (ALWAN_CORE_LITERAL(2.0) * p1) +
        (p2 - p0) * t +
        (ALWAN_CORE_LITERAL(2.0) * p0 - ALWAN_CORE_LITERAL(5.0) * p1 +
         ALWAN_CORE_LITERAL(4.0) * p2 - p3) * t2 +
        (ALWAN_CORE_LITERAL(3.0) * (p1 - p2) + p3 - p0) * t3);
    return r;
}

/* Element-wise weighted blend of two matrices (CVD severity ramps). The
 * weighted form, which in C is alwan_lerp bit for bit. */
ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FNVLIT(alwan_table_blend_mat3)(
        ALWAN_CORE_MAT3X3 m0, ALWAN_CORE_MAT3X3 m1, ALWAN_CORE_T frac) {
    ALWAN_CORE_MAT3X3 result;
    int i;
    for (i = 0; i < 9; i++) {
        result.m[i] = ALWAN_CORE_FNVLIT(alwan_table_blend)(m0.m[i], m1.m[i], frac);
    }
    return result;
}

/* Trilinear over the eight corners of a cell. The corner names are vABC with A
 * the FIRST axis blended, by f1, then B by f2, then C by f3. The order is part
 * of the result: blending the axes in another order rounds differently, and
 * each layout has always blended its fastest axis first. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_table_blend_trilinear)(
        ALWAN_CORE_T v000, ALWAN_CORE_T v100, ALWAN_CORE_T v010, ALWAN_CORE_T v110,
        ALWAN_CORE_T v001, ALWAN_CORE_T v101, ALWAN_CORE_T v011, ALWAN_CORE_T v111,
        ALWAN_CORE_T f1, ALWAN_CORE_T f2, ALWAN_CORE_T f3) {
    const ALWAN_CORE_T c00 = v000 * (ALWAN_CORE_LITERAL(1.0) - f1) + v100 * f1;
    const ALWAN_CORE_T c01 = v001 * (ALWAN_CORE_LITERAL(1.0) - f1) + v101 * f1;
    const ALWAN_CORE_T c10 = v010 * (ALWAN_CORE_LITERAL(1.0) - f1) + v110 * f1;
    const ALWAN_CORE_T c11 = v011 * (ALWAN_CORE_LITERAL(1.0) - f1) + v111 * f1;
    const ALWAN_CORE_T c0 = c00 * (ALWAN_CORE_LITERAL(1.0) - f2) + c10 * f2;
    const ALWAN_CORE_T c1 = c01 * (ALWAN_CORE_LITERAL(1.0) - f2) + c11 * f2;
    ALWAN_DET_PRECISE ALWAN_CORE_T r = c0 * (ALWAN_CORE_LITERAL(1.0) - f3) + c1 * f3;
    return r;
}

/* Tetrahedral: the walk 000 -> P -> Q -> 111 along three edges of the
 * tetrahedron, weighted by the sorted fractions w1 >= w2 >= w3. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_table_blend_tetrahedral)(
        ALWAN_CORE_T v000, ALWAN_CORE_T vP, ALWAN_CORE_T vQ, ALWAN_CORE_T v111,
        ALWAN_CORE_T w1, ALWAN_CORE_T w2, ALWAN_CORE_T w3) {
    ALWAN_DET_PRECISE ALWAN_CORE_T r = v000
         + w1 * (vP   - v000)
         + w2 * (vQ   - vP)
         + w3 * (v111 - vQ);
    return r;
}

/* Flat index of channel 0 of one node, per layout. The three layouts differ in
 * which axis is fastest and in whether the channels are interleaved, and a
 * mismatch between them compiles and returns plausible colours, so each has
 * one spelling, here, and no reader computes an address any other way. */

/* Interleaved RGB cube, R-fastest: ((b*size + g)*size + r)*3 */
ALWAN_INLINE size_t ALWAN_CORE_FNVLIT(alwan_table3d_index)(int size, int r, int g, int b) {
    return ((size_t)b * (size_t)size * (size_t)size +
            (size_t)g * (size_t)size + (size_t)r) * 3;
}

/* The same cube as a (size*size) x size strip: (g*size*size + b*size + r)*3 */
ALWAN_INLINE size_t ALWAN_CORE_FNVLIT(alwan_table2d_strip_index)(int size, int r, int g, int b) {
    return ((size_t)g * (size_t)size * (size_t)size +
            (size_t)b * (size_t)size + (size_t)r) * 3;
}

/* One plane of three, B-fastest: (r*size + g)*size + b */
ALWAN_INLINE size_t ALWAN_CORE_FNVLIT(alwan_table3d_planar_index)(int size, int r, int g, int b) {
    return (size_t)r * (size_t)size * (size_t)size +
           (size_t)g * (size_t)size + (size_t)b;
}

/* ================================================================
 * THE READERS
 *
 * One body for every backend, in alwan_table_reader.inc, bound here to a
 * pointer. A shading language has no pointer and binds its own accessor
 * instead. See that file.
 * ================================================================ */
#if ALWAN_HAS_POINTERS
#define ALWAN_TABLE_READER_POINTER 1
#include "alwan_table_reader.inc"
#endif
