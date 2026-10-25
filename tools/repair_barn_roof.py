"""Add closed continuous pitched backing under the existing barn roof tiles."""
import io
import numpy as np
from PIL import Image

from glb_geometry_repair import add_hexahedron, append_array, array_from, load_asset, merge_same_material_primitives, save_repair


def painted_terracotta_uv(doc, chunks):
    blob = next(blob for typ, blob in chunks if typ == 0x004e4942)
    view = doc['bufferViews'][doc['images'][0]['bufferView']]
    offset = view.get('byteOffset', 0)
    image = np.asarray(Image.open(io.BytesIO(blob[offset:offset + view['byteLength']]))).astype(float)
    # Reuse a small existing painted terracotta patch. Select a stable patch in
    # both V orientations so the backing cannot accidentally sample cream wall.
    score = 2 * image[:, :, 0] - 1.4 * image[:, :, 1] - 1.8 * image[:, :, 2]
    score = np.minimum(score, score[::-1])
    # A 24-pixel painted patch adds colour variation without sampling wall UVs.
    original_score = score.copy()
    for dy in (-12, 0, 12):
        for dx in (-12, 0, 12):
            score = np.minimum(score, np.roll(np.roll(original_score, dy, axis=0), dx, axis=1))
    score[:4] = score[-4:] = -1e6
    score[:, :4] = score[:, -4:] = -1e6
    y, x = np.unravel_index(np.argmax(score), score.shape)
    return np.array([[x - 10, y - 10], [x + 10, y - 10], [x, y + 10]]) / 1024


def main():
    asset = 'barn'
    record, audit, raw, doc, chunks = load_asset(asset)
    uv = painted_terracotta_uv(doc, chunks)
    primitive = doc['meshes'][0]['primitives'][0]
    positions = array_from(doc, chunks, primitive['attributes']['POSITION'])
    indices = array_from(doc, chunks, primitive['indices']).reshape(-1, 3)
    centres = positions[indices].mean(axis=1)
    pitch = .371 - .364 * np.abs((centres[:, 2] + .028) / .5)
    damaged_roof = (centres[:, 1] > pitch - .065) & (centres[:, 1] > -.08)
    primitive['indices'] = append_array(doc, chunks, indices[~damaged_roof].reshape(-1, 1), 5125, 'SCALAR', 34963)
    for eave_z in (-.485, .485):
        profile = [[-.465, .371, -.028], [.465, .371, -.028],
                   [.465, .007, eave_z], [-.465, .007, eave_z]]
        lower = [[x, y - .018, z] for x, y, z in profile]
        add_hexahedron(doc, chunks, profile + lower, uv)
        for column in range(6):
            x0 = -.465 + column * .155 + .0015
            x1 = x0 + .152
            for row in range(3):
                start = max(0., row / 3 - .015)
                end = min(1., (row + 1) / 3 + .015)
                shingle_lift = .018 + (2 - row) * .014
                y0, z0 = .371 - .364 * start + shingle_lift, -.028 + (eave_z + .028) * start
                y1, z1 = .371 - .364 * end + shingle_lift, -.028 + (eave_z + .028) * end
                top = [[x0, y0, z0], [x1, y0, z0], [x1, y1, z1], [x0, y1, z1]]
                add_hexahedron(doc, chunks, top + [[x, y - .014, z] for x, y, z in top], uv)
    for column in range(6):
        x0 = -.467 + column * .156
        top = [[x0, .430, -.061], [x0 + .153, .430, -.061],
               [x0 + .153, .430, .005], [x0, .430, .005]]
        add_hexahedron(doc, chunks, top + [[x, y - .030, z] for x, y, z in top], uv)
    merge_same_material_primitives(doc, chunks)
    save_repair(asset, record, audit, raw, doc, chunks,
                'Removed only the failed roof-zone triangles and rebuilt a regular relief tile roof over two closed pitched backing slabs; original cream walls, door and footprint preserved. Reused existing terracotta paint atlas; one material primitive.',
                {'removed_damaged_roof_triangles': int(damaged_roof.sum()), 'added_triangles': 528, 'raw_ridge_yz': [.371, -.028],
                 'raw_eave_y': .007, 'raw_half_width_x': .465, 'raw_eave_z': [-.485, .485],
                 'raw_backing_thickness': .018, 'existing_terracotta_uv_patch': uv.tolist()})


if __name__ == '__main__':
    main()
