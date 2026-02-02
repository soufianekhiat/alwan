"""
Generate reference data for ACES 2.0 GamutCompress20 tests.

OCIO's ACES_GAMUT_COMPRESS_20 requires 9 parameters:
  - peak_luminance
  - AP1 primaries: r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y

This transform operates on JMh values (output of RGB_to_JMh20) and
compresses out-of-gamut colors to fit within the display gamut.
"""

import numpy as np
import PyOpenColorIO as ocio
import os

# Peak luminance values to test
PEAK_LUMINANCES = [1000.0]

# AP1 primaries chromaticity coordinates
AP1_RED_X = 0.713
AP1_RED_Y = 0.293
AP1_GREEN_X = 0.165
AP1_GREEN_Y = 0.830
AP1_BLUE_X = 0.128
AP1_BLUE_Y = 0.044
AP1_WHITE_X = 0.32168
AP1_WHITE_Y = 0.33767

# Test RGB inputs - same as other ACES 2.0 tests
TEST_RGB_INPUTS = [
    [1.0, 0.0, 0.0],              # Red
    [0.0, 1.0, 0.0],              # Green
    [0.0, 0.0, 1.0],              # Blue
    [1.0, 1.0, 0.0],              # Yellow
    [0.0, 1.0, 1.0],              # Cyan
    [1.0, 0.0, 1.0],              # Magenta
    [0.0, 0.0, 0.0],              # Black
    [0.18, 0.18, 0.18],           # 18% gray
    [0.5, 0.5, 0.5],              # Mid gray
    [1.0, 1.0, 1.0],              # White
    [0.8, 0.1, 0.05],             # Saturated red-orange
    [1.0, 0.2, 0.1],              # Orange-red
    [0.5, 0.0, 0.0],              # Dark red
    [1.0, 0.5, 0.0],              # Orange
    [0.8, 0.4, 0.2],              # Brown/tan
    [0.05, 0.05, 0.05],           # Very dark
    [0.02, 0.01, 0.005],          # Very low
    [2.0, 1.5, 0.5],              # HDR
    [5.0, 4.0, 3.0],              # HDR bright
    [1.5, -0.2, 0.0],             # Out of gamut
    [1.0, 1.0, -0.5],             # Out of gamut
    [-0.2, 0.5, 0.3],             # Negative red
]


def create_gamut_compress_processor(peak_luminance):
    """
    Create OCIO processor for GamutCompress20 only (operates on JMh).

    Note: GamutCompress20 expects JMh input, not RGB!
    """
    config = ocio.Config.CreateRaw()

    ap1_params = [
        AP1_RED_X, AP1_RED_Y,
        AP1_GREEN_X, AP1_GREEN_Y,
        AP1_BLUE_X, AP1_BLUE_Y,
        AP1_WHITE_X, AP1_WHITE_Y
    ]

    # GamutCompress20 takes 9 params: peak_luminance + AP1 primaries
    params = [peak_luminance] + ap1_params

    transform = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_GAMUT_COMPRESS_20,
        params=params
    )

    return config.getProcessor(transform)


def create_complete_pipeline_processor(peak_luminance):
    """
    Create OCIO processor for complete RGB -> JMh -> GamutCompress -> RGB pipeline.

    This chains:
    1. RGB_TO_JMH_20: Convert AP1 RGB to JMh
    2. GAMUT_COMPRESS_20: Apply gamut compression on JMh
    3. JMH_TO_RGB_20 (inverse): Convert JMh back to RGB
    """
    config = ocio.Config.CreateRaw()
    group = ocio.GroupTransform()

    ap1_params = [
        AP1_RED_X, AP1_RED_Y,
        AP1_GREEN_X, AP1_GREEN_Y,
        AP1_BLUE_X, AP1_BLUE_Y,
        AP1_WHITE_X, AP1_WHITE_Y
    ]

    # Step 1: RGB to JMh
    rgb_to_jmh = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_RGB_TO_JMH_20,
        params=ap1_params
    )
    group.appendTransform(rgb_to_jmh)

    # Step 2: GamutCompress20 (operates on JMh)
    gamut_compress_params = [peak_luminance] + ap1_params
    gamut_compress = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_GAMUT_COMPRESS_20,
        params=gamut_compress_params
    )
    group.appendTransform(gamut_compress)

    # Step 3: JMh to RGB (inverse)
    jmh_to_rgb = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_RGB_TO_JMH_20,
        params=ap1_params,
        direction=ocio.TRANSFORM_DIR_INVERSE
    )
    group.appendTransform(jmh_to_rgb)

    return config.getProcessor(group)


def process_rgb(processor, rgb):
    """Process a single RGB value through the OCIO processor."""
    cpu = processor.getDefaultCPUProcessor()
    result = np.array(rgb, dtype=np.float32).copy()
    cpu.applyRGB(result)
    return result.tolist()


def generate_gamut_compress_data(peak_luminance, inputs, output_dir):
    """Generate reference data for GamutCompress20 at a specific peak luminance."""
    print(f"\nGenerating GamutCompress20 data at {peak_luminance} nits...")
    print("(Complete RGB -> JMh -> GamutCompress -> RGB pipeline)")

    processor = create_complete_pipeline_processor(peak_luminance)

    outputs = []
    for rgb_in in inputs:
        rgb_out = process_rgb(processor, rgb_in)
        outputs.append(rgb_out)
        print(f"  {rgb_in} -> [{rgb_out[0]:.6f}, {rgb_out[1]:.6f}, {rgb_out[2]:.6f}]")

    # Write output CSV
    output_file = os.path.join(output_dir, f"aces_gamut_compress20_{int(peak_luminance)}_output.csv")
    with open(output_file, 'w') as f:
        for rgb in outputs:
            f.write(f"{rgb[0]:.16e},{rgb[1]:.16e},{rgb[2]:.16e},\n")
    print(f"  Written: {output_file}")

    return outputs


def generate_jmh_gamut_compress_data(peak_luminance, output_dir):
    """
    Generate reference data for JMh-domain GamutCompress20.

    This generates JMh input/output pairs for testing the gamut compression
    directly in JMh space (without the RGB roundtrip).
    """
    print(f"\nGenerating JMh-domain GamutCompress20 data at {peak_luminance} nits...")

    config = ocio.Config.CreateRaw()

    # Get RGB to JMh processor
    ap1_params = [
        AP1_RED_X, AP1_RED_Y,
        AP1_GREEN_X, AP1_GREEN_Y,
        AP1_BLUE_X, AP1_BLUE_Y,
        AP1_WHITE_X, AP1_WHITE_Y
    ]

    rgb_to_jmh = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_RGB_TO_JMH_20,
        params=ap1_params
    )
    jmh_processor = config.getProcessor(rgb_to_jmh)
    jmh_cpu = jmh_processor.getDefaultCPUProcessor()

    # Get GamutCompress processor
    gamut_compress_params = [peak_luminance] + ap1_params
    gamut_compress = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_GAMUT_COMPRESS_20,
        params=gamut_compress_params
    )
    gc_processor = config.getProcessor(gamut_compress)
    gc_cpu = gc_processor.getDefaultCPUProcessor()

    # First convert RGB to JMh
    jmh_inputs = []
    for rgb in TEST_RGB_INPUTS:
        jmh = np.array(rgb, dtype=np.float32).copy()
        jmh_cpu.applyRGB(jmh)
        jmh_inputs.append(jmh.tolist())

    # Then apply GamutCompress to JMh
    jmh_outputs = []
    for jmh_in in jmh_inputs:
        jmh_out = np.array(jmh_in, dtype=np.float32).copy()
        gc_cpu.applyRGB(jmh_out)
        jmh_outputs.append(jmh_out.tolist())

    # Write JMh input CSV
    input_file = os.path.join(output_dir, f"aces_gamut_compress20_{int(peak_luminance)}_jmh_input.csv")
    with open(input_file, 'w') as f:
        for jmh in jmh_inputs:
            f.write(f"{jmh[0]:.16e},{jmh[1]:.16e},{jmh[2]:.16e},\n")
    print(f"  Written: {input_file}")

    # Write JMh output CSV
    output_file = os.path.join(output_dir, f"aces_gamut_compress20_{int(peak_luminance)}_jmh_output.csv")
    with open(output_file, 'w') as f:
        for jmh in jmh_outputs:
            f.write(f"{jmh[0]:.16e},{jmh[1]:.16e},{jmh[2]:.16e},\n")
    print(f"  Written: {output_file}")

    print("\nJMh domain transformations:")
    for i, rgb in enumerate(TEST_RGB_INPUTS):
        jmh_in = jmh_inputs[i]
        jmh_out = jmh_outputs[i]
        print(f"  RGB=({rgb[0]:.2f},{rgb[1]:.2f},{rgb[2]:.2f})")
        print(f"    JMh_in =({jmh_in[0]:8.3f}, {jmh_in[1]:8.3f}, {jmh_in[2]:8.3f})")
        print(f"    JMh_out=({jmh_out[0]:8.3f}, {jmh_out[1]:8.3f}, {jmh_out[2]:8.3f})")


def main():
    # Output directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_dir = os.path.join(script_dir, "..", "..", "tests", "reference_values")
    os.makedirs(output_dir, exist_ok=True)

    print("ACES 2.0 GamutCompress20 Reference Data Generator")
    print(f"OCIO Version: {ocio.GetVersion()}")
    print(f"Output directory: {output_dir}")

    # Generate output data for each peak luminance
    for peak in PEAK_LUMINANCES:
        generate_gamut_compress_data(peak, TEST_RGB_INPUTS, output_dir)
        generate_jmh_gamut_compress_data(peak, output_dir)

    print("\nDone!")


if __name__ == "__main__":
    main()
