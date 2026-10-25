"""Seat the generated floating spindle in its bearing and align it to +Z."""
import numpy as np

from glb_geometry_repair import append_array, array_from, load_asset, save_repair


def main():
    asset = 'windmill-mast'
    record, audit, raw, doc, chunks = load_asset(asset)
    primitive = doc['meshes'][0]['primitives'][0]
    positions = array_from(doc, chunks, primitive['attributes']['POSITION'])
    normals = array_from(doc, chunks, primitive['attributes']['NORMAL'])
    mask = (positions[:, 0] > .065) & (positions[:, 1] > .420) & (positions[:, 2] > .055)
    selected = positions[mask]
    if not 500 < len(selected) < 900:
        raise ValueError('Unexpected spindle topology; inspect the downloaded input again')
    centre = selected.mean(axis=0)
    _, _, basis = np.linalg.svd(selected - centre)
    axis = basis[0]
    if axis[2] < 0:
        axis = -axis
    target = np.array([0., 0., 1.])
    cross = np.cross(axis, target)
    skew = np.array([[0, -cross[2], cross[1]], [cross[2], 0, -cross[0]], [-cross[1], cross[0], 0]])
    rotation = np.eye(3) + skew + skew @ skew / (1 + np.dot(axis, target))
    rear = centre + axis * np.min((selected - centre) @ axis)
    anchor = np.array([-.018, .446, .025])
    positions[mask] = (selected - rear) @ rotation.T + anchor
    normals[mask] = normals[mask] @ rotation.T
    primitive['attributes']['POSITION'] = append_array(doc, chunks, positions, 5126, 'VEC3', 34962)
    primitive['attributes']['NORMAL'] = append_array(doc, chunks, normals, 5126, 'VEC3', 34962)
    save_repair(asset, record, audit, raw, doc, chunks,
                'Moved the detached diagonal spindle island, including its bolt geometry, into the existing bearing and aligned its axis horizontally to +Z.',
                {'moved_vertex_count': int(mask.sum()), 'original_axis': axis.tolist(),
                 'target_axis': target.tolist(), 'raw_bearing_anchor': anchor.tolist(), 'added_triangles': 0})


if __name__ == '__main__':
    main()
