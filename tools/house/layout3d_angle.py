"""Angled-wall slots for layout3d: ('name', ('angle', degrees, face), (at_lo, at_hi), floor)."""
import math

import numpy as np

import build
import layout3d

_place = layout3d.place
_corridor = layout3d.corridor_cells


def is_angle(slot):
    return len(slot) == 4


def place(cells, slot, at, gap):
    if not is_angle(slot):
        return _place(cells, slot, at, gap)
    name, spec, rng, floor = slot
    W = build.Wall.of(spec)
    res = {}
    for (d, l), (lo, hi) in cells.items():
        for dd in (d, d + 0.99):
            for ll in (l, l + 0.99):
                p = W.P(at + ll, 0.0, W.face + gap + dd)
                k = (math.floor(p[0]), math.floor(p[1]))
                a, b = res.get(k, (lo + floor, hi + floor))
                res[k] = (min(a, lo + floor), max(b, hi + floor))
    return res


def corridor_cells(slot, at, gap, width=5, height=6):
    if not is_angle(slot):
        return _corridor(slot, at, gap, width, height)
    name, spec, rng, floor = slot
    W = build.Wall.of(spec)
    res = {}
    for t in np.arange(0, gap + 0.01, 0.5):
        for u in np.arange(-width / 2, width / 2 + 0.01, 0.5):
            p = W.P(at + u, 0.0, W.face + t)
            res[(math.floor(p[0]), math.floor(p[1]))] = (floor, floor + height)
    return res


def install():
    """Makes layout3d.solve use angled slots (its range is read from slot[-2])."""
    layout3d.place = place
    layout3d.corridor_cells = corridor_cells


def solve(rooms, slots, hall_cells, fixed=None, gaps=(1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 14.0)):
    """layout3d.solve, with a slot's range and name wherever they sit."""
    best = [None, 1e9]
    opts = {}
    for i, (label, cells) in enumerate(rooms):
        for si, slot in enumerate(slots):
            if fixed and label in fixed and slot[0] != fixed[label]:
                continue
            lo, hi = slot[-2]
            ats = [lo + k for k in range(int(hi - lo) + 1)] or [lo]
            found = []
            for at in ats:
                for gap in gaps:
                    w = place(cells, slot, at, gap)
                    if layout3d.clash(w, hall_cells):
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
                if any(layout3d.clash(w, p[4]) or layout3d.clash(w, p[5]) or layout3d.clash(cor, p[4]) for p in placed):
                    continue
                placed.append((rooms[i][0], slot, at, gap, w, cor))
                rec(i + 1, used | {si}, placed, cost + gap)
                placed.pop()
    rec(0, frozenset(), [], 0.0)
    return best
