#!/usr/bin/env bash
set -euo pipefail
game_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_root="${1:-$game_root/build/dev}"
out="$game_root/build/mobile-qa-tools"
mkdir -p "$out"
"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -Werror \
  -I "$game_root/native/human_resources_storage/include" \
  -I "$game_root/native/human_resources_gameplay/include" \
  -I "$game_root/native/human_resources_progression/include" \
  "$game_root/tools/native_account_inspect.cpp" \
  "$build_root/native/human_resources_progression/libhuman_resources_progression.a" \
  "$build_root/native/human_resources_gameplay/libhuman_resources_gameplay.a" \
  "$build_root/libhuman_resources_storage.a" -o "$out/account-inspect"
python3 - "$game_root" "$build_root" "$out" <<'PY'
import hashlib,json,sys
from pathlib import Path
root,build,out=map(Path,sys.argv[1:])
paths=[root/'tools/native_account_inspect.cpp',out/'account-inspect',
       build/'native/human_resources_progression/libhuman_resources_progression.a',
       build/'native/human_resources_gameplay/libhuman_resources_gameplay.a',build/'libhuman_resources_storage.a']
(out/'account-inspect-build.json').write_text(json.dumps({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},indent=2)+'\n')
PY
