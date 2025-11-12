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
with open('data/fixtures/srgb_descriptor.csv', 'w', newline='') as f:
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
    with open('data/fixtures/srgb_rgb_to_xyz.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in rgb_to_xyz.flatten()]
        f.write(','.join(values) + '\n')
    print(f"  data/fixtures/srgb_rgb_to_xyz.csv")
except Exception as e:
    print(f"  Warning: Could not generate sRGB matrix: {e}", file=sys.stderr)

# Test fixture: BT.2020 descriptor
bt2020 = colour.RGB_COLOURSPACES['ITU-R BT.2020']
with open('data/fixtures/bt2020_descriptor.csv', 'w', newline='') as f:
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
with open('data/fixtures/aces_ap0_descriptor.csv', 'w', newline='') as f:
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
with open('data/fixtures/aces_ap1_descriptor.csv', 'w', newline='') as f:
    values = [
        format_scalar(aces_ap1.primaries[0][0]), format_scalar(aces_ap1.primaries[0][1]),
        format_scalar(aces_ap1.primaries[1][0]), format_scalar(aces_ap1.primaries[1][1]),
        format_scalar(aces_ap1.primaries[2][0]), format_scalar(aces_ap1.primaries[2][1]),
        format_scalar(aces_ap1.whitepoint[0]), format_scalar(aces_ap1.whitepoint[1])
    ]
    f.write(','.join(values) + '\n')
print(f"  data/fixtures/aces_ap1_descriptor.csv")

# ================================================================
# M2 Test Fixtures: Color Space Conversions
# ================================================================
print("\nGenerating M2 test fixtures (color space conversions)...")

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
with open('data/fixtures/m2_xyz_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_xyz_values.csv")

# Compute corresponding xyY values and flatten to single line
with open('data/fixtures/m2_xyy_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        xyy = colour.XYZ_to_xyY(xyz)
        all_values.extend([format_scalar(v) for v in xyy])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_xyy_values.csv")

# Test case 2: XYZ ↔ Lab (D65 white point)
with open('data/fixtures/m2_lab_d65_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        lab = colour.XYZ_to_Lab(xyz, illuminant=d65)
        all_values.extend([format_scalar(v) for v in lab])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_lab_d65_values.csv")

# Test case 3: XYZ ↔ Lab (D50 white point)
with open('data/fixtures/m2_lab_d50_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        lab = colour.XYZ_to_Lab(xyz, illuminant=d50)
        all_values.extend([format_scalar(v) for v in lab])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_lab_d50_values.csv")

# Test case 4: XYZ ↔ Luv (D65 white point)
with open('data/fixtures/m2_luv_d65_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        luv = colour.XYZ_to_Luv(xyz, illuminant=d65)
        all_values.extend([format_scalar(v) for v in luv])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_luv_d65_values.csv")

# Test case 5: Lab ↔ LCh
test_lab_values = [
    [50.0, 25.0, 25.0],
    [75.0, -10.0, 50.0],
    [25.0, 0.0, 0.0],
]

with open('data/fixtures/m2_lab_for_lch.csv', 'w', newline='') as f:
    all_values = []
    for lab in test_lab_values:
        all_values.extend([format_scalar(v) for v in lab])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_lab_for_lch.csv")

with open('data/fixtures/m2_lch_values.csv', 'w', newline='') as f:
    all_values = []
    for lab in test_lab_values:
        lch = colour.Lab_to_LCHab(lab)
        all_values.extend([format_scalar(v) for v in lch])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_lch_values.csv")

# Test case 6: Luv ↔ LCh(uv)
test_luv_values = [
    [50.0, 20.0, 30.0],
    [75.0, -15.0, 45.0],
    [25.0, 0.0, 0.0],
]

with open('data/fixtures/m2_luv_for_lchuv.csv', 'w', newline='') as f:
    all_values = []
    for luv in test_luv_values:
        all_values.extend([format_scalar(v) for v in luv])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_luv_for_lchuv.csv")

with open('data/fixtures/m2_lchuv_values.csv', 'w', newline='') as f:
    all_values = []
    for luv in test_luv_values:
        lchuv = colour.Luv_to_LCHuv(luv)
        all_values.extend([format_scalar(v) for v in lchuv])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_lchuv_values.csv")

# ================================================================
# M2 Test Fixtures: ΔE Metrics
# ================================================================
print("\nGenerating M2 test fixtures (ΔE metrics)...")

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
with open('data/fixtures/m2_delta_e_lab1.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        all_values.extend([format_scalar(v) for v in lab1])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_delta_e_lab1.csv")

with open('data/fixtures/m2_delta_e_lab2.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        all_values.extend([format_scalar(v) for v in lab2])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_delta_e_lab2.csv")

# Compute ΔE76 (flatten to single line)
with open('data/fixtures/m2_delta_e_76.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        # ΔE76 is Euclidean distance in Lab space
        dL = lab2[0] - lab1[0]
        da = lab2[1] - lab1[1]
        db = lab2[2] - lab1[2]
        de76 = np.sqrt(dL*dL + da*da + db*db)
        all_values.append(format_scalar(de76))
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_delta_e_76.csv")

# Compute ΔE94 (flatten to single line)
with open('data/fixtures/m2_delta_e_94.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        de94 = colour.difference.delta_E_CIE1994(np.array(lab1), np.array(lab2))
        all_values.append(format_scalar(de94))
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_delta_e_94.csv")

# Compute ΔE CMC(2:1) - acceptability (flatten to single line)
with open('data/fixtures/m2_delta_e_cmc.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        de_cmc = colour.difference.delta_E_CMC(np.array(lab1), np.array(lab2), l=2, c=1)
        all_values.append(format_scalar(de_cmc))
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_delta_e_cmc.csv")

# Compute ΔE2000 (flatten to single line)
with open('data/fixtures/m2_delta_e_2000.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        de2000 = colour.difference.delta_E_CIE2000(np.array(lab1), np.array(lab2))
        all_values.append(format_scalar(de2000))
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m2_delta_e_2000.csv")

# Store D65 and D50 white points in XYZ for tests
with open('data/fixtures/m2_d65_xyz.csv', 'w', newline='') as f:
    f.write(','.join([format_scalar(v) for v in d65_xyz]) + '\n')
print(f"  data/fixtures/m2_d65_xyz.csv")

with open('data/fixtures/m2_d50_xyz.csv', 'w', newline='') as f:
    f.write(','.join([format_scalar(v) for v in d50_xyz]) + '\n')
print(f"  data/fixtures/m2_d50_xyz.csv")

# ================================================================
# M3 Test Fixtures: Chromatic Adaptation Transform (CAT)
# ================================================================
print("\nGenerating M3 test fixtures (chromatic adaptation)...")

# White points in XYZ (normalized to Y=1)
d60_xyz = colour.xy_to_XYZ(d60)
d55_xyz = colour.xy_to_XYZ(d55)
a_xyz = colour.xy_to_XYZ(a)
e_xyz = colour.xy_to_XYZ(e)

# Store white points for tests
white_points = {
    'd65': d65_xyz,
    'd60': d60_xyz,
    'd55': d55_xyz,
    'd50': d50_xyz,
    'a': a_xyz,
    'e': e_xyz,
}

for name, wp_xyz in white_points.items():
    with open(f'data/fixtures/m3_{name}_xyz.csv', 'w', newline='') as f:
        f.write(','.join([format_scalar(v) for v in wp_xyz]) + '\n')
    print(f"  data/fixtures/m3_{name}_xyz.csv")

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

for src_name, dst_name, method in cat_test_cases:
    src_wp = white_points[src_name]
    dst_wp = white_points[dst_name]

    # Compute CAT matrix
    cat_matrix = colour.adaptation.matrix_chromatic_adaptation_VonKries(
        src_wp, dst_wp, transform=method
    )

    # Flatten matrix to single line (row-major order)
    filename = f'm3_cat_{src_name}_to_{dst_name}_{method.lower().replace(" ", "_")}.csv'
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
with open('data/fixtures/m3_test_xyz_colors.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_colors:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m3_test_xyz_colors.csv")

# Adaptation test case: D65 → D50 (Bradford)
adapted_colors = []
for xyz in test_xyz_colors:
    adapted = colour.adaptation.chromatic_adaptation_VonKries(
        xyz, d65_xyz, d50_xyz, transform='Bradford'
    )
    adapted_colors.append(adapted)

with open('data/fixtures/m3_adapted_d65_to_d50_bradford.csv', 'w', newline='') as f:
    all_values = []
    for xyz in adapted_colors:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m3_adapted_d65_to_d50_bradford.csv")

# Adaptation test case: A → D65 (Bradford)
adapted_colors_a = []
for xyz in test_xyz_colors:
    adapted = colour.adaptation.chromatic_adaptation_VonKries(
        xyz, a_xyz, d65_xyz, transform='Bradford'
    )
    adapted_colors_a.append(adapted)

with open('data/fixtures/m3_adapted_a_to_d65_bradford.csv', 'w', newline='') as f:
    all_values = []
    for xyz in adapted_colors_a:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  data/fixtures/m3_adapted_a_to_d65_bradford.csv")

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
