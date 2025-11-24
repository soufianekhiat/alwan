"""
Generate RGB color space definitions.
Source: colour.RGB_COLOURSPACES

NO HARDCODED VALUES - all data from colour-science.
RGB space list is the only configuration (inputs), values come from colour-science.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import ensure_dir, format_scalar

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


# RGB color spaces to generate (filename, colour-science name)
# This is CONFIGURATION (inputs) - actual values come from colour-science
RGB_SPACES = [
    # Core spaces (M1)
    ('sRGB', 'sRGB'),
    ('BT.709', 'ITU-R BT.709'),
    ('Display P3', 'Display P3'),
    ('BT.2020', 'ITU-R BT.2020'),
    ('ACES2065-1', 'ACES2065-1'),
    ('ACEScg', 'ACEScg'),
    ('ACESproxy', 'ACESproxy'),

    # ACES Family
    ('ACEScc', 'ACEScc'),
    ('ACEScct', 'ACEScct'),

    # ARRI Camera Spaces
    ('ARRI Wide Gamut 3', 'ARRI Wide Gamut 3'),
    ('ARRI Wide Gamut 4', 'ARRI Wide Gamut 4'),

    # RED Camera Spaces
    ('REDcolor', 'REDcolor'),
    ('REDcolor2', 'REDcolor2'),
    ('REDcolor3', 'REDcolor3'),
    ('REDcolor4', 'REDcolor4'),
    ('DRAGONcolor', 'DRAGONcolor'),
    ('DRAGONcolor2', 'DRAGONcolor2'),

    # Sony Camera Spaces
    ('Venice S-Gamut3', 'Venice S-Gamut3'),
    ('Venice S-Gamut3.Cine', 'Venice S-Gamut3.Cine'),

    # Professional/Photography
    ('Adobe RGB 1998', 'Adobe RGB (1998)'),
    ('ProPhoto RGB', 'ProPhoto RGB'),
    ('Adobe Wide Gamut RGB', 'Adobe Wide Gamut RGB'),
    ('ROMM RGB', 'ROMM RGB'),

    # Digital Cinema
    ('FilmLight E-Gamut', 'FilmLight E-Gamut'),
    ('DCDM XYZ', 'DCDM XYZ'),

    # Legacy Broadcast
    ('ITU-R BT.470 - 525', 'ITU-R BT.470 - 525'),
    ('ITU-R BT.470 - 625', 'ITU-R BT.470 - 625'),
    ('SMPTE 240M', 'SMPTE 240M'),
    ('SMPTE C', 'SMPTE C'),

    # Historical/Reference
    ('CIE RGB', 'CIE RGB'),
    ('Sharp RGB', 'Sharp RGB'),
    ('Best RGB', 'Best RGB'),
    ('Beta RGB', 'Beta RGB'),
]


def write_rgb_space_csv(filepath, space):
    """Write RGB space definition to CSV (primaries + whitepoint)."""
    ensure_dir(filepath)
    with open(filepath, 'w', newline='') as f:
        primaries = space.primaries
        whitepoint = space.whitepoint

        # Format: rx,ry,gx,gy,bx,by,wx,wy
        values = [
            format_scalar(primaries[0][0]), format_scalar(primaries[0][1]),
            format_scalar(primaries[1][0]), format_scalar(primaries[1][1]),
            format_scalar(primaries[2][0]), format_scalar(primaries[2][1]),
            format_scalar(whitepoint[0]), format_scalar(whitepoint[1])
        ]
        f.write(','.join(values) + '\n')


def generate_rgb_spaces(output_dir):
    """Generate RGB color space definitions."""

    print("\nGenerating RGB color spaces...")

    # Generate sRGB primaries (legacy format for backward compatibility)
    srgb = colour.RGB_COLOURSPACES['sRGB']
    filepath = os.path.join(output_dir, 'srgb_primaries_3x2.csv')
    ensure_dir(filepath)
    with open(filepath, 'w', newline='') as f:
        primaries = srgb.primaries
        values = [
            format_scalar(primaries[0][0]), format_scalar(primaries[0][1]),
            format_scalar(primaries[1][0]), format_scalar(primaries[1][1]),
            format_scalar(primaries[2][0]), format_scalar(primaries[2][1])
        ]
        f.write(','.join(values) + '\n')
    print(f"  {filepath} (legacy format)")

    # Generate full RGB space definitions
    for filename, space_name in RGB_SPACES:
        try:
            # Get space from colour-science (NO HARDCODED VALUES)
            space = colour.RGB_COLOURSPACES[space_name]

            # Generate filename
            safe_filename = filename.lower().replace(" ", "_").replace(".", "")
            filepath = os.path.join(output_dir, 'rgb_spaces', f'{safe_filename}.csv')

            write_rgb_space_csv(filepath, space)

            primaries = space.primaries
            whitepoint = space.whitepoint
            print(f"  {filepath}")
            print(f"    Primaries: R({primaries[0][0]:.4f}, {primaries[0][1]:.4f}), "
                  f"G({primaries[1][0]:.4f}, {primaries[1][1]:.4f}), "
                  f"B({primaries[2][0]:.4f}, {primaries[2][1]:.4f})")
            print(f"    Whitepoint: ({whitepoint[0]:.4f}, {whitepoint[1]:.4f})")

        except KeyError:
            print(f"  WARNING: RGB space '{space_name}' not found in colour-science")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python rgb_spaces.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_rgb_spaces(output_dir)
