"""Real offline physical geometry, shipping inventory and source preservation."""
from pathlib import Path
import hashlib
import json
import math
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from art_tools import art_python


class ShipComponentCookingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix='ship-component-contract-')
        cls.output = Path(cls.temporary.name)
        cls.recipe = json.loads((ROOT / 'assets/production/ship-components.json').read_text())
        source = ROOT / cls.recipe['source']
        cls.original = hashlib.sha256(source.read_bytes()).hexdigest()
        code = "import sys,json;from pathlib import Path;sys.path.insert(0,sys.argv[1]);from cook_ship_components import cook_components;out=Path(sys.argv[2]);(out/'report.json').write_text(json.dumps(cook_components(out)))"
        subprocess.run([str(art_python(ROOT)), '-c', code, str(ROOT / 'tools'), str(cls.output)], check=True)
        cls.rows = {r['id']: r for r in json.loads((cls.output / 'report.json').read_text())}

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def vertices(self, name):
        data = (self.output / (name + '.hrm')).read_bytes()
        magic, count, indices, width, height = struct.unpack_from('<4sIIII', data)
        self.assertEqual(magic, b'HRM1')
        self.assertEqual(len(data), 20 + count * 56 + indices * 4 + width * height * 4)
        self.assertLessEqual(count, 32768)
        self.assertEqual(indices % 3, 0)
        rows = list(struct.iter_unpack('<14f', data[20:20 + count * 56]))
        self.assertTrue(all(math.isfinite(v) for row in rows for v in row))
        self.assertTrue(all(abs(math.sqrt(sum(v * v for v in row[3:6])) - 1) < .0001 for row in rows))
        return rows

    def test_exact_consumed_parts_and_bounded_geometry(self):
        self.assertEqual(set(self.rows), set(self.recipe['models']))
        shipping = json.loads((ROOT / 'assets/runtime/game-content.json').read_text())['models']
        for name in self.rows:
            with self.subTest(component=name):
                self.assertIn(name, shipping)
                self.vertices(name)
        self.assertLess(sum(p.stat().st_size for p in self.output.glob('*.hrm')), 16 * 1024 * 1024)

    def test_emp_changes_actual_profile_without_changing_bearing(self):
        heights = []
        for rank in range(4):
            vertices = self.vertices(f'ship-rotor-{rank}')
            bore = [v for v in vertices if math.hypot(v[0], v[2]) < .605]
            self.assertTrue(bore)
            self.assertTrue(all(abs(math.hypot(v[0], v[2]) - .604) < .00001 for v in bore))
            self.assertTrue(all(v[13] == 1 for v in vertices))
            heights.append(max(v[1] for v in vertices if math.hypot(v[0], v[2]) > 1.2))
        self.assertEqual(heights, sorted(set(heights)))

    def test_weapon_ranks_change_mesh_geometry(self):
        lengths = [max(v[2] for v in self.vertices(f'ship-laser-barrel-{r}')) for r in range(1, 4)]
        self.assertEqual(lengths, sorted(set(lengths)))
        for family in ('ship-armour-', 'ship-reactor-', 'ship-scanner-'):
            self.assertFalse(any(name.startswith(family) for name in self.rows), 'retired hardware does not ship unused meshes')

    def test_original_art_is_never_rewritten(self):
        self.assertEqual(self.original, hashlib.sha256((ROOT / self.recipe['source']).read_bytes()).hexdigest())
        self.assertTrue(all(row['source_sha256'] == self.original for row in self.rows.values()))

    def test_phase_canopy_keeps_its_mounting_flange(self):
        widths, heights = [], []
        for rank in range(4):
            vertices = self.vertices('ship-dome' if rank == 0 else f'ship-dome-phase-{rank}')
            flange = [v for v in vertices if abs(v[1]) < .00001]
            self.assertTrue(flange)
            self.assertLessEqual(max(math.hypot(v[0], v[2]) for v in flange), .50501)
            widths.append(max(abs(v[2]) for v in vertices))
            heights.append(max(v[1] for v in vertices))
        self.assertEqual(widths, sorted(set(widths)))
        self.assertEqual(heights, sorted(set(heights), reverse=True))


if __name__ == '__main__':
    unittest.main()
