"""
Generate Kim2009 test data.
Source: colour.XYZ_to_Kim2009()

Test inputs are hardcoded (as per requirements).
Expected outputs are computed from colour-science.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_test_data

# Import colour-science
try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_kim2009_tests(output_dir):
    """Generate Kim2009 test cases."""

    print("\nGenerating Kim2009 test data...")

    # D65 white point (from colour-science)
    # CCS_ILLUMINANTS returns (x, y) chromaticity, convert to XYZ
    d65_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']
    x, y = d65_xy[0], d65_xy[1]
    Y = 100.0
    X = (x / y) * Y
    Z = ((1.0 - x - y) / y) * Y
    d65_white = [X, Y, Z]

    # Test cases: [XYZ_in, XYZ_w, La, Yb]
    # Only INPUTS are hardcoded - outputs come from colour-science
    test_cases = [
        # Mid-gray
        ([19.01, 20.0, 21.78], d65_white, 318.31, 20.0),
        # Red
        ([41.24, 21.26, 1.93], d65_white, 318.31, 20.0),
        # Green
        ([35.76, 71.52, 11.92], d65_white, 318.31, 20.0),
        # Blue
        ([18.05, 7.22, 95.05], d65_white, 318.31, 20.0),
        # White
        (d65_white, d65_white, 318.31, 20.0),
        # Dark gray
        ([5.0, 5.0, 5.44], d65_white, 318.31, 20.0),
        # Light gray
        ([80.0, 80.0, 87.04], d65_white, 318.31, 20.0),
        # Yellow
        ([77.0, 92.78, 13.85], d65_white, 318.31, 20.0),
    ]

    kim2009_test_data = []

    for xyz_in, xyz_w, La, Yb in test_cases:
        xyz_arr = np.array(xyz_in)
        xyz_w_arr = np.array(xyz_w)

        # Get viewing conditions from colour-science
        # Note: C implementation uses E=1.0 (High-luminance LCD Display)
        # not CRT Displays (E=1.4572)
        media = colour.MEDIA_PARAMETERS_KIM2009['High-luminance LCD Display']
        surround = colour.VIEWING_CONDITIONS_KIM2009['Average']

        # Compute correlates using colour-science
        specification = colour.XYZ_to_Kim2009(xyz_arr, xyz_w_arr, La, media=media, surround=surround)

        # Extract correlates
        J = specification.J
        C = specification.C
        h = specification.h

        # Append to test data: XYZ_in (3), XYZ_w (3), La, Yb, J, C, h
        kim2009_test_data.extend(xyz_in)
        kim2009_test_data.extend(xyz_w)
        kim2009_test_data.append(La)
        kim2009_test_data.append(Yb)
        kim2009_test_data.append(J)
        kim2009_test_data.append(C)
        kim2009_test_data.append(h)

    # Save test data
    filepath = os.path.join(output_dir, 'kim2009.csv')
    save_test_data(kim2009_test_data, filepath,
                   f"{len(test_cases)} test cases, 12 values each")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python kim2009.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_kim2009_tests(output_dir)
