#!/usr/bin/env bash
set -euo pipefail

game_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
platform="${1:-all}"

"$game_root/scripts/build-host-tools.sh"
python3 "$game_root/tools/generators/compile_shaders.py" \
    --shadercross "$game_root/build/host-tools/shadercross/shadercross" \
    --source-root "$game_root/shaders" \
    --output-root "$game_root/build/shader-audit/assets/shaders" \
    --include-output "$game_root/build/shader-audit/include" \
    --platform "$platform" \
    --apple-deployment-target 17.0
