/* ================================================================
 * Alwan - Color Correction & Grading
 * Per-pixel math in alwan_color_correction_core.h
 *
 * Only enum dispatch, pointer validation, memory allocation,
 * and loop-based solvers live here.
 * ================================================================ */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_color_correction_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Lift/Gamma/Gain (LGG)
 * ================================================================ */

void alwan_lgg_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in, alwan_rgb_f32 const *lift,
                    alwan_rgb_f32 const *gamma, alwan_rgb_f32 const *gain)
{
    if (!rgb_out || !rgb_in || !lift || !gamma || !gain) {
        return;
    }

    *rgb_out = alwan_lgg_apply_f32_v(*rgb_in, *lift, *gamma, *gain);
}

void alwan_lgg_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in, alwan_rgb_f64 const *lift,
                    alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain)
{
    if (!rgb_out || !rgb_in || !lift || !gamma || !gain) {
        return;
    }

    *rgb_out = alwan_lgg_apply_f64_v(*rgb_in, *lift, *gamma, *gain);
}

/* ================================================================
 * Color Matrix Grading
 * ================================================================ */

void alwan_color_matrix_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in,
                              alwan_mat3x3_f32 const *matrix_3x3)
{
    if (!rgb_out || !rgb_in || !matrix_3x3) {
        return;
    }

    *rgb_out = alwan_color_matrix_apply_f32_v(*rgb_in, *matrix_3x3);
}

void alwan_color_matrix_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in,
                              alwan_mat3x3_f64 const *matrix_3x3)
{
    if (!rgb_out || !rgb_in || !matrix_3x3) {
        return;
    }

    *rgb_out = alwan_color_matrix_apply_f64_v(*rgb_in, *matrix_3x3);
}

int alwan_color_matrix_get_preset_f64(alwan_mat3x3_f64 *matrix_3x3, alwan_color_matrix_preset_f64 preset)
{
    if (!matrix_3x3) {
        return ALWAN_E_INVALID;
    }

    /* Preset matrices for common color grading looks.
     * All matrices below (except Sepia) are artistic presets with no published
     * source; values are chosen for plausible visual effect and should be
     * treated as implementation-defined defaults, not industry standards.
     *
     * Sepia: classic formula widely published in image processing literature
     *   (e.g. Microsoft Imaging documentation, CSS filter drafts). Values:
     *   R' = 0.393R + 0.769G + 0.189B
     *   G' = 0.349R + 0.686G + 0.168B
     *   B' = 0.272R + 0.534G + 0.131B
     *
     * Monochrome: BT.601 luma weights (Kr=0.299, Kg=0.587, Kb=0.114)
     *   per ITU-R BT.601-7. All channels set to luma value. */
    switch (preset) {
        case ALWAN_COLOR_MATRIX_SEPIA:
            /* Sepia tone: warm brown/yellow tint (classic formula) */
            matrix_3x3->m[0] = 0.393; matrix_3x3->m[1] = 0.769; matrix_3x3->m[2] = 0.189;
            matrix_3x3->m[3] = 0.349; matrix_3x3->m[4] = 0.686; matrix_3x3->m[5] = 0.168;
            matrix_3x3->m[6] = 0.272; matrix_3x3->m[7] = 0.534; matrix_3x3->m[8] = 0.131;
            break;

        case ALWAN_COLOR_MATRIX_VINTAGE:
            /* Vintage: reduced saturation, warm shift (artistic preset) */
            matrix_3x3->m[0] = 0.9; matrix_3x3->m[1] = 0.1; matrix_3x3->m[2] = 0.1;
            matrix_3x3->m[3] = 0.1; matrix_3x3->m[4] = 0.8; matrix_3x3->m[5] = 0.0;
            matrix_3x3->m[6] = 0.0; matrix_3x3->m[7] = 0.1; matrix_3x3->m[8] = 0.7;
            break;

        case ALWAN_COLOR_MATRIX_BLEACH_BYPASS:
            /* Bleach bypass: high contrast, reduced saturation (artistic preset) */
            matrix_3x3->m[0] = 1.2; matrix_3x3->m[1] = 0.2; matrix_3x3->m[2] = 0.0;
            matrix_3x3->m[3] = 0.1; matrix_3x3->m[4] = 1.1; matrix_3x3->m[5] = 0.1;
            matrix_3x3->m[6] = 0.0; matrix_3x3->m[7] = 0.2; matrix_3x3->m[8] = 1.0;
            break;

        case ALWAN_COLOR_MATRIX_COOL:
            /* Cool tone: blue shift (artistic preset) */
            matrix_3x3->m[0] = 0.8; matrix_3x3->m[1] = 0.0; matrix_3x3->m[2] = 0.2;
            matrix_3x3->m[3] = 0.0; matrix_3x3->m[4] = 0.9; matrix_3x3->m[5] = 0.1;
            matrix_3x3->m[6] = 0.0; matrix_3x3->m[7] = 0.0; matrix_3x3->m[8] = 1.2;
            break;

        case ALWAN_COLOR_MATRIX_WARM:
            /* Warm tone: red/yellow shift (artistic preset) */
            matrix_3x3->m[0] = 1.2; matrix_3x3->m[1] = 0.1; matrix_3x3->m[2] = 0.0;
            matrix_3x3->m[3] = 0.1; matrix_3x3->m[4] = 1.1; matrix_3x3->m[5] = 0.0;
            matrix_3x3->m[6] = 0.0; matrix_3x3->m[7] = 0.1; matrix_3x3->m[8] = 0.7;
            break;

        case ALWAN_COLOR_MATRIX_MONOCHROME:
            /* Black and white: BT.601 luma weights Kr=0.299, Kg=0.587, Kb=0.114
             * per ITU-R BT.601-7; all channels equal to luma. */
            matrix_3x3->m[0] = 0.299; matrix_3x3->m[1] = 0.587; matrix_3x3->m[2] = 0.114;
            matrix_3x3->m[3] = 0.299; matrix_3x3->m[4] = 0.587; matrix_3x3->m[5] = 0.114;
            matrix_3x3->m[6] = 0.299; matrix_3x3->m[7] = 0.587; matrix_3x3->m[8] = 0.114;
            break;

        case ALWAN_COLOR_MATRIX_NIGHT_VISION:
            /* Night vision: green monochrome (artistic preset) */
            matrix_3x3->m[0] = 0.0; matrix_3x3->m[1] = 1.0; matrix_3x3->m[2] = 0.0;
            matrix_3x3->m[3] = 0.0; matrix_3x3->m[4] = 1.0; matrix_3x3->m[5] = 0.0;
            matrix_3x3->m[6] = 0.0; matrix_3x3->m[7] = 1.0; matrix_3x3->m[8] = 0.0;
            break;

        default:
            return ALWAN_E_INVALID;
    }

    return ALWAN_OK;
}

/* ================================================================
 * Printer Lights
 * ================================================================ */

void alwan_printer_lights_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in,
                                alwan_f32 red_lights, alwan_f32 green_lights,
                                alwan_f32 blue_lights)
{
    if (!rgb_out || !rgb_in) {
        return;
    }

    *rgb_out = alwan_printer_lights_apply_f32_v(*rgb_in, red_lights, green_lights, blue_lights);
}

void alwan_printer_lights_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in,
                                alwan_f64 red_lights, alwan_f64 green_lights,
                                alwan_f64 blue_lights)
{
    if (!rgb_out || !rgb_in) {
        return;
    }

    *rgb_out = alwan_printer_lights_apply_f64_v(*rgb_in, red_lights, green_lights, blue_lights);
}

/* ================================================================
 * Polynomial Color Correction - Cheung 2004, Finlayson 2015, Vandermonde
 * Reference: colour-science implementation for exact term ordering
 * ================================================================ */

int alwan_poly_expand_cheung2004_f64(alwan_f64 *out, alwan_rgb_f64 const *rgb,
                                  alwan_poly_cheung_terms terms)
{
    if (!rgb || !out) {
        return ALWAN_E_INVALID;
    }

    alwan_f64 R = rgb->r;
    alwan_f64 G = rgb->g;
    alwan_f64 B = rgb->b;

    /* Pre-compute common products */
    alwan_f64 RG = R * G;
    alwan_f64 RB = R * B;
    alwan_f64 GB = G * B;
    alwan_f64 RGB = R * G * B;
    alwan_f64 R2 = R * R;
    alwan_f64 G2 = G * G;
    alwan_f64 B2 = B * B;
    alwan_f64 R3 = R2 * R;
    alwan_f64 G3 = G2 * G;
    alwan_f64 B3 = B2 * B;
    alwan_f64 R4 = R2 * R2;
    alwan_f64 G4 = G2 * G2;
    alwan_f64 B4 = B2 * B2;

    /* Term ordering matches colour-science exactly */
    switch (terms) {
        case ALWAN_POLY_CHEUNG_3:
            /* [R, G, B] */
            out[0] = R; out[1] = G; out[2] = B;
            break;

        case ALWAN_POLY_CHEUNG_4:
            /* [R, G, B, 1] */
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = 1.0;
            break;

        case ALWAN_POLY_CHEUNG_5:
            /* [R, G, B, RGB, 1] */
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RGB;
            out[4] = 1.0;
            break;

        case ALWAN_POLY_CHEUNG_7:
            /* [R, G, B, RG, RB, GB, 1] */
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = 1.0;
            break;

        case ALWAN_POLY_CHEUNG_8:
            /* [R, G, B, RG, RB, GB, RGB, 1] */
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = RGB;
            out[7] = 1.0;
            break;

        case ALWAN_POLY_CHEUNG_10:
            /* [R, G, B, RG, RB, GB, R^2, G^2, B^2, 1] */
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = 1.0;
            break;

        case ALWAN_POLY_CHEUNG_11:
            /* [R, G, B, RG, RB, GB, R^2, G^2, B^2, RGB, 1] */
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = 1.0;
            break;

        case ALWAN_POLY_CHEUNG_14:
            /* [R, G, B, RG, RB, GB, R^2, G^2, B^2, RGB, R^3, G^3, B^3, 1] */
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = R3; out[11] = G3; out[12] = B3;
            out[13] = 1.0;
            break;

        case ALWAN_POLY_CHEUNG_16:
            /* [R, G, B, RG, RB, GB, R^2, G^2, B^2, RGB, R^2G, G^2B, RB^2, R^3, G^3, B^3] */
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = R2 * G; out[11] = G2 * B; out[12] = R * B2;
            out[13] = R3; out[14] = G3; out[15] = B3;
            break;

        case ALWAN_POLY_CHEUNG_17:
            /* [R, G, B, RG, RB, GB, R^2, G^2, B^2, RGB, R^2G, G^2B, RB^2, R^3, G^3, B^3, 1] */
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = R2 * G; out[11] = G2 * B; out[12] = R * B2;
            out[13] = R3; out[14] = G3; out[15] = B3;
            out[16] = 1.0;
            break;

        case ALWAN_POLY_CHEUNG_19:
            /* [R, G, B, RG, RB, GB, R^2, G^2, B^2, RGB, R^2G, G^2B, RB^2, R^2B, RG^2, GB^2, R^3, G^3, B^3] */
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = R2 * G; out[11] = G2 * B; out[12] = R * B2;
            out[13] = R2 * B; out[14] = R * G2; out[15] = G * B2;
            out[16] = R3; out[17] = G3; out[18] = B3;
            break;

        case ALWAN_POLY_CHEUNG_20:
            /* Same as 19 + 1 */
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = R2 * G; out[11] = G2 * B; out[12] = R * B2;
            out[13] = R2 * B; out[14] = R * G2; out[15] = G * B2;
            out[16] = R3; out[17] = G3; out[18] = B3;
            out[19] = 1.0;
            break;

        case ALWAN_POLY_CHEUNG_22:
            /* [R, G, B, RG, RB, GB, R^2, G^2, B^2, RGB, R^2G, G^2B, RB^2, R^2B, RG^2, GB^2, R^3, G^3, B^3, R^2GB, RG^2B, RGB^2] */
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = R2 * G; out[11] = G2 * B; out[12] = R * B2;
            out[13] = R2 * B; out[14] = R * G2; out[15] = G * B2;
            out[16] = R3; out[17] = G3; out[18] = B3;
            out[19] = R2 * G * B; out[20] = R * G2 * B; out[21] = R * G * B2;
            break;

        case ALWAN_POLY_CHEUNG_35:
            /* Full 35-term expansion */
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = R2 * G; out[11] = G2 * B; out[12] = R * B2;
            out[13] = R2 * B; out[14] = R * G2; out[15] = G * B2;
            out[16] = R3; out[17] = G3; out[18] = B3;
            out[19] = R3 * G; out[20] = R3 * B; out[21] = R * G3;
            out[22] = G3 * B; out[23] = R * B3; out[24] = G * B3;
            out[25] = R2 * G * B; out[26] = R * G2 * B; out[27] = R * G * B2;
            out[28] = R2 * G2; out[29] = R2 * B2; out[30] = G2 * B2;
            out[31] = R4; out[32] = G4; out[33] = B4;
            out[34] = 1.0;
            break;

        default:
            return ALWAN_E_INVALID;
    }

    return ALWAN_OK;
}

int alwan_poly_expand_finlayson2015_f64(alwan_f64 *out, int *out_size,
                                     alwan_rgb_f64 const *rgb, int degree, int root_poly)
{
    if (!rgb || !out || !out_size) {
        return ALWAN_E_INVALID;
    }

    if (degree < 1 || degree > 4) {
        return ALWAN_E_INVALID;
    }

    alwan_f64 R = rgb->r;
    alwan_f64 G = rgb->g;
    alwan_f64 B = rgb->b;

    if (root_poly) {
        /* Root-polynomial expansion: (RGB)^(1/d) for each degree d */
        /* Sizes: degree 1=3, degree 2=6, degree 3=13, degree 4=22 */
        int idx = 0;

        /* Degree 1: [R, G, B] */
        out[idx++] = R;
        out[idx++] = G;
        out[idx++] = B;

        if (degree >= 2) {
            /* Degree 2 root: sqrt of products */
            out[idx++] = ALWAN_SQRT(R * G);
            out[idx++] = ALWAN_SQRT(G * B);
            out[idx++] = ALWAN_SQRT(R * B);
        }

        if (degree >= 3) {
            /* Degree 3 root: cube root of products (ordering matches colour-science) */
            out[idx++] = ALWAN_POW(G * G * R, 1.0 / 3.0);  /* cbrt(G^2R) */
            out[idx++] = ALWAN_POW(B * B * G, 1.0 / 3.0);  /* cbrt(B^2G) */
            out[idx++] = ALWAN_POW(B * B * R, 1.0 / 3.0);  /* cbrt(B^2R) */
            out[idx++] = ALWAN_POW(R * R * G, 1.0 / 3.0);  /* cbrt(R^2G) */
            out[idx++] = ALWAN_POW(G * G * B, 1.0 / 3.0);  /* cbrt(G^2B) */
            out[idx++] = ALWAN_POW(R * R * B, 1.0 / 3.0);  /* cbrt(R^2B) */
            out[idx++] = ALWAN_POW(R * G * B, 1.0 / 3.0);  /* cbrt(RGB) */
        }

        if (degree >= 4) {
            /* Degree 4 root: fourth root of products (ordering matches colour-science) */
            out[idx++] = ALWAN_POW(R * R * R * G, 0.25);  /* qrt(R^3G) */
            out[idx++] = ALWAN_POW(R * R * R * B, 0.25);  /* qrt(R^3B) */
            out[idx++] = ALWAN_POW(G * G * G * R, 0.25);  /* qrt(G^3R) */
            out[idx++] = ALWAN_POW(G * G * G * B, 0.25);  /* qrt(G^3B) */
            out[idx++] = ALWAN_POW(B * B * B * R, 0.25);  /* qrt(B^3R) */
            out[idx++] = ALWAN_POW(B * B * B * G, 0.25);  /* qrt(B^3G) */
            out[idx++] = ALWAN_POW(R * R * G * B, 0.25);  /* qrt(R^2GB) */
            out[idx++] = ALWAN_POW(G * G * R * B, 0.25);  /* qrt(G^2RB) */
            out[idx++] = ALWAN_POW(B * B * R * G, 0.25);  /* qrt(B^2RG) */
        }

        *out_size = idx;
    } else {
        /* Standard polynomial expansion */
        /* Sizes: degree 1=3, degree 2=9, degree 3=19, degree 4=34 */
        int idx = 0;

        /* Degree 1: [R, G, B] */
        out[idx++] = R;
        out[idx++] = G;
        out[idx++] = B;

        if (degree >= 2) {
            /* Degree 2: squares and cross products */
            out[idx++] = R * R;
            out[idx++] = G * G;
            out[idx++] = B * B;
            out[idx++] = R * G;
            out[idx++] = G * B;
            out[idx++] = R * B;
        }

        if (degree >= 3) {
            /* Degree 3: cubes and mixed (ordering matches colour-science) */
            out[idx++] = R * R * R;
            out[idx++] = G * G * G;
            out[idx++] = B * B * B;
            out[idx++] = G * G * R;  /* G^2R */
            out[idx++] = B * B * G;  /* B^2G */
            out[idx++] = B * B * R;  /* B^2R */
            out[idx++] = R * R * G;  /* R^2G */
            out[idx++] = G * G * B;  /* G^2B */
            out[idx++] = R * R * B;  /* R^2B */
            out[idx++] = R * G * B;
        }

        if (degree >= 4) {
            /* Degree 4: fourth powers and mixed (ordering matches colour-science) */
            out[idx++] = R * R * R * R;   /* R^4 */
            out[idx++] = G * G * G * G;   /* G^4 */
            out[idx++] = B * B * B * B;   /* B^4 */
            out[idx++] = R * R * R * G;   /* R^3G */
            out[idx++] = R * R * R * B;   /* R^3B */
            out[idx++] = G * G * G * R;   /* G^3R */
            out[idx++] = G * G * G * B;   /* G^3B */
            out[idx++] = B * B * B * R;   /* B^3R */
            out[idx++] = B * B * B * G;   /* B^3G */
            out[idx++] = R * R * G * G;   /* R^2G^2 */
            out[idx++] = G * G * B * B;   /* G^2B^2 */
            out[idx++] = R * R * B * B;   /* R^2B^2 */
            out[idx++] = R * R * G * B;   /* R^2GB */
            out[idx++] = G * G * R * B;   /* G^2RB */
            out[idx++] = B * B * R * G;   /* B^2RG */
        }

        *out_size = idx;
    }

    return ALWAN_OK;
}

int alwan_poly_expand_vandermonde_f64(alwan_f64 *out, int *out_size,
                                   alwan_f64 const *a, int a_size, int degree)
{
    if (!a || !out || !out_size || a_size <= 0 || degree < 1) {
        return ALWAN_E_INVALID;
    }

    /* Vandermonde expansion: [a^degree, a^(degree-1), ..., a^1, 1]
     * For each element, compute all powers from degree down to 0
     * Output size = a_size * degree + 1 */

    int idx = 0;

    /* For each power from degree down to 1 */
    for (int d = degree; d >= 1; d--) {
        for (int i = 0; i < a_size; i++) {
            alwan_f64 power = 1.0;
            for (int p = 0; p < d; p++) {
                power *= a[i];
            }
            out[idx++] = power;
        }
    }

    /* Constant term */
    out[idx++] = 1.0;

    *out_size = idx;
    return ALWAN_OK;
}

/* ================================================================
 * Colour Correction Matrix Computation
 * Uses least-squares regression to find correction matrix
 * ================================================================ */

/* Simple least-squares solve for Ax = b using normal equations
 * A: m x n matrix (row-major)
 * b: m x 3 matrix (row-major, for RGB output)
 * x: n x 3 matrix output (row-major)
 * Returns ALWAN_OK on success */
static int least_squares_solve(alwan_f64 const *A, alwan_f64 const *b,
                                int m, int n, alwan_f64 *x)
{
    /* Compute A^T * A (n x n) and A^T * b (n x 3) */
    /* Then solve (A^T * A) * x = A^T * b using Gauss elimination */

    /* Stack allocation for small matrices - max 35 terms */
    alwan_f64 AtA[35 * 35];
    alwan_f64 Atb[35 * 3];

    if (n > 35) return ALWAN_E_INVALID;

    /* Compute A^T * A */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            alwan_f64 sum = 0.0;
            for (int k = 0; k < m; k++) {
                sum += A[k * n + i] * A[k * n + j];
            }
            AtA[i * n + j] = sum;
        }
    }

    /* Compute A^T * b */
    for (int i = 0; i < n; i++) {
        for (int c = 0; c < 3; c++) {
            alwan_f64 sum = 0.0;
            for (int k = 0; k < m; k++) {
                sum += A[k * n + i] * b[k * 3 + c];
            }
            Atb[i * 3 + c] = sum;
        }
    }

    /* Solve AtA * x = Atb using Gaussian elimination with partial pivoting */
    /* Augmented matrix: [AtA | Atb] */
    alwan_f64 aug[35 * 38];  /* n x (n + 3) */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            aug[i * (n + 3) + j] = AtA[i * n + j];
        }
        for (int c = 0; c < 3; c++) {
            aug[i * (n + 3) + n + c] = Atb[i * 3 + c];
        }
    }

    /* Forward elimination */
    for (int col = 0; col < n; col++) {
        /* Find pivot */
        int max_row = col;
        alwan_f64 max_val = ALWAN_ABS(aug[col * (n + 3) + col]);
        for (int row = col + 1; row < n; row++) {
            alwan_f64 val = ALWAN_ABS(aug[row * (n + 3) + col]);
            if (val > max_val) {
                max_val = val;
                max_row = row;
            }
        }

        /* Swap rows if needed */
        if (max_row != col) {
            for (int j = 0; j < n + 3; j++) {
                alwan_f64 tmp = aug[col * (n + 3) + j];
                aug[col * (n + 3) + j] = aug[max_row * (n + 3) + j];
                aug[max_row * (n + 3) + j] = tmp;
            }
        }

        /* Check for singular matrix */
        if (ALWAN_ABS(aug[col * (n + 3) + col]) < 1e-12) {
            return ALWAN_E_DIVZERO;  /* Singular matrix */
        }

        /* Eliminate column */
        for (int row = col + 1; row < n; row++) {
            alwan_f64 factor = aug[row * (n + 3) + col] / aug[col * (n + 3) + col];
            for (int j = col; j < n + 3; j++) {
                aug[row * (n + 3) + j] -= factor * aug[col * (n + 3) + j];
            }
        }
    }

    /* Back substitution */
    for (int row = n - 1; row >= 0; row--) {
        for (int c = 0; c < 3; c++) {
            alwan_f64 sum = aug[row * (n + 3) + n + c];
            for (int j = row + 1; j < n; j++) {
                sum -= aug[row * (n + 3) + j] * x[j * 3 + c];
            }
            x[row * 3 + c] = sum / aug[row * (n + 3) + row];
        }
    }

    return ALWAN_OK;
}

int alwan_colour_correction_matrix_cheung2004_f64(alwan_f64 *matrix_out,
                                               alwan_f64 const *M_T,
                                               alwan_f64 const *M_R,
                                               int num_samples,
                                               alwan_poly_cheung_terms terms)
{
    if (!M_T || !M_R || !matrix_out || num_samples < (int)terms) {
        return ALWAN_E_INVALID;
    }

    /* Build expanded matrix from test values (with overflow protection) */
    size_t row_size = alwan_safe_array_size((size_t)terms, sizeof(alwan_f64));
    if (row_size == 0) return ALWAN_E_NOMEM;
    size_t alloc_size = alwan_safe_array_size((size_t)num_samples, row_size);
    if (alloc_size == 0) return ALWAN_E_NOMEM;

    alwan_f64 *A = (alwan_f64 *)ALWAN_ALLOC(alloc_size, sizeof(alwan_f64));
    if (!A) return ALWAN_E_NOMEM;

    for (int i = 0; i < num_samples; i++) {
        alwan_rgb_f64 rgb;
        rgb.r = M_T[i * 3 + 0];
        rgb.g = M_T[i * 3 + 1];
        rgb.b = M_T[i * 3 + 2];

        int result = alwan_poly_expand_cheung2004_f64(&A[i * terms], &rgb, terms);
        if (result != ALWAN_OK) {
            ALWAN_FREE(A);
            return result;
        }
    }

    /* Solve least squares: A * matrix = M_R */
    int result = least_squares_solve(A, M_R, num_samples, terms, matrix_out);
    ALWAN_FREE(A);

    return result;
}

void alwan_colour_correct_cheung2004_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb,
                                     alwan_f32 const *matrix, alwan_poly_cheung_terms terms)
{
    if (!rgb || !matrix || !rgb_out) {
        return;
    }

    /* Expand input via f64 path (upcast, expand, downcast) */
    alwan_rgb_f64 rgb64;
    rgb64.r = (alwan_f64)rgb->r;
    rgb64.g = (alwan_f64)rgb->g;
    rgb64.b = (alwan_f64)rgb->b;

    alwan_f64 expanded[35];
    int result = alwan_poly_expand_cheung2004_f64(expanded, &rgb64, terms);
    if (result != ALWAN_OK) return;

    /* Apply matrix: RGB_out = expanded * matrix */
    alwan_f64 r = 0.0, g = 0.0, b = 0.0;

    for (int i = 0; i < (int)terms; i++) {
        r += expanded[i] * (alwan_f64)matrix[i * 3 + 0];
        g += expanded[i] * (alwan_f64)matrix[i * 3 + 1];
        b += expanded[i] * (alwan_f64)matrix[i * 3 + 2];
    }

    rgb_out->r = (float)r;
    rgb_out->g = (float)g;
    rgb_out->b = (float)b;
}

void alwan_colour_correct_cheung2004_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb,
                                     alwan_f64 const *matrix, alwan_poly_cheung_terms terms)
{
    if (!rgb || !matrix || !rgb_out) {
        return;
    }

    /* Expand input */
    alwan_f64 expanded[35];
    int result = alwan_poly_expand_cheung2004_f64(expanded, rgb, terms);
    if (result != ALWAN_OK) return;

    /* Apply matrix: RGB_out = expanded * matrix */
    rgb_out->r = 0.0;
    rgb_out->g = 0.0;
    rgb_out->b = 0.0;

    for (int i = 0; i < (int)terms; i++) {
        rgb_out->r += expanded[i] * matrix[i * 3 + 0];
        rgb_out->g += expanded[i] * matrix[i * 3 + 1];
        rgb_out->b += expanded[i] * matrix[i * 3 + 2];
    }
}

int alwan_colour_correction_matrix_finlayson2015_f64(alwan_f64 *matrix_out, int *matrix_size,
                                                  alwan_f64 const *M_T,
                                                  alwan_f64 const *M_R,
                                                  int num_samples, int degree, int root_poly)
{
    if (!M_T || !M_R || !matrix_out || !matrix_size) {
        return ALWAN_E_INVALID;
    }

    if (degree < 1 || degree > 4) {
        return ALWAN_E_INVALID;
    }

    /* Determine expansion size */
    alwan_rgb_f64 test_rgb = {0.5, 0.5, 0.5};
    alwan_f64 test_out[34];
    int exp_size;
    int result = alwan_poly_expand_finlayson2015_f64(test_out, &exp_size, &test_rgb, degree, root_poly);
    if (result != ALWAN_OK) return result;

    if (num_samples < exp_size) {
        return ALWAN_E_INVALID;
    }

    /* Build expanded matrix (with overflow protection) */
    size_t row_size = alwan_safe_array_size((size_t)exp_size, sizeof(alwan_f64));
    if (row_size == 0) return ALWAN_E_NOMEM;
    size_t alloc_size = alwan_safe_array_size((size_t)num_samples, row_size);
    if (alloc_size == 0) return ALWAN_E_NOMEM;

    alwan_f64 *A = (alwan_f64 *)ALWAN_ALLOC(alloc_size, sizeof(alwan_f64));
    if (!A) return ALWAN_E_NOMEM;

    for (int i = 0; i < num_samples; i++) {
        alwan_rgb_f64 rgb;
        rgb.r = M_T[i * 3 + 0];
        rgb.g = M_T[i * 3 + 1];
        rgb.b = M_T[i * 3 + 2];

        int actual_size;
        result = alwan_poly_expand_finlayson2015_f64(&A[i * exp_size], &actual_size, &rgb, degree, root_poly);
        if (result != ALWAN_OK) {
            ALWAN_FREE(A);
            return result;
        }
    }

    /* Solve least squares */
    result = least_squares_solve(A, M_R, num_samples, exp_size, matrix_out);
    ALWAN_FREE(A);

    *matrix_size = exp_size * 3;
    return result;
}

void alwan_colour_correct_finlayson2015_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb,
                                        alwan_f32 const *matrix, int degree, int root_poly)
{
    if (!rgb || !matrix || !rgb_out) {
        return;
    }

    /* Expand input via f64 path (upcast, expand, downcast) */
    alwan_rgb_f64 rgb64;
    rgb64.r = (alwan_f64)rgb->r;
    rgb64.g = (alwan_f64)rgb->g;
    rgb64.b = (alwan_f64)rgb->b;

    alwan_f64 expanded[34];
    int exp_size;
    int result = alwan_poly_expand_finlayson2015_f64(expanded, &exp_size, &rgb64, degree, root_poly);
    if (result != ALWAN_OK) return;

    /* Apply matrix */
    alwan_f64 r = 0.0, g = 0.0, b = 0.0;

    for (int i = 0; i < exp_size; i++) {
        r += expanded[i] * (alwan_f64)matrix[i * 3 + 0];
        g += expanded[i] * (alwan_f64)matrix[i * 3 + 1];
        b += expanded[i] * (alwan_f64)matrix[i * 3 + 2];
    }

    rgb_out->r = (float)r;
    rgb_out->g = (float)g;
    rgb_out->b = (float)b;
}

void alwan_colour_correct_finlayson2015_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb,
                                        alwan_f64 const *matrix, int degree, int root_poly)
{
    if (!rgb || !matrix || !rgb_out) {
        return;
    }

    /* Expand input */
    alwan_f64 expanded[34];
    int exp_size;
    int result = alwan_poly_expand_finlayson2015_f64(expanded, &exp_size, rgb, degree, root_poly);
    if (result != ALWAN_OK) return;

    /* Apply matrix */
    rgb_out->r = 0.0;
    rgb_out->g = 0.0;
    rgb_out->b = 0.0;

    for (int i = 0; i < exp_size; i++) {
        rgb_out->r += expanded[i] * matrix[i * 3 + 0];
        rgb_out->g += expanded[i] * matrix[i * 3 + 1];
        rgb_out->b += expanded[i] * matrix[i * 3 + 2];
    }
}

/* ================================================================
 * White Balance
 * ================================================================ */

void alwan_white_balance_from_gray_f32(alwan_rgb_f32 *multipliers_out, alwan_rgb_f32 const *measured_gray)
{
    if (!multipliers_out || !measured_gray) {
        return;
    }

    /* Check for zero/negative channels */
    float min_val = measured_gray->r;
    if (measured_gray->g < min_val) min_val = measured_gray->g;
    if (measured_gray->b < min_val) min_val = measured_gray->b;

    if (min_val <= 0.0f) {
        return;
    }

    *multipliers_out = alwan_white_balance_from_gray_f32_v(*measured_gray);
}

void alwan_white_balance_from_gray_f64(alwan_rgb_f64 *multipliers_out, alwan_rgb_f64 const *measured_gray)
{
    if (!multipliers_out || !measured_gray) {
        return;
    }

    /* Check for zero/negative channels */
    alwan_f64 min_val = measured_gray->r;
    if (measured_gray->g < min_val) min_val = measured_gray->g;
    if (measured_gray->b < min_val) min_val = measured_gray->b;

    if (min_val <= 0.0) {
        return;
    }

    *multipliers_out = alwan_white_balance_from_gray_f64_v(*measured_gray);
}

void alwan_white_balance_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb,
                               alwan_rgb_f32 const *multipliers)
{
    if (!rgb_out || !rgb || !multipliers) {
        return;
    }

    *rgb_out = alwan_white_balance_apply_f32_v(*rgb, *multipliers);
}

void alwan_white_balance_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb,
                               alwan_rgb_f64 const *multipliers)
{
    if (!rgb_out || !rgb || !multipliers) {
        return;
    }

    *rgb_out = alwan_white_balance_apply_f64_v(*rgb, *multipliers);
}

/* ================================================================
 * f32 wrappers for color-correction utilities
 *
 * Delegate to the f64 implementations via temporary f64 buffers.
 * ================================================================ */

int alwan_color_matrix_get_preset_f32(alwan_mat3x3_f32 *matrix_3x3, alwan_color_matrix_preset_f32 preset) {
    if (!matrix_3x3) return ALWAN_E_INVALID;
    alwan_mat3x3_f64 tmp;
    int rc = alwan_color_matrix_get_preset_f64(&tmp, (alwan_color_matrix_preset_f64)preset);
    if (rc != ALWAN_OK) return rc;
    for (int i = 0; i < 9; i++) matrix_3x3->m[i] = (float)tmp.m[i];
    return ALWAN_OK;
}

int alwan_poly_expand_cheung2004_f32(alwan_f32 *out, alwan_rgb_f32 const *rgb,
                                  alwan_poly_cheung_terms terms) {
    if (!rgb || !out) return ALWAN_E_INVALID;
    alwan_rgb_f64 rgb64 = { (double)rgb->r, (double)rgb->g, (double)rgb->b };
    alwan_f64 tmp[35];
    int rc = alwan_poly_expand_cheung2004_f64(tmp, &rgb64, terms);
    if (rc != ALWAN_OK) return rc;
    for (int i = 0; i < (int)terms; i++) out[i] = (alwan_f32)tmp[i];
    return ALWAN_OK;
}

int alwan_poly_expand_finlayson2015_f32(alwan_f32 *out, int *out_size,
                                     alwan_rgb_f32 const *rgb, int degree, int root_poly) {
    if (!rgb || !out || !out_size) return ALWAN_E_INVALID;
    alwan_rgb_f64 rgb64 = { (double)rgb->r, (double)rgb->g, (double)rgb->b };
    alwan_f64 tmp[35];
    int rc = alwan_poly_expand_finlayson2015_f64(tmp, out_size, &rgb64, degree, root_poly);
    if (rc != ALWAN_OK) return rc;
    for (int i = 0; i < *out_size; i++) out[i] = (alwan_f32)tmp[i];
    return ALWAN_OK;
}

int alwan_poly_expand_vandermonde_f32(alwan_f32 *out, int *out_size,
                                   alwan_f32 const *a, int a_size, int degree) {
    if (!a || !out || !out_size || a_size <= 0 || degree < 1) return ALWAN_E_INVALID;
    size_t a_sz = (size_t)a_size;
    alwan_f64 *a64 = (alwan_f64 *)ALWAN_ALLOC(a_sz * sizeof(alwan_f64), sizeof(alwan_f64));
    if (!a64) return ALWAN_E_NOMEM;
    for (size_t i = 0; i < a_sz; i++) a64[i] = (alwan_f64)a[i];

    size_t max_out = a_sz * (size_t)degree + 1;
    alwan_f64 *tmp = (alwan_f64 *)ALWAN_ALLOC(max_out * sizeof(alwan_f64), sizeof(alwan_f64));
    if (!tmp) { ALWAN_FREE(a64); return ALWAN_E_NOMEM; }

    int rc = alwan_poly_expand_vandermonde_f64(tmp, out_size, a64, a_size, degree);
    if (rc == ALWAN_OK) {
        for (int i = 0; i < *out_size; i++) out[i] = (alwan_f32)tmp[i];
    }

    ALWAN_FREE(tmp);
    ALWAN_FREE(a64);
    return rc;
}

int alwan_colour_correction_matrix_cheung2004_f32(alwan_f32 *matrix_out,
                                               alwan_f32 const *M_T,
                                               alwan_f32 const *M_R,
                                               int num_samples,
                                               alwan_poly_cheung_terms terms) {
    if (!M_T || !M_R || !matrix_out || num_samples < (int)terms) return ALWAN_E_INVALID;

    size_t ns = (size_t)num_samples * 3;
    alwan_f64 *MT64 = (alwan_f64 *)ALWAN_ALLOC(ns * sizeof(alwan_f64), sizeof(alwan_f64));
    alwan_f64 *MR64 = (alwan_f64 *)ALWAN_ALLOC(ns * sizeof(alwan_f64), sizeof(alwan_f64));
    size_t mat_sz = (size_t)terms * 3;
    alwan_f64 *mat64 = (alwan_f64 *)ALWAN_ALLOC(mat_sz * sizeof(alwan_f64), sizeof(alwan_f64));
    if (!MT64 || !MR64 || !mat64) {
        if (MT64) ALWAN_FREE(MT64);
        if (MR64) ALWAN_FREE(MR64);
        if (mat64) ALWAN_FREE(mat64);
        return ALWAN_E_NOMEM;
    }
    for (size_t i = 0; i < ns; i++) { MT64[i] = (alwan_f64)M_T[i]; MR64[i] = (alwan_f64)M_R[i]; }

    int rc = alwan_colour_correction_matrix_cheung2004_f64(mat64, MT64, MR64, num_samples, terms);
    if (rc == ALWAN_OK) {
        for (size_t i = 0; i < mat_sz; i++) matrix_out[i] = (alwan_f32)mat64[i];
    }
    ALWAN_FREE(MT64); ALWAN_FREE(MR64); ALWAN_FREE(mat64);
    return rc;
}

int alwan_colour_correction_matrix_finlayson2015_f32(alwan_f32 *matrix_out, int *matrix_size,
                                                  alwan_f32 const *M_T,
                                                  alwan_f32 const *M_R,
                                                  int num_samples, int degree, int root_poly) {
    if (!M_T || !M_R || !matrix_out || !matrix_size) return ALWAN_E_INVALID;

    size_t ns = (size_t)num_samples * 3;
    alwan_f64 *MT64 = (alwan_f64 *)ALWAN_ALLOC(ns * sizeof(alwan_f64), sizeof(alwan_f64));
    alwan_f64 *MR64 = (alwan_f64 *)ALWAN_ALLOC(ns * sizeof(alwan_f64), sizeof(alwan_f64));
    /* max size for finlayson is 22 features * 3 */
    alwan_f64 mat64[22 * 3];
    if (!MT64 || !MR64) {
        if (MT64) ALWAN_FREE(MT64);
        if (MR64) ALWAN_FREE(MR64);
        return ALWAN_E_NOMEM;
    }
    for (size_t i = 0; i < ns; i++) { MT64[i] = (alwan_f64)M_T[i]; MR64[i] = (alwan_f64)M_R[i]; }

    int rc = alwan_colour_correction_matrix_finlayson2015_f64(mat64, matrix_size, MT64, MR64, num_samples, degree, root_poly);
    if (rc == ALWAN_OK) {
        int total = (*matrix_size) * 3;
        for (int i = 0; i < total; i++) matrix_out[i] = (alwan_f32)mat64[i];
    }
    ALWAN_FREE(MT64); ALWAN_FREE(MR64);
    return rc;
}
