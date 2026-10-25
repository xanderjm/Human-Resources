"""Encode native RGBA evidence and safely reclaim only verified build captures."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def encode(args: argparse.Namespace) -> dict:
    raw, output = args.raw.resolve(), args.output.resolve()
    if raw == output or output.exists():
        raise ValueError("Output must be a new path distinct from the input")
    expected = args.width * args.height * 4 * args.frames
    if raw.stat().st_size != expected:
        raise ValueError(f"Raw size must be exactly {expected} bytes")
    if args.width % 2 or args.height % 2:
        raise ValueError("H.264 4:2:0 dimensions must be even")
    if args.delete_raw and (not raw.is_relative_to(ROOT / "build") or raw.suffix != ".rgba" or args.raw.is_symlink()):
        raise ValueError("Raw removal is restricted to ordinary .rgba files under build/")
    raw_hash = digest(raw)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="encode-", dir=output.parent) as temporary:
        movie = Path(temporary) / "encoded.mp4"
        command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-f", "rawvideo", "-pixel_format", "rgba", "-video_size", f"{args.width}x{args.height}", "-framerate", str(args.fps), "-i", str(raw), "-an", "-c:v", "libx264", "-preset", "fast", "-crf", "18", "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(movie)]
        subprocess.run(command, check=True, timeout=1800)
        probe_command = ["ffprobe", "-v", "error", "-count_frames", "-show_entries", "stream=codec_name,width,height,r_frame_rate,nb_read_frames,pix_fmt:format=duration", "-of", "json", str(movie)]
        probe = json.loads(subprocess.check_output(probe_command, text=True, timeout=1800))
        stream = probe["streams"][0]
        if (int(stream["width"]), int(stream["height"]), int(stream["nb_read_frames"]), stream["r_frame_rate"]) != (args.width, args.height, args.frames, f"{args.fps}/1"):
            raise ValueError(f"Decoded capture dimensions/rate/count differ: {probe}")
        if abs(float(probe["format"]["duration"]) - args.frames / args.fps) > .002:
            raise ValueError("Encoded capture duration differs")
        # Error-on-decode verifies all compressed packets, independently of metadata.
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-xerror", "-nostdin", "-i", str(movie), "-f", "null", "-"], check=True, timeout=1800)
        evidence = {"schema": 1, "source": "native SDL_GPU RGBA readback; encoded without frame synthesis", "raw": str(raw.relative_to(ROOT)) if raw.is_relative_to(ROOT) else str(raw), "raw_sha256": raw_hash, "raw_bytes": expected, "original_producer": args.producer, "fps_note": args.fps_note, "output": str(output.relative_to(ROOT)) if output.is_relative_to(ROOT) else str(output), "video_sha256": digest(movie), "probe": probe, "encode_command": command, "full_decode": "pass", "ffmpeg_version": subprocess.check_output(["ffmpeg", "-version"], text=True).splitlines()[0], "ffprobe_version": subprocess.check_output(["ffprobe", "-version"], text=True).splitlines()[0], "logs": [{"path": str(path), "sha256": digest(path), "text": path.read_text()} for path in args.log], "raw_removed": False}
        if digest(raw) != raw_hash:
            raise ValueError("Raw capture changed while encoding")
        movie.replace(output)
    report = output.with_suffix(".encoding.json")
    report.write_text(json.dumps(evidence, indent=2) + "\n")
    if args.delete_raw:
        # Durable verified movie + report exist before the redundant raw is removed.
        raw.unlink()
        evidence["raw_removed"] = True
        report.write_text(json.dumps(evidence, indent=2) + "\n")
    return evidence


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("raw", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--width", type=int, required=True)
    parser.add_argument("--height", type=int, required=True)
    parser.add_argument("--frames", type=int, required=True)
    parser.add_argument("--fps", type=int, default=60)
    parser.add_argument("--fps-note", default="Fixed native frame sequence; movie rate does not prove live pacing")
    parser.add_argument("--producer", default="Original executable hash unavailable; do not infer a current source snapshot")
    parser.add_argument("--log", type=Path, action="append", default=[])
    parser.add_argument("--delete-raw", action="store_true")
    args = parser.parse_args()
    if min(args.width, args.height, args.frames, args.fps) <= 0:
        parser.error("Dimensions, frames and FPS must be positive")
    result = encode(args)
    print(json.dumps({key: result[key] for key in ("output", "raw_bytes", "full_decode", "raw_removed")}))


if __name__ == "__main__":
    main()
