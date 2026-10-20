"""The guild level-up's visual effect: the modern model spells/tradeskilllevelup.m2 (exported with
export_fx.py) laid out for the client's modern M2 plugin (WarcraftXL, as mod-limit-break's visuals) in
data/art/Spells/GuildLevel, and its client rows: SpellVisualEffectName, SpellVisualKit (the model at
the feet) and SpellVisual 92028 (the kit played once at the cast), into installer.json. Spell 92028
itself is tools/perk_spells.py's.

    python build_fx.py <export folder>        (the folder export_fx.py wrote)
"""
import json
import os
import struct
import sys

sys.path.insert(0, 'E:/WoW-mods/mod-limit-break/tools/m2port')
import pack  # noqa: E402

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
GAME_DIR = 'Spells\\GuildLevel'
NAME = 'guild_levelup'
VISUAL = 92028
SCALE = 1.0                         # the model's size: SpellVisualEffectName
MIN_SCALE = struct.unpack('<i', struct.pack('<f', 0.01))[0]
MAX_SCALE = struct.unpack('<i', struct.pack('<f', 100.0))[0]
DBC_FILES = {
    'SpellVisualEffectName.dbc': {'fields': 7, 'text_fields': [1, 2]},
    'SpellVisualKit.dbc': {'fields': 38},
    'SpellVisual.dbc': {'fields': 32},
}


def main(export_dir):
    src = os.path.join(export_dir, 'spells', 'tradeskilllevelup.m2')
    out = os.path.join(ROOT, 'data', 'art', *GAME_DIR.split('\\'))
    for f in os.listdir(out) if os.path.isdir(out) else []:
        os.remove(os.path.join(out, f))
    notes = pack.pack(src, out, NAME, GAME_DIR, {}, None)
    for n in notes:
        print(n)
    rows = {
        'SpellVisualEffectName.dbc': [{'0': VISUAL, '1': NAME, '2': GAME_DIR + '\\' + NAME + '.mdx', '3': 0,
                                       '4': struct.unpack('<i', struct.pack('<f', SCALE))[0],
                                       '5': MIN_SCALE, '6': MAX_SCALE}],
        # the model at the base of the unit (field 5)
        'SpellVisualKit.dbc': [{'0': VISUAL, '1': -1, '2': -1, '5': VISUAL, '17': -1, '18': -1, '19': -1, '20': -1}],
        # played once on the caster at the cast (field 2)
        'SpellVisual.dbc': [{'0': VISUAL, '2': VISUAL, '10': 1, '16': -1}],
    }
    p = os.path.join(ROOT, 'installer.json')
    m = json.load(open(p, encoding='utf-8'))
    m['dbc'] = [d for d in m['dbc'] if d['file'] not in DBC_FILES] + [
        {'file': name, **spec, 'client': True, 'server': False, 'rows': rows[name]} for name, spec in DBC_FILES.items()]
    owned = m['game_files'].setdefault('owned_folders', [])
    for folder in ('Spells/GuildLevel', 'Sound/GuildLevel', 'Interface/GuildLevel'):
        if folder not in owned:
            owned.append(folder)
    open(p, 'w', encoding='utf-8', newline='\n').write(json.dumps(m, indent=2, ensure_ascii=False) + '\n')
    print(sorted(os.listdir(out)))


if __name__ == '__main__':
    main(sys.argv[1])
