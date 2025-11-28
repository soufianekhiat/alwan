"""
Generate reference color data.
Source: colour-science

Generates:
- ColorChecker Classic patches (D50, xyY)
- Test Color Sample (TCS) spectral reflectances
- Pointer's gamut boundary (xy chromaticity)
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_vector, save_matrix

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_reference_data(output_dir):
    """Generate reference color data."""

    print("\nGenerating reference color data...")

    # Create output subdirectories
    os.makedirs(os.path.join(output_dir, 'colorchecker'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'gamut'), exist_ok=True)

    # 1. ColorChecker Classic D50 xyY
    print("  Generating ColorChecker Classic patches...")

    # Get ColorChecker data from colour-science
    colorchecker = colour.CCS_COLOURCHECKERS['ColorChecker 2005']

    # Convert to XYZ under D50
    d50_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D50']
    d50_xyz = colour.xy_to_XYZ(d50_xy)

    # Collect xyY values
    xyy_values = []
    for patch_name, patch_data in colorchecker.data.items():
        # patch_data is in xyY format
        xyy_values.extend([patch_data[0], patch_data[1], patch_data[2]])

    filepath = os.path.join(output_dir, 'colorchecker', 'classic_d50_xyy.csv')
    save_vector(xyy_values, filepath, f"{len(colorchecker)} ColorChecker patches (xyY, D50)")

    # 2. Test Color Sample 01 reflectance
    print("  Generating TCS reflectances...")

    # CIE Test Color Samples (TCS) from colour-science
    try:
        # TCS are from CIE 13.3-1995 (used for CRI calculations)
        from colour.quality import SDS_TCS

        # Get TCS 01 (first sample) from colour-science
        tcs_01_spd = list(SDS_TCS.values())[0]  # First TCS sample

        # Extract wavelengths and reflectance values
        wavelengths = tcs_01_spd.wavelengths
        tcs_01_reflectance = tcs_01_spd.values

        filepath = os.path.join(output_dir, 'fixtures', 'tcs_01_reflectance.csv')
        save_vector(tcs_01_reflectance.tolist(), filepath,
                   f"{len(tcs_01_reflectance)} TCS 01 reflectance values ({wavelengths[0]:.0f}-{wavelengths[-1]:.0f}nm)")
    except Exception as e:
        print(f"  WARNING: Could not generate TCS reflectances: {e}")

    # 3. Pointer's gamut boundary
    print("  Generating Pointer's gamut boundary...")

    try:
        # Get Pointer's gamut from colour-science
        from colour.models import CCS_POINTER_GAMUT_BOUNDARY
        pointer_gamut = CCS_POINTER_GAMUT_BOUNDARY

        # Extract xy coordinates (flatten the array)
        xy_coords = []
        for point in pointer_gamut:
            xy_coords.extend([point[0], point[1]])

        filepath = os.path.join(output_dir, 'gamut', 'pointer_gamut_boundary_xy.csv')
        save_vector(xy_coords, filepath, f"{len(pointer_gamut)} Pointer's gamut boundary points (xy)")
    except Exception as e:
        print(f"  WARNING: Could not generate Pointer's gamut: {e}")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python reference_data.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_reference_data(output_dir)
