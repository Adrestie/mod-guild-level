"""Writes a side's client files (<side>_game_files.pkl) into the module's data/art."""
import os
import pickle
import sys

files = pickle.load(open('%s_game_files.pkl' % sys.argv[1], 'rb'))
base = r'E:\WoW-mods\mod-guild-level\data\art'
for k, v in files.items():
    p = os.path.join(base, *k.replace('\\', '/').split('/'))
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, 'wb').write(v)
print(len(files), 'files;', [k for k in files if not k.endswith('.wmo')])
