"""Run the actual portrait client with deterministic QA inputs and source evidence.

CPU submission includes driver waits; only Metal HUD data may describe GPU time.
The live no-capture run is the pacing qualification; readback films are separate.
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
TIMINGS = ("frame_interval_ms", "game_advance_ms", "presentation_ms", "render_submit_ms", "cpu_frame_ms")
APPLE_HUD_DOC = "https://developer.apple.com/documentation/xcode/monitoring-your-metal-apps-graphics-performance"


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def snapshot(executable: Path) -> dict:
    paths = set(ROOT.glob("native/**/*.cpp")) | set(ROOT.glob("native/**/*.h")) | set(ROOT.glob("native/**/*.hlsl"))
    paths.update(ROOT.glob("native/**/*.mm"))
    paths.update(ROOT.glob("native/**/CMakeLists.txt"))
    paths.update(ROOT.glob("native/**/*.json"))
    paths.update(ROOT.glob("native/**/tools/*.py"))
    paths.update(ROOT.glob("assets/runtime/**/*.json"))
    paths.update(ROOT.glob("design/balance/*.json"))
    paths.update(ROOT.glob("shaders/src/*.hlsl"))
    paths.update(ROOT.glob("tools/playable_qa/*.py"))
    paths.update(ROOT / name for name in ("project.json", "CMakeLists.txt", "CMakePresets.json"))
    paths.update(executable.parent.glob("assets/runtime/**/source-manifest.json"))
    paths.update(ROOT.glob("build/dev/assets/runtime/**/*"))
    paths.update(executable.parent.parent.glob("Resources/assets/runtime/**/*"))
    return {"executable": {"path": str(executable), "sha256": digest(executable)}, "files": {str(path.relative_to(ROOT)): digest(path) for path in sorted(paths) if path.is_file()}}


def distribution(values: list[float]) -> dict:
    values = sorted(values)
    if not values:
        return {"samples": 0}
    def percentile(fraction: float) -> float:
        return values[min(len(values) - 1, math.ceil(fraction * len(values)) - 1)]
    return {"samples": len(values), "mean": sum(values) / len(values), "p50": percentile(.5), "p95": percentile(.95), "p99": percentile(.99), "max": values[-1]}


def summarize(rows: list[dict], warmup: int) -> dict:
    measured = rows[warmup:]
    active = [row for row in measured if row["phase"] == "1"]
    groups = {"all_after_warmup": measured, "active_run": active, "late_combat_120s_plus": [row for row in active if float(row["active_seconds"]) >= 120], "late_combat_300s_plus": [row for row in active if float(row["active_seconds"]) >= 300]}
    result = {}
    for name, group in groups.items():
        result[name] = {column: distribution([float(row[column]) for row in group]) for column in TIMINGS}
        result[name]["intervals_over_25_ms"] = sum(float(row["frame_interval_ms"]) > 25 for row in group)
        result[name]["intervals_over_33_34_ms"] = sum(float(row["frame_interval_ms"]) > 33.34 for row in group)
    result["peak_counts"] = {column: max((float(row[column]) for row in rows), default=0) for column in ("entities", "projectiles", "beams", "world_draws", "shadow_draws", "triangles")}
    result["phase_counts"] = {phase: sum(row["phase"] == phase for row in rows) for phase in sorted({row["phase"] for row in rows})}
    result["max_active_seconds"] = max((float(row["active_seconds"]) for row in rows), default=0)
    return result


def process_memory(parent_pid: int) -> list[dict]:
    # /usr/bin/time owns the client child. ps runs outside the timed process.
    listing = subprocess.check_output(["ps", "-axo", "pid=,ppid=,rss="], text=True)
    result = []
    for line in listing.splitlines():
        pid, parent, rss = map(int, line.split())
        if parent == parent_pid:
            result.append({"pid": pid, "rss_bytes": rss * 1024})
    return result


def metal_hud(text: str) -> dict:
    values = []
    for line in text.splitlines():
        if "metal-HUD:" not in line:
            continue
        payload = line.split("metal-HUD:", 1)[1].strip()
        try:
            parsed = [float(value) for value in payload.split(",")]
        except ValueError:
            continue
        # Apple's documented record: first frame, graphics memory, process
        # memory, followed by repeated present interval / GPU time pairs.
        if len(parsed) >= 5 and (len(parsed) - 3) % 2 == 0:
            values.extend(parsed[4::2])
    return {"source": APPLE_HUD_DOC, "status": "available" if values else "unavailable_no_console_samples", "reported_gpu_time": distribution(values), "units": "Apple HUD native log units; raw log retained, no inferred conversion"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=ROOT / "build/dev/human_resources_player")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=1800)
    parser.add_argument("--step", type=float, default=1 / 60)
    parser.add_argument("--seed", type=int, default=514)
    parser.add_argument("--width", type=int, default=390)
    parser.add_argument("--height", type=int, default=844)
    parser.add_argument("--warmup", type=int, default=120)
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("--metal-hud", action="store_true")
    args = parser.parse_args()
    if args.frames <= args.warmup or args.frames > 2_000_000 or not 0 < args.step <= .25 or args.seed < 0:
        parser.error("Require frames > warmup, frames <= 2000000, 0 < step <= .25 and seed >= 0")
    args.output = args.output.resolve()
    args.executable = args.executable.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    before = snapshot(args.executable)
    (args.output / "source-before.json").write_text(json.dumps(before, indent=2) + "\n")
    native_log, time_log, metrics = (args.output / name for name in ("native.log", "process-time.log", "frames.csv"))
    command = ["/usr/bin/time", "-l", "-o", str(time_log), str(args.executable), "--width", str(args.width), "--height", str(args.height), "--qa-frames", str(args.frames), "--qa-step-seconds", str(args.step), "--qa-seed", str(args.seed), "--qa-metrics", str(metrics)]
    environment = {**os.environ, "SDL_ASSERT": "abort", "MTL_HUD_ENABLED": "0", "MTL_HUD_LOG_ENABLED": "0", "MTL_HUD_LOG_SHADER_ENABLED": "0"}
    if args.metal_hud:
        environment.update(MTL_HUD_ENABLED="1", MTL_HUD_LOG_ENABLED="1", MTL_HUD_LOG_SHADER_ENABLED="1")
    memory = []
    started = time.monotonic()
    with native_log.open("w") as stream:
        process = subprocess.Popen(command, cwd=ROOT, env=environment, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            while process.poll() is None:
                elapsed = time.monotonic() - started
                if elapsed > args.timeout:
                    os.killpg(process.pid, signal.SIGTERM)
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid, signal.SIGKILL)
                        process.wait()
                    raise TimeoutError("Native gameplay qualification exceeded timeout")
                memory.extend({"elapsed_seconds": elapsed, **sample} for sample in process_memory(process.pid))
                time.sleep(.5)
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
    after = snapshot(args.executable)
    if metrics.exists():
        with metrics.open() as stream:
            rows = list(csv.DictReader(stream))
    else:
        rows = []
    changed = [path for path in before["files"].keys() | after["files"].keys() if before["files"].get(path) != after["files"].get(path)]
    stable = not changed and before["executable"] == after["executable"]
    report = {"schema": 1, "timestamp_utc": datetime.now(timezone.utc).isoformat(), "host": platform.platform(), "scope": "macOS native gameplay at portrait desktop dimensions; not physical mobile qualification", "command": command, "exit_code": process.returncode, "wall_seconds": time.monotonic() - started, "requested_frames": args.frames, "recorded_frames": len(rows), "warmup_frames": args.warmup, "fixed_seed": args.seed, "simulation_seconds_per_frame": args.step, "logical_window": [args.width, args.height], "capture_readback": False, "source_stable": stable, "changed_source_paths": changed, "executable_stable": before["executable"] == after["executable"], "summary": summarize(rows, args.warmup) if rows else {}, "memory_samples": memory, "native_time_report": time_log.read_text() if time_log.exists() else "missing", "metal_hud": metal_hud(native_log.read_text()), "timing_note": "CPU render_submit includes driver/present waits and is not GPU time. Fixed simulation steps do not prove human pacing.", "source_after": after}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in ("exit_code", "wall_seconds", "recorded_frames", "source_stable", "summary", "metal_hud")}, indent=2))
    if process.returncode or len(rows) != args.frames or not stable:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
