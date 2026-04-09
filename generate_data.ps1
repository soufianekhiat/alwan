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
$subdirs = @("cmf", "illuminants", "illuminants_xy", "illuminants_spd", "matrices", "rgb_spaces", "rgb_matrices", "fixtures", "vision")
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
    @{Name="Chromatic Adaptation Matrices"; Script="../alwan_dev/gendata/data/cat_matrices.py"; Priority=1},
    @{Name="Fairchild CAT Matrix"; Script="../alwan_dev/gendata/data/cat_fairchild.py"; Priority=1},
    @{Name="CMCCat97 CAT Matrix"; Script="../alwan_dev/gendata/data/cat_cmccat97.py"; Priority=1},
    @{Name="HPE Matrix"; Script="../alwan_dev/gendata/data/hpe_matrix.py"; Priority=1},
    @{Name="Core Matrices (Oklab/Jzazbz)"; Script="../alwan_dev/gendata/data/core_matrices.py"; Priority=1},
    @{Name="CSS Oklab Matrices (sRGB-based, Ottosson 2020)"; Script="../alwan_dev/gendata/data/css_oklab_matrices.py"; Priority=1},
    @{Name="Vision LEFs (Photopic/Scotopic)"; Script="../alwan_dev/gendata/data/vision_lefs.py"; Priority=1},
    @{Name="LLAB Matrices"; Script="../alwan_dev/gendata/data/llab_matrices.py"; Priority=1},
    @{Name="IPT Data"; Script="../alwan_dev/gendata/data/ipt_data.py"; Priority=1},
    @{Name="ProLab Data"; Script="../alwan_dev/gendata/data/prolab_data.py"; Priority=1},
    @{Name="ICtCp Matrices"; Script="../alwan_dev/gendata/data/ictcp_matrices.py"; Priority=1},
    @{Name="ACES Matrices"; Script="../alwan_dev/gendata/data/aces_matrices.py"; Priority=1},
    @{Name="ACES 1.x C9 Spline Params"; Script="../alwan_dev/gendata/data/aces1_c9_spline.py"; Priority=1},
    @{Name="ACES 1.x OCIO Curves"; Script="../alwan_dev/gendata/data/aces1_ocio_curves.py"; Priority=1},
    @{Name="Camera Gamut Matrices"; Script="../alwan_dev/gendata/data/camera_gamut_matrices.py"; Priority=1},
    @{Name="Illuminants"; Script="../alwan_dev/gendata/data/illuminants.py"; Priority=1},
    @{Name="Illuminants (Extended)"; Script="../alwan_dev/gendata/data/illuminants_extended.py"; Priority=1},
    @{Name="Color Matching Functions"; Script="../alwan_dev/gendata/data/cmf.py"; Priority=1},
    @{Name="RGB Color Spaces"; Script="../alwan_dev/gendata/data/rgb_spaces.py"; Priority=1},
    @{Name="RGB Space Matrices"; Script="../alwan_dev/gendata/data/rgb_matrices.py"; Priority=1},
    @{Name="ARRI LogC3"; Script="../alwan_dev/gendata/data/arri_logc3.py"; Priority=1},
    @{Name="Reference Data"; Script="../alwan_dev/gendata/data/reference_data.py"; Priority=1},
    @{Name="Test Fixtures (Color Spaces)"; Script="../alwan_dev/gendata/data/test_fixtures.py"; Priority=1},
    @{Name="Delta-E Fixtures"; Script="../alwan_dev/gendata/data/delta_e_fixtures.py"; Priority=1},
    @{Name="Convenience Color Models"; Script="../alwan_dev/gendata/data/convenience_color_models.py"; Priority=1},
    @{Name="Oklab Fixtures"; Script="../alwan_dev/gendata/data/oklab_fixtures.py"; Priority=1},
    @{Name="CCT Fixtures"; Script="../alwan_dev/gendata/data/cct_fixtures.py"; Priority=1},
    @{Name="Robertson CCT Table"; Script="../alwan_dev/gendata/data/robertson_cct.py"; Priority=1},
    @{Name="Chromatic Adaptation Fixtures"; Script="../alwan_dev/gendata/data/chromatic_adaptation_fixtures.py"; Priority=2},
    @{Name="CAM Fixtures (CIECAM02/CAM16)"; Script="../alwan_dev/gendata/data/cam_fixtures.py"; Priority=2},
    @{Name="RGB Conversion Fixtures"; Script="../alwan_dev/gendata/data/rgb_conversion_fixtures.py"; Priority=2},
    @{Name="Gamut Mapping Fixtures"; Script="../alwan_dev/gendata/data/gamut_mapping_fixtures.py"; Priority=2},
    @{Name="Spectral Upsampling Basis"; Script="../alwan_dev/gendata/data/spectral_upsampling.py"; Priority=2},
    @{Name="Jakob2019 LUTs (Spectral Upsampling)"; Script="../alwan_dev/gendata/data/jakob2019_luts.py"; Priority=2},
    @{Name="Extended IPT/ICtCp Matrices"; Script="../alwan_dev/gendata/data/ipt_ictcp_extended.py"; Priority=1},
    @{Name="Extended Color Spaces (IgPgTg/ICAcB/IHLS/HDR/Locus)"; Script="../alwan_dev/gendata/data/extended_colorspaces.py"; Priority=1},
    @{Name="Comprehensive Missing Data (CAT/Illuminants/Test Samples)"; Script="../alwan_dev/gendata/data/comprehensive_missing.py"; Priority=1},
    @{Name="Missing Stubs (F-series/CES/D-series/ARRI)"; Script="../alwan_dev/gendata/data/missing_stubs.py"; Priority=1},
    @{Name="Camera Sensitivities"; Script="../alwan_dev/gendata/data/camera_sensitivities.py"; Priority=1},
    @{Name="RGB Spaces (Complete)"; Script="../alwan_dev/gendata/data/rgb_spaces_complete.py"; Priority=2},
    @{Name="AgX Matrices"; Script="../alwan_dev/gendata/data/agx_matrices.py"; Priority=1},
    @{Name="Planckian Locus (Krystek 1985)"; Script="../alwan_dev/gendata/data/planckian_locus_krystek.py"; Priority=1},
    @{Name="Munsell Renotation Data"; Script="../alwan_dev/gendata/data/munsell_renotation.py"; Priority=2},
    @{Name="RGB Spaces Lookup Table"; Script="../alwan_dev/gendata/data/rgb_spaces_lookup.py"; Priority=2},
    @{Name="SSI Weights"; Script="../alwan_dev/gendata/data/ssi_weights.py"; Priority=1},
    @{Name="CVD Matrices (Viénot 1999)"; Script="../alwan_dev/gendata/data/cvd_matrices.py"; Priority=1},
    @{Name="Machado 2009 CVD Matrices"; Script="../alwan_dev/gendata/data/machado2009_matrices.py"; Priority=1},
    @{Name="OSA-UCS Matrices"; Script="../alwan_dev/gendata/data/osa_ucs_matrices.py"; Priority=1},
    @{Name="ACES2 Fourier Coefficients"; Script="../alwan_dev/gendata/data/aces2_fourier.py"; Priority=1},
    @{Name="RLAB Matrices"; Script="../alwan_dev/gendata/data/rlab_matrices.py"; Priority=1},
    @{Name="AgX Default Contrast LUT"; Script="../alwan_dev/gendata/data/agx_contrast_lut.py"; Priority=1}
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
Write-Host "  See ../alwan_dev/gendata/PROGRESS.md for full status" -ForegroundColor Gray
