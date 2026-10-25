"""Apply measured head trunnion origin and authoring-only component sockets."""
import json
import numpy as np

from normalize_glb import ROOT, matrix, pack, points, unpack


def main():
    record = ROOT / 'assets/production/records/jeep-minigun-head'
    path = record / 'model-audit.json'
    audit = json.loads(path.read_text())
    model = ROOT / audit['model_file']
    doc, chunks = unpack(model)
    root = doc['nodes'][-1]
    transform = matrix(root)
    raw_centre = np.array([-.0029296875, -.033203125, .0029296875, 1.])
    origin = (transform @ raw_centre)[:3]
    transform[:3, 3] -= origin
    root['matrix'] = transform.flatten(order='F').tolist()
    root['name'] = 'jeep-minigun-head-trunnion-pivot'
    doc['asset']['extras']['pivot'] = 'measured-trunnion-centre'
    doc['asset']['extras']['trunnion_axis'] = '+X'
    pack(doc, chunks, model)
    vertices, stats = points(*unpack(model))
    audit['import_review'].update(pivot='measured-trunnion-centre',
        bounds_m=[vertices.min(axis=0).tolist(), vertices.max(axis=0).tolist()],
        dimensions_m=np.ptp(vertices, axis=0).tolist(), vertex_checks=stats,
        trunnion_origin_raw=raw_centre[:3].tolist(), trunnion_axis='+X')
    audit['import_review']['notes'] += ' Origin is the measured pair-of-boss bounding centre; no character pose, rig or animation added.'
    path.write_text(json.dumps(audit, indent=2) + '\n')


if __name__ == '__main__':
    main()
