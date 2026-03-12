/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * SIMD dispatch header - compile-time ISA selection
 *
 * Include chain:
 *   alwan_simd.h -> alwan_simd_{avx2,avx,sse2,scalar}.h -> alwan_simd_types.h
 *
 * Highest available ISA wins. No runtime dispatch.
 */

#ifndef ALWAN_SIMD_H
#define ALWAN_SIMD_H

#if defined(__AVX2__)
#  include "alwan_simd_avx2.h"
#elif defined(__AVX__)
#  include "alwan_simd_avx.h"
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#  include "alwan_simd_sse2.h"
#elif defined(__aarch64__) || defined(__ARM_NEON)
#  include "alwan_simd_neon.h"
#else
#  include "alwan_simd_scalar.h"
#endif

#endif /* ALWAN_SIMD_H */
