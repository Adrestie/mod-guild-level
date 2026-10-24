"""The quests that give a Customization object (src/GuildDecor.h, GuildDecorMgr::OnQuestRewarded): about 800
objects go to quests, spread over the expansions and the zones: quests open to both factions first,
then pairs of quests (one of each faction, of the same expansion) giving the same object, so that the Alliance and the Horde can get the same objects. One object a quest (or pair); the
other objects are only looted. A quest's object is one of the theme (loot.json) of its zone if any.

A quest is taken when it can really be done once by anyone: not a test or development quest, not
disabled, not a raid's, not daily, weekly or repeatable, not a class's, a profession's or a
holiday's (no negative QuestSortID), with a quest giver and a quest ender in the world (a spawned
creature or gameobject, or an item that starts it). Its faction: its races, and the faction of its
creature giver (a giver hostile to a faction closes it to that faction).

Reads the world database of a worldserver (its folder: the one that holds worldserver.exe) and the
game's FactionTemplate.dbc; writes data/sql/world/guild_level_14_decor_quests.sql.

    python build_quests.py <worldserver folder>
"""
import json
import os
import random
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, r'E:\WoW-mods\installer')
sys.path.insert(0, os.path.join(HERE, '..', 'house'))
import core  # noqa: E402
import game  # noqa: E402  (tools/house/game.py: the client's files)

SQL = os.path.join(ROOT, 'data', 'sql', 'world', 'guild_level_14_decor_quests.sql')
ALLIANCE_RACES = 1 | 4 | 8 | 64 | 1024          # human, dwarf, night elf, gnome, draenei
HORDE_RACES = 2 | 16 | 32 | 128 | 512           # orc, undead, tauren, troll, blood elf
ALLIANCE_GROUP, HORDE_GROUP = 2, 4              # FactionTemplate groups
QUEST_FLAGS_DAILY, QUEST_FLAGS_WEEKLY = 0x1000, 0x8000
QUEST_INFO_RAID = (62, 88, 89)                   # raid, raid (10), raid (25)
SPECIAL_REPEATABLE = 0x1
BAD_TITLE = re.compile(r"\b(test|testing|unused|deprecated|nyi|dnd|placeholder|zzold|old)\b|\[|<|^zz|^z ", re.I)
SEED = 335
OBJECTS = 800                                    # the objects quests give


def expansion(level):
    return 0 if level <= 60 else 1 if level <= 70 else 2


def faction_groups():
    """FactionTemplate ID -> (hostile to the Alliance, hostile to the Horde)."""
    d = game.read('DBFilesClient' + chr(92) + 'FactionTemplate.dbc')
    n, fields, size, _ = struct.unpack_from('<4I', d, 4)
    out = {}
    for r in range(n):
        row = struct.unpack_from('<%dI' % fields, d, 20 + r * size)
        enemy_group = row[5]
        enemies = row[6:10]
        faction = row[1]
        out[row[0]] = (bool(enemy_group & ALLIANCE_GROUP) or 469 in enemies,
                       bool(enemy_group & HORDE_GROUP) or 67 in enemies)
    return out


def main(server_folder):
    server = core.Server(server_folder)
    dbs = core.open_databases(server, core.find_mysql(server, core.load_settings()))
    world = dbs['world']
    hostile = faction_groups()

    quests = world.run(
        "SELECT q.ID, q.LogTitle, q.QuestSortID, q.QuestInfoID, q.AllowableRaces, q.Flags, q.QuestLevel, "
        "COALESCE(a.SpecialFlags, 0), COALESCE(a.AllowableClasses, 0), COALESCE(a.RequiredSkillID, 0) "
        "FROM quest_template q LEFT JOIN quest_template_addon a ON a.ID = q.ID")
    disabled = {int(r[0]) for r in world.run("SELECT entry FROM disables WHERE sourceType = 1")}
    # givers and enders that are in the world
    giver_factions = {}
    for quest, faction in world.run(
            "SELECT s.quest, t.faction FROM creature_queststarter s JOIN creature_template t ON t.entry = s.id "
            "WHERE EXISTS (SELECT 1 FROM creature c WHERE c.id = s.id)"):
        giver_factions.setdefault(int(quest), []).append(int(faction))
    go_givers = {int(r[0]) for r in world.run(
        "SELECT s.quest FROM gameobject_queststarter s WHERE EXISTS (SELECT 1 FROM gameobject g WHERE g.id = s.id)")}
    item_givers = {int(r[0]) for r in world.run("SELECT startquest FROM item_template WHERE startquest > 0")}
    enders = {int(r[0]) for r in world.run(
        "SELECT e.quest FROM creature_questender e WHERE EXISTS (SELECT 1 FROM creature c WHERE c.id = e.id) "
        "UNION SELECT e.quest FROM gameobject_questender e WHERE EXISTS (SELECT 1 FROM gameobject g WHERE g.id = e.id)")}

    open_to = {}                    # quest -> (alliance, horde, zone)
    for row in quests:
        qid, title, sort, info, races, flags, level, special, classes, skill = row
        qid, sort, info, races, flags, level = int(qid), int(sort), int(info), int(races), int(flags), int(level)
        if qid in disabled or sort < 0 or info in QUEST_INFO_RAID or level <= 0:
            continue
        if flags & (QUEST_FLAGS_DAILY | QUEST_FLAGS_WEEKLY) or int(special) & SPECIAL_REPEATABLE:
            continue
        if int(classes) or int(skill) or BAD_TITLE.search(title or ''):
            continue
        if qid not in enders or not (qid in giver_factions or qid in go_givers or qid in item_givers):
            continue
        alliance = races == 0 or bool(races & ALLIANCE_RACES)
        horde = races == 0 or bool(races & HORDE_RACES)
        if qid in giver_factions:
            # some giver each faction can talk to
            alliance = alliance and any(not hostile.get(f, (False, False))[0] for f in giver_factions[qid])
            horde = horde and any(not hostile.get(f, (False, False))[1] for f in giver_factions[qid])
        if alliance or horde:
            open_to[qid] = (alliance, horde, sort, level)

    catalog = json.load(open(os.path.join(HERE, 'catalog.json'), encoding='utf-8'))
    spec = json.load(open(os.path.join(HERE, 'loot.json'), encoding='utf-8'))
    rng = random.Random(SEED)
    pools = {'both': [], 'alliance': [], 'horde': []}
    for qid, (a, h, zone, level) in open_to.items():
        pools['both' if a and h else 'alliance' if a else 'horde'].append(qid)

    def spread(pool, count):
        """count quests of the pool, spread over its expansions and zones: sorted by expansion and zone,
        one every len/count."""
        pool = sorted(pool, key=lambda q: (expansion(open_to[q][3]), open_to[q][2], rng.random()))
        if not count:
            return []
        step = len(pool) / count
        start = rng.random() * step
        return [pool[int(start + k * step)] for k in range(count)]

    # OBJECTS objects: quests open to both first, then pairs (an Alliance quest and a Horde one of the
    # same expansion) when those run short
    neutral = spread(pools['both'], min(OBJECTS, len(pools['both'])))
    pairs_count = min(OBJECTS - len(neutral), len(pools['alliance']), len(pools['horde']))
    side = {f: sorted(spread(pools[f], pairs_count), key=lambda q: (open_to[q][3], q))
            for f in ('alliance', 'horde')}
    slots = [(q,) for q in neutral] + list(zip(side['alliance'], side['horde']))
    rng.shuffle(slots)

    # an object a slot: one of the theme of its zone if any is left, else any one
    def themes_of(name):
        name = name.lower()
        return [t for t in spec['themes'] if any(re.search(r'(^|[^a-z])' + re.escape(k), name) for k in t['keywords'])]
    by_zone = {}
    catalog = [e for e in catalog if not e.get('removed')]
    objects = [e['decor'] for e in catalog]
    rng.shuffle(objects)
    names = {e['decor']: e['name'] for e in catalog}
    for decor in objects:
        for t in themes_of(names[decor]):
            for z in t.get('zones', []):
                by_zone.setdefault(z, []).append(decor)
    used = set()
    rows = []
    for slot in slots:
        decor = next((d for q in slot for d in by_zone.get(open_to[q][2], []) if d not in used), None)
        if decor is None:
            decor = next(d for d in objects if d not in used)
        used.add(decor)
        rows += [(q, decor) for q in slot]

    out = [
        '-- mod-guild-level — the quests that give a Customization object (tools/decor/build_quests.py).',
        '--',
        '-- Rewarding one of these quests, a member of a guild also gets the item of its object (by mail',
        '-- when his bags are full): src/GuildDecor.h. About 800 objects: a quest open to both factions,',
        '-- or a pair (one of each faction) giving the same object. No quest of the game is changed.',
        '--',
        '-- Idempotent: the table is created once, its rows are written again.',
        '',
        'CREATE TABLE IF NOT EXISTS `mod_guild_level_decor_quest` (',
        "  `quest` INT UNSIGNED NOT NULL COMMENT 'quest_template',",
        "  `decor` INT UNSIGNED NOT NULL COMMENT 'mod_guild_level_decor',",
        '  PRIMARY KEY (`quest`)',
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='GuildLevel: the quests that give a Customization object';",
        '',
        'DELETE FROM `mod_guild_level_decor_quest`;',
        'INSERT INTO `mod_guild_level_decor_quest` (`quest`, `decor`) VALUES',
        ',\n'.join('(%d, %d)' % r for r in sorted(rows)) + ';',
        '',
        "DELETE FROM `module_string` WHERE `module` = 'mod-guild-level' AND `id` = 179;",
        "INSERT INTO `module_string` (`module`, `id`, `string`) VALUES",
        "('mod-guild-level', 179, 'For your guild, this quest also brings an object of its house: {}.');",
        "DELETE FROM `module_string_locale` WHERE `module` = 'mod-guild-level' AND `id` = 179 AND `locale` = 'frFR';",
        "INSERT INTO `module_string_locale` (`module`, `id`, `locale`, `string`) VALUES",
        "('mod-guild-level', 179, 'frFR', 'Pour votre guilde, cette quête rapporte aussi un objet de sa maison : {}.');",
        '',
    ]
    open(SQL, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    eligible = {k: len(v) for k, v in pools.items()}
    taken = [0, 0, 0]
    for q, d in rows:
        taken[expansion(open_to[q][3])] += 1
    print('%d eligible quests %s; %d taken (%d open to both, %d pairs) for %d objects; '
          'Vanilla %d, Burning Crusade %d, Wrath %d' % (sum(eligible.values()), eligible, len(rows), len(neutral),
                                                     len(side['alliance']), len(used), *taken))


if __name__ == '__main__':
    main(sys.argv[1])
