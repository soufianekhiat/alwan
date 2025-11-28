"""
Generate IPT color space data.
Source: colour-science IPT constants

Generates:
- data/ipt_exponent.csv - IPT compression exponent
- data/matrices/lms_to_ipt_hdr.csv - LMS to IPT HDR transformation matrix
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


def generate_ipt_data(output_dir):
    """Generate IPT color space data."""

    print("\nGenerating IPT color space data...")

    # Get IPT matrices from colour-science
    from colour.models.ipt import (
        MATRIX_IPT_XYZ_TO_LMS, MATRIX_IPT_LMS_TO_XYZ,
        MATRIX_IPT_LMS_P_TO_IPT, MATRIX_IPT_IPT_TO_LMS_P
    )

    # IPT exponent (used for compression function)
    # IPT uses a power function: f(x) = sign(x) * |x|^exponent
    # Standard IPT uses exponent = 0.43 (this constant is not available in colour-science)
    ipt_exponent = 0.43

    # Save exponent as single value
    filepath = os.path.join(output_dir, 'ipt_exponent.csv')
    save_vector([ipt_exponent], filepath, "IPT compression exponent")

    # XYZ to LMS transformation matrix from colour-science
    xyz_to_lms = MATRIX_IPT_XYZ_TO_LMS

    # Save XYZ to LMS matrix
    filepath = os.path.join(output_dir, 'ipt_xyz_to_lms.csv')
    save_matrix(xyz_to_lms, filepath, "IPT XYZ to LMS transformation matrix")

    # LMS to XYZ inverse matrix from colour-science
    lms_to_xyz = MATRIX_IPT_LMS_TO_XYZ

    # LMS' to IPT transformation matrix from colour-science
    # (LMS' = LMS after applying power function with exponent 0.43)
    lms_to_ipt = MATRIX_IPT_LMS_P_TO_IPT

    # Save LMS' to IPT matrix
    filepath = os.path.join(output_dir, 'matrices', 'lms_to_ipt_hdr.csv')
    save_matrix(lms_to_ipt, filepath, "LMS' to IPT transformation matrix (from colour-science)")

    # IPT to LMS' inverse matrix from colour-science
    ipt_to_lms = MATRIX_IPT_IPT_TO_LMS_P

    # Save IPT to LMS' inverse matrix
    filepath = os.path.join(output_dir, 'matrices', 'ipt_to_lms_hdr.csv')
    save_matrix(ipt_to_lms, filepath, "IPT to LMS' inverse transformation matrix (from colour-science)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python ipt_data.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_ipt_data(output_dir)
