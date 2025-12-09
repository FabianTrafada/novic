#!/bin/bash
set -e

# Build DEB package for Novic
# Usage: ./scripts/build-deb.sh [version]

VERSION="${1:-0.1.0}"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
PKG_DIR="$PROJECT_DIR/package"

echo "=== Building Novic DEB Package v${VERSION} ==="

# Check dependencies
echo "Checking dependencies..."
DEPS="cmake g++ pkg-config libgtkmm-3.0-dev libpulse-dev"
MISSING=""
for dep in $DEPS; do
    if ! dpkg -s "$dep" &>/dev/null; then
        MISSING="$MISSING $dep"
    fi
done

if [ -n "$MISSING" ]; then
    echo "Missing dependencies:$MISSING"
    echo "Install with: sudo apt install$MISSING"
    exit 1
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

# Create package structure
echo "Creating package structure..."
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/DEBIAN"
mkdir -p "$PKG_DIR/usr/bin"
mkdir -p "$PKG_DIR/usr/share/applications"

# Copy binary
cp "$BUILD_DIR/novic" "$PKG_DIR/usr/bin/"
chmod 755 "$PKG_DIR/usr/bin/novic"

# Create control file
cat > "$PKG_DIR/DEBIAN/control" << EOF
Package: novic
Version: ${VERSION}
Section: multimedia
Priority: optional
Architecture: amd64
Depends: libgtkmm-3.0-1v5, libpulse0
Maintainer: Novic Developers
Description: Dynamic Island-style media player widget for Wayland
 A sleek, floating media player widget that displays currently playing
 media with real-time audio visualization. Features hover-to-expand
 controls similar to Apple's Dynamic Island.
 .
 Note: Requires gtk-layer-shell (may need to be built from source).
EOF

# Create desktop file
cat > "$PKG_DIR/usr/share/applications/novic.desktop" << EOF
[Desktop Entry]
Name=Novic
Comment=Dynamic Island Media Widget
Exec=novic
Icon=novic
Type=Application
Categories=AudioVideo;Audio;Player;
Keywords=media;player;music;widget;
EOF

# Build package
echo "Building DEB package..."
cd "$PROJECT_DIR"
dpkg-deb --build package "novic_${VERSION}_amd64.deb"

echo ""
echo "=== Package built successfully! ==="
echo "Output: $PROJECT_DIR/novic_${VERSION}_amd64.deb"
echo ""
echo "Install with: sudo dpkg -i novic_${VERSION}_amd64.deb"

