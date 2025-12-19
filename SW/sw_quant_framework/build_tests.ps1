param(
    [string]$Test = "index_generator",
    [switch]$Clean,
    [switch]$Help
)

if ($Help) {
    Write-Host "Test Build Script for Golden Reference Tests"
    Write-Host "Usage: .\build_tests.ps1 [-Test test_name] [-Clean] [-Help]"
    Write-Host ""
    Write-Host "Tests:"
    Write-Host "  index_generator        Build and run test_index_generator"
    Write_Host "  accelerator_model      Build and run test_accelerator_model"
    Write-Host "  complete_pipeline      Build and run test_complete_pipeline"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Clean                 Clean build directory before building"
    Write-Host "  -Help                  Show this help message"
    exit 0
}

$BuildDir = "build_tests"
$TestsDir = "src\goldenReference"
$SrcDir = "src"

# Create build directory
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Test 1: index_generator
if ($Test -eq "index_generator") {
    Write-Host "Compiling index_generator test..." -ForegroundColor Cyan
    
    $sourceFiles = @(
        "$TestsDir\test_index_generator.cpp",
        "$TestsDir\IndexGenerator.cpp"
    )
    
    $output = "$BuildDir\test_index_generator.exe"
    
    & g++ -std=c++11 -Wall -O2 -I$SrcDir -I$TestsDir @sourceFiles -o $output
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`nCompiled successfully" -ForegroundColor Green
        Write-Host "`nRunning test..." -ForegroundColor Cyan
        Write-Host "========================================" -ForegroundColor Gray
        & $output
        Write-Host "========================================" -ForegroundColor Gray
        Write-Host "`nTest complete" -ForegroundColor Green
    }
    else {
        Write-Host " Compilation failed" -ForegroundColor Red
        exit 1
    }
}
# Test 2: accelerator_model
elseif ($Test -eq "accelerator_model") {
    Write-Host "Compiling accelerator_model test..." -ForegroundColor Cyan
    
    $sourceFiles = @(
        "$TestsDir\test_accelerator_model.cpp",
        "$TestsDir\IndexGenerator.cpp",
        "$TestsDir\Dequantization.cpp"
    )
    
    $output = "$BuildDir\test_accelerator_model.exe"
    
    & g++ -std=c++11 -Wall -O2 -I$SrcDir -I$TestsDir @sourceFiles -o $output
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`nCompiled successfully" -ForegroundColor Green
        Write-Host "`nRunning test..." -ForegroundColor Cyan
        Write-Host "========================================" -ForegroundColor Gray
        & $output
        Write-Host "========================================" -ForegroundColor Gray
        Write-Host "`nTest complete" -ForegroundColor Green
    }
    else {
        Write-Host " Compilation failed" -ForegroundColor Red
        exit 1
    }
}
# Test 3: complete_pipeline
elseif ($Test -eq "complete_pipeline") {
    Write-Host "Compiling complete_pipeline test..." -ForegroundColor Cyan
    
    $sourceFiles = @(
        "$TestsDir\test_complete_pipeline.cpp",
        "$TestsDir\IndexGenerator.cpp",
        "$TestsDir\StagedMAC.cpp",
        "$TestsDir\Dequantization.cpp",
        "$TestsDir\OutputStorage.cpp"
    )
    
    $output = "$BuildDir\test_complete_pipeline.exe"
    
    & g++ -std=c++11 -Wall -O2 -I$SrcDir -I$TestsDir @sourceFiles -o $output
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`nCompiled successfully" -ForegroundColor Green
        Write-Host "`nRunning test..." -ForegroundColor Cyan
        Write-Host "========================================" -ForegroundColor Gray
        & $output
        Write-Host "========================================" -ForegroundColor Gray
        Write-Host "`nTest complete" -ForegroundColor Green
    }
    else {
        Write-Host " Compilation failed" -ForegroundColor Red
        exit 1
    }
}
else {
    Write-Host "Unknown test: $Test" -ForegroundColor Red
    Write-Host "Available tests: index_generator, accelerator_model, complete_pipeline"
}
