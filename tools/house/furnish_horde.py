"""The Horde guild hall's furniture by wing: rows 46..82 of mod_guild_level_house_spawn (map 450),
same ids and entries, new spots. The fortress, the largest wing, stays empty."""
import json
import math
import pickle
import re

import numpy as np

import build
import furnish
import horde
import wmo

files = pickle.load(open('horde_files.pkl', 'rb'))
root = wmo.Root(files[horde.OUT])
furnish.BOXES = [i[1:7] for i in root.group_infos]
furnish.STEP = 2.5                  # the navmesh's stairs, a cell at a time
furnish.REACH = [tuple(map(float, s.split(','))) for s in json.load(open('../newnav/room_450.json'))]


def door_of(key):
    a = next(x for x in horde.ATT if x['key'] == key)
    W = build.Wall.of(a['wall'])
    p = W.P(a['at'], 0.0, W.face + a['gap'])
    return (-p[0], -p[1]), (-W.n[0], -W.n[1])       # the world is the WMO turned half a turn


ROWS = {}
for line in open(r'E:\WoW-mods\mod-guild-level\data\sql\world\guild_level_08_house_layout.sql', encoding='utf-8'):
    m = re.match(r"\((\d+), 450, (\d+), (\d+), (\d+), [-\d.]+, [-\d.]+, [-\d.]+, [-\d.]+, ('[^']*')\)", line)
    if m:
        ROWS[int(m.group(1))] = (int(m.group(2)), int(m.group(3)), int(m.group(4)), m.group(5))

WINGS = {
    'barracks': dict(groups=[23, 24, 25], ids=list(range(46, 57))),
    'greathall': dict(groups=[26, 27, 28], ids=list(range(57, 68)) + [71, 72]),
    'inn': dict(groups=[29, 30], ids=[68, 69, 70, 73, 74, 75]),
    'house': dict(groups=[31, 32, 33, 34, 35], ids=list(range(76, 83)), spacing=3.0, keep_off=0.0, level=-3),   # its cellar
}

if __name__ == '__main__':
    out = []
    for name, w in WINGS.items():
        door, inward = door_of(name)
        floor = furnish.room_cells(w['groups'], door, inward, level=w.get('level'), door_z=horde.FLOOR)
        pts, centre, mind = furnish.spots(floor, len(w['ids']), door, keep_off=w.get('keep_off', 4.5),
                                          spacing=w.get('spacing', 2.2))
        placed = []
        for rid, (x, y, z, o) in zip(w['ids'], pts):
            paragon, kind, entry, label = ROWS[rid]
            out.append((rid, 450, paragon, kind, entry, x, y, z + 0.05, o, label))
            placed.append((x, y, z, o, label.strip("'").split(': ')[-1], kind))
        print('%-10s %2d spots, %d floor cells, closest two %.1f yd apart' % (name, len(pts), len(floor), mind))
        furnish.picture('h_' + name, floor, placed, door)
    out.sort()
    with open('horde_rows.sql', 'w', encoding='utf-8') as f:
        for r in out:
            f.write("(%d, %d, %d, %d, %d, %.2f, %.2f, %.2f, %.3f, %s),\n" % r)
    print(len(out), 'rows')
