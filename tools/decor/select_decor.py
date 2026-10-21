"""The first catalogue of the Customization mode: about 150 furniture models of the modern housing
(HouseDecor, Midnight), read through the local wow.export bridge (http://127.0.0.1:9455, retail
loaded), a share of each kind. Writes catalog.json beside this file, the list to correct by hand
before build_decor.py turns it into game files: one entry per object, its HouseDecor ID first.
The objects already in catalog.json stay, first and in their order (their gameobjects and displays
follow the order): a run only adds to them, up to each kind's share.

    python select_decor.py
"""
import collections
import json
import os
import urllib.request

BRIDGE = 'http://127.0.0.1:9455'
HERE = os.path.dirname(os.path.abspath(__file__))
# (category, subcategory): how many objects of that kind
QUOTAS = {
    ('Furnishings', 'Seating'): 40, ('Furnishings', 'Tables and Desks'): 35, ('Furnishings', 'Beds'): 15,
    ('Furnishings', 'Storage'): 40, ('Furnishings', 'Misc Furnishings'): 15,
    ('Lighting', 'Large Lights'): 20, ('Lighting', 'Ceiling Lights'): 15, ('Lighting', 'Small Lights'): 15,
    ('Lighting', 'Wall Lights'): 15, ('Lighting', 'Misc Lighting'): 10,
    ('Accents', 'Wall Hangings'): 35, ('Accents', 'Ornamental'): 40, ('Accents', 'Floor'): 20,
    ('Accents', 'Food and Drink'): 20, ('Accents', 'Misc Accents'): 20,
    ('Nature', 'Large Foliage'): 15, ('Nature', 'Small Foliage'): 10, ('Nature', 'Bushes'): 10,
    ('Nature', 'Ground Cover'): 10, ('Nature', 'Vines and Hanging Plants'): 8, ('Nature', 'Misc Nature'): 12,
    ('Structural', 'Doors'): 8, ('Structural', 'Windows'): 10, ('Structural', 'Misc Structural'): 15,
    ('Structural', 'Construction'): 10,
    ('Functional', 'Utility'): 10, ('Functional', 'Pet Beds'): 6, ('Functional', 'Misc Functional'): 6,
}
M2 = 1      # HouseDecor.ModelType: 0 a WMO, 1 an M2


def table(name):
    with urllib.request.urlopen('%s/db2?table=%s&limit=100000' % (BRIDGE, name), timeout=900) as r:
        return json.loads(r.read().decode('utf-8'))['rows']


def main():
    categories = {r['ID']: r['Name_lang'] for r in table('DecorCategory')}
    kinds = {r['ID']: (categories.get(r['DecorCategoryID']), r['Name_lang']) for r in table('DecorSubcategory')}
    kind_of = collections.defaultdict(list)
    for r in table('DecorXDecorSubcategory'):
        kind_of[r['HouseDecorID']].append(kinds.get(r['DecorSubcategoryID']))
    pools = collections.defaultdict(list)
    seen_models = set()
    for d in sorted(table('HouseDecor'), key=lambda d: d['OrderIndex']):
        if d['Name_lang'].startswith('[DNT]') or d['ModelType'] != M2 or not d['ModelFileDataID'] \
                or not d['ItemID'] or d['ModelFileDataID'] in seen_models:
            continue
        kind = next((k for k in kind_of[d['ID']] if k in QUOTAS), None)
        if kind:
            seen_models.add(d['ModelFileDataID'])
            pools[kind].append(d)
    path = os.path.join(HERE, 'catalog.json')
    catalog = json.load(open(path, encoding='utf-8')) if os.path.isfile(path) else []
    chosen = {e['decor'] for e in catalog}
    have = collections.Counter((e['category'], e['kind']) for e in catalog)
    for kind, n in QUOTAS.items():
        pool = [d for d in pools[kind] if d['ID'] not in chosen]
        want = n - have[kind]
        if want <= 0:
            continue
        # spread over the kind (the catalogue order mixes the themes): every len/n-th object
        step = len(pool) / want if len(pool) > want else 1
        for i in range(min(want, len(pool))):
            d = pool[int(i * step)]
            chosen.add(d['ID'])
            catalog.append({'decor': d['ID'], 'name': d['Name_lang'], 'category': kind[0], 'kind': kind[1],
                            'model': d['ModelFileDataID'], 'item': d['ItemID'],
                            'thumbnail': d['ThumbnailFileDataID'], 'scale': d['InitialScale'] or 1})
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(catalog, f, ensure_ascii=False, indent=1)
        f.write('\n')
    print(len(catalog), 'objects')


if __name__ == '__main__':
    main()
