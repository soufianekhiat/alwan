"""
Generate ATD95 test data.
Source: colour.XYZ_to_ATD95()

Test inputs are hardcoded (as per requirements).
Expected outputs are computed from colour-science.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_test_data

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_atd95_tests(output_dir):
    """Generate ATD95 test cases."""

    print("\nGenerating ATD95 test data...")

    # Get D65 white point from colour-science
    observer = 'CIE 1931 2 Degree Standard Observer'
    d65_xy = colour.CCS_ILLUMINANTS[observer]['D65']
    d65_xyz = colour.xy_to_XYZ(d65_xy)

    # Standard viewing conditions
    XYZ_0 = (d65_xyz * 100).tolist()  # Reference white, scale to Y=100
    XYZ_0_arr = np.array(XYZ_0)

    # ATD95 parameters
    Y_0 = 318.31  # Absolute adapting field luminance (cd/m²)
    k_1 = 0.0     # Coefficient for sigma (default)
    k_2 = 15.0    # Coefficient for tau (default)

    # Test cases: ONLY INPUTS are hardcoded
    # XYZ values at Y=100 scale
    test_cases = [
        (XYZ_0, XYZ_0, Y_0, k_1, k_2),  # White
        ([50.0, 50.0, 50.0], XYZ_0, Y_0, k_1, k_2),  # Mid-gray
        ([41.2456, 21.2673, 1.9334], XYZ_0, Y_0, k_1, k_2),  # sRGB red
        ([35.7576, 71.5152, 11.9192], XYZ_0, Y_0, k_1, k_2),  # sRGB green
        ([18.0437, 7.2175, 95.0304], XYZ_0, Y_0, k_1, k_2),  # sRGB blue
        ([77.0, 92.8, 10.1], XYZ_0, Y_0, k_1, k_2),  # Lime
        ([31.4, 15.9, 9.8], XYZ_0, Y_0, k_1, k_2),  # Brown
        # Different adaptation levels
        ([50.0, 50.0, 50.0], XYZ_0, 100.0, k_1, k_2),  # Lower Y_0
        ([50.0, 50.0, 50.0], XYZ_0, 1000.0, k_1, k_2),  # Higher Y_0
    ]

    test_data = []

    for xyz_in, xyz_0, Y0, k1, k2 in test_cases:
        # Compute ATD95 correlates from colour-science (NO HARDCODING)
        xyz_arr = np.array(xyz_in)
        xyz_0_arr = np.array(xyz_0)

        # ATD95 returns (A, T1, D, T2) - achromatic, tritanopic, deuteranopic, and second tritanopic signals
        atd = colour.XYZ_to_ATD95(xyz_arr, xyz_0_arr, Y0, k1, k2)

        # Append inputs and outputs: XYZ_in (3), XYZ_0 (3), Y_0, k_1, k_2, A, T1, D, T2
        test_data.extend(xyz_in)
        test_data.extend(xyz_0)
        test_data.append(Y0)
        test_data.append(k1)
        test_data.append(k2)
        test_data.append(atd.A)
        test_data.append(atd.T_1)
        test_data.append(atd.D)
        test_data.append(atd.T_2)

    # Save
    filepath = os.path.join(output_dir, 'atd95.csv')
    save_test_data(test_data, filepath, f"{len(test_cases)} test cases, 13 values each")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python atd95.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_atd95_tests(output_dir)
