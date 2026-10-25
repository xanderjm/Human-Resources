"""Geometry and continuity checks for the offline footprint study."""
import math

from .model import CORE_HALF_WIDTH, HALF_WIDTH, LENGTH, SECTION_MARGIN, TRACK_CLEARANCE


def bounds(poly):
    return (min(p[0] for p in poly), max(p[0] for p in poly),
            min(p[1] for p in poly), max(p[1] for p in poly))


def overlap(a,b):
    """Separating axes for the conservative convex exclusion envelopes."""
    ax0,ax1,az0,az1=bounds(a)
    bx0,bx1,bz0,bz1=bounds(b)
    if ax1 <= bx0 or bx1 <= ax0 or az1 <= bz0 or bz1 <= az0:
        return False
    for poly in (a,b):
        for p,q in zip(poly,poly[1:]+poly[:1]):
            nx,nz=-(q[1]-p[1]),q[0]-p[0]
            aa=[x*nx+z*nz for x,z in a]
            bb=[x*nx+z*nz for x,z in b]
            if max(aa) <= min(bb)+1e-9 or max(bb) <= min(aa)+1e-9:
                return False
    return True


def contains_circle(hull,x,z,r):
    for p,q in zip(hull,hull[1:]+hull[:1]):
        dx,dz=q[0]-p[0],q[1]-p[1]
        signed=(dx*(z-p[1])-dz*(x-p[0]))/math.hypot(dx,dz)
        if signed < r-1e-8:
            return False
    return True


def section_issues(section):
    issues=[]
    track=section["track"]
    road_min,road_max=track.bounds()
    if road_max+track.width/2 > HALF_WIDTH+1e-8 or track.width <= 0:
        issues.append("track outside world or invalid width")
    prefabs=section["prefabs"]
    ids=[p["id"] for p in prefabs]
    if len(set(ids)) != len(ids):
        issues.append("duplicate prefab identity")
    for p in prefabs:
        values=[v for point in p["hull"] for v in point]+[p["x"],p["z"],p["yaw"]]
        if not all(math.isfinite(v) for v in values):
            issues.append(f'{p["id"]}: nonfinite transform')
            continue
        x0,x1,z0,z1=bounds(p["hull"])
        if x0 < -HALF_WIDTH-1e-8 or x1 > HALF_WIDTH+1e-8:
            issues.append(f'{p["id"]}: footprint outside world')
        if z0 < SECTION_MARGIN-1e-8 or z1 > LENGTH-SECTION_MARGIN+1e-8:
            issues.append(f'{p["id"]}: footprint crosses section margin')
        if x1 > -CORE_HALF_WIDTH and x0 < CORE_HALF_WIDTH:
            issues.append(f'{p["id"]}: protected field intrusion')
        # The curve's control hull bounds its whole width. This conservative
        # test covers between-sample bulges as well as the sampled surface.
        if x1+TRACK_CLEARANCE > road_min-track.width/2+1e-8:
            issues.append(f'{p["id"]}: track clearance')
        for instance in p["props"]:
            if not contains_circle(p["hull"],instance["x"],instance["z"],instance["r"]):
                issues.append(f'{instance["id"]}: prop exceeds envelope')
    for i,a in enumerate(prefabs):
        for b in prefabs[i+1:]:
            if overlap(a["hull"],b["hull"]):
                issues.append(f'{a["id"]}/{b["id"]}: prefab overlap')
    return issues


def join_issues(a,b):
    issues=[]
    if b["index"] != a["index"]+1:
        return ["nonadjacent sections"]
    ap,at=a["track"].at(LENGTH)
    bp,bt=b["track"].at(0)
    if abs(ap-bp) > 1e-9:
        issues.append("track position discontinuity")
    if abs(at-bt) > 1e-9:
        issues.append("track tangent discontinuity")
    if a["track"].width != b["track"].width:
        issues.append("track width discontinuity")
    if a["template"] == b["template"]:
        issues.append("adjacent composition repeat")
    for x in a["prefabs"]:
        for y in b["prefabs"]:
            shifted=[(px,pz+LENGTH) for px,pz in y["hull"]]
            if overlap(x["hull"],shifted):
                issues.append("prefab overlap across join")
    return issues


def clear_width(section,station):
    left,right=-HALF_WIDTH,HALF_WIDTH
    for p in section["prefabs"]:
        intersections=[]
        poly=p["hull"]
        for a,b in zip(poly,poly[1:]+poly[:1]):
            if min(a[1],b[1]) <= station < max(a[1],b[1]):
                t=(station-a[1])/(b[1]-a[1])
                intersections.append(a[0]+(b[0]-a[0])*t)
        if intersections:
            if p["side"] == -1:
                left=max(left,max(intersections))
            else:
                right=min(right,min(intersections))
    # The dirt track is scenery too; the open-field measurement stops before it.
    right=min(right,section["track"].at(station)[0]-section["track"].width/2)
    return right-left
