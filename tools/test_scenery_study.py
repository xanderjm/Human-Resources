"""Observable geometry and streaming tests for the lightweight farmland study."""
import copy
import math
import unittest

from scenery_study.checks import join_issues, overlap, section_issues
from scenery_study.model import (LENGTH, TEMPLATES, Track, compose,
                                compose_template, query, template_index)
from stress_scenery_study import export_case


class SceneryStudyTests(unittest.TestCase):
    def test_every_arrangement_and_ordered_join(self):
        for seed in (0,17,77,514,2**63-1):
            for i in range(len(TEMPLATES)):
                a=compose_template(seed,0,i)
                self.assertEqual(section_issues(a),[],(seed,i))
                for j in range(len(TEMPLATES)):
                    b=compose_template(seed,1,j)
                    issues=join_issues(a,b)
                    self.assertEqual(issues,["adjacent composition repeat"] if i==j else [],(seed,i,j))

    def test_seam_checks_detect_independent_faults(self):
        a,b=compose(77,0),compose(77,1)
        for change,expected in (("position","track position discontinuity"),
                                ("tangent","track tangent discontinuity"),
                                ("width","track width discontinuity")):
            broken=copy.deepcopy(b)
            controls=[list(c) for c in b["track"].controls]
            if change=="position":
                controls[0][0] += .5
            elif change=="tangent":
                controls[0][1] += .5
            broken["track"]=Track(tuple(tuple(c) for c in controls),4 if change=="width" else 3)
            self.assertIn(expected,join_issues(a,broken))

    def test_overlap_checker_handles_rotation_and_containment(self):
        square=[(-2,-2),(2,-2),(2,2),(-2,2)]
        diamond=[(0,-1),(1,0),(0,1),(-1,0)]
        self.assertTrue(overlap(square,diamond))
        self.assertFalse(overlap(square,[(x+5,z) for x,z in diamond]))

    def test_checker_rejects_field_path_overlap_and_overhang(self):
        section=compose(17,0)
        p=section["prefabs"][0]
        broken=copy.deepcopy(section)
        broken["prefabs"][0]["hull"]=[(-1,10),(1,10),(1,20),(-1,20)]
        self.assertTrue(any("protected field intrusion" in e for e in section_issues(broken)))
        broken=copy.deepcopy(section)
        broken["prefabs"][0]["hull"]=[(26,10),(28,10),(28,20),(26,20)]
        self.assertTrue(any("track clearance" in e for e in section_issues(broken)))
        broken=copy.deepcopy(section)
        broken["prefabs"].append(copy.deepcopy(p))
        self.assertTrue(any("prefab overlap" in e for e in section_issues(broken)))
        broken=copy.deepcopy(section)
        broken["prefabs"][0]["props"][0]["x"]=0
        self.assertTrue(any("prop exceeds envelope" in e for e in section_issues(broken)))

    def test_query_order_does_not_reroll_sections(self):
        before=compose(514,3)
        for i in (1000000,-4,2,0,4):
            compose(514,i)
        self.assertEqual(before,compose(514,3))
        self.assertNotEqual(before,compose(77,3))

    def test_overlapping_windows_preserve_world_transforms_and_ids(self):
        for station in (0,63.9,64,128,10**8):
            a={p["id"]:p for p in query(514,station)}
            b={p["id"]:p for p in query(514,station+.25)}
            self.assertTrue(a.keys() & b.keys())
            for identity in a.keys() & b.keys():
                self.assertEqual(a[identity]["x"],b[identity]["x"])
                self.assertAlmostEqual(a[identity]["z"],b[identity]["z"]+.25,places=7)
                self.assertEqual(a[identity]["yaw"],b[identity]["yaw"])
            self.assertLessEqual(len(query(514,station,128)),4*100)

    def test_negative_and_large_section_boundaries(self):
        for index in (-1000000,-1,0,1,1562500,10**9-1):
            a,b=compose(514,index),compose(514,index+1)
            self.assertEqual(section_issues(a),[])
            self.assertEqual(join_issues(a,b),[])

    def test_no_adjacent_repeats_and_all_six_templates_per_block(self):
        for seed in range(24):
            values=[template_index(seed,i) for i in range(-12,60)]
            self.assertTrue(all(a!=b for a,b in zip(values,values[1:])))
            for i in range(0,len(values),6):
                self.assertEqual(set(values[i:i+6]),set(range(6)))

    def test_invalid_query_inputs_are_rejected(self):
        for distance,depth in ((math.nan,36),(math.inf,36),(0,0),(0,-1),(0,129)):
            with self.assertRaises(ValueError):
                query(17,distance,depth)
        for seed,index in ((1.5,0),(0,.5),(0,10**9+1)):
            with self.assertRaises(ValueError):
                compose(seed,index)

    def test_gallery_includes_absolute_offset_and_buffer_sections(self):
        case=export_case(19,31)
        self.assertEqual(case["start"],31)
        self.assertEqual([s["index"] for s in case["sections"]],list(range(30,38)))
        self.assertEqual(case["issues"],[])
        self.assertGreaterEqual(case["minimum_clear_width"],24)
        first=case["sections"][1]
        original=compose(19,31)
        self.assertEqual(first["name"],original["name"])
        self.assertEqual(first["prefabs"][0]["id"],original["prefabs"][0]["id"])


if __name__=="__main__":
    unittest.main()
