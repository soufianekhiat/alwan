#!/usr/bin/env python3
"""
Generate test reference data for Section 5 (ACES 2.0) and Section 9 (Transfer Functions)
using OpenColorIO as the authoritative reference.
"""

import PyOpenColorIO as ocio
import numpy as np
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

def apply_fixed_function(style, params, test_values):
    """Apply OCIO fixed function transform to test values."""
    config = ocio.Config.CreateRaw()
    if params:
        ff = ocio.FixedFunctionTransform(style, params)
    else:
        ff = ocio.FixedFunctionTransform(style)
    proc = config.getProcessor(ff)
    cpu = proc.getDefaultCPUProcessor()

    results = []
    for rgb in test_values:
        result = cpu.applyRGB(list(rgb))
        results.append(result)
    return np.array(results)

def apply_builtin(style, test_values, direction=ocio.TRANSFORM_DIR_FORWARD):
    """Apply OCIO builtin transform to test values."""
    config = ocio.Config.CreateRaw()
    bt = ocio.BuiltinTransform(style, direction=direction)
    proc = config.getProcessor(bt)
    cpu = proc.getDefaultCPUProcessor()

    results = []
    for rgb in test_values:
        result = cpu.applyRGB(list(rgb))
        results.append(result)
    return np.array(results)

print("=" * 70)
print("Generating Section 5 & 9 test reference data")
print("=" * 70)
print(f"OpenColorIO version: {ocio.__version__}")
print(f"Output directory: {OUTPUT_DIR}")
print()

os.makedirs(OUTPUT_DIR, exist_ok=True)

# ============================================================================
# Test values for transfer functions (1D - single channel applied to all RGB)
# ============================================================================
# Linear values for encoding tests
linear_test_values = np.array([
    0.0, 0.001, 0.005, 0.01, 0.02, 0.05, 0.1, 0.18, 0.5, 1.0,
    2.0, 5.0, 10.0, 12.0, 100.0
])

# Encoded values (0-1 range) for decoding tests
encoded_test_values = np.array([
    0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0
])

# Convert 1D to RGB triplets for OCIO
linear_rgb = np.column_stack([linear_test_values] * 3)
encoded_rgb = np.column_stack([encoded_test_values] * 3)

# ============================================================================
# Section 9: Transfer Functions
# ============================================================================
print()
print("=== Section 9: Transfer Functions ===")

# ----- Apple Log -----
print("Apple Log...")

# Apple Log to Linear (decoding)
result = apply_builtin('CURVE - APPLE_LOG_to_LINEAR', encoded_rgb)
save_csv("apple_log_decode_input.csv", encoded_test_values)
save_csv("apple_log_decode_output.csv", result[:, 0])  # Just R channel

# Linear to Apple Log (encoding) - inverse direction
result = apply_builtin('CURVE - APPLE_LOG_to_LINEAR', linear_rgb,
                       direction=ocio.TRANSFORM_DIR_INVERSE)
save_csv("apple_log_encode_input.csv", linear_test_values)
save_csv("apple_log_encode_output.csv", result[:, 0])  # Just R channel

# ----- DCDM Gamma 2.6 -----
print("DCDM (Gamma 2.6)...")

# XYZ to DCDM (encoding)
result = apply_builtin('DISPLAY - CIE-XYZ-D65_to_DCDM-D65', linear_rgb)
save_csv("dcdm_encode_input.csv", linear_test_values)
save_csv("dcdm_encode_output.csv", result[:, 0])  # Just R channel

# DCDM to XYZ (decoding) - inverse direction
result = apply_builtin('DISPLAY - CIE-XYZ-D65_to_DCDM-D65', encoded_rgb,
                       direction=ocio.TRANSFORM_DIR_INVERSE)
save_csv("dcdm_decode_input.csv", encoded_test_values)
save_csv("dcdm_decode_output.csv", result[:, 0])  # Just R channel

# ============================================================================
# Test RGB values for ACES 2.0 (comprehensive set)
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
    # Saturated colors
    [0.8, 0.1, 0.05],    # Dark saturated red
    [1.0, 0.2, 0.1],     # Bright saturated red
    [0.5, 0.0, 0.0],     # Medium red
    # Mixed colors
    [1.0, 0.5, 0.0],     # Orange
    [0.8, 0.4, 0.2],     # Warm orange
    # Low values
    [0.05, 0.05, 0.05],  # Very dark
    [0.02, 0.01, 0.005], # Nearly black
    # HDR values
    [2.0, 1.5, 0.5],     # HDR bright
    [5.0, 4.0, 3.0],     # Very bright
    # Out of gamut
    [1.5, -0.2, 0.0],    # Negative green
    [1.0, 1.0, -0.5],    # Negative blue
    [-0.2, 0.5, 0.3],    # Negative red
])

save_csv("aces20_test_rgb_input.csv", test_rgb)

# ============================================================================
# Section 5: ACES 2.0 Components
# ============================================================================
print()
print("=== Section 5: ACES 2.0 Components ===")

# NOTE: TonescaleCompress20 is generated by aces_tonescale_compress20.py
# which does the proper RGB -> JMh -> TonescaleCompress -> JMh -> RGB pipeline.
# The raw FIXED_FUNCTION_ACES_TONESCALE_COMPRESS_20 expects JMh input, not RGB,
# so we cannot use it directly on RGB values.

# ----- ACES RGB to JMh 20 -----
# AP1 primaries from ACES spec
ap1_params = [
    0.713, 0.293,      # red xy
    0.165, 0.830,      # green xy
    0.128, 0.044,      # blue xy
    0.32168, 0.33767   # D60 white xy
]

print("ACES_RGB_to_JMh20 (AP1 primaries)...")
try:
    result = apply_fixed_function(
        ocio.FIXED_FUNCTION_ACES_RGB_TO_JMH_20,
        ap1_params,
        test_rgb
    )
    save_csv("aces_rgb_to_jmh20_output.csv", result)
    save_csv("aces_rgb_to_jmh20_params.csv", np.array(ap1_params))
except Exception as e:
    print(f"  Error: {e}")

# ----- ACES GamutCompress20 -----
# Parameters: peak_luminance, then limiting primaries (rx, ry, gx, gy, bx, by, wx, wy)
# Using Rec.709 primaries with D65 white
gamut_compress_20_params = [
    1000.0,           # peak luminance (nits)
    0.64, 0.33,       # limiting red xy (Rec.709)
    0.30, 0.60,       # limiting green xy
    0.15, 0.06,       # limiting blue xy
    0.3127, 0.3290,   # limiting white xy (D65)
]

print("ACES_GamutCompress20...")
try:
    result = apply_fixed_function(
        ocio.FIXED_FUNCTION_ACES_GAMUT_COMPRESS_20,
        gamut_compress_20_params,
        test_rgb
    )
    save_csv("aces_gamut_compress20_output.csv", result)
    save_csv("aces_gamut_compress20_params.csv", np.array(gamut_compress_20_params))
except Exception as e:
    print(f"  Error: {e}")

# ----- ACES OutputTransform20 -----
# This requires many parameters - let's see what we can do
print("ACES_OutputTransform20...")
# OutputTransform20 parameters (from OCIO docs):
# Peak luminance, then limiting primaries (rx,ry,gx,gy,bx,by,wx,wy)
# Using Rec.709 primaries as limiting primaries with D65 white
ot20_params = [
    1000.0,           # peak luminance (nits)
    0.64, 0.33,       # limiting red xy (Rec.709)
    0.30, 0.60,       # limiting green xy
    0.15, 0.06,       # limiting blue xy
    0.3127, 0.3290,   # limiting white xy (D65)
]

try:
    result = apply_fixed_function(
        ocio.FIXED_FUNCTION_ACES_OUTPUT_TRANSFORM_20,
        ot20_params,
        test_rgb
    )
    save_csv("aces_output_transform20_output.csv", result)
    save_csv("aces_output_transform20_params.csv", np.array(ot20_params))
except Exception as e:
    print(f"  Needs different parameters: {e}")
    # Try to find correct parameter count
    for n in range(1, 20):
        try:
            test_params = [1000.0] + [0.5] * (n-1) if n > 0 else []
            result = apply_fixed_function(
                ocio.FIXED_FUNCTION_ACES_OUTPUT_TRANSFORM_20,
                test_params,
                test_rgb[:1]
            )
            print(f"  Works with {n} parameters!")
            break
        except:
            pass

print()
print("=" * 70)
print("Done! Generated Section 5 & 9 test reference data.")
print("=" * 70)
