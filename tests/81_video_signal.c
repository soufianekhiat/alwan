/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 81: Video signal encoding/decoding
 */

#include "test_common.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Test: Full range U8 roundtrip (sRGB)
 * encode linear -> sRGB U8 full range, then decode back
 * ---------------------------------------------------------------- */
static int test_full_range_u8_roundtrip(void) {
    printf("  test_full_range_u8_roundtrip...\n");

    alwan_ctx *ctx = alwan_create(NULL);

    /* Linear values that produce clean sRGB code values */
    alwan_f64 linear_in[9] = {
        0.0, 0.0, 0.0,       /* black */
        1.0, 1.0, 1.0,       /* white */
        0.21586, 0.21586, 0.21586  /* ~50% sRGB (code ~128) */
    };

    uint8_t encoded[9];
    int status = alwan_video_encode(encoded, ALWAN_PIXEL_U8,
                                     linear_in, 3,
                                     ctx, ALWAN_RGB_SPACE_SRGB,
                                     ALWAN_VIDEO_RANGE_FULL, 8);
    if (status != ALWAN_OK) {
        printf("  FAIL: encode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    /* Black should encode to 0, white to 255 */
    if (encoded[0] != 0 || encoded[1] != 0 || encoded[2] != 0) {
        printf("  FAIL: black -> (%d,%d,%d), expected (0,0,0)\n",
               encoded[0], encoded[1], encoded[2]);
        alwan_destroy(ctx);
        return 1;
    }
    if (encoded[3] != 255 || encoded[4] != 255 || encoded[5] != 255) {
        printf("  FAIL: white -> (%d,%d,%d), expected (255,255,255)\n",
               encoded[3], encoded[4], encoded[5]);
        alwan_destroy(ctx);
        return 1;
    }

    /* Decode back to linear */
    alwan_f64 decoded[9];
    status = alwan_video_decode(decoded, encoded, ALWAN_PIXEL_U8, 3,
                                 ctx, ALWAN_RGB_SPACE_SRGB,
                                 ALWAN_VIDEO_RANGE_FULL, 8);
    if (status != ALWAN_OK) {
        printf("  FAIL: decode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    /* Black and white should roundtrip exactly */
    TEST_CHECK_NEAR(decoded[0], 0.0, 1e-10);
    TEST_CHECK_NEAR(decoded[3], 1.0, 1e-3);  /* 255/255 through sRGB EOTF */

    /* Mid-gray roundtrip: expect quantization error of at most 1/(2*255) in sRGB */
    for (int c = 0; c < 3; c++) {
        double err = fabs((double)decoded[6+c] - (double)linear_in[6+c]);
        if (err > 0.005) {
            printf("  FAIL: mid-gray roundtrip error %.4e (channel %d)\n", err, c);
            alwan_destroy(ctx);
            return 1;
        }
    }

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: Narrow range U8 (sRGB)
 * Verify black maps to code 16, white to code 235
 * ---------------------------------------------------------------- */
static int test_narrow_range_u8(void) {
    printf("  test_narrow_range_u8...\n");

    alwan_ctx *ctx = alwan_create(NULL);

    alwan_f64 linear_in[6] = {
        0.0, 0.0, 0.0,   /* black */
        1.0, 1.0, 1.0    /* white */
    };

    uint8_t encoded[6];
    int status = alwan_video_encode(encoded, ALWAN_PIXEL_U8,
                                     linear_in, 2,
                                     ctx, ALWAN_RGB_SPACE_SRGB,
                                     ALWAN_VIDEO_RANGE_NARROW, 8);
    if (status != ALWAN_OK) {
        printf("  FAIL: encode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    /* Black -> 16, White -> 235 */
    for (int c = 0; c < 3; c++) {
        if (encoded[c] != 16) {
            printf("  FAIL: black channel %d = %d, expected 16\n", c, encoded[c]);
            alwan_destroy(ctx);
            return 1;
        }
        if (encoded[3+c] != 235) {
            printf("  FAIL: white channel %d = %d, expected 235\n", c, encoded[3+c]);
            alwan_destroy(ctx);
            return 1;
        }
    }

    /* Decode roundtrip */
    alwan_f64 decoded[6];
    status = alwan_video_decode(decoded, encoded, ALWAN_PIXEL_U8, 2,
                                 ctx, ALWAN_RGB_SPACE_SRGB,
                                 ALWAN_VIDEO_RANGE_NARROW, 8);
    if (status != ALWAN_OK) {
        printf("  FAIL: decode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    TEST_CHECK_NEAR(decoded[0], 0.0, 1e-10);
    TEST_CHECK_NEAR(decoded[3], 1.0, 1e-3);

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: Narrow range 10-bit in U16
 * Black -> 64, White -> 940
 * ---------------------------------------------------------------- */
static int test_narrow_range_10bit(void) {
    printf("  test_narrow_range_10bit...\n");

    alwan_ctx *ctx = alwan_create(NULL);

    alwan_f64 linear_in[6] = {
        0.0, 0.0, 0.0,
        1.0, 1.0, 1.0
    };

    uint16_t encoded[6];
    int status = alwan_video_encode(encoded, ALWAN_PIXEL_U16,
                                     linear_in, 2,
                                     ctx, ALWAN_RGB_SPACE_SRGB,
                                     ALWAN_VIDEO_RANGE_NARROW, 10);
    if (status != ALWAN_OK) {
        printf("  FAIL: encode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    /* 10-bit narrow: foot = 16*4 = 64, head = 235*4 = 940 */
    for (int c = 0; c < 3; c++) {
        if (encoded[c] != 64) {
            printf("  FAIL: black ch%d = %d, expected 64\n", c, encoded[c]);
            alwan_destroy(ctx);
            return 1;
        }
        if (encoded[3+c] != 940) {
            printf("  FAIL: white ch%d = %d, expected 940\n", c, encoded[3+c]);
            alwan_destroy(ctx);
            return 1;
        }
    }

    /* Roundtrip */
    alwan_f64 decoded[6];
    status = alwan_video_decode(decoded, encoded, ALWAN_PIXEL_U16, 2,
                                 ctx, ALWAN_RGB_SPACE_SRGB,
                                 ALWAN_VIDEO_RANGE_NARROW, 10);
    if (status != ALWAN_OK) {
        printf("  FAIL: decode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    TEST_CHECK_NEAR(decoded[0], 0.0, 1e-10);
    TEST_CHECK_NEAR(decoded[3], 1.0, 1e-3);

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: 12-bit narrow range
 * Black -> 256, White -> 3760
 * ---------------------------------------------------------------- */
static int test_narrow_range_12bit(void) {
    printf("  test_narrow_range_12bit...\n");

    alwan_ctx *ctx = alwan_create(NULL);

    alwan_f64 linear_in[6] = {
        0.0, 0.0, 0.0,
        1.0, 1.0, 1.0
    };

    uint16_t encoded[6];
    int status = alwan_video_encode(encoded, ALWAN_PIXEL_U16,
                                     linear_in, 2,
                                     ctx, ALWAN_RGB_SPACE_SRGB,
                                     ALWAN_VIDEO_RANGE_NARROW, 12);
    if (status != ALWAN_OK) {
        printf("  FAIL: encode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    /* 12-bit narrow: foot = 16*16 = 256, head = 235*16 = 3760 */
    for (int c = 0; c < 3; c++) {
        if (encoded[c] != 256) {
            printf("  FAIL: black ch%d = %d, expected 256\n", c, encoded[c]);
            alwan_destroy(ctx);
            return 1;
        }
        if (encoded[3+c] != 3760) {
            printf("  FAIL: white ch%d = %d, expected 3760\n", c, encoded[3+c]);
            alwan_destroy(ctx);
            return 1;
        }
    }

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: Full range 10-bit in U16
 * Black -> 0, White -> 1023
 * ---------------------------------------------------------------- */
static int test_full_range_10bit(void) {
    printf("  test_full_range_10bit...\n");

    alwan_ctx *ctx = alwan_create(NULL);

    alwan_f64 linear_in[6] = {
        0.0, 0.0, 0.0,
        1.0, 1.0, 1.0
    };

    uint16_t encoded[6];
    int status = alwan_video_encode(encoded, ALWAN_PIXEL_U16,
                                     linear_in, 2,
                                     ctx, ALWAN_RGB_SPACE_SRGB,
                                     ALWAN_VIDEO_RANGE_FULL, 10);
    if (status != ALWAN_OK) {
        printf("  FAIL: encode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    for (int c = 0; c < 3; c++) {
        if (encoded[c] != 0) {
            printf("  FAIL: black ch%d = %d, expected 0\n", c, encoded[c]);
            alwan_destroy(ctx);
            return 1;
        }
        if (encoded[3+c] != 1023) {
            printf("  FAIL: white ch%d = %d, expected 1023\n", c, encoded[3+c]);
            alwan_destroy(ctx);
            return 1;
        }
    }

    /* Roundtrip */
    alwan_f64 decoded[6];
    status = alwan_video_decode(decoded, encoded, ALWAN_PIXEL_U16, 2,
                                 ctx, ALWAN_RGB_SPACE_SRGB,
                                 ALWAN_VIDEO_RANGE_FULL, 10);
    if (status != ALWAN_OK) {
        printf("  FAIL: decode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    TEST_CHECK_NEAR(decoded[0], 0.0, 1e-10);
    TEST_CHECK_NEAR(decoded[3], 1.0, 1e-3);

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: Float output with narrow range
 * ---------------------------------------------------------------- */
static int test_float_narrow_range(void) {
    printf("  test_float_narrow_range...\n");

    alwan_ctx *ctx = alwan_create(NULL);

    alwan_f64 linear_in[6] = {
        0.0, 0.0, 0.0,
        1.0, 1.0, 1.0
    };

    float encoded[6];
    int status = alwan_video_encode(encoded, ALWAN_PIXEL_F32,
                                     linear_in, 2,
                                     ctx, ALWAN_RGB_SPACE_SRGB,
                                     ALWAN_VIDEO_RANGE_NARROW, 8);
    if (status != ALWAN_OK) {
        printf("  FAIL: encode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    /* Black should map to 16/255, white to 235/255 */
    double foot_norm = 16.0 / 255.0;
    double head_norm = 235.0 / 255.0;

    for (int c = 0; c < 3; c++) {
        TEST_CHECK_NEAR(encoded[c], foot_norm, 1e-5);
        TEST_CHECK_NEAR(encoded[3+c], head_norm, 1e-3);
    }

    /* Roundtrip via float decode */
    alwan_f64 decoded[6];
    status = alwan_video_decode(decoded, encoded, ALWAN_PIXEL_F32, 2,
                                 ctx, ALWAN_RGB_SPACE_SRGB,
                                 ALWAN_VIDEO_RANGE_NARROW, 8);
    if (status != ALWAN_OK) {
        printf("  FAIL: decode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    TEST_CHECK_NEAR(decoded[0], 0.0, 1e-6);
    TEST_CHECK_NEAR(decoded[3], 1.0, 1e-3);

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: PQ (Rec.2100) encode/decode roundtrip
 * ---------------------------------------------------------------- */
static int test_pq_roundtrip(void) {
    printf("  test_pq_roundtrip...\n");

    alwan_ctx *ctx = alwan_create(NULL);

    alwan_f64 linear_in[9] = {
        0.0, 0.0, 0.0,
        0.5, 0.5, 0.5,
        1.0, 1.0, 1.0
    };

    /* Encode to float so we don't lose precision to quantization */
    double encoded[9];
    int status = alwan_video_encode(encoded, ALWAN_PIXEL_F64,
                                     linear_in, 3,
                                     ctx, ALWAN_RGB_SPACE_REC2100_PQ,
                                     ALWAN_VIDEO_RANGE_FULL, 0);
    if (status != ALWAN_OK) {
        printf("  FAIL: encode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    /* PQ-encoded values should be in [0,1] and monotonically increasing */
    if (encoded[0] < -1e-10) {
        printf("  FAIL: PQ(0.0) = %f, expected >= 0\n", encoded[0]);
        alwan_destroy(ctx);
        return 1;
    }
    if (encoded[3] <= encoded[0] || encoded[6] <= encoded[3]) {
        printf("  FAIL: PQ values not monotonic: %.6f, %.6f, %.6f\n",
               encoded[0], encoded[3], encoded[6]);
        alwan_destroy(ctx);
        return 1;
    }

    /* Decode */
    alwan_f64 decoded[9];
    status = alwan_video_decode(decoded, encoded, ALWAN_PIXEL_F64, 3,
                                 ctx, ALWAN_RGB_SPACE_REC2100_PQ,
                                 ALWAN_VIDEO_RANGE_FULL, 0);
    if (status != ALWAN_OK) {
        printf("  FAIL: decode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    TEST_CHECK_NEAR(decoded[0], 0.0, TEST_REL_EPSILON);
    TEST_CHECK_NEAR(decoded[3], 0.5, TEST_REL_EPSILON);
    TEST_CHECK_NEAR(decoded[6], 1.0, TEST_REL_EPSILON);

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: Display P3 encode/decode roundtrip
 * Uses sRGB TF (which has matching OETF/EOTF inverses), with
 * different primaries from the sRGB test above.
 * Note: BT.709/BT.1886 and HLG have intentionally asymmetric
 * OETF/EOTF (system gamma ~1.2) and do NOT roundtrip.
 * ---------------------------------------------------------------- */
static int test_display_p3_roundtrip(void) {
    printf("  test_display_p3_roundtrip...\n");

    alwan_ctx *ctx = alwan_create(NULL);

    alwan_f64 linear_in[15] = {
        0.0, 0.0, 0.0,
        0.1, 0.2, 0.3,
        0.5, 0.5, 0.5,
        0.8, 0.9, 0.7,
        1.0, 1.0, 1.0
    };

    double encoded[15];
    int status = alwan_video_encode(encoded, ALWAN_PIXEL_F64,
                                     linear_in, 5,
                                     ctx, ALWAN_RGB_SPACE_DISPLAY_P3,
                                     ALWAN_VIDEO_RANGE_FULL, 0);
    if (status != ALWAN_OK) {
        printf("  FAIL: encode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    alwan_f64 decoded[15];
    status = alwan_video_decode(decoded, encoded, ALWAN_PIXEL_F64, 5,
                                 ctx, ALWAN_RGB_SPACE_DISPLAY_P3,
                                 ALWAN_VIDEO_RANGE_FULL, 0);
    if (status != ALWAN_OK) {
        printf("  FAIL: decode returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    for (int i = 0; i < 15; i++) {
        TEST_CHECK_NEAR(decoded[i], linear_in[i], TEST_REL_EPSILON);
    }

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: Narrow range roundtrip with multiple bit depths
 * ---------------------------------------------------------------- */
static int test_narrow_roundtrip_multi_depth(void) {
    printf("  test_narrow_roundtrip_multi_depth...\n");

    alwan_ctx *ctx = alwan_create(NULL);

    alwan_f64 linear_in[9] = {
        0.1, 0.3, 0.5,
        0.7, 0.2, 0.9,
        0.0, 1.0, 0.5
    };

    int depths[] = {8, 10, 12};
    int num_depths = 3;

    for (int d = 0; d < num_depths; d++) {
        int bit_depth = depths[d];
        uint16_t encoded[9];
        int status;
        if (bit_depth == 8) {
            uint8_t enc8[9];
            status = alwan_video_encode(enc8, ALWAN_PIXEL_U8,
                                         linear_in, 3,
                                         ctx, ALWAN_RGB_SPACE_SRGB,
                                         ALWAN_VIDEO_RANGE_NARROW, 8);
            if (status != ALWAN_OK) {
                printf("  FAIL: encode %d-bit returned %d\n", bit_depth, status);
                alwan_destroy(ctx);
                return 1;
            }

            alwan_f64 decoded[9];
            status = alwan_video_decode(decoded, enc8, ALWAN_PIXEL_U8, 3,
                                         ctx, ALWAN_RGB_SPACE_SRGB,
                                         ALWAN_VIDEO_RANGE_NARROW, 8);
            if (status != ALWAN_OK) {
                printf("  FAIL: decode %d-bit returned %d\n", bit_depth, status);
                alwan_destroy(ctx);
                return 1;
            }

            /* 8-bit narrow range has only 219 levels, expect ~0.5% error */
            for (int i = 0; i < 9; i++) {
                double err = fabs((double)decoded[i] - (double)linear_in[i]);
                if (err > 0.01) {
                    printf("  FAIL: %d-bit roundtrip error %.4e at index %d\n",
                           bit_depth, err, i);
                    alwan_destroy(ctx);
                    return 1;
                }
            }
        } else {
            status = alwan_video_encode(encoded, ALWAN_PIXEL_U16,
                                         linear_in, 3,
                                         ctx, ALWAN_RGB_SPACE_SRGB,
                                         ALWAN_VIDEO_RANGE_NARROW, bit_depth);
            if (status != ALWAN_OK) {
                printf("  FAIL: encode %d-bit returned %d\n", bit_depth, status);
                alwan_destroy(ctx);
                return 1;
            }

            alwan_f64 decoded[9];
            status = alwan_video_decode(decoded, encoded, ALWAN_PIXEL_U16, 3,
                                         ctx, ALWAN_RGB_SPACE_SRGB,
                                         ALWAN_VIDEO_RANGE_NARROW, bit_depth);
            if (status != ALWAN_OK) {
                printf("  FAIL: decode %d-bit returned %d\n", bit_depth, status);
                alwan_destroy(ctx);
                return 1;
            }

            /* Higher bit depths should have lower error */
            double max_err = (bit_depth == 10) ? 0.002 : 0.0005;
            for (int i = 0; i < 9; i++) {
                double err = fabs((double)decoded[i] - (double)linear_in[i]);
                if (err > max_err) {
                    printf("  FAIL: %d-bit roundtrip error %.4e at index %d (limit %.4e)\n",
                           bit_depth, err, i, max_err);
                    alwan_destroy(ctx);
                    return 1;
                }
            }
        }

        printf("    %d-bit narrow roundtrip OK\n", bit_depth);
    }

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: NULL checks
 * ---------------------------------------------------------------- */
static int test_video_null_checks(void) {
    printf("  test_video_null_checks...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_f64 linear[3] = {0.5, 0.5, 0.5};
    uint8_t buf[3];

    if (alwan_video_encode(NULL, ALWAN_PIXEL_U8, linear, 1,
                            ctx, ALWAN_RGB_SPACE_SRGB,
                            ALWAN_VIDEO_RANGE_FULL, 8) != ALWAN_E_INVALID)
        return 1;
    if (alwan_video_encode(buf, ALWAN_PIXEL_U8, NULL, 1,
                            ctx, ALWAN_RGB_SPACE_SRGB,
                            ALWAN_VIDEO_RANGE_FULL, 8) != ALWAN_E_INVALID)
        return 1;
    if (alwan_video_encode(buf, ALWAN_PIXEL_U8, linear, 0,
                            ctx, ALWAN_RGB_SPACE_SRGB,
                            ALWAN_VIDEO_RANGE_FULL, 8) != ALWAN_E_INVALID)
        return 1;

    if (alwan_video_decode(NULL, buf, ALWAN_PIXEL_U8, 1,
                            ctx, ALWAN_RGB_SPACE_SRGB,
                            ALWAN_VIDEO_RANGE_FULL, 8) != ALWAN_E_INVALID)
        return 1;
    if (alwan_video_decode(linear, NULL, ALWAN_PIXEL_U8, 1,
                            ctx, ALWAN_RGB_SPACE_SRGB,
                            ALWAN_VIDEO_RANGE_FULL, 8) != ALWAN_E_INVALID)
        return 1;

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */
int test_81_video_signal_main(void) {
    int fail = 0;

    fail += test_full_range_u8_roundtrip();
    fail += test_narrow_range_u8();
    fail += test_narrow_range_10bit();
    fail += test_narrow_range_12bit();
    fail += test_full_range_10bit();
    fail += test_float_narrow_range();
    fail += test_pq_roundtrip();
    fail += test_display_p3_roundtrip();
    fail += test_narrow_roundtrip_multi_depth();
    fail += test_video_null_checks();

    return fail;
}
