"""
Generate test fixtures for color space conversions.
Source: colour conversion functions (XYZ_to_Lab, XYZ_to_Luv, etc.)

Test input values are hardcoded (as per requirements).
Expected output values are computed from colour-science.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import ensure_dir, format_scalar, save_vector

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


# Test XYZ values (inputs - hardcoded as per requirements)
TEST_XYZ_VALUES = [
    [0.95047, 1.0, 1.08883],             # D65 white
    [0.5, 0.5, 0.5],                     # Mid-gray
    [0.412456, 0.212673, 0.019334],      # sRGB red
    [0.0, 0.0, 0.0],                     # Black
]

# Test Lab values (inputs - hardcoded)
TEST_LAB_VALUES = [
    [50.0, 25.0, 25.0],
    [75.0, -10.0, 50.0],
    [25.0, 0.0, 0.0],
]

# Test Luv values (inputs - hardcoded)
TEST_LUV_VALUES = [
    [50.0, 20.0, 30.0],
    [75.0, -15.0, 45.0],
    [25.0, 0.0, 0.0],
]


def generate_rgb_space_descriptors(output_dir):
    """Generate RGB color space descriptors (primaries + whitepoint)."""

    print("\nGenerating RGB space descriptors...")

    spaces = {
        'srgb': 'sRGB',
        'bt2020': 'ITU-R BT.2020',
        'aces_ap0': 'ACES2065-1',
        'aces_ap1': 'ACEScg',
    }

    for filename, space_name in spaces.items():
        try:
            space = colour.RGB_COLOURSPACES[space_name]
            primaries = space.primaries
            whitepoint = space.whitepoint

            # Save descriptor: rx,ry,gx,gy,bx,by,wx,wy
            filepath = os.path.join(output_dir, 'fixtures', f'{filename}_descriptor.csv')
            values = [
                primaries[0][0], primaries[0][1],
                primaries[1][0], primaries[1][1],
                primaries[2][0], primaries[2][1],
                whitepoint[0], whitepoint[1]
            ]
            save_vector(values, filepath, f"{space_name} descriptor")

            # For sRGB, also save RGB->XYZ matrix
            if filename == 'srgb':
                rgb_to_xyz = space.matrix_RGB_to_XYZ
                filepath = os.path.join(output_dir, 'fixtures', 'srgb_rgb_to_xyz.csv')
                flat = rgb_to_xyz.flatten()
                save_vector(flat, filepath, "sRGB RGB->XYZ matrix")

        except KeyError:
            print(f"  WARNING: RGB space '{space_name}' not found")


def generate_colorspace_conversion_fixtures(output_dir):
    """Generate color space conversion test fixtures."""

    print("\nGenerating color space conversion test fixtures...")

    # Get illuminant chromaticities from colour-science
    observer = 'CIE 1931 2 Degree Standard Observer'
    d65 = colour.CCS_ILLUMINANTS[observer]['D65']
    d50 = colour.CCS_ILLUMINANTS[observer]['D50']

    # 1. XYZ values (input)
    filepath = os.path.join(output_dir, 'fixtures', 'xyz_values.csv')
    flat_xyz = []
    for xyz in TEST_XYZ_VALUES:
        flat_xyz.extend(xyz)
    save_vector(flat_xyz, filepath, f"{len(TEST_XYZ_VALUES)} XYZ test values")

    # 2. xyY values (computed from XYZ)
    filepath = os.path.join(output_dir, 'fixtures', 'xyy_values.csv')
    flat_xyy = []
    for xyz in TEST_XYZ_VALUES:
        xyy = colour.XYZ_to_xyY(xyz)
        flat_xyy.extend(xyy)
    save_vector(flat_xyy, filepath, "Corresponding xyY values")

    # 3. Lab D65 values
    filepath = os.path.join(output_dir, 'fixtures', 'lab_d65_values.csv')
    flat_lab = []
    for xyz in TEST_XYZ_VALUES:
        lab = colour.XYZ_to_Lab(xyz, illuminant=d65)
        flat_lab.extend(lab)
    save_vector(flat_lab, filepath, "Lab values (D65 white point)")

    # 4. Lab D50 values
    filepath = os.path.join(output_dir, 'fixtures', 'lab_d50_values.csv')
    flat_lab = []
    for xyz in TEST_XYZ_VALUES:
        lab = colour.XYZ_to_Lab(xyz, illuminant=d50)
        flat_lab.extend(lab)
    save_vector(flat_lab, filepath, "Lab values (D50 white point)")

    # 5. Luv D65 values
    filepath = os.path.join(output_dir, 'fixtures', 'luv_d65_values.csv')
    flat_luv = []
    for xyz in TEST_XYZ_VALUES:
        luv = colour.XYZ_to_Luv(xyz, illuminant=d65)
        flat_luv.extend(luv)
    save_vector(flat_luv, filepath, "Luv values (D65 white point)")

    # 6. Lab -> LCh
    filepath = os.path.join(output_dir, 'fixtures', 'lab_for_lch.csv')
    flat_lab = []
    for lab in TEST_LAB_VALUES:
        flat_lab.extend(lab)
    save_vector(flat_lab, filepath, f"{len(TEST_LAB_VALUES)} Lab values for LCh conversion")

    filepath = os.path.join(output_dir, 'fixtures', 'lch_values.csv')
    flat_lch = []
    for lab in TEST_LAB_VALUES:
        lch = colour.Lab_to_LCHab(lab)
        flat_lch.extend(lch)
    save_vector(flat_lch, filepath, "Corresponding LCh values")

    # 7. Luv -> LCh(uv)
    filepath = os.path.join(output_dir, 'fixtures', 'luv_for_lchuv.csv')
    flat_luv = []
    for luv in TEST_LUV_VALUES:
        flat_luv.extend(luv)
    save_vector(flat_luv, filepath, f"{len(TEST_LUV_VALUES)} Luv values for LCh(uv) conversion")

    filepath = os.path.join(output_dir, 'fixtures', 'lchuv_values.csv')
    flat_lchuv = []
    for luv in TEST_LUV_VALUES:
        lchuv = colour.Luv_to_LCHuv(luv)
        flat_lchuv.extend(lchuv)
    save_vector(flat_lchuv, filepath, "Corresponding LCh(uv) values")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python test_fixtures.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_rgb_space_descriptors(output_dir)
    generate_colorspace_conversion_fixtures(output_dir)
