"""Shared frame-accurate film chapters for render and typography."""


def timeline(spec):
    shots = []
    start = 0
    for clip, seconds in [('idle',3),('walk',3),('run',2.1),('special',3.2),
                          ('alert',1.8),('startle',.9),('hit_react',.6),
                          ('beam_lift',1.5),('beam_struggle',2.4),('beam_release',1.4)]:
        shots.append({'clip': clip, 'start': start, 'end': start+seconds,
                      'loop': spec['clips'][clip]['loop']})
        start += seconds
    return shots


def locate(shots, time):
    for shot in shots:
        if time < shot['end']-1e-6:
            return shot, time-shot['start']
    return shots[-1], shots[-1]['end']-shots[-1]['start']
