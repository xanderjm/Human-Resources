#!/usr/bin/env python3
"""Render the tested short-run plan and measured model outcomes as local HTML."""
import html
import hashlib
import json
from pathlib import Path

DIRECTORY = Path(__file__).resolve().parents[1] / "design/balance"


def table(headers, rows):
    def cells(tag, values):
        return "<tr>" + "".join(f"<{tag}>{html.escape(str(value))}</{tag}>" for value in values) + "</tr>"
    return "<table><thead>" + cells("th", headers) + "</thead><tbody>" + "".join(cells("td", row) for row in rows) + "</tbody></table>"


def main():
    result = json.loads((DIRECTORY / "short-run-results.json").read_text())
    calibration = json.loads((DIRECTORY / "short-run-calibration-results.json").read_text())
    plan = result["plan_snapshot"]
    total = result["simulated_runs"] + calibration["simulated_runs"]
    rows = []
    for policy, fresh in result["holdout"]["fresh"].items():
        maxed = result["holdout"]["maxed"][policy]
        rows.append([policy, fresh["median_observed_minutes"],
                     f'{fresh["p10_observed_minutes"]}–{fresh["p90_observed_minutes"]}',
                     maxed["median_observed_minutes"], fresh["median_picks"],
                     f'{fresh["evolution_any_pct"]}%', fresh["median_base_credits"]])
    runs_table = table(["Selection policy", "Fresh median (min)", "Fresh 10th–90th percentile", "Maxed median (min)",
                        "Fresh picks", "Fresh runs evolving", "Fresh base credits"], rows)
    rows = []
    for scenario, policies in result["campaigns"].items():
        for policy, value in policies.items():
            rows.append([scenario, policy, value["first_purchase_median_attempt"],
                         value["median_attempt_to_cap_among_reached"], f'{value["maxed_pct"]}%',
                         value["median_attempt_all_unlocks_among_reached"], value["median_completed_ads_assumed"]])
    campaign_table = table(["Scenario", "Policy / dodge model", "First purchase (attempt)",
                            "Cap attempt, among reached", "Capped by attempt 40", "All unlocks, among reached", "Completed ads assumed"], rows)
    rows = []
    for case, states in result["sensitivity"].items():
        for state, policies in states.items():
            rows.append([case, state, *[policies[p]["median_observed_minutes"] for p in ("Hybrid", "Collector", "Gunship")]])
    stress_table = table(["Sensitivity case", "Account", "Hybrid median min", "Collector median min", "Gunship median min"], rows)
    track_table = table(["Permanent track", "One purchase", "Maximum"], [
        ["Weapon calibration", "+3% base damage", "+15%"], ["Hull", "+4 maximum/start hull", "+20 hull"],
        ["Shield", "+4 maximum/start shield", "+20 shield; no faster recharge"],
        ["Biological capture", "+2% base capture rate", "+10%; no extra beam"]])
    unlock_table = table(["Blueprint", "Free cumulative achievement"], [[u["id"].replace("_", " ").title(),
                           f'{u["threshold"]:,} {u["counter"].replace("_", " ")}'] for u in plan["unlocks"]])
    steps = "".join(f"<li>{html.escape(step)}</li>" for step in plan["implementation_order"])
    first = result["holdout"]["fresh"]["Hybrid"]
    capped = result["holdout"]["maxed"]["Hybrid"]
    no_ads = result["campaigns"]["no_ads"]["Hybrid_regular"]
    bonuses = result["campaigns"]["credit_bonus"]["Hybrid_regular"]
    revive = result["sensitivity"]["revive"]["fresh"]["Hybrid"]
    gates = {
        "Fresh regular Hybrid median in 5–10 minutes": 5 <= first["median_observed_minutes"] <= 10,
        "All maxed regular policy medians at most 13 minutes": all(v["median_observed_minutes"] <= 13 for v in result["holdout"]["maxed"].values()),
        "Median first purchase by attempt two without ads": no_ads["first_purchase_median_attempt"] <= 2,
        "Hybrid first evolution median at most six minutes, among reached": first["median_first_evolution_minutes_among_reached"] is not None and first["median_first_evolution_minutes_among_reached"] <= 6,
        "At least 90% fresh Hybrid runs survive one minute": first["survival_1min_pct"] >= 90,
    }
    gate_table = table(["Proposed model gate", "Observed"], [[key, "Pass in model" if value else "Needs revision"] for key, value in gates.items()])
    acceptance = {"scope": "Derived model gates only; no native, human fun or commercial qualification.",
                  "simulated_runs": total, "gates": gates,
                  "source_sha256": {name: hashlib.sha256((DIRECTORY / name).read_bytes()).hexdigest()
                                    for name in ("short-run-results.json", "short-run-calibration-results.json", "short-run-progression.json")}}
    (DIRECTORY / "short-run-model-acceptance.json").write_text(json.dumps(acceptance, indent=2) + "\n")
    rows = []
    for policy in ("Hybrid", "Collector", "Gunship"):
        ordinary = result["holdout"]["fresh"][policy]["mean_credits_per_active_minute"]
        quit_rate = result["sensitivity"]["quit_2min"]["fresh"][policy]["mean_credits_per_active_minute"]
        rows.append([policy, ordinary, quit_rate])
    farming_table = table(["Policy", "Death-run credits / active minute", "Two-minute cash-out credits / active minute"], rows)
    body = f'''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Human Resources — Short-run first-playable plan</title><style>
body{{max-width:1140px;margin:48px auto;padding:0 24px;background:#fafbf9;color:#172332;font:16px/1.6 system-ui,sans-serif}}
h1{{font-size:40px;line-height:1.12;max-width:850px}}h2{{margin-top:38px}}p,li{{max-width:1000px}}li{{margin:8px 0}}
table{{width:100%;border-collapse:collapse;font-size:14px;margin:18px 0}}th,td{{padding:10px 12px;text-align:left;border-bottom:1px solid #c9d5d0}}th{{background:#e5efeb}}
a{{color:#155b9c}}code{{padding:2px 4px;background:#e9eeea}}.lead{{font-size:21px}}.note{{padding:18px 24px;background:#e5eee9;border-left:4px solid #4e7d68}}small{{color:#51636b}}pre{{white-space:pre-wrap}}
</style><body><small>24 October 2026 · Proposed first playable · Offline evidence</small>
<h1>Short runs. Temporary builds.<br>Permanent, bounded progress.</h1>
<p class="lead">Keep the excitement of choosing three random upgrades, let every meaningful attempt earn persistent Research Credits, and unlock new equipment through play. Target roughly 5–10 active minutes for a regular fresh build, with longer strong runs. The game remains endless.</p>
<div class="note">{total:,} recorded model runs: {calibration['simulated_runs']:,} calibration and {result['simulated_runs']:,} confirmation/sensitivity/account runs. The regular Hybrid policy lasted {first['median_observed_minutes']} minutes fresh and {capped['median_observed_minutes']} minutes at capped permanent power. These are model medians, not measured human run lengths, retention or revenue.</div>
<h2>The player loop</h2>
<ol><li><strong>Prepare.</strong> Use exactly ten unlocked blueprints for the upgrade pool. This is separate from the six hardware modules that can be installed during a run. Newly earned equipment can replace a blueprint; players cannot thin the pool to force particular offers. Every saucer starts with one laser and one abduction beam, plus purchased permanent stat bonuses.</li>
<li><strong>Play.</strong> Collect specimens, avoid military attacks and earn run XP. Pause at each earned level, draw three different eligible upgrade families and choose one. Preserve surplus XP. Installed modules use one slot each; further picks improve the same module to III.</li>
<li><strong>Develop a build.</strong> Beam Splitter adds exactly one beam per pick: 1 → 2 → 3 → 4. Matching rank-III pairs can earn an evolution; the first two completed recipes activate. Research remains available after hardware is complete.</li>
<li><strong>Finish.</strong> Death ends the attempt unless an eligible optional revive is completed. Bank earned Research Credits, update records and achievements, then show the next affordable upgrade/unlock.</li>
<li><strong>Improve and retry.</strong> Buy capped permanent improvements or change the blueprint loadout. Temporary run XP, levels, modules, research, evolutions and rerolls reset. Credits, permanent ranks, unlocks, records and cosmetics persist.</li></ol>
<p>For all candidates, rarity weights remain Common 60 / Uncommon 35 / Rare 5 for discovering blueprints and research quality, with unavailable buckets renormalized. Owned module ranks move into the Common draw bucket while retaining their original rarity label; an owned card is never guaranteed. Give two rerolls per run. Four uncapped repeatable run-research families gain 1× on choices 1–15, 2× on 16–30, 3× on 31–45 and so on, adding against fixed bases. These gains reset after the run. Fire rate, capture speed and emergency repairs do not receive this bundle multiplier.</p>
<h2>Permanent progression</h2>
{track_table}
<p>Each track has five ranks costing <strong>80, 140, 240, 400 and 640 credits</strong>. Twenty purchases cap all four tracks for 6,000 credits. There are no permanent additional lasers, beams, reflection chances, invulnerability or recharge upgrades. This keeps major build changes inside the run. Simulation accounts buy the lowest-rank affordable track, with ties ordered damage → hull → shield → capture; this is a declared heuristic, not an optimal purchasing strategy.</p>
<p>A run lasting at least 30 active seconds awards <code>floor(seconds / 8) + floor(score / 5) + 2 × vehicle takedowns + 5 × boss kills</code> credits. Vehicle takedowns count jeep/tank kills or captures once. Score bonuses can improve credit earnings, but never XP. Voluntarily ending a run banks earned rewards under the same rules; no entry fee, energy system or loss of previously earned currency.</p>
<h2>Equipment unlocks</h2>
<p>All sixteen existing module concepts are included in the first playable. Ten begin unlocked: Laser Array, Rail Lens, Orbit Drones, EMP Ring, Beam Splitter, Tractor Lens, Shield Generator, Repair Swarm, Vector Thrusters and Bounty Scanner. Six are free cumulative milestones:</p>
{unlock_table}
<p>Milestone counters count actual play and base credit earnings; rewarded credit bonuses do not advance them. Unlocking a blueprint adds a possible choice for future runs. It neither installs it nor increases its starting rank. Offer an explicit loadout swap when a new blueprint is earned, so an expanding collection does not endlessly dilute the draw pool. Automated loadouts follow each selection policy’s fixed preferences, without knowledge of the world seed. New options are intended as alternative builds; their equivalence still needs player testing.</p>
<h2>Endless difficulty, without a death timer</h2>
<p>The same active-time director runs for every account. Soldiers appear from the start, jeeps from 0:30 and tanks from 1:40. A boss arrives at 2:30, then every three minutes. After two minutes, ordinary/boss health adds 20% of its spawn template per elapsed minute and damage adds 25%; successive bosses also have a larger health template. Ordinary spawn rates grow with time up to a fourfold cap. The selected pressure multiplier is 0.65, applied to ordinary spawning and boss health. Active caps remain eighteen soldiers, five jeeps and three tanks. All are model assumptions requiring native telegraph and reaction-time tests.</p>
<p>The director never reads permanent ranks, selected equipment, spending or ad choices. A player can survive beyond every target duration. Do not turn higher permanent power into a matching hidden enemy buff: progression should actually help.</p>
<h2>Monetisation without interrupting the build</h2>
<ul><li><strong>Optional revive:</strong> at most once per run, after at least 90 active seconds. Restore 50% maximum hull, zero shield, three seconds of invulnerability and clear ordinary active threats; keep bosses, build, XP and run time. Difficulty does not restart. In the fresh Hybrid sensitivity sample the median time from revive to second death/horizon was {revive['median_revive_extension_seconds']} seconds.</li>
<li><strong>Optional credit bonus:</strong> one extra copy of the base run credit reward after a completed ad. No recursive doubling, bonus XP or doubled achievements. Hide the offer when all remaining permanent upgrades are already affordable. Keep surplus credits, with no promise of future purchasing value.</li>
<li><strong>Cosmetic packs:</strong> sell appearance changes after the game is fun enough to support them. No price or conversion assumption is used here. Permanent power is earned through play in this candidate.</li></ul>
<p>Decline, unavailable inventory or a failed/incomplete ad always allows ordinary results/restart and never grants a completion reward. Level-up choices and active gameplay have no ad placement. The first playable uses local placement test doubles; live ads, billing, consent/audience decisions and provider integration are a later slice.</p>
<h2>Short-run confirmation: 200 seeds per cell</h2>
<p>Calibration used seeds 0–39 across five pressures. Pressure 0.65 was selected because fresh mixed/combat builds approached 8–9 minutes while capped versions stayed near 9–11 minutes. The table below uses different seeds, 10000–10199, a 30-minute observation horizon and the same director for every account. Maxed accounts have all twenty permanent ranks and all unlocks; this combines stat and equipment availability effects. Selection/dodging bots are approximations, not measured players.</p>
{runs_table}
<p>“Fresh runs evolving” includes all attempts, not only survivors. Evolution timing in the summary JSON is conditional on reaching an evolution. A survivor at the 30-minute horizon is censored, never treated as a death. Upgrade menus pause the active clock; human decision, shop and ad time make actual sessions longer.</p>
{gate_table}
<h2>Repeated accounts and optional ads</h2>
<p>Each scenario forces twenty model accounts per listed policy to play forty attempts. Skill stays fixed; there is no assumed learning, churn or voluntary return behavior. The same account/attempt seed is paired across ad scenarios. “Credit bonus”, “revive” and “both” assume every eligible offered reward is accepted and completed. They are exposure bounds, not predicted ad uptake.</p>
{campaign_table}
<p>For the regular Hybrid policy, the median cap attempt was {no_ads['median_attempt_to_cap_among_reached']} without ads and {bonuses['median_attempt_to_cap_among_reached']} with eligible credit bonuses, among accounts reaching the cap. Report the capped percentage alongside these conditional medians; accounts not capped by forty attempts have unknown eventual cap times. The model assumes thirty wall-clock seconds per completed ad. Its wall-clock totals exclude player thinking, shop time and loading, and are therefore not complete session durations.</p>
<p>After permanent power is capped, scores, skill and builds remain playable, but this first-playable economy has a finite progression goal. It does not demonstrate months of live-game content. Do not add new permanent power indefinitely to manufacture another spending target.</p>
<h2>Sensitivity and farming checks</h2>
<p>Sixty seeds per cell, separate from calibration/confirmation. Density varies by ±35%; novice/expert differ only in hit/dash proxy values. Expanded pool makes all unlocked blueprints eligible instead of curating ten. Quit-at-two-minutes is a voluntary settlement test, not a simulated death. Halving the timestep is a sensitivity check, not a convergence proof.</p>
{stress_table}
{farming_table}
<p>The cash-out rate is compared with ordinary observed death runs on a different seed set, without ads. It can flag a large early-reset exploit but does not prove that no optimal farming strategy exists. Native movement, reward settlement and exploit tests remain necessary.</p>
<h2>Issues to revisit before balance approval</h2>
<ul><li><strong>Specialist rewards still need work.</strong> Fresh Score builds last only 4.43 minutes and earn 24.66 credits per active minute, below Hybrid’s 28.19. Collector’s 25.96 is also lower when fresh. Capped collectors improve substantially, but these starting rewards do not yet establish an attractive specialist tradeoff.</li>
<li><strong>Recipe access and guidance matter.</strong> The fresh Defender preset never evolved in the confirmation samples. Its preferred rare defensive equipment is locked and the bot does not plan a different recipe. This flags progression/tutorial design and policy limitations; it does not prove that no fresh defensive build can evolve.</li>
<li><strong>Revive value varies.</strong> The fresh Hybrid median gained 76 seconds after revival, while capped Gunship’s median was only 44 seconds in its sensitivity sample. Test whether that feels worthwhile before treating revives as a reliable product benefit.</li>
<li><strong>Two rewards can shorten the progression goal sharply.</strong> Regular Hybrid accounts capped at median attempt nine when both rewards were always completed, versus twenty-three without ads. That is a finite first-playable economy, not proof of durable retention. Review the pace with humans before adding a larger upgrade tree or shipping both placements.</li>
<li><strong>The shorter target is for active play.</strong> Hybrid’s first choice arrives at median sixteen seconds and its median choice interval is twenty seconds, with twenty-three picks per fresh run. Decision time increases wall-clock session length. Check whether that many pauses are enjoyable on mobile.</li></ul>
<h2>Implementation plan and acceptance</h2>
<ol>{steps}</ol>
<p><strong>Persistence proposal:</strong> the account record owns credits, capped ranks, counters, loadout, records and the pending reward receipt. Save a bounded run reward checkpoint every ten active seconds and on backgrounding. The initial slice can recover a terminated process to results using that checkpoint; full active-run resume is deferred. Ordinary pauses within the same process resume the run. At most ten seconds of uncheckpointed reward progress can be lost after process termination. Settle the checkpoint once, with no offline earnings. This is a planned contract, not implemented native storage.</p>
<p>Keep a bounded pending/latest-settled reward receipt rather than accumulating every historical run in the shipped save. Atomic reward settlement and bonus completion must survive interruption, duplicate callbacks and reloads. The offline Account keeps forty receipts for audit and tests idempotency in memory; it does not qualify durable save/recovery behavior.</p>
<p><strong>First-playable acceptance:</strong> the complete run → results → purchase/unlock → restart loop works without ads or a network; previews show exact per-pick changes; pause/ad/background time gives no XP or difficulty; unavailable ads do not block progress; a new attempt retains only declared account state; save failures remain distinguishable from a fresh account. Run focused native tests and the repository aggregate gate when those owners are implemented.</p>
<p>Thirteen progression-model tests and twelve inherited encounter-model tests pass, including reward idempotency, capped purchases, reset/unlock behavior, single-revive limits and identical results across serial/parallel batches. The foundation static audit passes. Native gameplay, durable storage and live ad callbacks have not been implemented or qualified here.</p>
<h2>What the study can and cannot establish</h2>
<p>This extends the earlier offline encounter model. It does not execute native gameplay, physically simulate lanes/collision, test real targeting or measure human enjoyment. Movement still affects dodge probability, encounters and ordinary enemy screen lifetime, which can overvalue thrusters if the real runner separates lateral speed from forward scrolling. Lasers/drones pool damage into the oldest threat; arcs, pulse geometry and capture retention are statistical proxies. Bosses can leave after ninety seconds, so surviving a boss does not imply defeating it. Vehicle capture score remains an assumed flat five before credit conversion.</p>
<p>The campaign does not model ad fill, consent, completion probability, price, eCPM, payer conversion, retention, acquisition cost or lifetime value. No revenue is forecast. Successful model gates support a playable experiment; they do not prove commercial success or fun. The next evidence should come from repeated human attempts and actual device controls/readability, not additional precision on assumed numbers.</p>
<h2>Evidence and reproduction</h2>
<p><a href="short-run-progression.json">Machine-readable plan</a> · <a href="short-run-results.json">Results, snapshots and source hashes</a> · <a href="short-run-calibration-results.json">Calibration</a> · <a href="endless-simulation-input.json">Frozen base catalogue</a> · <a href="endless-encounter-assumptions.json">Inherited encounter assumptions</a>. Compressed JSONL files next to each result retain every modeled run.</p>
<pre>python3 tools/test_short_progression.py
python3 tools/test_endless_balance.py
python3 tools/simulate_short_progression.py --pilot --runs 40
python3 tools/simulate_short_progression.py --runs 200 --players 20 --attempts 40 --stress-runs 60
python3 tools/report_short_progression.py</pre>
</body></html>'''
    (DIRECTORY / "short-run-plan-and-results.html").write_text(body)
    print("Wrote", DIRECTORY / "short-run-plan-and-results.html")


if __name__ == "__main__":
    main()
