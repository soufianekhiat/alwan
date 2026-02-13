"""
Generate reference data for ACES 2.0 GamutCompress20 tests.

Uses pure Python float64 implementation of the Hellwig 2022 color appearance
model (same as section5_section9.py). This avoids OCIO's float32 precision
which limits accuracy to ~7 significant digits.

For AP1 limit primaries, GamutCompress20 is a pass-through (no compression),
so the pipeline RGB -> JMh -> GamutCompress -> JMh -> RGB simplifies to a
JMh roundtrip.
"""

import numpy as np
import os
import sys

# Add gendata/tests to path so we can import from section5_section9
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from section5_section9 import JMhParams, rgb_to_jmh20, jmh_to_rgb20

# AP1 primaries chromaticity coordinates
AP1_RED_X = 0.713
AP1_RED_Y = 0.293
AP1_GREEN_X = 0.165
AP1_GREEN_Y = 0.830
AP1_BLUE_X = 0.128
AP1_BLUE_Y = 0.044
AP1_WHITE_X = 0.32168
AP1_WHITE_Y = 0.33767

# Peak luminance values to test
PEAK_LUMINANCES = [1000.0]

# Test RGB inputs - MUST match aces_ff_test_rgb_input.csv exactly!
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


def generate_gamut_compress_data(peak_luminance, inputs, output_dir):
    """Generate reference data for GamutCompress20 at a specific peak luminance.

    For AP1 limit primaries, GamutCompress20 is a pass-through, so the pipeline
    is effectively a JMh roundtrip: RGB -> JMh -> RGB.
    """
    print(f"\nGenerating GamutCompress20 data at {peak_luminance} nits (float64)...")
    print("(Complete RGB -> JMh -> GamutCompress(pass-through) -> RGB pipeline)")

    ap1 = JMhParams(AP1_RED_X, AP1_RED_Y, AP1_GREEN_X, AP1_GREEN_Y,
                     AP1_BLUE_X, AP1_BLUE_Y, AP1_WHITE_X, AP1_WHITE_Y)

    outputs = []
    for rgb_in in inputs:
        r, g, b = float(rgb_in[0]), float(rgb_in[1]), float(rgb_in[2])

        # Forward: RGB -> JMh
        jmh = rgb_to_jmh20(r, g, b, ap1)

        # GamutCompress with AP1 limit primaries = pass-through (no compression)
        # So jmh_out = jmh_in

        # Inverse: JMh -> RGB
        rgb_out = jmh_to_rgb20(jmh[0], jmh[1], jmh[2], ap1)

        outputs.append([rgb_out[0], rgb_out[1], rgb_out[2]])
        print(f"  [{r:.2f}, {g:.2f}, {b:.2f}] -> [{rgb_out[0]:.16e}, {rgb_out[1]:.16e}, {rgb_out[2]:.16e}]")

    # Write output CSV
    output_file = os.path.join(output_dir, f"aces_gamut_compress20_{int(peak_luminance)}_output.csv")
    with open(output_file, 'w') as f:
        for rgb in outputs:
            f.write(f"{rgb[0]:.16e},{rgb[1]:.16e},{rgb[2]:.16e},\n")
    print(f"  Written: {output_file}")

    return outputs


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    output_dir = os.path.join(script_dir, "..", "..", "tests", "reference_values")
    os.makedirs(output_dir, exist_ok=True)

    print("ACES 2.0 GamutCompress20 Reference Data Generator (float64)")
    print(f"Output directory: {output_dir}")

    for peak in PEAK_LUMINANCES:
        generate_gamut_compress_data(peak, TEST_RGB_INPUTS, output_dir)

    print("\nDone!")


if __name__ == "__main__":
    main()
