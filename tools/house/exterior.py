"""Makes a built hall's groups exterior (indoor flag 0x2000 off, exterior 0x8 on), in the root's group
list (MOGI) and in each group's header (MOGP): the client then draws, from inside the hall, what
stands on the terrain under it -- the Customization editor's previews (tools/house/terrain.py). Its
light is then the map's (tools/house/light.py). Run on data/art once a hall is built.

    python exterior.py alliance|horde
"""
import glob
import os
import struct
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
HOUSE = os.path.join(ROOT, 'data', 'art', 'World', 'GuildLevel', 'House')
NAMES = {'alliance': 'GuildHall_Alliance', 'horde': 'GuildHall_Horde'}


def exterior(flags):
    return (flags & ~0x2000) | 0x8


def main(side):
    name = NAMES[side]
    root = bytearray(open(os.path.join(HOUSE, name + '.wmo'), 'rb').read())
    i = root.find(b'IGOM')
    n = struct.unpack_from('<I', root, i + 4)[0]
    for k in range(n // 32):
        o = i + 8 + k * 32
        struct.pack_into('<I', root, o, exterior(struct.unpack_from('<I', root, o)[0]))
    open(os.path.join(HOUSE, name + '.wmo'), 'wb').write(bytes(root))
    groups = sorted(glob.glob(os.path.join(HOUSE, name + '_[0-9][0-9][0-9].wmo')))
    for p in groups:
        d = bytearray(open(p, 'rb').read())
        o = d.find(b'PGOM') + 16
        struct.pack_into('<I', d, o, exterior(struct.unpack_from('<I', d, o)[0]))
        open(p, 'wb').write(bytes(d))
    print(side, n // 32, 'groups listed,', len(groups), 'group files made exterior')


if __name__ == '__main__':
    main(sys.argv[1])
