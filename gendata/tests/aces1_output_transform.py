#!/usr/bin/env python3
"""
Generate reference data for ACES 1.x Output Transform tests.

ACES 1.x (1.0, 1.0.3, 1.1, 1.2, 1.3) uses a combined RRT+ODT pipeline:
  - Reference Rendering Transform (RRT)
  - Output Device Transform (ODT)

In OCIO, these are implemented as BuiltinTransforms that combine both steps.
This script generates reference values using PyOpenColorIO for all output presets.
"""

import numpy as np
import PyOpenColorIO as ocio
import os

# Output directory
OUTPUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "..", "..", "tests", "reference_values")

# Test RGB inputs (AP0 linear / ACES2065-1) - comprehensive set
# Note: ACES 1.x RRT/ODT expects input in AP0 (ACES2065-1)
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

# ACES 1.x Output Transform presets mapping to Alwan enum order
# These use OCIO BuiltinTransforms which combine RRT+ODT
# Input is expected in ACES2065-1 (AP0)
OUTPUT_PRESETS = [
    # ALWAN_ACES1_OUT_REC709_100NIT - Rec.709 100 nits (D65)
    {'name': 'rec709_100nit', 'builtin': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0'},
    # ALWAN_ACES1_OUT_SRGB_100NIT - sRGB 100 nits (D65)
    {'name': 'srgb_100nit', 'builtin': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0'},
    # ALWAN_ACES1_OUT_SRGB_D60_100NIT - sRGB 100 nits (D60)
    {'name': 'srgb_d60_100nit', 'builtin': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0'},
    # ALWAN_ACES1_OUT_P3DCI_48NIT - P3-DCI 48 nits
    {'name': 'p3dci_48nit', 'builtin': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-CINEMA_1.0'},
    # ALWAN_ACES1_OUT_P3D60_48NIT - P3-D60 48 nits
    {'name': 'p3d60_48nit', 'builtin': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-CINEMA_1.0'},
    # ALWAN_ACES1_OUT_P3D65_48NIT - P3-D65 48 nits
    {'name': 'p3d65_48nit', 'builtin': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-CINEMA_1.0'},
    # ALWAN_ACES1_OUT_P3D65_100NIT - P3-D65 100 nits
    {'name': 'p3d65_100nit', 'builtin': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0'},
    # ALWAN_ACES1_OUT_REC2020_100NIT - Rec.2020 100 nits
    {'name': 'rec2020_100nit', 'builtin': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0'},
    # ALWAN_ACES1_OUT_REC2020_1000NIT_PQ - Rec.2020 1000 nits PQ
    {'name': 'rec2020_1000nit_pq', 'builtin': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - HDR-VIDEO-1000nit-15nit-P3lim_1.1'},
    # ALWAN_ACES1_OUT_REC2020_2000NIT_PQ - Rec.2020 2000 nits PQ
    {'name': 'rec2020_2000nit_pq', 'builtin': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - HDR-VIDEO-2000nit-15nit-P3lim_1.1'},
    # ALWAN_ACES1_OUT_REC2020_4000NIT_PQ - Rec.2020 4000 nits PQ
    {'name': 'rec2020_4000nit_pq', 'builtin': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - HDR-VIDEO-4000nit-15nit-P3lim_1.1'},
    # ALWAN_ACES1_OUT_DCDM_48NIT - DCDM 48 nits
    {'name': 'dcdm_48nit', 'builtin': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-CINEMA_1.0'},
]

# Alternative: Direct BuiltinTransform names for specific ODTs
# These transform from ACES2065-1 to specific displays
DIRECT_ODTS = {
    'rec709_100nit': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0',
    'srgb_100nit': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0',
    'p3dci_48nit': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-CINEMA_1.0',
    'p3d65_1000nit_pq': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - HDR-VIDEO-1000nit-15nit-P3lim_1.1',
    'rec2020_1000nit_pq': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - HDR-VIDEO-1000nit-15nit-P3lim_1.1',
}


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


def list_builtin_transforms():
    """List all available OCIO BuiltinTransforms."""
    print("\nAvailable ACES BuiltinTransforms:")
    registry = ocio.BuiltinTransformRegistry()
    for transform in registry:
        name = transform
        if 'ACES' in name:
            print(f"  {name}")


def create_aces1_processor_direct(output_name):
    """
    Create OCIO processor for ACES 1.x Output Transform using direct BuiltinTransform.

    The BuiltinTransforms combine RRT + specific ODT in a single transform.
    Input: ACES2065-1 (AP0 linear)
    Output: Display-encoded values
    """
    config = ocio.Config.CreateRaw()

    # Map preset names to OCIO BuiltinTransform names
    # These are the actual transforms available in OCIO
    builtin_map = {
        'rec709_100nit': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0',
        'srgb_100nit': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0',
        'srgb_d60_100nit': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0',
        'p3dci_48nit': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-CINEMA_1.0',
        'p3d60_48nit': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-CINEMA_1.0',
        'p3d65_48nit': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-CINEMA_1.0',
        'p3d65_100nit': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0',
        'rec2020_100nit': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-VIDEO_1.0',
        'rec2020_1000nit_pq': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - HDR-VIDEO-1000nit-15nit-P3lim_1.1',
        'rec2020_2000nit_pq': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - HDR-VIDEO-2000nit-15nit-P3lim_1.1',
        'rec2020_4000nit_pq': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - HDR-VIDEO-4000nit-15nit-P3lim_1.1',
        'dcdm_48nit': 'ACES-OUTPUT - ACES2065-1_to_CIE-XYZ-D65 - SDR-CINEMA_1.0',
    }

    builtin_name = builtin_map.get(output_name)
    if not builtin_name:
        raise ValueError(f"Unknown output preset: {output_name}")

    try:
        transform = ocio.BuiltinTransform(builtin_name)
        return config.getProcessor(transform)
    except Exception as e:
        print(f"  Warning: Could not create BuiltinTransform '{builtin_name}': {e}")
        return None


def create_aces1_processor_via_config(output_name):
    """
    Create OCIO processor using the ACES Studio Config.

    This uses the official ACES OCIO config to access the transforms.
    """
    try:
        # Try to load the ACES Studio config
        config = ocio.Config.CreateFromBuiltinConfig("studio-config-v2.1.0_aces-v1.3_ocio-v2.3")
    except Exception as e:
        print(f"  Warning: Could not load ACES config: {e}")
        return None

    # Map preset names to OCIO color space names
    colorspace_map = {
        'rec709_100nit': 'Rec.709 - Display',
        'srgb_100nit': 'sRGB - Display',
        'srgb_d60_100nit': 'sRGB - Display',
        'p3dci_48nit': 'P3-DCI - Display',
        'p3d60_48nit': 'P3-D60 - Display',
        'p3d65_48nit': 'P3-D65 - Display',
        'p3d65_100nit': 'P3-D65 - Display',
        'rec2020_100nit': 'Rec.2020 - Display',
        'rec2020_1000nit_pq': 'Rec.2100-PQ - Display',
        'rec2020_2000nit_pq': 'Rec.2100-PQ - Display',
        'rec2020_4000nit_pq': 'Rec.2100-PQ - Display',
        'dcdm_48nit': 'DCDM - Display',
    }

    colorspace = colorspace_map.get(output_name)
    if not colorspace:
        return None

    try:
        return config.getProcessor('ACES2065-1', colorspace)
    except Exception as e:
        print(f"  Warning: Could not get processor for '{colorspace}': {e}")
        return None


def process_rgb_batch(processor, rgb_values):
    """Process a batch of RGB values through the OCIO processor."""
    if processor is None:
        return None

    cpu = processor.getDefaultCPUProcessor()
    results = []
    for rgb in rgb_values:
        result = np.array(rgb, dtype=np.float32).copy()
        try:
            cpu.applyRGB(result)
        except Exception as e:
            # Handle errors gracefully - return NaN
            result = np.array([np.nan, np.nan, np.nan], dtype=np.float32)
        results.append(result.tolist())
    return np.array(results, dtype=np.float32)


def generate_using_builtin_config():
    """Generate reference data using OCIO builtin ACES config."""
    print("\nUsing OCIO Builtin ACES Config...")

    try:
        # Load the builtin ACES config
        config = ocio.Config.CreateFromBuiltinConfig("studio-config-v2.1.0_aces-v1.3_ocio-v2.3")
        print(f"  Loaded config: studio-config-v2.1.0_aces-v1.3_ocio-v2.3")
    except Exception as e:
        print(f"  Could not load builtin config: {e}")
        print("  Trying alternative approach...")
        return False

    # Map our preset names to OCIO display/view names
    preset_views = [
        # (preset_name, OCIO source colorspace, OCIO display, OCIO view)
        ('rec709_100nit', 'ACES2065-1', 'sRGB - Display', None),
        ('srgb_100nit', 'ACES2065-1', 'sRGB - Display', None),
        ('p3d65_100nit', 'ACES2065-1', 'P3-D65 - Display', None),
        ('rec2020_100nit', 'ACES2065-1', 'Rec.2020 - Display', None),
        ('rec2020_1000nit_pq', 'ACES2065-1', 'Rec.2100-PQ - Display', None),
        ('dcdm_48nit', 'ACES2065-1', 'DCDM/XYZ - Display', None),
    ]

    for preset_name, src_cs, display_cs, view in preset_views:
        try:
            processor = config.getProcessor(src_cs, display_cs)
            outputs = process_rgb_batch(processor, TEST_RGB_INPUTS)
            if outputs is not None:
                filename = f"aces1_output_{preset_name}_output.csv"
                save_csv(filename, outputs)
                print(f"  Sample [{preset_name}]: [0.18, 0.18, 0.18] -> [{outputs[7][0]:.6f}, {outputs[7][1]:.6f}, {outputs[7][2]:.6f}]")
        except Exception as e:
            print(f"  Warning: Failed to process {preset_name}: {e}")

    return True


def generate_using_fixed_functions():
    """
    Generate reference data using OCIO FixedFunctionTransforms for ACES 1.x components.

    This builds the RRT+ODT pipeline from individual components:
    - Glow module (softens highlights)
    - RedMod (red hue modifier)
    - Segmented spline tone curve
    - Color space conversion
    - Display encoding
    """
    print("\nUsing OCIO FixedFunctionTransforms...")

    config = ocio.Config.CreateRaw()

    # The ACES 1.x pipeline components available as FixedFunctions:
    # - FIXED_FUNCTION_ACES_GLOW_03 / _10
    # - FIXED_FUNCTION_ACES_RED_MOD_03 / _10
    # - FIXED_FUNCTION_ACES_DARK_TO_DIM_10
    # - FIXED_FUNCTION_ACES_GAMUT_COMP_13

    # For now, we'll just test the components we have
    # Full RRT+ODT would require the segmented spline which isn't exposed as FixedFunction

    # Test Glow
    print("\n  Testing Glow10...")
    try:
        glow = ocio.FixedFunctionTransform(style=ocio.FIXED_FUNCTION_ACES_GLOW_10)
        proc = config.getProcessor(glow)
        outputs = process_rgb_batch(proc, TEST_RGB_INPUTS)
        if outputs is not None:
            save_csv("aces1_glow10_output.csv", outputs)
    except Exception as e:
        print(f"    Error: {e}")

    # Test RedMod
    print("  Testing RedMod10...")
    try:
        redmod = ocio.FixedFunctionTransform(style=ocio.FIXED_FUNCTION_ACES_RED_MOD_10)
        proc = config.getProcessor(redmod)
        outputs = process_rgb_batch(proc, TEST_RGB_INPUTS)
        if outputs is not None:
            save_csv("aces1_redmod10_output.csv", outputs)
    except Exception as e:
        print(f"    Error: {e}")

    # Test DarkToDim
    print("  Testing DarkToDim10...")
    try:
        d2d = ocio.FixedFunctionTransform(style=ocio.FIXED_FUNCTION_ACES_DARK_TO_DIM_10)
        proc = config.getProcessor(d2d)
        outputs = process_rgb_batch(proc, TEST_RGB_INPUTS)
        if outputs is not None:
            save_csv("aces1_darktodim10_output.csv", outputs)
    except Exception as e:
        print(f"    Error: {e}")

    # Test GamutComp13
    print("  Testing GamutComp13...")
    try:
        gc = ocio.FixedFunctionTransform(style=ocio.FIXED_FUNCTION_ACES_GAMUT_COMP_13)
        proc = config.getProcessor(gc)
        outputs = process_rgb_batch(proc, TEST_RGB_INPUTS)
        if outputs is not None:
            save_csv("aces1_gamutcomp13_output.csv", outputs)
    except Exception as e:
        print(f"    Error: {e}")

    return True


def generate_using_cg_config():
    """Generate reference data using ACES CG config for common outputs."""
    print("\nUsing ACES CG Config...")

    try:
        config = ocio.Config.CreateFromBuiltinConfig("cg-config-v2.1.0_aces-v1.3_ocio-v2.3")
        print(f"  Loaded config: cg-config-v2.1.0_aces-v1.3_ocio-v2.3")
    except Exception as e:
        print(f"  Could not load CG config: {e}")
        return False

    # Map presets to CG config color spaces
    preset_configs = [
        ('rec709_100nit', 'ACEScg', 'sRGB - Display'),
        ('srgb_100nit', 'ACEScg', 'sRGB - Display'),
        ('p3d65_100nit', 'ACEScg', 'Display P3 - Display'),
    ]

    for preset_name, src_cs, dst_cs in preset_configs:
        try:
            processor = config.getProcessor(src_cs, dst_cs)
            outputs = process_rgb_batch(processor, TEST_RGB_INPUTS)
            if outputs is not None:
                filename = f"aces1_cg_{preset_name}_output.csv"
                save_csv(filename, outputs)
                print(f"  Sample [{preset_name}]: [0.18, 0.18, 0.18] -> [{outputs[7][0]:.6f}, {outputs[7][1]:.6f}, {outputs[7][2]:.6f}]")
        except Exception as e:
            print(f"  Warning: Failed to process {preset_name}: {e}")

    return True


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    print("=" * 60)
    print("ACES 1.x Output Transform Reference Data Generator")
    print("=" * 60)
    print(f"OCIO Version: {ocio.GetVersion()}")
    print(f"Output directory: {OUTPUT_DIR}")

    # List available transforms
    list_builtin_transforms()

    # Save input RGB values
    save_csv("aces1_output_transform_input.csv", TEST_RGB_INPUTS)

    # Try different approaches to generate reference data
    print("\n" + "=" * 60)
    print("Generating Reference Data")
    print("=" * 60)

    # Method 1: Use builtin ACES config
    generate_using_builtin_config()

    # Method 2: Use individual FixedFunctionTransforms for components
    generate_using_fixed_functions()

    # Method 3: Use CG config
    generate_using_cg_config()

    print("\n" + "=" * 60)
    print("Done!")
    print("=" * 60)
    print("\nNote: ACES 1.x full RRT+ODT pipeline uses LUT-based BuiltinTransforms.")
    print("For exact matching with Alwan implementation, component tests are preferred:")
    print("  - Glow, RedMod, DarkToDim, GamutComp (FixedFunctions)")
    print("  - Segmented spline tone curve (not exposed in OCIO)")


if __name__ == "__main__":
    main()
