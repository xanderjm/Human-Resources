"""Small, repeatable farmland composition model for the visual planning study.

All dimensions are provisional study metres. The native presentation generator
remains independent; these layouts are proposals to inspect before integration.
"""
from dataclasses import dataclass
import hashlib
import math

LENGTH = 64.0
HALF_WIDTH = 30.0
CORE_HALF_WIDTH = 12.0
TRACK_WIDTH = 3.0
TRACK_CLEARANCE = 1.0
SECTION_MARGIN = 2.0

FAMILIES = {
    "A": "Barn corner", "B": "Crop strip", "C": "Grove bulge",
    "D": "Silo corner", "E": "Windmill corner", "F": "Fence connector",
}

# Primary anchors are composed together; no independent scatter of landmarks.
# (name, track bend, (family, side, station) ...)
TEMPLATES = (
    ("Barn clearing", -1.8, (("A", -1, 16), ("C", -1, 47),
                             ("B", 1, 35))),
    ("Crop bend", 1.7, (("C", -1, 13), ("F", -1, 46),
                       ("B", 1, 21), ("C", 1, 49))),
    ("Grove pockets", -.8, (("C", -1, 15), ("C", -1, 47),
                             ("C", 1, 28))),
    ("Utility clearing", 1.3, (("D", -1, 35),
                              ("E", 1, 21), ("F", 1, 49))),
    ("Open pasture", -1.4, (("F", -1, 12), ("C", 1, 46))),
    ("Farmyard bend", 1.9, (("A", -1, 32), ("C", 1, 12), ("E", 1, 50))),
)


def unit(seed, index, salt):
    """A location-specific draw: query order and camera position cannot reroll it."""
    raw = f"{seed}:{index}:{salt}".encode("ascii")
    return int.from_bytes(hashlib.blake2s(raw, digest_size=8).digest(), "big") / 2**64


def template_index(seed, index):
    # Each six-section block contains all six arrangements, with no adjacent
    # duplicate across block boundaries. Direct section queries need no history.
    block, offset = divmod(index, len(TEMPLATES))
    order = sorted(range(6), key=lambda n: unit(seed,0,f"template-order-{n}"))
    middle = sorted(order[1:5], key=lambda n: unit(seed,block,f"template-middle-{n}"))
    return ([order[0]] + middle + [order[5]])[offset]


def convex_hull(points):
    points = sorted(set(points))

    def cross(o, a, b):
        return (a[0]-o[0])*(b[1]-o[1]) - (a[1]-o[1])*(b[0]-o[0])

    lower, upper = [], []
    for p in points:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(points):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def port(seed, boundary):
    return (25.6 + 2.2*unit(seed, boundary, "track-x"),
            .07*(unit(seed, boundary, "track-tangent")-.5))


@dataclass(frozen=True)
class Track:
    """Two cubic Beziers, sharing the middle tangent and boundary ports."""
    controls: tuple
    width: float = TRACK_WIDTH

    def at(self, station):
        if not 0 <= station <= LENGTH:
            raise ValueError("track station outside section")
        i = 0 if station <= 32 else 1
        t = (station-i*32)/32
        a, b, c, d = self.controls[i]
        x = (1-t)**3*a + 3*(1-t)**2*t*b + 3*(1-t)*t*t*c + t**3*d
        tangent = 3*((1-t)**2*(b-a)+2*(1-t)*t*(c-b)+t*t*(d-c))/32
        return x, tangent

    def bounds(self):
        # Bezier convex-hull bounds enclose the entire curve, not just samples.
        return min(x for c in self.controls for x in c), max(x for c in self.controls for x in c)


def make_track(seed, index, bend):
    x0, m0 = port(seed, index)
    x1, m1 = port(seed, index+1)
    xm = min(28.0, max(25.0, (x0+x1)/2+bend))
    mm = (x1-x0)/64
    return Track(((x0, x0+m0*32/3, xm-mm*32/3, xm),
                  (xm, xm+mm*32/3, x1-m1*32/3, x1)))


def local_prefab(family, side, variant):
    """Footprint and source-sized prop proxies. Rotation never scales a mesh."""
    if family == "A":
        points = [(-7,-8),(1,-8),(1,-4),(6,-4),(6,8),(-7,8)]
        props = [("barn",-1,0,3.55),("hay",3.8,4.5,.75),
                 ("bush",-5,5,.65),("bush",-4.7,-5,.65)]
    elif family == "B":
        points = [(-3.5,-10),(2.5,-10),(3.5,-8),(3.5,10),(-3.5,10)]
        props = [("crop", x, z, .2) for x in (-2,0,2) for z in range(-7,8,2)]
    elif family == "C":
        half = 6.5 if side == -1 else 3.8
        points = [(-half,-6),(-half+1,-9),(half-1,-7),
                  (half,1),(half-1,7),(-half+2,9),(-half,5)]
        props = [("tree",-half+2.2,-3.5,1.3),
                 ("tree",-half+2.1,3.1,1.3),
                 ("tree",.6 if side == -1 else .1,0,1.0),
                 ("bush",half-1.7,3.6,.55),("rock",half-1.7,-3,.4)]
        if variant:
            props += [("bush",0,6,.55),("rock",-1,-6,.4)]
    elif family == "D":
        points = [(-6,-7),(3,-7),(5,-3),(5,6),(-4,7),(-6,3)]
        props = [("silo",-2,-1,1.45),("tree",.6,3.7,1.3),
                 ("bush",2,-3.5,.6),("rock",-3.7,3.5,.4)]
    elif family == "E":
        points = [(-3.5,-7),(2,-7),(3.5,-3),(3.5,6),(-2.5,7),(-3.5,2)]
        props = [("windmill",0,-1,1),("hay",.5,3.7,.75),
                 ("bush",-1.5,-4.7,.55)]
    else:
        points = [(-1.5,-8),(1.5,-8),(1.5,8),(-1.5,8)]
        props = [("fence",0,z,.15) for z in range(-7,8,2)]
    return points, props


def transform(point, x, z, angle):
    c, s = math.cos(angle), math.sin(angle)
    return (x + point[0]*c - point[1]*s, z + point[0]*s + point[1]*c)


def place_prefab(seed, index, slot, anchor, track):
    family, side, station = anchor
    variant = int(unit(seed,index,f"variant-{slot}")*2)
    points, props = local_prefab(family, side, variant)
    # A 180-degree rotation creates the inward-facing right variant. Landmark
    # angles stay fixed; secondary shapes accept a small authored range.
    jitter = 0 if family in "ADE" else math.radians(6 if side == -1 else 3)
    angle = (math.pi if side == 1 else 0) + (unit(seed,index,f"yaw-{slot}")-.5)*2*jitter
    rotated = [transform(p,0,0,angle) for p in points]
    z = station + (unit(seed,index,f"z-{slot}")-.5)*5
    z = max(SECTION_MARGIN-min(p[1] for p in rotated),
            min(LENGTH-SECTION_MARGIN-max(p[1] for p in rotated), z))
    if side == -1:
        x = -23 + (unit(seed,index,f"x-{slot}")-.5)*2
        x = max(-HALF_WIDTH+.5-min(p[0] for p in rotated),
                min(-CORE_HALF_WIDTH-.5-max(p[0] for p in rotated),x))
    else:
        outer = track.bounds()[0] - TRACK_WIDTH/2 - TRACK_CLEARANCE
        x = outer - max(p[0] for p in rotated) - .2 - unit(seed,index,f"x-{slot}")*.5
    # Keep section-relative vertices for accurate geometry at very large indices.
    poly = [transform(p,x,z,angle) for p in points]
    instances = []
    for n,(kind,px,pz,radius) in enumerate(props):
        qx,qz = transform((px,pz),x,z,angle)
        instances.append({"id":f"{index}:{slot}:{n}","kind":kind,
                          "x":qx,"z":qz,"r":radius,"yaw":angle})
    return {"id":f"{index}:{slot}","family":family,"side":side,
            "variant":variant,"x":x,"z":z,"yaw":angle,
            "polygon":poly,"hull":convex_hull(poly),"props":instances}


def compose(seed, index):
    if type(seed) is not int or type(index) is not int:
        raise ValueError("seed and absolute section index must be integers")
    if abs(index) > 10**9:
        raise ValueError("study section index outside supported range")
    return compose_template(seed,index,template_index(seed,index))


def compose_template(seed,index,n):
    """Explicit fixture composition for exhaustive join tests."""
    if type(n) is not int or not 0 <= n < len(TEMPLATES):
        raise ValueError("unknown study composition")
    name,bend,anchors = TEMPLATES[n]
    track = make_track(seed,index,bend)
    return {"index":index,"template":n,"name":name,"track":track,
            "prefabs":[place_prefab(seed,index,s,a,track) for s,a in enumerate(anchors)]}


def query(seed, distance, depth=36):
    """Only nearby sections; querying a new window never moves existing content."""
    if not math.isfinite(distance) or not math.isfinite(depth) or not 0 < depth <= 128:
        raise ValueError("invalid study window")
    low, high = distance-depth/2, distance+depth/2
    first,last = math.floor(low/LENGTH),math.floor(high/LENGTH)
    result=[]
    for index in range(first,last+1):
        section=compose(seed,index)
        for prefab in section["prefabs"]:
            for p in prefab["props"]:
                world_z=index*LENGTH+p["z"]
                if world_z+p["r"] >= low and world_z-p["r"] <= high:
                    result.append({**p,"z":world_z-distance})
    return result
