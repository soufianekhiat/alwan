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
    ('ALEXA Wide Gamut', 'ALEXA Wide Gamut'),

    # RED Camera Spaces
    ('REDcolor', 'REDcolor'),
    ('REDcolor2', 'REDcolor2'),
    ('REDcolor3', 'REDcolor3'),
    ('REDcolor4', 'REDcolor4'),
    ('DRAGONcolor', 'DRAGONcolor'),
    ('DRAGONcolor2', 'DRAGONcolor2'),
    ('REDWideGamutRGB', 'REDWideGamutRGB'),

    # Sony Camera Spaces
    ('Venice S-Gamut3', 'Venice S-Gamut3'),
    ('Venice S-Gamut3.Cine', 'Venice S-Gamut3.Cine'),
    ('S-Gamut', 'S-Gamut'),
    ('S-Gamut3', 'S-Gamut3'),
    ('S-Gamut3.Cine', 'S-Gamut3.Cine'),

    # Fujifilm Camera Spaces
    ('F-Gamut', 'F-Gamut'),

    # Nikon Camera Spaces
    ('N-Gamut', 'N-Gamut'),

    # Panasonic Camera Spaces
    ('V-Gamut', 'V-Gamut'),

    # Canon Camera Spaces
    ('Cinema Gamut', 'Cinema Gamut'),

    # DJI Camera Spaces
    ('DJI D-Gamut', 'DJI D-Gamut'),

    # GoPro Camera Spaces
    ('Protune Native', 'Protune Native'),

    # Blackmagic Camera Spaces
    ('Blackmagic Wide Gamut', 'Blackmagic Wide Gamut'),
    ('DaVinci Wide Gamut', 'DaVinci Wide Gamut'),

    # Professional/Photography
    ('Adobe RGB 1998', 'Adobe RGB (1998)'),
    ('ProPhoto RGB', 'ProPhoto RGB'),
    ('Adobe Wide Gamut RGB', 'Adobe Wide Gamut RGB'),
    ('ROMM RGB', 'ROMM RGB'),
    ('Apple RGB', 'Apple RGB'),
    ('ColorMatch RGB', 'ColorMatch RGB'),

    # Digital Cinema
    ('FilmLight E-Gamut', 'FilmLight E-Gamut'),
    ('DCDM XYZ', 'DCDM XYZ'),
    ('DCI-P3', 'DCI-P3'),
    ('DCI-P3+', 'DCI-P3+'),
    ('P3-D65', 'P3-D65'),
    ('P3-D60', 'P3-D60'),

    # Legacy Broadcast
    ('ITU-R BT.470 - 525', 'ITU-R BT.470 - 525'),
    ('ITU-R BT.470 - 625', 'ITU-R BT.470 - 625'),
    ('SMPTE 240M', 'SMPTE 240M'),
    ('SMPTE C', 'SMPTE C'),
    ('NTSC 1953', 'NTSC (1953)'),
    ('NTSC 1987', 'NTSC (1987)'),
    ('PAL/SECAM', 'PAL/SECAM'),
    ('EBU Tech. 3213-E', 'EBU Tech. 3213-E'),

    # Historical/Reference
    ('CIE RGB', 'CIE RGB'),
    ('Sharp RGB', 'Sharp RGB'),
    ('Best RGB', 'Best RGB'),
    ('Beta RGB', 'Beta RGB'),
    ('Don RGB 4', 'Don RGB 4'),
    ('Ekta Space PS 5', 'Ekta Space PS 5'),
    ('Max RGB', 'Max RGB'),
    ('Russell RGB', 'Russell RGB'),
    ('Xtreme RGB', 'Xtreme RGB'),
    ('ECI RGB v2', 'ECI RGB v2'),

    # RIMM/ROMM family
    ('RIMM RGB', 'RIMM RGB'),
    ('ERIMM RGB', 'ERIMM RGB'),
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

    # Generate srgb_primaries_only.csv (just xy coordinates, no whitepoint)
    filepath = os.path.join(output_dir, 'srgb_primaries_only.csv')
    ensure_dir(filepath)
    with open(filepath, 'w', newline='') as f:
        primaries = srgb.primaries
        values = [
            format_scalar(primaries[0][0]), format_scalar(primaries[0][1]),
            format_scalar(primaries[1][0]), format_scalar(primaries[1][1]),
            format_scalar(primaries[2][0]), format_scalar(primaries[2][1])
        ]
        f.write(','.join(values) + '\n')
    print(f"  {filepath} (primaries only)")

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
