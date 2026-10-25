"""Fit only the upper fork/neck of the fictional game-prop pedestal."""
import numpy as np

from glb_geometry_repair import append_array, array_from, load_asset, save_repair


def vertex_components(positions, triangles):
    unique, inverse = np.unique(np.round(positions, 5), axis=0, return_inverse=True)
    parents = np.arange(len(unique))
    def find(index):
        while parents[index] != index:
            parents[index] = parents[parents[index]]
            index = parents[index]
        return index
    for triangle in inverse[triangles]:
        for index in triangle[1:]:
            parents[find(index)] = find(triangle[0])
    groups = {}
    for index, value in enumerate(inverse):
        groups.setdefault(find(value), []).append(index)
    return [np.array(indices) for indices in groups.values()]


def main():
    asset = 'jeep-minigun-mount'
    record, audit, raw, doc, chunks = load_asset(asset)
    primitive = doc['meshes'][0]['primitives'][0]
    positions = array_from(doc, chunks, primitive['attributes']['POSITION'])
    normals = array_from(doc, chunks, primitive['attributes']['NORMAL'])
    triangles = array_from(doc, chunks, primitive['indices']).reshape(-1, 3)
    before = positions.copy()
    groups = vertex_components(positions, triangles)
    fork_indices = next(g for g in groups if len(g) > 200 and positions[g, 1].min() > .35)
    fork = positions[fork_indices].copy()
    original = fork.copy()
    # Translate cheeks outwards, stretching only their connecting bridge.
    fork[:, 2] += .103 * np.clip(fork[:, 2] / .0576171875, -1, 1)
    # Preserve bearing centres and the 1.05 m maximum height while lowering the
    # U floor. The upper neck is shortened separately, remaining overlapped.
    weight = np.clip((.425 - fork[:, 1]) / .020, 0, 1)
    fork[:, 1] -= .095 * weight
    centre = np.array([-.0048828125, .453125])
    xy = original[:, :2] - centre
    radius = np.linalg.norm(xy, axis=1)
    hole = (original[:, 1] > .42) & (np.abs(original[:, 2]) > .05) & (radius < .030)
    new_radius = .0442 + (radius[hole] - .018) * .04
    fork[hole, :2] = centre + xy[hole] * (new_radius / radius[hole])[:, None]
    outer = (original[:, 1] > .4) & ~hole
    fork[outer, 0] = centre[0] + (fork[outer, 0] - centre[0]) * 1.25
    positions[fork_indices] = fork
    for group in groups:
        low, high = before[group, 1].min(), before[group, 1].max()
        if low > .25 and high < .36:
            positions[group, 1] -= .065
        elif low < -.4 and .30 < high < .34:
            upper = group[before[group, 1] > .24]
            positions[upper, 1] = .24 + (before[upper, 1] - .24) * (.265 - .24) / (high - .24)
    # Recalculate geometry normals after this controlled shape change. Welded
    # index seams remain closed because equal source positions share the warp.
    accum = np.zeros_like(positions)
    tri = positions[triangles]
    face = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    for corner in range(3):
        np.add.at(accum, triangles[:, corner], face)
    lengths = np.linalg.norm(accum, axis=1)
    normals[lengths > 0] = accum[lengths > 0] / lengths[lengths > 0, None]
    assert np.array_equal(before[before[:, 1] < .24], positions[before[:, 1] < .24])
    primitive['attributes']['POSITION'] = append_array(doc, chunks, positions, 5126, 'VEC3', 34962)
    primitive['attributes']['NORMAL'] = append_array(doc, chunks, normals, 5126, 'VEC3', 34962)
    save_repair(asset, record, audit, raw, doc, chunks,
                'Widened only the upper fork cheeks/bridge and enlarged the visual bearing bores; lowered its floor and shortened the upper neck for neutral head clearance. Base, bolts, lower column, maximum height and existing paint atlas retained.',
                {'raw_cheek_translation_each_side': .103, 'raw_fork_floor_lowering': .095,
                 'raw_upper_neck_shortening': .065, 'raw_bearing_xy_centre': centre.tolist(),
                 'added_triangles': 0, 'lower_geometry_below_raw_y': .24,
                 'neutral_fit_only': True, 'pitch_sweep': 'Future owner rigging review; not qualified.'})


if __name__ == '__main__':
    main()
