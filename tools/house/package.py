"""Game files of a built hall (WMO + the map's WDT), and the server's collision and navigation
data made from them with AzerothCore's own tools, map by map.

python package.py alliance|horde"""
import os
import pickle
import shutil
import struct
import subprocess
import sys

sys.path.insert(0, r'E:\WoW-mods\installer')
import mpq_archive  # noqa: E402

import game
import wmo

TOOLS = r'E:\Serveur\bin\RelWithDebInfo'
WORK = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'extract')

SIDES = {
    'alliance': dict(map=449, files='alliance_files.pkl', wmo='World\\GuildLevel\\House\\GuildHall_Alliance.wmo'),
    'horde': dict(map=450, files='horde_files.pkl', wmo='World\\GuildLevel\\House\\GuildHall_Horde.wmo'),
}


def map_dir(mapid):
    dbc = game.read('DBFilesClient\\Map.dbc')
    n, fields, rsize, ssize = struct.unpack_from('<4I', dbc, 4)
    strings = dbc[20 + n * rsize:]
    for r in range(n):
        rid, off = struct.unpack_from('<II', dbc, 20 + r * rsize)
        if rid == mapid:
            return strings[off:strings.index(b'\0', off)].decode()


def chunks(data):
    i = 0
    while i + 8 <= len(data):
        tag = data[i:i + 4]
        size = struct.unpack_from('<I', data, i + 4)[0]
        yield tag, data[i + 8:i + 8 + size]
        i += 8 + size


def patch_wdt(data, wmo_name, bbox):
    out = b''
    for tag, body in chunks(data):
        if tag == b'OMWM':        # MWMO, reversed
            body = wmo_name.encode('latin1') + b'\0'
        elif tag == b'FDOM':      # MODF
            body = bytearray(body)
            # placement space: (y, z, x) of the WMO's own axes
            x0, y0, z0, x1, y1, z1 = bbox
            struct.pack_into('<6f', body, 32, y0, z0, x0, y1, z1, x1)
            body = bytes(body)
        out += tag + struct.pack('<I', len(body)) + body
    return out


def game_files(side):
    s = SIDES[side]
    built = pickle.load(open(s['files'], 'rb'))
    old_root = [k for k in built if not k[-8:-4].startswith('_')][0]
    files = {}
    for k, v in built.items():
        files[s['wmo'][:-4] + k[len(old_root) - 4:]] = v
    d = map_dir(s['map'])
    wdt_name = 'World\\Maps\\%s\\%s.wdt' % (d, d)
    root = wmo.Root(files[s['wmo']])
    files[wdt_name] = patch_wdt(game.read(wdt_name), s['wmo'], root.bbox)
    return files, root


def doodad_models(root):
    names = set()
    for dd in root.doodads:
        n = root.modn.get(dd[0] & 0xFFFFFF)
        if n:
            names.add(n[:-4] + '.m2' if n.lower().endswith(('.mdx', '.mdl')) else n)
    return sorted(names)


def extract(side):
    s = SIDES[side]
    files, root = game_files(side)
    src = dict(files)
    src['DBFilesClient\\Map.dbc'] = game.read('DBFilesClient\\Map.dbc')
    src['DBFilesClient\\GameObjectDisplayInfo.dbc'] = game.read('DBFilesClient\\GameObjectDisplayInfo.dbc')
    missing = []
    for m in doodad_models(root):
        data = game.read(m)
        if data is None:
            missing.append(m)
        else:
            src[m] = data
    if missing:
        print('doodad models not found:', missing)
    work = os.path.join(WORK, side)
    if os.path.isdir(work):
        shutil.rmtree(work)
    os.makedirs(os.path.join(work, 'Data'))
    mpq_archive.create_archive(os.path.join(work, 'Data', 'patch.MPQ'), {k: v for k, v in src.items()},
                               hash_entries=4096)
    run = lambda *a: subprocess.run(list(a), cwd=work, check=True, input=b'\n', stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT)
    r = run(os.path.join(TOOLS, 'vmap4_extractor.exe'), '-d', os.path.join(work, 'Data') + os.sep)
    open(os.path.join(work, 'vmap4_extractor.log'), 'wb').write(r.stdout)
    os.makedirs(os.path.join(work, 'vmaps'), exist_ok=True)
    r = run(os.path.join(TOOLS, 'vmap4_assembler.exe'), 'Buildings', 'vmaps')
    open(os.path.join(work, 'vmap4_assembler.log'), 'wb').write(r.stdout)
    os.makedirs(os.path.join(work, 'maps'), exist_ok=True)
    open(os.path.join(work, 'maps', 'keep.txt'), 'w').write('the generator wants a maps folder that is not empty\n')
    os.makedirs(os.path.join(work, 'mmaps'), exist_ok=True)
    cfg = open(os.path.join(TOOLS, 'mmaps-config.yaml')).read()
    open(os.path.join(work, 'mmaps-config.yaml'), 'w').write(cfg)
    r = run(os.path.join(TOOLS, 'mmaps_generator.exe'), '--config', os.path.join(work, 'mmaps-config.yaml'),
            str(s['map']))
    open(os.path.join(work, 'mmaps_generator.log'), 'wb').write(r.stdout)
    return files


if __name__ == '__main__':
    side = sys.argv[1]
    files = extract(side)
    pickle.dump(files, open('%s_game_files.pkl' % side, 'wb'))
    work = os.path.join(WORK, side)
    for d in ('vmaps', 'mmaps'):
        print(d, sorted(os.listdir(os.path.join(work, d)))[:40])
