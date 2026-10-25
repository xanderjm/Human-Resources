#!/bin/sh
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$task_root"
cmake --preset dev
cmake --build --preset dev --target human_resources_style_preview
exec "$task_root/build/dev/human_resources_style_preview" "$@"
