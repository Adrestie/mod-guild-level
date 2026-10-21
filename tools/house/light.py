"""The guild halls' light. Their groups are exterior (so that the Customization editor's previews,
held by the terrain under the hall, are drawn from inside), and exterior groups are lit by the map's
light, which maps 449 and 450 have none of: the client's default is far too bright. One light for
each hall, all over the map, with one set of parameters that never changes with the time of day: a
dim warm direct light and an ambient near the hall's indoor one; the rest (sky, fog, water) the
noon of Azeroth's own light (LightParams 12). Written into installer.json as client rows of
Light.dbc, LightParams.dbc, LightIntBand.dbc and LightFloatBand.dbc.

    python light.py
"""
import json
import os
import struct
import sys

import game

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
LIGHTS = {449: 92200, 450: 92201}  # map: Light row
PARAMS = 92200                      # LightParams (and its bands, ID * 18 - 17 + n and ID * 6 - 5 + n)
SOURCE = 12                         # Azeroth's LightParams, the rest taken from
NOON = 1440                         # in half-minutes
DIRECT = 0x6e6458                   # band 0: the direct light (RGB)
AMBIENT = 0x5a544c                  # band 1: the ambient light (RGB)
DEATH = 4                           # the light the game shows a dead player (Light's 5th set)
DBC_FILES = {
    'Light.dbc': {'fields': 15},
    'LightParams.dbc': {'fields': 9},
    'LightIntBand.dbc': {'fields': 34},
    'LightFloatBand.dbc': {'fields': 34},
}


def rows(name):
    d = game.read('DBFilesClient' + chr(92) + name)
    n, fields, size, _ = struct.unpack_from('<4I', d, 4)
    return {struct.unpack_from('<I', d, 20 + r * size)[0]: d[20 + r * size:20 + (r + 1) * size] for r in range(n)}


def bits(f):
    return struct.unpack('<i', struct.pack('<f', f))[0]


def at_noon(times, values, n):
    """A band's value at noon (the last key not after it)."""
    best = values[0]
    for k in range(n):
        if times[k] <= NOON:
            best = values[k]
    return best


def band(row_id, value):
    """One key, at time 0: the same all day."""
    return {'0': row_id, '1': 1, '2': 0, '18': value}


def main():
    params = rows('LightParams.dbc')[SOURCE]
    p = list(struct.unpack('<4I5f', params))
    light_params = {'0': PARAMS, '1': 0, '2': 0, '3': p[3], '4': bits(0.0),      # no glow (bloom)
                    '5': bits(p[5]), '6': bits(p[6]), '7': bits(p[7]), '8': bits(p[8])}

    ints = rows('LightIntBand.dbc')
    int_bands = []
    for n in range(18):
        v = struct.unpack('<34I', ints[SOURCE * 18 - 17 + n])
        value = DIRECT if n == 0 else AMBIENT if n == 1 else at_noon(v[2:18], v[18:34], v[1])
        int_bands.append(band(PARAMS * 18 - 17 + n, value))

    floats = rows('LightFloatBand.dbc')
    float_bands = []
    for n in range(6):
        v = struct.unpack('<2I16I16f', floats[SOURCE * 6 - 5 + n])
        float_bands.append(band(PARAMS * 6 - 5 + n, bits(at_noon(v[2:18], v[18:34], v[1]))))

    lights = [{'0': light, '1': mapid, '7': PARAMS, '8': PARAMS, '9': PARAMS, '10': PARAMS, '11': DEATH}
              for mapid, light in sorted(LIGHTS.items())]

    new = {'Light.dbc': lights, 'LightParams.dbc': [light_params], 'LightIntBand.dbc': int_bands,
           'LightFloatBand.dbc': float_bands}
    path = os.path.join(ROOT, 'installer.json')
    m = json.load(open(path, encoding='utf-8'))
    m['dbc'] = [d for d in m['dbc'] if d['file'] not in DBC_FILES] + [
        {'file': name, **spec, 'client': True, 'server': False, 'rows': new[name]} for name, spec in DBC_FILES.items()]
    open(path, 'w', encoding='utf-8', newline='\n').write(json.dumps(m, indent=2, ensure_ascii=False) + '\n')
    print({k: len(v) for k, v in new.items()})


if __name__ == '__main__':
    sys.exit(main())
