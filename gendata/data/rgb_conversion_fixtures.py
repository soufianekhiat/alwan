"""
Generate RGB-to-RGB conversion test fixtures.
Source: colour.RGB_to_RGB()

Test RGB colors are hardcoded (inputs).
Converted RGB values are computed from colour-science.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_vector, format_scalar, ensure_dir

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_rgb_conversion_fixtures(output_dir):
    """Generate RGB-to-RGB conversion test fixtures."""

    print("\nGenerating RGB-to-RGB conversion test fixtures...")

    # Test colors in sRGB space (INPUTS - hardcoded)
    rgb_test_colors = [
        [1.0, 0.0, 0.0],  # Red
        [0.0, 1.0, 0.0],  # Green
        [0.0, 0.0, 1.0],  # Blue
        [1.0, 1.0, 0.0],  # Yellow
        [0.0, 1.0, 1.0],  # Cyan
        [1.0, 0.0, 1.0],  # Magenta
        [1.0, 1.0, 1.0],  # White
        [0.5, 0.5, 0.5],  # Gray
    ]

    # Get RGB color space definitions from colour-science (NO HARDCODING)
    srgb = colour.RGB_COLOURSPACES['sRGB']
    bt709 = colour.RGB_COLOURSPACES['ITU-R BT.709']
    display_p3 = colour.RGB_COLOURSPACES['Display P3']
    bt2020 = colour.RGB_COLOURSPACES['ITU-R BT.2020']
    aces_ap1 = colour.RGB_COLOURSPACES['ACEScg']

    # Test case 1: sRGB to Display P3 (same white point D65, no CAT needed)
    print("  sRGB -> Display P3...")
    srgb_to_p3_results = []
    for rgb in rgb_test_colors:
        rgb_array = np.array(rgb)
        # Convert sRGB to Display P3 using colour-science (NO HARDCODING)
        p3_rgb = colour.RGB_to_RGB(rgb_array, srgb, display_p3)
        srgb_to_p3_results.extend(rgb)  # Source RGB
        srgb_to_p3_results.extend(p3_rgb.tolist())  # Destination RGB

    filepath = os.path.join(output_dir, 'fixtures', 'rgb_convert_srgb_to_p3.csv')
    save_vector(srgb_to_p3_results, filepath, f"sRGB to Display P3 ({len(rgb_test_colors)} colors)")

    # Test case 2: sRGB to BT.2020 (same white point D65, wider gamut)
    print("  sRGB -> BT.2020...")
    srgb_to_bt2020_results = []
    for rgb in rgb_test_colors:
        rgb_array = np.array(rgb)
        bt2020_rgb = colour.RGB_to_RGB(rgb_array, srgb, bt2020)
        srgb_to_bt2020_results.extend(rgb)
        srgb_to_bt2020_results.extend(bt2020_rgb.tolist())

    filepath = os.path.join(output_dir, 'fixtures', 'rgb_convert_srgb_to_bt2020.csv')
    save_vector(srgb_to_bt2020_results, filepath, f"sRGB to BT.2020 ({len(rgb_test_colors)} colors)")

    # Test case 3: sRGB to ACEScg (different white point, D65->D60, needs CAT)
    print("  sRGB -> ACEScg (with CAT)...")
    srgb_to_aces_results = []
    for rgb in rgb_test_colors:
        rgb_array = np.array(rgb)
        aces_rgb = colour.RGB_to_RGB(rgb_array, srgb, aces_ap1,
                                      chromatic_adaptation_transform='Bradford')
        srgb_to_aces_results.extend(rgb)
        srgb_to_aces_results.extend(aces_rgb.tolist())

    filepath = os.path.join(output_dir, 'fixtures', 'rgb_convert_srgb_to_acescg.csv')
    save_vector(srgb_to_aces_results, filepath, f"sRGB to ACEScg ({len(rgb_test_colors)} colors)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python rgb_conversion_fixtures.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_rgb_conversion_fixtures(output_dir)
