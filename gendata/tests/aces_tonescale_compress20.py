"""
Generate reference data for ACES 2.0 TonescaleCompress20 tests.

IMPORTANT: OCIO's ACES_TONESCALE_COMPRESS_20 expects JMh input, not RGB!
The complete RGB->RGB pipeline requires:
  1. RGB_TO_JMH_20: RGB -> JMh
  2. TONESCALE_COMPRESS_20: JMh -> JMh (tonescale + chroma compress)
  3. JMH_TO_RGB_20 (inverse of step 1): JMh -> RGB

Alwan's alwan_aces_tonescale_compress20() does the complete RGB->RGB pipeline
internally, so we need to test it against the chained OCIO transforms.

This script generates CSV files with input RGB values and expected output RGB values
from OCIO's implementation, which serves as the reference for Alwan's implementation.
"""

import numpy as np
import PyOpenColorIO as ocio
import os

# Peak luminance values to test
PEAK_LUMINANCES = [1000.0]  # nits

# AP1 primaries chromaticity coordinates
AP1_RED_X = 0.713
AP1_RED_Y = 0.293
AP1_GREEN_X = 0.165
AP1_GREEN_Y = 0.830
AP1_BLUE_X = 0.128
AP1_BLUE_Y = 0.044
AP1_WHITE_X = 0.32168
AP1_WHITE_Y = 0.33767

# Test RGB inputs (AP1 primaries) - MUST match aces_ff_test_rgb_input.csv exactly!
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


def create_complete_pipeline_processor(peak_luminance):
    """
    Create OCIO processor for complete RGB -> tonescale/chroma -> RGB pipeline.

    This chains:
    1. RGB_TO_JMH_20: Convert AP1 RGB to JMh
    2. TONESCALE_COMPRESS_20: Apply tonescale and chroma compression
    3. JMH_TO_RGB_20 (inverse): Convert JMh back to RGB
    """
    config = ocio.Config.CreateRaw()

    # Create group transform for chaining
    group = ocio.GroupTransform()

    # AP1 primaries as 8 parameters: r_x, r_y, g_x, g_y, b_x, b_y, w_x, w_y
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

    # Step 2: Tonescale + Chroma Compress (operates on JMh)
    tonescale_compress = ocio.FixedFunctionTransform(
        style=ocio.FIXED_FUNCTION_ACES_TONESCALE_COMPRESS_20,
        params=[peak_luminance]
    )
    group.appendTransform(tonescale_compress)

    # Step 3: JMh to RGB (inverse of step 1)
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


def generate_tonescale_compress_data(peak_luminance, inputs, output_dir):
    """Generate reference data for a specific peak luminance."""
    print(f"\nGenerating TonescaleCompress20 data at {peak_luminance} nits...")
    print("(Complete RGB -> JMh -> TonescaleCompress -> RGB pipeline)")

    processor = create_complete_pipeline_processor(peak_luminance)

    outputs = []
    for rgb_in in inputs:
        rgb_out = process_rgb(processor, rgb_in)
        outputs.append(rgb_out)
        print(f"  {rgb_in} -> [{rgb_out[0]:.6f}, {rgb_out[1]:.6f}, {rgb_out[2]:.6f}]")

    # Write output CSV (named to match test file expectation)
    output_file = os.path.join(output_dir, f"aces_tonescale20_{int(peak_luminance)}_output.csv")
    with open(output_file, 'w') as f:
        for rgb in outputs:
            f.write(f"{rgb[0]:.16e},{rgb[1]:.16e},{rgb[2]:.16e},\n")
    print(f"  Written: {output_file}")

    return outputs


def main():
    # Output directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_dir = os.path.join(script_dir, "..", "..", "tests", "reference_values")
    os.makedirs(output_dir, exist_ok=True)

    print("ACES 2.0 TonescaleCompress20 Reference Data Generator")
    print(f"OCIO Version: {ocio.GetVersion()}")
    print(f"Output directory: {output_dir}")

    # Generate input CSV if it doesn't already contain our test values
    input_file = os.path.join(output_dir, "aces_tonescale_compress20_input.csv")
    with open(input_file, 'w') as f:
        for rgb in TEST_RGB_INPUTS:
            f.write(f"{rgb[0]:.16e},{rgb[1]:.16e},{rgb[2]:.16e},\n")
    print(f"\nWritten input: {input_file}")

    # Generate output data for each peak luminance
    for peak in PEAK_LUMINANCES:
        generate_tonescale_compress_data(peak, TEST_RGB_INPUTS, output_dir)

    print("\nDone!")


if __name__ == "__main__":
    main()
