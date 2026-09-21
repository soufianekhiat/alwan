/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Table readers, written against an accessor instead of a pointer.
 * No include guard - included once per table.
 */

/* ================================================================
 * WHY THIS FILE EXISTS
 *
 * A reader is arithmetic on a handful of table elements. The arithmetic is the
 * same on every backend. What differs is how an element is REACHED: a pointer
 * in C, CUDA and OpenCL, a StructuredBuffer or a static array in HLSL, a
 * buffer block or a uniform array in GLSL. HLSL and GLSL have no pointer type,
 * so a reader that takes `const T *table` cannot be compiled by either, and
 * for a long time that kept this core and the two that include it out of every
 * shader.
 *
 * So the readers here never name a pointer. They read through
 * ALWAN_TABLE_READ(i), and whoever includes this file says what that means.
 *
 * POINTER BINDING. alwan_table_core.h defines ALWAN_TABLE_READER_POINTER and
 * includes this file on every backend that has pointers. That emits the
 * readers the library has always had, alwan_table1d_sample_linear_f64(table,
 * size, coord) and the rest, with the table as the leading parameter.
 *
 * NAMED BINDING. A shader defines a name and an accessor, then includes this
 * file, once per table:
 *
 *     StructuredBuffer<float> GradeLut : register(t1);
 *
 *     #define ALWAN_TABLE_NAME     grade
 *     #define ALWAN_TABLE_READ(i)  GradeLut[i]
 *     #include "core/alwan_table_reader.inc"
 *
 *     alwan_vec3 c = alwan_table3d_sample_tetrahedral_grade(33, rgb);
 *
 * The name is appended to every reader, and the table parameter is gone: the
 * accessor already says which table. Everything this file defines is undefined
 * again at the end, so the next table is another three lines.
 *
 * Three accessors, each optional, each enabling its own family:
 *     ALWAN_TABLE_READ(i)        scalar element   rank 1, rank 2, both cubes
 *     ALWAN_TABLE_READ_MAT3(i)   alwan_mat3x3     the rank 1 matrix ramp
 *     ALWAN_TABLE_READ_P0(i), _P1(i), _P2(i)      the three-plane cube
 *
 * The accessor is evaluated with an index already clamped by the gate in
 * alwan_table_core, under the same ALWAN_READ_DATA_NO_BOUND_CHECK switch, so
 * it needs no bounds check of its own. It may be evaluated more than once per
 * call and must have no side effects.
 *
 * Both bindings compile the SAME bodies, which is the point. There is no GPU
 * twin of a reader to drift from the C one.
 * ================================================================ */

#if defined(ALWAN_TABLE_READER_POINTER)

# define ALWAN_TABLE_FN_(base)        ALWAN_CORE_FNVLIT(base)
# define ALWAN_TABLE_PARAMS_          const ALWAN_CORE_T *table,
# define ALWAN_TABLE_ARGS_            table,
# define ALWAN_TABLE_READ(i)          table[i]
# define ALWAN_TABLE_MAT3_PARAMS_     const ALWAN_CORE_MAT3X3 *table,
# define ALWAN_TABLE_READ_MAT3(i)     table[i]
# define ALWAN_TABLE_PLANAR_PARAMS_   const ALWAN_CORE_T *cube0, const ALWAN_CORE_T *cube1, const ALWAN_CORE_T *cube2,
# define ALWAN_TABLE_PLANAR_ARGS_     cube0, cube1, cube2,
# define ALWAN_TABLE_READ_P0(i)       cube0[i]
# define ALWAN_TABLE_READ_P1(i)       cube1[i]
# define ALWAN_TABLE_READ_P2(i)       cube2[i]

#else

# if !defined(ALWAN_TABLE_NAME)
#  error "alwan_table_reader.inc: define ALWAN_TABLE_NAME and an ALWAN_TABLE_READ accessor before including it"
# endif
# define ALWAN_TABLE_PASTE2_(a, b)    a##_##b
# define ALWAN_TABLE_PASTE_(a, b)     ALWAN_TABLE_PASTE2_(a, b)
# define ALWAN_TABLE_FN_(base)        ALWAN_TABLE_PASTE_(base, ALWAN_TABLE_NAME)
# define ALWAN_TABLE_PARAMS_
# define ALWAN_TABLE_ARGS_
# define ALWAN_TABLE_MAT3_PARAMS_
# define ALWAN_TABLE_PLANAR_PARAMS_
# define ALWAN_TABLE_PLANAR_ARGS_

#endif

#if defined(ALWAN_TABLE_READ)

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
ALWAN_INLINE ALWAN_CORE_T ALWAN_TABLE_FN_(alwan_table1d_row)(
        ALWAN_TABLE_PARAMS_ int size, int index) {
    return ALWAN_TABLE_READ(ALWAN_CORE_FNVLIT(alwan_table_row)(index, size));
}

/* Weighted form, a*(1-f) + b*f. Matches alwan_lerp and the shipped
 * alwan_lut1d_sample bit for bit. This is ALWAN_SAMPLE_LINEAR. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_TABLE_FN_(alwan_table1d_sample_linear)(
        ALWAN_TABLE_PARAMS_ int size, ALWAN_CORE_T coord) {
    const ALWAN_CORE_TABLE_CELL c = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord, size);
    return ALWAN_CORE_FNVLIT(alwan_table_blend)(
        ALWAN_TABLE_READ(c.i0), ALWAN_TABLE_READ(c.i1), c.frac);
}

/* Delta form, a + f*(b-a). Algebraically the same line, not the same bits: one
 * rounding lands differently. The AgX contrast LUT has always been read this
 * way and its output is pinned by the determinism MD5s, so the spelling is
 * preserved rather than silently re-rounded. Same gate, same bounds
 * guarantee -- only the blend differs. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_TABLE_FN_(alwan_table1d_sample_linear_delta)(
        ALWAN_TABLE_PARAMS_ int size, ALWAN_CORE_T coord) {
    const ALWAN_CORE_TABLE_CELL c = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord, size);
    return ALWAN_CORE_FNVLIT(alwan_table_blend_delta)(
        ALWAN_TABLE_READ(c.i0), ALWAN_TABLE_READ(c.i1), c.frac);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_TABLE_FN_(alwan_table1d_sample_nearest)(
        ALWAN_TABLE_PARAMS_ int size, ALWAN_CORE_T coord) {
    const ALWAN_CORE_TABLE_CELL c = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord, size);
    return ALWAN_TABLE_READ(ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(c));
}

/* Catmull-Rom: the four-tap cubic through table[i-1 .. i+2]. The two outer
 * taps are clamped by alwan_table_row, so the end intervals see a doubled
 * endpoint. That is the standard clamped-edge form: the curve stays
 * interpolating at the ends and its slope there is one-sided. The kernel, and
 * the warning that it can overshoot, are on alwan_table_blend_catmull_rom. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_TABLE_FN_(alwan_table1d_sample_catmull_rom)(
        ALWAN_TABLE_PARAMS_ int size, ALWAN_CORE_T coord) {
    const ALWAN_CORE_TABLE_CELL c = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord, size);
    const int im1 = ALWAN_CORE_FNVLIT(alwan_table_row)(c.i0 - 1, size);
    const int ip2 = ALWAN_CORE_FNVLIT(alwan_table_row)(c.i1 + 1, size);
    return ALWAN_CORE_FNVLIT(alwan_table_blend_catmull_rom)(
        ALWAN_TABLE_READ(im1), ALWAN_TABLE_READ(c.i0),
        ALWAN_TABLE_READ(c.i1), ALWAN_TABLE_READ(ip2), c.frac);
}

/* Dispatch. The fallback returns the default mode for the rank: a core reader
 * has no status channel and must never leave the dispatch without a value, and
 * must stay free of error paths for the GPU backends. A mode the rank cannot
 * honour is rejected once, at the API tier, before any loop. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_TABLE_FN_(alwan_table1d_sample)(
        ALWAN_TABLE_PARAMS_ int size, ALWAN_CORE_T coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return ALWAN_TABLE_FN_(alwan_table1d_sample_nearest)(ALWAN_TABLE_ARGS_ size, coord);
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_CATMULL_ROM)
        return ALWAN_TABLE_FN_(alwan_table1d_sample_catmull_rom)(ALWAN_TABLE_ARGS_ size, coord);
    return ALWAN_TABLE_FN_(alwan_table1d_sample_linear)(ALWAN_TABLE_ARGS_ size, coord);
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

ALWAN_INLINE ALWAN_CORE_T ALWAN_TABLE_FN_(alwan_table2d_row_at)(
        ALWAN_TABLE_PARAMS_ int rows, int cols, int row, int col) {
    const int r = ALWAN_CORE_FNVLIT(alwan_table_row)(row, rows);
    const int c = ALWAN_CORE_FNVLIT(alwan_table_row)(col, cols);
    return ALWAN_TABLE_READ((size_t)r * (size_t)cols + (size_t)c);
}

/* Bilinear over the same rows x cols grid, both axes in [0,1].
 *
 * This is what ALWAN_SAMPLE_BILINEAR means in alwan: a genuine 2-d grid, not
 * the flattened cube in the strip readers below, which is sampled trilinearly
 * and still rejects BILINEAR.
 *
 * Row and column are separate axes with separate extents, so each gets its own
 * cell and both go through the same gate as the integer reader above. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_TABLE_FN_(alwan_table2d_grid_sample_bilinear)(
        ALWAN_TABLE_PARAMS_ int rows, int cols,
        ALWAN_CORE_T row_coord, ALWAN_CORE_T col_coord) {
    const ALWAN_CORE_TABLE_CELL cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(row_coord, rows);
    const ALWAN_CORE_TABLE_CELL cc = ALWAN_CORE_FNVLIT(alwan_table_cell)(col_coord, cols);
    const ALWAN_CORE_T v00 = ALWAN_TABLE_FN_(alwan_table2d_row_at)(ALWAN_TABLE_ARGS_ rows, cols, cr.i0, cc.i0);
    const ALWAN_CORE_T v01 = ALWAN_TABLE_FN_(alwan_table2d_row_at)(ALWAN_TABLE_ARGS_ rows, cols, cr.i0, cc.i1);
    const ALWAN_CORE_T v10 = ALWAN_TABLE_FN_(alwan_table2d_row_at)(ALWAN_TABLE_ARGS_ rows, cols, cr.i1, cc.i0);
    const ALWAN_CORE_T v11 = ALWAN_TABLE_FN_(alwan_table2d_row_at)(ALWAN_TABLE_ARGS_ rows, cols, cr.i1, cc.i1);
    const ALWAN_CORE_T top = ALWAN_CORE_FNVLIT(alwan_table_blend)(v00, v01, cc.frac);
    const ALWAN_CORE_T bot = ALWAN_CORE_FNVLIT(alwan_table_blend)(v10, v11, cc.frac);
    return ALWAN_CORE_FNVLIT(alwan_table_blend)(top, bot, cr.frac);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_TABLE_FN_(alwan_table2d_grid_sample_nearest)(
        ALWAN_TABLE_PARAMS_ int rows, int cols,
        ALWAN_CORE_T row_coord, ALWAN_CORE_T col_coord) {
    const ALWAN_CORE_TABLE_CELL cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(row_coord, rows);
    const ALWAN_CORE_TABLE_CELL cc = ALWAN_CORE_FNVLIT(alwan_table_cell)(col_coord, cols);
    return ALWAN_TABLE_FN_(alwan_table2d_row_at)(ALWAN_TABLE_ARGS_ rows, cols,
        ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(cr),
        ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(cc));
}

/* LINEAR resolves to bilinear here, the same way it resolves to trilinear at
 * rank 3: it is the zero value, so a zero-initialised mode must interpolate. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_TABLE_FN_(alwan_table2d_grid_sample)(
        ALWAN_TABLE_PARAMS_ int rows, int cols,
        ALWAN_CORE_T row_coord, ALWAN_CORE_T col_coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return ALWAN_TABLE_FN_(alwan_table2d_grid_sample_nearest)(ALWAN_TABLE_ARGS_ rows, cols, row_coord, col_coord);
    return ALWAN_TABLE_FN_(alwan_table2d_grid_sample_bilinear)(ALWAN_TABLE_ARGS_ rows, cols, row_coord, col_coord);
}

/* ================================================================
 * Rank 3, interleaved RGB cube, R-fastest
 * index = ((b*size + g)*size + r)*3 + channel
 * ================================================================ */

/* One channel of an interleaved node blend, over the corner indices the reader
 * has in scope. The three channels are written out rather than looped: legacy
 * HLSL cannot index an l-value by a loop counter, unrolls the loop itself, and
 * says so with warning X3550 in every shader that includes this file. */
#define ALWAN_TABLE_TRILINEAR_CH_(k) \
    ALWAN_CORE_FNVLIT(alwan_table_blend_trilinear)( \
        ALWAN_TABLE_READ(c000 + (k)), ALWAN_TABLE_READ(c100 + (k)), \
        ALWAN_TABLE_READ(c010 + (k)), ALWAN_TABLE_READ(c110 + (k)), \
        ALWAN_TABLE_READ(c001 + (k)), ALWAN_TABLE_READ(c101 + (k)), \
        ALWAN_TABLE_READ(c011 + (k)), ALWAN_TABLE_READ(c111 + (k)), \
        cr.frac, cg.frac, cb.frac)
#define ALWAN_TABLE_TETRAHEDRAL_CH_(k) \
    ALWAN_CORE_FNVLIT(alwan_table_blend_tetrahedral)( \
        ALWAN_TABLE_READ(c000 + (k)), ALWAN_TABLE_READ(cP + (k)), \
        ALWAN_TABLE_READ(cQ + (k)), ALWAN_TABLE_READ(c111 + (k)), w1, w2, w3)

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_TABLE_FN_(alwan_table3d_sample_trilinear)(
        ALWAN_TABLE_PARAMS_ int size, ALWAN_CORE_VEC3 coord) {
    const ALWAN_CORE_TABLE_CELL cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    const ALWAN_CORE_TABLE_CELL cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    const ALWAN_CORE_TABLE_CELL cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);

    const size_t c000 = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, cr.i0, cg.i0, cb.i0);
    const size_t c100 = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, cr.i1, cg.i0, cb.i0);
    const size_t c010 = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, cr.i0, cg.i1, cb.i0);
    const size_t c110 = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, cr.i1, cg.i1, cb.i0);
    const size_t c001 = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, cr.i0, cg.i0, cb.i1);
    const size_t c101 = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, cr.i1, cg.i0, cb.i1);
    const size_t c011 = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, cr.i0, cg.i1, cb.i1);
    const size_t c111 = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, cr.i1, cg.i1, cb.i1);

    ALWAN_CORE_VEC3 result;
    result.v[0] = ALWAN_TABLE_TRILINEAR_CH_(0);
    result.v[1] = ALWAN_TABLE_TRILINEAR_CH_(1);
    result.v[2] = ALWAN_TABLE_TRILINEAR_CH_(2);
    return result;
}

/* Tetrahedral: the ordering of (fr,fg,fb) picks the tetrahedron, giving two
 * intermediate corners and sorted weights w1 >= w2 >= w3. Matches OCIO, and
 * keeps the neutral axis symmetric, which trilinear does not. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_TABLE_FN_(alwan_table3d_sample_tetrahedral)(
        ALWAN_TABLE_PARAMS_ int size, ALWAN_CORE_VEC3 coord) {
    const ALWAN_CORE_TABLE_CELL cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    const ALWAN_CORE_TABLE_CELL cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    const ALWAN_CORE_TABLE_CELL cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);

    const int r0 = cr.i0, g0 = cg.i0, b0 = cb.i0;
    const int r1 = cr.i1, g1 = cg.i1, b1 = cb.i1;
    const ALWAN_CORE_T fr = cr.frac;
    const ALWAN_CORE_T fg = cg.frac;
    const ALWAN_CORE_T fb = cb.frac;

    const size_t c000 = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r0, g0, b0);
    const size_t c111 = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r1, g1, b1);
    size_t cP;
    size_t cQ;
    ALWAN_CORE_T w1, w2, w3;

    if (fr >= fg) {
        if (fg >= fb)      { cP = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r1, g0, b0); cQ = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r1, g1, b0); w1 = fr; w2 = fg; w3 = fb; }
        else if (fr >= fb) { cP = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r1, g0, b0); cQ = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r1, g0, b1); w1 = fr; w2 = fb; w3 = fg; }
        else               { cP = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r0, g0, b1); cQ = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r1, g0, b1); w1 = fb; w2 = fr; w3 = fg; }
    } else {
        if (fb >= fg)      { cP = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r0, g0, b1); cQ = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r0, g1, b1); w1 = fb; w2 = fg; w3 = fr; }
        else if (fb >= fr) { cP = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r0, g1, b0); cQ = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r0, g1, b1); w1 = fg; w2 = fb; w3 = fr; }
        else               { cP = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r0, g1, b0); cQ = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size, r1, g1, b0); w1 = fg; w2 = fr; w3 = fb; }
    }

    ALWAN_CORE_VEC3 result;
    result.v[0] = ALWAN_TABLE_TETRAHEDRAL_CH_(0);
    result.v[1] = ALWAN_TABLE_TETRAHEDRAL_CH_(1);
    result.v[2] = ALWAN_TABLE_TETRAHEDRAL_CH_(2);
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_TABLE_FN_(alwan_table3d_sample_nearest)(
        ALWAN_TABLE_PARAMS_ int size, ALWAN_CORE_VEC3 coord) {
    const ALWAN_CORE_TABLE_CELL cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    const ALWAN_CORE_TABLE_CELL cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    const ALWAN_CORE_TABLE_CELL cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);
    const size_t c = ALWAN_CORE_FNVLIT(alwan_table3d_index)(size,
        ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(cr),
        ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(cg),
        ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(cb));
    ALWAN_CORE_VEC3 result;
    result.v[0] = ALWAN_TABLE_READ(c);
    result.v[1] = ALWAN_TABLE_READ(c + 1);
    result.v[2] = ALWAN_TABLE_READ(c + 2);
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_TABLE_FN_(alwan_table3d_sample)(
        ALWAN_TABLE_PARAMS_ int size, ALWAN_CORE_VEC3 coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return ALWAN_TABLE_FN_(alwan_table3d_sample_nearest)(ALWAN_TABLE_ARGS_ size, coord);
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_TETRAHEDRAL)
        return ALWAN_TABLE_FN_(alwan_table3d_sample_tetrahedral)(ALWAN_TABLE_ARGS_ size, coord);
    return ALWAN_TABLE_FN_(alwan_table3d_sample_trilinear)(ALWAN_TABLE_ARGS_ size, coord);
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

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_TABLE_FN_(alwan_table2d_sample_trilinear)(
        ALWAN_TABLE_PARAMS_ int size, ALWAN_CORE_VEC3 coord) {
    const ALWAN_CORE_TABLE_CELL cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    const ALWAN_CORE_TABLE_CELL cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    const ALWAN_CORE_TABLE_CELL cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);

    const size_t c000 = ALWAN_CORE_FNVLIT(alwan_table2d_strip_index)(size, cr.i0, cg.i0, cb.i0);
    const size_t c100 = ALWAN_CORE_FNVLIT(alwan_table2d_strip_index)(size, cr.i1, cg.i0, cb.i0);
    const size_t c010 = ALWAN_CORE_FNVLIT(alwan_table2d_strip_index)(size, cr.i0, cg.i1, cb.i0);
    const size_t c110 = ALWAN_CORE_FNVLIT(alwan_table2d_strip_index)(size, cr.i1, cg.i1, cb.i0);
    const size_t c001 = ALWAN_CORE_FNVLIT(alwan_table2d_strip_index)(size, cr.i0, cg.i0, cb.i1);
    const size_t c101 = ALWAN_CORE_FNVLIT(alwan_table2d_strip_index)(size, cr.i1, cg.i0, cb.i1);
    const size_t c011 = ALWAN_CORE_FNVLIT(alwan_table2d_strip_index)(size, cr.i0, cg.i1, cb.i1);
    const size_t c111 = ALWAN_CORE_FNVLIT(alwan_table2d_strip_index)(size, cr.i1, cg.i1, cb.i1);

    ALWAN_CORE_VEC3 result;
    result.v[0] = ALWAN_TABLE_TRILINEAR_CH_(0);
    result.v[1] = ALWAN_TABLE_TRILINEAR_CH_(1);
    result.v[2] = ALWAN_TABLE_TRILINEAR_CH_(2);
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_TABLE_FN_(alwan_table2d_sample_nearest)(
        ALWAN_TABLE_PARAMS_ int size, ALWAN_CORE_VEC3 coord) {
    const ALWAN_CORE_TABLE_CELL cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    const ALWAN_CORE_TABLE_CELL cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    const ALWAN_CORE_TABLE_CELL cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);
    const size_t c = ALWAN_CORE_FNVLIT(alwan_table2d_strip_index)(size,
        ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(cr),
        ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(cg),
        ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(cb));
    ALWAN_CORE_VEC3 result;
    result.v[0] = ALWAN_TABLE_READ(c);
    result.v[1] = ALWAN_TABLE_READ(c + 1);
    result.v[2] = ALWAN_TABLE_READ(c + 2);
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_TABLE_FN_(alwan_table2d_sample)(
        ALWAN_TABLE_PARAMS_ int size, ALWAN_CORE_VEC3 coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return ALWAN_TABLE_FN_(alwan_table2d_sample_nearest)(ALWAN_TABLE_ARGS_ size, coord);
    return ALWAN_TABLE_FN_(alwan_table2d_sample_trilinear)(ALWAN_TABLE_ARGS_ size, coord);
}

#undef ALWAN_TABLE_TRILINEAR_CH_
#undef ALWAN_TABLE_TETRAHEDRAL_CH_

#endif /* ALWAN_TABLE_READ */

#if defined(ALWAN_TABLE_READ_MAT3)

/* ================================================================
 * Rank 1, mat3x3 element (CVD severity ramps)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_TABLE_FN_(alwan_table1d_mat3_sample_linear)(
        ALWAN_TABLE_MAT3_PARAMS_ int size, ALWAN_CORE_T coord) {
    const ALWAN_CORE_TABLE_CELL c = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord, size);
    return ALWAN_CORE_FNVLIT(alwan_table_blend_mat3)(
        ALWAN_TABLE_READ_MAT3(c.i0), ALWAN_TABLE_READ_MAT3(c.i1), c.frac);
}

ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_TABLE_FN_(alwan_table1d_mat3_sample_nearest)(
        ALWAN_TABLE_MAT3_PARAMS_ int size, ALWAN_CORE_T coord) {
    const ALWAN_CORE_TABLE_CELL c = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord, size);
    return ALWAN_TABLE_READ_MAT3(ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(c));
}

/* Not CATMULL_ROM: the reference CVD model defines linear severity
 * interpolation and a 4-tap kernel would change published output. */
ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_TABLE_FN_(alwan_table1d_mat3_sample)(
        ALWAN_TABLE_MAT3_PARAMS_ int size, ALWAN_CORE_T coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return ALWAN_TABLE_FN_(alwan_table1d_mat3_sample_nearest)(ALWAN_TABLE_ARGS_ size, coord);
    return ALWAN_TABLE_FN_(alwan_table1d_mat3_sample_linear)(ALWAN_TABLE_ARGS_ size, coord);
}

#endif /* ALWAN_TABLE_READ_MAT3 */

#if defined(ALWAN_TABLE_READ_P0) && defined(ALWAN_TABLE_READ_P1) && defined(ALWAN_TABLE_READ_P2)

/* ================================================================
 * Rank 3, three PLANAR scalar cubes sharing one coordinate, B-fastest
 * index = (r*size + g)*size + b
 *
 * This is the Jakob 2019 coefficient layout: three independent scalar fields,
 * not one interleaved RGB cube, and the fastest axis is b rather than r. A
 * separate reader rather than a flag on alwan_table3d_sample, because a
 * layout mismatch that compiles is exactly the trap this file exists to close.
 *
 * The blend runs b, then g, then r: fastest axis first, as it always has here.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_TABLE_FN_(alwan_table3d_planar3_sample_trilinear)(
        ALWAN_TABLE_PLANAR_PARAMS_ int size, ALWAN_CORE_VEC3 coord) {
    const ALWAN_CORE_TABLE_CELL cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    const ALWAN_CORE_TABLE_CELL cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    const ALWAN_CORE_TABLE_CELL cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);

    const size_t i000 = ALWAN_CORE_FNVLIT(alwan_table3d_planar_index)(size, cr.i0, cg.i0, cb.i0);
    const size_t i001 = ALWAN_CORE_FNVLIT(alwan_table3d_planar_index)(size, cr.i0, cg.i0, cb.i1);
    const size_t i010 = ALWAN_CORE_FNVLIT(alwan_table3d_planar_index)(size, cr.i0, cg.i1, cb.i0);
    const size_t i011 = ALWAN_CORE_FNVLIT(alwan_table3d_planar_index)(size, cr.i0, cg.i1, cb.i1);
    const size_t i100 = ALWAN_CORE_FNVLIT(alwan_table3d_planar_index)(size, cr.i1, cg.i0, cb.i0);
    const size_t i101 = ALWAN_CORE_FNVLIT(alwan_table3d_planar_index)(size, cr.i1, cg.i0, cb.i1);
    const size_t i110 = ALWAN_CORE_FNVLIT(alwan_table3d_planar_index)(size, cr.i1, cg.i1, cb.i0);
    const size_t i111 = ALWAN_CORE_FNVLIT(alwan_table3d_planar_index)(size, cr.i1, cg.i1, cb.i1);

    ALWAN_CORE_VEC3 result;
    result.v[0] = ALWAN_CORE_FNVLIT(alwan_table_blend_trilinear)(
        ALWAN_TABLE_READ_P0(i000), ALWAN_TABLE_READ_P0(i001),
        ALWAN_TABLE_READ_P0(i010), ALWAN_TABLE_READ_P0(i011),
        ALWAN_TABLE_READ_P0(i100), ALWAN_TABLE_READ_P0(i101),
        ALWAN_TABLE_READ_P0(i110), ALWAN_TABLE_READ_P0(i111),
        cb.frac, cg.frac, cr.frac);
    result.v[1] = ALWAN_CORE_FNVLIT(alwan_table_blend_trilinear)(
        ALWAN_TABLE_READ_P1(i000), ALWAN_TABLE_READ_P1(i001),
        ALWAN_TABLE_READ_P1(i010), ALWAN_TABLE_READ_P1(i011),
        ALWAN_TABLE_READ_P1(i100), ALWAN_TABLE_READ_P1(i101),
        ALWAN_TABLE_READ_P1(i110), ALWAN_TABLE_READ_P1(i111),
        cb.frac, cg.frac, cr.frac);
    result.v[2] = ALWAN_CORE_FNVLIT(alwan_table_blend_trilinear)(
        ALWAN_TABLE_READ_P2(i000), ALWAN_TABLE_READ_P2(i001),
        ALWAN_TABLE_READ_P2(i010), ALWAN_TABLE_READ_P2(i011),
        ALWAN_TABLE_READ_P2(i100), ALWAN_TABLE_READ_P2(i101),
        ALWAN_TABLE_READ_P2(i110), ALWAN_TABLE_READ_P2(i111),
        cb.frac, cg.frac, cr.frac);
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_TABLE_FN_(alwan_table3d_planar3_sample_nearest)(
        ALWAN_TABLE_PLANAR_PARAMS_ int size, ALWAN_CORE_VEC3 coord) {
    const ALWAN_CORE_TABLE_CELL cr = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[0], size);
    const ALWAN_CORE_TABLE_CELL cg = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[1], size);
    const ALWAN_CORE_TABLE_CELL cb = ALWAN_CORE_FNVLIT(alwan_table_cell)(coord.v[2], size);
    const size_t i = ALWAN_CORE_FNVLIT(alwan_table3d_planar_index)(size,
        ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(cr),
        ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(cg),
        ALWAN_CORE_FNVLIT(alwan_table_cell_nearest)(cb));
    ALWAN_CORE_VEC3 result;
    result.v[0] = ALWAN_TABLE_READ_P0(i);
    result.v[1] = ALWAN_TABLE_READ_P1(i);
    result.v[2] = ALWAN_TABLE_READ_P2(i);
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_TABLE_FN_(alwan_table3d_planar3_sample)(
        ALWAN_TABLE_PLANAR_PARAMS_ int size, ALWAN_CORE_VEC3 coord, alwan_sample_mode mode) {
    if (ALWAN_SAMPLE_BASE(mode) == ALWAN_SAMPLE_NEAREST)
        return ALWAN_TABLE_FN_(alwan_table3d_planar3_sample_nearest)(ALWAN_TABLE_PLANAR_ARGS_ size, coord);
    return ALWAN_TABLE_FN_(alwan_table3d_planar3_sample_trilinear)(ALWAN_TABLE_PLANAR_ARGS_ size, coord);
}

#endif /* ALWAN_TABLE_READ_P0..P2 */

/* Leave nothing behind, so the next table can bind its own. */
#undef ALWAN_TABLE_FN_
#undef ALWAN_TABLE_PARAMS_
#undef ALWAN_TABLE_ARGS_
#undef ALWAN_TABLE_MAT3_PARAMS_
#undef ALWAN_TABLE_PLANAR_PARAMS_
#undef ALWAN_TABLE_PLANAR_ARGS_
#if defined(ALWAN_TABLE_PASTE_)
# undef ALWAN_TABLE_PASTE_
# undef ALWAN_TABLE_PASTE2_
#endif
#if defined(ALWAN_TABLE_READ)
# undef ALWAN_TABLE_READ
#endif
#if defined(ALWAN_TABLE_READ_MAT3)
# undef ALWAN_TABLE_READ_MAT3
#endif
#if defined(ALWAN_TABLE_READ_P0)
# undef ALWAN_TABLE_READ_P0
#endif
#if defined(ALWAN_TABLE_READ_P1)
# undef ALWAN_TABLE_READ_P1
#endif
#if defined(ALWAN_TABLE_READ_P2)
# undef ALWAN_TABLE_READ_P2
#endif
#if defined(ALWAN_TABLE_NAME)
# undef ALWAN_TABLE_NAME
#endif
#if defined(ALWAN_TABLE_READER_POINTER)
# undef ALWAN_TABLE_READER_POINTER
#endif
