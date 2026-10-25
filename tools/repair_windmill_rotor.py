"""Bridge each of the eight existing rotor vanes to its hub with narrow spokes."""
import numpy as np

from glb_geometry_repair import add_hexahedron, array_from, load_asset, sample_uv_triangle, save_repair


def main():
    asset = 'windmill-rotor'
    record, audit, raw, doc, chunks = load_asset(asset)
    primitive = doc['meshes'][0]['primitives'][0]
    positions = array_from(doc, chunks, primitive['attributes']['POSITION'])
    uv = sample_uv_triangle(doc, chunks, [-.06, .12, 0])
    # The thin axis is X. Eight directions are measured from the outer vanes,
    # not assumed perfectly symmetric; each spoke overlaps hub and vane.
    angles = np.arctan2(positions[:, 2], positions[:, 1])
    radii = np.linalg.norm(positions[:, 1:], axis=1)
    directions = []
    for nominal in np.arange(8) * np.pi / 4:
        difference = np.angle(np.exp(1j * (angles - nominal)))
        selected = positions[(radii > .31) & (np.abs(difference) < .34)]
        centre = selected[:, 1:].mean(axis=0)
        direction = centre / np.linalg.norm(centre)
        directions.append(direction.tolist())
        tangent = np.array([-direction[1], direction[0]]) * .018
        start, end = direction * .075, direction * .325
        profile = [start - tangent, end - tangent, end + tangent, start + tangent]
        corners = [[x, *point] for x in (-.087, -.042) for point in profile]
        add_hexahedron(doc, chunks, corners, uv)
    save_repair(asset, record, audit, raw, doc, chunks,
                'Added eight narrow supported radial spokes to close the visible hub-to-vane gaps.',
                {'added_triangles': 96, 'measured_radial_directions_yz': directions,
                 'raw_radial_span': [.075, .325], 'raw_spoke_width': .036, 'raw_thickness': .045})


if __name__ == '__main__':
    main()
