#!/usr/bin/env bash
set -euo pipefail

game_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$game_root"
expected="22.1.8"
tool="${CLANG_TIDY:-clang-tidy}"
if ! command -v "$tool" >/dev/null 2>&1; then
    echo "clang-tidy $expected is required; provision the pinned .tool-versions toolchain." >&2
    exit 2
fi
version="$($tool --version)"
if [[ "$version" != *"$expected"* ]]; then
    echo "clang-tidy $expected is required; found: $version" >&2
    exit 2
fi

"$game_root/scripts/build-host-tools.sh"
cmake --preset analysis -S "$game_root" -DGAME_CLANG_TIDY_EXECUTABLE="$tool"
cmake --build --preset analysis --parallel "${GAME_BUILD_JOBS:-4}"
echo "C++ static-analysis gate passed with clang-tidy $expected"
