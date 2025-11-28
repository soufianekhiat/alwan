"""
Generate LLAB test data.
Source: colour.XYZ_to_LLAB()

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


def generate_llab_tests(output_dir):
    """Generate LLAB test cases."""

    print("\nGenerating LLAB test data...")

    # D65 white point (from colour-science)
    # CCS_ILLUMINANTS returns (x, y) chromaticity, convert to XYZ
    d65_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']
    x, y = d65_xy[0], d65_xy[1]
    Y = 100.0
    X = (x / y) * Y
    Z = ((1.0 - x - y) / y) * Y
    d65_white = [X, Y, Z]

    # LLAB viewing conditions from colour-science
    surround_names = ['Reference Samples & Images, Average Surround, Subtending > 4',
                      'Television & VDU Displays, Dim Surround',
                      '35mm Projection Transparency, Dark Surround']

    # Test cases: [XYZ_in, XYZ_0, XYZ_r, Y_b, surround_idx]
    # Only INPUTS are hardcoded - outputs come from colour-science
    test_cases = [
        # Mid-gray, average surround, same illuminant
        ([19.01, 20.0, 21.78], d65_white, d65_white, 20.0, 0),
        # Red, average surround
        ([41.24, 21.26, 1.93], d65_white, d65_white, 20.0, 0),
        # Green, average surround
        ([35.76, 71.52, 11.92], d65_white, d65_white, 20.0, 0),
        # Blue, average surround
        ([18.05, 7.22, 95.05], d65_white, d65_white, 20.0, 0),
        # White, average surround
        (d65_white, d65_white, d65_white, 20.0, 0),
        # Mid-gray, dim surround
        ([50.0, 50.0, 54.3], d65_white, d65_white, 20.0, 1),
    ]

    llab_test_data = []

    for xyz_in, xyz_0, xyz_r, Y_b, surround_idx in test_cases:
        xyz_arr = np.array(xyz_in)
        xyz_0_arr = np.array(xyz_0)
        xyz_r_arr = np.array(xyz_r)

        # Get surround from colour-science
        surround = colour.VIEWING_CONDITIONS_LLAB[surround_names[surround_idx]]

        # Compute correlates using colour-science
        specification = colour.XYZ_to_LLAB(xyz_arr, xyz_0_arr, Y_b, L=100.0, surround=surround)

        # Extract correlates
        L = specification.J  # LLAB uses J for lightness in colour-science
        Ch = specification.C  # Chroma

        # Append to test data: XYZ_in (3), XYZ_0 (3), XYZ_r (3), Y_b, surround, L, Ch
        llab_test_data.extend(xyz_in)
        llab_test_data.extend(xyz_0)
        llab_test_data.extend(xyz_r)
        llab_test_data.append(Y_b)
        llab_test_data.append(float(surround_idx))
        llab_test_data.append(L)
        llab_test_data.append(Ch)

    # Save test data
    filepath = os.path.join(output_dir, 'llab.csv')
    save_test_data(llab_test_data, filepath,
                   f"{len(test_cases)} test cases, 12 values each")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python llab.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_llab_tests(output_dir)
