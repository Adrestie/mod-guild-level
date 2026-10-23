"""The Customization objects' game files, from export_decor.py's export:
  * each model laid out for the client's modern M2 plugin (WarcraftXL's wxl-modern-m2, through
    mod-limit-break's m2port) in data/art/World/GuildLevel/Decor, its textures named by path there
    and brought down to 256 at most (shrink_blp.py);
  * the catalogue the interface reads, data/lua/GuildLevel/GuildHouse_Decor.lua (one row per object:
    its HouseDecor ID, kind, model and English name);
  * its display (GameObjectDisplayInfo, the n-th number of DISPLAY_BLOCKS) for the client, beside the Guild Chest's in
    data/dbc/guild_level_GameObjectDisplayInfo.dbc;
  * data/sql/world/guild_level_11_decor.sql: the objects (mod_guild_level_decor), the server's
    displays and the gameobjects (920100 + n).

    python build_decor.py <export folder>
"""
import json
import os
import struct
import sys

sys.path.insert(0, 'E:/WoW-mods/mod-limit-break/tools/m2port')
import pack  # noqa: E402

import preview_camera  # noqa: E402
import shrink_blp  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
GAME_DIR = 'World\\GuildLevel\\Decor'
LUA = os.path.join(ROOT, 'data', 'lua', 'GuildLevel', 'GuildHouse_Decor.lua')
DBC = os.path.join(ROOT, 'data', 'dbc', 'guild_level_GameObjectDisplayInfo.dbc')
SQL = os.path.join(ROOT, 'data', 'sql', 'world', 'guild_level_11_decor.sql')
PREVIEW_ASPECT = 280 / 224         # the interface's preview: its model's width over height
# the module's blocks (WoW-mods ID_RANGES.md) the objects' numbers are taken from, in order: the
# n-th object's display (and, later, its item) is the n-th number of these
DISPLAY_BLOCKS = [(92100, 94599)]
ENTRY = 920100                      # its gameobject: ENTRY + n (the blocks' gameobjects, x 10)
KEEP_DISPLAYS = (92010, 92011)      # the Guild Chest's, in the same file
# a modern model leaves its texture unit lookup empty (each texture's UV set follows from the
# shader); the client reads it per batch: filled, the effects' environment maps (the reflections
# of shader 0x8000) are drawn as such instead of from whatever the batch points at
PACK_OPTIONS = {'fill_units': True}
BACKSLASH = chr(92)


def lua_string(s):
    return '"%s"' % s.replace(BACKSLASH, BACKSLASH * 2).replace('"', BACKSLASH + '"')


def sql_string(s):
    return "'%s'" % s.replace(BACKSLASH, BACKSLASH * 2).replace("'", "''")


# blending modes that only add light (glows, halos, light shafts): left out of the bounds
ADDITIVE = (3, 4)
CARD_VERTICES = 8                   # a see-through section this small is a glow's card


# a skin's FileDataID no path table holds: the plugin then reads the skin by its name beside the model
OWN_SKIN_ID = 0x7FFF0000


def own_skins(path):
    """The model's skins (SFID) pointed at its own files. A model of the old world keeps the
    FileDataIDs of its 3.3.5 skins (Coffin00.skin...): WarcraftXL resolves them (wxl-db2's
    ModelFilePath) and the client reads the old skin against the modern vertices -- a preview from
    inside a tangle of faces. FileDataIDs no table holds send it to the files beside the model."""
    data = bytearray(open(path, 'rb').read())
    i = 0
    while i + 8 <= len(data):
        tag, size = data[i:i + 4], struct.unpack_from('<I', data, i + 4)[0]
        if tag == b'SFID':
            for k in range(size // 4):
                struct.pack_into('<I', data, i + 8 + 4 * k, OWN_SKIN_ID + k)
        i += 8 + size
    open(path, 'wb').write(data)


def _array(b, off):
    return struct.unpack_from('<II', b, off)


def bounds(path):
    """The box of what the model draws: min x, y, z, max x, y, z of the vertices of its first skin's
    batches, those that add light and the see-through cards of glows left out (unless it has
    nothing else). The header's box (MD20,
    0xA0) holds its lights' and particles' reach too: a candle's is ten times its size."""
    data = open(path, 'rb').read()
    i = data.find(b'MD20')
    m = data[i:]
    header = struct.unpack_from('<6f', m, 0xA0)
    skin = path[:-3] + '00.skin'
    if not os.path.isfile(skin):
        return header
    s = open(skin, 'rb').read()
    nv, ov = _array(m, 0x3C)
    nmat, omat = _array(m, 0x70)
    blending = [struct.unpack_from('<HH', m, omat + 4 * k)[1] for k in range(nmat)]
    nlook, olook = _array(s, 4)
    lookup = struct.unpack_from('<%dH' % nlook, s, olook)
    nsub, osub = _array(s, 28)
    nbatch, obatch = _array(s, 36)
    solid, every = set(), set()
    for k in range(nbatch):
        section, material = struct.unpack_from('<H', s, obatch + 24 * k + 4)[0],             struct.unpack_from('<H', s, obatch + 24 * k + 10)[0]
        every.add(section)
        mode = blending[material] if material < len(blending) else 0
        count = struct.unpack_from('<H', s, osub + 48 * section + 6)[0]
        # light that adds up, and the see-through cards of a few vertices (glows, beams)
        if mode in ADDITIVE or (mode >= 2 and count <= CARD_VERTICES):
            continue
        solid.add(section)
    used = set()
    for section in solid or every:
        start, count = struct.unpack_from('<HH', s, osub + 48 * section + 4)
        used.update(lookup[start:start + count])
    points = [struct.unpack_from('<3f', m, ov + 48 * v) for v in used if v < nv]
    if not points:
        return header
    return tuple(min(p[a] for p in points) for a in range(3)) + tuple(max(p[a] for p in points) for a in range(3))


def display_of(n):
    """The n-th object's number in the module's blocks."""
    for first, last in DISPLAY_BLOCKS:
        if n <= last - first:
            return first + n
        n -= last - first + 1
    raise SystemExit('more objects than the blocks of DISPLAY_BLOCKS hold: add a block')


def write_dbc(rows):
    """GameObjectDisplayInfo rows (ID, model, ten sounds, the geometry box, effect package), the Guild
    Chest's kept."""
    old = open(DBC, 'rb').read()
    n, fields, size, _ = struct.unpack_from('<4I', old, 4)
    old_strings = old[20 + n * size:]
    kept = []
    for r in range(n):
        rec = list(struct.unpack_from('<%dI' % fields, old, 20 + r * size))
        if rec[0] in KEEP_DISPLAYS:
            end = old_strings.index(b'\0', rec[1])
            kept.append((rec, old_strings[rec[1]:end].decode('latin1')))
    strings = bytearray(b'\0')
    out = []
    for rec, name in kept + rows:
        rec = list(rec)
        rec[1] = len(strings)
        strings += name.encode('latin1') + b'\0'
        out.append(struct.pack('<%dI' % fields, *rec))
    open(DBC, 'wb').write(b'WDBC' + struct.pack('<4I', len(out), fields, size, len(strings))
                          + b''.join(out) + bytes(strings))


def write_sql(rows):
    first, last = rows[0][2], rows[-1][2]
    out = [
        '-- mod-guild-level — the Customization objects (Paragon 9), written by tools/decor/build_decor.py.',
        '--',
        '-- Furniture of the modern housing: each one a gameobject (%d to %d) with its display' % (first, last),
        "-- (in %s; the client's rows are in data/dbc/guild_level_GameObjectDisplayInfo.dbc)." % ', '.join(
            '%d-%d' % block for block in DISPLAY_BLOCKS),
        "-- The guilds' stock and placed objects are in the characters database (src/GuildDecor.h).",
        '--',
        '-- Idempotent: the table is created once, its rows are written again.',
        '',
        'CREATE TABLE IF NOT EXISTS `mod_guild_level_decor` (',
        "  `decor` INT UNSIGNED NOT NULL COMMENT 'HouseDecor ID',",
        "  `entry` INT UNSIGNED NOT NULL COMMENT 'gameobject_template',",
        "  `kind` VARCHAR(40) NOT NULL DEFAULT '',",
        "  `name` VARCHAR(100) NOT NULL DEFAULT '',",
        '  PRIMARY KEY (`decor`)',
        ') ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci',
        "  COMMENT='GuildLevel: the Customization objects of the guild houses';",
        '',
        'DELETE FROM `mod_guild_level_decor`;',
        'INSERT INTO `mod_guild_level_decor` (`decor`, `entry`, `kind`, `name`) VALUES',
        ',\n'.join('(%d, %d, %s, %s)' % (e['decor'], entry, sql_string(e['kind']), sql_string(e['name']))
                   for e, display, entry, mdx, box in rows) + ';',
        '',
        'DELETE FROM `gameobjectdisplayinfo_dbc` WHERE %s;' % ' OR '.join(
            '`ID` BETWEEN %d AND %d' % block for block in DISPLAY_BLOCKS),
        'INSERT INTO `gameobjectdisplayinfo_dbc` (`ID`, `ModelName`, `GeoBoxMinX`, `GeoBoxMinY`, `GeoBoxMinZ`, '
        '`GeoBoxMaxX`, `GeoBoxMaxY`, `GeoBoxMaxZ`) VALUES',
        ',\n'.join('(%d, %s, %s)' % (display, sql_string(mdx), ', '.join('%.4f' % v for v in box))
                   for e, display, entry, mdx, box in rows) + ';',
        '',
        '-- Generic objects (type 5): nothing to use, seen from afar (large).',
        'DELETE FROM `gameobject_template` WHERE `entry` BETWEEN %d AND %d;' % (first, last),
        'INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `size`, `Data3`) VALUES',
        ',\n'.join('(%d, 5, %d, %s, 1, 1)' % (entry, display, sql_string(e['name']))
                   for e, display, entry, mdx, box in rows) + ';',
        '',
    ]
    open(SQL, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))


def main(export_dir):
    catalog = json.load(open(os.path.join(HERE, 'catalog.json'), encoding='utf-8'))
    models = json.load(open(os.path.join(export_dir, 'models.json'), encoding='utf-8'))
    out = os.path.join(ROOT, 'data', 'art', *GAME_DIR.split(BACKSLASH))
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):
        os.remove(os.path.join(out, f))
    rows = []
    for e in catalog:
        name = models.get(str(e['decor']))
        if not name:
            print('not exported:', e['decor'], e['name'])
            continue
        if e.get('removed'):
            # taken out: its number stays (the rows' order), its files are not written
            rows.append((e, None))
            continue
        pack.pack(os.path.join(export_dir, name, name + '.m2'), out, name, GAME_DIR, PACK_OPTIONS, None)
        m2 = os.path.join(out, name + '.m2')
        own_skins(m2)
        # a camera of its own for the interface's turntable preview
        data, camera = preview_camera.add(open(m2, 'rb').read(), PREVIEW_ASPECT, bounds(m2))
        open(m2, 'wb').write(data)
        e = dict(e, camera=camera)
        rows.append((e, GAME_DIR + BACKSLASH + name + '.m2'))

    # textures at 256 at most: the levels above are dropped
    shrink_blp.main(out, 256)

    dbc_rows, sql_rows = [], []
    for n, (e, path) in enumerate(rows):
        # an object taken out ("removed" in catalog.json) keeps its number, unused
        if e.get('removed'):
            continue
        box = bounds(os.path.join(out, path.split(BACKSLASH)[-1]))
        mdx = path[:-3] + '.mdx'
        fbits = [struct.unpack('<I', struct.pack('<f', v))[0] for v in box]
        dbc_rows.append(([display_of(n), 0] + [0] * 10 + fbits + [0], mdx))
        sql_rows.append((e, display_of(n), ENTRY + n, mdx, box))
    write_dbc(dbc_rows)
    write_sql(sql_rows)

    lines = [
        '-- The Customization objects (tools/decor/build_decor.py, from tools/decor/catalog.json):',
        '-- HouseDecor ID = { category, kind, model, English name, scale, the model\'s bounds (min x, y, z,',
        '-- max x, y, z) }.',
        'if AIO.AddAddon() then return end',
        '',
        'GuildHouseDecor = {',
    ]
    for (e, path), (_, _, _, _, box) in zip([r for r in rows if not r[0].get('removed')], sql_rows):
        lines.append('    [%d] = { %s, %s, %s, %s, %g, %s, %d },' % (
            e['decor'], lua_string(e['category']), lua_string(e['kind']), lua_string(path), lua_string(e['name']),
            e.get('scale', 1), ', '.join('%.3f' % v for v in box), e['camera']))
    lines.append('}')
    open(LUA, 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')
    size = sum(os.path.getsize(os.path.join(out, f)) for f in os.listdir(out))
    print(len(rows), 'objects,', len(os.listdir(out)), 'files,', round(size / 1e6, 1), 'MB')


if __name__ == '__main__':
    main(sys.argv[1])
