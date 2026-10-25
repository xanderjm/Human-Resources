"""Stress the offline prefab model and export bounded data for visual review."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import statistics

from scenery_study.checks import clear_width, join_issues, section_issues
from scenery_study.model import CORE_HALF_WIDTH, FAMILIES, LENGTH, compose

ROOT=Path(__file__).resolve().parents[1]
GALLERY_SEEDS=(17,77,514,2601,31,91,820,4033,43,129,911,6029,59,211,1237,8053)


def stress(seed_count,section_count):
    counts=Counter()
    transitions=set()
    failures=[]
    minimum=float("inf")
    minimum_case=None
    sections=joins=props=0
    for seed in range(seed_count):
        previous=None
        for index in range(-2,section_count-2):
            s=compose(seed,index)
            errors=section_issues(s)
            widths=[clear_width(s,z) for z in range(65)]
            narrowest=min(widths)
            if narrowest < minimum:
                minimum,minimum_case=narrowest,{"seed":seed,"section":index,"template":s["name"]}
            if narrowest < 2*CORE_HALF_WIDTH:
                errors.append("insufficient open-field width")
            if previous:
                errors += join_issues(previous,s)
                transitions.add((previous["template"],s["template"]))
                joins+=1
            if errors:
                failures.append({"seed":seed,"section":index,"issues":errors})
            counts[s["name"]]+=1
            props+=sum(len(p["props"]) for p in s["prefabs"])
            sections+=1
            previous=s
    return {"seed_count":seed_count,"sections":sections,"joins":joins,
            "props":props,"failures":failures,"minimum_clear_width":minimum,
            "narrowest_case":minimum_case,"composition_counts":dict(counts),
            "ordered_transitions":len(transitions),"transition_pairs":sorted(transitions)}


def rounded(value):
    if isinstance(value,float):
        return round(value,4)
    if isinstance(value,(list,tuple)):
        return [rounded(v) for v in value]
    if isinstance(value,dict):
        return {k:rounded(v) for k,v in value.items()}
    return value


def export_case(seed,start=0):
    exported=[]
    issues=[]
    widths=[]
    previous=None
    families=Counter()
    for index in range(start-1,start+7):
        s=compose(seed,index)
        issues+=section_issues(s)
        if previous:
            issues+=join_issues(previous,s)
        if start <= index < start+6:
            widths += [clear_width(s,z) for z in range(65)]
        for p in s["prefabs"]:
            families[p["family"]]+=1
        exported.append({"index":index,"name":s["name"],
                         "prefabs":s["prefabs"],"width":s["track"].width,
                         "track":[[s["track"].at(z)[0],z] for z in range(65)]})
        previous=s
    return rounded({"seed":seed,"start":start,"sections":exported,"issues":issues,
                    "minimum_clear_width":min(widths),"median_clear_width":statistics.median(widths),
                    "families":dict(families)})


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--seeds",type=int,default=256)
    p.add_argument("--sections",type=int,default=32)
    p.add_argument("--output",type=Path,default=ROOT/"build/evidence/scenery-prefab-study.json")
    p.add_argument("--gallery",type=Path,default=ROOT/"build/scenery-prefab-study/gallery.json")
    a=p.parse_args()
    if not 1 <= a.seeds <= 4096 or not 6 <= a.sections <= 128:
        p.error("seeds must be 1..4096 and sections 6..128")
    report=stress(a.seeds,a.sections)
    source_paths=[Path(__file__),Path(__file__).with_name("test_scenery_study.py"),
                  Path(__file__).with_name("scenery_study")/"model.py",
                  Path(__file__).with_name("scenery_study")/"checks.py"]
    report["source_sha256"]={str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest()
                             for f in source_paths}
    cases=[export_case(seed) for seed in GALLERY_SEEDS]
    narrowest=report["narrowest_case"]
    cases[-1]=export_case(narrowest["seed"],narrowest["section"])
    cases[-1]["narrowest"]=True
    gallery_issues=[{"seed":c["seed"],"issues":c["issues"]} for c in cases if c["issues"]]
    report["gallery_failures"]=gallery_issues
    report["gallery_cases"]=len(cases)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(rounded(report),indent=2)+"\n")
    a.gallery.parent.mkdir(parents=True,exist_ok=True)
    gallery={"version":1,"families":FAMILIES,"section_length":LENGTH,
             "core_half_width":CORE_HALF_WIDTH,"report":rounded(report),"cases":cases}
    a.gallery.write_text(json.dumps(gallery,separators=(",",":"))+"\n")
    print(json.dumps({k:report[k] for k in ("sections","joins","props","ordered_transitions",
                                            "minimum_clear_width","narrowest_case")}))
    if report["failures"] or gallery_issues:
        print(json.dumps((report["failures"]+gallery_issues)[:12],indent=2))
        raise SystemExit(1)
    print(f"PASS: {a.seeds} seeds, {len(cases)} visual cases; {a.output}")


if __name__=="__main__":
    main()
