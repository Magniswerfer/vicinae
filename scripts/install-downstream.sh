#!/usr/bin/env bash
# Build the downstream fork in release mode, sign it with a stable identity and install it to
# /Applications, replacing the running Vicinae.
#
# Signing with the same certificate every time keeps macOS privacy permissions (Accessibility, ...)
# valid across rebuilds. The version is stamped `<upstream tag>-downstream`, which also disables the
# built-in updater so an upstream release never overwrites this build.
#
# Usage: scripts/install-downstream.sh
# Env:   VICINAE_CODESIGN_IDENTITY  certificate SHA-1 or name (see `security find-identity -v -p codesigning`)
set -euo pipefail

[[ "$(uname -s)" == "Darwin" ]] || { echo "macOS only" >&2; exit 1; }

SRC_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$SRC_DIR/build-release"
# 6.11.2+ fixes QQuickPopupWindow dropping clicks and drags inside popups (QTBUG-146887).
QT_VERSION="${QT_VERSION:-6.11.3}"
QT_DIR="$HOME/Qt/$QT_VERSION/macos"
IDENTITY="${VICINAE_CODESIGN_IDENTITY:-D9772E6279F32720C55D12784D5D876E0854379B}"
TAG="$(git -C "$SRC_DIR" describe --tags --abbrev=0)-downstream"
APP=/Applications/Vicinae.app

[[ -d "$QT_DIR" ]] || { echo "Qt $QT_VERSION not found at $QT_DIR (run: QT_VERSION=$QT_VERSION scripts/macos-setup.sh)" >&2; exit 1; }

# CMake keeps the Qt6*_DIR entries it cached, so a different Qt needs a fresh configure.
if ! grep -q "^Qt6_DIR:PATH=$QT_DIR/" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null; then
  rm -f "$BUILD_DIR/CMakeCache.txt"
fi

echo "==> building $TAG against Qt $QT_VERSION"
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLTO=ON \
  -DLOCAL_AI=ON \
  -DQT_DIR="$QT_DIR" \
  -DVICINAE_GIT_TAG="$TAG"
cmake --build "$BUILD_DIR" --parallel "$(sysctl -n hw.ncpu)"

VICINAE_CODESIGN_IDENTITY="$IDENTITY" "$SRC_DIR/scripts/macdeploy.sh" "$BUILD_DIR"
codesign --verify --deep --strict "$BUILD_DIR/bin/Vicinae.app"

echo "==> installing to $APP"
if pgrep -qx Vicinae; then
  osascript -e 'tell application "Vicinae" to quit' >/dev/null 2>&1 || true
  for _ in $(seq 1 20); do pgrep -qx Vicinae || break; sleep 0.5; done
  pgrep -qx Vicinae && pkill -x Vicinae || true
fi
rm -rf "$APP"
ditto "$BUILD_DIR/bin/Vicinae.app" "$APP"
open "$APP"
echo "==> installed $TAG"
