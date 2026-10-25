#!/usr/bin/env bash
set -euo pipefail

game_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
apk="${1:-$game_root/android/app/build/outputs/apk/debug/app-debug.apk}"
if [[ ! -f "$apk" ]]; then
    echo "Android APK is missing: $apk" >&2
    exit 1
fi
shasum -a 256 "$apk"
python3 - "$game_root" "$apk" <<'PY'
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path(sys.argv[1])
sys.path.insert(0, str(root / "tools"))
from game_support.toolchains import android_sdk_root

orientation = json.loads((root / "project.json").read_text())["platform_settings"]["orientation"]
expected = 1 if orientation == "portrait" else 6  # Android ActivityInfo orientation values.
sdk = android_sdk_root()
aapt = sdk / "build-tools/36.0.0/aapt2"
result = subprocess.run((str(aapt), "dump", "xmltree", "--file", "AndroidManifest.xml", sys.argv[2]),
                        check=True, capture_output=True, text=True)
values = re.findall(r'android:screenOrientation\([^)]*\)=((?:0x)?[0-9a-fA-F]+)\b', result.stdout)
if len(values) != 1 or int(values[0], 0) != expected:
    raise SystemExit("Android packaged orientation disagrees with project.json")
subprocess.run((str(sdk / "build-tools/36.0.0/zipalign"), "-c", "-P", "16", "4", sys.argv[2]),
               check=True)
PY
entries="$(unzip -Z1 "$apk")"
for required in \
    "lib/arm64-v8a/libmain.so" \
    "lib/arm64-v8a/libSDL3.so" \
    "lib/x86_64/libmain.so" \
    "lib/x86_64/libSDL3.so" \
    "assets/shaders/spirv/bootstrap.vertex.spv" \
    "assets/shaders/spirv/bootstrap.fragment.spv" \
    "assets/shaders/spirv/style_world.vertex.spv" \
    "assets/shaders/spirv/style_world.fragment.spv" \
    "assets/shaders/spirv/product_ui.vertex.spv" \
    "assets/shaders/spirv/product_ui.fragment.spv" \
    "assets/shaders/package-manifest.json" \
    "assets/runtime/game-content.json" \
    "assets/runtime/game/ufo-modular.hrm" \
    "assets/runtime/game/ui-atlas.hrm" \
    "assets/runtime/package-manifest.json"; do
    if ! printf '%s\n' "$entries" | grep -Fqx "$required"; then
        echo "Android APK is missing required entry: $required" >&2
        exit 1
    fi
done
if printf '%s\n' "$entries" | grep -Eq '(^|/)(shadercross|flatc)(/|$)|[.](hlsl|hlsli|metal|air|dxil)$|assets/shaders/(dxil|metal)/'; then
    echo "Android APK contains a host tool, shader source, or non-SPIR-V backend." >&2
    exit 1
fi
python3 "$game_root/tools/generators/audit_asset_package.py" \
    --archive "$apk" --prefix assets/shaders --kind shaders \
    --expected-manifest "$game_root/build/android-assets/shaders/package-manifest.json" \
    --expected-source-root "$game_root/shaders"
python3 "$game_root/tools/generators/audit_asset_package.py" \
    --archive "$apk" --prefix assets/runtime --kind runtime \
    --expected-manifest "$game_root/build/android-assets/runtime/package-manifest.json" \
    --expected-source-root "$game_root/assets/runtime"
python3 "$game_root/tools/generators/audit_android_native.py" "$apk"
python3 "$game_root/tools/generators/audit_app_icon.py" --android-apk "$apk"
echo "Android APK ABI and shader allowlist audit passed."
echo "Android APK verified. Physical Vulkan device evidence remains a release gate."
