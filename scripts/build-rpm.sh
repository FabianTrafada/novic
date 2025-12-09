#!/bin/bash
set -e

# Build RPM package for Novic
# Usage: ./scripts/build-rpm.sh [version]

VERSION="${1:-0.1.0}"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

echo "=== Building Novic RPM Package v${VERSION} ==="

# Check if on RPM-based distro
if ! command -v rpmbuild &>/dev/null; then
    echo "rpmbuild not found. Install with:"
    echo "  Fedora: sudo dnf install rpm-build rpmdevtools"
    echo "  openSUSE: sudo zypper install rpm-build"
    exit 1
fi

# Check dependencies
echo "Checking dependencies..."
if command -v dnf &>/dev/null; then
    DEPS="cmake gcc-c++ pkg-config gtkmm30-devel pulseaudio-libs-devel"
    for dep in $DEPS; do
        if ! rpm -q "$dep" &>/dev/null; then
            echo "Missing: $dep"
            echo "Install with: sudo dnf install $DEPS"
            exit 1
        fi
    done
fi

# Check for gtk-layer-shell (may be built from source)
if ! pkg-config --exists gtk-layer-shell-0; then
    echo "Warning: gtk-layer-shell not found via pkg-config"
    echo "If you built it from source, make sure it's installed and PKG_CONFIG_PATH is set"
    read -p "Continue anyway? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Build project
echo "Building project..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Setup RPM build environment
echo "Setting up RPM build environment..."
rpmdev-setuptree 2>/dev/null || true

# Create spec file
cat > ~/rpmbuild/SPECS/novic.spec << EOF
Name:           novic
Version:        ${VERSION}
Release:        1%{?dist}
Summary:        Dynamic Island-style media player widget for Wayland

License:        MIT
URL:            https://github.com/yourusername/novic

Requires:       gtkmm30, pulseaudio-libs

%description
A sleek, floating media player widget that displays currently playing
media with real-time audio visualization. Features hover-to-expand
controls similar to Apple's Dynamic Island.

Note: Requires gtk-layer-shell (may need to be built from source).

%install
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/usr/share/applications
cp ${BUILD_DIR}/novic %{buildroot}/usr/bin/
chmod 755 %{buildroot}/usr/bin/novic

cat > %{buildroot}/usr/share/applications/novic.desktop << DESKTOP
[Desktop Entry]
Name=Novic
Comment=Dynamic Island Media Widget
Exec=novic
Icon=novic
Type=Application
Categories=AudioVideo;Audio;Player;
DESKTOP

%files
/usr/bin/novic
/usr/share/applications/novic.desktop

%changelog
* $(date "+%a %b %d %Y") Novic Developers - ${VERSION}-1
- Initial package
EOF

# Build RPM
echo "Building RPM package..."
rpmbuild -bb ~/rpmbuild/SPECS/novic.spec

# Copy to project directory
cp ~/rpmbuild/RPMS/*/*.rpm "$PROJECT_DIR/"

echo ""
echo "=== Package built successfully! ==="
echo "Output: $PROJECT_DIR/novic-${VERSION}-1.*.rpm"
echo ""
echo "Install with: sudo dnf install novic-${VERSION}-1.*.rpm"

