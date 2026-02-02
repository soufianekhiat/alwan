"""
Generate illuminant data (xy coordinates and SPDs).
Source: colour.CCS_ILLUMINANTS, colour.SDS_ILLUMINANTS
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

import numpy as np
from common import save_vector, ensure_dir, format_scalar

try:
    import colour
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_illuminant_xy_coordinates(output_dir):
    """Generate xy chromaticity coordinates for standard illuminants."""

    print("\nGenerating illuminant xy chromaticity coordinates...")

    # Standard illuminants to generate
    illuminants = {
        'a': 'A',
        'c': 'C',
        'd50': 'D50',
        'd55': 'D55',
        'd60': 'D60',
        'd65': 'D65',
        'd75': 'D75',
        'e': 'E',
        'f2': 'FL2',
        'f7': 'FL7',
        'f11': 'FL11'
    }

    observer = 'CIE 1931 2 Degree Standard Observer'

    for filename, illuminant_name in illuminants.items():
        try:
            xy = colour.CCS_ILLUMINANTS[observer][illuminant_name]
            filepath = os.path.join(output_dir, 'illuminants_xy', f'{filename}_xy.csv')
            save_vector([xy[0], xy[1]], filepath, f"{illuminant_name}")
        except KeyError:
            print(f"  WARNING: Illuminant '{illuminant_name}' not found")


def generate_illuminant_spds(output_dir):
    """Generate Spectral Power Distributions for illuminants."""

    print("\nGenerating illuminant SPDs...")

    # Illuminants with SPD data
    illuminants_spd = {
        'a': 'A',
        'd50': 'D50',
        'd55': 'D55',
        'd65': 'D65',
        'd75': 'D75',
        'f2': 'FL2',
        'f7': 'FL7',
        'f11': 'FL11'
    }

    for filename, illuminant_name in illuminants_spd.items():
        try:
            spd = colour.SDS_ILLUMINANTS[illuminant_name]

            # Extract wavelengths and values
            wavelengths = spd.wavelengths
            values = spd.values

            # Save SPD data (wavelength, value pairs)
            filepath = os.path.join(output_dir, 'illuminants_spd', f'{filename}_spd.csv')
            ensure_dir(filepath)

            with open(filepath, 'w', newline='') as f:
                # Format: wavelength1,value1,wavelength2,value2,...
                spd_data = []
                for wl, val in zip(wavelengths, values):
                    spd_data.append(format_scalar(wl))
                    spd_data.append(format_scalar(val))
                f.write(','.join(spd_data) + '\n')

            print(f"  {filepath} ({len(wavelengths)} samples, {wavelengths[0]}-{wavelengths[-1]}nm)")

        except KeyError:
            print(f"  WARNING: SPD for '{illuminant_name}' not found")


def generate_additional_d_series(output_dir):
    """Generate additional D-series illuminants (D40, D45, D93)."""

    print("\nGenerating additional D-series illuminants...")

    # Additional D-series CCTs
    d_series_ccts = {
        'd40': 4000,
        'd45': 4500,
        'd93': 9300
    }

    for filename, cct in d_series_ccts.items():
        try:
            # Compute D-series xy from CCT
            xy = colour.xy_to_CCT_CIE_D(colour.CCT_to_xy_CIE_D(cct))
            xy = colour.CCT_to_xy_CIE_D(cct)

            filepath = os.path.join(output_dir, 'illuminants_xy', f'{filename}_xy.csv')
            save_vector([xy[0], xy[1]], filepath, f"D{cct//100} (CCT={cct}K)")
        except Exception as e:
            print(f"  WARNING: Could not generate {filename}: {e}")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python illuminants.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_illuminant_xy_coordinates(output_dir)
    generate_illuminant_spds(output_dir)
    generate_additional_d_series(output_dir)
