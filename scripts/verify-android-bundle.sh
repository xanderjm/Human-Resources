#!/usr/bin/env bash
set -euo pipefail
game_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bundle="${1:-$game_root/android/app/build/outputs/bundle/release/app-release.aab}"
report="${2:-$game_root/build/evidence/mobile-release/aab}"
if [[ ! -f "$bundle" ]]; then
  echo "Release AAB missing; run the pinned Gradle :app:bundleRelease task." >&2
  exit 1
fi
# bundletool runs from the Android project, so bind caller-relative input now.
bundle="$(cd "$(dirname "$bundle")" && pwd)/$(basename "$bundle")"
mkdir -p "$report"
# Reuse the repository's healthy XML-capable generator interpreter selection.
python="$(python3 - "$game_root" <<'PYTHON'
from pathlib import Path
import sys
sys.path.insert(0, str(Path(sys.argv[1]) / 'tools'))
from game_support.toolchains import build_python_executable
print(build_python_executable())
PYTHON
)"
"$python" "$game_root/tools/generators/run_android_bundletool.py" validate --bundle="$bundle"
"$python" "$game_root/tools/generators/run_android_bundletool.py" --capture "$report/config.json" dump config --bundle="$bundle"
"$python" "$game_root/tools/generators/run_android_bundletool.py" --capture "$report/manifest.xml" dump manifest --bundle="$bundle" --module=base
"$python" "$game_root/tools/generators/audit_android_bundle.py" "$bundle" --config "$report/config.json" --manifest "$report/manifest.xml"
shasum -a 256 "$bundle"
