"""Four views from the most open spot of a room's main floor: python t_room4.py out.png wmo:g,g ..."""
import sys
from PIL import Image
import numpy as np
import game, wmo, render


def spot(grp):
    V = np.array(grp.verts)
    T = np.array(grp.indices).reshape(-1, 3)
    P = V[T]
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    ln = np.linalg.norm(n, axis=1) + 1e-9
    nz = n[:, 2] / ln
    floor = nz > 0.7
    area = ln / 2
    zc = P[:, :, 2].mean(1)
    # the floor level holding the most area
    levels = {}
    for z, a in zip(zc[floor], area[floor]):
        levels[round(z)] = levels.get(round(z), 0) + a
    fz = max(levels, key=levels.get)
    cand = P[floor & (np.abs(zc - fz) < 1.0)].mean(1)
    wallmask = (np.abs(nz) < 0.3)
    W = P[wallmask].reshape(-1, 3)
    W = W[(W[:, 2] > fz + 0.3) & (W[:, 2] < fz + 3)]
    best = max(cand, key=lambda c: np.min(np.hypot(W[:, 0] - c[0], W[:, 1] - c[1])) if len(W) else 0)
    return best[0], best[1], fz


rows = []
for spec in sys.argv[2:]:
    n, gs = spec.split(':')
    gs = [int(g) for g in gs.split(',')]
    root = wmo.Root(game.read(n))
    groups = {g: wmo.Group(game.read('%s_%03d.wmo' % (n[:-4], g))) for g in gs}
    sc = render.Scene()
    sc.add_wmo(root, groups, game.read)
    x, y, z = spot(groups[gs[0]])
    views = [sc.render([x, y, z + 1.8], yaw, 8, fov=100, size=(420, 300)) for yaw in (0, 90, 180, 270)]
    row = Image.new('RGB', (4 * 425, 300))
    for k, v in enumerate(views):
        row.paste(v, (k * 425, 0))
    rows.append(row)
sheet = Image.new('RGB', (4 * 425, 305 * len(rows)))
for k, r in enumerate(rows):
    sheet.paste(r, (0, 305 * k))
sheet.save(sys.argv[1])
