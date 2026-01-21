#!/bin/bash
set -e

# Configuration
APP_NAME="PixelBridge"
BUILD_DIR="build/release"
APP_DIR="AppDir"

# Tools (Assumed to be downloaded by Workflow/CI)
if [ -f "./linuxdeploy-x86_64.AppImage" ]; then
    LINUXDEPLOY="./linuxdeploy-x86_64.AppImage"
elif [ -f "./linuxdeploy-aarch64.AppImage" ]; then
    LINUXDEPLOY="./linuxdeploy-aarch64.AppImage"
else
    # Fallback to PATH
    LINUXDEPLOY="linuxdeploy"
fi

# Clean up and prepare AppDir
rm -rf $APP_DIR
mkdir -p $APP_DIR

# Ensure build is up to date and installed to AppDir
# Use absolute path for DESTDIR to avoid confusion
DESTDIR=$(pwd)/$APP_DIR cmake --build $BUILD_DIR --target install

# Set Qt environment for linuxdeploy
if [ -z "$QMAKE" ]; then
    export QMAKE=$(which qmake6 || which qmake)
fi
export QT_VERSION=6
export QT_PLUGIN_PATH=$( "$QMAKE" -query QT_INSTALL_PLUGINS )
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$( "$QMAKE" -query QT_INSTALL_LIBS )

# Run linuxdeploy
# export QMAKE=/usr/lib/qt6/bin/qmake # Adjust path if needed if not in PATH
export VERSION=1.0.0
# Tell the Qt plugin where to look for QML files to bundle dependencies
export QML_SOURCES_PATHS="$(pwd)/qml"
# Force inclusion of all necessary platform plugins and shell extensions
# Using categories ensures that both wayland and xcb (and others) are included
export EXTRA_QT_PLUGINS="platforms;imageformats;iconengines;wayland-shell-extensions;multimedia"

# Fix for modern distributions (like Arch) where the bundled 'strip' in linuxdeploy
# does not support the new SHT_RELR relocation format.
export NO_STRIP=1

$LINUXDEPLOY \
    --appdir $APP_DIR \
    --plugin qt \
    --output appimage \
    --desktop-file PixelBridge.desktop \
    --icon-file assets/icons/pixelbridge.svg \
    --executable $APP_DIR/usr/bin/PixelBridge

echo "AppImage generated successfully!"
