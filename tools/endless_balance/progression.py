"""Offline account economy, blueprint loadouts and one-time reward settlement."""
import copy
from dataclasses import dataclass, field

from .ship import POLICIES


@dataclass
class Account:
    credits: int = 0
    ranks: dict = field(default_factory=dict)
    counters: dict = field(default_factory=dict)
    unlocked: set = field(default_factory=set)
    receipts: dict = field(default_factory=dict)
    purchases: int = 0

    def available(self, plan):
        return set(plan["starting_blueprints"]) | self.unlocked

    def loadout(self, plan, policy, expand=False):
        available = self.available(plan)
        if expand:
            return sorted(available)
        preference = POLICIES[policy][0] + plan["starting_blueprints"]
        preference += [item["id"] for item in plan["unlocks"]]
        ordered = list(dict.fromkeys(preference))
        return [key for key in ordered if key in available][:plan["run"]["active_blueprints"]]

    def purchase(self, plan, order=("damage", "hull", "shield", "capture")):
        bought = []
        tracks = plan["permanent"]["tracks"]
        while True:
            eligible = [key for key in order if self.ranks.get(key, 0) < tracks[key]["maximum_ranks"]
                        and plan["permanent"]["costs_per_rank"][self.ranks.get(key, 0)] <= self.credits]
            if not eligible:
                return bought
            key = min(eligible, key=lambda k: self.ranks.get(k, 0))
            self.credits -= plan["permanent"]["costs_per_rank"][self.ranks.get(key, 0)]
            self.ranks[key] = self.ranks.get(key, 0) + 1
            self.purchases += 1
            bought.append(key)

    def settle(self, plan, run_id, row):
        if run_id in self.receipts:
            return 0
        base = base_credits(plan, row)
        self.credits += base
        remaining_cost = sum(sum(plan["permanent"]["costs_per_rank"][self.ranks.get(key, 0):])
                             for key in plan["permanent"]["tracks"])
        self.receipts[run_id] = {"base": base, "bonus": False,
                                 "bonus_eligible": self.credits < remaining_cost}
        increments = {"captures": row["captures"], "base_credits": base,
                      "military_kills": sum(row["kills"].get(k, 0) for k in ("soldier", "jeep", "tank")),
                      "active_seconds": row["seconds"]}
        for key, value in increments.items():
            self.counters[key] = self.counters.get(key, 0) + value
        for unlock in plan["unlocks"]:
            if self.counters.get(unlock["counter"], 0) >= unlock["threshold"]:
                self.unlocked.add(unlock["id"])
        return base

    def claim_bonus(self, run_id, completed):
        receipt = self.receipts.get(run_id)
        if not completed or receipt is None or receipt["bonus"] or not receipt["bonus_eligible"]:
            return 0
        receipt["bonus"] = True
        self.credits += receipt["base"]
        return receipt["base"]


def base_credits(plan, row):
    rule = plan["credits"]
    if row["seconds"] < rule["minimum_active_seconds"]:
        return 0
    vehicles = row["vehicle_captures"] + sum(row["kills"].get(k, 0) for k in ("jeep", "tank"))
    return (int(row["seconds"] // rule["seconds_per_credit"])
            + int(row["score"] // rule["score_per_credit"])
            + vehicles * rule["per_vehicle_takedown"]
            + row["kills"].get("boss", 0) * rule["per_boss_kill"])


def run_spec(source, plan, account, policy, expand=False):
    spec = copy.deepcopy(source)
    ids = account.loadout(plan, policy, expand)
    spec["hardware"] = [module for module in spec["hardware"] if module["id"] in ids]
    spec["study_tuning"] = {key: plan["run"][key] for key in (
        "ordinary_owned_upgrades", "rerolls", "research_bundle_every_levels")}
    for key in ("next_level_base_xp", "next_level_per_current_level_xp"):
        spec["xp_starting_proposal"][key] = plan["run"][key]
    base = spec["loop"]["baseline"]
    tracks = plan["permanent"]["tracks"]
    for key in ("hull", "shield"):
        base[key] += account.ranks.get(key, 0) * tracks[key]["per_rank"]
    multiplier = 1 + account.ranks.get("damage", 0) * tracks["damage"]["per_rank"]
    base["laser_damage"] *= multiplier
    spec["effect_constants"]["drone_damage"] *= multiplier
    spec["effect_constants"]["mirrorball_base_damage_cap"] *= multiplier
    for module in spec["hardware"]:
        for effects in module["rank_effects"]:
            if "pulse_damage" in effects:
                effects["pulse_damage"] *= multiplier
    # Separate biological duration: the existing vehicle model uses its own constant.
    base["capture_seconds"] /= 1 + account.ranks.get("capture", 0) * tracks["capture"]["per_rank"]
    return spec
