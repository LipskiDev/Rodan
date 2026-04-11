#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ASSIMP_SRC="$ROOT_DIR/external/assimp"
ASSIMP_BUILD="$ROOT_DIR/external/assimp-build"
ASSIMP_INSTALL="$ROOT_DIR/external/assimp-install"

BUILD_TYPE="${1:-Debug}"

if [[ "$BUILD_TYPE" != "Debug" && "$BUILD_TYPE" != "Release" ]]; then
    echo "Usage: $0 [Debug|Release]"
    exit 1
fi

echo "==> Building Assimp ($BUILD_TYPE)"
echo "    Source : $ASSIMP_SRC"
echo "    Build  : $ASSIMP_BUILD/$BUILD_TYPE"
echo "    Install: $ASSIMP_INSTALL"

cmake -S "$ASSIMP_SRC" \
      -B "$ASSIMP_BUILD/$BUILD_TYPE" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_INSTALL_PREFIX="$ASSIMP_INSTALL" \
      -DASSIMP_BUILD_TESTS=OFF \
      -DASSIMP_BUILD_ASSIMP_TOOLS=OFF \
      -DASSIMP_BUILD_SAMPLES=OFF \
      -DASSIMP_WARNINGS_AS_ERRORS=OFF \
      -DBUILD_SHARED_LIBS=OFF

cmake --build "$ASSIMP_BUILD/$BUILD_TYPE" -j"$(nproc)"
cmake --install "$ASSIMP_BUILD/$BUILD_TYPE"

echo "==> Assimp build complete"
echo "    Headers: $ASSIMP_INSTALL/include"
echo "    Libs   : $ASSIMP_INSTALL/lib"
