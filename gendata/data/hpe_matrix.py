"""
Generate Hunt-Pointer-Estevez (HPE) matrix.
Source: colour.MATRIX_HPE

Used by: alwan_cam.c, alwan_hunt.c, alwan_kim2009.c, alwan_rlab.c
The HPE matrix is used for CAM conversions (XYZ to LMS).
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


def generate_hpe_matrix(output_dir):
    """Generate HPE matrix."""

    print("\nGenerating HPE matrix...")

    # Get HPE matrix from colour-science
    from colour.appearance.ciecam02 import MATRIX_XYZ_TO_HPE, MATRIX_HPE_TO_XYZ
    hpe = MATRIX_XYZ_TO_HPE
    hpe_inv = MATRIX_HPE_TO_XYZ

    # Save forward matrix
    filepath = os.path.join(output_dir, 'matrices', 'hpe.csv')
    save_matrix(hpe, filepath, "Hunt-Pointer-Estevez matrix (XYZ to LMS)")

    # Save inverse matrix
    filepath = os.path.join(output_dir, 'matrices', 'hpe_inv.csv')
    save_matrix(hpe_inv, filepath, "Hunt-Pointer-Estevez inverse matrix (LMS to XYZ)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python hpe_matrix.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_hpe_matrix(output_dir)
