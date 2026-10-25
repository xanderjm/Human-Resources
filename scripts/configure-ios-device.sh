#!/usr/bin/env bash
set -euo pipefail

game_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$game_root/scripts/build-host-tools.sh"
cmake -S "$game_root" -B "$game_root/build/ios-device" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT=iphoneos \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
    -DHUMAN_RESOURCES_WARNINGS_AS_ERRORS=ON \
    -DHUMAN_RESOURCES_BUILD_TESTS=OFF \
    -DGAME_SHADER_PLATFORM=ios \
    -DGAME_FLATC_EXECUTABLE="$game_root/build/host-tools/flatbuffers/flatc" \
    -DGAME_SHADERCROSS_EXECUTABLE="$game_root/build/host-tools/shadercross/shadercross"

