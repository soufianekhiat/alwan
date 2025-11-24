"""
Generate Chromatic Adaptation Transform (CAT) matrices.
Source: colour.CHROMATIC_ADAPTATION_TRANSFORMS
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


def generate_cat_matrices(output_dir):
    """Generate CAT matrices from colour-science."""

    print("\nGenerating CAT matrices...")

    # List of CAT transforms to generate
    cat_transforms = ['CAT02', 'CAT16', 'Bradford', 'Von Kries', 'XYZ Scaling', 'Sharp']

    for cat_name in cat_transforms:
        if cat_name not in colour.CHROMATIC_ADAPTATION_TRANSFORMS:
            print(f"  WARNING: {cat_name} not found in colour-science")
            continue

        # Get matrix from colour-science
        cat_matrix = colour.CHROMATIC_ADAPTATION_TRANSFORMS[cat_name]

        # Save forward matrix
        filename = cat_name.lower().replace(' ', '_')
        filepath = os.path.join(output_dir, 'matrices', f'cat_{filename}.csv')
        save_matrix(cat_matrix, filepath, f"{cat_name} CAT")

        # Save inverse matrix
        cat_inv_matrix = np.linalg.inv(cat_matrix)
        filepath = os.path.join(output_dir, 'matrices', f'cat_{filename}_inv.csv')
        save_matrix(cat_inv_matrix, filepath, f"{cat_name} CAT inverse")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python cat_matrices.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_cat_matrices(output_dir)
