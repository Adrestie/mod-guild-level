import pickle
import time

import layout3d

B = 'World/wmo/Azeroth/Buildings/'
SLOTS = [
    ('end', 1, -38.1, -1, (-6.0, 7.0), 1.06),
    ('mezz', 1, 8.72, 1, (-6.0, 7.0), 5.49),
    ('w_back', 0, -9.6, -1, (-35.0, -20.0), 1.06),
    ('w_front', 0, -9.6, -1, (-14.0, -5.0), -0.26),
    ('e_back', 0, 10.6, 1, (-35.0, -20.0), 1.06),

]
BAR = [23, 0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 24, 25, 26, 27]
t = time.time()
rooms = [
    ('barracks', layout3d.occupancy(B + 'Human_Barracks/Human_Barracks.wmo', BAR, 0)),
    ('inn', layout3d.occupancy(B + 'GoldshireInn/GoldshireInn.wmo', [5, 0, 1, 2, 3, 6, 7, 8, 9, 10], 0)),
    ('guildhouseB', layout3d.occupancy(B + 'GuildHouses/GuildHouseB.wmo', [2, 1], 1)),
    ('guildhouseA', layout3d.occupancy(B + 'GuildHouses/GuildHouseA.wmo', [0, 2, 3, 4], 1)),
    ('townhall', layout3d.occupancy(B + 'TownHall/TownHall.wmo', [0], 3)),
]
print('occupancy', round(time.time() - t, 1), [len(c) for _, c in rooms])
hall = layout3d.hall_occupancy()
sol, cost = layout3d.solve(rooms, SLOTS, hall, fixed={'guildhouseA': 'mezz'})
print('cost', cost, round(time.time() - t, 1))
if sol:
    for label, slot, at, gap, w, cor in sol:
        xs = [k[0] for k in w]; ys = [k[1] for k in w]
        print('  %-12s %-8s at %6.1f gap %4.1f  x %4d..%4d  y %4d..%4d' % (label, slot[0], at, gap, min(xs), max(xs), min(ys), max(ys)))
    pickle.dump([(s[0], s[1], s[2], s[3]) for s in sol], open('layout_sol.pkl', 'wb'))
