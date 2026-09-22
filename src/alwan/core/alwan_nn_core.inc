/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Neural layer arithmetic (parameterized precision).
 * No include guard - included twice via setup/teardown on C, once on GPU.
 *
 * The activations are plain value functions and live here. The kernels that
 * read a tensor live in alwan_nn_reader.inc, written against an accessor, and
 * are bound to pointers below wherever pointers exist. See that file for the
 * layout and the summation-order contract.
 */

/* ----------------------------------------------------------------
 * Activations. The saturating ones go through exp alone, so a deterministic
 * build reaches them through the one committed polynomial: tanh is
 * 1 - 2 / (1 + exp(2x)), sigmoid is 1 / (1 + exp(-x)). GELU is the tanh form,
 * 0.5 x (1 + tanh(sqrt(2/pi) (x + 0.044715 x^3))), which PyTorch calls
 * approximate='tanh'; the erf form is not offered because the core has no erf,
 * and the reference tests ask torch for the tanh form explicitly.
 * ---------------------------------------------------------------- */

#define ALWAN_NN_ACT_RELU     0
#define ALWAN_NN_ACT_LEAKY    1
#define ALWAN_NN_ACT_SIGMOID  2
#define ALWAN_NN_ACT_TANH     3
#define ALWAN_NN_ACT_GELU     4

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_relu)(ALWAN_CORE_T x) {
    return ALWAN_CORE_SELECT(x > ALWAN_CORE_LITERAL(0.0), x, ALWAN_CORE_LITERAL(0.0));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_leaky_relu)(ALWAN_CORE_T x, ALWAN_CORE_T alpha) {
    return ALWAN_CORE_SELECT(x > ALWAN_CORE_LITERAL(0.0), x, alpha * x);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_sigmoid)(ALWAN_CORE_T x) {
    return ALWAN_CORE_LITERAL(1.0) / (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-x));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_tanh)(ALWAN_CORE_T x) {
    return ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_LITERAL(2.0) / (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(2.0) * x));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_gelu)(ALWAN_CORE_T x) {
    const ALWAN_CORE_T k = ALWAN_CORE_LITERAL(0.7978845608028654);   /* sqrt(2 / pi) */
    const ALWAN_CORE_T u = k * (x + ALWAN_CORE_LITERAL(0.044715) * x * x * x);
    return ALWAN_CORE_LITERAL(0.5) * x * (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_FNVLIT(alwan_nn_tanh)(u));
}

/* One activation by kind; alpha is read by LEAKY only. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_activation)(ALWAN_CORE_T x, int kind, ALWAN_CORE_T alpha) {
    if (kind == ALWAN_NN_ACT_RELU)    return ALWAN_CORE_FNVLIT(alwan_nn_relu)(x);
    if (kind == ALWAN_NN_ACT_LEAKY)   return ALWAN_CORE_FNVLIT(alwan_nn_leaky_relu)(x, alpha);
    if (kind == ALWAN_NN_ACT_SIGMOID) return ALWAN_CORE_FNVLIT(alwan_nn_sigmoid)(x);
    if (kind == ALWAN_NN_ACT_TANH)    return ALWAN_CORE_FNVLIT(alwan_nn_tanh)(x);
    return ALWAN_CORE_FNVLIT(alwan_nn_gelu)(x);
}

/* ----------------------------------------------------------------
 * The tensor kernels, bound to pointers wherever pointers exist. A shading
 * language binds them itself: see the top of alwan_nn_reader.inc.
 * ---------------------------------------------------------------- */
#if ALWAN_HAS_POINTERS
#define ALWAN_NN_READER_POINTER 1
#include "alwan_nn_reader.inc"
#endif
