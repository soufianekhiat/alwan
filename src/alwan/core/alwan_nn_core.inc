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
#define ALWAN_NN_ACT_SILU     5
#define ALWAN_NN_ACT_ELU      6
#define ALWAN_NN_ACT_MISH     7
#define ALWAN_NN_ACT_SOFTPLUS 8
#define ALWAN_NN_ACT_HARDSWISH   9
#define ALWAN_NN_ACT_HARDSIGMOID 10

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

/* log1p and expm1 without the cancellation of log(1 + y) and exp(x) - 1, which torch's
 * ELU, Softplus and Mish avoid too: at x = -40 Mish is -1.7e-16, not 0. Near zero both
 * are series, plain arithmetic with no transcendental, so the deterministic build's
 * polynomial log2, accurate in absolute terms but not relative to u - 1 near u = 1,
 * never decides the digits there: log1p(y) = 2 atanh(z), z = y / (2 + y), through z^17
 * for y < 0.25 (z <= 1/9, the next term below 1e-18 of the sum); expm1(x) the Taylor
 * polynomial through x^12 for |x| < 0.25 (the next term below 1e-17). Further out
 * neither cancels and the plain forms are used. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_log1p)(ALWAN_CORE_T y) {
    const ALWAN_CORE_T z = y / (ALWAN_CORE_LITERAL(2.0) + y);
    const ALWAN_CORE_T z2 = z * z;
    const ALWAN_CORE_T series = ALWAN_CORE_LITERAL(2.0) * z *
        (ALWAN_CORE_LITERAL(1.0) + z2 * (ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(3.0) +
         z2 * (ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(5.0) +
         z2 * (ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(7.0) +
         z2 * (ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(9.0) +
         z2 * (ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(11.0) +
         z2 * (ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(13.0) +
         z2 * (ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(15.0) +
         z2 * (ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(17.0))))))))));
    const ALWAN_CORE_T u = ALWAN_CORE_LITERAL(1.0) + y;
    const ALWAN_CORE_T usafe = ALWAN_CORE_SELECT(y < ALWAN_CORE_LITERAL(0.25), ALWAN_CORE_LITERAL(2.0), u);
    const ALWAN_CORE_T plain = ALWAN_CORE_LOG2(usafe) * ALWAN_CORE_LITERAL(0.69314718055994530942);
    return ALWAN_CORE_SELECT(y < ALWAN_CORE_LITERAL(0.25), series, plain);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_expm1)(ALWAN_CORE_T x) {
    const ALWAN_CORE_T ax = ALWAN_CORE_ABS(x);
    const ALWAN_CORE_T series = x *
        (ALWAN_CORE_LITERAL(1.0) + x / ALWAN_CORE_LITERAL(2.0) *
        (ALWAN_CORE_LITERAL(1.0) + x / ALWAN_CORE_LITERAL(3.0) *
        (ALWAN_CORE_LITERAL(1.0) + x / ALWAN_CORE_LITERAL(4.0) *
        (ALWAN_CORE_LITERAL(1.0) + x / ALWAN_CORE_LITERAL(5.0) *
        (ALWAN_CORE_LITERAL(1.0) + x / ALWAN_CORE_LITERAL(6.0) *
        (ALWAN_CORE_LITERAL(1.0) + x / ALWAN_CORE_LITERAL(7.0) *
        (ALWAN_CORE_LITERAL(1.0) + x / ALWAN_CORE_LITERAL(8.0) *
        (ALWAN_CORE_LITERAL(1.0) + x / ALWAN_CORE_LITERAL(9.0) *
        (ALWAN_CORE_LITERAL(1.0) + x / ALWAN_CORE_LITERAL(10.0) *
        (ALWAN_CORE_LITERAL(1.0) + x / ALWAN_CORE_LITERAL(11.0) *
        (ALWAN_CORE_LITERAL(1.0) + x / ALWAN_CORE_LITERAL(12.0))))))))))));
    const ALWAN_CORE_T xs = ALWAN_CORE_SELECT(ax < ALWAN_CORE_LITERAL(0.25), ALWAN_CORE_LITERAL(1.0), x);
    const ALWAN_CORE_T plain = ALWAN_CORE_EXP(xs) - ALWAN_CORE_LITERAL(1.0);
    return ALWAN_CORE_SELECT(ax < ALWAN_CORE_LITERAL(0.25), series, plain);
}

/* SiLU (swish-1): x / (1 + exp(-x)), the form torch evaluates. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_silu)(ALWAN_CORE_T x) {
    return x / (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EXP(-x));
}

/* ELU: alpha expm1(x) below zero, x above; alpha 0 means torch's default 1. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_elu)(ALWAN_CORE_T x, ALWAN_CORE_T alpha) {
    const ALWAN_CORE_T a = ALWAN_CORE_SELECT(alpha == ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0), alpha);
    const ALWAN_CORE_T xn = ALWAN_CORE_SELECT(x > ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0), x);
    return ALWAN_CORE_SELECT(x > ALWAN_CORE_LITERAL(0.0), x, a * ALWAN_CORE_FNVLIT(alwan_nn_expm1)(xn));
}

/* Softplus: log1p(exp(beta x)) / beta, and x itself once beta x passes torch's fixed
 * threshold of 20; alpha carries beta, 0 meaning torch's default 1. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_softplus)(ALWAN_CORE_T x, ALWAN_CORE_T alpha) {
    const ALWAN_CORE_T beta = ALWAN_CORE_SELECT(alpha == ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0), alpha);
    const ALWAN_CORE_T bx = beta * x;
    const ALWAN_CORE_T bxs = ALWAN_CORE_SELECT(bx > ALWAN_CORE_LITERAL(20.0), ALWAN_CORE_LITERAL(0.0), bx);
    const ALWAN_CORE_T sp = ALWAN_CORE_FNVLIT(alwan_nn_log1p)(ALWAN_CORE_EXP(bxs)) / beta;
    return ALWAN_CORE_SELECT(bx > ALWAN_CORE_LITERAL(20.0), x, sp);
}

/* Mish: x tanh(log1p(exp(x))). torch has no threshold here; above 20 the tanh is 1
 * to the last bit in both precisions, so x is returned there, which also keeps exp
 * from overflowing into inf / inf. The softplus s is never negative, and its tanh is
 * taken as expm1(2s) / (expm1(2s) + 2): a tanh written 1 - 2 / (1 + exp(2s)), as the
 * deterministic build's is, returns 0 for s = 4e-18 (x = -40) where the answer is s. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_mish)(ALWAN_CORE_T x) {
    const ALWAN_CORE_T xs = ALWAN_CORE_SELECT(x > ALWAN_CORE_LITERAL(20.0), ALWAN_CORE_LITERAL(0.0), x);
    const ALWAN_CORE_T sp = ALWAN_CORE_FNVLIT(alwan_nn_log1p)(ALWAN_CORE_EXP(xs));
    const ALWAN_CORE_T em = ALWAN_CORE_FNVLIT(alwan_nn_expm1)(ALWAN_CORE_LITERAL(2.0) * sp);
    return ALWAN_CORE_SELECT(x > ALWAN_CORE_LITERAL(20.0), x, x * (em / (em + ALWAN_CORE_LITERAL(2.0))));
}

/* HardSigmoid: relu6(x + 3) / 6; HardSwish: x relu6(x + 3) / 6, in torch's order. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_relu6_shift)(ALWAN_CORE_T x) {
    const ALWAN_CORE_T t = x + ALWAN_CORE_LITERAL(3.0);
    const ALWAN_CORE_T lo = ALWAN_CORE_SELECT(t > ALWAN_CORE_LITERAL(0.0), t, ALWAN_CORE_LITERAL(0.0));
    return ALWAN_CORE_SELECT(lo < ALWAN_CORE_LITERAL(6.0), lo, ALWAN_CORE_LITERAL(6.0));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_hardswish)(ALWAN_CORE_T x) {
    return x * ALWAN_CORE_FNVLIT(alwan_nn_relu6_shift)(x) / ALWAN_CORE_LITERAL(6.0);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_hardsigmoid)(ALWAN_CORE_T x) {
    return ALWAN_CORE_FNVLIT(alwan_nn_relu6_shift)(x) / ALWAN_CORE_LITERAL(6.0);
}

/* One activation by kind; alpha is read by LEAKY (the slope), ELU (alpha, 0 = 1) and
 * SOFTPLUS (beta, 0 = 1) only. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_nn_activation)(ALWAN_CORE_T x, int kind, ALWAN_CORE_T alpha) {
    if (kind == ALWAN_NN_ACT_RELU)    return ALWAN_CORE_FNVLIT(alwan_nn_relu)(x);
    if (kind == ALWAN_NN_ACT_LEAKY)   return ALWAN_CORE_FNVLIT(alwan_nn_leaky_relu)(x, alpha);
    if (kind == ALWAN_NN_ACT_SIGMOID) return ALWAN_CORE_FNVLIT(alwan_nn_sigmoid)(x);
    if (kind == ALWAN_NN_ACT_TANH)    return ALWAN_CORE_FNVLIT(alwan_nn_tanh)(x);
    if (kind == ALWAN_NN_ACT_SILU)    return ALWAN_CORE_FNVLIT(alwan_nn_silu)(x);
    if (kind == ALWAN_NN_ACT_ELU)     return ALWAN_CORE_FNVLIT(alwan_nn_elu)(x, alpha);
    if (kind == ALWAN_NN_ACT_MISH)    return ALWAN_CORE_FNVLIT(alwan_nn_mish)(x);
    if (kind == ALWAN_NN_ACT_SOFTPLUS) return ALWAN_CORE_FNVLIT(alwan_nn_softplus)(x, alpha);
    if (kind == ALWAN_NN_ACT_HARDSWISH) return ALWAN_CORE_FNVLIT(alwan_nn_hardswish)(x);
    if (kind == ALWAN_NN_ACT_HARDSIGMOID) return ALWAN_CORE_FNVLIT(alwan_nn_hardsigmoid)(x);
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
