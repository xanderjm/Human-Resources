#!/usr/bin/env bash
set -euo pipefail

game_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
expected="22.1.8"
tool="${CLANG_FORMAT:-clang-format}"
if ! command -v "$tool" >/dev/null 2>&1; then
    echo "clang-format $expected is required; provision the pinned .tool-versions toolchain." >&2
    exit 2
fi
version="$($tool --version)"
if [[ "$version" != *"$expected"* ]]; then
    echo "clang-format $expected is required; found: $version" >&2
    exit 2
fi
find "$game_root/native" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.mm' \) -print0 \
    | xargs -0 "$tool" --dry-run --Werror
echo "C++ format gate passed with clang-format $expected"
