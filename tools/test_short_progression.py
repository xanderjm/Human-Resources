"""Contracts for the offline short-run progression proposal, not native saves/ads."""
import copy
from concurrent.futures import ProcessPoolExecutor
import json
import multiprocessing
from pathlib import Path
import unittest

from endless_balance.progression import Account, base_credits, run_spec
from endless_balance.journey import account_journey
from endless_balance.short_run import ShortRun, short_world
from simulate_short_progression import simulate_one

DIRECTORY = Path(__file__).resolve().parents[1] / "design/balance"
PLAN = json.loads((DIRECTORY / "short-run-progression.json").read_text())
SPEC = json.loads((DIRECTORY / "endless-simulation-input.json").read_text())
WORLD = json.loads((DIRECTORY / "endless-encounter-assumptions.json").read_text())


def result(seconds=480):
    return {"seconds": seconds, "score": 100, "captures": 100,
            "kills": {"soldier": 10, "jeep": 2, "tank": 1, "boss": 1}, "vehicle_captures": 1}


class ProgressionTests(unittest.TestCase):
    def test_credit_formula_and_short_attempt_floor(self):
        self.assertEqual(base_credits(PLAN, result()), 60 + 20 + 8 + 5)
        self.assertEqual(base_credits(PLAN, result(29)), 0)

    def test_base_and_bonus_settle_once_and_bonus_never_unlocks_achievements(self):
        account = Account()
        amount = account.settle(PLAN, "run-1", result())
        counters = dict(account.counters)
        self.assertEqual(account.settle(PLAN, "run-1", result()), 0)
        self.assertEqual(account.claim_bonus("run-1", False), 0)
        self.assertEqual(account.claim_bonus("run-1", True), amount)
        self.assertEqual(account.claim_bonus("run-1", True), 0)
        self.assertEqual(account.credits, 2 * amount)
        self.assertEqual(account.counters, counters)
        self.assertEqual(account.claim_bonus("unknown", True), 0)

    def test_permanent_power_caps_and_total_cost(self):
        account = Account(credits=10000)
        account.purchase(PLAN)
        self.assertEqual(account.ranks, {key: 5 for key in PLAN["permanent"]["tracks"]})
        self.assertEqual(account.credits, 4000)
        self.assertEqual(account.purchases, 20)
        self.assertEqual(account.purchase(PLAN), [])

    def test_credit_ad_is_hidden_when_remaining_progress_is_already_affordable(self):
        account = Account(credits=6000)
        account.settle(PLAN, "run-1", result())
        self.assertFalse(account.receipts["run-1"]["bonus_eligible"])
        self.assertEqual(account.claim_bonus("run-1", True), 0)

    def test_unaffordable_purchase_does_not_spend_or_change_ranks(self):
        account = Account(credits=79)
        self.assertEqual(account.purchase(PLAN), [])
        self.assertEqual(account.credits, 79)
        self.assertEqual(account.ranks, {})

    def test_unlock_is_available_next_run_but_not_installed(self):
        account = Account()
        spec = run_spec(SPEC, PLAN, account, "Collector")
        self.assertNotIn("heavy_winch", {m["id"] for m in spec["hardware"]})
        row = result()
        row["captures"] = 300
        account.settle(PLAN, "run-1", row)
        next_spec = run_spec(SPEC, PLAN, account, "Collector")
        self.assertIn("heavy_winch", {m["id"] for m in next_spec["hardware"]})
        run = ShortRun(next_spec, short_world(WORLD, PLAN), PLAN, 1, "Collector", horizon=60)
        self.assertEqual(run.build.hardware, {})
        self.assertEqual(run.level, 1)

    def test_loadout_is_bounded_and_research_remains_available(self):
        account = Account(unlocked={u["id"] for u in PLAN["unlocks"]})
        spec = run_spec(SPEC, PLAN, account, "Defender")
        self.assertEqual(len(spec["hardware"]), 10)
        self.assertEqual(len(spec["research"]), 7)
        self.assertEqual(len(run_spec(SPEC, PLAN, account, "Defender", expand=True)["hardware"]), 16)

    def test_capped_meta_changes_starting_templates_without_mutating_source(self):
        before = copy.deepcopy(SPEC)
        account = Account(ranks={key: 5 for key in PLAN["permanent"]["tracks"]})
        spec = run_spec(SPEC, PLAN, account, "Hybrid")
        base = spec["loop"]["baseline"]
        self.assertEqual(base["hull"], 120)
        self.assertEqual(base["shield"], 40)
        self.assertAlmostEqual(base["laser_damage"], 9.2)
        self.assertAlmostEqual(base["capture_seconds"], 2 / 1.1)
        self.assertEqual(spec["effect_constants"]["vehicle_base_capture_seconds"], SPEC["effect_constants"]["vehicle_base_capture_seconds"])
        self.assertEqual(SPEC, before)

    def test_account_has_no_effect_on_director(self):
        fresh = ShortRun(run_spec(SPEC, PLAN, Account(), "Hybrid"), short_world(WORLD, PLAN), PLAN, 91, "Hybrid")
        maxed = ShortRun(run_spec(SPEC, PLAN, Account(ranks={"damage": 5}), "Hybrid"), short_world(WORLD, PLAN), PLAN, 91, "Hybrid")
        self.assertEqual(fresh.events, maxed.events)

    def test_single_revive_preserves_build_xp_and_run_clock(self):
        spec = run_spec(SPEC, PLAN, Account(), "Hybrid")
        run = ShortRun(spec, short_world(WORLD, PLAN), PLAN, 1, "Hybrid", revive=True)
        run.hull = 0
        self.assertFalse(run.try_revive(89))
        run.xp = 11
        run.build.hardware["beam_splitter"] = 1
        self.assertTrue(run.try_revive(90))
        self.assertEqual(run.hull, 50)
        self.assertEqual(run.shield, 0)
        self.assertEqual(run.xp, 11)
        self.assertEqual(run.build.hardware, {"beam_splitter": 1})
        self.assertEqual(run.revive_at, 90)
        run.hull = 0
        self.assertFalse(run.try_revive(120))

    def test_same_seed_repeats_and_starting_build_resets_between_runs(self):
        account = Account(ranks={"damage": 1})
        spec = run_spec(SPEC, PLAN, account, "Hybrid")
        rows = [ShortRun(spec, short_world(WORLD, PLAN), PLAN, 81, "Hybrid", horizon=120).play() for _ in range(2)]
        self.assertEqual(rows[0], rows[1])
        self.assertEqual(account.ranks, {"damage": 1})

    def test_parallel_batches_preserve_seeded_results(self):
        tasks = [(SPEC, WORLD, PLAN, Account(), "Hybrid", seed, {"horizon": 120}) for seed in (71, 72)]
        expected = list(map(simulate_one, tasks))
        with ProcessPoolExecutor(max_workers=2, mp_context=multiprocessing.get_context("spawn")) as executor:
            self.assertEqual(list(executor.map(simulate_one, tasks)), expected)

    def test_journey_records_starting_ranks_without_later_purchase_mutation(self):
        observation, rows = account_journey((SPEC, WORLD, PLAN, 0, 2, "Hybrid", "regular", "no_ads", False, False))
        self.assertEqual(rows[0]["account_ranks"], {})
        self.assertGreater(observation["purchases"], 0)
        self.assertEqual(len(rows), 2)


if __name__ == "__main__":
    unittest.main()
