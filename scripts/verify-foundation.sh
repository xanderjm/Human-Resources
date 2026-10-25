#!/usr/bin/env bash
set -euo pipefail

game_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
python3 "$game_root/tools/generators/verify_foundation.py" --root "$game_root"

