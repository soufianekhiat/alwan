/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Neural layer kernels, written against an accessor instead of a pointer.
 * No include guard - included once per binding.
 */

/* ================================================================
 * WHAT ONE OUTPUT ELEMENT IS
 *
 * Every function here computes ONE element of a layer's output, by value,
 * from tensor elements it reaches through accessors. The arithmetic is the
 * same on every backend; how an element is reached is not, and HLSL and GLSL
 * have no pointer to pass. So, exactly as alwan_table_reader.inc does for
 * tables, nothing here names a pointer. The kernels read through
 *
 *     ALWAN_NN_READ_IN(i)    the input tensor
 *     ALWAN_NN_READ_W(i)     the weights
 *     ALWAN_NN_READ_B(i)     the bias
 *
 * and whoever includes this file says what those mean.
 *
 * POINTER BINDING. alwan_nn_core.inc defines ALWAN_NN_READER_POINTER and
 * includes this file on every backend that has pointers. That emits the
 * functions the API tier loops over, with the tensors as leading parameters.
 *
 * NAMED BINDING. A shader defines a name and the accessors and includes the
 * file, once per set of tensors:
 *
 *     StructuredBuffer<float> Act  : register(t0);
 *     StructuredBuffer<float> W1   : register(t1);
 *     StructuredBuffer<float> B1   : register(t2);
 *
 *     #define ALWAN_NN_NAME      layer1
 *     #define ALWAN_NN_READ_IN(i) Act[i]
 *     #define ALWAN_NN_READ_W(i)  W1[i]
 *     #define ALWAN_NN_READ_B(i)  B1[i]
 *     #include "core/alwan_nn_reader.inc"
 *
 *     float o = alwan_nn_conv2d_at_layer1(H, W, Cin, 3, 3, Cout, 1, 1, 1, 1, y, x, c);
 *
 * LAYOUT. Tensors are channels-last, C order: an image element is
 * in[(y * W + x) * C + c]. Convolution weights are HWIO,
 * w[((ky * KW + kx) * Cg + ci) * Cout + o] with Cg = Cin / groups the channels
 * one group sees; a dense weight is w[i * n_out + o]. PyTorch stores NCHW and
 * OIHW; gendata permutes when it writes weights, and the reference tests do
 * the same, so a model converted for alwan carries its weights already in this
 * order and no transpose happens at run time.
 *
 * SUMMATION ORDER IS PART OF THE CONTRACT. Every accumulate runs in the same
 * fixed order (ky, kx, ci for a convolution; i for a dense layer), into a
 * local declared ALWAN_DET_PRECISE, and touches no intrinsic that hides an
 * expression, so a deterministic build is bit-exact across backends: the
 * reason this code exists rather than a call into a vendor library that picks
 * an algorithm at run time. Nothing here uses a fused multiply-add on purpose;
 * the CUDA build needs --fmad=false for the same reason the C build needs
 * -ffp-contract=off.
 *
 * Zero padding on a convolution and a pool is an out-of-range test on the
 * index, not a read of a padded copy: an element outside the image contributes
 * nothing (a convolution, an average pool) or is skipped (a max pool), and the
 * accessor is never evaluated for it.
 * ================================================================ */

/* Integer division on a shading language is signed and slow, and fxc says so
 * with a warning on every one, which a consumer's warnings-as-errors build
 * would turn into a failure. Every quotient here has non-negative operands,
 * so it is taken unsigned there and plain in C, and the two agree exactly.
 * The loop attribute keeps fxc from unrolling a channel loop whose count it
 * can see is one (a depthwise kernel), which it otherwise warns about. */
#if ALWAN_BACKEND == ALWAN_BACKEND_HLSL
# define ALWAN_NN_DIV(a, b)  ((int)((uint)(a) / (uint)(b)))
# define ALWAN_NN_LOOP       [loop]
/* fxc warns (X3557) that a loop runs once when a caller's constants make it
 * so, as a depthwise kernel's channel loop does. That is the caller's shape,
 * not a defect in the template, and a consumer building with warnings as
 * errors should not fail on it. dxc does not warn and does not take this
 * pragma, so it is fxc's alone. */
# if !defined(__DXC_VERSION_MAJOR)
#  pragma warning(disable : 3557)
# endif
#elif ALWAN_BACKEND == ALWAN_BACKEND_GLSL
# define ALWAN_NN_DIV(a, b)  ((int)((uint)(a) / (uint)(b)))
# define ALWAN_NN_LOOP
#else
# define ALWAN_NN_DIV(a, b)  ((a) / (b))
# define ALWAN_NN_LOOP
#endif

#if defined(ALWAN_NN_READER_POINTER)

# define ALWAN_NN_FN_(base)        ALWAN_CORE_FNVLIT(base)
# define ALWAN_NN_PARAMS_          const ALWAN_CORE_T *in, const ALWAN_CORE_T *w, const ALWAN_CORE_T *b,
# define ALWAN_NN_IN_PARAMS_       const ALWAN_CORE_T *in,
# define ALWAN_NN_READ_IN(i)       in[i]
# define ALWAN_NN_READ_W(i)        w[i]
# define ALWAN_NN_READ_B(i)        b[i]

#else

# if !defined(ALWAN_NN_NAME) || !defined(ALWAN_NN_READ_IN)
#  error "alwan_nn_reader.inc: define ALWAN_NN_NAME and the ALWAN_NN_READ_* accessors before including it"
# endif
# if !defined(ALWAN_NN_READ_W)
#  define ALWAN_NN_READ_W(i) ALWAN_NN_READ_IN(i)
# endif
# if !defined(ALWAN_NN_READ_B)
#  define ALWAN_NN_READ_B(i) ALWAN_NN_READ_IN(i)
# endif
# define ALWAN_NN_PASTE2_(a, b)    a##_##b
# define ALWAN_NN_PASTE_(a, b)     ALWAN_NN_PASTE2_(a, b)
# define ALWAN_NN_FN_(base)        ALWAN_NN_PASTE_(base, ALWAN_NN_NAME)
# define ALWAN_NN_PARAMS_
# define ALWAN_NN_IN_PARAMS_

#endif

/* Dense: out[o] = sum_i in[i] w[i * n_out + o] + b[o]. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_NN_FN_(alwan_nn_dense_at)(ALWAN_NN_PARAMS_ int n_in, int n_out, int has_bias, int o) {
    ALWAN_DET_PRECISE ALWAN_CORE_T acc = ALWAN_CORE_LITERAL(0.0);
    int i;
    ALWAN_NN_LOOP for (i = 0; i < n_in; i++) {
        acc = acc + ALWAN_NN_READ_IN(i) * ALWAN_NN_READ_W(i * n_out + o);
    }
    if (has_bias) acc = acc + ALWAN_NN_READ_B(o);
    return acc;
}

/* 2D convolution, cross-correlation as every framework means it: one output
 * element at (y, x, o) of an image H x W x Cin, kernel KH x KW, Cout outputs,
 * one stride and one zero padding for both axes, groups dividing both Cin and
 * Cout (groups == Cin == Cout is depthwise). The output is
 * ((H + 2 pad - KH) / stride + 1) x ((W + 2 pad - KW) / stride + 1) x Cout. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_NN_FN_(alwan_nn_conv2d_at)(ALWAN_NN_PARAMS_ int H, int W, int Cin, int KH, int KW, int Cout,
                                                            int stride, int pad, int groups, int has_bias,
                                                            int y, int x, int o) {
    ALWAN_DET_PRECISE ALWAN_CORE_T acc = ALWAN_CORE_LITERAL(0.0);
    const int Cg = ALWAN_NN_DIV(Cin, groups);
    const int g = ALWAN_NN_DIV(o, ALWAN_NN_DIV(Cout, groups));
    int ky, kx, ci;
    ALWAN_NN_LOOP for (ky = 0; ky < KH; ky++) {
        const int iy = y * stride - pad + ky;
        if (iy < 0 || iy >= H) continue;
        ALWAN_NN_LOOP for (kx = 0; kx < KW; kx++) {
            const int ix = x * stride - pad + kx;
            if (ix < 0 || ix >= W) continue;
            ALWAN_NN_LOOP for (ci = 0; ci < Cg; ci++) {
                acc = acc + ALWAN_NN_READ_IN((iy * W + ix) * Cin + g * Cg + ci)
                          * ALWAN_NN_READ_W(((ky * KW + kx) * Cg + ci) * Cout + o);
            }
        }
    }
    if (has_bias) acc = acc + ALWAN_NN_READ_B(o);
    return acc;
}

/* Max (mode 0) or average (mode 1) pooling over a k x k window at one stride
 * with zero padding. An average counts the padded positions in its divisor,
 * k * k, which is PyTorch's count_include_pad default; a max skips them, so a
 * window entirely in the padding, which the output size rules out, would be
 * the lowest value the type holds. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_NN_FN_(alwan_nn_pool2d_at)(ALWAN_NN_IN_PARAMS_ int H, int W, int C, int k, int stride, int pad,
                                                            int mode, int y, int x, int c) {
    ALWAN_DET_PRECISE ALWAN_CORE_T acc = ALWAN_CORE_LITERAL(0.0);
    int seen = 0;
    int ky, kx;
    ALWAN_NN_LOOP for (ky = 0; ky < k; ky++) {
        const int iy = y * stride - pad + ky;
        if (iy < 0 || iy >= H) continue;
        ALWAN_NN_LOOP for (kx = 0; kx < k; kx++) {
            const int ix = x * stride - pad + kx;
            ALWAN_CORE_T v;
            if (ix < 0 || ix >= W) continue;
            v = ALWAN_NN_READ_IN((iy * W + ix) * C + c);
            if (mode == 0) {
                acc = ALWAN_CORE_SELECT(seen == 0 || v > acc, v, acc);
            } else {
                acc = acc + v;
            }
            seen = 1;
        }
    }
    if (mode != 0) acc = acc / (ALWAN_CORE_T)(k * k);
    return acc;
}

/* Global average over the image, one channel. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_NN_FN_(alwan_nn_global_avg_at)(ALWAN_NN_IN_PARAMS_ int H, int W, int C, int c) {
    ALWAN_DET_PRECISE ALWAN_CORE_T acc = ALWAN_CORE_LITERAL(0.0);
    int p;
    ALWAN_NN_LOOP for (p = 0; p < H * W; p++) acc = acc + ALWAN_NN_READ_IN(p * C + c);
    return acc / (ALWAN_CORE_T)(H * W);
}

/* Upsampling by an integer factor: nearest (mode 0) takes the source element
 * at floor(dst / scale); bilinear (mode 1) is PyTorch's align_corners=False,
 * source coordinate (dst + 0.5) / scale - 0.5 clamped at zero, blended from
 * the two source rows and columns around it, the upper one clamped to the
 * last. The blend is written as v0 * (1 - f) + v1 * f, the spelling every
 * other blend in the library uses; not an intrinsic. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_NN_FN_(alwan_nn_upsample2d_at)(ALWAN_NN_IN_PARAMS_ int H, int W, int C, int scale, int mode,
                                                                int yo, int xo, int c) {
    if (mode == 0) {
        return ALWAN_NN_READ_IN((ALWAN_NN_DIV(yo, scale) * W + ALWAN_NN_DIV(xo, scale)) * C + c);
    } else {
        const ALWAN_CORE_T sy = ALWAN_CORE_SELECT(((ALWAN_CORE_T)yo + ALWAN_CORE_LITERAL(0.5)) / (ALWAN_CORE_T)scale - ALWAN_CORE_LITERAL(0.5) < ALWAN_CORE_LITERAL(0.0),
                                                  ALWAN_CORE_LITERAL(0.0),
                                                  ((ALWAN_CORE_T)yo + ALWAN_CORE_LITERAL(0.5)) / (ALWAN_CORE_T)scale - ALWAN_CORE_LITERAL(0.5));
        const ALWAN_CORE_T sx = ALWAN_CORE_SELECT(((ALWAN_CORE_T)xo + ALWAN_CORE_LITERAL(0.5)) / (ALWAN_CORE_T)scale - ALWAN_CORE_LITERAL(0.5) < ALWAN_CORE_LITERAL(0.0),
                                                  ALWAN_CORE_LITERAL(0.0),
                                                  ((ALWAN_CORE_T)xo + ALWAN_CORE_LITERAL(0.5)) / (ALWAN_CORE_T)scale - ALWAN_CORE_LITERAL(0.5));
        const int y0 = (int)sy, x0 = (int)sx;
        const int y1 = ALWAN_CORE_SELECT(y0 + 1 < H, y0 + 1, H - 1);
        const int x1 = ALWAN_CORE_SELECT(x0 + 1 < W, x0 + 1, W - 1);
        const ALWAN_CORE_T fy = sy - (ALWAN_CORE_T)y0, fx = sx - (ALWAN_CORE_T)x0;
        const ALWAN_CORE_T v00 = ALWAN_NN_READ_IN((y0 * W + x0) * C + c);
        const ALWAN_CORE_T v01 = ALWAN_NN_READ_IN((y0 * W + x1) * C + c);
        const ALWAN_CORE_T v10 = ALWAN_NN_READ_IN((y1 * W + x0) * C + c);
        const ALWAN_CORE_T v11 = ALWAN_NN_READ_IN((y1 * W + x1) * C + c);
        ALWAN_DET_PRECISE const ALWAN_CORE_T top = v00 * (ALWAN_CORE_LITERAL(1.0) - fx) + v01 * fx;
        ALWAN_DET_PRECISE const ALWAN_CORE_T bot = v10 * (ALWAN_CORE_LITERAL(1.0) - fx) + v11 * fx;
        ALWAN_DET_PRECISE const ALWAN_CORE_T r = top * (ALWAN_CORE_LITERAL(1.0) - fy) + bot * fy;
        return r;
    }
}

/* Softmax over n elements, one element: exp(x_i - max) / sum exp(x_j - max).
 * O(n) per element, so a whole vector is O(n^2); the vectors these models
 * apply it to are a few dozen wide, and the API tier's whole-vector form
 * computes the max and the sum once. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_NN_FN_(alwan_nn_softmax_at)(ALWAN_NN_IN_PARAMS_ int n, int i) {
    ALWAN_CORE_T m = ALWAN_NN_READ_IN(0);
    ALWAN_DET_PRECISE ALWAN_CORE_T sum = ALWAN_CORE_LITERAL(0.0);
    int j;
    ALWAN_NN_LOOP for (j = 1; j < n; j++) {
        const ALWAN_CORE_T v = ALWAN_NN_READ_IN(j);
        m = ALWAN_CORE_SELECT(v > m, v, m);
    }
    ALWAN_NN_LOOP for (j = 0; j < n; j++) sum = sum + ALWAN_CORE_EXP(ALWAN_NN_READ_IN(j) - m);
    return ALWAN_CORE_EXP(ALWAN_NN_READ_IN(i) - m) / sum;
}

#undef ALWAN_NN_FN_
#undef ALWAN_NN_PARAMS_
#undef ALWAN_NN_IN_PARAMS_
#undef ALWAN_NN_READ_IN
#undef ALWAN_NN_READ_W
#undef ALWAN_NN_READ_B
#if !defined(ALWAN_NN_READER_POINTER)
# undef ALWAN_NN_PASTE2_
# undef ALWAN_NN_PASTE_
# undef ALWAN_NN_NAME
#endif
#undef ALWAN_NN_DIV
#undef ALWAN_NN_LOOP
#undef ALWAN_NN_READER_POINTER
