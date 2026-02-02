"""
Generate Fairchild chromatic adaptation transform matrix.
Source: colour.MATRIX_FAIRCHILD

The Fairchild CAT is used in certain color appearance models.
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


def generate_cat_fairchild(output_dir):
    """Generate Fairchild CAT matrix."""

    print("\nGenerating Fairchild CAT matrix...")

    # Get Fairchild matrix from colour-science
    from colour.adaptation import CAT_FAIRCHILD
    fairchild = CAT_FAIRCHILD

    # Save matrix
    filepath = os.path.join(output_dir, 'matrices', 'cat_fairchild.csv')
    save_matrix(fairchild, filepath, "Fairchild chromatic adaptation matrix")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python cat_fairchild.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_cat_fairchild(output_dir)
