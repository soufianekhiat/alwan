#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Generate Alwan test data from colour-science (Modular Architecture).
.DESCRIPTION
    Orchestrates Python scripts to generate test reference data.
    Test inputs are hardcoded, expected outputs come from colour-science.
#>

$ErrorActionPreference = "Stop"

Write-Host "========================================"
Write-Host "Alwan Test Data Generation (Modular v2.0)"
Write-Host "========================================"
Write-Host ""

# Output directory
$TEST_DIR = "tests/reference_values"

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

# Ensure output directory exists
if (-not (Test-Path $TEST_DIR)) {
    New-Item -ItemType Directory -Path $TEST_DIR -Force | Out-Null
}
Write-Host "  Output directory: $TEST_DIR" -ForegroundColor Cyan
Write-Host ""

# Test generation scripts (colour-science based)
$testScripts = @(
    @{Name="Test Reference Values"; Script="gendata/data/test_reference_values.py"},
    @{Name="Hellwig2022"; Script="gendata/tests/hellwig2022.py"},
    @{Name="Kim2009"; Script="gendata/tests/kim2009.py"},
    @{Name="LLAB"; Script="gendata/tests/llab.py"},
    @{Name="ATD95"; Script="gendata/tests/atd95.py"},
    @{Name="Zhai2018 CAM-UCS Delta E"; Script="gendata/tests/zhai2018_cam_ucs_delta_e.py"},
    @{Name="Polynomial Color Correction"; Script="gendata/tests/polynomial_color_correction.py"},
    @{Name="Rayleigh Scattering"; Script="gendata/tests/rayleigh_scattering.py"},
    @{Name="Barten 1999 CSF"; Script="gendata/tests/barten1999_csf.py"}
)

# ACES/OCIO test generation scripts (output to tests/reference_values/)
$acesScripts = @(
    @{Name="ACES Fixed Functions (RedMod, Glow, DarkToDim, GamutComp13)"; Script="gendata/tests/aces_fixed_functions.py"},
    @{Name="ACES 2.0 Section5/Section9 (TonescaleCompress, RGB-JMh, GamutCompress)"; Script="gendata/tests/section5_section9.py"},
    @{Name="ACES 2.0 TonescaleCompress Pipeline"; Script="gendata/tests/aces_tonescale_compress20.py"},
    @{Name="ACES 2.0 GamutCompress Pipeline"; Script="gendata/tests/aces_gamut_compress20.py"},
    @{Name="ACES 2.0 Output Transform (all presets)"; Script="gendata/tests/aces2_output_transform.py"},
    @{Name="ACES 1.x Output Transform (RRT+ODT)"; Script="gendata/tests/aces1_output_transform.py"},
    @{Name="CCT and Cineon"; Script="gendata/tests/cct_cineon.py"}
)

# Run CAM fixtures generator separately (outputs to parent dir)
Write-Host "Generating CAM fixtures (CIECAM02, CAM16)..." -ForegroundColor Yellow
try {
    $output = cmd /c "python gendata/data/cam_fixtures.py tests 2>&1"
    $exitCode = $LASTEXITCODE
    if ($exitCode -eq 0) {
        Write-Host $output
        Write-Host "  [OK] CAM fixtures generated" -ForegroundColor Green
    } else {
        Write-Host "  WARNING: Failed to generate CAM fixtures" -ForegroundColor Red
        Write-Host $output -ForegroundColor Gray
    }
} catch {
    Write-Host "  WARNING: Failed to generate CAM fixtures: $($_.Exception.Message)" -ForegroundColor Red
}
Write-Host ""

# Run each test generation script
$successCount = 0
$failCount = 0

foreach ($item in $testScripts) {
    Write-Host "Generating $($item.Name) test data..." -ForegroundColor Yellow

    # Capture output and errors, suppress PowerShell error handling
    try {
        $output = cmd /c "python `"$($item.Script)`" `"$TEST_DIR`" 2>&1"
        $exitCode = $LASTEXITCODE
    } catch {
        $output = $_.Exception.Message
        $exitCode = 1
    }

    if ($exitCode -eq 0) {
        Write-Host $output
        $successCount++
    } else {
        Write-Host "  WARNING: Failed to generate $($item.Name) test data" -ForegroundColor Red
        Write-Host $output -ForegroundColor Gray
        $failCount++
    }
    Write-Host ""
}

# Run ACES/OCIO test generation scripts (require PyOpenColorIO)
Write-Host ""
Write-Host "========================================"
Write-Host "ACES/OCIO Test Data Generation"
Write-Host "========================================"
Write-Host "Note: These scripts require PyOpenColorIO (install with: pip install PyOpenColorIO)" -ForegroundColor Cyan
Write-Host "Output directory: tests/reference_values/" -ForegroundColor Cyan
Write-Host ""

$acesSuccessCount = 0
$acesFailCount = 0

foreach ($item in $acesScripts) {
    Write-Host "Generating $($item.Name)..." -ForegroundColor Yellow

    # ACES scripts don't take directory argument - they hardcode their output paths
    try {
        $output = cmd /c "python `"$($item.Script)`" 2>&1"
        $exitCode = $LASTEXITCODE
    } catch {
        $output = $_.Exception.Message
        $exitCode = 1
    }

    if ($exitCode -eq 0) {
        Write-Host $output
        $acesSuccessCount++
    } else {
        Write-Host "  WARNING: Failed to generate $($item.Name)" -ForegroundColor Red
        Write-Host $output -ForegroundColor Gray
        $acesFailCount++
    }
    Write-Host ""
}

# Summary
Write-Host "========================================"
Write-Host "Test Data Generation Summary"
Write-Host "========================================"
Write-Host "  colour-science modules: $successCount completed" -ForegroundColor Green
if ($failCount -gt 0) {
    Write-Host "  colour-science modules: $failCount failed" -ForegroundColor Red
}
Write-Host "  ACES/OCIO modules: $acesSuccessCount completed" -ForegroundColor Green
if ($acesFailCount -gt 0) {
    Write-Host "  ACES/OCIO modules: $acesFailCount failed" -ForegroundColor Red
}
Write-Host "  Output directory: $TEST_DIR" -ForegroundColor Cyan
Write-Host ""

$totalFailed = $failCount + $acesFailCount
if ($totalFailed -eq 0) {
    Write-Host "All test data generation completed successfully!" -ForegroundColor Green
} else {
    Write-Host "WARNING: Some test modules failed (see above)" -ForegroundColor Yellow
}
Write-Host ""

# Show what's remaining
Write-Host "Note: All test generators completed!" -ForegroundColor Gray
Write-Host "  - Test Reference Values: Hunter Lab, white points, Delta E, etc." -ForegroundColor Green
Write-Host "  - Test-specific: Hellwig2022, Kim2009, LLAB, ATD95, Zhai2018/CAM-UCS" -ForegroundColor Green
Write-Host "  - Polynomial Color Correction: Cheung2004, Finlayson2015, Vandermonde" -ForegroundColor Green
Write-Host "  - Fixture-based: CIECAM02, CAM16 (generated by cam_fixtures.py)" -ForegroundColor Green
Write-Host "  - ACES/OCIO: Fixed functions, ACES 2.0 components, Output Transform" -ForegroundColor Green
Write-Host "  See gendata/PROGRESS.md for full status" -ForegroundColor Gray
