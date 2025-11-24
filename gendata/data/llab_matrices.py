"""
Generate LLAB transformation matrices.
Source: colour.appearance.llab module constants
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_matrix

# Import colour-science
try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_llab_matrices(output_dir):
    """Generate LLAB XYZ<->RGB transformation matrices from colour-science."""

    print("\nGenerating LLAB matrices...")

    # Get LLAB_XYZ_TO_RGB_MATRIX from colour-science
    # This is defined in colour.appearance.llab module
    from colour.appearance.llab import LLAB_XYZ_TO_RGB_MATRIX, LLAB_RGB_TO_XYZ_MATRIX

    # Save forward matrix
    filepath = os.path.join(output_dir, 'matrices', 'llab_xyz_to_rgb.csv')
    save_matrix(LLAB_XYZ_TO_RGB_MATRIX, filepath, "LLAB XYZ to RGB")

    # Save inverse matrix
    filepath = os.path.join(output_dir, 'matrices', 'llab_rgb_to_xyz.csv')
    save_matrix(LLAB_RGB_TO_XYZ_MATRIX, filepath, "LLAB RGB to XYZ (inverse)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python llab_matrices.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_llab_matrices(output_dir)
