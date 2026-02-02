#!/usr/bin/env python3
"""
Generate test reference data for ACES Fixed Functions using OpenColorIO.
Uses PyOpenColorIO as the authoritative reference.
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

save_csv("aces_ff_test_rgb_input.csv", test_rgb)

# ============================================================================
# Section 6: ACES Fixed Functions (RRT Components)
# ============================================================================
print()
print("=== Section 6: ACES Fixed Functions ===")

# ACES RedMod03
print("ACES_RedMod03...")
result = apply_transform(ocio.FIXED_FUNCTION_ACES_RED_MOD_03, [], test_rgb)
save_csv("aces_redmod03_output.csv", result)

# ACES RedMod10
print("ACES_RedMod10...")
result = apply_transform(ocio.FIXED_FUNCTION_ACES_RED_MOD_10, [], test_rgb)
save_csv("aces_redmod10_output.csv", result)

# ACES Glow03
print("ACES_Glow03...")
result = apply_transform(ocio.FIXED_FUNCTION_ACES_GLOW_03, [], test_rgb)
save_csv("aces_glow03_output.csv", result)

# ACES Glow10
print("ACES_Glow10...")
result = apply_transform(ocio.FIXED_FUNCTION_ACES_GLOW_10, [], test_rgb)
save_csv("aces_glow10_output.csv", result)

# ACES DarkToDim10
print("ACES_DarkToDim10...")
result = apply_transform(ocio.FIXED_FUNCTION_ACES_DARK_TO_DIM_10, [], test_rgb)
save_csv("aces_dark_to_dim10_output.csv", result)

# ACES GamutComp13 (requires 7 parameters)
# Default ACES 1.3 parameters: limCyan, limMagenta, limYellow, thrCyan, thrMagenta, thrYellow, power
gamut_comp_13_params = [1.147, 1.264, 1.312, 0.815, 0.803, 0.880, 1.2]
print("ACES_GamutComp13...")
result = apply_transform(ocio.FIXED_FUNCTION_ACES_GAMUT_COMP_13, gamut_comp_13_params, test_rgb)
save_csv("aces_gamut_comp13_output.csv", result)
save_csv("aces_gamut_comp13_params.csv", np.array(gamut_comp_13_params))

# Rec2100 Surround (requires 1 parameter: gamma)
# Standard gamma for dim surround
rec2100_gamma = 0.78
print("Rec2100_Surround (gamma=0.78)...")
result = apply_transform(ocio.FIXED_FUNCTION_REC2100_SURROUND, [rec2100_gamma], test_rgb)
save_csv("rec2100_surround_output.csv", result)
save_csv("rec2100_surround_gamma.csv", np.array([rec2100_gamma]))

# ============================================================================
# Section 5: ACES 2.0 Components
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
# For AP1 primaries: red_xy, green_xy, blue_xy, white_xy
# AP1 primaries from ACES spec
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
# These are the chroma compression parameters
# Default ACES 2.0 parameters (from OCIO source)
gamut_compress_20_params = [
    0.89,   # limitCyan
    0.83,   # limitMagenta
    0.94,   # limitYellow
    0.96,   # threshCyan
    0.94,   # threshMagenta
    0.98,   # threshYellow
    1.20,   # cuspMidBlend
    1.59,   # focusDist
    0.42,   # focusGainBlend
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
    # Try without parameters first
    result = apply_transform(ocio.FIXED_FUNCTION_ACES_OUTPUT_TRANSFORM_20, [], test_rgb)
    save_csv("aces_output_transform20_output.csv", result)
except Exception as e:
    print(f"  Needs parameters: {e}")
    # Try with common parameters
    # ACES 2.0 OT params: peak_luminance, mid_gray, min_luminance, limiting_primaries...
    # This is complex - may need to check OCIO docs

print()
print("Done! Generated ACES fixed function test reference data.")
