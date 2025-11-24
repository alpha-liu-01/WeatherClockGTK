# WeatherClockGTK Launcher Script
# This script ensures DLLs are found from PATH before launching the application

param(
    [string[]]$Arguments = @()
)

$ErrorActionPreference = "Stop"

Write-Host "=== WeatherClockGTK Launcher ===" -ForegroundColor Cyan
Write-Host ""

# Get the script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

# Add MSYS2 clang64 bin to PATH if not already present
$Clang64Bin = "C:\msys64\clang64\bin"
if (Test-Path $Clang64Bin) {
    $CurrentPath = $env:PATH
    if ($CurrentPath -notlike "*$Clang64Bin*") {
        Write-Host "Adding clang64 bin to PATH..." -ForegroundColor Yellow
        $env:PATH = "$Clang64Bin;$env:PATH"
    } else {
        Write-Host "clang64 bin already in PATH" -ForegroundColor Gray
    }
} else {
    Write-Host "Warning: clang64 bin directory not found at $Clang64Bin" -ForegroundColor Yellow
    Write-Host "Make sure MSYS2 clang64 is installed and PATH is configured correctly." -ForegroundColor Yellow
}

Write-Host ""

# Find the executable
$exePath = $null
if (Test-Path "build\weatherclock.exe") {
    $exePath = "build\weatherclock.exe"
} elseif (Test-Path "build\weatherclock") {
    $exePath = "build\weatherclock"
} elseif (Test-Path "weatherclock.exe") {
    $exePath = "weatherclock.exe"
} elseif (Test-Path "weatherclock") {
    $exePath = "weatherclock"
}

if (-not $exePath) {
    Write-Host "Error: Executable not found!" -ForegroundColor Red
    Write-Host "Please build the project first using: .\build.ps1" -ForegroundColor Yellow
    exit 1
}

Write-Host "Found executable: $exePath" -ForegroundColor Green
Write-Host "Launching application..." -ForegroundColor Cyan
Write-Host ""

# Run the executable with any provided arguments
& $exePath $Arguments

$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    Write-Host ""
    Write-Host "Application exited with code: $exitCode" -ForegroundColor Yellow
}

exit $exitCode

