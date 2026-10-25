"""Offline regression checks for final measured GLB outputs and real joins."""
import io
import json
import unittest

import numpy as np
from PIL import Image
from normalize_glb import accessor, unpack
from build_measured_modules import ROOT, build, material, sample, world_uv


EXPECTED = {'fence-post': [.16,1.1,.16], 'fence-rail':[2,.12,.09],
    'ground-tile':[16,0,16], 'path-straight':[3,0,16],
    'path-corner':[7.5,0,7.5], 'soil-bed':[1,0,4],
    'road-straight':[6,0,16], 'road-corner':[12,0,12],
    'road-junction':[16,0,11], 'pavement-straight':[1.5,.16,16],
    'pavement-corner':[13.5,.16,13.5], 'pavement-end':[1.5,.16,.4]}


def decoded(name, yaw=0):
    filename=name+(f'-yaw{yaw}' if yaw else '')+'.glb'
    path=ROOT/'assets/models'/name/filename
    doc,chunks=unpack(path);blob=next(b for t,b in chunks if t==0x004e4942)
    primitive=doc['meshes'][0]['primitives'][0]
    attributes={key:accessor(doc,blob,index) for key,index in primitive['attributes'].items()}
    return doc,blob,attributes,accessor(doc,blob,primitive['indices']).reshape(-1)


class MeasuredModules(unittest.TestCase):
    def test_final_geometry_dimensions_finite_and_nonzero_triangles(self):
        for name,expected in EXPECTED.items():
            with self.subTest(name=name):
                doc,blob,a,indices=decoded(name);p=a['POSITION']
                self.assertTrue(all(np.isfinite(v).all() for v in a.values()))
                self.assertLess(int(indices.max()),len(p))
                self.assertTrue(np.allclose(np.ptp(p,axis=0),expected,atol=1e-6))
                self.assertAlmostEqual(float(p[:,1].min()),0,places=6)
                tri=p[indices.reshape(-1,3)]
                self.assertTrue((np.linalg.norm(np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]),axis=1)>1e-9).all())
                self.assertTrue(np.allclose(np.linalg.norm(a['NORMAL'],axis=1),1,atol=1e-6))

    def test_embedded_periodic_albedo_matches_approved_source(self):
        meshes=build()
        for m in meshes:
            with self.subTest(name=m.name):
                doc,blob,a,indices=decoded(m.name)
                self.assertEqual(doc['samplers'][0]['wrapS'],10497)
                self.assertEqual(doc['samplers'][0]['wrapT'],10497)
                view=doc['bufferViews'][doc['images'][0]['bufferView']]
                start=view['byteOffset'];image=Image.open(io.BytesIO(blob[start:start+view['byteLength']])).convert('RGB')
                self.assertTrue(np.array_equal(np.asarray(image),material(m.texture)[2]))
                mat=doc['materials'][0]
                self.assertNotIn('normalTexture',mat);self.assertNotIn('occlusionTexture',mat)
                self.assertNotIn('metallicRoughnessTexture',mat['pbrMetallicRoughness'])

    def test_actual_geometry_and_uv_at_assembled_tangent_connections(self):
        pairs=[]
        for prefix,r in [('path',6),('road',9)]:
            pairs.extend([(prefix+'-straight','end',0,[r,0,-16],prefix+'-corner','start',0,[0,0,0]),
                (prefix+'-corner','end',0,[0,0,0],prefix+'-straight','start',270,[0,0,r])])
        pairs.extend([('road-straight','end',90,[-24,0,0],'road-junction','west',0,[0,0,0]),
            ('road-junction','east',0,[0,0,0],'road-straight','start',90,[8,0,0]),
            ('road-junction','north',0,[0,0,0],'road-straight','start',0,[0,0,8]),
            ('pavement-straight','end',0,[12,0,-16],'pavement-corner','start',0,[0,0,0]),
            ('pavement-corner','end',0,[0,0,0],'pavement-straight','start',270,[0,0,12])])
        lookup={m.name:m for m in build()};results=[]
        for na,pa,ya,ta,nb,pb,yb,tb in pairs:
            da,_,aa,_=decoded(na,ya);db,_,ab,_=decoded(nb,yb)
            a=next(p for p in da['asset']['extras']['ports'] if p['name']==pa)
            b=next(p for p in db['asset']['extras']['ports'] if p['name']==pb)
            ea=np.asarray(a['edge_m'])+ta;eb=np.asarray(b['edge_m'])+tb
            gap=float(np.max(np.linalg.norm(ea-eb,axis=1)));self.assertLess(gap,1e-6)
            for name,port,attrs in [(na,a,aa),(nb,b,ab)]:
                if name.startswith('pavement'):continue
                for point,uv in zip(port['edge_m'],port['edge_uv']):
                    hits=np.linalg.norm(attrs['POSITION']-point,axis=1)<1e-6
                    delta=attrs['TEXCOORD_0'][hits]-uv
                    self.assertTrue(np.any(np.linalg.norm(delta-np.rint(delta),axis=1)<1e-6))
            t=np.linspace(0,1,4097)[:,None]
            uva=(1-t)*np.asarray(a['edge_uv'])[0]+t*np.asarray(a['edge_uv'])[1]
            uvb=(1-t)*np.asarray(b['edge_uv'])[0]+t*np.asarray(b['edge_uv'])[1]
            error=float(np.abs(sample(lookup[na].texture,uva)-sample(lookup[nb].texture,uvb)).max())
            self.assertLess(error,1e-5)
            results.append({'a':na+':'+pa,'yaw_a':ya,'b':nb+':'+pb,'yaw_b':yb,'position_gap_m':gap,'samples':4097,'max_rgb_error':error})
        # Every surface variant must keep truly uniform top UV density. This
        # catches the former T-centre brush stretching even when endpoints pass.
        for name in EXPECTED:
            if name.startswith('fence'):continue
            for yaw in [0,90,180,270]:
                _,_,attrs,_=decoded(name,yaw)
                top=attrs['NORMAL'][:,1]>.8
                self.assertTrue(np.allclose(attrs['TEXCOORD_0'][top],attrs['POSITION'][top][:,[0,2]],atol=1e-6))
        path=ROOT/'assets/production/measured-modules/assembled-connection-checks.json'
        path.write_text(json.dumps({'status':'passed','connections':results,'uniform_planar_uv_check':'passed all40 surface orientations'},indent=2)+'\n')


if __name__=='__main__':unittest.main()
