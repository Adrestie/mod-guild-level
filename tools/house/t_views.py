"""Views of a built hall: python t_views.py files.pkl out.png "x,y,z,yaw,pitch" ..."""
import pickle
import sys

from PIL import Image

import game, render, wmo

files = pickle.load(open(sys.argv[1], 'rb'))
rootname = [k for k in files if not k[-8:-4].startswith('_')][0]
root = wmo.Root(files[rootname])
groups = {g: wmo.Group(files['%s_%03d.wmo' % (rootname[:-4], g)]) for g in range(len(root.group_infos))}
sc = render.Scene()
sc.add_wmo(root, groups, game.read)
views = []
for spec in sys.argv[3:]:
    x, y, z, yaw, pitch = [float(v) for v in spec.split(',')]
    views.append(sc.render([x, y, z], yaw, pitch, fov=90, size=(640, 400)))
cols = 2
rows = (len(views) + 1) // 2
sheet = Image.new('RGB', (1290, 410 * rows))
for k, v in enumerate(views):
    sheet.paste(v, ((k % 2) * 650, (k // 2) * 410))
sheet.save(sys.argv[2])
