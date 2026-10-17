"""Footprint of a room relative to its door: depth behind the door, extent to the door's left and
right (seen from inside the hall, looking at the door), and the rotation used."""
import math
import sys

import numpy as np

import game, merge, wmo


def footprint(name, groups, door):
    root = wmo.Root(game.read(name))
    side = None
    pts = []
    for g in groups:
        grp = wmo.Group(game.read('%s_%03d.wmo' % (name[:-4], g)))
        pts += grp.verts
        for k in range(grp.portal_start, grp.portal_start + grp.portal_count):
            p, other, sd, _ = root.portal_refs[k]
            if p == door:
                side = sd
    n = np.array(root.portals[door][2:5])
    out = -side * n
    s, c = root.portals[door][:2]
    dv = np.array(root.portal_verts[s:s + c])
    dc = dv.mean(0)
    P = np.array(pts) - dc
    # local frame: x = from door into the room (= -out), y = to the left when standing in the hall
    # facing the door (hall looks along -out... standing in hall facing the room means looking along -out)
    fwd = -out
    left = np.array([-fwd[1], fwd[0], 0.0])
    depth = P @ fwd
    lat = P @ left
    return dict(depth=(float(depth.min()), float(depth.max())), left=float(lat.max()), right=float(-lat.min()),
                door_w=float(np.ptp(dv @ left)), door_h=float(np.ptp(dv[:, 2])), door_z=float(dv[:, 2].min()),
                floor=float(np.array(pts)[:, 2].min()))


if __name__ == '__main__':
    for spec in sys.argv[1:]:
        n, gs, door = spec.split(':')
        f = footprint(n.replace('/', chr(92)), [int(g) for g in gs.split(',')], int(door))
        print(n.split('/')[-1], gs, door, {k: (tuple(round(x, 1) for x in v) if isinstance(v, tuple) else round(v, 1)) for k, v in f.items()})
