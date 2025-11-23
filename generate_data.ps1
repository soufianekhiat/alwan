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
$dataDir = "src/alwan/data"
$subdirs = @("cmf", "illuminants", "matrices", "rgb_spaces", "fixtures")

# Create base data directory
if (-not (Test-Path $dataDir)) {
    Write-Host "  Creating directory: $dataDir" -ForegroundColor Gray
    New-Item -ItemType Directory -Path $dataDir -Force | Out-Null
}

# Create all subdirectories
foreach ($subdir in $subdirs) {
    $path = "$dataDir/$subdir"
    if (-not (Test-Path $path)) {
        Write-Host "  Creating directory: $path" -ForegroundColor Gray
        New-Item -ItemType Directory -Path $path -Force | Out-Null
    }
}

Write-Host ""
Write-Host "Generating data files..." -ForegroundColor Yellow

# Create Python script for data generation
$pythonScript = @"
import warnings
warnings.filterwarnings('ignore')  # Suppress matplotlib and other warnings

import colour
import sys
import os

# Base data directory
DATA_DIR = 'src/alwan/data'

def ensure_dir(filepath):
    """Ensure the directory for the given filepath exists"""
    directory = os.path.dirname(filepath)
    if directory and not os.path.exists(directory):
        os.makedirs(directory, exist_ok=True)

def format_scalar(value):
    """Format scalar with maximum precision for C parsing"""
    return f"{value:.17g}"

def write_xy(filepath, x, y):
    """Write xy chromaticity coordinates to CSV"""
    ensure_dir(filepath)
    with open(filepath, 'w', newline='') as f:
        f.write(f"{format_scalar(x)},{format_scalar(y)}\n")

def write_primaries_3x2(filepath, primaries):
    """Write RGB primaries as 3x2 matrix (rx,ry,gx,gy,bx,by)"""
    ensure_dir(filepath)
    with open(filepath, 'w', newline='') as f:
        values = [
            format_scalar(primaries[0][0]), format_scalar(primaries[0][1]),
            format_scalar(primaries[1][0]), format_scalar(primaries[1][1]),
            format_scalar(primaries[2][0]), format_scalar(primaries[2][1])
        ]
        f.write(','.join(values) + '\n')

def write_rgb_space_csv(filepath, name, space):
    """Write RGB space definition to CSV (numeric only for C parsing)"""
    ensure_dir(filepath)
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

# Create illuminants_xy subdirectory
ensure_dir(f'{DATA_DIR}/illuminants_xy')

# D65 (2° observer)
d65 = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']
write_xy(f'{DATA_DIR}/illuminants_xy/d65_xy.csv', d65[0], d65[1])
print(f"  {DATA_DIR}/illuminants_xy/d65_xy.csv: {format_scalar(d65[0])}, {format_scalar(d65[1])}")

# D60 (2° observer) - used by ACES
d60 = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D60']
write_xy(f'{DATA_DIR}/illuminants_xy/d60_xy.csv', d60[0], d60[1])
print(f"  {DATA_DIR}/illuminants_xy/d60_xy.csv: {format_scalar(d60[0])}, {format_scalar(d60[1])}")

# D50 (2° observer) - ICC profile connection space
d50 = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D50']
write_xy(f'{DATA_DIR}/illuminants_xy/d50_xy.csv', d50[0], d50[1])
print(f"  {DATA_DIR}/illuminants_xy/d50_xy.csv: {format_scalar(d50[0])}, {format_scalar(d50[1])}")

# D55 (2° observer)
d55 = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D55']
write_xy(f'{DATA_DIR}/illuminants_xy/d55_xy.csv', d55[0], d55[1])
print(f"  {DATA_DIR}/illuminants_xy/d55_xy.csv: {format_scalar(d55[0])}, {format_scalar(d55[1])}")

# Illuminant A (2° observer) - incandescent
a = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['A']
write_xy(f'{DATA_DIR}/illuminants_xy/a_xy.csv', a[0], a[1])
print(f"  {DATA_DIR}/illuminants_xy/a_xy.csv: {format_scalar(a[0])}, {format_scalar(a[1])}")

# Illuminant E (2° observer) - equal energy
e = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['E']
write_xy(f'{DATA_DIR}/illuminants_xy/e_xy.csv', e[0], e[1])
print(f"  {DATA_DIR}/illuminants_xy/e_xy.csv: {format_scalar(e[0])}, {format_scalar(e[1])}")

# Fluorescent illuminants F1-F12 (2° observer)
f_illuminants = ['F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', 'F8', 'F9', 'F10', 'F11', 'F12']
for f_name in f_illuminants:
    try:
        f_ill = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer'][f_name]
        write_xy(f'{DATA_DIR}/illuminants_xy/{f_name.lower()}_xy.csv', f_ill[0], f_ill[1])
        print(f"  {DATA_DIR}/illuminants_xy/{f_name.lower()}_xy.csv: {format_scalar(f_ill[0])}, {format_scalar(f_ill[1])}")
    except KeyError:
        print(f"  Warning: Illuminant {f_name} not found in colour-science library")

# Generate RGB space primaries
print("\nGenerating RGB space primaries...")

# sRGB primaries (legacy format)
srgb = colour.RGB_COLOURSPACES['sRGB']
write_primaries_3x2(f'{DATA_DIR}/srgb_primaries_3x2.csv', srgb.primaries)
print(f"  {DATA_DIR}/srgb_primaries_3x2.csv")

# RGB color spaces with full definitions
rgb_spaces = [
    # Core spaces (M1)
    ('sRGB', 'sRGB'),
    ('BT.709', 'ITU-R BT.709'),
    ('Display P3', 'Display P3'),
    ('BT.2020', 'ITU-R BT.2020'),
    ('ACES2065-1', 'ACES2065-1'),
    ('ACEScg', 'ACEScg'),
    ('ACESproxy', 'ACESproxy'),

    # ACES Family extensions
    ('ACEScc', 'ACEScc'),
    ('ACEScct', 'ACEScct'),

    # ARRI Camera Spaces
    ('ARRI Wide Gamut 3', 'ARRI Wide Gamut 3'),
    ('ARRI Wide Gamut 4', 'ARRI Wide Gamut 4'),
    ('ARRI LogC3', 'ARRI Wide Gamut 3'),  # Same primaries as WG3
    ('ARRI LogC4', 'ARRI Wide Gamut 4'),  # Same primaries as WG4

    # RED Camera Spaces (extended)
    ('REDcolor', 'REDcolor'),
    ('REDcolor2', 'REDcolor2'),
    ('REDcolor3', 'REDcolor3'),
    ('REDcolor4', 'REDcolor4'),
    ('DRAGONcolor', 'DRAGONcolor'),
    ('DRAGONcolor2', 'DRAGONcolor2'),
    ('REDLog', 'REDWideGamutRGB'),  # Same primaries as REDWideGamutRGB

    # Sony Camera Spaces (extended)
    ('Venice S-Gamut3', 'Venice S-Gamut3'),
    ('Venice S-Gamut3.Cine', 'Venice S-Gamut3.Cine'),
    ('S-Log', 'S-Gamut3'),  # Same primaries as S-Gamut3
    ('S-Log2', 'S-Gamut3'),  # Same primaries as S-Gamut3
    ('S-Log3', 'S-Gamut3'),  # Same primaries as S-Gamut3

    # Historical/Reference
    ('CIE RGB', 'CIE RGB'),

    # Professional/Photography (extended)
    ('Adobe Wide Gamut RGB', 'Adobe Wide Gamut RGB'),
    ('ROMM RGB', 'ROMM RGB'),
    ('RIMM RGB', 'RIMM RGB'),
    ('ERIMM RGB', 'ERIMM RGB'),

    # DaVinci/FilmLight
    ('FilmLight E-Gamut', 'FilmLight E-Gamut'),
    ('FilmLight T-Log', 'FilmLight E-Gamut'),  # Same primaries as E-Gamut

    # ========== MEDIUM PRIORITY NEW SPACES ==========

    # Fujifilm Camera Spaces
    ('F-Gamut', 'F-Gamut'),
    ('Fujifilm F-Log', 'F-Gamut'),  # Same primaries as F-Gamut

    # Nikon Camera Spaces
    ('N-Gamut', 'N-Gamut'),
    ('N-Log', 'N-Gamut'),  # Same primaries as N-Gamut

    # DJI Camera Spaces
    ('DJI D-Gamut', 'DJI D-Gamut'),

    # GoPro Camera Spaces
    ('Protune Native', 'Protune Native'),

    # Legacy Broadcast (extended)
    ('ITU-R BT.470 - 525', 'ITU-R BT.470 - 525'),
    ('ITU-R BT.470 - 625', 'ITU-R BT.470 - 625'),
    ('SMPTE 240M', 'SMPTE 240M'),
    ('SMPTE C', 'SMPTE C'),

    # Digital Cinema & Mastering
    ('DCDM XYZ', 'DCDM XYZ'),

    # ========== LOWER PRIORITY NEW SPACES ==========

    # Print/Specialized Spaces
    ('Best RGB', 'Best RGB'),
    ('Beta RGB', 'Beta RGB'),
    ('Don RGB 4', 'Don RGB 4'),
    ('Ekta Space PS 5', 'Ekta Space PS 5'),
    ('Max RGB', 'Max RGB'),
    ('Russell RGB', 'Russell RGB'),

    # Historical/Reference (additional)
    ('Sharp RGB', 'Sharp RGB'),
    ('ECI RGB v2', 'ECI RGB v2'),

    # ========== EXISTING SPACES (kept for compatibility) ==========

    # Adobe RGB (1998) - Photography/print workflow
    ('Adobe RGB 1998', 'Adobe RGB (1998)'),

    # ProPhoto RGB / ROMM RGB - Wide gamut professional
    ('ProPhoto RGB', 'ProPhoto RGB'),

    # Cinema/Broadcast spaces - Professional video production
    ('DaVinci Wide Gamut', 'DaVinci Wide Gamut'),
    ('DaVinci Intermediate', 'DaVinci Wide Gamut'),  # Same primaries as DaVinci WG
    ('Blackmagic Wide Gamut', 'Blackmagic Wide Gamut'),
    ('Blackmagic Film', 'Blackmagic Wide Gamut'),  # Same primaries as Blackmagic WG
    ('Blackmagic Film Gen5', 'Blackmagic Wide Gamut'),  # Same primaries as Blackmagic WG
    ('V-Gamut', 'V-Gamut'),
    ('V-Log', 'V-Gamut'),  # Same primaries as V-Gamut
    ('S-Gamut', 'S-Gamut'),
    ('S-Gamut3', 'S-Gamut3'),
    ('S-Gamut3.Cine', 'S-Gamut3.Cine'),
    ('Cinema Gamut', 'Cinema Gamut'),
    ('Canon Log', 'Cinema Gamut'),  # Same primaries as Cinema Gamut
    ('REDWideGamutRGB', 'REDWideGamutRGB'),
    ('DCI-P3', 'DCI-P3'),
    ('DCI-P3-P', 'DCI-P3-P'),  # DCI-P3+ (extended primaries)
    ('P3-D65', 'P3-D65'),

    # Legacy spaces - Historical compatibility
    ('NTSC 1953', 'NTSC (1953)'),
    ('NTSC 1987', 'NTSC (1987)'),
    ('PAL SECAM', 'PAL/SECAM'),
    ('EBU Tech 3213-E', 'EBU Tech. 3213-E'),
    ('Apple RGB', 'Apple RGB'),
    ('ColorMatch RGB', 'ColorMatch RGB'),

    # Additional RGB spaces
    ('ALEXA Wide Gamut', 'ARRI Wide Gamut 3'),
    ('P3-D60', 'P3-D60'),  # Custom: P3 primaries with D60 white point
    ('Xtreme RGB', 'Xtreme RGB'),
    ('Linear sRGB', 'sRGB'),  # Same primaries as sRGB, but linear
    ('Linear Rec.2020', 'ITU-R BT.2020'),  # Same primaries as BT.2020, but linear
    ('Linear Adobe RGB 1998', 'Adobe RGB (1998)'),  # Same primaries as Adobe RGB, but linear
    ('Linear P3_D65', 'P3-D65'),  # Same primaries as P3-D65, but linear
    ('Linear Display P3', 'Display P3'),  # Same primaries as Display P3, but linear
    ('Linear ProPhoto RGB', 'ProPhoto RGB'),  # Same primaries as ProPhoto RGB, but linear
    ('Linear DCI_P3', 'DCI-P3'),  # Same primaries as DCI-P3, but linear
    ('Linear Adobe Wide Gamut RGB', 'Adobe Wide Gamut RGB'),  # Same primaries as Adobe WG, but linear
    ('Linear Apple RGB', 'Apple RGB'),  # Same primaries as Apple RGB, but linear
    ('Linear ColorMatch RGB', 'ColorMatch RGB'),  # Same primaries as ColorMatch RGB, but linear
    ('Linear P3_D60', 'P3-D60'),  # Same primaries as P3-D60, but linear
    ('Linear BT470_525', 'ITU-R BT.470 - 525'),  # Same primaries as BT.470-525, but linear
    ('Linear BT470_625', 'ITU-R BT.470 - 625'),  # Same primaries as BT.470-625, but linear
    ('Linear SMPTE 240M', 'SMPTE 240M'),  # Same primaries as SMPTE 240M, but linear

    # Specialized/Standard spaces
    ('ITU-T H273 22 Unspecified', 'ITU-T H.273 - 22 Unspecified'),
    ('ITU-T H273 Generic Film', 'ITU-T H.273 - Generic Film'),
    ('PLASA ANSI E154', 'PLASA ANSI E1.54'),

    # Gamma-encoded variants (same primaries as parent, but with simple gamma transfer function)
    ('Gamma 2.2 Rec.709', 'ITU-R BT.709'),  # Same primaries as BT.709, but with gamma 2.2
    ('Gamma 2.2 Adobe RGB', 'Adobe RGB (1998)'),  # Same primaries as Adobe RGB, but with gamma 2.2
    ('Gamma 2.2 P3-D65', 'P3-D65'),  # Same primaries as P3-D65, but with gamma 2.2
    ('Gamma 2.2 AP1', 'ACEScg'),  # Same primaries as ACEScg (AP1), but with gamma 2.2
    ('Gamma 1.8 Rec.709', 'ITU-R BT.709'),  # Same primaries as BT.709, but with gamma 1.8
]

print("\nGenerating RGB space definitions...")

# Create custom P3-D60 colorspace (P3 primaries with D60 white point)
import numpy as np
p3_primaries = np.array([[0.68, 0.32], [0.265, 0.69], [0.15, 0.06]])
d60_whitepoint = colour.CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D60']
p3_d60_space = colour.RGB_Colourspace('P3-D60', p3_primaries, d60_whitepoint)

for filename, space_name in rgb_spaces:
    try:
        # Handle special case for P3-D60
        if space_name == 'P3-D60':
            space = p3_d60_space
        else:
            space = colour.RGB_COLOURSPACES[space_name]

        filepath = f'{DATA_DIR}/rgb_spaces/{filename.lower().replace(" ", "_").replace(".", "")}.csv'
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
os.makedirs(f'{DATA_DIR}/fixtures', exist_ok=True)

# Test fixture: sRGB descriptor (primaries + white point)
srgb = colour.RGB_COLOURSPACES['sRGB']
ensure_dir('src/alwan/data/fixtures/srgb_descriptor.csv')
with open('src/alwan/data/fixtures/srgb_descriptor.csv', 'w', newline='') as f:
    values = [
        format_scalar(srgb.primaries[0][0]), format_scalar(srgb.primaries[0][1]),
        format_scalar(srgb.primaries[1][0]), format_scalar(srgb.primaries[1][1]),
        format_scalar(srgb.primaries[2][0]), format_scalar(srgb.primaries[2][1]),
        format_scalar(srgb.whitepoint[0]), format_scalar(srgb.whitepoint[1])
    ]
    f.write(','.join(values) + '\n')
print(f"  {DATA_DIR}/fixtures/srgb_descriptor.csv")

# Test fixture: sRGB RGB->XYZ matrix (reference from colour-science)
try:
    import numpy as np
    rgb_to_xyz = srgb.matrix_RGB_to_XYZ
    ensure_dir('src/alwan/data/fixtures/srgb_rgb_to_xyz.csv')
    with open('src/alwan/data/fixtures/srgb_rgb_to_xyz.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in rgb_to_xyz.flatten()]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/fixtures/srgb_rgb_to_xyz.csv")
except Exception as e:
    print(f"  Warning: Could not generate sRGB matrix: {e}", file=sys.stderr)

# Test fixture: BT.2020 descriptor
bt2020 = colour.RGB_COLOURSPACES['ITU-R BT.2020']
ensure_dir('src/alwan/data/fixtures/bt2020_descriptor.csv')
with open('src/alwan/data/fixtures/bt2020_descriptor.csv', 'w', newline='') as f:
    values = [
        format_scalar(bt2020.primaries[0][0]), format_scalar(bt2020.primaries[0][1]),
        format_scalar(bt2020.primaries[1][0]), format_scalar(bt2020.primaries[1][1]),
        format_scalar(bt2020.primaries[2][0]), format_scalar(bt2020.primaries[2][1]),
        format_scalar(bt2020.whitepoint[0]), format_scalar(bt2020.whitepoint[1])
    ]
    f.write(','.join(values) + '\n')
print(f"  {DATA_DIR}/fixtures/bt2020_descriptor.csv")

# Test fixture: ACES AP0 descriptor
aces_ap0 = colour.RGB_COLOURSPACES['ACES2065-1']
ensure_dir('src/alwan/data/fixtures/aces_ap0_descriptor.csv')
with open('src/alwan/data/fixtures/aces_ap0_descriptor.csv', 'w', newline='') as f:
    values = [
        format_scalar(aces_ap0.primaries[0][0]), format_scalar(aces_ap0.primaries[0][1]),
        format_scalar(aces_ap0.primaries[1][0]), format_scalar(aces_ap0.primaries[1][1]),
        format_scalar(aces_ap0.primaries[2][0]), format_scalar(aces_ap0.primaries[2][1]),
        format_scalar(aces_ap0.whitepoint[0]), format_scalar(aces_ap0.whitepoint[1])
    ]
    f.write(','.join(values) + '\n')
print(f"  {DATA_DIR}/fixtures/aces_ap0_descriptor.csv")

# Test fixture: ACES AP1 descriptor
aces_ap1 = colour.RGB_COLOURSPACES['ACEScg']
ensure_dir('src/alwan/data/fixtures/aces_ap1_descriptor.csv')
with open('src/alwan/data/fixtures/aces_ap1_descriptor.csv', 'w', newline='') as f:
    values = [
        format_scalar(aces_ap1.primaries[0][0]), format_scalar(aces_ap1.primaries[0][1]),
        format_scalar(aces_ap1.primaries[1][0]), format_scalar(aces_ap1.primaries[1][1]),
        format_scalar(aces_ap1.primaries[2][0]), format_scalar(aces_ap1.primaries[2][1]),
        format_scalar(aces_ap1.whitepoint[0]), format_scalar(aces_ap1.whitepoint[1])
    ]
    f.write(','.join(values) + '\n')
print(f"  {DATA_DIR}/fixtures/aces_ap1_descriptor.csv")

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
ensure_dir('src/alwan/data/fixtures/xyz_values.csv')
with open('src/alwan/data/fixtures/xyz_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/xyz_values.csv")

# Compute corresponding xyY values and flatten to single line
ensure_dir('src/alwan/data/fixtures/xyy_values.csv')
with open('src/alwan/data/fixtures/xyy_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        xyy = colour.XYZ_to_xyY(xyz)
        all_values.extend([format_scalar(v) for v in xyy])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/xyy_values.csv")

# Test case 2: XYZ ↔ Lab (D65 white point)
ensure_dir('src/alwan/data/fixtures/lab_d65_values.csv')
with open('src/alwan/data/fixtures/lab_d65_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        lab = colour.XYZ_to_Lab(xyz, illuminant=d65)
        all_values.extend([format_scalar(v) for v in lab])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/lab_d65_values.csv")

# Test case 3: XYZ ↔ Lab (D50 white point)
ensure_dir('src/alwan/data/fixtures/lab_d50_values.csv')
with open('src/alwan/data/fixtures/lab_d50_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        lab = colour.XYZ_to_Lab(xyz, illuminant=d50)
        all_values.extend([format_scalar(v) for v in lab])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/lab_d50_values.csv")

# Test case 4: XYZ ↔ Luv (D65 white point)
ensure_dir('src/alwan/data/fixtures/luv_d65_values.csv')
with open('src/alwan/data/fixtures/luv_d65_values.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_values:
        luv = colour.XYZ_to_Luv(xyz, illuminant=d65)
        all_values.extend([format_scalar(v) for v in luv])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/luv_d65_values.csv")

# Test case 5: Lab ↔ LCh
test_lab_values = [
    [50.0, 25.0, 25.0],
    [75.0, -10.0, 50.0],
    [25.0, 0.0, 0.0],
]

ensure_dir('src/alwan/data/fixtures/lab_for_lch.csv')
with open('src/alwan/data/fixtures/lab_for_lch.csv', 'w', newline='') as f:
    all_values = []
    for lab in test_lab_values:
        all_values.extend([format_scalar(v) for v in lab])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/lab_for_lch.csv")

ensure_dir('src/alwan/data/fixtures/lch_values.csv')
with open('src/alwan/data/fixtures/lch_values.csv', 'w', newline='') as f:
    all_values = []
    for lab in test_lab_values:
        lch = colour.Lab_to_LCHab(lab)
        all_values.extend([format_scalar(v) for v in lch])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/lch_values.csv")

# Test case 6: Luv ↔ LCh(uv)
test_luv_values = [
    [50.0, 20.0, 30.0],
    [75.0, -15.0, 45.0],
    [25.0, 0.0, 0.0],
]

ensure_dir('src/alwan/data/fixtures/luv_for_lchuv.csv')
with open('src/alwan/data/fixtures/luv_for_lchuv.csv', 'w', newline='') as f:
    all_values = []
    for luv in test_luv_values:
        all_values.extend([format_scalar(v) for v in luv])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/luv_for_lchuv.csv")

ensure_dir('src/alwan/data/fixtures/lchuv_values.csv')
with open('src/alwan/data/fixtures/lchuv_values.csv', 'w', newline='') as f:
    all_values = []
    for luv in test_luv_values:
        lchuv = colour.Luv_to_LCHuv(luv)
        all_values.extend([format_scalar(v) for v in lchuv])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/lchuv_values.csv")

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
ensure_dir('src/alwan/data/fixtures/delta_e_lab1.csv')
with open('src/alwan/data/fixtures/delta_e_lab1.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        all_values.extend([format_scalar(v) for v in lab1])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/delta_e_lab1.csv")

ensure_dir('src/alwan/data/fixtures/delta_e_lab2.csv')
with open('src/alwan/data/fixtures/delta_e_lab2.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        all_values.extend([format_scalar(v) for v in lab2])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/delta_e_lab2.csv")

# Compute ΔE76 (flatten to single line)
ensure_dir('src/alwan/data/fixtures/delta_e_76.csv')
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
print(f"  {DATA_DIR}/fixtures/delta_e_76.csv")

# Compute ΔE94 (flatten to single line)
ensure_dir('src/alwan/data/fixtures/delta_e_94.csv')
with open('src/alwan/data/fixtures/delta_e_94.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        de94 = colour.difference.delta_E_CIE1994(np.array(lab1), np.array(lab2))
        all_values.append(format_scalar(de94))
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/delta_e_94.csv")

# Compute ΔE CMC(2:1) - acceptability (flatten to single line)
ensure_dir('src/alwan/data/fixtures/delta_e_cmc.csv')
with open('src/alwan/data/fixtures/delta_e_cmc.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        de_cmc = colour.difference.delta_E_CMC(np.array(lab1), np.array(lab2), l=2, c=1)
        all_values.append(format_scalar(de_cmc))
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/delta_e_cmc.csv")

# Compute ΔE2000 (flatten to single line)
ensure_dir('src/alwan/data/fixtures/delta_e_2000.csv')
with open('src/alwan/data/fixtures/delta_e_2000.csv', 'w', newline='') as f:
    all_values = []
    for lab1, lab2 in delta_e_test_pairs:
        de2000 = colour.difference.delta_E_CIE2000(np.array(lab1), np.array(lab2))
        all_values.append(format_scalar(de2000))
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/delta_e_2000.csv")

# Store D65 and D50 white points in XYZ for tests
ensure_dir('src/alwan/data/fixtures/d65_xyz.csv')
with open('src/alwan/data/fixtures/d65_xyz.csv', 'w', newline='') as f:
    f.write(','.join([format_scalar(v) for v in d65_xyz]) + '\n')
print(f"  {DATA_DIR}/fixtures/d65_xyz.csv")

ensure_dir('src/alwan/data/fixtures/d50_xyz.csv')
with open('src/alwan/data/fixtures/d50_xyz.csv', 'w', newline='') as f:
    f.write(','.join([format_scalar(v) for v in d50_xyz]) + '\n')
print(f"  {DATA_DIR}/fixtures/d50_xyz.csv")

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
    ensure_dir(f'{DATA_DIR}/fixtures/{name}_xyz.csv')
    with open(f'{DATA_DIR}/fixtures/{name}_xyz.csv', 'w', newline='') as f:
        f.write(','.join([format_scalar(v) for v in wp_xyz]) + '\n')
    print(f"  {DATA_DIR}/fixtures/{name}_xyz.csv")

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
    ensure_dir(f'{DATA_DIR}/fixtures/{filename}')
    with open(f'{DATA_DIR}/fixtures/{filename}', 'w', newline='') as f:
        values = [format_scalar(v) for v in cat_matrix.flatten()]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/fixtures/{filename}")

# Test XYZ colors for adaptation
test_xyz_colors = [
    [0.95047, 1.0, 1.08883],     # D65 white
    [0.5, 0.5, 0.5],             # Mid-gray
    [0.412456, 0.212673, 0.019334],  # sRGB red (D65)
    [0.357576, 0.715152, 0.119192],  # sRGB green (D65)
    [0.180437, 0.072175, 0.950304],  # sRGB blue (D65)
]

# Store test colors
ensure_dir('src/alwan/data/fixtures/test_xyz_colors.csv')
with open('src/alwan/data/fixtures/test_xyz_colors.csv', 'w', newline='') as f:
    all_values = []
    for xyz in test_xyz_colors:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/test_xyz_colors.csv")

# Adaptation test case: D65 → D50 (Bradford)
adapted_colors = []
for xyz in test_xyz_colors:
    adapted = colour.adaptation.chromatic_adaptation_VonKries(
        xyz, d65_xyz, d50_xyz, transform='Bradford'
    )
    adapted_colors.append(adapted)

ensure_dir('src/alwan/data/fixtures/adapted_d65_to_d50_bradford.csv')
with open('src/alwan/data/fixtures/adapted_d65_to_d50_bradford.csv', 'w', newline='') as f:
    all_values = []
    for xyz in adapted_colors:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/adapted_d65_to_d50_bradford.csv")

# Adaptation test case: A → D65 (Bradford)
adapted_colors_a = []
for xyz in test_xyz_colors:
    adapted = colour.adaptation.chromatic_adaptation_VonKries(
        xyz, a_xyz, d65_xyz, transform='Bradford'
    )
    adapted_colors_a.append(adapted)

ensure_dir('src/alwan/data/fixtures/adapted_a_to_d65_bradford.csv')
with open('src/alwan/data/fixtures/adapted_a_to_d65_bradford.csv', 'w', newline='') as f:
    all_values = []
    for xyz in adapted_colors_a:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/adapted_a_to_d65_bradford.csv")

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
ensure_dir('src/alwan/data/fixtures/cam_viewing_conditions.csv')
with open('src/alwan/data/fixtures/cam_viewing_conditions.csv', 'w', newline='') as f:
    # XYZ_w (3 values), L_A (1 value), Y_b (1 value)
    values = [format_scalar(v) for v in XYZ_w]
    values.extend([format_scalar(L_A), format_scalar(Y_b)])
    f.write(','.join(values) + '\n')
print(f"  {DATA_DIR}/fixtures/cam_viewing_conditions.csv (XYZ_w, L_A, Y_b)")

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
ensure_dir('src/alwan/data/fixtures/ciecam02_xyz_input.csv')
with open('src/alwan/data/fixtures/ciecam02_xyz_input.csv', 'w', newline='') as f:
    all_values = []
    for xyz in ciecam02_test_xyz:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/ciecam02_xyz_input.csv ({len(ciecam02_test_xyz)} colors)")

# Write CIECAM02 correlates (J, C, h, Q, M, s, H for each color)
ensure_dir('src/alwan/data/fixtures/ciecam02_correlates.csv')
with open('src/alwan/data/fixtures/ciecam02_correlates.csv', 'w', newline='') as f:
    all_values = []
    for corr in ciecam02_correlates:
        all_values.extend([format_scalar(v) for v in corr])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/ciecam02_correlates.csv ({len(ciecam02_correlates)} colors)")

# Test inverse: correlates → XYZ
# Use J, C, h from the correlates to reconstruct XYZ
ciecam02_xyz_reconstructed = []
for corr in ciecam02_correlates:
    # Create specification from J, C, h
    spec = colour.CAM_Specification_CIECAM02(J=corr[0], C=corr[1], h=corr[2])
    xyz_recon = colour.CIECAM02_to_XYZ(spec, XYZ_w, L_A, Y_b, surround)
    ciecam02_xyz_reconstructed.append(xyz_recon)

# Write reconstructed XYZ (should match input within numerical tolerance)
ensure_dir('src/alwan/data/fixtures/ciecam02_xyz_reconstructed.csv')
with open('src/alwan/data/fixtures/ciecam02_xyz_reconstructed.csv', 'w', newline='') as f:
    all_values = []
    for xyz in ciecam02_xyz_reconstructed:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/ciecam02_xyz_reconstructed.csv ({len(ciecam02_xyz_reconstructed)} colors)")

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
ensure_dir('src/alwan/data/fixtures/cam16_correlates.csv')
with open('src/alwan/data/fixtures/cam16_correlates.csv', 'w', newline='') as f:
    all_values = []
    for corr in cam16_correlates:
        all_values.extend([format_scalar(v) for v in corr])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/cam16_correlates.csv ({len(cam16_correlates)} colors)")

# Test inverse: correlates -> XYZ
cam16_xyz_reconstructed = []
for corr in cam16_correlates:
    # Create specification from J, C, h
    spec = colour.CAM_Specification_CAM16(J=corr[0], C=corr[1], h=corr[2])
    xyz_recon = colour.CAM16_to_XYZ(spec, XYZ_w, L_A, Y_b, surround)
    cam16_xyz_reconstructed.append(xyz_recon)

# Write reconstructed XYZ
ensure_dir('src/alwan/data/fixtures/cam16_xyz_reconstructed.csv')
with open('src/alwan/data/fixtures/cam16_xyz_reconstructed.csv', 'w', newline='') as f:
    all_values = []
    for xyz in cam16_xyz_reconstructed:
        all_values.extend([format_scalar(v) for v in xyz])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/cam16_xyz_reconstructed.csv ({len(cam16_xyz_reconstructed)} colors)")

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
ensure_dir('src/alwan/data/fixtures/cam16_ucs_jab.csv')
with open('src/alwan/data/fixtures/cam16_ucs_jab.csv', 'w', newline='') as f:
    all_values = []
    for jab in cam16_ucs_jab:
        all_values.extend([format_scalar(v) for v in jab])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/cam16_ucs_jab.csv ({len(cam16_ucs_jab)} colors)")

# ================================================================
# M9: Convenience Color Models (HSV, HSL, CMY, CMYK, YCbCr)
# ================================================================
print("\nGenerating convenience color model test fixtures...")

# Test RGB colors for HSV/HSL round-trip
# Use diverse set including edge cases
conv_test_rgb = [
    [1.0, 0.0, 0.0],      # Pure red
    [0.0, 1.0, 0.0],      # Pure green
    [0.0, 0.0, 1.0],      # Pure blue
    [1.0, 1.0, 0.0],      # Yellow
    [1.0, 0.0, 1.0],      # Magenta
    [0.0, 1.0, 1.0],      # Cyan
    [1.0, 1.0, 1.0],      # White
    [0.0, 0.0, 0.0],      # Black
    [0.5, 0.5, 0.5],      # Gray
    [0.75, 0.25, 0.25],   # Mixed color
    [0.2, 0.6, 0.3],      # Mixed color
]

# Write test RGB colors
ensure_dir('src/alwan/data/fixtures/conv_rgb_input.csv')
with open('src/alwan/data/fixtures/conv_rgb_input.csv', 'w', newline='') as f:
    all_values = []
    for rgb in conv_test_rgb:
        all_values.extend([format_scalar(v) for v in rgb])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/conv_rgb_input.csv ({len(conv_test_rgb)} colors)")

# Compute HSV values using colour-science
hsv_values = []
for rgb in conv_test_rgb:
    # colour-science uses RGB_to_HSV (already returns hue in [0, 1])
    hsv = colour.RGB_to_HSV(np.array(rgb))
    # Handle NaN for grayscale colors
    hsv[0] = 0.0 if hsv[0] is None or np.isnan(hsv[0]) else hsv[0]
    hsv_values.append(hsv)

ensure_dir('src/alwan/data/fixtures/conv_hsv_values.csv')
with open('src/alwan/data/fixtures/conv_hsv_values.csv', 'w', newline='') as f:
    all_values = []
    for hsv in hsv_values:
        all_values.extend([format_scalar(v) for v in hsv])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/conv_hsv_values.csv ({len(hsv_values)} colors)")

# Compute HSL values using colour-science
hsl_values = []
for rgb in conv_test_rgb:
    # colour-science uses RGB_to_HSL (already returns hue in [0, 1])
    hsl = colour.RGB_to_HSL(np.array(rgb))
    # Handle NaN for grayscale colors
    hsl[0] = 0.0 if hsl[0] is None or np.isnan(hsl[0]) else hsl[0]
    hsl_values.append(hsl)

ensure_dir('src/alwan/data/fixtures/conv_hsl_values.csv')
with open('src/alwan/data/fixtures/conv_hsl_values.csv', 'w', newline='') as f:
    all_values = []
    for hsl in hsl_values:
        all_values.extend([format_scalar(v) for v in hsl])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/conv_hsl_values.csv ({len(hsl_values)} colors)")

# CMY/CMYK test values
# CMY is simple complement
cmy_values = []
for rgb in conv_test_rgb:
    cmy = [1.0 - rgb[0], 1.0 - rgb[1], 1.0 - rgb[2]]
    cmy_values.append(cmy)

ensure_dir('src/alwan/data/fixtures/conv_cmy_values.csv')
with open('src/alwan/data/fixtures/conv_cmy_values.csv', 'w', newline='') as f:
    all_values = []
    for cmy in cmy_values:
        all_values.extend([format_scalar(v) for v in cmy])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/conv_cmy_values.csv ({len(cmy_values)} colors)")

# CMYK conversion from CMY
cmyk_values = []
for cmy in cmy_values:
    k = min(cmy[0], cmy[1], cmy[2])
    if k < 1.0:
        c = (cmy[0] - k) / (1.0 - k)
        m = (cmy[1] - k) / (1.0 - k)
        y = (cmy[2] - k) / (1.0 - k)
    else:
        c = m = y = 0.0
    cmyk_values.append([c, m, y, k])

ensure_dir('src/alwan/data/fixtures/conv_cmyk_values.csv')
with open('src/alwan/data/fixtures/conv_cmyk_values.csv', 'w', newline='') as f:
    all_values = []
    for cmyk in cmyk_values:
        all_values.extend([format_scalar(v) for v in cmyk])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/conv_cmyk_values.csv ({len(cmyk_values)} colors)")

# YCbCr test values for BT.601, BT.709, BT.2020
# colour-science uses RGB_to_YCbCr
ycbcr_bt601_values = []
ycbcr_bt709_values = []
ycbcr_bt2020_values = []

for rgb in conv_test_rgb:
    rgb_arr = np.array(rgb)

    # BT.601 coefficients
    K_601 = np.array([0.299, 0.587, 0.114])
    Y_601 = np.dot(K_601, rgb_arr)
    Cb_601 = 0.5 * (rgb_arr[2] - Y_601) / (1.0 - K_601[2])
    Cr_601 = 0.5 * (rgb_arr[0] - Y_601) / (1.0 - K_601[0])
    ycbcr_bt601_values.append([Y_601, Cb_601 + 0.5, Cr_601 + 0.5])

    # BT.709 coefficients
    K_709 = np.array([0.2126, 0.7152, 0.0722])
    Y_709 = np.dot(K_709, rgb_arr)
    Cb_709 = 0.5 * (rgb_arr[2] - Y_709) / (1.0 - K_709[2])
    Cr_709 = 0.5 * (rgb_arr[0] - Y_709) / (1.0 - K_709[0])
    ycbcr_bt709_values.append([Y_709, Cb_709 + 0.5, Cr_709 + 0.5])

    # BT.2020 coefficients
    K_2020 = np.array([0.2627, 0.6780, 0.0593])
    Y_2020 = np.dot(K_2020, rgb_arr)
    Cb_2020 = 0.5 * (rgb_arr[2] - Y_2020) / (1.0 - K_2020[2])
    Cr_2020 = 0.5 * (rgb_arr[0] - Y_2020) / (1.0 - K_2020[0])
    ycbcr_bt2020_values.append([Y_2020, Cb_2020 + 0.5, Cr_2020 + 0.5])

ensure_dir('src/alwan/data/fixtures/conv_ycbcr_bt601.csv')
with open('src/alwan/data/fixtures/conv_ycbcr_bt601.csv', 'w', newline='') as f:
    all_values = []
    for ycbcr in ycbcr_bt601_values:
        all_values.extend([format_scalar(v) for v in ycbcr])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/conv_ycbcr_bt601.csv ({len(ycbcr_bt601_values)} colors)")

ensure_dir('src/alwan/data/fixtures/conv_ycbcr_bt709.csv')
with open('src/alwan/data/fixtures/conv_ycbcr_bt709.csv', 'w', newline='') as f:
    all_values = []
    for ycbcr in ycbcr_bt709_values:
        all_values.extend([format_scalar(v) for v in ycbcr])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/conv_ycbcr_bt709.csv ({len(ycbcr_bt709_values)} colors)")

ensure_dir('src/alwan/data/fixtures/conv_ycbcr_bt2020.csv')
with open('src/alwan/data/fixtures/conv_ycbcr_bt2020.csv', 'w', newline='') as f:
    all_values = []
    for ycbcr in ycbcr_bt2020_values:
        all_values.extend([format_scalar(v) for v in ycbcr])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/conv_ycbcr_bt2020.csv ({len(ycbcr_bt2020_values)} colors)")

# YcCbcCrc (constant luminance) for BT.2020
# This is more complex with conditional formulas
yccbccrc_values = []
K_2020 = np.array([0.2627, 0.6780, 0.0593])

for rgb in conv_test_rgb:
    rgb_arr = np.array(rgb)
    r, g, b = rgb_arr

    # Constant luminance Yc
    yc = np.dot(K_2020, rgb_arr)

    # Conditional Cbc formula
    # Handle edge cases where division would be undefined
    if b <= 0.0 or yc <= 0.0:
        cbc = 0.5
    elif yc >= 1.0:
        cbc = 0.5  # White: no chroma
    elif b < yc:
        cbc = (b - yc) / (2.0 * yc * (1.0 - K_2020[2])) + 0.5
    else:
        cbc = (b - yc) / (2.0 * (1.0 - yc) * (1.0 - K_2020[2])) + 0.5

    # Conditional Crc formula
    if r <= 0.0 or yc <= 0.0:
        crc = 0.5
    elif yc >= 1.0:
        crc = 0.5  # White: no chroma
    elif r < yc:
        crc = (r - yc) / (2.0 * yc * (1.0 - K_2020[0])) + 0.5
    else:
        crc = (r - yc) / (2.0 * (1.0 - yc) * (1.0 - K_2020[0])) + 0.5

    yccbccrc_values.append([yc, cbc, crc])

ensure_dir('src/alwan/data/fixtures/conv_yccbccrc_bt2020.csv')
with open('src/alwan/data/fixtures/conv_yccbccrc_bt2020.csv', 'w', newline='') as f:
    all_values = []
    for ycc in yccbccrc_values:
        all_values.extend([format_scalar(v) for v in ycc])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/conv_yccbccrc_bt2020.csv ({len(yccbccrc_values)} colors)")

# ================================================================
# M10: Light Quality & CCT
# ================================================================
print("\nGenerating CCT and light quality test fixtures...")

# CCT test cases: various points with known CCT values
# Test McCamy and Robertson methods
cct_test_cases = [
    # (x, y, expected_cct_approx)
    (0.31271, 0.32902, 6504),   # D65
    (0.34570, 0.35850, 5003),   # D50
    (0.33242, 0.34743, 5503),   # D55
    (0.32168, 0.33767, 6000),   # D60 (approximate)
    (0.44757, 0.40745, 2856),   # Illuminant A
    (0.25, 0.25, 25000),        # Bluish
    (0.40, 0.40, 3500),         # Warm white
]

# Compute CCT using colour-science for verification
cct_results = []
for x, y, approx_cct in cct_test_cases:
    xy = np.array([x, y])
    try:
        # Use colour-science CCT calculation
        cct_calc = colour.xy_to_CCT(xy, method='McCamy 1992')
        cct_results.append([x, y, cct_calc])
    except:
        # If colour-science fails, use the approximate value
        cct_results.append([x, y, approx_cct])

ensure_dir('src/alwan/data/fixtures/cct_test_cases.csv')
with open('src/alwan/data/fixtures/cct_test_cases.csv', 'w', newline='') as f:
    all_values = []
    for result in cct_results:
        all_values.extend([format_scalar(v) for v in result])
    f.write(','.join(all_values) + '\n')
print(f"  {DATA_DIR}/fixtures/cct_test_cases.csv ({len(cct_results)} test cases)")

# Generate Robertson CCT lookup table (Planckian locus in CIE 1960 UCS)
# This table is used by the Robertson method for accurate CCT estimation
print("\nGenerating Robertson CCT lookup table...")

# CCT values for lookup table (Kelvin)
robertson_ccts = [
    1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000, 5500,
    6000, 6500, 7000, 7500, 8000, 8500, 9000, 9500, 10000,
    12000, 15000, 20000
]

# Compute Planckian locus points in CIE 1960 UCS with slopes
robertson_table = []
for i, cct in enumerate(robertson_ccts):
    # Get xy for blackbody at this CCT using Planckian locus
    # Use uv_to_CCT inverse function - compute directly from Planckian locus
    xy = colour.CCT_to_xy(cct)

    # Convert CIE 1931 xy to CIE 1960 UCS uv
    u = 4 * xy[0] / (12 * xy[1] - 2 * xy[0] + 3)
    v = 6 * xy[1] / (12 * xy[1] - 2 * xy[0] + 3)

    # Compute slopes (du/dT and dv/dT) for interpolation
    # Use finite differences with neighboring points
    if i == 0:
        # Forward difference for first point
        xy_next = colour.CCT_to_xy(robertson_ccts[i+1])
        u_next = 4 * xy_next[0] / (12 * xy_next[1] - 2 * xy_next[0] + 3)
        v_next = 6 * xy_next[1] / (12 * xy_next[1] - 2 * xy_next[0] + 3)
        du = (u_next - u) / (robertson_ccts[i+1] - cct)
        dv = (v_next - v) / (robertson_ccts[i+1] - cct)
    elif i == len(robertson_ccts) - 1:
        # Backward difference for last point
        xy_prev = colour.CCT_to_xy(robertson_ccts[i-1])
        u_prev = 4 * xy_prev[0] / (12 * xy_prev[1] - 2 * xy_prev[0] + 3)
        v_prev = 6 * xy_prev[1] / (12 * xy_prev[1] - 2 * xy_prev[0] + 3)
        du = (u - u_prev) / (cct - robertson_ccts[i-1])
        dv = (v - v_prev) / (cct - robertson_ccts[i-1])
    else:
        # Central difference for middle points
        xy_prev = colour.CCT_to_xy(robertson_ccts[i-1])
        xy_next = colour.CCT_to_xy(robertson_ccts[i+1])
        u_prev = 4 * xy_prev[0] / (12 * xy_prev[1] - 2 * xy_prev[0] + 3)
        v_prev = 6 * xy_prev[1] / (12 * xy_prev[1] - 2 * xy_prev[0] + 3)
        u_next = 4 * xy_next[0] / (12 * xy_next[1] - 2 * xy_next[0] + 3)
        v_next = 6 * xy_next[1] / (12 * xy_next[1] - 2 * xy_next[0] + 3)
        du = (u_next - u_prev) / (robertson_ccts[i+1] - robertson_ccts[i-1])
        dv = (v_next - v_prev) / (robertson_ccts[i+1] - robertson_ccts[i-1])

    robertson_table.append([cct, u, v, du, dv])

# Write Robertson table as single-line CSV for C embedding
# Format: cct1,u1,v1,du1,dv1,cct2,u2,v2,du2,dv2,...
# Use fixed-point notation to avoid scientific notation issues in C compilation
filename = f'{DATA_DIR}/fixtures/robertson_cct_locus.csv'
ensure_dir(filename)
with open(filename, 'w', newline='') as f:
    all_values = []
    for row in robertson_table:
        # Format with enough precision, avoiding scientific notation
        formatted_values = [f'{v:.17f}' if abs(v) < 1.0 else format_scalar(v) for v in row]
        all_values.extend(formatted_values)
    f.write(','.join(all_values) + '\n')
print(f"  {filename} ({len(robertson_table)} CCT points, {len(all_values)} values)")

# ================================================================
# M11: RGB-to-RGB Conversion Test Fixtures
# ================================================================
print("\nGenerating M11 RGB-to-RGB conversion test fixtures...")

# Test colors in sRGB space
rgb_test_colors = [
    [1.0, 0.0, 0.0],  # Red
    [0.0, 1.0, 0.0],  # Green
    [0.0, 0.0, 1.0],  # Blue
    [1.0, 1.0, 0.0],  # Yellow
    [0.0, 1.0, 1.0],  # Cyan
    [1.0, 0.0, 1.0],  # Magenta
    [1.0, 1.0, 1.0],  # White
    [0.5, 0.5, 0.5],  # Gray
]

# Get RGB color space definitions
srgb = colour.RGB_COLOURSPACES['sRGB']
bt709 = colour.RGB_COLOURSPACES['ITU-R BT.709']
display_p3 = colour.RGB_COLOURSPACES['Display P3']
bt2020 = colour.RGB_COLOURSPACES['ITU-R BT.2020']
aces_ap1 = colour.RGB_COLOURSPACES['ACEScg']

# Test case 1: sRGB to Display P3 (same white point D65, no CAT needed)
print("  sRGB -> Display P3...")
srgb_to_p3_results = []
for rgb in rgb_test_colors:
    rgb_array = np.array(rgb)
    # Convert sRGB to Display P3
    p3_rgb = colour.RGB_to_RGB(rgb_array, srgb, display_p3)
    srgb_to_p3_results.extend(rgb)  # Source RGB
    srgb_to_p3_results.extend(p3_rgb.tolist())  # Destination RGB

filename = f'{DATA_DIR}/fixtures/rgb_convert_srgb_to_p3.csv'
ensure_dir(filename)
with open(filename, 'w', newline='') as f:
    formatted_values = [format_scalar(v) for v in srgb_to_p3_results]
    f.write(','.join(formatted_values) + '\n')
print(f"    {filename} ({len(rgb_test_colors)} colors)")

# Test case 2: sRGB to BT.2020 (same white point D65, wider gamut)
print("  sRGB -> BT.2020...")
srgb_to_bt2020_results = []
for rgb in rgb_test_colors:
    rgb_array = np.array(rgb)
    bt2020_rgb = colour.RGB_to_RGB(rgb_array, srgb, bt2020)
    srgb_to_bt2020_results.extend(rgb)
    srgb_to_bt2020_results.extend(bt2020_rgb.tolist())

filename = f'{DATA_DIR}/fixtures/rgb_convert_srgb_to_bt2020.csv'
ensure_dir(filename)
with open(filename, 'w', newline='') as f:
    formatted_values = [format_scalar(v) for v in srgb_to_bt2020_results]
    f.write(','.join(formatted_values) + '\n')
print(f"    {filename} ({len(rgb_test_colors)} colors)")

# Test case 3: sRGB to ACEScg (different white point, D65->D60, needs CAT)
print("  sRGB -> ACEScg (with CAT)...")
srgb_to_aces_results = []
for rgb in rgb_test_colors:
    rgb_array = np.array(rgb)
    aces_rgb = colour.RGB_to_RGB(rgb_array, srgb, aces_ap1,
                                  chromatic_adaptation_transform='Bradford')
    srgb_to_aces_results.extend(rgb)
    srgb_to_aces_results.extend(aces_rgb.tolist())

filename = f'{DATA_DIR}/fixtures/rgb_convert_srgb_to_acescg.csv'
ensure_dir(filename)
with open(filename, 'w', newline='') as f:
    formatted_values = [format_scalar(v) for v in srgb_to_aces_results]
    f.write(','.join(formatted_values) + '\n')
print(f"    {filename} ({len(rgb_test_colors)} colors)")

# ================================================================
# M11: Gamut Mapping Test Fixtures
# ================================================================
print("\nGenerating M11 gamut mapping test fixtures...")

# Out-of-gamut test colors (RGB values outside [0,1])
out_of_gamut_colors = [
    [-0.2, 0.5, 0.8],    # Negative R
    [0.5, -0.1, 0.6],    # Negative G
    [0.3, 0.7, -0.3],    # Negative B
    [1.5, 0.5, 0.3],     # R > 1
    [0.4, 1.8, 0.6],     # G > 1
    [0.2, 0.3, 2.0],     # B > 1
    [-0.3, 1.5, 0.7],    # Mixed out of gamut
    [0.5, 0.5, 0.5],     # In gamut (control)
]

# Generate clipped results (simple clipping to [0,1])
print("  Generating clip mapping results...")
gamut_map_clip_results = []
for rgb in out_of_gamut_colors:
    rgb_array = np.array(rgb)
    # Clip mapping: simple clamp to [0,1]
    clipped = np.clip(rgb_array, 0.0, 1.0)
    gamut_map_clip_results.extend(rgb)  # Input
    gamut_map_clip_results.extend(clipped.tolist())  # Output

filename = f'{DATA_DIR}/fixtures/gamut_map_clip.csv'
ensure_dir(filename)
with open(filename, 'w', newline='') as f:
    formatted_values = [format_scalar(v) for v in gamut_map_clip_results]
    f.write(','.join(formatted_values) + '\n')
print(f"    {filename} ({len(out_of_gamut_colors)} colors)")

# Generate hue-preserving mapping results
# For simplicity, we'll use a basic algorithm: scale towards gray while preserving ratios
print("  Generating hue-preserving mapping results...")
gamut_map_hue_results = []
for rgb in out_of_gamut_colors:
    rgb_array = np.array(rgb)

    # If already in gamut, return as-is
    if np.all(rgb_array >= 0) and np.all(rgb_array <= 1):
        mapped = rgb_array
    else:
        # Compute luminance (sRGB weights)
        L = 0.2126 * rgb_array[0] + 0.7152 * rgb_array[1] + 0.0722 * rgb_array[2]
        L_clamped = np.clip(L, 0.0, 1.0)
        neutral = np.array([L_clamped, L_clamped, L_clamped])

        # Binary search for largest t where t*rgb + (1-t)*neutral is in [0,1]^3
        t_min = 0.0
        t_max = 1.0
        mapped = neutral  # fallback

        for _ in range(20):
            t = (t_min + t_max) * 0.5
            test = t * rgb_array + (1.0 - t) * neutral
            if np.all(test >= 0) and np.all(test <= 1):
                t_min = t
                mapped = test
            else:
                t_max = t

    gamut_map_hue_results.extend(rgb)  # Input
    gamut_map_hue_results.extend(mapped.tolist())  # Output

filename = f'{DATA_DIR}/fixtures/gamut_map_hue_preserving.csv'
ensure_dir(filename)
with open(filename, 'w', newline='') as f:
    formatted_values = [format_scalar(v) for v in gamut_map_hue_results]
    f.write(','.join(formatted_values) + '\n')
print(f"    {filename} ({len(out_of_gamut_colors)} colors)")

# ================================================================
# Oklab & Oklch Test Fixtures
# ================================================================
print("\nGenerating Oklab & Oklch test fixtures...")

# Test colors in XYZ (D65)
oklab_test_xyz = [
    [0.95047, 1.00000, 1.08883],  # D65 white
    [0.00000, 0.00000, 0.00000],  # Black
    [0.41246, 0.21267, 0.01933],  # sRGB red
    [0.35758, 0.71515, 0.11919],  # sRGB green
    [0.18048, 0.07217, 0.95030],  # sRGB blue
    [0.76986, 0.92783, 0.13853],  # sRGB yellow
    [0.53806, 0.78732, 1.06950],  # sRGB cyan
    [0.59294, 0.28484, 0.96963],  # sRGB magenta
    [0.20517, 0.21586, 0.23306],  # Mid gray
    [0.09505, 0.10000, 0.10888],  # Dark gray
    [0.76032, 0.80000, 0.87064],  # Light gray
]

# Generate Oklab values
print("  Generating Oklab values...")
oklab_values = []
for xyz in oklab_test_xyz:
    oklab = colour.XYZ_to_Oklab(np.array(xyz))
    oklab_values.extend(xyz)       # Input XYZ
    oklab_values.extend(oklab.tolist())  # Output Oklab

filename = f'{DATA_DIR}/fixtures/oklab_values.csv'
ensure_dir(filename)
with open(filename, 'w', newline='') as f:
    formatted_values = [format_scalar(v) for v in oklab_values]
    f.write(','.join(formatted_values) + '\n')
print(f"    {filename} ({len(oklab_test_xyz)} colors)")

# Generate Oklch values from Oklab
print("  Generating Oklch values...")
oklch_test_data = []
for xyz in oklab_test_xyz:
    oklab = colour.XYZ_to_Oklab(np.array(xyz))

    # Oklab to Oklch (L, C, h)
    L = oklab[0]
    C = np.sqrt(oklab[1]**2 + oklab[2]**2)
    h = np.arctan2(oklab[2], oklab[1])  # in radians

    oklch_test_data.extend(oklab.tolist())  # Input Oklab
    oklch_test_data.extend([L, C, h])       # Output Oklch

filename = f'{DATA_DIR}/fixtures/oklch_values.csv'
ensure_dir(filename)
with open(filename, 'w', newline='') as f:
    formatted_values = [format_scalar(v) for v in oklch_test_data]
    f.write(','.join(formatted_values) + '\n')
print(f"    {filename} ({len(oklab_test_xyz)} colors)")

# ================================================================
# M5: Spectral Data - Color Matching Functions (CMFs)
# ================================================================
print("\nGenerating Color Matching Functions (CMFs)...")

# Create CMF and illuminants directories
os.makedirs(f'{DATA_DIR}/cmf', exist_ok=True)
os.makedirs(f'{DATA_DIR}/illuminants', exist_ok=True)

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
ensure_dir('src/alwan/data/cmf/cie_1931_2deg_x_360_830_1nm.csv')
with open('src/alwan/data/cmf/cie_1931_2deg_x_360_830_1nm.csv', 'w', newline='') as f:
    values = [format_scalar(v) for v in x_bar_filtered]
    f.write(','.join(values) + '\n')
print(f"  {DATA_DIR}/cmf/cie_1931_2deg_x_360_830_1nm.csv ({len(x_bar_filtered)} samples)")

ensure_dir('src/alwan/data/cmf/cie_1931_2deg_y_360_830_1nm.csv')
with open('src/alwan/data/cmf/cie_1931_2deg_y_360_830_1nm.csv', 'w', newline='') as f:
    values = [format_scalar(v) for v in y_bar_filtered]
    f.write(','.join(values) + '\n')
print(f"  {DATA_DIR}/cmf/cie_1931_2deg_y_360_830_1nm.csv ({len(y_bar_filtered)} samples)")

ensure_dir('src/alwan/data/cmf/cie_1931_2deg_z_360_830_1nm.csv')
with open('src/alwan/data/cmf/cie_1931_2deg_z_360_830_1nm.csv', 'w', newline='') as f:
    values = [format_scalar(v) for v in z_bar_filtered]
    f.write(','.join(values) + '\n')
print(f"  {DATA_DIR}/cmf/cie_1931_2deg_z_360_830_1nm.csv ({len(z_bar_filtered)} samples)")

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

    ensure_dir('src/alwan/data/cmf/cie_1964_10deg_x_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_1964_10deg_x_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in x_bar_1964_filtered]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_1964_10deg_x_360_830_1nm.csv ({len(x_bar_1964_filtered)} samples)")

    ensure_dir('src/alwan/data/cmf/cie_1964_10deg_y_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_1964_10deg_y_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in y_bar_1964_filtered]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_1964_10deg_y_360_830_1nm.csv ({len(y_bar_1964_filtered)} samples)")

    ensure_dir('src/alwan/data/cmf/cie_1964_10deg_z_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_1964_10deg_z_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in z_bar_1964_filtered]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_1964_10deg_z_360_830_1nm.csv ({len(z_bar_1964_filtered)} samples)")
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

    ensure_dir('src/alwan/data/cmf/cie_2012_2deg_x_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_2012_2deg_x_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in x_bar_2012_2_filtered]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_2012_2deg_x_360_830_1nm.csv ({len(x_bar_2012_2_filtered)} samples)")

    ensure_dir('src/alwan/data/cmf/cie_2012_2deg_y_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_2012_2deg_y_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in y_bar_2012_2_filtered]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_2012_2deg_y_360_830_1nm.csv ({len(y_bar_2012_2_filtered)} samples)")

    ensure_dir('src/alwan/data/cmf/cie_2012_2deg_z_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_2012_2deg_z_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in z_bar_2012_2_filtered]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_2012_2deg_z_360_830_1nm.csv ({len(z_bar_2012_2_filtered)} samples)")
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

    ensure_dir('src/alwan/data/cmf/cie_2012_10deg_x_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_2012_10deg_x_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in x_bar_2012_10_filtered]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_2012_10deg_x_360_830_1nm.csv ({len(x_bar_2012_10_filtered)} samples)")

    ensure_dir('src/alwan/data/cmf/cie_2012_10deg_y_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_2012_10deg_y_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in y_bar_2012_10_filtered]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_2012_10deg_y_360_830_1nm.csv ({len(y_bar_2012_10_filtered)} samples)")

    ensure_dir('src/alwan/data/cmf/cie_2012_10deg_z_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_2012_10deg_z_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in z_bar_2012_10_filtered]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_2012_10deg_z_360_830_1nm.csv ({len(z_bar_2012_10_filtered)} samples)")
except Exception as e:
    print(f"  Warning: Could not generate CIE 2012 10° CMFs: {e}", file=sys.stderr)

# Stockman & Sharpe 2000 2° Cone Fundamentals (360-830nm, 1nm steps)
try:
    cmfs_ss = colour.MSDS_CMFS['Stockman & Sharpe 2 Degree Cone Fundamentals']

    x_bar_ss = cmfs_ss.values[:, 0]
    y_bar_ss = cmfs_ss.values[:, 1]
    z_bar_ss = cmfs_ss.values[:, 2]
    wavelengths_ss = cmfs_ss.wavelengths

    mask_ss = (wavelengths_ss >= 360) & (wavelengths_ss <= 830)
    x_bar_ss_filtered = x_bar_ss[mask_ss]
    y_bar_ss_filtered = y_bar_ss[mask_ss]
    z_bar_ss_filtered = z_bar_ss[mask_ss]

    ensure_dir('src/alwan/data/cmf/stockman_sharpe_2deg_x_360_830_1nm.csv')
    with open('src/alwan/data/cmf/stockman_sharpe_2deg_x_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in x_bar_ss_filtered]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/stockman_sharpe_2deg_x_360_830_1nm.csv ({len(x_bar_ss_filtered)} samples)")

    ensure_dir('src/alwan/data/cmf/stockman_sharpe_2deg_y_360_830_1nm.csv')
    with open('src/alwan/data/cmf/stockman_sharpe_2deg_y_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in y_bar_ss_filtered]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/stockman_sharpe_2deg_y_360_830_1nm.csv ({len(y_bar_ss_filtered)} samples)")

    ensure_dir('src/alwan/data/cmf/stockman_sharpe_2deg_z_360_830_1nm.csv')
    with open('src/alwan/data/cmf/stockman_sharpe_2deg_z_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in z_bar_ss_filtered]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/stockman_sharpe_2deg_z_360_830_1nm.csv ({len(z_bar_ss_filtered)} samples)")
except Exception as e:
    print(f"  Warning: Could not generate Stockman & Sharpe 2° CMFs: {e}", file=sys.stderr)

# CIE 2015 2° Cone Fundamentals (390-830nm originally, filtered to 360-830nm)
try:
    cmfs_2015_2 = colour.MSDS_CMFS['CIE 2015 2 Degree Standard Observer']

    x_bar_2015_2 = cmfs_2015_2.values[:, 0]
    y_bar_2015_2 = cmfs_2015_2.values[:, 1]
    z_bar_2015_2 = cmfs_2015_2.values[:, 2]
    wavelengths_2015_2 = cmfs_2015_2.wavelengths

    # Filter to 360-830nm range (original is 390-830, so pad with zeros for 360-389)
    target_wl = np.arange(360, 831, 1)
    x_bar_2015_2_full = np.interp(target_wl, wavelengths_2015_2, x_bar_2015_2, left=0, right=0)
    y_bar_2015_2_full = np.interp(target_wl, wavelengths_2015_2, y_bar_2015_2, left=0, right=0)
    z_bar_2015_2_full = np.interp(target_wl, wavelengths_2015_2, z_bar_2015_2, left=0, right=0)

    ensure_dir('src/alwan/data/cmf/cie_2015_2deg_x_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_2015_2deg_x_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in x_bar_2015_2_full]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_2015_2deg_x_360_830_1nm.csv ({len(x_bar_2015_2_full)} samples)")

    ensure_dir('src/alwan/data/cmf/cie_2015_2deg_y_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_2015_2deg_y_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in y_bar_2015_2_full]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_2015_2deg_y_360_830_1nm.csv ({len(y_bar_2015_2_full)} samples)")

    ensure_dir('src/alwan/data/cmf/cie_2015_2deg_z_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_2015_2deg_z_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in z_bar_2015_2_full]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_2015_2deg_z_360_830_1nm.csv ({len(z_bar_2015_2_full)} samples)")
except Exception as e:
    print(f"  Warning: Could not generate CIE 2015 2° CMFs: {e}", file=sys.stderr)

# CIE 2015 10° Cone Fundamentals (390-830nm originally, filtered to 360-830nm)
try:
    cmfs_2015_10 = colour.MSDS_CMFS['CIE 2015 10 Degree Standard Observer']

    x_bar_2015_10 = cmfs_2015_10.values[:, 0]
    y_bar_2015_10 = cmfs_2015_10.values[:, 1]
    z_bar_2015_10 = cmfs_2015_10.values[:, 2]
    wavelengths_2015_10 = cmfs_2015_10.wavelengths

    # Filter to 360-830nm range (original is 390-830, so pad with zeros for 360-389)
    target_wl = np.arange(360, 831, 1)
    x_bar_2015_10_full = np.interp(target_wl, wavelengths_2015_10, x_bar_2015_10, left=0, right=0)
    y_bar_2015_10_full = np.interp(target_wl, wavelengths_2015_10, y_bar_2015_10, left=0, right=0)
    z_bar_2015_10_full = np.interp(target_wl, wavelengths_2015_10, z_bar_2015_10, left=0, right=0)

    ensure_dir('src/alwan/data/cmf/cie_2015_10deg_x_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_2015_10deg_x_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in x_bar_2015_10_full]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_2015_10deg_x_360_830_1nm.csv ({len(x_bar_2015_10_full)} samples)")

    ensure_dir('src/alwan/data/cmf/cie_2015_10deg_y_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_2015_10deg_y_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in y_bar_2015_10_full]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_2015_10deg_y_360_830_1nm.csv ({len(y_bar_2015_10_full)} samples)")

    ensure_dir('src/alwan/data/cmf/cie_2015_10deg_z_360_830_1nm.csv')
    with open('src/alwan/data/cmf/cie_2015_10deg_z_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in z_bar_2015_10_full]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/cie_2015_10deg_z_360_830_1nm.csv ({len(z_bar_2015_10_full)} samples)")
except Exception as e:
    print(f"  Warning: Could not generate CIE 2015 10° CMFs: {e}", file=sys.stderr)

# Wright & Guild 1931 2° RGB CMFs (380-780nm originally, extended to 360-830nm)
try:
    cmfs_wg = colour.MSDS_CMFS['Wright & Guild 1931 2 Degree RGB CMFs']

    r_bar_wg = cmfs_wg.values[:, 0]
    g_bar_wg = cmfs_wg.values[:, 1]
    b_bar_wg = cmfs_wg.values[:, 2]
    wavelengths_wg = cmfs_wg.wavelengths

    # Extend to 360-830nm range (original is 380-780, so pad with zeros)
    target_wl = np.arange(360, 831, 1)
    r_bar_wg_full = np.interp(target_wl, wavelengths_wg, r_bar_wg, left=0, right=0)
    g_bar_wg_full = np.interp(target_wl, wavelengths_wg, g_bar_wg, left=0, right=0)
    b_bar_wg_full = np.interp(target_wl, wavelengths_wg, b_bar_wg, left=0, right=0)

    ensure_dir('src/alwan/data/cmf/wright_guild_1931_r_360_830_1nm.csv')
    with open('src/alwan/data/cmf/wright_guild_1931_r_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in r_bar_wg_full]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/wright_guild_1931_r_360_830_1nm.csv ({len(r_bar_wg_full)} samples)")

    ensure_dir('src/alwan/data/cmf/wright_guild_1931_g_360_830_1nm.csv')
    with open('src/alwan/data/cmf/wright_guild_1931_g_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in g_bar_wg_full]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/wright_guild_1931_g_360_830_1nm.csv ({len(g_bar_wg_full)} samples)")

    ensure_dir('src/alwan/data/cmf/wright_guild_1931_b_360_830_1nm.csv')
    with open('src/alwan/data/cmf/wright_guild_1931_b_360_830_1nm.csv', 'w', newline='') as f:
        values = [format_scalar(v) for v in b_bar_wg_full]
        f.write(','.join(values) + '\n')
    print(f"  {DATA_DIR}/cmf/wright_guild_1931_b_360_830_1nm.csv ({len(b_bar_wg_full)} samples)")
except Exception as e:
    print(f"  Warning: Could not generate Wright & Guild 1931 RGB CMFs: {e}", file=sys.stderr)

# ================================================================
# M5: Spectral Data - Illuminant SPDs
# ================================================================
print("\nGenerating Illuminant SPDs...")

# List of illuminants to generate
illuminant_names = ['A', 'D50', 'D55', 'D65', 'E',
                    'F1', 'F2', 'F3', 'F4', 'F5', 'F6',
                    'F7', 'F8', 'F9', 'F10', 'F11', 'F12',
                    'B', 'C', 'D60', 'D75',  # Basic extended illuminants
                    'LED-B1', 'LED-B2', 'LED-B3', 'LED-B4', 'LED-B5',  # LED illuminants
                    'LED-BH1', 'LED-V1', 'LED-V2', 'LED-RGB1',
                    'HP1', 'HP2', 'HP3', 'HP4', 'HP5']  # High Pressure illuminants

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
        filename = f'{DATA_DIR}/illuminants/{illum_name}_360_830_1nm.csv'
        ensure_dir(filename)
        with open(filename, 'w', newline='') as f:
            formatted_values = [format_scalar(v) for v in resampled_values]
            f.write(','.join(formatted_values) + '\n')
        print(f"  {filename} ({len(resampled_values)} samples)")

    except Exception as e:
        print(f"  Warning: Could not generate illuminant {illum_name}: {e}", file=sys.stderr)

# ================================================================
# Additional D-series Illuminants (computed from CCT)
# ================================================================
print("\nGenerating additional D-series illuminants (D40, D45, D93)...")

# D-series CCTs to generate
d_series_to_generate = {
    'D40': 4000,
    'D45': 4500,
    'D93': 9300,
}

for d_name, cct in d_series_to_generate.items():
    try:
        # Compute xy from CCT using Planckian locus
        xy = colour.CCT_to_xy(cct, method='Kang 2002')

        # Generate D-series SPD from xy chromaticity
        illum_spd = colour.sd_CIE_illuminant_D_series(xy)

        # Resample to 360-830nm, 1nm steps
        target_wavelengths = np.arange(360, 831, 1)
        resampled_values = np.interp(
            target_wavelengths,
            illum_spd.wavelengths,
            illum_spd.values,
            left=0.0,
            right=0.0
        )

        # Write illuminant SPD data
        filename = f'{DATA_DIR}/illuminants/{d_name}_360_830_1nm.csv'
        ensure_dir(filename)
        with open(filename, 'w', newline='') as f:
            formatted_values = [format_scalar(v) for v in resampled_values]
            f.write(','.join(formatted_values) + '\n')
        print(f"  {filename} ({len(resampled_values)} samples, {cct}K)")

    except Exception as e:
        print(f"  Warning: Could not generate {d_name}: {e}", file=sys.stderr)

# ================================================================
# Illuminant xy Chromaticity Coordinates
# ================================================================
print("\nGenerating Illuminant xy Chromaticity Coordinates...")

# List of illuminants to generate xy coordinates for
illuminant_xy_names = ['A', 'D50', 'D55', 'D60', 'D65', 'E', 'B', 'C', 'D75',
                       'LED-B1', 'LED-B2', 'LED-B3', 'LED-B4', 'LED-B5',
                       'LED-BH1', 'LED-V1', 'LED-V2', 'LED-RGB1',
                       'HP1', 'HP2', 'HP3', 'HP4', 'HP5']

for illum_name in illuminant_xy_names:
    try:
        # Get illuminant SPD from colour-science
        if illum_name in colour.SDS_ILLUMINANTS:
            illum_spd = colour.SDS_ILLUMINANTS[illum_name]
        else:
            print(f"  Warning: Illuminant {illum_name} not found in colour-science", file=sys.stderr)
            continue

        # Get CIE 1931 2° Standard Observer
        cmfs = colour.MSDS_CMFS['CIE 1931 2 Degree Standard Observer']

        # Compute XYZ tristimulus values
        XYZ = colour.sd_to_XYZ(illum_spd, cmfs)

        # Convert XYZ to xy chromaticity coordinates
        xy = colour.XYZ_to_xy(XYZ)

        # Write xy coordinates (2 values: x, y)
        filename = f'{DATA_DIR}/illuminants_xy/{illum_name.lower()}_xy.csv'
        ensure_dir(filename)
        with open(filename, 'w', newline='') as f:
            formatted_values = [format_scalar(v) for v in xy]
            f.write(','.join(formatted_values) + '\n')
        print(f"  {filename} (x={xy[0]:.10f}, y={xy[1]:.10f})")

    except Exception as e:
        print(f"  Warning: Could not generate xy for illuminant {illum_name}: {e}", file=sys.stderr)

# Generate xy coordinates for D40, D45, D93 (from SPDs we just generated)
print("\nGenerating xy coordinates for D40, D45, D93...")
for d_name, cct in d_series_to_generate.items():
    try:
        # Load the SPD we just generated
        filename_spd = f'{DATA_DIR}/illuminants/{d_name}_360_830_1nm.csv'
        with open(filename_spd, 'r') as f:
            spd_values = [float(v) for v in f.read().strip().split(',')]

        # Create wavelengths array
        wavelengths = np.arange(360, 831, 1)

        # Get CIE 1931 2° Standard Observer CMF values at these wavelengths
        cmfs = colour.MSDS_CMFS['CIE 1931 2 Degree Standard Observer']

        # Resample CMF to match our wavelengths
        x_bar = np.interp(wavelengths, cmfs.wavelengths, cmfs.values[:, 0], left=0, right=0)
        y_bar = np.interp(wavelengths, cmfs.wavelengths, cmfs.values[:, 1], left=0, right=0)
        z_bar = np.interp(wavelengths, cmfs.wavelengths, cmfs.values[:, 2], left=0, right=0)

        # Compute XYZ (Simpson integration approximation using trapezoidal)
        spd_array = np.array(spd_values)
        X = np.trapz(spd_array * x_bar, wavelengths)
        Y = np.trapz(spd_array * y_bar, wavelengths)
        Z = np.trapz(spd_array * z_bar, wavelengths)

        # Normalize and convert to xy
        XYZ = np.array([X, Y, Z])
        xy = colour.XYZ_to_xy(XYZ)

        # Write xy coordinates
        filename_xy = f'{DATA_DIR}/illuminants_xy/{d_name.lower()}_xy.csv'
        ensure_dir(filename_xy)
        with open(filename_xy, 'w', newline='') as f:
            formatted_values = [format_scalar(v) for v in xy]
            f.write(','.join(formatted_values) + '\n')
        print(f"  {filename_xy} (x={xy[0]:.10f}, y={xy[1]:.10f})")

    except Exception as e:
        print(f"  Warning: Could not generate xy for {d_name}: {e}", file=sys.stderr)

# ================================================================
# Spectral Upsampling - Basis Functions
# ================================================================
print("\n--- Generating Spectral Upsampling Basis Functions ---")

# ----------------------------------------------------------------
# Smits1999 Basis Spectra
# ----------------------------------------------------------------
print("\nGenerating Smits1999 basis spectra...")

try:
    import colour.recovery.smits1999 as smits

    # Create output directory
    smits_dir = f'{DATA_DIR}/spectral_basis/smits1999'
    os.makedirs(smits_dir, exist_ok=True)

    # Get Smits basis spectra
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
                wl_filename = f'{smits_dir}/wavelengths.csv'
                with open(wl_filename, 'w', newline='') as f:
                    formatted_wl = [format_scalar(w) for w in wavelengths]
                    f.write(','.join(formatted_wl) + '\n')
                print(f"  {wl_filename} ({len(wavelengths)} wavelengths)")

            # Values file
            val_filename = f'{smits_dir}/{name}.csv'
            with open(val_filename, 'w', newline='') as f:
                formatted_vals = [format_scalar(v) for v in values]
                f.write(','.join(formatted_vals) + '\n')
            print(f"  {val_filename} ({len(values)} values)")
        else:
            print(f"  Warning: {name} basis not found in Smits1999 data", file=sys.stderr)

except Exception as e:
    print(f"  Error: Could not generate Smits1999 basis spectra: {e}", file=sys.stderr)
    import traceback
    traceback.print_exc(file=sys.stderr)

# ----------------------------------------------------------------
# Mallett2019 Basis Functions
# ----------------------------------------------------------------
print("\nGenerating Mallett2019 basis functions...")

try:
    import colour.recovery

    # Create output directory
    mallett_dir = f'{DATA_DIR}/spectral_basis/mallett2019'
    os.makedirs(mallett_dir, exist_ok=True)

    # Get Mallett2019 sRGB basis functions
    mallett_basis = colour.recovery.MSDS_BASIS_FUNCTIONS_sRGB_MALLETT2019

    wavelengths = mallett_basis.wavelengths
    labels = mallett_basis.labels  # ['red', 'green', 'blue']

    # Write wavelengths (same for all basis functions)
    wl_filename = f'{mallett_dir}/wavelengths.csv'
    with open(wl_filename, 'w', newline='') as f:
        formatted_wl = [format_scalar(w) for w in wavelengths]
        f.write(','.join(formatted_wl) + '\n')
    print(f"  {wl_filename} ({len(wavelengths)} wavelengths)")

    # Write each basis function (red, green, blue)
    for idx, label in enumerate(labels):
        basis_values = mallett_basis.values[:, idx]
        val_filename = f'{mallett_dir}/{label}.csv'
        with open(val_filename, 'w', newline='') as f:
            formatted_vals = [format_scalar(v) for v in basis_values]
            f.write(','.join(formatted_vals) + '\n')
        print(f"  {val_filename} ({len(basis_values)} values)")

except Exception as e:
    print(f"  Error: Could not generate Mallett2019 basis functions: {e}", file=sys.stderr)
    import traceback
    traceback.print_exc(file=sys.stderr)

# ============================================================
# Jakob2019 Polynomial LUT (using native C++ tool)
# ============================================================
print("\n--- Generating Jakob2019 Polynomial LUTs ---")

# Create output directory
jakob_dir = f'{DATA_DIR}/spectral_lut/jakob2019'
os.makedirs(jakob_dir, exist_ok=True)

# Check if the rgb2spec tool exists
tool_path = 'tools/datagen/gen_jakob2019_table.exe'
if not os.path.exists(tool_path):
    print(f"  Warning: {tool_path} not found!")
    print(f"  Please build the RGB2SpecTool project first.")
    print(f"  Skipping Jakob2019 LUT generation.")
else:
    import struct
    import subprocess

    # Resolution for the LUT (64^3 as specified in the paper)
    resolution = 64

    # Supported gamuts (as defined in rgb2spec)
    # Generate for all supported gamuts
    gamuts_to_generate = ['sRGB', 'ProPhotoRGB', 'ACES2065_1', 'REC2020', 'eRGB', 'XYZ']

    def read_rgb2spec_binary(filename):
        """Read the binary LUT file generated by rgb2spec tool"""
        with open(filename, 'rb') as f:
            # Read header
            magic = f.read(4)
            if magic != b'SPEC':
                raise ValueError(f"Invalid magic number: {magic}")

            # Read resolution
            res_bytes = f.read(4)
            res = struct.unpack('I', res_bytes)[0]

            # Read scale array
            scale_data = f.read(res * 4)  # 4 bytes per float
            scale = struct.unpack(f'{res}f', scale_data)

            # Read coefficient data: 3 channels * 3 coefficients * res^3 entries
            num_floats = 3 * 3 * res * res * res
            coeff_data = f.read(num_floats * 4)
            coeffs = struct.unpack(f'{num_floats}f', coeff_data)

            return res, scale, coeffs

    def convert_to_csv_format(res, coeffs):
        """Convert binary coefficients to per-channel CSV format"""
        # Coefficients are stored as [channel][coeff][entry]
        # where channel = 3 (RGB), coeff = 3 (c0,c1,c2), entry = res^3
        num_entries = res * res * res

        c0_values = []
        c1_values = []
        c2_values = []

        # The data is organized as 3 color channels, each with 3*res^3 values
        # Each entry has 3 coefficients (c0, c1, c2)
        for i in range(num_entries):
            # For each RGB grid point, we need the 3 coefficients
            # The tool outputs coefficients for each of 3 color channels
            # We'll use the red channel (index 0) for now
            offset = i * 3
            c0_values.append(format_scalar(coeffs[offset + 0]))
            c1_values.append(format_scalar(coeffs[offset + 1]))
            c2_values.append(format_scalar(coeffs[offset + 2]))

        return c0_values, c1_values, c2_values

    for gamut in gamuts_to_generate:
        # Check if output files already exist
        gamut_suffix = '' if gamut == 'sRGB' else f'_{gamut.lower()}'
        c0_file = f'{jakob_dir}/jakob2019_lut_c0{gamut_suffix}.csv'
        c1_file = f'{jakob_dir}/jakob2019_lut_c1{gamut_suffix}.csv'
        c2_file = f'{jakob_dir}/jakob2019_lut_c2{gamut_suffix}.csv'

        if os.path.exists(c0_file) and os.path.exists(c1_file) and os.path.exists(c2_file):
            print(f"\n{gamut} {resolution}x{resolution}x{resolution} LUT files already exist, skipping generation.")
            continue

        print(f"\nGenerating {gamut} {resolution}x{resolution}x{resolution} LUT...")

        # Temporary binary output file
        binary_file = f'{jakob_dir}/temp_{gamut.lower()}.bin'

        try:
            # Run the rgb2spec tool
            cmd = [tool_path, str(resolution), binary_file, gamut]
            print(f"  Running: {' '.join(cmd)}")
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)

            if result.returncode != 0:
                print(f"  Error running tool: {result.stderr}")
                continue

            print(f"  {result.stdout.strip()}")

            # Read the binary output
            print(f"  Reading binary output...")
            res, scale, coeffs = read_rgb2spec_binary(binary_file)

            if res != resolution:
                print(f"  Warning: Expected resolution {resolution}, got {res}")
                continue

            # Convert to CSV format
            print(f"  Converting to CSV format...")
            c0_values, c1_values, c2_values = convert_to_csv_format(res, coeffs)

            # Write per-channel CSV files
            gamut_suffix = '' if gamut == 'sRGB' else f'_{gamut.lower()}'

            c0_file = f'{jakob_dir}/jakob2019_lut_c0{gamut_suffix}.csv'
            with open(c0_file, 'w') as f:
                f.write(','.join(c0_values) + '\n')
            print(f"  {c0_file}")

            c1_file = f'{jakob_dir}/jakob2019_lut_c1{gamut_suffix}.csv'
            with open(c1_file, 'w') as f:
                f.write(','.join(c1_values) + '\n')
            print(f"  {c1_file}")

            c2_file = f'{jakob_dir}/jakob2019_lut_c2{gamut_suffix}.csv'
            with open(c2_file, 'w') as f:
                f.write(','.join(c2_values) + '\n')
            print(f"  {c2_file}")

            # Clean up temporary binary file
            os.remove(binary_file)

            # Calculate file size
            num_entries = resolution ** 3
            bytes_f64 = num_entries * 3 * 8
            print(f"  Total size: {bytes_f64:,} bytes ({bytes_f64/1024:.1f} KB, {bytes_f64/1024/1024:.2f} MB)")
            print(f"  Generated Jakob2019 {gamut} {resolution}x{resolution}x{resolution} LUT")

        except subprocess.TimeoutExpired:
            print(f"  Error: Tool timed out after 10 minutes")
        except Exception as e:
            print(f"  Error generating {gamut} LUT: {e}")
            import traceback
            traceback.print_exc(file=sys.stderr)

# ============================================================
# View Transform Matrices
# ============================================================
print("\n--- Generating View Transform Matrices ---")

# Create matrices directory
os.makedirs(f'{DATA_DIR}/matrices', exist_ok=True)

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
    filename = f'{DATA_DIR}/matrices/aces_ap1_to_ap0.csv'
    ensure_dir(filename)
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
    filename = f'{DATA_DIR}/matrices/aces_odt_rec709.csv'
    ensure_dir(filename)
    with open(filename, 'w', newline='') as f:
        flat_matrix = odt_matrix.flatten()
        formatted_values = [format_scalar(v) for v in flat_matrix]
        f.write(','.join(formatted_values) + '\n')
    print(f"  {filename} (3x3 matrix, 9 values)")

except Exception as e:
    print(f"  Warning: Could not generate ODT matrix: {e}", file=sys.stderr)

try:
    # Chromatic Adaptation Transform (CAT) matrices
    print("\nGenerating HPE (Hunt-Pointer-Estevez) matrix for CIECAM02/CAM16...")

    # HPE matrix for XYZ to LMS conversion (used in CIECAM02/CAM16)
    # This is a standard matrix in color appearance models
    hpe_matrix = np.array([
        [ 0.38971, 0.68898, -0.07868],
        [-0.22981, 1.18340,  0.04641],
        [ 0.00000, 0.00000,  1.00000]
    ])

    filename = f'{DATA_DIR}/matrices/hpe.csv'
    ensure_dir(filename)
    with open(filename, 'w', newline='') as f:
        flat_matrix = hpe_matrix.flatten()
        formatted_values = [format_scalar(v) for v in flat_matrix]
        f.write(','.join(formatted_values) + '\n')
    print(f"  {filename} (3x3 matrix, 9 values)")

    # HPE inverse matrix for LMS to XYZ conversion
    hpe_inv_matrix = np.linalg.inv(hpe_matrix)

    filename = f'{DATA_DIR}/matrices/hpe_inv.csv'
    ensure_dir(filename)
    with open(filename, 'w', newline='') as f:
        flat_matrix = hpe_inv_matrix.flatten()
        formatted_values = [format_scalar(v) for v in flat_matrix]
        f.write(','.join(formatted_values) + '\n')
    print(f"  {filename} (3x3 matrix, 9 values)")

    print("\nGenerating CAT matrices...")

    # Bradford CAT matrix
    bradford_matrix = colour.CHROMATIC_ADAPTATION_TRANSFORMS['Bradford']
    filename = f'{DATA_DIR}/matrices/cat_bradford.csv'
    ensure_dir(filename)
    with open(filename, 'w', newline='') as f:
        flat_matrix = bradford_matrix.flatten()
        formatted_values = [format_scalar(v) for v in flat_matrix]
        f.write(','.join(formatted_values) + '\n')
    print(f"  {filename} (3x3 matrix, 9 values)")

    # CAT02 matrix
    cat02_matrix = colour.CHROMATIC_ADAPTATION_TRANSFORMS['CAT02']
    filename = f'{DATA_DIR}/matrices/cat_cat02.csv'
    ensure_dir(filename)
    with open(filename, 'w', newline='') as f:
        flat_matrix = cat02_matrix.flatten()
        formatted_values = [format_scalar(v) for v in flat_matrix]
        f.write(','.join(formatted_values) + '\n')
    print(f"  {filename} (3x3 matrix, 9 values)")

    # CAT16 matrix
    cat16_matrix = colour.CHROMATIC_ADAPTATION_TRANSFORMS['CAT16']
    filename = f'{DATA_DIR}/matrices/cat_cat16.csv'
    ensure_dir(filename)
    with open(filename, 'w', newline='') as f:
        flat_matrix = cat16_matrix.flatten()
        formatted_values = [format_scalar(v) for v in flat_matrix]
        f.write(','.join(formatted_values) + '\n')
    print(f"  {filename} (3x3 matrix, 9 values)")

    # Extended CAT matrices
    print("\nGenerating extended CAT matrices...")

    p7_cat_transforms = {
        'Sharp': 'cat_sharp.csv',
        'Fairchild': 'cat_fairchild.csv',
        'CMCCAT97': 'cat_cmccat97.csv',
        'CMCCAT2000': 'cat_cmccat2000.csv',
        'CAT02 Brill 2008': 'cat_cat02_brill_2008.csv',
        'Bianco 2010': 'cat_bianco_2010.csv',
        'Bianco PC 2010': 'cat_bianco_pc_2010.csv'
    }

    for transform_name, csv_name in p7_cat_transforms.items():
        cat_matrix = colour.CHROMATIC_ADAPTATION_TRANSFORMS[transform_name]
        filename = f'{DATA_DIR}/matrices/{csv_name}'
        ensure_dir(filename)
        with open(filename, 'w', newline='') as f:
            flat_matrix = cat_matrix.flatten()
            formatted_values = [format_scalar(v) for v in flat_matrix]
            f.write(','.join(formatted_values) + '\n')
        print(f"  {filename} (3x3 matrix, 9 values)")

except Exception as e:
    print(f"  Warning: Could not generate CAT matrices: {e}", file=sys.stderr)

# ================================================================
# Generate TCS (Test Color Samples) for CRI
# ================================================================
print("\nGenerating TCS (Test Color Samples) for CRI...")

try:
    from colour.quality import SDS_TCS
    import gzip

    # TCS samples are from CIE 13.3-1995 (14 samples for CRI)
    tcs_samples = list(SDS_TCS.values())[:14]  # First 14 samples

    # Target wavelength range: 360-830nm @ 5nm intervals (95 samples)
    target_wavelengths = np.arange(360, 835, 5)

    for i, sd in enumerate(tcs_samples, start=1):
        # Interpolate to target wavelengths
        resampled = np.interp(target_wavelengths, sd.wavelengths, sd.values)

        # Write CSV file with ALWAN_LITERAL formatting
        filename = f'{DATA_DIR}/fixtures/tcs_{i:02d}_reflectance.csv'
        ensure_dir(filename)
        with open(filename, 'w') as f:
            # Write 10 values per line for readability
            for j in range(0, len(resampled), 10):
                chunk = resampled[j:j+10]
                line = ','.join([f'ALWAN_LITERAL({r:.15f})' for r in chunk])
                if j + 10 < len(resampled):
                    f.write(line + ',\n')
                else:
                    f.write(line + '\n')

    print(f"  Generated {len(tcs_samples)} TCS reflectance files (360-830nm @ 5nm, 95 samples each)")

except Exception as e:
    print(f"  Warning: Could not generate TCS data: {e}", file=sys.stderr)

# ================================================================
# Generate VS (Vivid Saturated) samples for CQS
# ================================================================
print("\nGenerating VS (Vivid Saturated) samples for CQS...")

try:
    from colour.quality import SDS_VS

    # Get VS samples from CQS 9.0 dataset (15 saturated Munsell samples)
    cqs_dataset = SDS_VS.get('NIST CQS 9.0')
    vs_samples = list(cqs_dataset.values())

    # Target wavelength range: 360-830nm @ 5nm intervals (95 samples)
    target_wavelengths = np.arange(360, 835, 5)

    for i, sd in enumerate(vs_samples, start=1):
        # Interpolate to target wavelengths
        resampled = np.interp(target_wavelengths, sd.wavelengths, sd.values)

        # Write CSV file with ALWAN_LITERAL formatting
        filename = f'{DATA_DIR}/fixtures/vs_{i:02d}_reflectance.csv'
        ensure_dir(filename)
        with open(filename, 'w') as f:
            # Write 10 values per line for readability
            for j in range(0, len(resampled), 10):
                chunk = resampled[j:j+10]
                line = ','.join([f'ALWAN_LITERAL({r:.15f})' for r in chunk])
                if j + 10 < len(resampled):
                    f.write(line + ',\n')
                else:
                    f.write(line + '\n')

    print(f"  Generated {len(vs_samples)} VS reflectance files (360-830nm @ 5nm, 95 samples each)")

except Exception as e:
    print(f"  Warning: Could not generate VS data: {e}", file=sys.stderr)

# ================================================================
# Generate CES (Color Evaluation Samples) for TM-30 and CIE 224:2017
# ================================================================
print("\nGenerating CES (Color Evaluation Samples) for TM-30/CIE 224...")

try:
    # Path to the CIE 2017 TCS data (5nm resolution)
    # This is embedded in the colour-science package
    import os.path
    import colour

    # Try to find the CES data file
    colour_path = os.path.dirname(colour.__file__)
    data_file_5nm = os.path.join(colour_path, 'quality', 'datasets', 'tcs_cfi2017_5_nm.csv.gz')
    data_file_1nm = os.path.join(colour_path, 'quality', 'datasets', 'tcs_cfi2017_1_nm.csv.gz')

    # Use 5nm data if available, otherwise fall back to 1nm and resample
    if os.path.exists(data_file_5nm):
        data_file = data_file_5nm
    elif os.path.exists(data_file_1nm):
        data_file = data_file_1nm
    else:
        raise FileNotFoundError("Could not find CES dataset in colour-science package")

    # Read the gzipped CSV file
    with gzip.open(data_file, 'rt') as f:
        lines = f.readlines()

    # First line is header with wavelengths
    header = lines[0].strip().split(',')
    wavelengths = [float(wl) for wl in header[1:]]  # Skip first column (sample name)

    # Parse CES samples (skip header line)
    ces_samples = []
    for line in lines[1:]:
        parts = line.strip().split(',')
        values = [float(v) for v in parts[1:]]  # Skip sample name
        ces_samples.append(np.array(values))

    # Target wavelength range: 360-830nm @ 5nm intervals (95 samples)
    target_wavelengths = np.arange(360, 835, 5)

    for i, values in enumerate(ces_samples, start=1):
        # Interpolate to target wavelengths
        resampled = np.interp(target_wavelengths, wavelengths, values)

        # Write CSV file with ALWAN_LITERAL formatting
        filename = f'{DATA_DIR}/fixtures/ces_{i:02d}_reflectance.csv'
        ensure_dir(filename)
        with open(filename, 'w') as f:
            # Write 10 values per line for readability
            for j in range(0, len(resampled), 10):
                chunk = resampled[j:j+10]
                line = ','.join([f'ALWAN_LITERAL({r:.15f})' for r in chunk])
                if j + 10 < len(resampled):
                    f.write(line + ',\n')
                else:
                    f.write(line + '\n')

    print(f"  Generated {len(ces_samples)} CES reflectance files (360-830nm @ 5nm, 95 samples each)")

except Exception as e:
    print(f"  Warning: Could not generate CES data: {e}", file=sys.stderr)

# ================================================================
# Generate Camera Sensitivity Data
# ================================================================
print("\nGenerating Camera Sensitivity Data...")

try:
    # Target wavelength range (matching our SPD format)
    TARGET_WL_MIN = 360
    TARGET_WL_MAX = 830
    TARGET_WL_INTERVAL = 1

    # Create camera sensitivities directory
    os.makedirs(f'{DATA_DIR}/camera_sensitivities', exist_ok=True)

    # Available cameras in colour-science
    cameras = [
        ('Nikon 5100 (NPL)', 'nikon_5100'),
        ('Sigma SDMerill (NPL)', 'sigma_sdmerill')
    ]

    for camera_name, prefix in cameras:
        print(f"\n  Generating {camera_name}:")

        # Load camera sensitivity from colour-science
        msds = colour.MSDS_CAMERA_SENSITIVITIES[camera_name]

        print(f"    Source range: {msds.wavelengths[0]:.0f}-{msds.wavelengths[-1]:.0f}nm")
        print(f"    Source samples: {len(msds.wavelengths)}")

        # Create target wavelength array
        target_wl = np.arange(TARGET_WL_MIN, TARGET_WL_MAX + TARGET_WL_INTERVAL, TARGET_WL_INTERVAL)

        # Resample each channel (R, G, B)
        # msds.values is shape (n_wavelengths, 3) where columns are R, G, B
        r_values = msds.values[:, 0]
        g_values = msds.values[:, 1]
        b_values = msds.values[:, 2]

        # Interpolate to target wavelengths (with zero extrapolation)
        r_resampled = np.interp(target_wl, msds.wavelengths, r_values, left=0, right=0)
        g_resampled = np.interp(target_wl, msds.wavelengths, g_values, left=0, right=0)
        b_resampled = np.interp(target_wl, msds.wavelengths, b_values, left=0, right=0)

        # Save each channel
        for channel_name, values in [('r', r_resampled), ('g', g_resampled), ('b', b_resampled)]:
            output_file = f'{DATA_DIR}/camera_sensitivities/{prefix}_{channel_name}.csv'

            # Format as comma-separated values (to match our illuminant format)
            with open(output_file, 'w') as f:
                formatted_values = [format_scalar(v) for v in values]
                for i in range(0, len(formatted_values), 10):
                    chunk = formatted_values[i:i+10]
                    if i + 10 < len(formatted_values):
                        f.write(', '.join(chunk) + ',\n')
                    else:
                        f.write(', '.join(chunk))

            print(f"    {output_file} ({len(values)} samples)")

    print(f"  Camera sensitivity data generation complete!")

except Exception as e:
    print(f"  Warning: Could not generate camera sensitivity data: {e}", file=sys.stderr)
    import traceback
    traceback.print_exc(file=sys.stderr)

# ----------------------------------------------------------------
# Gamut Analysis & Mapping Data
# ----------------------------------------------------------------

print("")
print("=" * 80)
print("Gamut Analysis & Mapping")
print("=" * 80)

try:
    import os
    os.makedirs(f'{DATA_DIR}/gamut', exist_ok=True)

    # ============================================================
    # Pointer's Gamut Boundary
    # ============================================================
    print("\n1. Generating Pointer's Gamut boundary data...")

    # Get Pointer's Gamut boundary from colour-science
    from colour.models import CCS_POINTER_GAMUT_BOUNDARY
    pointer_boundary = CCS_POINTER_GAMUT_BOUNDARY
    print(f"  Found {len(pointer_boundary)} boundary points")

    # Save as pairs of x,y values
    output_file = f'{DATA_DIR}/gamut/pointer_gamut_boundary_xy.csv'
    with open(output_file, 'w') as f:
        for i, xy in enumerate(pointer_boundary):
            x_str = format_scalar(xy[0])
            y_str = format_scalar(xy[1])
            if i < len(pointer_boundary) - 1:
                f.write(f'{x_str}, {y_str},\n')
            else:
                f.write(f'{x_str}, {y_str}')

    print(f"  {output_file} ({len(pointer_boundary)} points)")

    # ============================================================
    # Spectral Locus from CIE 1931 2° CMFs
    # ============================================================
    print("\n2. Generating Spectral Locus xy data...")

    # Get CIE 1931 2° CMFs (360-830nm, 1nm interval)
    cmfs = colour.MSDS_CMFS['CIE 1931 2 Degree Standard Observer']
    wavelengths = cmfs.wavelengths
    xyz_values = cmfs.values

    # Compute xy chromaticity for each wavelength
    spectral_locus_data = []
    for wl_idx in range(len(wavelengths)):
        xyz = xyz_values[wl_idx]
        xyz_sum = np.sum(xyz)
        if xyz_sum > 0:
            x = xyz[0] / xyz_sum
            y = xyz[1] / xyz_sum
            spectral_locus_data.append((wavelengths[wl_idx], x, y))

    print(f"  Computed {len(spectral_locus_data)} spectral locus points")
    print(f"  Wavelength range: {spectral_locus_data[0][0]:.0f}-{spectral_locus_data[-1][0]:.0f}nm")

    # Save wavelength, x, y triplets
    output_file = f'{DATA_DIR}/gamut/spectral_locus_xy_360_830_1nm.csv'
    with open(output_file, 'w') as f:
        for i, (wl, x, y) in enumerate(spectral_locus_data):
            wl_str = format_scalar(wl)
            x_str = format_scalar(x)
            y_str = format_scalar(y)
            if i < len(spectral_locus_data) - 1:
                f.write(f'{wl_str}, {x_str}, {y_str},\n')
            else:
                f.write(f'{wl_str}, {x_str}, {y_str}')

    print(f"  {output_file} ({len(spectral_locus_data)} triplets)")

    # Also save just the xy values in a separate file for faster lookup
    output_file_xy = f'{DATA_DIR}/gamut/spectral_locus_xy_only_360_830_1nm.csv'
    with open(output_file_xy, 'w') as f:
        for i, (wl, x, y) in enumerate(spectral_locus_data):
            x_str = format_scalar(x)
            y_str = format_scalar(y)
            if i < len(spectral_locus_data) - 1:
                f.write(f'{x_str}, {y_str},\n')
            else:
                f.write(f'{x_str}, {y_str}')

    print(f"  {output_file_xy} (xy pairs only)")

    print(f"\n  Gamut data generation complete!")

except Exception as e:
    print(f"  Warning: Could not generate P9 gamut data: {e}", file=sys.stderr)
    import traceback
    traceback.print_exc(file=sys.stderr)

# ================================================================
# ColorChecker Reference Data
# ================================================================
print("\nGenerating ColorChecker reference data...")

try:
    from colour.characterisation import SDS_COLOURCHECKERS

    # ColorChecker Classic 24-patch data under D50
    print("  Processing ColorChecker Classic (24 patches)...")
    cc_classic = SDS_COLOURCHECKERS['ColorChecker N Ohta']

    # Get D50 illuminant
    d50_ill = colour.SDS_ILLUMINANTS['D50']

    # Convert each patch to XYZ under D50, then to xyY
    classic_xyy_values = []
    patches = list(cc_classic.values())

    for i, patch_sd in enumerate(patches):
        # Convert reflectance to XYZ under D50
        xyz = colour.sd_to_XYZ(patch_sd, illuminant=d50_ill) / 100.0

        # Convert XYZ to xyY
        xyy = colour.XYZ_to_xyY(xyz)
        classic_xyy_values.extend([xyy[0], xyy[1], xyy[2]])

    # Write ColorChecker Classic data (24 patches × 3 values = 72 values)
    output_file = f'{DATA_DIR}/colorchecker/classic_d50_xyy.csv'
    ensure_dir(output_file)
    with open(output_file, 'w') as f:
        formatted = [format_scalar(v) for v in classic_xyy_values]
        f.write(','.join(formatted))

    print(f"  {output_file} (24 patches, 72 xyY values)")

    # Try to get ColorChecker SG (140 patches)
    try:
        print("  Processing ColorChecker SG (140 patches)...")
        cc_sg = SDS_COLOURCHECKERS['ColorChecker24 - After November 2014']

        sg_xyy_values = []
        sg_patches = list(cc_sg.values())

        for patch_sd in sg_patches:
            xyz = colour.sd_to_XYZ(patch_sd, illuminant=d50_ill) / 100.0
            xyy = colour.XYZ_to_xyY(xyz)
            sg_xyy_values.extend([xyy[0], xyy[1], xyy[2]])

        output_file = f'{DATA_DIR}/colorchecker/sg_d50_xyy.csv'
        ensure_dir(output_file)
        with open(output_file, 'w') as f:
            formatted = [format_scalar(v) for v in sg_xyy_values]
            f.write(','.join(formatted))

        print(f"  {output_file} ({len(sg_patches)} patches, {len(sg_xyy_values)} xyY values)")
    except Exception as e_sg:
        print(f"  Note: ColorChecker SG not available in colour-science: {e_sg}")

    print("  ColorChecker data generation complete!")

except Exception as e:
    print(f"  Warning: Could not generate ColorChecker data: {e}", file=sys.stderr)
    import traceback
    traceback.print_exc(file=sys.stderr)

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
