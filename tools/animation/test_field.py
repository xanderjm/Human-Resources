"""Portable test-field choreography: metres, Blender XY ground/+Z up.

No game state or balance rules. Motion is distance-driven so a walk cycle
advances by the same stride distance as its actor's world translation.
"""
import bisect
import math

TAU = math.tau


class Oval:
    def __init__(self, radii, samples=4096):
        self.rx, self.ry = radii
        self.samples = samples
        self.distances = [0.0]
        previous = self.point(0)
        for i in range(1, samples+1):
            current = self.point(TAU*i/samples)
            self.distances.append(self.distances[-1]+math.dist(previous, current))
            previous = current
        self.length = self.distances[-1]

    def point(self, angle):
        return self.rx*math.cos(angle), self.ry*math.sin(angle)

    def distance_at(self, angle):
        u = (angle % TAU)/TAU*self.samples
        i = int(u)
        return self.distances[i]+(u-i)*(self.distances[i+1]-self.distances[i])

    def sample(self, distance):
        d = distance % self.length
        i = min(self.samples-1, max(0,bisect.bisect_right(self.distances,d)-1))
        u = (d-self.distances[i])/(self.distances[i+1]-self.distances[i])
        a = TAU*(i+u)/self.samples
        x,y = self.point(a)
        dx,dy = -self.rx*math.sin(a), self.ry*math.cos(a)
        heading = math.atan2(dx,-dy)  # Imported GLBs face Blender -Y.
        curvature = self.rx*self.ry/(dx*dx+dy*dy)**1.5
        return (x,y),heading,curvature


def character_pose(route, character, seconds):
    speed = character['stride_m']/character['walk_seconds']
    angle = route['phase']+seconds*speed/route['radius']
    x = route['center'][0]+route['radius']*math.cos(angle)
    y = route['center'][1]+route['radius']*math.sin(angle)
    heading = math.atan2(-math.sin(angle),-math.cos(angle))
    gait_seconds = seconds+route['gait_phase']*character['walk_seconds']
    return (x,y),heading,gait_seconds


def ease(t):
    t = min(1,max(0,t))
    return t*t*(3-2*t)


def camera_pose(seconds):
    # Deliberate cuts keep the world moving while changing review scale.
    if seconds < 10:
        u = seconds/10
        return (13-3*u,-19,17), (0,0,.6), 26.5, 'ALL CHARACTERS / TRUE SCALE'
    if seconds < 20:
        u = (seconds-10)/10
        return (5-4*u,-14,10), (0,.0,.75), 15.5, 'WALKING / ANIMALS + HUMANOIDS'
    u = (seconds-20)/10
    return (10-3*u,-18,12), (1,-1,.6), 24.0, 'PATROL LOOP / JEEP + TANK'


def validate_routes(spec, characters):
    oval = Oval(spec['road']['radii'])
    smallest = 1e9
    travelled = {r['id']:0.0 for r in spec['characters']}
    previous = {}
    for frame in range(round(spec['duration_seconds']*spec['fps'])+1):
        t = frame/spec['fps']
        positions = {}
        for route in spec['characters']:
            c = characters[route['id']]
            p,yaw,gait = character_pose(route,c,t)
            assert all(math.isfinite(v) for v in (*p,yaw,gait))
            if route['id'] in previous:
                travelled[route['id']] += math.dist(previous[route['id']],p)
            previous[route['id']] = p
            positions[route['id']] = p
        for v in spec['vehicles']:
            d = oval.distance_at(v['phase'])+v['speed_mps']*t
            positions[v['id']] = oval.sample(d)[0]
        items = list(positions.items())
        for i,(name,p) in enumerate(items):
            for other,q in items[i+1:]:
                gap = math.dist(p,q)
                smallest = min(smallest,gap)
                # Conservative footprint circles: no scripted intersections.
                radii = {'cow':1.1,'pig':.7,'sheep':.6,'jeep':1.85,'tank':1.75}
                minimum = radii.get(name,.65)+radii.get(other,.65)
                if gap < minimum:
                    raise ValueError(f'Routes intersect: {name}/{other} at {t:.2f}s ({gap:.3f}m)')
    for name, distance in travelled.items():
        c = characters[name]
        expected = spec['duration_seconds']*c['stride_m']/c['walk_seconds']
        if abs(distance-expected) > .002:
            raise ValueError('Stride/world-distance mismatch: '+name)
    return {'sampled_frames':round(spec['duration_seconds']*spec['fps'])+1,
            'minimum_actor_center_distance_m':smallest,'character_distance_m':travelled,
            'road_length_m':oval.length,'status':'passed'}
