"""Close tiny join gaps below both healthy house roofs, preserving their tiles."""
import io

import numpy as np
from PIL import Image

from glb_geometry_repair import add_hexahedron, load_asset, merge_same_material_primitives, save_repair


def existing_paint_patch(doc, chunks, target):
    blob = next(blob for typ, blob in chunks if typ == 0x004e4942)
    view = doc['bufferViews'][doc['images'][0]['bufferView']]
    offset = view.get('byteOffset', 0)
    pixels = np.asarray(Image.open(io.BytesIO(blob[offset:offset + view['byteLength']]))).astype(float)
    distance = np.sum((pixels - np.asarray(target))**2, axis=2)
    distance = np.maximum(distance, distance[::-1])
    original = distance.copy()
    for dy in (-5, 0, 5):
        for dx in (-5, 0, 5):
            distance = np.maximum(distance, np.roll(np.roll(original, dy, axis=0), dx, axis=1))
    distance[:6] = distance[-6:] = 1e9
    distance[:, :6] = distance[:, -6:] = 1e9
    y, x = np.unravel_index(np.argmin(distance), distance.shape)
    return np.array([[x - 4, y - 4], [x + 4, y - 4], [x, y + 4]]) / 1024


def repair(asset, target, eave_y):
    record, audit, raw, doc, chunks = load_asset(asset)
    uv = existing_paint_patch(doc, chunks, target)
    for eave_z in (-.470, .470):
        top = [[-.465, .380, 0], [.465, .380, 0],
               [.465, eave_y, eave_z], [-.465, eave_y, eave_z]]
        lower = [[x, y - .012, z] for x, y, z in top]
        add_hexahedron(doc, chunks, top + lower, uv)
    merge_same_material_primitives(doc, chunks)
    save_repair(asset, record, audit, raw, doc, chunks,
                'Added two thin continuous closed roof underlays behind existing healthy tile and ridge joins. Original tile/body geometry and original embedded paint atlas retained unchanged.',
                {'added_triangles': 24, 'raw_ridge_y': .380, 'raw_eave_y': eave_y,
                 'raw_x_bounds': [-.465, .465], 'raw_z_bounds': [-.470, .470],
                 'raw_thickness': .012, 'source_paint_uv_patch': uv.tolist(),
                 'large_teal_light_patch': 'Existing baked albedo on intact tile, confirmed by unlit and wireframe overhead review; preserved.'})


if __name__ == '__main__':
    repair('house-teal', [45, 90, 110], .008)
    repair('house-coral', [175, 70, 40], .045)
