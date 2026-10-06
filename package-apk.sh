#!/bin/sh
# Alpine apk builder for WeatherClockGTK
# Targets postmarketOS / Alpine Linux aarch64 (arm64).
# Run this on the device (ash is enough; bash is not required).

set -e

# Colors
RED=$(printf '\033[0;31m')
GREEN=$(printf '\033[0;32m')
YELLOW=$(printf '\033[1;33m')
NC=$(printf '\033[0m')

# Configuration
PACKAGE_NAME="weatherclockgtk"
PKGREL="0"
MAINTAINER="WeatherClockGTK Maintainer <maintainer@example.com>"
DESCRIPTION="GTK4-based clock and weather application for repurposed tablets"
URL="https://github.com/alpha-liu-01/WeatherClockGTK"

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"

if [ -f /etc/os-release ]; then
    # shellcheck disable=SC1091
    . /etc/os-release
fi

VERSION=$(sed -n 's/^project(WeatherClockGTK VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt)
if [ -z "$VERSION" ]; then
    printf '%b\n' "${RED}Error: could not read the project version from CMakeLists.txt${NC}"
    exit 1
fi

TARBALL_NAME="${PACKAGE_NAME}-${VERSION}.tar.gz"
APK_NAME="${PACKAGE_NAME}-${VERSION}-r${PKGREL}.apk"
APK_OUT="${PACKAGE_NAME}-${VERSION}-r${PKGREL}.aarch64.apk"

if command -v doas >/dev/null 2>&1; then
    ROOTCMD="doas"
elif command -v sudo >/dev/null 2>&1; then
    ROOTCMD="sudo"
else
    ROOTCMD="doas"
fi

printf '%s\n' "=== Alpine Package Builder ==="
printf '%s\n' "Package: $PACKAGE_NAME"
printf '%s\n' "Version: $VERSION-r$PKGREL"
printf '%s\n' "OS: ${PRETTY_NAME:-unknown}"
printf '%s\n' "Architecture: $(uname -m)"
printf '\n'

if ! command -v apk >/dev/null 2>&1; then
    printf '%b\n' "${RED}Error: apk was not found${NC}"
    printf '%s\n' "This script must be run on postmarketOS or Alpine Linux aarch64."
    printf '%s\n' "Copy the project to the device, then run:"
    printf '%s\n' "  $ROOTCMD apk add alpine-sdk cmake gtk4.0-dev libsoup3-dev json-glib-dev"
    printf '%s\n' "  ./package-apk.sh"
    exit 1
fi

if [ "$(uname -m)" != "aarch64" ]; then
    printf '%b\n' "${RED}Error: unsupported architecture: $(uname -m)${NC}"
    printf '%s\n' "This package is built for postmarketOS arm64 (aarch64)."
    exit 1
fi

if [ "$(id -u)" -eq 0 ]; then
    printf '%b\n' "${RED}Error: do not run this script as root${NC}"
    printf '%s\n' "abuild only runs as a normal user. It uses $ROOTCMD when it needs to install build dependencies."
    exit 1
fi

if ! command -v abuild >/dev/null 2>&1 || ! command -v abuild-keygen >/dev/null 2>&1; then
    printf '%b\n' "${RED}Error: abuild is not installed${NC}"
    printf '%s\n' "Install the Alpine SDK with:"
    printf '%s\n' "  $ROOTCMD apk add alpine-sdk"
    exit 1
fi

printf '%s\n' "Checking build dependencies..."
MISSING=""

if ! command -v cmake >/dev/null 2>&1; then
    MISSING="$MISSING cmake"
fi

if ! command -v gcc >/dev/null 2>&1; then
    MISSING="$MISSING build-base"
fi

if ! command -v pkg-config >/dev/null 2>&1; then
    MISSING="$MISSING pkgconf"
else
    if ! pkg-config --exists gtk4; then
        MISSING="$MISSING gtk4.0-dev"
    fi

    if ! pkg-config --exists libsoup-3.0; then
        MISSING="$MISSING libsoup3-dev"
    fi

    if ! pkg-config --exists json-glib-1.0; then
        MISSING="$MISSING json-glib-dev"
    fi
fi

if [ -n "$MISSING" ]; then
    printf '%b\n' "${YELLOW}Missing build dependencies:${NC}"
    for dep in $MISSING; do
        printf '%s\n' "  - $dep"
    done
    printf '\n'
    printf '%s\n' "Install them with:"
    printf '%s\n' "  $ROOTCMD apk add$MISSING"
    printf '%s\n' "gtk4.0-dev is in the community repository. Enable community in /etc/apk/repositories if apk cannot find it."
    exit 1
fi

for required in \
    CMakeLists.txt \
    LICENSE \
    src/main.c \
    data/com.weatherclock.app.desktop \
    data/icons/com.weatherclock.app.png
do
    if [ ! -f "$required" ]; then
        printf '%b\n' "${RED}Error: required file not found: $required${NC}"
        exit 1
    fi
done

printf '%b\n' "${GREEN}All dependencies found${NC}"
printf '\n'

if [ -f "$HOME/.abuild/abuild.conf" ]; then
    # shellcheck disable=SC1091
    . "$HOME/.abuild/abuild.conf"
fi

if [ -z "${PACKAGER:-}" ]; then
    PACKAGER=$MAINTAINER
fi
export PACKAGER

if [ -z "${PACKAGER_PRIVKEY:-}" ] || [ ! -f "$PACKAGER_PRIVKEY" ]; then
    printf '%s\n' "No abuild signing key found. Creating one in $HOME/.abuild ..."
    abuild-keygen -a -n
    printf '\n'
fi

printf '%s\n' "Creating Alpine package structure..."
rm -rf alpine packages
rm -f "$APK_OUT"
mkdir -p alpine

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT INT TERM

mkdir -p "$STAGE/$PACKAGE_NAME-$VERSION"
cp CMakeLists.txt LICENSE "$STAGE/$PACKAGE_NAME-$VERSION/"
cp -a src data "$STAGE/$PACKAGE_NAME-$VERSION/"
# Drop local build products so the archive stays source-only.
find "$STAGE" -type f \( -name '*.o' -o -name '*.obj' -o -name '*.d' \) -exec rm -f {} \;
tar -C "$STAGE" -czf "alpine/$TARBALL_NAME" "$PACKAGE_NAME-$VERSION"

SUM_LINE=$(cd alpine && sha512sum "$TARBALL_NAME")

cat > "alpine/$PACKAGE_NAME.post-install" << 'EOF'
#!/bin/sh

if [ -x /usr/bin/update-desktop-database ]; then
    update-desktop-database /usr/share/applications >/dev/null 2>&1 || true
fi

if [ -x /usr/bin/gtk-update-icon-cache ] && [ -d /usr/share/icons/hicolor ]; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor >/dev/null 2>&1 || true
fi

exit 0
EOF

cp "alpine/$PACKAGE_NAME.post-install" "alpine/$PACKAGE_NAME.post-upgrade"
cp "alpine/$PACKAGE_NAME.post-install" "alpine/$PACKAGE_NAME.post-deinstall"
chmod +x \
    "alpine/$PACKAGE_NAME.post-install" \
    "alpine/$PACKAGE_NAME.post-upgrade" \
    "alpine/$PACKAGE_NAME.post-deinstall"

cat > alpine/APKBUILD << EOF
# Contributor: $MAINTAINER
# Maintainer: $MAINTAINER
pkgname=$PACKAGE_NAME
pkgver=$VERSION
pkgrel=$PKGREL
pkgdesc="$DESCRIPTION"
url="$URL"
arch="aarch64"
license="MIT"
depends="gtk4.0 libsoup3 json-glib hicolor-icon-theme desktop-file-utils gtk-update-icon-cache"
makedepends="cmake build-base pkgconf gtk4.0-dev libsoup3-dev json-glib-dev"
install="\$pkgname.post-install \$pkgname.post-upgrade \$pkgname.post-deinstall"
source="$TARBALL_NAME"
options="!check"

build() {
    cmake -S "\$builddir" -B "\$builddir"/build \\
        -DCMAKE_BUILD_TYPE=Release \\
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build "\$builddir"/build --parallel
}

package() {
    DESTDIR="\$pkgdir" cmake --install "\$builddir"/build
    install -Dm644 "\$builddir"/LICENSE \\
        "\$pkgdir"/usr/share/licenses/\$pkgname/LICENSE
}

sha512sums="
$SUM_LINE
"
EOF

printf '\n'
printf '%s\n' "Building Alpine package..."
printf '\n'

if ! (
    cd alpine
    REPODEST="$ROOT/packages" abuild -r
); then
    printf '%b\n' "${RED}Package build failed${NC}"
    exit 1
fi

BUILT="alpine/$APK_NAME"
if [ ! -f "$BUILT" ]; then
    BUILT=$(find packages -type f -name "$APK_NAME" 2>/dev/null | head -n 1)
fi

if [ -n "$BUILT" ] && [ -f "$BUILT" ]; then
    cp "$BUILT" "$APK_OUT"
    printf '\n'
    printf '%b\n' "${GREEN}=== Package built successfully! ===${NC}"
    printf '\n'
    printf '%s\n' "Package file: $ROOT/$APK_OUT"
    printf '%s\n' "Package size: $(du -h "$APK_OUT" | cut -f1)"
    printf '\n'
    printf '%s\n' "To install:"
    printf '%s\n' "  $ROOTCMD apk add --allow-untrusted $ROOT/$APK_OUT"
    printf '\n'
    printf '%s\n' "To trust this signing key so later installs do not need --allow-untrusted:"
    printf '%s\n' "  $ROOTCMD cp ~/.abuild/*.rsa.pub /etc/apk/keys/"
    printf '%s\n' "  $ROOTCMD apk add $ROOT/$APK_OUT"
    printf '\n'
    printf '%s\n' "Simplified Chinese text needs a CJK font. Install one if glyphs are missing:"
    printf '%s\n' "  $ROOTCMD apk add font-noto-cjk"
else
    printf '%b\n' "${RED}Package build failed: $APK_NAME was not produced${NC}"
    exit 1
fi
