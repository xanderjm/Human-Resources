"""Measure neutral fixture fit of separate fictional gun and pedestal assets."""
import hashlib
import json

import numpy as np

from normalize_glb import ROOT, accessor, matrix, unpack
from repair_jeep_minigun_mount import vertex_components


def load(asset):
    path = ROOT / 'assets/production/records' / asset / 'model-audit.json'
    audit = json.loads(path.read_text())
    model = ROOT / audit['model_file']
    doc, chunks = unpack(model)
    blob = next(blob for typ, blob in chunks if typ == 0x004e4942)
    primitive = doc['meshes'][0]['primitives'][0]
    raw = accessor(doc, blob, primitive['attributes']['POSITION'])
    indices = accessor(doc, blob, primitive['indices']).reshape(-1, 3)
    transform = matrix(doc['nodes'][-1])
    world = (transform @ np.column_stack([raw, np.ones(len(raw))]).T).T[:, :3]
    return audit, model, raw, indices, transform, world


def main():
    head, head_file, hp, hi, ht, hv = load('jeep-minigun-head')
    mount, mount_file, mp, mi, mt, mv = load('jeep-minigun-mount')
    bearing_raw = np.array([-.0048828125, .453125, .0029296875, 1.])
    socket = (mt @ bearing_raw)[:3]
    head_pivot = np.array(head['import_review']['trunnion_origin_raw'])
    groups = vertex_components(hp, hi)
    bosses = next(g for g in groups if hp[g, 0].min() > -.06 and hp[g, 0].max() < .05
                  and hp[g, 2].min() < -.20 and hp[g, 2].max() > .20)
    boss_points = hp[bosses][np.abs(hp[bosses, 2]) > .18]
    boss_radius = np.linalg.norm(boss_points[:, :2] - head_pivot[:2], axis=1).max() * head['import_review']['uniform_scale']
    upper = mp[:, 1] > .42
    positive_inner = mp[upper & (mp[:, 2] > 0), 2].min()
    negative_inner = mp[upper & (mp[:, 2] < 0), 2].max()
    slot = (positive_inner - negative_inner) * mount['import_review']['uniform_scale']
    core = mv[(np.abs(mv[:, 0] - socket[0]) < slot / 2 - .015) & (mv[:, 1] < socket[1] - .04)]
    floor_y = core[:, 1].max()
    placed_head = hv + socket
    z_bounds = [core[:, 2].min(), core[:, 2].max()]
    within = placed_head[(placed_head[:, 2] >= z_bounds[0]) & (placed_head[:, 2] <= z_bounds[1])]
    head_bottom = within[:, 1].min()
    original_doc, original_chunks = unpack(ROOT / mount['generation_raw_model_file'])
    original_blob = next(blob for typ, blob in original_chunks if typ == 0x004e4942)
    original_primitive = original_doc['meshes'][0]['primitives'][0]
    original_positions = accessor(original_doc, original_blob, original_primitive['attributes']['POSITION'])
    original_radius = np.linalg.norm(original_positions[:, :2] - bearing_raw[:2], axis=1)
    hole_mask = (original_positions[:, 1] > .42) & (np.abs(original_positions[:, 2]) > .05) & (original_radius < .030)
    apothems = []
    for plane in np.unique(original_positions[hole_mask, 2]):
        ring = mp[hole_mask & (original_positions[:, 2] == plane), :2] - bearing_raw[:2]
        ring = np.unique(ring, axis=0)
        if len(ring) < 3:
            continue
        ring = ring[np.argsort(np.arctan2(ring[:, 1], ring[:, 0]))]
        next_ring = np.roll(ring, -1, axis=0)
        cross = ring[:, 0] * next_ring[:, 1] - ring[:, 1] * next_ring[:, 0]
        apothems.extend(np.abs(cross) / np.linalg.norm(next_ring - ring, axis=1))
    bore_min = min(apothems) * mount['import_review']['uniform_scale']
    report = {'status': 'pending_visual_review', 'scope': 'Static neutral component fixture only; no jeep, operator, rig or animation.',
              'head_model': head['model_file'], 'mount_model': mount['model_file'],
              'head_sha256': hashlib.sha256(head_file.read_bytes()).hexdigest(),
              'mount_sha256': hashlib.sha256(mount_file.read_bytes()).hexdigest(),
              'head_translation_m': socket.tolist(), 'head_rotation_xyzw': [0, 0, 0, 1],
              'head_trunnion_axis': '+X', 'head_trunnion_origin_m': [0, 0, 0],
              'mount_bearing_centre_m': socket.tolist(), 'mount_bearing_axis': '+X',
              'measured_slot_clear_width_m': float(slot), 'head_trunnion_radius_max_m': float(boss_radius),
              'measured_bore_polygon_apothem_min_m': float(bore_min), 'radial_clearance_min_m': float(bore_min - boss_radius),
              'bridge_neck_y_max_under_head_m': float(floor_y), 'head_y_min_above_bridge_m': float(head_bottom),
              'vertical_clearance_m': float(head_bottom - floor_y),
              'head_axle_ends_local_m': [hv[bosses].min(axis=0).tolist(), hv[bosses].max(axis=0).tolist()],
              'mount_base_bounds_m': [mv[mp[:, 1] < -.44].min(axis=0).tolist(), mv[mp[:, 1] < -.44].max(axis=0).tolist()],
              'pitch_sweep': 'Not qualified; future owner rig/pose work.',
              'native_or_physics_qualification': 'None; vertex bounds and actual rendered static fixture only.'}
    if report['vertical_clearance_m'] <= 0 or report['radial_clearance_min_m'] <= 0:
        raise ValueError(f'Neutral component clearance failed: {report}')
    output = ROOT / 'assets/production/records/jeep-minigun-head/component-fit.json'
    if output.exists():
        previous = json.loads(output.read_text())
        if all(previous.get(key) == report[key] for key in ('head_sha256', 'mount_sha256')):
            report['status'] = previous.get('status', report['status'])
            if 'visual_review' in previous:
                report['visual_review'] = previous['visual_review']
    output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
