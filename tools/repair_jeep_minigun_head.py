"""Close a generated receiver/ring separation in the fictional game prop."""
from glb_geometry_repair import add_hexahedron, load_asset, merge_same_material_primitives, sample_uv_triangle, save_repair


def main():
    asset = 'jeep-minigun-head'
    record, audit, raw, doc, chunks = load_asset(asset)
    uv = sample_uv_triangle(doc, chunks, [.04, .10, .10])
    top = [[-.075, .12, -.135], [.020, .12, -.135],
           [.020, .12, .135], [-.075, .12, .135]]
    lower = [[x, -.16, z] for x, y, z in top]
    add_hexahedron(doc, chunks, top + lower, uv)
    merge_same_material_primitives(doc, chunks)
    save_repair(asset, record, audit, raw, doc, chunks,
                'Added a small closed matching navy connector collar behind the existing rear barrel ring to bridge the unintended receiver separation. Existing game-prop geometry and embedded paint atlas retained.',
                {'added_triangles': 12, 'raw_collar_bounds': [[-.075, -.16, -.135], [.020, .12, .135]],
                 'intended_openings_retained': 'Six stylized barrel muzzle insets and rear handle gap.',
                 'source_uv_triangle': uv.tolist()})


if __name__ == '__main__':
    main()
