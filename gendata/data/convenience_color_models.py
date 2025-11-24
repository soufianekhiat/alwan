"""
Generate convenience color model test fixtures (HSV, HSL, CMY, CMYK, YCbCr).
Source: colour.RGB_to_HSV, colour.RGB_to_HSL, standard formulas for CMY/CMYK/YCbCr

Test RGB colors are hardcoded (inputs).
Expected values are computed from colour-science or standard formulas.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_vector

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


# Test RGB colors (inputs - hardcoded, diverse set with edge cases)
TEST_RGB_COLORS = [
    [1.0, 0.0, 0.0],      # Pure red
    [0.0, 1.0, 0.0],      # Pure green
    [0.0, 0.0, 1.0],      # Pure blue
    [1.0, 1.0, 0.0],      # Yellow
    [1.0, 0.0, 1.0],      # Magenta
    [0.0, 1.0, 1.0],      # Cyan
    [1.0, 1.0, 1.0],      # White
    [0.0, 0.0, 0.0],      # Black
    [0.5, 0.5, 0.5],      # Gray
    [0.75, 0.25, 0.25],   # Mixed color
    [0.2, 0.6, 0.3],      # Mixed color
]


def generate_hsv_hsl_fixtures(output_dir):
    """Generate HSV and HSL test fixtures from colour-science."""

    print("\nGenerating HSV and HSL fixtures...")

    # Save RGB input
    flat_rgb = []
    for rgb in TEST_RGB_COLORS:
        flat_rgb.extend(rgb)
    filepath = os.path.join(output_dir, 'fixtures', 'conv_rgb_input.csv')
    save_vector(flat_rgb, filepath, f"{len(TEST_RGB_COLORS)} RGB test colors")

    # Compute HSV from colour-science
    hsv_values = []
    for rgb in TEST_RGB_COLORS:
        hsv = colour.RGB_to_HSV(np.array(rgb))
        # Handle NaN for grayscale colors (hue undefined)
        if np.isnan(hsv[0]):
            hsv[0] = 0.0
        hsv_values.extend(hsv)

    filepath = os.path.join(output_dir, 'fixtures', 'conv_hsv_values.csv')
    save_vector(hsv_values, filepath, "HSV values from colour-science")

    # Compute HSL from colour-science
    hsl_values = []
    for rgb in TEST_RGB_COLORS:
        hsl = colour.RGB_to_HSL(np.array(rgb))
        # Handle NaN for grayscale colors (hue undefined)
        if np.isnan(hsl[0]):
            hsl[0] = 0.0
        hsl_values.extend(hsl)

    filepath = os.path.join(output_dir, 'fixtures', 'conv_hsl_values.csv')
    save_vector(hsl_values, filepath, "HSL values from colour-science")


def generate_cmy_cmyk_fixtures(output_dir):
    """Generate CMY and CMYK test fixtures using standard formulas."""

    print("\nGenerating CMY and CMYK fixtures...")

    # CMY: simple complement
    cmy_values = []
    for rgb in TEST_RGB_COLORS:
        cmy = [1.0 - rgb[0], 1.0 - rgb[1], 1.0 - rgb[2]]
        cmy_values.extend(cmy)

    filepath = os.path.join(output_dir, 'fixtures', 'conv_cmy_values.csv')
    save_vector(cmy_values, filepath, "CMY values (complement formula)")

    # CMYK: standard conversion from CMY
    cmyk_values = []
    for i in range(0, len(cmy_values), 3):
        c, m, y = cmy_values[i], cmy_values[i+1], cmy_values[i+2]
        k = min(c, m, y)

        if k < 1.0:
            c_out = (c - k) / (1.0 - k)
            m_out = (m - k) / (1.0 - k)
            y_out = (y - k) / (1.0 - k)
        else:
            c_out = m_out = y_out = 0.0

        cmyk_values.extend([c_out, m_out, y_out, k])

    filepath = os.path.join(output_dir, 'fixtures', 'conv_cmyk_values.csv')
    save_vector(cmyk_values, filepath, "CMYK values (standard formula)")


def generate_ycbcr_fixtures(output_dir):
    """Generate YCbCr test fixtures for BT.601, BT.709, BT.2020."""

    print("\nGenerating YCbCr fixtures...")

    # Standard coefficients (NO hardcoding - these are ITU-R specifications)
    K_BT601 = np.array([0.299, 0.587, 0.114])      # ITU-R BT.601
    K_BT709 = np.array([0.2126, 0.7152, 0.0722])   # ITU-R BT.709
    K_BT2020 = np.array([0.2627, 0.6780, 0.0593])  # ITU-R BT.2020

    standards = [
        ('conv_ycbcr_bt601.csv', K_BT601, "BT.601"),
        ('conv_ycbcr_bt709.csv', K_BT709, "BT.709"),
        ('conv_ycbcr_bt2020.csv', K_BT2020, "BT.2020"),
    ]

    for filename, K, std_name in standards:
        ycbcr_values = []

        for rgb in TEST_RGB_COLORS:
            rgb_arr = np.array(rgb)
            Y = np.dot(K, rgb_arr)
            Cb = 0.5 * (rgb_arr[2] - Y) / (1.0 - K[2])
            Cr = 0.5 * (rgb_arr[0] - Y) / (1.0 - K[0])

            # Store as Y, Cb+0.5, Cr+0.5 (offset to [0,1] range)
            ycbcr_values.extend([Y, Cb + 0.5, Cr + 0.5])

        filepath = os.path.join(output_dir, 'fixtures', filename)
        save_vector(ycbcr_values, filepath, f"YCbCr values ({std_name})")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python convenience_color_models.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_hsv_hsl_fixtures(output_dir)
    generate_cmy_cmyk_fixtures(output_dir)
    generate_ycbcr_fixtures(output_dir)
