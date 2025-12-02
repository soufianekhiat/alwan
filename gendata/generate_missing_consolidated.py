"""
Consolidated script to generate all missing data files in one run.
This avoids multiple Python startup/import costs and potential hanging issues.
"""

import sys
import os
import numpy as np

# Add parent dir to path for common module
sys.path.insert(0, os.path.dirname(__file__))
from common import save_matrix, save_vector

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)

def main():
    if len(sys.argv) != 2:
        print("Usage: python generate_missing_consolidated.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]

    print("\nConsolidated Missing Data Generation")
    print("=" * 50)

    # Create subdirectories
    os.makedirs(os.path.join(output_dir, 'matrices'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'illuminants'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'illuminants_xy'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'illuminants_spd'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'rgb_spaces'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'colorchecker'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'gamut'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'fixtures'), exist_ok=True)

    # 1. HPE Matrix + Inverse
    print("\n[1/7] HPE Matrix...")
    from colour.appearance.ciecam02 import MATRIX_XYZ_TO_HPE, MATRIX_HPE_TO_XYZ
    hpe = MATRIX_XYZ_TO_HPE
    hpe_inv = MATRIX_HPE_TO_XYZ
    save_matrix(hpe, os.path.join(output_dir, 'matrices', 'hpe.csv'), "Hunt-Pointer-Estevez XYZ to HPE")
    save_matrix(hpe_inv, os.path.join(output_dir, 'matrices', 'hpe_inv.csv'), "HPE to XYZ inverse")
    print("  [OK] HPE matrices generated")

    # 2. IPT Data
    print("\n[2/7] IPT Data...")
    from colour.models.ipt import MATRIX_IPT_XYZ_TO_LMS, MATRIX_IPT_LMS_TO_XYZ

    ipt_exponent = 0.43
    save_vector([ipt_exponent], os.path.join(output_dir, 'ipt_exponent.csv'), "IPT compression exponent")

    xyz_to_lms = MATRIX_IPT_XYZ_TO_LMS
    save_matrix(xyz_to_lms, os.path.join(output_dir, 'ipt_xyz_to_lms.csv'), "IPT XYZ to LMS")

    lms_to_xyz = MATRIX_IPT_LMS_TO_XYZ
    lms_to_ipt = np.linalg.inv(xyz_to_lms)
    save_matrix(lms_to_ipt, os.path.join(output_dir, 'matrices', 'lms_to_ipt_hdr.csv'), "LMS to IPT HDR")

    ipt_to_lms = np.linalg.inv(lms_to_ipt)
    save_matrix(ipt_to_lms, os.path.join(output_dir, 'matrices', 'ipt_to_lms_hdr.csv'), "IPT to LMS inverse")
    print("  [OK] IPT data generated")

    # 3. ICtCp Matrices
    print("\n[3/7] ICtCp Matrices...")
    from colour.models.rgb.ictcp import MATRIX_ICTCP_RGB_TO_LMS, MATRIX_ICTCP_LMS_TO_RGB

    rgb_to_lms = MATRIX_ICTCP_RGB_TO_LMS
    save_matrix(rgb_to_lms, os.path.join(output_dir, 'ictcp_rgb_to_lms.csv'), "ICtCp RGB to LMS")

    lms_to_rgb = MATRIX_ICTCP_LMS_TO_RGB
    save_matrix(lms_to_rgb, os.path.join(output_dir, 'ictcp_lms_to_rgb.csv'), "ICtCp LMS to RGB inverse")
    print("  [OK] ICtCp matrices generated")

    # 4. ACES Matrices
    print("\n[4/7] ACES Matrices...")
    aces_ap0 = colour.RGB_COLOURSPACES['ACES2065-1']
    aces_ap1 = colour.RGB_COLOURSPACES['ACEScg']

    ap0_to_xyz = aces_ap0.matrix_RGB_to_XYZ
    ap1_to_xyz = aces_ap1.matrix_RGB_to_XYZ

    xyz_to_ap0 = np.linalg.inv(ap0_to_xyz)
    ap1_to_ap0 = np.dot(xyz_to_ap0, ap1_to_xyz)

    save_matrix(ap1_to_ap0, os.path.join(output_dir, 'matrices', 'aces_ap1_to_ap0.csv'), "ACES AP1 to AP0")
    print("  [OK] ACES matrices generated")

    # 5. Extended Illuminants
    print("\n[5/7] Extended Illuminants...")
    # Illuminant B
    b_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['B']
    save_vector([b_xy[0], b_xy[1]], os.path.join(output_dir, 'illuminants_xy', 'b_xy.csv'), "Illuminant B xy")

    # Illuminant A extended range
    a_spd = colour.SDS_ILLUMINANTS['A']
    wavelengths = np.arange(360, 831, 1)  # 360-830nm @ 1nm
    a_extended = [float(a_spd[wl]) for wl in wavelengths]
    save_vector(a_extended, os.path.join(output_dir, 'illuminants', 'A_360_830_1nm.csv'),
               f"Illuminant A extended ({len(a_extended)} samples)")
    print("  [OK] Extended illuminants generated")

    # 6. ARRI LogC3
    print("\n[6/7] ARRI LogC3...")
    arri_space = colour.RGB_COLOURSPACES['ARRI Wide Gamut 3']
    primaries = arri_space.primaries
    whitepoint = arri_space.whitepoint

    space_data = [
        primaries[0][0], primaries[0][1],  # Red xy
        primaries[1][0], primaries[1][1],  # Green xy
        primaries[2][0], primaries[2][1],  # Blue xy
        whitepoint[0], whitepoint[1]       # White xy
    ]

    save_vector(space_data, os.path.join(output_dir, 'rgb_spaces', 'arri_logc3.csv'), "ARRI LogC3 color space")
    print("  [OK] ARRI LogC3 generated")

    # 7. Reference Data
    print("\n[7/7] Reference Data...")

    # ColorChecker Classic D50 xyY
    colorchecker = colour.CCS_COLOURCHECKERS['ColorChecker 2005']
    d50_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D50']

    xyy_values = []
    for patch_name, patch_data in colorchecker.items():
        xyy_values.extend([patch_data[0], patch_data[1], patch_data[2]])

    save_vector(xyy_values, os.path.join(output_dir, 'colorchecker', 'classic_d50_xyy.csv'),
               f"ColorChecker Classic ({len(colorchecker)} patches)")

    # TCS 01 reflectance
    from colour.quality import SDS_TCS
    tcs_01_spd = list(SDS_TCS.values())[0]
    tcs_01_reflectance = tcs_01_spd.values
    save_vector(tcs_01_reflectance.tolist(), os.path.join(output_dir, 'fixtures', 'tcs_01_reflectance.csv'),
               f"TCS 01 reflectance ({len(tcs_01_reflectance)} values)")

    # Pointer's gamut boundary
    from colour.models import CCS_POINTER_GAMUT_BOUNDARY
    pointer_gamut = CCS_POINTER_GAMUT_BOUNDARY

    xy_coords = []
    for point in pointer_gamut:
        xy_coords.extend([point[0], point[1]])

    save_vector(xy_coords, os.path.join(output_dir, 'gamut', 'pointer_gamut_boundary_xy.csv'),
               f"Pointer's gamut ({len(pointer_gamut)} points)")

    print("  [OK] Reference data generated")

    print("\n" + "=" * 50)
    print("All missing data files generated successfully!")
    print("=" * 50)

if __name__ == '__main__':
    main()
