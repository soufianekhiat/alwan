/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only table addressing gate and readers
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
    (void)size;
    return pos;
#else
    alwan_scalar const hi = (alwan_scalar)(size - 1);
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
    (void)count;
    return index;
#else
    int const lo = index < 0 ? 0 : index;
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
    alwan_scalar const p = alwan_table_coord_v(coord, size);
    alwan_table_cell r;
    r.i0 = (int)p;                                    /* gate proves [0, size-1] */
    r.i1 = (r.i0 + 1 < size) ? (r.i0 + 1) : (size - 1);
    r.frac = p - (alwan_scalar)r.i0;
    return r;
}

/* Cell from a coordinate already in table units. */
ALWAN_INLINE alwan_table_cell alwan_table_cell_unit_v(alwan_scalar pos, int size) {
    alwan_scalar const p = alwan_table_coord_unit_v(pos, size);
    alwan_table_cell r;
    r.i0 = (int)p;
    r.i1 = (r.i0 + 1 < size) ? (r.i0 + 1) : (size - 1);
    r.frac = p - (alwan_scalar)r.i0;
    return r;
}

/* ================================================================
 * Rank 1, scalar element
 * ================================================================ */

/* Element read at an INTEGER index -- the rank-1 counterpart of the float
 * readers below. Same gate, same policy, no interpolation and no arithmetic:
 * for an index the caller has already proved in range this is the identity on
 * the address, so folding an existing raw subscript onto it cannot move a
 * value. It exists so a table addressed by a loop counter or a validated enum
 * is governed by ALWAN_READ_DATA_NO_BOUND_CHECK exactly like a table addressed
 * by a coordinate, instead of by whatever bound the call site happened to
 * spell. Header-only for the same reason as everything else here: with no /GL
 * and no /LTCG a reader in a .c is a real call per element. */
ALWAN_INLINE alwan_scalar alwan_table1d_row_v(
        alwan_scalar const *table, int size, int index) {
    return table[alwan_table_row_v(index, size)];
}

/* Weighted form, a*(1-f) + b*f. Matches alwan_lerp and the shipped
 * alwan_lut1d_sample bit for bit. This is ALWAN_SAMPLE_LINEAR. */
ALWAN_INLINE alwan_scalar alwan_table1d_sample_linear_v(
        alwan_scalar const *table, int size, alwan_scalar coord) {
    alwan_table_cell const c = alwan_table_cell_v(coord, size);
    return table[c.i0] * (ALWAN_LITERAL(1.0) - c.frac) + table[c.i1] * c.frac;
}

/* Delta form, a + f*(b-a). Algebraically the same line, not the same bits: one
 * rounding lands differently. The AgX contrast LUT has always been read this
 * way and its output is pinned by the determinism MD5s, so the spelling is
 * preserved rather than silently re-rounded. Same gate, same bounds
 * guarantee -- only the blend differs. */
ALWAN_INLINE alwan_scalar alwan_table1d_sample_linear_delta_v(
        alwan_scalar const *table, int size, alwan_scalar coord) {
    alwan_table_cell const c = alwan_table_cell_v(coord, size);
    return table[c.i0] + c.frac * (table[c.i1] - table[c.i0]);
}

ALWAN_INLINE alwan_scalar alwan_table1d_sample_nearest_v(
        alwan_scalar const *table, int size, alwan_scalar coord) {
    alwan_table_cell const c = alwan_table_cell_v(coord, size);
    return (c.frac < ALWAN_LITERAL(0.5)) ? table[c.i0] : table[c.i1];
}

/* Catmull-Rom: the four-tap cubic through table[i-1 .. i+2], which is C1 and
 * interpolating, so it passes through every sample rather than smoothing them.
 * Worth having for LUT sampling, where linear leaves visible facets on a coarse
 * grid.
 *
 * The two outer taps are clamped by alwan_table_row, so the end intervals see a
 * doubled endpoint. That is the standard clamped-edge form: the curve stays
 * interpolating at the ends and its slope there is one-sided.
 *
 * Unlike the linear reader this can OVERSHOOT: a 4-tap cubic through a step
 * leaves the convex hull of its taps by up to about 1/8 of the step. Callers
 * sampling a table whose values must stay in a range have to clamp the result,
 * which is why this is not the default for any rank. */
ALWAN_INLINE alwan_scalar alwan_table1d_sample_catmull_rom_v(
        alwan_scalar const *table, int size, alwan_scalar coord) {
    alwan_table_cell const c = alwan_table_cell_v(coord, size);
    int const im1 = alwan_table_row_v(c.i0 - 1, size);
    int const ip2 = alwan_table_row_v(c.i1 + 1, size);

    alwan_scalar const p0 = table[im1];
    alwan_scalar const p1 = table[c.i0];
    alwan_scalar const p2 = table[c.i1];
    alwan_scalar const p3 = table[ip2];

    alwan_scalar const t  = c.frac;
    alwan_scalar const t2 = t * t;
    alwan_scalar const t3 = t2 * t;

    return ALWAN_LITERAL(0.5) * (
        (ALWAN_LITERAL(2.0) * p1) +
        (p2 - p0) * t +
        (ALWAN_LITERAL(2.0) * p0 - ALWAN_LITERAL(5.0) * p1 +
         ALWAN_LITERAL(4.0) * p2 - p3) * t2 +
        (ALWAN_LITERAL(3.0) * (p1 - p2) + p3 - p0) * t3);
}

/* Dispatch. The fallback returns the default mode for the rank: a core reader
 * has no status channel and must never leave the dispatch without a value, and
 * must stay free of error paths for the GPU backends. A mode the rank cannot
 * honour is rejected once, at the API tier, before any loop. */
ALWAN_INLINE alwan_scalar alwan_table1d_sample_v(
        alwan_scalar const *table, int size, alwan_scalar coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return alwan_table1d_sample_nearest_v(table, size, coord);
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_CATMULL_ROM)
        return alwan_table1d_sample_catmull_rom_v(table, size, coord);
    return alwan_table1d_sample_linear_v(table, size, coord);
}

/* ================================================================
 * Rank 1, mat3x3 element (CVD severity ramps)
 * ================================================================ */

ALWAN_INLINE alwan_mat3x3 alwan_table1d_mat3_sample_linear_v(
        alwan_mat3x3 const *table, int size, alwan_scalar coord) {
    alwan_table_cell const c = alwan_table_cell_v(coord, size);
    alwan_mat3x3 result;
    int i;
    for (i = 0; i < 9; i++) {
        result.m[i] = alwan_lerp(table[c.i0].m[i], table[c.i1].m[i], c.frac);
    }
    return result;
}

ALWAN_INLINE alwan_mat3x3 alwan_table1d_mat3_sample_nearest_v(
        alwan_mat3x3 const *table, int size, alwan_scalar coord) {
    alwan_table_cell const c = alwan_table_cell_v(coord, size);
    return (c.frac < ALWAN_LITERAL(0.5)) ? table[c.i0] : table[c.i1];
}

/* Not CATMULL_ROM: the reference CVD model defines linear severity
 * interpolation and a 4-tap kernel would change published output. */
ALWAN_INLINE alwan_mat3x3 alwan_table1d_mat3_sample_v(
        alwan_mat3x3 const *table, int size, alwan_scalar coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return alwan_table1d_mat3_sample_nearest_v(table, size, coord);
    return alwan_table1d_mat3_sample_linear_v(table, size, coord);
}

/* ================================================================
 * Rank 2, row-major with a fixed stride, INTEGER row and column
 * index = row*stride + col
 *
 * Both coordinates go through the row gate, not just the row. A stride table
 * is exactly where a correct row bound and a wrong column bound read into the
 * NEXT ROW rather than off the end: silent wrong data, no crash, no warning.
 * That is the shape the 109 reflectance sample sets had while they were 109
 * separate arrays behind an array of pointers.
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_table2d_row_at_v(
        alwan_scalar const *table, int rows, int cols, int row, int col) {
    int const r = alwan_table_row_v(row, rows);
    int const c = alwan_table_row_v(col, cols);
    return table[(size_t)r * (size_t)cols + (size_t)c];
}

/* Bilinear over the same rows x cols grid, both axes in [0,1].
 *
 * This is what ALWAN_SAMPLE_BILINEAR means in alwan: a genuine 2-d grid, not
 * the flattened cube in the strip readers below, which is sampled trilinearly
 * and still rejects BILINEAR.
 *
 * Row and column are separate axes with separate extents, so each gets its own
 * cell and both go through the same gate as the integer reader above. */
ALWAN_INLINE alwan_scalar alwan_table2d_grid_sample_bilinear_v(
        alwan_scalar const *table, int rows, int cols,
        alwan_scalar row_coord, alwan_scalar col_coord) {
    alwan_table_cell const cr = alwan_table_cell_v(row_coord, rows);
    alwan_table_cell const cc = alwan_table_cell_v(col_coord, cols);

    alwan_scalar const v00 = alwan_table2d_row_at_v(table, rows, cols, cr.i0, cc.i0);
    alwan_scalar const v01 = alwan_table2d_row_at_v(table, rows, cols, cr.i0, cc.i1);
    alwan_scalar const v10 = alwan_table2d_row_at_v(table, rows, cols, cr.i1, cc.i0);
    alwan_scalar const v11 = alwan_table2d_row_at_v(table, rows, cols, cr.i1, cc.i1);

    alwan_scalar const top = alwan_lerp(v00, v01, cc.frac);
    alwan_scalar const bot = alwan_lerp(v10, v11, cc.frac);
    return alwan_lerp(top, bot, cr.frac);
}

ALWAN_INLINE alwan_scalar alwan_table2d_grid_sample_nearest_v(
        alwan_scalar const *table, int rows, int cols,
        alwan_scalar row_coord, alwan_scalar col_coord) {
    alwan_table_cell const cr = alwan_table_cell_v(row_coord, rows);
    alwan_table_cell const cc = alwan_table_cell_v(col_coord, cols);
    int const r = (cr.frac < ALWAN_LITERAL(0.5)) ? cr.i0 : cr.i1;
    int const c = (cc.frac < ALWAN_LITERAL(0.5)) ? cc.i0 : cc.i1;
    return alwan_table2d_row_at_v(table, rows, cols, r, c);
}

/* LINEAR resolves to bilinear here, the same way it resolves to trilinear at
 * rank 3: it is the zero value, so a zero-initialised mode must interpolate. */
ALWAN_INLINE alwan_scalar alwan_table2d_grid_sample_v(
        alwan_scalar const *table, int rows, int cols,
        alwan_scalar row_coord, alwan_scalar col_coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return alwan_table2d_grid_sample_nearest_v(table, rows, cols, row_coord, col_coord);
    return alwan_table2d_grid_sample_bilinear_v(table, rows, cols, row_coord, col_coord);
}

/* ================================================================
 * Rank 3, interleaved RGB cube, R-fastest
 * index = ((b*size + g)*size + r)*3 + channel
 * ================================================================ */

#define ALWAN_TABLE3D_AT_(cube, size, rr, gg, bb) \
    ((cube) + ((size_t)(bb) * (size_t)(size) * (size_t)(size) + \
               (size_t)(gg) * (size_t)(size) + (size_t)(rr)) * 3)

ALWAN_INLINE alwan_vec3 alwan_table3d_sample_trilinear_v(
        alwan_scalar const *cube, int size, alwan_vec3 coord) {
    alwan_table_cell const cr = alwan_table_cell_v(coord.v[0], size);
    alwan_table_cell const cg = alwan_table_cell_v(coord.v[1], size);
    alwan_table_cell const cb = alwan_table_cell_v(coord.v[2], size);

    alwan_scalar const fr = cr.frac;
    alwan_scalar const fg = cg.frac;
    alwan_scalar const fb = cb.frac;

    alwan_scalar const *c000 = ALWAN_TABLE3D_AT_(cube, size, cr.i0, cg.i0, cb.i0);
    alwan_scalar const *c100 = ALWAN_TABLE3D_AT_(cube, size, cr.i1, cg.i0, cb.i0);
    alwan_scalar const *c010 = ALWAN_TABLE3D_AT_(cube, size, cr.i0, cg.i1, cb.i0);
    alwan_scalar const *c110 = ALWAN_TABLE3D_AT_(cube, size, cr.i1, cg.i1, cb.i0);
    alwan_scalar const *c001 = ALWAN_TABLE3D_AT_(cube, size, cr.i0, cg.i0, cb.i1);
    alwan_scalar const *c101 = ALWAN_TABLE3D_AT_(cube, size, cr.i1, cg.i0, cb.i1);
    alwan_scalar const *c011 = ALWAN_TABLE3D_AT_(cube, size, cr.i0, cg.i1, cb.i1);
    alwan_scalar const *c111 = ALWAN_TABLE3D_AT_(cube, size, cr.i1, cg.i1, cb.i1);

    alwan_vec3 result;
    for (int ch = 0; ch < 3; ch++) {
        alwan_scalar c00 = c000[ch] * (ALWAN_LITERAL(1.0) - fr) + c100[ch] * fr;
        alwan_scalar c01 = c001[ch] * (ALWAN_LITERAL(1.0) - fr) + c101[ch] * fr;
        alwan_scalar c10 = c010[ch] * (ALWAN_LITERAL(1.0) - fr) + c110[ch] * fr;
        alwan_scalar c11 = c011[ch] * (ALWAN_LITERAL(1.0) - fr) + c111[ch] * fr;

        alwan_scalar c0 = c00 * (ALWAN_LITERAL(1.0) - fg) + c10 * fg;
        alwan_scalar c1 = c01 * (ALWAN_LITERAL(1.0) - fg) + c11 * fg;

        result.v[ch] = c0 * (ALWAN_LITERAL(1.0) - fb) + c1 * fb;
    }
    return result;
}

/* Tetrahedral: the ordering of (fr,fg,fb) picks the tetrahedron, giving two
 * intermediate corners and sorted weights w1 >= w2 >= w3. Matches OCIO, and
 * keeps the neutral axis symmetric, which trilinear does not. */
ALWAN_INLINE alwan_vec3 alwan_table3d_sample_tetrahedral_v(
        alwan_scalar const *cube, int size, alwan_vec3 coord) {
    alwan_table_cell const cr = alwan_table_cell_v(coord.v[0], size);
    alwan_table_cell const cg = alwan_table_cell_v(coord.v[1], size);
    alwan_table_cell const cb = alwan_table_cell_v(coord.v[2], size);

    int const r0 = cr.i0, g0 = cg.i0, b0 = cb.i0;
    int const r1 = cr.i1, g1 = cg.i1, b1 = cb.i1;
    alwan_scalar const fr = cr.frac;
    alwan_scalar const fg = cg.frac;
    alwan_scalar const fb = cb.frac;

    alwan_scalar const *c000 = ALWAN_TABLE3D_AT_(cube, size, r0, g0, b0);
    alwan_scalar const *c111 = ALWAN_TABLE3D_AT_(cube, size, r1, g1, b1);
    alwan_scalar const *cP;
    alwan_scalar const *cQ;
    alwan_scalar w1, w2, w3;
    if (fr >= fg) {
        if (fg >= fb)      { cP = ALWAN_TABLE3D_AT_(cube, size, r1, g0, b0); cQ = ALWAN_TABLE3D_AT_(cube, size, r1, g1, b0); w1 = fr; w2 = fg; w3 = fb; }
        else if (fr >= fb) { cP = ALWAN_TABLE3D_AT_(cube, size, r1, g0, b0); cQ = ALWAN_TABLE3D_AT_(cube, size, r1, g0, b1); w1 = fr; w2 = fb; w3 = fg; }
        else               { cP = ALWAN_TABLE3D_AT_(cube, size, r0, g0, b1); cQ = ALWAN_TABLE3D_AT_(cube, size, r1, g0, b1); w1 = fb; w2 = fr; w3 = fg; }
    } else {
        if (fb >= fg)      { cP = ALWAN_TABLE3D_AT_(cube, size, r0, g0, b1); cQ = ALWAN_TABLE3D_AT_(cube, size, r0, g1, b1); w1 = fb; w2 = fg; w3 = fr; }
        else if (fb >= fr) { cP = ALWAN_TABLE3D_AT_(cube, size, r0, g1, b0); cQ = ALWAN_TABLE3D_AT_(cube, size, r0, g1, b1); w1 = fg; w2 = fb; w3 = fr; }
        else               { cP = ALWAN_TABLE3D_AT_(cube, size, r0, g1, b0); cQ = ALWAN_TABLE3D_AT_(cube, size, r1, g1, b0); w1 = fg; w2 = fr; w3 = fb; }
    }

    alwan_vec3 result;
    for (int ch = 0; ch < 3; ch++) {
        alwan_scalar v0 = c000[ch];
        result.v[ch] = v0
                     + w1 * (cP[ch]   - v0)
                     + w2 * (cQ[ch]   - cP[ch])
                     + w3 * (c111[ch] - cQ[ch]);
    }
    return result;
}

ALWAN_INLINE alwan_vec3 alwan_table3d_sample_nearest_v(
        alwan_scalar const *cube, int size, alwan_vec3 coord) {
    alwan_table_cell const cr = alwan_table_cell_v(coord.v[0], size);
    alwan_table_cell const cg = alwan_table_cell_v(coord.v[1], size);
    alwan_table_cell const cb = alwan_table_cell_v(coord.v[2], size);
    int const ri = (cr.frac < ALWAN_LITERAL(0.5)) ? cr.i0 : cr.i1;
    int const gi = (cg.frac < ALWAN_LITERAL(0.5)) ? cg.i0 : cg.i1;
    int const bi = (cb.frac < ALWAN_LITERAL(0.5)) ? cb.i0 : cb.i1;
    alwan_scalar const *c = ALWAN_TABLE3D_AT_(cube, size, ri, gi, bi);
    alwan_vec3 result;
    result.v[0] = c[0];
    result.v[1] = c[1];
    result.v[2] = c[2];
    return result;
}

ALWAN_INLINE alwan_vec3 alwan_table3d_sample_v(
        alwan_scalar const *cube, int size, alwan_vec3 coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return alwan_table3d_sample_nearest_v(cube, size, coord);
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_TETRAHEDRAL)
        return alwan_table3d_sample_tetrahedral_v(cube, size, coord);
    return alwan_table3d_sample_trilinear_v(cube, size, coord);
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

ALWAN_INLINE alwan_vec3 alwan_table2d_sample_trilinear_v(
        alwan_scalar const *strip, int size, alwan_vec3 coord) {
    alwan_table_cell const cr = alwan_table_cell_v(coord.v[0], size);
    alwan_table_cell const cg = alwan_table_cell_v(coord.v[1], size);
    alwan_table_cell const cb = alwan_table_cell_v(coord.v[2], size);

    alwan_scalar const fr = cr.frac;
    alwan_scalar const fg = cg.frac;
    alwan_scalar const fb = cb.frac;

    alwan_scalar const *c000 = ALWAN_TABLE2D_AT_(strip, size, cr.i0, cg.i0, cb.i0);
    alwan_scalar const *c100 = ALWAN_TABLE2D_AT_(strip, size, cr.i1, cg.i0, cb.i0);
    alwan_scalar const *c010 = ALWAN_TABLE2D_AT_(strip, size, cr.i0, cg.i1, cb.i0);
    alwan_scalar const *c110 = ALWAN_TABLE2D_AT_(strip, size, cr.i1, cg.i1, cb.i0);
    alwan_scalar const *c001 = ALWAN_TABLE2D_AT_(strip, size, cr.i0, cg.i0, cb.i1);
    alwan_scalar const *c101 = ALWAN_TABLE2D_AT_(strip, size, cr.i1, cg.i0, cb.i1);
    alwan_scalar const *c011 = ALWAN_TABLE2D_AT_(strip, size, cr.i0, cg.i1, cb.i1);
    alwan_scalar const *c111 = ALWAN_TABLE2D_AT_(strip, size, cr.i1, cg.i1, cb.i1);

    alwan_vec3 result;
    for (int ch = 0; ch < 3; ch++) {
        alwan_scalar c00 = c000[ch] * (ALWAN_LITERAL(1.0) - fr) + c100[ch] * fr;
        alwan_scalar c01 = c001[ch] * (ALWAN_LITERAL(1.0) - fr) + c101[ch] * fr;
        alwan_scalar c10 = c010[ch] * (ALWAN_LITERAL(1.0) - fr) + c110[ch] * fr;
        alwan_scalar c11 = c011[ch] * (ALWAN_LITERAL(1.0) - fr) + c111[ch] * fr;

        alwan_scalar c0 = c00 * (ALWAN_LITERAL(1.0) - fg) + c10 * fg;
        alwan_scalar c1 = c01 * (ALWAN_LITERAL(1.0) - fg) + c11 * fg;

        result.v[ch] = c0 * (ALWAN_LITERAL(1.0) - fb) + c1 * fb;
    }
    return result;
}

ALWAN_INLINE alwan_vec3 alwan_table2d_sample_nearest_v(
        alwan_scalar const *strip, int size, alwan_vec3 coord) {
    alwan_table_cell const cr = alwan_table_cell_v(coord.v[0], size);
    alwan_table_cell const cg = alwan_table_cell_v(coord.v[1], size);
    alwan_table_cell const cb = alwan_table_cell_v(coord.v[2], size);
    int const ri = (cr.frac < ALWAN_LITERAL(0.5)) ? cr.i0 : cr.i1;
    int const gi = (cg.frac < ALWAN_LITERAL(0.5)) ? cg.i0 : cg.i1;
    int const bi = (cb.frac < ALWAN_LITERAL(0.5)) ? cb.i0 : cb.i1;
    alwan_scalar const *c = ALWAN_TABLE2D_AT_(strip, size, ri, gi, bi);
    alwan_vec3 result;
    result.v[0] = c[0];
    result.v[1] = c[1];
    result.v[2] = c[2];
    return result;
}

ALWAN_INLINE alwan_vec3 alwan_table2d_sample_v(
        alwan_scalar const *strip, int size, alwan_vec3 coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return alwan_table2d_sample_nearest_v(strip, size, coord);
    return alwan_table2d_sample_trilinear_v(strip, size, coord);
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

ALWAN_INLINE alwan_vec3 alwan_table3d_planar3_sample_trilinear_v(
        alwan_scalar const *cube0, alwan_scalar const *cube1, alwan_scalar const *cube2,
        int size, alwan_vec3 coord) {
    alwan_table_cell const cr = alwan_table_cell_v(coord.v[0], size);
    alwan_table_cell const cg = alwan_table_cell_v(coord.v[1], size);
    alwan_table_cell const cb = alwan_table_cell_v(coord.v[2], size);

    alwan_scalar const fr = cr.frac;
    alwan_scalar const fg = cg.frac;
    alwan_scalar const fb = cb.frac;

    size_t const i000 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i0, cg.i0, cb.i0);
    size_t const i001 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i0, cg.i0, cb.i1);
    size_t const i010 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i0, cg.i1, cb.i0);
    size_t const i011 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i0, cg.i1, cb.i1);
    size_t const i100 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i1, cg.i0, cb.i0);
    size_t const i101 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i1, cg.i0, cb.i1);
    size_t const i110 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i1, cg.i1, cb.i0);
    size_t const i111 = ALWAN_TABLE3D_PLANAR_AT_(size, cr.i1, cg.i1, cb.i1);

    alwan_scalar const one = ALWAN_LITERAL(1.0);
    alwan_vec3 result;

    alwan_scalar const a_00 = cube0[i000] * (one - fb) + cube0[i001] * fb;
    alwan_scalar const a_01 = cube0[i010] * (one - fb) + cube0[i011] * fb;
    alwan_scalar const a_10 = cube0[i100] * (one - fb) + cube0[i101] * fb;
    alwan_scalar const a_11 = cube0[i110] * (one - fb) + cube0[i111] * fb;
    alwan_scalar const a_0 = a_00 * (one - fg) + a_01 * fg;
    alwan_scalar const a_1 = a_10 * (one - fg) + a_11 * fg;
    result.v[0] = a_0 * (one - fr) + a_1 * fr;

    alwan_scalar const b_00 = cube1[i000] * (one - fb) + cube1[i001] * fb;
    alwan_scalar const b_01 = cube1[i010] * (one - fb) + cube1[i011] * fb;
    alwan_scalar const b_10 = cube1[i100] * (one - fb) + cube1[i101] * fb;
    alwan_scalar const b_11 = cube1[i110] * (one - fb) + cube1[i111] * fb;
    alwan_scalar const b_0 = b_00 * (one - fg) + b_01 * fg;
    alwan_scalar const b_1 = b_10 * (one - fg) + b_11 * fg;
    result.v[1] = b_0 * (one - fr) + b_1 * fr;

    alwan_scalar const c_00 = cube2[i000] * (one - fb) + cube2[i001] * fb;
    alwan_scalar const c_01 = cube2[i010] * (one - fb) + cube2[i011] * fb;
    alwan_scalar const c_10 = cube2[i100] * (one - fb) + cube2[i101] * fb;
    alwan_scalar const c_11 = cube2[i110] * (one - fb) + cube2[i111] * fb;
    alwan_scalar const c_0 = c_00 * (one - fg) + c_01 * fg;
    alwan_scalar const c_1 = c_10 * (one - fg) + c_11 * fg;
    result.v[2] = c_0 * (one - fr) + c_1 * fr;

    return result;
}

ALWAN_INLINE alwan_vec3 alwan_table3d_planar3_sample_nearest_v(
        alwan_scalar const *cube0, alwan_scalar const *cube1, alwan_scalar const *cube2,
        int size, alwan_vec3 coord) {
    alwan_table_cell const cr = alwan_table_cell_v(coord.v[0], size);
    alwan_table_cell const cg = alwan_table_cell_v(coord.v[1], size);
    alwan_table_cell const cb = alwan_table_cell_v(coord.v[2], size);
    int const ri = (cr.frac < ALWAN_LITERAL(0.5)) ? cr.i0 : cr.i1;
    int const gi = (cg.frac < ALWAN_LITERAL(0.5)) ? cg.i0 : cg.i1;
    int const bi = (cb.frac < ALWAN_LITERAL(0.5)) ? cb.i0 : cb.i1;
    size_t const i = ALWAN_TABLE3D_PLANAR_AT_(size, ri, gi, bi);
    alwan_vec3 result;
    result.v[0] = cube0[i];
    result.v[1] = cube1[i];
    result.v[2] = cube2[i];
    return result;
}

ALWAN_INLINE alwan_vec3 alwan_table3d_planar3_sample_v(
        alwan_scalar const *cube0, alwan_scalar const *cube1, alwan_scalar const *cube2,
        int size, alwan_vec3 coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return alwan_table3d_planar3_sample_nearest_v(cube0, cube1, cube2, size, coord);
    return alwan_table3d_planar3_sample_trilinear_v(cube0, cube1, cube2, size, coord);
}
#endif /* ALWAN_BACKEND */

#endif /* ALWAN_TABLE_CORE_H */
