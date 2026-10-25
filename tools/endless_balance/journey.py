"""One sequential account journey; independent accounts may run on local workers."""
from .progression import Account, run_spec
from .short_run import ShortRun, short_world


def account_journey(task):
    spec, world, plan, player, attempts, policy, skill, scenario, revive, bonus = task
    account = Account()
    first_purchase = capped_at = all_unlocks_at = None
    unlock_at, snapshots, rows = {}, {}, []
    active, wall, ads, earnings, first_purchase_minutes = 0, 0, 0, 0, None
    for attempt in range(1, attempts + 1):
        seed = 100000 + player * 1000 + attempt
        row = ShortRun(run_spec(spec, plan, account, policy), short_world(world, plan), plan,
                       seed, policy, revive=revive, skill=skill, horizon=1800).play()
        rows.append({"phase": f"campaign_{scenario}_{policy}_{skill}", "player": player,
                     "account_ranks": dict(account.ranks),
                     "available_blueprints": sorted(account.available(plan)), **row})
        run_id = f"{player}/{attempt}"
        base = account.settle(plan, run_id, row)
        extra = account.claim_bonus(run_id, True) if bonus and base else 0
        run_ads = row["revives_used"] + int(bool(extra))
        active += row["seconds"]
        wall += row["seconds"] + run_ads * plan["monetisation"]["scenario_ad_seconds"]
        ads += run_ads
        earnings += base + extra
        purchases = account.purchase(plan)
        if purchases and first_purchase is None:
            first_purchase, first_purchase_minutes = attempt, active / 60
        if account.purchases == 20 and capped_at is None:
            capped_at = attempt
        for blueprint in account.unlocked:
            unlock_at.setdefault(blueprint, attempt)
        if len(account.unlocked) == len(plan["unlocks"]) and all_unlocks_at is None:
            all_unlocks_at = attempt
        if attempt in (1, 2, 5, 10, 20, attempts):
            snapshots[str(attempt)] = {"run_minutes": row["seconds"] / 60,
                                       "purchases": account.purchases, "credits": account.credits,
                                       "unlocks": len(account.unlocked)}
    return {"first_purchase": first_purchase, "first_purchase_minutes": first_purchase_minutes,
            "capped_at": capped_at, "all_unlocks_at": all_unlocks_at,
            "unlock_at": unlock_at, "active_minutes": active / 60,
            "wall_minutes_assumed": wall / 60, "completed_ads_assumed": ads,
            "credits_earned": earnings, "purchases": account.purchases,
            "unspent_credits": account.credits, "snapshots": snapshots}, rows
