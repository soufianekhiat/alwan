"""
Generate Jakob2019 spectral upsampling LUTs using the gen_jakob2019_table.exe tool.
Source: Pre-built C++ executable in tools/datagen/

Generates LUTs for 6 RGB gamuts (sRGB, eRGB, XYZ, ProPhotoRGB, ACES2065_1, REC2020).
Each gamut requires 3 coefficient tables (c0, c1, c2).
"""

import sys
import os
import subprocess
import struct

def convert_spec_to_csv(spec_file, output_dir, gamut_suffix=""):
    """
    Convert binary SPEC format to CSV coefficient files.

    Resamples from channel-based polar parameterization to regular RGB grid.

    Binary format (from rgb2spec.c):
    - 4 bytes: "SPEC" header
    - 4 bytes: resolution (uint32)
    - res * 4 bytes: scale array (float[res])
    - 3 * 3 * res^3 * 4 bytes: coefficient data
      - Data layout: (((channel * res + zi) * res + yi) * res + xi) * 3 + coeff_idx
      - channel: 0-2 (which RGB component is largest)
      - zi, yi, xi: indices in polar-like parameterization
      - 3 coefficients (c0, c1, c2) per entry

    Output format (for Alwan C code):
    - Regular RGB grid: index = r * res² + g * res + b
    - 3 separate arrays for c0, c1, c2
    - Filename format: jakob2019_lut_c{0-2}{suffix}.csv

    Args:
        spec_file: Path to binary SPEC file
        output_dir: Directory for output CSV files
        gamut_suffix: Suffix for gamut (e.g., "_prophotorgb"), empty for sRGB

    Returns:
        True if conversion successful, False otherwise
    """
    try:
        with open(spec_file, 'rb') as f:
            # Read header and resolution
            header = f.read(4)
            if header != b'SPEC':
                print(f"    [ERROR] Invalid SPEC file header: {header}")
                return False

            resolution = struct.unpack('I', f.read(4))[0]

            # Read scale array
            scale_data = f.read(resolution * 4)
            scale = struct.unpack(f'{resolution}f', scale_data)

            # Read coefficient data (3 channels * 3 coeffs * res^3 entries)
            lut_size = resolution * resolution * resolution
            total_floats = 3 * 3 * lut_size
            data_bytes = f.read(total_floats * 4)

            if len(data_bytes) != total_floats * 4:
                print(f"    [ERROR] Incomplete data: expected {total_floats * 4} bytes, got {len(data_bytes)}")
                return False

            # Unpack all coefficients
            data = struct.unpack(f'{total_floats}f', data_bytes)

        # Helper function: find interval in scale array (binary search)
        def find_interval(z_val):
            left, right = 0, resolution - 2
            while left < right:
                mid = (left + right + 1) // 2
                if scale[mid] <= z_val:
                    left = mid
                else:
                    right = mid - 1
            return min(left, resolution - 2)

        # Helper function: fetch coefficients using rgb2spec algorithm
        def fetch_coeffs(r, g, b):
            """Implements rgb2spec_fetch algorithm from rgb2spec.c"""
            # Clamp RGB to [0, 1]
            rgb = [max(0.0, min(1.0, r)), max(0.0, min(1.0, g)), max(0.0, min(1.0, b))]

            # Find largest component
            i = 0 if rgb[0] >= rgb[1] and rgb[0] >= rgb[2] else (1 if rgb[1] >= rgb[2] else 2)

            # Polar-like parameterization
            z = rgb[i]
            if z == 0:
                # Black - return zero coefficients
                return [0.0, 0.0, 0.0]

            scale_val = (resolution - 1) / z
            x = rgb[(i + 1) % 3] * scale_val
            y = rgb[(i + 2) % 3] * scale_val

            # Integer indices (clamped)
            xi = min(int(x), resolution - 2)
            yi = min(int(y), resolution - 2)
            zi = find_interval(z)

            # Base offset in data array
            offset = (((i * resolution + zi) * resolution + yi) * resolution + xi) * 3

            # Trilinear interpolation weights
            x1, x0 = x - xi, 1.0 - (x - xi)
            y1, y0 = y - yi, 1.0 - (y - yi)

            # Z interpolation weight
            if zi + 1 < resolution:
                z1 = (z - scale[zi]) / (scale[zi + 1] - scale[zi])
            else:
                z1 = 0.0
            z0 = 1.0 - z1

            # Compute strides
            dx = 3
            dy = 3 * resolution
            dz = 3 * resolution * resolution

            # Trilinear interpolation for each coefficient
            coeffs = []
            for coeff_idx in range(3):
                # 8 corner values of the interpolation cube
                c000 = data[offset + coeff_idx]
                c100 = data[offset + dx + coeff_idx]
                c010 = data[offset + dy + coeff_idx]
                c110 = data[offset + dy + dx + coeff_idx]
                c001 = data[offset + dz + coeff_idx] if zi + 1 < resolution else c000
                c101 = data[offset + dz + dx + coeff_idx] if zi + 1 < resolution else c100
                c011 = data[offset + dz + dy + coeff_idx] if zi + 1 < resolution else c010
                c111 = data[offset + dz + dy + dx + coeff_idx] if zi + 1 < resolution else c110

                # Trilinear interpolation
                c00 = c000 * x0 + c100 * x1
                c01 = c010 * x0 + c110 * x1
                c10 = c001 * x0 + c101 * x1
                c11 = c011 * x0 + c111 * x1

                c0_interp = c00 * y0 + c01 * y1
                c1_interp = c10 * y0 + c11 * y1

                coeff_val = c0_interp * z0 + c1_interp * z1
                coeffs.append(coeff_val)

            return coeffs

        # Resample to regular RGB grid
        print(f"    Resampling from channel-based to regular RGB grid...")
        c0_values = []
        c1_values = []
        c2_values = []

        # Sample at regular grid points
        for ri in range(resolution):
            r = ri / (resolution - 1)
            for gi in range(resolution):
                g = gi / (resolution - 1)
                for bi in range(resolution):
                    b = bi / (resolution - 1)

                    # Fetch coefficients using rgb2spec algorithm
                    coeffs = fetch_coeffs(r, g, b)
                    c0_values.append(coeffs[0])
                    c1_values.append(coeffs[1])
                    c2_values.append(coeffs[2])

        # Write CSV files with correct naming format
        # Format: jakob2019_lut_c{num}{suffix}.csv (coefficient number BEFORE suffix)
        for coeff_num, values in enumerate([c0_values, c1_values, c2_values]):
            filename = f"jakob2019_lut_c{coeff_num}{gamut_suffix}.csv"
            csv_path = os.path.join(output_dir, filename)
            with open(csv_path, 'w') as csv_f:
                for i, val in enumerate(values):
                    csv_f.write(f"{val:.8f}")
                    if i < len(values) - 1:
                        csv_f.write(",")
                        if (i + 1) % 10 == 0:  # Line break every 10 values
                            csv_f.write("\n")
                csv_f.write("\n")

        return True

    except Exception as e:
        print(f"    [ERROR] Conversion failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def generate_jakob2019_luts(output_dir):
    """Generate Jakob2019 LUT tables using the executable."""

    print("\nGenerating Jakob2019 spectral upsampling LUTs...")

    # Path to the generator executable
    tool_path = os.path.join("tools", "datagen", "gen_jakob2019_table.exe")

    if not os.path.exists(tool_path):
        print(f"  ERROR: Tool not found: {tool_path}")
        print("  Please build the solution first to generate the tool.")
        return False

    # Create output directory
    lut_dir = os.path.join(output_dir, 'spectral_lut', 'jakob2019')
    os.makedirs(lut_dir, exist_ok=True)

    # Resolution (from JAKOB2019_LUT_RES in C code)
    resolution = 64

    # Gamut configurations: (gamut_name, file_suffix)
    gamuts = [
        ('sRGB', ''),
        ('eRGB', '_ergb'),
        ('XYZ', '_xyz'),
        ('ProPhotoRGB', '_prophotorgb'),
        ('ACES2065_1', '_aces2065_1'),
        ('REC2020', '_rec2020'),
    ]

    print(f"  Using tool: {tool_path}")
    print(f"  Resolution: {resolution}^3 = {resolution**3} values per LUT")
    print(f"  Output directory: {lut_dir}")
    print()

    success_count = 0
    fail_count = 0

    for gamut_name, suffix in gamuts:
        print(f"  Generating LUTs for {gamut_name}...")

        # Check if files already exist (filename format: jakob2019_lut_c{0-2}{suffix}.csv)
        c0_file = os.path.join(lut_dir, f"jakob2019_lut_c0{suffix}.csv")
        c1_file = os.path.join(lut_dir, f"jakob2019_lut_c1{suffix}.csv")
        c2_file = os.path.join(lut_dir, f"jakob2019_lut_c2{suffix}.csv")

        if os.path.exists(c0_file) and os.path.exists(c1_file) and os.path.exists(c2_file):
            print(f"    [SKIP] {gamut_name} LUTs already exist")
            success_count += 1
            continue

        # Generate binary SPEC file to temporary location
        temp_spec_file = os.path.join(lut_dir, f"jakob2019_lut{suffix}.spec")

        # Build command - tool outputs single binary SPEC file
        cmd = [tool_path, str(resolution), temp_spec_file, gamut_name]

        try:
            # Run the tool
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=300  # 5 minute timeout
            )

            if result.returncode == 0:
                # Convert binary SPEC file to CSV coefficient files
                if os.path.exists(temp_spec_file):
                    print(f"    Converting SPEC to CSV...")
                    if convert_spec_to_csv(temp_spec_file, lut_dir, suffix):
                        # Verify CSV files were created
                        if os.path.exists(c0_file) and os.path.exists(c1_file) and os.path.exists(c2_file):
                            print(f"    [OK] Generated {gamut_name} LUTs (c0, c1, c2)")
                            success_count += 1
                            # Clean up temporary SPEC file
                            try:
                                os.remove(temp_spec_file)
                            except:
                                pass
                        else:
                            print(f"    [ERROR] Conversion succeeded but CSV files not found")
                            fail_count += 1
                    else:
                        print(f"    [ERROR] Failed to convert SPEC to CSV")
                        fail_count += 1
                else:
                    print(f"    [ERROR] Tool completed but SPEC file not found: {temp_spec_file}")
                    fail_count += 1
            else:
                print(f"    [ERROR] Tool failed with exit code {result.returncode}")
                if result.stderr:
                    print(f"    Error: {result.stderr.strip()}")
                fail_count += 1

        except subprocess.TimeoutExpired:
            print(f"    [ERROR] Tool timed out after 5 minutes")
            fail_count += 1
        except Exception as e:
            print(f"    [ERROR] Failed to run tool: {e}")
            fail_count += 1

    print()
    print(f"  Generated: {success_count * 3} LUT files ({success_count}/6 gamuts)")
    if fail_count > 0:
        print(f"  Failed: {fail_count} gamuts")

    return fail_count == 0


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python jakob2019_luts.py <output_dir>")
        sys.exit(1)

    output_dir = sys.argv[1]
    success = generate_jakob2019_luts(output_dir)
    sys.exit(0 if success else 1)
