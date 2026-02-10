#!/usr/bin/env bash
#
# Package PathView as a macOS .app bundle inside a .dmg disk image.
#
# Usage: ./scripts/package_macos.sh <build-dir>
#
# Prerequisites: dylibbundler (brew install dylibbundler), iconutil (ships with Xcode)
#
set -euo pipefail

BUILD_DIR="${1:?Usage: $0 <build-dir>}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

VERSION="$(cat "$PROJECT_DIR/VERSION" | tr -d '[:space:]')"
ARCH="$(uname -m)"

APP_NAME="PathView"
APP_BUNDLE="$APP_NAME.app"
CONTENTS="$APP_BUNDLE/Contents"
DMG_NAME="PathView-${VERSION}-${ARCH}.dmg"

echo "=== Packaging $APP_NAME $VERSION ($ARCH) ==="

# Clean previous artifacts
rm -rf "$APP_BUNDLE" "$DMG_NAME"

# ── 1. Create .app bundle structure ──────────────────────────────────────────
mkdir -p "$CONTENTS/MacOS"
mkdir -p "$CONTENTS/Resources/fonts"
mkdir -p "$CONTENTS/Frameworks"

# ── 2. Generate Info.plist ───────────────────────────────────────────────────
cat > "$CONTENTS/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>PathView</string>
    <key>CFBundleDisplayName</key>
    <string>PathView</string>
    <key>CFBundleIdentifier</key>
    <string>com.pathview.app</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundleExecutable</key>
    <string>pathview</string>
    <key>CFBundleIconFile</key>
    <string>PathView</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>12.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
</dict>
</plist>
PLIST

echo "  Info.plist generated"

# ── 3. Generate .icns from iconset ───────────────────────────────────────────
ICONSET="$PROJECT_DIR/assets/icon-pathview.iconset"
if [ -d "$ICONSET" ]; then
    iconutil --convert icns --output "$CONTENTS/Resources/PathView.icns" "$ICONSET"
    echo "  PathView.icns generated"
else
    echo "  Warning: iconset not found at $ICONSET, skipping icon generation"
fi

# ── 4. Copy binaries ────────────────────────────────────────────────────────
cp "$BUILD_DIR/pathview" "$CONTENTS/MacOS/pathview"
if [ -f "$BUILD_DIR/pathview-mcp" ]; then
    cp "$BUILD_DIR/pathview-mcp" "$CONTENTS/MacOS/pathview-mcp"
    echo "  Copied pathview + pathview-mcp"
else
    echo "  Copied pathview (pathview-mcp not found, skipping)"
fi

# ── 5. Copy resources ───────────────────────────────────────────────────────
cp "$PROJECT_DIR/resources/fonts/Inter-Regular.ttf"      "$CONTENTS/Resources/fonts/"
cp "$PROJECT_DIR/resources/fonts/Inter-Medium.ttf"       "$CONTENTS/Resources/fonts/"
cp "$PROJECT_DIR/resources/fonts/FontAwesome6-Solid.ttf" "$CONTENTS/Resources/fonts/"
cp "$PROJECT_DIR/resources/fonts/IconsFontAwesome6.h"    "$CONTENTS/Resources/fonts/"

# Copy icon assets for runtime (window icon)
cp -R "$PROJECT_DIR/assets/icon-pathview.iconset" "$CONTENTS/Resources/icon-pathview.iconset"

echo "  Resources copied"

# ── 6. Bundle shared libraries with dylibbundler ────────────────────────────
echo "  Bundling shared libraries..."

DYLIBBUNDLER_ARGS=(
    -od -b
    -x "$CONTENTS/MacOS/pathview"
    -d "$CONTENTS/Frameworks"
    -p "@executable_path/../Frameworks/"
)

# Also bundle pathview-mcp if it exists
if [ -f "$CONTENTS/MacOS/pathview-mcp" ]; then
    DYLIBBUNDLER_ARGS+=(-x "$CONTENTS/MacOS/pathview-mcp")
fi

dylibbundler "${DYLIBBUNDLER_ARGS[@]}"

echo "  Shared libraries bundled"

# ── 7. Ad-hoc code sign ─────────────────────────────────────────────────────
echo "  Code signing..."
codesign --force --deep --sign - "$APP_BUNDLE"
echo "  Code signed (ad-hoc)"

# ── 8. Create DMG ───────────────────────────────────────────────────────────
echo "  Creating DMG..."

STAGING_DIR="$(mktemp -d)"
cp -R "$APP_BUNDLE" "$STAGING_DIR/"
ln -s /Applications "$STAGING_DIR/Applications"

hdiutil create \
    -volname "$APP_NAME" \
    -srcfolder "$STAGING_DIR" \
    -ov \
    -format UDZO \
    "$DMG_NAME"

rm -rf "$STAGING_DIR"

echo ""
echo "=== Done: $DMG_NAME ==="
ls -lh "$DMG_NAME"
