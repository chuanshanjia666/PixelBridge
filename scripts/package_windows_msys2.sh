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
rm -rf "$STAGE_DIR"
cmake --install build/ucrt64 --prefix "$STAGE_DIR"

EXE_PATH="$STAGE_DIR/bin/PixelBridge.exe"
if [ ! -f "$EXE_PATH" ]; then
  echo "Error: Missing expected executable: $EXE_PATH"
  exit 1
fi

# windeployqt is a native Windows tool; feed it Windows paths
WINDEPLOYQT_EXE="$QT_BIN_DIR/windeployqt6.exe"
QML_DIR_WIN=$(cygpath -w "$PWD/qml")
TARGET_EXE_WIN=$(cygpath -w "$EXE_PATH")
STAGE_BIN_WIN=$(cygpath -w "$STAGE_DIR/bin")

echo "Using windeployqt from: $WINDEPLOYQT_EXE"
echo "Staging to: $STAGE_BIN_WIN"

# Run windeployqt to deploy Qt dependencies
if ! "$WINDEPLOYQT_EXE" --verbose 1 --qmldir "$QML_DIR_WIN" --no-translations --no-opengl-sw "$TARGET_EXE_WIN"; then
  echo "Error: windeployqt failed"
  exit 1
fi

echo "Qt deployment complete. Now collecting non-Qt runtime dependencies..."

# Recursively collect all MinGW DLL dependencies using ntldd
STAGE_BIN_POSIX="$STAGE_DIR/bin"

# Function to check if a DLL should be bundled
should_bundle_dll() {
  local dll_path="$1"
  local dll_name=$(basename "$dll_path" | tr '[:upper:]' '[:lower:]')
  
  # Skip if not a file
  [ ! -f "$dll_path" ] && return 1
  
  # Must be from MSYSTEM_PREFIX (MinGW/UCRT64 libraries)
  case "$dll_path" in
    "$MSYSTEM_PREFIX/"*) ;;
    *) return 1 ;;
  esac
  
  # Exclude Windows system DLLs (shouldn't appear, but just in case)
  case "$dll_name" in
    kernel32.dll|user32.dll|advapi32.dll|shell32.dll|ws2_32.dll|\
    gdi32.dll|ole32.dll|msvcr*.dll|msvcp*.dll|vcruntime*.dll|\
    ucrtbase.dll|api-ms-*.dll|ext-ms-*.dll)
      return 1 ;;
  esac
  
  return 0
}

# Recursively collect dependencies
collect_dependencies() {
  local binary="$1"
  local target_dir="$2"
  
  # Parse ntldd output and extract DLL paths
  ntldd "$binary" 2>/dev/null | grep "=>" | awk '{print $3}' | while IFS= read -r dll_win_path; do
    [ -z "$dll_win_path" ] && continue
    [ "$dll_win_path" = "not" ] && continue
    
    # Convert Windows path to POSIX
    local dll_posix=$(cygpath -u "$dll_win_path" 2>/dev/null || echo "")
    [ -z "$dll_posix" ] && continue
    
    # Check if should bundle
    should_bundle_dll "$dll_posix" || continue
    
    local dll_name=$(basename "$dll_posix")
    local target_path="$target_dir/$dll_name"
    
    # Skip if already copied
    [ -f "$target_path" ] && continue
    
    # Copy the DLL
    echo "  Copying: $dll_name"
    cp "$dll_posix" "$target_path"
    
    # Recursively process this DLL's dependencies
    collect_dependencies "$target_path" "$target_dir"
  done
}

echo "Collecting MinGW runtime dependencies..."

# Start with all executables and Qt DLLs already staged
find "$STAGE_DIR" \( -iname "*.exe" -o -iname "*.dll" \) -type f | while IFS= read -r binary; do
  echo "Processing: $(basename "$binary")"
  collect_dependencies "$binary" "$STAGE_BIN_POSIX"
done

echo "Dependency collection complete."

# Final verification
echo "Verifying all dependencies are satisfied..."
MISSING_DEPS=$(find "$STAGE_DIR" \( -iname "*.exe" -o -iname "*.dll" \) -type f -exec ntldd {} \; 2>/dev/null | \
  grep "not found" | grep -Eiv "api-ms-|ext-ms-" || true)

if [ -n "$MISSING_DEPS" ]; then
  echo "Error: Unresolved runtime dependencies detected:"
  echo "$MISSING_DEPS"
  exit 1
fi

echo "All dependencies satisfied."

# Create a zip from the staged install
echo "Creating distribution package..."
ZIP_FILE="PixelBridge-windows-${ARCH}-v${PIXELBRIDGE_VERSION}.zip"
(
  cd "$STAGE_DIR"
  cmake -E tar cf "../$ZIP_FILE" --format=zip .
)

if [ -f "build/$ZIP_FILE" ]; then
  ZIP_SIZE=$(du -h "build/$ZIP_FILE" | cut -f1)
  echo "Package created successfully: build/$ZIP_FILE ($ZIP_SIZE)"
else
  echo "Error: Failed to create package"
  exit 1
fi

echo "version=$PIXELBRIDGE_VERSION"
