"""
Generate LLAB test data.
Source: colour.XYZ_to_LLAB()

ALL values come from colour-science - no hardcoded values.
Test colors are from ColorChecker dataset.
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


def get_d65_xyz_100():
    """Get D65 white point XYZ with Y=100 from colour-science."""
    d65_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']
    xyz = colour.xy_to_XYZ(d65_xy)
    # Scale to Y=100
    return xyz * 100.0


def get_colorchecker_xyz_d65():
    """Get ColorChecker XYZ values under D65 from colour-science."""
    cc = colour.SDS_COLOURCHECKERS['ColorChecker N Ohta']
    cmfs = colour.MSDS_CMFS['CIE 1931 2 Degree Standard Observer']
    illuminant = colour.SDS_ILLUMINANTS['D65']

    colors = {}
    for name, sd in cc.items():
        XYZ = colour.sd_to_XYZ(sd, cmfs, illuminant)
        colors[name] = XYZ
    return colors


# LLAB surround conditions from colour-science
LLAB_SURROUNDS = [
    ('Reference Samples & Images, Average Surround, Subtending > 4', 0),
    ('Television & VDU Displays, Dim Surround', 1),
    ('35mm Projection Transparency, Dark Surround', 2),
]


def generate_llab_tests(output_dir):
    """Generate LLAB test cases using colour-science data."""

    print("\nGenerating LLAB test data...")

    # D65 white point from colour-science (Y=100 scale)
    d65_white = get_d65_xyz_100()

    # Get ColorChecker colors from colour-science
    cc_colors = get_colorchecker_xyz_d65()

    # Standard background relative luminance (20% gray)
    Y_b = 20.0

    # Reference luminance (100 cd/m2 standard)
    L = 100.0

    # Select test colors from ColorChecker (diverse set)
    # Names from colour.SDS_COLOURCHECKERS['ColorChecker N Ohta']
    test_color_names = [
        'dark skin',              # Low luminance, reddish
        'light skin',             # Medium, skin tone
        'blue sky',               # Blue
        'foliage',                # Green
        'white 9.5 (.05 D)',      # Near-white
        'neutral 5 (.70 D)',      # Mid-gray
    ]

    llab_test_data = []
    test_count = 0

    for color_name in test_color_names:
        if color_name not in cc_colors:
            print(f"  Warning: '{color_name}' not found in ColorChecker")
            continue

        xyz_in = cc_colors[color_name]
        xyz_0 = d65_white  # Test condition illuminant
        xyz_r = d65_white  # Reference condition illuminant

        # Use average surround for most tests, dim for last one
        surround_idx = 0 if test_count < len(test_color_names) - 1 else 1
        surround_name, surround_code = LLAB_SURROUNDS[surround_idx]
        surround = colour.VIEWING_CONDITIONS_LLAB[surround_name]

        # Compute correlates using colour-science
        specification = colour.XYZ_to_LLAB(
            xyz_in, xyz_0, Y_b,
            L=L,
            surround=surround
        )

        # Extract correlates (LLAB uses J for lightness in colour-science)
        L_out = specification.J
        Ch = specification.C

        # Append to test data: XYZ_in (3), XYZ_0 (3), XYZ_r (3), Y_b, surround, L, Ch
        llab_test_data.extend(xyz_in.tolist())
        llab_test_data.extend(xyz_0.tolist())
        llab_test_data.extend(xyz_r.tolist())
        llab_test_data.append(float(Y_b))
        llab_test_data.append(float(surround_code))
        llab_test_data.append(float(L_out))
        llab_test_data.append(float(Ch))

        test_count += 1

    # Save test data
    filepath = os.path.join(output_dir, 'llab.csv')
    save_test_data(llab_test_data, filepath,
                   f"{test_count} ColorChecker colors, 13 values each")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python llab.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_llab_tests(output_dir)
