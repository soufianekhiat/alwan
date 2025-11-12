#!/usr/bin/env pwsh
# Alwan Data Generation Script
# Generates C-parsable CSV data files from Python Colour package

param(
    [switch]$Force = $false
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Alwan Data Generation Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check Python version
Write-Host "Checking Python installation..." -ForegroundColor Yellow
try {
    $pythonVersion = python --version 2>&1
    Write-Host "  Found: $pythonVersion" -ForegroundColor Green
} catch {
    Write-Host "  ERROR: Python not found in PATH" -ForegroundColor Red
    exit 1
}

# Check if colour-science is installed
Write-Host "Checking colour-science package..." -ForegroundColor Yellow

# Use -W ignore to suppress warnings and only get version
$checkInstall = python -W ignore -c "import colour; print(colour.__version__)" 2>$null
if ($LASTEXITCODE -eq 0 -and $checkInstall) {
    Write-Host "  colour-science version: $checkInstall" -ForegroundColor Green
} else {
    Write-Host "  colour-science not found. Installing..." -ForegroundColor Yellow
    Write-Host "  This may take a minute..." -ForegroundColor Yellow

    $pipOutput = python -m pip install colour-science 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ERROR: Failed to install colour-science" -ForegroundColor Red
        Write-Host $pipOutput
        exit 1
    }

    $colourVersion = python -W ignore -c "import colour; print(colour.__version__)" 2>$null
    if ($LASTEXITCODE -eq 0 -and $colourVersion) {
        Write-Host "  colour-science installed successfully (version: $colourVersion)" -ForegroundColor Green
    } else {
        Write-Host "  ERROR: colour-science still not available after installation" -ForegroundColor Red
        exit 1
    }
}

# Create output directories
$dataDir = "data"
$rgbSpacesDir = "$dataDir/rgb_spaces"

if (-not (Test-Path $dataDir)) {
    New-Item -ItemType Directory -Path $dataDir | Out-Null
}

if (-not (Test-Path $rgbSpacesDir)) {
    New-Item -ItemType Directory -Path $rgbSpacesDir | Out-Null
}

Write-Host ""
Write-Host "Generating data files..." -ForegroundColor Yellow

# Create Python script for data generation
$pythonScript = @"
import warnings
warnings.filterwarnings('ignore')  # Suppress matplotlib and other warnings

import colour
import sys

def format_scalar(value):
    """Format scalar with maximum precision for C parsing"""
    return f"{value:.17g}"

def write_xy(filepath, x, y):
    """Write xy chromaticity coordinates to CSV"""
    with open(filepath, 'w', newline='') as f:
        f.write(f"{format_scalar(x)},{format_scalar(y)}\n")

def write_primaries_3x2(filepath, primaries):
    """Write RGB primaries as 3x2 matrix (rx,ry,gx,gy,bx,by)"""
    with open(filepath, 'w', newline='') as f:
        values = [
            format_scalar(primaries[0][0]), format_scalar(primaries[0][1]),
            format_scalar(primaries[1][0]), format_scalar(primaries[1][1]),
            format_scalar(primaries[2][0]), format_scalar(primaries[2][1])
        ]
        f.write(','.join(values) + '\n')

def write_rgb_space_csv(filepath, name, space):
    """Write RGB space definition to CSV (numeric only for C parsing)"""
    with open(filepath, 'w', newline='') as f:
        primaries = space.primaries
        whitepoint = space.whitepoint

        # Only write numeric values: rx,ry,gx,gy,bx,by,wx,wy
        values = [
            format_scalar(primaries[0][0]), format_scalar(primaries[0][1]),
            format_scalar(primaries[1][0]), format_scalar(primaries[1][1]),
            format_scalar(primaries[2][0]), format_scalar(primaries[2][1]),
            format_scalar(whitepoint[0]), format_scalar(whitepoint[1])
        ]
        f.write(','.join(values) + '\n')

# Generate illuminant data
print("Generating illuminants...")

# D65 (2° observer)
d65 = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']
write_xy('data/d65_xy.csv', d65[0], d65[1])
print(f"  data/d65_xy.csv: {format_scalar(d65[0])}, {format_scalar(d65[1])}")

# D60 (2° observer) - used by ACES
d60 = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D60']
write_xy('data/d60_xy.csv', d60[0], d60[1])
print(f"  data/d60_xy.csv: {format_scalar(d60[0])}, {format_scalar(d60[1])}")

# D50 (2° observer) - ICC profile connection space
d50 = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D50']
write_xy('data/d50_xy.csv', d50[0], d50[1])
print(f"  data/d50_xy.csv: {format_scalar(d50[0])}, {format_scalar(d50[1])}")

# D55 (2° observer)
d55 = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D55']
write_xy('data/d55_xy.csv', d55[0], d55[1])
print(f"  data/d55_xy.csv: {format_scalar(d55[0])}, {format_scalar(d55[1])}")

# Illuminant A (2° observer) - incandescent
a = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['A']
write_xy('data/a_xy.csv', a[0], a[1])
print(f"  data/a_xy.csv: {format_scalar(a[0])}, {format_scalar(a[1])}")

# Illuminant E (2° observer) - equal energy
e = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['E']
write_xy('data/e_xy.csv', e[0], e[1])
print(f"  data/e_xy.csv: {format_scalar(e[0])}, {format_scalar(e[1])}")

# Generate RGB space primaries
print("\nGenerating RGB space primaries...")

# sRGB primaries (legacy format)
srgb = colour.RGB_COLOURSPACES['sRGB']
write_primaries_3x2('data/srgb_primaries_3x2.csv', srgb.primaries)
print(f"  data/srgb_primaries_3x2.csv")

# RGB color spaces with full definitions
rgb_spaces = [
    ('sRGB', 'sRGB'),
    ('BT.709', 'ITU-R BT.709'),
    ('Display P3', 'Display P3'),
    ('BT.2020', 'ITU-R BT.2020'),
    ('ACES2065-1', 'ACES2065-1'),
    ('ACEScg', 'ACEScg'),
    ('ACESproxy', 'ACESproxy'),
]

print("\nGenerating RGB space definitions...")
for filename, space_name in rgb_spaces:
    try:
        space = colour.RGB_COLOURSPACES[space_name]
        filepath = f'data/rgb_spaces/{filename.lower().replace(" ", "_").replace(".", "")}.csv'
        write_rgb_space_csv(filepath, space_name, space)

        primaries = space.primaries
        whitepoint = space.whitepoint
        print(f"  {filepath}")
        print(f"    Primaries: R({format_scalar(primaries[0][0])}, {format_scalar(primaries[0][1])}), "
              f"G({format_scalar(primaries[1][0])}, {format_scalar(primaries[1][1])}), "
              f"B({format_scalar(primaries[2][0])}, {format_scalar(primaries[2][1])})")
        print(f"    Whitepoint: ({format_scalar(whitepoint[0])}, {format_scalar(whitepoint[1])})")
    except KeyError:
        print(f"  WARNING: RGB space '{space_name}' not found in colour-science", file=sys.stderr)
        continue

# Generate test fixtures
print("\nGenerating test fixtures...")
import os
os.makedirs('data/fixtures', exist_ok=True)

# Test fixture: sRGB descriptor (primaries + white point)
srgb = colour.RGB_COLOURSPACES['sRGB']
with open('src/alwan/data/fixtures/srgb_descriptor.csv', 'w', newline='') as f:
    values = [
        format_scalar(srgb.primaries[0][0]), format_scalar(srgb.primaries[0][1]),
        format_scalar(srgb.primaries[1][0]), format_scalar(srgb.primaries[1][1]),
        format_scalar(srgb.primaries[2][0]), format_scalar(srgb.primaries[2][1]),
        format_scalar(srgb.whitepoint[0]), format_scalar(srgb.whitepoint[1])
    ]
    f.write(','.join(values) + '\n')
print(f"  data/fixtures/srgb_descriptor.csv")

# Test fixture: sRGB RGB->XYZ matrix (reference from colour-science)
try:
    import numpy as np
    rgb_to_xyz = srgb.matrix_RGB_to_XYZ
    with open('src/alwan/data/fixtures/srgb_rgb_to_xyz.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in rgb_to_xyz.flatten()]
        f.write(','.join(values) + '\n')
    print(f"  data/fixtures/srgb_rgb_to_xyz.csv")
except Exception as e:
    print(f"  Warning: Could not generate sRGB matrix: {e}", file=sys.stderr)

# Test fixture: BT.2020 descriptor
bt2020 = colour.RGB_COLOURSPACES['ITU-R BT.2020']
with open('src/alwan/data/fixtures/bt2020_descriptor.csv', 'w', newline='') as f:
    values = [
        format_scalar(bt2020.primaries[0][0]), format_scalar(bt2020.primaries[0][1]),
        format_scalar(bt2020.primaries[1][0]), format_scalar(bt2020.primaries[1][1]),
        format_scalar(bt2020.primaries[2][0]), format_scalar(bt2020.primaries[2][1]),
        format_scalar(bt2020.whitepoint[0]), format_scalar(bt2020.whitepoint[1])
    ]
    f.write(','.join(values) + '\n')
print(f"  data/fixtures/bt2020_descriptor.csv")

# Test fixture: ACES AP0 descriptor
aces_ap0 = colour.RGB_COLOURSPACES['ACES2065-1']
with open('src/alwan/data/fixtures/aces_ap0_descriptor.csv', 'w', newline='') as f:
    values = [
        format_scalar(aces_ap0.primaries[0][0]), format_scalar(aces_ap0.primaries[0][1]),
        format_scalar(aces_ap0.primaries[1][0]), format_scalar(aces_ap0.primaries[1][1]),
        format_scalar(aces_ap0.primaries[2][0]), format_scalar(aces_ap0.primaries[2][1]),
        format_scalar(aces_ap0.whitepoint[0]), format_scalar(aces_ap0.whitepoint[1])
    ]
    f.write(','.join(values) + '\n')
print(f"  data/fixtures/aces_ap0_descriptor.csv")

# Test fixture: ACES AP1 descriptor
aces_ap1 = colour.RGB_COLOURSPACES['ACEScg']
with open('src/alwan/data/fixtures/aces_ap1_descriptor.csv', 'w', newline='') as f:
    values = [
        format_scalar(aces_ap1.primaries[0][0]), format_scalar(aces_ap1.primaries[0][1]),
        format_scalar(aces_ap1.primaries[1][0]), format_scalar(aces_ap1.primaries[1][1]),
        format_scalar(aces_ap1.primaries[2][0]), format_scalar(aces_ap1.primaries[2][1]),
        format_scalar(aces_ap1.whitepoint[0]), format_scalar(aces_ap1.whitepoint[1])
    ]
    f.write(','.join(values) + '\n')
print(f"  data/fixtures/aces_ap1_descriptor.csv")

# ================================================================
# Test Fixtures: Color Space Conversions
# ================================================================
print("\nGenerating color space conversion test fixtures...")

# White points in XYZ (normalized to Y=1) for C code
d65_xyz = colour.xy_to_XYZ(d65)
d50_xyz = colour.xy_to_XYZ(d50)

# Note: colour-science XYZ_to_Lab expects xy chromaticity as illuminant parameter, not XYZ!

# Test case 1: XYZ ↔ xyY round-trip
test_xyz_values = [
    [0.95047, 1.0, 1.08883],     # D65 white
    [0.5, 0.5, 0.5],             # Mid-gray
    [0.412456, 0.212673, 0.019334],  # sRGB red
    [0.0, 0.0, 0.0],             # Black
]

# Flatten XYZ values to single line
with open('src/alwan/data/fixtures/xyz_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/xyz_values.csv")

# Compute corresponding xyY values and flatten to single line
with open('src/alwan/data/fixtures/xyy_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        xyy = colour.XYZ_to_xyY(xyz)
        all_values.extend([format_scalar(v) for v in xyy])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/xyy_values.csv")

# Test case 2: XYZ ↔ Lab (D65 white point)
with open('src/alwan/data/fixtures/lab_d65_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        lab = colour.XYZ_to_Lab(xyz, illuminant=d65)
        all_values.extend([format_scalar(v) for v in lab])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/lab_d65_values.csv")

# Test case 3: XYZ ↔ Lab (D50 white point)
with open('src/alwan/data/fixtures/lab_d50_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        lab = colour.XYZ_to_Lab(xyz, illuminant=d50)
        all_values.extend([format_scalar(v) for v in lab])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/lab_d50_values.csv")

# Test case 4: XYZ ↔ Luv (D65 white point)
with open('src/alwan/data/fixtures/luv_d65_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        luv = colour.XYZ_to_Luv(xyz, illuminant=d65)
        all_values.extend([format_scalar(v) for v in luv])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/luv_d65_values.csv")

# Test case 5: Lab ↔ LCh
test_lab_values = [
    [50.0, 25.0, 25.0],
    [75.0, -10.0, 50.0],
    [25.0, 0.0, 0.0],
]

with open('src/alwan/data/fixtures/lab_for_lch.csv', 'w', newline='') as f:
    all_values = []
    for lab in test_lab_values:
        all_values.extend([format_scalar(v) for v in lab])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/lab_for_lch.csv")

with open('src/alwan/data/fixtures/lch_values.csv', 'w', newline='') as f:
    all_values = []
    for lab in test_lab_values:
        lch = colour.Lab_to_LCHab(lab)
        all_values.extend([format_scalar(v) for v in lch])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/lch_values.csv")

# Test case 6: Luv ↔ LCh(uv)
test_luv_values = [
    [50.0, 20.0, 30.0],
    [75.0, -15.0, 45.0],
    [25.0, 0.0, 0.0],
]

with open('src/alwan/data/fixtures/luv_for_lchuv.csv', 'w', newline='') as f:
    all_values = []
    for luv in test_luv_values:
        all_values.extend([format_scalar(v) for v in luv])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/luv_for_lchuv.csv")

with open('src/alwan/data/fixtures/lchuv_values.csv', 'w', newline='') as f:
    all_values = []
    for luv in test_luv_values:
        lchuv = colour.Luv_to_LCHuv(luv)
        all_values.extend([format_scalar(v) for v in lchuv])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/lchuv_values.csv")

# ================================================================
# Test Fixtures: ΔE Metrics
# ================================================================
print("\nGenerating ΔE metric test fixtures...")

# ΔE test pairs (Lab1, Lab2, ΔE76, ΔE94, ΔE_CMC(2:1), ΔE00)
delta_e_test_pairs = [
    # Pair 1: Small difference
    ([50.0, 2.5, -1.0], [50.0, 0.0, -1.0]),
    # Pair 2: Moderate difference
    ([50.0, 2.5, -1.0], [60.0, 5.0, 3.0]),
    # Pair 3: Large difference
    ([50.0, 2.5, -1.0], [80.0, 30.0, 20.0]),
    # Pair 4: Hue shift
    ([50.0, 10.0, 0.0], [50.0, 0.0, 10.0]),
]

# Store Lab pairs (flatten to single line)
with open('src/alwan/data/fixtures/delta_e_lab1.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        all_values.extend([format_scalar(v) for v in lab1])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/delta_e_lab1.csv")

with open('src/alwan/data/fixtures/delta_e_lab2.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        all_values.extend([format_scalar(v) for v in lab2])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/delta_e_lab2.csv")

# Compute ΔE76 (flatten to single line)
with open('src/alwan/data/fixtures/delta_e_76.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        # ΔE76 is Euclidean distance in Lab space
        dL = lab2[0] - lab1[0]
        da = lab2[1] - lab1[1]
        db = lab2[2] - lab1[2]
        de76 = np.sqrt(dL*dL + da*da + db*db)
        all_values.append(format_scalar(de76))
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/delta_e_76.csv")

# Compute ΔE94 (flatten to single line)
with open('src/alwan/data/fixtures/delta_e_94.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        de94 = colour.difference.delta_E_CIE1994(np.array(lab1), np.array(lab2))
        all_values.append(format_scalar(de94))
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/delta_e_94.csv")

# Compute ΔE CMC(2:1) - acceptability (flatten to single line)
with open('src/alwan/data/fixtures/delta_e_cmc.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        de_cmc = colour.difference.delta_E_CMC(np.array(lab1), np.array(lab2), l=2, c=1)
        all_values.append(format_scalar(de_cmc))
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/delta_e_cmc.csv")

# Compute ΔE2000 (flatten to single line)
with open('src/alwan/data/fixtures/delta_e_2000.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        de2000 = colour.difference.delta_E_CIE2000(np.array(lab1), np.array(lab2))
        all_values.append(format_scalar(de2000))
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/delta_e_2000.csv")

# Store D65 and D50 white points in XYZ for tests
with open('src/alwan/data/fixtures/d65_xyz.csv', 'w', newline='') as f:
    f.write(','.join([format_scalar(v) for v in d65_xyz]) + '\n')
print(f"  data/fixtures/d65_xyz.csv")

with open('src/alwan/data/fixtures/d50_xyz.csv', 'w', newline='') as f:
    f.write(','.join([format_scalar(v) for v in d50_xyz]) + '\n')
print(f"  data/fixtures/d50_xyz.csv")

# ================================================================
# Test Fixtures: Chromatic Adaptation Transform (CAT)
# ================================================================
print("\nGenerating chromatic adaptation test fixtures...")

# White points in XYZ (normalized to Y=1)
d60_xyz = colour.xy_to_XYZ(d60)
d55_xyz = colour.xy_to_XYZ(d55)
a_xyz = colour.xy_to_XYZ(a)
e_xyz = colour.xy_to_XYZ(e)

# Store additional white points for tests (d65 and d50 already stored above)
white_points = {
    'd60': d60_xyz,
    'd55': d55_xyz,
    'a': a_xyz,
    'e': e_xyz,
}

for name, wp_xyz in white_points.items():
    with open(f'data/fixtures/{name}_xyz.csv', 'w', newline='') as f:
        f.write(','.join([format_scalar(v) for v in wp_xyz]) + '\n')
    print(f"  data/fixtures/{name}_xyz.csv")

# CAT matrix test cases: (src_white, dst_white, method)
cat_test_cases = [
    ('d65', 'd50', 'Bradford'),
    ('d50', 'd65', 'Bradford'),
    ('a', 'd65', 'Bradford'),
    ('d65', 'd60', 'Bradford'),
    ('d65', 'd50', 'CAT02'),
    ('d65', 'd50', 'CAT16'),
    ('d65', 'd50', 'XYZ Scaling'),
]

# Map for white points (including already generated ones)
all_white_points = {
    'd65': d65_xyz,
    'd60': d60_xyz,
    'd55': d55_xyz,
    'd50': d50_xyz,
    'a': a_xyz,
    'e': e_xyz,
}

for src_name, dst_name, method in cat_test_cases:
    src_wp = all_white_points[src_name]
    dst_wp = all_white_points[dst_name]

    # Compute CAT matrix
    cat_matrix = colour.adaptation.matrix_chromatic_adaptation_VonKries(
        src_wp, dst_wp, transform=method
    )

    # Flatten matrix to single line (row-major order)
    filename = f'cat_{src_name}_to_{dst_name}_{method.lower().replace(" ", "_")}.csv'
    with open(f'data/fixtures/{filename}', 'w', newline='') as f:
        values = [format_scalar(v) for v in cat_matrix.flatten()]
        f.write(','.join(values) + '\n')
    print(f"  data/fixtures/{filename}")

# Test XYZ colors for adaptation
test_xyz_colors = [
    [0.95047, 1.0, 1.08883],     # D65 white
    [0.5, 0.5, 0.5],             # Mid-gray
    [0.412456, 0.212673, 0.019334],  # sRGB red (D65)
    [0.357576, 0.715152, 0.119192],  # sRGB green (D65)
    [0.180437, 0.072175, 0.950304],  # sRGB blue (D65)
]

# Store test colors
with open('src/alwan/data/fixtures/test_xyz_colors.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_colors:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/test_xyz_colors.csv")

# Adaptation test case: D65 → D50 (Bradford)
adapted_colors = []
for xyz in test_xyz_colors:
    adapted = colour.adaptation.chromatic_adaptation_VonKries(
        xyz, d65_xyz, d50_xyz, transform='Bradford'
    )
    adapted_colors.append(adapted)

with open('src/alwan/data/fixtures/adapted_d65_to_d50_bradford.csv', 'w', newline='') as f:
    all_values = []
    for xyz in adapted_colors:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/adapted_d65_to_d50_bradford.csv")

# Adaptation test case: A → D65 (Bradford)
adapted_colors_a = []
for xyz in test_xyz_colors:
    adapted = colour.adaptation.chromatic_adaptation_VonKries(
        xyz, a_xyz, d65_xyz, transform='Bradford'
    )
    adapted_colors_a.append(adapted)

with open('src/alwan/data/fixtures/adapted_a_to_d65_bradford.csv', 'w', newline='') as f:
    all_values = []
    for xyz in adapted_colors_a:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/adapted_a_to_d65_bradford.csv")

# ================================================================
# M7: CIECAM02 Test Fixtures
# ================================================================
print("\nGenerating CIECAM02 test fixtures...")

# Define standard CIECAM02 viewing conditions (D65, average surround)
# XYZ_w: white point (Y=100 for standard viewing)
# L_A: adapting luminance (cd/m²), typically 20% of white Y
# Y_b: background relative luminance (typically 20 for 20% gray)
XYZ_w = d65_xyz * 100  # Scale to Y=100
L_A = 64.0             # Adapting luminance
Y_b = 20.0             # Background luminance
surround = colour.VIEWING_CONDITIONS_CIECAM02['Average']

# Write viewing conditions to fixture for use in tests
with open('src/alwan/data/fixtures/cam_viewing_conditions.csv', 'w', newline='') as f:
    # XYZ_w (3 values), L_A (1 value), Y_b (1 value)
    values = [format_scalar(v) for v in XYZ_w]
    values.extend([format_scalar(L_A), format_scalar(Y_b)])
    f.write(','.join(values) + '\n')
print(f"  data/fixtures/cam_viewing_conditions.csv (XYZ_w, L_A, Y_b)")

# Test XYZ colors for CIECAM02
# Use exact computed XYZ_w for white point to ensure J=100
ciecam02_test_xyz = [
    list(XYZ_w),                   # D65 white (exact match to XYZ_w)
    [50.0, 50.0, 50.0],            # Mid-gray
    [41.2456, 21.2673, 1.9334],    # sRGB red (D65, Y=100 scale)
    [35.7576, 71.5152, 11.9192],   # sRGB green
    [18.0437, 7.2175, 95.0304],    # sRGB blue
    [77.0, 92.8, 10.1],            # Lime
    [31.4, 15.9, 9.8],             # Brown
]

# Compute CIECAM02 correlates for each test color
ciecam02_correlates = []
for xyz in ciecam02_test_xyz:
    xyz_arr = np.array(xyz)
    spec = colour.XYZ_to_CIECAM02(xyz_arr, XYZ_w, L_A, Y_b, surround)
    # Store J, C, h, Q, M, s, H
    ciecam02_correlates.append([spec.J, spec.C, spec.h, spec.Q, spec.M, spec.s, spec.H])

# Write test XYZ colors
with open('src/alwan/data/fixtures/ciecam02_xyz_input.csv', 'w', newline='') as f:
    all_values = []
    for xyz in ciecam02_test_xyz:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/ciecam02_xyz_input.csv ({len(ciecam02_test_xyz)} colors)")

# Write CIECAM02 correlates (J, C, h, Q, M, s, H for each color)
with open('src/alwan/data/fixtures/ciecam02_correlates.csv', 'w', newline='') as f:
    all_values = []
    for corr in ciecam02_correlates:
        all_values.extend([format_scalar(v) for v in corr])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/ciecam02_correlates.csv ({len(ciecam02_correlates)} colors)")

# Test inverse: correlates → XYZ
# Use J, C, h from the correlates to reconstruct XYZ
ciecam02_xyz_reconstructed = []
for corr in ciecam02_correlates:
    # Create specification from J, C, h
    spec = colour.CAM_Specification_CIECAM02(J=corr[0], C=corr[1], h=corr[2])
    xyz_recon = colour.CIECAM02_to_XYZ(spec, XYZ_w, L_A, Y_b, surround)
    ciecam02_xyz_reconstructed.append(xyz_recon)

# Write reconstructed XYZ (should match input within numerical tolerance)
with open('src/alwan/data/fixtures/ciecam02_xyz_reconstructed.csv', 'w', newline='') as f:
    all_values = []
    for xyz in ciecam02_xyz_reconstructed:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/ciecam02_xyz_reconstructed.csv ({len(ciecam02_xyz_reconstructed)} colors)")

# ================================================================
# M8: CAM16 Test Fixtures
# ================================================================
print("\nGenerating CAM16 test fixtures...")

# Use same viewing conditions as CIECAM02 for consistency
# Compute CAM16 correlates for test colors
cam16_correlates = []
for xyz in ciecam02_test_xyz:
    xyz_arr = np.array(xyz)
    spec = colour.XYZ_to_CAM16(xyz_arr, XYZ_w, L_A, Y_b, surround)
    # Store J, C, h, Q, M, s, H
    cam16_correlates.append([spec.J, spec.C, spec.h, spec.Q, spec.M, spec.s, spec.H])

# Write CAM16 correlates (J, C, h, Q, M, s, H for each color)
with open('src/alwan/data/fixtures/cam16_correlates.csv', 'w', newline='') as f:
    all_values = []
    for corr in cam16_correlates:
        all_values.extend([format_scalar(v) for v in corr])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/cam16_correlates.csv ({len(cam16_correlates)} colors)")

# Test inverse: correlates -> XYZ
cam16_xyz_reconstructed = []
for corr in cam16_correlates:
    # Create specification from J, C, h
    spec = colour.CAM_Specification_CAM16(J=corr[0], C=corr[1], h=corr[2])
    xyz_recon = colour.CAM16_to_XYZ(spec, XYZ_w, L_A, Y_b, surround)
    cam16_xyz_reconstructed.append(xyz_recon)

# Write reconstructed XYZ
with open('src/alwan/data/fixtures/cam16_xyz_reconstructed.csv', 'w', newline='') as f:
    all_values = []
    for xyz in cam16_xyz_reconstructed:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/cam16_xyz_reconstructed.csv ({len(cam16_xyz_reconstructed)} colors)")

# Test CAM16-UCS transform
cam16_ucs_jab = []
for corr in cam16_correlates:
    # Convert J, M, h to CAM16-UCS Jab
    J_prime = 1.7 * corr[0] / (1.0 + 0.007 * corr[0])
    M = corr[4]  # Colorfulness
    M_prime = (1.0 / 0.0228) * np.log(1.0 + 0.0228 * M)
    h_rad = np.radians(corr[2])
    a_prime = M_prime * np.cos(h_rad)
    b_prime = M_prime * np.sin(h_rad)
    cam16_ucs_jab.append([J_prime, a_prime, b_prime])

# Write CAM16-UCS Jab coordinates
with open('src/alwan/data/fixtures/cam16_ucs_jab.csv', 'w', newline='') as f:
    all_values = []
    for jab in cam16_ucs_jab:
        all_values.extend([format_scalar(v) for v in jab])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/cam16_ucs_jab.csv ({len(cam16_ucs_jab)} colors)")

# ================================================================
# M5: Spectral Data - Color Matching Functions (CMFs)
# ================================================================
print("\nGenerating Color Matching Functions (CMFs)...")

# Create CMF and illuminants directories
os.makedirs('data/cmf', exist_ok=True)
os.makedirs('data/illuminants', exist_ok=True)

# CIE 1931 2° Standard Observer (360-830nm, 1nm steps)
# colour-science provides this in standard_observers
cmfs_1931 = colour.MSDS_CMFS['CIE 1931 2 Degree Standard Observer']

# Extract wavelengths and values for 360-830nm range
wavelengths = cmfs_1931.wavelengths
x_bar_vals = cmfs_1931.values[:, 0]
y_bar_vals = cmfs_1931.values[:, 1]
z_bar_vals = cmfs_1931.values[:, 2]

# Filter to 360-830nm range if needed
mask = (wavelengths >= 360) & (wavelengths <= 830)
wavelengths_filtered = wavelengths[mask]
x_bar_filtered = x_bar_vals[mask]
y_bar_filtered = y_bar_vals[mask]
z_bar_filtered = z_bar_vals[mask]

# Write CMF data (one value per line for easier C parsing)
with open('src/alwan/data/cmf/cie_1931_2deg_x_360_830_1nm.csv', 'w', newline='') as f:
    values = [format_scalar(v) for v in x_bar_filtered]
    f.write(','.join(values) + '\n')
print(f"  data/cmf/cie_1931_2deg_x_360_830_1nm.csv ({len(x_bar_filtered)} samples)")

with open('src/alwan/data/cmf/cie_1931_2deg_y_360_830_1nm.csv', 'w', newline='') as f:
    values = [format_scalar(v) for v in y_bar_filtered]
    f.write(','.join(values) + '\n')
print(f"  data/cmf/cie_1931_2deg_y_360_830_1nm.csv ({len(y_bar_filtered)} samples)")

with open('src/alwan/data/cmf/cie_1931_2deg_z_360_830_1nm.csv', 'w', newline='') as f:
    values = [format_scalar(v) for v in z_bar_filtered]
    f.write(','.join(values) + '\n')
print(f"  data/cmf/cie_1931_2deg_z_360_830_1nm.csv ({len(z_bar_filtered)} samples)")

# CIE 1964 10° Standard Observer (360-830nm, 1nm steps)
try:
    cmfs_1964 = colour.MSDS_CMFS['CIE 1964 10 Degree Standard Observer']

    x_bar_1964 = cmfs_1964.values[:, 0]
    y_bar_1964 = cmfs_1964.values[:, 1]
    z_bar_1964 = cmfs_1964.values[:, 2]
    wavelengths_1964 = cmfs_1964.wavelengths

    mask_1964 = (wavelengths_1964 >= 360) & (wavelengths_1964 <= 830)
    x_bar_1964_filtered = x_bar_1964[mask_1964]
    y_bar_1964_filtered = y_bar_1964[mask_1964]
    z_bar_1964_filtered = z_bar_1964[mask_1964]

    with open('src/alwan/data/cmf/cie_1964_10deg_x_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in x_bar_1964_filtered]
        f.write(','.join(values) + '\n')
    print(f"  data/cmf/cie_1964_10deg_x_360_830_1nm.csv ({len(x_bar_1964_filtered)} samples)")

    with open('src/alwan/data/cmf/cie_1964_10deg_y_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in y_bar_1964_filtered]
        f.write(','.join(values) + '\n')
    print(f"  data/cmf/cie_1964_10deg_y_360_830_1nm.csv ({len(y_bar_1964_filtered)} samples)")

    with open('src/alwan/data/cmf/cie_1964_10deg_z_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in z_bar_1964_filtered]
        f.write(','.join(values) + '\n')
    print(f"  data/cmf/cie_1964_10deg_z_360_830_1nm.csv ({len(z_bar_1964_filtered)} samples)")
except Exception as e:
    print(f"  Warning: Could not generate CIE 1964 10° CMFs: {e}", file=sys.stderr)

# CIE 2015 2° Standard Observer (360-830nm, 1nm steps)
# M6: Add physiologically-based 2015 observers (based on CIE 2006/2012 work)
try:
    cmfs_2012_2 = colour.MSDS_CMFS['CIE 2015 2 Degree Standard Observer']

    x_bar_2012_2 = cmfs_2012_2.values[:, 0]
    y_bar_2012_2 = cmfs_2012_2.values[:, 1]
    z_bar_2012_2 = cmfs_2012_2.values[:, 2]
    wavelengths_2012_2 = cmfs_2012_2.wavelengths

    mask_2012_2 = (wavelengths_2012_2 >= 360) & (wavelengths_2012_2 <= 830)
    x_bar_2012_2_filtered = x_bar_2012_2[mask_2012_2]
    y_bar_2012_2_filtered = y_bar_2012_2[mask_2012_2]
    z_bar_2012_2_filtered = z_bar_2012_2[mask_2012_2]

    with open('src/alwan/data/cmf/cie_2012_2deg_x_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in x_bar_2012_2_filtered]
        f.write(','.join(values) + '\n')
    print(f"  data/cmf/cie_2012_2deg_x_360_830_1nm.csv ({len(x_bar_2012_2_filtered)} samples)")

    with open('src/alwan/data/cmf/cie_2012_2deg_y_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in y_bar_2012_2_filtered]
        f.write(','.join(values) + '\n')
    print(f"  data/cmf/cie_2012_2deg_y_360_830_1nm.csv ({len(y_bar_2012_2_filtered)} samples)")

    with open('src/alwan/data/cmf/cie_2012_2deg_z_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in z_bar_2012_2_filtered]
        f.write(','.join(values) + '\n')
    print(f"  data/cmf/cie_2012_2deg_z_360_830_1nm.csv ({len(z_bar_2012_2_filtered)} samples)")
except Exception as e:
    print(f"  Warning: Could not generate CIE 2012 2° CMFs: {e}", file=sys.stderr)

# CIE 2015 10° Standard Observer (360-830nm, 1nm steps)
try:
    cmfs_2012_10 = colour.MSDS_CMFS['CIE 2015 10 Degree Standard Observer']

    x_bar_2012_10 = cmfs_2012_10.values[:, 0]
    y_bar_2012_10 = cmfs_2012_10.values[:, 1]
    z_bar_2012_10 = cmfs_2012_10.values[:, 2]
    wavelengths_2012_10 = cmfs_2012_10.wavelengths

    mask_2012_10 = (wavelengths_2012_10 >= 360) & (wavelengths_2012_10 <= 830)
    x_bar_2012_10_filtered = x_bar_2012_10[mask_2012_10]
    y_bar_2012_10_filtered = y_bar_2012_10[mask_2012_10]
    z_bar_2012_10_filtered = z_bar_2012_10[mask_2012_10]

    with open('src/alwan/data/cmf/cie_2012_10deg_x_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in x_bar_2012_10_filtered]
        f.write(','.join(values) + '\n')
    print(f"  data/cmf/cie_2012_10deg_x_360_830_1nm.csv ({len(x_bar_2012_10_filtered)} samples)")

    with open('src/alwan/data/cmf/cie_2012_10deg_y_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in y_bar_2012_10_filtered]
        f.write(','.join(values) + '\n')
    print(f"  data/cmf/cie_2012_10deg_y_360_830_1nm.csv ({len(y_bar_2012_10_filtered)} samples)")

    with open('src/alwan/data/cmf/cie_2012_10deg_z_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in z_bar_2012_10_filtered]
        f.write(','.join(values) + '\n')
    print(f"  data/cmf/cie_2012_10deg_z_360_830_1nm.csv ({len(z_bar_2012_10_filtered)} samples)")
except Exception as e:
    print(f"  Warning: Could not generate CIE 2012 10° CMFs: {e}", file=sys.stderr)

# ================================================================
# M5: Spectral Data - Illuminant SPDs
# ================================================================
print("\nGenerating Illuminant SPDs...")

# List of illuminants to generate
illuminant_names = ['A', 'D50', 'D55', 'D65', 'E',
                    'F1', 'F2', 'F3', 'F4', 'F5', 'F6',
                    'F7', 'F8', 'F9', 'F10', 'F11', 'F12']

for illum_name in illuminant_names:
    try:
        # Get illuminant SPD from colour-science
        if illum_name in colour.SDS_ILLUMINANTS:
            illum_spd = colour.SDS_ILLUMINANTS[illum_name]
        else:
            # Try with different naming
            try:
                illum_spd = colour.SDS_ILLUMINANTS[f'FL{illum_name[1:]}']  # F1 -> FL1
            except:
                print(f"  Warning: Illuminant {illum_name} not found", file=sys.stderr)
                continue

        # Resample to 360-830nm, 1nm steps
        # Use interpolate method instead of align for newer versions
        target_wavelengths = np.arange(360, 831, 1)
        resampled_values = np.interp(
            target_wavelengths,
            illum_spd.wavelengths,
            illum_spd.values,
            left=0.0,
            right=0.0
        )

        # Write illuminant data
        filename = f'data/illuminants/{illum_name}_360_830_1nm.csv'
        with open(filename, 'w', newline='') as f:
            formatted_values = [format_scalar(v) for v in resampled_values]
            f.write(','.join(formatted_values) + '\n')
        print(f"  {filename} ({len(resampled_values)} samples)")

    except Exception as e:
        print(f"  Warning: Could not generate illuminant {illum_name}: {e}", file=sys.stderr)

# ============================================================
# View Transform Matrices
# ============================================================
print("\n--- Generating View Transform Matrices ---")

# Create matrices directory
os.makedirs('data/matrices', exist_ok=True)

try:
    # AP1 to AP0 conversion matrix (ACEScg to ACES2065-1)
    print("\nGenerating ACES AP1→AP0 matrix...")

    # Get ACES color spaces from colour-science
    aces_ap0 = colour.RGB_COLOURSPACES['ACES2065-1']
    aces_ap1 = colour.RGB_COLOURSPACES['ACEScg']

    # Compute conversion matrix from AP1 to AP0
    # This uses the chromatic adaptation and primary conversion
    ap1_to_ap0 = colour.matrix_RGB_to_RGB(
        aces_ap1,
        aces_ap0,
        chromatic_adaptation_transform=None  # Both use D60, no CAT needed
    )

    # Write matrix as 9 comma-separated values (row-major 3x3)
    filename = 'data/matrices/aces_ap1_to_ap0.csv'
    with open(filename, 'w', newline='') as f:
        flat_matrix = ap1_to_ap0.flatten()
        formatted_values = [format_scalar(v) for v in flat_matrix]
        f.write(','.join(formatted_values) + '\n')
    print(f"  {filename} (3x3 matrix, 9 values)")

except Exception as e:
    print(f"  Warning: Could not generate AP1→AP0 matrix: {e}", file=sys.stderr)

try:
    # ACES ODT matrix for Rec.709 (simplified)
    print("\nGenerating ACES ODT matrix for Rec.709...")

    # ODT converts from ACES2065-1 (AP0, D60) to Rec.709 (D65)
    # This includes chromatic adaptation from D60 to D65
    aces_ap0 = colour.RGB_COLOURSPACES['ACES2065-1']
    rec709 = colour.RGB_COLOURSPACES['ITU-R BT.709']

    # Compute conversion matrix with Bradford chromatic adaptation
    odt_matrix = colour.matrix_RGB_to_RGB(
        aces_ap0,
        rec709,
        chromatic_adaptation_transform='Bradford'
    )

    # Write matrix as 9 comma-separated values (row-major 3x3)
    filename = 'data/matrices/aces_odt_rec709.csv'
    with open(filename, 'w', newline='') as f:
        flat_matrix = odt_matrix.flatten()
        formatted_values = [format_scalar(v) for v in flat_matrix]
        f.write(','.join(formatted_values) + '\n')
    print(f"  {filename} (3x3 matrix, 9 values)")

except Exception as e:
    print(f"  Warning: Could not generate ODT matrix: {e}", file=sys.stderr)

try:
    # Chromatic Adaptation Transform (CAT) matrices
    print("\nGenerating CAT matrices...")

    # Bradford CAT matrix
    bradford_matrix = colour.CHROMATIC_ADAPTATION_TRANSFORMS['Bradford']
    filename = 'data/matrices/cat_bradford.csv'
    with open(filename, 'w', newline='') as f:
        flat_matrix = bradford_matrix.flatten()
        formatted_values = [format_scalar(v) for v in flat_matrix]
        f.write(','.join(formatted_values) + '\n')
    print(f"  {filename} (3x3 matrix, 9 values)")

    # CAT02 matrix
    cat02_matrix = colour.CHROMATIC_ADAPTATION_TRANSFORMS['CAT02']
    filename = 'data/matrices/cat_cat02.csv'
    with open(filename, 'w', newline='') as f:
        flat_matrix = cat02_matrix.flatten()
        formatted_values = [format_scalar(v) for v in flat_matrix]
        f.write(','.join(formatted_values) + '\n')
    print(f"  {filename} (3x3 matrix, 9 values)")

    # CAT16 matrix
    cat16_matrix = colour.CHROMATIC_ADAPTATION_TRANSFORMS['CAT16']
    filename = 'data/matrices/cat_cat16.csv'
    with open(filename, 'w', newline='') as f:
        flat_matrix = cat16_matrix.flatten()
        formatted_values = [format_scalar(v) for v in flat_matrix]
        f.write(','.join(formatted_values) + '\n')
    print(f"  {filename} (3x3 matrix, 9 values)")

except Exception as e:
    print(f"  Warning: Could not generate CAT matrices: {e}", file=sys.stderr)

print("\nData generation complete!")
"@

# Write Python script to temp file
$tempScript = [System.IO.Path]::GetTempFileName() + ".py"
Set-Content -Path $tempScript -Value $pythonScript -Encoding UTF8

try {
    # Run Python script
    python $tempScript

    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "ERROR: Data generation failed" -ForegroundColor Red
        exit 1
    }

    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Data generation completed successfully!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green

} finally {
    # Clean up temp file
    if (Test-Path $tempScript) {
        Remove-Item $tempScript -Force
    }
}
