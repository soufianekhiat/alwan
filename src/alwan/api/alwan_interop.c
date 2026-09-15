/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Color interop IDs: bidirectional lookup between alwan_rgb_space and the
 * identifiers of the ASWF Color Interop Forum.
 * Reference: ColorInterop, Recommendations/01_TextureAssetColorSpaces (v1.1.0,
 * 2026-07-14), 02_DisplayColorSpaces (v1.0.0, 2026-01-26) and 03_ColorInteropID
 * (v1.0.0, 2026-07-14).
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>

typedef struct {
    alwan_rgb_space space;
    char const *id;
} interop_entry;

/* ----------------------------------------------------------------
 * One row per space: the ID alwan_interop_format returns. A space with a
 * Forum ID carries it as published, the scene-referred one where the Forum
 * publishes both. A space the Forum has not published carries an ID in the
 * alwan namespace, as 03_ColorInteropID requires of IDs an organisation
 * generates.
 * ---------------------------------------------------------------- */

static interop_entry const g_interop_table[] = {
    /* Forum texture asset colour spaces, scene-referred */
    { ALWAN_RGB_SPACE_ACESCG,                "lin_ap1_scene" },
    { ALWAN_RGB_SPACE_ACES2065_1,            "lin_ap0_scene" },
    { ALWAN_RGB_SPACE_LINEAR_REC709,         "lin_rec709_scene" },
    { ALWAN_RGB_SPACE_LINEAR_P3_D65,         "lin_p3d65_scene" },
    { ALWAN_RGB_SPACE_LINEAR_REC2020,        "lin_rec2020_scene" },
    { ALWAN_RGB_SPACE_LINEAR_ADOBE_RGB_1998, "lin_adobergb_scene" },
    { ALWAN_RGB_SPACE_LINEAR_CIE_XYZ_D65,    "lin_ciexyzd65_scene" },
    { ALWAN_RGB_SPACE_SRGB,                  "srgb_rec709_scene" },
    { ALWAN_RGB_SPACE_GAMMA24_REC709,        "g24_rec709_scene" },
    { ALWAN_RGB_SPACE_GAMMA22_REC709,        "g22_rec709_scene" },
    { ALWAN_RGB_SPACE_GAMMA18_REC709,        "g18_rec709_scene" },
    { ALWAN_RGB_SPACE_SRGB_AP1,              "srgb_ap1_scene" },
    { ALWAN_RGB_SPACE_GAMMA22_AP1,           "g22_ap1_scene" },
    { ALWAN_RGB_SPACE_DISPLAY_P3,            "srgb_p3d65_scene" },
    { ALWAN_RGB_SPACE_GAMMA22_ADOBE_RGB,     "g22_adobergb_scene" },

    /* Forum display colour spaces with no scene-referred ID */
    { ALWAN_RGB_SPACE_DISPLAY_P3_HDR,        "pq_p3d65_display" },
    { ALWAN_RGB_SPACE_REC2100_PQ,            "pq_rec2020_display" },
    { ALWAN_RGB_SPACE_REC2100_HLG,           "hlg_rec2020_display" },
    { ALWAN_RGB_SPACE_P3_D65,                "g26_p3d65_display" },

    /* Not published by the Forum: the alwan namespace */
    { ALWAN_RGB_SPACE_ACESCC,                "alwan:acescc" },
    { ALWAN_RGB_SPACE_ACESCCT,               "alwan:acescct" },
    { ALWAN_RGB_SPACE_ACESPROXY,             "alwan:acesproxy" },
    { ALWAN_RGB_SPACE_ARRI_LOGC3,            "alwan:logc3_awg3" },
    { ALWAN_RGB_SPACE_ARRI_LOGC4,            "alwan:logc4_awg4" },
    { ALWAN_RGB_SPACE_S_LOG3,                "alwan:slog3_sgamut3" },
    { ALWAN_RGB_SPACE_V_LOG,                 "alwan:vlog_vgamut" },
    { ALWAN_RGB_SPACE_CANON_LOG,             "alwan:clog_cgamut" },
    { ALWAN_RGB_SPACE_REDLOG,                "alwan:redlog_rwg" },
    { ALWAN_RGB_SPACE_FILMLIGHT_T_LOG,       "alwan:tlog_egamut" },
    { ALWAN_RGB_SPACE_DAVINCI_INTERMEDIATE,  "alwan:di_dwg" },
    { ALWAN_RGB_SPACE_FUJIFILM_F_LOG,        "alwan:flog_fgamut" },
    { ALWAN_RGB_SPACE_N_LOG,                 "alwan:nlog_ngamut" },
    { ALWAN_RGB_SPACE_ARRI_WIDE_GAMUT_3,     "alwan:lin_awg3" },
    { ALWAN_RGB_SPACE_ARRI_WIDE_GAMUT_4,     "alwan:lin_awg4" },
    { ALWAN_RGB_SPACE_S_GAMUT3,              "alwan:lin_sgamut3" },
    { ALWAN_RGB_SPACE_S_GAMUT3_CINE,         "alwan:lin_sgamut3cine" },
    { ALWAN_RGB_SPACE_V_GAMUT,               "alwan:lin_vgamut" },
    { ALWAN_RGB_SPACE_CINEMA_GAMUT,          "alwan:lin_cgamut" },
    { ALWAN_RGB_SPACE_REDWIDEGAMUTRGB,       "alwan:lin_rwg" },
    { ALWAN_RGB_SPACE_FILMLIGHT_E_GAMUT,     "alwan:lin_egamut" },
    { ALWAN_RGB_SPACE_FILMLIGHT_E_GAMUT_2,   "alwan:lin_egamut2" },
    { ALWAN_RGB_SPACE_DAVINCI_WIDE_GAMUT,    "alwan:lin_dwg" },
    { ALWAN_RGB_SPACE_F_GAMUT,               "alwan:lin_fgamut" },
    { ALWAN_RGB_SPACE_F_GAMUT_C,             "alwan:lin_fgamutc" },
    { ALWAN_RGB_SPACE_N_GAMUT,               "alwan:lin_ngamut" },
    { ALWAN_RGB_SPACE_LINEAR_DISPLAY_P3,     "alwan:lin_displayp3" },
    { ALWAN_RGB_SPACE_REC1886_REC709,        "alwan:rec1886_rec709" },
    { ALWAN_RGB_SPACE_BT709,                 "alwan:bt709" },
    { ALWAN_RGB_SPACE_BT2020,                "alwan:bt2020" },
    { ALWAN_RGB_SPACE_DCI_P3,                "alwan:dci_p3" },
    { ALWAN_RGB_SPACE_ADOBE_RGB_1998,        "alwan:adobergb" },
    { ALWAN_RGB_SPACE_PROPHOTO_RGB,          "alwan:prophoto" },
};

static size_t const g_interop_table_size =
    sizeof(g_interop_table) / sizeof(g_interop_table[0]);

/* ----------------------------------------------------------------
 * IDs that parse but are never formatted: the Forum's display-referred IDs for
 * spaces whose formatted ID is the scene-referred one, and the bare IDs alwan
 * wrote before it followed the published lists, so that files written then
 * still read.
 * ---------------------------------------------------------------- */

static interop_entry const g_interop_aliases[] = {
    /* Forum display IDs of spaces formatted as scene-referred */
    { ALWAN_RGB_SPACE_SRGB,                  "srgb_rec709_display" },
    { ALWAN_RGB_SPACE_GAMMA24_REC709,        "g24_rec709_display" },
    { ALWAN_RGB_SPACE_DISPLAY_P3,            "srgb_p3d65_display" },
    { ALWAN_RGB_SPACE_GAMMA22_REC709,        "g22_rec709_display" },
    { ALWAN_RGB_SPACE_GAMMA22_ADOBE_RGB,     "g22_adobergb_display" },
    { ALWAN_RGB_SPACE_LINEAR_REC709,         "lin_rec709_display" },
    { ALWAN_RGB_SPACE_LINEAR_P3_D65,         "lin_p3d65_display" },
    { ALWAN_RGB_SPACE_LINEAR_REC2020,        "lin_rec2020_display" },

    /* IDs alwan wrote before 3.0.0 */
    { ALWAN_RGB_SPACE_ACES2065_1,            "lin_ap0" },
    { ALWAN_RGB_SPACE_ACESCG,                "lin_ap1" },
    { ALWAN_RGB_SPACE_LINEAR_REC709,         "lin_srgb" },
    { ALWAN_RGB_SPACE_LINEAR_REC2020,        "lin_rec2020" },
    { ALWAN_RGB_SPACE_LINEAR_DISPLAY_P3,     "lin_displayp3" },
    { ALWAN_RGB_SPACE_LINEAR_P3_D65,         "lin_p3d65" },
    { ALWAN_RGB_SPACE_ACESCC,                "acescc" },
    { ALWAN_RGB_SPACE_ACESCCT,               "acescct" },
    { ALWAN_RGB_SPACE_ACESPROXY,             "acesproxy" },
    { ALWAN_RGB_SPACE_ARRI_LOGC3,            "logc3_awg3" },
    { ALWAN_RGB_SPACE_ARRI_LOGC4,            "logc4_awg4" },
    { ALWAN_RGB_SPACE_S_LOG3,                "slog3_sgamut3" },
    { ALWAN_RGB_SPACE_V_LOG,                 "vlog_vgamut" },
    { ALWAN_RGB_SPACE_CANON_LOG,             "clog_cgamut" },
    { ALWAN_RGB_SPACE_REDLOG,                "redlog_rwg" },
    { ALWAN_RGB_SPACE_FILMLIGHT_T_LOG,       "tlog_egamut" },
    { ALWAN_RGB_SPACE_DAVINCI_INTERMEDIATE,  "di_dwg" },
    { ALWAN_RGB_SPACE_FUJIFILM_F_LOG,        "flog_fgamut" },
    { ALWAN_RGB_SPACE_N_LOG,                 "nlog_ngamut" },
    { ALWAN_RGB_SPACE_ARRI_WIDE_GAMUT_3,     "lin_awg3" },
    { ALWAN_RGB_SPACE_ARRI_WIDE_GAMUT_4,     "lin_awg4" },
    { ALWAN_RGB_SPACE_S_GAMUT3,              "lin_sgamut3" },
    { ALWAN_RGB_SPACE_S_GAMUT3_CINE,         "lin_sgamut3cine" },
    { ALWAN_RGB_SPACE_V_GAMUT,               "lin_vgamut" },
    { ALWAN_RGB_SPACE_CINEMA_GAMUT,          "lin_cgamut" },
    { ALWAN_RGB_SPACE_REDWIDEGAMUTRGB,       "lin_rwg" },
    { ALWAN_RGB_SPACE_FILMLIGHT_E_GAMUT,     "lin_egamut" },
    { ALWAN_RGB_SPACE_DAVINCI_WIDE_GAMUT,    "lin_dwg" },
    { ALWAN_RGB_SPACE_F_GAMUT,               "lin_fgamut" },
    { ALWAN_RGB_SPACE_N_GAMUT,               "lin_ngamut" },
    { ALWAN_RGB_SPACE_SRGB,                  "srgb_texture" },
    { ALWAN_RGB_SPACE_DISPLAY_P3,            "srgb_displayp3" },
    { ALWAN_RGB_SPACE_REC1886_REC709,        "rec1886_rec709" },
    { ALWAN_RGB_SPACE_REC2100_PQ,            "rec2100_pq" },
    { ALWAN_RGB_SPACE_REC2100_HLG,           "rec2100_hlg" },
    { ALWAN_RGB_SPACE_DISPLAY_P3_HDR,        "display_p3_hdr" },
    { ALWAN_RGB_SPACE_BT709,                 "bt709" },
    { ALWAN_RGB_SPACE_BT2020,                "bt2020" },
    { ALWAN_RGB_SPACE_DCI_P3,                "dci_p3" },
    { ALWAN_RGB_SPACE_ADOBE_RGB_1998,        "adobergb" },
    { ALWAN_RGB_SPACE_PROPHOTO_RGB,          "prophoto" },
};

static size_t const g_interop_aliases_size =
    sizeof(g_interop_aliases) / sizeof(g_interop_aliases[0]);

/* ----------------------------------------------------------------
 * Parse: string -> enum
 * ---------------------------------------------------------------- */

alwan_status alwan_interop_parse_f64(alwan_rgb_space *space, char const *id) {
    if (!space || !id) return ALWAN_E_INVALID;

    for (size_t i = 0; i < g_interop_table_size; i++) {
        if (strcmp(g_interop_table[i].id, id) == 0) {
            *space = g_interop_table[i].space;
            return ALWAN_OK;
        }
    }
    for (size_t i = 0; i < g_interop_aliases_size; i++) {
        if (strcmp(g_interop_aliases[i].id, id) == 0) {
            *space = g_interop_aliases[i].space;
            return ALWAN_OK;
        }
    }

    return ALWAN_E_NODATA;
}

alwan_status alwan_interop_parse_f32(alwan_rgb_space *space, char const *id) {
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
 * Query: the formatted IDs and their count
 * ---------------------------------------------------------------- */

size_t alwan_interop_count(void) {
    return g_interop_table_size;
}

alwan_status alwan_interop_entry_at_f64(alwan_rgb_space *space, char const **id, size_t index) {
    if (index >= g_interop_table_size) return ALWAN_E_RANGE;
    if (space) *space = g_interop_table[index].space;
    if (id) *id = g_interop_table[index].id;
    return ALWAN_OK;
}

alwan_status alwan_interop_entry_at_f32(alwan_rgb_space *space, char const **id, size_t index) {
    return alwan_interop_entry_at_f64(space, id, index);
}
