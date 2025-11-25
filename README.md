# WeatherClockGTK

A GTK4-based clock and weather application designed for repurposed PC-based tablets. Features a large, prominent clock display with hourly weather forecasts below.

![Screenshot](https://i.imgur.com/ZWMw3wZ.png)

## Features

- **Large Clock Display**: Takes up most of the window with current time and date
- **Hourly Weather Forecast**: Shows the next 6 hours of weather data
- **Fullscreen Mode**: Optimized for tablet displays
- **Auto-refresh**: Clock updates every second, weather updates every minute
- **Cross-platform**: Works on Windows (via MSYS2) and Linux

## Requirements

### Windows (MSYS2 clang64)

Install the following packages in your MSYS2 clang64 terminal:

```bash
pacman -S mingw-w64-clang-x86_64-gtk4 \
          mingw-w64-clang-x86_64-toolchain \
          mingw-w64-clang-x86_64-pkg-config \
          mingw-w64-clang-x86_64-glib2 \
          mingw-w64-clang-x86_64-cairo \
          mingw-w64-clang-x86_64-pango \
          mingw-w64-clang-x86_64-gdk-pixbuf2 \
          mingw-w64-clang-x86_64-libsoup \
          mingw-w64-clang-x86_64-json-glib
```

### Windows (MSYS2 clang64) - CMake

For CMake builds, also install:

```bash
pacman -S mingw-w64-clang-x86_64-cmake
```

### Linux (Ubuntu/Debian)

```bash
sudo apt-get install libgtk-4-dev libsoup-3.0-dev libjson-glib-dev pkg-config build-essential cmake
```

### Linux (Fedora)

```bash
sudo dnf install gtk4-devel libsoup3-devel json-glib-devel pkg-config gcc cmake
```

## Building

### Quick Build Scripts

#### Windows (PowerShell Script)

The easiest way to build and run on Windows:

```powershell
.\build.ps1
```

This script will:
- Clean the build directory
- Configure with CMake (using clang64 compiler)
- Build the project
- Launch the executable

**Options:**
- `.\build.ps1 -SkipClean` - Skip cleaning the build directory
- `.\build.ps1 -SkipRun` - Build but don't launch the executable
- `.\build.ps1 -Deploy` - Build and create deployment package
- `.\build.ps1 40.7128 -74.0060` - Pass arguments to the executable (e.g., location coordinates)

**Running the Application:**

After building, you can run the application using:

```powershell
.\run.ps1
```

Or with location arguments:

```powershell
.\run.ps1 40.7128 -74.0060
```

The `run.ps1` script automatically ensures that MSYS2 clang64 DLLs are found from PATH, so you don't need to manually set environment variables. For distribution, you'll need to copy the required DLLs to the executable's directory (see Distribution section below).

#### Linux (Bash Script)

The easiest way to build and run on Linux:

```bash
./build.sh
```

This script will:
- Check for required dependencies
- Clean the build directory
- Configure with CMake
- Build the project
- Launch the executable

**Options:**
- `./build.sh --skip-clean` - Skip cleaning the build directory
- `./build.sh --skip-run` - Build but don't launch the executable
- `./build.sh --deploy` - Create deployment package
- `./build.sh --help` - Show help message

**Running the Application:**

After building, you can run the application using:

```bash
./build/weatherclock
```

Or with location arguments:

```bash
./build/weatherclock 40.7128 -74.0060
```

### Manual Build with CMake

Create a build directory and configure:

**For Windows (MSYS2 clang64):**

Make sure you're using the clang64 terminal, then:

```bash
mkdir build
cd build
cmake -DCMAKE_C_COMPILER=C:/msys64/clang64/bin/clang.exe ..
```

Or if you're in the clang64 environment, CMake should auto-detect clang. If it doesn't, explicitly specify it:

```bash
cmake -DCMAKE_C_COMPILER=clang ..
```

**For Linux:**

```bash
mkdir build
cd build
cmake ..
```

Then build:

```bash
cmake --build .
```

This will create the `weatherclock` executable (or `weatherclock.exe` on Windows) in the `build` directory.

**Note:** If CMake is using the wrong compiler (e.g., mingw64 instead of clang64), you can explicitly specify it:
```bash
cmake -DCMAKE_C_COMPILER=C:/msys64/clang64/bin/clang.exe ..
```

To clean build artifacts:

```bash
cd build
cmake --build . --target clean
```

Or simply delete the `build` directory:

```bash
rm -rf build
```

### Alternative: Using Makefile

If you prefer the traditional Makefile approach, you can still use:

```bash
make
make clean
```

## Usage

Run the application:

**If built with CMake:**
```bash
./build/weatherclock
```

**If built with Makefile:**
```bash
./weatherclock
```

Or on Windows:

```bash
./build/weatherclock.exe
# or
./weatherclock.exe
```

### Command Line Arguments

You can specify a custom location by providing latitude and longitude:

```bash
./weatherclock 40.7128 -74.0060  # New York City
./weatherclock 51.5074 -0.1278   # London
```

Default location is Berlin, Germany (52.52, 13.41).

### Controls

- The application starts in fullscreen mode
- Press `F11` or `Alt+F4` to exit (depending on your window manager)
- The clock updates every second
- Weather data refreshes every minute

## Distribution

### Windows Deployment

The application depends on several DLLs from MSYS2 clang64. When running from the build scripts (`build.ps1` or `run.ps1`), the clang64 bin directory is automatically added to PATH, so DLLs are found automatically.

**For development:**
- Ensure `C:\msys64\clang64\bin` is in your system PATH, or use the provided scripts

**For distribution:**
- Use `.\build.ps1 -Deploy` to create a deployment package with all required DLLs
- The deployment script automatically collects all dependencies, GDK-Pixbuf loaders, GIO modules, SSL certificates, and creates a `run.bat` launcher
- The `deploy/` directory contains everything needed to run on a fresh Windows installation

### Linux Deployment

#### Debian Package (.deb)

To create a Debian package for amd64 or arm64:

```bash
./package-deb.sh
```

This script will:
- Check for build dependencies
- Create a proper Debian package structure
- Build a `.deb` package compatible with your architecture
- Output the package file in the parent directory

**Installing the package:**

```bash
sudo dpkg -i weatherclockgtk_1.0.0_amd64.deb
# or
sudo dpkg -i weatherclockgtk_1.0.0_arm64.deb
```

If dependencies are missing:

```bash
sudo apt-get install -f
```

**Building for different architectures:**

The package script automatically detects your current architecture. To build for a different architecture, you'll need to use cross-compilation tools or build on a system with that architecture.

**Package dependencies:**

The `.deb` package includes runtime dependencies:
- `libgtk-4-1`
- `libsoup-3.0-0`
- `libjson-glib-1.0-0`
- `libglib2.0-0`
- Standard C library

These will be automatically installed when installing the package.

## Weather API

This application uses the [Open-Meteo API](https://open-meteo.com/), which is:
- **Free**: No API key required for non-commercial use
- **Open-source**: Community-driven weather data
- **Reliable**: Uses data from national weather services

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

**Note**: This application uses GTK4 and related libraries (libsoup, json-glib, GLib) which are licensed under LGPL 2.1+. These libraries are dynamically linked and their licenses do not affect the licensing of this application's source code. 
