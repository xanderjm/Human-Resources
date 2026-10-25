#!/usr/bin/env python3
"""Run the existing pinned AGP bundletool; no additional download/dependency."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from game_support.toolchains import android_environment


def run(arguments: list[str], capture: Path | None = None) -> int:
    with tempfile.TemporaryDirectory(prefix='hr-bundletool-') as directory:
        path = Path(directory) / 'arguments.json'
        path.write_text(json.dumps(arguments))
        command = [str(ROOT / 'android/gradlew'), '--no-daemon', '--console=plain',
                   '-I', str(Path(__file__).with_name('android_bundletool.init.gradle')),
                   '-Dhr.bundleArgs=' + str(path), 'hrBundleTool']
        if capture is not None:
            capture.parent.mkdir(parents=True, exist_ok=True)
            command.insert(-1, '-Dhr.bundleOutput=' + str(capture.resolve()))
        return subprocess.run(command, cwd=ROOT / 'android', env=android_environment(), check=False).returncode


if __name__ == '__main__':
    arguments = sys.argv[1:]
    capture = None
    if arguments[:1] == ['--capture']:
        if len(arguments) < 3:
            raise SystemExit('Usage: run_android_bundletool.py [--capture PATH] COMMAND ARGS...')
        capture, arguments = Path(arguments[1]), arguments[2:]
    raise SystemExit(run(arguments, capture))
