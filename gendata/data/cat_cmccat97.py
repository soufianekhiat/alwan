"""
Generate CMCCat97 chromatic adaptation transform matrix.
Source: colour-science

CMCCat97 is used in certain color appearance models.
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


def generate_cat_cmccat97(output_dir):
    """Generate CMCCat97 CAT matrix."""

    print("\nGenerating CMCCat97 CAT matrix...")

    # Get CMCCat97 matrix from colour-science
    from colour.adaptation import CAT_CMCCAT97
    cmccat97 = CAT_CMCCAT97

    # Save matrix
    filepath = os.path.join(output_dir, 'matrices', 'cat_cmccat97.csv')
    save_matrix(cmccat97, filepath, "CMCCat97 chromatic adaptation matrix")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python cat_cmccat97.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_cat_cmccat97(output_dir)
