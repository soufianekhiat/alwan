/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Planar twins that run their interleave form on a packed tile.
 *
 * The operations here have an interleaved buffer form whose per-pixel body is
 * either written inline in the loop (BT.2408, PU21, the film look stages, the
 * camera IDT, the DNG highlight blend, CLF) or a SIMD kernel (AgX, JP2499, the
 * view transform, the matrix transform). There is no scalar function a planar
 * loop could call and be bit-identical to. So each twin gathers a tile of
 * pixels out of the three planes into packed triples, runs the interleave
 * form on the tile, and scatters the result back. That is identical to the
 * interleave form by construction, kernels included, and suite 175 asserts it
 * per pixel over a plane stride that is not the interleave stride.
 *
 * The tile is 256 pixels, 12 KiB of stack in f64 for both buffers. Per-buffer
 * setup an interleave form does once (Zhai 2018's gains, BT.2408's system
 * gamma, the film look's tables) is redone per tile; it is deterministic, so
 * the result does not move, and it is small against 256 pixels of work.
 *
 * A count of zero is handed to the interleave form as a count of zero, so
 * the verdict on an empty buffer is the interleave form's own: most say
 * ALWAN_OK, CLF says ALWAN_E_INVALID, and the twins do not disagree.
 *
 * Generated once from the interleave declarations in alwan.h, then kept by
 * hand; the argument lists after count are the interleave form's, in its
 * order.
 */

#include "../alwan.h"
#include "../alwan_internal.h"

#define ALWAN_PLANAR_TILE 256

#define ALWAN_PLANAR_GATHER(T, tile, p0, p1, p2, stride, from, n)                                       \
    do {                                                                                                 \
        size_t k_;                                                                                       \
        for (k_ = 0; k_ < (n); k_++) {                                                                   \
            (tile)[k_ * 3 + 0] = *(T const *)((char const *)(p0) + ((from) + k_) * (stride));            \
            (tile)[k_ * 3 + 1] = *(T const *)((char const *)(p1) + ((from) + k_) * (stride));            \
            (tile)[k_ * 3 + 2] = *(T const *)((char const *)(p2) + ((from) + k_) * (stride));            \
        }                                                                                                \
    } while (0)

#define ALWAN_PLANAR_SCATTER(T, tile, p0, p1, p2, stride, from, n)                                      \
    do {                                                                                                 \
        size_t k_;                                                                                       \
        for (k_ = 0; k_ < (n); k_++) {                                                                   \
            *(T *)((char *)(p0) + ((from) + k_) * (stride)) = (tile)[k_ * 3 + 0];                        \
            *(T *)((char *)(p1) + ((from) + k_) * (stride)) = (tile)[k_ * 3 + 1];                        \
            *(T *)((char *)(p2) + ((from) + k_) * (stride)) = (tile)[k_ * 3 + 2];                        \
        }                                                                                                \
    } while (0)

#if ALWAN_WITH_F32

alwan_status alwan_aces1_output_transform_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_aces1_output output, alwan_aces_interp interp) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_aces1_output_transform_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, output, interp);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_aces1_output_transform_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, output, interp);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_aces2_output_transform_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_aces2_output output) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_aces2_output_transform_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, output);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_aces2_output_transform_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, output);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_aces2_output_transform_inv_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_aces2_output output) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_aces2_output_transform_inv_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, output);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_aces2_output_transform_inv_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, output);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_camera_rgb_to_aces2065_1_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_mat3x3_f32 const *idt, alwan_rgb_f32 const *white_balance, alwan_f32 exposure, int clip) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_camera_rgb_to_aces2065_1_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, idt, white_balance, exposure, clip);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_camera_rgb_to_aces2065_1_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, idt, white_balance, exposure, clip);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_cat_zhai2018_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_xyz_f32 const *xyz_src, alwan_xyz_f32 const *xyz_dst, alwan_f32 D_src, alwan_f32 D_dst, alwan_xyz_f32 const *xyz_baseline, alwan_cat_method transform) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_cat_zhai2018_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, xyz_src, xyz_dst, D_src, D_dst, xyz_baseline, transform);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_cat_zhai2018_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, xyz_src, xyz_dst, D_src, D_dst, xyz_baseline, transform);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_clf_apply_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_clf const *clf) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_clf_apply_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, clf);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_clf_apply_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, clf);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_highlights_recovery_blend_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_rgb_f32 const *multipliers, alwan_f32 threshold) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_highlights_recovery_blend_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, multipliers, threshold);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_highlights_recovery_blend_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, multipliers, threshold);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_bt2408_hlg_to_pq_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_f32 hlg_peak_nits) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_bt2408_hlg_to_pq_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, hlg_peak_nits);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_bt2408_hlg_to_pq_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, hlg_peak_nits);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_bt2408_pq_to_hlg_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_f32 hlg_peak_nits, int clip_to_peak) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_bt2408_pq_to_hlg_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, hlg_peak_nits, clip_to_peak);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_bt2408_pq_to_hlg_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, hlg_peak_nits, clip_to_peak);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_bt2408_sdr_to_hlg_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_f32 sdr_white_nits, alwan_f32 hlg_peak_nits) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_bt2408_sdr_to_hlg_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, sdr_white_nits, hlg_peak_nits);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_bt2408_sdr_to_hlg_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, sdr_white_nits, hlg_peak_nits);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_bt2408_sdr_to_pq_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_f32 sdr_white_nits) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_bt2408_sdr_to_pq_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, sdr_white_nits);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_bt2408_sdr_to_pq_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, sdr_white_nits);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_pu21_encode_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_pu21_variant variant) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_pu21_encode_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, variant);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_pu21_encode_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, variant);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_pu21_decode_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_pu21_variant variant) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_pu21_decode_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, variant);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_pu21_decode_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, variant);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_film_look_develop_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_film_look const *look, alwan_ctx *ctx) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_film_look_develop_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, look, ctx);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_film_look_develop_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, look, ctx);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_film_look_expose_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_film_look const *look, alwan_ctx *ctx) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_film_look_expose_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, look, ctx);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_film_look_expose_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, look, ctx);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_film_look_finish_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_film_look const *look, alwan_ctx *ctx) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_film_look_finish_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, look, ctx);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_film_look_finish_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, look, ctx);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_film_look_print_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_film_look const *look, alwan_ctx *ctx) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_film_look_print_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, look, ctx);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_film_look_print_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, look, ctx);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_film_render_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_film_look const *look, alwan_ctx *ctx) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_film_render_rgb_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, look, ctx);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_film_render_rgb_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, look, ctx);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_agx_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_agx_params_f32 const *params) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_agx_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, params);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_agx_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, params);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_jp2499_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_jp2499_params_f32 const *params) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_jp2499_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, params);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_jp2499_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, params);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_view_transform_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_view_transform vt, alwan_ctx *ctx) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_view_transform_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, vt, ctx);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_view_transform_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, vt, ctx);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_mat3_transform_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2,
        alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count,
        alwan_mat3x3_f32 const *matrix) {
    alwan_f32 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_mat3_transform_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), 0, matrix);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f32, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_mat3_transform_f32_map_interleave(tout, 3 * sizeof(alwan_f32), tin, 3 * sizeof(alwan_f32), n, matrix);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f32, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

#endif /* ALWAN_WITH_F32 */

#if ALWAN_WITH_F64

alwan_status alwan_aces1_output_transform_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_aces1_output output, alwan_aces_interp interp) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_aces1_output_transform_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, output, interp);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_aces1_output_transform_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, output, interp);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_aces2_output_transform_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_aces2_output output) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_aces2_output_transform_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, output);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_aces2_output_transform_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, output);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_aces2_output_transform_inv_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_aces2_output output) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_aces2_output_transform_inv_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, output);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_aces2_output_transform_inv_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, output);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_camera_rgb_to_aces2065_1_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_mat3x3_f64 const *idt, alwan_rgb_f64 const *white_balance, alwan_f64 exposure, int clip) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_camera_rgb_to_aces2065_1_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, idt, white_balance, exposure, clip);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_camera_rgb_to_aces2065_1_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, idt, white_balance, exposure, clip);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_cat_zhai2018_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_xyz_f64 const *xyz_src, alwan_xyz_f64 const *xyz_dst, alwan_f64 D_src, alwan_f64 D_dst, alwan_xyz_f64 const *xyz_baseline, alwan_cat_method transform) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_cat_zhai2018_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, xyz_src, xyz_dst, D_src, D_dst, xyz_baseline, transform);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_cat_zhai2018_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, xyz_src, xyz_dst, D_src, D_dst, xyz_baseline, transform);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_clf_apply_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_clf const *clf) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_clf_apply_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, clf);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_clf_apply_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, clf);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_highlights_recovery_blend_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_rgb_f64 const *multipliers, alwan_f64 threshold) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_highlights_recovery_blend_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, multipliers, threshold);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_highlights_recovery_blend_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, multipliers, threshold);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_bt2408_hlg_to_pq_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_f64 hlg_peak_nits) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_bt2408_hlg_to_pq_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, hlg_peak_nits);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_bt2408_hlg_to_pq_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, hlg_peak_nits);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_bt2408_pq_to_hlg_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_f64 hlg_peak_nits, int clip_to_peak) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_bt2408_pq_to_hlg_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, hlg_peak_nits, clip_to_peak);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_bt2408_pq_to_hlg_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, hlg_peak_nits, clip_to_peak);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_bt2408_sdr_to_hlg_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_f64 sdr_white_nits, alwan_f64 hlg_peak_nits) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_bt2408_sdr_to_hlg_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, sdr_white_nits, hlg_peak_nits);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_bt2408_sdr_to_hlg_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, sdr_white_nits, hlg_peak_nits);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_bt2408_sdr_to_pq_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_f64 sdr_white_nits) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_bt2408_sdr_to_pq_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, sdr_white_nits);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_bt2408_sdr_to_pq_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, sdr_white_nits);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_pu21_encode_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_pu21_variant variant) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_pu21_encode_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, variant);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_pu21_encode_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, variant);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_pu21_decode_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_pu21_variant variant) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_pu21_decode_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, variant);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_pu21_decode_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, variant);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_film_look_develop_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_film_look const *look, alwan_ctx *ctx) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_film_look_develop_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, look, ctx);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_film_look_develop_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, look, ctx);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_film_look_expose_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_film_look const *look, alwan_ctx *ctx) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_film_look_expose_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, look, ctx);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_film_look_expose_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, look, ctx);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_film_look_finish_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_film_look const *look, alwan_ctx *ctx) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_film_look_finish_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, look, ctx);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_film_look_finish_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, look, ctx);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_film_look_print_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_film_look const *look, alwan_ctx *ctx) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_film_look_print_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, look, ctx);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_film_look_print_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, look, ctx);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_film_render_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_film_look const *look, alwan_ctx *ctx) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_film_render_rgb_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, look, ctx);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_film_render_rgb_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, look, ctx);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_agx_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_agx_params_f64 const *params) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_agx_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, params);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_agx_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, params);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_jp2499_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_jp2499_params_f64 const *params) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_jp2499_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, params);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_jp2499_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, params);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_view_transform_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_view_transform vt, alwan_ctx *ctx) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_view_transform_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, vt, ctx);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_view_transform_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, vt, ctx);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_mat3_transform_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2,
        alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count,
        alwan_mat3x3_f64 const *matrix) {
    alwan_f64 tin[ALWAN_PLANAR_TILE * 3], tout[ALWAN_PLANAR_TILE * 3];
    size_t done = 0;
    alwan_status st;
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;
    if (count == 0) return alwan_mat3_transform_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), 0, matrix);
    while (done < count) {
        size_t n = count - done;
        if (n > ALWAN_PLANAR_TILE) n = ALWAN_PLANAR_TILE;
        ALWAN_PLANAR_GATHER(alwan_f64, tin, in_ch0, in_ch1, in_ch2, in_stride, done, n);
        st = alwan_mat3_transform_f64_map_interleave(tout, 3 * sizeof(alwan_f64), tin, 3 * sizeof(alwan_f64), n, matrix);
        if (st != ALWAN_OK) return st;
        ALWAN_PLANAR_SCATTER(alwan_f64, tout, out_ch0, out_ch1, out_ch2, out_stride, done, n);
        done += n;
    }
    return ALWAN_OK;
}

#endif /* ALWAN_WITH_F64 */

