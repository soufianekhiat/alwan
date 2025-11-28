"""
Generate gamut mapping test fixtures.
Source: numpy.clip() for simple clipping, custom algorithm for hue-preserving

Out-of-gamut test RGB colors are hardcoded (inputs).
Mapped RGB values are computed from algorithms.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_vector, format_scalar

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_gamut_mapping_fixtures(output_dir):
    """Generate gamut mapping test fixtures."""

    print("\nGenerating gamut mapping test fixtures...")

    # Get sRGB luminance weights from colour-science
    srgb_space = colour.RGB_COLOURSPACES['sRGB']
    # Y row of RGB_to_XYZ matrix contains luminance weights
    luminance_weights = srgb_space.matrix_RGB_to_XYZ[1]

    # Out-of-gamut test colors (INPUTS - hardcoded)
    # RGB values outside [0,1]
    out_of_gamut_colors = [
        [-0.2, 0.5, 0.8],    # Negative R
        [0.5, -0.1, 0.6],    # Negative G
        [0.3, 0.7, -0.3],    # Negative B
        [1.5, 0.5, 0.3],     # R > 1
        [0.4, 1.8, 0.6],     # G > 1
        [0.2, 0.3, 2.0],     # B > 1
        [-0.3, 1.5, 0.7],    # Mixed out of gamut
        [0.5, 0.5, 0.5],     # In gamut (control)
    ]

    # Generate clipped results (simple clipping to [0,1])
    print("  Generating clip mapping results...")
    gamut_map_clip_results = []
    for rgb in out_of_gamut_colors:
        rgb_array = np.array(rgb)
        # Clip mapping: simple clamp to [0,1]
        clipped = np.clip(rgb_array, 0.0, 1.0)
        gamut_map_clip_results.extend(rgb)  # Input
        gamut_map_clip_results.extend(clipped.tolist())  # Output

    filepath = os.path.join(output_dir, 'fixtures', 'gamut_map_clip.csv')
    save_vector(gamut_map_clip_results, filepath, f"Clip mapping ({len(out_of_gamut_colors)} colors)")

    # Generate hue-preserving mapping results
    # For simplicity, we'll use a basic algorithm: scale towards gray while preserving ratios
    print("  Generating hue-preserving mapping results...")
    gamut_map_hue_results = []
    for rgb in out_of_gamut_colors:
        rgb_array = np.array(rgb)

        # If already in gamut, return as-is
        if np.all(rgb_array >= 0) and np.all(rgb_array <= 1):
            mapped = rgb_array
        else:
            # Compute luminance using sRGB weights from colour-science
            L = np.dot(luminance_weights, rgb_array)
            L_clamped = np.clip(L, 0.0, 1.0)
            neutral = np.array([L_clamped, L_clamped, L_clamped])

            # Binary search for largest t where t*rgb + (1-t)*neutral is in [0,1]^3
            t_min = 0.0
            t_max = 1.0
            mapped = neutral  # fallback

            for _ in range(20):
                t = (t_min + t_max) * 0.5
                test = t * rgb_array + (1.0 - t) * neutral
                if np.all(test >= 0) and np.all(test <= 1):
                    t_min = t
                    mapped = test
                else:
                    t_max = t

        gamut_map_hue_results.extend(rgb)  # Input
        gamut_map_hue_results.extend(mapped.tolist())  # Output

    filepath = os.path.join(output_dir, 'fixtures', 'gamut_map_hue_preserving.csv')
    save_vector(gamut_map_hue_results, filepath, f"Hue-preserving mapping ({len(out_of_gamut_colors)} colors)")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python gamut_mapping_fixtures.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_gamut_mapping_fixtures(output_dir)
