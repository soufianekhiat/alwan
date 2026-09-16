/* No include guard -- included twice (f32 and f64 passes) */

/*
 * PSNR per channel and CPSNR, with a border crop.
 *
 * 10 log10(data_range^2 / MSE): per channel from that channel's own MSE, and pooled
 * from ONE MSE taken over every channel. The pooled figure is what scikit-image's
 * peak_signal_noise_ratio returns for a multi-channel image and what the demosaicing
 * literature calls CPSNR. It is not the mean of the per-channel decibels; the log of an
 * average and the average of logs are different numbers, and the difference is small
 * enough to look like rounding, which is exactly why the suite asserts against it.
 *
 * The border is dropped before anything is accumulated rather than after. A demosaicked
 * image is least reliable at its edges, every paper crops before comparing, and a figure
 * computed over the whole frame is not comparable with the ones they publish.
 *
 * Accumulates in double in both precisions, as SSIM does. A four megapixel frame sums
 * millions of squared differences and an f32 accumulator loses the small ones long
 * before the sum is finished.
 */

#ifndef ALWAN__PSNR_ONCE
#define ALWAN__PSNR_ONCE

#define ALWAN__PSNR_MAX_CHANNELS 4

/* +inf for an exact match, built through volatiles the way alwan__ccm_nan builds its
 * NaN, so fast math cannot fold the division away. */
static double alwan__psnr_inf(void) {
    volatile double one = 1.0, zero = 0.0;
    return one / zero;
}

static double alwan__psnr_db(double sse, double count, double dr) {
    double const mse = sse / count;
    if (!(mse > 0.0)) return alwan__psnr_inf();
    return 10.0 * ALWAN_LOG10_F64(dr * dr / mse);
}

#endif /* ALWAN__PSNR_ONCE */

alwan_status ALWAN_CORE_FNLIT(alwan_psnr)(ALWAN_CORE_T *psnr_out, ALWAN_CORE_T *cpsnr_out,
                                          ALWAN_CORE_T const *test, size_t test_row_stride,
                                          ALWAN_CORE_T const *ref, size_t ref_row_stride,
                                          size_t width, size_t height, size_t channels,
                                          size_t border, ALWAN_CORE_T data_range)
{
    double sse[ALWAN__PSNR_MAX_CHANNELS];
    double const dr = (double)data_range;
    double pooled = 0.0, count;
    size_t x, y, c;

    if (!test || !ref) return ALWAN_E_INVALID;
    if (!psnr_out && !cpsnr_out) return ALWAN_E_INVALID;
    if (channels < 1 || channels > (size_t)ALWAN__PSNR_MAX_CHANNELS) return ALWAN_E_INVALID;
    if (!(dr > 0.0) || dr - dr != 0.0) return ALWAN_E_INVALID;
    if (width == 0 || height == 0) return ALWAN_E_INVALID;
    if (2 * border >= width || 2 * border >= height) return ALWAN_E_INVALID;

    for (c = 0; c < channels; c++) sse[c] = 0.0;

    for (y = border; y < height - border; y++) {
        ALWAN_CORE_T const *const tr =
            (ALWAN_CORE_T const *)(void const *)((unsigned char const *)test + y * test_row_stride);
        ALWAN_CORE_T const *const rr =
            (ALWAN_CORE_T const *)(void const *)((unsigned char const *)ref + y * ref_row_stride);
        for (x = border; x < width - border; x++) {
            for (c = 0; c < channels; c++) {
                double const d = (double)tr[x * channels + c] - (double)rr[x * channels + c];
                sse[c] += d * d;
            }
        }
    }

    count = (double)((width - 2 * border) * (height - 2 * border));
    for (c = 0; c < channels; c++) pooled += sse[c];

    if (psnr_out) {
        for (c = 0; c < channels; c++) {
            psnr_out[c] = (ALWAN_CORE_T)alwan__psnr_db(sse[c], count, dr);
        }
    }
    if (cpsnr_out) {
        *cpsnr_out = (ALWAN_CORE_T)alwan__psnr_db(pooled, count * (double)channels, dr);
    }
    return ALWAN_OK;
}
