#!/usr/bin/env bash
# Usage: ./configure.sh [debug|release] [options CMake supplémentaires]
#   ./configure.sh debug
#   ./configure.sh release -DDELOS_BUILD_STUDIO=OFF

BUILD_TYPE=${1:-debug}
shift || true

case "$BUILD_TYPE" in
    debug)   CMAKE_BUILD_TYPE="Debug"   ;;
    release) CMAKE_BUILD_TYPE="Release" ;;
    *)
        echo "Usage: $0 [debug|release]"
        exit 1
        ;;
esac

BUILD_DIR="build/${BUILD_TYPE}"
mkdir -p "$BUILD_DIR"

cmake -S . -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    "$@"

echo ""
echo "Configuré dans $BUILD_DIR. Pour compiler :"
echo "  cmake --build $BUILD_DIR"