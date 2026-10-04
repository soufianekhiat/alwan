/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2026 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Internal declarations shared by alwan's image-processing files: the edge-aware filter
 * workers, segmentation, warping and resizing, the OpenCV ports and their DFT / DCT, the
 * DFT, local contrast, histogram matching and the quantisers. They are image processing,
 * bound for suwar, and are kept in this one header so they move as a unit. Included by
 * alwan_internal.h; nothing outside alwan reads it.
 */

#ifndef ALWAN_INTERNAL_IMAGE_H
#define ALWAN_INTERNAL_IMAGE_H

#include "alwan.h"
/* alwan_quantize_octree.c: ALWAN_QUANTIZE_FAST_OCTREE of alwan_quantize_u8 */
alwan_status alwan__quantize_octree(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                                    unsigned char const *rgb, size_t pixel_stride, size_t count, size_t max_colors);
/* alwan_quantize_max_coverage.c: ALWAN_QUANTIZE_MAX_COVERAGE of alwan_quantize_u8 */
alwan_status alwan__quantize_max_coverage(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                                          unsigned char const *rgb, size_t pixel_stride, size_t count, size_t max_colors);
/* alwan_quantize_ext.c: ALWAN_QUANTIZE_KMEANS, _WU and _OCTREE_CLASSIC of alwan_quantize_ex_u8 */
alwan_status alwan__quantize_ext(unsigned char *palette_out, size_t *count_out, unsigned int *index_out,
                                 unsigned char const *rgb, size_t pixel_stride, size_t count, size_t max_colors,
                                 alwan_quantize_method method, alwan_quantize_params const *params);


#endif /* ALWAN_INTERNAL_IMAGE_H */
