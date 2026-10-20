"""The guild talent tree of the owner's drawing (guild_talents.png), into the world database."""
import sys

sys.path.insert(0, r'E:\WoW-mods\installer')
import core

# id, x, y, effect, amount per point, max points, chance %, cooldown s, tier
NODES = [
    (1, 0, 0, 60, 0, 1, 0, 0, 1),        # origin
    (2, 0, -2, 1, 10, 10, 0, 0, 1),      # Stamina
    (3, -3, -1, 2, 10, 10, 0, 0, 1),     # Strength
    (4, -3, 2, 3, 10, 10, 0, 0, 1),      # Agility
    (5, 3, -1, 5, 10, 10, 0, 0, 1),      # Spirit
    (6, 3, 2, 4, 10, 10, 0, 0, 1),       # Intellect
    (7, -2, -4, 21, 10, 10, 0, 0, 2),    # block rating
    (8, 0, -4, 20, 10, 10, 0, 0, 2),     # parry rating
    (9, 2, -4, 19, 10, 10, 0, 0, 2),     # dodge rating
    (10, -5, -2, 14, 10, 10, 0, 0, 2),   # armor penetration rating
    (11, -5, -1, 15, 20, 10, 0, 0, 2),   # attack power
    (12, -5, 0, 12, 10, 10, 0, 0, 2),    # hit rating
    (13, -5, 2, 10, 10, 10, 0, 0, 2),    # crit rating
    (14, -5, 3, 13, 10, 10, 0, 0, 2),    # expertise rating
    (15, 5, -1, 28, 12, 10, 0, 0, 2),    # healing power
    (16, 5, 1, 29, 12, 10, 0, 0, 2),     # spell damage
    (17, 5, 2, 11, 10, 10, 0, 0, 2),     # haste rating
    (18, -2, -6, 64, 10, 1, 15, 60, 3),  # block: shield of 10% health
    (19, 0, -6, 65, 2, 1, 15, 8, 3),     # parry: riposte for 2% health
    (20, 2, -6, 66, 2, 1, 100, 1, 3),    # dodge: -2% damage taken, 5 s, 5 stacks
    (21, -7, -2, 61, 100, 1, 10, 15, 3), # hits ignore armor
    (22, -7, -1, 62, 5, 1, 10, 30, 3),   # hits: +5% damage, 10 s
    (23, -7, 0, 63, 1, 1, 10, 20, 3),    # hits strike a second time
    (24, -7, 2, 54, 10, 1, 0, 0, 3),     # crits: +10% damage
    (25, 7, -1, 67, 1, 1, 5, 60, 3),     # next spell free
    (26, 7, 1, 68, 1, 1, 5, 60, 3),      # next spell instant
    (27, 7, 2, 69, 5, 1, 10, 10, 3),     # resource
]
LINKS = [(1, 2), (1, 3), (1, 4), (1, 5), (1, 6), (2, 7), (2, 8), (2, 9), (3, 10), (3, 11), (3, 12), (4, 13), (4, 14),
         (5, 15), (6, 16), (6, 17), (7, 18), (8, 19), (9, 20), (10, 21), (11, 22), (12, 23), (13, 24), (15, 25),
         (16, 26), (17, 27)]

server = core.Server(r'E:\Serveur\bin\RelWithDebInfo')
dbs = core.open_databases(server, core.find_mysql(server, core.load_settings()))
w = dbs['world']
assert w.run('SELECT COUNT(*) FROM mod_guild_level_talent_node;')[0][0] == '0', 'the tree is not empty'
sql = 'INSERT INTO mod_guild_level_talent_node (id, x, y, effect, amount, max_points, chance, cooldown, tier, start, icon) VALUES\n'
sql += ',\n'.join('(%d, %d, %d, %d, %g, %d, %g, %d, %d, 0, \'\')' % n for n in NODES) + ';\n'
sql += 'INSERT INTO mod_guild_level_talent_link (node_a, node_b) VALUES ' + ', '.join('(%d, %d)' % l for l in LINKS) + ';\n'
w.run(sql)
print(w.run('SELECT COUNT(*) FROM mod_guild_level_talent_node;'), w.run('SELECT COUNT(*) FROM mod_guild_level_talent_link;'))
