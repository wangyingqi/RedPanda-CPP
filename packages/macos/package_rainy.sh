#!/bin/bash
#
# Build and package Rainy-DevCPP-Mac: a Release build of the app, with the
# macOS learning resources (compat shims, compiler discovery hint, starter
# problem set, reference examples and the 100-example library) bundled into
# Contents/Resources, deployed Qt frameworks, an ad-hoc signature and a DMG.
#
# Usage: packages/macos/package_rainy.sh [-c|--clean] [--qt-dir <dir>]

set -euxo pipefail

_MAC_APP_VERSION="1.0"          # our own product version (see MACDEVCPP_VERSION)
_APP_NAME="Rainy-DevCPP-Mac"

_CLEAN=0
# CMAKE_PREFIX_PATH root that find(Qt6) searches (Homebrew: /opt/homebrew,
# where qtbase/qtsvg/qttools are symlinked). --qt-dir overrides it.
_QT_PREFIX="${_QT_PREFIX:-/opt/homebrew}"
while [[ $# -gt 0 ]]; do
  case "$1" in
    -c|--clean) _CLEAN=1 ;;
    --qt-dir) _QT_PREFIX="$2"; shift ;;
    -h|--help) echo "Usage: $0 [-c|--clean] [--qt-dir <dir>]"; exit 0 ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
  shift
done

_PROJECT_ROOT="$PWD"
_BUILD_DIR="$_PROJECT_ROOT/build/macos"
_PKG_DIR="$_PROJECT_ROOT/build/macos-pkg"
_MAC_RES="$_PROJECT_ROOT/platform/macos"
_HINT_LUA="$_PROJECT_ROOT/packages/macos/compiler_hint.lua"

# Locate the Qt host tools (qmake/macdeployqt). Homebrew keeps them in the
# qtbase formula; a monolithic Qt keeps them under <prefix>/bin.
_QT_BIN=""
for cand in "$_QT_PREFIX/bin" "$_QT_PREFIX/opt/qtbase/bin"; do
  if [[ -x "$cand/qmake" ]]; then _QT_BIN="$cand"; break; fi
done
if [[ -z "$_QT_BIN" ]]; then
  echo "qmake not found under $_QT_PREFIX. Pass --qt-dir <prefix>."; exit 1
fi
export PATH="$_QT_BIN:$PATH"

if [[ $_CLEAN -eq 1 ]]; then
  rm -rf "$_BUILD_DIR" "$_PKG_DIR"
fi
mkdir -p dist

arch_info=$(uname -m)
cmake -S . -B "$_BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$_QT_PREFIX" \
  -DCMAKE_INSTALL_PREFIX="$_PKG_DIR" \
  -DLUA_ADDON=ON \
  -DCMAKE_OSX_ARCHITECTURES="$arch_info"
cmake --build "$_BUILD_DIR" --parallel "$(sysctl -n hw.logicalcpu)"

rm -rf "$_PKG_DIR"
cmake --install "$_BUILD_DIR"

_APP="$_PKG_DIR/RedPandaIDE.app"
_RES="$_APP/Contents/Resources"

# --- deploy Qt frameworks -------------------------------------------------
macdeployqt "$_APP" || true
# Homebrew Qt: macdeployqt leaves an @rpath reference in libbrotlidec
_brotlidec="$_APP/Contents/Frameworks/libbrotlidec.1.dylib"
if [[ -f "$_brotlidec" ]]; then
  install_name_tool -change @rpath/libbrotlicommon.1.dylib \
    @executable_path/../Frameworks/libbrotlicommon.1.dylib "$_brotlidec" || true
fi

# --- bundle the learning resources into Contents/Resources ---------------
# (non-code only; keeping it out of Contents/MacOS is required for codesign --deep)
mkdir -p "$_RES"
cp -R "$_MAC_RES/compat"                  "$_RES/"
cp -R "$_MAC_RES/examples"                "$_RES/"
cp -R "$_MAC_RES/library"                 "$_RES/"
cp    "$_MAC_RES/starter_problemset.json" "$_RES/"
cp    "$_HINT_LUA"                        "$_RES/compiler_hint.lua"

# --- rename bundle to the product name -----------------------------------
_FINAL_APP="$_PKG_DIR/$_APP_NAME.app"
rm -rf "$_FINAL_APP"
mv "$_APP" "$_FINAL_APP"

# --- sign (after all content is in place; adding files invalidates a sig) --
xattr -cr "$_FINAL_APP"
codesign --force --deep --sign "-" "$_FINAL_APP"
# Sanity check the signature. Not --strict: recent macOS auto-stamps a benign
# "com.apple.provenance" xattr onto signed executables (even onto the
# _CodeSignature codesign just wrote), which --strict rejects as "detritus"
# although it does not affect Gatekeeper or launching. Report, don't abort.
codesign --verify --deep "$_FINAL_APP" \
  || echo "warning: codesign --verify reported issues (benign provenance xattrs)"

# --- DMG ------------------------------------------------------------------
_DMG="dist/$_APP_NAME-$_MAC_APP_VERSION.dmg"
_STAGE="$(mktemp -d)/$_APP_NAME"
mkdir -p "$_STAGE"
cp -R "$_FINAL_APP" "$_STAGE/"
ln -s /Applications "$_STAGE/Applications"
rm -f "$_DMG"
hdiutil create -volname "$_APP_NAME" -srcfolder "$_STAGE" -ov -format UDZO "$_DMG"
rm -rf "$_STAGE"

# --- portable tarball -----------------------------------------------------
COPYFILE_DISABLE=1 tar --no-xattrs -C "$_PKG_DIR" \
  -cJf "dist/$_APP_NAME-$_MAC_APP_VERSION-macOS.tar.xz" "$_APP_NAME.app"

echo "Done:"
echo "  $_DMG"
echo "  dist/$_APP_NAME-$_MAC_APP_VERSION-macOS.tar.xz"
