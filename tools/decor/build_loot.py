"""The Customization objects as items, and where they drop (src/GuildDecor.h):
  * each object's icon, from its catalogue picture (export_decor.py's thumbnails), 64 x 64 in
    data/art/Interface/Icons/GuildLevel_Decor_<HouseDecor ID>.blp, and its item display
    (ItemDisplayInfo, the object's own number) in data/dbc/guild_level_ItemDisplayInfo.dbc;
  * data/sql/world/guild_level_13_decor_items.sql: the items (item_template, the object's number:
    build_decor.py's DISPLAY_BLOCKS), the item -> object table, where each object drops (loot.json:
    the bosses' own, the themes' maps), the use spell (92080, spell_guild_decor_collect) and the
    hidden mark of the members who may loot one (92081), the texts;
  * installer.json: the client's Spell.dbc row of 92080, the DBC file, the rows to remove.

    python build_loot.py <export folder>
"""
import json
import os
import re
import struct
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(HERE, '..'))
sys.path.insert(0, os.path.join(HERE, '..', 'house'))
import blp  # noqa: E402  (tools/house/blp.py: BLP2 decoding)
import build_decor  # noqa: E402
import perk_spells  # noqa: E402

ICONS = os.path.join(ROOT, 'data', 'art', 'Interface', 'Icons')
ICON_PREFIX = 'GuildLevel_Decor_'
ICON_SIZE = 64
FALLBACK_DISPLAY = 7913             # INV_Crate_02, for an object without a picture
ITEM_DBC = os.path.join(ROOT, 'data', 'dbc', 'guild_level_ItemDisplayInfo.dbc')
SQL = os.path.join(ROOT, 'data', 'sql', 'world', 'guild_level_13_decor_items.sql')
SELL_PRICE = 5000                               # copper: 50 silver
COLLECT, MARK = 92080, 92081
BACKSLASH = chr(92)


# --- icons: DXT1, one colour block of 4 x 4 at a time --------------------------------------------

def _dxt1(rgb):
    """rgb: h x w x 3 (multiples of 4) -> DXT1 blocks."""
    h, w, _ = rgb.shape
    out = bytearray()
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            block = rgb[by:by + 4, bx:bx + 4].reshape(16, 3).astype(np.int32)
            lum = block @ np.array([3, 6, 1])
            hi, lo = block[lum.argmax()], block[lum.argmin()]

            def pack(c):
                return ((int(c[0]) >> 3) << 11) | ((int(c[1]) >> 2) << 5) | (int(c[2]) >> 3)

            c0, c1 = pack(hi), pack(lo)
            if c0 < c1:
                c0, c1, hi, lo = c1, c0, lo, hi
            if c0 == c1:
                out += struct.pack('<HHI', c0, c1, 0)
                continue
            pal = np.array([hi, lo, (2 * hi + lo) // 3, (hi + 2 * lo) // 3])
            idx = ((block[:, None, :] - pal[None, :, :]) ** 2).sum(2).argmin(1)
            bits = 0
            for i, v in enumerate(idx):
                bits |= int(v) << (2 * i)
            out += struct.pack('<HHI', c0, c1, bits)
    return bytes(out)


def write_icon(picture, path):
    """The picture on a dark ground, square, ICON_SIZE wide, as a BLP2 (DXT1, with its mip levels)."""
    img = picture.convert('RGBA')
    side = max(img.size)
    ground = Image.new('RGBA', (side, side), (24, 20, 16, 255))
    ground.alpha_composite(img, ((side - img.width) // 2, (side - img.height) // 2))
    levels = []
    size = ICON_SIZE
    while size >= 4:
        levels.append(_dxt1(np.asarray(ground.convert('RGB').resize((size, size), Image.LANCZOS))))
        size //= 2
    header = bytearray(b'BLP2' + struct.pack('<I', 1) + bytes([2, 0, 0, 1]) + struct.pack('<II', ICON_SIZE, ICON_SIZE))
    offsets, sizes = [], []
    at = 148
    for data in levels:
        offsets.append(at)
        sizes.append(len(data))
        at += len(data)
    header += struct.pack('<16I', *(offsets + [0] * (16 - len(offsets))))
    header += struct.pack('<16I', *(sizes + [0] * (16 - len(sizes))))
    open(path, 'wb').write(bytes(header) + b''.join(levels))


# --- ItemDisplayInfo: ID, models, textures, icon (field 5), the rest empty -----------------------

def write_item_dbc(rows):
    fields = 25
    strings = bytearray(b'\0')
    recs = []
    for display, icon in rows:
        rec = [0] * fields
        rec[0] = display
        rec[5] = len(strings)
        strings += icon.encode('latin1') + b'\0'
        recs.append(struct.pack('<%dI' % fields, *rec))
    open(ITEM_DBC, 'wb').write(b'WDBC' + struct.pack('<4I', len(recs), fields, fields * 4, len(strings))
                               + b''.join(recs) + bytes(strings))


# --- where each object drops ---------------------------------------------------------------------

def loot_rows(catalog):
    spec = json.load(open(os.path.join(HERE, 'loot.json'), encoding='utf-8'))
    rows = set()
    for e in catalog:
        if e.get('removed'):
            continue
        name = e['name'].lower()
        for theme in spec['themes']:
            if any(re.search(r'(^|[^a-z])' + re.escape(k), name) for k in theme['keywords']):
                for m in theme['maps']:
                    rows.add((e['decor'], m, 0))
    for creature, decors in spec.get('bosses', {}).items():
        for d in decors:
            rows.add((int(d), 0, int(creature)))
    return sorted(rows)


def sql_string(s):
    return "'%s'" % s.replace(BACKSLASH, BACKSLASH * 2).replace("'", "''")


COLLECT_SPELL = dict(id=COLLECT, icon=3708, cast=1, visual=0,
    name=dict(en='Guild Object', fr='Objet de guilde', de='Gildengegenstand', es='Objeto de hermandad',
              ru='Предмет гильдии'),
    desc=dict(en="Adds this object to your guild's Customization stock.",
              fr="Ajoute cet objet au stock de Personnalisation de votre guilde.",
              de='Fügt diesen Gegenstand dem Gestaltungsvorrat Eurer Gilde hinzu.',
              es='Añade este objeto a las existencias de Personalización de tu hermandad.',
              ru='Добавляет этот предмет в запас оформления вашей гильдии.'))
# hidden, ten minutes (SpellDuration 6), a dummy aura on the member: the drop's loot condition
MARK_SPELL = {0: MARK, 4: 0x80000180, 7: 0x100000, 28: 1, 40: 6, 46: 1, 68: -1, 71: 6, 74: 1, 80: -1, 86: 1,
              95: 4, 136: 'Guild Object (loot)', 216: perk_spells.ONE, 217: perk_spells.ONE, 218: perk_spells.ONE,
              225: 1, 152: perk_spells.MASK, 169: perk_spells.MASK, 186: perk_spells.MASK, 203: perk_spells.MASK}


def spell_sql(fields, columns):
    names, vals = [], []
    for i in sorted(fields):
        v = fields[i]
        names.append('`%s`' % columns[i])
        if v == perk_spells.ONE and i in (216, 217, 218):
            vals.append('1.0')
        elif isinstance(v, str):
            vals.append(sql_string(v))
        else:
            vals.append(str(v))
    return 'INSERT INTO `spell_dbc` (%s)\nVALUES (%s);' % (', '.join(names), ', '.join(vals))


def main(export_dir):
    catalog = json.load(open(os.path.join(HERE, 'catalog.json'), encoding='utf-8'))
    os.makedirs(ICONS, exist_ok=True)
    for f in os.listdir(ICONS):
        if f.startswith(ICON_PREFIX):
            os.remove(os.path.join(ICONS, f))
    items, dbc_rows = [], []
    for n, e in enumerate(catalog):
        if e.get('removed'):            # taken out: its number stays unused
            continue
        number = build_decor.display_of(n)
        thumb = os.path.join(export_dir, 'thumbnails', '%d.blp' % e['decor'])
        display = FALLBACK_DISPLAY
        if os.path.isfile(thumb):
            try:
                rgba = blp.decode(open(thumb, 'rb').read())
                icon = ICON_PREFIX + str(e['decor'])
                write_icon(Image.fromarray(np.asarray(rgba, dtype=np.uint8), 'RGBA'), os.path.join(ICONS, icon + '.blp'))
                dbc_rows.append((number, icon))
                display = number
            except Exception as x:          # noqa: BLE001 - a picture it cannot read: the crate
                print('no icon for', e['decor'], x)
        items.append((e, number, display))
    write_item_dbc(dbc_rows)

    columns = json.load(open(os.path.join(HERE, 'spell_dbc_columns.json')))
    loot = loot_rows(catalog)
    first, last = build_decor.DISPLAY_BLOCKS[0][0], build_decor.DISPLAY_BLOCKS[-1][1]
    ranges = ' OR '.join('`entry` BETWEEN %d AND %d' % b for b in build_decor.DISPLAY_BLOCKS)
    item_rows = []
    for e, number, display in items:
        # Miscellaneous (15/4), rare, not bound (it is traded, sold: 50 silver), in stacks of 20; its use
        # (92080) uses it up
        item_rows.append('(%d, 15, 4, -1, %s, %d, 3, 0, 1, 0, %d, 1, 20, %d, 0, -1, 0, -1, -1, 0, %s, 4, %s)' % (
            number, sql_string(e['name']), display, SELL_PRICE, COLLECT,
            sql_string('Customization object of the guild house.'), sql_string('')))
    out = [
        '-- mod-guild-level — the Customization objects as items, and where they drop (tools/decor/build_loot.py).',
        '--',
        '-- Each object is an item of its own number (its display\'s); used, it goes into the guild\'s stock',
        '-- (spell %d, spell_guild_decor_collect). A dungeon or raid boss may drop one for a guild there in' % COLLECT,
        '-- numbers (src/GuildDecor.h): the boss\'s own objects first, else those of the map\'s theme',
        '-- (tools/decor/loot.json), else one of no theme. Spell %d is the hidden mark of the members who may' % MARK,
        '-- loot it (server side only).',
        '--',
        '-- Idempotent: the tables are created once, their rows are written again.',
        '',
        'CREATE TABLE IF NOT EXISTS `mod_guild_level_decor_item` (',
        "  `decor` INT UNSIGNED NOT NULL COMMENT 'mod_guild_level_decor',",
        "  `item` INT UNSIGNED NOT NULL COMMENT 'item_template',",
        '  PRIMARY KEY (`decor`), KEY `item` (`item`)',
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='GuildLevel: the item of each Customization object';",
        '',
        'CREATE TABLE IF NOT EXISTS `mod_guild_level_decor_loot` (',
        "  `decor` INT UNSIGNED NOT NULL COMMENT 'mod_guild_level_decor',",
        "  `map` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'a dungeon or raid of its theme, 0 for a boss row',",
        "  `creature` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'a boss that drops it first, 0 for a theme row',",
        '  PRIMARY KEY (`decor`, `map`, `creature`)',
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='GuildLevel: where the Customization objects drop';",
        '',
        'DELETE FROM `mod_guild_level_decor_item`;',
        'INSERT INTO `mod_guild_level_decor_item` (`decor`, `item`) VALUES',
        ',\n'.join('(%d, %d)' % (e['decor'], number) for e, number, display in items) + ';',
        '',
        'DELETE FROM `mod_guild_level_decor_loot`;',
        'INSERT INTO `mod_guild_level_decor_loot` (`decor`, `map`, `creature`) VALUES',
        ',\n'.join('(%d, %d, %d)' % r for r in loot) + ';',
        '',
        'DELETE FROM `item_template` WHERE %s;' % ranges,
        'INSERT INTO `item_template` (`entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, '
        '`Quality`, `Flags`, `BuyCount`, `BuyPrice`, `SellPrice`, `ItemLevel`, `stackable`, `spellid_1`, '
        '`spelltrigger_1`, `spellcharges_1`, `spellcooldown_1`, `spellcategorycooldown_1`, `spellppmRate_1`, '
        '`bonding`, `description`, `Material`, `ScriptName`) VALUES',
        ',\n'.join(item_rows) + ';',
        '',
        'DELETE FROM `spell_dbc` WHERE `ID` IN (%d, %d);' % (COLLECT, MARK),
        spell_sql(perk_spells.fields(COLLECT_SPELL), columns),
        spell_sql(MARK_SPELL, columns),
        '',
        "DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_guild_decor_collect';",
        "INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (%d, 'spell_guild_decor_collect');" % COLLECT,
        '',
        "DELETE FROM `module_string` WHERE `module` = 'mod-guild-level' AND `id` = 178;",
        "INSERT INTO `module_string` (`module`, `id`, `string`) VALUES",
        "('mod-guild-level', 178, '{} goes into your guild''s objects: it owns {} of them.');",
        "DELETE FROM `module_string_locale` WHERE `module` = 'mod-guild-level' AND `id` = 178 AND `locale` = 'frFR';",
        "INSERT INTO `module_string_locale` (`module`, `id`, `locale`, `string`) VALUES",
        "('mod-guild-level', 178, 'frFR', '{} rejoint les objets de votre guilde : elle en possède {}.');",
        '',
    ]
    open(SQL, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))

    # installer.json: the client's display rows and spell, what removal takes away
    p = os.path.join(ROOT, 'installer.json')
    m = json.load(open(p, encoding='utf-8'))
    m['dbc'] = [d for d in m['dbc'] if d['file'] != 'ItemDisplayInfo.dbc'] + [{
        'file': 'ItemDisplayInfo.dbc', 'fields': 25, 'text_fields': [1, 2, 3, 4, 5, 6, 15, 16, 17, 18, 19, 20, 21, 22],
        'client': 'data/dbc/guild_level_ItemDisplayInfo.dbc', 'server': False}]
    spell = next(d for d in m['dbc'] if d['file'] == 'Spell.dbc')
    spell['rows'] = [r for r in spell['rows'] if r.get('0') != COLLECT] + [
        {str(k): v for k, v in sorted(perk_spells.fields(COLLECT_SPELL).items())}]
    world = m['database']['world']
    for t in ('mod_guild_level_decor_item', 'mod_guild_level_decor_loot'):
        if t not in world['tables']:
            world['tables'].append(t)
    wanted = [
        {'table': 'item_template', 'where': ranges.replace('`', '')},
        {'table': 'spell_dbc', 'where': 'ID IN (%d, %d)' % (COLLECT, MARK)},
        {'table': 'spell_script_names', 'where': "ScriptName = 'spell_guild_decor_collect'"},
    ]
    rows = world['rows']
    rows[:] = [r for r in rows if r['table'] != 'item_template' or 'entry BETWEEN 92100' not in r['where']]
    for r in wanted:
        if r not in rows:
            rows.insert(len(rows) - 1, r)
    open(p, 'w', encoding='utf-8', newline='\n').write(json.dumps(m, indent=2, ensure_ascii=False) + '\n')
    print(len(items), 'items,', len(dbc_rows), 'icons,', len(loot), 'loot rows')


if __name__ == '__main__':
    main(sys.argv[1])
