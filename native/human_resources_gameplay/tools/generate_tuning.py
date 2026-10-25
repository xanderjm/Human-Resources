#!/usr/bin/env python3
"""Validate authoring authorities and emit the bounded native numeric contract.

Module numbers are extracted from their exact catalogue phrases. A catalogue
wording change fails the cook instead of silently retaining stale runtime tuning.
The abstract balance simulator is never imported or executed.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re

MODULE_IDS = ('laser_array', 'rail_lens', 'arc_coil', 'orbit_drones', 'emp_ring',
              'beam_splitter', 'tractor_lens', 'heavy_winch', 'overtime_reactor',
              'reserve_battery', 'shield_generator', 'repair_swarm', 'mirror_plating',
              'vector_thrusters', 'phase_drive', 'bounty_scanner')
RARITIES = ('Common', 'Uncommon', 'Rare')


def validate(arch, cat, encounter, spatial):
    def finite(value, name, minimum=0, maximum=1e9):
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or not minimum <= value <= maximum:
            raise ValueError(f'invalid bounded rule {name}: {value!r}')

    baseline = cat['loop']['baseline']
    for key, value in baseline.items():
        finite(value, key)
    for key in ('hull', 'laser_damage', 'laser_shots_per_second', 'capture_seconds',
                'capture_radius_metres', 'movement_metres_per_second'):
        finite(baseline[key], key, 0.001)
    if baseline['lasers'] != 1 or baseline['tractor_beams'] != 1:
        raise ValueError('native start contract requires one original laser and beam')
    finite(baseline['capture_movement_penalty_percent'], 'capture penalty', 0, 100)
    if cat['loop']['cards_per_level'] != 3 or cat['loop']['hardware_ranks'] != 3:
        raise ValueError('native upgrade contract requires three choices and three ranks')
    if len(cat['hardware']) != 16 or len(cat['research']) != 7 or len(cat['evolutions']) != 6:
        raise ValueError('native catalogue cardinality changed')
    if not 1 <= arch['run']['hardware_slots'] <= 16 or arch['run']['research_bundle_every_levels'] <= 0:
        raise ValueError('invalid hardware slots or research bundle divisor')
    if arch['profiles'][arch['selected_profile']]['module_limit'] != arch['run']['hardware_slots']:
        raise ValueError('selected profile disagrees with module limit')
    for rarity in RARITIES:
        finite(cat['loop']['rarity_weights'][rarity], 'rarity weight', 0.001)
    for weight in cat['loop']['family_weights'].values():
        finite(weight, 'family weight', 0.001)
    for research in cat['research']:
        for value in research['values'].values():
            finite(value, 'research gain', 0.001, 100)
        if research['cap'] is not None:
            finite(research['cap'], 'research cap', 0.001)
    for value in arch['director'].values():
        if isinstance(value, (int, float)):
            finite(value, 'director')
    for key in ('soldier_rate', 'jeep_rate', 'tank_rate', 'boss_interval_seconds'):
        finite(arch['director'][key], key, 0.0001)
    for target, requirements in arch['eligibility_prerequisites'].items():
        if target not in MODULE_IDS or any(module not in MODULE_IDS or not 0 <= rank <= 3 for module, rank in requirements.items()):
            raise ValueError('invalid native prerequisite')
    costs = arch['permanent']['costs_per_rank']
    if len(costs) != 5 or any(isinstance(c, bool) or not isinstance(c, int) or not 0 < c <= 1000000 for c in costs):
        raise ValueError('invalid permanent cost table')
    selected = arch['profiles'][arch['selected_profile']]
    for name, track in arch['permanent']['tracks'].items():
        if track['maximum_ranks'] != len(costs) or selected[name + '_per_rank'] != track['per_rank']:
            raise ValueError('selected profile disagrees with permanent tracks')
        finite(track['per_rank'], 'permanent track', 0, 100)
    for key in ('seconds_per_credit', 'score_per_credit'):
        finite(arch['credits'][key], key, 1)
    finite(arch['monetisation']['reward_bonus_denominator'], 'bonus denominator', 1)
    if len(arch['starting_blueprints']) != 10 or len(set(arch['starting_blueprints'])) != 10:
        raise ValueError('starter blueprints must be ten distinct modules')
    if len(arch['unlocks']) != 6 or set(arch['starting_blueprints']) | {r['id'] for r in arch['unlocks']} != set(MODULE_IDS):
        raise ValueError('blueprint unlock catalogue mismatch')
    for row in encounter['enemies'].values():
        for key in ('hp', 'damage', 'attack_period', 'active_cap', 'lifetime'):
            finite(row[key], key, 0.001)
    finite(spatial['fixed_hz'], 'fixed tick frequency', 30, 240)
    finite(spatial['maximum_catchup_seconds'], 'catchup budget', 1 / spatial['fixed_hz'], 0.5)
    if spatial['initial_biological_count'] > spatial['biological_active_cap'] or spatial['warning_active_cap'] > spatial['projectile_active_cap']:
        raise ValueError('spatial population caps inconsistent')
    if sum(row['active_cap'] for row in encounter['enemies'].values()) + spatial['biological_active_cap'] + spatial['boss_active_cap'] > 128:
        raise ValueError('combined actor budget exceeds bounded native capacity')
    spacing = spatial['military_spacing']
    if len(spacing['body_radius']) != 4:
        raise ValueError('military spacing requires soldier, jeep, tank and boss envelopes')
    for radius in spacing['body_radius']:
        finite(radius, 'military body radius', .1, spatial['field_half_width'] / 2)
    finite(spacing['lookahead_seconds'], 'military lookahead', .1, 1)
    finite(spacing['margin_metres'], 'military spacing margin', .01, .3)
    if spacing['spawn_rows'] != 8:
        raise ValueError('bounded military ingress requires eight candidate rows')
    finite(spatial['attack_prediction_fraction'], 'attack prediction', 0, 1)
    scroll = spatial['scroll_progression']
    finite(scroll['maximum_metres_per_second'], 'maximum forward speed', spatial['forward_scroll_metres_per_second'], 8)
    for key in ('per_capture_metres_per_second', 'per_level_metres_per_second'):
        finite(scroll[key], 'scroll progression ' + key, .0001, .5)
    if any(speed >= spatial['forward_scroll_metres_per_second'] for speed in spatial['biological_movement_speed']):
        raise ValueError('civilian fleeing must remain slower than starting world travel')
    gates = spatial['native_director']
    if set(gates) != {'jeep_start_second', 'tank_start_second', 'boss_first_second'}:
        raise ValueError('native encounter gate keys changed')
    if not 0 < gates['jeep_start_second'] < gates['tank_start_second'] < gates['boss_first_second']:
        raise ValueError('native threats must enter in an ordered ramp')
    for key, value in gates.items():
        finite(value, 'native encounter gate ' + key, .1, 600)
    for key, value in spatial['enemy_tactics'].items():
        finite(value, 'enemy tactics ' + key, .01, 16)
    finite(spatial['encounter_rhythm']['military_pressure_multiplier'], 'military pressure multiplier', .1, 4)
    rhythm = spatial['encounter_rhythm']
    for key in ('period_seconds', 'build_seconds', 'assault_seconds',
                'build_soldier_multiplier', 'assault_soldier_multiplier', 'assault_heavy_multiplier',
                'boss_ingress_quiet_seconds', 'open_route_half_width', 'boss_entry_half_width'):
        finite(rhythm[key], 'encounter rhythm ' + key, .001, 120)
    if rhythm['build_seconds'] + rhythm['assault_seconds'] >= rhythm['period_seconds']:
        raise ValueError('encounter rhythm must retain a recovery window')
    finite(rhythm['recovery_soldier_multiplier'], 'recovery soldier rate', .001, 4)
    soldier_total = (rhythm['build_seconds'] * rhythm['build_soldier_multiplier'] +
                     rhythm['assault_seconds'] * rhythm['assault_soldier_multiplier'] +
                     (rhythm['period_seconds'] - rhythm['build_seconds'] - rhythm['assault_seconds']) *
                     rhythm['recovery_soldier_multiplier'])
    if abs(soldier_total - rhythm['period_seconds']) > .0001 or abs(
            rhythm['assault_seconds'] * rhythm['assault_heavy_multiplier'] - rhythm['period_seconds']) > .0001:
        raise ValueError('encounter rhythm must distribute the existing per-cycle spawn budget')
    for key in ('build_warning_cap', 'assault_warning_cap', 'boss_warning_cap', 'recovery_warning_cap'):
        value = rhythm[key]
        if isinstance(value, bool) or not isinstance(value, int) or not 1 <= value <= spatial['warning_active_cap']:
            raise ValueError('encounter warning cap exceeds bounded military budget')
    if any(radius + spatial['player_radius'] + rhythm['open_route_half_width'] >= spatial['field_half_width']
           for radius in spatial['military_contact_radius'][:3]):
        raise ValueError('encounter flank has no legal entry interval')
    if rhythm['boss_entry_half_width'] >= spatial['field_half_width']:
        raise ValueError('boss entry must remain within the field')
    if 2 * rhythm['boss_ingress_quiet_seconds'] >= arch['director']['boss_interval_seconds']:
        raise ValueError('boss ingress gap cannot cover the entire boss interval')
    if isinstance(rhythm['cohort_size'], bool) or not isinstance(rhythm['cohort_size'], int) or rhythm['cohort_size'] != 4:
        raise ValueError('social pocket geometry requires four specimens')
    if spatial['initial_biological_count'] % rhythm['cohort_size']:
        raise ValueError('opening population must contain whole social pockets')
    for key in ('herd_lane_metres', 'herd_lateral_step', 'herd_forward_step', 'opening_near',
                'opening_group_step', 'opening_lane_metres', 'herd_lane_jitter_metres',
                'cattle_lateral_step', 'cattle_lane_offset_metres', 'intro_escort_second', 'warning_entry_ahead'):
        finite(rhythm[key], 'encounter delivery ' + key, .001, 32)
    if rhythm['warning_entry_ahead'] >= spatial['enemy_spawn_ahead']:
        raise ValueError('military must enter visibly before warning commitment')
    finite(rhythm['warning_retire_behind'], 'visible warning retirement', .1, spatial['enemy_retire_behind'])
    harvest = spatial['harvest']
    for key in ('core_radius_fraction', 'minimum_pilot_speed', 'sweep_fraction', 'sweep_multiplier',
                'chain_window_seconds', 'overdrive_seconds', 'overdrive_multiplier'):
        finite(harvest[key], 'harvest ' + key, .001, 32)
    if not 0 < harvest['core_radius_fraction'] < 1 or not 0 < harvest['sweep_fraction'] <= 1 or not 1 <= harvest['sweep_multiplier'] <= 8 or not 1 <= harvest['overdrive_multiplier'] <= 2:
        raise ValueError('harvest gain must retain bounded normal capture')
    if isinstance(harvest['chain_target'], bool) or not isinstance(harvest['chain_target'], int) or not 2 <= harvest['chain_target'] <= 8:
        raise ValueError('harvest chain count must be bounded')
    starter = spatial['starter_dash']
    finite(starter['distance'], 'starter dash distance', .1, 3)
    finite(starter['invulnerability'], 'starter dash invulnerability', .01, .2)
    finite(starter['cooldown'], 'starter dash cooldown', 1, 30)
    if starter['invulnerability'] >= starter['cooldown']:
        raise ValueError('starter dash cannot chain invulnerability')

def generate(root, output):
    balance = root / 'design/balance'
    arch = json.loads((balance / 'archero-ufo-system.json').read_text())
    cat = json.loads((balance / 'endless-upgrade-system.json').read_text())
    encounter = json.loads((balance / 'endless-encounter-assumptions.json').read_text())
    spatial = json.loads((root / 'native/human_resources_gameplay/runtime-spatial.json').read_text())
    validate(arch, cat, encounter, spatial)
    modules = {row['id']: row for row in cat['hardware']}
    if set(modules) != set(MODULE_IDS):
        raise ValueError('native module enum/catalogue mismatch')
    if arch['selected_profile'] != 'hangar_25':
        raise ValueError('native profile requires review after selected profile changes')
    selected = arch['profiles'][arch['selected_profile']]
    if selected['xp_base'] != arch['run']['next_level_base_xp'] or selected['xp_slope'] != arch['run']['next_level_per_current_level_xp']:
        raise ValueError('selected XP profile disagrees with run contract')
    lines = ['// Generated from validated Human Resources design authorities. Do not edit.',
             '#pragma once', '#include <array>', '#include <cstdint>', '#include <string_view>',
             'namespace human_resources::gameplay::tuning {']

    def numeric(value):
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
            raise ValueError(f'invalid finite tuning number {value!r}')
        return f'{float(value):.9g}' + ('F' if '.' in f'{float(value):.9g}' or 'e' in f'{float(value):.9g}' else '.0F')

    def scalar(name, value):
        lines.append(f'inline constexpr float {name} = {numeric(value)};')

    def integer(name, value):
        if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= 4294967295:
            raise ValueError(f'invalid integer {name}')
        lines.append(f'inline constexpr std::uint32_t {name} = {value}U;')

    def array(name, values, kind='float'):
        formatted = [numeric(v) for v in values] if kind == 'float' else [f'{v}U' for v in values]
        lines.append(f'inline constexpr std::array<{kind}, {len(values)}> {name}{{{{{", ".join(formatted)}}}}};')

    vehicle = json.loads((root / 'assets/production/vehicle-weapons.json').read_text())
    for path, expected in vehicle['source_sha256'].items():
        if hashlib.sha256((root / path).read_bytes()).hexdigest() != expected:
            raise ValueError('Vehicle geometry authority changed: ' + path)
    minigun = spatial['minigun']
    for recipe in ('characters.json', 'infantry.json'):
        authored = json.loads((root / 'assets/animation' / recipe).read_text())
        if spatial['capture_return_seconds'] != authored['clips']['beam_release']['seconds']:
            raise ValueError('Shared capture return must match accepted source release duration')
    if any(abs(vehicle['pitch_offset_from_yaw_m'][axis]) > 1e-7 for axis in (0,2)):
        raise ValueError('Analytic minigun solve requires reviewed vertical trunnion offset')
    if not 0 < minigun['minimum_muzzle_range'] <= 20 or not 0 < minigun['maximum_elevation_degrees'] <= 14:
        raise ValueError('Minigun operating envelope exceeds reviewed source')
    if minigun['minimum_elevation_degrees'] != 0 or not 0 < spatial['capture_return_seconds'] <= 2:
        raise ValueError('Invalid capture/minigun contract')
    spacing = spatial['military_spacing']
    array('military_body_radius', spacing['body_radius'])
    scalar('military_spacing_lookahead', spacing['lookahead_seconds'])
    scalar('military_spacing_margin', spacing['margin_metres'])
    integer('military_spawn_rows', spacing['spawn_rows'])
    scalar('capture_return_seconds', spatial['capture_return_seconds'])
    scalar('minigun_minimum_range', minigun['minimum_muzzle_range'])
    scalar('minigun_maximum_elevation', math.radians(minigun['maximum_elevation_degrees']))
    scalar('minigun_spawn_forward_offset', minigun['spawn_forward_offset'])
    scalar('minigun_visible_tracer_radius', minigun['visible_tracer_radius'])
    scalar('minigun_visible_flash_radius', minigun['visible_flash_radius'])
    array('minigun_anchor', vehicle['anchor_actor_m'])
    array('minigun_yaw_pivot', vehicle['yaw_pivot_mount_m'])
    array('minigun_pitch_offset', vehicle['pitch_offset_from_yaw_m'])
    assemblies = json.loads((root / 'assets/production/assemblies.json').read_text())['assemblies']
    tank = next(row for row in assemblies if row['id'] == 'tank-with-turret')
    array('cannon_socket', tank['sockets'][0]['translation_m'])
    array('cannon_muzzle', vehicle['cannon']['muzzle'])
    scalar('cannon_launch_forward_offset', vehicle['cannon']['launch_forward_offset'])
    scalar('cannon_visible_missile_scale', vehicle['cannon']['visible_missile_scale'])
    if vehicle['cannon']['visible_missile_bounding_sphere'] + .005 >= vehicle['cannon']['launch_to_turret_surface_minimum']:
        raise ValueError('Finite cannon missile no longer fits qualified emitter clearance')
    muzzles = [row['raw_head_position_m'] for row in vehicle['muzzles']]
    if len(muzzles) != 6 or any(len(row) != 3 for row in muzzles):
        raise ValueError('Minigun must retain six actual openings')
    formatted = ','.join('{' + ','.join(numeric(v) for v in row) + '}' for row in muzzles)
    lines.append('inline constexpr std::array<std::array<float,3>,6> minigun_muzzles{{' + formatted + '}};')

    def source(module):
        row = modules[module]
        return ' '.join(row['ranks']) + ' ' + row.get('rules', '')

    def extract(module, pattern):
        match = re.search(pattern, source(module))
        if not match:
            raise ValueError(f'catalogue phrase changed: {module}: {pattern}')
        return float(match.group(1))

    def number(name, module, pattern, divisor=1):
        scalar(name, extract(module, pattern) / divisor)

    for key, value in cat['loop']['baseline'].items():
        scalar('base_' + key, value)
    for key in ('fixed_hz', 'maximum_catchup_seconds', 'field_half_width', 'forward_scroll_metres_per_second',
                'player_radius', 'laser_range', 'laser_flash_seconds', 'pierce_alignment_metres',
                'enemy_spawn_ahead', 'enemy_retire_behind', 'biological_spawn_per_second',
                'hit_invulnerability_seconds', 'projectile_lifetime_seconds',
                'soldier_capture_seconds_multiplier', 'vehicle_score', 'steering_acceleration', 'biological_separation_speed',
                'emergency_disable_radius', 'attack_prediction_fraction', 'player_forward_allowance', 'player_backward_allowance'):
        if spatial[key] <= 0:
            raise ValueError(f'nonpositive spatial rule {key}')
        scalar(key, spatial[key])
    for key in ('biological_active_cap', 'projectile_active_cap', 'warning_active_cap', 'boss_active_cap', 'initial_biological_count', 'biological_spawn_candidates'):
        if not 0 < spatial[key] <= 512:
            raise ValueError(f'unbounded spatial rule {key}')
        integer(key, spatial[key])
    for key, value in spatial['encounter_rhythm'].items():
        if key.endswith('_cap') or key == 'cohort_size':
            integer('encounter_' + key, value)
        else:
            scalar('encounter_' + key, value)
    for key, value in spatial['harvest'].items():
        if key == 'chain_target':
            integer('harvest_' + key, value)
        else:
            scalar('harvest_' + key, value)
    for key, value in spatial['starter_dash'].items():
        scalar('starter_dash_' + key, value)
    for key, value in spatial['scroll_progression'].items():
        scalar('scroll_' + key, value)
    for key, value in spatial['enemy_tactics'].items():
        scalar('tactics_' + key, value)
    radii = spatial['biological_body_radius']
    if len(radii) != 5 or any(not 0 < radius < 1 for radius in radii):
        raise ValueError('invalid biological body radii')
    array('biological_body_radius', radii)
    for key in ('biological_score', 'biological_movement_speed', 'military_movement_speed',
                'projectile_speed', 'projectile_radius', 'attack_warning_seconds', 'military_contact_radius'):
        if len(spatial[key]) != 4 or any(v <= 0 for v in spatial[key]):
            raise ValueError(f'invalid spatial array {key}')
        array(key, spatial[key])
    infantry = spatial['infantry']
    if infantry['roles'] != ['rifleman', 'shotgunner', 'rocketeer']:
        raise ValueError('infantry role order changed')
    for key in ('pellets', 'damage_fraction', 'spread_radians', 'projectile_speed',
                'warning_seconds', 'cooldown_seconds', 'recovery_seconds', 'weapon_yaw_radians'):
        values = infantry[key]
        if len(values) != 3 or any(isinstance(v, bool) or not isinstance(v, (int, float)) or
                                   not math.isfinite(v) or not 0 <= v <= 16 for v in values):
            raise ValueError('invalid infantry profile: ' + key)
        if key == 'pellets' and any(not isinstance(v, int) or not 1 <= v <= 3 for v in values):
            raise ValueError('infantry fan exceeds bounded profile')
        array('infantry_' + key, values, 'unsigned' if key == 'pellets' else 'float')
    for i in range(3):
        if (infantry['projectile_speed'][i] <= 0 or infantry['warning_seconds'][i] <= 0 or
                infantry['recovery_seconds'][i] <= 0 or
                infantry['recovery_seconds'][i] > infantry['cooldown_seconds'][i] or
                abs(infantry['pellets'][i] * infantry['damage_fraction'][i] - 1) > 1e-6 or
                abs(infantry['warning_seconds'][i] + infantry['cooldown_seconds'][i] - 3.9) > 1e-6):
            raise ValueError('infantry damage/cycle budget changed')
    muzzle = infantry['neutral_muzzle']
    if len(muzzle) != 3 or any(len(v) != 3 or any(not math.isfinite(n) or abs(n) > 3 for n in v) for v in muzzle):
        raise ValueError('invalid source-derived infantry muzzles')
    lines.append('inline constexpr std::array<std::array<float,3>,3> infantry_neutral_muzzle{{' +
                 ','.join('{' + ','.join(numeric(n) for n in v) + '}' for v in muzzle) + '}};')
    scalar('projectile_target_height', spatial['projectile_target_height'])
    run = arch['run']
    for name, key in (('hardware_slots', 'hardware_slots'), ('rerolls', 'rerolls'),
                      ('research_bundle_levels', 'research_bundle_every_levels')):
        integer(name, run[key])
    scalar('xp_base', run['next_level_base_xp'])
    scalar('xp_slope', run['next_level_per_current_level_xp'])
    scalar('sector_seconds', arch['sectors']['seconds'])
    array('rarity_weights', [cat['loop']['rarity_weights'][r] for r in RARITIES])
    scalar('hardware_weight', cat['loop']['family_weights']['hardware'])
    scalar('research_weight', cat['loop']['family_weights']['research'])
    array('module_rarity', [RARITIES.index(modules[m]['rarity']) for m in MODULE_IDS], 'std::uint8_t')
    lines.append('inline constexpr std::array<std::string_view, 16> module_ids{{' + ','.join(json.dumps(m) for m in MODULE_IDS) + '}};')
    lines.append('inline constexpr std::array<std::string_view, 16> module_names{{' + ','.join(json.dumps(modules[m]['name']) for m in MODULE_IDS) + '}};')
    prerequisites = arch['eligibility_prerequisites']
    lines.append('inline constexpr std::array<std::array<std::uint8_t, 16>, 16> prerequisites{{')
    for module in MODULE_IDS:
        lines.append('  {{' + ','.join(str(prerequisites.get(module, {}).get(m, 0)) for m in MODULE_IDS) + '}},')
    lines.append('}};')
    research = cat['research']
    if [r['id'] for r in research] != ['weapon_damage', 'hull_reinforcement', 'shield_capacity', 'specimen_value', 'capture_speed', 'fire_rate', 'emergency_repairs']:
        raise ValueError('research enum mismatch')
    lines.append('inline constexpr std::array<std::array<float, 3>, 7> research_values{{')
    lines.extend('  {{' + ','.join(numeric(r['values'][rarity]) for rarity in RARITIES) + '}},' for r in research)
    lines.append('}};')
    lines.append('inline constexpr std::array<std::string_view, 7> research_names{{' + ','.join(json.dumps(r['name']) for r in research) + '}};')
    array('research_caps', [r['cap'] if r['cap'] is not None else 0 for r in research])
    limits = cat['stacking_and_limits']
    for key in ('capture_minimum_seconds', 'movement_maximum_base_multiplier', 'firing_rate_maximum_base_multiplier', 'capture_shield_restoration_per_second_cap'):
        scalar(key, limits[key])
    for key, value in cat['xp_starting_proposal'].items():
        if isinstance(value, (int, float)):
            scalar(key, value)
    for key, value in arch['director'].items():
        if isinstance(value, (int, float)):
            scalar('director_' + key, spatial['native_director'].get(key, value))
    enemy_rows = [encounter['enemies'][name] for name in ('soldier', 'jeep', 'tank')] + [encounter['boss']]
    for field in ('hp', 'damage', 'attack_period', 'start_second', 'active_cap', 'lifetime', 'score'):
        vals = [row.get(field, arch['director']['boss_base_hp'] if field == 'hp' else 0) for row in enemy_rows]
        if field == 'start_second':
            # The current endless candidate overrides the historical encounter gates.
            vals[1:3] = [spatial['native_director']['jeep_start_second'], spatial['native_director']['tank_start_second']]
        if field == 'damage':
            vals[-1] = arch['director']['boss_damage']
        if field == 'active_cap':
            vals[-1] = spatial['boss_active_cap']
        array('enemy_' + field, vals)
    tracks = [arch['permanent']['tracks'][name] for name in ('damage', 'hull', 'shield', 'capture')]
    array('permanent_per_rank', [track['per_rank'] for track in tracks])
    array('permanent_maximum_ranks', [track['maximum_ranks'] for track in tracks], 'std::uint8_t')
    array('permanent_costs', arch['permanent']['costs_per_rank'], 'std::uint64_t')
    credits = arch['credits']
    for name, key in (('credit_seconds_per', 'seconds_per_credit'), ('credit_score_per', 'score_per_credit'),
                      ('credit_per_vehicle', 'per_vehicle_takedown'), ('credit_per_boss', 'per_boss_kill'),
                      ('credit_minimum_seconds', 'minimum_active_seconds')):
        integer(name, credits[key])
    integer('bonus_numerator', arch['monetisation']['reward_bonus_numerator'])
    integer('bonus_denominator', arch['monetisation']['reward_bonus_denominator'])
    for key, value in arch['revive'].items():
        if isinstance(value, (int, float)):
            scalar('revive_' + key, value)
    integer('revive_minimum_seconds', arch['revive']['minimum_run_seconds'])
    integer('revive_maximum', arch['revive']['maximum_per_run'])
    integer('starter_blueprints', sum(1 << MODULE_IDS.index(m) for m in arch['starting_blueprints']))
    lines.append('struct UnlockRule { std::uint32_t blueprint; std::string_view counter; std::uint64_t threshold; };')
    lines.append('inline constexpr std::array<UnlockRule, 6> unlock_rules{{')
    for rule in arch['unlocks']:
        lines.append(f'  {{{1 << MODULE_IDS.index(rule["id"])}U, {json.dumps(rule["counter"])}, {rule["threshold"]}U}},')
    lines.append('}};')
    # Module values come from the catalogue itself, rather than a second numeric authority.
    specs = [
        ('added_laser_fraction', 'laser_array', r'added laser deals (\d+)%', 100),
        ('focus_bonus', 'laser_array', r'up to \+(\d+)% damage', 100),
        ('focus_expiry', 'laser_array', r'expires after ([\d.]+)s', 1),
        ('focus_hits', 'laser_array', r'(five) repeated', None),
        ('pierce_fraction', 'rail_lens', r'receiving (\d+)%', 100),
        ('arc_chance_base', 'arc_coil', r'(\d+)% chance', 100),
        ('arc_chance_step', 'arc_coil', r'Add (\d+) percentage points', 100),
        ('arc_fraction', 'arc_coil', r'for (\d+)% damage', 100),
        ('arc_range', 'arc_coil', r'([\d.]+)m jump', 1),
        ('drone_damage', 'orbit_drones', r'dealing (\d+) damage', 1),
        ('drone_rate', 'orbit_drones', r'(twice) per second', None),
        ('pulse_damage_base', 'emp_ring', r'dealing (\d+) damage', 1),
        ('pulse_damage_step', 'emp_ring', r'Add (\d+) pulse damage', 1),
        ('pulse_radius_base', 'emp_ring', r'within (\d+)m', 1),
        ('pulse_radius_step', 'emp_ring', r'add ([\d.]+)m radius', 1),
        ('pulse_cooldown_base', 'emp_ring', r'every (\d+)s', 1),
        ('pulse_disable_seconds', 'emp_ring', r'a ([\d.]+)s nearby', 1),
        ('pulse_disable_cooldown', 'emp_ring', r'shared (\d+)s cooldown', 1),
        ('splitter_burst', 'beam_splitter', r'beams \+(\d+)% capture speed', 100),
        ('splitter_burst_seconds', 'beam_splitter', r'capture speed for (\d+)s', 1),
        ('lens_radius_step', 'tractor_lens', r'Add (\d+)% of base', 100),
        ('lens_grace_step', 'tractor_lens', r'unlock ([\d.]+)s', 1),
        ('lens_pull_extension', 'tractor_lens', r'from (\d+)% beyond', 100),
        ('lens_pull_speed', 'tractor_lens', r'up to ([\d.]+)m/s', 1),
        ('lens_pull_movement_cap', 'tractor_lens', r'exceed (\d+)% of a target', 100),
        ('vehicle_capture_seconds', 'heavy_winch', r'base (\d+)s capture', 1),
        ('tank_capture_minimum', 'heavy_winch', r'tank minimum (\d+)s', 1),
        ('vehicle_capture_cooldown', 'heavy_winch', r'per (\d+)s', 1),
        ('vehicle_capture_speed_step', 'heavy_winch', r'add \+(\d+)% vehicle', 100),
        ('vehicle_bio_burst', 'heavy_winch', r'unlock \+(\d+)% biological', 100),
        ('vehicle_bio_burst_seconds', 'heavy_winch', r'biological capture speed for (\d+)s', 1),
        ('overtime_base', 'overtime_reactor', r'Unlock \+(\d+)% firing', 100),
        ('overtime_step', 'overtime_reactor', r'Add (\d+) percentage points', 100),
        ('reserve_base', 'reserve_battery', r'Unlock \+(\d+)% weapon', 100),
        ('reserve_step', 'reserve_battery', r'Add (\d+) percentage points', 100),
        ('shield_capacity_step', 'shield_generator', r'Add (\d+) shield capacity', 1),
        ('shield_recharge_step', 'shield_generator', r'Increase recharge by (\d+)/s', 1),
        ('repair_base', 'repair_swarm', r'repair of (\d+) hull', 1),
        ('repair_capture_interval', 'repair_swarm', r'every (\d+) biological', 1),
        ('reflect_step', 'mirror_plating', r'a (\d+)% chance', 100),
        ('movement_step', 'vector_thrusters', r'Add (\d+)% of base', 100),
        ('capture_penalty_step', 'vector_thrusters', r'penalty by (\d+) percentage', 100),
        ('movement_burst', 'vector_thrusters', r'unlock \+(\d+)% movement', 100),
        ('movement_burst_seconds', 'vector_thrusters', r'movement for (\d+)s', 1),
        ('dash_distance', 'phase_drive', r'a player-activated (\d+)m dash', 1),
        ('dash_invulnerability', 'phase_drive', r'with ([\d.]+)s invulnerability', 1),
        ('dash_invulnerability_step', 'phase_drive', r'Add ([\d.]+)s invulnerability', 1),
        ('dash_cooldown', 'phase_drive', r'a (\d+)s cooldown', 1),
        ('dash_cooldown_step', 'phase_drive', r'cooldown by ([\d.]+)s', 1),
        ('bounty_step', 'bounty_scanner', r'Add \+(\d+)% specimen', 100),
        ('bounty_streak_base', 'bounty_scanner', r'bonus of \+(\d+)%', 100),
        ('bounty_streak_interval', 'bounty_scanner', r'per (\d+) uninterrupted', 1),
        ('bounty_streak_cap', 'bounty_scanner', r'another \+(\d+)%', 100),
    ]
    for name, module, pattern, divisor in specs:
        if divisor is None:
            scalar(name, {'five': 5, 'twice': 2}[re.search(pattern, source(module)).group(1)])
        else:
            number(name, module, pattern, divisor)
    array('jeep_capture_threshold', [extract('heavy_winch', r'at or below (\d+)%'), extract('heavy_winch', r'40% -> (\d+)%'), extract('heavy_winch', r'60% -> (\d+)%')])
    number('tank_capture_threshold', 'heavy_winch', r'tank capture at or below (\d+)%', 100)
    array('shield_delays', [extract('shield_generator', r'6 -> ([\d.]+)s'), extract('shield_generator', r'5.5 -> ([\d.]+)s'), extract('shield_generator', r'5 -> ([\d.]+)s')])
    recipes = cat['evolutions']
    lines.append('inline constexpr std::array<std::array<std::uint8_t, 2>, 6> evolution_ingredients{{' + ','.join('{{'+','.join(str(MODULE_IDS.index(m)) for m in r['ingredients'])+'}}' for r in recipes) + '}};')
    lines.append('inline constexpr std::array<std::string_view, 6> evolution_names{{' + ','.join(json.dumps(r['name']) for r in recipes) + '}};')
    def evolution_number(name, index, pattern, divisor=1):
        match = re.search(pattern, recipes[index]['effect'])
        if not match:
            raise ValueError(f'evolution phrase changed: {name}')
        scalar(name, float(match.group(1)) / divisor)
    evolution_number('evolution_laser_bonus', 1, r'\+(\d+)%', 100)
    evolution_number('evolution_tank_threshold', 2, r'below (\d+)%', 100)
    evolution_number('mirrorball_period', 3, r'Every (\d+)s')
    evolution_number('mirrorball_damage_cap', 3, r'capped at (\d+) base')
    if 'up to four enemies' not in recipes[3]['effect']:
        raise ValueError('mirrorball target count changed')
    integer('mirrorball_targets', 4)
    evolution_number('benefits_damage_bonus', 4, r'\+(\d+)%', 100)
    evolution_number('benefits_duration', 4, r'for (\d+)s')
    evolution_number('benefits_intercept_period', 4, r'per (\d+)s')
    evolution_number('benefits_shield_cost', 4, r'consuming (\d+) shield')
    evolution_number('storm_radius', 5, r'becomes (\d+)m')
    evolution_number('storm_slow', 5, r'by (\d+)%', 100)
    evolution_number('storm_boss_slow', 5, r'at (\d+)%', 100)
    evolution_number('storm_slow_seconds', 5, r'for (\d+)s')
    # These discrete counts are spelled out in the authority; assert its wording.
    if 'six-target' not in recipes[0]['effect'] or 'At most two evolutions' not in cat['evolution_rule']:
        raise ValueError('evolution cardinality changed')
    integer('evolution_beams', 6)
    integer('maximum_evolutions', 2)
    # Persisted authority follows the emitted semantic numbers, not catalogue
    # prose, display names, JSON formatting or Python source formatting.
    numeric_contract = '\n'.join(line for line in lines
                                 if 'string_view' not in line and '"' not in line)
    fingerprint = int.from_bytes(hashlib.sha256(numeric_contract.encode('utf-8')).digest()[:8], 'little')
    lines.append(f'inline constexpr std::uint64_t save_rules_fingerprint = {fingerprint}ULL;')
    lines.append('} // namespace human_resources::gameplay::tuning')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    generate(args.root, args.output)
