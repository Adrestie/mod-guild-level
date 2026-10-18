"""The spells of the guild level's active perks: SQL for the server (spell_dbc) and client Spell.dbc
rows in installer.json, from one definition."""
import json
import struct

MASK = 0xFF01FE
ONE = struct.unpack('<I', struct.pack('<f', 1.0))[0]
# locales of the client's string columns: enUS koKR frFR deDE zhCN zhTW esES esMX ruRU
LOC = {'en': 0, 'fr': 2, 'de': 3, 'es': 6, 'mx': 7, 'ru': 8}

SPELLS = [
    dict(id=92021, icon=164, cast=1, visual=0,
         name=dict(en='Guild Rally', fr='Ralliement de guilde', de='Gildensammlung', es='Reunión de hermandad',
                   ru='Сбор гильдии'),
         desc=dict(en='Asks every member of your group to come to you.',
                   fr='Propose à chaque membre de votre groupe de vous rejoindre.',
                   de='Bittet jedes Mitglied Eurer Gruppe, zu Euch zu kommen.',
                   es='Pide a cada miembro de tu grupo que venga hasta ti.',
                   ru='Предлагает каждому участнику группы переместиться к вам.')),
    dict(id=92022, icon=121, cast=1, visual=0,
         name=dict(en='Mass Resurrection', fr='Résurrection de masse', de='Massenwiederbelebung',
                   es='Resurrección en masa', ru='Массовое воскрешение'),
         desc=dict(en='Offers a resurrection to the dead of your group nearby.',
                   fr='Propose une résurrection aux morts de votre groupe qui sont près de vous.',
                   de='Bietet den Toten Eurer Gruppe in der Nähe eine Wiederbelebung an.',
                   es='Ofrece una resurrección a los muertos de tu grupo cercanos.',
                   ru='Предлагает воскрешение погибшим участникам группы поблизости.')),
    dict(id=92023, icon=3708, cast=1, visual=0,
         name=dict(en='Mobile Banking', fr='Banque mobile', de='Mobile Bank', es='Banca móvil', ru='Мобильный банк'),
         desc=dict(en='Calls a guild chest to you for a few minutes.',
                   fr='Fait venir un coffre de guilde près de vous pendant quelques minutes.',
                   de='Ruft für einige Minuten eine Gildentruhe zu Euch.',
                   es='Hace aparecer un cofre de hermandad a tu lado durante unos minutos.',
                   ru='Призывает к вам сундук гильдии на несколько минут.')),
    dict(id=92024, icon=2801, cast=1, visual=0,
         name=dict(en='Guild Supplier', fr='Fournisseur de guilde', de='Gildenversorger', es='Proveedor de hermandad',
                   ru='Поставщик гильдии'),
         desc=dict(en='Calls the Guild Supplier, reagents and repairs, for a few minutes.',
                   fr='Fait venir le Fournisseur de guilde, composants et réparations, pendant quelques minutes.',
                   de='Ruft den Gildenversorger, Reagenzien und Reparaturen, für einige Minuten herbei.',
                   es='Hace venir al Proveedor de hermandad, componentes y reparaciones, durante unos minutos.',
                   ru='Призывает поставщика гильдии (реагенты и ремонт) на несколько минут.')),
    # the Hearthstone's cast: 10 seconds, its visual, broken by moving or damage
    dict(id=92025, icon=2637, cast=7, visual=220, interrupt=31,
         attributes={4: 0x8010000, 5: 0x400, 7: 0x20000000},   # the Hearthstone's
         name=dict(en='Guild House', fr='Maison de guilde', de='Gildenhaus', es='Casa de hermandad', ru='Дом гильдии'),
         desc=dict(en="Takes you to your guild's house; inside, takes you back where you came from.",
                   fr="Vous emmène dans la maison de votre guilde ; à l'intérieur, vous ramène d'où vous veniez.",
                   de='Bringt Euch ins Haus Eurer Gilde; darin bringt er Euch dorthin zurück, woher Ihr kamt.',
                   es='Te lleva a la casa de tu hermandad; dentro, te devuelve a donde estabas.',
                   ru='Переносит вас в дом гильдии; внутри дома — обратно туда, откуда вы пришли.')),
]


# THE GUILD TALENTS' AURA: visible, on the members of a guild with talents, kept through death. Its
# effects: run speed (0) and maximum health (1), set by the module; the tooltip's lines are the
# talent window's client file's.
TALENT_AURA = dict(id=92026, icon=1800,
    name=dict(en='Guild Talents', fr='Talents de guilde', de='Gildentalente', es='Talentos de hermandad',
              ru='Таланты гильдии'),
    desc=dict(en="The talents of your guild.", fr='Les talents de votre guilde.', de='Die Talente Eurer Gilde.',
              es='Los talentos de tu hermandad.', ru='Таланты вашей гильдии.'),
    aura=dict(en="The talents of your guild grant:", fr='Les talents de votre guilde accordent :',
              de='Die Talente Eurer Gilde gewähren:', es='Los talentos de tu hermandad otorgan:',
              ru='Таланты вашей гильдии дают:'))


def aura_fields(sp):
    f = {0: sp['id'], 4: 0x80000000, 7: 0x100000, 28: 1, 35: 101, 40: 21, 46: 1, 68: -1,
         71: 6, 72: 6, 73: 6, 74: 1, 75: 1, 76: 1, 80: -1, 81: -1, 82: -1, 86: 1, 87: 1, 88: 1,
         95: 31, 96: 133, 97: 4, 133: sp['icon'], 216: ONE, 217: ONE, 218: ONE, 225: 1,
         152: MASK, 169: MASK, 186: MASK, 203: MASK}
    for loc, i in LOC.items():
        lang = 'es' if loc == 'mx' else loc
        f[136 + i] = sp['name'][lang]
        f[170 + i] = sp['desc'][lang]
        f[187 + i] = sp['aura'][lang]
    return f


def fields(sp):
    if 'aura' in sp:
        return aura_fields(sp)
    f = {
        0: sp['id'], 28: sp['cast'], 31: sp.get('interrupt', 0), 46: 1, 68: -1,
        71: 3, 74: 1, 86: 1, 216: ONE, 217: ONE, 218: ONE,
        131: sp['visual'], 133: sp['icon'], 205: 133, 206: 1500, 225: 1,
        152: MASK, 169: MASK, 186: MASK, 203: MASK,
    }
    f.update(sp.get('attributes', {}))
    for loc, i in LOC.items():
        lang = 'es' if loc == 'mx' else loc
        f[136 + i] = sp['name'][lang]
        f[170 + i] = sp['desc'][lang]
    return f


# server: spell_dbc, columns in Spell.dbc order (1-based in the table)
COLS = None


def sql():
    import sys
    sys.path.insert(0, r'E:\WoW-mods\installer')
    import core
    server = core.Server(r'E:\Serveur\bin\RelWithDebInfo')
    dbs = core.open_databases(server, core.find_mysql(server, core.load_settings()))
    cols = [r[0] for r in dbs['world'].run(
        "SELECT COLUMN_NAME FROM information_schema.columns WHERE table_schema = DATABASE() "
        "AND table_name = 'spell_dbc' ORDER BY ORDINAL_POSITION;")]
    assert len(cols) == 234
    out = []
    for sp in SPELLS + [TALENT_AURA]:
        f = fields(sp)
        names = [cols[i] for i in sorted(f)]
        vals = []
        for i in sorted(f):
            v = f[i]
            if i in (216, 217, 218):
                vals.append('1.0')
            elif isinstance(v, str):
                vals.append("'%s'" % v.replace("'", "''"))
            else:
                vals.append(str(v))
        out.append('INSERT INTO `spell_dbc` (%s)\nVALUES (%s);' % (
            ', '.join('`%s`' % n for n in names), ', '.join(vals)))
    return out


if __name__ == '__main__':
    ids = ', '.join(str(s['id']) for s in SPELLS + [TALENT_AURA])
    text = """-- mod-guild-level — the active perks as spells, in the General tab of the
-- spellbook (spells %s, tranche 92 of the WoW-mods registry).
--
-- Server side: AzerothCore completes Spell.dbc from `spell_dbc` at startup;
-- the client has the same rows (installer.json, DBC Spell.dbc). Each spell is
-- a dummy effect on the caster that does what its `.gperk` command does
-- (spell_guild_level_perk); the module teaches them to the members of a guild
-- of the level and sets their cooldowns. The guild house's spell has the
-- Hearthstone's 10-second cast. Spell 92026 is the guild talents' aura.
--
-- Idempotent: every row is deleted before being written again.

DELETE FROM `spell_dbc` WHERE `ID` IN (%s);
%s

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_guild_level_perk';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
%s;

-- THE WAY OUT OF THE HALLS: back where the player came from (at_guild_level_house_exit).
DELETE FROM `areatrigger_scripts` WHERE `ScriptName` = 'at_guild_level_house_exit';
INSERT INTO `areatrigger_scripts` (`entry`, `ScriptName`) VALUES
(2530, 'at_guild_level_house_exit'),
(2534, 'at_guild_level_house_exit');
""" % (ids, ids, '\n'.join(sql()), ',\n'.join("(%d, 'spell_guild_level_perk')" % s['id'] for s in SPELLS))
    open(r'E:\WoW-mods\mod-guild-level\data\sql\world\guild_level_09_perk_spells.sql', 'w', encoding='utf-8',
         newline='\n').write(text)

    p = r'E:\WoW-mods\mod-guild-level\installer.json'
    m = json.load(open(p, encoding='utf-8'))
    rows = [{str(k): v for k, v in sorted(fields(sp).items())} for sp in SPELLS + [TALENT_AURA]]
    m['dbc'] = [d for d in m['dbc'] if d['file'] != 'Spell.dbc'] + [{
        'file': 'Spell.dbc', 'fields': 234, 'text_fields': [[136, 151], [153, 168], [170, 185], [187, 202]],
        'client': True, 'server': False, 'rows': rows}]
    world = m['database']['world']['rows']
    world[:] = [r for r in world if r != {'table': 'spell_dbc', 'where': 'ID BETWEEN 92021 AND 92025'}]
    for r in ({'table': 'spell_dbc', 'where': 'ID BETWEEN 92021 AND 92026'},
              {'table': 'spell_script_names', 'where': "ScriptName = 'spell_guild_level_perk'"}):
        if r not in world:
            world.insert(len(world) - 1, r)
    open(p, 'w', encoding='utf-8', newline='\n').write(json.dumps(m, indent=2, ensure_ascii=False) + '\n')
    print('ok')
