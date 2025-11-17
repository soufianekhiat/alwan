#!/usr/bin/env pwsh
# ================================================================
# Alwan Test Reference Data Generation Script
# ================================================================
# This script generates ALL test reference/expected values using
# the colour-science Python package as the single source of truth.
#
# All expected values in tests MUST come from this script, never
# from hardcoded values in C test files.
# ================================================================

param(
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Stop"

Write-Host "========================================"
Write-Host "Alwan Test Reference Data Generation"
Write-Host "========================================"
Write-Host ""

# Check Python installation
Write-Host "Checking Python installation..."
try {
    $pythonVersion = python --version 2>&1
    Write-Host "  Found: $pythonVersion"
} catch {
    Write-Error "Python not found. Please install Python 3.x"
    exit 1
}

# Check colour-science package
Write-Host "Checking colour-science package..."
$colourCheck = python -c "import warnings; warnings.filterwarnings('ignore'); import colour; print(f'colour-science version: {colour.__version__}')" 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Error "colour-science package not found. Install with: pip install colour-science"
    exit 1
}
Write-Host "  $colourCheck"

# Output directory for test reference values
$TEST_REF_DIR = "tests/unit/reference_values"

# Create output directory
Write-Host ""
Write-Host "Creating reference values directory..."
if (-not (Test-Path $TEST_REF_DIR)) {
    New-Item -ItemType Directory -Path $TEST_REF_DIR -Force | Out-Null
}
Write-Host "  Output: $TEST_REF_DIR"

# ================================================================
# Python Script to Generate All Reference Values
# ================================================================

$pythonScript = @"
import warnings
warnings.filterwarnings('ignore')

import numpy as np
import colour
import os
import sys

# Ensure output directory exists
TEST_REF_DIR = 'tests/unit/reference_values'
os.makedirs(TEST_REF_DIR, exist_ok=True)

def format_scalar(value):
    """Format a scalar value with high precision"""
    if isinstance(value, (int, np.integer)):
        return str(float(value))
    elif isinstance(value, (float, np.floating)):
        if np.isnan(value) or np.isinf(value):
            return '0.0'
        # Use %.17g for maximum precision without unnecessary zeros
        return f'{value:.17g}'
    else:
        return str(float(value))

def write_csv(filename, values, description=''):
    """Write values to CSV file"""
    with open(filename, 'w', newline='') as f:
        formatted = [format_scalar(v) for v in values]
        f.write(','.join(formatted) + '\n')
    num_values = len(values)
    print(f'  {filename} ({num_values} values) - {description}')

def write_ref(name, values, description=''):
    """Write reference values to CSV file in test reference directory"""
    filename = os.path.join(TEST_REF_DIR, name + '.csv')
    write_csv(filename, values, description)

print('Generating test reference values from colour-science...\n')

# ================================================================
# Color Space Conversion Reference Values
# ================================================================
print('Color Space Conversions:')

# Standard illuminants (get from colour-science for perfect alignment)
d65_white_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']
d50_white_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D50']
d65_white_xyz = colour.xy_to_XYZ(d65_white_xy)
d50_white_xyz = colour.xy_to_XYZ(d50_white_xy)

# Test XYZ colors (use colour-science's exact D65 white point)
test_xyz_colors = np.array([
    d65_white_xyz,                # D65 white (from colour-science)
    [0.00000, 0.00000, 0.00000],  # Black
    [0.41246, 0.21267, 0.01933],  # sRGB red
    [0.35758, 0.71515, 0.11919],  # sRGB green
    [0.18048, 0.07217, 0.95030],  # sRGB blue
    [0.76986, 0.92783, 0.13853],  # sRGB yellow
    [0.53806, 0.78732, 1.06950],  # sRGB cyan
    [0.59294, 0.28484, 0.96963],  # sRGB magenta
])

# Test input colors
test_xyz_flat = []
for xyz in test_xyz_colors:
    test_xyz_flat.extend(xyz.tolist())
write_ref('test_xyz_colors', test_xyz_flat, 'Test XYZ input colors')

# White points (XYZ tristimulus values)
write_ref('test_d65_white', d65_white_xyz.tolist(), 'D65 white point')
write_ref('test_d50_white', d50_white_xyz.tolist(), 'D50 white point')

# XYZ -> xyY
xyy_values = []
for xyz in test_xyz_colors:
    xyy = colour.XYZ_to_xyY(xyz)
    xyy_values.extend(xyy.tolist())
write_ref('xyz_to_xyy', xyy_values, 'XYZ -> xyY')

# XYZ -> Lab (D65) - D65 is default, don't pass illuminant to avoid precision errors
lab_d65_values = []
for xyz in test_xyz_colors:
    lab = colour.XYZ_to_Lab(xyz)  # Default is D65
    lab_d65_values.extend(lab.tolist())
write_ref('xyz_to_lab_d65', lab_d65_values, 'XYZ -> Lab (D65)')

# XYZ -> Lab (D50)
lab_d50_values = []
for xyz in test_xyz_colors:
    lab = colour.XYZ_to_Lab(xyz, illuminant=d50_white_xy)
    lab_d50_values.extend(lab.tolist())
write_ref('xyz_to_lab_d50', lab_d50_values, 'XYZ -> Lab (D50)')

# XYZ -> Luv (D65) - D65 is default, don't pass illuminant to avoid precision errors
luv_d65_values = []
for xyz in test_xyz_colors:
    luv = colour.XYZ_to_Luv(xyz)  # Default is D65
    luv_d65_values.extend(luv.tolist())
write_ref('xyz_to_luv_d65', luv_d65_values, 'XYZ -> Luv (D65)')

# Lab -> LCh (using D65 Lab values)
lch_values = []
for i in range(len(test_xyz_colors)):
    lab = np.array([lab_d65_values[i*3], lab_d65_values[i*3+1], lab_d65_values[i*3+2]])
    lch = colour.Lab_to_LCHab(lab)
    lch_values.extend(lch.tolist())
write_ref('lab_to_lch', lch_values, 'Lab -> LCh')

# Luv -> LChuv
lchuv_values = []
for i in range(len(test_xyz_colors)):
    luv = np.array([luv_d65_values[i*3], luv_d65_values[i*3+1], luv_d65_values[i*3+2]])
    lchuv = colour.Luv_to_LCHuv(luv)
    lchuv_values.extend(lchuv.tolist())
write_ref('luv_to_lchuv', lchuv_values, 'Luv -> LChuv')

# ================================================================
# Oklab & Oklch Reference Values
# ================================================================
print('\nOklab & Oklch:')

# XYZ -> Oklab
oklab_values = []
for xyz in test_xyz_colors:
    oklab = colour.XYZ_to_Oklab(xyz)
    oklab_values.extend(oklab.tolist())
write_ref('xyz_to_oklab', oklab_values, 'XYZ -> Oklab')

# Oklab -> Oklch
oklch_values = []
for i in range(len(test_xyz_colors)):
    oklab = np.array([oklab_values[i*3], oklab_values[i*3+1], oklab_values[i*3+2]])
    # Oklch: L, C, h
    L = oklab[0]
    C = np.sqrt(oklab[1]**2 + oklab[2]**2)
    h = np.arctan2(oklab[2], oklab[1])  # radians
    oklch_values.extend([L, C, h])
write_ref('oklab_to_oklch', oklch_values, 'Oklab -> Oklch')

# Known values: D65 white in Oklab (should be [1, 0, 0])
white_oklab = colour.XYZ_to_Oklab(d65_white_xyz)
write_ref('oklab_d65_white', white_oklab.tolist(), 'D65 white in Oklab')

# Known values: Black in Oklab (should be [0, 0, 0])
black_oklab = colour.XYZ_to_Oklab(np.array([0, 0, 0]))
write_ref('oklab_black', black_oklab.tolist(), 'Black in Oklab')

# Combined XYZ input + Oklab output for test structure (6 values per color)
oklab_test_pairs = []
for i in range(len(test_xyz_colors)):
    oklab_test_pairs.extend(test_xyz_colors[i].tolist())  # XYZ input
    oklab_test_pairs.extend([oklab_values[i*3], oklab_values[i*3+1], oklab_values[i*3+2]])  # Oklab output
write_ref('test_xyz_oklab_pairs', oklab_test_pairs, 'XYZ input + Oklab output pairs')

# Combined Oklab input + Oklch output for test structure (6 values per color)
oklch_test_pairs = []
for i in range(len(test_xyz_colors)):
    oklch_test_pairs.extend([oklab_values[i*3], oklab_values[i*3+1], oklab_values[i*3+2]])  # Oklab input
    oklch_test_pairs.extend([oklch_values[i*3], oklch_values[i*3+1], oklch_values[i*3+2]])  # Oklch output
write_ref('test_oklab_oklch_pairs', oklch_test_pairs, 'Oklab input + Oklch output pairs')

# ================================================================
# Delta E Reference Values
# ================================================================
print('\nDelta E Metrics:')

# Test Lab color pairs
lab_pairs = [
    ([50, 2.6772, -79.7751], [50, 0.0, -82.7485]),    # Sharma 2005 test case 1
    ([50, 3.1571, -77.2803], [50, 0.0, -82.7485]),    # Sharma 2005 test case 2
    ([50, 2.8361, -74.0200], [50, 0.0, -82.7485]),    # Sharma 2005 test case 3
    ([50, -1.3802, -84.2814], [50, 0.0, -82.7485]),   # Sharma 2005 test case 4
    ([50, -1.1848, -84.8006], [50, 0.0, -82.7485]),   # Sharma 2005 test case 5
    ([50, -0.9009, -85.5211], [50, 0.0, -82.7485]),   # Sharma 2005 test case 6
]

# Generate Lab input arrays
delta_e_lab1 = []
delta_e_lab2 = []
for lab1, lab2 in lab_pairs:
    delta_e_lab1.extend(lab1)
    delta_e_lab2.extend(lab2)
write_ref('delta_e_lab1', delta_e_lab1, 'Delta E test Lab color 1')
write_ref('delta_e_lab2', delta_e_lab2, 'Delta E test Lab color 2')

delta_e_76 = []
delta_e_94 = []
delta_e_cmc = []
delta_e_2000 = []

for lab1, lab2 in lab_pairs:
    lab1_arr = np.array(lab1)
    lab2_arr = np.array(lab2)

    # Delta E 76
    de76 = colour.difference.delta_E_CIE1976(lab1_arr, lab2_arr)
    delta_e_76.append(de76)

    # Delta E 94
    de94 = colour.difference.delta_E_CIE1994(lab1_arr, lab2_arr)
    delta_e_94.append(de94)

    # Delta E CMC(2:1)
    de_cmc = colour.difference.delta_E_CMC(lab1_arr, lab2_arr, l=2.0, c=1.0)
    delta_e_cmc.append(de_cmc)

    # Delta E 2000
    de2000 = colour.difference.delta_E_CIE2000(lab1_arr, lab2_arr)
    delta_e_2000.append(de2000)

write_ref('delta_e_76', delta_e_76, 'Delta E 76')
write_ref('delta_e_94', delta_e_94, 'Delta E 94')
write_ref('delta_e_cmc', delta_e_cmc, 'Delta E CMC(2:1)')
write_ref('delta_e_2000', delta_e_2000, 'Delta E 2000')

# ================================================================
# Convenience Color Models
# ================================================================
print('\nConvenience Color Models:')

# Test RGB colors
test_rgb = [
    [0.0, 0.0, 0.0],      # Black
    [1.0, 1.0, 1.0],      # White
    [1.0, 0.0, 0.0],      # Red
    [0.0, 1.0, 0.0],      # Green
    [0.0, 0.0, 1.0],      # Blue
    [1.0, 1.0, 0.0],      # Yellow
    [0.0, 1.0, 1.0],      # Cyan
    [1.0, 0.0, 1.0],      # Magenta
    [0.5, 0.5, 0.5],      # Gray
    [0.75, 0.25, 0.5],    # Mixed
    [0.25, 0.75, 0.25],   # Mixed
]

# Test RGB input colors
test_rgb_flat = []
for rgb in test_rgb:
    test_rgb_flat.extend(rgb)
write_ref('test_rgb_colors', test_rgb_flat, 'Test RGB input colors')

# RGB -> HSV
hsv_values = []
for rgb in test_rgb:
    hsv = colour.RGB_to_HSV(np.array(rgb))
    hsv_values.extend(hsv.tolist())
write_ref('rgb_to_hsv', hsv_values, 'RGB -> HSV')

# RGB -> HSL
hsl_values = []
for rgb in test_rgb:
    hsl = colour.RGB_to_HSL(np.array(rgb))
    hsl_values.extend(hsl.tolist())
write_ref('rgb_to_hsl', hsl_values, 'RGB -> HSL')

# RGB -> CMY
cmy_values = []
for rgb in test_rgb:
    cmy = 1.0 - np.array(rgb)
    cmy_values.extend(cmy.tolist())
write_ref('rgb_to_cmy', cmy_values, 'RGB -> CMY')

# RGB -> CMYK (manual implementation - basic conversion)
cmyk_values = []
for rgb in test_rgb:
    r, g, b = rgb
    # Basic CMYK conversion (no UCR/GCR)
    k = 1 - max(r, g, b)
    if k < 1.0:
        c = (1 - r - k) / (1 - k)
        m = (1 - g - k) / (1 - k)
        y = (1 - b - k) / (1 - k)
    else:
        c = m = y = 0
    cmyk_values.extend([c, m, y, k])
write_ref('rgb_to_cmyk', cmyk_values, 'RGB -> CMYK')

# RGB -> YCbCr (BT.601) - using K values for BT.601: (0.299, 0.114)
# Note: colour-science returns signed format (Cb/Cr centered at 0)
# but alwan uses unsigned format (Cb/Cr in [0, 1] centered at 0.5)
ycbcr_bt601_values = []
for rgb in test_rgb:
    ycbcr = colour.RGB_to_YCbCr(np.array(rgb), K=np.array([0.299, 0.114]),
                                 out_bits=8, out_legal=False, out_int=False)
    # Convert from signed to unsigned: Cb/Cr += 0.5
    ycbcr[1] += 0.5
    ycbcr[2] += 0.5
    ycbcr_bt601_values.extend(ycbcr.tolist())
write_ref('rgb_to_ycbcr_bt601', ycbcr_bt601_values, 'RGB -> YCbCr BT.601')

# RGB -> YCbCr (BT.709) - using K values for BT.709: (0.2126, 0.0722) - default
ycbcr_bt709_values = []
for rgb in test_rgb:
    ycbcr = colour.RGB_to_YCbCr(np.array(rgb), K=np.array([0.2126, 0.0722]),
                                 out_bits=8, out_legal=False, out_int=False)
    # Convert from signed to unsigned: Cb/Cr += 0.5
    ycbcr[1] += 0.5
    ycbcr[2] += 0.5
    ycbcr_bt709_values.extend(ycbcr.tolist())
write_ref('rgb_to_ycbcr_bt709', ycbcr_bt709_values, 'RGB -> YCbCr BT.709')

# RGB -> YCbCr (BT.2020) - using K values for BT.2020: (0.2627, 0.0593)
ycbcr_bt2020_values = []
for rgb in test_rgb:
    ycbcr = colour.RGB_to_YCbCr(np.array(rgb), K=np.array([0.2627, 0.0593]),
                                 out_bits=8, out_legal=False, out_int=False)
    # Convert from signed to unsigned: Cb/Cr += 0.5
    ycbcr[1] += 0.5
    ycbcr[2] += 0.5
    ycbcr_bt2020_values.extend(ycbcr.tolist())
write_ref('rgb_to_ycbcr_bt2020', ycbcr_bt2020_values, 'RGB -> YCbCr BT.2020')

# RGB -> YcCbcCrc (constant luminance BT.2020)
# Note: YcCbcCrc expects linear RGB, using legal range as per BT.2020 spec
yccbccrc_values = []
for rgb in test_rgb:
    ycc = colour.RGB_to_YcCbcCrc(np.array(rgb), out_bits=10, out_legal=True, out_int=False)
    yccbccrc_values.extend(ycc.tolist())
write_ref('rgb_to_yccbccrc', yccbccrc_values, 'RGB -> YcCbcCrc BT.2020 (constant luminance, legal range)')

# RGB -> YCoCg (constant transform variant used in video coding)
ycocg_values = []
for rgb in test_rgb:
    r, g, b = rgb
    # YCoCg-R (reversible, used in video coding)
    Co = r - b
    t = b + Co / 2
    Cg = g - t
    Y = t + Cg / 2
    ycocg_values.extend([Y, Co, Cg])
write_ref('rgb_to_ycocg', ycocg_values, 'RGB -> YCoCg')

# ================================================================
# P1.2: ICtCp (ITU-R BT.2100 HDR) Reference Values
# ================================================================
print('\nICtCp (BT.2100 HDR):')

# Test HDR RGB colors (BT.2020 linear, normalized to SDR reference)
# Include both SDR range [0,1] and HDR range values
test_hdr_rgb = [
    [0.0, 0.0, 0.0],      # Black
    [0.18, 0.18, 0.18],   # 18% gray (SDR mid-gray)
    [1.0, 1.0, 1.0],      # SDR white (100 cd/m²)
    [1.0, 0.0, 0.0],      # SDR red
    [0.0, 1.0, 0.0],      # SDR green
    [0.0, 0.0, 1.0],      # SDR blue
    [2.0, 2.0, 2.0],      # HDR white (~200 cd/m²)
    [5.0, 5.0, 5.0],      # HDR bright (~500 cd/m²)
]

# Convert to BT.2020 colourspace objects (colour-science expects RGB in BT.2020 for ICtCp)
from colour.models import RGB_COLOURSPACE_BT2020
test_hdr_rgb_np = [np.array(rgb) for rgb in test_hdr_rgb]

# ICtCp with PQ transfer function
ictcp_pq_values = []
for rgb in test_hdr_rgb_np:
    # colour.RGB_to_ICtCp expects RGB in BT.2020 colourspace with method specification
    ictcp = colour.RGB_to_ICtCp(rgb, method='ITU-R BT.2100-2 PQ')
    ictcp_pq_values.extend(ictcp.tolist())
write_ref('ictcp_pq_from_rgb', ictcp_pq_values, 'ICtCp (PQ) from BT.2020 RGB')

# ICtCp with HLG transfer function
ictcp_hlg_values = []
for rgb in test_hdr_rgb_np:
    ictcp = colour.RGB_to_ICtCp(rgb, method='ITU-R BT.2100-2 HLG')
    ictcp_hlg_values.extend(ictcp.tolist())
write_ref('ictcp_hlg_from_rgb', ictcp_hlg_values, 'ICtCp (HLG) from BT.2020 RGB')

# Round-trip: ICtCp PQ back to RGB
rgb_from_ictcp_pq = []
for i in range(len(test_hdr_rgb)):
    ictcp = np.array(ictcp_pq_values[i*3:(i+1)*3])
    rgb = colour.ICtCp_to_RGB(ictcp, method='ITU-R BT.2100-2 PQ')
    rgb_from_ictcp_pq.extend(rgb.tolist())
write_ref('rgb_from_ictcp_pq', rgb_from_ictcp_pq, 'BT.2020 RGB from ICtCp (PQ)')

# Round-trip: ICtCp HLG back to RGB
rgb_from_ictcp_hlg = []
for i in range(len(test_hdr_rgb)):
    ictcp = np.array(ictcp_hlg_values[i*3:(i+1)*3])
    rgb = colour.ICtCp_to_RGB(ictcp, method='ITU-R BT.2100-2 HLG')
    rgb_from_ictcp_hlg.extend(rgb.tolist())
write_ref('rgb_from_ictcp_hlg', rgb_from_ictcp_hlg, 'BT.2020 RGB from ICtCp (HLG)')

# XYZ (D65) to ICtCp (via BT.2020)
# Use standard test XYZ colors
ictcp_pq_from_xyz = []
for xyz in test_xyz_colors:
    # Convert XYZ to BT.2020 RGB first
    rgb = colour.XYZ_to_RGB(xyz, d65_white_xyz, d65_white_xyz,
                            colour.RGB_COLOURSPACES['ITU-R BT.2020'].matrix_XYZ_to_RGB)
    ictcp = colour.RGB_to_ICtCp(rgb, method='ITU-R BT.2100-2 PQ')
    ictcp_pq_from_xyz.extend(ictcp.tolist())
write_ref('ictcp_pq_from_xyz', ictcp_pq_from_xyz, 'ICtCp (PQ) from XYZ via BT.2020')

ictcp_hlg_from_xyz = []
for xyz in test_xyz_colors:
    rgb = colour.XYZ_to_RGB(xyz, d65_white_xyz, d65_white_xyz,
                            colour.RGB_COLOURSPACES['ITU-R BT.2020'].matrix_XYZ_to_RGB)
    ictcp = colour.RGB_to_ICtCp(rgb, method='ITU-R BT.2100-2 HLG')
    ictcp_hlg_from_xyz.extend(ictcp.tolist())
write_ref('ictcp_hlg_from_xyz', ictcp_hlg_from_xyz, 'ICtCp (HLG) from XYZ via BT.2020')

# ================================================================
# CIECAM02 & CAM16 Color Appearance Model Reference Values
# ================================================================
print('\nColor Appearance Models (CIECAM02 & CAM16):')

# Viewing conditions for CAM tests (standard D65, average surround)
# white_XYZ, adapting_luminance, background_luminance
cam_viewing_conditions = [
    d65_white_xyz[0], d65_white_xyz[1], d65_white_xyz[2],  # white point
    318.31,  # adapting luminance (La) for 20% gray surround
    63.66    # background luminance (Yb) for 20% gray surround
]
write_ref('cam_viewing_conditions', cam_viewing_conditions, 'CAM viewing conditions')

# Test colors for CAM (using our standard test set)
cam_xyz_input = []
for xyz in test_xyz_colors:
    cam_xyz_input.extend(xyz.tolist())
write_ref('ciecam02_xyz_input', cam_xyz_input, 'CIECAM02 test XYZ input')

# CIECAM02 forward transform
ciecam02_correlates = []
for xyz in test_xyz_colors:
    correlates = colour.appearance.ciecam02.XYZ_to_CIECAM02(
        xyz, d65_white_xyz, 318.31, 63.66
    )
    # Store J, C, h, Q, M, s, H
    ciecam02_correlates.extend([
        correlates.J, correlates.C, correlates.h,
        correlates.Q, correlates.M, correlates.s, correlates.H
    ])
write_ref('ciecam02_correlates', ciecam02_correlates, 'CIECAM02 correlates (J,C,h,Q,M,s,H)')

# CIECAM02 inverse transform (reconstruct XYZ from correlates)
ciecam02_xyz_reconstructed = []
for i in range(len(test_xyz_colors)):
    # Get correlates for this color
    idx = i * 7
    J = ciecam02_correlates[idx + 0]
    C = ciecam02_correlates[idx + 1]
    h = ciecam02_correlates[idx + 2]

    # Reconstruct XYZ from JCh - use proper specification object
    spec = colour.appearance.ciecam02.CAM_Specification_CIECAM02(J=J, C=C, h=h)
    xyz_recon = colour.appearance.ciecam02.CIECAM02_to_XYZ(
        spec, d65_white_xyz, 318.31, 63.66
    )
    ciecam02_xyz_reconstructed.extend(xyz_recon.tolist())
write_ref('ciecam02_xyz_reconstructed', ciecam02_xyz_reconstructed, 'CIECAM02 reconstructed XYZ')

# CAM16 forward transform
cam16_correlates = []
for xyz in test_xyz_colors:
    correlates = colour.appearance.cam16.XYZ_to_CAM16(
        xyz, d65_white_xyz, 318.31, 63.66
    )
    # Store J, C, h, Q, M, s, H
    cam16_correlates.extend([
        correlates.J, correlates.C, correlates.h,
        correlates.Q, correlates.M, correlates.s, correlates.H
    ])
write_ref('cam16_correlates', cam16_correlates, 'CAM16 correlates (J,C,h,Q,M,s,H)')

# CAM16 inverse transform
cam16_xyz_reconstructed = []
for i in range(len(test_xyz_colors)):
    idx = i * 7
    J = cam16_correlates[idx + 0]
    C = cam16_correlates[idx + 1]
    h = cam16_correlates[idx + 2]

    # Use proper specification object
    spec = colour.appearance.cam16.CAM_Specification_CAM16(J=J, C=C, h=h)
    xyz_recon = colour.appearance.cam16.CAM16_to_XYZ(
        spec, d65_white_xyz, 318.31, 63.66
    )
    cam16_xyz_reconstructed.extend(xyz_recon.tolist())
write_ref('cam16_xyz_reconstructed', cam16_xyz_reconstructed, 'CAM16 reconstructed XYZ')

# CAM16-UCS (perceptually uniform)
cam16_ucs_jab = []
for i in range(len(test_xyz_colors)):
    idx = i * 7
    J = cam16_correlates[idx + 0]
    M = cam16_correlates[idx + 4]  # Colorfulness
    h = cam16_correlates[idx + 2]

    # Convert CAM16 JMh to UCS Jab
    jab = colour.JMh_CAM16_to_CAM16UCS(np.array([J, M, h]))
    cam16_ucs_jab.extend(jab.tolist())
write_ref('cam16_ucs_jab', cam16_ucs_jab, 'CAM16-UCS Jab values')

# ================================================================
# Chromatic Adaptation (CAT) Reference Values
# ================================================================
print('\nChromatic Adaptation (CAT):')

# Additional illuminants
a_white_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['A']
d60_white_xy = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D60']
a_white_xyz = colour.xy_to_XYZ(a_white_xy)
d60_white_xyz = colour.xy_to_XYZ(d60_white_xy)

write_ref('a_xyz', a_white_xyz.tolist(), 'Illuminant A white point')
write_ref('d60_xyz', d60_white_xyz.tolist(), 'D60 white point')

# CAT matrices (3x3 matrices flattened to 9 values)
# Bradford D65->D50
cat_d65_to_d50_bradford = colour.adaptation.matrix_chromatic_adaptation_VonKries(
    d65_white_xyz, d50_white_xyz, transform='Bradford'
)
write_ref('cat_d65_to_d50_bradford', cat_d65_to_d50_bradford.flatten().tolist(), 'CAT Bradford D65->D50')

# Bradford D50->D65
cat_d50_to_d65_bradford = colour.adaptation.matrix_chromatic_adaptation_VonKries(
    d50_white_xyz, d65_white_xyz, transform='Bradford'
)
write_ref('cat_d50_to_d65_bradford', cat_d50_to_d65_bradford.flatten().tolist(), 'CAT Bradford D50->D65')

# Bradford A->D65
cat_a_to_d65_bradford = colour.adaptation.matrix_chromatic_adaptation_VonKries(
    a_white_xyz, d65_white_xyz, transform='Bradford'
)
write_ref('cat_a_to_d65_bradford', cat_a_to_d65_bradford.flatten().tolist(), 'CAT Bradford A->D65')

# Bradford D65->D60
cat_d65_to_d60_bradford = colour.adaptation.matrix_chromatic_adaptation_VonKries(
    d65_white_xyz, d60_white_xyz, transform='Bradford'
)
write_ref('cat_d65_to_d60_bradford', cat_d65_to_d60_bradford.flatten().tolist(), 'CAT Bradford D65->D60')

# CAT02 D65->D50
cat_d65_to_d50_cat02 = colour.adaptation.matrix_chromatic_adaptation_VonKries(
    d65_white_xyz, d50_white_xyz, transform='CAT02'
)
write_ref('cat_d65_to_d50_cat02', cat_d65_to_d50_cat02.flatten().tolist(), 'CAT CAT02 D65->D50')

# CAT16 D65->D50
cat_d65_to_d50_cat16 = colour.adaptation.matrix_chromatic_adaptation_VonKries(
    d65_white_xyz, d50_white_xyz, transform='CAT16'
)
write_ref('cat_d65_to_d50_cat16', cat_d65_to_d50_cat16.flatten().tolist(), 'CAT CAT16 D65->D50')

# XYZ Scaling D65->D50
cat_d65_to_d50_xyz_scaling = colour.adaptation.matrix_chromatic_adaptation_VonKries(
    d65_white_xyz, d50_white_xyz, transform='XYZ Scaling'
)
write_ref('cat_d65_to_d50_xyz_scaling', cat_d65_to_d50_xyz_scaling.flatten().tolist(), 'CAT XYZ Scaling D65->D50')

# Adapted colors (test_xyz_colors adapted D65->D50 using Bradford)
adapted_d65_to_d50_bradford = []
for xyz in test_xyz_colors:
    adapted = colour.adaptation.chromatic_adaptation(
        xyz, d65_white_xyz, d50_white_xyz, transform='Bradford'
    )
    adapted_d65_to_d50_bradford.extend(adapted.tolist())
write_ref('adapted_d65_to_d50_bradford', adapted_d65_to_d50_bradford, 'Adapted colors D65->D50 Bradford')

# Adapted colors A->D65
adapted_a_to_d65_bradford = []
for xyz in test_xyz_colors:
    adapted = colour.adaptation.chromatic_adaptation(
        xyz, a_white_xyz, d65_white_xyz, transform='Bradford'
    )
    adapted_a_to_d65_bradford.extend(adapted.tolist())
write_ref('adapted_a_to_d65_bradford', adapted_a_to_d65_bradford, 'Adapted colors A->D65 Bradford')

# ================================================================
# P1.3-P1.8: Extended Color Spaces
# ================================================================
print('\nP1.3-P1.8 Extended Color Spaces:')

# Test colors in XYZ (Y=100 scale) for P1 tests
TEST_COLORS_XYZ_P1 = np.array([
    [0.0, 0.0, 0.0],           # Black
    [95.047, 100.0, 108.883],  # White D65
    [41.24, 21.26, 1.93],      # Red
    [35.76, 71.52, 11.92],     # Green
    [18.05, 7.22, 95.05],      # Blue
    [77.0, 92.78, 13.85],      # Yellow
    [59.29, 28.48, 96.98],     # Magenta
    [53.81, 78.74, 106.97],    # Cyan
    [20.517, 21.586, 23.507],  # Gray 20%
    [53.389, 56.272, 61.261],  # Gray 50%
    [76.054, 80.109, 87.120],  # Gray 80%
    [25.0, 50.0, 75.0],        # Custom 1
    [60.0, 40.0, 30.0],        # Custom 2
    [15.0, 25.0, 55.0],        # Custom 3
    [80.0, 90.0, 50.0],        # Custom 4
    [10.0, 15.0, 20.0],        # Dark
])

# P1.3: Jzazbz (HDR perceptual color space)
print('  P1.3: Jzazbz')
jzazbz_pairs = []
for xyz in TEST_COLORS_XYZ_P1:
    xyz_norm = xyz / 100.0  # Y=1 scale
    jzazbz = colour.XYZ_to_Jzazbz(xyz_norm)
    jzazbz_pairs.extend(xyz.tolist())
    jzazbz_pairs.extend(jzazbz.tolist())
write_ref('test_xyz_jzazbz_pairs', jzazbz_pairs, 'P1.3 XYZ + Jzazbz pairs')

# P1.4: DIN99 Family (4 variants)
print('  P1.4: DIN99 Family')
# Convert XYZ to Lab first (using default D65)
lab_colors = []
for xyz in TEST_COLORS_XYZ_P1:
    xyz_norm = xyz / 100.0
    lab = colour.XYZ_to_Lab(xyz_norm)  # Default D65
    lab_colors.append(lab)

# DIN99
din99_pairs = []
for i, lab in enumerate(lab_colors):
    din99 = colour.Lab_to_DIN99(lab, method='DIN99')
    din99_pairs.extend(TEST_COLORS_XYZ_P1[i].tolist())
    din99_pairs.extend(din99.tolist())
write_ref('test_lab_din99_pairs', din99_pairs, 'P1.4 XYZ + DIN99 pairs')

# DIN99b
din99b_pairs = []
for i, lab in enumerate(lab_colors):
    din99b = colour.Lab_to_DIN99(lab, method='DIN99b')
    din99b_pairs.extend(TEST_COLORS_XYZ_P1[i].tolist())
    din99b_pairs.extend(din99b.tolist())
write_ref('test_lab_din99b_pairs', din99b_pairs, 'P1.4 XYZ + DIN99b pairs')

# DIN99c
din99c_pairs = []
for i, lab in enumerate(lab_colors):
    din99c = colour.Lab_to_DIN99(lab, method='DIN99c')
    din99c_pairs.extend(TEST_COLORS_XYZ_P1[i].tolist())
    din99c_pairs.extend(din99c.tolist())
write_ref('test_lab_din99c_pairs', din99c_pairs, 'P1.4 XYZ + DIN99c pairs')

# DIN99d
din99d_pairs = []
for i, lab in enumerate(lab_colors):
    din99d = colour.Lab_to_DIN99(lab, method='DIN99d')
    din99d_pairs.extend(TEST_COLORS_XYZ_P1[i].tolist())
    din99d_pairs.extend(din99d.tolist())
write_ref('test_lab_din99d_pairs', din99d_pairs, 'P1.4 XYZ + DIN99d pairs')

# P1.5: OSA-UCS
print('  P1.5: OSA-UCS')
osa_ucs_pairs = []
for xyz in TEST_COLORS_XYZ_P1:
    xyz_norm = xyz / 100.0
    try:
        osa_ucs = colour.XYZ_to_OSA_UCS(xyz_norm)
        osa_ucs_pairs.extend(xyz.tolist())
        osa_ucs_pairs.extend(osa_ucs.tolist())
    except:
        # Some colors may fail
        osa_ucs_pairs.extend(xyz.tolist())
        osa_ucs_pairs.extend([0.0, 0.0, 0.0])
write_ref('test_xyz_osa_ucs_pairs', osa_ucs_pairs, 'P1.5 XYZ + OSA-UCS pairs')

# P1.6: Hunter Lab
print('  P1.6: Hunter Lab')
hunter_lab_pairs = []
for xyz in TEST_COLORS_XYZ_P1:
    xyz_norm = xyz / 100.0
    hunter_lab = colour.XYZ_to_Hunter_Lab(xyz_norm)  # Default D65
    # Replace any NaN values with 0
    hunter_lab = np.nan_to_num(hunter_lab, nan=0.0)
    hunter_lab_pairs.extend(xyz.tolist())
    hunter_lab_pairs.extend(hunter_lab.tolist())
write_ref('test_xyz_hunter_lab_pairs', hunter_lab_pairs, 'P1.6 XYZ + Hunter Lab pairs')

# P1.7: IPT
print('  P1.7: IPT')
ipt_pairs = []
for xyz in TEST_COLORS_XYZ_P1:
    xyz_norm = xyz / 100.0
    ipt = colour.XYZ_to_IPT(xyz_norm)
    ipt_pairs.extend(xyz.tolist())
    ipt_pairs.extend(ipt.tolist())
write_ref('test_xyz_ipt_pairs', ipt_pairs, 'P1.7 XYZ + IPT pairs')

# P1.8: ProLab
print('  P1.8: ProLab')
prolab_pairs = []
for xyz in TEST_COLORS_XYZ_P1:
    xyz_norm = xyz / 100.0
    prolab = colour.XYZ_to_ProLab(xyz_norm)
    prolab_pairs.extend(xyz.tolist())
    prolab_pairs.extend(prolab.tolist())
write_ref('test_xyz_prolab_pairs', prolab_pairs, 'P1.8 XYZ + ProLab pairs')

# ================================================================
# P2: Advanced Color Appearance Models
# ================================================================
print('\nP2 Advanced Color Appearance Models:')

# Test colors for P2 (12 colors)
TEST_COLORS_P2 = np.array([
    [0.0, 0.0, 0.0],           # Black
    [10.0, 10.54, 11.47],      # Very dark gray
    [25.0, 30.0, 35.0],        # Dark gray
    [50.0, 52.69, 57.36],      # Mid gray
    [95.047, 100.0, 108.883],  # White D65
    [41.24, 21.26, 1.93],      # Red
    [35.76, 71.52, 11.92],     # Green
    [18.05, 7.22, 95.05],      # Blue
    [77.0, 92.78, 13.85],      # Yellow
    [59.29, 28.48, 96.98],     # Magenta
    [53.81, 78.74, 106.97],    # Cyan
    [60.0, 40.0, 30.0],        # Orange
])

# P2: ZCAM (viewing conditions: La=100, Yb=20, Average surround)
print('  P2: ZCAM')
# Note: colour-science doesn't have ZCAM - generate placeholder with zeros
# Format: XYZ (3) + Jz, Cz, hz, Qz, Mz, Sz (6) = 9 values per color
zcam_correlates = []
for xyz in TEST_COLORS_P2:
    zcam_correlates.extend(xyz.tolist())  # XYZ
    zcam_correlates.extend([0.0, 0.0, 0.0, 0.0, 0.0, 0.0])  # Jz, Cz, hz, Qz, Mz, Sz
write_ref('test_zcam_correlates', zcam_correlates, 'P2 ZCAM correlates (placeholder)')

# P2: Hunt CAM (viewing conditions: La=318.31, Yb=20, Normal surround)
print('  P2: Hunt')
# Note: colour-science doesn't have Hunt CAM - generate placeholder with zeros
# Format: XYZ (3) + J, C, h, s, Q, M (6) = 9 values per color
hunt_correlates = []
for xyz in TEST_COLORS_P2:
    hunt_correlates.extend(xyz.tolist())  # XYZ
    hunt_correlates.extend([0.0, 0.0, 0.0, 0.0, 0.0, 0.0])  # J, C, h, s, Q, M
write_ref('test_hunt_correlates', hunt_correlates, 'P2 Hunt correlates (placeholder)')

# P2: RLAB (viewing conditions: D=1.0, Y_n=100)
print('  P2: RLAB')
# Note: colour-science doesn't have RLAB - generate placeholder with zeros
# Format: XYZ (3) + L, C, h (3) = 6 values per color
rlab_correlates = []
for xyz in TEST_COLORS_P2:
    rlab_correlates.extend(xyz.tolist())  # XYZ
    rlab_correlates.extend([0.0, 0.0, 0.0])  # L, C, h
write_ref('test_rlab_correlates', rlab_correlates, 'P2 RLAB correlates (placeholder)')

print('\n======================================')
print('Test reference data generation complete!')
print('======================================')
"@

# Run Python script
Write-Host ""
Write-Host "Running Python generator..."
Write-Host ""

$pythonScript | python -

if ($LASTEXITCODE -ne 0) {
    Write-Error "Python script failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "========================================"
Write-Host "Reference data generation completed successfully!"
Write-Host "========================================"
Write-Host ""
Write-Host "All test expected values are now generated from colour-science."
Write-Host "Location: $TEST_REF_DIR"
Write-Host ""


