/* ----------------------------------------------------------------
 * Embedded RGB Space Data (optional)
 * Used when ALWAN_EMBED_DATA is defined
 * IMPORTANT: Array order MUST match enum alwan_rgb_space order!
 * ---------------------------------------------------------------- */
#if ALWAN_EMBED_DATA

/* Disable float conversion warnings for embedded CSV data */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

#if defined(_MSC_VER)
__pragma(warning( disable: 4211 ))  /* nonstandard extension: redefined extern to static */
#endif

/* Core RGB spaces - Order MUST match enum in alwan.h */
static alwan_scalar const g_srgb[] = {
#include "data/rgb_spaces/srgb.csv"
};

static alwan_scalar const g_bt709[] = {
#include "data/rgb_spaces/bt709.csv"
};

static alwan_scalar const g_display_p3[] = {
#include "data/rgb_spaces/display_p3.csv"
};

static alwan_scalar const g_bt2020[] = {
#include "data/rgb_spaces/bt2020.csv"
};

static alwan_scalar const g_aces2065_1[] = {
#include "data/rgb_spaces/aces2065-1.csv"
};

static alwan_scalar const g_acescg[] = {
#include "data/rgb_spaces/acescg.csv"
};

static alwan_scalar const g_acesproxy[] = {
#include "data/rgb_spaces/acesproxy.csv"
};

static alwan_scalar const g_acescc[] = {
#include "data/rgb_spaces/acescc.csv"
};

static alwan_scalar const g_acescct[] = {
#include "data/rgb_spaces/acescct.csv"
};

static alwan_scalar const g_arri_wide_gamut_3[] = {
#include "data/rgb_spaces/arri_wide_gamut_3.csv"
};

static alwan_scalar const g_arri_wide_gamut_4[] = {
#include "data/rgb_spaces/arri_wide_gamut_4.csv"
};

static alwan_scalar const g_redcolor[] = {
#include "data/rgb_spaces/redcolor.csv"
};

static alwan_scalar const g_redcolor2[] = {
#include "data/rgb_spaces/redcolor2.csv"
};

static alwan_scalar const g_redcolor3[] = {
#include "data/rgb_spaces/redcolor3.csv"
};

static alwan_scalar const g_redcolor4[] = {
#include "data/rgb_spaces/redcolor4.csv"
};

static alwan_scalar const g_dragoncolor[] = {
#include "data/rgb_spaces/dragoncolor.csv"
};

static alwan_scalar const g_dragoncolor2[] = {
#include "data/rgb_spaces/dragoncolor2.csv"
};

static alwan_scalar const g_venice_s_gamut3[] = {
#include "data/rgb_spaces/venice_s-gamut3.csv"
};

static alwan_scalar const g_venice_s_gamut3_cine[] = {
#include "data/rgb_spaces/venice_s-gamut3cine.csv"
};

static alwan_scalar const g_cie_rgb[] = {
#include "data/rgb_spaces/cie_rgb.csv"
};

static alwan_scalar const g_adobe_wide_gamut_rgb[] = {
#include "data/rgb_spaces/adobe_wide_gamut_rgb.csv"
};

static alwan_scalar const g_romm_rgb[] = {
#include "data/rgb_spaces/romm_rgb.csv"
};

static alwan_scalar const g_rimm_rgb[] = {
#include "data/rgb_spaces/rimm_rgb.csv"
};

static alwan_scalar const g_erimm_rgb[] = {
#include "data/rgb_spaces/erimm_rgb.csv"
};

static alwan_scalar const g_filmlight_e_gamut[] = {
#include "data/rgb_spaces/filmlight_e-gamut.csv"
};

static alwan_scalar const g_f_gamut[] = {
#include "data/rgb_spaces/f-gamut.csv"
};

static alwan_scalar const g_n_gamut[] = {
#include "data/rgb_spaces/n-gamut.csv"
};

static alwan_scalar const g_dji_d_gamut[] = {
#include "data/rgb_spaces/dji_d-gamut.csv"
};

static alwan_scalar const g_protune_native[] = {
#include "data/rgb_spaces/protune_native.csv"
};

static alwan_scalar const g_itu_r_bt470_525[] = {
#include "data/rgb_spaces/itu-r_bt470_-_525.csv"
};

static alwan_scalar const g_itu_r_bt470_625[] = {
#include "data/rgb_spaces/itu-r_bt470_-_625.csv"
};

static alwan_scalar const g_smpte_240m[] = {
#include "data/rgb_spaces/smpte_240m.csv"
};

static alwan_scalar const g_smpte_c[] = {
#include "data/rgb_spaces/smpte_c.csv"
};

static alwan_scalar const g_dcdm_xyz[] = {
#include "data/rgb_spaces/dcdm_xyz.csv"
};

static alwan_scalar const g_best_rgb[] = {
#include "data/rgb_spaces/best_rgb.csv"
};

static alwan_scalar const g_beta_rgb[] = {
#include "data/rgb_spaces/beta_rgb.csv"
};

static alwan_scalar const g_don_rgb_4[] = {
#include "data/rgb_spaces/don_rgb_4.csv"
};

static alwan_scalar const g_ekta_space_ps5[] = {
#include "data/rgb_spaces/ekta_space_ps_5.csv"
};

static alwan_scalar const g_max_rgb[] = {
#include "data/rgb_spaces/max_rgb.csv"
};

static alwan_scalar const g_russell_rgb[] = {
#include "data/rgb_spaces/russell_rgb.csv"
};

static alwan_scalar const g_sharp_rgb[] = {
#include "data/rgb_spaces/sharp_rgb.csv"
};

static alwan_scalar const g_eci_rgb_v2[] = {
#include "data/rgb_spaces/eci_rgb_v2.csv"
};

/* EXISTING SPACES */
static alwan_scalar const g_adobe_rgb_1998[] = {
#include "data/rgb_spaces/adobe_rgb_1998.csv"
};

static alwan_scalar const g_prophoto_rgb[] = {
#include "data/rgb_spaces/prophoto_rgb.csv"
};

static alwan_scalar const g_davinci_wide_gamut[] = {
#include "data/rgb_spaces/davinci_wide_gamut.csv"
};

static alwan_scalar const g_blackmagic_wide_gamut[] = {
#include "data/rgb_spaces/blackmagic_wide_gamut.csv"
};

static alwan_scalar const g_v_gamut[] = {
#include "data/rgb_spaces/v-gamut.csv"
};

static alwan_scalar const g_s_gamut[] = {
#include "data/rgb_spaces/s-gamut.csv"
};

static alwan_scalar const g_s_gamut3[] = {
#include "data/rgb_spaces/s-gamut3.csv"
};

static alwan_scalar const g_s_gamut3_cine[] = {
#include "data/rgb_spaces/s-gamut3cine.csv"
};

static alwan_scalar const g_cinema_gamut[] = {
#include "data/rgb_spaces/cinema_gamut.csv"
};

static alwan_scalar const g_redwidegamutrgb[] = {
#include "data/rgb_spaces/redwidegamutrgb.csv"
};

static alwan_scalar const g_dci_p3[] = {
#include "data/rgb_spaces/dci-p3.csv"
};

static alwan_scalar const g_p3_d65[] = {
#include "data/rgb_spaces/p3-d65.csv"
};

static alwan_scalar const g_ntsc_1953[] = {
#include "data/rgb_spaces/ntsc_1953.csv"
};

static alwan_scalar const g_ntsc_1987[] = {
#include "data/rgb_spaces/ntsc_1987.csv"
};

static alwan_scalar const g_pal_secam[] = {
#include "data/rgb_spaces/pal_secam.csv"
};

static alwan_scalar const g_apple_rgb[] = {
#include "data/rgb_spaces/apple_rgb.csv"
};

static alwan_scalar const g_colormatch_rgb[] = {
#include "data/rgb_spaces/colormatch_rgb.csv"
};

/* Additional high-priority spaces (11.1 continued) */
static alwan_scalar const g_alexa_wide_gamut[] = {
#include "data/rgb_spaces/alexa_wide_gamut.csv"
};

static alwan_scalar const g_p3_d60[] = {
#include "data/rgb_spaces/p3_d60.csv"
};

static alwan_scalar const g_linear_srgb[] = {
#include "data/rgb_spaces/linear_srgb.csv"
};

static alwan_scalar const g_linear_rec2020[] = {
#include "data/rgb_spaces/linear_rec2020.csv"
};

ALWAN_DIAG_POP

/* Array of pointers to RGB space data - Order MUST match enum */
static alwan_scalar const * const g_rgb_space_data[] = {
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
    g_redcolor,
    g_redcolor2,
    g_redcolor3,
    g_redcolor4,
    g_dragoncolor,
    g_dragoncolor2,
    g_venice_s_gamut3,
    g_venice_s_gamut3_cine,
    g_cie_rgb,
    g_adobe_wide_gamut_rgb,
    g_romm_rgb,
    g_rimm_rgb,
    g_erimm_rgb,
    g_filmlight_e_gamut,
    g_f_gamut,
    g_n_gamut,
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
    g_blackmagic_wide_gamut,
    g_v_gamut,
    g_s_gamut,
    g_s_gamut3,
    g_s_gamut3_cine,
    g_cinema_gamut,
    g_redwidegamutrgb,
    g_dci_p3,
    g_p3_d65,
    g_ntsc_1953,
    g_ntsc_1987,
    g_pal_secam,
    g_apple_rgb,
    g_colormatch_rgb,
    g_alexa_wide_gamut,
    g_p3_d60,
    g_linear_srgb,
    g_linear_rec2020
};

static size_t const g_rgb_space_data_count = sizeof(g_rgb_space_data) / sizeof(g_rgb_space_data[0]);

#endif /* ALWAN_EMBED_DATA */
