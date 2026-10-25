"""Independent source parity for the reviewed rigid mount split."""
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import art_tools


class VehicleContentTests(unittest.TestCase):
    def test_split_preserves_all_source_triangles_uv_winding_and_albedo(self):
        script = r'''
from pathlib import Path
import hashlib, json, sys, tempfile
import numpy as np
root = Path(sys.argv[1]); sys.path.insert(0, str(root / 'tools'))
from cook_vehicle_content import split_mount
from validate_infantry_animation import read_glb, source_geometry, geometry_preserved, image_pixels
contract = json.loads((root / 'assets/production/vehicle-weapons.json').read_text())
source, source_blob = read_glb(root / 'assets/models/jeep-minigun-mount/jeep-minigun-mount.glb')
with tempfile.TemporaryDirectory(prefix='vehicle-source-parity-') as directory:
    output = Path(directory)
    derived = split_mount(output)
    positions, uvs, indices = [], [], []
    for name, path in derived.items():
        document, blob = read_glb(path)
        vertices, uv, triangles = source_geometry(document, blob)
        if name.endswith('yaw-cap'): vertices += contract['yaw_pivot_mount_m']
        assert np.array_equal(image_pixels(document, blob), image_pixels(source, source_blob))
        indices.append(triangles + sum(map(len, positions)))
        positions.append(vertices); uvs.append(uv)
    # Independent validator compares complete directed triangles in source
    # space, including the UV for each corner. A dropped/duplicated/reversed
    # face or a split-boundary texture seam cannot pass a triangle-count check.
    error = geometry_preserved(np.concatenate(positions), np.concatenate(uvs),
        np.concatenate(indices), source_geometry(source, source_blob))
    assert error < 1e-6, error
    assert sum(index.size // 3 for index in indices) == 984
    first = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in derived.items()}
    repeated = split_mount(output)
    assert first == {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in repeated.items()}
print('984 source triangles, UV/winding/albedo parity and repeated bytes passed')
'''
        result = subprocess.run([str(art_tools.art_python()), "-c", script, str(ROOT)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
