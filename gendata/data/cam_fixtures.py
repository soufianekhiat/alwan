"""
Generate Color Appearance Model (CAM) test fixtures for CIECAM02 and CAM16.
Source: colour.XYZ_to_CIECAM02, colour.XYZ_to_CAM16

Test XYZ colors are hardcoded (inputs).
CAM correlates are computed from colour-science.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_vector, format_scalar

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


# Test XYZ colors (inputs - hardcoded, Y=100 scale)
# Will be computed relative to XYZ_w
def get_test_colors(XYZ_w):
    """Get test colors including the exact white point."""
    return [
        list(XYZ_w),                      # D65 white (exact match)
        [50.0, 50.0, 50.0],               # Mid-gray
        [41.2456, 21.2673, 1.9334],       # sRGB red (D65)
        [35.7576, 71.5152, 11.9192],      # sRGB green
        [18.0437, 7.2175, 95.0304],       # sRGB blue
        [77.0, 92.8, 10.1],               # Lime
        [31.4, 15.9, 9.8],                # Brown
    ]


def generate_ciecam02_fixtures(output_dir):
    """Generate CIECAM02 test fixtures."""

    print("\nGenerating CIECAM02 test fixtures...")

    # Get D65 white point from colour-science
    observer = 'CIE 1931 2 Degree Standard Observer'
    d65_xy = colour.CCS_ILLUMINANTS[observer]['D65']
    d65_xyz = colour.xy_to_XYZ(d65_xy)

    # Scale to Y=100 (standard viewing)
    XYZ_w = d65_xyz * 100
    L_A = 64.0   # Adapting luminance (cd/m²)
    Y_b = 20.0   # Background luminance

    # Get viewing conditions from colour-science
    surround = colour.VIEWING_CONDITIONS_CIECAM02['Average']

    # Save viewing conditions
    filepath = os.path.join(output_dir, 'fixtures', 'cam_viewing_conditions.csv')
    vc_values = list(XYZ_w) + [L_A, Y_b]
    save_vector(vc_values, filepath, "CAM viewing conditions (XYZ_w, L_A, Y_b)")

    # Get test colors
    test_xyz = get_test_colors(XYZ_w)

    # Save test XYZ colors
    flat_xyz = []
    for xyz in test_xyz:
        flat_xyz.extend(xyz)
    filepath = os.path.join(output_dir, 'fixtures', 'ciecam02_xyz_input.csv')
    save_vector(flat_xyz, filepath, f"{len(test_xyz)} test XYZ colors")

    # Compute CIECAM02 correlates from colour-science
    correlates = []
    for xyz in test_xyz:
        spec = colour.XYZ_to_CIECAM02(np.array(xyz), XYZ_w, L_A, Y_b, surround)
        # Store J, C, h, Q, M, s, H
        correlates.append([spec.J, spec.C, spec.h, spec.Q, spec.M, spec.s, spec.H])

    # Save correlates
    flat_corr = []
    for corr in correlates:
        flat_corr.extend(corr)
    filepath = os.path.join(output_dir, 'fixtures', 'ciecam02_correlates.csv')
    save_vector(flat_corr, filepath, f"{len(correlates)} CIECAM02 correlates (J,C,h,Q,M,s,H)")

    # Test inverse transform: correlates -> XYZ
    xyz_reconstructed = []
    for corr in correlates:
        spec = colour.CAM_Specification_CIECAM02(J=corr[0], C=corr[1], h=corr[2])
        xyz_recon = colour.CIECAM02_to_XYZ(spec, XYZ_w, L_A, Y_b, surround)
        xyz_reconstructed.extend(xyz_recon)

    filepath = os.path.join(output_dir, 'fixtures', 'ciecam02_xyz_reconstructed.csv')
    save_vector(xyz_reconstructed, filepath, "Reconstructed XYZ (inverse transform)")


def generate_cam16_fixtures(output_dir):
    """Generate CAM16 test fixtures."""

    print("\nGenerating CAM16 test fixtures...")

    # Use same viewing conditions as CIECAM02 for consistency
    observer = 'CIE 1931 2 Degree Standard Observer'
    d65_xy = colour.CCS_ILLUMINANTS[observer]['D65']
    d65_xyz = colour.xy_to_XYZ(d65_xy)

    XYZ_w = d65_xyz * 100
    L_A = 64.0
    Y_b = 20.0

    surround = colour.VIEWING_CONDITIONS_CAM16['Average']

    # Get test colors (same as CIECAM02)
    test_xyz = get_test_colors(XYZ_w)

    # Compute CAM16 correlates from colour-science
    correlates = []
    for xyz in test_xyz:
        spec = colour.XYZ_to_CAM16(np.array(xyz), XYZ_w, L_A, Y_b, surround)
        correlates.append([spec.J, spec.C, spec.h, spec.Q, spec.M, spec.s, spec.H])

    # Save correlates
    flat_corr = []
    for corr in correlates:
        flat_corr.extend(corr)
    filepath = os.path.join(output_dir, 'fixtures', 'cam16_correlates.csv')
    save_vector(flat_corr, filepath, f"{len(correlates)} CAM16 correlates (J,C,h,Q,M,s,H)")

    # Test inverse transform
    xyz_reconstructed = []
    for corr in correlates:
        spec = colour.CAM_Specification_CAM16(J=corr[0], C=corr[1], h=corr[2])
        xyz_recon = colour.CAM16_to_XYZ(spec, XYZ_w, L_A, Y_b, surround)
        xyz_reconstructed.extend(xyz_recon)

    filepath = os.path.join(output_dir, 'fixtures', 'cam16_xyz_reconstructed.csv')
    save_vector(xyz_reconstructed, filepath, "Reconstructed XYZ (inverse transform)")

    # CAM16-UCS transform
    ucs_jab = []
    for corr in correlates:
        J = corr[0]
        M = corr[4]
        h = corr[2]

        # CAM16-UCS formulas
        J_prime = 1.7 * J / (1.0 + 0.007 * J)
        M_prime = (1.0 / 0.0228) * np.log(1.0 + 0.0228 * M)
        h_rad = np.radians(h)
        a_prime = M_prime * np.cos(h_rad)
        b_prime = M_prime * np.sin(h_rad)

        ucs_jab.extend([J_prime, a_prime, b_prime])

    filepath = os.path.join(output_dir, 'fixtures', 'cam16_ucs_jab.csv')
    save_vector(ucs_jab, filepath, "CAM16-UCS Jab coordinates")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python cam_fixtures.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_ciecam02_fixtures(output_dir)
    generate_cam16_fixtures(output_dir)
