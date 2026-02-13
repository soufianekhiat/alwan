#!/usr/bin/env python3
"""
Generate test reference data for ACES Fixed Functions.

Section 6 (ACES fixed functions): Pure Python float64 implementations matching
the ACES spec / OCIO FixedFunctionOpCPU.cpp algorithms. This avoids OCIO's
float32 internal processing which limits precision to ~7 significant digits.

Section 5 (ACES 2.0 components): Uses OpenColorIO as reference (float32 precision).
"""

import PyOpenColorIO as ocio
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

def apply_transform(style, params, test_rgb):
    """Apply OCIO fixed function transform to test RGB values."""
    config = ocio.Config.CreateRaw()
    if params:
        ff = ocio.FixedFunctionTransform(style, params)
    else:
        ff = ocio.FixedFunctionTransform(style)
    proc = config.getProcessor(ff)
    cpu = proc.getDefaultCPUProcessor()

    results = []
    for rgb in test_rgb:
        result = cpu.applyRGB(list(rgb))
        results.append(result)
    return np.array(results)


# ============================================================================
# Pure Python float64 ACES Fixed Function Implementations
# Reference: OCIO FixedFunctionOpCPU.cpp, alwan_aces_ff_core.h
# ============================================================================

NOISE_LIMIT = 1e-2

def calc_sat_weight(red, grn, blu):
    """Saturation weight calculation."""
    min_val = min(red, grn, blu)
    max_val = max(red, grn, blu)
    clamped_max = max(max_val, 1e-10)
    clamped_min = max(min_val, 1e-10)
    denom = max(max_val, NOISE_LIMIT)
    return (clamped_max - clamped_min) / denom

def calc_hue_weight(red, grn, blu, inv_width):
    """Hue weight calculation using B-spline."""
    sqrt3 = 1.7320508075688772
    a = 2.0 * red - (grn + blu)
    b = sqrt3 * (grn - blu)
    hue = math.atan2(b, a)

    knot_coord = hue * inv_width + 2.0
    j = int(knot_coord)

    # B-spline matrix coefficients
    M = [
        [ 0.25,  0.00,  0.00, 0.00],
        [-0.75,  0.75,  0.75, 0.25],
        [ 0.75, -1.50,  0.00, 1.00],
        [-0.25,  0.75, -0.75, 0.25],
    ]

    if 0 <= j < 4:
        t = knot_coord - j
        c0, c1, c2, c3 = M[j]
        return c3 + t * (c2 + t * (c1 + t * c0))
    return 0.0

def rgb_to_yc(red, grn, blu):
    """YC (luminance with chroma weighting) calculation."""
    YC_RADIUS_WEIGHT = 1.75
    chroma = math.sqrt(blu * (blu - grn) + grn * (grn - red) + red * (red - blu))
    return (blu + grn + red + YC_RADIUS_WEIGHT * chroma) / 3.0

def sigmoid_shaper(sat):
    """Sigmoid shaper for saturation."""
    x = (sat - 0.4) * 5.0
    sign = 1.0 if x >= 0.0 else -1.0
    t = 1.0 - 0.5 * sign * x
    if t < 0.0:
        t = 0.0
    return (1.0 + sign * (1.0 - t * t)) * 0.5


def redmod03_forward(red, grn, blu):
    """ACES RedMod03 forward. Reference: OCIO Renderer_ACES_RedMod03_Fwd."""
    SCALE = 0.85
    PIVOT = 0.03
    INV_WIDTH = 1.9098593171027443  # 6/pi

    f_H = calc_hue_weight(red, grn, blu, INV_WIDTH)
    if f_H > 0.0:
        f_S = calc_sat_weight(red, grn, blu)
        one_minus_scale = 1.0 - SCALE
        new_red = red + f_H * f_S * (PIVOT - red) * one_minus_scale

        # Preserve hue by adjusting green or blue
        delta_red = new_red - red
        if abs(delta_red) > 1e-5:
            if grn >= blu:
                denom = red - blu
                if denom > 1e-10:
                    hue_fac = (grn - blu) / denom
                    grn = hue_fac * (new_red - blu) + blu
            else:
                denom = red - grn
                if denom > 1e-10:
                    hue_fac = (blu - grn) / denom
                    blu = hue_fac * (new_red - grn) + grn
        red = new_red
    return [red, grn, blu]


def redmod10_forward(red, grn, blu):
    """ACES RedMod10 forward. Reference: OCIO Renderer_ACES_RedMod10_Fwd."""
    SCALE = 0.82
    PIVOT = 0.03
    INV_WIDTH = 1.6976527263135504  # 16/(3*pi)

    f_H = calc_hue_weight(red, grn, blu, INV_WIDTH)
    if f_H > 0.0:
        f_S = calc_sat_weight(red, grn, blu)
        one_minus_scale = 1.0 - SCALE
        red = red + f_H * f_S * (PIVOT - red) * one_minus_scale
    return [red, grn, blu]


def glow_forward(red, grn, blu, glow_gain_param, glow_mid_param):
    """ACES Glow forward. Reference: OCIO Renderer_ACES_Glow*_Fwd."""
    YC = rgb_to_yc(red, grn, blu)
    sat = calc_sat_weight(red, grn, blu)
    s = sigmoid_shaper(sat)

    glow_gain = glow_gain_param * s
    glow_mid = glow_mid_param

    if YC >= glow_mid * 2.0:
        glow_gain_out = 0.0
    elif YC <= glow_mid * 2.0 / 3.0:
        glow_gain_out = glow_gain
    else:
        glow_gain_out = glow_gain * (glow_mid / YC - 0.5)

    added_glow = 1.0 + glow_gain_out
    return [red * added_glow, grn * added_glow, blu * added_glow]


def glow03_forward(red, grn, blu):
    return glow_forward(red, grn, blu, 0.075, 0.1)


def glow10_forward(red, grn, blu):
    return glow_forward(red, grn, blu, 0.05, 0.08)


def dark_to_dim10_forward(red, grn, blu):
    """ACES DarkToDim10 forward. Reference: OCIO Renderer_ACES_DarkToDim10_Fwd."""
    GAMMA = -0.0189  # 0.9811 - 1.0
    MIN_LUM = 1e-10

    # ACEScg luminance coefficients (AP1 Y row)
    Y_r = 0.27222871678091454
    Y_g = 0.67408176581114831
    Y_b = 0.053689517407937051

    Y = Y_r * red + Y_g * grn + Y_b * blu
    if Y < MIN_LUM:
        Y = MIN_LUM

    Ypow_over_Y = Y ** GAMMA
    return [red * Ypow_over_Y, grn * Ypow_over_Y, blu * Ypow_over_Y]


def rec2100_surround_forward(red, grn, blu, gamma):
    """Rec2100 Surround. Reference: OCIO Renderer_REC2100_Surround."""
    MIN_LUM = 1e-4

    # BT.2020/2100 luminance coefficients
    Y_r = 0.2627
    Y_g = 0.6780
    Y_b = 0.0593

    Y = Y_r * red + Y_g * grn + Y_b * blu
    Y = abs(Y)
    if Y < MIN_LUM:
        Y = MIN_LUM

    m_gamma = gamma - 1.0
    Ypow_over_Y = Y ** m_gamma
    return [red * Ypow_over_Y, grn * Ypow_over_Y, blu * Ypow_over_Y]


def calc_gamut_comp_scale(lim, thr, power):
    """Compute scale from limit, threshold and power."""
    base = (1.0 - thr) / (lim - thr)
    inner = base ** (-power) - 1.0
    denom = inner ** (1.0 / power)
    return (lim - thr) / denom


def compress_dist(dist, thr, scale, power):
    """Compression function (forward)."""
    nd = (dist - thr) / scale
    p = nd ** power
    return thr + scale * nd / (1.0 + p) ** (1.0 / power)


def gamut_comp_channel(val, ach, thr, scale, power):
    """Per-channel gamut compression."""
    if ach == 0.0:
        return 0.0
    dist = (ach - val) / abs(ach)
    if dist < thr:
        return val
    compr_dist = compress_dist(dist, thr, scale, power)
    return ach - compr_dist * abs(ach)


def gamut_comp13_forward(red, grn, blu, params):
    """ACES GamutComp13 forward. Reference: OCIO Renderer_ACES_GamutComp13_Fwd."""
    lim_c, lim_m, lim_y, thr_c, thr_m, thr_y, power = params

    scale_c = calc_gamut_comp_scale(lim_c, thr_c, power)
    scale_m = calc_gamut_comp_scale(lim_m, thr_m, power)
    scale_y = calc_gamut_comp_scale(lim_y, thr_y, power)

    ach = max(red, grn, blu)

    out_r = gamut_comp_channel(red, ach, thr_c, scale_c, power)
    out_g = gamut_comp_channel(grn, ach, thr_m, scale_m, power)
    out_b = gamut_comp_channel(blu, ach, thr_y, scale_y, power)

    return [out_r, out_g, out_b]


def apply_py_func(func, test_rgb):
    """Apply a Python function to all test RGB values."""
    results = []
    for rgb in test_rgb:
        r, g, b = float(rgb[0]), float(rgb[1]), float(rgb[2])
        results.append(func(r, g, b))
    return np.array(results)


print("Generating ACES Fixed Function test reference data...")
print(f"OpenColorIO version: {ocio.__version__}")
print(f"Output directory: {OUTPUT_DIR}")
print()

# ============================================================================
# Test RGB values - comprehensive set for ACES testing
# ============================================================================
test_rgb = np.array([
    # Primary colors
    [1.0, 0.0, 0.0],     # Pure red
    [0.0, 1.0, 0.0],     # Pure green
    [0.0, 0.0, 1.0],     # Pure blue
    # Secondary colors
    [1.0, 1.0, 0.0],     # Yellow
    [0.0, 1.0, 1.0],     # Cyan
    [1.0, 0.0, 1.0],     # Magenta
    # Grayscale
    [0.0, 0.0, 0.0],     # Black
    [0.18, 0.18, 0.18],  # 18% gray
    [0.5, 0.5, 0.5],     # 50% gray
    [1.0, 1.0, 1.0],     # White
    # Saturated reds (for RedMod testing)
    [0.8, 0.1, 0.05],    # Dark saturated red
    [1.0, 0.2, 0.1],     # Bright saturated red
    [0.5, 0.0, 0.0],     # Medium red
    # Orange/warm colors
    [1.0, 0.5, 0.0],     # Orange
    [0.8, 0.4, 0.2],     # Warm orange
    # Low values (for Glow testing)
    [0.05, 0.05, 0.05],  # Very dark
    [0.02, 0.01, 0.005], # Nearly black
    # HDR values
    [2.0, 1.5, 0.5],     # HDR bright
    [5.0, 4.0, 3.0],     # Very bright
    # Out of gamut (for GamutComp13)
    [1.5, -0.2, 0.0],    # Negative green
    [1.0, 1.0, -0.5],    # Negative blue
    [-0.2, 0.5, 0.3],    # Negative red
])

os.makedirs(OUTPUT_DIR, exist_ok=True)
save_csv("aces_ff_test_rgb_input.csv", test_rgb)

# ============================================================================
# Section 6: ACES Fixed Functions (float64 pure Python)
# ============================================================================
print()
print("=== Section 6: ACES Fixed Functions (float64) ===")

# ACES RedMod03
print("ACES_RedMod03 (float64)...")
result = apply_py_func(redmod03_forward, test_rgb)
save_csv("aces_redmod03_output.csv", result)

# ACES RedMod10
print("ACES_RedMod10 (float64)...")
result = apply_py_func(redmod10_forward, test_rgb)
save_csv("aces_redmod10_output.csv", result)

# ACES Glow03
print("ACES_Glow03 (float64)...")
result = apply_py_func(glow03_forward, test_rgb)
save_csv("aces_glow03_output.csv", result)

# ACES Glow10
print("ACES_Glow10 (float64)...")
result = apply_py_func(glow10_forward, test_rgb)
save_csv("aces_glow10_output.csv", result)

# ACES DarkToDim10
print("ACES_DarkToDim10 (float64)...")
result = apply_py_func(dark_to_dim10_forward, test_rgb)
save_csv("aces_dark_to_dim10_output.csv", result)

# ACES GamutComp13 (requires 7 parameters)
gamut_comp_13_params = [1.147, 1.264, 1.312, 0.815, 0.803, 0.880, 1.2]
print("ACES_GamutComp13 (float64)...")
result = apply_py_func(lambda r, g, b: gamut_comp13_forward(r, g, b, gamut_comp_13_params), test_rgb)
save_csv("aces_gamut_comp13_output.csv", result)
save_csv("aces_gamut_comp13_params.csv", np.array(gamut_comp_13_params))

# Rec2100 Surround (requires 1 parameter: gamma)
rec2100_gamma = 0.78
print(f"Rec2100_Surround (gamma={rec2100_gamma}, float64)...")
result = apply_py_func(lambda r, g, b: rec2100_surround_forward(r, g, b, rec2100_gamma), test_rgb)
save_csv("rec2100_surround_output.csv", result)
save_csv("rec2100_surround_gamma.csv", np.array([rec2100_gamma]))

# ============================================================================
# Section 5: ACES 2.0 Components (still uses OCIO - float32 precision)
# ============================================================================
print()
print("=== Section 5: ACES 2.0 Components ===")

# ACES Tonescale Compress 20 (requires peak luminance parameter)
peak_luminance = 1000.0  # 1000 nits
print(f"ACES_TonescaleCompress20 (peak={peak_luminance} nits)...")
result = apply_transform(ocio.FIXED_FUNCTION_ACES_TONESCALE_COMPRESS_20, [peak_luminance], test_rgb)
save_csv("aces_tonescale_compress20_output.csv", result)
save_csv("aces_tonescale_compress20_peak.csv", np.array([peak_luminance]))

# ACES RGB to JMh 20 (requires 8 encoding primaries parameters)
ap1_params = [
    0.713, 0.293,   # red xy
    0.165, 0.830,   # green xy
    0.128, 0.044,   # blue xy
    0.32168, 0.33767  # D60 white xy
]
print("ACES_RGB_to_JMh20 (AP1 primaries)...")
try:
    result = apply_transform(ocio.FIXED_FUNCTION_ACES_RGB_TO_JMH_20, ap1_params, test_rgb)
    save_csv("aces_rgb_to_jmh20_output.csv", result)
    save_csv("aces_rgb_to_jmh20_params.csv", np.array(ap1_params))
except Exception as e:
    print(f"  Error: {e}")

# ACES Gamut Compress 20 (requires 9 parameters)
gamut_compress_20_params = [
    0.89, 0.83, 0.94, 0.96, 0.94, 0.98, 1.20, 1.59, 0.42,
]
print("ACES_GamutCompress20...")
try:
    result = apply_transform(ocio.FIXED_FUNCTION_ACES_GAMUT_COMPRESS_20, gamut_compress_20_params, test_rgb)
    save_csv("aces_gamut_compress20_output.csv", result)
    save_csv("aces_gamut_compress20_params.csv", np.array(gamut_compress_20_params))
except Exception as e:
    print(f"  Error: {e}")

# ACES Output Transform 20 (complex - check parameters needed)
print("ACES_OutputTransform20...")
try:
    result = apply_transform(ocio.FIXED_FUNCTION_ACES_OUTPUT_TRANSFORM_20, [], test_rgb)
    save_csv("aces_output_transform20_output.csv", result)
except Exception as e:
    print(f"  Needs parameters: {e}")

print()
print("Done! Generated ACES fixed function test reference data.")
