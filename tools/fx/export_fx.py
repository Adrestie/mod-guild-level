"""Exports a modern spell model through the local wow.export bridge (http://127.0.0.1:9455) in the
layout mod-limit-break's m2port export.py writes: <out>/spells/<name>.m2, its views <name>00.skin,
its levels of detail, its textures (raw .blp), its .anim files and <name>.manifest.json.

    python export_fx.py <output folder> <model path>     e.g. spells/tradeskilllevelup.m2
"""
import json
import os
import struct
import sys
import urllib.parse
import urllib.request

BRIDGE = 'http://127.0.0.1:9455'


def call(path, params):
    url = BRIDGE + path + '?' + urllib.parse.urlencode(params, doseq=True)
    with urllib.request.urlopen(url, timeout=900) as r:
        return json.loads(r.read().decode('utf-8'))


def chunk_ids(b, tag):
    off = 0
    while off + 8 <= len(b):
        t, size = struct.unpack_from('<4sI', b, off)
        if t == tag:
            return list(struct.unpack_from('<%dI' % (size // 4), b, off + 8))
        off += 8 + size
    return []


def fetch(fid, path):
    """One file by FileDataID, written at path."""
    tmp = os.path.join(os.path.dirname(path), '_tmp')
    r = call('/export', {'fdid': fid, 'dest': tmp})['exported'][0]
    if not r['ok']:
        raise RuntimeError('%d: %s' % (fid, r.get('error')))
    os.replace(r['path'], path)
    return r['file']


def main(out, model):
    folder = os.path.join(out, 'spells')
    os.makedirs(folder, exist_ok=True)
    name = os.path.splitext(os.path.basename(model))[0].lower()
    res = call('/resolve', {'file': model})['results'][model]
    if not res:
        raise SystemExit(model + ' not in the listfile')
    fid = res['fileDataID']
    m2path = os.path.join(folder, name + '.m2')
    fetch(fid, m2path)
    m2 = open(m2path, 'rb').read()
    manifest = {'fileDataID': fid, 'textures': [], 'skins': [], 'lodSkins': [], 'anims': []}
    o = m2.find(b'MD20')
    views = struct.unpack_from('<I', m2, o + 0x44)[0] if o >= 0 else 1
    for i, sid in enumerate(chunk_ids(m2, b'SFID')):
        if not sid:
            continue
        lod = i >= views
        file = f'{name}_lod{i - views + 1:02d}.skin' if lod else f'{name}{i:02d}.skin'
        fetch(sid, os.path.join(folder, file))
        manifest['lodSkins' if lod else 'skins'].append({'fileDataID': sid, 'file': file})
    for tid in chunk_ids(m2, b'TXID'):
        if not tid:
            continue
        listed = fetch(tid, os.path.join(folder, '_t.blp'))
        file = os.path.basename(listed).lower()
        if not file.endswith('.blp'):
            file = f'{tid}.blp'
        os.replace(os.path.join(folder, '_t.blp'), os.path.join(folder, file))
        manifest['textures'].append({'fileDataID': tid, 'file': file})
    afid = chunk_ids(m2, b'AFID')
    for k in range(0, len(afid), 2):
        anim_id, variation = afid[k] & 0xFFFF, afid[k] >> 16
        if not afid[k + 1]:
            continue
        file = f'{name}{anim_id:04d}-{variation:02d}.anim'
        fetch(afid[k + 1], os.path.join(folder, file))
        manifest['anims'].append({'animID': anim_id, 'subAnimID': variation, 'fileDataID': afid[k + 1], 'file': file})
    json.dump(manifest, open(os.path.join(folder, name + '.manifest.json'), 'w'), indent=1)
    print(name, len(manifest['skins']), 'skin(s)', len(manifest['textures']), 'texture(s)',
          len(manifest['anims']), 'anim(s)')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
