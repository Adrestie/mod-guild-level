"""Room placement with real occupancy: each room's geometry rasterised into 1-yard cells with the
height span it fills, rooms may not share a cell at overlapping heights (with a margin), nor enter
the hall's volume."""
import math

import numpy as np

import game, wmo

MARGIN_Z = 1.0


def occupancy(name, groups, door, pad=0.75):
    """Cells of the room in the door's frame: {(depth, lateral): (zmin, zmax)} with z relative to the
    door's floor, depth along the way in, lateral to the left seen from the hall."""
    root = wmo.Root(game.read(name))
    side = None
    pts = []
    for g in groups:
        grp = wmo.Group(game.read('%s_%03d.wmo' % (name[:-4], g)))
        V = np.array(grp.verts)
        T = np.array(grp.indices).reshape(-1, 3)
        # sample every triangle densely enough for 1-yard cells
        for a, b, c in T:
            p, q, r = V[a], V[b], V[c]
            n = int(max(np.linalg.norm(q - p), np.linalg.norm(r - p), np.linalg.norm(r - q)) / 0.7) + 1
            for i in range(n + 1):
                for j in range(n + 1 - i):
                    pts.append(p + (q - p) * i / n + (r - p) * j / n)
        for k in range(grp.portal_start, grp.portal_start + grp.portal_count):
            pp, other, sd, _ = root.portal_refs[k]
            if pp == door:
                side = sd
    nrm = np.array(root.portals[door][2:5])
    s, c = root.portals[door][:2]
    dv = np.array(root.portal_verts[s:s + c])
    dc = dv.mean(0)
    fwd = side * nrm            # into the room
    fwd[2] = 0
    fwd /= np.linalg.norm(fwd)
    left = np.array([-fwd[1], fwd[0], 0.0])
    P = np.array(pts) - dc
    z0 = dv[:, 2].min()
    d = np.floor(P @ fwd).astype(int)
    l = np.floor(P @ left).astype(int)
    z = np.array(pts)[:, 2] - z0
    cells = {}
    for a, b, zz in zip(d, l, z):
        lo, hi = cells.get((a, b), (zz, zz))
        cells[(a, b)] = (min(lo, zz), max(hi, zz))
    return cells


def place(cells, slot, at, gap):
    """World cells {(x, y): (zmin, zmax)} of a room at a slot."""
    name, axis, face, out, rng, floor = slot
    fwd = (out, 0) if axis == 0 else (0, out)
    left = (-fwd[1], fwd[0])
    ox = (face + out * gap) if axis == 0 else at
    oy = (face + out * gap) if axis == 1 else at
    res = {}
    for (d, l), (lo, hi) in cells.items():
        for dd in (d, d + 0.99):
            for ll in (l, l + 0.99):
                x = math.floor(ox + fwd[0] * dd + left[0] * ll)
                y = math.floor(oy + fwd[1] * dd + left[1] * ll)
                a, b = res.get((x, y), (lo + floor, hi + floor))
                res[(x, y)] = (min(a, lo + floor), max(b, hi + floor))
    return res


def clash(a, b):
    if len(a) > len(b):
        a, b = b, a
    for k, (lo, hi) in a.items():
        o = b.get(k)
        if o and lo < o[1] + MARGIN_Z and o[0] < hi + MARGIN_Z:
            return True
    return False


def corridor_cells(slot, at, gap, width=5, height=6):
    name, axis, face, out, rng, floor = slot
    res = {}
    for t in np.arange(0, gap + 0.01, 0.5):
        for u in np.arange(-width / 2, width / 2 + 0.01, 0.5):
            x = face + out * t if axis == 0 else at + u
            y = face + out * t if axis == 1 else at + u
            res[(math.floor(x), math.floor(y))] = (floor, floor + height)
    return res


def solve(rooms, slots, hall_cells, fixed=None, gaps=(1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 14.0)):
    """rooms: [(label, cells)], largest first; depth-first, cheapest corridors first."""
    best = [None, 1e9]
    opts = {}
    for i, (label, cells) in enumerate(rooms):
        for si, slot in enumerate(slots):
            if fixed and label in fixed and slot[0] != fixed[label]:
                continue
            lo, hi = slot[4]
            ats = [lo + k for k in range(int(hi - lo) + 1)] or [lo]
            found = []
            for at in ats:
                for gap in gaps:
                    w = place(cells, slot, at, gap)
                    if clash(w, hall_cells):
                        continue
                    found.append((gap, at, w, corridor_cells(slot, at, gap)))
                    break
            opts[(i, si)] = sorted(found, key=lambda o: o[0])

    def rec(i, used, placed, cost):
        if cost >= best[1]:
            return
        if i == len(rooms):
            best[0], best[1] = list(placed), cost
            return
        for si, slot in enumerate(slots):
            if si in used or (i, si) not in opts:
                continue
            for gap, at, w, cor in opts[(i, si)]:
                if cost + gap >= best[1]:
                    break
                if any(clash(w, p[4]) or clash(w, p[5]) or clash(cor, p[4]) for p in placed):
                    continue
                placed.append((rooms[i][0], slot, at, gap, w, cor))
                rec(i + 1, used | {si}, placed, cost + gap)
                placed.pop()
    rec(0, frozenset(), [], 0.0)
    return best


def hall_occupancy():
    name = 'World/wmo/Azeroth/Buildings/Stormwind/AZ_PvPbarracks.wmo'
    g = wmo.Group(game.read(name[:-4] + '_000.wmo'))
    V = np.array(g.verts)
    res = {}
    xs, ys = np.arange(math.floor(V[:, 0].min()), math.ceil(V[:, 0].max())), np.arange(math.floor(V[:, 1].min()), math.ceil(V[:, 1].max()))
    for x in xs:
        for y in ys:
            res[(int(x), int(y))] = (float(V[:, 2].min()), float(V[:, 2].max()))
    return res
