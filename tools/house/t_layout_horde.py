import math
import pickle
import time

import numpy as np

import build
import game
import layout3d
import layout3d_angle
import wmo

K = 'World/wmo/Kalimdor/Buildings/'
HALL = 'World/wmo/Kalimdor/Ogrimmar/KL_PvPBarracks.wmo'
C = np.array([-243.25, -84.9])
FACE_R = 24.0
FLOOR = 25.72


def side(deg):
    t = math.radians(deg)
    n = np.array([math.cos(t), math.sin(t)])
    face = float(C @ n) + FACE_R
    a = np.array([-n[1], n[0]])
    at = float(C @ a)
    return ('s%d' % round(deg), ('angle', deg, face), (at - 2, at + 2), FLOOR)


SLOTS = [side(d) for d in (-178.5, -133.5, -88.5, -43.5, 91.5, 136.5)]


def hall_occupancy():
    res = {}
    for x in range(-275, -210):
        for y in range(-117, -52):
            if math.hypot(x + 0.5 - C[0], y + 0.5 - C[1]) < FACE_R + 2.5:
                res[(x, y)] = (24.0, 67.0)
    for g in (11, 12):
        G = wmo.Group(game.read('%s_%03d.wmo' % (HALL[:-4], g)))
        b = G.bbox
        for x in range(math.floor(b[0]), math.ceil(b[3])):
            for y in range(math.floor(b[1]), math.ceil(b[4])):
                res[(x, y)] = (b[2], b[5])
    return res


if __name__ == '__main__':
    FORT = [2, 0] + list(range(3, 21))
    t = time.time()
    rooms = [
        ('fortress', layout3d.occupancy(K + 'OrcFortress/OrcFortress.wmo', FORT, 2)),
        ('barracks', layout3d.occupancy(K + 'OrcBarracks/OrcBarracks.wmo', [1, 2, 3], 3)),
        ('greathall', layout3d.occupancy(K + 'OrcGreatHall/OrcGreatHall.wmo', [0, 1, 2], 0)),
        ('inn', layout3d.occupancy(K + 'OrcInn/OrcInn.wmo', [0, 1], 0)),
        ('house', layout3d.occupancy(HALL, [1, 2, 3, 4, 5], 3)),
    ]
    print('occupancy', round(time.time() - t, 1), [len(c) for _, c in rooms])
    sol, cost = layout3d_angle.solve(rooms, SLOTS, hall_occupancy())
    print('cost', cost, round(time.time() - t, 1))
    if sol:
        for label, slot, at, gap, w, cor in sol:
            print('  %-10s %-6s at %7.2f gap %4.1f  cells %d' % (label, slot[0], at, gap, len(w)))
        pickle.dump([(s[0], s[1], s[2], s[3]) for s in sol], open('layout_horde.pkl', 'wb'))
