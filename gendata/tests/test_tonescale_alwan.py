"""
Quick test to compare Alwan's TonescaleCompress20 output with OCIO's reference.
This uses ctypes to call the Alwan DLL directly.
"""

import ctypes
import os
import sys

# Test inputs (same as OCIO test)
TEST_RGB_INPUTS = [
    [1.0, 0.0, 0.0],   # Red
    [0.0, 1.0, 0.0],   # Green
    [0.0, 0.0, 1.0],   # Blue
    [1.0, 1.0, 0.0],   # Yellow
    [0.0, 1.0, 1.0],   # Cyan
    [1.0, 0.0, 1.0],   # Magenta
    [0.0, 0.0, 0.0],   # Black
    [0.18, 0.18, 0.18], # 18% gray
    [0.5, 0.5, 0.5],   # Mid gray
    [1.0, 1.0, 1.0],   # White
]

# OCIO reference outputs at 1000 nits
OCIO_OUTPUTS = [
    [0.014550653286278248, 0.0, 0.0],
    [0.0, float('nan'), 0.0],
    [0.0, 0.0, 1.0],
    [0.014550653286278248, 0.024226989597082138, 0.0],
    [0.0, float('nan'), 1.0],
    [0.014550653286278248, 0.0, 1.0],
    [0.0, 0.0, 0.0],
    [0.0002815892512444407, 0.0006019878201186657, 0.18000000715255737],
    [0.0029533004853874445, 0.0054757967591285706, 0.5],
    [0.014550653286278248, 0.024215200915932655, 1.0],
]

def main():
    print("Comparing Alwan TonescaleCompress20 with OCIO reference")
    print("=" * 70)

    # Try to load the Alwan library
    dll_path = r"C:\git\alwan\tmp\lib\win64_debug_f64\Alwan.lib"
    print(f"Note: This is a static library, we need to use the test executable")
    print()

    # For now, just display the OCIO reference values
    print("OCIO Reference values at 1000 nits peak luminance:")
    print("-" * 70)
    print(f"{'Input RGB':<30} {'OCIO Output RGB':<40}")
    print("-" * 70)

    for inp, out in zip(TEST_RGB_INPUTS, OCIO_OUTPUTS):
        print(f"{str(inp):<30} {str(out):<40}")

    print()
    print("Analysis:")
    print("- Pure green [0,1,0] produces NaN (CAM16 imaginary primaries)")
    print("- Blue channel often unchanged (e.g., [1,1,1] -> [0.0145, 0.024, 1.0])")
    print("- This suggests special handling in OCIO's implementation")
    print()
    print("To test Alwan's implementation, we need to add a test that prints output.")


if __name__ == "__main__":
    main()
