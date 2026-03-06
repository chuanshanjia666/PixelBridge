#!/bin/bash
set -euo pipefail

# Usage: package_windows_msys2.sh [arch]
ARCH="${1:-x64}"

# Read version from VERSION file
PIXELBRIDGE_VERSION=$(tr -d '\n\r' < VERSION)

# Ensure Qt helper tools (including qmlimportscanner) are in PATH for windeployqt
QT_BIN_DIR=$(cygpath -u "$MSYSTEM_PREFIX/bin")
QT_HELPER_DIR=$(cygpath -u "$MSYSTEM_PREFIX/share/qt6/bin")
export PATH="$QT_BIN_DIR:$QT_HELPER_DIR:$PATH"
export QMAKE="$QT_BIN_DIR/qmake6.exe"

# Install to a staging directory first
STAGE_DIR="$PWD/build/package"
cmake --install build/ucrt64 --prefix "$STAGE_DIR"

EXE_PATH="$STAGE_DIR/bin/PixelBridge.exe"
if [ ! -f "$EXE_PATH" ]; then
  echo "Missing expected executable: $EXE_PATH"
  exit 1
fi

# windeployqt is a native Windows tool; feed it Windows paths
WINDEPLOYQT_EXE=$(cygpath -w "$QT_BIN_DIR/windeployqt6.exe")
QML_DIR_WIN=$(cygpath -w "$PWD/qml")
TARGET_EXE_WIN=$(cygpath -w "$EXE_PATH")
STAGE_BIN_WIN=$(cygpath -w "$STAGE_DIR/bin")

echo "Using windeployqt from: $WINDEPLOYQT_EXE"
echo "Staging to: $STAGE_BIN_WIN"

"$WINDEPLOYQT_EXE" --verbose 1 --qmldir "$QML_DIR_WIN" --no-translations --no-opengl-sw "$TARGET_EXE_WIN"

# Copy non-Qt runtime DLLs (FFmpeg/OpenSSL/spdlog/MinGW runtime) alongside the exe
STAGE_BIN_POSIX="$STAGE_DIR/bin"
for pattern in \
  avcodec-*.dll avformat-*.dll avutil-*.dll avdevice-*.dll \
  swscale-*.dll swresample-*.dll libspdlog-*.dll \
  libssl-*.dll libcrypto-*.dll \
  libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
  find "$MSYSTEM_PREFIX/bin" -maxdepth 1 -name "$pattern" -exec cp -u {} "$STAGE_BIN_POSIX" \;
done

# Auto-collect transitive non-system DLL dependencies from staged binaries.
DEP_LIST="$PWD/.pixelbridge_runtime_deps.txt"
: > "$DEP_LIST"
find "$STAGE_DIR" \( -iname "*.exe" -o -iname "*.dll" \) -print0 | while IFS= read -r -d '' bin; do
  ntldd "$bin" 2>/dev/null | awk '/=>/ {print $3}' | while IFS= read -r dep; do
    dep_posix=$(cygpath -u "$dep" 2>/dev/null || true)
    case "$dep_posix" in
      "$MSYSTEM_PREFIX/bin/"*.dll) echo "$dep_posix" >> "$DEP_LIST" ;;
    esac
  done
done
sort -u "$DEP_LIST" | while IFS= read -r dep; do
  [ -n "$dep" ] && cp -u "$dep" "$STAGE_BIN_POSIX" || true
done

# Fail packaging early if there are unresolved DLL dependencies.
MISSING_DLLS=$(find "$STAGE_DIR" \( -iname "*.exe" -o -iname "*.dll" \) -print0 | while IFS= read -r -d '' bin; do
  ntldd "$bin" 2>/dev/null | grep -E "=> not found|not found" | grep -Eiv '^[[:space:]]*(api-ms-|ext-ms-)'
done)
if [ -n "$MISSING_DLLS" ]; then
  echo "Unresolved runtime dependencies detected:"
  echo "$MISSING_DLLS"
  exit 1
fi

# Create a zip from the staged install
(
  cd "$STAGE_DIR"
  cmake -E tar cf "../PixelBridge-windows-${ARCH}-v${PIXELBRIDGE_VERSION}.zip" --format=zip .
)

echo "version=$PIXELBRIDGE_VERSION"
