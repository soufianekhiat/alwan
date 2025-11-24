"""
Generate Hellwig2022 test data.
Source: colour.XYZ_to_Hellwig2022()

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


def generate_hellwig2022_tests(output_dir):
    """Generate Hellwig2022 test cases."""

    print("\nGenerating Hellwig2022 test data...")

    # D65 white point (from colour-science)
    d65_white_xyz = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']
    d65_white_xyz = [d65_white_xyz[0], d65_white_xyz[1], d65_white_xyz[2]]

    # Test cases: [XYZ_in, XYZ_w, La, Yb, surround_idx]
    # Only INPUTS are hardcoded - outputs come from colour-science
    surround_names = ['Average', 'Dim', 'Dark']

    test_cases = [
        # Mid-gray, average surround
        ([19.01, 20.0, 21.78], d65_white_xyz, 318.31, 20.0, 0),
        # Red, average surround
        ([41.24, 21.26, 1.93], d65_white_xyz, 318.31, 20.0, 0),
        # Green, average surround
        ([35.76, 71.52, 11.92], d65_white_xyz, 318.31, 20.0, 0),
        # Blue, average surround
        ([18.05, 7.22, 95.05], d65_white_xyz, 318.31, 20.0, 0),
        # White, average surround
        (d65_white_xyz, d65_white_xyz, 318.31, 20.0, 0),
        # Mid-gray, dim surround
        ([19.01, 20.0, 21.78], d65_white_xyz, 318.31, 20.0, 1),
        # Mid-gray, dark surround
        ([19.01, 20.0, 21.78], d65_white_xyz, 318.31, 20.0, 2),
        # Red, dim surround
        ([41.24, 21.26, 1.93], d65_white_xyz, 318.31, 20.0, 1),
    ]

    hellwig2022_test_data = []

    for xyz_in, xyz_w, La, Yb, surround_idx in test_cases:
        xyz_arr = np.array(xyz_in)
        xyz_w_arr = np.array(xyz_w)

        # Get surround from colour-science
        surround = colour.VIEWING_CONDITIONS_HELLWIG2022[surround_names[surround_idx]]

        # Compute correlates using colour-science
        specification = colour.XYZ_to_Hellwig2022(xyz_arr, xyz_w_arr, La, Yb, surround)

        # Extract correlates
        J = specification.J
        C = specification.C
        h = specification.h

        # Append to test data: XYZ_in (3), XYZ_w (3), La, Yb, surround, J, C, h
        hellwig2022_test_data.extend(xyz_in)
        hellwig2022_test_data.extend(xyz_w)
        hellwig2022_test_data.append(La)
        hellwig2022_test_data.append(Yb)
        hellwig2022_test_data.append(float(surround_idx))
        hellwig2022_test_data.append(J)
        hellwig2022_test_data.append(C)
        hellwig2022_test_data.append(h)

    # Save test data
    filepath = os.path.join(output_dir, 'hellwig2022.csv')
    save_test_data(hellwig2022_test_data, filepath,
                   f"{len(test_cases)} test cases, 12 values each")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python hellwig2022.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_hellwig2022_tests(output_dir)
