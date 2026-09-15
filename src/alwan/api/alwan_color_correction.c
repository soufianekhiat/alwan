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
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Lift/Gamma/Gain (LGG)
 * ================================================================ */

#if ALWAN_WITH_F32
void alwan_lgg_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in, alwan_rgb_f32 const *lift,
                    alwan_rgb_f32 const *gamma, alwan_rgb_f32 const *gain)
{
    if (!rgb_out || !rgb_in || !lift || !gamma || !gain) {
        return;
    }

    *rgb_out = alwan_lgg_apply_f32_v(*rgb_in, *lift, *gamma, *gain);
}
#endif

#if ALWAN_WITH_F64
void alwan_lgg_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in, alwan_rgb_f64 const *lift,
                    alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain)
{
    if (!rgb_out || !rgb_in || !lift || !gamma || !gain) {
        return;
    }

    *rgb_out = alwan_lgg_apply_f64_v(*rgb_in, *lift, *gamma, *gain);
}
#endif

/* ================================================================
 * Color Matrix Grading
 * ================================================================ */

#if ALWAN_WITH_F32
void alwan_color_matrix_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in,
                              alwan_mat3x3_f32 const *matrix_3x3)
{
    if (!rgb_out || !rgb_in || !matrix_3x3) {
        return;
    }

    *rgb_out = alwan_color_matrix_apply_f32_v(*rgb_in, *matrix_3x3);
}
#endif

#if ALWAN_WITH_F64
void alwan_color_matrix_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in,
                              alwan_mat3x3_f64 const *matrix_3x3)
{
    if (!rgb_out || !rgb_in || !matrix_3x3) {
        return;
    }

    *rgb_out = alwan_color_matrix_apply_f64_v(*rgb_in, *matrix_3x3);
}

alwan_status alwan_color_matrix_get_preset_f64(alwan_mat3x3_f64 *matrix_3x3, alwan_color_matrix_preset_f64 preset)
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
#endif

/* ================================================================
 * Printer Lights
 * ================================================================ */

#if ALWAN_WITH_F32
void alwan_printer_lights_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in,
                                alwan_f32 red_lights, alwan_f32 green_lights,
                                alwan_f32 blue_lights)
{
    if (!rgb_out || !rgb_in) {
        return;
    }

    *rgb_out = alwan_printer_lights_apply_f32_v(*rgb_in, red_lights, green_lights, blue_lights);
}
#endif

#if ALWAN_WITH_F64
void alwan_printer_lights_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in,
                                alwan_f64 red_lights, alwan_f64 green_lights,
                                alwan_f64 blue_lights)
{
    if (!rgb_out || !rgb_in) {
        return;
    }

    *rgb_out = alwan_printer_lights_apply_f64_v(*rgb_in, red_lights, green_lights, blue_lights);
}
#endif /* ALWAN_WITH_F64 */

/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE
 * (matrix-fit workers below back the cheung2004/finlayson2015 f32 facades). */
#if ALWAN_WITH_F64_FACADE

/* ================================================================
 * Polynomial Color Correction - Cheung 2004, Finlayson 2015, Vandermonde
 * Reference: colour-science implementation for exact term ordering
 * ================================================================ */

alwan_status alwan_poly_expand_cheung2004_f64(alwan_f64 *out, alwan_rgb_f64 const *rgb,
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

alwan_status alwan_poly_expand_finlayson2015_f64(alwan_f64 *out, int *out_size,
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

alwan_status alwan_poly_expand_vandermonde_f64(alwan_f64 *out, int *out_size,
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

/* Least-squares solve for Ax = b by Householder QR.
 *
 * A: m x n matrix (row-major), DESTROYED; both callers free it straight after
 * b: m x 3 matrix (row-major, the three output channels), read only
 * x: n x 3 matrix output (row-major)
 *
 * This used to form the normal equations, AtA x = Atb, and run Gaussian
 * elimination on them. That squares the condition number, and the polynomial
 * bases here are not well conditioned to begin with: the Finlayson root
 * expansion at degree 4 recovered a known matrix to only 1.3e-3, having lost
 * about thirteen digits. Factoring A itself gets that to 4.9e-11. Suite 44
 * measures both at each term count, so the difference is on the record rather
 * than asserted.
 *
 * Reducing A to R in place, applying the same reflections to a copy of b, and
 * back-substituting costs about what the normal equations did at these sizes
 * (n <= 35), and it removes the fixed 35-term stack arrays with it: the work
 * is two allocations that scale with the problem.
 *
 * Returns ALWAN_E_DIVZERO when a pivot falls below a tolerance scaled by the
 * norm of A, which is the rank-deficient case: too few distinct levels in a
 * channel to separate the terms asking about it, whatever the sample count.
 */
static int least_squares_solve(alwan_f64 *A, alwan_f64 const *b,
                                int m, int n, alwan_f64 *x)
{
    if (m < n || n <= 0 || m <= 0) return ALWAN_E_INVALID;

    /* Scale the singularity test to the data: an absolute threshold calls a
     * legitimately small-valued fit singular and lets a large-valued
     * degenerate one through. */
    alwan_f64 frob = 0.0;
    for (int i = 0; i < m * n; i++) frob += A[i] * A[i];
    frob = ALWAN_SQRT(frob);
    if (!(frob > 0.0)) return ALWAN_E_DIVZERO;
    alwan_f64 const pivot_tol = 1e-12 * frob;

    size_t qtb_bytes = alwan_safe_array_size((size_t)m * 3, sizeof(alwan_f64));
    size_t diag_bytes = alwan_safe_array_size((size_t)n, sizeof(alwan_f64));
    if (qtb_bytes == 0 || diag_bytes == 0) return ALWAN_E_NOMEM;
    alwan_f64 *qtb = (alwan_f64 *)ALWAN_ALLOC(qtb_bytes, sizeof(alwan_f64));
    alwan_f64 *diag = (alwan_f64 *)ALWAN_ALLOC(diag_bytes, sizeof(alwan_f64));
    if (!qtb || !diag) {
        if (qtb) ALWAN_FREE(qtb);
        if (diag) ALWAN_FREE(diag);
        return ALWAN_E_NOMEM;
    }
    for (int i = 0; i < m * 3; i++) qtb[i] = b[i];

    /* Householder reduction. Column j's reflector is stored in the column it
     * zeroes, below and including the diagonal; the resulting diagonal of R
     * goes in diag[] so the column stays free to hold it. */
    for (int j = 0; j < n; j++) {
        alwan_f64 norm = 0.0;
        for (int i = j; i < m; i++) norm += A[i * n + j] * A[i * n + j];
        norm = ALWAN_SQRT(norm);
        if (norm < pivot_tol) {
            ALWAN_FREE(qtb); ALWAN_FREE(diag);
            return ALWAN_E_DIVZERO;   /* rank deficient */
        }
        /* Point the reflection away from the pivot: subtracting two nearly
         * equal numbers here is the classic way to lose the whole column. */
        alwan_f64 alpha = (A[j * n + j] > 0.0) ? -norm : norm;
        A[j * n + j] -= alpha;

        alwan_f64 vnorm2 = 0.0;
        for (int i = j; i < m; i++) vnorm2 += A[i * n + j] * A[i * n + j];

        if (vnorm2 > 0.0) {
            for (int k = j + 1; k < n; k++) {
                alwan_f64 dot = 0.0;
                for (int i = j; i < m; i++) dot += A[i * n + j] * A[i * n + k];
                alwan_f64 s = 2.0 * dot / vnorm2;
                for (int i = j; i < m; i++) A[i * n + k] -= s * A[i * n + j];
            }
            for (int c = 0; c < 3; c++) {
                alwan_f64 dot = 0.0;
                for (int i = j; i < m; i++) dot += A[i * n + j] * qtb[i * 3 + c];
                alwan_f64 s = 2.0 * dot / vnorm2;
                for (int i = j; i < m; i++) qtb[i * 3 + c] -= s * A[i * n + j];
            }
        }
        diag[j] = alpha;
    }

    /* Back substitution on the upper triangle, three right-hand sides. */
    for (int c = 0; c < 3; c++) {
        for (int i = n - 1; i >= 0; i--) {
            alwan_f64 sum = qtb[i * 3 + c];
            for (int k = i + 1; k < n; k++) sum -= A[i * n + k] * x[k * 3 + c];
            x[i * 3 + c] = sum / diag[i];
        }
    }

    ALWAN_FREE(qtb);
    ALWAN_FREE(diag);
    return ALWAN_OK;
}

/* Weighted, ridge-regularised least squares on the expanded samples A (m x n):
 *
 *     minimise  sum_i w_i |r_i - a_i X|^2 + ridge |X|_F^2
 *
 * by the same QR, on the augmented system [sqrt(w_i) a_i ; sqrt(ridge) I] X =
 * [sqrt(w_i) r_i ; 0], so the normal equations are still never formed. This is
 * sklearn's Ridge(fit_intercept=False) with sample_weight, every coefficient
 * penalised alike. A sample of weight 0 is left out. With no weights and no ridge the
 * system is A itself and the result is the unweighted fit, bit for bit. */
/* Least squares by one-sided Jacobi SVD (Hestenes): rotate pairs of columns of A
 * until they are mutually orthogonal, which leaves A = U S and accumulates V, then
 * take the minimum-norm solution over the singular values above cutoff. That is
 * LAPACK's gelsd, which numpy.linalg.lstsq calls. Jacobi is slow for large systems
 * and very accurate for small ones, and a CCM has at most 35 terms. A is m x n
 * row-major with m >= n, destroyed; B is m x 3. Returns the rank, -1 when out of
 * memory. */
static int alwan__ccm_svd_solve(alwan_f64 *A, alwan_f64 const *B, int m, int n, alwan_f64 cutoff, alwan_f64 *x)
{
    alwan_f64 *V = (alwan_f64 *)ALWAN_ALLOC((size_t)n * (size_t)n * sizeof(alwan_f64), sizeof(alwan_f64));
    alwan_f64 *sv = (alwan_f64 *)ALWAN_ALLOC((size_t)n * sizeof(alwan_f64), sizeof(alwan_f64));
    alwan_f64 smax = 0.0, thr;
    int i, j, p, q, c, sweep, rank = 0;
    if (!V || !sv) {
        if (V) ALWAN_FREE(V);
        if (sv) ALWAN_FREE(sv);
        return -1;
    }
    for (i = 0; i < n; i++) for (j = 0; j < n; j++) V[i * n + j] = i == j ? 1.0 : 0.0;
    for (sweep = 0; sweep < 80; sweep++) {
        int rotated = 0;
        for (p = 0; p < n - 1; p++) {
            for (q = p + 1; q < n; q++) {
                alwan_f64 alpha = 0.0, beta = 0.0, gamma = 0.0, zeta, t, cs, sn;
                for (i = 0; i < m; i++) {
                    alwan_f64 const ap = A[i * n + p], aq = A[i * n + q];
                    alpha += ap * ap;
                    beta += aq * aq;
                    gamma += ap * aq;
                }
                if (gamma == 0.0 || fabs(gamma) <= DBL_EPSILON * sqrt(alpha * beta)) continue;
                rotated = 1;
                /* The smaller root of t^2 + 2 zeta t - 1 = 0, which zeroes the pair's
                 * inner product. */
                zeta = (beta - alpha) / (2.0 * gamma);
                t = fabs(zeta) > 1e150 ? 0.5 / zeta
                                       : (zeta >= 0.0 ? 1.0 : -1.0) / (fabs(zeta) + sqrt(1.0 + zeta * zeta));
                cs = 1.0 / sqrt(1.0 + t * t);
                sn = cs * t;
                for (i = 0; i < m; i++) {
                    alwan_f64 const ap = A[i * n + p], aq = A[i * n + q];
                    A[i * n + p] = cs * ap - sn * aq;
                    A[i * n + q] = sn * ap + cs * aq;
                }
                for (i = 0; i < n; i++) {
                    alwan_f64 const vp = V[i * n + p], vq = V[i * n + q];
                    V[i * n + p] = cs * vp - sn * vq;
                    V[i * n + q] = sn * vp + cs * vq;
                }
            }
        }
        if (!rotated) break;
    }
    for (j = 0; j < n; j++) {
        alwan_f64 s2 = 0.0;
        for (i = 0; i < m; i++) s2 += A[i * n + j] * A[i * n + j];
        sv[j] = sqrt(s2);
        if (sv[j] > smax) smax = sv[j];
    }
    thr = cutoff * smax;
    for (i = 0; i < n * 3; i++) x[i] = 0.0;
    for (j = 0; j < n; j++) {
        if (!(sv[j] > thr) || !(sv[j] > 0.0)) continue;   /* at or below the cutoff counts as zero, as in gelsd */
        rank++;
        for (c = 0; c < 3; c++) {
            alwan_f64 dot = 0.0, coef;
            for (i = 0; i < m; i++) dot += A[i * n + j] * B[i * 3 + c];
            coef = dot / (sv[j] * sv[j]);                  /* (U_j . b) / s_j, with U_j = A_j / s_j */
            for (i = 0; i < n; i++) x[i * 3 + c] += V[i * n + j] * coef;
        }
    }
    ALWAN_FREE(V);
    ALWAN_FREE(sv);
    return rank;
}

static alwan_status alwan__ccm_solve(alwan_f64 *matrix_out, alwan_f64 const *A, alwan_f64 const *M_R, int m, int n,
                                     alwan_ccm_fit_params const *params)
{
    alwan_f64 const *w = params ? params->weights : NULL;
    alwan_f64 const ridge = params ? params->ridge : 0.0;
    alwan_ccm_solver const solver = params ? params->solver : ALWAN_CCM_SOLVER_QR;
    alwan_f64 const rcond = params ? params->rcond : 0.0;
    int *const rank_out = params ? params->rank_out : NULL;
    int rows = 0, extra, total, alloc_rows, i, j, r;
    alwan_f64 *Aa, *ba;
    alwan_status st;

    if (m < 1 || n < 1 || !(ridge >= 0.0) || ridge - ridge != 0.0) return ALWAN_E_INVALID;
    if ((int)solver < (int)ALWAN_CCM_SOLVER_QR || (int)solver > (int)ALWAN_CCM_SOLVER_SVD
        || !(rcond >= 0.0) || rcond - rcond != 0.0) {
        return ALWAN_E_INVALID;
    }
    for (i = 0; i < m; i++) {
        if (w && (!(w[i] >= 0.0) || w[i] - w[i] != 0.0)) return ALWAN_E_INVALID;
        if (!w || w[i] > 0.0) rows++;
    }
    extra = ridge > 0.0 ? n : 0;
    total = rows + extra;
    /* QR needs as many equations as terms; the SVD answers with fewer, the
     * minimum-norm fit, and pads the system with zero rows to square it. */
    if (total < 1 || (solver == ALWAN_CCM_SOLVER_QR && total < n)) return ALWAN_E_INVALID;
    alloc_rows = total < n ? n : total;

    size_t a_bytes = alwan_safe_array_size((size_t)alloc_rows * (size_t)n, sizeof(alwan_f64));
    size_t b_bytes = alwan_safe_array_size((size_t)alloc_rows * 3, sizeof(alwan_f64));
    if (a_bytes == 0 || b_bytes == 0) return ALWAN_E_NOMEM;
    Aa = (alwan_f64 *)ALWAN_ALLOC(a_bytes, sizeof(alwan_f64));
    ba = (alwan_f64 *)ALWAN_ALLOC(b_bytes, sizeof(alwan_f64));
    if (!Aa || !ba) {
        if (Aa) ALWAN_FREE(Aa);
        if (ba) ALWAN_FREE(ba);
        return ALWAN_E_NOMEM;
    }
    for (i = total * n; i < alloc_rows * n; i++) Aa[i] = 0.0;
    for (i = total * 3; i < alloc_rows * 3; i++) ba[i] = 0.0;
    for (i = 0, r = 0; i < m; i++) {
        alwan_f64 s;
        if (w && !(w[i] > 0.0)) continue;
        s = w ? ALWAN_SQRT(w[i]) : 1.0;
        for (j = 0; j < n; j++) Aa[r * n + j] = w ? s * A[i * n + j] : A[i * n + j];
        for (j = 0; j < 3; j++) ba[r * 3 + j] = w ? s * M_R[i * 3 + j] : M_R[i * 3 + j];
        r++;
    }
    if (extra) {
        alwan_f64 const s = ALWAN_SQRT(ridge);
        for (i = 0; i < n; i++, r++) {
            for (j = 0; j < n; j++) Aa[r * n + j] = i == j ? s : 0.0;
            for (j = 0; j < 3; j++) ba[r * 3 + j] = 0.0;
        }
    }
    if (solver == ALWAN_CCM_SOLVER_SVD) {
        /* numpy's default cutoff: machine precision times the larger dimension. */
        alwan_f64 const cutoff = rcond > 0.0 ? rcond : DBL_EPSILON * (alwan_f64)(total > n ? total : n);
        int const rank = alwan__ccm_svd_solve(Aa, ba, alloc_rows, n, cutoff, matrix_out);
        st = rank < 0 ? ALWAN_E_NOMEM : ALWAN_OK;
        if (rank_out && rank >= 0) *rank_out = rank;
    } else {
        st = (alwan_status)least_squares_solve(Aa, ba, total, n, matrix_out);
        if (rank_out) {
            if (st == ALWAN_OK) *rank_out = n;
            else if (st == ALWAN_E_DIVZERO) *rank_out = -1;
        }
    }
    ALWAN_FREE(Aa);
    ALWAN_FREE(ba);
    return st;
}

alwan_status alwan_colour_correction_matrix_cheung2004_f64(alwan_f64 *matrix_out,
                                               alwan_f64 const *M_T,
                                               alwan_f64 const *M_R,
                                               int num_samples,
                                               alwan_poly_cheung_terms terms)
{
    if (!M_T || !M_R || !matrix_out || num_samples < (int)terms) {
        return ALWAN_E_INVALID;
    }
    return alwan_ccm_fit_cheung2004_f64(matrix_out, M_T, M_R, num_samples, terms, NULL);
}

alwan_status alwan_ccm_fit_cheung2004_f64(alwan_f64 *matrix_out, alwan_f64 const *M_T, alwan_f64 const *M_R,
                                          int num_samples, alwan_poly_cheung_terms terms,
                                          alwan_ccm_fit_params const *params)
{
    if (!M_T || !M_R || !matrix_out || num_samples < 1 || (int)terms < 1) {
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
    int result = alwan__ccm_solve(matrix_out, A, M_R, num_samples, (int)terms, params);
    ALWAN_FREE(A);

    return result;
}
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F32
void alwan_colour_correct_cheung2004_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb,
                                     alwan_f32 const *matrix, alwan_poly_cheung_terms terms)
{
    if (!rgb || !matrix || !rgb_out) {
        return;
    }

    /* Expand input natively in f32 */
    alwan_f32 expanded[35];
    int result = alwan_poly_expand_cheung2004_f32(expanded, rgb, terms);
    if (result != ALWAN_OK) return;

    /* Apply matrix: RGB_out = expanded * matrix */
    alwan_f32 r = 0.0f, g = 0.0f, b = 0.0f;

    for (int i = 0; i < (int)terms; i++) {
        r += expanded[i] * matrix[i * 3 + 0];
        g += expanded[i] * matrix[i * 3 + 1];
        b += expanded[i] * matrix[i * 3 + 2];
    }

    rgb_out->r = r;
    rgb_out->g = g;
    rgb_out->b = b;
}
#endif

#if ALWAN_WITH_F64
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
#endif /* ALWAN_WITH_F64 */

/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE */
#if ALWAN_WITH_F64_FACADE
alwan_status alwan_colour_correction_matrix_finlayson2015_f64(alwan_f64 *matrix_out, int *matrix_size,
                                                  alwan_f64 const *M_T,
                                                  alwan_f64 const *M_R,
                                                  int num_samples, int degree, int root_poly)
{
    return alwan_ccm_fit_finlayson2015_f64(matrix_out, matrix_size, M_T, M_R, num_samples, degree, root_poly, NULL);
}

alwan_status alwan_ccm_fit_finlayson2015_f64(alwan_f64 *matrix_out, int *matrix_size, alwan_f64 const *M_T,
                                             alwan_f64 const *M_R, int num_samples, int degree, int root_poly,
                                             alwan_ccm_fit_params const *params)
{
    if (!M_T || !M_R || !matrix_out || !matrix_size || num_samples < 1) {
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

    /* Without a ridge there must be at least as many samples as terms; the solve
     * checks the samples that carry weight. */
    if (num_samples < exp_size && !(params && (params->ridge > 0.0 || params->solver == ALWAN_CCM_SOLVER_SVD))) {
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
    result = alwan__ccm_solve(matrix_out, A, M_R, num_samples, exp_size, params);
    ALWAN_FREE(A);

    *matrix_size = exp_size * 3;
    return result;
}

/* ================================================================
 * Choosing a fit by leave-one-out
 *
 * Each sample of positive weight is left out in turn, by giving it weight 0; the
 * rest are fitted with the caller's params and the fit predicts the one left out.
 * The score is the root mean square of those held-out residuals, over the samples
 * predicted and the three channels, in the reference's units. A fit that learned
 * the chart instead of the camera does well on the samples it saw and badly on the
 * one it did not, so this is where it shows.
 * ================================================================ */

static alwan_f64 alwan__ccm_nan(void) {
    volatile alwan_f64 zero = 0.0;
    return zero / zero;
}

static int const alwan__cheung_counts[14] = { 3, 4, 5, 7, 8, 10, 11, 14, 16, 17, 19, 20, 22, 35 };

static alwan_status alwan__ccm_loo(alwan_f64 *rms_out, alwan_f64 *pred_out, alwan_f64 const *M_T,
                                   alwan_f64 const *M_R, int n, int finlayson, int a, int root,
                                   alwan_ccm_fit_params const *params)
{
    alwan_ccm_fit_params p;
    alwan_f64 *w;
    alwan_f64 m[35 * 3], e[35];
    alwan_f64 sum = 0.0;
    int k, i, c, count = 0;
    alwan_status st = ALWAN_OK;

    if (!rms_out || !M_T || !M_R || n < 2) return ALWAN_E_INVALID;
    if (params) p = *params;
    else memset(&p, 0, sizeof(p));
    p.rank_out = NULL;
    w = (alwan_f64 *)ALWAN_ALLOC((size_t)n * sizeof(alwan_f64), sizeof(alwan_f64));
    if (!w) return ALWAN_E_NOMEM;
    for (i = 0; i < n; i++) {
        w[i] = params && params->weights ? params->weights[i] : 1.0;
        if (!(w[i] >= 0.0) || w[i] - w[i] != 0.0) {
            ALWAN_FREE(w);
            return ALWAN_E_INVALID;
        }
    }
    p.weights = w;
    for (k = 0; k < n && st == ALWAN_OK; k++) {
        alwan_f64 const wk = w[k];
        alwan_rgb_f64 rgb;
        int nt = 0;
        if (!(wk > 0.0)) {
            if (pred_out) pred_out[3 * k] = pred_out[3 * k + 1] = pred_out[3 * k + 2] = alwan__ccm_nan();
            continue;
        }
        w[k] = 0.0;
        if (!finlayson) {
            st = alwan_ccm_fit_cheung2004_f64(m, M_T, M_R, n, (alwan_poly_cheung_terms)a, &p);
        } else {
            int size = 0;
            st = alwan_ccm_fit_finlayson2015_f64(m, &size, M_T, M_R, n, a, root, &p);
        }
        w[k] = wk;
        if (st != ALWAN_OK) break;
        rgb.r = M_T[3 * k];
        rgb.g = M_T[3 * k + 1];
        rgb.b = M_T[3 * k + 2];
        if (!finlayson) {
            st = alwan_poly_expand_cheung2004_f64(e, &rgb, (alwan_poly_cheung_terms)a);
            nt = a;
        } else {
            st = alwan_poly_expand_finlayson2015_f64(e, &nt, &rgb, a, root);
        }
        if (st != ALWAN_OK) break;
        for (c = 0; c < 3; c++) {
            alwan_f64 v = 0.0, dlt;
            for (i = 0; i < nt; i++) v += e[i] * m[i * 3 + c];   /* the apply's own order */
            dlt = v - M_R[3 * k + c];
            sum += dlt * dlt;
            if (pred_out) pred_out[3 * k + c] = v;
        }
        count++;
    }
    ALWAN_FREE(w);
    if (st != ALWAN_OK) return st;
    if (count == 0) return ALWAN_E_INVALID;   /* every weight 0 */
    *rms_out = sqrt(sum / (3.0 * (alwan_f64)count));
    return ALWAN_OK;
}

alwan_status alwan_ccm_loo_cheung2004_f64(alwan_f64 *rms_out, alwan_f64 *pred_out, alwan_f64 const *M_T,
                                          alwan_f64 const *M_R, int num_samples, alwan_poly_cheung_terms terms,
                                          alwan_ccm_fit_params const *params)
{
    return alwan__ccm_loo(rms_out, pred_out, M_T, M_R, num_samples, 0, (int)terms, 0, params);
}

alwan_status alwan_ccm_loo_finlayson2015_f64(alwan_f64 *rms_out, alwan_f64 *pred_out, alwan_f64 const *M_T,
                                             alwan_f64 const *M_R, int num_samples, int degree, int root_poly,
                                             alwan_ccm_fit_params const *params)
{
    return alwan__ccm_loo(rms_out, pred_out, M_T, M_R, num_samples, 1, degree, root_poly, params);
}

alwan_status alwan_ccm_select_cheung2004_f64(alwan_poly_cheung_terms *terms_out, alwan_f64 *rms_out,
                                             alwan_f64 const *M_T, alwan_f64 const *M_R, int num_samples,
                                             alwan_ccm_fit_params const *params)
{
    alwan_status first = ALWAN_OK;
    alwan_f64 best = 0.0;
    int j, found = 0;
    if (!terms_out) return ALWAN_E_INVALID;
    for (j = 0; j < 14; j++) {
        alwan_f64 r = 0.0;
        alwan_status const st = alwan__ccm_loo(&r, NULL, M_T, M_R, num_samples, 0, alwan__cheung_counts[j], 0,
                                               params);
        if (st == ALWAN_E_NOMEM) return st;
        if (st != ALWAN_OK) {
            /* This count cannot be fitted from what is left once a sample is out. */
            if (first == ALWAN_OK) first = st;
            if (rms_out) rms_out[j] = alwan__ccm_nan();
            continue;
        }
        if (rms_out) rms_out[j] = r;
        if (!found || r < best) {
            best = r;
            *terms_out = (alwan_poly_cheung_terms)alwan__cheung_counts[j];
            found = 1;
        }
    }
    return found ? ALWAN_OK : first;
}
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F32
void alwan_colour_correct_finlayson2015_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb,
                                        alwan_f32 const *matrix, int degree, int root_poly)
{
    if (!rgb || !matrix || !rgb_out) {
        return;
    }

    /* Expand input natively in f32 */
    alwan_f32 expanded[34];
    int exp_size;
    int result = alwan_poly_expand_finlayson2015_f32(expanded, &exp_size, rgb, degree, root_poly);
    if (result != ALWAN_OK) return;

    /* Apply matrix */
    alwan_f32 r = 0.0f, g = 0.0f, b = 0.0f;

    for (int i = 0; i < exp_size; i++) {
        r += expanded[i] * matrix[i * 3 + 0];
        g += expanded[i] * matrix[i * 3 + 1];
        b += expanded[i] * matrix[i * 3 + 2];
    }

    rgb_out->r = r;
    rgb_out->g = g;
    rgb_out->b = b;
}
#endif

#if ALWAN_WITH_F64
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
#endif

/* ================================================================
 * White Balance
 * ================================================================ */

#if ALWAN_WITH_F32
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
#endif

#if ALWAN_WITH_F64
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
#endif

#if ALWAN_WITH_F32
void alwan_white_balance_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb,
                               alwan_rgb_f32 const *multipliers)
{
    if (!rgb_out || !rgb || !multipliers) {
        return;
    }

    *rgb_out = alwan_white_balance_apply_f32_v(*rgb, *multipliers);
}
#endif

#if ALWAN_WITH_F64
void alwan_white_balance_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb,
                               alwan_rgb_f64 const *multipliers)
{
    if (!rgb_out || !rgb || !multipliers) {
        return;
    }

    *rgb_out = alwan_white_balance_apply_f64_v(*rgb, *multipliers);
}
#endif

/* ================================================================
 * f32 wrappers for color-correction utilities
 *
 * Delegate to the f64 implementations via temporary f64 buffers.
 * ================================================================ */

#if ALWAN_WITH_F32 && ALWAN_WITH_F64
alwan_status alwan_color_matrix_get_preset_f32(alwan_mat3x3_f32 *matrix_3x3, alwan_color_matrix_preset_f32 preset) {
    if (!matrix_3x3) return ALWAN_E_INVALID;
    alwan_mat3x3_f64 tmp;
    int rc = alwan_color_matrix_get_preset_f64(&tmp, (alwan_color_matrix_preset_f64)preset);
    if (rc != ALWAN_OK) return rc;
    for (int i = 0; i < 9; i++) matrix_3x3->m[i] = (float)tmp.m[i];
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 && ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
alwan_status alwan_poly_expand_cheung2004_f32(alwan_f32 *out, alwan_rgb_f32 const *rgb,
                                  alwan_poly_cheung_terms terms) {
    if (!rgb || !out) return ALWAN_E_INVALID;

    alwan_f32 R = rgb->r;
    alwan_f32 G = rgb->g;
    alwan_f32 B = rgb->b;

    /* Pre-compute common products (mirrors the f64 worker exactly) */
    alwan_f32 RG = R * G;
    alwan_f32 RB = R * B;
    alwan_f32 GB = G * B;
    alwan_f32 RGB = R * G * B;
    alwan_f32 R2 = R * R;
    alwan_f32 G2 = G * G;
    alwan_f32 B2 = B * B;
    alwan_f32 R3 = R2 * R;
    alwan_f32 G3 = G2 * G;
    alwan_f32 B3 = B2 * B;
    alwan_f32 R4 = R2 * R2;
    alwan_f32 G4 = G2 * G2;
    alwan_f32 B4 = B2 * B2;

    switch (terms) {
        case ALWAN_POLY_CHEUNG_3:
            out[0] = R; out[1] = G; out[2] = B;
            break;

        case ALWAN_POLY_CHEUNG_4:
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = 1.0f;
            break;

        case ALWAN_POLY_CHEUNG_5:
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RGB;
            out[4] = 1.0f;
            break;

        case ALWAN_POLY_CHEUNG_7:
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = 1.0f;
            break;

        case ALWAN_POLY_CHEUNG_8:
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = RGB;
            out[7] = 1.0f;
            break;

        case ALWAN_POLY_CHEUNG_10:
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = 1.0f;
            break;

        case ALWAN_POLY_CHEUNG_11:
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = 1.0f;
            break;

        case ALWAN_POLY_CHEUNG_14:
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = R3; out[11] = G3; out[12] = B3;
            out[13] = 1.0f;
            break;

        case ALWAN_POLY_CHEUNG_16:
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = R2 * G; out[11] = G2 * B; out[12] = R * B2;
            out[13] = R3; out[14] = G3; out[15] = B3;
            break;

        case ALWAN_POLY_CHEUNG_17:
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = R2 * G; out[11] = G2 * B; out[12] = R * B2;
            out[13] = R3; out[14] = G3; out[15] = B3;
            out[16] = 1.0f;
            break;

        case ALWAN_POLY_CHEUNG_19:
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = R2 * G; out[11] = G2 * B; out[12] = R * B2;
            out[13] = R2 * B; out[14] = R * G2; out[15] = G * B2;
            out[16] = R3; out[17] = G3; out[18] = B3;
            break;

        case ALWAN_POLY_CHEUNG_20:
            out[0] = R; out[1] = G; out[2] = B;
            out[3] = RG; out[4] = RB; out[5] = GB;
            out[6] = R2; out[7] = G2; out[8] = B2;
            out[9] = RGB;
            out[10] = R2 * G; out[11] = G2 * B; out[12] = R * B2;
            out[13] = R2 * B; out[14] = R * G2; out[15] = G * B2;
            out[16] = R3; out[17] = G3; out[18] = B3;
            out[19] = 1.0f;
            break;

        case ALWAN_POLY_CHEUNG_22:
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
            out[34] = 1.0f;
            break;

        default:
            return ALWAN_E_INVALID;
    }

    return ALWAN_OK;
}

alwan_status alwan_poly_expand_finlayson2015_f32(alwan_f32 *out, int *out_size,
                                     alwan_rgb_f32 const *rgb, int degree, int root_poly) {
    if (!rgb || !out || !out_size) return ALWAN_E_INVALID;

    if (degree < 1 || degree > 4) return ALWAN_E_INVALID;

    alwan_f32 R = rgb->r;
    alwan_f32 G = rgb->g;
    alwan_f32 B = rgb->b;

    if (root_poly) {
        /* Root-polynomial expansion (mirrors the f64 worker exactly) */
        int idx = 0;

        out[idx++] = R;
        out[idx++] = G;
        out[idx++] = B;

        if (degree >= 2) {
            out[idx++] = ALWAN_SQRT_F32(R * G);
            out[idx++] = ALWAN_SQRT_F32(G * B);
            out[idx++] = ALWAN_SQRT_F32(R * B);
        }

        if (degree >= 3) {
            out[idx++] = ALWAN_POW_F32(G * G * R, 1.0f / 3.0f);  /* cbrt(G^2R) */
            out[idx++] = ALWAN_POW_F32(B * B * G, 1.0f / 3.0f);  /* cbrt(B^2G) */
            out[idx++] = ALWAN_POW_F32(B * B * R, 1.0f / 3.0f);  /* cbrt(B^2R) */
            out[idx++] = ALWAN_POW_F32(R * R * G, 1.0f / 3.0f);  /* cbrt(R^2G) */
            out[idx++] = ALWAN_POW_F32(G * G * B, 1.0f / 3.0f);  /* cbrt(G^2B) */
            out[idx++] = ALWAN_POW_F32(R * R * B, 1.0f / 3.0f);  /* cbrt(R^2B) */
            out[idx++] = ALWAN_POW_F32(R * G * B, 1.0f / 3.0f);  /* cbrt(RGB) */
        }

        if (degree >= 4) {
            out[idx++] = ALWAN_POW_F32(R * R * R * G, 0.25f);  /* qrt(R^3G) */
            out[idx++] = ALWAN_POW_F32(R * R * R * B, 0.25f);  /* qrt(R^3B) */
            out[idx++] = ALWAN_POW_F32(G * G * G * R, 0.25f);  /* qrt(G^3R) */
            out[idx++] = ALWAN_POW_F32(G * G * G * B, 0.25f);  /* qrt(G^3B) */
            out[idx++] = ALWAN_POW_F32(B * B * B * R, 0.25f);  /* qrt(B^3R) */
            out[idx++] = ALWAN_POW_F32(B * B * B * G, 0.25f);  /* qrt(B^3G) */
            out[idx++] = ALWAN_POW_F32(R * R * G * B, 0.25f);  /* qrt(R^2GB) */
            out[idx++] = ALWAN_POW_F32(G * G * R * B, 0.25f);  /* qrt(G^2RB) */
            out[idx++] = ALWAN_POW_F32(B * B * R * G, 0.25f);  /* qrt(B^2RG) */
        }

        *out_size = idx;
    } else {
        /* Standard polynomial expansion */
        int idx = 0;

        out[idx++] = R;
        out[idx++] = G;
        out[idx++] = B;

        if (degree >= 2) {
            out[idx++] = R * R;
            out[idx++] = G * G;
            out[idx++] = B * B;
            out[idx++] = R * G;
            out[idx++] = G * B;
            out[idx++] = R * B;
        }

        if (degree >= 3) {
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

alwan_status alwan_poly_expand_vandermonde_f32(alwan_f32 *out, int *out_size,
                                   alwan_f32 const *a, int a_size, int degree) {
    if (!a || !out || !out_size || a_size <= 0 || degree < 1) return ALWAN_E_INVALID;

    /* Vandermonde expansion in native f32 (mirrors the f64 worker exactly):
     * [a^degree, a^(degree-1), ..., a^1, 1], power by repeated multiply. */
    int idx = 0;

    for (int d = degree; d >= 1; d--) {
        for (int i = 0; i < a_size; i++) {
            alwan_f32 power = 1.0f;
            for (int p = 0; p < d; p++) {
                power *= a[i];
            }
            out[idx++] = power;
        }
    }

    out[idx++] = 1.0f;

    *out_size = idx;
    return ALWAN_OK;
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_colour_correction_matrix_cheung2004_f32(alwan_f32 *matrix_out,
                                               alwan_f32 const *M_T,
                                               alwan_f32 const *M_R,
                                               int num_samples,
                                               alwan_poly_cheung_terms terms) {
    if (!M_T || !M_R || !matrix_out || num_samples < (int)terms) return ALWAN_E_INVALID;
    return alwan_ccm_fit_cheung2004_f32(matrix_out, M_T, M_R, num_samples, terms, NULL);
}

alwan_status alwan_colour_correction_matrix_finlayson2015_f32(alwan_f32 *matrix_out, int *matrix_size,
                                                  alwan_f32 const *M_T,
                                                  alwan_f32 const *M_R,
                                                  int num_samples, int degree, int root_poly) {
    if (!M_T || !M_R || !matrix_out || !matrix_size) return ALWAN_E_INVALID;

    return alwan_ccm_fit_finlayson2015_f32(matrix_out, matrix_size, M_T, M_R, num_samples, degree, root_poly, NULL);
}

alwan_status alwan_ccm_fit_cheung2004_f32(alwan_f32 *matrix_out, alwan_f32 const *M_T, alwan_f32 const *M_R,
                                          int num_samples, alwan_poly_cheung_terms terms,
                                          alwan_ccm_fit_params const *params) {
    if (!M_T || !M_R || !matrix_out || num_samples < 1 || (int)terms < 1) return ALWAN_E_INVALID;

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

    int rc = alwan_ccm_fit_cheung2004_f64(mat64, MT64, MR64, num_samples, terms, params);
    if (rc == ALWAN_OK) {
        for (size_t i = 0; i < mat_sz; i++) matrix_out[i] = (alwan_f32)mat64[i];
    }
    ALWAN_FREE(MT64); ALWAN_FREE(MR64); ALWAN_FREE(mat64);
    return rc;
}

alwan_status alwan_ccm_fit_finlayson2015_f32(alwan_f32 *matrix_out, int *matrix_size, alwan_f32 const *M_T,
                                             alwan_f32 const *M_R, int num_samples, int degree, int root_poly,
                                             alwan_ccm_fit_params const *params) {
    if (!M_T || !M_R || !matrix_out || !matrix_size || num_samples < 1) return ALWAN_E_INVALID;

    size_t ns = (size_t)num_samples * 3;
    alwan_f64 *MT64 = (alwan_f64 *)ALWAN_ALLOC(ns * sizeof(alwan_f64), sizeof(alwan_f64));
    alwan_f64 *MR64 = (alwan_f64 *)ALWAN_ALLOC(ns * sizeof(alwan_f64), sizeof(alwan_f64));
    /* The widest basis, plain degree 4, has 34 terms. matrix_size comes back as the
     * element count, terms x 3, which is exactly what is copied out. */
    alwan_f64 mat64[34 * 3];
    if (!MT64 || !MR64) {
        if (MT64) ALWAN_FREE(MT64);
        if (MR64) ALWAN_FREE(MR64);
        return ALWAN_E_NOMEM;
    }
    for (size_t i = 0; i < ns; i++) { MT64[i] = (alwan_f64)M_T[i]; MR64[i] = (alwan_f64)M_R[i]; }

    int rc = alwan_ccm_fit_finlayson2015_f64(mat64, matrix_size, MT64, MR64, num_samples, degree, root_poly, params);
    if (rc == ALWAN_OK) {
        for (int i = 0; i < *matrix_size; i++) matrix_out[i] = (alwan_f32)mat64[i];
    }
    ALWAN_FREE(MT64); ALWAN_FREE(MR64);
    return rc;
}

/* Leave-one-out for f32 samples: widen, score in f64, narrow. */
static alwan_status alwan__ccm_loo_f32(alwan_f32 *rms_out, alwan_f32 *pred_out, alwan_f32 const *M_T,
                                       alwan_f32 const *M_R, int num_samples, int finlayson, int a, int root,
                                       alwan_ccm_fit_params const *params) {
    size_t const ns = num_samples > 0 ? (size_t)num_samples * 3 : 0;
    alwan_f64 *buf, rms = 0.0;
    int rc;
    if (!rms_out || !M_T || !M_R || num_samples < 2) return ALWAN_E_INVALID;
    buf = (alwan_f64 *)ALWAN_ALLOC(3 * ns * sizeof(alwan_f64), sizeof(alwan_f64));
    if (!buf) return ALWAN_E_NOMEM;
    for (size_t i = 0; i < ns; i++) { buf[i] = (alwan_f64)M_T[i]; buf[ns + i] = (alwan_f64)M_R[i]; }
    rc = finlayson ? alwan_ccm_loo_finlayson2015_f64(&rms, pred_out ? buf + 2 * ns : NULL, buf, buf + ns,
                                                      num_samples, a, root, params)
                   : alwan_ccm_loo_cheung2004_f64(&rms, pred_out ? buf + 2 * ns : NULL, buf, buf + ns, num_samples,
                                                  (alwan_poly_cheung_terms)a, params);
    if (rc == ALWAN_OK) {
        *rms_out = (alwan_f32)rms;
        if (pred_out) for (size_t i = 0; i < ns; i++) pred_out[i] = (alwan_f32)buf[2 * ns + i];
    }
    ALWAN_FREE(buf);
    return rc;
}

alwan_status alwan_ccm_loo_cheung2004_f32(alwan_f32 *rms_out, alwan_f32 *pred_out, alwan_f32 const *M_T,
                                          alwan_f32 const *M_R, int num_samples, alwan_poly_cheung_terms terms,
                                          alwan_ccm_fit_params const *params) {
    return alwan__ccm_loo_f32(rms_out, pred_out, M_T, M_R, num_samples, 0, (int)terms, 0, params);
}

alwan_status alwan_ccm_loo_finlayson2015_f32(alwan_f32 *rms_out, alwan_f32 *pred_out, alwan_f32 const *M_T,
                                             alwan_f32 const *M_R, int num_samples, int degree, int root_poly,
                                             alwan_ccm_fit_params const *params) {
    return alwan__ccm_loo_f32(rms_out, pred_out, M_T, M_R, num_samples, 1, degree, root_poly, params);
}

alwan_status alwan_ccm_select_cheung2004_f32(alwan_poly_cheung_terms *terms_out, alwan_f32 *rms_out,
                                             alwan_f32 const *M_T, alwan_f32 const *M_R, int num_samples,
                                             alwan_ccm_fit_params const *params) {
    size_t const ns = num_samples > 0 ? (size_t)num_samples * 3 : 0;
    alwan_f64 *buf, rms64[14];
    int rc;
    if (!terms_out || !M_T || !M_R || num_samples < 2) return ALWAN_E_INVALID;
    buf = (alwan_f64 *)ALWAN_ALLOC(2 * ns * sizeof(alwan_f64), sizeof(alwan_f64));
    if (!buf) return ALWAN_E_NOMEM;
    for (size_t i = 0; i < ns; i++) { buf[i] = (alwan_f64)M_T[i]; buf[ns + i] = (alwan_f64)M_R[i]; }
    rc = alwan_ccm_select_cheung2004_f64(terms_out, rms64, buf, buf + ns, num_samples, params);
    if (rc == ALWAN_OK && rms_out) for (int j = 0; j < 14; j++) rms_out[j] = (alwan_f32)rms64[j];
    ALWAN_FREE(buf);
    return rc;
}
#endif /* ALWAN_WITH_F32 */
