"""
Generate only the 3 missing reference data files.
"""

import sys
import os
import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from common import save_vector

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed")
    sys.exit(1)

def main():
    if len(sys.argv) != 2:
        print("Usage: python generate_reference_only.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]

    print("\nGenerating Missing Reference Data Files")
    print("=" * 50)

    # Create subdirectories
    os.makedirs(os.path.join(output_dir, 'colorchecker'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'gamut'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'fixtures'), exist_ok=True)

    # 1. ColorChecker Classic D50 xyY
    print("\n[1/3] ColorChecker Classic...")
    colorchecker = colour.CCS_COLOURCHECKERS['ColorChecker 2005']
    d50_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D50']

    xyy_values = []
    for patch_name, patch_data in colorchecker.data.items():
        xyy_values.extend([patch_data[0], patch_data[1], patch_data[2]])

    filepath = os.path.join(output_dir, 'colorchecker', 'classic_d50_xyy.csv')
    save_vector(xyy_values, filepath, f"ColorChecker Classic ({len(colorchecker.data)} patches)")
    print(f"  Generated: {filepath}")

    # 2. TCS 01 reflectance
    print("\n[2/3] TCS reflectances...")
    from colour.quality import SDS_TCS
    tcs_01_spd = list(SDS_TCS.values())[0]
    tcs_01_reflectance = tcs_01_spd.values

    filepath = os.path.join(output_dir, 'fixtures', 'tcs_01_reflectance.csv')
    save_vector(tcs_01_reflectance.tolist(), filepath,
               f"TCS 01 reflectance ({len(tcs_01_reflectance)} values)")
    print(f"  Generated: {filepath}")

    # 3. Pointer's gamut boundary
    print("\n[3/3] Pointer's gamut...")
    from colour.models import CCS_POINTER_GAMUT_BOUNDARY
    pointer_gamut = CCS_POINTER_GAMUT_BOUNDARY

    xy_coords = []
    for point in pointer_gamut:
        xy_coords.extend([point[0], point[1]])

    filepath = os.path.join(output_dir, 'gamut', 'pointer_gamut_boundary_xy.csv')
    save_vector(xy_coords, filepath, f"Pointer's gamut ({len(pointer_gamut)} points)")
    print(f"  Generated: {filepath}")

    print("\n" + "=" * 50)
    print("All 3 reference data files generated successfully!")
    print("=" * 50)

if __name__ == '__main__':
    main()
