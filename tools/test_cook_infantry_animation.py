"""Focused isolated infantry cook boundaries; no shipping registration."""
from copy import deepcopy
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

import cook_infantry_animation as infantry


class InfantryCookTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe = infantry.checked_recipe()
        cls.export_root = infantry.ROOT / 'build/animation/infantry'

    def test_source_paths_reject_escape(self):
        for name in ('../outside.glb', '/tmp/body.glb', 'assets/../../outside.glb'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                infantry.source_path(name)

    def test_recipe_requires_three_distinct_roles(self):
        altered = deepcopy(self.recipe)
        altered['variants'][1]['role'] = altered['variants'][0]['role']
        with patch.object(infantry.RECIPE.__class__, 'read_text', return_value=json.dumps(altered)):
            with self.assertRaisesRegex(ValueError, 'distinct infantry roles'):
                infantry.checked_recipe()

    def test_source_report_detects_changed_export(self):
        original_digest = infantry.digest

        def changed(path):
            return '0' * 64 if Path(path).suffix == '.glb' and 'exports' in Path(path).parts else original_digest(path)

        with patch.object(infantry, 'digest', side_effect=changed):
            with self.assertRaisesRegex(ValueError, 'validation/export|producer manifest'):
                infantry.validated_inputs(self.recipe, self.export_root)

    def test_cook_rejects_overlapping_roots(self):
        with self.assertRaisesRegex(ValueError, 'must be separate'):
            infantry.cook(self.export_root / 'cooked', self.export_root)

    def test_actual_weapon_axes_and_source_markers(self):
        expected_yaw = {'rifleman': 45, 'shotgunner': 55, 'rocketeer': 40}
        for variant in self.recipe['variants']:
            with self.subTest(role=variant['role']):
                metadata = infantry.socket_metadata(self.export_root / 'exports' / (variant['id'] + '.glb'), variant)
                self.assertAlmostEqual(metadata['aim_yaw_radians'] * 180 / 3.141592653589793,
                                       expected_yaw[variant['role']], places=2)
                self.assertEqual(set(metadata['markers']), set(variant['weapon_sockets']))
                self.assertEqual(metadata['weapon_socket'], 'socket_weapon')
                self.assertEqual(metadata['head_socket'], 'socket_head')

    def test_socket_reference_uses_actual_baked_duration(self):
        variant = self.recipe['variants'][0]
        report = json.loads((self.export_root / 'reports' / (variant['id'] + '.json')).read_text())
        actual_run = next(c for c in report['clips'] if c['name'] == 'run')
        self.assertNotEqual(actual_run['seconds'], self.recipe['clips']['run']['seconds'])
        with tempfile.TemporaryDirectory(dir=infantry.ROOT / 'build') as temporary:
            output = Path(temporary)
            infantry.write_socket_reference(self.export_root / 'exports' / (variant['id'] + '.glb'),
                                            variant, report['clips'], output)
            payload = (output / (variant['id'] + '.hws')).read_bytes()
        tag, samples, nodes = struct.unpack_from('<4sII', payload)
        self.assertEqual(tag, b'HWS1')
        self.assertEqual(samples, sum(2 * c['frames'] - 1 for c in report['clips']))
        doc, _ = infantry.unpack(self.export_root / 'exports' / (variant['id'] + '.glb'))
        run_index = next(i for i, c in enumerate(doc['animations']) if c['name'] == 'run')
        run_times = []
        offset = 12 + nodes * 32
        for _ in range(samples):
            index, seconds = struct.unpack_from('<If', payload, offset)
            if index == run_index:
                run_times.append(seconds)
            offset += 8 + nodes * 64
        self.assertAlmostEqual(run_times[-1], actual_run['seconds'], places=6)
        self.assertEqual(offset, len(payload))

    def test_failed_cook_keeps_prior_output_and_encoder_source(self):
        original_exports = infantry.encoder.EXPORTS
        calls = 0

        def failing_cook(variant, recipe, output, reference):
            nonlocal calls
            calls += 1
            (output / (variant['id'] + '.hra')).write_bytes(b'not-published')
            if calls == 2:
                raise ValueError('injected encoder failure')
            return {'id': variant['id']}

        with tempfile.TemporaryDirectory(dir=infantry.ROOT / 'build') as temporary:
            output = Path(temporary) / 'cooked'
            output.mkdir()
            sentinel = output / 'prior-manifest.json'
            sentinel.write_bytes(b'prior validated set')
            with patch.object(infantry.encoder, 'cook', side_effect=failing_cook), \
                    patch.object(infantry, 'socket_metadata', return_value={}):
                with self.assertRaisesRegex(ValueError, 'injected encoder failure'):
                    infantry.cook(output, self.export_root)
            self.assertEqual(list(output.iterdir()), [sentinel])
            self.assertEqual(sentinel.read_bytes(), b'prior validated set')
            self.assertEqual(infantry.encoder.EXPORTS, original_exports)


if __name__ == '__main__':
    unittest.main()
