"""Exports the models of catalog.json through the local wow.export bridge (http://127.0.0.1:9455,
retail loaded) in the layout mod-limit-break's m2port reads: <out>/<name>/<name>.m2, its views
<name>00.skin, its levels of detail, its textures (raw .blp), its .anim files and
<name>.manifest.json; and each object's catalogue picture, <out>/thumbnails/<decor>.png. A model
already exported is kept.

    python export_decor.py <output folder>
"""
import json
import os
import struct
import sys
import urllib.parse
import urllib.request

BRIDGE = 'http://127.0.0.1:9455'
HERE = os.path.dirname(os.path.abspath(__file__))


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
    """One file by FileDataID, written at path; its listfile name."""
    tmp = os.path.join(os.path.dirname(path), '_tmp')
    r = call('/export', {'fdid': fid, 'dest': tmp})['exported'][0]
    if not r['ok']:
        raise RuntimeError('%d: %s' % (fid, r.get('error')))
    os.replace(r['path'], path)
    return r['file']


def export_model(out, fid):
    os.makedirs(os.path.join(out, '_probe'), exist_ok=True)
    probe = os.path.join(out, '_probe', 'model.m2')
    listed = fetch(fid, probe)
    name = os.path.splitext(os.path.basename(listed))[0].lower()
    folder = os.path.join(out, name)
    if os.path.isfile(os.path.join(folder, name + '.manifest.json')):
        return name
    os.makedirs(folder, exist_ok=True)
    m2path = os.path.join(folder, name + '.m2')
    os.replace(probe, m2path)
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
    return name


def main(out):
    catalog = json.load(open(os.path.join(HERE, 'catalog.json'), encoding='utf-8'))
    names = {}
    for e in catalog:
        try:
            names[str(e['decor'])] = export_model(out, e['model'])
        except Exception as x:          # noqa: BLE001 - one failed object does not stop the rest
            print('FAILED', e['decor'], e['name'], x)
            continue
        if e.get('thumbnail'):
            os.makedirs(os.path.join(out, 'thumbnails'), exist_ok=True)
            t = os.path.join(out, 'thumbnails', '%d.blp' % e['decor'])
            if not os.path.isfile(t):
                try:
                    fetch(e['thumbnail'], t)
                except Exception as x:  # noqa: BLE001
                    print('no thumbnail', e['decor'], x)
        print(e['decor'], names[str(e['decor'])])
    json.dump(names, open(os.path.join(out, 'models.json'), 'w'), indent=1)


if __name__ == '__main__':
    main(sys.argv[1])
