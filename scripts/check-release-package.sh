#!/usr/bin/env bash
set -euo pipefail

game_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
package_root="${1:-$game_root/build/release}"
python3 "$game_root/tools/generators/audit_runtime_package.py" --package-root "$package_root" \
    --expected-shader-manifest "$game_root/build/release/generated/assets/shaders/package-manifest.json" \
    --expected-runtime-manifest "$game_root/build/release/generated/assets/runtime/package-manifest.json" \
    --expected-shader-source-root "$game_root/shaders" \
    --expected-runtime-source-root "$game_root/assets/runtime"
