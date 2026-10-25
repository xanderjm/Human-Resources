"""Record deterministic native portrait gameplay directly to a verified movie.

A FIFO keeps raw RGBA frames off disk. Readback runs are visual evidence only,
and never substitute for the no-readback performance qualification.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

from profile import ROOT, digest, snapshot


def capture_pixels(value: str) -> tuple[int, int]:
    parts = value.split("x")
    if len(parts) != 2 or any(not part or any(ch < "0" or ch > "9" for ch in part) for part in parts):
        raise argparse.ArgumentTypeError("Capture pixels require WIDTHxHEIGHT")
    try:
        width, height = map(int, parts)
    except ValueError as error:
        raise argparse.ArgumentTypeError("Capture pixels contain an excessive integer") from error
    if not 1 <= width <= 4096 or not 1 <= height <= 4096 or width * height > 8 * 1024 * 1024:
        raise argparse.ArgumentTypeError("Capture pixels require sides 1..4096 and at most 8388608 pixels")
    return width, height


def output_extent(args: argparse.Namespace) -> tuple[int, int]:
    if args.capture_pixels is None:
        return args.render_width, args.render_height
    width, height = args.capture_pixels
    if abs(width * args.window_height - height * args.window_width) > max(args.window_width, args.window_height):
        raise ValueError("Capture aspect must match logical window: e.g. 1080x1920 with 360x640, 1206x2622 with 402x874")
    return width, height


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=ROOT / "build/dev/human_resources_player")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--frames", type=int, default=480)
    parser.add_argument("--fps", type=int, default=60)
    parser.add_argument("--step", type=float, default=1 / 60)
    parser.add_argument("--seed", type=int, default=514)
    parser.add_argument("--theme", choices=("farmland", "suburbia", "desert"), default="farmland")
    parser.add_argument("--skip-frames", type=int, default=0, help="Run these real fixed-step GPU frames before recording; no rule/state overrides")
    parser.add_argument("--render-width", type=int, default=588, help="Expected legacy world-sized capture width; not a world resolution override")
    parser.add_argument("--render-height", type=int, default=1280, help="Expected legacy world-sized capture height; not a world resolution override")
    parser.add_argument("--capture-pixels", type=capture_pixels, help="Explicit output WIDTHxHEIGHT; world cap stays 1280. Match window aspect.")
    parser.add_argument("--window-width", type=int, default=390)
    parser.add_argument("--window-height", type=int, default=844)
    parser.add_argument("--flow", action="store_true", help="Use short screen-flow smoke instead of sustained gameplay")
    args = parser.parse_args()
    if args.flow:
        args.frames = 280
    if args.skip_frames < 0 or args.skip_frames + args.frames > 2000000 or (args.flow and args.skip_frames):
        parser.error("Skip must be nonnegative, total frames bounded to 2000000, and unavailable with flow smoke")
    if min(args.frames, args.fps, args.render_width, args.render_height) <= 0 or args.render_width % 2 or args.render_height % 2:
        parser.error("Require positive count/rate and even positive dimensions")
    if not 320 <= args.window_width <= 2560 or not 320 <= args.window_height <= 2560:
        parser.error("Window dimensions must match the native client's bounded 320..2560 point range")
    try:
        width, height = output_extent(args)
    except ValueError as error:
        parser.error(str(error))
    if width % 2 or height % 2:
        parser.error("Movie output dimensions must be even; the native RGBA capture itself supports odd dimensions")
    output, executable = args.output.resolve(), args.executable.resolve()
    if output.exists():
        parser.error("Output must be a new movie path")
    output.parent.mkdir(parents=True, exist_ok=True)
    before = snapshot(executable)
    log = output.with_suffix(".native.log")
    with tempfile.TemporaryDirectory(prefix="hr-gameplay-film-", dir=output.parent) as temporary:
        directory = Path(temporary)
        fifo, candidate = directory / "native.rgba", directory / "candidate.mp4"
        os.mkfifo(fifo)
        encode_command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-f", "rawvideo", "-pixel_format", "rgba", "-video_size", f"{width}x{height}", "-framerate", str(args.fps), "-i", str(fifo), "-an", "-c:v", "libx264", "-preset", "fast", "-crf", "18", "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(candidate)]
        command = [str(executable), "--width", str(args.window_width), "--height", str(args.window_height), "--qa-theme", args.theme, "--capture", str(fifo)]
        if args.capture_pixels is not None:
            command += ["--capture-pixels", f"{width}x{height}"]
        metrics = output.with_suffix(".frames.csv")
        if args.flow:
            command += ["--playthrough-smoke"]
        else:
            command += ["--qa-frames", str(args.frames + args.skip_frames), "--qa-step-seconds", str(args.step), "--qa-seed", str(args.seed), "--qa-metrics", str(metrics)]
            if args.skip_frames:
                command += ["--qa-capture-skip-frames", str(args.skip_frames)]
        encoder = subprocess.Popen(encode_command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        try:
            with log.open("w") as stream:
                subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=900)
            _, errors = encoder.communicate(timeout=300)
            if encoder.returncode:
                raise RuntimeError(f"Capture encoder failed: {errors}")
        finally:
            if encoder.poll() is None:
                encoder.kill()
                encoder.wait()
        probe = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-count_frames", "-show_entries", "stream=width,height,nb_read_frames,r_frame_rate:format=duration", "-of", "json", str(candidate)], text=True))
        stream = probe["streams"][0]
        actual = (int(stream["width"]), int(stream["height"]), int(stream["nb_read_frames"]), stream["r_frame_rate"])
        if actual != (width, height, args.frames, f"{args.fps}/1"):
            raise ValueError(f"Native movie does not match capture contract: {probe}")
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-xerror", "-nostdin", "-i", str(candidate), "-f", "null", "-"], check=True, timeout=300)
        after = snapshot(executable)
        stable = before == after
        report = {"schema": 1, "scope": "native portrait SDL_GPU readback, no synthesized frames; macOS visual evidence", "composition": "explicit-pixel-composite" if args.capture_pixels is not None else "world-sized-composite", "output_pixels": [width, height], "logical_window": [args.window_width, args.window_height], "world_longest_side_limit": 1280, "ui_samples": 1, "command": command, "encode_command": encode_command, "probe": probe, "skipped_real_frames": args.skip_frames, "source_stable": stable, "source_before": before, "source_after": after, "video_sha256": digest(candidate), "native_log_sha256": digest(log), "full_decode": "pass", "pacing_note": "Video frame rate is the encoding rate, not measured live pacing; fixed step controls simulation time. Metrics include skipped real GPU/gameplay frames; video frame zero maps to metrics frame skipped_real_frames.", "ffmpeg_version": subprocess.check_output(["ffmpeg", "-version"], text=True).splitlines()[0]}
        candidate.replace(output)
    output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    for name, frame in (("early", 1), ("middle", args.frames // 2), ("late", args.frames - 2)):
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-i", str(output), "-vf", f"select=eq(n\\,{frame})", "-frames:v", "1", str(output.with_name(f"{output.stem}-{name}.png"))], check=True, timeout=60)
    print(json.dumps({"output": str(output), "probe": probe, "source_stable": stable}))
    if not stable:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
