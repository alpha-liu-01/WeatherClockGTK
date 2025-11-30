# WeatherClockGTK Build and Run Script
# This script cleans, builds, and optionally deploys the WeatherClockGTK application

param(
    [switch]$SkipClean,
    [switch]$SkipRun,
    [switch]$Deploy,           # Create deployable package with all DLLs
    [switch]$x86,              # Build 32-bit version using mingw32 toolchain
    [string]$DeployDir = "deploy",  # Deployment output directory
    [string]$Compiler = "",
    [string]$MSYS2Root = ""
)

$ErrorActionPreference = "Stop"

# Determine toolchain based on -x86 flag
if ($x86) {
    if ([string]::IsNullOrEmpty($Compiler)) {
        $Compiler = "C:/msys64/mingw32/bin/gcc.exe"
    }
    if ([string]::IsNullOrEmpty($MSYS2Root)) {
        $MSYS2Root = "C:/msys64/mingw32"
    }
    if ([string]::IsNullOrEmpty($DeployDir) -or $DeployDir -eq "deploy") {
        $DeployDir = "deploy32"
    }
    Write-Host "=== WeatherClockGTK Build Script (32-bit) ===" -ForegroundColor Cyan
} else {
    if ([string]::IsNullOrEmpty($Compiler)) {
        $Compiler = "C:/msys64/clang64/bin/clang.exe"
    }
    if ([string]::IsNullOrEmpty($MSYS2Root)) {
        $MSYS2Root = "C:/msys64/clang64"
    }
    Write-Host "=== WeatherClockGTK Build Script (64-bit) ===" -ForegroundColor Cyan
}
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
Write-Host "Using toolchain: $MSYS2Root" -ForegroundColor Gray

# Verify compiler exists
if (-not (Test-Path $Compiler)) {
    Write-Host "ERROR: Compiler not found at $Compiler" -ForegroundColor Red
    Write-Host "Please ensure MSYS2 is installed with the correct toolchain:" -ForegroundColor Yellow
    if ($x86) {
        Write-Host "  pacman -S mingw-w64-i686-toolchain mingw-w64-i686-gtk4 mingw-w64-i686-libsoup3 mingw-w64-i686-json-glib mingw-w64-i686-cmake mingw-w64-i686-ninja" -ForegroundColor White
    } else {
        Write-Host "  pacman -S mingw-w64-clang-x86_64-toolchain mingw-w64-clang-x86_64-gtk4 mingw-w64-clang-x86_64-libsoup3 mingw-w64-clang-x86_64-json-glib" -ForegroundColor White
    }
    exit 1
}

# Use CMake from the toolchain to ensure compatibility with toolchain binaries
$CMakePath = Join-Path $MSYS2Root "bin\cmake.exe"
$CMakePath = $CMakePath -replace '\\', '/'
if (-not (Test-Path $CMakePath)) {
    Write-Host "ERROR: CMake not found in toolchain: $CMakePath" -ForegroundColor Red
    Write-Host "Please install cmake for the target architecture:" -ForegroundColor Yellow
    if ($x86) {
        Write-Host "  pacman -S mingw-w64-i686-cmake" -ForegroundColor Cyan
    } else {
        Write-Host "  pacman -S mingw-w64-x86_64-cmake" -ForegroundColor Cyan
    }
    exit 1
}
Write-Host "Using CMake: $CMakePath" -ForegroundColor Gray

# Add the toolchain bin directory to PATH so tools can find their dependencies
$ToolchainBin = Join-Path $MSYS2Root "bin"
$MSYS2UsrBin = "C:\msys64\usr\bin"
$env:PATH = "$ToolchainBin;$MSYS2UsrBin;$env:PATH"
Write-Host "Added to PATH: $ToolchainBin" -ForegroundColor Gray

# Determine the correct build tool (ninja or make) from the same toolchain
$MakeProgramParam = @()
$NinjaPath = Join-Path $MSYS2Root "bin\ninja.exe"
# Normalize path separators for CMake (forward slashes)
$NinjaPath = $NinjaPath -replace '\\', '/'
if (Test-Path $NinjaPath) {
    $MakeProgramParam = @("-DCMAKE_MAKE_PROGRAM=$NinjaPath")
    Write-Host "Using build tool: $NinjaPath" -ForegroundColor Gray
} else {
    Write-Host "Warning: Ninja not found in toolchain, using default" -ForegroundColor Yellow
}

try {
    $CMakeArgs = @("-DCMAKE_C_COMPILER=$Compiler") + $MakeProgramParam + @("..")
    & $CMakePath @CMakeArgs
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
    & $CMakePath --build .
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

# Step 5: Deploy (optional)
if ($Deploy) {
    Write-Host "=== Deployment Stage ===" -ForegroundColor Cyan
    Write-Host ""
    
    # Go back to project root
    Set-Location $ScriptDir
    
    # Create deployment directory
    $DeployPath = Join-Path $ScriptDir $DeployDir
    Write-Host "Creating deployment directory: $DeployPath" -ForegroundColor Yellow
    if (Test-Path $DeployPath) {
        Remove-Item -Recurse -Force $DeployPath
    }
    New-Item -ItemType Directory -Force -Path $DeployPath | Out-Null
    
    # Copy executable
    $ExeSrc = Join-Path $ScriptDir "build\weatherclock.exe"
    if (Test-Path $ExeSrc) {
        Copy-Item $ExeSrc $DeployPath
        Write-Host "Copied: weatherclock.exe" -ForegroundColor Green
    } else {
        Write-Host "ERROR: Executable not found at $ExeSrc" -ForegroundColor Red
        exit 1
    }
    
    # Find DLL dependencies using ntldd (MSYS2 tool)
    Write-Host ""
    Write-Host "Finding DLL dependencies..." -ForegroundColor Yellow
    
    $NtlddPath = "$MSYS2Root\bin\ntldd.exe"
    $LddPath = "C:\msys64\usr\bin\ldd.exe"
    $MSYS2Bin = "$MSYS2Root\bin"
    
    # Get list of DLLs to copy
    $DllsToCopy = @()
    
    if (Test-Path $NtlddPath) {
        # Use ntldd to find dependencies
        $ntlddOutput = & $NtlddPath -R "$DeployPath\weatherclock.exe" 2>&1
        foreach ($line in $ntlddOutput) {
            if ($line -match "=>") {
                $parts = $line -split "=>"
                if ($parts.Count -ge 2) {
                    $dllPath = $parts[1].Trim() -replace "\s*\(.*\)$", ""
                    $dllPath = $dllPath.Trim()
                    if ($dllPath -and (Test-Path $dllPath)) {
                        # Only copy DLLs from MSYS2
                        if ($dllPath -like "*msys64*") {
                            $DllsToCopy += $dllPath
                        }
                    }
                }
            }
        }
    } else {
        Write-Host "ntldd not found, using fallback method..." -ForegroundColor Yellow
        
        # Fallback: copy commonly required GTK4 DLLs
        $CommonDlls = @(
            # Core GTK4
            "libgtk-4-1.dll",
            "libgdk_pixbuf-2.0-0.dll",
            "libgio-2.0-0.dll",
            "libglib-2.0-0.dll",
            "libgmodule-2.0-0.dll",
            "libgobject-2.0-0.dll",
            "libgraphene-1.0-0.dll",
            "libepoxy-0.dll",
            
            # Internationalization
            "libintl-8.dll",
            "libiconv-2.dll",
            
            # Pango (text rendering)
            "libpango-1.0-0.dll",
            "libpangocairo-1.0-0.dll",
            "libpangowin32-1.0-0.dll",
            "libpangoft2-1.0-0.dll",
            
            # Cairo (2D graphics)
            "libcairo-2.dll",
            "libcairo-gobject-2.dll",
            "libcairo-script-interpreter-2.dll",
            
            # Text shaping
            "libfribidi-0.dll",
            "libharfbuzz-0.dll",
            "libharfbuzz-subset-0.dll",
            
            # Fonts
            "libfontconfig-1.dll",
            "libfreetype-6.dll",
            
            # Image formats
            "libpixman-1-0.dll",
            "libpng16-16.dll",
            "libjpeg-8.dll",
            "libtiff-6.dll",
            "libwebp-7.dll",
            "libwebpdemux-2.dll",
            "libwebpmux-3.dll",
            "libsharpyuv-0.dll",
            "libLerc.dll",
            "libjbig-0.dll",
            "libdeflate.dll",
            
            # SVG support (for icons and pixbuf loader)
            "librsvg-2-2.dll",
            "libxml2-2.dll",
            "libcroco-0.6-3.dll",      # CSS parser for SVG
            "libgdk_pixbuf-2.0-0.dll", # Already listed above but needed here too
            
            # Compression
            "zlib1.dll",
            "libbz2-1.dll",
            "libbrotlidec.dll",
            "libbrotlicommon.dll",
            "liblzma-5.dll",
            "libzstd.dll",
            
            # Core dependencies
            "libexpat-1.dll",
            "libffi-8.dll",
            "libpcre2-8-0.dll",
            
            # C/C++ runtime (needed by libraries like libLerc)
            "libstdc++-6.dll",
            "libgcc_s_dw2-1.dll",      # 32-bit GCC runtime (DW2 exception handling)
            "libgcc_s_seh-1.dll",      # 64-bit GCC runtime (SEH exception handling)
            "libwinpthread-1.dll",
            
            # Network (libsoup)
            "libsoup-3.0-0.dll",
            "libjson-glib-1.0-0.dll",
            "libpsl-5.dll",
            "libsqlite3-0.dll",
            "libnghttp2-14.dll",
            
            # TLS/SSL support (architecture-specific, may not exist for both 32/64-bit)
            "libssl-3-x64.dll",        # 64-bit OpenSSL
            "libcrypto-3-x64.dll",     # 64-bit OpenSSL
            "libssl-3.dll",            # 32-bit OpenSSL (alternative naming)
            "libcrypto-3.dll",         # 32-bit OpenSSL (alternative naming)
            "libgnutls-30.dll",
            "libhogweed-6.dll",
            "libnettle-8.dll",
            "libgmp-10.dll",
            "libtasn1-6.dll",
            "libp11-kit-0.dll",
            "libidn2-0.dll",
            "libunistring-5.dll",
            
            # C++ runtime (clang)
            "libc++.dll",
            "libunwind.dll"

            # Other
            "libdatrie-1.dll",
            "libgraphite2.dll",
            "liblzo2-2.dll",
            "libthai-0.dll"
        )
        
        foreach ($dll in $CommonDlls) {
            $dllPath = Join-Path $MSYS2Bin $dll
            if (Test-Path $dllPath) {
                $DllsToCopy += $dllPath
            }
        }
    }
    
    # Also copy DLLs needed by pixbuf loaders (RECURSIVELY)
    Write-Host "Finding pixbuf loader dependencies..." -ForegroundColor Yellow
    $LoadersDir = "$MSYS2Root\lib\gdk-pixbuf-2.0\2.10.0\loaders"
    if (Test-Path $LoadersDir) {
        $loaderDlls = Get-ChildItem "$LoadersDir\*.dll" -ErrorAction SilentlyContinue
        foreach ($loader in $loaderDlls) {
            Write-Host "  Checking loader: $($loader.Name)" -ForegroundColor Gray
            # Try to find dependencies of each loader
            if (Test-Path $NtlddPath) {
                $loaderDeps = & $NtlddPath -R $loader.FullName 2>&1
                foreach ($line in $loaderDeps) {
                    if ($line -match "=>") {
                        $parts = $line -split "=>"
                        if ($parts.Count -ge 2) {
                            $dllPath = $parts[1].Trim() -replace "\s*\(.*\)$", ""
                            $dllPath = $dllPath.Trim()
                            if ($dllPath -and (Test-Path $dllPath) -and ($dllPath -like "*msys64*")) {
                                $DllsToCopy += $dllPath
                                Write-Host "    Will copy: $(Split-Path -Leaf $dllPath)" -ForegroundColor DarkGray
                            }
                        }
                    }
                }
            }
        }
    }
    
    # Copy DLLs
    $copiedCount = 0
    foreach ($dllPath in $DllsToCopy | Sort-Object -Unique) {
        $dllName = Split-Path -Leaf $dllPath
        $destPath = Join-Path $DeployPath $dllName
        if (-not (Test-Path $destPath)) {
            Copy-Item $dllPath $destPath
            Write-Host "  Copied: $dllName" -ForegroundColor Gray
            $copiedCount++
        }
    }
    Write-Host "Copied $copiedCount DLLs" -ForegroundColor Green
    
    # Copy GDK-Pixbuf loaders
    Write-Host ""
    Write-Host "Copying GDK-Pixbuf loaders..." -ForegroundColor Yellow
    $PixbufSrc = "$MSYS2Root\lib\gdk-pixbuf-2.0"
    $PixbufDest = "$DeployPath\lib\gdk-pixbuf-2.0"
    if (Test-Path $PixbufSrc) {
        New-Item -ItemType Directory -Force -Path $PixbufDest | Out-Null
        Copy-Item -Recurse -Force "$PixbufSrc\*" $PixbufDest
        Write-Host "Copied GDK-Pixbuf loaders" -ForegroundColor Green
    }
    
    # Copy GIO modules (CRITICAL for TLS/SSL support!)
    Write-Host ""
    Write-Host "Copying GIO modules (TLS support)..." -ForegroundColor Yellow
    $GioSrc = "$MSYS2Root\lib\gio\modules"
    $GioDest = "$DeployPath\lib\gio\modules"
    if (Test-Path $GioSrc) {
        New-Item -ItemType Directory -Force -Path $GioDest | Out-Null
        Copy-Item -Force "$GioSrc\*.dll" $GioDest -ErrorAction SilentlyContinue
        Write-Host "Copied GIO modules" -ForegroundColor Green
        
        # Find and copy dependencies of GIO modules RECURSIVELY
        $gioModules = Get-ChildItem "$GioSrc\*.dll" -ErrorAction SilentlyContinue
        $gioDepsAdded = @{}
        foreach ($module in $gioModules) {
            Write-Host "  Found GIO module: $($module.Name)" -ForegroundColor Gray
            if (Test-Path $NtlddPath) {
                $moduleDeps = & $NtlddPath -R $module.FullName 2>&1
                foreach ($line in $moduleDeps) {
                    if ($line -match "=>") {
                        $parts = $line -split "=>"
                        if ($parts.Count -ge 2) {
                            $dllPath = $parts[1].Trim() -replace "\s*\(.*\)$", ""
                            $dllPath = $dllPath.Trim()
                            if ($dllPath -and (Test-Path $dllPath) -and ($dllPath -like "*msys64*")) {
                                $dllName = Split-Path -Leaf $dllPath
                                if (-not $gioDepsAdded.ContainsKey($dllName)) {
                                    $destPath = Join-Path $DeployPath $dllName
                                    if (-not (Test-Path $destPath)) {
                                        Copy-Item $dllPath $destPath
                                        Write-Host "    Copied dependency: $dllName" -ForegroundColor Gray
                                    }
                                    $gioDepsAdded[$dllName] = $true
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        Write-Host "WARNING: GIO modules not found at $GioSrc" -ForegroundColor Yellow
    }
    
    # Copy SSL certificates
    Write-Host ""
    Write-Host "Copying SSL certificates..." -ForegroundColor Yellow
    $SslCertSrc = "$MSYS2Root\etc\ssl\certs\ca-bundle.crt"
    $SslDest = "$DeployPath\ssl\certs"
    if (Test-Path $SslCertSrc) {
        New-Item -ItemType Directory -Force -Path $SslDest | Out-Null
        Copy-Item -Force $SslCertSrc $SslDest
        Write-Host "Copied SSL CA bundle" -ForegroundColor Green
    } else {
        # Try alternate locations
        $AltPaths = @(
            "$MSYS2Root\etc\pki\ca-trust\extracted\pem\tls-ca-bundle.pem",
            "C:\msys64\etc\ssl\certs\ca-bundle.crt",
            "C:\msys64\usr\ssl\certs\ca-bundle.crt"
        )
        $found = $false
        foreach ($altPath in $AltPaths) {
            if (Test-Path $altPath) {
                New-Item -ItemType Directory -Force -Path $SslDest | Out-Null
                Copy-Item -Force $altPath "$SslDest\ca-bundle.crt"
                Write-Host "Copied SSL CA bundle from $altPath" -ForegroundColor Green
                $found = $true
                break
            }
        }
        if (-not $found) {
            Write-Host "WARNING: SSL certificates not found - HTTPS may not work!" -ForegroundColor Yellow
        }
    }
    
    # Copy GLib schemas
    Write-Host ""
    Write-Host "Copying GLib schemas..." -ForegroundColor Yellow
    $SchemasSrc = "$MSYS2Root\share\glib-2.0\schemas"
    $SchemasDest = "$DeployPath\share\glib-2.0\schemas"
    if (Test-Path $SchemasSrc) {
        New-Item -ItemType Directory -Force -Path $SchemasDest | Out-Null
        Copy-Item -Force "$SchemasSrc\gschemas.compiled" $SchemasDest -ErrorAction SilentlyContinue
        Write-Host "Copied GLib schemas" -ForegroundColor Green
    }
    
    # Copy GTK4 settings
    Write-Host ""
    Write-Host "Copying GTK4 settings..." -ForegroundColor Yellow
    $Gtk4Src = "$MSYS2Root\share\gtk-4.0"
    $Gtk4Dest = "$DeployPath\share\gtk-4.0"
    if (Test-Path $Gtk4Src) {
        New-Item -ItemType Directory -Force -Path $Gtk4Dest | Out-Null
        Copy-Item -Recurse -Force "$Gtk4Src\*" $Gtk4Dest
        Write-Host "Copied GTK4 settings" -ForegroundColor Green
    }
    
    # Copy Adwaita icons (essential for GTK4)
    Write-Host ""
    Write-Host "Copying icons (this may take a moment)..." -ForegroundColor Yellow
    $IconsSrc = "$MSYS2Root\share\icons"
    $IconsDest = "$DeployPath\share\icons"
    if (Test-Path $IconsSrc) {
        New-Item -ItemType Directory -Force -Path $IconsDest | Out-Null
        # Copy only essential icon themes
        $IconThemes = @("Adwaita", "hicolor")
        foreach ($theme in $IconThemes) {
            $themeSrc = "$IconsSrc\$theme"
            if (Test-Path $themeSrc) {
                Copy-Item -Recurse -Force $themeSrc "$IconsDest\"
                Write-Host "  Copied icon theme: $theme" -ForegroundColor Gray
            }
        }
        Write-Host "Copied icons" -ForegroundColor Green
    }
    
    # Create a launcher script
    Write-Host ""
    Write-Host "Creating launcher script..." -ForegroundColor Yellow
    $LauncherContent = @"
@echo off
cd /d "%~dp0"
set GDK_PIXBUF_MODULE_FILE=%~dp0lib\gdk-pixbuf-2.0\2.10.0\loaders.cache
set GIO_MODULE_DIR=%~dp0lib\gio\modules
set GSETTINGS_SCHEMA_DIR=%~dp0share\glib-2.0\schemas
set GTK_DATA_PREFIX=%~dp0
set XDG_DATA_DIRS=%~dp0share
set SSL_CERT_DIR=%~dp0ssl\certs
set SSL_CERT_FILE=%~dp0ssl\certs\ca-bundle.crt
set GIO_EXTRA_MODULES=%~dp0lib\gio\modules
weatherclock.exe %*
"@
    $LauncherContent | Out-File -FilePath "$DeployPath\run.bat" -Encoding ASCII
    Write-Host "Created run.bat launcher" -ForegroundColor Green
    
    # Update the pixbuf loaders cache
    Write-Host ""
    Write-Host "Updating pixbuf loaders cache..." -ForegroundColor Yellow
    $PixbufQueryLoaders = "$MSYS2Root\bin\gdk-pixbuf-query-loaders.exe"
    $LoadersDir = "$DeployPath\lib\gdk-pixbuf-2.0\2.10.0\loaders"
    $CacheFile = "$DeployPath\lib\gdk-pixbuf-2.0\2.10.0\loaders.cache"
    if ((Test-Path $PixbufQueryLoaders) -and (Test-Path $LoadersDir)) {
        $loaderDlls = Get-ChildItem "$LoadersDir\*.dll" | ForEach-Object { $_.FullName }
        if ($loaderDlls) {
            & $PixbufQueryLoaders $loaderDlls > $CacheFile 2>&1
            # Fix paths in cache to be relative
            $cacheContent = Get-Content $CacheFile -Raw
            $cacheContent = $cacheContent -replace [regex]::Escape($DeployPath), "."
            $cacheContent | Out-File -FilePath $CacheFile -Encoding UTF8 -NoNewline
            Write-Host "Updated loaders cache" -ForegroundColor Green
        }
    }
    
    # Calculate deployment size
    $deploySize = (Get-ChildItem -Recurse $DeployPath | Measure-Object -Property Length -Sum).Sum / 1MB
    
    Write-Host ""
    Write-Host "=== Deployment Complete ===" -ForegroundColor Cyan
    Write-Host "Location: $DeployPath" -ForegroundColor White
    Write-Host "Size: $([math]::Round($deploySize, 2)) MB" -ForegroundColor White
    Write-Host ""
    Write-Host "To run the deployed app:" -ForegroundColor Yellow
    Write-Host "  cd $DeployDir" -ForegroundColor White
    Write-Host "  .\run.bat" -ForegroundColor White
    Write-Host ""
    
    # Return to build directory for potential run
    Set-Location "$ScriptDir\build"
}

# Step 6: Find and run the executable
if (-not $SkipRun -and -not $Deploy) {
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
        $Clang64Bin = "$MSYS2Root\bin"
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
} elseif (-not $Deploy) {
    Write-Host "Skipping launch (-SkipRun specified)" -ForegroundColor Gray
    Write-Host ""
    Write-Host "To run manually:" -ForegroundColor Yellow
    Write-Host "  .\build\weatherclock.exe" -ForegroundColor White
}

Write-Host ""
Write-Host "=== Done ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Usage examples:" -ForegroundColor Gray
Write-Host "  .\build.ps1                    # Build and run (64-bit)" -ForegroundColor Gray
Write-Host "  .\build.ps1 -SkipRun           # Build only (64-bit)" -ForegroundColor Gray
Write-Host "  .\build.ps1 -Deploy            # Build and create deployment package (64-bit)" -ForegroundColor Gray
Write-Host "  .\build.ps1 -x86               # Build and run (32-bit)" -ForegroundColor Gray
Write-Host "  .\build.ps1 -x86 -Deploy       # Build and create deployment package (32-bit)" -ForegroundColor Gray
Write-Host "  .\build.ps1 -Deploy -SkipClean # Deploy without cleaning" -ForegroundColor Gray

