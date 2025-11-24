# WeatherClockGTK Build and Run Script
# This script cleans, builds, and runs the WeatherClockGTK application

param(
    [switch]$SkipClean,
    [switch]$SkipRun,
    [string]$Compiler = "C:/msys64/clang64/bin/clang.exe"
)

$ErrorActionPreference = "Stop"

Write-Host "=== WeatherClockGTK Build Script ===" -ForegroundColor Cyan
Write-Host ""

# Get the script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

# Step 1: Clean build directory
if (-not $SkipClean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build"
        Write-Host "Build directory removed." -ForegroundColor Green
    } else {
        Write-Host "Build directory does not exist, skipping clean." -ForegroundColor Gray
    }
    Write-Host ""
}

# Step 2: Create build directory
Write-Host "Creating build directory..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path "build" | Out-Null
Set-Location "build"
Write-Host "Build directory created." -ForegroundColor Green
Write-Host ""

# Step 3: Configure with CMake
Write-Host "Configuring with CMake..." -ForegroundColor Yellow
Write-Host "Using compiler: $Compiler" -ForegroundColor Gray

try {
    cmake -DCMAKE_C_COMPILER="$Compiler" ..
    if ($LASTEXITCODE -ne 0) {
        Write-Host "CMake configuration failed!" -ForegroundColor Red
        exit 1
    }
    Write-Host "CMake configuration successful." -ForegroundColor Green
} catch {
    Write-Host "Error during CMake configuration: $_" -ForegroundColor Red
    exit 1
}
Write-Host ""

# Step 4: Build the project
Write-Host "Building project..." -ForegroundColor Yellow
try {
    cmake --build .
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        exit 1
    }
    Write-Host "Build successful!" -ForegroundColor Green
} catch {
    Write-Host "Error during build: $_" -ForegroundColor Red
    exit 1
}
Write-Host ""

# Step 5: Find and run the executable
if (-not $SkipRun) {
    Write-Host "Looking for executable..." -ForegroundColor Yellow
    
    $exePath = $null
    if (Test-Path "weatherclock.exe") {
        $exePath = "weatherclock.exe"
    } elseif (Test-Path "weatherclock") {
        $exePath = "weatherclock"
    }
    
    if ($exePath) {
        Write-Host "Found executable: $exePath" -ForegroundColor Green
        
        # Add MSYS2 clang64 bin to PATH for DLL resolution
        $Clang64Bin = "C:\msys64\clang64\bin"
        if (Test-Path $Clang64Bin) {
            $CurrentPath = $env:PATH
            if ($CurrentPath -notlike "*$Clang64Bin*") {
                Write-Host "Adding clang64 bin to PATH for DLL resolution..." -ForegroundColor Yellow
                $env:PATH = "$Clang64Bin;$env:PATH"
            }
        }
        
        Write-Host ""
        Write-Host "Launching application..." -ForegroundColor Cyan
        Write-Host ""
        
        # Run the executable
        & ".\$exePath" $args
    } else {
        Write-Host "Executable not found!" -ForegroundColor Red
        Write-Host "Expected: weatherclock.exe or weatherclock" -ForegroundColor Yellow
        exit 1
    }
} else {
    Write-Host "Skipping launch (--SkipRun specified)" -ForegroundColor Gray
    Write-Host ""
    Write-Host "To run manually:" -ForegroundColor Yellow
    Write-Host "  .\build\weatherclock.exe" -ForegroundColor White
    if ($args.Count -gt 0) {
        Write-Host "  With arguments: $args" -ForegroundColor White
    }
}

Write-Host ""
Write-Host "=== Done ===" -ForegroundColor Cyan

