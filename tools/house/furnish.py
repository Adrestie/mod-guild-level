"""Spots for the guild house's NPCs and objects, room by room: along the walls of each room's main
floor (walkable cells of the server's navmesh, reachable from the entrance), evenly spread, facing
the middle, away from the doorway. Writes SQL rows and a picture per room."""
import json
import math
import pickle

import numpy as np
from PIL import Image, ImageDraw

BOXES = pickle.load(open('alliance_bboxes.pkl', 'rb'))      # wmo coords
REACH = [tuple(map(float, s.split(','))) for s in json.load(open('../newnav/room_449.json'))]


def world_box(g):
    x0, y0, z0, x1, y1, z1 = BOXES[g]
    return -x1, -y1, z0, -x0, -y0, z1


def room_cells(groups, door=None, inward=None, level=None):
    """Walkable cells of a room. Rooms' boxes may overlap in plan: with a door (world x, y) and the
    way in (unit vector), only the cells reachable from just inside that door are kept."""
    nodes = set()
    for x, y, z in REACH:
        cx, cy = x + 0.5, y + 0.5
        for g in groups:
            a = world_box(g)
            if a[0] <= cx <= a[3] and a[1] <= cy <= a[4] and a[2] - 0.5 <= z <= a[5]:
                nodes.add((int(x), int(y), z))
                break
    if door is not None:
        sx, sy = door[0] + inward[0] * 2.5, door[1] + inward[1] * 2.5
        start = min(nodes, key=lambda n: (n[0] + 0.5 - sx) ** 2 + (n[1] + 0.5 - sy) ** 2)
        by = {}
        for n in nodes:
            by.setdefault(n[:2], []).append(n[2])
        seen = {start}
        todo = [start]
        while todo:
            x, y, z = todo.pop()
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    for nz in by.get((x + dx, y + dy), []):
                        n = (x + dx, y + dy, nz)
                        if n not in seen and abs(nz - z) <= 1.0:
                            seen.add(n)
                            todo.append(n)
        nodes = seen
    cells = {}
    for x, y, z in nodes:
        # nothing in the corridor, or on the hall's side of the door
        if door is not None and (x + 0.5 - door[0]) * inward[0] + (y + 0.5 - door[1]) * inward[1] < 1.0:
            continue
        cells.setdefault((x, y), []).append(z)
    levels = {}
    for k, zs in cells.items():
        for z in zs:
            levels[round(z)] = levels.get(round(z), 0) + 1
    if level is None:
        level = max(levels, key=levels.get)
    floor = {k: min(zs, key=lambda z: abs(z - level)) for k, zs in cells.items()
             if any(abs(z - level) < 0.8 for z in zs)}
    return floor


def spots(floor, n, door, keep_off=4.5, spacing=2.2, inset=1):
    """n spots along the walls: cells `inset` steps in from the edge."""
    keys = set(floor)
    # distance to the edge, in cells
    dist = {}
    frontier = [k for k in keys if any((k[0] + dx, k[1] + dy) not in keys
                                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))]
    for k in frontier:
        dist[k] = 0
    d = 0
    while frontier:
        d += 1
        nxt = []
        for x, y in frontier:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                k = (x + dx, y + dy)
                if k in keys and k not in dist:
                    dist[k] = d
                    nxt.append(k)
        frontier = nxt
    ring = [k for k in keys if dist.get(k) == inset and math.hypot(k[0] + 0.5 - door[0], k[1] + 0.5 - door[1]) > keep_off]
    cx = np.mean([k[0] + 0.5 for k in keys])
    cy = np.mean([k[1] + 0.5 for k in keys])
    ring.sort(key=lambda k: math.atan2(k[1] + 0.5 - cy, k[0] + 0.5 - cx))
    # the ring's length, then n points spread along it, each at least `spacing` from the others
    if len(ring) < n:
        raise ValueError('room too small: %d places for %d' % (len(ring), n))
    step = len(ring) / n
    out = []
    start = 0
    best = None
    for off in range(int(step) + 1):
        pts = [ring[int(off + i * step) % len(ring)] for i in range(n)]
        mind = min(math.hypot(a[0] - b[0], a[1] - b[1]) for i, a in enumerate(pts) for b in pts[i + 1:]) if n > 1 else 9
        if best is None or mind > best[0]:
            best = (mind, pts)
    for k in best[1]:
        x, y = k[0] + 0.5, k[1] + 0.5
        o = math.atan2(cy - y, cx - x) % (2 * math.pi)
        out.append((x, y, floor[k], o))
    return out, (cx, cy), best[0]


def picture(name, floor, placed, door):
    xs = [k[0] for k in floor]; ys = [k[1] for k in floor]
    x0, x1, y0, y1 = min(xs) - 2, max(xs) + 3, min(ys) - 2, max(ys) + 3
    S = 12
    img = Image.new('RGB', ((y1 - y0) * S, (x1 - x0) * S), (20, 20, 20))
    dr = ImageDraw.Draw(img)
    for (x, y) in floor:
        u, v = (y1 - y) * S, (x1 - x) * S
        dr.rectangle([u - S, v - S, u - 1, v - 1], fill=(70, 90, 110))
    u, v = (y1 - door[1]) * S, (x1 - door[0]) * S
    dr.ellipse([u - 6, v - 6, u + 6, v + 6], outline=(255, 0, 0), width=2)
    for x, y, z, o, label, kind in placed:
        u, v = (y1 - y) * S, (x1 - x) * S
        col = (255, 200, 0) if kind == 0 else (120, 255, 120)
        dr.ellipse([u - 5, v - 5, u + 5, v + 5], fill=col)
        dr.text((u + 6, v - 6), label[:14], fill=(255, 255, 255))
    img.save('furnish_%s.png' % name)
