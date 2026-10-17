"""The Horde guild hall: the Hall of Legends plus five Orc rooms around its octagon."""
import math
import pickle

import numpy as np

import build

HALL = 'World/wmo/Kalimdor/Ogrimmar/KL_PvPBarracks.wmo'
OUT = 'World\\wmo\\GuildHouse\\GuildHall_Horde.wmo'
K = 'World/wmo/Kalimdor/Buildings/'
C = np.array([-243.25, -84.9])          # the octagon's centre (WMO coordinates)
FACE_R = 24.0                           # its walls' distance from the centre
FLOOR = 25.72
TRIM = 14                               # MM_OGRMR_INT_TRIM_01, the hall's wood
WALL = 27                               # MM_OGRMR_INT_WALL_06


def side(deg):
    """The wall of the octagon whose outward normal points at deg: (wall spec, the door's middle)."""
    t = math.radians(deg)
    n = np.array([math.cos(t), math.sin(t)])
    a = np.array([-n[1], n[0]])
    return ('angle', deg, float(C @ n) + FACE_R), float(C @ a)


def att(key, wmo_name, groups, door, deg, gap, **kw):
    spec, at = side(deg)
    return dict(key=key, wmo=wmo_name, groups=groups, door=door, wall=spec, at=at, floor=FLOOR, gap=gap,
                trim_material=TRIM, auto_floor=True, cut_in=3.0, floor_back=1.5, floor_fwd=1.5, **kw)


FORTRESS = [2, 0] + list(range(3, 21))
ATT = [
    # the orc fortress, the largest, through its gate
    att('fortress', K + 'OrcFortress/OrcFortress.wmo', FORTRESS, 2, 91.5, 4.0, door_group=2,
        sets=('Set_$DefaultGlobal', 'set_a'), max_h=6.0),
    # the barracks' arena, through its gate
    att('barracks', K + 'OrcBarracks/OrcBarracks.wmo', [1, 2, 3], 3, -88.5, 3.0, door_group=1,
        sets=('Set_$DefaultGlobal', 'set_a'), max_w=5.0, max_h=7.0),
    # the great hall and its upper floor, through its front door
    att('greathall', K + 'OrcGreatHall/OrcGreatHall.wmo', [0, 1, 2], 0, -178.5, 6.0, door_group=0,
        sets=('Set_$DefaultGlobal', 'set_a')),
    # the inn and its upper floor
    att('inn', K + 'OrcInn/OrcInn.wmo', [0, 1], 0, -133.5, 3.0, door_group=0),
    # an Orgrimmar house of the Valley of Strength (in the hall's own WMO), three floors
    att('house', HALL, [1, 2, 3, 4, 5], 3, -43.5, 10.0, door_group=1),
]


def make():
    return build.build(HALL, [13, 12, 11], ATT, seal_material=WALL)


if __name__ == '__main__':
    b, res = make()
    for a, (ids, removed) in zip(ATT, res):
        print(a['key'], 'groups', ids[0], '..', ids[-1], 'cut', {k: round(float(v), 1) for k, v in removed.items()})
    files = b.files(OUT)
    pickle.dump(files, open('horde_files.pkl', 'wb'))
    print('materials', len(b.root.materials), 'doodads', len(b.root.doodads), 'groups', len(b.groups),
          'portals', len(b.root.portals), 'bbox', [round(x, 1) for x in b.root.bbox])
