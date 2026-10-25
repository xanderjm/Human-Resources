#!/usr/bin/env bash
set -euo pipefail

game_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Refresh the target/resource graph before Xcode computes its build plan.
"$game_root/scripts/configure-ios-device.sh"
cmake --build "$game_root/build/ios-device" --config "${GAME_BUILD_CONFIG:-Release}" \
    --target human_resources_player -- CODE_SIGNING_ALLOWED=NO
