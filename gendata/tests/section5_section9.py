#!/usr/bin/env python3
"""
Generate test reference data for Section 5 (ACES 2.0) and Section 9 (Transfer Functions).

Section 9 (Apple Log, DCDM): Pure Python float64 implementations matching the
specifications. This avoids OCIO's float32 internal processing which limits
precision to ~7 significant digits.

Section 5 (ACES 2.0): Pure Python float64 implementations of the Hellwig 2022
color appearance model used in ACES 2.0 (RGB_to_JMh20).
"""

import numpy as np
import math
import os

# Output directory
OUTPUT_DIR = "tests/reference_values"

def format_value(x):
    """Format a single value for C inclusion."""
    if np.isnan(x):
        return 'NAN'
    elif np.isinf(x):
        return 'INFINITY' if x > 0 else '-INFINITY'
    else:
        return f'{x:.16e}'

def save_csv(filename, data, comment=""):
    """Save data to CSV with maximum precision for C inclusion."""
    filepath = os.path.join(OUTPUT_DIR, filename)
    with open(filepath, 'w') as f:
        if isinstance(data, np.ndarray):
            if data.ndim == 1:
                # Single row - comma separated
                f.write(','.join(format_value(x) for x in data) + '\n')
            else:
                # Multiple rows - comma at end of each line
                for i, row in enumerate(data):
                    line = ','.join(format_value(x) for x in row)
                    if i < len(data) - 1:
                        line += ','
                    f.write(line + '\n')
        else:
            f.write(format_value(data) + '\n')
    print(f"  {filename}")


# ============================================================================
# Pure Python float64 Transfer Function Implementations
# ============================================================================

def apple_log_oetf(linear):
    """Apple Log encoding (linear -> encoded). float64 precision.
    Reference: Apple Technical Note TN3171."""
    R0 = -0.05641088
    Rt = 0.01
    c = 47.28711236
    beta = 0.00964052
    gamma_coeff = 0.08550479
    delta = 0.69336945
    if linear < R0:
        return 0.0
    elif linear < Rt:
        return c * (linear - R0) ** 2
    else:
        return gamma_coeff * math.log2(linear + beta) + delta


def apple_log_eotf(encoded):
    """Apple Log decoding (encoded -> linear). float64 precision.
    Reference: Apple Technical Note TN3171."""
    R0 = -0.05641088
    c = 47.28711236
    beta = 0.00964052
    gamma_coeff = 0.08550479
    delta = 0.69336945
    Pt = c * (0.01 - R0) ** 2  # parabolic threshold in encoded domain
    if encoded < 0.0:
        return R0
    elif encoded < Pt:
        return math.sqrt(encoded / c) + R0
    else:
        return 2.0 ** ((encoded - delta) / gamma_coeff) - beta


def dcdm_oetf(linear):
    """DCDM encoding (linear XYZ -> encoded). float64 precision.
    Reference: SMPTE ST 428-1, OCIO CIE-XYZ-D65_to_DCDM-D65."""
    if linear <= 0.0:
        return 0.0
    return (linear * 48.0 / 52.37) ** (1.0 / 2.6)


def dcdm_eotf(encoded):
    """DCDM decoding (encoded -> linear XYZ). float64 precision.
    Reference: SMPTE ST 428-1."""
    if encoded <= 0.0:
        return 0.0
    return encoded ** 2.6 * 52.37 / 48.0


# ============================================================================
# ACES 2.0 Color Appearance Model (Hellwig 2022) - Pure Python float64
# Reference: ACES CTL, OCIO FixedFunctionOpCPU.cpp
# ============================================================================

# --- Constants ---
ACES2_REF_LUMINANCE = 100.0
ACES2_L_A = 100.0
ACES2_Y_b = 20.0
ACES2_SURROUND_C = 0.59
ACES2_SURROUND_N_c = 0.9
CAM_NL_OFFSET = 27.13
CAM_NL_SCALE = 400.0
J_SCALE = 100.0

# CAM16 reference primaries (from ACES/OCIO)
CAM16_PRI = {
    'rx': 0.8336,   'ry': 0.1735,
    'gx': 2.3854,   'gy': -1.4659,
    'bx': 0.087,    'by': -0.125,
    'wx': 1.0/3.0,  'wy': 1.0/3.0,
}

# Cone-to-Aab base matrix (achromatic + opponent channels)
CONE_TO_AAB_BASE = np.array([
    [2.0,  1.0,  0.05],
    [1.0, -12.0/11.0,  1.0/11.0],
    [1.0/9.0,  1.0/9.0, -2.0/9.0],
])


def compute_npm(rx, ry, gx, gy, bx, by, wx, wy, Y=1.0):
    """Compute Normalized Primary Matrix (RGB->XYZ) from primaries and white."""
    XYZ_r = np.array([rx/ry, 1.0, (1-rx-ry)/ry])
    XYZ_g = np.array([gx/gy, 1.0, (1-gx-gy)/gy])
    XYZ_b = np.array([bx/by, 1.0, (1-bx-by)/by])
    W = np.array([wx/wy, Y, (1-wx-wy)/wy])
    P = np.column_stack([XYZ_r, XYZ_g, XYZ_b])
    S = np.linalg.solve(P, W)
    return P @ np.diag(S)


def cone_response_fwd(v):
    """Post-adaptation cone response compression: Ra = sign(v) * |v|^0.42 / (27.13 + |v|^0.42)"""
    abs_v = abs(v)
    if abs_v < 1e-10:
        return 0.0
    f = abs_v ** 0.42
    Ra = f / (CAM_NL_OFFSET + f)
    return math.copysign(Ra, v)


def cone_response_inv(Ra):
    """Inverse cone response: recover v from Ra."""
    abs_Ra = abs(Ra)
    if abs_Ra < 1e-10:
        return 0.0
    abs_Ra = min(abs_Ra, 0.99)
    f = CAM_NL_OFFSET * abs_Ra / (1.0 - abs_Ra)
    v = f ** (1.0 / 0.42)
    return math.copysign(v, Ra)


class JMhParams:
    """Pre-computed parameters for RGB <-> JMh conversion."""

    def __init__(self, rx, ry, gx, gy, bx, by, wx, wy):
        # F_L (luminance adaptation factor)
        k = 1.0 / (5.0 * ACES2_L_A + 1.0)
        k4 = k ** 4
        F_L = (0.2 * k4 * (5.0 * ACES2_L_A)
               + 0.1 * (1.0 - k4) ** 2 * (5.0 * ACES2_L_A) ** (1.0/3.0))
        F_L_n = F_L / ACES2_REF_LUMINANCE
        self.F_L_n = F_L_n

        # Model gamma
        self.cz = ACES2_SURROUND_C * (1.48 + math.sqrt(ACES2_Y_b / ACES2_REF_LUMINANCE))
        self.inv_cz = 1.0 / self.cz

        # Build RGB->XYZ for input primaries
        rgb_to_xyz = compute_npm(rx, ry, gx, gy, bx, by, wx, wy, 1.0)

        # Build XYZ->CAM16 RGB
        cam16_rgb_to_xyz = compute_npm(
            CAM16_PRI['rx'], CAM16_PRI['ry'],
            CAM16_PRI['gx'], CAM16_PRI['gy'],
            CAM16_PRI['bx'], CAM16_PRI['by'],
            CAM16_PRI['wx'], CAM16_PRI['wy'], 1.0)
        xyz_to_cam16 = np.linalg.inv(cam16_rgb_to_xyz)

        # White XYZ
        white_xyz = rgb_to_xyz @ np.array([1.0, 1.0, 1.0])
        Y_W = white_xyz[1]

        # White in CAM16 space
        RGB_w = xyz_to_cam16 @ white_xyz

        # Chromatic adaptation coefficients
        D_RGB = np.array([F_L_n * Y_W / RGB_w[i] for i in range(3)])

        # RGB->CAM16 with chromatic adaptation (scaled by REF_LUMINANCE)
        rgb_to_cam16_base = xyz_to_cam16 @ rgb_to_xyz * ACES2_REF_LUMINANCE
        self.MATRIX_RGB_to_CAM16 = np.diag(D_RGB) @ rgb_to_cam16_base

        # White adapted
        white_cam16 = self.MATRIX_RGB_to_CAM16 @ np.array([1.0, 1.0, 1.0])

        # Cone response on white
        rgb_a_w = np.array([cone_response_fwd(white_cam16[i]) for i in range(3)])

        # Build cone_to_aab (scaled)
        cone_to_aab = CAM_NL_SCALE * CONE_TO_AAB_BASE

        # Achromatic response of white
        A_w = cone_to_aab[0] @ rgb_a_w

        # Build cone_response_to_Aab matrix
        ab_scale = 43.0 * ACES2_SURROUND_N_c
        self.MATRIX_cone_to_Aab = np.zeros((3, 3))
        self.MATRIX_cone_to_Aab[0] = cone_to_aab[0] / A_w
        self.MATRIX_cone_to_Aab[1] = cone_to_aab[1] * ab_scale
        self.MATRIX_cone_to_Aab[2] = cone_to_aab[2] * ab_scale

        # A_w_J for J normalization
        self.A_w_J = cone_response_fwd(F_L)
        self.inv_A_w_J = 1.0 / self.A_w_J

        # Inverse matrices
        self.MATRIX_CAM16_to_RGB = np.linalg.inv(self.MATRIX_RGB_to_CAM16)
        self.MATRIX_Aab_to_cone = np.linalg.inv(self.MATRIX_cone_to_Aab)


def rgb_to_jmh20(r, g, b, params):
    """Convert AP1 RGB to JMh using Hellwig 2022 model. Pure float64."""
    rgb = np.array([r, g, b])

    # Step 1: RGB to adapted CAM16
    rgb_m = params.MATRIX_RGB_to_CAM16 @ rgb

    # Step 2: Cone response compression
    rgb_a = np.array([cone_response_fwd(rgb_m[i]) for i in range(3)])

    # Step 3: Opponent color space
    aab = params.MATRIX_cone_to_Aab @ rgb_a

    # Step 4: Aab to JMh
    if aab[0] <= 0.0:
        return np.array([0.0, 0.0, 0.0])

    J = J_SCALE * aab[0] ** params.cz
    M = math.sqrt(aab[1] ** 2 + aab[2] ** 2)
    h_rad = math.atan2(aab[2], aab[1])
    h_deg = math.degrees(h_rad)
    if h_deg < 0:
        h_deg += 360.0

    return np.array([J, M, h_deg])


def jmh_to_rgb20(J, M, h_deg, params):
    """Convert JMh to AP1 RGB using Hellwig 2022 model inverse. Pure float64."""
    if J <= 0.0:
        return np.array([0.0, 0.0, 0.0])

    # JMh to Aab
    A = (J / J_SCALE) ** params.inv_cz
    h_rad = math.radians(h_deg)
    aab = np.array([A, M * math.cos(h_rad), M * math.sin(h_rad)])

    # Aab to cone response
    rgb_a = params.MATRIX_Aab_to_cone @ aab

    # Inverse cone response
    rgb_m = np.array([cone_response_inv(rgb_a[i]) for i in range(3)])

    # CAM16 to RGB
    return params.MATRIX_CAM16_to_RGB @ rgb_m

def main():
    print("=" * 70)
    print("Generating Section 5 & 9 test reference data")
    print("=" * 70)
    print(f"Output directory: {OUTPUT_DIR}")
    print()

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # ========================================================================
    # Test values for transfer functions (1D - single channel applied to all RGB)
    # ========================================================================
    linear_test_values = np.array([
        0.0, 0.001, 0.005, 0.01, 0.02, 0.05, 0.1, 0.18, 0.5, 1.0,
        2.0, 5.0, 10.0, 12.0, 100.0
    ])

    encoded_test_values = np.array([
        0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0
    ])

    # ========================================================================
    # Section 9: Transfer Functions (float64)
    # ========================================================================
    print()
    print("=== Section 9: Transfer Functions (float64) ===")

    print("Apple Log (float64)...")
    apple_log_decoded = np.array([apple_log_eotf(float(v)) for v in encoded_test_values])
    save_csv("apple_log_decode_input.csv", encoded_test_values)
    save_csv("apple_log_decode_output.csv", apple_log_decoded)

    apple_log_encoded = np.array([apple_log_oetf(float(v)) for v in linear_test_values])
    save_csv("apple_log_encode_input.csv", linear_test_values)
    save_csv("apple_log_encode_output.csv", apple_log_encoded)

    print("DCDM (float64)...")
    dcdm_encoded = np.array([dcdm_oetf(float(v)) for v in linear_test_values])
    save_csv("dcdm_encode_input.csv", linear_test_values)
    save_csv("dcdm_encode_output.csv", dcdm_encoded)

    dcdm_decoded = np.array([dcdm_eotf(float(v)) for v in encoded_test_values])
    save_csv("dcdm_decode_input.csv", encoded_test_values)
    save_csv("dcdm_decode_output.csv", dcdm_decoded)

    # ========================================================================
    # Test RGB values for ACES 2.0 (comprehensive set)
    # ========================================================================
    test_rgb = np.array([
        [1.0, 0.0, 0.0],     # Pure red
        [0.0, 1.0, 0.0],     # Pure green
        [0.0, 0.0, 1.0],     # Pure blue
        [1.0, 1.0, 0.0],     # Yellow
        [0.0, 1.0, 1.0],     # Cyan
        [1.0, 0.0, 1.0],     # Magenta
        [0.0, 0.0, 0.0],     # Black
        [0.18, 0.18, 0.18],  # 18% gray
        [0.5, 0.5, 0.5],     # 50% gray
        [1.0, 1.0, 1.0],     # White
        [0.8, 0.1, 0.05],    # Dark saturated red
        [1.0, 0.2, 0.1],     # Bright saturated red
        [0.5, 0.0, 0.0],     # Medium red
        [1.0, 0.5, 0.0],     # Orange
        [0.8, 0.4, 0.2],     # Warm orange
        [0.05, 0.05, 0.05],  # Very dark
        [0.02, 0.01, 0.005], # Nearly black
        [2.0, 1.5, 0.5],     # HDR bright
        [5.0, 4.0, 3.0],     # Very bright
        [1.5, -0.2, 0.0],    # Negative green
        [1.0, 1.0, -0.5],    # Negative blue
        [-0.2, 0.5, 0.3],    # Negative red
    ])

    save_csv("aces20_test_rgb_input.csv", test_rgb)

    # ========================================================================
    # Section 5: ACES 2.0 Components (Pure Python float64)
    # ========================================================================
    print()
    print("=== Section 5: ACES 2.0 Components (float64) ===")

    print("ACES_RGB_to_JMh20 (AP1 primaries, float64)...")
    ap1 = JMhParams(0.713, 0.293, 0.165, 0.830, 0.128, 0.044, 0.32168, 0.33767)

    jmh_results = []
    for rgb in test_rgb:
        jmh = rgb_to_jmh20(float(rgb[0]), float(rgb[1]), float(rgb[2]), ap1)
        jmh_results.append(jmh)

    jmh_results = np.array(jmh_results)
    save_csv("aces_rgb_to_jmh20_output.csv", jmh_results)

    ap1_params = np.array([0.713, 0.293, 0.165, 0.830, 0.128, 0.044, 0.32168, 0.33767])
    save_csv("aces_rgb_to_jmh20_params.csv", ap1_params)

    print()
    print("=" * 70)
    print("Done! Generated Section 5 & 9 test reference data.")
    print("=" * 70)


if __name__ == "__main__":
    main()
