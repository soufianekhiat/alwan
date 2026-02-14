"""
Generate precomputed RGB<->XYZ matrices (NPM and inverse) for all RGB spaces.
Source: colour-science normalised_primary_matrix()

For each RGB space, computes:
  - rgb_to_xyz: Normalised Primary Matrix (NPM)
  - xyz_to_rgb: Inverse NPM
and saves as a single 18-value CSV (9 rgb_to_xyz + 9 xyz_to_rgb).
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
import colour
from common import save_vector

# Import the complete filename->colour-science mapping
from rgb_spaces_complete import RGB_SPACE_MAPPINGS

# Overrides for spaces not found in colour-science under their mapped name.
# These use the same primaries/whitepoint as their base gamut.
_MATRIX_OVERRIDES = {
    'arri_logc3': 'ARRI Wide Gamut 3',  # LogC3 uses WG3 primaries
}


def generate_rgb_matrices(output_dir):
    """Generate precomputed NPM and inverse NPM for all RGB spaces."""

    print("\nGenerating RGB Space Matrices (NPM + inverse)...")
    print("=" * 70)

    matrices_dir = os.path.join(output_dir, 'rgb_matrices')
    os.makedirs(matrices_dir, exist_ok=True)

    generated = 0
    failed = []

    # Merge overrides into mappings
    mappings = dict(RGB_SPACE_MAPPINGS)
    mappings.update(_MATRIX_OVERRIDES)

    for filename_base, cs_name in mappings.items():
        filepath = os.path.join(matrices_dir, f'{filename_base}.csv')

        try:
            cs = colour.RGB_COLOURSPACES[cs_name]

            # Compute NPM (RGB -> XYZ) from primaries and whitepoint
            npm = colour.normalised_primary_matrix(cs.primaries, cs.whitepoint)
            npm_inv = np.linalg.inv(npm)

            # Concatenate: 9 rgb_to_xyz values + 9 xyz_to_rgb values = 18 total
            data = np.concatenate([npm.flatten(), npm_inv.flatten()])

            save_vector(data, filepath,
                        f"{filename_base} NPM + inverse (from {cs_name})")
            generated += 1

        except KeyError:
            failed.append((filename_base, cs_name))
            print(f"  [WARNING] '{cs_name}' not found in colour-science for '{filename_base}'")
        except Exception as e:
            failed.append((filename_base, str(e)))
            print(f"  [ERROR] Failed to generate '{filename_base}': {e}")

    print("\n" + "=" * 70)
    print(f"RGB Space Matrices Generation Complete!")
    print(f"  Generated: {generated} files")
    print(f"  Failed: {len(failed)} files")

    if failed:
        print("\nFailed files:")
        for item in failed:
            print(f"  - {item[0]}: {item[1]}")

    print("=" * 70)

    return len(failed) == 0


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python rgb_matrices.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    success = generate_rgb_matrices(output_dir)
    sys.exit(0 if success else 1)
