"""The Alliance guild hall: Champions' Hall plus five rooms."""
import pickle
import sys

import build

HALL = 'World/wmo/Azeroth/Buildings/Stormwind/AZ_PvPbarracks.wmo'
OUT = 'World\\wmo\\GuildHouse\\GuildHall_Alliance.wmo'
B = 'World/wmo/Azeroth/Buildings/'
ATT = [
    # the great hall, straight through the back wall
    dict(key='cathedral', wmo='World/wmo/Northrend/Dragonblight/NewHearthglen/NH_Cathedral.wmo', groups=[0], door=2,
         wall=(1, -38.1, -1), at=0.45, floor=1.06, gap=2.0, max_h=5.2),
    # through the entrance
    dict(key='guildhouse', wmo=B + 'GuildHouses/GuildHouseA.wmo', groups=[2, 3, 4, 0], door=0, door_group=2,
         wall=(1, 1.99, 1), at=0.42, floor=-0.26, gap=2.0, through=True),
    dict(key='townhall', wmo=B + 'TownHall/TownHall.wmo', groups=[0], door=3, sets=('Set_$DefaultGlobal', 'Set_A'),
         wall=(0, -9.6, -1), at=-6.5, floor=-0.26, gap=1.5),
    dict(key='barracks', wmo=B + 'Human_Barracks/Human_Barracks.wmo', groups=[1], door=28,
         sets=('Set_$DefaultGlobal', 'set_Barracks01'), wall=(0, 10.6, 1), at=-33.0, floor=1.06, gap=1.5, max_h=4.4),
    dict(key='chapel', wmo=B + 'Chapel/RedridgeChapel.wmo', groups=[0], door=0, sets=('set_Chapel',),
         wall=(0, -9.6, -1), at=-30.0, floor=1.06, gap=1.5),
]


def make():
    b, res = build.build(HALL, [0], ATT, seal_material=0)
    return b, res


if __name__ == '__main__':
    b, res = make()
    for att, (ids, removed) in zip(ATT, res):
        print(att['key'], 'groups', ids, 'cut', {k: round(float(v), 1) for k, v in removed.items()})
    files = b.files(OUT)
    pickle.dump(files, open('alliance_files.pkl', 'wb'))
    print('materials', len(b.root.materials), 'doodads', len(b.root.doodads), 'lights', len(b.root.lights),
          'groups', len(b.groups), 'portals', len(b.root.portals), 'bbox', [round(x, 1) for x in b.root.bbox])
    for g, grp in enumerate(b.groups):
        print('  g%d %-20s verts %5d tris %5d bsp %4d portals %d' % (g, b.root.group_name(g), len(grp.verts), len(grp.indices) // 3,
                                                                  len(grp.bsp_nodes), grp.portal_count))
