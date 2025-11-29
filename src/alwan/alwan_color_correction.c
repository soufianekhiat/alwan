/* alwan_color_correction.c - Color correction and grading tools
 *
 * Implements:
 * - Lift/Gamma/Gain (LGG)
 * - Color matrix grading presets
 * - Printer lights correction
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <math.h>
#include <string.h>

/* ================================================================
 * Lift/Gamma/Gain (LGG)
 * ================================================================ */

int alwan_lgg_apply(alwan_rgb const *rgb_in, alwan_rgb const *lift,
                    alwan_rgb const *gamma, alwan_rgb const *gain, alwan_rgb *rgb_out)
{
    if (!rgb_in || !lift || !gamma || !gain || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* Apply LGG formula per channel: ((rgb + lift) ^ (1/gamma)) * gain
     * This is a standard color grading formula used in tools like DaVinci Resolve */

    /* Process R channel */
    alwan_scalar lifted_r = rgb_in->r + lift->r;
    if (lifted_r < 0.0) lifted_r = 0.0;
    alwan_scalar gamma_r = gamma->r;
    if (gamma_r <= 0.0001) gamma_r = 0.0001;
    alwan_scalar gamma_corrected_r = ALWAN_POW(lifted_r, 1.0 / gamma_r);
    rgb_out->r = gamma_corrected_r * gain->r;

    /* Process G channel */
    alwan_scalar lifted_g = rgb_in->g + lift->g;
    if (lifted_g < 0.0) lifted_g = 0.0;
    alwan_scalar gamma_g = gamma->g;
    if (gamma_g <= 0.0001) gamma_g = 0.0001;
    alwan_scalar gamma_corrected_g = ALWAN_POW(lifted_g, 1.0 / gamma_g);
    rgb_out->g = gamma_corrected_g * gain->g;

    /* Process B channel */
    alwan_scalar lifted_b = rgb_in->b + lift->b;
    if (lifted_b < 0.0) lifted_b = 0.0;
    alwan_scalar gamma_b = gamma->b;
    if (gamma_b <= 0.0001) gamma_b = 0.0001;
    alwan_scalar gamma_corrected_b = ALWAN_POW(lifted_b, 1.0 / gamma_b);
    rgb_out->b = gamma_corrected_b * gain->b;

    return ALWAN_OK;
}

/* ================================================================
 * Color Matrix Grading
 * ================================================================ */

int alwan_color_matrix_apply(alwan_rgb const *rgb_in, alwan_mat3x3 const *matrix_3x3,
                              alwan_rgb *rgb_out)
{
    if (!rgb_in || !matrix_3x3 || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* Apply 3x3 matrix transformation
     * Result = Matrix * RGB (treating RGB as column vector) */
    rgb_out->r = matrix_3x3->m[0] * rgb_in->r + matrix_3x3->m[1] * rgb_in->g + matrix_3x3->m[2] * rgb_in->b;
    rgb_out->g = matrix_3x3->m[3] * rgb_in->r + matrix_3x3->m[4] * rgb_in->g + matrix_3x3->m[5] * rgb_in->b;
    rgb_out->b = matrix_3x3->m[6] * rgb_in->r + matrix_3x3->m[7] * rgb_in->g + matrix_3x3->m[8] * rgb_in->b;

    return ALWAN_OK;
}

int alwan_color_matrix_get_preset(alwan_color_matrix_preset preset, alwan_mat3x3 *matrix_3x3)
{
    if (!matrix_3x3) {
        return ALWAN_E_INVALID;
    }

    /* Define preset matrices for common color grading looks */
    switch (preset) {
        case ALWAN_COLOR_MATRIX_SEPIA:
            /* Sepia tone: warm brown/yellow tint */
            matrix_3x3->m[0] = 0.393; matrix_3x3->m[1] = 0.769; matrix_3x3->m[2] = 0.189;
            matrix_3x3->m[3] = 0.349; matrix_3x3->m[4] = 0.686; matrix_3x3->m[5] = 0.168;
            matrix_3x3->m[6] = 0.272; matrix_3x3->m[7] = 0.534; matrix_3x3->m[8] = 0.131;
            break;

        case ALWAN_COLOR_MATRIX_VINTAGE:
            /* Vintage: reduced saturation, warm shift */
            matrix_3x3->m[0] = 0.9; matrix_3x3->m[1] = 0.1; matrix_3x3->m[2] = 0.1;
            matrix_3x3->m[3] = 0.1; matrix_3x3->m[4] = 0.8; matrix_3x3->m[5] = 0.0;
            matrix_3x3->m[6] = 0.0; matrix_3x3->m[7] = 0.1; matrix_3x3->m[8] = 0.7;
            break;

        case ALWAN_COLOR_MATRIX_BLEACH_BYPASS:
            /* Bleach bypass: high contrast, reduced saturation */
            matrix_3x3->m[0] = 1.2; matrix_3x3->m[1] = 0.2; matrix_3x3->m[2] = 0.0;
            matrix_3x3->m[3] = 0.1; matrix_3x3->m[4] = 1.1; matrix_3x3->m[5] = 0.1;
            matrix_3x3->m[6] = 0.0; matrix_3x3->m[7] = 0.2; matrix_3x3->m[8] = 1.0;
            break;

        case ALWAN_COLOR_MATRIX_COOL:
            /* Cool tone: blue shift */
            matrix_3x3->m[0] = 0.8; matrix_3x3->m[1] = 0.0; matrix_3x3->m[2] = 0.2;
            matrix_3x3->m[3] = 0.0; matrix_3x3->m[4] = 0.9; matrix_3x3->m[5] = 0.1;
            matrix_3x3->m[6] = 0.0; matrix_3x3->m[7] = 0.0; matrix_3x3->m[8] = 1.2;
            break;

        case ALWAN_COLOR_MATRIX_WARM:
            /* Warm tone: red/yellow shift */
            matrix_3x3->m[0] = 1.2; matrix_3x3->m[1] = 0.1; matrix_3x3->m[2] = 0.0;
            matrix_3x3->m[3] = 0.1; matrix_3x3->m[4] = 1.1; matrix_3x3->m[5] = 0.0;
            matrix_3x3->m[6] = 0.0; matrix_3x3->m[7] = 0.1; matrix_3x3->m[8] = 0.7;
            break;

        case ALWAN_COLOR_MATRIX_MONOCHROME:
            /* Black and white: luminance weights */
            matrix_3x3->m[0] = 0.299; matrix_3x3->m[1] = 0.587; matrix_3x3->m[2] = 0.114;
            matrix_3x3->m[3] = 0.299; matrix_3x3->m[4] = 0.587; matrix_3x3->m[5] = 0.114;
            matrix_3x3->m[6] = 0.299; matrix_3x3->m[7] = 0.587; matrix_3x3->m[8] = 0.114;
            break;

        case ALWAN_COLOR_MATRIX_NIGHT_VISION:
            /* Night vision: green monochrome */
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

int alwan_printer_lights_apply(alwan_rgb const *rgb_in, alwan_scalar red_lights,
                                alwan_scalar green_lights, alwan_scalar blue_lights,
                                alwan_rgb *rgb_out)
{
    if (!rgb_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* Printer lights formula: Each light unit represents ~0.025 log exposure change
     * Default is 25 lights (neutral)
     * Formula: output = input * 10^((25 - lights) * 0.025)
     *
     * This mimics film printing where changing printer light values
     * adjusts the exposure logarithmically */

    const alwan_scalar default_lights = 25.0;
    const alwan_scalar log_step = 0.025;  /* Log exposure per light unit */

    /* Compute exposure multipliers for each channel */
    alwan_scalar red_exposure = (default_lights - red_lights) * log_step;
    alwan_scalar green_exposure = (default_lights - green_lights) * log_step;
    alwan_scalar blue_exposure = (default_lights - blue_lights) * log_step;

    /* Apply exposure change: multiply by 10^exposure
     * 10^x = e^(x * ln(10)) */
    const alwan_scalar ln10 = 2.302585092994046;  /* ln(10) */

    rgb_out->r = rgb_in->r * ALWAN_EXP(red_exposure * ln10);
    rgb_out->g = rgb_in->g * ALWAN_EXP(green_exposure * ln10);
    rgb_out->b = rgb_in->b * ALWAN_EXP(blue_exposure * ln10);

    return ALWAN_OK;
}
