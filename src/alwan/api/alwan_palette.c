/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Named colour palettes: Freetone (Stuart Semple, device CMYK) and the CSS Color 3
 * keywords, with lookup by name and substring search; and HEX notation, as
 * colour-science's RGB_to_HEX and HEX_to_RGB.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stddef.h>
#include <string.h>

typedef struct {
    char const *name;
    double c, m, y, k;
} alwan__freetone_row;

static alwan__freetone_row const k_freetone[] = {
#include "../data/palettes/freetone.inc"
};

typedef struct {
    char const *name;
    unsigned char r, g, b;
} alwan__css3_row;

static alwan__css3_row const k_css3[] = {
#include "../data/palettes/css_color_3.inc"
};

size_t alwan_palette_size(alwan_palette palette) {
    switch (palette) {
    case ALWAN_PALETTE_FREETONE: return sizeof(k_freetone) / sizeof(k_freetone[0]);
    case ALWAN_PALETTE_CSS_COLOR_3: return sizeof(k_css3) / sizeof(k_css3[0]);
    default: return 0;
    }
}

static char const *alwan__palette_name(alwan_palette palette, size_t index) {
    return palette == ALWAN_PALETTE_FREETONE ? k_freetone[index].name : k_css3[index].name;
}

alwan_status alwan_palette_entry_at(alwan_palette_entry *entry_out, alwan_palette palette, size_t index) {
    if (!entry_out || index >= alwan_palette_size(palette)) return ALWAN_E_INVALID;
    if (palette == ALWAN_PALETTE_FREETONE) {
        entry_out->name = k_freetone[index].name;
        entry_out->model = ALWAN_PALETTE_MODEL_CMYK;
        entry_out->value[0] = k_freetone[index].c;
        entry_out->value[1] = k_freetone[index].m;
        entry_out->value[2] = k_freetone[index].y;
        entry_out->value[3] = k_freetone[index].k;
    } else {
        entry_out->name = k_css3[index].name;
        entry_out->model = ALWAN_PALETTE_MODEL_SRGB;
        entry_out->value[0] = (double)k_css3[index].r / 255.0;
        entry_out->value[1] = (double)k_css3[index].g / 255.0;
        entry_out->value[2] = (double)k_css3[index].b / 255.0;
        entry_out->value[3] = 0.0;
    }
    return ALWAN_OK;
}

/* A name as lookup compares it: ASCII lowercase, every space, tab and newline
 * removed. Returns 0 when it does not fit in cap bytes with its terminator. */
static int alwan__palette_norm(char *buf, size_t cap, char const *s) {
    size_t n = 0;
    for (; *s; s++) {
        char c = *s;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (n + 1 >= cap) return 0;
        buf[n++] = c;
    }
    buf[n] = '\0';
    return 1;
}

enum { ALWAN__PALETTE_NAME_MAX = 256 };

alwan_status alwan_palette_find(size_t *index_out, alwan_palette palette, char const *name) {
    char q[ALWAN__PALETTE_NAME_MAX], e[ALWAN__PALETTE_NAME_MAX];
    size_t const n = alwan_palette_size(palette);
    size_t i;
    if (!index_out || !name || n == 0) return ALWAN_E_INVALID;
    if (!alwan__palette_norm(q, sizeof(q), name)) return ALWAN_E_NODATA;
    for (i = 0; i < n; i++) {
        if (alwan__palette_norm(e, sizeof(e), alwan__palette_name(palette, i)) && strcmp(q, e) == 0) {
            *index_out = i;
            return ALWAN_OK;
        }
    }
    return ALWAN_E_NODATA;
}

alwan_status alwan_palette_search(size_t *match_count_out, size_t *indices_out, size_t capacity, alwan_palette palette,
                                  char const *text) {
    char q[ALWAN__PALETTE_NAME_MAX], e[ALWAN__PALETTE_NAME_MAX];
    size_t const n = alwan_palette_size(palette);
    size_t i, found = 0;
    if (!match_count_out || !text || n == 0 || (capacity > 0 && !indices_out)) return ALWAN_E_INVALID;
    if (alwan__palette_norm(q, sizeof(q), text)) {
        for (i = 0; i < n; i++) {
            if (alwan__palette_norm(e, sizeof(e), alwan__palette_name(palette, i)) && strstr(e, q)) {
                if (found < capacity) indices_out[found] = i;
                found++;
            }
        }
    }
    *match_count_out = found;
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- HEX */

/* colour-science's RGB_to_HEX: each channel truncated to an integer of 0-255
 * (as_int_array(RGB * 255)), two lowercase digits each. Outside [0, 1], where
 * colour-science clips or rescales with a warning, is ALWAN_E_INVALID. */
static alwan_status alwan__rgb_to_hex(char *hex_out, double r, double g, double b) {
    double const v[3] = { r, g, b };
    int i;
    if (!hex_out) return ALWAN_E_INVALID;
    for (i = 0; i < 3; i++) {
        if (!(v[i] >= 0.0 && v[i] <= 1.0)) return ALWAN_E_INVALID;
    }
    hex_out[0] = '#';
    for (i = 0; i < 3; i++) {
        int const q = (int)(v[i] * 255.0);
        int const hi = q / 16, lo = q % 16;
        hex_out[1 + 2 * i] = (char)(hi < 10 ? '0' + hi : 'a' + hi - 10);
        hex_out[2 + 2 * i] = (char)(lo < 10 ? '0' + lo : 'a' + lo - 10);
    }
    hex_out[7] = '\0';
    return ALWAN_OK;
}

static int alwan__hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Six digits as colour-science's HEX_to_RGB reads them; three as CSS reads them,
 * each digit doubled (#abc is #aabbcc), where colour-science takes a digit as the
 * whole value. The leading '#' is optional; any other length is ALWAN_E_INVALID. */
static alwan_status alwan__hex_to_rgb(double *rgb, char const *hex) {
    int d[6];
    size_t n, i;
    if (!hex) return ALWAN_E_INVALID;
    if (*hex == '#') hex++;
    n = strlen(hex);
    if (n != 3 && n != 6) return ALWAN_E_INVALID;
    for (i = 0; i < n; i++) {
        d[i] = alwan__hex_digit(hex[i]);
        if (d[i] < 0) return ALWAN_E_INVALID;
    }
    for (i = 0; i < 3; i++) {
        rgb[i] = (double)(n == 6 ? d[2 * i] * 16 + d[2 * i + 1] : d[i] * 17) / 255.0;
    }
    return ALWAN_OK;
}

alwan_status alwan_rgb_to_hex_f64(char *hex_out, alwan_rgb_f64 const *rgb) {
    if (!rgb) return ALWAN_E_INVALID;
    return alwan__rgb_to_hex(hex_out, rgb->r, rgb->g, rgb->b);
}

alwan_status alwan_rgb_to_hex_f32(char *hex_out, alwan_rgb_f32 const *rgb) {
    if (!rgb) return ALWAN_E_INVALID;
    return alwan__rgb_to_hex(hex_out, (double)rgb->r, (double)rgb->g, (double)rgb->b);
}

alwan_status alwan_hex_to_rgb_f64(alwan_rgb_f64 *rgb_out, char const *hex) {
    double v[3];
    alwan_status st;
    if (!rgb_out) return ALWAN_E_INVALID;
    st = alwan__hex_to_rgb(v, hex);
    if (st != ALWAN_OK) return st;
    rgb_out->r = v[0];
    rgb_out->g = v[1];
    rgb_out->b = v[2];
    return ALWAN_OK;
}

alwan_status alwan_hex_to_rgb_f32(alwan_rgb_f32 *rgb_out, char const *hex) {
    double v[3];
    alwan_status st;
    if (!rgb_out) return ALWAN_E_INVALID;
    st = alwan__hex_to_rgb(v, hex);
    if (st != ALWAN_OK) return st;
    rgb_out->r = (alwan_f32)v[0];
    rgb_out->g = (alwan_f32)v[1];
    rgb_out->b = (alwan_f32)v[2];
    return ALWAN_OK;
}

/* ---------------------------------------------------------------- CSS Color 3 keywords */

static alwan_status alwan__css3_keyword(double *rgb, char const *keyword) {
    size_t i;
    alwan_status st;
    if (!keyword) return ALWAN_E_INVALID;
    st = alwan_palette_find(&i, ALWAN_PALETTE_CSS_COLOR_3, keyword);
    if (st != ALWAN_OK) return st;
    rgb[0] = (double)k_css3[i].r / 255.0;
    rgb[1] = (double)k_css3[i].g / 255.0;
    rgb[2] = (double)k_css3[i].b / 255.0;
    return ALWAN_OK;
}

alwan_status alwan_css_color_3_keyword_to_rgb_f64(alwan_rgb_f64 *rgb_out, char const *keyword) {
    double v[3];
    alwan_status st;
    if (!rgb_out) return ALWAN_E_INVALID;
    st = alwan__css3_keyword(v, keyword);
    if (st != ALWAN_OK) return st;
    rgb_out->r = v[0];
    rgb_out->g = v[1];
    rgb_out->b = v[2];
    return ALWAN_OK;
}

alwan_status alwan_css_color_3_keyword_to_rgb_f32(alwan_rgb_f32 *rgb_out, char const *keyword) {
    double v[3];
    alwan_status st;
    if (!rgb_out) return ALWAN_E_INVALID;
    st = alwan__css3_keyword(v, keyword);
    if (st != ALWAN_OK) return st;
    rgb_out->r = (alwan_f32)v[0];
    rgb_out->g = (alwan_f32)v[1];
    rgb_out->b = (alwan_f32)v[2];
    return ALWAN_OK;
}
