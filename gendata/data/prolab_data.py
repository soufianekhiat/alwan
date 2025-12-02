"""
Generate ProLab color space data.
Source: colour-science ProLab constants

ProLab: Perceptually Uniform Projective Color Coordinate System
Reference: Konovalenko et al. (2021) - arXiv:2012.07653

Generates:
- data/prolab_matrix_q.csv - Projective transformation matrix Q (4x4)
- data/prolab_matrix_q_inv.csv - Inverse projective transformation matrix Q^-1 (4x4)
- data/white_d65_xyz_y1.csv - D65 white point XYZ (Y=1 normalized)
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_matrix, save_vector

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_prolab_data(output_dir):
    """Generate ProLab color space data from colour-science."""

    print("\nGenerating ProLab color space data...")

    # Get ProLab matrices from colour-science
    matrix_q = colour.models.prolab.MATRIX_Q
    matrix_q_inv = colour.models.prolab.MATRIX_INVERSE_Q

    # Save MATRIX_Q (4x4 projective transformation)
    filepath = os.path.join(output_dir, 'prolab_matrix_q.csv')
    save_matrix(matrix_q, filepath, "Projective transformation matrix Q")

    # Save MATRIX_INVERSE_Q (4x4 inverse projective transformation)
    filepath = os.path.join(output_dir, 'prolab_matrix_q_inv.csv')
    save_matrix(matrix_q_inv, filepath, "Inverse projective transformation matrix Q^-1")

    # Get D65 chromaticity from colour-science illuminants
    d65_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']

    # Convert to XYZ with Y=1 (normalized)
    d65_xyz = colour.xyY_to_XYZ(np.array([d65_xy[0], d65_xy[1], 1.0]))

    # Save D65 white point XYZ
    filepath = os.path.join(output_dir, 'white_d65_xyz_y1.csv')
    save_vector(d65_xyz, filepath, "D65 white point XYZ (Y=1 normalized)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python prolab_data.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_prolab_data(output_dir)
