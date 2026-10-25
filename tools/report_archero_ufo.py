#!/usr/bin/env python3
"""Render the selected Archero-style approach and its offline evidence."""
import hashlib
import html
import json
from pathlib import Path

DIRECTORY = Path(__file__).resolve().parents[1] / "design/balance"


def table(headers, rows):
    def row(tag, cells):
        return "<tr>" + "".join(f"<{tag}>{html.escape(str(cell))}</{tag}>" for cell in cells) + "</tr>"
    return "<table><thead>" + row("th",headers) + "</thead><tbody>" + "".join(row("td",cells) for cells in rows) + "</tbody></table>"


def main():
    result = json.loads((DIRECTORY / "archero-ufo-results.json").read_text())
    comparison = json.loads((DIRECTORY / "archero-comparison-results.json").read_text())
    followup = json.loads((DIRECTORY / "archero-unlock-followup.json").read_text())
    plan = result["plan_snapshot"]
    total = sum(r["simulated_runs"] for r in (result,comparison,followup))
    states = result["holdout"]["states"]
    fresh, maxed = states["fresh"]["Hybrid"], states["maxed"]["Hybrid"]
    rows = [[p,*[states[s][p]["median_observed_minutes"] for s in ("fresh","stats_only","unlocks_only","maxed")],
             states["fresh"][p]["median_picks"],f'{states["fresh"][p]["evolution_any_pct"]}%'] for p in states["fresh"]]
    outcomes = table(["Policy","Fresh median min","Hangar only","Unlocks only","Hangar + unlocks","Fresh picks","Fresh evolving"],rows)
    rows = []
    for scenario,policies in result["journeys"].items():
        for policy,value in policies.items():
            rows.append([scenario,policy,value["median_first_purchase_attempt"],value["median_cap_attempt_among_reached"],
                         f'{value["cap_reached_pct"]}%',value["median_all_unlocks_attempt_among_reached"],value["median_assumed_ads"]])
    journey_table = table(["Reward scenario","Policy / dodge model","First purchase","Cap attempt*","Capped by 40","All unlocks*","Ads assumed"],rows)
    rows = []
    for name,profiles in comparison["comparison"].items():
        for state,policies in profiles.items():
            rows.append([name,state,*[policies[p]["median_observed_minutes"] for p in ("Hybrid","Collector","Gunship")],
                         policies["Hybrid"]["median_picks"],f'{policies["Hybrid"]["evolution_any_pct"]}%'])
    comparison_table = table(["Candidate","Account","Hybrid min","Collector min","Gunship min","Hybrid picks","Hybrid evolving"],rows)
    rows = [[case,state,*[policies[p]["median_observed_minutes"] for p in ("Hybrid","Collector","Gunship")]]
            for case,states_ in result["sensitivity"].items() for state,policies in states_.items()]
    sensitivity_table = table(["Sensitivity","Account","Hybrid median min","Collector median min","Gunship median min"],rows)
    unlocks = table(["Equipment","Free cumulative milestone"],[[u["id"].replace("_"," ").title(),f'{u["threshold"]:,} {u["counter"].replace("_"," ")}'] for u in plan["unlocks"]])
    prerequisites = table(["Unlocked equipment","Run prerequisites before it can appear"],
                          [[key.replace("_"," ").title(),", ".join(f'{k.replace("_"," ").title()} {v}' for k,v in values.items())]
                           for key,values in plan["eligibility_prerequisites"].items()])
    no_ads = result["journeys"]["no_ads"]["Hybrid_regular"]
    bonus = result["journeys"]["credits_50"]["Hybrid_regular"]
    both = result["journeys"]["both_50"]["Hybrid_regular"]
    doubled = result["journeys"]["both_100"]["Hybrid_regular"]
    gates = {"Fresh Hybrid median between five and ten active minutes":5<=fresh["median_observed_minutes"]<=10,
             "Maxed Hybrid median at most thirteen active minutes":maxed["median_observed_minutes"]<=13,
             "Hangar-only paired median improvement at least one minute":result["holdout"]["paired_effects_against_fresh"]["Hybrid"]["stats_only"]["median_paired_lifetime_change_minutes"]>=1,
             "First Hybrid purchase by median attempt two without ads":no_ads["median_first_purchase_attempt"]<=2,
             "At least ninety percent of fresh Hybrid runs survive one minute":fresh["survival_1min_pct"]>=90}
    review = {"scope":"Derived model gates, not gameplay, human fun or commercial qualification.","runs_in_current_datasets":total,
              "selected_profile":plan["selected_profile"],"gates":gates,
              "risks":["Collector and other specialist policies are not fully balanced.","Automatic unlocks change offers and can weaken individual drafts.",
                       "Menus, player input, spatial targeting and actual readability are unmeasured.","No revenue or retention forecast; campaigns force repeated participation."]}
    review["evidence_sha256"] = {name:hashlib.sha256((DIRECTORY/name).read_bytes()).hexdigest() for name in (
        "archero-ufo-results.json","archero-comparison-results.json","archero-unlock-followup.json","archero-ufo-system.json")}
    (DIRECTORY/"archero-model-review.json").write_text(json.dumps(review,indent=2)+"\n")
    gate_table = table(["Proposed model gate","Result"],[[key,"Pass in model" if value else "Needs revision"] for key,value in gates.items()])
    steps = "".join(f"<li>{html.escape(s)}</li>" for s in plan["implementation_order"])
    c_before = followup["results"]["unrestricted"]["maxed"]["Collector"]["median_observed_minutes"]
    c_after = followup["results"]["contextual"]["maxed"]["Collector"]["median_observed_minutes"]
    body = f'''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Human Resources — Archero model</title><style>body{{max-width:1120px;margin:48px auto;padding:0 24px;font:16px/1.6 system-ui,sans-serif;color:#172332;background:#fafbf9}}h1{{font-size:40px;line-height:1.15}}h2{{margin-top:34px}}table{{width:100%;border-collapse:collapse;font-size:14px;margin:18px 0}}th,td{{padding:10px;text-align:left;border-bottom:1px solid #ccd7d1}}th{{background:#e4eee8}}a{{color:#165c9e}}li{{margin:7px 0}}.lead{{font-size:21px}}.note{{padding:18px 22px;background:#e5eee8;border-left:4px solid #527f69}}small{{color:#50636b}}details{{margin:24px 0}}summary{{cursor:pointer;font-weight:650}}code{{background:#e8eeea;padding:2px 5px}}pre{{white-space:pre-wrap}}</style><body>
<small>24 October 2026 · Owner-selected model · Numerical playtest candidate</small>
<h1>Archero progression.<br>An endless UFO run.</h1>
<p class="lead">Play immediately, choose temporary abilities during the attempt, then use earned credits to improve the next starting ship. Keep one optional Hangar screen and let equipment unlocks join future runs automatically.</p>
<div class="note">The owner selected Archero as the primary reference. This plan adopts its two-layer progression, adapting it to the UFO runner. {total:,} runs are recorded across the current comparison, confirmation, sensitivity, repeated-account and unlock follow-up datasets. The fresh mixed-build median is {fresh['median_observed_minutes']} active minutes; the fully developed account median is {maxed['median_observed_minutes']}.</div>
<p>The <a href="https://play.google.com/store/apps/details?id=com.habby.archero&amp;hl=en">official Archero listing</a> describes random skills and equipment that increases character stats. Our particular values, six-module boundary, endless director, eligibility rules and rewards are proposed UFO design decisions, not claims about Archero’s exact rules or economy.</p>
<h2>The approach to build</h2>
<ol><li><strong>Start.</strong> One starting laser and one beam, plus purchased Hangar stat bonuses. No mandatory blueprint preparation. Lasers continue firing while the UFO moves.</li>
<li><strong>Upgrade during the run.</strong> Earn XP, pause, see three distinct eligible cards and choose one. Start with the <code>16 + 3 × current level</code> XP threshold. Preserve surplus XP. Install at most six modules and improve each through three separate picks. Beam Splitter still gives +1 beam per pick: 1 → 2 → 3 → 4.</li>
<li><strong>Build towards combinations.</strong> First two completed rank-III recipes evolve automatically. Research sustains choices beyond completed modules. Two free rerolls per attempt; no ads on level-up cards. Visible rarity remains 60/35/5 for new blueprint/research quality draws, with unavailable tiers renormalized; owned ranks draw from Common while keeping their original rarity label.</li>
<li><strong>End, bank and retry.</strong> Death ends the attempt unless an eligible optional revive completes. Bank Research Credits and achievement progress. Retry immediately or spend credits in the Hangar. Run levels, modules, temporary research and evolutions reset.</li></ol>
<p>Use ninety-second sectors as distance/record goals within the endless run. Sectors do not open shops, refill the ship, reset difficulty or end the attempt. The current director escalates from active run time, shared by all accounts and independent of their purchases, equipment or ad choices.</p>
<h2>One permanent upgrade screen</h2>
{table(['Hangar track','One rank','Cap after five ranks'],[['Weapon calibration','+5% original base weapon damage','+25%'],['Hull reinforcement','+6 starting/maximum hull','+30 hull'],['Shield capacity','+4 starting/maximum shield','+20 shield'],['Biological capture','+3% base capture rate','+15%']])}
<p>Each track costs <strong>80, 140, 240, 400, 640</strong> credits for ranks I–V: twenty purchases and 6,000 credits to cap the Hangar. These gains add to fixed starting templates. Permanent upgrades never add lasers, beams, reflection chance, invulnerability or faster shield regeneration; those build changes remain earned during each run. Run modifiers then act on the improved templates.</p>
<p>Keep run XP and Research Credits visibly separate. Runs lasting at least thirty active seconds award <code>floor(seconds/8) + floor(score/5) + 2×vehicle takedowns + 5×boss kills</code> credits. Each takedown is counted once. Death or voluntary ending banks earned rewards under the same rules. A spend screen is optional, and there is no entry currency.</p>
<h2>Unlock options automatically, when they are usable</h2>
<p>All sixteen module concepts are built for the first playable. Ten start unlocked: Laser Array, Rail Lens, Orbit Drones, EMP Ring, Beam Splitter, Tractor Lens, Shield Generator, Repair Swarm, Vector Thrusters and Bounty Scanner. Six free cumulative milestones add alternatives:</p>
{unlocks}
<p>An unlock changes future card eligibility; it does not install the module or grant rank III. Some unlocked cards wait for visible run prerequisites. Show these in unlock summaries and previews:</p>
{prerequisites}
<p>Heavy Winch waits for three beam slots and an upgraded laser, so lifting a vehicle can leave a biological beam available. In a separate paired follow-up, contextual prerequisites changed the capped Collector median from {c_before} to {c_after} minutes on one hundred seeds. Other build effects were small in that sample. This is a useful mitigation, not proof that every new unlock improves every future draft.</p>
<h2>Results on new confirmation seeds</h2>
<p>Two hundred runs per policy/state, using seeds 60000–60199 rather than the comparison seeds. “Hangar only” has capped permanent stats with the initial ten-blueprint pool. “Unlocks only” has all blueprints and no permanent stats. The final column state combines both. All use the same enemy curve. Preset bots are heuristic selection policies, not measured human players.</p>
{outcomes}
<p>Fresh Hybrid’s first pick arrives at median {fresh['median_first_pick_seconds']} seconds; its median pick interval is {fresh['median_pick_interval_seconds']} seconds. Observation ends at thirty minutes; survivors are censored, not assigned a death. Menus, shop decisions and ads add wall-clock time. At an illustrative three seconds per pick, {fresh['median_picks']} picks add about {round(fresh['median_picks']*3)} seconds of decision time; that assumption has not been measured.</p>
{gate_table}
<h2>Progress without ads; optional rewards accelerate it</h2>
<p>For regular Hybrid accounts, the median first purchase is attempt {no_ads['median_first_purchase_attempt']}. Permanent power caps at attempt {no_ads['median_cap_attempt_among_reached']} without ads, {bonus['median_cap_attempt_among_reached']} with the +50% credit reward always completed, and {both['median_cap_attempt_among_reached']} with both reward types always completed. The double-credit comparison caps at {doubled['median_cap_attempt_among_reached']} with both rewards, which consumes this finite progression goal sooner.</p>
<ul><li><strong>Revive:</strong> at most one after ninety active seconds; restore half maximum hull, zero shield and three seconds of invulnerability. Clear ordinary active threats, retain bosses, XP, build and run time.</li>
<li><strong>Credit reward:</strong> one completed optional placement grants <code>floor(base credits/2)</code> extra. No extra XP or achievement credit. Hide it once remaining permanent ranks are already affordable.</li>
<li><strong>Cosmetic packs:</strong> a later purchase option. The first playable uses local ad/billing test doubles and needs no online service.</li></ul>
<p>Declining an offer, unavailable inventory, failure or incomplete ads permit ordinary results/restart. Rewards settle once. The +50% amount is an economy candidate; this study does not measure whether players want that reward or forecast ad revenue.</p>
<details><summary>Repeated-account evidence and all reward scenarios</summary><p>Twelve accounts per policy/scenario, forty attempts each. Participation and skill are fixed; all eligible rewards in a scenario are assumed completed. This is not a retention model. Cap/unlock attempts marked * are conditional on reaching the goal; the reach percentage is shown alongside. Thirty ad seconds is an assumed exposure cost, not measured inventory.</p>{journey_table}</details>
<h2>What the comparisons changed</h2>
<ul><li>Use the steadier XP curve: the faster curve produces more picks and longer attempts; the slowest curve leaves many builds without a completed evolution.</li>
<li>Keep six installed modules. Allowing all sixteen families to be installed spreads upgrades out and reduces evolution completion for several preset policies. It remains a design choice, not a direct copy of Archero.</li>
<li>Use capped +25% weapon damage as the starting permanent-power candidate. The larger +50% profile extends runs further and creates a stronger progression dependency; it is available in the comparison data for review.</li>
<li>Use automatic unlocked-card eligibility, with prerequisites that make vehicle/conditional technologies usable. Keep the Hangar focused on four stat tracks.</li>
<li>Test +50% credits rather than doubling to preserve more of the first-playable progression goal.</li></ul>
<details><summary>All six candidates — forty runs per cell</summary><p>Comparison seeds 50000–50039. The selected profile is hangar_25. Each candidate applies the same contextual eligibility overlay; their XP, module-count and permanent-power parameters are in the plan JSON.</p>{comparison_table}</details>
<details><summary>Dodging, target-density, revive and timestep sensitivity</summary><p>Sixty runs per cell using seeds 70000–70059. Density varies by ±35%. Novice/expert change hit/dash proxies. The half-second sample is sensitivity evidence, not a paired convergence proof.</p>{sensitivity_table}</details>
<h2>Remaining risks and next evidence</h2>
<p>Collector and other specialist rewards still require tuning, and new card options can weaken individual random drafts. The bots do not plan recipes optimally. Do not interpret preset survival rankings as settled human build balance. Check upgrade clarity, meaningful choices, avoidable deaths, revive value and voluntary replay with repeated hands-on attempts.</p>
<p>The encounter model still approximates spatial movement, dodging, capture retention and laser targeting. Thrusters affect both dodging and encounter/lifetime proxies; actual lateral movement may behave differently. Laser/drone damage is pooled; geometry, visual readability and input latency are unmeasured. Bosses may leave after ninety seconds, so survival does not imply killing them. Vehicle capture score remains an assumed five. There is no native gameplay, human fun, learning, churn, retention, eCPM, ad-fill, purchase-conversion or revenue measurement.</p>
<p>The Hangar and six unlocks are a finite first-playable goal. Continued play can pursue score, distance and builds; this is not a content plan for months of live operation. Keep the chosen Archero progression model stable while using human/device evidence to revise numbers.</p>
<h2>Implementation order</h2><ol>{steps}</ol>
<p>Persist credits, Hangar ranks, unlocks, records and cosmetic ownership; temporary abilities reset. Use a bounded pending/latest-settled reward receipt and atomic save replacement. The existing proposal checkpoints earned rewards every ten active seconds and on backgrounding; terminated-process recovery settles to results, while full active-run resume is deferred. These remain native implementation requirements. Offline tests prove in-memory transaction behavior only.</p>
<h2>Reproduction</h2>
<p><a href="archero-ufo-system.json">Selected approach and numerical input</a> · <a href="archero-ufo-results.json">Confirmation, sensitivity and account data</a> · <a href="archero-comparison-results.json">Six-candidate comparison</a> · <a href="archero-unlock-followup.json">Prerequisite follow-up</a> · <a href="archero-model-review.json">Derived model gates</a> · <a href="archero-ufo-examples.json">Traced offers</a>. Compressed JSONL files retain every run. The earlier unrestricted experiment and its exact changed-source archive remain alongside these files as historical evidence.</p>
<p>Thirty-six focused model tests pass (eleven for this adaptation, thirteen for account progression and twelve for the inherited encounter model). The foundation static audit passes. All 19,360 records pass slot/rank/eligibility/evolution/revive constraints and recorded source hashes match. Twenty traced replays reproduce observed outcomes and pass per-offer prerequisite checks; those replays are excluded from the study count. No native gameplay, platform build or live SDK qualification is inferred.</p>
<pre>python3 tools/test_archero_ufo.py
python3 tools/test_short_progression.py
python3 tools/test_endless_balance.py
python3 tools/simulate_archero_ufo.py --compare --runs 40 --workers 6
python3 tools/check_archero_unlocks.py --runs 100
python3 tools/simulate_archero_ufo.py --runs 200 --players 12 --attempts 40 --stress-runs 60 --workers 6
python3 tools/report_archero_ufo.py</pre>
</body></html>'''
    (DIRECTORY/"archero-ufo-approach.html").write_text(body)
    print("Wrote",DIRECTORY/"archero-ufo-approach.html")


if __name__ == "__main__":
    main()
