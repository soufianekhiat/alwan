/* ----------------------------------------------------------------
 * Embedded RGB Space Data (optional)
 * Used when ALWAN_EMBED_DATA is defined
 * IMPORTANT: Array order MUST match enum alwan_rgb_space order!
 * ---------------------------------------------------------------- */
#if ALWAN_EMBED_DATA

/* Compiled in every build: the f64-internal facades resolve RGB space
 * descriptors in f64 from their f32 entry points, so this half has to exist
 * even when the f64 public surface is excluded. See ALWAN_WITH_F64_FACADE. */
#if ALWAN_WITH_F64_FACADE

/* Disable float conversion warnings for embedded CSV data */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_DIAG_DISABLE_EXTERN_TO_STATIC

/* Core RGB spaces - Order MUST match enum in alwan.h */
static alwan_f64 const g_srgb[] = {
#include "data/rgb_spaces/srgb.csv"
};

static alwan_f64 const g_bt709[] = {
#include "data/rgb_spaces/bt709.csv"
};

static alwan_f64 const g_display_p3[] = {
#include "data/rgb_spaces/display_p3.csv"
};

static alwan_f64 const g_bt2020[] = {
#include "data/rgb_spaces/bt2020.csv"
};

static alwan_f64 const g_aces2065_1[] = {
#include "data/rgb_spaces/aces2065-1.csv"
};

static alwan_f64 const g_acescg[] = {
#include "data/rgb_spaces/acescg.csv"
};

static alwan_f64 const g_acesproxy[] = {
#include "data/rgb_spaces/acesproxy.csv"
};

static alwan_f64 const g_acescc[] = {
#include "data/rgb_spaces/acescc.csv"
};

static alwan_f64 const g_acescct[] = {
#include "data/rgb_spaces/acescct.csv"
};

static alwan_f64 const g_arri_wide_gamut_3[] = {
#include "data/rgb_spaces/arri_wide_gamut_3.csv"
};

static alwan_f64 const g_arri_wide_gamut_4[] = {
#include "data/rgb_spaces/arri_wide_gamut_4.csv"
};

static alwan_f64 const g_arri_logc3[] = {
#include "data/rgb_spaces/arri_logc3.csv"
};

static alwan_f64 const g_arri_logc4[] = {
#include "data/rgb_spaces/arri_logc4.csv"
};

static alwan_f64 const g_redcolor[] = {
#include "data/rgb_spaces/redcolor.csv"
};

static alwan_f64 const g_redcolor2[] = {
#include "data/rgb_spaces/redcolor2.csv"
};

static alwan_f64 const g_redcolor3[] = {
#include "data/rgb_spaces/redcolor3.csv"
};

static alwan_f64 const g_redcolor4[] = {
#include "data/rgb_spaces/redcolor4.csv"
};

static alwan_f64 const g_dragoncolor[] = {
#include "data/rgb_spaces/dragoncolor.csv"
};

static alwan_f64 const g_dragoncolor2[] = {
#include "data/rgb_spaces/dragoncolor2.csv"
};

static alwan_f64 const g_redlog[] = {
#include "data/rgb_spaces/redlog.csv"
};

static alwan_f64 const g_venice_s_gamut3[] = {
#include "data/rgb_spaces/venice_s-gamut3.csv"
};

static alwan_f64 const g_venice_s_gamut3_cine[] = {
#include "data/rgb_spaces/venice_s-gamut3cine.csv"
};

static alwan_f64 const g_s_log[] = {
#include "data/rgb_spaces/s-log.csv"
};

static alwan_f64 const g_s_log2[] = {
#include "data/rgb_spaces/s-log2.csv"
};

static alwan_f64 const g_s_log3[] = {
#include "data/rgb_spaces/s-log3.csv"
};

static alwan_f64 const g_cie_rgb[] = {
#include "data/rgb_spaces/cie_rgb.csv"
};

static alwan_f64 const g_adobe_wide_gamut_rgb[] = {
#include "data/rgb_spaces/adobe_wide_gamut_rgb.csv"
};

static alwan_f64 const g_romm_rgb[] = {
#include "data/rgb_spaces/romm_rgb.csv"
};

static alwan_f64 const g_rimm_rgb[] = {
#include "data/rgb_spaces/rimm_rgb.csv"
};

static alwan_f64 const g_erimm_rgb[] = {
#include "data/rgb_spaces/erimm_rgb.csv"
};

static alwan_f64 const g_filmlight_e_gamut[] = {
#include "data/rgb_spaces/filmlight_e-gamut.csv"
};

static alwan_f64 const g_filmlight_t_log[] = {
#include "data/rgb_spaces/filmlight_t-log.csv"
};

static alwan_f64 const g_f_gamut[] = {
#include "data/rgb_spaces/f-gamut.csv"
};

static alwan_f64 const g_fujifilm_f_log[] = {
#include "data/rgb_spaces/fujifilm_f-log.csv"
};

static alwan_f64 const g_n_gamut[] = {
#include "data/rgb_spaces/n-gamut.csv"
};

static alwan_f64 const g_n_log[] = {
#include "data/rgb_spaces/n-log.csv"
};

static alwan_f64 const g_dji_d_gamut[] = {
#include "data/rgb_spaces/dji_d-gamut.csv"
};

static alwan_f64 const g_protune_native[] = {
#include "data/rgb_spaces/protune_native.csv"
};

static alwan_f64 const g_itu_r_bt470_525[] = {
#include "data/rgb_spaces/itu-r_bt470_-_525.csv"
};

static alwan_f64 const g_itu_r_bt470_625[] = {
#include "data/rgb_spaces/itu-r_bt470_-_625.csv"
};

static alwan_f64 const g_smpte_240m[] = {
#include "data/rgb_spaces/smpte_240m.csv"
};

static alwan_f64 const g_smpte_c[] = {
#include "data/rgb_spaces/smpte_c.csv"
};

static alwan_f64 const g_dcdm_xyz[] = {
#include "data/rgb_spaces/dcdm_xyz.csv"
};

static alwan_f64 const g_best_rgb[] = {
#include "data/rgb_spaces/best_rgb.csv"
};

static alwan_f64 const g_beta_rgb[] = {
#include "data/rgb_spaces/beta_rgb.csv"
};

static alwan_f64 const g_don_rgb_4[] = {
#include "data/rgb_spaces/don_rgb_4.csv"
};

static alwan_f64 const g_ekta_space_ps5[] = {
#include "data/rgb_spaces/ekta_space_ps_5.csv"
};

static alwan_f64 const g_max_rgb[] = {
#include "data/rgb_spaces/max_rgb.csv"
};

static alwan_f64 const g_russell_rgb[] = {
#include "data/rgb_spaces/russell_rgb.csv"
};

static alwan_f64 const g_sharp_rgb[] = {
#include "data/rgb_spaces/sharp_rgb.csv"
};

static alwan_f64 const g_eci_rgb_v2[] = {
#include "data/rgb_spaces/eci_rgb_v2.csv"
};

/* EXISTING SPACES */
static alwan_f64 const g_adobe_rgb_1998[] = {
#include "data/rgb_spaces/adobe_rgb_1998.csv"
};

static alwan_f64 const g_prophoto_rgb[] = {
#include "data/rgb_spaces/prophoto_rgb.csv"
};

static alwan_f64 const g_davinci_wide_gamut[] = {
#include "data/rgb_spaces/davinci_wide_gamut.csv"
};

static alwan_f64 const g_davinci_intermediate[] = {
#include "data/rgb_spaces/davinci_intermediate.csv"
};

static alwan_f64 const g_blackmagic_wide_gamut[] = {
#include "data/rgb_spaces/blackmagic_wide_gamut.csv"
};

static alwan_f64 const g_blackmagic_film[] = {
#include "data/rgb_spaces/blackmagic_film.csv"
};

static alwan_f64 const g_blackmagic_film_gen5[] = {
#include "data/rgb_spaces/blackmagic_film_gen5.csv"
};

static alwan_f64 const g_v_gamut[] = {
#include "data/rgb_spaces/v-gamut.csv"
};

static alwan_f64 const g_v_log[] = {
#include "data/rgb_spaces/v-log.csv"
};

static alwan_f64 const g_s_gamut[] = {
#include "data/rgb_spaces/s-gamut.csv"
};

static alwan_f64 const g_s_gamut3[] = {
#include "data/rgb_spaces/s-gamut3.csv"
};

static alwan_f64 const g_s_gamut3_cine[] = {
#include "data/rgb_spaces/s-gamut3cine.csv"
};

static alwan_f64 const g_cinema_gamut[] = {
#include "data/rgb_spaces/cinema_gamut.csv"
};

static alwan_f64 const g_canon_log[] = {
#include "data/rgb_spaces/canon_log.csv"
};

static alwan_f64 const g_redwidegamutrgb[] = {
#include "data/rgb_spaces/redwidegamutrgb.csv"
};

static alwan_f64 const g_dci_p3[] = {
#include "data/rgb_spaces/dci-p3.csv"
};

static alwan_f64 const g_dci_p3_p[] = {
#include "data/rgb_spaces/dci-p3-p.csv"
};

static alwan_f64 const g_p3_d65[] = {
#include "data/rgb_spaces/p3-d65.csv"
};

static alwan_f64 const g_ntsc_1953[] = {
#include "data/rgb_spaces/ntsc_1953.csv"
};

static alwan_f64 const g_ntsc_1987[] = {
#include "data/rgb_spaces/ntsc_1987.csv"
};

static alwan_f64 const g_pal_secam[] = {
#include "data/rgb_spaces/pal_secam.csv"
};

static alwan_f64 const g_ebu_tech_3213_e[] = {
#include "data/rgb_spaces/ebu_tech_3213-e.csv"
};

static alwan_f64 const g_apple_rgb[] = {
#include "data/rgb_spaces/apple_rgb.csv"
};

static alwan_f64 const g_colormatch_rgb[] = {
#include "data/rgb_spaces/colormatch_rgb.csv"
};

/* Additional RGB color spaces */
static alwan_f64 const g_alexa_wide_gamut[] = {
#include "data/rgb_spaces/alexa_wide_gamut.csv"
};

static alwan_f64 const g_p3_d60[] = {
#include "data/rgb_spaces/p3-d60.csv"
};

static alwan_f64 const g_xtreme_rgb[] = {
#include "data/rgb_spaces/xtreme_rgb.csv"
};

static alwan_f64 const g_linear_rec709[] = {
#include "data/rgb_spaces/linear_srgb.csv"
};

static alwan_f64 const g_linear_rec2020[] = {
#include "data/rgb_spaces/linear_rec2020.csv"
};

static alwan_f64 const g_linear_adobe_rgb_1998[] = {
#include "data/rgb_spaces/linear_adobe_rgb_1998.csv"
};

static alwan_f64 const g_linear_p3_d65[] = {
#include "data/rgb_spaces/linear_p3_d65.csv"
};

static alwan_f64 const g_linear_display_p3[] = {
#include "data/rgb_spaces/linear_display_p3.csv"
};

static alwan_f64 const g_linear_prophoto_rgb[] = {
#include "data/rgb_spaces/linear_prophoto_rgb.csv"
};

static alwan_f64 const g_linear_dci_p3[] = {
#include "data/rgb_spaces/linear_dci_p3.csv"
};

static alwan_f64 const g_linear_adobe_wide_gamut_rgb[] = {
#include "data/rgb_spaces/linear_adobe_wide_gamut_rgb.csv"
};

static alwan_f64 const g_linear_apple_rgb[] = {
#include "data/rgb_spaces/linear_apple_rgb.csv"
};

static alwan_f64 const g_linear_colormatch_rgb[] = {
#include "data/rgb_spaces/linear_colormatch_rgb.csv"
};

static alwan_f64 const g_linear_p3_d60[] = {
#include "data/rgb_spaces/linear_p3_d60.csv"
};

static alwan_f64 const g_linear_bt470_525[] = {
#include "data/rgb_spaces/linear_bt470_525.csv"
};

static alwan_f64 const g_linear_bt470_625[] = {
#include "data/rgb_spaces/linear_bt470_625.csv"
};

static alwan_f64 const g_linear_smpte_240m[] = {
#include "data/rgb_spaces/linear_smpte_240m.csv"
};

static alwan_f64 const g_itu_t_h273_22_unspecified[] = {
#include "data/rgb_spaces/itu-t_h273_22_unspecified.csv"
};

static alwan_f64 const g_itu_t_h273_generic_film[] = {
#include "data/rgb_spaces/itu-t_h273_generic_film.csv"
};

static alwan_f64 const g_plasa_ansi_e154[] = {
#include "data/rgb_spaces/plasa_ansi_e154.csv"
};

static alwan_f64 const g_gamma22_rec709[] = {
#include "data/rgb_spaces/gamma_22_rec709.csv"
};

static alwan_f64 const g_gamma22_adobe_rgb[] = {
#include "data/rgb_spaces/gamma_22_adobe_rgb.csv"
};

static alwan_f64 const g_gamma22_p3_d65[] = {
#include "data/rgb_spaces/gamma_22_p3-d65.csv"
};

static alwan_f64 const g_gamma22_ap1[] = {
#include "data/rgb_spaces/gamma_22_ap1.csv"
};

static alwan_f64 const g_gamma18_rec709[] = {
#include "data/rgb_spaces/gamma_18_rec709.csv"
};

/* ColorInterop Display Color Spaces */
static alwan_f64 const g_rec1886_rec709[] = {
#include "data/rgb_spaces/bt709.csv"
};
static alwan_f64 const g_rec2100_pq[] = {
#include "data/rgb_spaces/bt2020.csv"
};
static alwan_f64 const g_rec2100_hlg[] = {
#include "data/rgb_spaces/bt2020.csv"
};
static alwan_f64 const g_display_p3_hdr[] = {
#include "data/rgb_spaces/display_p3.csv"
};

ALWAN_DIAG_POP

/* Array of pointers to RGB space data - Order MUST match enum */
static alwan_f64 const * const g_rgb_space_data[] = {
    g_srgb,
    g_bt709,
    g_display_p3,
    g_bt2020,
    g_aces2065_1,
    g_acescg,
    g_acesproxy,
    g_acescc,
    g_acescct,
    g_arri_wide_gamut_3,
    g_arri_wide_gamut_4,
    g_arri_logc3,
    g_arri_logc4,
    g_redcolor,
    g_redcolor2,
    g_redcolor3,
    g_redcolor4,
    g_dragoncolor,
    g_dragoncolor2,
    g_redlog,
    g_venice_s_gamut3,
    g_venice_s_gamut3_cine,
    g_s_log,
    g_s_log2,
    g_s_log3,
    g_cie_rgb,
    g_adobe_wide_gamut_rgb,
    g_romm_rgb,
    g_rimm_rgb,
    g_erimm_rgb,
    g_filmlight_e_gamut,
    g_filmlight_t_log,
    g_f_gamut,
    g_fujifilm_f_log,
    g_n_gamut,
    g_n_log,
    g_dji_d_gamut,
    g_protune_native,
    g_itu_r_bt470_525,
    g_itu_r_bt470_625,
    g_smpte_240m,
    g_smpte_c,
    g_dcdm_xyz,
    g_best_rgb,
    g_beta_rgb,
    g_don_rgb_4,
    g_ekta_space_ps5,
    g_max_rgb,
    g_russell_rgb,
    g_sharp_rgb,
    g_eci_rgb_v2,
    g_adobe_rgb_1998,
    g_prophoto_rgb,
    g_davinci_wide_gamut,
    g_davinci_intermediate,
    g_blackmagic_wide_gamut,
    g_blackmagic_film,
    g_blackmagic_film_gen5,
    g_v_gamut,
    g_v_log,
    g_s_gamut,
    g_s_gamut3,
    g_s_gamut3_cine,
    g_cinema_gamut,
    g_canon_log,
    g_redwidegamutrgb,
    g_dci_p3,
    g_dci_p3_p,
    g_p3_d65,
    g_ntsc_1953,
    g_ntsc_1987,
    g_pal_secam,
    g_ebu_tech_3213_e,
    g_apple_rgb,
    g_colormatch_rgb,
    g_alexa_wide_gamut,
    g_p3_d60,
    g_xtreme_rgb,
    g_linear_rec709,
    g_linear_rec2020,
    g_linear_adobe_rgb_1998,
    g_linear_p3_d65,
    g_linear_display_p3,
    g_linear_prophoto_rgb,
    g_linear_dci_p3,
    g_linear_adobe_wide_gamut_rgb,
    g_linear_apple_rgb,
    g_linear_colormatch_rgb,
    g_linear_p3_d60,
    g_linear_bt470_525,
    g_linear_bt470_625,
    g_linear_smpte_240m,
    g_itu_t_h273_22_unspecified,
    g_itu_t_h273_generic_film,
    g_plasa_ansi_e154,
    g_gamma22_rec709,
    g_gamma22_adobe_rgb,
    g_gamma22_p3_d65,
    g_gamma22_ap1,
    g_gamma18_rec709,
    g_rec1886_rec709,
    g_rec2100_pq,
    g_rec2100_hlg,
    g_display_p3_hdr
};


#endif /* ALWAN_WITH_F64_FACADE */

/* ----------------------------------------------------------------
 * f32 native twins of the embedded tables above.
 * Same CSV bytes, stored as float for native f32 readers (no f64->f32
 * double-rounding). Order MUST match enum / the f64 arrays.
 * ---------------------------------------------------------------- */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_DIAG_DISABLE_EXTERN_TO_STATIC

static float const g_srgb_f32[] = {
#include "data/rgb_spaces/srgb.csv"
};

static float const g_bt709_f32[] = {
#include "data/rgb_spaces/bt709.csv"
};

static float const g_display_p3_f32[] = {
#include "data/rgb_spaces/display_p3.csv"
};

static float const g_bt2020_f32[] = {
#include "data/rgb_spaces/bt2020.csv"
};

static float const g_aces2065_1_f32[] = {
#include "data/rgb_spaces/aces2065-1.csv"
};

static float const g_acescg_f32[] = {
#include "data/rgb_spaces/acescg.csv"
};

static float const g_acesproxy_f32[] = {
#include "data/rgb_spaces/acesproxy.csv"
};

static float const g_acescc_f32[] = {
#include "data/rgb_spaces/acescc.csv"
};

static float const g_acescct_f32[] = {
#include "data/rgb_spaces/acescct.csv"
};

static float const g_arri_wide_gamut_3_f32[] = {
#include "data/rgb_spaces/arri_wide_gamut_3.csv"
};

static float const g_arri_wide_gamut_4_f32[] = {
#include "data/rgb_spaces/arri_wide_gamut_4.csv"
};

static float const g_arri_logc3_f32[] = {
#include "data/rgb_spaces/arri_logc3.csv"
};

static float const g_arri_logc4_f32[] = {
#include "data/rgb_spaces/arri_logc4.csv"
};

static float const g_redcolor_f32[] = {
#include "data/rgb_spaces/redcolor.csv"
};

static float const g_redcolor2_f32[] = {
#include "data/rgb_spaces/redcolor2.csv"
};

static float const g_redcolor3_f32[] = {
#include "data/rgb_spaces/redcolor3.csv"
};

static float const g_redcolor4_f32[] = {
#include "data/rgb_spaces/redcolor4.csv"
};

static float const g_dragoncolor_f32[] = {
#include "data/rgb_spaces/dragoncolor.csv"
};

static float const g_dragoncolor2_f32[] = {
#include "data/rgb_spaces/dragoncolor2.csv"
};

static float const g_redlog_f32[] = {
#include "data/rgb_spaces/redlog.csv"
};

static float const g_venice_s_gamut3_f32[] = {
#include "data/rgb_spaces/venice_s-gamut3.csv"
};

static float const g_venice_s_gamut3_cine_f32[] = {
#include "data/rgb_spaces/venice_s-gamut3cine.csv"
};

static float const g_s_log_f32[] = {
#include "data/rgb_spaces/s-log.csv"
};

static float const g_s_log2_f32[] = {
#include "data/rgb_spaces/s-log2.csv"
};

static float const g_s_log3_f32[] = {
#include "data/rgb_spaces/s-log3.csv"
};

static float const g_cie_rgb_f32[] = {
#include "data/rgb_spaces/cie_rgb.csv"
};

static float const g_adobe_wide_gamut_rgb_f32[] = {
#include "data/rgb_spaces/adobe_wide_gamut_rgb.csv"
};

static float const g_romm_rgb_f32[] = {
#include "data/rgb_spaces/romm_rgb.csv"
};

static float const g_rimm_rgb_f32[] = {
#include "data/rgb_spaces/rimm_rgb.csv"
};

static float const g_erimm_rgb_f32[] = {
#include "data/rgb_spaces/erimm_rgb.csv"
};

static float const g_filmlight_e_gamut_f32[] = {
#include "data/rgb_spaces/filmlight_e-gamut.csv"
};

static float const g_filmlight_t_log_f32[] = {
#include "data/rgb_spaces/filmlight_t-log.csv"
};

static float const g_f_gamut_f32[] = {
#include "data/rgb_spaces/f-gamut.csv"
};

static float const g_fujifilm_f_log_f32[] = {
#include "data/rgb_spaces/fujifilm_f-log.csv"
};

static float const g_n_gamut_f32[] = {
#include "data/rgb_spaces/n-gamut.csv"
};

static float const g_n_log_f32[] = {
#include "data/rgb_spaces/n-log.csv"
};

static float const g_dji_d_gamut_f32[] = {
#include "data/rgb_spaces/dji_d-gamut.csv"
};

static float const g_protune_native_f32[] = {
#include "data/rgb_spaces/protune_native.csv"
};

static float const g_itu_r_bt470_525_f32[] = {
#include "data/rgb_spaces/itu-r_bt470_-_525.csv"
};

static float const g_itu_r_bt470_625_f32[] = {
#include "data/rgb_spaces/itu-r_bt470_-_625.csv"
};

static float const g_smpte_240m_f32[] = {
#include "data/rgb_spaces/smpte_240m.csv"
};

static float const g_smpte_c_f32[] = {
#include "data/rgb_spaces/smpte_c.csv"
};

static float const g_dcdm_xyz_f32[] = {
#include "data/rgb_spaces/dcdm_xyz.csv"
};

static float const g_best_rgb_f32[] = {
#include "data/rgb_spaces/best_rgb.csv"
};

static float const g_beta_rgb_f32[] = {
#include "data/rgb_spaces/beta_rgb.csv"
};

static float const g_don_rgb_4_f32[] = {
#include "data/rgb_spaces/don_rgb_4.csv"
};

static float const g_ekta_space_ps5_f32[] = {
#include "data/rgb_spaces/ekta_space_ps_5.csv"
};

static float const g_max_rgb_f32[] = {
#include "data/rgb_spaces/max_rgb.csv"
};

static float const g_russell_rgb_f32[] = {
#include "data/rgb_spaces/russell_rgb.csv"
};

static float const g_sharp_rgb_f32[] = {
#include "data/rgb_spaces/sharp_rgb.csv"
};

static float const g_eci_rgb_v2_f32[] = {
#include "data/rgb_spaces/eci_rgb_v2.csv"
};

static float const g_adobe_rgb_1998_f32[] = {
#include "data/rgb_spaces/adobe_rgb_1998.csv"
};

static float const g_prophoto_rgb_f32[] = {
#include "data/rgb_spaces/prophoto_rgb.csv"
};

static float const g_davinci_wide_gamut_f32[] = {
#include "data/rgb_spaces/davinci_wide_gamut.csv"
};

static float const g_davinci_intermediate_f32[] = {
#include "data/rgb_spaces/davinci_intermediate.csv"
};

static float const g_blackmagic_wide_gamut_f32[] = {
#include "data/rgb_spaces/blackmagic_wide_gamut.csv"
};

static float const g_blackmagic_film_f32[] = {
#include "data/rgb_spaces/blackmagic_film.csv"
};

static float const g_blackmagic_film_gen5_f32[] = {
#include "data/rgb_spaces/blackmagic_film_gen5.csv"
};

static float const g_v_gamut_f32[] = {
#include "data/rgb_spaces/v-gamut.csv"
};

static float const g_v_log_f32[] = {
#include "data/rgb_spaces/v-log.csv"
};

static float const g_s_gamut_f32[] = {
#include "data/rgb_spaces/s-gamut.csv"
};

static float const g_s_gamut3_f32[] = {
#include "data/rgb_spaces/s-gamut3.csv"
};

static float const g_s_gamut3_cine_f32[] = {
#include "data/rgb_spaces/s-gamut3cine.csv"
};

static float const g_cinema_gamut_f32[] = {
#include "data/rgb_spaces/cinema_gamut.csv"
};

static float const g_canon_log_f32[] = {
#include "data/rgb_spaces/canon_log.csv"
};

static float const g_redwidegamutrgb_f32[] = {
#include "data/rgb_spaces/redwidegamutrgb.csv"
};

static float const g_dci_p3_f32[] = {
#include "data/rgb_spaces/dci-p3.csv"
};

static float const g_dci_p3_p_f32[] = {
#include "data/rgb_spaces/dci-p3-p.csv"
};

static float const g_p3_d65_f32[] = {
#include "data/rgb_spaces/p3-d65.csv"
};

static float const g_ntsc_1953_f32[] = {
#include "data/rgb_spaces/ntsc_1953.csv"
};

static float const g_ntsc_1987_f32[] = {
#include "data/rgb_spaces/ntsc_1987.csv"
};

static float const g_pal_secam_f32[] = {
#include "data/rgb_spaces/pal_secam.csv"
};

static float const g_ebu_tech_3213_e_f32[] = {
#include "data/rgb_spaces/ebu_tech_3213-e.csv"
};

static float const g_apple_rgb_f32[] = {
#include "data/rgb_spaces/apple_rgb.csv"
};

static float const g_colormatch_rgb_f32[] = {
#include "data/rgb_spaces/colormatch_rgb.csv"
};

static float const g_alexa_wide_gamut_f32[] = {
#include "data/rgb_spaces/alexa_wide_gamut.csv"
};

static float const g_p3_d60_f32[] = {
#include "data/rgb_spaces/p3-d60.csv"
};

static float const g_xtreme_rgb_f32[] = {
#include "data/rgb_spaces/xtreme_rgb.csv"
};

static float const g_linear_rec709_f32[] = {
#include "data/rgb_spaces/linear_srgb.csv"
};

static float const g_linear_rec2020_f32[] = {
#include "data/rgb_spaces/linear_rec2020.csv"
};

static float const g_linear_adobe_rgb_1998_f32[] = {
#include "data/rgb_spaces/linear_adobe_rgb_1998.csv"
};

static float const g_linear_p3_d65_f32[] = {
#include "data/rgb_spaces/linear_p3_d65.csv"
};

static float const g_linear_display_p3_f32[] = {
#include "data/rgb_spaces/linear_display_p3.csv"
};

static float const g_linear_prophoto_rgb_f32[] = {
#include "data/rgb_spaces/linear_prophoto_rgb.csv"
};

static float const g_linear_dci_p3_f32[] = {
#include "data/rgb_spaces/linear_dci_p3.csv"
};

static float const g_linear_adobe_wide_gamut_rgb_f32[] = {
#include "data/rgb_spaces/linear_adobe_wide_gamut_rgb.csv"
};

static float const g_linear_apple_rgb_f32[] = {
#include "data/rgb_spaces/linear_apple_rgb.csv"
};

static float const g_linear_colormatch_rgb_f32[] = {
#include "data/rgb_spaces/linear_colormatch_rgb.csv"
};

static float const g_linear_p3_d60_f32[] = {
#include "data/rgb_spaces/linear_p3_d60.csv"
};

static float const g_linear_bt470_525_f32[] = {
#include "data/rgb_spaces/linear_bt470_525.csv"
};

static float const g_linear_bt470_625_f32[] = {
#include "data/rgb_spaces/linear_bt470_625.csv"
};

static float const g_linear_smpte_240m_f32[] = {
#include "data/rgb_spaces/linear_smpte_240m.csv"
};

static float const g_itu_t_h273_22_unspecified_f32[] = {
#include "data/rgb_spaces/itu-t_h273_22_unspecified.csv"
};

static float const g_itu_t_h273_generic_film_f32[] = {
#include "data/rgb_spaces/itu-t_h273_generic_film.csv"
};

static float const g_plasa_ansi_e154_f32[] = {
#include "data/rgb_spaces/plasa_ansi_e154.csv"
};

static float const g_gamma22_rec709_f32[] = {
#include "data/rgb_spaces/gamma_22_rec709.csv"
};

static float const g_gamma22_adobe_rgb_f32[] = {
#include "data/rgb_spaces/gamma_22_adobe_rgb.csv"
};

static float const g_gamma22_p3_d65_f32[] = {
#include "data/rgb_spaces/gamma_22_p3-d65.csv"
};

static float const g_gamma22_ap1_f32[] = {
#include "data/rgb_spaces/gamma_22_ap1.csv"
};

static float const g_gamma18_rec709_f32[] = {
#include "data/rgb_spaces/gamma_18_rec709.csv"
};

static float const g_rec1886_rec709_f32[] = {
#include "data/rgb_spaces/bt709.csv"
};

static float const g_rec2100_pq_f32[] = {
#include "data/rgb_spaces/bt2020.csv"
};

static float const g_rec2100_hlg_f32[] = {
#include "data/rgb_spaces/bt2020.csv"
};

static float const g_display_p3_hdr_f32[] = {
#include "data/rgb_spaces/display_p3.csv"
};

ALWAN_DIAG_POP

static float const * const g_rgb_space_data_f32[] = {
    g_srgb_f32,
    g_bt709_f32,
    g_display_p3_f32,
    g_bt2020_f32,
    g_aces2065_1_f32,
    g_acescg_f32,
    g_acesproxy_f32,
    g_acescc_f32,
    g_acescct_f32,
    g_arri_wide_gamut_3_f32,
    g_arri_wide_gamut_4_f32,
    g_arri_logc3_f32,
    g_arri_logc4_f32,
    g_redcolor_f32,
    g_redcolor2_f32,
    g_redcolor3_f32,
    g_redcolor4_f32,
    g_dragoncolor_f32,
    g_dragoncolor2_f32,
    g_redlog_f32,
    g_venice_s_gamut3_f32,
    g_venice_s_gamut3_cine_f32,
    g_s_log_f32,
    g_s_log2_f32,
    g_s_log3_f32,
    g_cie_rgb_f32,
    g_adobe_wide_gamut_rgb_f32,
    g_romm_rgb_f32,
    g_rimm_rgb_f32,
    g_erimm_rgb_f32,
    g_filmlight_e_gamut_f32,
    g_filmlight_t_log_f32,
    g_f_gamut_f32,
    g_fujifilm_f_log_f32,
    g_n_gamut_f32,
    g_n_log_f32,
    g_dji_d_gamut_f32,
    g_protune_native_f32,
    g_itu_r_bt470_525_f32,
    g_itu_r_bt470_625_f32,
    g_smpte_240m_f32,
    g_smpte_c_f32,
    g_dcdm_xyz_f32,
    g_best_rgb_f32,
    g_beta_rgb_f32,
    g_don_rgb_4_f32,
    g_ekta_space_ps5_f32,
    g_max_rgb_f32,
    g_russell_rgb_f32,
    g_sharp_rgb_f32,
    g_eci_rgb_v2_f32,
    g_adobe_rgb_1998_f32,
    g_prophoto_rgb_f32,
    g_davinci_wide_gamut_f32,
    g_davinci_intermediate_f32,
    g_blackmagic_wide_gamut_f32,
    g_blackmagic_film_f32,
    g_blackmagic_film_gen5_f32,
    g_v_gamut_f32,
    g_v_log_f32,
    g_s_gamut_f32,
    g_s_gamut3_f32,
    g_s_gamut3_cine_f32,
    g_cinema_gamut_f32,
    g_canon_log_f32,
    g_redwidegamutrgb_f32,
    g_dci_p3_f32,
    g_dci_p3_p_f32,
    g_p3_d65_f32,
    g_ntsc_1953_f32,
    g_ntsc_1987_f32,
    g_pal_secam_f32,
    g_ebu_tech_3213_e_f32,
    g_apple_rgb_f32,
    g_colormatch_rgb_f32,
    g_alexa_wide_gamut_f32,
    g_p3_d60_f32,
    g_xtreme_rgb_f32,
    g_linear_rec709_f32,
    g_linear_rec2020_f32,
    g_linear_adobe_rgb_1998_f32,
    g_linear_p3_d65_f32,
    g_linear_display_p3_f32,
    g_linear_prophoto_rgb_f32,
    g_linear_dci_p3_f32,
    g_linear_adobe_wide_gamut_rgb_f32,
    g_linear_apple_rgb_f32,
    g_linear_colormatch_rgb_f32,
    g_linear_p3_d60_f32,
    g_linear_bt470_525_f32,
    g_linear_bt470_625_f32,
    g_linear_smpte_240m_f32,
    g_itu_t_h273_22_unspecified_f32,
    g_itu_t_h273_generic_film_f32,
    g_plasa_ansi_e154_f32,
    g_gamma22_rec709_f32,
    g_gamma22_adobe_rgb_f32,
    g_gamma22_p3_d65_f32,
    g_gamma22_ap1_f32,
    g_gamma18_rec709_f32,
    g_rec1886_rec709_f32,
    g_rec2100_pq_f32,
    g_rec2100_hlg_f32,
    g_display_p3_hdr_f32
};

/* The f32 twin is indexed by the same enum as the f64 table and is bounds-checked
 * against the same count, so it has to be the same length. In a dual build nothing
 * else ties them together. */
_Static_assert(
    sizeof(g_rgb_space_data_f32) / sizeof(g_rgb_space_data_f32[0]) == (size_t)ALWAN_RGB_SPACE_COUNT,
    "g_rgb_space_data_f32[] must have one row per alwan_rgb_space"
);

#endif /* ALWAN_WITH_F32 */

#endif /* ALWAN_EMBED_DATA */
