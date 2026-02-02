"""
Generate extended color space transformations.
Includes IgPgTg, ICAcB, IHLS, HDR reference data, and spectral locus.
"""

import sys
import os
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
from common import save_matrix, save_vector

try:
    import colour
    from colour.models.igpgtg import (
        MATRIX_IGPGTG_LMS_P_TO_IGPGTG, MATRIX_IGPGTG_IGPGTG_TO_LMS_P,
        MATRIX_IGPGTG_XYZ_TO_LMS, MATRIX_IGPGTG_LMS_TO_XYZ
    )
    from colour.models.icacb import (
        MATRIX_ICACB_XYZ_TO_LMS, MATRIX_ICACB_LMS_TO_XYZ
    )
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)

def generate_extended_colorspaces(output_dir):
    """Generate extended color space matrices and reference data."""

    print("\nGenerating Extended Color Space Data")
    print("=" * 50)

    os.makedirs(os.path.join(output_dir, 'matrices'), exist_ok=True)
    os.makedirs(os.path.join(output_dir, 'gamut'), exist_ok=True)
    os.makedirs(output_dir, exist_ok=True)

    # ==================================================================
    # IgPgTg Color Space
    # ==================================================================
    print("\n[1/9] IgPgTg color space matrices...")

    lms_p_to_igpgtg = MATRIX_IGPGTG_LMS_P_TO_IGPGTG
    igpgtg_to_lms_p = MATRIX_IGPGTG_IGPGTG_TO_LMS_P
    xyz_to_lms_igpgtg = MATRIX_IGPGTG_XYZ_TO_LMS
    lms_to_xyz_igpgtg = MATRIX_IGPGTG_LMS_TO_XYZ

    save_matrix(lms_p_to_igpgtg, os.path.join(output_dir, 'matrices', 'lms_to_igpgtg.csv'), "LMS' to IgPgTg")
    save_matrix(igpgtg_to_lms_p, os.path.join(output_dir, 'matrices', 'igpgtg_to_lms.csv'), "IgPgTg to LMS'")
    save_matrix(xyz_to_lms_igpgtg, os.path.join(output_dir, 'matrices', 'xyz_to_lms_igpgtg.csv'), "XYZ to LMS for IgPgTg")
    save_matrix(lms_to_xyz_igpgtg, os.path.join(output_dir, 'matrices', 'lms_to_xyz_igpgtg.csv'), "LMS to XYZ for IgPgTg")
    print("  [OK] IgPgTg matrices generated (4 files)")

    # IgPgTg LMS scale factor
    print("\n[2/9] IgPgTg LMS scale factor...")
    # Extract from colour-science XYZ_to_IgPgTg function source
    import inspect
    src = inspect.getsource(colour.XYZ_to_IgPgTg)
    # Find the line with the scale factor: "np.array([18.36, 21.46, 19435])"
    for line in src.split('\n'):
        if '18.36' in line and 'np.array' in line:
            # Extract the scale values from colour-science source
            import re
            match = re.search(r'np\.array\(\[([\d.,\s]+)\]\)', line)
            if match:
                values_str = match.group(1)
                igpgtg_lms_scale = np.array([float(x.strip()) for x in values_str.split(',')])
                break
    else:
        # Fallback: if extraction fails, error out to avoid hardcoding
        raise ValueError("Could not extract IgPgTg LMS scale from colour-science")

    save_vector(igpgtg_lms_scale.tolist(), os.path.join(output_dir, 'igpgtg_lms_scale.csv'), "IgPgTg LMS scale")
    print(f"  [OK] IgPgTg scale generated ({igpgtg_lms_scale.tolist()})")

    # ==================================================================
    # ICAcB Color Space
    # ==================================================================
    print("\n[3/9] ICAcB color space matrices...")

    xyz_to_lms_icacb = MATRIX_ICACB_XYZ_TO_LMS
    lms_to_xyz_icacb = MATRIX_ICACB_LMS_TO_XYZ

    # In colour-science ICAcB implementation:
    # - MATRIX_ICACB_XYZ_TO_LMS: XYZ to LMS transformation
    # - MATRIX_ICACB_XYZ_TO_LMS_2: LMS' to ICAcB opponent color matrix
    # The second matrix is the opponent color transformation
    from colour.models.icacb import MATRIX_ICACB_XYZ_TO_LMS_2, MATRIX_ICACB_LMS_TO_XYZ_2

    lms_to_icacb = MATRIX_ICACB_XYZ_TO_LMS_2  # This is the LMS' to ICAcB matrix
    icacb_to_lms = MATRIX_ICACB_LMS_TO_XYZ_2  # This is the inverse

    save_matrix(lms_to_icacb, os.path.join(output_dir, 'matrices', 'lms_to_icacb.csv'), "LMS' to ICAcB")
    save_matrix(icacb_to_lms, os.path.join(output_dir, 'matrices', 'icacb_to_lms.csv'), "ICAcB to LMS'")
    save_matrix(xyz_to_lms_icacb, os.path.join(output_dir, 'matrices', 'xyz_to_lms_icacb.csv'), "XYZ to LMS for ICAcB")
    save_matrix(lms_to_xyz_icacb, os.path.join(output_dir, 'matrices', 'lms_to_xyz_icacb.csv'), "LMS to XYZ for ICAcB")
    print("  [OK] ICAcB matrices generated (4 files)")

    # ==================================================================
    # IHLS Color Space
    # ==================================================================
    print("\n[4/9] IHLS color space matrices...")

    # Extract IHLS matrices from colour-science function globals
    from colour import RGB_to_IHLS, IHLS_to_RGB
    rgb_to_yc1c2 = RGB_to_IHLS.__globals__['MATRIX_RGB_TO_YC_1_C_2']
    yc1c2_to_rgb = IHLS_to_RGB.__globals__['MATRIX_YC_1_C_2_TO_RGB']

    save_matrix(rgb_to_yc1c2, os.path.join(output_dir, 'ihls_rgb_to_yc1c2.csv'), "IHLS RGB to YC1C2")
    save_matrix(yc1c2_to_rgb, os.path.join(output_dir, 'ihls_yc1c2_to_rgb.csv'), "IHLS YC1C2 to RGB")
    print("  [OK] IHLS matrices generated (2 files)")

    # ==================================================================
    # HDR Reference Data
    # ==================================================================
    print("\n[5/9] HDR reference white point...")

    # D65 white point for HDR reference (normalized to 1.0)
    d65_white = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']
    # Convert xy to XYZ with Y=1.0
    d65_xyz = colour.xy_to_XYZ(d65_white)
    save_vector(d65_xyz.tolist(), os.path.join(output_dir, 'hdr_d65_white.csv'), "HDR D65 white point")
    print("  [OK] HDR reference white generated")

    # ==================================================================
    # Spectral Locus
    # ==================================================================
    print("\n[6/9] Spectral locus gamut boundary...")

    # Generate spectral locus from monochromatic wavelengths (360-830nm @ 1nm)
    wavelengths = np.arange(360, 831, 1)

    # Compute xy coordinates for each monochromatic wavelength
    spectral_locus_xy = []
    for wl in wavelengths:
        # Create a monochromatic SPD
        cmfs = colour.MSDS_CMFS['CIE 1931 2 Degree Standard Observer']
        wl_range = cmfs.wavelengths

        if wl in wl_range:
            # Get the XYZ values for this wavelength from CMFs
            xyz = cmfs[wl]
            # Normalize to get xy chromaticity
            xy = colour.XYZ_to_xy(xyz)
            spectral_locus_xy.extend([xy[0], xy[1]])

    filepath = os.path.join(output_dir, 'gamut', 'spectral_locus_xy_only_360_830_1nm.csv')
    save_vector(spectral_locus_xy, filepath, f"Spectral locus ({len(wavelengths)} points)")
    print(f"  [OK] Spectral locus generated ({len(wavelengths)} wavelengths)")

    print("\n" + "=" * 50)
    print("Extended color space data generated successfully!")
    print("  - IgPgTg: 5 files (4 matrices + 1 scale)")
    print("  - ICAcB: 4 files (matrices)")
    print("  - IHLS: 2 files (RGB<->YC1C2)")
    print("  - HDR: 1 file (D65 white)")
    print("  - Spectral locus: 1 file (gamut boundary)")
    print("  Total: 13 files")
    print("=" * 50)

    return True

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python extended_colorspaces.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    success = generate_extended_colorspaces(output_dir)
    sys.exit(0 if success else 1)
