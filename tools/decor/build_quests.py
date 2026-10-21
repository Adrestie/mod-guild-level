"""The quests that give a Customization object (src/GuildDecor.h, GuildDecorMgr::OnQuestRewarded): every
object goes to quests both factions can do -- a quest open to both, else one quest of each faction
-- so that the Alliance and the Horde can get the same objects. One object a quest. An object of a
theme (loot.json) goes first to the quests of the theme's zones.

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
            open_to[qid] = (alliance, horde, sort)

    catalog = json.load(open(os.path.join(HERE, 'catalog.json'), encoding='utf-8'))
    spec = json.load(open(os.path.join(HERE, 'loot.json'), encoding='utf-8'))
    rng = random.Random(SEED)
    pools = {'both': [], 'alliance': [], 'horde': []}
    for qid, (a, h, zone) in sorted(open_to.items()):
        pools['both' if a and h else 'alliance' if a else 'horde'].append(qid)
    for p in pools.values():
        rng.shuffle(p)
    zone_of = {qid: z for qid, (a, h, z) in open_to.items()}

    def theme_zones(name):
        name = name.lower()
        zones = set()
        for t in spec['themes']:
            if any(re.search(r'(^|[^a-z])' + re.escape(k), name) for k in t['keywords']):
                zones.update(t.get('zones', []))
        return zones

    def take(pool, zones):
        """A quest of the pool, one of the zones if any."""
        if zones:
            for i, q in enumerate(pool):
                if zone_of[q] in zones:
                    return pool.pop(i)
        return pool.pop() if pool else None

    rows = []
    short = 0
    for e in catalog:
        zones = theme_zones(e['name'])
        # a quest open to both while there are, else one of each faction
        q = take(pools['both'], zones) if len(pools['both']) > 0 and (
            not pools['alliance'] or not pools['horde'] or rng.random() < 0.5) else None
        if q:
            rows.append((q, e['decor']))
            continue
        qa, qh = take(pools['alliance'], zones), take(pools['horde'], zones)
        if qa and qh:
            rows += [(qa, e['decor']), (qh, e['decor'])]
            continue
        for q in (qa, qh):
            if q:
                pools['alliance' if q == qa else 'horde'].append(q)
        q = take(pools['both'], zones)
        if q:
            rows.append((q, e['decor']))
        else:
            short += 1

    out = [
        '-- mod-guild-level — the quests that give a Customization object (tools/decor/build_quests.py).',
        '--',
        '-- Rewarding one of these quests, a member of a guild also gets the item of its object (by mail',
        '-- when his bags are full): src/GuildDecor.h. Every object goes to a quest both factions can do,',
        '-- or to one quest of each faction. No quest of the game is changed.',
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
        "DELETE FROM `module_string_locale` WHERE `module` = 'mod-guild-level' AND `id` = 179;",
        "INSERT INTO `module_string_locale` (`module`, `id`, `locale`, `string`) VALUES",
        "('mod-guild-level', 179, 'frFR', 'Pour votre guilde, cette quête rapporte aussi un objet de sa maison : {}.');",
        '',
    ]
    open(SQL, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    both = sum(1 for q, d in rows if open_to[q][0] and open_to[q][1])
    print('%d quests taken (%d open to both, %d of one faction) for %d objects; %d objects without; '
          'eligible: %s' % (len(rows), both, len(rows) - both, len(catalog) - short, short,
                            {k: len(v) for k, v in pools.items()}))


if __name__ == '__main__':
    main(sys.argv[1])
