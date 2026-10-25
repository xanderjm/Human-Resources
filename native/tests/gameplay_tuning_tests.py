#!/usr/bin/env python3
"""Reject broken authoring input before compiling native rules."""
import importlib.util
import json
import re
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('hr_native_tuning', ROOT / 'native/human_resources_gameplay/tools/generate_tuning.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class NativeTuningTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        vehicle = json.loads((ROOT / 'assets/production/vehicle-weapons.json').read_text())
        inputs = ('design/balance/archero-ufo-system.json', 'design/balance/endless-upgrade-system.json',
                  'design/balance/endless-encounter-assumptions.json', 'native/human_resources_gameplay/runtime-spatial.json',
                  'assets/production/vehicle-weapons.json', 'assets/animation/characters.json', 'assets/animation/infantry.json')
        for name in dict.fromkeys((*inputs, *vehicle['source_sha256'])):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)
        self.output = self.root / 'generated/tuning.h'

    def tearDown(self):
        self.temporary.cleanup()

    def mutate(self, relative, apply):
        path = self.root / relative
        value = json.loads(path.read_text())
        apply(value)
        path.write_text(json.dumps(value))

    def reject(self):
        with self.assertRaises((ValueError, KeyError)):
            MODULE.generate(self.root, self.output)
        self.assertFalse(self.output.exists())

    def test_reproducible_source_bound_generation(self):
        MODULE.generate(self.root, self.output)
        first = self.output.read_bytes()
        MODULE.generate(self.root, self.output)
        self.assertEqual(first, self.output.read_bytes())
        self.assertIn(b'xp_base = 16.0F', first)
        self.assertIn(b'jeep_capture_threshold', first)

    def test_negative_baseline_rejected(self):
        self.mutate('design/balance/endless-upgrade-system.json', lambda v: v['loop']['baseline'].update(hull=-1))
        self.reject()

    def fingerprint(self):
        MODULE.generate(self.root, self.output)
        return re.search(r'save_rules_fingerprint = (\d+)ULL', self.output.read_text()).group(1)

    def test_numeric_save_contract_change_invalidates_resume(self):
        original = self.fingerprint()
        self.mutate('native/human_resources_gameplay/runtime-spatial.json', lambda v: v.update(steering_acceleration=29))
        self.assertNotEqual(original, self.fingerprint())

    def test_display_only_change_preserves_save_compatibility(self):
        original = self.fingerprint()
        def rename(value):
            value['hardware'][0]['name'] = 'Renamed display title'
            value['research'][0]['name'] = 'Renamed research title'
            value['evolutions'][0]['name'] = 'Renamed evolution title'
        self.mutate('design/balance/endless-upgrade-system.json', rename)
        self.assertEqual(original, self.fingerprint())

    def test_vehicle_source_numeric_contract_invalidates_resume(self):
        original = self.fingerprint()
        self.mutate('assets/production/vehicle-weapons.json', lambda v: v['cannon']['muzzle'].__setitem__(0, -.007))
        self.assertNotEqual(original, self.fingerprint())

    def test_capture_return_cannot_drift_from_source_release(self):
        self.mutate('native/human_resources_gameplay/runtime-spatial.json', lambda v: v.update(capture_return_seconds=1.3))
        self.reject()

    def test_harvest_contract_changes_invalidate_resume(self):
        original = self.fingerprint()
        self.mutate('native/human_resources_gameplay/runtime-spatial.json',
                    lambda v: v['harvest'].update(overdrive_seconds=7))
        self.assertNotEqual(original, self.fingerprint())

    def test_invalid_harvest_chain_rejected(self):
        self.mutate('native/human_resources_gameplay/runtime-spatial.json',
                    lambda v: v['harvest'].update(chain_target=0))
        self.reject()

    def test_scroll_curve_and_native_gate_changes_invalidate_resume(self):
        original = self.fingerprint()
        self.mutate('native/human_resources_gameplay/runtime-spatial.json',
                    lambda v: v['scroll_progression'].update(per_capture_metres_per_second=.009))
        self.assertNotEqual(original, self.fingerprint())

    def test_civilian_speed_cannot_overtake_scroll(self):
        self.mutate('native/human_resources_gameplay/runtime-spatial.json',
                    lambda v: v['biological_movement_speed'].__setitem__(0, 3))
        self.reject()

    def test_native_ramp_must_be_ordered(self):
        self.mutate('native/human_resources_gameplay/runtime-spatial.json',
                    lambda v: v['native_director'].update(tank_start_second=1))
        self.reject()

    def test_military_spacing_changes_invalidate_resume(self):
        original = self.fingerprint()
        self.mutate('native/human_resources_gameplay/runtime-spatial.json',
                    lambda v: v['military_spacing'].update(lookahead_seconds=.5))
        self.assertNotEqual(original, self.fingerprint())

    def test_invalid_military_spacing_radius_rejected(self):
        self.mutate('native/human_resources_gameplay/runtime-spatial.json',
                    lambda v: v['military_spacing']['body_radius'].__setitem__(1, -1))
        self.reject()

    def test_invalid_starter_dash_rejected(self):
        self.mutate('native/human_resources_gameplay/runtime-spatial.json',
                    lambda v: v['starter_dash'].update(cooldown=.1))
        self.reject()

    def test_rhythm_cannot_delete_recovery_or_overfill_field(self):
        self.mutate('native/human_resources_gameplay/runtime-spatial.json',
                    lambda v: v['encounter_rhythm'].update(open_route_half_width=20))
        self.reject()

    def test_changed_catalogue_phrase_rejected(self):
        def edit(value):
            value['hardware'][0]['ranks'][0] = 'Replacement that has no numeric damage rule.'
        self.mutate('design/balance/endless-upgrade-system.json', edit)
        self.reject()

    def test_profile_conflict_rejected(self):
        self.mutate('design/balance/archero-ufo-system.json', lambda v: v['profiles']['hangar_25'].update(damage_per_rank=0.5))
        self.reject()

    def test_excess_population_rejected(self):
        self.mutate('native/human_resources_gameplay/runtime-spatial.json', lambda v: v.update(biological_active_cap=300))
        self.reject()

    def test_zero_reward_divisor_rejected(self):
        self.mutate('design/balance/archero-ufo-system.json', lambda v: v['credits'].update(seconds_per_credit=0))
        self.reject()


if __name__ == '__main__':
    unittest.main()
