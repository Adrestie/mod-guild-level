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
# effects: run speed (0), maximum health (1) and critical damage (2), set by the module; the tooltip's lines are the
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
         95: 31, 96: 133, 97: 163, 112: 127, 133: sp['icon'], 216: ONE, 217: ONE, 218: ONE, 225: 1,
         152: MASK, 169: MASK, 186: MASK, 203: MASK}
    for loc, i in LOC.items():
        lang = 'es' if loc == 'mx' else loc
        f[136 + i] = sp['name'][lang]
        f[170 + i] = sp['desc'][lang]
        f[187 + i] = sp['aura'][lang]
    return f


# THE SPECIAL EFFECTS OF THE TALENTS. 92027 is hidden and server side only: a passive aura whose
# procs (hits done, melee taken, spells cast) spell_guild_talent_procs turns into the effects below.
PROC_AURA = {0: 92027, 4: 0x800001C0, 7: 0x100000, 28: 1, 34: 0x4 | 0x8 | 0x10 | 0x20 | 0x40 | 0x100 | 0x400 | 0x1000
             | 0x4000 | 0x10000, 35: 101, 40: 21, 46: 1, 68: -1, 71: 6, 74: 1, 80: -1, 86: 1, 95: 4, 216: ONE,
             217: ONE, 218: ONE, 225: 1, 136: 'Guild Talents (procs)', 152: MASK, 169: MASK, 186: MASK, 203: MASK}

# (class, its spell family): the free and instant spells must share the family of the spells they change.
FAMILIES = [(1, 4), (2, 10), (3, 9), (4, 8), (5, 6), (6, 15), (7, 11), (8, 3), (9, 5), (11, 7)]


def buff(id, icon, name, desc, aura, misc=0, base=-1, duration=1, stack=0, charges=0, family=0, target=1,
         spell_effect=6, school=1, range_index=1, every_spell=False):
    """A visible spell of one effect: an aura (spell_effect 6) or anything else."""
    f = {0: id, 28: 1, 35: 101, 40: duration, 46: range_index, 68: -1, 71: spell_effect, 74: 1, 80: base,
         86: target, 95: aura, 110: misc, 133: icon, 216: ONE, 217: ONE, 218: ONE, 225: school,
         152: MASK, 169: MASK, 186: MASK, 203: MASK}
    if stack:
        f[49] = stack
    if charges:
        f[36] = charges
    if family:
        f[208] = family
    if every_spell:
        f[122] = f[125] = f[128] = 0xFFFFFFFF
    for loc, i in LOC.items():
        lang = 'es' if loc == 'mx' else loc
        f[136 + i] = name[lang]
        f[170 + i] = desc[lang]
        f[187 + i] = desc[lang]
    return f


def T(en, fr, de, es, ru):
    return dict(en=en, fr=fr, de=de, es=es, ru=ru)


BUFFS = [
    buff(92030, 38, T('Guild Fervor', 'Ferveur de guilde', 'Gildeneifer', 'Fervor de hermandad', 'Пыл гильдии'),
         T('Damage done increased.', 'Dégâts infligés augmentés.', 'Verursachter Schaden erhöht.',
           'Daño infligido aumentado.', 'Наносимый урон увеличен.'), 79, misc=127, duration=1),
    buff(92031, 566, T('Guild Bulwark', 'Rempart de guilde', 'Gildenbollwerk', 'Baluarte de hermandad', 'Оплот гильдии'),
         T('Absorbs damage.', 'Absorbe des dégâts.', 'Absorbiert Schaden.', 'Absorbe daño.', 'Поглощает урон.'),
         69, misc=127, duration=1),
    buff(92032, 178, T('Guild Evasion', 'Dérobade de guilde', 'Gildenausweichen', 'Evasión de hermandad', 'Уклонение гильдии'),
         T('Damage taken reduced.', 'Dégâts subis réduits.', 'Erlittener Schaden verringert.', 'Daño recibido reducido.',
           'Получаемый урон уменьшен.'), 87, misc=127, duration=28, stack=5),
    buff(92033, 278, T('Guild Riposte', 'Riposte de guilde', 'Gildenriposte', 'Réplica de hermandad', 'Ответный удар гильдии'),
         T('Strikes back.', 'Contre-attaque.', 'Schlägt zurück.', 'Contraataca.', 'Ответный удар.'),
         0, spell_effect=2, target=6, range_index=6),
    buff(92034, 565, T('Guild Sunder', 'Brèche de guilde', 'Gildenbresche', 'Brecha de hermandad', 'Пролом гильдии'),
         T("Your attacks ignore part of the target's armor.", "Vos attaques ignorent une partie de l'armure de la cible.",
           'Eure Angriffe ignorieren einen Teil der Rüstung des Ziels.',
           'Tus ataques ignoran parte de la armadura del objetivo.', 'Ваши атаки игнорируют часть брони цели.'),
         280, duration=27),
]
for cls, family in FAMILIES:
    BUFFS.append(buff(92040 + cls, 212,
                      T('Guild Inspiration', 'Inspiration de guilde', 'Gildeninspiration', 'Inspiración de hermandad',
                        'Вдохновение гильдии'),
                      T('Your next spell costs nothing.', 'Votre prochain sort ne coûte rien.',
                        'Euer nächster Zauber kostet nichts.', 'Tu próximo hechizo no cuesta nada.',
                        'Ваше следующее заклинание ничего не стоит.'),
                      108, misc=14, base=-101, duration=8, charges=1, family=family, every_spell=True))
    BUFFS.append(buff(92060 + cls, 139,
                      T('Guild Celerity', 'Célérité de guilde', 'Gildeneile', 'Celeridad de hermandad', 'Стремительность гильдии'),
                      T('Your next spell is instant.', 'Votre prochain sort est instantané.',
                        'Euer nächster Zauber wird sofort gewirkt.', 'Tu próximo hechizo es instantáneo.',
                        'Ваше следующее заклинание будет мгновенным.'),
                      108, misc=10, base=-101, duration=8, charges=1, family=family, every_spell=True))


def fields(sp):
    if isinstance(sp, dict) and 0 in sp:
        return sp
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
    for sp in SPELLS + [TALENT_AURA, PROC_AURA] + BUFFS:
        f = fields(sp)
        names = [cols[i] for i in sorted(f)]
        vals = []
        for i in sorted(f):
            v = f[i]
            if i in (216, 217, 218):
                vals.append('1.0')
            elif v == ONE:
                vals.append('1.0')
            elif isinstance(v, str):
                vals.append("'%s'" % v.replace("'", "''"))
            else:
                vals.append(str(v))
        out.append('INSERT INTO `spell_dbc` (%s)\nVALUES (%s);' % (
            ', '.join('`%s`' % n for n in names), ', '.join(vals)))
    return out


if __name__ == '__main__':
    ids = ', '.join(str(s['id']) for s in SPELLS + [TALENT_AURA]) + ', ' + \
        ', '.join(str(b[0]) for b in [PROC_AURA] + BUFFS)
    text = """-- mod-guild-level — the active perks as spells, in the General tab of the
-- spellbook (spells %s, tranche 92 of the WoW-mods registry).
--
-- Server side: AzerothCore completes Spell.dbc from `spell_dbc` at startup;
-- the client has the same rows (installer.json, DBC Spell.dbc). Each spell is
-- a dummy effect on the caster that does what its `.gperk` command does
-- (spell_guild_level_perk); the module teaches them to the members of a guild
-- of the level and sets their cooldowns. The guild house's spell has the
-- Hearthstone's 10-second cast. Spell 92026 is the guild talents' aura; 92027
-- the hidden aura of their special effects (server side only), whose procs
-- spell_guild_talent_procs turns into 92030-92034, and, by class, 92041-92051
-- (next spell free) and 92061-92071 (next spell instant).
--
-- Idempotent: every row is deleted before being written again.

DELETE FROM `spell_dbc` WHERE `ID` IN (%s);
%s

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_guild_level_perk';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
%s;

-- THE PROCS OF THE TALENTS: hits, melee blocked, parried or dodged, spells.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_guild_talent_procs';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (92027, 'spell_guild_talent_procs');
DELETE FROM `spell_proc` WHERE `SpellId` = 92027;
INSERT INTO `spell_proc` (`SpellId`, `HitMask`, `SpellPhaseMask`, `Chance`) VALUES (92027, 9331, 2, 100);

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
    rows = [{str(k): v for k, v in sorted(fields(sp).items())} for sp in SPELLS + [TALENT_AURA] + BUFFS]
    m['dbc'] = [d for d in m['dbc'] if d['file'] != 'Spell.dbc'] + [{
        'file': 'Spell.dbc', 'fields': 234, 'text_fields': [[136, 151], [153, 168], [170, 185], [187, 202]],
        'client': True, 'server': False, 'rows': rows}]
    world = m['database']['world']['rows']
    world[:] = [r for r in world if r not in ({'table': 'spell_dbc', 'where': 'ID BETWEEN 92021 AND 92025'},
                                              {'table': 'spell_dbc', 'where': 'ID BETWEEN 92021 AND 92026'})]
    for r in ({'table': 'spell_dbc', 'where': 'ID BETWEEN 92021 AND 92071'},
              {'table': 'spell_script_names', 'where': "ScriptName = 'spell_guild_talent_procs'"},
              {'table': 'spell_proc', 'where': 'SpellId = 92027'},
              {'table': 'spell_script_names', 'where': "ScriptName = 'spell_guild_level_perk'"}):
        if r not in world:
            world.insert(len(world) - 1, r)
    open(p, 'w', encoding='utf-8', newline='\n').write(json.dumps(m, indent=2, ensure_ascii=False) + '\n')
    print('ok')
