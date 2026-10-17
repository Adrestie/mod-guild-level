"""The Alliance guild hall: Champions' Hall plus five rooms."""
import pickle
import sys

import build

HALL = 'World/wmo/Azeroth/Buildings/Stormwind/AZ_PvPbarracks.wmo'
OUT = 'World\\wmo\\GuildHouse\\GuildHall_Alliance.wmo'
B = 'World/wmo/Azeroth/Buildings/'
ATT = [
    # the inn, whole, through its front door in the back wall
    dict(key='inn', wmo=B + 'GoldshireInn/GoldshireInn.wmo', groups=[5, 0, 1, 2, 3, 6, 7, 8, 9, 10], door=0, door_group=0,
         wall=(1, -38.1, -1), at=7.0, floor=1.06, gap=1.5),
    # Blizzard's unused guild house, entered by its upper door from the hall's mezzanine
    dict(key='guildhouseA', wmo=B + 'GuildHouses/GuildHouseA.wmo', groups=[0, 2, 3, 4], door=1, door_group=0,
         wall=(1, 8.72, 1), at=-1.0, floor=5.49, gap=5.0),
    # the other one, entered by its upper door
    dict(key='guildhouseB', wmo=B + 'GuildHouses/GuildHouseB.wmo', groups=[2, 1], door=1, door_group=2,
         wall=(0, 10.6, 1), at=-30.0, floor=1.06, gap=1.5),
    dict(key='townhall', wmo=B + 'TownHall/TownHall.wmo', groups=[0], door=3, sets=('Set_$DefaultGlobal', 'Set_A'),
         wall=(0, -9.6, -1), at=-12.0, floor=-0.26, gap=1.5),
    dict(key='barracks', wmo=B + 'Human_Barracks/Human_Barracks.wmo', groups=[1], door=28,
         sets=('Set_$DefaultGlobal', 'set_Barracks01'), wall=(0, -9.6, -1), at=-35.0, floor=1.06, gap=3.0, max_h=4.4),
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
