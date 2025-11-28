"""
Generate LLAB transformation matrices.
Source: LLAB(1996) standard matrices - Hunt-Pointer-Estevez normalized to D65

Note: These matrices are defined in the LLAB(1996) paper by Luo et al.
They are the standard Hunt-Pointer-Estevez transformation matrices
normalized for D65 illuminant, used in the LLAB color appearance model.

Reference:
Luo, M. R., Lo, M. C., & Kuo, W. G. (1996). The LLAB(l:c) colour model.
Color Research & Application, 21(6), 412-429.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_matrix

# Import colour-science to verify LLAB implementation exists
try:
    import colour
    # Verify LLAB is available
    from colour.appearance import llab
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_llab_matrices(output_dir):
    """Generate LLAB XYZ<->RGB transformation matrices.

    These are the standard Hunt-Pointer-Estevez matrices normalized to D65,
    as specified in the LLAB(1996) paper.
    """

    print("\nGenerating LLAB matrices...")

    # LLAB XYZ to RGB matrix (Hunt-Pointer-Estevez, D65 normalized)
    # Source: colour-science LLAB implementation
    llab_xyz_to_rgb = llab.MATRIX_XYZ_TO_RGB_LLAB

    # Save forward matrix
    filepath = os.path.join(output_dir, 'matrices', 'llab_xyz_to_rgb.csv')
    save_matrix(llab_xyz_to_rgb, filepath, "LLAB XYZ to RGB (Hunt-Pointer-Estevez, D65)")

    # Get inverse matrix from colour-science
    llab_rgb_to_xyz = llab.MATRIX_RGB_TO_XYZ_LLAB
    filepath = os.path.join(output_dir, 'matrices', 'llab_rgb_to_xyz.csv')
    save_matrix(llab_rgb_to_xyz, filepath, "LLAB RGB to XYZ (inverse)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python llab_matrices.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_llab_matrices(output_dir)
