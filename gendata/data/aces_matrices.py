"""
Generate ACES transformation matrices.
Source: colour-science RGB color spaces

ACES (Academy Color Encoding System) matrices for AP0 and AP1 primaries,
Bradford chromatic adaptation, and display NPMs.

All matrices are computed from primaries/whitepoints (not pre-stored) to
ensure full precision and self-consistency across the pipeline.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
import colour
from common import save_matrix


def _npm(cs):
    """Compute NPM from primaries and whitepoint (full precision)."""
    return colour.normalised_primary_matrix(cs.primaries, cs.whitepoint)


def generate_aces_matrices(output_dir):
    """Generate ACES transformation matrices."""

    print("\nGenerating ACES transformation matrices...")

    matrices_dir = os.path.join(output_dir, 'matrices')

    # Get color spaces
    aces_ap0 = colour.RGB_COLOURSPACES['ACES2065-1']
    aces_ap1 = colour.RGB_COLOURSPACES['ACEScg']
    srgb = colour.RGB_COLOURSPACES['sRGB']
    p3d65 = colour.RGB_COLOURSPACES['Display P3']
    bt2020 = colour.RGB_COLOURSPACES['ITU-R BT.2020']

    # Compute NPMs from primaries for maximum precision
    npm_ap0 = _npm(aces_ap0)
    npm_ap1 = _npm(aces_ap1)

    # AP0 <-> AP1 (derived from NPMs for self-consistency)
    ap0_to_ap1 = np.linalg.inv(npm_ap1) @ npm_ap0
    ap1_to_ap0 = np.linalg.inv(npm_ap0) @ npm_ap1

    save_matrix(ap0_to_ap1, os.path.join(matrices_dir, 'aces_ap0_to_ap1.csv'),
                "AP0 (ACES2065-1) to AP1 (ACEScg) matrix")
    save_matrix(ap1_to_ap0, os.path.join(matrices_dir, 'aces_ap1_to_ap0.csv'),
                "AP1 (ACEScg) to AP0 (ACES2065-1) matrix")

    # Bradford chromatic adaptation D60 <-> D65
    # Use the ACES whitepoint (not CIE D60 -- they differ slightly)
    aces_wp = aces_ap0.whitepoint
    d65 = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']

    d60_to_d65 = colour.adaptation.matrix_chromatic_adaptation_VonKries(
        colour.xy_to_XYZ(aces_wp), colour.xy_to_XYZ(d65), 'Bradford')
    d65_to_d60 = np.linalg.inv(d60_to_d65)

    save_matrix(d60_to_d65, os.path.join(matrices_dir, 'aces_d60_to_d65_bradford.csv'),
                "D60 to D65 Bradford chromatic adaptation")
    save_matrix(d65_to_d60, os.path.join(matrices_dir, 'aces_d65_to_d60_bradford.csv'),
                "D65 to D60 Bradford chromatic adaptation")

    # AP1 NPM and inverse
    save_matrix(npm_ap1, os.path.join(matrices_dir, 'aces_ap1_to_xyz_d60.csv'),
                "AP1 (ACEScg) to XYZ D60 NPM")
    save_matrix(np.linalg.inv(npm_ap1), os.path.join(matrices_dir, 'aces_xyz_d60_to_ap1.csv'),
                "XYZ D60 to AP1 (ACEScg) NPM inverse")

    # Display color space NPMs and inverse NPMs
    # Compute from primaries for full precision (sRGB pre-stored matrix is
    # rounded per IEC 61966-2-1; others are fine but we stay consistent)
    for cs, prefix, label in [
        (srgb,   'rec709',  'sRGB/Rec.709'),
        (p3d65,  'p3d65',   'Display P3'),
        (bt2020, 'rec2020', 'BT.2020'),
    ]:
        npm = _npm(cs)
        npm_inv = np.linalg.inv(npm)
        save_matrix(npm_inv, os.path.join(matrices_dir, f'aces_xyz_to_{prefix}.csv'),
                    f"XYZ to {label} inverse NPM")
        save_matrix(npm, os.path.join(matrices_dir, f'aces_{prefix}_to_xyz.csv'),
                    f"{label} to XYZ NPM")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python aces_matrices.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_aces_matrices(output_dir)
