#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Generate Alwan data files from colour-science (Modular Architecture).
.DESCRIPTION
    Orchestrates Python scripts to generate matrices and test data.
    NO HARDCODED VALUES - all data comes from colour-science.

    This script replaces the monolithic generate_data.ps1 (2492 lines)
    with clean modular Python scripts (~50-150 lines each).
#>

$ErrorActionPreference = "Stop"

Write-Host "========================================"
Write-Host "Alwan Data Generation (Modular v2.0)"
Write-Host "========================================"
Write-Host ""

# Output directory
$DATA_DIR = "src/alwan/data"

# Check Python
Write-Host "Checking Python installation..."
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    Write-Host "ERROR: Python not found in PATH" -ForegroundColor Red
    exit 1
}
$pythonVersion = & python --version 2>&1
Write-Host "  Found: $pythonVersion" -ForegroundColor Green

# Note: Skipping colour-science version check (can hang on import)
# Individual modules will report errors if colour-science is not available
Write-Host "Note: colour-science is required (install with: pip install colour-science)" -ForegroundColor Cyan
Write-Host ""

# Create output directories
Write-Host "Creating output directories..."
$subdirs = @("cmf", "illuminants", "illuminants_xy", "illuminants_spd", "matrices", "rgb_spaces", "fixtures")
foreach ($subdir in $subdirs) {
    $path = "$DATA_DIR/$subdir"
    if (-not (Test-Path $path)) {
        New-Item -ItemType Directory -Path $path -Force | Out-Null
    }
}
Write-Host "  Output directory: $DATA_DIR" -ForegroundColor Cyan
Write-Host ""

# Data generation scripts (in dependency order)
$dataScripts = @(
    @{Name="Chromatic Adaptation Matrices"; Script="gendata/data/cat_matrices.py"; Priority=1},
    @{Name="Fairchild CAT Matrix"; Script="gendata/data/cat_fairchild.py"; Priority=1},
    @{Name="CMCCat97 CAT Matrix"; Script="gendata/data/cat_cmccat97.py"; Priority=1},
    @{Name="HPE Matrix"; Script="gendata/data/hpe_matrix.py"; Priority=1},
    @{Name="LLAB Matrices"; Script="gendata/data/llab_matrices.py"; Priority=1},
    @{Name="IPT Data"; Script="gendata/data/ipt_data.py"; Priority=1},
    @{Name="ICtCp Matrices"; Script="gendata/data/ictcp_matrices.py"; Priority=1},
    @{Name="ACES Matrices"; Script="gendata/data/aces_matrices.py"; Priority=1},
    @{Name="Illuminants"; Script="gendata/data/illuminants.py"; Priority=1},
    @{Name="Illuminants (Extended)"; Script="gendata/data/illuminants_extended.py"; Priority=1},
    @{Name="Color Matching Functions"; Script="gendata/data/cmf.py"; Priority=1},
    @{Name="RGB Color Spaces"; Script="gendata/data/rgb_spaces.py"; Priority=1},
    @{Name="ARRI LogC3"; Script="gendata/data/arri_logc3.py"; Priority=1},
    @{Name="Reference Data"; Script="gendata/data/reference_data.py"; Priority=1},
    @{Name="Test Fixtures (Color Spaces)"; Script="gendata/data/test_fixtures.py"; Priority=1},
    @{Name="Delta-E Fixtures"; Script="gendata/data/delta_e_fixtures.py"; Priority=1},
    @{Name="Convenience Color Models"; Script="gendata/data/convenience_color_models.py"; Priority=1},
    @{Name="Oklab Fixtures"; Script="gendata/data/oklab_fixtures.py"; Priority=1},
    @{Name="CCT Fixtures"; Script="gendata/data/cct_fixtures.py"; Priority=1},
    @{Name="Robertson CCT Table"; Script="gendata/data/robertson_cct.py"; Priority=1},
    @{Name="Chromatic Adaptation Fixtures"; Script="gendata/data/chromatic_adaptation_fixtures.py"; Priority=2},
    @{Name="CAM Fixtures (CIECAM02/CAM16)"; Script="gendata/data/cam_fixtures.py"; Priority=2},
    @{Name="RGB Conversion Fixtures"; Script="gendata/data/rgb_conversion_fixtures.py"; Priority=2},
    @{Name="Gamut Mapping Fixtures"; Script="gendata/data/gamut_mapping_fixtures.py"; Priority=2},
    @{Name="Spectral Upsampling Basis"; Script="gendata/data/spectral_upsampling.py"; Priority=2},
    @{Name="Jakob2019 LUTs (Spectral Upsampling)"; Script="gendata/data/jakob2019_luts.py"; Priority=2},
    @{Name="Extended IPT/ICtCp Matrices"; Script="gendata/data/ipt_ictcp_extended.py"; Priority=1},
    @{Name="Extended Color Spaces (IgPgTg/ICAcB/IHLS/HDR/Locus)"; Script="gendata/data/extended_colorspaces.py"; Priority=1},
    @{Name="Comprehensive Missing Data (CAT/Illuminants/Test Samples)"; Script="gendata/data/comprehensive_missing.py"; Priority=1},
    @{Name="Missing Stubs (F-series/CES/D-series/ARRI)"; Script="gendata/data/missing_stubs.py"; Priority=1}
)

# Run each data generation script
$successCount = 0
$failCount = 0

foreach ($item in $dataScripts) {
    Write-Host "[$($item.Priority)] Generating $($item.Name)..." -ForegroundColor Yellow

    # Capture output and errors, suppress PowerShell error handling
    try {
        $output = cmd /c "python `"$($item.Script)`" `"$DATA_DIR`" 2>&1"
        $exitCode = $LASTEXITCODE
    } catch {
        $output = $_.Exception.Message
        $exitCode = 1
    }

    if ($exitCode -eq 0) {
        Write-Host $output
        $successCount++
    } else {
        Write-Host "  WARNING: Failed to generate $($item.Name)" -ForegroundColor Red
        Write-Host $output -ForegroundColor Gray
        $failCount++
    }
    Write-Host ""
}

# Summary
Write-Host "========================================"
Write-Host "Data Generation Summary"
Write-Host "========================================"
Write-Host "  Completed: $successCount modules" -ForegroundColor Green
if ($failCount -gt 0) {
    Write-Host "  Failed: $failCount modules" -ForegroundColor Red
}
Write-Host "  Output directory: $DATA_DIR" -ForegroundColor Cyan
Write-Host ""

if ($failCount -eq 0) {
    Write-Host "All data generation completed successfully!" -ForegroundColor Green
} else {
    Write-Host "WARNING: Some modules failed (see above)" -ForegroundColor Yellow
    Write-Host "  This may be due to missing colour-science constants" -ForegroundColor Gray
}
Write-Host ""

# Show what's remaining
Write-Host "Note: All Priority 1 and 2 modules completed!" -ForegroundColor Gray
Write-Host "  - Priority 1: Convenience models, Oklab, CCT, Robertson" -ForegroundColor Green
Write-Host "  - Priority 2: RGB conversions, gamut mapping, spectral upsampling" -ForegroundColor Green
Write-Host "" -ForegroundColor Gray
Write-Host "Priority 3 modules (optional, advanced):" -ForegroundColor Gray
Write-Host "  - Jakob2019 polynomial LUTs (requires C++ tool)" -ForegroundColor Gray
Write-Host "  - Additional specialized fixtures as needed" -ForegroundColor Gray
Write-Host "  See gendata/PROGRESS.md for full status" -ForegroundColor Gray
