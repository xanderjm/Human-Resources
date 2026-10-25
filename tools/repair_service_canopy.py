"""Close tiny gaps between generated roof panels with a continuous backing."""
from glb_geometry_repair import add_hexahedron, load_asset, sample_uv_triangle, save_repair


def main():
    asset = 'service-canopy'
    record, audit, raw, doc, chunks = load_asset(asset)
    uv = sample_uv_triangle(doc, chunks, [.32, .175, .25])
    top = [[-.325, .177, -.475], [.325, .177, -.475], [.325, .177, .475], [-.325, .177, .475]]
    bottom = [[x, .165, z] for x, y, z in top]
    add_hexahedron(doc, chunks, top + bottom, uv)
    save_repair(asset, record, audit, raw, doc, chunks,
                'Added a thin continuous cream roof backing under the existing red panels to close tiny through-gaps; four supports and open space beneath retained.',
                {'added_triangles': 12, 'raw_backing_bounds': [[-.325, .165, -.475], [.325, .177, .475]]})


if __name__ == '__main__':
    main()
