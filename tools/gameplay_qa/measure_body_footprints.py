"""Measure consumed HRM bodies using only Python's standard library.

No collider or gameplay tuning is created. Whole-mesh boxes indicate candidate
contacts; the neutral y=.5m cross-section disk is a stricter body-core proxy.
Animation, capture transforms and actual triangle/triangle collision are outside
this measurement. Hats are intentionally separate and excluded.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import struct


MODELS = ("cow", "pig", "sheep", "farmer-body", "resident-body", "soldier-body")


def measure(path, height):
    data = path.read_bytes()
    magic, count, index_count, width, texture_height = struct.unpack_from("<4sIIII", data)
    if magic != b"HRM1" or not count or not index_count or index_count % 3:
        raise ValueError(f"Invalid HRM: {path}")
    if len(data) != 20 + count * 56 + index_count * 4 + width * texture_height * 4:
        raise ValueError(f"Invalid HRM length: {path}")
    points = [struct.unpack_from("<3f", data, 20 + i * 56) for i in range(count)]
    indices = struct.unpack_from(f"<{index_count}I", data, 20 + count * 56)
    if max(indices) >= count or not all(math.isfinite(v) for p in points for v in p):
        raise ValueError(f"Invalid HRM geometry: {path}")
    segments = []
    for i in range(0, index_count, 3):
        triangle = [points[j] for j in indices[i:i + 3]]
        crossings = []
        for a, b in zip(triangle, triangle[1:] + triangle[:1]):
            if a[1] <= height < b[1] or b[1] <= height < a[1]:
                t = (height - a[1]) / (b[1] - a[1])
                crossings.append((a[0] + t * (b[0] - a[0]), a[2] + t * (b[2] - a[2])))
        if len(crossings) == 2:
            segments.append(crossings)
    inside = sum(
        (a[1] > 0) != (b[1] > 0)
        and a[0] - a[1] * (b[0] - a[0]) / (b[1] - a[1]) > 0
        for a, b in segments
    ) % 2 == 1

    def distance(a, b):
        dx, dz = b[0] - a[0], b[1] - a[1]
        length_squared = dx * dx + dz * dz
        t = max(0, min(1, -(a[0] * dx + a[1] * dz) / length_squared)) if length_squared else 0
        return math.hypot(a[0] + t * dx, a[1] + t * dz)

    minimum = [min(p[j] for p in points) for j in range(3)]
    maximum = [max(p[j] for p in points) for j in range(3)]
    return {
        "path": str(path), "sha256": hashlib.sha256(data).hexdigest(),
        "vertices": count, "triangles": index_count // 3,
        "minimum": minimum, "maximum": maximum,
        "box_center_xz": [(minimum[j] + maximum[j]) / 2 for j in (0, 2)],
        "box_half_extents_xz": [(maximum[j] - minimum[j]) / 2 for j in (0, 2)],
        "enclosing_vertex_radius_xz": max(math.hypot(p[0], p[2]) for p in points),
        "cross_section_y": height, "cross_section_origin_inside": inside,
        "neutral_core_radius": min((distance(*s) for s in segments), default=0) if inside else 0,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--content", type=Path, default=Path("build/dev/assets/runtime/game"))
    parser.add_argument("--height", type=float, default=.5)
    args = parser.parse_args()
    print(json.dumps({"schema": 1, "scope": "neutral consumed bodies; no hats, no capture or skin deformation", "models": {
        name: measure(args.content / (name + ".hrm"), args.height) for name in MODELS
    }}, indent=2))


if __name__ == "__main__":
    main()
