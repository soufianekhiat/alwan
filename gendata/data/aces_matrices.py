"""
Generate ACES transformation matrices.
Source: colour-science RGB color spaces

ACES (Academy Color Encoding System) matrices for AP0 and AP1 primaries.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_matrix

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_aces_matrices(output_dir):
    """Generate ACES transformation matrices."""

    print("\nGenerating ACES transformation matrices...")

    # Get ACES AP0 and AP1 color spaces
    aces_ap0 = colour.RGB_COLOURSPACES['ACES2065-1']
    aces_ap1 = colour.RGB_COLOURSPACES['ACEScg']

    # Get transformation matrices from colour-science
    ap0_to_xyz = aces_ap0.matrix_RGB_to_XYZ
    ap1_to_xyz = aces_ap1.matrix_RGB_to_XYZ
    xyz_to_ap0 = aces_ap0.matrix_XYZ_to_RGB

    # Compute AP1 to AP0 transformation
    # AP1 -> XYZ -> AP0
    ap1_to_ap0 = np.dot(xyz_to_ap0, ap1_to_xyz)

    # Save AP1 to AP0 matrix
    filepath = os.path.join(output_dir, 'matrices', 'aces_ap1_to_ap0.csv')
    save_matrix(ap1_to_ap0, filepath, "ACES AP1 (ACEScg) to AP0 (ACES2065-1) matrix")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python aces_matrices.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_aces_matrices(output_dir)
