"""Puts the Horde wings' spots (horde_rows.sql) into the module's layout SQL and the world database,
row by row (only those ids, map 450)."""
import re
import sys

sys.path.insert(0, r'E:\WoW-mods\installer')
import core

NEW = {}
for line in open('horde_rows.sql', encoding='utf-8'):
    m = re.match(r"\((\d+), 450, \d+, \d+, \d+, ([-\d.]+), ([-\d.]+), ([-\d.]+), ([-\d.]+), ", line)
    if m:
        NEW[int(m.group(1))] = tuple(m.group(k) for k in range(2, 6))

p = r'E:\WoW-mods\mod-guild-level\data\sql\world\guild_level_08_house_layout.sql'
s = open(p, encoding='utf-8').read()
done = []


def fix(m):
    rid = int(m.group(2))
    if rid not in NEW:
        return m.group(0)
    done.append(rid)
    return '%s%s, %s, %s, %s%s' % (m.group(1), *NEW[rid], m.group(3))


s = re.sub(r"(?m)^(\((\d+), 450, \d+, \d+, \d+, )[-\d.]+, [-\d.]+, [-\d.]+, [-\d.]+(, '.*)$", fix, s)
assert sorted(done) == sorted(NEW), set(NEW) - set(done)
open(p, 'w', encoding='utf-8', newline='\n').write(s)

server = core.Server(r'E:\Serveur\bin\RelWithDebInfo')
dbs = core.open_databases(server, core.find_mysql(server, core.load_settings()))
sql = ''.join("UPDATE mod_guild_level_house_spawn SET x=%s, y=%s, z=%s, o=%s WHERE id=%d AND map=450;\n"
              % (*v, k) for k, v in sorted(NEW.items()))
dbs['world'].run(sql)
print(len(done), 'rows in the file and the database')
