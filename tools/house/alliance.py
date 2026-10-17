"""The Alliance guild hall: Champions' Hall plus five rooms."""
import pickle
import sys

import build

HALL = 'World/wmo/Azeroth/Buildings/Stormwind/AZ_PvPbarracks.wmo'
OUT = 'World\\wmo\\GuildHouse\\GuildHall_Alliance.wmo'
B = 'World/wmo/Azeroth/Buildings/'
# the barracks without its front-left wing, which would cross the hall: entered by its upstairs hall
BARRACKS = [1, 0, 2, 5, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 23, 24, 26, 27]
ATT = [
    dict(key='barracks', wmo=B + 'Human_Barracks/Human_Barracks.wmo', groups=BARRACKS, door=28, door_group=1,
         sets=('Set_$DefaultGlobal', 'set_Barracks01'), wall=(0, 10.6, 1), at=-35.0, floor=1.06, gap=1.5, max_h=4.4),
    # the whole inn, through its front door
    dict(key='inn', wmo=B + 'GoldshireInn/GoldshireInn.wmo', groups=[5, 0, 1, 2, 3, 6, 7, 8, 9, 10], door=0, door_group=0,
         wall=(0, -9.6, -1), at=-8.0, floor=-0.26, gap=1.5),
    # Blizzard's unused guild houses, both entered by their upper door
    dict(key='guildhouseB', wmo=B + 'GuildHouses/GuildHouseB.wmo', groups=[2, 1], door=1, door_group=2,
         wall=(0, -9.6, -1), at=-27.0, floor=1.06, gap=1.5),
    dict(key='guildhouseA', wmo=B + 'GuildHouses/GuildHouseA.wmo', groups=[0, 2, 3, 4], door=1, door_group=0,
         wall=(1, 8.72, 1), at=-6.0, floor=5.49, gap=12.0),
    dict(key='townhall', wmo=B + 'TownHall/TownHall.wmo', groups=[0], door=3, sets=('Set_$DefaultGlobal', 'Set_A'),
         wall=(1, -38.1, -1), at=-2.0, floor=1.06, gap=1.0),
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
