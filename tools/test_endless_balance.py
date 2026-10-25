"""Behavior checks for the offline encounter experiment, not native gameplay."""
import copy
import json
from pathlib import Path
import random
import unittest

from endless_balance.run import Run
from endless_balance.ship import Build, draw
from endless_balance.world import Enemy

ROOT = Path(__file__).resolve().parents[1]
SPEC = json.loads((ROOT / "design/balance/endless-simulation-input.json").read_text())
WORLD = json.loads((ROOT / "design/balance/endless-encounter-assumptions.json").read_text())


def hardware(key):
    return {"family":key,"kind":"hardware","rarity":"Common","value":1}


def enemy(identity=1, hp=1000):
    return Enemy(identity, "tank", hp, hp, 18, 6, .45, 0, 48, 100, random.Random(identity))


class EndlessExperimentTests(unittest.TestCase):
    def test_each_beam_pick_adds_one_and_uses_the_same_slot(self):
        build = Build()
        self.assertEqual(build.stats(SPEC)["beams"], 1)
        for expected in (2, 3, 4):
            build.apply(hardware("beam_splitter"), SPEC)
            self.assertEqual(build.stats(SPEC)["beams"], expected)
            self.assertEqual(len(build.hardware), 1)

    def test_damage_research_adds_against_original_base(self):
        build = Build()
        for _ in range(50):
            build.apply({"family":"weapon_damage","kind":"research","value":3}, SPEC)
        self.assertEqual(build.stats(SPEC)["damage_multiplier"], 2.5)

    def test_piercing_does_not_increase_damage_to_the_primary(self):
        plain, pierce = Run(SPEC, WORLD, 1, "Gunship"), Run(SPEC, WORLD, 1, "Gunship")
        pierce.build.hardware["rail_lens"] = 3
        pierce.stats = pierce.build.stats(SPEC)
        for run in (plain, pierce):
            run.enemies = [enemy(1), enemy(2)]
            run.weapons(1, 0, 0)
        self.assertAlmostEqual(plain.enemies[0].hp, pierce.enemies[0].hp)
        self.assertLess(pierce.enemies[1].hp, plain.enemies[1].hp)

    def test_lasers_skip_a_vehicle_that_is_being_lifted(self):
        run = Run(SPEC, WORLD, 1, "Collector")
        lifted, other = enemy(1), enemy(2)
        run.enemies, run.vehicle = [lifted, other], lifted
        run.weapons(1, 1, 0)
        self.assertEqual(lifted.hp, 1000)
        self.assertLess(other.hp, 1000)

    def test_capture_awards_xp_once_and_not_also_a_kill(self):
        world = copy.deepcopy(WORLD)
        world["soldier_fraction_of_captures"] = 1
        run = Run(SPEC, world, 1, "Collector")
        soldier = enemy()
        soldier.kind = "soldier"
        run.enemies = [soldier]
        run.complete_biological(1, 1)
        self.assertEqual(run.xp, 1)
        self.assertEqual(run.enemies, [])
        self.assertEqual(sum(run.kills.values()), 0)

    def test_capture_shield_restore_obeys_the_global_limit(self):
        run = Run(SPEC, WORLD, 1, "Collector")
        run.build.hardware["repair_swarm"] = 3
        run.stats = run.build.stats(SPEC)
        run.shield = 0
        run.complete_biological(1, 20)
        self.assertEqual(run.shield, 4)

    def test_two_recipe_completions_follow_the_declared_order(self):
        build = Build({"beam_splitter":2,"tractor_lens":3,"heavy_winch":3})
        build.apply(hardware("beam_splitter"), SPEC)
        self.assertEqual(build.evolutions, ["The Great Vacuum", "Hostile Acquisition"])
        self.assertEqual(build.stats(SPEC)["beams"], 6)

    def test_maxed_build_keeps_three_distinct_choices(self):
        build = Build({m["id"]:3 for m in SPEC["hardware"][:6]}, {"capture_speed":100,"fire_rate":50})
        for seed in range(100):
            cards = draw(SPEC, build, 200, 1, random.Random(seed))
            self.assertEqual(len({c["family"] for c in cards}), 3)
            self.assertTrue(all(c["kind"] == "research" for c in cards))

    def test_survival_xp_carries_over_and_the_horizon_is_not_death(self):
        world = copy.deepcopy(WORLD)
        world["biological_encounters_per_second"] = 0
        for rule in world["enemies"].values():
            rule["initial_rate"] = 0
        run = Run(SPEC, world, 2, "Hybrid", horizon=60)
        result = run.play()
        self.assertEqual(result["pick_intervals"], [24, 28])
        self.assertEqual(run.xp, 8)
        self.assertTrue(result["censored"])
        self.assertEqual(result["seconds"], 60)

    def test_repeat_seed_reproduces_world_and_picks(self):
        left = Run(SPEC, WORLD, 71, "Hybrid", horizon=600).play()
        right = Run(SPEC, WORLD, 71, "Hybrid", horizon=600).play()
        self.assertEqual(left, right)

    def test_research_bundles_do_not_compound_or_expand_capped_stats(self):
        spec = copy.deepcopy(SPEC)
        spec["study_tuning"] = {"research_bundle_every_levels":10}
        build = Build(research={"weapon_damage":100})
        lookup = {item["id"]:item for item in spec["research"]}
        for seed in range(100):
            for card in draw(spec, build, 21, 1, random.Random(seed)):
                if card["kind"] == "research":
                    item = lookup[card["family"]]
                    expected = item["values"][card["rarity"]] * (3 if item["cap"] is None else 1)
                    self.assertEqual(card["value"], expected)
        self.assertEqual(build.research["weapon_damage"], 100)

    def test_rare_blueprint_upgrades_keep_their_identity_in_an_ordinary_draw(self):
        spec = copy.deepcopy(SPEC)
        spec["study_tuning"] = {"ordinary_owned_upgrades":True}
        build = Build(hardware={"arc_coil":1})
        seen = []
        for seed in range(100):
            seen += [c for c in draw(spec, build, 10, 1, random.Random(seed)) if c["family"] == "arc_coil"]
        self.assertTrue(seen)
        self.assertTrue(all(c["rarity"] == "Rare" and c["draw_rarity"] == "Common" for c in seen))


if __name__ == "__main__":
    unittest.main()
