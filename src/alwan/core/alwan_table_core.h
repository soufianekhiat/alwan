/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only table addressing gate, blends and readers
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * The rationale (why an ADDRESS may be clamped when a COLOUR VALUE may not,
 * why the guard is a ternary and not a max intrinsic, and why NaN resolves to
 * the low edge) lives at the top of alwan_table_core.inc.
 */

#ifndef ALWAN_TABLE_CORE_H
#define ALWAN_TABLE_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
/* ALWAN_READ_DATA_NO_BOUND_CHECK lives here. Without this include the gate
 * below reads an UNDEFINED macro, which `#if` silently evaluates to 0, so a
 * user who sets the switch in alwan_config.h still gets the checked path in
 * every TU that reaches this header without going through alwan.h first --
 * including the GPU/Halide branch this file exists for. Fail-safe, but the
 * documented opt-out simply did not work. */
#include "../alwan_config.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_table_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_table_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only
 * ================================================================ */

/* THE GATE. The one place a float coordinate becomes a table position.
 * Postcondition: the result is in [0, size-1] and is not NaN. Every reader
 * below depends on it, and no reader bounds-checks an index afterwards.
 *
 * Written as a ternary, not a max intrinsic. C guarantees NaN >= 0 is false on
 * every target, but the intrinsics disagree: x86 MAXSS returns its second
 * operand on NaN while ARMv8 FMAX (what vmaxq_f32 lowers to) PROPAGATES NaN,
 * so a max intrinsic would fix x86 and leave NEON crashing. ALWAN_SELECT
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
ALWAN_INLINE alwan_scalar alwan_table_coord_v(alwan_scalar coord, int size) {
#if ALWAN_READ_DATA_NO_BOUND_CHECK
    /* Caller asserts every coordinate is finite and in [0,1]. A NaN here is an
     * out-of-bounds read, not a wrong colour. See alwan_config.h. */
    return coord * (alwan_scalar)(size - 1);
#else
    alwan_scalar c = ALWAN_SELECT(coord >= ALWAN_ZERO, coord, ALWAN_ZERO);
    c = ALWAN_SELECT(c <= ALWAN_LITERAL(1.0), c, ALWAN_LITERAL(1.0));
    return c * (alwan_scalar)(size - 1);
#endif
}

/* Same gate for a coordinate already in TABLE UNITS (hue degrees, severity*10)
 * rather than normalized [0,1]. No rescale, same NaN-to-low-edge rule. */
ALWAN_INLINE alwan_scalar alwan_table_coord_unit_v(alwan_scalar pos, int size) {
#if ALWAN_READ_DATA_NO_BOUND_CHECK
    ALWAN_UNUSED(size);
    return pos;
#else
    const alwan_scalar hi = (alwan_scalar)(size - 1);
    alwan_scalar c = ALWAN_SELECT(pos >= ALWAN_ZERO, pos, ALWAN_ZERO);
    return ALWAN_SELECT(c <= hi, c, hi);
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
ALWAN_INLINE int alwan_table_row_v(int index, int count) {
#if ALWAN_READ_DATA_NO_BOUND_CHECK
    ALWAN_UNUSED(count);
    return index;
#else
    const int lo = index < 0 ? 0 : index;
    return lo >= count ? (count > 0 ? count - 1 : 0) : lo;
#endif
}


/* Predicate backing ALWAN_SAMPLE_STRICT at the API tier. False for NaN,
 * because both compares fail, which is the whole point. */
ALWAN_INLINE int alwan_table_coord_in_range_v(alwan_scalar coord) {
    return (coord >= ALWAN_ZERO && coord <= ALWAN_LITERAL(1.0)) ? 1 : 0;
}

/* Resolve a normalized coordinate to a cell. Holding the i1 clamp and the frac
 * subtraction here means they exist once instead of once per reader. */
ALWAN_INLINE alwan_table_cell alwan_table_cell_v(alwan_scalar coord, int size) {
    const alwan_scalar p = alwan_table_coord_v(coord, size);
    alwan_table_cell r;
    r.i0 = (int)p;                                    /* gate proves [0, size-1] */
    r.i1 = (r.i0 + 1 < size) ? (r.i0 + 1) : (size - 1);
    r.frac = p - (alwan_scalar)r.i0;
    return r;
}

/* Cell from a coordinate already in table units. */
ALWAN_INLINE alwan_table_cell alwan_table_cell_unit_v(alwan_scalar pos, int size) {
    const alwan_scalar p = alwan_table_coord_unit_v(pos, size);
    alwan_table_cell r;
    r.i0 = (int)p;
    r.i1 = (r.i0 + 1 < size) ? (r.i0 + 1) : (size - 1);
    r.frac = p - (alwan_scalar)r.i0;
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
ALWAN_INLINE int alwan_table_cell_nearest_v(alwan_table_cell c) {
    return (c.frac < ALWAN_LITERAL(0.5)) ? c.i0 : c.i1;
}

/* Weighted form, a*(1-f) + b*f. Matches alwan_lerp bit for bit. */
ALWAN_INLINE alwan_scalar alwan_table_blend_v(
        alwan_scalar v0, alwan_scalar v1, alwan_scalar frac) {
    ALWAN_DET_PRECISE alwan_scalar r = v0 * (ALWAN_LITERAL(1.0) - frac) + v1 * frac;
    return r;
}

/* Delta form, a + f*(b-a). Not the same bits as the weighted form. See
 * alwan_table1d_sample_linear_delta for why both are kept. */
ALWAN_INLINE alwan_scalar alwan_table_blend_delta_v(
        alwan_scalar v0, alwan_scalar v1, alwan_scalar frac) {
    ALWAN_DET_PRECISE alwan_scalar r = v0 + frac * (v1 - v0);
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
ALWAN_INLINE alwan_scalar alwan_table_blend_catmull_rom_v(
        alwan_scalar p0, alwan_scalar p1, alwan_scalar p2, alwan_scalar p3,
        alwan_scalar t) {
    const alwan_scalar t2 = t * t;
    const alwan_scalar t3 = t2 * t;

    ALWAN_DET_PRECISE alwan_scalar r = ALWAN_LITERAL(0.5) * (
        (ALWAN_LITERAL(2.0) * p1) +
        (p2 - p0) * t +
        (ALWAN_LITERAL(2.0) * p0 - ALWAN_LITERAL(5.0) * p1 +
         ALWAN_LITERAL(4.0) * p2 - p3) * t2 +
        (ALWAN_LITERAL(3.0) * (p1 - p2) + p3 - p0) * t3);
    return r;
}

/* Element-wise weighted blend of two matrices (CVD severity ramps). The
 * weighted form, which in C is alwan_lerp bit for bit. */
ALWAN_INLINE alwan_mat3x3 alwan_table_blend_mat3_v(
        alwan_mat3x3 m0, alwan_mat3x3 m1, alwan_scalar frac) {
    alwan_mat3x3 result;
    int i;
    for (i = 0; i < 9; i++) {
        result.m[i] = alwan_table_blend_v(m0.m[i], m1.m[i], frac);
    }
    return result;
}

/* Trilinear over the eight corners of a cell. The corner names are vABC with A
 * the FIRST axis blended, by f1, then B by f2, then C by f3. The order is part
 * of the result: blending the axes in another order rounds differently, and
 * each layout has always blended its fastest axis first. */
ALWAN_INLINE alwan_scalar alwan_table_blend_trilinear_v(
        alwan_scalar v000, alwan_scalar v100, alwan_scalar v010, alwan_scalar v110,
        alwan_scalar v001, alwan_scalar v101, alwan_scalar v011, alwan_scalar v111,
        alwan_scalar f1, alwan_scalar f2, alwan_scalar f3) {
    const alwan_scalar c00 = v000 * (ALWAN_LITERAL(1.0) - f1) + v100 * f1;
    const alwan_scalar c01 = v001 * (ALWAN_LITERAL(1.0) - f1) + v101 * f1;
    const alwan_scalar c10 = v010 * (ALWAN_LITERAL(1.0) - f1) + v110 * f1;
    const alwan_scalar c11 = v011 * (ALWAN_LITERAL(1.0) - f1) + v111 * f1;
    const alwan_scalar c0 = c00 * (ALWAN_LITERAL(1.0) - f2) + c10 * f2;
    const alwan_scalar c1 = c01 * (ALWAN_LITERAL(1.0) - f2) + c11 * f2;
    ALWAN_DET_PRECISE alwan_scalar r = c0 * (ALWAN_LITERAL(1.0) - f3) + c1 * f3;
    return r;
}

/* Tetrahedral: the walk 000 -> P -> Q -> 111 along three edges of the
 * tetrahedron, weighted by the sorted fractions w1 >= w2 >= w3. */
ALWAN_INLINE alwan_scalar alwan_table_blend_tetrahedral_v(
        alwan_scalar v000, alwan_scalar vP, alwan_scalar vQ, alwan_scalar v111,
        alwan_scalar w1, alwan_scalar w2, alwan_scalar w3) {
    ALWAN_DET_PRECISE alwan_scalar r = v000
         + w1 * (vP   - v000)
         + w2 * (vQ   - vP)
         + w3 * (v111 - vQ);
    return r;
}

/* Prism: the triangle 000 -> P1 -> P2 in the r-g plane, P1 the corner along the
 * larger of the two fractions (f1 >= f2), and the same triangle one blue step up
 * (Q0, Q1, Q2); barycentric in the triangle, linear in fb. */
ALWAN_INLINE alwan_scalar alwan_table_blend_prism_v(
        alwan_scalar p0, alwan_scalar p1, alwan_scalar p2,
        alwan_scalar q0, alwan_scalar q1, alwan_scalar q2,
        alwan_scalar f1, alwan_scalar f2, alwan_scalar fb) {
    ALWAN_DET_PRECISE alwan_scalar r = p0
         + f1 * (p1 - p0)
         + f2 * (p2 - p1)
         + fb * (q0 - p0)
         + f1 * fb * ((q1 - q0) - (p1 - p0))
         + f2 * fb * ((q2 - q1) - (p2 - p1));
    return r;
}

/* Pyramid: base square v000, vA, vB, vAB on the face where the smallest fraction s
 * is zero, apex v111. pa = fA - s, pb = fB - s, d = 1 - s; the base is read
 * bilinearly at (pa / d, pb / d) and blended towards the apex by s. Written out so
 * the only division is the bilinear cross term, which vanishes at the apex. */
ALWAN_INLINE alwan_scalar alwan_table_blend_pyramid_v(
        alwan_scalar v000, alwan_scalar vA, alwan_scalar vB, alwan_scalar vAB, alwan_scalar v111,
        alwan_scalar pa, alwan_scalar pb, alwan_scalar s) {
    const alwan_scalar d = ALWAN_LITERAL(1.0) - s;
    const alwan_scalar q = ALWAN_SELECT(d > ALWAN_LITERAL(0.0), pa * pb / ALWAN_SELECT(d > ALWAN_LITERAL(0.0), d, ALWAN_LITERAL(1.0)), ALWAN_LITERAL(0.0));
    ALWAN_DET_PRECISE alwan_scalar r = v000 * (d - pa - pb)
         + vA * pa
         + vB * pb
         + v111 * s
         + ((v000 - vA) - (vB - vAB)) * q;
    return r;
}

/* Flat index of channel 0 of one node, per layout. The three layouts differ in
 * which axis is fastest and in whether the channels are interleaved, and a
 * mismatch between them compiles and returns plausible colours, so each has
 * one spelling, here, and no reader computes an address any other way. */

/* Interleaved RGB cube, R-fastest: ((b*size + g)*size + r)*3 */
ALWAN_INLINE size_t alwan_table3d_index_v(int size, int r, int g, int b) {
    return ((size_t)b * (size_t)size * (size_t)size +
            (size_t)g * (size_t)size + (size_t)r) * 3;
}

/* The same cube as a (size*size) x size strip: (g*size*size + b*size + r)*3 */
ALWAN_INLINE size_t alwan_table2d_strip_index_v(int size, int r, int g, int b) {
    return ((size_t)g * (size_t)size * (size_t)size +
            (size_t)b * (size_t)size + (size_t)r) * 3;
}

/* One plane of three, B-fastest: (r*size + g)*size + b */
ALWAN_INLINE size_t alwan_table3d_planar_index_v(int size, int r, int g, int b) {
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

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_TABLE_CORE_H */
