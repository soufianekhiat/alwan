"""
Generate basic color space conversion test data.
Source: colour.XYZ_to_Lab(), colour.XYZ_to_Luv(), etc.

Test inputs are hardcoded (8 test colors).
Expected outputs are computed from colour-science.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from common import save_test_data, format_scalar

try:
    import colour
    import numpy as np
except ImportError:
    print("ERROR: colour-science not installed")
    sys.exit(1)


def generate_basic_colorspace_tests(output_dir):
    """Generate XYZ/Lab/Luv/xyY/LCh test cases."""

    print("\nGenerating basic color space conversion test data...")

    # Test colors (8 XYZ tristimulus values)
    test_xyz = np.array([
        [0.0, 0.0, 0.0],          # Black
        [1.0, 1.0, 1.0],          # White
        [0.5, 0.5, 0.5],          # Mid gray
        [0.95047, 1.0, 1.08883],  # D65 white
        [0.950456, 1.0, 1.089058], # Slightly different D65
        [0.412453, 0.212671, 0.019334],  # sRGB red
        [0.357580, 0.715160, 0.119193],  # sRGB green
        [0.180423, 0.072169, 0.950227]   # sRGB blue
    ])

    # White points
    d65_white = np.array([0.95047, 1.0, 1.08883])
    d50_white = np.array([0.96422, 1.0, 0.82521])

    # Generate XYZ -> xyY
    xyz_to_xyy = []
    for xyz in test_xyz:
        xyy = colour.XYZ_to_xyY(xyz)
        xyz_to_xyy.extend(xyy.tolist())

    filepath = os.path.join(output_dir, 'xyz_to_xyy.csv')
    save_test_data(xyz_to_xyy, filepath, f"{len(test_xyz)} XYZ->xyY conversions")

    # Generate XYZ -> Lab (D65)
    xyz_to_lab_d65 = []
    for xyz in test_xyz:
        lab = colour.XYZ_to_Lab(xyz, d65_white)
        xyz_to_lab_d65.extend(lab.tolist())

    filepath = os.path.join(output_dir, 'xyz_to_lab_d65.csv')
    save_test_data(xyz_to_lab_d65, filepath, f"{len(test_xyz)} XYZ->Lab (D65) conversions")

    # Generate XYZ -> Lab (D50)
    xyz_to_lab_d50 = []
    for xyz in test_xyz:
        lab = colour.XYZ_to_Lab(xyz, d50_white)
        xyz_to_lab_d50.extend(lab.tolist())

    filepath = os.path.join(output_dir, 'xyz_to_lab_d50.csv')
    save_test_data(xyz_to_lab_d50, filepath, f"{len(test_xyz)} XYZ->Lab (D50) conversions")

    # Generate XYZ -> Luv (D65)
    xyz_to_luv_d65 = []
    for xyz in test_xyz:
        luv = colour.XYZ_to_Luv(xyz, d65_white)
        xyz_to_luv_d65.extend(luv.tolist())

    filepath = os.path.join(output_dir, 'xyz_to_luv_d65.csv')
    save_test_data(xyz_to_luv_d65, filepath, f"{len(test_xyz)} XYZ->Luv (D65) conversions")

    # Generate Lab -> LCh (using the D65 Lab values)
    lab_to_lch = []
    for i in range(len(test_xyz)):
        lab = np.array(xyz_to_lab_d65[i*3:(i+1)*3])
        lch = colour.Lab_to_LCHab(lab)
        lab_to_lch.extend(lch.tolist())

    filepath = os.path.join(output_dir, 'lab_to_lch.csv')
    save_test_data(lab_to_lch, filepath, f"{len(test_xyz)} Lab->LCh conversions")

    # Generate Luv -> LCh(uv) (using the D65 Luv values)
    luv_to_lchuv = []
    for i in range(len(test_xyz)):
        luv = np.array(xyz_to_luv_d65[i*3:(i+1)*3])
        lchuv = colour.Luv_to_LCHuv(luv)
        luv_to_lchuv.extend(lchuv.tolist())

    filepath = os.path.join(output_dir, 'luv_to_lchuv.csv')
    save_test_data(luv_to_lchuv, filepath, f"{len(test_xyz)} Luv->LCh(uv) conversions")

    # Save white points
    filepath = os.path.join(output_dir, 'test_d65_white.csv')
    save_test_data(d65_white.tolist(), filepath, "D65 white point (XYZ)")

    filepath = os.path.join(output_dir, 'test_d50_white.csv')
    save_test_data(d50_white.tolist(), filepath, "D50 white point (XYZ)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python basic_colorspace_tests.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_basic_colorspace_tests(output_dir)
