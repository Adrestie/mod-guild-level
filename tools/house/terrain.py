"""A flat terrain under a guild hall, deep below its floors, so that the hall's map is a terrain map
with the hall placed on it rather than a map of one building: the Customization editor attaches its
local preview models to terrain chunks (the way wxl-housing places its decor), which a map of one
building has none of. Client files only: the server keeps the hall's collision and navigation.

Writes into data/art/World/Maps/<dir>: the map's WDT (its tiles listed, no building of its own) and
one ADT per tile the hall stands on, each placing the hall at the world's origin as the WDT did.

    python terrain.py alliance|horde
"""
import os
import struct
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
TILE = 1600.0 / 3.0                 # yards per ADT
CHUNK = TILE / 16.0
HALF = 32 * TILE                    # 17066.66: the placement space's origin, in world yards
DEPTH = 40.0                        # yards under the hall's lowest point
TEMPLATE = 'World\\Maps\\Azeroth\\Azeroth_32_48.adt'     # the game's tile the hall's are made from

# area: the hall's AreaTable row, written in the chunks
SIDES = {
    'alliance': dict(dir='AlliancePVPBarracks', wmo='World\\GuildLevel\\House\\GuildHall_Alliance.wmo', area=2918),
    'horde': dict(dir='HordePVPBarracks', wmo='World\\GuildLevel\\House\\GuildHall_Horde.wmo', area=0),
}
UNIQUE_ID = 1                       # the hall's one placement, the same in every ADT (drawn once)


def chunk(tag, body):
    return tag[::-1] + struct.pack('<I', len(body)) + body


def read_chunks(data):
    i = 0
    while i + 8 <= len(data):
        tag = data[i:i + 4][::-1]
        size = struct.unpack_from('<I', data, i + 4)[0]
        yield tag, data[i + 8:i + 8 + size]
        i += 8 + size


def hall_bbox(wdt):
    """The hall's world box from the WDT's placement: (x0, y0, z0, x1, y1, z1)."""
    for tag, body in read_chunks(wdt):
        if tag == b'MODF':
            y0, z0, x0, y1, z1, x1 = struct.unpack_from('<6f', body, 32)
            return x0, y0, z0, x1, y1, z1
    raise SystemExit('the WDT places no building')


def tiles_of(bbox):
    """ADT (x, y) of every tile the hall's box touches. ADT x runs against the world's y, ADT y
    against its x."""
    x0, y0, _, x1, y1, _ = bbox
    tx = range(int((HALF - y1) // TILE), int((HALF - y0) // TILE) + 1)
    ty = range(int((HALF - x1) // TILE), int((HALF - x0) // TILE) + 1)
    return [(a, b) for a in tx for b in ty]


def template():
    """The game's own tile the hall's tiles are made from: every block as the game writes it."""
    import game
    data = game.read(TEMPLATE)
    if data is None:
        raise SystemExit(TEMPLATE + ' not in the game')
    return data


def template_chunks(data):
    """Its 256 MCNK, in MCIN's order: (header, {sub-chunk tag: (bytes, offset in the MCNK)})."""
    mcin = next(b for t, b in read_chunks(data) if t == b'MCIN')
    out = []
    for k in range(256):
        at = struct.unpack_from('<I', mcin, k * 16)[0]
        size = struct.unpack_from('<I', data, at + 4)[0]
        body = data[at:at + 8 + size]
        out.append(body)
    return out


# MCNK header fields (in uint32 slots) of the sub-chunks it points at
OFS = {'MCVT': 5, 'MCNR': 6, 'MCLY': 7, 'MCRF': 8, 'MCAL': 9, 'MCSH': 11}


def mcnk(src, ix, iy, tx, ty, height, area):
    """One chunk of the tile from the template's: flat at height, no doodad, the hall referenced
    (MCRF), no liquid; its layers, alpha and shadow maps as the game's."""
    head = bytearray(src[8:136])
    f = list(struct.unpack_from('<15I', head))
    subs = {}
    for name, slot in OFS.items():
        o = f[slot]
        if not o or src[o:o + 4][::-1] != name.encode():
            continue
        size = struct.unpack_from('<I', src, o + 4)[0]
        subs[name] = src[o:o + 8 + size]
    subs['MCVT'] = chunk(b'MCVT', struct.pack('<145f', *([0.0] * 145)))
    # the normals keep their 13 bytes of padding after the chunk, outside its size, as the game's
    subs['MCNR'] = chunk(b'MCNR', bytes([0, 127, 0]) * 145) + bytes(13)
    subs['MCRF'] = chunk(b'MCRF', struct.pack('<I', 0))
    pos = 136
    body = b''
    for name in ('MCVT', 'MCNR', 'MCLY', 'MCRF', 'MCSH', 'MCAL'):
        if name not in subs:
            f[OFS[name]] = 0
            continue
        f[OFS[name]] = pos
        body += subs[name]
        pos += len(subs[name])
    f[0] &= ~(0x4 | 0x8 | 0x10 | 0x20)              # no liquid (MCLQ)
    f[1], f[2] = ix, iy
    f[4] = 0                                        # doodads
    f[13] = area
    f[14] = 1                                       # the hall
    struct.pack_into('<15I', head, 0, *f)
    struct.pack_into('<2I', head, 96, 0, 0)         # liquid
    struct.pack_into('<2I', head, 88, pos - 8, 0)   # no sound emitter: the chunk's size, as the game's
    wx = HALF - ty * TILE - iy * CHUNK
    wy = HALF - tx * TILE - ix * CHUNK
    struct.pack_into('<3f', head, 104, wx, wy, height)
    struct.pack_into('<2I', head, 116, 0, 0)        # MCCV, MCLV
    return chunk(b'MCNK', bytes(head) + body)


def adt(tx, ty, wmo_name, bbox, src, area):
    x0, y0, z0, x1, y1, z1 = bbox
    height = z0 - DEPTH
    mver = chunk(b'MVER', struct.pack('<I', 18))
    mtex = chunk(b'MTEX', next(b for t, b in read_chunks(src) if t == b'MTEX'))
    mmdx = chunk(b'MMDX', b'')
    mmid = chunk(b'MMID', b'')
    mwmo = chunk(b'MWMO', wmo_name.encode('latin1') + b'\0')
    mwid = chunk(b'MWID', struct.pack('<I', 0))
    mddf = chunk(b'MDDF', b'')
    # placement space: x = HALF - world y, y = world z, z = HALF - world x; the hall at the world's
    # origin, unturned, as the WDT placed it (the culling box: wide enough whichever way the client
    # reads the hall's axes)
    ry, rx = max(abs(y0), abs(y1)), max(abs(x0), abs(x1))
    lower = (HALF - ry, z0, HALF - rx)
    upper = (HALF + ry, z1, HALF + rx)
    modf = chunk(b'MODF', struct.pack('<2I3f3f3f3f4H', 0, UNIQUE_ID, HALF, 0.0, HALF, 0.0, 0.0, 0.0,
                                      *lower, *upper, 0, 0, 0, 0))
    body = [mtex, mmdx, mmid, mwmo, mwid, mddf, modf]
    start = len(mver)                               # MHDR's chunk
    data_start = start + 8                          # MHDR offsets count from its data
    pos = start + 8 + 64
    mcin_at = pos
    pos += 8 + 256 * 16
    at = []
    for c in body:
        at.append(pos)
        pos += len(c)
    sources = template_chunks(src)
    cnks, entries = [], []
    for k in range(256):
        iy, ix = divmod(k, 16)
        c = mcnk(sources[k], ix, iy, tx, ty, height, area)
        entries.append(struct.pack('<4I', pos, len(c), 0, 0))
        cnks.append(c)
        pos += len(c)
    rel = lambda p: p - data_start
    mhdr = chunk(b'MHDR', struct.pack('<16I', 0, rel(mcin_at), rel(at[0]), rel(at[1]), rel(at[2]), rel(at[3]),
                                      rel(at[4]), rel(at[5]), rel(at[6]), 0, 0, 0, 0, 0, 0, 0))
    mcin = chunk(b'MCIN', b''.join(entries))
    return mver + mhdr + mcin + b''.join(body) + b''.join(cnks)


def wdt(tiles):
    main = bytearray(64 * 64 * 8)
    for tx, ty in tiles:
        struct.pack_into('<2I', main, (ty * 64 + tx) * 8, 1, 0)
    return (chunk(b'MVER', struct.pack('<I', 18)) + chunk(b'MPHD', bytes(32)) + chunk(b'MAIN', bytes(main))
            + chunk(b'MWMO', b''))


def main(side):
    s = SIDES[side]
    folder = os.path.join(ROOT, 'data', 'art', 'World', 'Maps', s['dir'])
    wdt_path = os.path.join(folder, s['dir'] + '.wdt')
    old = open(wdt_path, 'rb').read()
    bbox = hall_bbox(old) if any(t == b'MODF' for t, _ in read_chunks(old)) else None
    if bbox is None:
        raise SystemExit('the WDT already lists tiles: built from the hall\'s placement, run on the original WDT')
    tiles = tiles_of(bbox)
    src = template()
    for tx, ty in tiles:
        open(os.path.join(folder, '%s_%d_%d.adt' % (s['dir'], tx, ty)), 'wb').write(
            adt(tx, ty, s['wmo'], bbox, src, s['area']))
    open(wdt_path, 'wb').write(wdt(tiles))
    print(side, 'tiles', tiles, 'terrain at', round(bbox[2] - DEPTH, 1))


if __name__ == '__main__':
    main(sys.argv[1])
