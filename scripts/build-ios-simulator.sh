#!/usr/bin/env bash
set -euo pipefail

game_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$game_root/scripts/configure-ios-simulator.sh"
cmake --build "$game_root/build/ios-simulator" --config "${GAME_BUILD_CONFIG:-Release}" \
    --target human_resources_player -- CODE_SIGNING_ALLOWED=NO
