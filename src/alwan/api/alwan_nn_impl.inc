/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Neural layers over whole tensors (parameterized precision).
 * No include guard - included twice via api setup/teardown.
 *
 * Each function is the core's one-output-element kernel in a loop over the
 * output, so it agrees with a per-element call to the bit, and a GPU that
 * binds the same kernel per thread agrees with both. Arguments are validated
 * once. Shapes are the caller's: the output buffer must hold what the header
 * says the layer produces.
 */

alwan_status ALWAN_CORE_FNLIT(alwan_nn_dense)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, ALWAN_CORE_T const *w,
                                              ALWAN_CORE_T const *b, int n_in, int n_out) {
    int o;
    if (!out || !in || !w || n_in <= 0 || n_out <= 0) return ALWAN_E_INVALID;
    for (o = 0; o < n_out; o++) {
        out[o] = ALWAN_CORE_FNVLIT(alwan_nn_dense_at)(in, w, b, n_in, n_out, b != NULL, o);
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_nn_conv2d)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, int H, int W, int Cin,
                                               ALWAN_CORE_T const *w, ALWAN_CORE_T const *b, int KH, int KW, int Cout,
                                               int stride, int pad, int groups) {
    int Ho, Wo, y, x, o;
    if (!out || !in || !w || H <= 0 || W <= 0 || Cin <= 0 || KH <= 0 || KW <= 0 || Cout <= 0 ||
        stride <= 0 || pad < 0 || groups <= 0) {
        return ALWAN_E_INVALID;
    }
    if (Cin % groups != 0 || Cout % groups != 0) return ALWAN_E_INVALID;
    Ho = (H + 2 * pad - KH) / stride + 1;
    Wo = (W + 2 * pad - KW) / stride + 1;
    if (H + 2 * pad < KH || W + 2 * pad < KW) return ALWAN_E_RANGE;
    for (y = 0; y < Ho; y++) {
        for (x = 0; x < Wo; x++) {
            for (o = 0; o < Cout; o++) {
                out[(y * Wo + x) * Cout + o] = ALWAN_CORE_FNVLIT(alwan_nn_conv2d_at)(in, w, b, H, W, Cin, KH, KW, Cout,
                                                                                    stride, pad, groups, b != NULL, y, x, o);
            }
        }
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_nn_activation)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, size_t count,
                                                   alwan_nn_activation_kind kind, ALWAN_CORE_T alpha) {
    size_t i;
    if (!out || !in) return ALWAN_E_INVALID;
    if ((int)kind < ALWAN_NN_ACT_RELU || (int)kind > ALWAN_NN_ACT_GELU) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) {
        out[i] = ALWAN_CORE_FNVLIT(alwan_nn_activation)(in[i], (int)kind, alpha);
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_nn_pool2d)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, int H, int W, int C,
                                               int k, int stride, int pad, alwan_nn_pool_kind kind) {
    int Ho, Wo, y, x, c;
    if (!out || !in || H <= 0 || W <= 0 || C <= 0 || k <= 0 || stride <= 0 || pad < 0) return ALWAN_E_INVALID;
    if (kind != ALWAN_NN_POOL_MAX && kind != ALWAN_NN_POOL_AVG) return ALWAN_E_INVALID;
    if (pad > k / 2) return ALWAN_E_RANGE;   /* PyTorch's own rule: padding at most half the window */
    Ho = (H + 2 * pad - k) / stride + 1;
    Wo = (W + 2 * pad - k) / stride + 1;
    if (H + 2 * pad < k || W + 2 * pad < k) return ALWAN_E_RANGE;
    for (y = 0; y < Ho; y++) {
        for (x = 0; x < Wo; x++) {
            for (c = 0; c < C; c++) {
                out[(y * Wo + x) * C + c] = ALWAN_CORE_FNVLIT(alwan_nn_pool2d_at)(in, H, W, C, k, stride, pad,
                                                                                 kind == ALWAN_NN_POOL_MAX ? 0 : 1, y, x, c);
            }
        }
    }
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_nn_global_avg)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, int H, int W, int C) {
    int c;
    if (!out || !in || H <= 0 || W <= 0 || C <= 0) return ALWAN_E_INVALID;
    for (c = 0; c < C; c++) out[c] = ALWAN_CORE_FNVLIT(alwan_nn_global_avg_at)(in, H, W, C, c);
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_nn_upsample2d)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, int H, int W, int C,
                                                   int scale, alwan_nn_upsample_kind kind) {
    int y, x, c;
    if (!out || !in || H <= 0 || W <= 0 || C <= 0 || scale <= 0) return ALWAN_E_INVALID;
    if (kind != ALWAN_NN_UPSAMPLE_NEAREST && kind != ALWAN_NN_UPSAMPLE_BILINEAR) return ALWAN_E_INVALID;
    for (y = 0; y < H * scale; y++) {
        for (x = 0; x < W * scale; x++) {
            for (c = 0; c < C; c++) {
                out[(y * W * scale + x) * C + c] = ALWAN_CORE_FNVLIT(alwan_nn_upsample2d_at)(in, H, W, C, scale,
                                                                                            kind == ALWAN_NN_UPSAMPLE_NEAREST ? 0 : 1, y, x, c);
            }
        }
    }
    return ALWAN_OK;
}

/* The max and the sum once for the vector, then each element, in the same
 * arithmetic as the per-element kernel so the two agree to the bit. */
alwan_status ALWAN_CORE_FNLIT(alwan_nn_softmax)(ALWAN_CORE_T *out, ALWAN_CORE_T const *in, int n) {
    ALWAN_CORE_T m;
    ALWAN_DET_PRECISE ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
    int i;
    if (!out || !in || n <= 0) return ALWAN_E_INVALID;
    m = in[0];
    for (i = 1; i < n; i++) m = ALWAN_CORE_SELECT(in[i] > m, in[i], m);
    for (i = 0; i < n; i++) sum = sum + ALWAN_CORE_EXP(in[i] - m);
    for (i = 0; i < n; i++) out[i] = ALWAN_CORE_EXP(in[i] - m) / sum;
    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_nn_add)(ALWAN_CORE_T *out, ALWAN_CORE_T const *a, ALWAN_CORE_T const *b, size_t count) {
    size_t i;
    if (!out || !a || !b) return ALWAN_E_INVALID;
    for (i = 0; i < count; i++) out[i] = a[i] + b[i];
    return ALWAN_OK;
}

/* Channel concatenation of two channels-last images of one H x W. */
alwan_status ALWAN_CORE_FNLIT(alwan_nn_concat_channels)(ALWAN_CORE_T *out, ALWAN_CORE_T const *a, int Ca,
                                                        ALWAN_CORE_T const *b, int Cb, int H, int W) {
    int p, c;
    if (!out || !a || !b || Ca <= 0 || Cb <= 0 || H <= 0 || W <= 0) return ALWAN_E_INVALID;
    for (p = 0; p < H * W; p++) {
        for (c = 0; c < Ca; c++) out[p * (Ca + Cb) + c] = a[p * Ca + c];
        for (c = 0; c < Cb; c++) out[p * (Ca + Cb) + Ca + c] = b[p * Cb + c];
    }
    return ALWAN_OK;
}
