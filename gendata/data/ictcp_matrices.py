"""
Generate ICtCp color space matrices.
Source: colour-science ICtCp constants

ICtCp (ITU-R BT.2100) is a color space designed for HDR and WCG content.
Generates: data/ictcp_rgb_to_lms.csv
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


def generate_ictcp_matrices(output_dir):
    """Generate ICtCp transformation matrices."""

    print("\nGenerating ICtCp matrices...")

    # Get ICtCp matrices from colour-science
    from colour.models.rgb.ictcp import MATRIX_ICTCP_RGB_TO_LMS, MATRIX_ICTCP_LMS_TO_RGB

    # ICtCp RGB to LMS matrix (BT.2020 to LMS) from colour-science
    rgb_to_lms = MATRIX_ICTCP_RGB_TO_LMS

    # Save RGB to LMS matrix
    filepath = os.path.join(output_dir, 'ictcp_rgb_to_lms.csv')
    save_matrix(rgb_to_lms, filepath, "ICtCp RGB (BT.2020) to LMS matrix")

    # LMS to RGB inverse matrix from colour-science
    lms_to_rgb = MATRIX_ICTCP_LMS_TO_RGB

    # Save LMS to RGB inverse matrix
    filepath = os.path.join(output_dir, 'ictcp_lms_to_rgb.csv')
    save_matrix(lms_to_rgb, filepath, "ICtCp LMS to RGB (BT.2020) inverse matrix")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python ictcp_matrices.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_ictcp_matrices(output_dir)
