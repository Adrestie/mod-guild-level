"""A small software renderer for WMO previews: textured, z-buffered, perspective camera.

scene = Scene(); scene.add_wmo(root, groups, read)  ->  scene.render(eye, yaw, pitch, ...)
Angles in degrees: yaw 0 looks along +X, 90 along +Y; pitch > 0 looks up."""
import math

import numpy as np
from PIL import Image

import blp


class Scene(object):
    def __init__(self):
        self.parts = []        # (verts n3, uvs n2, shade n3, tris m3, texid m)
        self.textures = []     # rgba arrays
        self.alpha_test = []   # per texture
        self._tex_index = {}

    def texture(self, name, read, alpha_test=False):
        key = (name.lower(), alpha_test)
        if key not in self._tex_index:
            img = None
            try:
                data = read(name)
                if data:
                    img = blp.decode(data, 1)
            except Exception:
                img = None
            if img is None:
                img = np.full((4, 4, 4), (255, 0, 255, 255), np.uint8)
            self._tex_index[key] = len(self.textures)
            self.textures.append(img)
            self.alpha_test.append(alpha_test)
        return self._tex_index[key]

    def add_wmo(self, root, groups, read, skip_flags=0x04):
        """groups: {index: wmo.Group}. Triangles flagged skip_flags (collision-only) are left out."""
        mats = []
        for m in root.materials:
            name = root.texture(m[3])
            blend = m[2]
            mats.append(self.texture(name, read, alpha_test=blend != 0 or bool(m[0] & 0x1 and 0)) if name else -1)
        for g, grp in groups.items():
            v = np.array(grp.verts, np.float64).reshape(-1, 3)
            if not len(v):
                continue
            uv = np.array(grp.uvs[0], np.float64).reshape(-1, 2) if grp.uvs else np.zeros((len(v), 2))
            interior = bool(root.group_infos[g][0] & 0x2000)
            if grp.colors:
                c = np.array([[x[2], x[1], x[0]] for x in grp.colors[0]], np.float64) / 255.0
                shade = np.clip(c * 1.6 + 0.30, 0, 1.6)
            else:
                n = np.array(grp.normals, np.float64).reshape(-1, 3)
                sun = np.array([0.4, 0.3, 0.86])
                shade = np.repeat((0.55 + 0.45 * np.clip(n @ sun, 0, 1))[:, None], 3, 1)
                if interior:
                    shade = shade * 0.0 + 0.9
            t = np.array(grp.indices, np.int64).reshape(-1, 3)
            keep = []
            texid = []
            for k in range(len(t)):
                mat = grp.tri_mats[k]
                if mat == 0xFF or mats[mat] < 0:
                    continue
                if grp.tri_flags[k] & skip_flags and not grp.tri_flags[k] & 0x20:
                    continue
                keep.append(k)
                texid.append(mats[mat])
            if keep:
                self.parts.append((v, uv, shade, t[keep], np.array(texid)))

    def add_mesh(self, verts, tris, color):
        """Flat-coloured helper geometry (markers)."""
        tid = len(self.textures)
        self.textures.append(np.full((2, 2, 4), tuple(color) + (255,), np.uint8))
        self.alpha_test.append(False)
        v = np.array(verts, np.float64)
        self.parts.append((v, np.zeros((len(v), 2)), np.ones((len(v), 3)), np.array(tris, np.int64),
                           np.full(len(tris), tid)))

    def render(self, eye, yaw, pitch, fov=75.0, size=(960, 600), near=0.05, background=(25, 25, 30)):
        W, H = size
        cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
        fwd = np.array([cy * cp, sy * cp, sp])
        right = np.array([sy, -cy, 0.0])
        up = np.cross(right, fwd)
        f = (W / 2) / math.tan(math.radians(fov) / 2)
        img = np.zeros((H, W, 3), np.float64)
        img[:] = background
        zbuf = np.full((H, W), np.inf)
        eye = np.array(eye, np.float64)
        for v, uv, shade, tris, texid in self.parts:
            rel = v - eye
            cam = np.stack([rel @ right, rel @ up, rel @ fwd], 1)   # x right, y up, z depth
            for k in range(len(tris)):
                ids = tris[k]
                pc = cam[ids]
                if (pc[:, 2] < near).all():
                    continue
                poly = [(pc[i], uv[ids[i]], shade[ids[i]]) for i in range(3)]
                if (pc[:, 2] < near).any():
                    poly = _clip_near(poly, near)
                    if len(poly) < 3:
                        continue
                for j in range(1, len(poly) - 1):
                    _raster(img, zbuf, (poly[0], poly[j], poly[j + 1]), f, W, H,
                            self.textures[texid[k]], self.alpha_test[texid[k]])
        return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))


def _clip_near(poly, near):
    out = []
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        ina, inb = a[0][2] >= near, b[0][2] >= near
        if ina:
            out.append(a)
        if ina != inb:
            t = (near - a[0][2]) / (b[0][2] - a[0][2])
            out.append(tuple(a[q] + (b[q] - a[q]) * t for q in range(3)))
    return out


def _raster(img, zbuf, tri, f, W, H, tex, alpha_test):
    (p0, t0, s0), (p1, t1, s1), (p2, t2, s2) = tri
    xs = np.array([W / 2 + f * p[0] / p[2] for p in (p0, p1, p2)])
    ys = np.array([H / 2 - f * p[1] / p[2] for p in (p0, p1, p2)])
    x0, x1 = max(int(math.floor(xs.min())), 0), min(int(math.ceil(xs.max())), W - 1)
    y0, y1 = max(int(math.floor(ys.min())), 0), min(int(math.ceil(ys.max())), H - 1)
    if x0 > x1 or y0 > y1:
        return
    area = (xs[1] - xs[0]) * (ys[2] - ys[0]) - (xs[2] - xs[0]) * (ys[1] - ys[0])
    if abs(area) < 1e-9:
        return
    gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
    w0 = ((xs[1] - gx) * (ys[2] - gy) - (xs[2] - gx) * (ys[1] - gy)) / area
    w1 = ((xs[2] - gx) * (ys[0] - gy) - (xs[0] - gx) * (ys[2] - gy)) / area
    w2 = 1 - w0 - w1
    inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
    if not inside.any():
        return
    iz = np.array([1 / p0[2], 1 / p1[2], 1 / p2[2]])
    invz = w0 * iz[0] + w1 * iz[1] + w2 * iz[2]
    depth = 1 / np.maximum(invz, 1e-9)
    region = zbuf[y0:y1 + 1, x0:x1 + 1]
    m = inside & (depth < region)
    if not m.any():
        return
    u = (w0 * t0[0] * iz[0] + w1 * t1[0] * iz[1] + w2 * t2[0] * iz[2]) * depth
    v = (w0 * t0[1] * iz[0] + w1 * t1[1] * iz[1] + w2 * t2[1] * iz[2]) * depth
    th, tw = tex.shape[:2]
    tx = (np.floor(u * tw).astype(np.int64)) % tw
    ty = (np.floor(v * th).astype(np.int64)) % th
    texel = tex[ty, tx]
    if alpha_test:
        m &= texel[:, :, 3] > 100
        if not m.any():
            return
    sh = (w0[:, :, None] * s0 * iz[0] + w1[:, :, None] * s1 * iz[1] + w2[:, :, None] * s2 * iz[2]) * depth[:, :, None]
    col = texel[:, :, :3] * sh
    region[m] = depth[m]
    img[y0:y1 + 1, x0:x1 + 1][m] = col[m]
