"""
Generate Hunt-Pointer-Estevez (HPE) matrix and derived products.
Source: colour-science CIECAM02 constants

Used by: alwan_cam.c, alwan_hunt.c, alwan_kim2009.c, alwan_rlab.c
The HPE matrix is used for CAM conversions (XYZ to LMS).
Also generates the precomputed HPE @ CAT02^-1 product used by CIECAM02.
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
    """Generate HPE matrix and derived products."""

    print("\nGenerating HPE matrices...")

    from colour.appearance.ciecam02 import MATRIX_XYZ_TO_HPE, MATRIX_HPE_TO_XYZ
    hpe = MATRIX_XYZ_TO_HPE
    hpe_inv = MATRIX_HPE_TO_XYZ

    # Save forward and inverse HPE
    save_matrix(hpe, os.path.join(output_dir, 'matrices', 'hpe.csv'),
                "Hunt-Pointer-Estevez matrix (XYZ to LMS)")
    save_matrix(hpe_inv, os.path.join(output_dir, 'matrices', 'hpe_inv.csv'),
                "Hunt-Pointer-Estevez inverse matrix (LMS to XYZ)")

    # Precomputed M_HPE @ M_CAT02^-1 (used by CIECAM02 for opponent signals)
    from colour.adaptation import CAT_CAT02
    cat02_inv = np.linalg.inv(CAT_CAT02)
    hpe_cat02_inv = hpe @ cat02_inv
    cat02_hpe_inv = CAT_CAT02 @ hpe_inv

    save_matrix(hpe_cat02_inv, os.path.join(output_dir, 'matrices', 'hpe_cat02_inv.csv'),
                "M_HPE @ M_CAT02^-1 (CIECAM02 opponent signals)")
    save_matrix(cat02_hpe_inv, os.path.join(output_dir, 'matrices', 'hpe_cat02_inv_inv.csv'),
                "M_CAT02 @ M_HPE^-1 (CIECAM02 inverse)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python hpe_matrix.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_hpe_matrix(output_dir)
