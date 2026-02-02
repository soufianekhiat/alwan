#!/usr/bin/env python3
"""
Generate test reference data for CCT methods and Cineon transfer function.
Uses colour-science as the authoritative reference.
"""

import numpy as np
import colour
import os

# Output directory
OUTPUT_DIR = "tests/reference_values"

def format_value(x):
    """Format a single value for C inclusion."""
    if np.isnan(x):
        return 'NAN'
    elif np.isinf(x):
        return 'INFINITY' if x > 0 else '-INFINITY'
    else:
        return f'{x:.16e}'

def save_csv(filename, data, comment=""):
    """Save data to CSV with maximum precision for C inclusion."""
    filepath = os.path.join(OUTPUT_DIR, filename)
    with open(filepath, 'w') as f:
        if isinstance(data, np.ndarray):
            if data.ndim == 1:
                # Single row - comma separated with trailing comma for C array inclusion
                f.write(','.join(format_value(x) for x in data) + ',\n')
            else:
                # Multiple rows - comma at end of each line for C array inclusion
                for row in data:
                    f.write(','.join(format_value(x) for x in row) + ',\n')
        else:
            f.write(format_value(data) + ',\n')
    print(f"  {filename}")

print("Generating CCT and Cineon test reference data...")
print(f"Output directory: {OUTPUT_DIR}")
print()

# ============================================================================
# Cineon Transfer Function
# ============================================================================
print("=== Cineon Transfer Function ===")

# Test linear values
linear_values = np.array([0.0, 0.01, 0.05, 0.10, 0.18, 0.25, 0.50, 0.75, 1.0])
save_csv("cineon_linear_input.csv", linear_values)

# Encoded values using colour-science
encoded_values = np.array([colour.models.log_encoding_Cineon(x) for x in linear_values])
save_csv("cineon_encoded.csv", encoded_values)

# Roundtrip: encode then decode
roundtrip_values = np.array([colour.models.log_decoding_Cineon(colour.models.log_encoding_Cineon(x)) for x in linear_values])
save_csv("cineon_roundtrip.csv", roundtrip_values)

# Test encoded values for decoding
encoded_test = np.array([0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0])
save_csv("cineon_encoded_input.csv", encoded_test)

decoded_values = np.array([colour.models.log_decoding_Cineon(x) for x in encoded_test])
save_csv("cineon_decoded.csv", decoded_values)

print()

# ============================================================================
# CCT Methods - Hernandez 1999
# ============================================================================
print("=== CCT Hernandez 1999 ===")

# Test xy chromaticity coordinates (various illuminants and Planckian locus points)
test_xy = np.array([
    [0.31270, 0.32900],  # D65
    [0.34567, 0.35850],  # D50
    [0.33243, 0.34744],  # ~5500K
    [0.44758, 0.40745],  # Illuminant A (~2856K)
    [0.28315, 0.29711],  # ~8000K
    [0.26428, 0.27726],  # ~10000K
    [0.24060, 0.24033],  # ~15000K
])
save_csv("cct_hernandez_xy_input.csv", test_xy)

# CCT values using Hernandez 1999
hernandez_cct = np.array([colour.temperature.xy_to_CCT_Hernandez1999(xy) for xy in test_xy])
save_csv("cct_hernandez_output.csv", hernandez_cct)

print()

# ============================================================================
# CCT Methods - Kang 2002
# ============================================================================
print("=== CCT Kang 2002 ===")

# Test CCT values for forward transform (CCT -> xy)
test_cct = np.array([2000.0, 2500.0, 3000.0, 4000.0, 5000.0, 6000.0, 6500.0, 8000.0, 10000.0, 15000.0, 20000.0])
save_csv("cct_kang_cct_input.csv", test_cct)

# xy values using Kang 2002 (CCT to xy)
kang_xy = np.array([colour.temperature.CCT_to_xy_Kang2002(cct) for cct in test_cct])
save_csv("cct_kang_xy_output.csv", kang_xy)

# Test inverse: xy back to CCT
# Use the xy values we just computed to verify the inverse
# Note: Kang xy_to_CCT uses optimization, results may vary slightly
kang_cct_inverse = np.array([colour.temperature.xy_to_CCT_Kang2002(xy) for xy in kang_xy])
save_csv("cct_kang_inverse_output.csv", kang_cct_inverse)

print()

# ============================================================================
# CCT Comparison - Multiple methods
# ============================================================================
print("=== CCT Method Comparison ===")

# D65 chromaticity for method comparison
d65_xy = np.array([0.31270, 0.32900])
save_csv("cct_comparison_d65_xy.csv", d65_xy)

# CCT from different methods
cct_mccamy = colour.temperature.xy_to_CCT_McCamy1992(d65_xy)
cct_hernandez = colour.temperature.xy_to_CCT_Hernandez1999(d65_xy)
cct_kang = colour.temperature.xy_to_CCT_Kang2002(d65_xy)

comparison = np.array([cct_mccamy, cct_hernandez, cct_kang])
save_csv("cct_comparison_methods.csv", comparison)

print()
print("Done! Generated test reference data for CCT and Cineon.")
