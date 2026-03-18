/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Benchmark suite -- measures throughput (Mpixels/sec) for all map pipelines.
 * Compares interleaved (AoS), planar (SoA), and per-pixel (scalar loop).
 * No external dependencies.  Cross-platform timer (Windows / POSIX).
 */

#include "alwan.h"
#include "simd/alwan_simd_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <conio.h>
#endif

/* ================================================================
 * Cross-platform high-resolution timer
 * ================================================================ */

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_MACRO_EXPANSION
#  include <windows.h>
ALWAN_DIAG_POP

static double timer_freq_inv;
static void timer_init(void) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    timer_freq_inv = 1.0 / (double)freq.QuadPart;
}
static double timer_now(void) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * timer_freq_inv;
}
#else
#  include <time.h>
static void timer_init(void) { (void)0; }
static double timer_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}
#endif

/* ================================================================
 * Console coloring
 * ================================================================ */

#ifdef _WIN32
static HANDLE g_hcon;
static WORD   g_default_attr;
static void color_init(void) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    g_hcon = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(g_hcon, &info))
        g_default_attr = info.wAttributes;
    else
        g_default_attr = 0x07;
}
static void color_set(int attr) { SetConsoleTextAttribute(g_hcon, (WORD)attr); }
static void color_reset(void)    { SetConsoleTextAttribute(g_hcon, g_default_attr); }
#else
static void color_init(void)     { (void)0; }
static void color_set(int attr)  { (void)attr; }
static void color_reset(void)    { (void)0; }
#endif

/* Windows console attributes for foreground colors:
 * 0x0A = green   0x0B = cyan   0x0E = yellow   0x0C = red
 * 0x0F = bright white   0x08 = dark gray   0x07 = default */
#define COL_GREEN  0x0A
#define COL_CYAN   0x0B
#define COL_YELLOW 0x0E
#define COL_RED    0x0C
#define COL_WHITE  0x0F
#define COL_GRAY   0x08

static int col_for_mpixs(double v) {
    if (v >= 400.0) return COL_GREEN;
    if (v >= 200.0) return COL_CYAN;
    if (v >= 50.0)  return COL_YELLOW;
    return COL_RED;
}

static int col_for_row(double v, double row_max) {
    double pct;
    if (row_max <= 0.0) return COL_RED;
    pct = v / row_max;
    if (pct >= 0.75) return COL_GREEN;
    if (pct >= 0.50) return COL_CYAN;
    if (pct >= 0.25) return COL_YELLOW;
    return COL_RED;
}

/* ================================================================
 * SIMD backend name
 * ================================================================ */

static char const *simd_backend_name(void) {
#if defined(__AVX2__)
    return "AVX2";
#elif defined(__AVX__)
    return "AVX";
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
    return "SSE2";
#elif defined(__aarch64__)
    return "NEON (aarch64)";
#elif defined(__ARM_NEON)
    return "NEON (ARMv7)";
#else
    return "Scalar";
#endif
}

/* ================================================================
 * Benchmark harness
 * ================================================================ */

#define BENCH_PIXELS   (1024 * 1024)
#define BENCH_WARMUP   2
#define BENCH_ITERS    5

static void fill_random(alwan_scalar *buf, size_t count) {
    unsigned int seed = 0x12345678u;
    size_t i;
    for (i = 0; i < count; i++) {
        seed = seed * 1103515245u + 12345u;
        buf[i] = (alwan_scalar)(seed >> 16) / (alwan_scalar)65535.0;
    }
}

static void fill_typed_random(void *buf, alwan_pixel_format fmt, size_t count) {
    unsigned int seed = 0xABCD1234u;
    size_t k;
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        uint8_t *p = (uint8_t *)buf;
        for (k = 0; k < count; k++) { seed = seed * 1103515245u + 12345u; p[k] = (uint8_t)(seed >> 16); }
    } break;
    case ALWAN_PIXEL_U16: {
        uint16_t *p = (uint16_t *)buf;
        for (k = 0; k < count; k++) { seed = seed * 1103515245u + 12345u; p[k] = (uint16_t)(seed >> 16); }
    } break;
    case ALWAN_PIXEL_F32: {
        float *p = (float *)buf;
        for (k = 0; k < count; k++) { seed = seed * 1103515245u + 12345u; p[k] = (float)(seed >> 16) / 65535.0f; }
    } break;
    case ALWAN_PIXEL_F16: {
        uint16_t *p = (uint16_t *)buf;
        for (k = 0; k < count; k++) {
            /* Generate valid half-float in [0,1]: exponent 0x3C00 = 1.0, scale mantissa */
            float v;
            seed = seed * 1103515245u + 12345u;
            v = (float)(seed >> 16) / 65535.0f;
            /* Use bit manipulation: IEEE 754 binary16 */
            {
                unsigned int bits;
                memcpy(&bits, &v, sizeof(bits));
                /* Simple float->half: shift exponent and mantissa */
                {
                    unsigned int s = (bits >> 16) & 0x8000u;
                    int e = (int)((bits >> 23) & 0xFFu) - 127 + 15;
                    unsigned int m = bits & 0x7FFFFFu;
                    if (e <= 0) p[k] = (uint16_t)s;
                    else if (e >= 31) p[k] = (uint16_t)(s | 0x7C00u);
                    else p[k] = (uint16_t)(s | ((unsigned int)e << 10) | (m >> 13));
                }
            }
        }
    } break;
    case ALWAN_PIXEL_F64: {
        double *p = (double *)buf;
        for (k = 0; k < count; k++) { seed = seed * 1103515245u + 12345u; p[k] = (double)(seed >> 16) / 65535.0; }
    } break;
    default: break;
    }
}

typedef void (*bench_fn)(void *ctx);

static double measure_best(bench_fn fn, void *ctx) {
    int w, i;
    double best;
    for (w = 0; w < BENCH_WARMUP; w++) fn(ctx);
    best = 1e30;
    for (i = 0; i < BENCH_ITERS; i++) {
        double t0 = timer_now();
        fn(ctx);
        double dt = timer_now() - t0;
        if (dt < best) best = dt;
    }
    return best;
}

/* ================================================================
 * Function pointer types
 * ================================================================ */

typedef int (*interleave_fn)(alwan_scalar *, alwan_scalar const *,
                             size_t, size_t, size_t);
typedef int (*planar_fn)(alwan_scalar *, alwan_scalar *, alwan_scalar *,
                         alwan_scalar const *, alwan_scalar const *, alwan_scalar const *,
                         size_t, size_t, size_t);
typedef void (*perpx_fn)(alwan_scalar *out, alwan_scalar const *in);
typedef int (*interleave_ex_fn)(void *, alwan_pixel_format, void const *, alwan_pixel_format,
                                 size_t, size_t, size_t);
typedef int (*planar_ex_fn)(void *, void *, void *, alwan_pixel_format,
                             void const *, void const *, void const *, alwan_pixel_format,
                             size_t, size_t, size_t);

/* ================================================================
 * Global defaults for wrapped functions
 * ================================================================ */

static alwan_xyz const g_d65 = {
    ALWAN_LITERAL(0.95047), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.08883)
};
static alwan_mat3x3 g_bench_mat = {{
    ALWAN_LITERAL(0.4124564), ALWAN_LITERAL(0.3575761), ALWAN_LITERAL(0.1804375),
    ALWAN_LITERAL(0.2126729), ALWAN_LITERAL(0.7151522), ALWAN_LITERAL(0.0721750),
    ALWAN_LITERAL(0.0193339), ALWAN_LITERAL(0.1191920), ALWAN_LITERAL(0.9503041)
}};
static alwan_rgb const g_lgg_lift  = { ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0) };
static alwan_rgb const g_lgg_gamma = { ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0) };
static alwan_rgb const g_lgg_gain  = { ALWAN_LITERAL(1.2), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.9) };
static alwan_rgb const g_wb_mults  = { ALWAN_LITERAL(1.1), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.85) };

/* ================================================================
 * Interleave wrappers (extra-param maps -> simple_map_fn)
 * ================================================================ */

#define IWRAP_WHITE(name, api)  static int name(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) { return api(o, i, &g_d65, n, si, so); }
#define IWRAP_PQ(name, api)     static int name(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) { return api(o, i, 1, n, si, so); }
#define IWRAP_DIN(name, api)    static int name(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) { return api(o, i, 0, n, si, so); }
#define IWRAP_YCBCR(name, api)  static int name(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) { return api(o, i, ALWAN_YCBCR_BT709, n, si, so); }
#define IWRAP_10B(name, api)    static int name(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) { return api(o, i, 10, n, si, so); }

IWRAP_WHITE(iw_xyz_to_lab,    alwan_xyz_to_lab_map_interleave)
IWRAP_WHITE(iw_lab_to_xyz,    alwan_lab_to_xyz_map_interleave)
IWRAP_WHITE(iw_xyz_to_luv,    alwan_xyz_to_luv_map_interleave)
IWRAP_WHITE(iw_luv_to_xyz,    alwan_luv_to_xyz_map_interleave)
IWRAP_WHITE(iw_xyz_to_uvw,    alwan_xyz_to_uvw_map_interleave)
IWRAP_WHITE(iw_uvw_to_xyz,    alwan_uvw_to_xyz_map_interleave)
IWRAP_WHITE(iw_xyz_to_hlab_c, alwan_xyz_to_hunter_lab_custom_map_interleave)
IWRAP_WHITE(iw_hlab_to_xyz_c, alwan_hunter_lab_to_xyz_custom_map_interleave)
IWRAP_WHITE(iw_xyz_to_plab_c, alwan_xyz_to_prolab_custom_map_interleave)
IWRAP_WHITE(iw_plab_to_xyz_c, alwan_prolab_to_xyz_custom_map_interleave)
IWRAP_PQ(iw_rgb_to_ictcp,     alwan_rgb_to_ictcp_map_interleave)
IWRAP_PQ(iw_ictcp_to_rgb,     alwan_ictcp_to_rgb_map_interleave)
IWRAP_PQ(iw_xyz_to_ictcp,     alwan_xyz_to_ictcp_map_interleave)
IWRAP_PQ(iw_ictcp_to_xyz,     alwan_ictcp_to_xyz_map_interleave)
IWRAP_DIN(iw_lab_to_din99,    alwan_lab_to_din99_map_interleave)
IWRAP_DIN(iw_din99_to_lab,    alwan_din99_to_lab_map_interleave)
IWRAP_YCBCR(iw_rgb_to_ycbcr,  alwan_rgb_to_ycbcr_map_interleave)
IWRAP_YCBCR(iw_ycbcr_to_rgb,  alwan_ycbcr_to_rgb_map_interleave)
IWRAP_10B(iw_rgb_to_yccbccrc, alwan_rgb_to_yccbccrc_map_interleave)
IWRAP_10B(iw_yccbccrc_to_rgb, alwan_yccbccrc_to_rgb_map_interleave)
IWRAP_10B(iw_ycbcr_f2l,       alwan_ycbcr_full_to_legal_map_interleave)
IWRAP_10B(iw_ycbcr_l2f,       alwan_ycbcr_legal_to_full_map_interleave)

static int iw_gamut_clip(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) {
    return alwan_gamut_map_interleave(o, ALWAN_GAMUT_MAP_CLIP, i, n, si, so);
}
static int iw_cvd_brettel(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) {
    return alwan_simulate_cvd_map_interleave(o, i, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(0.8), n, si, so);
}
static int iw_cvd_protan(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) {
    return alwan_simulate_protanopia_map_interleave(o, i, ALWAN_LITERAL(0.8), n, si, so);
}
static int iw_cvd_deutan(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) {
    return alwan_simulate_deuteranopia_map_interleave(o, i, ALWAN_LITERAL(0.8), n, si, so);
}
static int iw_cvd_tritan(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) {
    return alwan_simulate_tritanopia_map_interleave(o, i, ALWAN_LITERAL(0.8), n, si, so);
}
static int iw_cvd_machado(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) {
    return alwan_simulate_cvd_machado_map_interleave(o, i, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(0.8), n, si, so);
}
static int iw_mat3(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) {
    return alwan_mat3_transform_map_interleave(o, &g_bench_mat, i, n, si, so);
}
static int iw_srgb_oetf(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) {
    return alwan_oetf_apply(o, ALWAN_TF_SRGB, i, n * 3, si / 3, so / 3);
}
static int iw_color_matrix(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) {
    return alwan_color_matrix_apply_map_interleave(o, i, &g_bench_mat, n, si, so);
}
static int iw_lgg(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) {
    return alwan_lgg_apply_map_interleave(o, i, &g_lgg_lift, &g_lgg_gamma, &g_lgg_gain, n, si, so);
}
static int iw_printer_lights(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) {
    return alwan_printer_lights_apply_map_interleave(o, i, ALWAN_LITERAL(1.1), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.9), n, si, so);
}
static int iw_white_balance(alwan_scalar *o, alwan_scalar const *i, size_t n, size_t si, size_t so) {
    return alwan_white_balance_apply_map_interleave(o, i, &g_wb_mults, n, si, so);
}

/* ================================================================
 * Planar wrappers (extra-param planar maps -> planar_fn)
 * ================================================================ */

#define PWRAP_WHITE(name, api) static int name(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2, \
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) { \
    return api(o0, o1, o2, i0, i1, i2, &g_d65, n, si, so); }
#define PWRAP_PQ(name, api) static int name(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2, \
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) { \
    return api(o0, o1, o2, i0, i1, i2, 1, n, si, so); }
#define PWRAP_DIN(name, api) static int name(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2, \
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) { \
    return api(o0, o1, o2, i0, i1, i2, 0, n, si, so); }
#define PWRAP_YCBCR(name, api) static int name(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2, \
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) { \
    return api(o0, o1, o2, i0, i1, i2, ALWAN_YCBCR_BT709, n, si, so); }
#define PWRAP_10B(name, api) static int name(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2, \
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) { \
    return api(o0, o1, o2, i0, i1, i2, 10, n, si, so); }

PWRAP_WHITE(pw_xyz_to_lab,    alwan_xyz_to_lab_map_planar)
PWRAP_WHITE(pw_lab_to_xyz,    alwan_lab_to_xyz_map_planar)
PWRAP_WHITE(pw_xyz_to_luv,    alwan_xyz_to_luv_map_planar)
PWRAP_WHITE(pw_luv_to_xyz,    alwan_luv_to_xyz_map_planar)
PWRAP_WHITE(pw_xyz_to_uvw,    alwan_xyz_to_uvw_map_planar)
PWRAP_WHITE(pw_uvw_to_xyz,    alwan_uvw_to_xyz_map_planar)
PWRAP_WHITE(pw_xyz_to_hlab_c, alwan_xyz_to_hunter_lab_custom_map_planar)
PWRAP_WHITE(pw_hlab_to_xyz_c, alwan_hunter_lab_to_xyz_custom_map_planar)
PWRAP_WHITE(pw_xyz_to_plab_c, alwan_xyz_to_prolab_custom_map_planar)
PWRAP_WHITE(pw_plab_to_xyz_c, alwan_prolab_to_xyz_custom_map_planar)
PWRAP_PQ(pw_rgb_to_ictcp,     alwan_rgb_to_ictcp_map_planar)
PWRAP_PQ(pw_ictcp_to_rgb,     alwan_ictcp_to_rgb_map_planar)
PWRAP_PQ(pw_xyz_to_ictcp,     alwan_xyz_to_ictcp_map_planar)
PWRAP_PQ(pw_ictcp_to_xyz,     alwan_ictcp_to_xyz_map_planar)
PWRAP_DIN(pw_lab_to_din99,    alwan_lab_to_din99_map_planar)
PWRAP_DIN(pw_din99_to_lab,    alwan_din99_to_lab_map_planar)
PWRAP_YCBCR(pw_rgb_to_ycbcr,  alwan_rgb_to_ycbcr_map_planar)
PWRAP_YCBCR(pw_ycbcr_to_rgb,  alwan_ycbcr_to_rgb_map_planar)
PWRAP_10B(pw_rgb_to_yccbccrc, alwan_rgb_to_yccbccrc_map_planar)
PWRAP_10B(pw_yccbccrc_to_rgb, alwan_yccbccrc_to_rgb_map_planar)
PWRAP_10B(pw_ycbcr_f2l,       alwan_ycbcr_full_to_legal_map_planar)
PWRAP_10B(pw_ycbcr_l2f,       alwan_ycbcr_legal_to_full_map_planar)

static int pw_cvd_brettel(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2,
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) {
    return alwan_simulate_cvd_map_planar(o0, o1, o2, i0, i1, i2, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(0.8), n, si, so);
}
static int pw_cvd_protan(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2,
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) {
    return alwan_simulate_protanopia_map_planar(o0, o1, o2, i0, i1, i2, ALWAN_LITERAL(0.8), n, si, so);
}
static int pw_cvd_deutan(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2,
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) {
    return alwan_simulate_deuteranopia_map_planar(o0, o1, o2, i0, i1, i2, ALWAN_LITERAL(0.8), n, si, so);
}
static int pw_cvd_tritan(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2,
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) {
    return alwan_simulate_tritanopia_map_planar(o0, o1, o2, i0, i1, i2, ALWAN_LITERAL(0.8), n, si, so);
}
static int pw_color_matrix(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2,
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) {
    return alwan_color_matrix_apply_map_planar(o0, o1, o2, i0, i1, i2, &g_bench_mat, n, si, so);
}
static int pw_lgg(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2,
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) {
    return alwan_lgg_apply_map_planar(o0, o1, o2, i0, i1, i2, &g_lgg_lift, &g_lgg_gamma, &g_lgg_gain, n, si, so);
}
static int pw_printer_lights(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2,
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) {
    return alwan_printer_lights_apply_map_planar(o0, o1, o2, i0, i1, i2,
        ALWAN_LITERAL(1.1), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.9), n, si, so);
}
static int pw_white_balance(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2,
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) {
    return alwan_white_balance_apply_map_planar(o0, o1, o2, i0, i1, i2, &g_wb_mults, n, si, so);
}

/* CMY -> CMYK planar (3 in -> 4 out, extra k channel) */
static int pw_cmy_to_cmyk(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2,
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) {
    static alwan_scalar *k_out = NULL;
    if (!k_out) k_out = (alwan_scalar *)malloc(BENCH_PIXELS * sizeof(alwan_scalar));
    return alwan_cmy_to_cmyk_map_planar(o0, o1, o2, k_out, i0, i1, i2, n, si, so);
}

/* CMYK -> CMY planar (4 in -> 3 out, extra k input channel) */
static int pw_cmyk_to_cmy(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2,
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) {
    static alwan_scalar *k_in = NULL;
    if (!k_in) {
        size_t j;
        k_in = (alwan_scalar *)malloc(BENCH_PIXELS * sizeof(alwan_scalar));
        for (j = 0; j < BENCH_PIXELS; j++) k_in[j] = ALWAN_LITERAL(0.2);
    }
    return alwan_cmyk_to_cmy_map_planar(o0, o1, o2, i0, i1, i2, k_in, n, si, so);
}

/* CVD Machado planar */
static int pw_cvd_machado(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2,
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) {
    return alwan_simulate_cvd_machado_map_planar(o0, o1, o2, i0, i1, i2, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(0.8), n, si, so);
}

/* Gamut map (clip) planar */
static int pw_gamut_clip(alwan_scalar *o0, alwan_scalar *o1, alwan_scalar *o2,
    alwan_scalar const *i0, alwan_scalar const *i1, alwan_scalar const *i2, size_t n, size_t si, size_t so) {
    return alwan_gamut_map_planar(o0, o1, o2, ALWAN_GAMUT_MAP_CLIP, i0, i1, i2, n, si, so);
}

/* ================================================================
 * Per-pixel wrappers (struct-based API -> perpx_fn)
 * ================================================================ */

/* Macro for simple fn(out*, in*) — works for both int and void return */
#define PP(wname, api_fn, out_t, in_t) \
static void wname(alwan_scalar *o, alwan_scalar const *i) { \
    in_t a; out_t b; memcpy(&a, i, sizeof(a)); api_fn(&b, &a); memcpy(o, &b, sizeof(b)); }

/* sRGB convenience */
PP(pp_srgb_to_xyz,   alwan_srgb_to_xyz,   alwan_xyz,   alwan_rgb)
PP(pp_xyz_to_srgb,   alwan_xyz_to_srgb,   alwan_rgb,   alwan_xyz)
PP(pp_srgb_to_lab,   alwan_srgb_to_lab,   alwan_lab,   alwan_rgb)
PP(pp_lab_to_srgb,   alwan_lab_to_srgb,   alwan_rgb,   alwan_lab)
PP(pp_srgb_to_oklab, alwan_srgb_to_oklab, alwan_oklab, alwan_rgb)
PP(pp_oklab_to_srgb, alwan_oklab_to_srgb, alwan_rgb,   alwan_oklab)

/* CIE colorimetry (void return) */
PP(pp_xyz_to_xyy,     alwan_xyz_to_xyy,     alwan_xyy,   alwan_xyz)
PP(pp_xyy_to_xyz,     alwan_xyy_to_xyz,     alwan_xyz,   alwan_xyy)
PP(pp_xyz_to_oklab,   alwan_xyz_to_oklab,   alwan_oklab, alwan_xyz)
PP(pp_oklab_to_xyz,   alwan_oklab_to_xyz,   alwan_xyz,   alwan_oklab)
PP(pp_oklab_to_oklch, alwan_oklab_to_oklch, alwan_oklch, alwan_oklab)
PP(pp_oklch_to_oklab, alwan_oklch_to_oklab, alwan_oklab, alwan_oklch)
PP(pp_lab_to_lch,     alwan_lab_to_lch,     alwan_lch,   alwan_lab)
PP(pp_lch_to_lab,     alwan_lch_to_lab,     alwan_lab,   alwan_lch)
PP(pp_luv_to_lchuv,   alwan_luv_to_lchuv,   alwan_lchuv, alwan_luv)
PP(pp_lchuv_to_luv,   alwan_lchuv_to_luv,   alwan_luv,   alwan_lchuv)

/* CIE with white point */
static void pp_xyz_to_lab(alwan_scalar *o, alwan_scalar const *i) {
    alwan_xyz a; alwan_lab b; memcpy(&a, i, sizeof(a)); alwan_xyz_to_lab(&b, &a, &g_d65); memcpy(o, &b, sizeof(b));
}
static void pp_lab_to_xyz(alwan_scalar *o, alwan_scalar const *i) {
    alwan_lab a; alwan_xyz b; memcpy(&a, i, sizeof(a)); alwan_lab_to_xyz(&b, &a, &g_d65); memcpy(o, &b, sizeof(b));
}
static void pp_xyz_to_luv(alwan_scalar *o, alwan_scalar const *i) {
    alwan_xyz a; alwan_luv b; memcpy(&a, i, sizeof(a)); alwan_xyz_to_luv(&b, &a, &g_d65); memcpy(o, &b, sizeof(b));
}
static void pp_luv_to_xyz(alwan_scalar *o, alwan_scalar const *i) {
    alwan_luv a; alwan_xyz b; memcpy(&a, i, sizeof(a)); alwan_luv_to_xyz(&b, &a, &g_d65); memcpy(o, &b, sizeof(b));
}
static void pp_xyz_to_uvw(alwan_scalar *o, alwan_scalar const *i) {
    alwan_xyz a; alwan_uvw b; memcpy(&a, i, sizeof(a)); alwan_xyz_to_uvw(&b, &a, &g_d65); memcpy(o, &b, sizeof(b));
}
static void pp_uvw_to_xyz(alwan_scalar *o, alwan_scalar const *i) {
    alwan_uvw a; alwan_xyz b; memcpy(&a, i, sizeof(a)); alwan_uvw_to_xyz(&b, &a, &g_d65); memcpy(o, &b, sizeof(b));
}

/* HSV/HSL/HSP/HSY */
PP(pp_rgb_to_hsv,   alwan_rgb_to_hsv,   alwan_hsv,   alwan_rgb)
PP(pp_hsv_to_rgb,   alwan_hsv_to_rgb,   alwan_rgb,   alwan_hsv)
PP(pp_rgb_to_hsl,   alwan_rgb_to_hsl,   alwan_hsl,   alwan_rgb)
PP(pp_hsl_to_rgb,   alwan_hsl_to_rgb,   alwan_rgb,   alwan_hsl)
PP(pp_rgb_to_hsp,   alwan_rgb_to_hsp,   alwan_hsp,   alwan_rgb)
PP(pp_hsp_to_rgb,   alwan_hsp_to_rgb,   alwan_rgb,   alwan_hsp)
PP(pp_rgb_to_hsplog, alwan_rgb_to_hsplog, alwan_hsplog, alwan_rgb)
PP(pp_hsplog_to_rgb, alwan_hsplog_to_rgb, alwan_rgb,   alwan_hsplog)
PP(pp_rgb_to_hsy,   alwan_rgb_to_hsy,   alwan_hsy,   alwan_rgb)
PP(pp_hsy_to_rgb,   alwan_hsy_to_rgb,   alwan_rgb,   alwan_hsy)
PP(pp_linsrgb_to_hsv, alwan_linear_srgb_to_hsv, alwan_hsv, alwan_rgb)
PP(pp_hsv_to_linsrgb, alwan_hsv_to_linear_srgb, alwan_rgb, alwan_hsv)
PP(pp_linsrgb_to_hsl, alwan_linear_srgb_to_hsl, alwan_hsl, alwan_rgb)
PP(pp_hsl_to_linsrgb, alwan_hsl_to_linear_srgb, alwan_rgb, alwan_hsl)

/* CMY/YCoCg */
PP(pp_rgb_to_cmy,   alwan_rgb_to_cmy,   alwan_cmy,   alwan_rgb)
PP(pp_cmy_to_rgb,   alwan_cmy_to_rgb,   alwan_rgb,   alwan_cmy)
PP(pp_rgb_to_ycocg, alwan_rgb_to_ycocg, alwan_ycocg, alwan_rgb)
PP(pp_ycocg_to_rgb, alwan_ycocg_to_rgb, alwan_rgb,   alwan_ycocg)

/* YCbCr */
static void pp_rgb_to_ycbcr(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb a; alwan_ycbcr b; memcpy(&a, i, sizeof(a)); alwan_rgb_to_ycbcr(&b, &a, ALWAN_YCBCR_BT709); memcpy(o, &b, sizeof(b));
}
static void pp_ycbcr_to_rgb(alwan_scalar *o, alwan_scalar const *i) {
    alwan_ycbcr a; alwan_rgb b; memcpy(&a, i, sizeof(a)); alwan_ycbcr_to_rgb(&b, &a, ALWAN_YCBCR_BT709); memcpy(o, &b, sizeof(b));
}
static void pp_rgb_to_yccbccrc(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb a; alwan_yccbccrc b; memcpy(&a, i, sizeof(a)); alwan_rgb_to_yccbccrc(&b, &a, 10); memcpy(o, &b, sizeof(b));
}
static void pp_yccbccrc_to_rgb(alwan_scalar *o, alwan_scalar const *i) {
    alwan_yccbccrc a; alwan_rgb b; memcpy(&a, i, sizeof(a)); alwan_yccbccrc_to_rgb(&b, &a, 10); memcpy(o, &b, sizeof(b));
}
static void pp_ycbcr_f2l(alwan_scalar *o, alwan_scalar const *i) {
    alwan_ycbcr a, b; memcpy(&a, i, sizeof(a)); alwan_ycbcr_full_to_legal(&b, &a, 10); memcpy(o, &b, sizeof(b));
}
static void pp_ycbcr_l2f(alwan_scalar *o, alwan_scalar const *i) {
    alwan_ycbcr a, b; memcpy(&a, i, sizeof(a)); alwan_ycbcr_legal_to_full(&b, &a, 10); memcpy(o, &b, sizeof(b));
}

/* JzAzBz / Jzczhz */
PP(pp_xyz_to_jzazbz,    alwan_xyz_to_jzazbz,    alwan_jzazbz, alwan_xyz)
PP(pp_jzazbz_to_xyz,    alwan_jzazbz_to_xyz,    alwan_xyz,    alwan_jzazbz)
PP(pp_jzazbz_to_jzczhz, alwan_jzazbz_to_jzczhz, alwan_jzczhz, alwan_jzazbz)
PP(pp_jzczhz_to_jzazbz, alwan_jzczhz_to_jzazbz, alwan_jzazbz, alwan_jzczhz)

/* IPT */
PP(pp_xyz_to_ipt, alwan_xyz_to_ipt, alwan_ipt, alwan_xyz)
PP(pp_ipt_to_xyz, alwan_ipt_to_xyz, alwan_xyz, alwan_ipt)

/* ICtCp (PQ) */
static void pp_rgb_to_ictcp(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb a; alwan_ictcp b; memcpy(&a, i, sizeof(a)); alwan_rgb_to_ictcp(&b, &a, 1); memcpy(o, &b, sizeof(b));
}
static void pp_ictcp_to_rgb(alwan_scalar *o, alwan_scalar const *i) {
    alwan_ictcp a; alwan_rgb b; memcpy(&a, i, sizeof(a)); alwan_ictcp_to_rgb(&b, &a, 1); memcpy(o, &b, sizeof(b));
}
static void pp_xyz_to_ictcp(alwan_scalar *o, alwan_scalar const *i) {
    alwan_xyz a; alwan_ictcp b; memcpy(&a, i, sizeof(a)); alwan_xyz_to_ictcp(&b, &a, 1); memcpy(o, &b, sizeof(b));
}
static void pp_ictcp_to_xyz(alwan_scalar *o, alwan_scalar const *i) {
    alwan_ictcp a; alwan_xyz b; memcpy(&a, i, sizeof(a)); alwan_ictcp_to_xyz(&b, &a, 1); memcpy(o, &b, sizeof(b));
}

/* Extended color spaces */
PP(pp_xyz_to_igpgtg,     alwan_xyz_to_igpgtg,     alwan_igpgtg,    alwan_xyz)
PP(pp_igpgtg_to_xyz,     alwan_igpgtg_to_xyz,     alwan_xyz,       alwan_igpgtg)
PP(pp_xyz_to_icacb,      alwan_xyz_to_icacb,      alwan_icacb,     alwan_xyz)
PP(pp_icacb_to_xyz,      alwan_icacb_to_xyz,      alwan_xyz,       alwan_icacb)
PP(pp_xyz_to_hdr_cielab, alwan_xyz_to_hdr_cielab, alwan_lab,       alwan_xyz)
PP(pp_hdr_cielab_to_xyz, alwan_hdr_cielab_to_xyz, alwan_xyz,       alwan_lab)
PP(pp_xyz_to_hdr_ipt,    alwan_xyz_to_hdr_ipt,    alwan_ipt,       alwan_xyz)
PP(pp_hdr_ipt_to_xyz,    alwan_hdr_ipt_to_xyz,    alwan_xyz,       alwan_ipt)
PP(pp_xyz_to_ucs,        alwan_xyz_to_ucs,        alwan_ucs,       alwan_xyz)
PP(pp_ucs_to_xyz,        alwan_ucs_to_xyz,        alwan_xyz,       alwan_ucs)
PP(pp_xyz_to_osa_ucs,    alwan_xyz_to_osa_ucs,    alwan_osa_ucs,   alwan_xyz)
PP(pp_osa_ucs_to_xyz,    alwan_osa_ucs_to_xyz,    alwan_xyz,       alwan_osa_ucs)
PP(pp_xyz_to_hunter_lab, alwan_xyz_to_hunter_lab, alwan_hunter_lab, alwan_xyz)
PP(pp_hunter_lab_to_xyz, alwan_hunter_lab_to_xyz, alwan_xyz,       alwan_hunter_lab)
PP(pp_xyz_to_prolab,     alwan_xyz_to_prolab,     alwan_prolab,    alwan_xyz)
PP(pp_prolab_to_xyz,     alwan_prolab_to_xyz,     alwan_xyz,       alwan_prolab)
PP(pp_rgb_to_prismatic,  alwan_rgb_to_prismatic,  alwan_prismatic, alwan_rgb)
PP(pp_prismatic_to_rgb,  alwan_prismatic_to_rgb,  alwan_rgb,       alwan_prismatic)
PP(pp_rgb_to_hcl,        alwan_rgb_to_hcl,        alwan_hcl,       alwan_rgb)
PP(pp_hcl_to_rgb,        alwan_hcl_to_rgb,        alwan_rgb,       alwan_hcl)
PP(pp_rgb_to_ihls,       alwan_rgb_to_ihls,       alwan_ihls,      alwan_rgb)
PP(pp_ihls_to_rgb,       alwan_ihls_to_rgb,       alwan_rgb,       alwan_ihls)

/* Hunter Lab / ProLab with custom white point */
static void pp_xyz_to_hlab_c(alwan_scalar *o, alwan_scalar const *i) {
    alwan_xyz a; alwan_hunter_lab b; memcpy(&a, i, sizeof(a)); alwan_xyz_to_hunter_lab_custom(&b, &a, &g_d65); memcpy(o, &b, sizeof(b));
}
static void pp_hlab_to_xyz_c(alwan_scalar *o, alwan_scalar const *i) {
    alwan_hunter_lab a; alwan_xyz b; memcpy(&a, i, sizeof(a)); alwan_hunter_lab_to_xyz_custom(&b, &a, &g_d65); memcpy(o, &b, sizeof(b));
}
static void pp_xyz_to_plab_c(alwan_scalar *o, alwan_scalar const *i) {
    alwan_xyz a; alwan_prolab b; memcpy(&a, i, sizeof(a)); alwan_xyz_to_prolab_custom(&b, &a, &g_d65); memcpy(o, &b, sizeof(b));
}
static void pp_plab_to_xyz_c(alwan_scalar *o, alwan_scalar const *i) {
    alwan_prolab a; alwan_xyz b; memcpy(&a, i, sizeof(a)); alwan_prolab_to_xyz_custom(&b, &a, &g_d65); memcpy(o, &b, sizeof(b));
}

/* DIN99 */
static void pp_lab_to_din99(alwan_scalar *o, alwan_scalar const *i) {
    alwan_lab a; alwan_din99 b; memcpy(&a, i, sizeof(a)); alwan_lab_to_din99(&b, &a, 0); memcpy(o, &b, sizeof(b));
}
static void pp_din99_to_lab(alwan_scalar *o, alwan_scalar const *i) {
    alwan_din99 a; alwan_lab b; memcpy(&a, i, sizeof(a)); alwan_din99_to_lab(&b, &a, 0); memcpy(o, &b, sizeof(b));
}

/* HWB (raw scalar I/O, not struct-based) */
static void pp_rgb_to_hwb(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb a; memcpy(&a, i, sizeof(a)); alwan_rgb_to_hwb(o, &a);
}
static void pp_hwb_to_rgb(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb b; alwan_hwb_to_rgb(&b, i); memcpy(o, &b, sizeof(b));
}

/* CMY <-> CMYK (non-standard signatures) */
static void pp_cmy_to_cmyk(alwan_scalar *o, alwan_scalar const *i) {
    alwan_cmy a; memcpy(&a, i, sizeof(a)); alwan_cmy_to_cmyk(&o[0], &o[1], &o[2], &o[3], &a);
}
static void pp_cmyk_to_cmy(alwan_scalar *o, alwan_scalar const *i) {
    alwan_cmy b; alwan_cmyk_to_cmy(&b, i[0], i[1], i[2], i[3]); memcpy(o, &b, sizeof(b));
}

/* CVD */
static void pp_cvd_brettel(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb a, b; memcpy(&a, i, sizeof(a)); alwan_simulate_cvd(&b, &a, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(0.8)); memcpy(o, &b, sizeof(b));
}
static void pp_cvd_protan(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb a, b; memcpy(&a, i, sizeof(a)); alwan_simulate_cvd(&b, &a, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(0.8)); memcpy(o, &b, sizeof(b));
}
static void pp_cvd_deutan(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb a, b; memcpy(&a, i, sizeof(a)); alwan_simulate_cvd(&b, &a, ALWAN_CVD_DEUTERANOPIA, ALWAN_LITERAL(0.8)); memcpy(o, &b, sizeof(b));
}
static void pp_cvd_tritan(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb a, b; memcpy(&a, i, sizeof(a)); alwan_simulate_cvd(&b, &a, ALWAN_CVD_TRITANOPIA, ALWAN_LITERAL(0.8)); memcpy(o, &b, sizeof(b));
}
static void pp_cvd_machado(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb a, b; memcpy(&a, i, sizeof(a)); alwan_simulate_cvd_machado(&b, &a, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(0.8)); memcpy(o, &b, sizeof(b));
}

/* Color correction */
static void pp_printer_lights(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb a, b; memcpy(&a, i, sizeof(a)); alwan_printer_lights_apply(&b, &a, ALWAN_LITERAL(1.1), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.9)); memcpy(o, &b, sizeof(b));
}
static void pp_white_balance(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb a, b; memcpy(&a, i, sizeof(a)); alwan_white_balance_apply(&b, &a, &g_wb_mults); memcpy(o, &b, sizeof(b));
}

/* Mat3 */
static void pp_mat3(alwan_scalar *o, alwan_scalar const *i) {
    alwan_vec3 a, b; memcpy(&a, i, sizeof(a)); alwan_mat3_mulv(&b, &g_bench_mat, &a); memcpy(o, &b, sizeof(b));
}

/* sRGB OETF (per-element, 3 channels) */
static void pp_srgb_oetf(alwan_scalar *o, alwan_scalar const *i) {
    alwan_oetf_apply(o, ALWAN_TF_SRGB, i, 3, sizeof(alwan_scalar), sizeof(alwan_scalar));
}

/* HSV <-> HWB (via map with count=1) */
static void pp_hsv_to_hwb(alwan_scalar *o, alwan_scalar const *i) {
    alwan_hsv_to_hwb_map_interleave(o, i, 1, 3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
}
static void pp_hwb_to_hsv(alwan_scalar *o, alwan_scalar const *i) {
    alwan_hwb_to_hsv_map_interleave(o, i, 1, 3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
}

/* Gamut map (clip) */
static void pp_gamut_clip(alwan_scalar *o, alwan_scalar const *i) {
    alwan_gamut_map_interleave(o, ALWAN_GAMUT_MAP_CLIP, i, 1, 3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
}

/* CSS gamut map */
static void pp_css_gamut(alwan_scalar *o, alwan_scalar const *i) {
    alwan_css_gamut_map_interleave(o, i, 1, 3 * sizeof(alwan_scalar), 3 * sizeof(alwan_scalar));
}

/* Color correction */
static void pp_lgg(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb a, b; memcpy(&a, i, sizeof(a)); alwan_lgg_apply(&b, &a, &g_lgg_lift, &g_lgg_gamma, &g_lgg_gain); memcpy(o, &b, sizeof(b));
}
static void pp_color_matrix(alwan_scalar *o, alwan_scalar const *i) {
    alwan_rgb a, b; memcpy(&a, i, sizeof(a)); alwan_color_matrix_apply(&b, &a, &g_bench_mat); memcpy(o, &b, sizeof(b));
}

/* ================================================================
 * Interleave _ex wrappers (extra-param maps -> interleave_ex_fn)
 * ================================================================ */

#define IXWRAP_WHITE(name, api) \
static int name(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf, \
    size_t n, size_t si, size_t so) { return api(o, of, i, inf, &g_d65, n, si, so); }
#define IXWRAP_PQ(name, api) \
static int name(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf, \
    size_t n, size_t si, size_t so) { return api(o, of, i, inf, 1, n, si, so); }
#define IXWRAP_DIN(name, api) \
static int name(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf, \
    size_t n, size_t si, size_t so) { return api(o, of, i, inf, 0, n, si, so); }
#define IXWRAP_YCBCR(name, api) \
static int name(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf, \
    size_t n, size_t si, size_t so) { return api(o, of, i, inf, ALWAN_YCBCR_BT709, n, si, so); }
#define IXWRAP_10B(name, api) \
static int name(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf, \
    size_t n, size_t si, size_t so) { return api(o, of, i, inf, 10, n, si, so); }

IXWRAP_WHITE(ixw_xyz_to_lab,    alwan_xyz_to_lab_map_interleave_ex)
IXWRAP_WHITE(ixw_lab_to_xyz,    alwan_lab_to_xyz_map_interleave_ex)
IXWRAP_WHITE(ixw_xyz_to_luv,    alwan_xyz_to_luv_map_interleave_ex)
IXWRAP_WHITE(ixw_luv_to_xyz,    alwan_luv_to_xyz_map_interleave_ex)
IXWRAP_WHITE(ixw_xyz_to_uvw,    alwan_xyz_to_uvw_map_interleave_ex)
IXWRAP_WHITE(ixw_uvw_to_xyz,    alwan_uvw_to_xyz_map_interleave_ex)
IXWRAP_WHITE(ixw_xyz_to_hlab_c, alwan_xyz_to_hunter_lab_custom_map_interleave_ex)
IXWRAP_WHITE(ixw_hlab_to_xyz_c, alwan_hunter_lab_to_xyz_custom_map_interleave_ex)
IXWRAP_WHITE(ixw_xyz_to_plab_c, alwan_xyz_to_prolab_custom_map_interleave_ex)
IXWRAP_WHITE(ixw_plab_to_xyz_c, alwan_prolab_to_xyz_custom_map_interleave_ex)
IXWRAP_PQ(ixw_rgb_to_ictcp,     alwan_rgb_to_ictcp_map_interleave_ex)
IXWRAP_PQ(ixw_ictcp_to_rgb,     alwan_ictcp_to_rgb_map_interleave_ex)
IXWRAP_PQ(ixw_xyz_to_ictcp,     alwan_xyz_to_ictcp_map_interleave_ex)
IXWRAP_PQ(ixw_ictcp_to_xyz,     alwan_ictcp_to_xyz_map_interleave_ex)
IXWRAP_DIN(ixw_lab_to_din99,    alwan_lab_to_din99_map_interleave_ex)
IXWRAP_DIN(ixw_din99_to_lab,    alwan_din99_to_lab_map_interleave_ex)
IXWRAP_YCBCR(ixw_rgb_to_ycbcr,  alwan_rgb_to_ycbcr_map_interleave_ex)
IXWRAP_YCBCR(ixw_ycbcr_to_rgb,  alwan_ycbcr_to_rgb_map_interleave_ex)
IXWRAP_10B(ixw_rgb_to_yccbccrc, alwan_rgb_to_yccbccrc_map_interleave_ex)
IXWRAP_10B(ixw_yccbccrc_to_rgb, alwan_yccbccrc_to_rgb_map_interleave_ex)
IXWRAP_10B(ixw_ycbcr_f2l,       alwan_ycbcr_full_to_legal_map_interleave_ex)
IXWRAP_10B(ixw_ycbcr_l2f,       alwan_ycbcr_legal_to_full_map_interleave_ex)

static int ixw_mat3(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_mat3_transform_map_interleave_ex(o, of, &g_bench_mat, i, inf, n, si, so);
}
static int ixw_cvd_brettel(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_simulate_cvd_map_interleave_ex(o, of, i, inf, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(0.8), n, si, so);
}
static int ixw_cvd_protan(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_simulate_protanopia_map_interleave_ex(o, of, i, inf, ALWAN_LITERAL(0.8), n, si, so);
}
static int ixw_cvd_deutan(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_simulate_deuteranopia_map_interleave_ex(o, of, i, inf, ALWAN_LITERAL(0.8), n, si, so);
}
static int ixw_cvd_tritan(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_simulate_tritanopia_map_interleave_ex(o, of, i, inf, ALWAN_LITERAL(0.8), n, si, so);
}
static int ixw_lgg(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_lgg_apply_map_interleave_ex(o, of, i, inf, &g_lgg_lift, &g_lgg_gamma, &g_lgg_gain, n, si, so);
}
static int ixw_color_matrix(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_color_matrix_apply_map_interleave_ex(o, of, i, inf, &g_bench_mat, n, si, so);
}
static int ixw_printer_lights(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_printer_lights_apply_map_interleave_ex(o, of, i, inf,
        ALWAN_LITERAL(1.1), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.9), n, si, so);
}
static int ixw_white_balance(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_white_balance_apply_map_interleave_ex(o, of, i, inf, &g_wb_mults, n, si, so);
}

/* Interleave _ex wrappers for the 5 newly-implemented _ex functions */
static int ixw_gamut_clip(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_gamut_map_interleave_ex(o, of, ALWAN_GAMUT_MAP_CLIP, i, inf, n, si, so);
}
static int ixw_css_gamut(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_css_gamut_map_interleave_ex(o, of, i, inf, n, si, so);
}
static int ixw_cvd_machado(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_simulate_cvd_machado_map_interleave_ex(o, of, i, inf, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(0.8), n, si, so);
}
static int ixw_cmy_to_cmyk(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_cmy_to_cmyk_map_interleave_ex(o, of, i, inf, n, si, so);
}
static int ixw_cmyk_to_cmy(void *o, alwan_pixel_format of, void const *i, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_cmyk_to_cmy_map_interleave_ex(o, of, i, inf, n, si, so);
}

/* ================================================================
 * Planar _ex wrappers (extra-param planar maps -> planar_ex_fn)
 * ================================================================ */

#define PXWRAP_WHITE(name, api) \
static int name(void *o0, void *o1, void *o2, alwan_pixel_format of, \
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf, \
    size_t n, size_t si, size_t so) { return api(o0, o1, o2, of, i0, i1, i2, inf, &g_d65, n, si, so); }
#define PXWRAP_PQ(name, api) \
static int name(void *o0, void *o1, void *o2, alwan_pixel_format of, \
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf, \
    size_t n, size_t si, size_t so) { return api(o0, o1, o2, of, i0, i1, i2, inf, 1, n, si, so); }
#define PXWRAP_DIN(name, api) \
static int name(void *o0, void *o1, void *o2, alwan_pixel_format of, \
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf, \
    size_t n, size_t si, size_t so) { return api(o0, o1, o2, of, i0, i1, i2, inf, 0, n, si, so); }
#define PXWRAP_YCBCR(name, api) \
static int name(void *o0, void *o1, void *o2, alwan_pixel_format of, \
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf, \
    size_t n, size_t si, size_t so) { return api(o0, o1, o2, of, i0, i1, i2, inf, ALWAN_YCBCR_BT709, n, si, so); }
#define PXWRAP_10B(name, api) \
static int name(void *o0, void *o1, void *o2, alwan_pixel_format of, \
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf, \
    size_t n, size_t si, size_t so) { return api(o0, o1, o2, of, i0, i1, i2, inf, 10, n, si, so); }

#define PX(fn) (planar_ex_fn)(fn)

PXWRAP_WHITE(pxw_xyz_to_lab,    alwan_xyz_to_lab_map_planar_ex)
PXWRAP_WHITE(pxw_lab_to_xyz,    alwan_lab_to_xyz_map_planar_ex)
PXWRAP_WHITE(pxw_xyz_to_luv,    alwan_xyz_to_luv_map_planar_ex)
PXWRAP_WHITE(pxw_luv_to_xyz,    alwan_luv_to_xyz_map_planar_ex)
PXWRAP_WHITE(pxw_xyz_to_uvw,    alwan_xyz_to_uvw_map_planar_ex)
PXWRAP_WHITE(pxw_uvw_to_xyz,    alwan_uvw_to_xyz_map_planar_ex)
PXWRAP_WHITE(pxw_xyz_to_hlab_c, alwan_xyz_to_hunter_lab_custom_map_planar_ex)
PXWRAP_WHITE(pxw_hlab_to_xyz_c, alwan_hunter_lab_to_xyz_custom_map_planar_ex)
PXWRAP_WHITE(pxw_xyz_to_plab_c, alwan_xyz_to_prolab_custom_map_planar_ex)
PXWRAP_WHITE(pxw_plab_to_xyz_c, alwan_prolab_to_xyz_custom_map_planar_ex)
PXWRAP_PQ(pxw_rgb_to_ictcp,     alwan_rgb_to_ictcp_map_planar_ex)
PXWRAP_PQ(pxw_ictcp_to_rgb,     alwan_ictcp_to_rgb_map_planar_ex)
PXWRAP_PQ(pxw_xyz_to_ictcp,     alwan_xyz_to_ictcp_map_planar_ex)
PXWRAP_PQ(pxw_ictcp_to_xyz,     alwan_ictcp_to_xyz_map_planar_ex)
PXWRAP_DIN(pxw_lab_to_din99,    alwan_lab_to_din99_map_planar_ex)
PXWRAP_DIN(pxw_din99_to_lab,    alwan_din99_to_lab_map_planar_ex)
PXWRAP_YCBCR(pxw_rgb_to_ycbcr,  alwan_rgb_to_ycbcr_map_planar_ex)
PXWRAP_YCBCR(pxw_ycbcr_to_rgb,  alwan_ycbcr_to_rgb_map_planar_ex)
PXWRAP_10B(pxw_rgb_to_yccbccrc, alwan_rgb_to_yccbccrc_map_planar_ex)
PXWRAP_10B(pxw_yccbccrc_to_rgb, alwan_yccbccrc_to_rgb_map_planar_ex)
PXWRAP_10B(pxw_ycbcr_f2l,       alwan_ycbcr_full_to_legal_map_planar_ex)
PXWRAP_10B(pxw_ycbcr_l2f,       alwan_ycbcr_legal_to_full_map_planar_ex)

static int pxw_cvd_brettel(void *o0, void *o1, void *o2, alwan_pixel_format of,
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_simulate_cvd_map_planar_ex(o0, o1, o2, of, i0, i1, i2, inf, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(0.8), n, si, so);
}
static int pxw_cvd_protan(void *o0, void *o1, void *o2, alwan_pixel_format of,
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_simulate_protanopia_map_planar_ex(o0, o1, o2, of, i0, i1, i2, inf, ALWAN_LITERAL(0.8), n, si, so);
}
static int pxw_cvd_deutan(void *o0, void *o1, void *o2, alwan_pixel_format of,
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_simulate_deuteranopia_map_planar_ex(o0, o1, o2, of, i0, i1, i2, inf, ALWAN_LITERAL(0.8), n, si, so);
}
static int pxw_cvd_tritan(void *o0, void *o1, void *o2, alwan_pixel_format of,
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_simulate_tritanopia_map_planar_ex(o0, o1, o2, of, i0, i1, i2, inf, ALWAN_LITERAL(0.8), n, si, so);
}
static int pxw_cvd_machado(void *o0, void *o1, void *o2, alwan_pixel_format of,
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_simulate_cvd_machado_map_planar_ex(o0, o1, o2, of, i0, i1, i2, inf, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(0.8), n, si, so);
}
static int pxw_gamut_clip(void *o0, void *o1, void *o2, alwan_pixel_format of,
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_gamut_map_planar_ex(o0, o1, o2, of, ALWAN_GAMUT_MAP_CLIP, i0, i1, i2, inf, n, si, so);
}
static int pxw_css_gamut(void *o0, void *o1, void *o2, alwan_pixel_format of,
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_css_gamut_map_planar_ex(o0, o1, o2, of, i0, i1, i2, inf, n, si, so);
}
static int pxw_lgg(void *o0, void *o1, void *o2, alwan_pixel_format of,
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_lgg_apply_map_planar_ex(o0, o1, o2, of, i0, i1, i2, inf, &g_lgg_lift, &g_lgg_gamma, &g_lgg_gain, n, si, so);
}
static int pxw_color_matrix(void *o0, void *o1, void *o2, alwan_pixel_format of,
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_color_matrix_apply_map_planar_ex(o0, o1, o2, of, i0, i1, i2, inf, &g_bench_mat, n, si, so);
}
static int pxw_printer_lights(void *o0, void *o1, void *o2, alwan_pixel_format of,
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_printer_lights_apply_map_planar_ex(o0, o1, o2, of, i0, i1, i2, inf,
        ALWAN_LITERAL(1.1), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.9), n, si, so);
}
static int pxw_white_balance(void *o0, void *o1, void *o2, alwan_pixel_format of,
    void const *i0, void const *i1, void const *i2, alwan_pixel_format inf,
    size_t n, size_t si, size_t so) {
    return alwan_white_balance_apply_map_planar_ex(o0, o1, o2, of, i0, i1, i2, inf, &g_wb_mults, n, si, so);
}

/* ================================================================
 * Unified map table
 * ================================================================ */

#define I(fn) (interleave_fn)(fn)
#define P(fn) (planar_fn)(fn)
#define IX(fn) (interleave_ex_fn)(fn)

typedef struct {
    char const      *name;
    interleave_fn    interleave;
    planar_fn        planar;         /* NULL if no planar variant */
    perpx_fn         per_pixel;      /* NULL if no per-pixel API  */
    interleave_ex_fn interleave_ex;  /* NULL if no _ex variant    */
    planar_ex_fn     planar_ex;      /* NULL if no planar _ex     */
    int              in_ch;
    int              out_ch;
    int              has_simd;       /* 1 = real SIMD vectorization, 0 = scalar loop */
} map_entry;

static map_entry const g_maps[] = {
    /* name, interleave, planar, per_pixel, interleave_ex, planar_ex, in_ch, out_ch, has_simd */

    /* Core / utility */
    { "mat3 transform",              iw_mat3,         NULL,                     pp_mat3,            ixw_mat3,      NULL,          3, 3, 1 },
    { "sRGB OETF",                   iw_srgb_oetf,    NULL,                     pp_srgb_oetf,       NULL,          NULL,          3, 3, 0 },

    /* sRGB convenience */
    { "sRGB -> XYZ",                 I(alwan_srgb_to_xyz_map_interleave),   P(alwan_srgb_to_xyz_map_planar),   pp_srgb_to_xyz,   IX(alwan_srgb_to_xyz_map_interleave_ex),   PX(alwan_srgb_to_xyz_map_planar_ex),   3, 3, 1 },
    { "XYZ -> sRGB",                 I(alwan_xyz_to_srgb_map_interleave),   P(alwan_xyz_to_srgb_map_planar),   pp_xyz_to_srgb,   IX(alwan_xyz_to_srgb_map_interleave_ex),   PX(alwan_xyz_to_srgb_map_planar_ex),   3, 3, 1 },
    { "sRGB -> CIELAB",              I(alwan_srgb_to_lab_map_interleave),   P(alwan_srgb_to_lab_map_planar),   pp_srgb_to_lab,   IX(alwan_srgb_to_lab_map_interleave_ex),   PX(alwan_srgb_to_lab_map_planar_ex),   3, 3, 1 },
    { "CIELAB -> sRGB",              I(alwan_lab_to_srgb_map_interleave),   P(alwan_lab_to_srgb_map_planar),   pp_lab_to_srgb,   IX(alwan_lab_to_srgb_map_interleave_ex),   PX(alwan_lab_to_srgb_map_planar_ex),   3, 3, 1 },
    { "sRGB -> Oklab",               I(alwan_srgb_to_oklab_map_interleave), P(alwan_srgb_to_oklab_map_planar), pp_srgb_to_oklab, IX(alwan_srgb_to_oklab_map_interleave_ex), PX(alwan_srgb_to_oklab_map_planar_ex), 3, 3, 1 },
    { "Oklab -> sRGB",               I(alwan_oklab_to_srgb_map_interleave), P(alwan_oklab_to_srgb_map_planar), pp_oklab_to_srgb, IX(alwan_oklab_to_srgb_map_interleave_ex), PX(alwan_oklab_to_srgb_map_planar_ex), 3, 3, 1 },

    /* CIE colorimetry (D65) */
    { "XYZ -> Lab (D65)",            iw_xyz_to_lab,   pw_xyz_to_lab,            pp_xyz_to_lab,      ixw_xyz_to_lab,   pxw_xyz_to_lab,   3, 3, 1 },
    { "Lab -> XYZ (D65)",            iw_lab_to_xyz,   pw_lab_to_xyz,            pp_lab_to_xyz,      ixw_lab_to_xyz,   pxw_lab_to_xyz,   3, 3, 1 },
    { "XYZ -> Luv (D65)",            iw_xyz_to_luv,   pw_xyz_to_luv,            pp_xyz_to_luv,      ixw_xyz_to_luv,   pxw_xyz_to_luv,   3, 3, 1 },
    { "Luv -> XYZ (D65)",            iw_luv_to_xyz,   pw_luv_to_xyz,            pp_luv_to_xyz,      ixw_luv_to_xyz,   pxw_luv_to_xyz,   3, 3, 1 },
    { "Lab -> LCH",                  I(alwan_lab_to_lch_map_interleave),    P(alwan_lab_to_lch_map_planar),    pp_lab_to_lch,    IX(alwan_lab_to_lch_map_interleave_ex),    PX(alwan_lab_to_lch_map_planar_ex),    3, 3, 1 },
    { "LCH -> Lab",                  I(alwan_lch_to_lab_map_interleave),    P(alwan_lch_to_lab_map_planar),    pp_lch_to_lab,    IX(alwan_lch_to_lab_map_interleave_ex),    PX(alwan_lch_to_lab_map_planar_ex),    3, 3, 1 },
    { "Luv -> LCHuv",               I(alwan_luv_to_lchuv_map_interleave),  P(alwan_luv_to_lchuv_map_planar),  pp_luv_to_lchuv,  IX(alwan_luv_to_lchuv_map_interleave_ex),  PX(alwan_luv_to_lchuv_map_planar_ex),  3, 3, 1 },
    { "LCHuv -> Luv",               I(alwan_lchuv_to_luv_map_interleave),  P(alwan_lchuv_to_luv_map_planar),  pp_lchuv_to_luv,  IX(alwan_lchuv_to_luv_map_interleave_ex),  PX(alwan_lchuv_to_luv_map_planar_ex),  3, 3, 1 },
    { "XYZ -> xyY",                  I(alwan_xyz_to_xyy_map_interleave),    P(alwan_xyz_to_xyy_map_planar),    pp_xyz_to_xyy,    IX(alwan_xyz_to_xyy_map_interleave_ex),    PX(alwan_xyz_to_xyy_map_planar_ex),    3, 3, 1 },
    { "xyY -> XYZ",                  I(alwan_xyy_to_xyz_map_interleave),    P(alwan_xyy_to_xyz_map_planar),    pp_xyy_to_xyz,    IX(alwan_xyy_to_xyz_map_interleave_ex),    PX(alwan_xyy_to_xyz_map_planar_ex),    3, 3, 1 },

    /* Oklab / OkLCH */
    { "XYZ -> Oklab",                I(alwan_xyz_to_oklab_map_interleave),  P(alwan_xyz_to_oklab_map_planar),  pp_xyz_to_oklab,  IX(alwan_xyz_to_oklab_map_interleave_ex),  PX(alwan_xyz_to_oklab_map_planar_ex),  3, 3, 1 },
    { "Oklab -> XYZ",                I(alwan_oklab_to_xyz_map_interleave),  P(alwan_oklab_to_xyz_map_planar),  pp_oklab_to_xyz,  IX(alwan_oklab_to_xyz_map_interleave_ex),  PX(alwan_oklab_to_xyz_map_planar_ex),  3, 3, 1 },
    { "Oklab -> OkLCH",              I(alwan_oklab_to_oklch_map_interleave),P(alwan_oklab_to_oklch_map_planar),pp_oklab_to_oklch,IX(alwan_oklab_to_oklch_map_interleave_ex),PX(alwan_oklab_to_oklch_map_planar_ex),3, 3, 1 },
    { "OkLCH -> Oklab",              I(alwan_oklch_to_oklab_map_interleave),P(alwan_oklch_to_oklab_map_planar),pp_oklch_to_oklab,IX(alwan_oklch_to_oklab_map_interleave_ex),PX(alwan_oklch_to_oklab_map_planar_ex),3, 3, 1 },

    /* JzAzBz / Jzczhz */
    { "XYZ -> JzAzBz",              I(alwan_xyz_to_jzazbz_map_interleave),  P(alwan_xyz_to_jzazbz_map_planar),  pp_xyz_to_jzazbz,    IX(alwan_xyz_to_jzazbz_map_interleave_ex),  PX(alwan_xyz_to_jzazbz_map_planar_ex),  3, 3, 1 },
    { "JzAzBz -> XYZ",              I(alwan_jzazbz_to_xyz_map_interleave),  P(alwan_jzazbz_to_xyz_map_planar),  pp_jzazbz_to_xyz,    IX(alwan_jzazbz_to_xyz_map_interleave_ex),  PX(alwan_jzazbz_to_xyz_map_planar_ex),  3, 3, 1 },
    { "JzAzBz -> Jzczhz",           I(alwan_jzazbz_to_jzczhz_map_interleave), P(alwan_jzazbz_to_jzczhz_map_planar), pp_jzazbz_to_jzczhz, IX(alwan_jzazbz_to_jzczhz_map_interleave_ex), PX(alwan_jzazbz_to_jzczhz_map_planar_ex), 3, 3, 1 },
    { "Jzczhz -> JzAzBz",           I(alwan_jzczhz_to_jzazbz_map_interleave), P(alwan_jzczhz_to_jzazbz_map_planar), pp_jzczhz_to_jzazbz, IX(alwan_jzczhz_to_jzazbz_map_interleave_ex), PX(alwan_jzczhz_to_jzazbz_map_planar_ex), 3, 3, 1 },

    /* IPT */
    { "XYZ -> IPT",                  I(alwan_xyz_to_ipt_map_interleave),    P(alwan_xyz_to_ipt_map_planar),    pp_xyz_to_ipt,   IX(alwan_xyz_to_ipt_map_interleave_ex),    PX(alwan_xyz_to_ipt_map_planar_ex),    3, 3, 1 },
    { "IPT -> XYZ",                  I(alwan_ipt_to_xyz_map_interleave),    P(alwan_ipt_to_xyz_map_planar),    pp_ipt_to_xyz,   IX(alwan_ipt_to_xyz_map_interleave_ex),    PX(alwan_ipt_to_xyz_map_planar_ex),    3, 3, 1 },

    /* ICtCp (PQ) */
    { "RGB -> ICtCp (PQ)",           iw_rgb_to_ictcp, pw_rgb_to_ictcp,      pp_rgb_to_ictcp,  ixw_rgb_to_ictcp,  pxw_rgb_to_ictcp,  3, 3, 1 },
    { "ICtCp -> RGB (PQ)",           iw_ictcp_to_rgb, pw_ictcp_to_rgb,      pp_ictcp_to_rgb,  ixw_ictcp_to_rgb,  pxw_ictcp_to_rgb,  3, 3, 1 },
    { "XYZ -> ICtCp (PQ)",           iw_xyz_to_ictcp, pw_xyz_to_ictcp,      pp_xyz_to_ictcp,  ixw_xyz_to_ictcp,  pxw_xyz_to_ictcp,  3, 3, 1 },
    { "ICtCp -> XYZ (PQ)",           iw_ictcp_to_xyz, pw_ictcp_to_xyz,      pp_ictcp_to_xyz,  ixw_ictcp_to_xyz,  pxw_ictcp_to_xyz,  3, 3, 1 },

    /* Extended color spaces */
    { "XYZ -> IgPgTg",              I(alwan_xyz_to_igpgtg_map_interleave),  P(alwan_xyz_to_igpgtg_map_planar),  pp_xyz_to_igpgtg, IX(alwan_xyz_to_igpgtg_map_interleave_ex), PX(alwan_xyz_to_igpgtg_map_planar_ex), 3, 3, 1 },
    { "IgPgTg -> XYZ",              I(alwan_igpgtg_to_xyz_map_interleave),  P(alwan_igpgtg_to_xyz_map_planar),  pp_igpgtg_to_xyz, IX(alwan_igpgtg_to_xyz_map_interleave_ex), PX(alwan_igpgtg_to_xyz_map_planar_ex), 3, 3, 1 },
    { "XYZ -> IaCaCb",               I(alwan_xyz_to_icacb_map_interleave),   P(alwan_xyz_to_icacb_map_planar),   pp_xyz_to_icacb,  IX(alwan_xyz_to_icacb_map_interleave_ex),  PX(alwan_xyz_to_icacb_map_planar_ex),  3, 3, 1 },
    { "IaCaCb -> XYZ",               I(alwan_icacb_to_xyz_map_interleave),   P(alwan_icacb_to_xyz_map_planar),   pp_icacb_to_xyz,  IX(alwan_icacb_to_xyz_map_interleave_ex),  PX(alwan_icacb_to_xyz_map_planar_ex),  3, 3, 1 },
    { "XYZ -> HDR CIELAB",          I(alwan_xyz_to_hdr_cielab_map_interleave), P(alwan_xyz_to_hdr_cielab_map_planar), pp_xyz_to_hdr_cielab, IX(alwan_xyz_to_hdr_cielab_map_interleave_ex), PX(alwan_xyz_to_hdr_cielab_map_planar_ex), 3, 3, 1 },
    { "HDR CIELAB -> XYZ",          I(alwan_hdr_cielab_to_xyz_map_interleave), P(alwan_hdr_cielab_to_xyz_map_planar), pp_hdr_cielab_to_xyz, IX(alwan_hdr_cielab_to_xyz_map_interleave_ex), PX(alwan_hdr_cielab_to_xyz_map_planar_ex), 3, 3, 1 },
    { "XYZ -> HDR IPT",             I(alwan_xyz_to_hdr_ipt_map_interleave), P(alwan_xyz_to_hdr_ipt_map_planar), pp_xyz_to_hdr_ipt, IX(alwan_xyz_to_hdr_ipt_map_interleave_ex), PX(alwan_xyz_to_hdr_ipt_map_planar_ex), 3, 3, 1 },
    { "HDR IPT -> XYZ",             I(alwan_hdr_ipt_to_xyz_map_interleave), P(alwan_hdr_ipt_to_xyz_map_planar), pp_hdr_ipt_to_xyz, IX(alwan_hdr_ipt_to_xyz_map_interleave_ex), PX(alwan_hdr_ipt_to_xyz_map_planar_ex), 3, 3, 1 },
    { "XYZ -> UCS",                  I(alwan_xyz_to_ucs_map_interleave),     P(alwan_xyz_to_ucs_map_planar),     pp_xyz_to_ucs,    IX(alwan_xyz_to_ucs_map_interleave_ex),    PX(alwan_xyz_to_ucs_map_planar_ex),    3, 3, 1 },
    { "UCS -> XYZ",                  I(alwan_ucs_to_xyz_map_interleave),     P(alwan_ucs_to_xyz_map_planar),     pp_ucs_to_xyz,    IX(alwan_ucs_to_xyz_map_interleave_ex),    PX(alwan_ucs_to_xyz_map_planar_ex),    3, 3, 1 },
    { "XYZ -> OSA-UCS",             I(alwan_xyz_to_osa_ucs_map_interleave), P(alwan_xyz_to_osa_ucs_map_planar), pp_xyz_to_osa_ucs, IX(alwan_xyz_to_osa_ucs_map_interleave_ex), PX(alwan_xyz_to_osa_ucs_map_planar_ex), 3, 3, 1 },
    { "OSA-UCS -> XYZ",             I(alwan_osa_ucs_to_xyz_map_interleave), P(alwan_osa_ucs_to_xyz_map_planar), pp_osa_ucs_to_xyz, IX(alwan_osa_ucs_to_xyz_map_interleave_ex), PX(alwan_osa_ucs_to_xyz_map_planar_ex), 3, 3, 1 },
    { "XYZ -> Hunter Lab",          I(alwan_xyz_to_hunter_lab_map_interleave), P(alwan_xyz_to_hunter_lab_map_planar), pp_xyz_to_hunter_lab, IX(alwan_xyz_to_hunter_lab_map_interleave_ex), PX(alwan_xyz_to_hunter_lab_map_planar_ex), 3, 3, 1 },
    { "Hunter Lab -> XYZ",          I(alwan_hunter_lab_to_xyz_map_interleave), P(alwan_hunter_lab_to_xyz_map_planar), pp_hunter_lab_to_xyz, IX(alwan_hunter_lab_to_xyz_map_interleave_ex), PX(alwan_hunter_lab_to_xyz_map_planar_ex), 3, 3, 1 },
    { "XYZ -> Hunter Lab (D65)",     iw_xyz_to_hlab_c, pw_xyz_to_hlab_c,     pp_xyz_to_hlab_c,  ixw_xyz_to_hlab_c,  pxw_xyz_to_hlab_c,  3, 3, 1 },
    { "Hunter Lab -> XYZ (D65)",     iw_hlab_to_xyz_c, pw_hlab_to_xyz_c,     pp_hlab_to_xyz_c,  ixw_hlab_to_xyz_c,  pxw_hlab_to_xyz_c,  3, 3, 1 },
    { "XYZ -> ProLab",              I(alwan_xyz_to_prolab_map_interleave),   P(alwan_xyz_to_prolab_map_planar),   pp_xyz_to_prolab,  IX(alwan_xyz_to_prolab_map_interleave_ex),  PX(alwan_xyz_to_prolab_map_planar_ex),  3, 3, 1 },
    { "ProLab -> XYZ",              I(alwan_prolab_to_xyz_map_interleave),   P(alwan_prolab_to_xyz_map_planar),   pp_prolab_to_xyz,  IX(alwan_prolab_to_xyz_map_interleave_ex),  PX(alwan_prolab_to_xyz_map_planar_ex),  3, 3, 1 },
    { "XYZ -> ProLab (D65)",         iw_xyz_to_plab_c, pw_xyz_to_plab_c,     pp_xyz_to_plab_c,  ixw_xyz_to_plab_c,  pxw_xyz_to_plab_c,  3, 3, 1 },
    { "ProLab -> XYZ (D65)",         iw_plab_to_xyz_c, pw_plab_to_xyz_c,     pp_plab_to_xyz_c,  ixw_plab_to_xyz_c,  pxw_plab_to_xyz_c,  3, 3, 1 },
    { "XYZ -> UVW (D65)",            iw_xyz_to_uvw,   pw_xyz_to_uvw,         pp_xyz_to_uvw,     ixw_xyz_to_uvw,     pxw_xyz_to_uvw,     3, 3, 1 },
    { "UVW -> XYZ (D65)",            iw_uvw_to_xyz,   pw_uvw_to_xyz,         pp_uvw_to_xyz,     ixw_uvw_to_xyz,     pxw_uvw_to_xyz,     3, 3, 1 },

    /* DIN99 */
    { "Lab -> DIN99",                iw_lab_to_din99, pw_lab_to_din99,        pp_lab_to_din99,   ixw_lab_to_din99,   pxw_lab_to_din99,   3, 3, 1 },
    { "DIN99 -> Lab",                iw_din99_to_lab, pw_din99_to_lab,        pp_din99_to_lab,   ixw_din99_to_lab,   pxw_din99_to_lab,   3, 3, 1 },

    /* RGB-derived: Prismatic, HCL, IHLS */
    { "RGB -> Prismatic",           I(alwan_rgb_to_prismatic_map_interleave), P(alwan_rgb_to_prismatic_map_planar), pp_rgb_to_prismatic, IX(alwan_rgb_to_prismatic_map_interleave_ex), PX(alwan_rgb_to_prismatic_map_planar_ex), 3, 3, 1 },
    { "Prismatic -> RGB",           I(alwan_prismatic_to_rgb_map_interleave), P(alwan_prismatic_to_rgb_map_planar), pp_prismatic_to_rgb, IX(alwan_prismatic_to_rgb_map_interleave_ex), PX(alwan_prismatic_to_rgb_map_planar_ex), 3, 3, 1 },
    { "RGB -> HCL",                 I(alwan_rgb_to_hcl_map_interleave),      P(alwan_rgb_to_hcl_map_planar),      pp_rgb_to_hcl,   IX(alwan_rgb_to_hcl_map_interleave_ex),   PX(alwan_rgb_to_hcl_map_planar_ex),   3, 3, 1 },
    { "HCL -> RGB",                 I(alwan_hcl_to_rgb_map_interleave),      P(alwan_hcl_to_rgb_map_planar),      pp_hcl_to_rgb,   IX(alwan_hcl_to_rgb_map_interleave_ex),   PX(alwan_hcl_to_rgb_map_planar_ex),   3, 3, 1 },
    { "RGB -> IHLS",                I(alwan_rgb_to_ihls_map_interleave),     P(alwan_rgb_to_ihls_map_planar),     pp_rgb_to_ihls,  IX(alwan_rgb_to_ihls_map_interleave_ex),  PX(alwan_rgb_to_ihls_map_planar_ex),  3, 3, 1 },
    { "IHLS -> RGB",                I(alwan_ihls_to_rgb_map_interleave),     P(alwan_ihls_to_rgb_map_planar),     pp_ihls_to_rgb,  IX(alwan_ihls_to_rgb_map_interleave_ex),  PX(alwan_ihls_to_rgb_map_planar_ex),  3, 3, 1 },

    /* HSV / HSL */
    { "RGB -> HSV",                 I(alwan_rgb_to_hsv_map_interleave),      P(alwan_rgb_to_hsv_map_planar),      pp_rgb_to_hsv,     IX(alwan_rgb_to_hsv_map_interleave_ex),     PX(alwan_rgb_to_hsv_map_planar_ex),     3, 3, 1 },
    { "HSV -> RGB",                 I(alwan_hsv_to_rgb_map_interleave),      P(alwan_hsv_to_rgb_map_planar),      pp_hsv_to_rgb,     IX(alwan_hsv_to_rgb_map_interleave_ex),     PX(alwan_hsv_to_rgb_map_planar_ex),     3, 3, 1 },
    { "RGB -> HSL",                 I(alwan_rgb_to_hsl_map_interleave),      P(alwan_rgb_to_hsl_map_planar),      pp_rgb_to_hsl,     IX(alwan_rgb_to_hsl_map_interleave_ex),     PX(alwan_rgb_to_hsl_map_planar_ex),     3, 3, 1 },
    { "HSL -> RGB",                 I(alwan_hsl_to_rgb_map_interleave),      P(alwan_hsl_to_rgb_map_planar),      pp_hsl_to_rgb,     IX(alwan_hsl_to_rgb_map_interleave_ex),     PX(alwan_hsl_to_rgb_map_planar_ex),     3, 3, 1 },
    { "RGB -> HSP",                 I(alwan_rgb_to_hsp_map_interleave),      P(alwan_rgb_to_hsp_map_planar),      pp_rgb_to_hsp,     IX(alwan_rgb_to_hsp_map_interleave_ex),     PX(alwan_rgb_to_hsp_map_planar_ex),     3, 3, 1 },
    { "HSP -> RGB",                 I(alwan_hsp_to_rgb_map_interleave),      P(alwan_hsp_to_rgb_map_planar),      pp_hsp_to_rgb,     IX(alwan_hsp_to_rgb_map_interleave_ex),     PX(alwan_hsp_to_rgb_map_planar_ex),     3, 3, 1 },
    { "RGB -> HSPlog",              I(alwan_rgb_to_hsplog_map_interleave),   P(alwan_rgb_to_hsplog_map_planar),   pp_rgb_to_hsplog,  IX(alwan_rgb_to_hsplog_map_interleave_ex),  PX(alwan_rgb_to_hsplog_map_planar_ex),  3, 3, 1 },
    { "HSPlog -> RGB",              I(alwan_hsplog_to_rgb_map_interleave),   P(alwan_hsplog_to_rgb_map_planar),   pp_hsplog_to_rgb,  IX(alwan_hsplog_to_rgb_map_interleave_ex),  PX(alwan_hsplog_to_rgb_map_planar_ex),  3, 3, 1 },
    { "RGB -> HSY",                 I(alwan_rgb_to_hsy_map_interleave),      P(alwan_rgb_to_hsy_map_planar),      pp_rgb_to_hsy,     IX(alwan_rgb_to_hsy_map_interleave_ex),     PX(alwan_rgb_to_hsy_map_planar_ex),     3, 3, 1 },
    { "HSY -> RGB",                 I(alwan_hsy_to_rgb_map_interleave),      P(alwan_hsy_to_rgb_map_planar),      pp_hsy_to_rgb,     IX(alwan_hsy_to_rgb_map_interleave_ex),     PX(alwan_hsy_to_rgb_map_planar_ex),     3, 3, 1 },
    { "Linear sRGB -> HSV",         I(alwan_linear_srgb_to_hsv_map_interleave), P(alwan_linear_srgb_to_hsv_map_planar), pp_linsrgb_to_hsv, IX(alwan_linear_srgb_to_hsv_map_interleave_ex), PX(alwan_linear_srgb_to_hsv_map_planar_ex), 3, 3, 1 },
    { "HSV -> Linear sRGB",         I(alwan_hsv_to_linear_srgb_map_interleave), P(alwan_hsv_to_linear_srgb_map_planar), pp_hsv_to_linsrgb, IX(alwan_hsv_to_linear_srgb_map_interleave_ex), PX(alwan_hsv_to_linear_srgb_map_planar_ex), 3, 3, 1 },
    { "Linear sRGB -> HSL",         I(alwan_linear_srgb_to_hsl_map_interleave), P(alwan_linear_srgb_to_hsl_map_planar), pp_linsrgb_to_hsl, IX(alwan_linear_srgb_to_hsl_map_interleave_ex), PX(alwan_linear_srgb_to_hsl_map_planar_ex), 3, 3, 1 },
    { "HSL -> Linear sRGB",         I(alwan_hsl_to_linear_srgb_map_interleave), P(alwan_hsl_to_linear_srgb_map_planar), pp_hsl_to_linsrgb, IX(alwan_hsl_to_linear_srgb_map_interleave_ex), PX(alwan_hsl_to_linear_srgb_map_planar_ex), 3, 3, 1 },

    /* CMY / CMYK / YCoCg / HWB */
    { "RGB -> CMY",                 I(alwan_rgb_to_cmy_map_interleave),      P(alwan_rgb_to_cmy_map_planar),      pp_rgb_to_cmy,   IX(alwan_rgb_to_cmy_map_interleave_ex),   PX(alwan_rgb_to_cmy_map_planar_ex),   3, 3, 1 },
    { "CMY -> RGB",                 I(alwan_cmy_to_rgb_map_interleave),      P(alwan_cmy_to_rgb_map_planar),      pp_cmy_to_rgb,   IX(alwan_cmy_to_rgb_map_interleave_ex),   PX(alwan_cmy_to_rgb_map_planar_ex),   3, 3, 1 },
    { "CMY -> CMYK",                I(alwan_cmy_to_cmyk_map_interleave),     pw_cmy_to_cmyk,                      pp_cmy_to_cmyk,  ixw_cmy_to_cmyk,  NULL,  3, 4, 1 },
    { "CMYK -> CMY",                I(alwan_cmyk_to_cmy_map_interleave),     pw_cmyk_to_cmy,                      pp_cmyk_to_cmy,  ixw_cmyk_to_cmy,  NULL,  4, 3, 1 },
    { "RGB -> YCoCg",               I(alwan_rgb_to_ycocg_map_interleave),    P(alwan_rgb_to_ycocg_map_planar),    pp_rgb_to_ycocg, IX(alwan_rgb_to_ycocg_map_interleave_ex), PX(alwan_rgb_to_ycocg_map_planar_ex), 3, 3, 1 },
    { "YCoCg -> RGB",               I(alwan_ycocg_to_rgb_map_interleave),    P(alwan_ycocg_to_rgb_map_planar),    pp_ycocg_to_rgb, IX(alwan_ycocg_to_rgb_map_interleave_ex), PX(alwan_ycocg_to_rgb_map_planar_ex), 3, 3, 1 },
    { "RGB -> HWB",                 I(alwan_rgb_to_hwb_map_interleave),      P(alwan_rgb_to_hwb_map_planar),      pp_rgb_to_hwb,   IX(alwan_rgb_to_hwb_map_interleave_ex),   PX(alwan_rgb_to_hwb_map_planar_ex),   3, 3, 1 },
    { "HWB -> RGB",                 I(alwan_hwb_to_rgb_map_interleave),      P(alwan_hwb_to_rgb_map_planar),      pp_hwb_to_rgb,   IX(alwan_hwb_to_rgb_map_interleave_ex),   PX(alwan_hwb_to_rgb_map_planar_ex),   3, 3, 1 },
    { "HSV -> HWB",                 I(alwan_hsv_to_hwb_map_interleave),      P(alwan_hsv_to_hwb_map_planar),      pp_hsv_to_hwb,   IX(alwan_hsv_to_hwb_map_interleave_ex),   PX(alwan_hsv_to_hwb_map_planar_ex),   3, 3, 1 },
    { "HWB -> HSV",                 I(alwan_hwb_to_hsv_map_interleave),      P(alwan_hwb_to_hsv_map_planar),      pp_hwb_to_hsv,   IX(alwan_hwb_to_hsv_map_interleave_ex),   PX(alwan_hwb_to_hsv_map_planar_ex),   3, 3, 1 },

    /* YCbCr / video */
    { "RGB -> YCbCr (BT.709)",       iw_rgb_to_ycbcr,  pw_rgb_to_ycbcr,     pp_rgb_to_ycbcr,    ixw_rgb_to_ycbcr,    pxw_rgb_to_ycbcr,    3, 3, 1 },
    { "YCbCr -> RGB (BT.709)",       iw_ycbcr_to_rgb,  pw_ycbcr_to_rgb,     pp_ycbcr_to_rgb,    ixw_ycbcr_to_rgb,    pxw_ycbcr_to_rgb,    3, 3, 1 },
    { "RGB -> YcCbcCrc (10b)",       iw_rgb_to_yccbccrc, pw_rgb_to_yccbccrc, pp_rgb_to_yccbccrc, ixw_rgb_to_yccbccrc, pxw_rgb_to_yccbccrc, 3, 3, 1 },
    { "YcCbcCrc -> RGB (10b)",       iw_yccbccrc_to_rgb, pw_yccbccrc_to_rgb, pp_yccbccrc_to_rgb, ixw_yccbccrc_to_rgb, pxw_yccbccrc_to_rgb, 3, 3, 1 },
    { "YCbCr full -> legal (10b)",   iw_ycbcr_f2l,     pw_ycbcr_f2l,        pp_ycbcr_f2l,       ixw_ycbcr_f2l,       pxw_ycbcr_f2l,       3, 3, 1 },
    { "YCbCr legal -> full (10b)",   iw_ycbcr_l2f,     pw_ycbcr_l2f,        pp_ycbcr_l2f,       ixw_ycbcr_l2f,       pxw_ycbcr_l2f,       3, 3, 1 },

    /* Gamut mapping */
    { "Gamut map (clip)",             iw_gamut_clip,    pw_gamut_clip,        pp_gamut_clip,      ixw_gamut_clip,      pxw_gamut_clip,      3, 3, 1 },
    { "CSS gamut map",               I(alwan_css_gamut_map_interleave),       P(alwan_css_gamut_map_planar),      pp_css_gamut,       ixw_css_gamut,       pxw_css_gamut,       3, 3, 1 },

    /* CVD simulation */
    { "CVD Brettel (protan 0.8)",     iw_cvd_brettel,  pw_cvd_brettel,       pp_cvd_brettel,     ixw_cvd_brettel,     pxw_cvd_brettel,     3, 3, 1 },
    { "CVD protanopia (0.8)",         iw_cvd_protan,   pw_cvd_protan,        pp_cvd_protan,      ixw_cvd_protan,      pxw_cvd_protan,      3, 3, 1 },
    { "CVD deuteranopia (0.8)",       iw_cvd_deutan,   pw_cvd_deutan,        pp_cvd_deutan,      ixw_cvd_deutan,      pxw_cvd_deutan,      3, 3, 1 },
    { "CVD tritanopia (0.8)",         iw_cvd_tritan,   pw_cvd_tritan,        pp_cvd_tritan,      ixw_cvd_tritan,      pxw_cvd_tritan,      3, 3, 1 },
    { "CVD Machado (protan 0.8)",     iw_cvd_machado,  pw_cvd_machado,       pp_cvd_machado,     ixw_cvd_machado,     pxw_cvd_machado,     3, 3, 1 },

    /* Color correction */
    { "Color matrix",                 iw_color_matrix, pw_color_matrix,       pp_color_matrix,    ixw_color_matrix,    pxw_color_matrix,    3, 3, 1 },
    { "Lift/Gamma/Gain",              iw_lgg,          pw_lgg,               pp_lgg,             ixw_lgg,             pxw_lgg,             3, 3, 1 },
    { "Printer lights",               iw_printer_lights, pw_printer_lights,  pp_printer_lights,  ixw_printer_lights,  pxw_printer_lights,  3, 3, 1 },
    { "White balance",                iw_white_balance, pw_white_balance,    pp_white_balance,   ixw_white_balance,   pxw_white_balance,   3, 3, 1 },
};

#define MAP_COUNT (sizeof(g_maps) / sizeof(g_maps[0]))

/* ================================================================
 * Benchmark runners
 * ================================================================ */

typedef struct { interleave_fn fn; alwan_scalar *in; alwan_scalar *out; size_t si; size_t so; } ileave_ctx;
static void run_ileave(void *ctx) { ileave_ctx *c = (ileave_ctx *)ctx; c->fn(c->out, c->in, BENCH_PIXELS, c->si, c->so); }

typedef struct { planar_fn fn; alwan_scalar *i0; alwan_scalar *i1; alwan_scalar *i2; alwan_scalar *o0; alwan_scalar *o1; alwan_scalar *o2; } planar_ctx;
static void run_planar(void *ctx) {
    planar_ctx *c = (planar_ctx *)ctx;
    c->fn(c->o0, c->o1, c->o2, c->i0, c->i1, c->i2, BENCH_PIXELS, sizeof(alwan_scalar), sizeof(alwan_scalar));
}

typedef struct { perpx_fn fn; alwan_scalar *in; alwan_scalar *out; int in_ch; int out_ch; } perpx_ctx;
static void run_perpx(void *ctx) {
    perpx_ctx *c = (perpx_ctx *)ctx;
    int p;
    for (p = 0; p < BENCH_PIXELS; p++)
        c->fn(&c->out[p * c->out_ch], &c->in[p * c->in_ch]);
}

typedef struct { interleave_ex_fn fn; void *in; void *out; alwan_pixel_format fmt; size_t si; size_t so; } ileave_ex_ctx;
static void run_ileave_ex(void *ctx) {
    ileave_ex_ctx *c = (ileave_ex_ctx *)ctx;
    c->fn(c->out, c->fmt, c->in, c->fmt, BENCH_PIXELS, c->si, c->so);
}

static size_t pixel_type_size(alwan_pixel_format fmt) {
    switch (fmt) {
        case ALWAN_PIXEL_U8:  return 1;
        case ALWAN_PIXEL_U16: return 2;
        case ALWAN_PIXEL_F16: return 2;
        case ALWAN_PIXEL_F32: return 4;
        case ALWAN_PIXEL_F64: return 8;
        default: return 0;
    }
}

typedef struct { planar_ex_fn fn; void *i0; void *i1; void *i2; void *o0; void *o1; void *o2; alwan_pixel_format fmt; size_t si; size_t so; } planar_ex_ctx;
static void run_planar_ex(void *ctx) {
    planar_ex_ctx *c = (planar_ex_ctx *)ctx;
    c->fn(c->o0, c->o1, c->o2, c->fmt, c->i0, c->i1, c->i2, c->fmt, BENCH_PIXELS, c->si, c->so);
}

/* CMY->CMYK / CMYK->CMY planar_ex (3<->4 channel) runners */
typedef struct {
    void *i0; void *i1; void *i2; void *i3;
    void *o0; void *o1; void *o2; void *o3;
    alwan_pixel_format fmt; size_t si; size_t so;
    int is_cmy_to_cmyk; /* 1 = CMY->CMYK (3in,4out), 0 = CMYK->CMY (4in,3out) */
} cmyk_pex_ctx;

static void run_cmyk_pex(void *ctx) {
    cmyk_pex_ctx *c = (cmyk_pex_ctx *)ctx;
    if (c->is_cmy_to_cmyk)
        alwan_cmy_to_cmyk_map_planar_ex(c->o0, c->o1, c->o2, c->o3, c->fmt,
                                        c->i0, c->i1, c->i2, c->fmt,
                                        BENCH_PIXELS, c->si, c->so);
    else
        alwan_cmyk_to_cmy_map_planar_ex(c->o0, c->o1, c->o2, c->fmt,
                                        c->i0, c->i1, c->i2, c->i3, c->fmt,
                                        BENCH_PIXELS, c->si, c->so);
}

/* ================================================================
 * Image convert (batch only)
 * ================================================================ */

typedef struct {
    uint8_t *src; float *dst;
    size_t src_stride; size_t dst_stride;
    alwan_rgb_space_desc srgb_desc; alwan_rgb_space_desc p3_desc;
} imgconv_ctx;

static void run_imgconv(void *ctx) {
    imgconv_ctx *c = (imgconv_ctx *)ctx;
    alwan_image_convert(c->dst, ALWAN_PIXEL_F32, c->dst_stride,
                        c->src, ALWAN_PIXEL_U8, c->src_stride,
                        BENCH_PIXELS, 1, NULL, &c->srgb_desc, &c->p3_desc);
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    size_t m;
    int max_ch, f;
    alwan_scalar *buf_in, *buf_out;
    /* Planar channel buffers */
    alwan_scalar *ch_in[3], *ch_out[3];
    double *res_il, *res_pl, *res_pp;
    double *res_ex[5]; /* U8, U16, F16, F32, F64 interleave _ex */
    double *res_pex[5]; /* U8, U16, F16, F32, F64 planar _ex */
    void *ex_buf_in, *ex_buf_out;
    void *pex_ch_in[4], *pex_ch_out[4];
    double img_result;
    static alwan_pixel_format const ex_fmts[5] = { ALWAN_PIXEL_U8, ALWAN_PIXEL_U16, ALWAN_PIXEL_F16, ALWAN_PIXEL_F32, ALWAN_PIXEL_F64 };

    timer_init();
    color_init();

    printf("========================================\n");
    printf("Alwan Benchmark Suite\n");
    printf("Scalar type : float64\n");
    printf("SIMD backend: %s (f32 x%d, f64 x%d)\n",
           simd_backend_name(), ALWAN_SIMD_F32_WIDTH, ALWAN_SIMD_F64_WIDTH);
    printf("Pixels/run  : %d (%d warmup, %d measured)\n",
           BENCH_PIXELS, BENCH_WARMUP, BENCH_ITERS);
    printf("========================================\n");

    /* Allocate shared buffers */
    max_ch = 4;
    buf_in  = (alwan_scalar *)malloc((size_t)BENCH_PIXELS * (size_t)max_ch * sizeof(alwan_scalar));
    buf_out = (alwan_scalar *)malloc((size_t)BENCH_PIXELS * (size_t)max_ch * sizeof(alwan_scalar));
    for (m = 0; m < 3; m++) {
        ch_in[m]  = (alwan_scalar *)malloc((size_t)BENCH_PIXELS * sizeof(alwan_scalar));
        ch_out[m] = (alwan_scalar *)malloc((size_t)BENCH_PIXELS * sizeof(alwan_scalar));
    }
    res_il = (double *)calloc(MAP_COUNT, sizeof(double));
    res_pl = (double *)calloc(MAP_COUNT, sizeof(double));
    res_pp = (double *)calloc(MAP_COUNT, sizeof(double));
    for (f = 0; f < 5; f++) {
        res_ex[f] = (double *)calloc(MAP_COUNT, sizeof(double));
        res_pex[f] = (double *)calloc(MAP_COUNT, sizeof(double));
    }

    /* Typed-format (_ex) buffers — large enough for 4-ch F64 */
    ex_buf_in  = malloc((size_t)BENCH_PIXELS * 4 * sizeof(double));
    ex_buf_out = malloc((size_t)BENCH_PIXELS * 4 * sizeof(double));

    /* Planar _ex typed channel buffers — one channel per buffer, max = BENCH_PIXELS * sizeof(double) */
    for (m = 0; m < 4; m++) {
        pex_ch_in[m]  = malloc((size_t)BENCH_PIXELS * sizeof(double));
        pex_ch_out[m] = malloc((size_t)BENCH_PIXELS * sizeof(double));
    }

    if (!buf_in || !buf_out || !res_il || !res_pl || !res_pp || !ex_buf_in || !ex_buf_out) {
        printf("ERROR: allocation failed\n");
        return 1;
    }

    fill_random(buf_in, (size_t)BENCH_PIXELS * (size_t)max_ch);
    /* Fill planar channel buffers */
    for (m = 0; m < 3; m++)
        fill_random(ch_in[m], BENCH_PIXELS);
    /* ex_buf_in / pex_ch_in filled per-format in the benchmark loop */
    /* Fill planar _ex typed channel buffers with random bytes */
    {
        unsigned int seed2 = 0x87654321u;
        size_t ch;
        for (ch = 0; ch < 4; ch++) {
            uint8_t *p = (uint8_t *)pex_ch_in[ch];
            size_t total = (size_t)BENCH_PIXELS * sizeof(double);
            size_t j;
            for (j = 0; j < total; j++) {
                seed2 = seed2 * 1103515245u + 12345u;
                p[j] = (uint8_t)(seed2 >> 16);
            }
        }
    }

    /* ---- Run all benchmarks ---- */
    printf("\nRunning %d benchmarks (x3 + 10 typed)...\n", (int)MAP_COUNT);

    for (m = 0; m < MAP_COUNT; m++) {
        map_entry const *e = &g_maps[m];
        double t;

        printf("  [%3d/%3d] %-35s", (int)(m + 1), (int)MAP_COUNT, e->name);
        fflush(stdout);

        /* Interleave (always present) */
        {
            ileave_ctx ctx;
            ctx.fn  = e->interleave;
            ctx.in  = buf_in;
            ctx.out = buf_out;
            ctx.si  = (size_t)e->in_ch  * sizeof(alwan_scalar);
            ctx.so  = (size_t)e->out_ch * sizeof(alwan_scalar);
            t = measure_best(run_ileave, &ctx);
            res_il[m] = (double)BENCH_PIXELS / t / 1e6;
        }

        /* Planar (if available) */
        if (e->planar) {
            planar_ctx ctx;
            ctx.fn = e->planar;
            ctx.i0 = ch_in[0]; ctx.i1 = ch_in[1]; ctx.i2 = ch_in[2];
            ctx.o0 = ch_out[0]; ctx.o1 = ch_out[1]; ctx.o2 = ch_out[2];
            t = measure_best(run_planar, &ctx);
            res_pl[m] = (double)BENCH_PIXELS / t / 1e6;
        }

        /* Per-pixel (if available) */
        if (e->per_pixel) {
            perpx_ctx ctx;
            ctx.fn = e->per_pixel;
            ctx.in = buf_in;
            ctx.out = buf_out;
            ctx.in_ch = e->in_ch;
            ctx.out_ch = e->out_ch;
            t = measure_best(run_perpx, &ctx);
            res_pp[m] = (double)BENCH_PIXELS / t / 1e6;
        }

        /* Typed-format interleave _ex (U8, U16, F32, F64) */
        if (e->interleave_ex) {
            int fi;
            for (fi = 0; fi < 5; fi++) {
                ileave_ex_ctx ctx;
                fill_typed_random(ex_buf_in, ex_fmts[fi], (size_t)BENCH_PIXELS * (size_t)e->in_ch);
                ctx.fn  = e->interleave_ex;
                ctx.in  = ex_buf_in;
                ctx.out = ex_buf_out;
                ctx.fmt = ex_fmts[fi];
                ctx.si  = (size_t)e->in_ch  * pixel_type_size(ex_fmts[fi]);
                ctx.so  = (size_t)e->out_ch * pixel_type_size(ex_fmts[fi]);
                t = measure_best(run_ileave_ex, &ctx);
                res_ex[fi][m] = (double)BENCH_PIXELS / t / 1e6;
            }
        }

        /* Typed-format planar _ex (U8, U16, F16, F32, F64) */
        if (e->planar_ex) {
            int fi;
            for (fi = 0; fi < 5; fi++) {
                planar_ex_ctx ctx;
                size_t esz = pixel_type_size(ex_fmts[fi]);
                int ch;
                for (ch = 0; ch < 3; ch++)
                    fill_typed_random(pex_ch_in[ch], ex_fmts[fi], BENCH_PIXELS);
                ctx.fn = e->planar_ex;
                ctx.i0 = pex_ch_in[0]; ctx.i1 = pex_ch_in[1]; ctx.i2 = pex_ch_in[2];
                ctx.o0 = pex_ch_out[0]; ctx.o1 = pex_ch_out[1]; ctx.o2 = pex_ch_out[2];
                ctx.fmt = ex_fmts[fi];
                ctx.si = esz;
                ctx.so = esz;
                t = measure_best(run_planar_ex, &ctx);
                res_pex[fi][m] = (double)BENCH_PIXELS / t / 1e6;
            }
        }

        /* CMY<->CMYK planar _ex (3<->4 channels, special handling) */
        if (!e->planar_ex && (e->in_ch == 3 && e->out_ch == 4)) {
            int fi;
            for (fi = 0; fi < 5; fi++) {
                cmyk_pex_ctx ctx;
                size_t esz = pixel_type_size(ex_fmts[fi]);
                int ch;
                for (ch = 0; ch < 3; ch++)
                    fill_typed_random(pex_ch_in[ch], ex_fmts[fi], BENCH_PIXELS);
                ctx.i0 = pex_ch_in[0]; ctx.i1 = pex_ch_in[1]; ctx.i2 = pex_ch_in[2]; ctx.i3 = NULL;
                ctx.o0 = pex_ch_out[0]; ctx.o1 = pex_ch_out[1]; ctx.o2 = pex_ch_out[2]; ctx.o3 = pex_ch_out[3];
                ctx.fmt = ex_fmts[fi]; ctx.si = esz; ctx.so = esz;
                ctx.is_cmy_to_cmyk = 1;
                t = measure_best(run_cmyk_pex, &ctx);
                res_pex[fi][m] = (double)BENCH_PIXELS / t / 1e6;
            }
        }
        if (!e->planar_ex && (e->in_ch == 4 && e->out_ch == 3)) {
            int fi;
            for (fi = 0; fi < 5; fi++) {
                cmyk_pex_ctx ctx;
                size_t esz = pixel_type_size(ex_fmts[fi]);
                int ch;
                for (ch = 0; ch < 4; ch++)
                    fill_typed_random(pex_ch_in[ch], ex_fmts[fi], BENCH_PIXELS);
                ctx.i0 = pex_ch_in[0]; ctx.i1 = pex_ch_in[1]; ctx.i2 = pex_ch_in[2]; ctx.i3 = pex_ch_in[3];
                ctx.o0 = pex_ch_out[0]; ctx.o1 = pex_ch_out[1]; ctx.o2 = pex_ch_out[2]; ctx.o3 = NULL;
                ctx.fmt = ex_fmts[fi]; ctx.si = esz; ctx.so = esz;
                ctx.is_cmy_to_cmyk = 0;
                t = measure_best(run_cmyk_pex, &ctx);
                res_pex[fi][m] = (double)BENCH_PIXELS / t / 1e6;
            }
        }

        printf(" done\n");
    }

    /* ---- Image convert ---- */
    {
        imgconv_ctx ictx;
        unsigned int seed;
        ictx.src_stride = (size_t)BENCH_PIXELS * 3;
        ictx.dst_stride = (size_t)BENCH_PIXELS * 3 * sizeof(float);
        ictx.src = (uint8_t *)malloc(ictx.src_stride);
        ictx.dst = (float *)malloc(ictx.dst_stride);
        img_result = 0.0;
        if (ictx.src && ictx.dst) {
            seed = 0xDEADBEEFu;
            for (m = 0; m < (size_t)BENCH_PIXELS * 3; m++) {
                seed = seed * 1103515245u + 12345u;
                ictx.src[m] = (uint8_t)(seed >> 16);
            }
            alwan_rgb_get_space_descriptor(&ictx.srgb_desc, NULL, ALWAN_RGB_SPACE_SRGB);
            alwan_rgb_get_space_descriptor(&ictx.p3_desc, NULL, ALWAN_RGB_SPACE_DISPLAY_P3);
            img_result = (double)BENCH_PIXELS / measure_best(run_imgconv, &ictx) / 1e6;
        }
        free(ictx.src);
        free(ictx.dst);
    }

    /* ================================================================
     * Print results
     * ================================================================ */

    printf("\n| %-35s | Vec | %10s | %10s | %10s | %7s %7s | %7s %7s | %7s %7s | %7s %7s | %7s %7s |\n",
           "Pipeline", "Interleave", "Planar", "Per-pixel",
           "U8i", "U8p", "U16i", "U16p", "F16i", "F16p", "F32i", "F32p", "F64i", "F64p");
    printf("|%-37s|-----|%12s|%12s|%12s|%8s%8s|%8s%8s|%8s%8s|%8s%8s|%8s%8s|\n",
           "-------------------------------------",
           "------------", "------------", "------------",
           "--------", "--------", "--------", "--------",
           "--------", "--------", "--------", "--------",
           "--------", "--------");

    for (m = 0; m < MAP_COUNT; m++) {
        int has_v = g_maps[m].has_simd;

        printf("| %-35s |  ", g_maps[m].name);
        color_set(has_v ? COL_GREEN : COL_RED);
        printf("%c", has_v ? 'V' : '-');
        color_reset();
        printf("  | ");

        /* Interleave */
        color_set(col_for_mpixs(res_il[m]));
        printf("%10.2f", res_il[m]);
        color_reset();

        /* Planar */
        printf(" | ");
        if (res_pl[m] > 0.0) {
            color_set(col_for_mpixs(res_pl[m]));
            printf("%10.2f", res_pl[m]);
            color_reset();
        } else {
            color_set(COL_GRAY);
            printf("%10s", "--");
            color_reset();
        }

        /* Per-pixel */
        printf(" | ");
        if (res_pp[m] > 0.0) {
            color_set(col_for_mpixs(res_pp[m]));
            printf("%10.2f", res_pp[m]);
            color_reset();
        } else {
            color_set(COL_GRAY);
            printf("%10s", "--");
            color_reset();
        }

        /* Typed _ex columns (5 formats x interleave + planar) */
        for (f = 0; f < 5; f++) {
            printf(" | ");
            if (res_ex[f][m] > 0.0) {
                color_set(col_for_mpixs(res_ex[f][m]));
                printf("%7.1f", res_ex[f][m]);
                color_reset();
            } else {
                color_set(COL_GRAY);
                printf("%7s", "-");
                color_reset();
            }
            printf(" ");
            if (res_pex[f][m] > 0.0) {
                color_set(col_for_mpixs(res_pex[f][m]));
                printf("%7.1f", res_pex[f][m]);
                color_reset();
            } else {
                color_set(COL_GRAY);
                printf("%7s", "-");
                color_reset();
            }
        }
        printf(" |\n");
    }

    /* Image convert row */
    if (img_result > 0.0) {
        printf("| %-35s |  ", "Image convert (sRGB U8 -> P3 F32)");
        color_set(COL_RED);
        printf("-");
        color_reset();
        printf("  | ");
        color_set(col_for_mpixs(img_result));
        printf("%10.2f", img_result);
        color_reset();
        printf(" | ");
        color_set(COL_GRAY);
        printf("%10s | %10s", "--", "--");
        color_reset();
        for (f = 0; f < 5; f++) {
            printf(" | ");
            color_set(COL_GRAY);
            printf("%7s %7s", "--", "--");
            color_reset();
        }
        printf(" |\n");
    }

    printf("\nAll values in Mpix/s (higher is better)\n");
    printf("Vec: ");
    color_set(COL_GREEN); printf("V"); color_reset();
    printf(" = SIMD vectorized, ");
    color_set(COL_RED); printf("-"); color_reset();
    printf(" = scalar loop\n");
    printf("U8i/U16i/F16i/F32i/F64i: _ex typed interleave,  U8p/U16p/F16p/F32p/F64p: _ex typed planar\n");

    /* ================================================================
     * Print results (per-row relative coloring)
     * ================================================================ */

    printf("\n| %-35s | Vec | %10s | %10s | %10s | %7s %7s | %7s %7s | %7s %7s | %7s %7s | %7s %7s |\n",
           "Pipeline (row-relative)", "Interleave", "Planar", "Per-pixel",
           "U8i", "U8p", "U16i", "U16p", "F16i", "F16p", "F32i", "F32p", "F64i", "F64p");
    printf("|%-37s|-----|%12s|%12s|%12s|%8s%8s|%8s%8s|%8s%8s|%8s%8s|%8s%8s|\n",
           "-------------------------------------",
           "------------", "------------", "------------",
           "--------", "--------", "--------", "--------",
           "--------", "--------", "--------", "--------",
           "--------", "--------");

    for (m = 0; m < MAP_COUNT; m++) {
        double row_max = 0.0;
        int has_v = g_maps[m].has_simd;

        /* Find row maximum across all columns */
        if (res_il[m] > row_max) row_max = res_il[m];
        if (res_pl[m] > row_max) row_max = res_pl[m];
        if (res_pp[m] > row_max) row_max = res_pp[m];
        for (f = 0; f < 5; f++) {
            if (res_ex[f][m]  > row_max) row_max = res_ex[f][m];
            if (res_pex[f][m] > row_max) row_max = res_pex[f][m];
        }

        printf("| %-35s |  ", g_maps[m].name);
        color_set(has_v ? COL_GREEN : COL_RED);
        printf("%c", has_v ? 'V' : '-');
        color_reset();
        printf("  | ");

        /* Interleave */
        color_set(col_for_row(res_il[m], row_max));
        printf("%10.2f", res_il[m]);
        color_reset();

        /* Planar */
        printf(" | ");
        if (res_pl[m] > 0.0) {
            color_set(col_for_row(res_pl[m], row_max));
            printf("%10.2f", res_pl[m]);
            color_reset();
        } else {
            color_set(COL_GRAY);
            printf("%10s", "--");
            color_reset();
        }

        /* Per-pixel */
        printf(" | ");
        if (res_pp[m] > 0.0) {
            color_set(col_for_row(res_pp[m], row_max));
            printf("%10.2f", res_pp[m]);
            color_reset();
        } else {
            color_set(COL_GRAY);
            printf("%10s", "--");
            color_reset();
        }

        /* Typed _ex columns */
        for (f = 0; f < 5; f++) {
            printf(" | ");
            if (res_ex[f][m] > 0.0) {
                color_set(col_for_row(res_ex[f][m], row_max));
                printf("%7.1f", res_ex[f][m]);
                color_reset();
            } else {
                color_set(COL_GRAY);
                printf("%7s", "-");
                color_reset();
            }
            printf(" ");
            if (res_pex[f][m] > 0.0) {
                color_set(col_for_row(res_pex[f][m], row_max));
                printf("%7.1f", res_pex[f][m]);
                color_reset();
            } else {
                color_set(COL_GRAY);
                printf("%7s", "-");
                color_reset();
            }
        }
        printf(" |\n");
    }

    /* Image convert row (only 1 value, always green relative to itself) */
    if (img_result > 0.0) {
        printf("| %-35s |  ", "Image convert (sRGB U8 -> P3 F32)");
        color_set(COL_RED);
        printf("-");
        color_reset();
        printf("  | ");
        color_set(COL_GREEN);
        printf("%10.2f", img_result);
        color_reset();
        printf(" | ");
        color_set(COL_GRAY);
        printf("%10s | %10s", "--", "--");
        color_reset();
        for (f = 0; f < 5; f++) {
            printf(" | ");
            color_set(COL_GRAY);
            printf("%7s %7s", "--", "--");
            color_reset();
        }
        printf(" |\n");
    }

    printf("\nColors: relative to row maximum (");
    color_set(COL_GREEN);  printf(">=75%%");  color_reset(); printf(" ");
    color_set(COL_CYAN);   printf(">=50%%");  color_reset(); printf(" ");
    color_set(COL_YELLOW); printf(">=25%%");  color_reset(); printf(" ");
    color_set(COL_RED);    printf("<25%%");   color_reset();
    printf(")\n\n");

    free(buf_in); free(buf_out);
    for (m = 0; m < 3; m++) { free(ch_in[m]); free(ch_out[m]); }
    free(res_il); free(res_pl); free(res_pp);
    for (f = 0; f < 5; f++) { free(res_ex[f]); free(res_pex[f]); }
    free(ex_buf_in); free(ex_buf_out);
    for (m = 0; m < 4; m++) { free(pex_ch_in[m]); free(pex_ch_out[m]); }
    return 0;
}
