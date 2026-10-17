"""Placing rooms around the hall: slots on its walls, rooms' footprints, no overlap."""
import itertools
import sys

import footprint

HALL_RECTS = [(-10.6, 11.6, -39.1, -1.3), (-2.7, 3.5, -2.3, 2.0)]   # interior + walls, vestibule
# slot: name, axis of the wall normal, wall face, outward sign, along range, floor z
SLOTS = [
    ('end', 1, -38.1, -1, (-6.0, 7.0), 1.06),
    ('mezz', 1, 8.72, 1, (-6.0, 7.0), 5.49),
    ('w_back', 0, -9.6, -1, (-36.0, -19.0), 1.06),
    ('w_front', 0, -9.6, -1, (-14.5, -4.5), -0.26),
    ('e_back', 0, 10.6, 1, (-36.0, -19.0), 1.06),
    ('e_front', 0, 10.6, 1, (-8.5, -4.5), -0.26),
    ('f_west', 1, -2.3, 1, (-8.0, -4.0), -0.26),
    ('f_east', 1, -2.3, 1, (5.0, 9.0), -0.26),
]


def rect(slot, fp, at, gap):
    name, axis, face, out, rng, z = slot
    d0, d1 = fp['depth']
    # hall-side view: left/right of the door seen from the hall looking out
    # looking along out*axis: left = rotate (+90deg)
    if axis == 0:
        fwd = (out, 0)
    else:
        fwd = (0, out)
    left = (-fwd[1], fwd[0])
    base = face + out * gap
    pts = []
    for depth in (d0, d1):
        for lat in (fp['left'], -fp['right']):
            x = (base if axis == 0 else at) + fwd[0] * depth + left[0] * lat
            y = (base if axis == 1 else at) + fwd[1] * depth + left[1] * lat
            pts.append((x, y))
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    return (min(xs), max(xs), min(ys), max(ys))


def overlap(a, b, margin=0.8):
    return a[0] < b[1] + margin and b[0] < a[1] + margin and a[2] < b[3] + margin and b[2] < a[3] + margin


def solve(rooms, fixed=None, weights=None):
    """rooms: [(label, footprint)]; returns the best [(label, slot, at, gap, rect)]."""
    best = [None, 1e9]
    slots = SLOTS

    def options(slot, fp):
        lo, hi = slot[4]
        ats = [lo + 1.0 * k for k in range(int((hi - lo) / 1.0) + 1)] or [lo]
        for at in ats:
            for gap in (1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 14.0):
                r = rect(slot, fp, at, gap)
                if any(overlap(r, h, 0.3) for h in HALL_RECTS):
                    continue
                yield at, gap, r

    opts = {}
    for i, (label, fp) in enumerate(rooms):
        for si, slot in enumerate(slots):
            if fixed and label in fixed and slot[0] != fixed[label]:
                continue
            opts[(i, si)] = sorted(options(slot, fp), key=lambda o: o[1])
    minrest = [0.0] * (len(rooms) + 1)
    for i in range(len(rooms) - 1, -1, -1):
        mins = [o[0][1] for (j, si), o in opts.items() if j == i and o]
        minrest[i] = minrest[i + 1] + (min(mins) if mins else 99)

    def rec(i, used, placed, cost):
        if cost + minrest[i] >= best[1]:
            return
        if i == len(rooms):
            best[0], best[1] = list(placed), cost
            return
        label, fp = rooms[i]
        for si, slot in enumerate(slots):
            if si in used or (i, si) not in opts:
                continue
            for at, gap, r in opts[(i, si)]:
                if cost + gap + minrest[i + 1] >= best[1]:
                    break
                if any(overlap(r, p[4]) for p in placed):
                    continue
                placed.append((label, slot, at, gap, r))
                rec(i + 1, used | {si}, placed, cost + gap)
                placed.pop()
    rec(0, frozenset(), [], 0.0)
    return best


if __name__ == '__main__':
    B = 'World/wmo/Azeroth/Buildings/'
    fps = {
        'inn': footprint.footprint(B + 'GoldshireInn/GoldshireInn.wmo', [5, 0, 1, 2, 3, 6, 7, 8, 9, 10], 0),
        'townhall': footprint.footprint(B + 'TownHall/TownHall.wmo', [0], 3),
        'barracks': footprint.footprint(B + 'Human_Barracks/Human_Barracks.wmo', [23, 0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 24, 25, 26, 27], 0),
        'guildhouseA': footprint.footprint(B + 'GuildHouses/GuildHouseA.wmo', [0, 2, 3, 4], 1),
        'guildhouseB0': footprint.footprint(B + 'GuildHouses/GuildHouseB.wmo', [2, 1], 0),
        'guildhouseB1': footprint.footprint(B + 'GuildHouses/GuildHouseB.wmo', [2, 1], 1),
    }
    for gb in ('guildhouseB1',):
        rooms = [(k, fps[k]) for k in ('guildhouseA', 'inn', gb, 'townhall', 'barracks')]
        sol, cost = solve(rooms, fixed={'guildhouseA': 'mezz'})
        print('==', gb, 'cost', cost)
        if sol:
            for label, slot, at, gap, r in sol:
                print('  %-12s %-8s at %6.1f gap %4.1f  x %6.1f..%6.1f  y %6.1f..%6.1f' % (label, slot[0], at, gap, *r))
