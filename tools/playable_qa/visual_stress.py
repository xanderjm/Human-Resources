"""Build/run one labelled native visual-capacity fixture without CMake edits.

Run only after the coordinator grants the foreground/GPU slot. --build-only
compiles the standalone tool and executes portable scene checks without SDL GPU.
The production application and gameplay rules are never patched or replaced.
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import tempfile
import time

from profile import ROOT, digest, distribution, metal_hud, process_memory, snapshot

BUILD = ROOT / "build/dev"
SOURCE = Path(__file__).resolve().with_suffix(".cpp")
FRAMES = 1440


def state(executable: Path) -> dict:
    result = snapshot(executable)
    extra = [SOURCE, Path(__file__).resolve(), BUILD / "generated/gameplay/include/human_resources/gameplay/tuning.h"]
    extra += list(SOURCE.parent.glob("*.h"))
    extra += list((BUILD / "assets/shaders").rglob("*"))
    result["files"].update({str(path.relative_to(ROOT)): digest(path) for path in extra if path.is_file()})
    return result


def build(output: Path) -> tuple[Path, dict]:
    commands = json.loads((BUILD / "compile_commands.json").read_text())
    app = next(entry for entry in commands if entry["file"].endswith("/human_resources_client/src/app.cpp"))
    compile_command = shlex.split(app["command"])
    bounds = set(re.findall(r"view\.shots\.size\(\) < (\d+)", (ROOT / "native/human_resources_gameplay/src/combat.cpp").read_text()))
    if len(bounds) != 1:
        raise ValueError("Production shot bound changed; review the synthetic fixture before compiling")
    shot_cap = int(bounds.pop())
    obj = output / "visual_stress.cpp.o"
    executable = output / "visual_stress"
    compile_command[compile_command.index("-o") + 1] = str(obj)
    compile_command[compile_command.index(app["file"])] = str(SOURCE)
    compile_command += [f"-DHR_QA_SHOT_CAP={shot_cap}", "-I" + str(BUILD / "generated/gameplay/include"), "-I" + str(ROOT / "native/human_resources_gameplay/src"), "-fno-access-control"]
    # Ninja's commands tool reads the existing graph; it performs no build or
    # regeneration. Reuse exact current library/framework linkage in isolation.
    ninja = shutil.which("ninja")
    if not ninja:
        raise RuntimeError("The configured native Ninja tool is required")
    lines = subprocess.check_output([ninja, "-C", str(BUILD), "-t", "commands", "human_resources_player"], text=True).splitlines()
    line = next(line for line in reversed(lines) if " -o human_resources_player " in line and " -c " not in line)
    link = shlex.split(line)
    if link[:2] == [":", "&&"]:
        link = link[2:]
    if "&&" in link:
        link = link[:link.index("&&")]
    old_objects = [i for i, token in enumerate(link) if token.endswith("human_resources_client/src/app.cpp.o")]
    if len(old_objects) != 1:
        raise ValueError("Native client link layout changed; refusing an ambiguous replacement")
    link[old_objects[0]] = str(obj)
    link[link.index("-o") + 1] = str(executable)
    libraries = [BUILD / token for token in link if token.endswith(".a")]
    library_hashes = {str(path.relative_to(ROOT)): digest(path) for path in libraries}
    source_before = state(ROOT / "build/dev/human_resources_player")
    with (output / "build.log").open("w") as stream:
        subprocess.run(compile_command, cwd=app["directory"], stdout=stream, stderr=subprocess.STDOUT, check=True)
        subprocess.run(link, cwd=BUILD, stdout=stream, stderr=subprocess.STDOUT, check=True)
    # Production shader reads use SDL_GetBasePath/assets; provide an isolated
    # read-only reference to the exact cooked tree, without duplicating 89 MiB.
    (output / "assets").symlink_to(BUILD / "assets", target_is_directory=True)
    validation = subprocess.check_output([str(executable), "--validate-only"], text=True)
    (output / "fixture-validation.log").write_text(validation)
    after_libraries = {str(path.relative_to(ROOT)): digest(path) for path in libraries}
    metadata = {"compile": compile_command, "link": link, "source_sha256": digest(SOURCE), "shot_cap_from_production": shot_cap, "library_sha256": library_hashes, "libraries_stable": library_hashes == after_libraries, "source_stable": source_before == state(ROOT / "build/dev/human_resources_player"), "fixture_validation": validation, "compiler": subprocess.check_output([compile_command[0], "--version"], text=True).splitlines()[0]}
    (output / "build.json").write_text(json.dumps(metadata, indent=2) + "\n")
    if not metadata["libraries_stable"] or not metadata["source_stable"]:
        raise RuntimeError("Source/libraries changed during standalone build; wait for the coordinated freeze")
    return executable, metadata


def summarize(rows: list[dict]) -> dict:
    timings = ("frame_interval_ms", "presentation_ms", "render_submit_ms", "cpu_frame_ms", "cpp_allocations", "cpp_allocated_bytes")
    result = {}
    for theme in range(3):
        # Each theme's first30frames retain separate first-use data. Resource
        # churn every120frames remains in measured samples, never excluded.
        local = [row for row in rows if int(row["theme"]) == theme]
        result[str(theme)] = {"all": {field: distribution([float(row[field]) for row in local]) for field in timings}, "after_first_30": {field: distribution([float(row[field]) for row in local[30:]]) for field in timings}, "hitch_rows": [row for row in local if float(row["frame_interval_ms"]) > 25 or float(row["cpu_frame_ms"]) > 25], "normal_peak_scene_instances": max(int(row["scene_instances"]) for row in local if row["reduced_motion"] == "0"), "reduced_peak_scene_instances": max(int(row["scene_instances"]) for row in local if row["reduced_motion"] == "1"), "peak_world_draws": max(int(row["world_draws"]) for row in local), "peak_shadow_draws": max(int(row["shadow_draws"]) for row in local), "peak_triangles": max(int(row["triangles"]) for row in local)}
    return result


def visual_proof(executable: Path, output: Path, metadata: dict, mode: str = "--visual-proof") -> None:
    before = state(executable)
    command = [str(executable), mode, str(BUILD / "assets/runtime/game"), str(output)]
    environment = {**os.environ, "SDL_ASSERT": "abort", "MTL_HUD_ENABLED": "0", "MTL_HUD_LOG_ENABLED": "0"}
    with (output / "native-visual-proof.log").open("w") as stream:
        subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, env=environment, timeout=45, check=True)
    images = []
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError("FFmpeg is required to preserve and verify native readbacks")
    raw_files = sorted(output.glob("*.rgba"))
    expected = {"--milestone-proof": 18, "--composition-proof": 2, "--visual-proof": 7, "--readable-ui-proof": 8, "--privacy-proof": 6}[mode]
    if len(raw_files) != expected:
        raise ValueError(f"Expected {expected} explicitly labelled native proof frames, found {len(raw_files)}")
    width, height = (320, 568) if mode in ("--readable-ui-proof", "--privacy-proof") else (588, 1280)
    for raw in raw_files:
        if raw.stat().st_size != width * height * 4:
            raise ValueError(f"Invalid native readback size: {raw}")
        original_hash = digest(raw)
        png = raw.with_suffix(".png")
        subprocess.run([ffmpeg, "-v", "error", "-f", "rawvideo", "-pixel_format", "rgba", "-video_size", f"{width}x{height}", "-i", str(raw), "-frames:v", "1", str(png)], check=True)
        decoded = subprocess.check_output([ffmpeg, "-v", "error", "-i", str(png), "-f", "rawvideo", "-pix_fmt", "rgba", "-"])
        if len(decoded) != raw.stat().st_size or hashlib.sha256(decoded).hexdigest() != original_hash:
            raise ValueError(f"PNG pixel verification failed; preserving raw capture {raw}")
        # A complete scene is far from a clear colour or blank readback. Keep
        # this bounded pixel check separate from the coordinator's art review.
        colours = set(decoded[index:index + 3] for index in range(0, len(decoded), 64))
        if len(colours) < 100:
            raise ValueError(f"Native readback appears blank/degenerate; preserving raw {raw}")
        images.append({"path": str(png), "sha256": digest(png), "decoded_rgba_sha256": original_hash, "width": width, "height": height, "sampled_distinct_rgb": len(colours)})
        raw.unlink()
    stable = before == state(executable)
    milestone = mode == "--milestone-proof"
    scope = "Native SYNTHETIC infantry/spatial/Farmland milestone: product::scene at 17m, stations 0/33.6, full-gear extremes, legal candidate threat poses and a separately forced fan/cactus case" if milestone else "Native visual proof: production starter plus SYNTHETIC six-slot/two-evolution gear and saturated-capacity compositions; no gameplay or timing claim"
    limits = ["All milestone positions/loadouts are labelled synthetic. They do not prove naturally earned gear, a natural encounter, human input/fun or performance.", "The forced cactus uses the existing outer-shotgun-lane unit fixture whose aim lies outside the current player band; it is geometric protection evidence only.", "Legal candidate threat poses use the actual source scenery at seed514/station33.6; the native log records the number of faded props. Alpha alone does not establish target/shadow readability."] if milestone else ["Gear composition retains six capture targets and an authoritative-radius EMP; clears military/projectiles/shots and other transient beats for readability.", "Capacity composition uses the full measured fixture, including all 55 actors, 96 projectiles, 64 shots and 48 cached beats."]
    if mode == "--composition-proof":
        scope = "Native SYNTHETIC targeted Farmland composition comparison: seed514 stations0/33.6, actual product::scene17m camera and unchanged starter poses from the earlier proof"
        limits = ["Two fixed native composition snapshots only; no natural encounter, moving occlusion, human input, animation acceptance or timing claim.", "Starter poses and UI match the earlier labelled native proof, including its zero-time hint and unadvanced actor positions. Compare equal displayed widths without stretching."]
    if mode == "--readable-ui-proof":
        scope = "Native SYNTHETIC corrected readable UI: eight actual320x568 frames, ordinary and maximum one-row SCORE HUD, scale3.2 actual-name-first direct previews1/2/3, actualchoice2 continuation and protected recovery"
        limits = ["Injected View preferences and labelled snapshot equipment/offers; no actual OS text-preference delivery, touch, earned progression or recreation claim. Mobile interaction qualification is separate.", "Complete text/choice identity/recovery exclusivity is also checked by focused portable tests; coordinator must inspect native pixels."]
    if mode == "--privacy-proof":
        scope = "Native320x568 Privacy Settings entry and actual reader first/last at default and largest3.2 text scale"
        limits = ["Six labelled View snapshots, not actual OS preference or touch/Back routing. Complete policy copy and routing retain focused portable coverage."]
    limits.append("Native readback timings are excluded from performance evidence; PNGs were decoded and pixel-hash matched before redundant raw removal. Nonblank checks do not establish art acceptance.")
    report = {"scope": scope, "source_stable": stable, "build": metadata, "command": command, "images": images, "source": before, "limits": limits}
    (output / "visual-proof.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"scope": report["scope"], "source_stable": stable, "images": images}, indent=2))
    if not stable:
        raise RuntimeError("Source changed during native visual proof")


def infantry_transition(executable: Path, output: Path, metadata: dict, vehicle: bool = False, shots: bool = False) -> None:
    before = state(executable)
    movie = output / ("READABILITY-vehicle-shots.mp4" if shots else "SCRIPTED-vehicle-transitions.mp4" if vehicle else "SCRIPTED-infantry-transitions.mp4")
    log = output / "native-transition.log"
    with tempfile.TemporaryDirectory(prefix="transition-", dir=output) as temporary:
        fifo = Path(temporary) / "frames.rgba"
        os.mkfifo(fifo)
        encode = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-f", "rawvideo", "-pixel_format", "rgba", "-video_size", ("320x696" if shots else "588x1280"), "-framerate", "60", "-i", str(fifo), "-an", "-c:v", "libx264", "-preset", "fast", "-crf", "18", "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(movie)]
        encoder = subprocess.Popen(encode, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        command = [str(executable), ("--vehicle-shot-proof" if shots else "--vehicle-transition" if vehicle else "--infantry-transition"), str(BUILD / "assets/runtime/game"), str(fifo)]
        try:
            with log.open("w") as stream:
                subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=90)
            _, errors = encoder.communicate(timeout=60)
            if encoder.returncode:
                raise RuntimeError(f"Transition encoder failed: {errors}")
        finally:
            if encoder.poll() is None:
                encoder.kill()
                encoder.wait()
    probe = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-count_frames", "-show_entries", "stream=width,height,nb_read_frames,r_frame_rate:format=duration", "-of", "json", str(movie)], text=True))
    stream = probe["streams"][0]
    if (int(stream["width"]), int(stream["height"]), int(stream["nb_read_frames"]), stream["r_frame_rate"]) != ((320, 696, 216, "60/1") if shots else (588, 1280, 1020 if vehicle else 720, "60/1")):
        raise ValueError(f"Scripted transition movie violates the 720-frame contract: {probe}")
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-xerror", "-nostdin", "-i", str(movie), "-f", "null", "-"], check=True, timeout=60)
    stable = before == state(executable)
    report = {"scope": "SCRIPTED NATIVE TRANSITIONS: typed three-role warning, one enemy-shot outcome per actor, firing-time launch bridge, recoil/recovery and stride-matched travel. All three bodies then replay one actual public-Game pig capture/range-loss trace including its immediate positive-to-zero reset; not a natural three-role capture or earned encounter.", "source_stable": stable, "build": metadata, "command": command, "encode": encode, "probe": probe, "full_decode": "pass", "movie_sha256": digest(movie), "native_log_sha256": digest(log), "source": before, "limits": ["Silent fixed-step native visual proof, no live performance, listening, human input/fun, physical mobile or progression qualification.", "The source trace is generated through neutral-input unmodified Game seed514 and source kind/id/reset times are retained in the native log. The role skins, three-beam composition and scripted firing are explicit fixtures.", "No reversed/smoothed capture progress or fake successful-capture outcome. The actual immediate loss is passed to the production presentation owner."]}
    if vehicle:
        report["scope"] = "SCRIPTED INITIAL GAME FIXTURE: actual public Game movement/fire/capture/range-loss/full return for jeep/tank; actual boss fan. Synthetic initial damaged400/4000 vehicles and Winch3/Splitter2; selected-source-only composition."
        report["limits"] = ["Fixed60Hz native readback proof, not live performance or earned/natural encounter/mobile qualification.", "Only initial private Game fixture is seeded; subsequent public Game advance owns all progress, warning, shots, immediate loss and return clocks. Native log binds actual frame/outcomes.", "Display isolates the selected real source actor/projectiles/beams; other naturally spawned actors and player shots are hidden for review, not removed from simulation."]
    if shots:
        report["scope"] = "Native320px optical enemy-shot readability correction:216frames/60Hz, jeep/tank/boss actual Game shot windows matching accepted initial fixture sourceframes60..131. Finite projectile geometry and all authority unchanged; no capture clip rerender."
        report["limits"] = ["Synthetic initial source fixture and isolated composition, not earned encounter/mobile/performance evidence.", "Native320x696 after proof; before sourceframes remain in the accepted17s capture. Readability acceptance requires coordinator inspection at equal displayed320px width."]
    (output / "transition-proof.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"movie": str(movie), "source_stable": stable, "probe": probe, "full_decode": "pass"}))
    if not stable:
        raise RuntimeError("Source changed during scripted transition proof")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--build-only", action="store_true")
    mode.add_argument("--visual-proof", action="store_true")
    mode.add_argument("--milestone-proof", action="store_true")
    mode.add_argument("--infantry-transition", action="store_true")
    mode.add_argument("--vehicle-transition", action="store_true")
    mode.add_argument("--vehicle-shot-proof", action="store_true")
    mode.add_argument("--privacy-proof", action="store_true")
    mode.add_argument("--composition-proof", action="store_true")
    mode.add_argument("--readable-ui-proof", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    executable, metadata = build(output)
    if args.build_only:
        print(metadata["fixture_validation"], end="")
        return
    if args.infantry_transition or args.vehicle_transition or args.vehicle_shot_proof:
        infantry_transition(executable, output, metadata, args.vehicle_transition, args.vehicle_shot_proof)
        return
    if args.visual_proof or args.milestone_proof or args.composition_proof or args.readable_ui_proof or args.privacy_proof:
        proof_mode = "--privacy-proof" if args.privacy_proof else "--readable-ui-proof" if args.readable_ui_proof else "--composition-proof" if args.composition_proof else "--milestone-proof" if args.milestone_proof else "--visual-proof"
        visual_proof(executable, output, metadata, proof_mode)
        return
    before = state(executable)
    (output / "source-before.json").write_text(json.dumps(before, indent=2) + "\n")
    metrics = output / "frames.csv"
    log = output / "native.log"
    timed = output / "process-time.log"
    command = ["/usr/bin/time", "-l", "-o", str(timed), str(executable), str(BUILD / "assets/runtime/game"), str(metrics)]
    environment = {**os.environ, "SDL_ASSERT": "abort", "MTL_HUD_ENABLED": "1", "MTL_HUD_LOG_ENABLED": "1"}
    memory = []
    started = time.monotonic()
    with log.open("w") as stream:
        process = subprocess.Popen(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, env=environment, start_new_session=True)
        try:
            while process.poll() is None:
                if time.monotonic() - started > 90:
                    raise TimeoutError("Bounded visual stress exceeded90seconds")
                memory.extend({"elapsed_seconds": time.monotonic() - started, **sample} for sample in process_memory(process.pid))
                time.sleep(.5)
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
    with metrics.open() as stream:
        rows = list(csv.DictReader(stream))
    after = state(executable)
    stable = before == after
    six_beams = all(int(row["beams"]) == 6 for row in rows)
    report = {"schema": 1, "timestamp_utc": datetime.now(timezone.utc).isoformat(), "scope": "SYNTHETIC VISUAL CAPACITY: production scene/effects/renderer, controlled Snapshot, not natural survival/gameplay/fun/input", "frames": len(rows), "requested_frames": FRAMES, "source_stable": stable, "exit_code": process.returncode, "six_beams_every_frame": six_beams, "build": metadata, "summary": summarize(rows), "metal_hud": metal_hud(log.read_text()), "gpu_units_source": "https://developer.apple.com/videos/play/tech-talks/110339/", "memory_samples": memory, "process_time": timed.read_text(), "source_after": after, "limits": ["One host with Metal HUD observer overhead; qualitative comparison only with earlier fresh gameplay snapshot.", "Simultaneous vector capacities are synthetic and do not establish a naturally reachable combat state.", "C++ allocation probe counts allocations/bytes on the render thread only; excludes SDL C allocation and GPU driver/private allocations. It does not report live heap size.", "Equipment geometry is exactly what the linked production scene implements. Populated module/evolution fields alone do not prove every visual identity; inspect the recorded starter/stress mesh proof.", "No raw readback/capture, no storage access, no gameplay mutation, no physical-phone or indefinite-memory qualification."]}
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in ("scope", "frames", "source_stable", "six_beams_every_frame", "summary", "metal_hud")}, indent=2))
    if process.returncode or len(rows) != FRAMES or not stable or not six_beams:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
