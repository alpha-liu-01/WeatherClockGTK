#!/bin/bash
# Debian Package Builder for WeatherClockGTK
# Supports both x86_64 (amd64) and arm64 architectures

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Configuration
PACKAGE_NAME="weatherclockgtk"
VERSION="1.0.0"
MAINTAINER="WeatherClockGTK Maintainer <maintainer@example.com>"
DESCRIPTION="GTK4-based clock and weather application for repurposed tablets"
ARCHITECTURES="amd64 arm64"

# Detect current architecture
CURRENT_ARCH=$(dpkg --print-architecture)
if [ "$CURRENT_ARCH" != "amd64" ] && [ "$CURRENT_ARCH" != "arm64" ]; then
    echo -e "${RED}Error: Unsupported architecture: $CURRENT_ARCH${NC}"
    echo "Supported architectures: amd64, arm64"
    exit 1
fi

echo "=== Debian Package Builder ==="
echo "Package: $PACKAGE_NAME"
echo "Version: $VERSION"
echo "Architecture: $CURRENT_ARCH"
echo ""

# Check for required tools
if ! command -v dpkg-buildpackage &> /dev/null; then
    echo -e "${RED}Error: dpkg-buildpackage is not installed${NC}"
    echo "Install with: sudo apt-get install devscripts"
    exit 1
fi

# Check for dh (provided by debhelper)
if ! command -v dh &> /dev/null; then
    echo -e "${RED}Error: debhelper (dh) is not installed${NC}"
    echo "Install with: sudo apt-get install debhelper"
    exit 1
fi

# Check for build dependencies
echo "Checking build dependencies..."
MISSING_DEPS=()

if ! pkg-config --exists gtk4; then
    MISSING_DEPS+=("libgtk-4-dev")
fi

if ! pkg-config --exists libsoup-3.0; then
    MISSING_DEPS+=("libsoup-3.0-dev")
fi

if ! pkg-config --exists json-glib-1.0; then
    MISSING_DEPS+=("json-glib-dev")
fi

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo -e "${YELLOW}Missing build dependencies:${NC}"
    for dep in "${MISSING_DEPS[@]}"; do
        echo "  - $dep"
    done
    echo ""
    echo "Install them with:"
    echo "  sudo apt-get install ${MISSING_DEPS[*]} build-essential cmake pkg-config"
    exit 1
fi

echo -e "${GREEN}All dependencies found${NC}"
echo ""

# Create debian directory structure
echo "Creating Debian package structure..."
rm -rf debian
mkdir -p debian

# Create debian/control
cat > debian/control << EOF
Source: $PACKAGE_NAME
Section: utils
Priority: optional
Maintainer: $MAINTAINER
Build-Depends: debhelper (>= 13),
               cmake (>= 3.16),
               pkg-config,
               libgtk-4-dev,
               libsoup-3.0-dev,
               libjson-glib-dev,
               build-essential
Standards-Version: 4.6.0
Homepage: https://github.com/yourusername/WeatherClockGTK

Package: $PACKAGE_NAME
Architecture: amd64 arm64
Depends: \${shlibs:Depends}, \${misc:Depends},
         libgtk-4-1,
         libsoup-3.0-0,
         libjson-glib-1.0-0,
         libglib2.0-0,
         libc6
Description: $DESCRIPTION
 WeatherClockGTK is a GTK4-based clock and weather application designed
 for repurposed PC-based tablets. It features a large, prominent clock
 display with hourly weather forecasts below.
 .
 Features:
  - Large clock display with current time and date
  - Hourly weather forecast (next 6 hours)
  - Customizable location (latitude/longitude)
  - Persistent location storage
  - Auto-refresh: clock updates every second, weather updates hourly
  - Cross-platform support (Windows and Linux)
EOF

# Create debian/rules
cat > debian/rules << 'EOF'
#!/usr/bin/make -f

%:
	dh $@

override_dh_auto_configure:
	mkdir -p build
	cd build && cmake -DCMAKE_INSTALL_PREFIX=/usr ..

override_dh_auto_build:
	cd build && cmake --build . -j$(nproc)

override_dh_auto_install:
	cd build && DESTDIR=$(CURDIR)/debian/weatherclockgtk cmake --install .
EOF

chmod +x debian/rules

# Create debian/changelog
cat > debian/changelog << EOF
$PACKAGE_NAME ($VERSION) unstable; urgency=medium

  * Initial release
  * GTK4-based clock and weather application
  * Supports Linux (amd64 and arm64)

 -- $MAINTAINER  $(date -R)
EOF

# Create debian/compat
echo "13" > debian/compat

# Create debian/postinst (optional post-installation script)
cat > debian/postinst << 'EOF'
#!/bin/bash
set -e

# Update desktop database if desktop file exists
if [ -f /usr/share/applications/weatherclockgtk.desktop ]; then
    update-desktop-database /usr/share/applications/ 2>/dev/null || true
fi

exit 0
EOF

chmod +x debian/postinst

# Create debian/postrm (optional post-removal script)
cat > debian/postrm << 'EOF'
#!/bin/bash
set -e

# Update desktop database if desktop file exists
if [ -f /usr/share/applications/weatherclockgtk.desktop ]; then
    update-desktop-database /usr/share/applications/ 2>/dev/null || true
fi

exit 0
EOF

chmod +x debian/postrm

# Build the package
echo ""
echo "Building Debian package..."
echo ""

# Clean any previous builds
rm -rf build
rm -f ../${PACKAGE_NAME}_*.deb ../${PACKAGE_NAME}_*.dsc ../${PACKAGE_NAME}_*.tar.gz ../${PACKAGE_NAME}_*.changes

# Build package
dpkg-buildpackage -b -uc -us

if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}=== Package built successfully! ===${NC}"
    echo ""
    
    # Find the built package
    DEB_FILE=$(ls -1 ../${PACKAGE_NAME}_${VERSION}_${CURRENT_ARCH}.deb 2>/dev/null | head -1)
    
    if [ -n "$DEB_FILE" ]; then
        echo "Package file: $DEB_FILE"
        SIZE=$(du -h "$DEB_FILE" | cut -f1)
        echo "Package size: $SIZE"
        echo ""
        echo "To install:"
        echo "  sudo dpkg -i $DEB_FILE"
        echo ""
        echo "To fix dependencies if needed:"
        echo "  sudo apt-get install -f"
    else
        echo -e "${YELLOW}Warning: Could not find built .deb file${NC}"
    fi
else
    echo -e "${RED}Package build failed${NC}"
    exit 1
fi

