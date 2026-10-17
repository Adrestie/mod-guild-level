"""The Alliance guild hall's furniture by wing (rows 3..42 of mod_guild_level_house_spawn, map 449)."""
import numpy as np

import furnish

CLASSES = [(914, 'Warrior trainer'), (928, 'Paladin trainer'), (5515, 'Hunter trainer'), (918, 'Rogue trainer'),
           (376, 'Priest trainer'), (28474, 'Death Knight trainer'), (20407, 'Shaman trainer'), (331, 'Mage trainer'),
           (461, 'Warlock trainer'), (5504, 'Druid trainer'), (9977, 'Stable Master')]
CRAFTS = [(28703, 'Alchemy trainer'), (28694, 'Blacksmithing trainer'), (28693, 'Enchanting trainer'),
          (28697, 'Engineering trainer'), (28704, 'Herbalism trainer'), (28702, 'Inscription trainer'),
          (28701, 'Jewelcrafting trainer'), (28700, 'Leatherworking trainer'), (28698, 'Mining trainer'),
          (28696, 'Skinning trainer'), (28699, 'Tailoring trainer')]
PORTALS = [(176296, 'Portal to Stormwind'), (176497, 'Portal to Ironforge'), (176498, 'Portal to Darnassus'),
           (182351, 'Portal to Exodar'), (183384, 'Portal to Shattrath'), (191164, 'Portal to Dalaran'),
           (920002, 'Portal to Caverns of Time')]


def door_world(axis, face, out, at, gap):
    """The doorway's end in world coordinates (the world is the WMO turned half a turn), and the
    way into the room."""
    p = [0.0, 0.0]
    p[axis] = face + out * gap
    p[1 - axis] = at
    v = [0.0, 0.0]
    v[axis] = -out
    return (-p[0], -p[1]), tuple(v)


# wing: groups, the doorway (world), what stands there: [(paragon, kind, entry, label)]
WINGS = {
    # the town hall: class trainers and the stable master
    'townhall': dict(groups=[38], door_in=door_world(1, -38.1, -1, -2.0, 1.0),
                     items=[(5, 0, e, l) for e, l in CLASSES]),
    # the second guild house: the crafts, the forge and the anvil
    'guildhouseB': dict(groups=[32, 33], door_in=door_world(0, -9.6, -1, -27.0, 1.5),
                        items=[(6, 0, e, l) for e, l in CRAFTS] + [(6, 1, 1685, 'Forge'), (6, 1, 1744, 'Anvil')]),
    # the inn: cooking, first aid, fishing by the fire, and the auction house
    'inn': dict(groups=[22, 23, 24], door_in=door_world(0, -9.6, -1, -8.0, 1.5),
                items=[(6, 0, 28705, 'Cooking trainer'), (6, 0, 28706, 'First Aid trainer'),
                       (6, 0, 28742, 'Fishing trainer'), (6, 1, 1798, 'Cooking fire'),
                       (7, 0, 8670, 'Auctioneer'), (7, 0, 8719, 'Auctioneer')]),
    # the first guild house, upstairs: the portals
    'guildhouseA': dict(groups=[34], door_in=door_world(1, 8.72, 1, -6.0, 12.0), keep_off=14.0, spacing=3.0,
                        items=[(8, 1, e, l) for e, l in PORTALS]),
}

if __name__ == '__main__':
    rows = []
    rid = 6       # 3..5 stay in the entrance hall: supplier, banker, repairer
    for name, w in WINGS.items():
        door, inward = w['door_in']
        floor = furnish.room_cells(w['groups'], door, inward)
        pts, centre, mind = furnish.spots(floor, len(w['items']), door, keep_off=w.get('keep_off', 4.5),
                                          spacing=w.get('spacing', 2.2))
        placed = []
        for (paragon, kind, entry, label), (x, y, z, o) in zip(w['items'], pts):
            placed.append((x, y, z, o, label, kind))
            rows.append((rid, 449, paragon, kind, entry, x, y, z + 0.05, o, 'Paragon %d: %s' % (paragon, label)))
            rid += 1
        print('%-12s %2d spots, %d floor cells, closest two %.1f yd apart' % (name, len(pts), len(floor), mind))
        furnish.picture(name, floor, placed, door)
    with open('alliance_rows.sql', 'w', encoding='utf-8') as f:
        for r in rows:
            f.write("(%d, %d, %d, %d, %d, %.2f, %.2f, %.2f, %.3f, '%s'),\n" % r)
    print(len(rows), 'rows, ids 6..%d' % (rid - 1))
