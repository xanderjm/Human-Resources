#!/usr/bin/env python3
"""Measure the unchanged textured cannon bore; never modify an authoring asset.

Run with build/tools/art-python/bin/python3. The inspected distal dark bore is
selected through texture and topology, not the biased outer-cap bounding box.
All coordinates have the original GLB node hierarchy applied exactly once.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from normalize_glb import ROOT, points, unpack
from render_prop_review import primitives

SOURCE = ROOT / "assets/models/tank-turret/tank-turret.glb"
MISSILE = ROOT / "assets/models/missile/missile.glb"
REVIEW = ROOT / "assets/production/reviews/assembly-tank-with-turret-final-front.png"
EXPECTED_SOURCE = "d99d8b1a49dac32adacae4e806434b0d685dc407d7515cc47816c1f9e650c4e0"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def boundary_loop(positions, triangles):
    # Texture seams duplicate vertices. Weld at one micrometre solely for
    # topology; retain the original double-precision normalized coordinates.
    welded = {}
    indices = []
    coordinates = []
    for p in positions:
        key = tuple(np.round(p, 6))
        if key not in welded:
            welded[key] = len(welded)
            coordinates.append(p)
        indices.append(welded[key])
    counts = Counter(tuple(sorted((indices[int(a)], indices[int(b)])))
                     for tri in triangles for a, b in zip(tri, np.roll(tri, -1)))
    if any(n > 2 for n in counts.values()):
        raise ValueError("Bore patch has a nonmanifold edge")
    boundary = [edge for edge, count in counts.items() if count == 1]
    neighbours = defaultdict(list)
    for a, b in boundary:
        neighbours[a].append(b)
        neighbours[b].append(a)
    if not neighbours or any(len(adj) != 2 for adj in neighbours.values()):
        raise ValueError("Bore boundary is not a closed simple loop")
    loop = [min(neighbours)]
    while True:
        nxt = next(n for n in neighbours[loop[-1]] if len(loop) == 1 or n != loop[-2])
        if nxt == loop[0]:
            break
        if nxt in loop:
            raise ValueError("Bore boundary self-repeats")
        loop.append(nxt)
    if len(loop) != len(boundary):
        raise ValueError("Bore patch has multiple boundaries")
    polygon = np.asarray(coordinates)[loop]
    xy = polygon[:, :2]
    signed = np.sum(xy[:, 0] * np.roll(xy[:, 1], -1) - np.roll(xy[:, 0], -1) * xy[:, 1])
    return polygon if signed > 0 else polygon[::-1]


def polygon_centroid(polygon):
    xy = polygon[:, :2]
    nxt = np.roll(xy, -1, axis=0)
    cross = xy[:, 0] * nxt[:, 1] - nxt[:, 0] * xy[:, 1]
    area = np.sum(cross) / 2
    if area <= 0:
        raise ValueError("Expected counterclockwise positive-area aperture")
    return np.sum((xy + nxt) * cross[:, None], axis=0) / (6 * area), float(area)


def point_triangles_distance(point, triangles):
    """Independent Euclidean closest distance to every triangle, including edges."""
    a, b, c = triangles[:, 0], triangles[:, 1], triangles[:, 2]
    best = np.full(len(triangles), np.inf)
    for start, end in ((a, b), (b, c), (c, a)):
        edge = end - start
        den = np.sum(edge * edge, axis=1)
        u = np.divide(np.sum((point - start) * edge, axis=1), den,
                      out=np.zeros(len(edge)), where=den > 1e-24)
        closest = start + np.clip(u, 0, 1)[:, None] * edge
        best = np.minimum(best, np.linalg.norm(closest - point, axis=1))
    ab, ac = b - a, c - a
    normal = np.cross(ab, ac)
    den = np.sum(normal * normal, axis=1)
    signed = np.sum((point - a) * normal, axis=1)
    projected = point - np.divide(signed, den, out=np.zeros(len(den)), where=den > 1e-24)[:, None] * normal
    d00, d01, d11 = np.sum(ab * ab, 1), np.sum(ab * ac, 1), np.sum(ac * ac, 1)
    ap = projected - a
    d20, d21 = np.sum(ap * ab, 1), np.sum(ap * ac, 1)
    denom = d00 * d11 - d01 * d01
    u = np.divide(d11 * d20 - d01 * d21, denom, out=np.zeros(len(denom)), where=denom > 1e-24)
    v = np.divide(d00 * d21 - d01 * d20, denom, out=np.zeros(len(denom)), where=denom > 1e-24)
    inside = (denom > 1e-24) & (u >= -1e-12) & (v >= -1e-12) & (u + v <= 1 + 1e-12)
    plane = np.divide(np.abs(signed), np.sqrt(den), out=np.full(len(den), np.inf), where=den > 1e-24)
    return np.minimum(best, np.where(inside, plane, np.inf))


def annotate(path, contour, rear_contour, centre, radius):
    image = Image.new("RGB", (1120, 570), "#f6f7f2")
    image.paste(Image.open(REVIEW).convert("RGB"), (0, 42))
    draw = ImageDraw.Draw(image)
    draw.text((18, 14), "Existing assembly front review; annotation only", fill="#263648")
    draw.line((470, 92, 248, 280), fill="#e65343", width=2)
    draw.text((348, 72), "Measured dark bore", fill="#b9342b")
    origin = np.array([810, 295])
    scale = 1450

    def screen(p):
        p = (np.asarray(p)[:2] - centre) * np.array([scale, -scale]) + origin
        return tuple(map(float, p))

    front = [screen(p) for p in contour]
    rear = [screen(p) for p in rear_contour]
    draw.polygon(front, fill="#dce8ec")
    draw.line(front + front[:1], fill="#177e73", width=3)
    draw.line(rear + rear[:1], fill="#9b88af", width=2)
    r = radius * scale
    draw.ellipse((origin[0]-r, origin[1]-r, origin[0]+r, origin[1]+r), outline="#e65343", width=2)
    draw.line((origin[0]-5, origin[1], origin[0]+5, origin[1]), fill="#263648", width=2)
    draw.line((origin[0], origin[1]-5, origin[0], origin[1]+5), fill="#263648", width=2)
    for index, p in enumerate(front):
        draw.text((p[0]+5, p[1]+3), str(index), fill="#263648")
    draw.text((570, 20), "Actual source XY projection, +Z out of page; CCW inner rim", fill="#263648")
    draw.text((570, 465), "Mint: ten-edge aperture / purple: dark back-disk boundary", fill="#263648")
    draw.text((570, 487), "Cross: polygon area centroid / coral: guarded finite missile radius", fill="#263648")
    draw.text((570, 509), "Nonplanar rim retained; this diagram is not a flat-cap assumption.", fill="#263648")
    image.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/vehicle-assembly-audit/tank-aperture.json")
    args = parser.parse_args()
    inputs = (SOURCE, MISSILE, REVIEW, Path(__file__), ROOT / "tools/normalize_glb.py",
              ROOT / "tools/render_prop_review.py")
    source_hashes = {str(p.relative_to(ROOT)): sha(p) for p in inputs}
    if source_hashes[str(SOURCE.relative_to(ROOT))] != EXPECTED_SOURCE:
        raise ValueError("Reviewed source changed; re-inspect the feature before updating its measurement")
    # Known plane, exterior-edge and degenerate-line cases independently check
    # the distance primitive used for the conservative whole-missile bound.
    fixture = np.array([[[0., 0, 0], [1, 0, 0], [0, 1, 0]]])
    for test_point, expected in (([.25, .25, 1], 1), ([-1., 0, 0], 1)):
        if abs(point_triangles_distance(np.array(test_point), fixture)[0]-expected) > 1e-12:
            raise ValueError("Triangle-distance control case failed")
    if abs(point_triangles_distance(np.array([.5, 1., 0]), np.array([[[0., 0, 0], [1, 0, 0], [1, 0, 0]]]))[0]-1) > 1e-12:
        raise ValueError("Degenerate triangle-distance control case failed")
    parts = list(primitives(*unpack(SOURCE)))
    if len(parts) != 1:
        raise ValueError("Reviewed turret primitive topology changed")
    pos, uv, indices, texture = parts[0]
    normalized, _ = points(*unpack(SOURCE))
    if not np.array_equal(pos, normalized):
        raise ValueError("Node-normalization boundary differs from canonical points()")
    selected = []
    colours = []
    for ti, triangle in enumerate(indices):
        # Inspected feature: the 130mm distal cannon band, with original dark
        # bore albedo. Centroid UV samples avoid texture-island seam bleed.
        if np.min(pos[triangle, 2]) < np.max(pos[:, 2]) - .13:
            continue
        sample = np.mean(uv[triangle], axis=0)
        x, y = np.clip((sample * [texture.shape[1]-1, texture.shape[0]-1]).astype(int),
                       [0, 0], [texture.shape[1]-1, texture.shape[0]-1])
        rgb = texture[y, x]
        if np.max(rgb) < 55:
            selected.append(ti)
            colours.append(rgb.tolist())
    if len(selected) != 30:
        raise ValueError("Expected inspected 10-disk/20-wall dark bore triangles")
    triangles = pos[indices[selected]]
    normals = np.cross(triangles[:, 1]-triangles[:, 0], triangles[:, 2]-triangles[:, 0])
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    rear_ids = [ti for ti, n in zip(selected, normals) if n[2] > .9]
    if len(rear_ids) != 10 or np.max(np.abs(normals[normals[:, 2] <= .9, 2])) > .1:
        raise ValueError("Selected patch is not the reviewed forward disk and inward bore walls")
    contour = boundary_loop(pos, indices[selected])
    rear = boundary_loop(pos, indices[rear_ids])
    if len(contour) != 10 or len(rear) != 10:
        raise ValueError("Expected both actual ten-sided bore contours")
    centre, area = polygon_centroid(contour)
    sides = np.roll(contour[:, :2], -1, axis=0) - contour[:, :2]
    turns = sides[:, 0]*np.roll(sides[:, 1], -1)-sides[:, 1]*np.roll(sides[:, 0], -1)
    if not np.all(turns > 0):
        raise ValueError("Inscribed-circle proof requires the actual convex inner aperture")
    wall_triangles = triangles[normals[:, 2] <= .9]
    projected_walls = wall_triangles.copy()
    projected_walls[:, :, 2] = 0
    bore_clearance = float(np.min(point_triangles_distance(np.r_[centre, 0], projected_walls)))
    edge_triangles = np.stack([contour, np.roll(contour, -1, axis=0), contour], axis=1)
    edge_triangles[:, :, 2] = 0
    aperture_clearance = float(np.min(point_triangles_distance(np.r_[centre, 0], edge_triangles)))
    # Keep the true aperture centroid and nonplanar contour, then choose a
    # separately named conservative centre-launch plane beyond the terminal rim.
    launch = np.r_[centre, np.max(contour[:, 2]) + .01]
    distances = point_triangles_distance(launch, pos[indices])
    closest_id = int(np.argmin(distances))
    launch_clearance = float(distances[closest_id])
    missile, _ = points(*unpack(MISSILE))
    radial = float(np.max(np.linalg.norm(missile[:, :2], axis=1)))
    bounding_ball = float(np.max(np.linalg.norm(missile, axis=1)))
    scale = .20
    missile_radius = radial * scale
    sphere_radius = bounding_ball * scale
    safety_guard = .005
    if min(bore_clearance, aperture_clearance)-missile_radius < safety_guard or launch_clearance-sphere_radius < safety_guard:
        raise ValueError("Candidate finite missile does not retain the required five-millimetre geometry guard")
    max_uniform_scale = min((bore_clearance-safety_guard)/radial, (launch_clearance-safety_guard)/bounding_ball)
    bestfit = np.linalg.lstsq(np.c_[contour[:, :2], np.ones(len(contour))], contour[:, 2], rcond=None)[0]
    fit_z = float(np.r_[centre, 1] @ bestfit)
    result = {
        "schema": 1, "status": "source_geometry_measured_not_runtime_qualification",
        "source_sha256": source_hashes,
        "coordinates": "Normalized source metres; node hierarchy applied once; +Y up, +Z forward.",
        "selection": {"distal_band_metres": .13, "centroid_rgb_max_exclusive": 55,
                      "dark_patch_triangle_ids": selected, "dark_back_disk_triangle_ids": rear_ids,
                      "centroid_rgb_samples": colours, "weld_metres": .000001,
                      "topology": "One connected 30-triangle dark bore with one ten-edge CCW inner boundary; ten back-disk and twenty wall triangles."},
        "inner_aperture": {"contour_ccw_m": contour.tolist(), "projected_area_m2": area,
                           "area_centroid_xy_m": centre.tolist(), "forward": [0, 0, 1],
                           "centroid_on_bestfit_rim_plane_m": [*centre.tolist(), fit_z],
                           "rim_z_bounds_m": [float(np.min(contour[:, 2])), float(np.max(contour[:, 2]))],
                           "centred_inscribed_radius_m": aperture_clearance,
                           "note": "Centroid is derived from the actual inner ten-sided area, not the outer cap or a distal vertex box. Rim is nonplanar; the best-fit point is a datum, not a flat physical surface."},
        "bore": {"back_disk_boundary_ccw_m": rear.tolist(),
                 "projected_inner_wall_clearance_m": bore_clearance,
                 "dark_back_disk_z_bounds_m": [float(np.min(pos[indices[rear_ids], 2])), float(np.max(pos[indices[rear_ids], 2]))],
                 "note": "Source contains a solid dark rear disk. It is not an open infinite tube."},
        "safe_launch": {"centre_m": launch.tolist(), "forward": [0, 0, 1],
                        "past_terminal_rim_m": .01, "nearest_source_triangle_id": closest_id,
                        "nearest_triangle_distance_m": launch_clearance,
                        "guarded_centred_sphere_radius_m": launch_clearance-safety_guard,
                        "proof": "Double-precision minimum Euclidean distance to all source triangles bounds a sphere around the complete scaled missile, with a five-millimetre safety floor. Forward +Z travel cannot reduce distance: launch Z exceeds every source vertex Z. This proves horizontal source departure only."},
        "missile": {"source_bounds_m": [missile.min(0).tolist(), missile.max(0).tolist()],
                    "bounds_centre_m": ((missile.min(0)+missile.max(0))/2).tolist(),
                    "source_radial_max_about_origin_plus_z_m": radial,
                    "source_bounding_sphere_radius_m": bounding_ball,
                    "recommended_uniform_scale": scale, "scaled_radius_m": missile_radius,
                    "scaled_length_m": float(np.ptp(missile[:, 2])*scale),
                    "scaled_sphere_radius_m": sphere_radius,
                    "whole_mesh_sphere_clearance_guard_m": launch_clearance-sphere_radius,
                    "maximum_uniform_scale_at_5mm_guard": max_uniform_scale,
                    "scale_policy": "Uniform XYZ scale preserves the original silhouette. Do not stretch length independently or use the abstract gameplay hit radius as mesh scale."},
        "assembly": {"turret_socket_m": [-.04, .955, -.24], "ordinary_actor_scale": 1, "boss_actor_scale": 1.6,
                     "application": "Apply the turret socket then turret yaw to the source launch once; apply actor scale once to both geometry and all offsets/radii. No turret pitch is qualified."},
        "limits": "Offline original-source geometry only. No live firing, new art, vertical guide clearance, gameplay collision change, performance or mobile qualification. Existing review image is illustrative; diagram is measured XY geometry."
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    image = args.output.with_suffix(".png")
    annotate(image, contour, rear, centre, missile_radius)
    result["annotated_review"] = str(image.relative_to(ROOT))
    result["distance_control_cases_passed"] = 3
    result["source_stable_during_measurement"] = source_hashes == {str(p.relative_to(ROOT)): sha(p) for p in inputs}
    if not result["source_stable_during_measurement"]:
        raise ValueError("Source changed during measurement; output not accepted")
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"output": str(args.output), "launch": launch.tolist(), "radius": missile_radius,
                      "aperture_radius": aperture_clearance, "guard": launch_clearance-sphere_radius}, indent=2))


if __name__ == "__main__":
    main()
