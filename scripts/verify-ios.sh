#!/usr/bin/env bash
set -euo pipefail

game_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app="${1:-$game_root/build/ios-device/Release-iphoneos/human_resources_player.app}"
build_root="${2:-$(dirname "$(dirname "$app")")}"
if [[ ! -d "$app" ]]; then
    echo "iPhoneOS app bundle is missing; run scripts/build-ios-device.sh." >&2
    exit 1
fi

# Use the same XML-capable interpreter selected for native project generation.
game_python="$(python3 - "$game_root" <<'PYTHON'
from pathlib import Path
import sys
sys.path.insert(0, str(Path(sys.argv[1]) / 'tools'))
from game_support.toolchains import build_python_executable
print(build_python_executable())
PYTHON
)"

for required in \
    "Info.plist" \
    "PrivacyInfo.xcprivacy" \
    "assets/shaders/metal/ios/bootstrap.vertex.metallib" \
    "assets/shaders/metal/ios/bootstrap.fragment.metallib" \
    "assets/shaders/metal/ios/style_world.vertex.metallib" \
    "assets/shaders/metal/ios/style_world.fragment.metallib" \
    "assets/shaders/metal/ios/product_ui.vertex.metallib" \
    "assets/shaders/metal/ios/product_ui.fragment.metallib" \
    "assets/shaders/package-manifest.json" \
    "assets/runtime/game-content.json" \
    "assets/runtime/game/ufo-modular.hrm" \
    "assets/runtime/game/ui-atlas.hrm" \
    "assets/runtime/package-manifest.json"; do
    if [[ ! -f "$app/$required" ]]; then
        echo "iPhoneOS bundle is missing required entry: $required" >&2
        exit 1
    fi
done

if [[ "$(plutil -extract UIRequiresFullScreen raw -o - "$app/Info.plist")" != "true" ]]; then
    echo "iPhoneOS bundle is not marked full-screen for its orientation contract." >&2
    exit 1
fi
"$game_python" - "$game_root/project.json" \
    "$(plutil -extract UISupportedInterfaceOrientations json -o - "$app/Info.plist")" \
    "$(plutil -extract 'UISupportedInterfaceOrientations~ipad' json -o - "$app/Info.plist")" <<'PY'
import json
from pathlib import Path
import sys

orientation = json.loads(Path(sys.argv[1]).read_text())["platform_settings"]["orientation"]
expected = (["UIInterfaceOrientationPortrait"] if orientation == "portrait" else
            ["UIInterfaceOrientationLandscapeLeft", "UIInterfaceOrientationLandscapeRight"])
if any(json.loads(value) != expected for value in sys.argv[2:]):
    raise SystemExit("iOS packaged orientations disagree with project.json")
PY
if find "$app" -type f \( -name '*.hlsl' -o -name '*.hlsli' -o -name '*.metal' -o -name '*.air' -o -name '*.spv' -o -name '*.dxil' -o -name '*shadercross*' -o -name '*flatc*' \) | grep -q .; then
    echo "iPhoneOS bundle contains a host tool, shader source, or non-Metal backend." >&2
    exit 1
fi
"$game_python" "$game_root/tools/generators/audit_runtime_package.py" --package-root "$app" \
    --expected-shader-manifest "$build_root/generated/assets/shaders/package-manifest.json" \
    --expected-runtime-manifest "$build_root/generated/assets/runtime/package-manifest.json" \
    --expected-shader-source-root "$game_root/shaders" \
    --expected-runtime-source-root "$game_root/assets/runtime"
"$game_python" "$game_root/tools/generators/audit_app_icon.py" --ios-app "$app"
"$game_python" "$game_root/tools/generators/audit_privacy_manifest.py" --ios-app "$app"
echo "iOS product bundle metadata, cooked content and shader allowlist audit passed."
echo "Package audit does not qualify physical-device gameplay or release readiness."
