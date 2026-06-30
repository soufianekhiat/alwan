/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Color Interop Forum -- Interop ID string table
 * Bidirectional lookup between alwan_rgb_space enum and canonical string IDs.
 * Reference: ASWF Color Interop Forum specification
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * Static interop ID table
 *
 * Maps alwan_rgb_space enum -> canonical Color Interop Forum string ID.
 * Only spaces with official interop IDs are included.
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_rgb_space space;
    char const *id;
} interop_entry;

static interop_entry const g_interop_table[] = {
    /* Scene-referred Linear */
    { ALWAN_RGB_SPACE_ACES2065_1,           "lin_ap0" },
    { ALWAN_RGB_SPACE_ACESCG,               "lin_ap1" },
    { ALWAN_RGB_SPACE_LINEAR_REC709,        "lin_srgb" },
    { ALWAN_RGB_SPACE_LINEAR_REC2020,       "lin_rec2020" },
    { ALWAN_RGB_SPACE_LINEAR_DISPLAY_P3,    "lin_displayp3" },
    { ALWAN_RGB_SPACE_LINEAR_P3_D65,        "lin_p3d65" },

    /* Scene-referred Non-linear (ACES family) */
    { ALWAN_RGB_SPACE_ACESCC,               "acescc" },
    { ALWAN_RGB_SPACE_ACESCCT,              "acescct" },
    { ALWAN_RGB_SPACE_ACESPROXY,            "acesproxy" },

    /* Scene-referred Non-linear (Camera log spaces) */
    { ALWAN_RGB_SPACE_ARRI_LOGC3,           "logc3_awg3" },
    { ALWAN_RGB_SPACE_ARRI_LOGC4,           "logc4_awg4" },
    { ALWAN_RGB_SPACE_S_LOG3,               "slog3_sgamut3" },
    { ALWAN_RGB_SPACE_V_LOG,                "vlog_vgamut" },
    { ALWAN_RGB_SPACE_CANON_LOG,            "clog_cgamut" },
    { ALWAN_RGB_SPACE_REDLOG,               "redlog_rwg" },
    { ALWAN_RGB_SPACE_FILMLIGHT_T_LOG,      "tlog_egamut" },
    { ALWAN_RGB_SPACE_DAVINCI_INTERMEDIATE, "di_dwg" },
    { ALWAN_RGB_SPACE_FUJIFILM_F_LOG,       "flog_fgamut" },
    { ALWAN_RGB_SPACE_N_LOG,                "nlog_ngamut" },

    /* Scene-referred Non-linear (Camera linear gamuts) */
    { ALWAN_RGB_SPACE_ARRI_WIDE_GAMUT_3,    "lin_awg3" },
    { ALWAN_RGB_SPACE_ARRI_WIDE_GAMUT_4,    "lin_awg4" },
    { ALWAN_RGB_SPACE_S_GAMUT3,             "lin_sgamut3" },
    { ALWAN_RGB_SPACE_S_GAMUT3_CINE,        "lin_sgamut3cine" },
    { ALWAN_RGB_SPACE_V_GAMUT,              "lin_vgamut" },
    { ALWAN_RGB_SPACE_CINEMA_GAMUT,         "lin_cgamut" },
    { ALWAN_RGB_SPACE_REDWIDEGAMUTRGB,      "lin_rwg" },
    { ALWAN_RGB_SPACE_FILMLIGHT_E_GAMUT,    "lin_egamut" },
    { ALWAN_RGB_SPACE_DAVINCI_WIDE_GAMUT,   "lin_dwg" },
    { ALWAN_RGB_SPACE_F_GAMUT,              "lin_fgamut" },
    { ALWAN_RGB_SPACE_N_GAMUT,              "lin_ngamut" },

    /* Display-referred */
    { ALWAN_RGB_SPACE_SRGB,                 "srgb_texture" },
    { ALWAN_RGB_SPACE_DISPLAY_P3,           "srgb_displayp3" },
    { ALWAN_RGB_SPACE_REC1886_REC709,       "rec1886_rec709" },
    { ALWAN_RGB_SPACE_REC2100_PQ,           "rec2100_pq" },
    { ALWAN_RGB_SPACE_REC2100_HLG,          "rec2100_hlg" },
    { ALWAN_RGB_SPACE_DISPLAY_P3_HDR,       "display_p3_hdr" },

    /* Additional well-known spaces */
    { ALWAN_RGB_SPACE_BT709,                "bt709" },
    { ALWAN_RGB_SPACE_BT2020,               "bt2020" },
    { ALWAN_RGB_SPACE_DCI_P3,               "dci_p3" },
    { ALWAN_RGB_SPACE_ADOBE_RGB_1998,       "adobergb" },
    { ALWAN_RGB_SPACE_PROPHOTO_RGB,         "prophoto" },
};

static size_t const g_interop_table_size =
    sizeof(g_interop_table) / sizeof(g_interop_table[0]);

/* ----------------------------------------------------------------
 * Parse: string -> enum
 * ---------------------------------------------------------------- */

int alwan_interop_parse_f64(alwan_rgb_space *space, char const *id) {
    if (!space || !id) return ALWAN_E_INVALID;

    for (size_t i = 0; i < g_interop_table_size; i++) {
        if (strcmp(g_interop_table[i].id, id) == 0) {
            *space = g_interop_table[i].space;
            return ALWAN_OK;
        }
    }

    return ALWAN_E_NODATA;
}

int alwan_interop_parse_f32(alwan_rgb_space *space, char const *id) {
    return alwan_interop_parse_f64(space, id);
}

/* ----------------------------------------------------------------
 * Format: enum -> string
 * ---------------------------------------------------------------- */

char const *alwan_interop_format(alwan_rgb_space space) {
    for (size_t i = 0; i < g_interop_table_size; i++) {
        if (g_interop_table[i].space == space) {
            return g_interop_table[i].id;
        }
    }
    return NULL;
}

/* ----------------------------------------------------------------
 * Query: get all interop IDs and their count
 * ---------------------------------------------------------------- */

size_t alwan_interop_count(void) {
    return g_interop_table_size;
}

int alwan_interop_entry_at_f64(alwan_rgb_space *space, char const **id, size_t index) {
    if (index >= g_interop_table_size) return ALWAN_E_RANGE;
    if (space) *space = g_interop_table[index].space;
    if (id) *id = g_interop_table[index].id;
    return ALWAN_OK;
}

int alwan_interop_entry_at_f32(alwan_rgb_space *space, char const **id, size_t index) {
    return alwan_interop_entry_at_f64(space, id, index);
}
