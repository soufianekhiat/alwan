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
