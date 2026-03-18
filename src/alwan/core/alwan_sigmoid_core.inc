/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Generic Sigmoid Utility (parameterized)
 * No include guard - included twice via setup/teardown.
 */

/* ================================================================
 * Generic Sigmoid
 *
 * Piece-wise power curve with smooth join at pivot:
 *   For x < pivot:  y = pivot * pow(x / pivot, toe_power)
 *   For x >= pivot: y = 1 - (1 - pivot) * pow((1 - x) / (1 - pivot), shoulder_power)
 *
 * slope scales the input around the pivot before applying the curve.
 * Parameters:
 *   x              - input value (typically [0,1])
 *   pivot          - midpoint of the sigmoid (default 0.18)
 *   slope          - contrast / gain around pivot (default 1.0)
 *   toe_power      - power exponent for the toe region (< pivot)
 *   shoulder_power - power exponent for the shoulder region (>= pivot)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_sigmoid)(ALWAN_CORE_T x,
                                           ALWAN_CORE_T pivot,
                                           ALWAN_CORE_T slope,
                                           ALWAN_CORE_T toe_power,
                                           ALWAN_CORE_T shoulder_power) {
    /* Scale input around pivot */
    ALWAN_CORE_T scaled = pivot + (x - pivot) * slope;
    ALWAN_CORE_T clamped = ALWAN_CORE_CLAMP(scaled, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);

    /* Safe division: avoid divide-by-zero when pivot is 0 or 1 */
    ALWAN_CORE_T safe_pivot = ALWAN_CORE_CLAMP(pivot, ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_LITERAL(1e-10));

    /* Toe region: x < pivot */
    ALWAN_CORE_T toe_ratio = clamped / safe_pivot;
    ALWAN_CORE_T toe_result = safe_pivot * ALWAN_CORE_POW(toe_ratio, toe_power);

    /* Shoulder region: x >= pivot */
    ALWAN_CORE_T shoulder_ratio = (ALWAN_CORE_ONE - clamped) / (ALWAN_CORE_ONE - safe_pivot);
    ALWAN_CORE_T shoulder_result = ALWAN_CORE_ONE - (ALWAN_CORE_ONE - safe_pivot) * ALWAN_CORE_POW(shoulder_ratio, shoulder_power);

    return ALWAN_CORE_SELECT(clamped < safe_pivot, toe_result, shoulder_result);
}

/* ================================================================
 * Generic Sigmoid Inverse
 *
 * Analytical inverse of the piece-wise power sigmoid.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_sigmoid_inv)(ALWAN_CORE_T y,
                                               ALWAN_CORE_T pivot,
                                               ALWAN_CORE_T slope,
                                               ALWAN_CORE_T toe_power,
                                               ALWAN_CORE_T shoulder_power) {
    ALWAN_CORE_T clamped = ALWAN_CORE_CLAMP(y, ALWAN_CORE_ZERO, ALWAN_CORE_ONE);

    ALWAN_CORE_T safe_pivot = ALWAN_CORE_CLAMP(pivot, ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_LITERAL(1e-10));
    ALWAN_CORE_T safe_slope = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(slope) < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(1e-10), slope);

    /* Inverse toe: y = pivot * (x/pivot)^p  =>  x = pivot * (y/pivot)^(1/p) */
    ALWAN_CORE_T inv_toe_power = ALWAN_CORE_ONE / toe_power;
    ALWAN_CORE_T toe_x = safe_pivot * ALWAN_CORE_POW(clamped / safe_pivot, inv_toe_power);

    /* Inverse shoulder: y = 1-(1-pivot)*((1-x)/(1-pivot))^p  =>  x = 1-(1-pivot)*((1-y)/(1-pivot))^(1/p) */
    ALWAN_CORE_T inv_shoulder_power = ALWAN_CORE_ONE / shoulder_power;
    ALWAN_CORE_T shoulder_x = ALWAN_CORE_ONE - (ALWAN_CORE_ONE - safe_pivot) *
        ALWAN_CORE_POW((ALWAN_CORE_ONE - clamped) / (ALWAN_CORE_ONE - safe_pivot), inv_shoulder_power);

    ALWAN_CORE_T x_scaled = ALWAN_CORE_SELECT(clamped < safe_pivot, toe_x, shoulder_x);

    /* Undo the slope scaling: scaled = pivot + (x - pivot) * slope => x = pivot + (scaled - pivot) / slope */
    return pivot + (x_scaled - pivot) / safe_slope;
}
