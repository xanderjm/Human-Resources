"""Contracts specific to the offline Archero-inspired adaptation."""
import copy
import json
from pathlib import Path
import unittest

from check_endless_upgrades import candidates
from endless_balance.archero import HangarAccount, Inputs, Journey, Trial, account_state, execute_journey, execute_run, profile_plan
from endless_balance.progression import run_spec
from endless_balance.ship import Build, draw
from endless_balance.short_run import ShortRun, short_world
import random

DIRECTORY = Path(__file__).resolve().parents[1] / "design/balance"
SOURCE = json.loads((DIRECTORY / "endless-simulation-input.json").read_text())
WORLD = json.loads((DIRECTORY / "endless-encounter-assumptions.json").read_text())
PLAN = json.loads((DIRECTORY / "archero-ufo-system.json").read_text())


class ArcheroAdaptationTests(unittest.TestCase):
    def test_all_unlocked_cards_enter_every_policys_pool(self):
        account = account_state(PLAN, "unlocks_only")
        pools = [{m["id"] for m in run_spec(SOURCE, PLAN, account, p, expand=True)["hardware"]}
                 for p in ("Hybrid", "Collector", "Gunship", "Defender", "Random")]
        self.assertTrue(all(pool == pools[0] for pool in pools))
        self.assertEqual(len(pools[0]), 16)

    def test_stats_and_unlocks_can_be_isolated(self):
        self.assertEqual(len(account_state(PLAN, "fresh").available(PLAN)), 10)
        self.assertEqual(account_state(PLAN, "unlocks_only").ranks, {})
        self.assertEqual(len(account_state(PLAN, "stats_only").available(PLAN)), 10)
        self.assertEqual(account_state(PLAN, "maxed").ranks, {k:5 for k in PLAN["permanent"]["tracks"]})

    def test_selected_hangar_changes_only_declared_starting_templates(self):
        before = copy.deepcopy(SOURCE)
        plan = profile_plan(PLAN, "hangar_25")
        spec = run_spec(SOURCE, plan, account_state(plan, "maxed"), "Hybrid", expand=True)
        baseline = spec["loop"]["baseline"]
        self.assertEqual(baseline["hull"], 130)
        self.assertEqual(baseline["shield"], 40)
        self.assertEqual(baseline["laser_damage"], 10)
        self.assertAlmostEqual(baseline["capture_seconds"], 2/1.15)
        self.assertEqual(baseline["lasers"], 1)
        self.assertEqual(baseline["tractor_beams"], 1)
        self.assertEqual(baseline["shield_regen_per_second"], 3)
        self.assertEqual(SOURCE, before)

    def test_credit_bonus_is_half_rounded_down_and_settles_once(self):
        account = HangarAccount()
        row = {"seconds":480,"score":100,"captures":100,"kills":{"jeep":2,"tank":1,"boss":1},"vehicle_captures":1}
        amount = account.settle(PLAN, "a", row)
        counters = dict(account.counters)
        self.assertEqual(account.claim_credit_bonus(PLAN, "a", False), 0)
        self.assertEqual(account.claim_credit_bonus(PLAN, "a", True), amount//2)
        self.assertEqual(account.claim_credit_bonus(PLAN, "a", True), 0)
        self.assertEqual(account.credits, amount + amount//2)
        self.assertEqual(account.counters, counters)

    def test_capped_hangar_has_no_credit_ad_offer(self):
        account = account_state(PLAN, "maxed")
        row = {"seconds":480,"score":100,"captures":100,"kills":{},"vehicle_captures":0}
        account.settle(PLAN, "a", row)
        self.assertEqual(account.claim_credit_bonus(PLAN, "a", True), 0)

    def test_six_module_limit_keeps_owned_ranks_and_research_available(self):
        spec = copy.deepcopy(SOURCE)
        spec["loop"]["hardware_slots"] = 6
        hardware = {m["id"]:3 for m in spec["hardware"][:6]}
        self.assertTrue(all(c["kind"] == "research" for c in candidates(spec,hardware,{})))
        hardware["laser_array"] = 2
        self.assertTrue(any(c["family"] == "laser_array" for c in candidates(spec,hardware,{})))
        for seed in range(50):
            cards = draw(spec, Build(hardware, {"capture_speed":100,"fire_rate":50}), 100, 1, random.Random(seed))
            self.assertEqual(len({c["family"] for c in cards}), 3)

    def test_new_attempt_resets_temporary_build_but_keeps_hangar(self):
        spec = run_spec(SOURCE, PLAN, account_state(PLAN,"maxed"), "Hybrid", expand=True)
        run = ShortRun(spec,short_world(WORLD,PLAN),PLAN,1,"Hybrid",horizon=120)
        self.assertEqual(run.level, 1)
        self.assertEqual(run.build.hardware, {})
        self.assertEqual(run.build.research, {})
        self.assertEqual(run.build.evolutions, [])
        self.assertEqual(run.hull, 130)

    def test_account_and_ad_choice_do_not_change_the_director(self):
        runs = [ShortRun(run_spec(SOURCE,PLAN,account_state(PLAN,state),"Hybrid",expand=True),
                         short_world(WORLD,PLAN),PLAN,71,"Hybrid",revive=revive)
                for state,revive in (("fresh",False),("maxed",True))]
        self.assertEqual(runs[0].events,runs[1].events)

    def test_journeys_reproduce_and_rank_snapshots_do_not_mutate(self):
        inputs = Inputs(SOURCE,WORLD,PLAN)
        task = Journey(inputs,0,2,"Hybrid","regular","credits_50")
        left,rows = execute_journey(task)
        self.assertEqual((left,rows),execute_journey(task))
        self.assertEqual(rows[0]["starting_ranks"],{})
        self.assertGreater(left["credits_earned"],0)
        self.assertEqual(len(rows),2)

    def test_each_trial_is_seeded_and_does_not_mutate_account(self):
        account = HangarAccount(ranks={"damage":1})
        task = Trial(Inputs(SOURCE,WORLD,PLAN),account,"Hybrid",17,{"horizon":120})
        self.assertEqual(execute_run(task),execute_run(task))
        self.assertEqual(account.ranks,{"damage":1})
        self.assertEqual(account.counters,{})

    def test_contextual_rules_keep_unready_vehicle_tech_out_of_opening_offers(self):
        plan = profile_plan(PLAN,"hangar_25")
        row = execute_run(Trial(Inputs(SOURCE,WORLD,plan),account_state(plan,"maxed"),"Collector",17,
                                {"horizon":120,"trace":True}))
        hardware = {}
        for event in row["trace"]:
            for card in event["cards"]:
                if card["kind"] == "hardware":
                    required = plan["eligibility_prerequisites"].get(card["family"],{})
                    self.assertTrue(all(hardware.get(key,0) >= rank for key,rank in required.items()))
            chosen = event["picked"]
            if chosen["kind"] == "hardware":
                hardware[chosen["family"]] = hardware.get(chosen["family"],0) + 1


if __name__ == "__main__":
    unittest.main()
