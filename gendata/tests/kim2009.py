"""
Generate Kim2009 test data.
Source: colour.XYZ_to_Kim2009()

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


def generate_kim2009_tests(output_dir):
    """Generate Kim2009 test cases using colour-science data."""

    print("\nGenerating Kim2009 test data...")

    # D65 white point from colour-science (Y=100 scale)
    d65_white = get_d65_xyz_100()

    # Get ColorChecker colors from colour-science
    cc_colors = get_colorchecker_xyz_d65()

    # Standard adapting luminance: 1000 lux / pi (from photometric standard)
    La = 1000.0 / np.pi

    # Standard background relative luminance (20% gray)
    Yb = 20.0

    # Media and surround from colour-science
    media = colour.MEDIA_PARAMETERS_KIM2009['High-luminance LCD Display']
    surround = colour.VIEWING_CONDITIONS_KIM2009['Average']

    # Select test colors from ColorChecker (diverse set)
    # Names from colour.SDS_COLOURCHECKERS['ColorChecker N Ohta']
    test_color_names = [
        'dark skin',              # Low luminance, reddish
        'light skin',             # Medium, skin tone
        'blue sky',               # Blue
        'foliage',                # Green
        'blue flower',            # Purple-blue
        'bluish green',           # Cyan-green
        'white 9.5 (.05 D)',      # Near-white
        'neutral 5 (.70 D)',      # Mid-gray
    ]

    kim2009_test_data = []
    test_count = 0

    for color_name in test_color_names:
        if color_name not in cc_colors:
            print(f"  Warning: '{color_name}' not found in ColorChecker")
            continue

        xyz_in = cc_colors[color_name]
        xyz_w = d65_white

        # Compute correlates using colour-science
        specification = colour.XYZ_to_Kim2009(
            xyz_in, xyz_w, La,
            media=media,
            surround=surround
        )

        # Extract correlates
        J = specification.J
        C = specification.C
        h = specification.h

        # Append to test data: XYZ_in (3), XYZ_w (3), La, Yb, J, C, h
        kim2009_test_data.extend(xyz_in.tolist())
        kim2009_test_data.extend(xyz_w.tolist())
        kim2009_test_data.append(float(La))
        kim2009_test_data.append(float(Yb))
        kim2009_test_data.append(float(J))
        kim2009_test_data.append(float(C))
        kim2009_test_data.append(float(h))

        test_count += 1

    # Save test data
    filepath = os.path.join(output_dir, 'kim2009.csv')
    save_test_data(kim2009_test_data, filepath,
                   f"{test_count} ColorChecker colors, 11 values each")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python kim2009.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_kim2009_tests(output_dir)
