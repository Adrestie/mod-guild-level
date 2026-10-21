"""The translations of the Customization objects: their names (the housing's own, HouseDecor of
retail, in each language), their families (DecorCategory, DecorSubcategory) and the places the
Collection window names (the dungeons and raids of loot.json, Map; the zones of the quests, AreaTable).

    python build_locales.py <worldserver folder> [--fetch]

--fetch reads them again from wago.tools (the retail tables, one language at a time) into
locales.json beside this file, the zones being those of the world database's quests; without it,
locales.json is used as it is. Writes:
  * data/sql/world/guild_level_15_decor_locales.sql: item_template_locale and
    gameobject_template_locale of the objects, and the module's Customization texts (170-179) in
    German, Spanish and Russian (French is beside the English, guild_level_12-14);
  * data/lua/GuildLevel/GuildHouse_Locale.lua: the families, dungeons and zones in each language,
    for the interface (the objects' names come from the server, in the player's language).

esMX takes the esES texts. Retail renamed a few places since 3.3.5: OVERRIDES gives them back.
"""
import csv
import io
import json
import os
import re
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, r'E:\WoW-mods\installer')

JSON = os.path.join(HERE, 'locales.json')
SQL = os.path.join(ROOT, 'data', 'sql', 'world', 'guild_level_15_decor_locales.sql')
LUA = os.path.join(ROOT, 'data', 'lua', 'GuildLevel', 'GuildHouse_Locale.lua')
LOCALES = ['enUS', 'frFR', 'deDE', 'esES', 'ruRU']
SQL_LOCALES = {'frFR': 'frFR', 'deDE': 'deDE', 'esES': 'esES', 'esMX': 'esES', 'ruRU': 'ruRU'}
WAGO = 'https://wago.tools/db2/%s/csv?locale=%s'
BACKSLASH = chr(92)

# the 3.3.5 names of the places retail renamed: (Map or AreaTable, ID) -> each language
OVERRIDES = {
    ('maps', 189): dict(enUS='Scarlet Monastery', frFR='Monastère Écarlate', deDE='Scharlachrotes Kloster',
                        esES='Monasterio Escarlata', ruRU='Монастырь Алого ордена'),
    ('maps', 309): dict(enUS="Zul'Gurub", frFR="Zul'Gurub", deDE="Zul'Gurub", esES="Zul'Gurub", ruRU="Зул'Гуруб"),
    ('zones', 17): dict(enUS='The Barrens', frFR='Les Tarides', deDE='Brachland', esES='Los Baldíos', ruRU='Степи'),
    ('zones', 33): dict(enUS='Stranglethorn Vale', frFR='Vallée de Strangleronce', deDE='Schlingendorntal',
                        esES='Vega de Tuercespina', ruRU='Тернистая долина'),
}

DESCRIPTION = {
    'frFR': 'Objet de Personnalisation de la maison de guilde.',
    'deDE': 'Gestaltungsgegenstand für das Gildenhaus.',
    'esES': 'Objeto de Personalización de la casa de hermandad.',
    'ruRU': 'Предмет оформления дома гильдии.',
}

# the module's Customization texts (src/GuildLevel.h); English and French: guild_level_12 to 14
STRINGS = {
    170: dict(deDE='Eure Gilde kann ihr Haus ab Paragonstufe {} gestalten.',
              esES='Tu hermandad decora su casa a partir del nivel de Paragón {}.',
              ruRU='Гильдия может обустраивать свой дом с {}-го уровня Парагона.'),
    171: dict(deDE='Nur die Gildenränge bis {} dürfen das Haus gestalten (0 ist der Gildenmeister).',
              esES='Solo los rangos de hermandad hasta el {} pueden decorar la casa (0 es el maestro de hermandad).',
              ruRU='Обустраивать дом могут только звания гильдии до {} (0 — глава гильдии).'),
    172: dict(deDE='Diesen Gegenstand gibt es nicht.', esES='Ese objeto no existe.',
              ruRU='Такого предмета не существует.'),
    173: dict(deDE='Eurer Gilde ist keiner dieser Gegenstände zum Platzieren geblieben.',
              esES='A tu hermandad no le quedan unidades de este objeto para colocar.',
              ruRU='У вашей гильдии не осталось таких предметов для размещения.'),
    174: dict(deDE='Ein anderes Mitglied bewegt diesen Gegenstand.', esES='Otro miembro está moviendo este objeto.',
              ruRU='Этот предмет сейчас перемещает другой член гильдии.'),
    175: dict(deDE='Dieser Gegenstand ist nicht in Eurem Gildenhaus.', esES='Ese objeto no está en la casa de tu hermandad.',
              ruRU='Этого предмета нет в доме вашей гильдии.'),
    176: dict(deDE='Dieser Gegenstand kann dort nicht platziert werden.', esES='Este objeto no se puede colocar ahí.',
              ruRU='Этот предмет нельзя разместить здесь.'),
    177: dict(deDE='{} von Gegenstand {} (0: alle Gegenstände) an die Gilde {} vergeben.',
              esES='{} del objeto {} (0: todos los objetos) entregados a la hermandad {}.',
              ruRU='Гильдии {2} выдано {0} шт. предмета {1} (0 — все предметы).'),
    178: dict(deDE='{} geht an die Gegenstände Eurer Gilde: Sie besitzt nun {} davon.',
              esES='{} se suma a los objetos de tu hermandad: ahora tiene {}.',
              ruRU='{} добавлен к предметам вашей гильдии: теперь их {}.'),
    179: dict(deDE='Für Eure Gilde bringt diese Quest auch einen Gegenstand für ihr Haus ein: {}.',
              esES='Para tu hermandad, esta misión también otorga un objeto para su casa: {}.',
              ruRU='Это задание также приносит вашей гильдии предмет для её дома: {}.'),
}


def clean(name):
    """A name as the 3.3.5 fonts show it: straight apostrophes, plain spaces, no note for the
    developers nor expansion after it."""
    name = name.replace('\u2019', "'").replace('\u2018', "'").replace('\xa0', ' ').replace('\u202f', ' ')
    name = re.sub(r'\s*DEPRECATED DONT USE\s*$', '', name)
    name = re.sub(r'\s*\((The )?(Burning Crusade|Classic)\)$', '', name)
    return name.strip()


def wago(table, locale):
    request = urllib.request.Request(WAGO % (table, locale), headers={'User-Agent': 'mod-guild-level build_locales'})
    with urllib.request.urlopen(request, timeout=300) as r:
        return list(csv.DictReader(io.StringIO(r.read().decode('utf-8'))))


def fetch(world):
    catalog = json.load(open(os.path.join(HERE, 'catalog.json'), encoding='utf-8'))
    spec = json.load(open(os.path.join(HERE, 'loot.json'), encoding='utf-8'))
    decors = {e['decor'] for e in catalog}
    maps = sorted({m for t in spec['themes'] for m in t.get('maps', [])})
    zones = sorted({int(r[0]) for r in world.run("SELECT DISTINCT QuestSortID FROM quest_template WHERE QuestSortID > 0")})
    out = {'objects': {}, 'categories': {}, 'kinds': {}, 'maps': {}, 'zones': {}}
    english = {}
    for locale in LOCALES:
        print('reading', locale)
        for r in wago('HouseDecor', locale):
            if int(r['ID']) in decors:
                out['objects'].setdefault(r['ID'], {})[locale] = clean(r['Name_lang'])
        for table, key in (('DecorCategory', 'categories'), ('DecorSubcategory', 'kinds')):
            for r in wago(table, locale):
                if locale == 'enUS':
                    english[(key, r['ID'])] = r['Name_lang']
                out[key].setdefault(english[(key, r['ID'])], {})[locale] = clean(r['Name_lang'])
        for r in wago('Map', locale):
            if int(r['ID']) in maps:
                out['maps'].setdefault(r['ID'], {})[locale] = clean(r['MapName_lang'])
        for r in wago('AreaTable', locale):
            if int(r['ID']) in zones:
                out['zones'].setdefault(r['ID'], {})[locale] = clean(r['AreaName_lang'])
    for (key, ident), names in OVERRIDES.items():
        out[key][str(ident)] = names
    json.dump(out, open(JSON, 'w', encoding='utf-8'), ensure_ascii=False, indent=1, sort_keys=True)


def sql_string(s):
    return "'%s'" % s.replace(BACKSLASH, BACKSLASH * 2).replace("'", "''")


def lua_string(s):
    return '"%s"' % s.replace(BACKSLASH, BACKSLASH * 2).replace('"', BACKSLASH + '"')


def pairs_of(path, table):
    """(decor, number) of the INSERT INTO `table` (`decor`, `...`) of a generated file."""
    text = open(path, encoding='utf-8').read()
    block = text[text.index('INSERT INTO `%s`' % table):]
    block = block[:block.index(';')]
    return {int(a): int(b) for a, b in re.findall(r'\((\d+), (\d+)', block)}


def build():
    data = json.load(open(JSON, encoding='utf-8'))
    world_sql = os.path.join(ROOT, 'data', 'sql', 'world')
    entries = pairs_of(os.path.join(world_sql, 'guild_level_11_decor.sql'), 'mod_guild_level_decor')
    items = pairs_of(os.path.join(world_sql, 'guild_level_13_decor_items.sql'), 'mod_guild_level_decor_item')

    item_rows, go_rows = [], []
    for decor in sorted(entries):
        names = data['objects'][str(decor)]
        for locale, source in SQL_LOCALES.items():
            name = names.get(source) or names['enUS']
            item_rows.append('(%d, %s, %s, %s)' % (items[decor], sql_string(locale), sql_string(name),
                                                   sql_string(DESCRIPTION[source])))
            go_rows.append('(%d, %s, %s, %s)' % (entries[decor], sql_string(locale), sql_string(name), "''"))
    string_rows = []
    for ident, texts in sorted(STRINGS.items()):
        for locale, source in SQL_LOCALES.items():
            if source in texts:
                string_rows.append("('mod-guild-level', %d, %s, %s)" % (ident, sql_string(locale), sql_string(texts[source])))
    item_where = '`ID` IN (SELECT `item` FROM `mod_guild_level_decor_item`)'
    out = [
        '-- mod-guild-level — the Customization objects and texts in each language (tools/decor/build_locales.py).',
        '--',
        '-- The objects\' names are the housing\'s own (HouseDecor of retail); esMX takes the esES texts.',
        '-- Idempotent: every row is deleted before being written again.',
        '',
        'DELETE FROM `item_template_locale` WHERE %s;' % item_where,
        'INSERT INTO `item_template_locale` (`ID`, `locale`, `Name`, `Description`) VALUES',
        ',\n'.join(item_rows) + ';',
        '',
        'DELETE FROM `gameobject_template_locale` WHERE `entry` IN (SELECT `entry` FROM `mod_guild_level_decor`);',
        'INSERT INTO `gameobject_template_locale` (`entry`, `locale`, `name`, `castBarCaption`) VALUES',
        ',\n'.join(go_rows) + ';',
        '',
        "DELETE FROM `module_string_locale` WHERE `module` = 'mod-guild-level' AND `id` BETWEEN 170 AND 179 "
        "AND `locale` IN ('deDE', 'esES', 'esMX', 'ruRU');",
        'INSERT INTO `module_string_locale` (`module`, `id`, `locale`, `string`) VALUES',
        ',\n'.join(string_rows) + ';',
        '',
    ]
    open(SQL, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))

    lua = [
        '-- The Customization\'s families, dungeons and zones in each language (tools/decor/build_locales.py):',
        '-- GuildHouseLocale[locale] = { categories = { English name = name }, kinds = { ... },',
        '-- maps = { Map ID = name }, zones = { AreaTable ID = name } }.',
        'if AIO.AddAddon() then return end',
        '',
        'GuildHouseLocale = {',
    ]
    for locale in LOCALES:
        lua.append('    %s = {' % locale)
        for key in ('categories', 'kinds'):
            lua.append('        %s = {' % key)
            for english in sorted(data[key]):
                lua.append('            [%s] = %s,' % (lua_string(english), lua_string(data[key][english].get(locale, english))))
            lua.append('        },')
        for key in ('maps', 'zones'):
            lua.append('        %s = {' % key)
            for ident in sorted(data[key], key=int):
                names = data[key][ident]
                lua.append('            [%s] = %s,' % (ident, lua_string(names.get(locale) or names.get('enUS', ''))))
            lua.append('        },')
        lua.append('    },')
    lua.append('}')
    lua.append('GuildHouseLocale.esMX = GuildHouseLocale.esES')
    lua.append('')
    open(LUA, 'w', encoding='utf-8', newline='\n').write('\n'.join(lua))
    print('%d objects, %d item rows, %d texts; %d maps, %d zones' % (
        len(entries), len(item_rows), len(string_rows), len(data['maps']), len(data['zones'])))


def main(argv):
    if '--fetch' in argv:
        import core  # noqa: E402
        server = core.Server(argv[0])
        world = core.open_databases(server, core.find_mysql(server, core.load_settings()))['world']
        fetch(world)
    build()


if __name__ == '__main__':
    main(sys.argv[1:])
