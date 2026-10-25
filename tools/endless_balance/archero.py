"""Archero-inspired UFO experiment: automatic unlock pool and capped Hangar."""
import copy
from dataclasses import dataclass

from .progression import Account, run_spec
from .short_run import ShortRun, short_world


@dataclass
class Inputs:
    spec: dict
    world: dict
    plan: dict


@dataclass
class Trial:
    inputs: Inputs
    account: Account
    policy: str
    seed: int
    options: dict


class HangarAccount(Account):
    def claim_credit_bonus(self, plan, run_id, completed, *, double=False):
        receipt = self.receipts.get(run_id)
        if not completed or receipt is None or receipt["bonus"] or not receipt["bonus_eligible"]:
            return 0
        rule = plan["monetisation"]
        amount = receipt["base"] if double else receipt["base"] * rule["reward_bonus_numerator"] // rule["reward_bonus_denominator"]
        receipt["bonus"] = True
        self.credits += amount
        return amount


def profile_plan(source, name):
    plan = copy.deepcopy(source)
    profile = plan["profiles"][name]
    plan["selected_profile"] = name
    plan["run"].update(next_level_base_xp=profile["xp_base"],
                       next_level_per_current_level_xp=profile["xp_slope"], hardware_slots=profile["module_limit"])
    for track in plan["permanent"]["tracks"]:
        plan["permanent"]["tracks"][track]["per_rank"] = profile[track + "_per_rank"]
    return plan


def account_state(plan, state):
    ranks = {key: track["maximum_ranks"] for key, track in plan["permanent"]["tracks"].items()}
    unlocked = {item["id"] for item in plan["unlocks"]}
    return HangarAccount(ranks=ranks if state in ("stats_only", "maxed") else {},
                         unlocked=unlocked if state in ("unlocks_only", "maxed") else set())


def execute_run(trial):
    inputs, account = trial.inputs, trial.account
    # All unlocked blueprints enter the pool; policy preferences never curate it.
    spec = run_spec(inputs.spec, inputs.plan, account, trial.policy, expand=True)
    spec["loop"]["hardware_slots"] = inputs.plan["run"]["hardware_slots"]
    for module in spec["hardware"]:
        module["requires"] = {**module.get("requires", {}),
                              **inputs.plan.get("eligibility_prerequisites", {}).get(module["id"], {})}
    row = ShortRun(spec, short_world(inputs.world, inputs.plan), inputs.plan, trial.seed,
                   trial.policy, **trial.options).play()
    return dict(row, starting_ranks=dict(account.ranks),
                eligible_blueprints=sorted(account.available(inputs.plan)),
                sectors=int(row["seconds"] // inputs.plan["sectors"]["seconds"]))


@dataclass
class Journey:
    inputs: Inputs
    player: int
    attempts: int
    policy: str
    skill: str
    scenario: str


def execute_journey(journey):
    inputs = journey.inputs
    plan, account = inputs.plan, HangarAccount()
    revive = journey.scenario in ("revive", "both_50", "both_100")
    bonus = journey.scenario in ("credits_50", "both_50", "both_100")
    first_purchase = cap_at = all_unlocked = None
    unlock_at, rows, checkpoints = {}, [], {}
    active = ads = credits = 0
    for attempt in range(1, journey.attempts + 1):
        seed = 300000 + journey.player * 1000 + attempt
        row = execute_run(Trial(inputs, account, journey.policy, seed,
                                {"horizon": 1800, "skill": journey.skill, "revive": revive}))
        run_id = f"{journey.player}/{attempt}"
        base = account.settle(plan, run_id, row)
        extra = account.claim_credit_bonus(plan, run_id, True, double=journey.scenario == "both_100") if bonus and base else 0
        purchases = account.purchase(plan)
        active += row["seconds"]
        ads += row["revives_used"] + int(extra > 0)
        credits += base + extra
        if purchases and first_purchase is None:
            first_purchase = attempt
        if account.purchases == 20 and cap_at is None:
            cap_at = attempt
        for key in account.unlocked:
            unlock_at.setdefault(key, attempt)
        if len(account.unlocked) == len(plan["unlocks"]) and all_unlocked is None:
            all_unlocked = attempt
        if attempt in (1, 5, 10, 20, journey.attempts):
            checkpoints[str(attempt)] = {"run_minutes": row["seconds"] / 60,
                                          "purchases": account.purchases, "unlocks": len(account.unlocked)}
        rows.append(dict(row, player=journey.player, attempt=attempt, base_credits=base,
                         bonus_credits=extra, purchases_after=account.purchases))
    return {"first_purchase": first_purchase, "cap_at": cap_at, "all_unlocks_at": all_unlocked,
            "unlock_at": unlock_at, "active_minutes": active / 60, "assumed_ads": ads,
            "credits_earned": credits, "unspent_credits": account.credits,
            "checkpoints": checkpoints}, rows
