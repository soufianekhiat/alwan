/* ----------------------------------------------------------------
 * Embedded RGB Space Matrices (optional)
 * Precomputed NPM (RGB->XYZ) and inverse NPM (XYZ->RGB) for each space.
 * Each array has 18 values: rgb_to_xyz[9] + xyz_to_rgb[9].
 * Used when ALWAN_EMBED_DATA is defined.
 * IMPORTANT: Array order MUST match enum alwan_rgb_space order!
 * ---------------------------------------------------------------- */
#if ALWAN_EMBED_DATA

#if ALWAN_WITH_F64

/* Disable float conversion warnings for embedded CSV data */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_DIAG_DISABLE_EXTERN_TO_STATIC

/* Core RGB spaces - Order MUST match enum in alwan.h */
static alwan_f64 const g_srgb_matrices[] = {
#include "data/rgb_matrices/srgb.csv"
};

static alwan_f64 const g_bt709_matrices[] = {
#include "data/rgb_matrices/bt709.csv"
};

static alwan_f64 const g_display_p3_matrices[] = {
#include "data/rgb_matrices/display_p3.csv"
};

static alwan_f64 const g_bt2020_matrices[] = {
#include "data/rgb_matrices/bt2020.csv"
};

static alwan_f64 const g_aces2065_1_matrices[] = {
#include "data/rgb_matrices/aces2065-1.csv"
};

static alwan_f64 const g_acescg_matrices[] = {
#include "data/rgb_matrices/acescg.csv"
};

static alwan_f64 const g_acesproxy_matrices[] = {
#include "data/rgb_matrices/acesproxy.csv"
};

static alwan_f64 const g_acescc_matrices[] = {
#include "data/rgb_matrices/acescc.csv"
};

static alwan_f64 const g_acescct_matrices[] = {
#include "data/rgb_matrices/acescct.csv"
};

static alwan_f64 const g_arri_wide_gamut_3_matrices[] = {
#include "data/rgb_matrices/arri_wide_gamut_3.csv"
};

static alwan_f64 const g_arri_wide_gamut_4_matrices[] = {
#include "data/rgb_matrices/arri_wide_gamut_4.csv"
};

static alwan_f64 const g_arri_logc3_matrices[] = {
#include "data/rgb_matrices/arri_logc3.csv"
};

static alwan_f64 const g_arri_logc4_matrices[] = {
#include "data/rgb_matrices/arri_logc4.csv"
};

static alwan_f64 const g_redcolor_matrices[] = {
#include "data/rgb_matrices/redcolor.csv"
};

static alwan_f64 const g_redcolor2_matrices[] = {
#include "data/rgb_matrices/redcolor2.csv"
};

static alwan_f64 const g_redcolor3_matrices[] = {
#include "data/rgb_matrices/redcolor3.csv"
};

static alwan_f64 const g_redcolor4_matrices[] = {
#include "data/rgb_matrices/redcolor4.csv"
};

static alwan_f64 const g_dragoncolor_matrices[] = {
#include "data/rgb_matrices/dragoncolor.csv"
};

static alwan_f64 const g_dragoncolor2_matrices[] = {
#include "data/rgb_matrices/dragoncolor2.csv"
};

static alwan_f64 const g_redlog_matrices[] = {
#include "data/rgb_matrices/redlog.csv"
};

static alwan_f64 const g_venice_s_gamut3_matrices[] = {
#include "data/rgb_matrices/venice_s-gamut3.csv"
};

static alwan_f64 const g_venice_s_gamut3_cine_matrices[] = {
#include "data/rgb_matrices/venice_s-gamut3cine.csv"
};

static alwan_f64 const g_s_log_matrices[] = {
#include "data/rgb_matrices/s-log.csv"
};

static alwan_f64 const g_s_log2_matrices[] = {
#include "data/rgb_matrices/s-log2.csv"
};

static alwan_f64 const g_s_log3_matrices[] = {
#include "data/rgb_matrices/s-log3.csv"
};

static alwan_f64 const g_cie_rgb_matrices[] = {
#include "data/rgb_matrices/cie_rgb.csv"
};

static alwan_f64 const g_adobe_wide_gamut_rgb_matrices[] = {
#include "data/rgb_matrices/adobe_wide_gamut_rgb.csv"
};

static alwan_f64 const g_romm_rgb_matrices[] = {
#include "data/rgb_matrices/romm_rgb.csv"
};

static alwan_f64 const g_rimm_rgb_matrices[] = {
#include "data/rgb_matrices/rimm_rgb.csv"
};

static alwan_f64 const g_erimm_rgb_matrices[] = {
#include "data/rgb_matrices/erimm_rgb.csv"
};

static alwan_f64 const g_filmlight_e_gamut_matrices[] = {
#include "data/rgb_matrices/filmlight_e-gamut.csv"
};

static alwan_f64 const g_filmlight_t_log_matrices[] = {
#include "data/rgb_matrices/filmlight_t-log.csv"
};

static alwan_f64 const g_f_gamut_matrices[] = {
#include "data/rgb_matrices/f-gamut.csv"
};

static alwan_f64 const g_fujifilm_f_log_matrices[] = {
#include "data/rgb_matrices/fujifilm_f-log.csv"
};

static alwan_f64 const g_n_gamut_matrices[] = {
#include "data/rgb_matrices/n-gamut.csv"
};

static alwan_f64 const g_n_log_matrices[] = {
#include "data/rgb_matrices/n-log.csv"
};

static alwan_f64 const g_dji_d_gamut_matrices[] = {
#include "data/rgb_matrices/dji_d-gamut.csv"
};

static alwan_f64 const g_protune_native_matrices[] = {
#include "data/rgb_matrices/protune_native.csv"
};

static alwan_f64 const g_itu_r_bt470_525_matrices[] = {
#include "data/rgb_matrices/itu-r_bt470_-_525.csv"
};

static alwan_f64 const g_itu_r_bt470_625_matrices[] = {
#include "data/rgb_matrices/itu-r_bt470_-_625.csv"
};

static alwan_f64 const g_smpte_240m_matrices[] = {
#include "data/rgb_matrices/smpte_240m.csv"
};

static alwan_f64 const g_smpte_c_matrices[] = {
#include "data/rgb_matrices/smpte_c.csv"
};

static alwan_f64 const g_dcdm_xyz_matrices[] = {
#include "data/rgb_matrices/dcdm_xyz.csv"
};

static alwan_f64 const g_best_rgb_matrices[] = {
#include "data/rgb_matrices/best_rgb.csv"
};

static alwan_f64 const g_beta_rgb_matrices[] = {
#include "data/rgb_matrices/beta_rgb.csv"
};

static alwan_f64 const g_don_rgb_4_matrices[] = {
#include "data/rgb_matrices/don_rgb_4.csv"
};

static alwan_f64 const g_ekta_space_ps5_matrices[] = {
#include "data/rgb_matrices/ekta_space_ps_5.csv"
};

static alwan_f64 const g_max_rgb_matrices[] = {
#include "data/rgb_matrices/max_rgb.csv"
};

static alwan_f64 const g_russell_rgb_matrices[] = {
#include "data/rgb_matrices/russell_rgb.csv"
};

static alwan_f64 const g_sharp_rgb_matrices[] = {
#include "data/rgb_matrices/sharp_rgb.csv"
};

static alwan_f64 const g_eci_rgb_v2_matrices[] = {
#include "data/rgb_matrices/eci_rgb_v2.csv"
};

/* EXISTING SPACES */
static alwan_f64 const g_adobe_rgb_1998_matrices[] = {
#include "data/rgb_matrices/adobe_rgb_1998.csv"
};

static alwan_f64 const g_prophoto_rgb_matrices[] = {
#include "data/rgb_matrices/prophoto_rgb.csv"
};

static alwan_f64 const g_davinci_wide_gamut_matrices[] = {
#include "data/rgb_matrices/davinci_wide_gamut.csv"
};

static alwan_f64 const g_davinci_intermediate_matrices[] = {
#include "data/rgb_matrices/davinci_intermediate.csv"
};

static alwan_f64 const g_blackmagic_wide_gamut_matrices[] = {
#include "data/rgb_matrices/blackmagic_wide_gamut.csv"
};

static alwan_f64 const g_blackmagic_film_matrices[] = {
#include "data/rgb_matrices/blackmagic_film.csv"
};

static alwan_f64 const g_blackmagic_film_gen5_matrices[] = {
#include "data/rgb_matrices/blackmagic_film_gen5.csv"
};

static alwan_f64 const g_v_gamut_matrices[] = {
#include "data/rgb_matrices/v-gamut.csv"
};

static alwan_f64 const g_v_log_matrices[] = {
#include "data/rgb_matrices/v-log.csv"
};

static alwan_f64 const g_s_gamut_matrices[] = {
#include "data/rgb_matrices/s-gamut.csv"
};

static alwan_f64 const g_s_gamut3_matrices[] = {
#include "data/rgb_matrices/s-gamut3.csv"
};

static alwan_f64 const g_s_gamut3_cine_matrices[] = {
#include "data/rgb_matrices/s-gamut3cine.csv"
};

static alwan_f64 const g_cinema_gamut_matrices[] = {
#include "data/rgb_matrices/cinema_gamut.csv"
};

static alwan_f64 const g_canon_log_matrices[] = {
#include "data/rgb_matrices/canon_log.csv"
};

static alwan_f64 const g_redwidegamutrgb_matrices[] = {
#include "data/rgb_matrices/redwidegamutrgb.csv"
};

static alwan_f64 const g_dci_p3_matrices[] = {
#include "data/rgb_matrices/dci-p3.csv"
};

static alwan_f64 const g_dci_p3_p_matrices[] = {
#include "data/rgb_matrices/dci-p3-p.csv"
};

static alwan_f64 const g_p3_d65_matrices[] = {
#include "data/rgb_matrices/p3-d65.csv"
};

static alwan_f64 const g_ntsc_1953_matrices[] = {
#include "data/rgb_matrices/ntsc_1953.csv"
};

static alwan_f64 const g_ntsc_1987_matrices[] = {
#include "data/rgb_matrices/ntsc_1987.csv"
};

static alwan_f64 const g_pal_secam_matrices[] = {
#include "data/rgb_matrices/pal_secam.csv"
};

static alwan_f64 const g_ebu_tech_3213_e_matrices[] = {
#include "data/rgb_matrices/ebu_tech_3213-e.csv"
};

static alwan_f64 const g_apple_rgb_matrices[] = {
#include "data/rgb_matrices/apple_rgb.csv"
};

static alwan_f64 const g_colormatch_rgb_matrices[] = {
#include "data/rgb_matrices/colormatch_rgb.csv"
};

/* Additional RGB color spaces */
static alwan_f64 const g_alexa_wide_gamut_matrices[] = {
#include "data/rgb_matrices/alexa_wide_gamut.csv"
};

static alwan_f64 const g_p3_d60_matrices[] = {
#include "data/rgb_matrices/p3-d60.csv"
};

static alwan_f64 const g_xtreme_rgb_matrices[] = {
#include "data/rgb_matrices/xtreme_rgb.csv"
};

static alwan_f64 const g_linear_rec709_matrices[] = {
#include "data/rgb_matrices/linear_srgb.csv"
};

static alwan_f64 const g_linear_rec2020_matrices[] = {
#include "data/rgb_matrices/linear_rec2020.csv"
};

static alwan_f64 const g_linear_adobe_rgb_1998_matrices[] = {
#include "data/rgb_matrices/linear_adobe_rgb_1998.csv"
};

static alwan_f64 const g_linear_p3_d65_matrices[] = {
#include "data/rgb_matrices/linear_p3_d65.csv"
};

static alwan_f64 const g_linear_display_p3_matrices[] = {
#include "data/rgb_matrices/linear_display_p3.csv"
};

static alwan_f64 const g_linear_prophoto_rgb_matrices[] = {
#include "data/rgb_matrices/linear_prophoto_rgb.csv"
};

static alwan_f64 const g_linear_dci_p3_matrices[] = {
#include "data/rgb_matrices/linear_dci_p3.csv"
};

static alwan_f64 const g_linear_adobe_wide_gamut_rgb_matrices[] = {
#include "data/rgb_matrices/linear_adobe_wide_gamut_rgb.csv"
};

static alwan_f64 const g_linear_apple_rgb_matrices[] = {
#include "data/rgb_matrices/linear_apple_rgb.csv"
};

static alwan_f64 const g_linear_colormatch_rgb_matrices[] = {
#include "data/rgb_matrices/linear_colormatch_rgb.csv"
};

static alwan_f64 const g_linear_p3_d60_matrices[] = {
#include "data/rgb_matrices/linear_p3_d60.csv"
};

static alwan_f64 const g_linear_bt470_525_matrices[] = {
#include "data/rgb_matrices/linear_bt470_525.csv"
};

static alwan_f64 const g_linear_bt470_625_matrices[] = {
#include "data/rgb_matrices/linear_bt470_625.csv"
};

static alwan_f64 const g_linear_smpte_240m_matrices[] = {
#include "data/rgb_matrices/linear_smpte_240m.csv"
};

static alwan_f64 const g_itu_t_h273_22_unspecified_matrices[] = {
#include "data/rgb_matrices/itu-t_h273_22_unspecified.csv"
};

static alwan_f64 const g_itu_t_h273_generic_film_matrices[] = {
#include "data/rgb_matrices/itu-t_h273_generic_film.csv"
};

static alwan_f64 const g_plasa_ansi_e154_matrices[] = {
#include "data/rgb_matrices/plasa_ansi_e154.csv"
};

static alwan_f64 const g_gamma22_rec709_matrices[] = {
#include "data/rgb_matrices/gamma_22_rec709.csv"
};

static alwan_f64 const g_gamma22_adobe_rgb_matrices[] = {
#include "data/rgb_matrices/gamma_22_adobe_rgb.csv"
};

static alwan_f64 const g_gamma22_p3_d65_matrices[] = {
#include "data/rgb_matrices/gamma_22_p3-d65.csv"
};

static alwan_f64 const g_gamma22_ap1_matrices[] = {
#include "data/rgb_matrices/gamma_22_ap1.csv"
};

static alwan_f64 const g_gamma18_rec709_matrices[] = {
#include "data/rgb_matrices/gamma_18_rec709.csv"
};

/* ColorInterop Display Color Spaces */
static alwan_f64 const g_rec1886_rec709_matrices[] = {
#include "data/rgb_matrices/bt709.csv"
};
static alwan_f64 const g_rec2100_pq_matrices[] = {
#include "data/rgb_matrices/bt2020.csv"
};
static alwan_f64 const g_rec2100_hlg_matrices[] = {
#include "data/rgb_matrices/bt2020.csv"
};
static alwan_f64 const g_display_p3_hdr_matrices[] = {
#include "data/rgb_matrices/display_p3.csv"
};

ALWAN_DIAG_POP

/* Array of pointers to RGB space matrix data - Order MUST match enum */
static alwan_f64 const * const g_rgb_space_matrices[] = {
    g_srgb_matrices,
    g_bt709_matrices,
    g_display_p3_matrices,
    g_bt2020_matrices,
    g_aces2065_1_matrices,
    g_acescg_matrices,
    g_acesproxy_matrices,
    g_acescc_matrices,
    g_acescct_matrices,
    g_arri_wide_gamut_3_matrices,
    g_arri_wide_gamut_4_matrices,
    g_arri_logc3_matrices,
    g_arri_logc4_matrices,
    g_redcolor_matrices,
    g_redcolor2_matrices,
    g_redcolor3_matrices,
    g_redcolor4_matrices,
    g_dragoncolor_matrices,
    g_dragoncolor2_matrices,
    g_redlog_matrices,
    g_venice_s_gamut3_matrices,
    g_venice_s_gamut3_cine_matrices,
    g_s_log_matrices,
    g_s_log2_matrices,
    g_s_log3_matrices,
    g_cie_rgb_matrices,
    g_adobe_wide_gamut_rgb_matrices,
    g_romm_rgb_matrices,
    g_rimm_rgb_matrices,
    g_erimm_rgb_matrices,
    g_filmlight_e_gamut_matrices,
    g_filmlight_t_log_matrices,
    g_f_gamut_matrices,
    g_fujifilm_f_log_matrices,
    g_n_gamut_matrices,
    g_n_log_matrices,
    g_dji_d_gamut_matrices,
    g_protune_native_matrices,
    g_itu_r_bt470_525_matrices,
    g_itu_r_bt470_625_matrices,
    g_smpte_240m_matrices,
    g_smpte_c_matrices,
    g_dcdm_xyz_matrices,
    g_best_rgb_matrices,
    g_beta_rgb_matrices,
    g_don_rgb_4_matrices,
    g_ekta_space_ps5_matrices,
    g_max_rgb_matrices,
    g_russell_rgb_matrices,
    g_sharp_rgb_matrices,
    g_eci_rgb_v2_matrices,
    g_adobe_rgb_1998_matrices,
    g_prophoto_rgb_matrices,
    g_davinci_wide_gamut_matrices,
    g_davinci_intermediate_matrices,
    g_blackmagic_wide_gamut_matrices,
    g_blackmagic_film_matrices,
    g_blackmagic_film_gen5_matrices,
    g_v_gamut_matrices,
    g_v_log_matrices,
    g_s_gamut_matrices,
    g_s_gamut3_matrices,
    g_s_gamut3_cine_matrices,
    g_cinema_gamut_matrices,
    g_canon_log_matrices,
    g_redwidegamutrgb_matrices,
    g_dci_p3_matrices,
    g_dci_p3_p_matrices,
    g_p3_d65_matrices,
    g_ntsc_1953_matrices,
    g_ntsc_1987_matrices,
    g_pal_secam_matrices,
    g_ebu_tech_3213_e_matrices,
    g_apple_rgb_matrices,
    g_colormatch_rgb_matrices,
    g_alexa_wide_gamut_matrices,
    g_p3_d60_matrices,
    g_xtreme_rgb_matrices,
    g_linear_rec709_matrices,
    g_linear_rec2020_matrices,
    g_linear_adobe_rgb_1998_matrices,
    g_linear_p3_d65_matrices,
    g_linear_display_p3_matrices,
    g_linear_prophoto_rgb_matrices,
    g_linear_dci_p3_matrices,
    g_linear_adobe_wide_gamut_rgb_matrices,
    g_linear_apple_rgb_matrices,
    g_linear_colormatch_rgb_matrices,
    g_linear_p3_d60_matrices,
    g_linear_bt470_525_matrices,
    g_linear_bt470_625_matrices,
    g_linear_smpte_240m_matrices,
    g_itu_t_h273_22_unspecified_matrices,
    g_itu_t_h273_generic_film_matrices,
    g_plasa_ansi_e154_matrices,
    g_gamma22_rec709_matrices,
    g_gamma22_adobe_rgb_matrices,
    g_gamma22_p3_d65_matrices,
    g_gamma22_ap1_matrices,
    g_gamma18_rec709_matrices,
    g_rec1886_rec709_matrices,
    g_rec2100_pq_matrices,
    g_rec2100_hlg_matrices,
    g_display_p3_hdr_matrices
};

static size_t const g_rgb_space_matrices_count = sizeof(g_rgb_space_matrices) / sizeof(g_rgb_space_matrices[0]);

/* Static assert: enum, primaries, and matrices arrays must all stay in lock-step */
_Static_assert(
    sizeof(g_rgb_space_matrices) / sizeof(g_rgb_space_matrices[0]) == (size_t)ALWAN_RGB_SPACE_COUNT &&
    sizeof(g_rgb_space_data)     / sizeof(g_rgb_space_data[0])     == (size_t)ALWAN_RGB_SPACE_COUNT,
    "alwan_rgb_space, g_rgb_space_data[], and g_rgb_space_matrices[] must have matching size"
);

#endif /* ALWAN_WITH_F64 */

/* ----------------------------------------------------------------
 * f32 native twins of the embedded tables above.
 * Same CSV bytes, stored as float for native f32 readers (no f64->f32
 * double-rounding). Order MUST match enum / the f64 arrays.
 * ---------------------------------------------------------------- */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_DIAG_DISABLE_EXTERN_TO_STATIC

static float const g_srgb_matrices_f32[] = {
#include "data/rgb_matrices/srgb.csv"
};

static float const g_bt709_matrices_f32[] = {
#include "data/rgb_matrices/bt709.csv"
};

static float const g_display_p3_matrices_f32[] = {
#include "data/rgb_matrices/display_p3.csv"
};

static float const g_bt2020_matrices_f32[] = {
#include "data/rgb_matrices/bt2020.csv"
};

static float const g_aces2065_1_matrices_f32[] = {
#include "data/rgb_matrices/aces2065-1.csv"
};

static float const g_acescg_matrices_f32[] = {
#include "data/rgb_matrices/acescg.csv"
};

static float const g_acesproxy_matrices_f32[] = {
#include "data/rgb_matrices/acesproxy.csv"
};

static float const g_acescc_matrices_f32[] = {
#include "data/rgb_matrices/acescc.csv"
};

static float const g_acescct_matrices_f32[] = {
#include "data/rgb_matrices/acescct.csv"
};

static float const g_arri_wide_gamut_3_matrices_f32[] = {
#include "data/rgb_matrices/arri_wide_gamut_3.csv"
};

static float const g_arri_wide_gamut_4_matrices_f32[] = {
#include "data/rgb_matrices/arri_wide_gamut_4.csv"
};

static float const g_arri_logc3_matrices_f32[] = {
#include "data/rgb_matrices/arri_logc3.csv"
};

static float const g_arri_logc4_matrices_f32[] = {
#include "data/rgb_matrices/arri_logc4.csv"
};

static float const g_redcolor_matrices_f32[] = {
#include "data/rgb_matrices/redcolor.csv"
};

static float const g_redcolor2_matrices_f32[] = {
#include "data/rgb_matrices/redcolor2.csv"
};

static float const g_redcolor3_matrices_f32[] = {
#include "data/rgb_matrices/redcolor3.csv"
};

static float const g_redcolor4_matrices_f32[] = {
#include "data/rgb_matrices/redcolor4.csv"
};

static float const g_dragoncolor_matrices_f32[] = {
#include "data/rgb_matrices/dragoncolor.csv"
};

static float const g_dragoncolor2_matrices_f32[] = {
#include "data/rgb_matrices/dragoncolor2.csv"
};

static float const g_redlog_matrices_f32[] = {
#include "data/rgb_matrices/redlog.csv"
};

static float const g_venice_s_gamut3_matrices_f32[] = {
#include "data/rgb_matrices/venice_s-gamut3.csv"
};

static float const g_venice_s_gamut3_cine_matrices_f32[] = {
#include "data/rgb_matrices/venice_s-gamut3cine.csv"
};

static float const g_s_log_matrices_f32[] = {
#include "data/rgb_matrices/s-log.csv"
};

static float const g_s_log2_matrices_f32[] = {
#include "data/rgb_matrices/s-log2.csv"
};

static float const g_s_log3_matrices_f32[] = {
#include "data/rgb_matrices/s-log3.csv"
};

static float const g_cie_rgb_matrices_f32[] = {
#include "data/rgb_matrices/cie_rgb.csv"
};

static float const g_adobe_wide_gamut_rgb_matrices_f32[] = {
#include "data/rgb_matrices/adobe_wide_gamut_rgb.csv"
};

static float const g_romm_rgb_matrices_f32[] = {
#include "data/rgb_matrices/romm_rgb.csv"
};

static float const g_rimm_rgb_matrices_f32[] = {
#include "data/rgb_matrices/rimm_rgb.csv"
};

static float const g_erimm_rgb_matrices_f32[] = {
#include "data/rgb_matrices/erimm_rgb.csv"
};

static float const g_filmlight_e_gamut_matrices_f32[] = {
#include "data/rgb_matrices/filmlight_e-gamut.csv"
};

static float const g_filmlight_t_log_matrices_f32[] = {
#include "data/rgb_matrices/filmlight_t-log.csv"
};

static float const g_f_gamut_matrices_f32[] = {
#include "data/rgb_matrices/f-gamut.csv"
};

static float const g_fujifilm_f_log_matrices_f32[] = {
#include "data/rgb_matrices/fujifilm_f-log.csv"
};

static float const g_n_gamut_matrices_f32[] = {
#include "data/rgb_matrices/n-gamut.csv"
};

static float const g_n_log_matrices_f32[] = {
#include "data/rgb_matrices/n-log.csv"
};

static float const g_dji_d_gamut_matrices_f32[] = {
#include "data/rgb_matrices/dji_d-gamut.csv"
};

static float const g_protune_native_matrices_f32[] = {
#include "data/rgb_matrices/protune_native.csv"
};

static float const g_itu_r_bt470_525_matrices_f32[] = {
#include "data/rgb_matrices/itu-r_bt470_-_525.csv"
};

static float const g_itu_r_bt470_625_matrices_f32[] = {
#include "data/rgb_matrices/itu-r_bt470_-_625.csv"
};

static float const g_smpte_240m_matrices_f32[] = {
#include "data/rgb_matrices/smpte_240m.csv"
};

static float const g_smpte_c_matrices_f32[] = {
#include "data/rgb_matrices/smpte_c.csv"
};

static float const g_dcdm_xyz_matrices_f32[] = {
#include "data/rgb_matrices/dcdm_xyz.csv"
};

static float const g_best_rgb_matrices_f32[] = {
#include "data/rgb_matrices/best_rgb.csv"
};

static float const g_beta_rgb_matrices_f32[] = {
#include "data/rgb_matrices/beta_rgb.csv"
};

static float const g_don_rgb_4_matrices_f32[] = {
#include "data/rgb_matrices/don_rgb_4.csv"
};

static float const g_ekta_space_ps5_matrices_f32[] = {
#include "data/rgb_matrices/ekta_space_ps_5.csv"
};

static float const g_max_rgb_matrices_f32[] = {
#include "data/rgb_matrices/max_rgb.csv"
};

static float const g_russell_rgb_matrices_f32[] = {
#include "data/rgb_matrices/russell_rgb.csv"
};

static float const g_sharp_rgb_matrices_f32[] = {
#include "data/rgb_matrices/sharp_rgb.csv"
};

static float const g_eci_rgb_v2_matrices_f32[] = {
#include "data/rgb_matrices/eci_rgb_v2.csv"
};

static float const g_adobe_rgb_1998_matrices_f32[] = {
#include "data/rgb_matrices/adobe_rgb_1998.csv"
};

static float const g_prophoto_rgb_matrices_f32[] = {
#include "data/rgb_matrices/prophoto_rgb.csv"
};

static float const g_davinci_wide_gamut_matrices_f32[] = {
#include "data/rgb_matrices/davinci_wide_gamut.csv"
};

static float const g_davinci_intermediate_matrices_f32[] = {
#include "data/rgb_matrices/davinci_intermediate.csv"
};

static float const g_blackmagic_wide_gamut_matrices_f32[] = {
#include "data/rgb_matrices/blackmagic_wide_gamut.csv"
};

static float const g_blackmagic_film_matrices_f32[] = {
#include "data/rgb_matrices/blackmagic_film.csv"
};

static float const g_blackmagic_film_gen5_matrices_f32[] = {
#include "data/rgb_matrices/blackmagic_film_gen5.csv"
};

static float const g_v_gamut_matrices_f32[] = {
#include "data/rgb_matrices/v-gamut.csv"
};

static float const g_v_log_matrices_f32[] = {
#include "data/rgb_matrices/v-log.csv"
};

static float const g_s_gamut_matrices_f32[] = {
#include "data/rgb_matrices/s-gamut.csv"
};

static float const g_s_gamut3_matrices_f32[] = {
#include "data/rgb_matrices/s-gamut3.csv"
};

static float const g_s_gamut3_cine_matrices_f32[] = {
#include "data/rgb_matrices/s-gamut3cine.csv"
};

static float const g_cinema_gamut_matrices_f32[] = {
#include "data/rgb_matrices/cinema_gamut.csv"
};

static float const g_canon_log_matrices_f32[] = {
#include "data/rgb_matrices/canon_log.csv"
};

static float const g_redwidegamutrgb_matrices_f32[] = {
#include "data/rgb_matrices/redwidegamutrgb.csv"
};

static float const g_dci_p3_matrices_f32[] = {
#include "data/rgb_matrices/dci-p3.csv"
};

static float const g_dci_p3_p_matrices_f32[] = {
#include "data/rgb_matrices/dci-p3-p.csv"
};

static float const g_p3_d65_matrices_f32[] = {
#include "data/rgb_matrices/p3-d65.csv"
};

static float const g_ntsc_1953_matrices_f32[] = {
#include "data/rgb_matrices/ntsc_1953.csv"
};

static float const g_ntsc_1987_matrices_f32[] = {
#include "data/rgb_matrices/ntsc_1987.csv"
};

static float const g_pal_secam_matrices_f32[] = {
#include "data/rgb_matrices/pal_secam.csv"
};

static float const g_ebu_tech_3213_e_matrices_f32[] = {
#include "data/rgb_matrices/ebu_tech_3213-e.csv"
};

static float const g_apple_rgb_matrices_f32[] = {
#include "data/rgb_matrices/apple_rgb.csv"
};

static float const g_colormatch_rgb_matrices_f32[] = {
#include "data/rgb_matrices/colormatch_rgb.csv"
};

static float const g_alexa_wide_gamut_matrices_f32[] = {
#include "data/rgb_matrices/alexa_wide_gamut.csv"
};

static float const g_p3_d60_matrices_f32[] = {
#include "data/rgb_matrices/p3-d60.csv"
};

static float const g_xtreme_rgb_matrices_f32[] = {
#include "data/rgb_matrices/xtreme_rgb.csv"
};

static float const g_linear_rec709_matrices_f32[] = {
#include "data/rgb_matrices/linear_srgb.csv"
};

static float const g_linear_rec2020_matrices_f32[] = {
#include "data/rgb_matrices/linear_rec2020.csv"
};

static float const g_linear_adobe_rgb_1998_matrices_f32[] = {
#include "data/rgb_matrices/linear_adobe_rgb_1998.csv"
};

static float const g_linear_p3_d65_matrices_f32[] = {
#include "data/rgb_matrices/linear_p3_d65.csv"
};

static float const g_linear_display_p3_matrices_f32[] = {
#include "data/rgb_matrices/linear_display_p3.csv"
};

static float const g_linear_prophoto_rgb_matrices_f32[] = {
#include "data/rgb_matrices/linear_prophoto_rgb.csv"
};

static float const g_linear_dci_p3_matrices_f32[] = {
#include "data/rgb_matrices/linear_dci_p3.csv"
};

static float const g_linear_adobe_wide_gamut_rgb_matrices_f32[] = {
#include "data/rgb_matrices/linear_adobe_wide_gamut_rgb.csv"
};

static float const g_linear_apple_rgb_matrices_f32[] = {
#include "data/rgb_matrices/linear_apple_rgb.csv"
};

static float const g_linear_colormatch_rgb_matrices_f32[] = {
#include "data/rgb_matrices/linear_colormatch_rgb.csv"
};

static float const g_linear_p3_d60_matrices_f32[] = {
#include "data/rgb_matrices/linear_p3_d60.csv"
};

static float const g_linear_bt470_525_matrices_f32[] = {
#include "data/rgb_matrices/linear_bt470_525.csv"
};

static float const g_linear_bt470_625_matrices_f32[] = {
#include "data/rgb_matrices/linear_bt470_625.csv"
};

static float const g_linear_smpte_240m_matrices_f32[] = {
#include "data/rgb_matrices/linear_smpte_240m.csv"
};

static float const g_itu_t_h273_22_unspecified_matrices_f32[] = {
#include "data/rgb_matrices/itu-t_h273_22_unspecified.csv"
};

static float const g_itu_t_h273_generic_film_matrices_f32[] = {
#include "data/rgb_matrices/itu-t_h273_generic_film.csv"
};

static float const g_plasa_ansi_e154_matrices_f32[] = {
#include "data/rgb_matrices/plasa_ansi_e154.csv"
};

static float const g_gamma22_rec709_matrices_f32[] = {
#include "data/rgb_matrices/gamma_22_rec709.csv"
};

static float const g_gamma22_adobe_rgb_matrices_f32[] = {
#include "data/rgb_matrices/gamma_22_adobe_rgb.csv"
};

static float const g_gamma22_p3_d65_matrices_f32[] = {
#include "data/rgb_matrices/gamma_22_p3-d65.csv"
};

static float const g_gamma22_ap1_matrices_f32[] = {
#include "data/rgb_matrices/gamma_22_ap1.csv"
};

static float const g_gamma18_rec709_matrices_f32[] = {
#include "data/rgb_matrices/gamma_18_rec709.csv"
};

static float const g_rec1886_rec709_matrices_f32[] = {
#include "data/rgb_matrices/bt709.csv"
};

static float const g_rec2100_pq_matrices_f32[] = {
#include "data/rgb_matrices/bt2020.csv"
};

static float const g_rec2100_hlg_matrices_f32[] = {
#include "data/rgb_matrices/bt2020.csv"
};

static float const g_display_p3_hdr_matrices_f32[] = {
#include "data/rgb_matrices/display_p3.csv"
};

ALWAN_DIAG_POP

static float const * const g_rgb_space_matrices_f32[] = {
    g_srgb_matrices_f32,
    g_bt709_matrices_f32,
    g_display_p3_matrices_f32,
    g_bt2020_matrices_f32,
    g_aces2065_1_matrices_f32,
    g_acescg_matrices_f32,
    g_acesproxy_matrices_f32,
    g_acescc_matrices_f32,
    g_acescct_matrices_f32,
    g_arri_wide_gamut_3_matrices_f32,
    g_arri_wide_gamut_4_matrices_f32,
    g_arri_logc3_matrices_f32,
    g_arri_logc4_matrices_f32,
    g_redcolor_matrices_f32,
    g_redcolor2_matrices_f32,
    g_redcolor3_matrices_f32,
    g_redcolor4_matrices_f32,
    g_dragoncolor_matrices_f32,
    g_dragoncolor2_matrices_f32,
    g_redlog_matrices_f32,
    g_venice_s_gamut3_matrices_f32,
    g_venice_s_gamut3_cine_matrices_f32,
    g_s_log_matrices_f32,
    g_s_log2_matrices_f32,
    g_s_log3_matrices_f32,
    g_cie_rgb_matrices_f32,
    g_adobe_wide_gamut_rgb_matrices_f32,
    g_romm_rgb_matrices_f32,
    g_rimm_rgb_matrices_f32,
    g_erimm_rgb_matrices_f32,
    g_filmlight_e_gamut_matrices_f32,
    g_filmlight_t_log_matrices_f32,
    g_f_gamut_matrices_f32,
    g_fujifilm_f_log_matrices_f32,
    g_n_gamut_matrices_f32,
    g_n_log_matrices_f32,
    g_dji_d_gamut_matrices_f32,
    g_protune_native_matrices_f32,
    g_itu_r_bt470_525_matrices_f32,
    g_itu_r_bt470_625_matrices_f32,
    g_smpte_240m_matrices_f32,
    g_smpte_c_matrices_f32,
    g_dcdm_xyz_matrices_f32,
    g_best_rgb_matrices_f32,
    g_beta_rgb_matrices_f32,
    g_don_rgb_4_matrices_f32,
    g_ekta_space_ps5_matrices_f32,
    g_max_rgb_matrices_f32,
    g_russell_rgb_matrices_f32,
    g_sharp_rgb_matrices_f32,
    g_eci_rgb_v2_matrices_f32,
    g_adobe_rgb_1998_matrices_f32,
    g_prophoto_rgb_matrices_f32,
    g_davinci_wide_gamut_matrices_f32,
    g_davinci_intermediate_matrices_f32,
    g_blackmagic_wide_gamut_matrices_f32,
    g_blackmagic_film_matrices_f32,
    g_blackmagic_film_gen5_matrices_f32,
    g_v_gamut_matrices_f32,
    g_v_log_matrices_f32,
    g_s_gamut_matrices_f32,
    g_s_gamut3_matrices_f32,
    g_s_gamut3_cine_matrices_f32,
    g_cinema_gamut_matrices_f32,
    g_canon_log_matrices_f32,
    g_redwidegamutrgb_matrices_f32,
    g_dci_p3_matrices_f32,
    g_dci_p3_p_matrices_f32,
    g_p3_d65_matrices_f32,
    g_ntsc_1953_matrices_f32,
    g_ntsc_1987_matrices_f32,
    g_pal_secam_matrices_f32,
    g_ebu_tech_3213_e_matrices_f32,
    g_apple_rgb_matrices_f32,
    g_colormatch_rgb_matrices_f32,
    g_alexa_wide_gamut_matrices_f32,
    g_p3_d60_matrices_f32,
    g_xtreme_rgb_matrices_f32,
    g_linear_rec709_matrices_f32,
    g_linear_rec2020_matrices_f32,
    g_linear_adobe_rgb_1998_matrices_f32,
    g_linear_p3_d65_matrices_f32,
    g_linear_display_p3_matrices_f32,
    g_linear_prophoto_rgb_matrices_f32,
    g_linear_dci_p3_matrices_f32,
    g_linear_adobe_wide_gamut_rgb_matrices_f32,
    g_linear_apple_rgb_matrices_f32,
    g_linear_colormatch_rgb_matrices_f32,
    g_linear_p3_d60_matrices_f32,
    g_linear_bt470_525_matrices_f32,
    g_linear_bt470_625_matrices_f32,
    g_linear_smpte_240m_matrices_f32,
    g_itu_t_h273_22_unspecified_matrices_f32,
    g_itu_t_h273_generic_film_matrices_f32,
    g_plasa_ansi_e154_matrices_f32,
    g_gamma22_rec709_matrices_f32,
    g_gamma22_adobe_rgb_matrices_f32,
    g_gamma22_p3_d65_matrices_f32,
    g_gamma22_ap1_matrices_f32,
    g_gamma18_rec709_matrices_f32,
    g_rec1886_rec709_matrices_f32,
    g_rec2100_pq_matrices_f32,
    g_rec2100_hlg_matrices_f32,
    g_display_p3_hdr_matrices_f32
};

_Static_assert(
    sizeof(g_rgb_space_matrices_f32) / sizeof(g_rgb_space_matrices_f32[0]) == (size_t)ALWAN_RGB_SPACE_COUNT,
    "g_rgb_space_matrices_f32[] must have one row per alwan_rgb_space"
);

#endif /* ALWAN_WITH_F32 */

#endif /* ALWAN_EMBED_DATA */
