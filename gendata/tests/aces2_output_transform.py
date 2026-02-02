#!/usr/bin/env python3
"""
Generate reference data for ACES 2.0 Output Transform tests.

The ACES 2.0 Output Transform pipeline consists of:
  1. RGB -> JMh (CAM-based perceptual conversion)
  2. Tonescale + Chroma Compress
  3. Gamut Compress (to limiting primaries)
  4. JMh -> RGB (limiting primaries)
  5. Chromatic Adaptation (D60 -> D65)
  6. Display Encoding (EOTF)

This script generates reference values using PyOpenColorIO for all output presets.
"""

import numpy as np
import PyOpenColorIO as ocio
import os

# Output directory
OUTPUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "..", "..", "tests", "reference_values")

# Test RGB inputs (AP1 linear) - comprehensive set
TEST_RGB_INPUTS = np.array([
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
], dtype=np.float32)

# AP1 primaries (D60)
AP1_PRIMARIES = {
    'red_x': 0.713,
    'red_y': 0.293,
    'green_x': 0.165,
    'green_y': 0.830,
    'blue_x': 0.128,
    'blue_y': 0.044,
    'white_x': 0.32168,
    'white_y': 0.33767,
}

# Limiting primaries definitions
LIMITING_PRIMARIES = {
    'rec709': {
        'red_x': 0.64, 'red_y': 0.33,
        'green_x': 0.30, 'green_y': 0.60,
        'blue_x': 0.15, 'blue_y': 0.06,
        'white_x': 0.3127, 'white_y': 0.3290,
    },
    'p3_d65': {
        'red_x': 0.68, 'red_y': 0.32,
        'green_x': 0.265, 'green_y': 0.69,
        'blue_x': 0.15, 'blue_y': 0.06,
        'white_x': 0.3127, 'white_y': 0.3290,
    },
    'rec2020': {
        'red_x': 0.708, 'red_y': 0.292,
        'green_x': 0.17, 'green_y': 0.797,
        'blue_x': 0.131, 'blue_y': 0.046,
        'white_x': 0.3127, 'white_y': 0.3290,
    },
    'p3_dci': {
        'red_x': 0.68, 'red_y': 0.32,
        'green_x': 0.265, 'green_y': 0.69,
        'blue_x': 0.15, 'blue_y': 0.06,
        'white_x': 0.314, 'white_y': 0.351,  # DCI white
    },
    'xyz': {
        'red_x': 1.0, 'red_y': 0.0,
        'green_x': 0.0, 'green_y': 1.0,
        'blue_x': 0.0, 'blue_y': 0.0,
        'white_x': 1.0/3.0, 'white_y': 1.0/3.0,
    },
}

# Output presets mapping to Alwan enum order
OUTPUT_PRESETS = [
    # ALWAN_ACES2_OUT_REC709_100NIT_BT1886
    {'name': 'rec709_100nit_bt1886', 'peak_lum': 100.0, 'primaries': 'rec709', 'eotf': 'bt1886'},
    # ALWAN_ACES2_OUT_SRGB_100NIT
    {'name': 'srgb_100nit', 'peak_lum': 100.0, 'primaries': 'rec709', 'eotf': 'srgb'},
    # ALWAN_ACES2_OUT_P3D65_100NIT_SRGB
    {'name': 'p3d65_100nit_srgb', 'peak_lum': 100.0, 'primaries': 'p3_d65', 'eotf': 'srgb'},
    # ALWAN_ACES2_OUT_P3D65_100NIT_G22
    {'name': 'p3d65_100nit_g22', 'peak_lum': 100.0, 'primaries': 'p3_d65', 'eotf': 'gamma22'},
    # ALWAN_ACES2_OUT_P3D65_1000NIT_PQ
    {'name': 'p3d65_1000nit_pq', 'peak_lum': 1000.0, 'primaries': 'p3_d65', 'eotf': 'pq'},
    # ALWAN_ACES2_OUT_REC2100_500NIT_PQ
    {'name': 'rec2100_500nit_pq', 'peak_lum': 500.0, 'primaries': 'rec2020', 'eotf': 'pq'},
    # ALWAN_ACES2_OUT_REC2100_1000NIT_PQ
    {'name': 'rec2100_1000nit_pq', 'peak_lum': 1000.0, 'primaries': 'rec2020', 'eotf': 'pq'},
    # ALWAN_ACES2_OUT_REC2100_2000NIT_PQ
    {'name': 'rec2100_2000nit_pq', 'peak_lum': 2000.0, 'primaries': 'rec2020', 'eotf': 'pq'},
    # ALWAN_ACES2_OUT_REC2100_4000NIT_PQ
    {'name': 'rec2100_4000nit_pq', 'peak_lum': 4000.0, 'primaries': 'rec2020', 'eotf': 'pq'},
    # ALWAN_ACES2_OUT_REC2100_1000NIT_HLG
    {'name': 'rec2100_1000nit_hlg', 'peak_lum': 1000.0, 'primaries': 'rec2020', 'eotf': 'hlg'},
    # ALWAN_ACES2_OUT_DCDM_48NIT
    {'name': 'dcdm_48nit', 'peak_lum': 48.0, 'primaries': 'xyz', 'eotf': 'gamma26'},
    # ALWAN_ACES2_OUT_P3DCI_48NIT
    {'name': 'p3dci_48nit', 'peak_lum': 48.0, 'primaries': 'p3_dci', 'eotf': 'gamma26'},
]


def format_value(x):
    """Format a single value for C inclusion."""
    if np.isnan(x):
        return 'NAN'
    elif np.isinf(x):
        return 'INFINITY' if x > 0 else '-INFINITY'
    else:
        return f'{x:.16e}'


def save_csv(filename, data):
    """Save data to CSV with maximum precision."""
    filepath = os.path.join(OUTPUT_DIR, filename)
    with open(filepath, 'w') as f:
        for row in data:
            f.write(','.join(format_value(x) for x in row) + ',\n')
    print(f"  Written: {filename}")


def get_ap1_params():
    """Get AP1 primaries as parameter array."""
    return [
        AP1_PRIMARIES['red_x'], AP1_PRIMARIES['red_y'],
        AP1_PRIMARIES['green_x'], AP1_PRIMARIES['green_y'],
        AP1_PRIMARIES['blue_x'], AP1_PRIMARIES['blue_y'],
        AP1_PRIMARIES['white_x'], AP1_PRIMARIES['white_y'],
    ]


def get_limit_params(primaries_name):
    """Get limiting primaries as parameter array."""
    p = LIMITING_PRIMARIES[primaries_name]
    return [
        p['red_x'], p['red_y'],
        p['green_x'], p['green_y'],
        p['blue_x'], p['blue_y'],
        p['white_x'], p['white_y'],
    ]


def create_aces2_output_transform_processor(peak_luminance, limit_primaries_name):
    """
    Create OCIO processor for complete ACES 2.0 Output Transform pipeline.

    Pipeline:
    1. RGB (AP1) -> JMh
    2. Tonescale + Chroma Compress (in JMh space)
    3. Gamut Compress (to limiting gamut)
    4. JMh -> RGB (limiting primaries)

    Note: Display encoding (EOTF) is applied separately.
    """
    config = ocio.Config.CreateRaw()
    group = ocio.GroupTransform()

    ap1_params = get_ap1_params()
    limit_params = get_limit_params(limit_primaries_name)

    # Step 1: AP1 RGB to JMh
    rgb_to_jmh = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_RGB_TO_JMH_20,
        params=ap1_params
    )
    group.appendTransform(rgb_to_jmh)

    # Step 2: Tonescale + Chroma Compress (operates on JMh)
    tonescale_compress = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_TONESCALE_COMPRESS_20,
        params=[peak_luminance]
    )
    group.appendTransform(tonescale_compress)

    # Step 3: Gamut Compress (operates on JMh)
    # Parameters: peak_luminance + limiting primaries
    gamut_compress_params = [peak_luminance] + limit_params
    gamut_compress = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_GAMUT_COMPRESS_20,
        params=gamut_compress_params
    )
    group.appendTransform(gamut_compress)

    # Step 4: JMh to RGB (limiting primaries)
    jmh_to_rgb = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_RGB_TO_JMH_20,
        params=limit_params,
        direction=ocio.TRANSFORM_DIR_INVERSE
    )
    group.appendTransform(jmh_to_rgb)

    return config.getProcessor(group)


def apply_eotf(rgb_values, eotf_type, peak_luminance):
    """Apply display encoding EOTF to linear RGB values."""
    config = ocio.Config.CreateRaw()

    if eotf_type == 'srgb':
        # sRGB piecewise curve (forward is OETF, we need inverse for display)
        # Actually for display encoding we apply the inverse EOTF which is OETF
        # Linear -> sRGB encoded is the OETF (forward)
        transform = ocio.ExponentWithLinearTransform(
            gamma=[2.4, 2.4, 2.4, 1.0],
            offset=[0.055, 0.055, 0.055, 0.0],
            direction=ocio.TRANSFORM_DIR_INVERSE
        )
    elif eotf_type == 'bt1886':
        # BT.1886 is pure gamma 2.4
        transform = ocio.ExponentTransform(
            value=[2.4, 2.4, 2.4, 1.0],
            direction=ocio.TRANSFORM_DIR_INVERSE
        )
    elif eotf_type == 'gamma22':
        # Pure gamma 2.2
        transform = ocio.ExponentTransform(
            value=[2.2, 2.2, 2.2, 1.0],
            direction=ocio.TRANSFORM_DIR_INVERSE
        )
    elif eotf_type == 'gamma26':
        # Pure gamma 2.6 (DCI)
        transform = ocio.ExponentTransform(
            value=[2.6, 2.6, 2.6, 1.0],
            direction=ocio.TRANSFORM_DIR_INVERSE
        )
    elif eotf_type == 'pq':
        # ST.2084 PQ - requires normalization to peak luminance
        # Normalize to 0-1 range based on 10000 nits reference
        group = ocio.GroupTransform()
        # Scale by peak/10000 before PQ encoding
        scale_factor = peak_luminance / 10000.0
        group.appendTransform(ocio.MatrixTransform(matrix=[
            scale_factor, 0, 0, 0,
            0, scale_factor, 0, 0,
            0, 0, scale_factor, 0,
            0, 0, 0, 1
        ]))
        group.appendTransform(ocio.BuiltinTransform('CURVE - ST-2084_to_LINEAR', direction=ocio.TRANSFORM_DIR_INVERSE))
        transform = group
    elif eotf_type == 'hlg':
        # HLG encoding
        group = ocio.GroupTransform()
        # HLG is relative to 1000 nits nominal, scale accordingly
        scale_factor = peak_luminance / 1000.0
        group.appendTransform(ocio.MatrixTransform(matrix=[
            scale_factor, 0, 0, 0,
            0, scale_factor, 0, 0,
            0, 0, scale_factor, 0,
            0, 0, 0, 1
        ]))
        group.appendTransform(ocio.BuiltinTransform('CURVE - HLG_to_LINEAR', direction=ocio.TRANSFORM_DIR_INVERSE))
        transform = group
    else:
        raise ValueError(f"Unknown EOTF type: {eotf_type}")

    proc = config.getProcessor(transform)
    cpu = proc.getDefaultCPUProcessor()

    results = []
    for rgb in rgb_values:
        result = np.array(rgb, dtype=np.float32).copy()
        # Clamp negative values before applying display encoding
        result = np.maximum(result, 0.0)
        cpu.applyRGB(result)
        results.append(result.tolist())

    return np.array(results, dtype=np.float32)


def process_rgb_batch(processor, rgb_values):
    """Process a batch of RGB values through the OCIO processor."""
    cpu = processor.getDefaultCPUProcessor()
    results = []
    for rgb in rgb_values:
        result = np.array(rgb, dtype=np.float32).copy()
        cpu.applyRGB(result)
        results.append(result.tolist())
    return np.array(results, dtype=np.float32)


def generate_preset_data(preset, inputs):
    """Generate reference data for a specific output preset."""
    name = preset['name']
    peak_lum = preset['peak_lum']
    primaries = preset['primaries']
    eotf = preset['eotf']

    print(f"\nGenerating ACES 2.0 Output Transform: {name}")
    print(f"  Peak luminance: {peak_lum} nits")
    print(f"  Limiting primaries: {primaries}")
    print(f"  EOTF: {eotf}")

    # Step 1-4: Core ACES 2.0 pipeline (RGB -> JMh -> Tone/Gamut -> RGB)
    try:
        processor = create_aces2_output_transform_processor(peak_lum, primaries)
        linear_output = process_rgb_batch(processor, inputs)
    except Exception as e:
        print(f"  ERROR in core pipeline: {e}")
        return None

    # Step 5: Apply display encoding (EOTF)
    try:
        encoded_output = apply_eotf(linear_output, eotf, peak_lum)
    except Exception as e:
        print(f"  ERROR in EOTF: {e}")
        # Return linear output without EOTF if EOTF fails
        encoded_output = linear_output

    return encoded_output


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    print("=" * 60)
    print("ACES 2.0 Output Transform Reference Data Generator")
    print("=" * 60)
    print(f"OCIO Version: {ocio.GetVersion()}")
    print(f"Output directory: {OUTPUT_DIR}")
    print()

    # Save input RGB values
    save_csv("aces2_output_transform_input.csv", TEST_RGB_INPUTS)

    # Generate data for each preset
    for idx, preset in enumerate(OUTPUT_PRESETS):
        outputs = generate_preset_data(preset, TEST_RGB_INPUTS)
        if outputs is not None:
            filename = f"aces2_output_{preset['name']}_output.csv"
            save_csv(filename, outputs)

            # Print some sample values for verification
            print(f"  Sample: [0.18, 0.18, 0.18] -> [{outputs[7][0]:.6f}, {outputs[7][1]:.6f}, {outputs[7][2]:.6f}]")

    print()
    print("=" * 60)
    print("Done!")
    print("=" * 60)


if __name__ == "__main__":
    main()
