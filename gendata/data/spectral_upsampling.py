"""
Generate spectral upsampling basis functions.
Source: colour.recovery.smits1999, colour.recovery.MSDS_BASIS_FUNCTIONS_sRGB_MALLETT2019

All data is extracted from colour-science.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from common import format_scalar, ensure_dir

try:
    import colour
    import colour.recovery
    import colour.recovery.smits1999 as smits
except ImportError:
    print("ERROR: colour-science not installed. Run: pip install colour-science")
    sys.exit(1)


def generate_smits1999_basis(output_dir):
    """Generate Smits1999 basis spectra."""

    print("\nGenerating Smits1999 basis spectra...")

    try:
        # Create output directory
        smits_dir = os.path.join(output_dir, 'spectral_basis', 'smits1999')
        os.makedirs(smits_dir, exist_ok=True)

        # Get Smits basis spectra from colour-science (NO HARDCODING)
        smits_sds = smits.SDS_SMITS1999
        basis_names = ['white', 'cyan', 'magenta', 'yellow', 'red', 'green', 'blue']

        for name in basis_names:
            if name in smits_sds:
                sd = smits_sds[name]
                wavelengths = sd.wavelengths
                values = sd.values

                # Write wavelengths and values to separate CSV files
                # Wavelengths file (same for all)
                if name == 'white':
                    wl_filename = os.path.join(smits_dir, 'wavelengths.csv')
                    with open(wl_filename, 'w', newline='') as f:
                        formatted_wl = [format_scalar(w) for w in wavelengths]
                        f.write(','.join(formatted_wl) + '\n')
                    print(f"  {wl_filename} ({len(wavelengths)} wavelengths)")

                # Values file
                val_filename = os.path.join(smits_dir, f'{name}.csv')
                with open(val_filename, 'w', newline='') as f:
                    formatted_vals = [format_scalar(v) for v in values]
                    f.write(','.join(formatted_vals) + '\n')
                print(f"  {val_filename} ({len(values)} values)")
            else:
                print(f"  Warning: {name} basis not found in Smits1999 data")

    except Exception as e:
        print(f"  ERROR: Could not generate Smits1999 basis spectra: {e}")
        import traceback
        traceback.print_exc()


def generate_mallett2019_basis(output_dir):
    """Generate Mallett2019 sRGB basis functions."""

    print("\nGenerating Mallett2019 basis functions...")

    try:
        # Create output directory
        mallett_dir = os.path.join(output_dir, 'spectral_basis', 'mallett2019')
        os.makedirs(mallett_dir, exist_ok=True)

        # Get Mallett2019 sRGB basis functions from colour-science (NO HARDCODING)
        mallett_basis = colour.recovery.MSDS_BASIS_FUNCTIONS_sRGB_MALLETT2019

        wavelengths = mallett_basis.wavelengths
        labels = mallett_basis.labels  # ['red', 'green', 'blue']

        # Write wavelengths (same for all basis functions)
        wl_filename = os.path.join(mallett_dir, 'wavelengths.csv')
        with open(wl_filename, 'w', newline='') as f:
            formatted_wl = [format_scalar(w) for w in wavelengths]
            f.write(','.join(formatted_wl) + '\n')
        print(f"  {wl_filename} ({len(wavelengths)} wavelengths)")

        # Write each basis function (red, green, blue)
        for idx, label in enumerate(labels):
            basis_values = mallett_basis.values[:, idx]
            val_filename = os.path.join(mallett_dir, f'{label}.csv')
            with open(val_filename, 'w', newline='') as f:
                formatted_vals = [format_scalar(v) for v in basis_values]
                f.write(','.join(formatted_vals) + '\n')
            print(f"  {val_filename} ({len(basis_values)} values)")

    except Exception as e:
        print(f"  ERROR: Could not generate Mallett2019 basis functions: {e}")
        import traceback
        traceback.print_exc()


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python spectral_upsampling.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    generate_smits1999_basis(output_dir)
    generate_mallett2019_basis(output_dir)
