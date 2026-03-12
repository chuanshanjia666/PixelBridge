#!/bin/bash
set -e

# Configuration
APP_NAME="PixelBridge"
BUILD_DIR="build/release"
APP_DIR="AppDir"

copy_qt_plugins() {
    local src_dir="$1"
    local dst_dir="$2"
    if [ -d "$src_dir" ]; then
        mkdir -p "$dst_dir"
        find "$src_dir" -maxdepth 1 -type f -name "*.so*" -exec cp -a {} "$dst_dir" \;
    fi
}

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
# Set CMAKE_INSTALL_PREFIX to /usr so it matches linuxdeploy expectations
cmake -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
DESTDIR=$(pwd)/$APP_DIR cmake --build $BUILD_DIR --target install

# Set Qt environment for linuxdeploy
if [ -z "$QMAKE" ]; then
    export QMAKE=$(which qmake6 || which qmake)
fi
export QT_VERSION=6
export QT_PLUGIN_PATH=$( "$QMAKE" -query QT_INSTALL_PLUGINS )
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$( "$QMAKE" -query QT_INSTALL_LIBS )

# Explicitly stage Wayland plugins into AppDir so they are available at runtime.
# linuxdeploy-plugin-qt may skip them when only xcb is detected at packaging time.
QT_PLUGINS_DIR="${QT_PLUGIN_PATH}"
APP_PLUGINS_DIR="$APP_DIR/usr/plugins"

copy_qt_plugins "$QT_PLUGINS_DIR/platforms" "$APP_PLUGINS_DIR/platforms"
copy_qt_plugins "$QT_PLUGINS_DIR/wayland-shell-integration" "$APP_PLUGINS_DIR/wayland-shell-integration"
copy_qt_plugins "$QT_PLUGINS_DIR/wayland-decoration-client" "$APP_PLUGINS_DIR/wayland-decoration-client"
copy_qt_plugins "$QT_PLUGINS_DIR/wayland-graphics-integration-client" "$APP_PLUGINS_DIR/wayland-graphics-integration-client"

# Run linuxdeploy
# export QMAKE=/usr/lib/qt6/bin/qmake # Adjust path if needed if not in PATH
VERSION=$(cat VERSION | tr -d '\n\r')
export VERSION
# Tell the Qt plugin where to look for QML files to bundle dependencies
export QML_SOURCES_PATHS="$(pwd)/qml"
# Keep Qt plugin categories broad, then explicitly include Wayland platform plugin.
export EXTRA_QT_PLUGINS="platforms;imageformats;iconengines;wayland-shell-integration;wayland-graphics-integration-client;wayland-decoration-client;multimedia"

LINUXDEPLOY_EXTRA_ARGS=()
if [ -f "$APP_PLUGINS_DIR/platforms/libqwayland.so" ]; then
    LINUXDEPLOY_EXTRA_ARGS+=(--library "$APP_PLUGINS_DIR/platforms/libqwayland.so")
fi
if [ -d "$APP_PLUGINS_DIR/wayland-shell-integration" ]; then
    while IFS= read -r -d '' sofile; do
        LINUXDEPLOY_EXTRA_ARGS+=(--library "$sofile")
    done < <(find "$APP_PLUGINS_DIR/wayland-shell-integration" -maxdepth 1 -type f -name "*.so*" -print0)
fi
if [ -d "$APP_PLUGINS_DIR/wayland-decoration-client" ]; then
    while IFS= read -r -d '' sofile; do
        LINUXDEPLOY_EXTRA_ARGS+=(--library "$sofile")
    done < <(find "$APP_PLUGINS_DIR/wayland-decoration-client" -maxdepth 1 -type f -name "*.so*" -print0)
fi
if [ -d "$APP_PLUGINS_DIR/wayland-graphics-integration-client" ]; then
    while IFS= read -r -d '' sofile; do
        LINUXDEPLOY_EXTRA_ARGS+=(--library "$sofile")
    done < <(find "$APP_PLUGINS_DIR/wayland-graphics-integration-client" -maxdepth 1 -type f -name "*.so*" -print0)
fi

# Fix for modern distributions (like Arch) where the bundled 'strip' in linuxdeploy
# does not support the new SHT_RELR relocation format.
export NO_STRIP=1

$LINUXDEPLOY \
    --appdir $APP_DIR \
    --plugin qt \
    --output appimage \
    --desktop-file PixelBridge.desktop \
    --icon-file assets/icons/pixelbridge.svg \
    --executable $APP_DIR/usr/bin/PixelBridge \
    "${LINUXDEPLOY_EXTRA_ARGS[@]}"

echo "AppImage generated successfully!"
