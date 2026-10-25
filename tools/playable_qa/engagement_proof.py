"""Source-bound public-Game engagement film plus labelled native View fixtures.

Run with the existing pinned build/tools/art-python/bin/python.
Run only with the coordinator's GPU slot. The user app and account are untouched;
all GPU work uses a hidden background SDL window and explicit offscreen readback.
Movie controls pursue real targets, evade actual danger and choose earned cards.
This does not prove human input/fun, live frame pacing or phone qualification.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import shutil
from pathlib import Path
import subprocess
import time
import wave

import visual_stress
from profile import ROOT, digest

SOURCE = Path(__file__).resolve().with_suffix(".cpp")


def bundle_tree(root: Path = ROOT / "build/dev/Human Resources.app") -> dict:
    return {str(p.relative_to(root)): digest(p) for p in sorted(root.rglob("*")) if p.is_file()}


def run(args: argparse.Namespace) -> dict:
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    if shutil.disk_usage(output).free < 256 * 1024 * 1024:
        raise RuntimeError("Insufficient free disk for bounded native readbacks; preserve user save capacity first")
    before_bundle = bundle_tree()
    original_baseline = json.loads((ROOT / "build/evidence/quality-native-integration/user-app-before.json").read_text())["files"]
    if before_bundle != original_baseline:
        raise RuntimeError("Protected original user app differs from its recorded baseline")
    preliminary_path = ROOT / "build/evidence/quality-native-integration/Human Resources Gameplay Preview.app"
    preliminary_baseline = json.loads((preliminary_path.parent / "preview-qualified.json").read_text())["bundle_files"]
    if bundle_tree(preliminary_path) != preliminary_baseline:
        raise RuntimeError("Protected running Gameplay Preview differs from its staged baseline")
    executable, build = visual_stress.build(output, SOURCE)
    if args.build_only:
        return build
    source_before = visual_stress.state(executable, SOURCE)
    environment = {**os.environ, "SDL_ASSERT": "abort", "SDL_MAC_BACKGROUND_APP": "1", "SDL_WINDOW_ACTIVATE_WHEN_SHOWN": "0", "MTL_HUD_ENABLED": "0", "MTL_HUD_LOG_ENABLED": "0"}
    content = ROOT / "build/dev/assets/runtime/game"
    images = []
    if not args.boss_checkpoint and not args.extents and not args.composition:
        from PIL import Image
        with (output / "ui.log").open("w") as stream:
            ui = subprocess.Popen([str(executable), "--ui-review" if args.ui_only else "--ui", str(content), str(output)], cwd=ROOT, env=environment, stdout=stream, stderr=subprocess.STDOUT)
            preview = output / "actual-starter-run-390.rgba"
            announced = False
            ui_started = time.monotonic()
            while ui.poll() is None:
                if not announced and preview.exists() and preview.stat().st_size == 780 * 1688 * 4:
                    Image.frombytes("RGBA", (780,1688), preview.read_bytes()).save(preview.with_suffix(".png"))
                    print(json.dumps({"first_native_png": str(preview.with_suffix(".png")), "ui_pid": ui.pid}), flush=True)
                    announced = True
                if time.monotonic() - ui_started > 90:
                    ui.terminate()
                    ui.wait(timeout=5)
                    raise TimeoutError("Native UI proof exceeded bounded90seconds")
                time.sleep(.1)
            if ui.returncode:
                raise RuntimeError(f"Native UI proof exited{ui.returncode}; log preserved")
        images = []
        raw_files = sorted(p for p in output.glob("*.rgba") if p.name != "ui-return.rgba")
        expected = 8 if args.ui_only else 20
        if len(raw_files) != expected:
            raise RuntimeError(f"Expected exactly{expected} labelled UI fixtures, found{len(raw_files)}")
        for raw in raw_files:
            size = (640, 1136) if raw.name.endswith("-320.rgba") else (780, 1688)
            pixels = raw.read_bytes()
            if len(pixels) != size[0] * size[1] * 4:
                raise RuntimeError(f"Wrong native extent for {raw.name}")
            image = Image.frombytes("RGBA", size, pixels)
            path = raw.with_suffix(".png")
            image.save(path)
            decoded = Image.open(path).convert("RGBA").tobytes()
            if decoded != pixels:
                raise RuntimeError(f"Native PNG did not preserve pixels: {path}")
            images.append({"path": str(path.relative_to(ROOT)), "sha256": digest(path), "rgba_sha256": hashlib.sha256(pixels).hexdigest(), "width": size[0], "height": size[1], "synthetic": path.name.startswith("synthetic-")})
            raw.unlink()
        raw_return = output / "ui-return.rgba"
        if raw_return.stat().st_size != 45 * 390 * 844 * 4:
            raise RuntimeError("Short selection return must contain exactly45 native30fps frames")
        return_movie = output / "selection-return.mp4"
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-f", "rawvideo", "-pixel_format", "rgba", "-video_size", "390x844", "-framerate", "30", "-i", str(raw_return), "-an", "-c:v", "libx264", "-preset", "veryfast", "-crf", "18", "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(return_movie)], check=True, timeout=30)
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-xerror", "-nostdin", "-i", str(return_movie), "-f", "null", "-"], check=True, timeout=30)
        with (output / "ui-return.csv").open() as trace:
            return_rows = list(csv.DictReader(trace))
        if len(return_rows) != 90 or not any(r["hold"] == "0" and r["receipt"] == "1" and float(r["active_seconds"]) > float(return_rows[0]["active_seconds"]) for r in return_rows):
            raise RuntimeError("Actual Game must resume while the installed receipt remains visible")
        raw_return.unlink()
        if args.ui_only:
            report = {"scope": "UI-only epoch: real public-Game earned choice at320/390, injected text/layout/damage fixtures, selected receipt at0.3s after120ms hold, and45native-frame short selection-return sequence. No combat film rerun.", "build": build, "source_before": source_before, "source_after": visual_stress.state(executable, SOURCE), "images": images, "ui_return": {"path": str(return_movie.relative_to(ROOT)), "sha256": digest(return_movie), "frames":45, "fps":30, "trace":str((output / "ui-return.csv").relative_to(ROOT)), "active_seconds_start": return_rows[0]["active_seconds"], "active_seconds_end":return_rows[-1]["active_seconds"], "confirmation_hold_frames":sum(r["hold"] == "1" for r in return_rows), "nonblocking_receipt_frames":sum(r["hold"] == "0" and r["receipt"] == "1" for r in return_rows), "audio":"Silent visual proof; no audio qualification"}, "user_bundle_unchanged": before_bundle == bundle_tree(), "preliminary_preview_unchanged":preliminary_baseline == bundle_tree(preliminary_path), "limits":["View fixtures inject text preferences/damage presentation; no actual OS preference, human input, fun or phone qualification claim.", "Selection return uses actual earned Game choice plus product PromotionFlow; no account I/O or persistence-ledger qualification.", "All four staged app bundles are separately protected by the evidence launcher."]}
            report["source_stable"] = report["source_before"] == report["source_after"]
            (output / "qualified.json").write_text(json.dumps(report,indent=2)+"\n")
            if not report["source_stable"] or not report["user_bundle_unchanged"] or not report["preliminary_preview_unchanged"]:
                raise RuntimeError("UI epoch source or protected bundle changed")
            return {key:report[key] for key in ("source_stable","user_bundle_unchanged","preliminary_preview_unchanged","images","ui_return")}
    fifo = output / "movie.rgba"
    os.mkfifo(fifo)
    movie = output / "silent.mp4"
    encoder_command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-f", "rawvideo", "-pixel_format", "rgba", "-video_size", "390x844", "-framerate", "30", "-i", str(fifo), "-map", "0:v", "-an", "-c:v", "libx264", "-preset", "veryfast", "-crf", "18", "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(movie), "-map", "0:v", "-c:v", "copy", "-f", "hash", "-hash", "sha256", str(output / "native-frame-stream.sha256")]
    started = time.monotonic()
    with (output / "encoder.log").open("w") as encoder_log, (output / "native.log").open("w") as native_log:
        encoder = subprocess.Popen(encoder_command, stdin=subprocess.DEVNULL, stdout=encoder_log, stderr=subprocess.STDOUT)
        native_command = [str(executable), "--boss" if args.boss_checkpoint else "--extents" if args.extents else "--composition" if args.composition else "--movie", str(content), str(output)]
        if args.boss_checkpoint:
            native_command.append(str(args.boss_checkpoint.resolve()))
        native = subprocess.Popen(native_command, cwd=ROOT, env=environment, stdin=subprocess.DEVNULL, stdout=native_log, stderr=subprocess.STDOUT)
        print(json.dumps({"native_pid": native.pid, "encoder_pid": encoder.pid, "output": str(output)}), flush=True)
        try:
            native.wait(timeout=300)
            encoder.wait(timeout=90)
        finally:
            for process in (native, encoder):
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
        if native.returncode or encoder.returncode:
            raise RuntimeError(f"Native/encoder exits {native.returncode}/{encoder.returncode}; logs preserved")
    fifo.unlink()
    probe = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-count_frames", "-show_entries", "stream=codec_name,width,height,r_frame_rate,nb_read_frames,pix_fmt:format=duration", "-of", "json", str(movie)], text=True))
    with (output / "frames.csv").open() as stream:
        rows = list(csv.DictReader(stream))
    observed = {
        "forward_speed_range": [min(float(r["forward_speed"]) for r in rows),max(float(r["forward_speed"]) for r in rows)],
        "player_relative_z_range": [min(float(r["player_relative_z"]) for r in rows),max(float(r["player_relative_z"]) for r in rows)],
        "promotion_entry_frames": sum(float(r["promotion_entry"]) >= 0 for r in rows),
        "promotion_confirmation_frames": sum(r["promotion_hold"] == "1" for r in rows),
        "damage_hud_frames": sum(float(r["damage_age"]) >= 0 for r in rows),
    }
    ground_trace = None
    if args.composition:
        import math
        for i,start in enumerate(rows[:-24]):
            segment=rows[i:i+25]
            if start["ground_id"]=="0" or any(r["ground_id"]!=start["ground_id"] or r["ground_beam_owned"]!="0" or float(r["ground_capture_progress"])!=0 for r in segment):continue
            end=segment[-1]
            if float(end["active_seconds"])-float(start["active_seconds"])<.39:continue
            dx=float(end["ground_x"])-float(start["ground_x"]);dz=float(end["ground_z"])-float(start["ground_z"])
            away_x=float(start["ground_x"])-float(start["player_x"]);away_z=float(start["ground_z"])-float(start["runner_distance"])-float(start["player_relative_z"])
            distance=math.hypot(away_x,away_z);outward=(dx*away_x+dz*away_z)/max(.001,distance)
            if math.hypot(dx,dz)>.2 and outward>.05:
                ground_trace={"actor_id":int(start["ground_id"]),"start_frame":int(start["frame"]),"end_frame":int(end["frame"]),"start":[float(start["ground_x"]),float(start["ground_z"])],"end":[float(end["ground_x"]),float(end["ground_z"])],"displacement_metres":math.hypot(dx,dz),"initial_distance_metres":distance,"outward_displacement_metres":outward,"scope":"Actual position displacement of an actor never beam-owned during this fresh run, capture_progress0 throughout; velocity alone is not evidence."}
                break
        observed["actual_unbeamed_ground_movement"]=ground_trace
    frame_count = (len(rows) + 1) // 2
    video = probe["streams"][0]
    if (video["width"], video["height"], video["r_frame_rate"], int(video["nb_read_frames"])) != (390, 844, "30/1", frame_count):
        raise RuntimeError(f"Frame count/extent mismatch: {probe}")
    pcm_path = output / "cues.s16le"
    pcm = pcm_path.read_bytes()
    if len(pcm) != len(rows) * 800 * 2 * 2:
        raise RuntimeError("Source Mixer PCM does not align to all60Hz native frames")
    wav = output / "source-mixer.wav"
    with wave.open(str(wav), "wb") as audio:
        audio.setnchannels(2)
        audio.setsampwidth(2)
        audio.setframerate(48000)
        audio.writeframes(pcm)
    with wave.open(str(wav), "rb") as audio:
        if audio.readframes(audio.getnframes()) != pcm:
            raise RuntimeError("WAV wrapper changed source Mixer PCM")
    pcm_path.unlink()
    silent_movie = movie
    movie = output / "engagement.mp4"
    mux_command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-i", str(silent_movie), "-i", str(wav), "-map", "0:v", "-map", "1:a", "-c:v", "copy", "-c:a", "aac", "-b:a", "192k", "-af", "apad", "-t", str(frame_count / 30), "-movflags", "+faststart", str(movie)]
    subprocess.run(mux_command, check=True, timeout=120)
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-xerror", "-nostdin", "-i", str(movie), "-f", "null", "-"], check=True, timeout=120)
    final_probe = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-count_frames", "-show_entries", "stream=codec_name,codec_type,width,height,r_frame_rate,nb_read_frames,pix_fmt,sample_rate,channels:format=duration", "-of", "json", str(movie)], text=True))
    final_video = next(s for s in final_probe["streams"] if s["codec_type"] == "video")
    final_audio = next(s for s in final_probe["streams"] if s["codec_type"] == "audio")
    if int(final_video["nb_read_frames"]) != frame_count or final_audio["sample_rate"] != "48000" or final_audio["channels"] != 2:
        raise RuntimeError("Final mux changed native frames or synchronized audio format")
    silent_movie.unlink()
    source_after = visual_stress.state(executable, SOURCE)
    native_lines = (output / "native.log").read_text().splitlines()
    result = next(json.loads(line) for line in reversed(native_lines) if line.startswith('{"frames":'))
    report = {"scope": ("Actual compatible public-Game boss checkpoint restored without state edits, followed for at most12active seconds or natural death. " if args.boss_checkpoint else ("Actual public Game5 fresh start with held forward for5active seconds then held back through15active seconds or natural death; no position injection. " if args.extents else "Actual public Game6 short encounter-composition route: held forward0.7s, neutral0.6s, then ordinary pursuit through8active seconds; no injected actor/player positions. " if args.composition else "Actual public Game5 fresh start followed for up to120active seconds or natural death with programmatic nearest-target movement, voluntary/real-danger starter Dash and first earned upgrade selections. PromotionFlow controls entry/selection presentation and120ms combat hold. UI fixtures explicitly labelled. ")) + "Offscreen native SDL_GPU rendering with source Mixer audio; no human-input/live-pacing claim.", "build": build, "source_stable": source_before == source_after, "source_before": source_before, "source_after": source_after, "user_bundle_unchanged": before_bundle == bundle_tree(), "user_bundle_files": len(before_bundle), "preliminary_preview_unchanged": preliminary_baseline == bundle_tree(preliminary_path), "preliminary_preview_files": len(preliminary_baseline), "images": images, "movie": str(movie.relative_to(ROOT)), "movie_sha256": digest(movie), "native_rgba_stream_hash": (output / "native-frame-stream.sha256").read_text().strip(), "raw_stream_bytes": frame_count * 390 * 844 * 4, "frame_count": frame_count, "probe": final_probe, "source_mixer_wav": {"path": str(wav.relative_to(ROOT)), "sha256": digest(wav), "pcm_sha256": hashlib.sha256(pcm).hexdigest(), "frames": len(rows) * 800, "scope": "Offline exact product Mixer synthesis and source feedback coalescing; no OS output-device/listening qualification"}, "full_decode": "pass", "wall_seconds_including_native_capture_encode": time.monotonic() - started, "outcome": result, "observed": observed, "checkpoints": [{"path": str(p.relative_to(ROOT)), "bytes": p.stat().st_size, "sha256": digest(p)} for p in sorted(output.glob("*.hrs"))], "boss_continuation": {"path": str(args.boss_checkpoint), "sha256": digest(args.boss_checkpoint)} if args.boss_checkpoint else None, "commands": {"native": native_command, "encode": encoder_command, "audio_mux": mux_command}, "limits": ["The film records fixed60Hz public-Game simulation, rendered every second step and encoded30fps without interpolated frames.", "The route is an active scripted pursuit policy, not human pacing or input evidence.", "Synthetic View fixtures do not prove naturally earned maximum gear or delivered OS text preferences.", "No primary save, installed bundle refresh, signing, packaging or store action occurred."]}
    (output / "qualified.json").write_text(json.dumps(report, indent=2) + "\n")
    if not report["source_stable"] or not report["user_bundle_unchanged"] or not report["preliminary_preview_unchanged"]:
        raise RuntimeError("Source or user bundle changed; retained report is not qualified")
    return {key: report[key] for key in ("source_stable", "user_bundle_unchanged", "preliminary_preview_unchanged", "frame_count", "outcome", "movie", "images")}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--build-only", action="store_true")
    parser.add_argument("--ui-only", action="store_true", help="Render only labelled UI fixtures and short earned selection return; skip combat film")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--boss-checkpoint", type=Path, help="Restore an actual compatible public-Game boss checkpoint for a bounded12active-second proof")
    modes.add_argument("--extents", action="store_true", help="Fresh actual Game: hold forward5active seconds then back through15active seconds")
    modes.add_argument("--composition", action="store_true", help="Fresh current Game: forward0.7s/neutral0.6s then ordinary pursuit through8active seconds")
    args = parser.parse_args()
    print(json.dumps(run(args), indent=2))


if __name__ == "__main__":
    main()
