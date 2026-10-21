"""The first catalogue of the Customization mode: about 150 furniture models of the modern housing
(HouseDecor, Midnight), read through the local wow.export bridge (http://127.0.0.1:9455, retail
loaded), a share of each kind. Writes catalog.json beside this file, the list to correct by hand
before build_decor.py turns it into game files: one entry per object, its HouseDecor ID first.

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
    ('Furnishings', 'Seating'): 18, ('Furnishings', 'Tables and Desks'): 15, ('Furnishings', 'Beds'): 6,
    ('Furnishings', 'Storage'): 15, ('Furnishings', 'Misc Furnishings'): 6,
    ('Lighting', 'Large Lights'): 8, ('Lighting', 'Ceiling Lights'): 6, ('Lighting', 'Small Lights'): 6,
    ('Lighting', 'Wall Lights'): 6,
    ('Accents', 'Wall Hangings'): 14, ('Accents', 'Ornamental'): 14, ('Accents', 'Floor'): 8,
    ('Accents', 'Food and Drink'): 8, ('Accents', 'Misc Accents'): 6,
    ('Nature', 'Large Foliage'): 5, ('Nature', 'Small Foliage'): 4, ('Nature', 'Bushes'): 3,
    ('Nature', 'Vines and Hanging Plants'): 2,
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
    catalog = []
    for kind, n in QUOTAS.items():
        pool = pools[kind]
        # spread over the kind (the catalogue order mixes the themes): every len/n-th object
        step = len(pool) / n if len(pool) > n else 1
        for i in range(min(n, len(pool))):
            d = pool[int(i * step)]
            catalog.append({'decor': d['ID'], 'name': d['Name_lang'], 'category': kind[0], 'kind': kind[1],
                            'model': d['ModelFileDataID'], 'item': d['ItemID'],
                            'thumbnail': d['ThumbnailFileDataID'], 'scale': d['InitialScale'] or 1})
    with open(os.path.join(HERE, 'catalog.json'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(catalog, f, ensure_ascii=False, indent=1)
        f.write('\n')
    print(len(catalog), 'objects')


if __name__ == '__main__':
    main()
