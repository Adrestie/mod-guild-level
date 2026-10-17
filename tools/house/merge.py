"""Building a guild hall WMO out of existing ones: groups copied with a transform, doorways
cut through walls, corridors and seals added, portals wired, batches and BSP rebuilt."""
import copy
import math
import struct

import numpy as np

import wmo

BT_TRANS, BT_INT, BT_EXT = 0, 1, 2


# ------------------------------------------------------------------ editable group

class Mesh(object):
    """A group's geometry as flat lists: vertex attributes and triangles with their face data."""

    def __init__(self, grp):
        self.grp = grp
        self.p = [np.array(v, np.float64) for v in grp.verts]
        self.n = [np.array(v, np.float64) for v in grp.normals]
        self.uv = [[np.array(t, np.float64) for t in s] for s in grp.uvs]
        self.col = [[np.array(c, np.float64) for c in s] for s in grp.colors]
        kinds = [BT_TRANS] * grp.trans_batches + [BT_INT] * grp.int_batches + [BT_EXT] * grp.ext_batches
        kind_of = {}
        for b, batch in enumerate(grp.batches):
            start, count = batch[6], batch[7]
            for i in range(start // 3, (start + count) // 3):
                kind_of[i] = kinds[b] if b < len(kinds) else BT_INT
        self.tris = []   # [a, b, c, flags, material, kind]
        for k in range(len(grp.indices) // 3):
            a, b, c = grp.indices[3 * k:3 * k + 3]
            self.tris.append([a, b, c, grp.tri_flags[k], grp.tri_mats[k], kind_of.get(k, BT_INT)])

    # vertices
    def vertex(self, k):
        return (self.p[k], self.n[k], [s[k] for s in self.uv], [s[k] for s in self.col])

    def add_vertex(self, rec):
        p, n, uvs, cols = rec
        self.p.append(np.array(p, np.float64))
        self.n.append(np.array(n, np.float64))
        for s, t in zip(self.uv, uvs):
            s.append(np.array(t, np.float64))
        for s, c in zip(self.col, cols):
            s.append(np.array(c, np.float64))
        return len(self.p) - 1

    def transform(self, R, t):
        self.p = [R @ v + t for v in self.p]
        self.n = [R @ v for v in self.n]

    def bbox(self):
        P = np.array(self.p)
        return list(P.min(0)) + list(P.max(0))


def lerp_rec(a, b, t):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t,
            [x + (y - x) * t for x, y in zip(a[2], b[2])], [x + (y - x) * t for x, y in zip(a[3], b[3])])


def clip_poly(poly, plane, keep_positive):
    """Sutherland-Hodgman on vertex records against plane (n, d): f = n.p + d."""
    n, d = plane
    out = []
    m = len(poly)
    for i in range(m):
        a, b = poly[i], poly[(i + 1) % m]
        fa, fb = float(n @ a[0] + d), float(n @ b[0] + d)
        ina = fa >= 0 if keep_positive else fa <= 0
        inb = fb >= 0 if keep_positive else fb <= 0
        if ina:
            out.append(a)
        if ina != inb:
            t = fa / (fa - fb)
            out.append(lerp_rec(a, b, t))
    return out


def poly_area(poly):
    s = np.zeros(3)
    for i in range(1, len(poly) - 1):
        s += np.cross(poly[i][0] - poly[0][0], poly[i + 1][0] - poly[0][0])
    return np.linalg.norm(s) / 2


def cut_box(mesh, planes, keep=lambda tri: False):
    """Removes the part of every triangle inside the convex region {f(p) < 0 for all planes};
    planes: [(n, d)] with f(p) = n.p + d > 0 outside. Returns the removed area per material."""
    removed = {}
    new_tris = []
    for tri in mesh.tris:
        recs = [mesh.vertex(i) for i in tri[:3]]
        P = np.array([r[0] for r in recs])
        # entirely outside one plane: untouched
        if keep(tri) or any((P @ n + d >= -1e-6).all() for n, d in planes):
            new_tris.append(tri)
            continue
        rest = recs
        pieces = []
        for n, d in planes:
            outside = clip_poly(rest, (n, d), True)
            if len(outside) >= 3 and poly_area(outside) > 1e-6:
                pieces.append(outside)
            rest = clip_poly(rest, (n, d), False)
            if len(rest) < 3:
                break
        if len(rest) >= 3:
            removed[tri[4]] = removed.get(tri[4], 0) + poly_area(rest)
        for piece in pieces:
            ids = [mesh.add_vertex(r) for r in piece]
            for j in range(1, len(ids) - 1):
                new_tris.append([ids[0], ids[j], ids[j + 1]] + tri[3:])
    mesh.tris = new_tris
    return removed


def add_quad(mesh, corners, normal, uvs, flags, material, kind, color=None):
    """corners: 4 points in order (counter-clockwise seen from where normal points)."""
    ids = []
    for c, uv in zip(corners, uvs):
        cols = [np.array(color if color is not None else (127, 127, 127, 255), np.float64) for _ in mesh.col]
        ids.append(mesh.add_vertex((np.array(c, np.float64), np.array(normal, np.float64),
                                    [np.array(uv, np.float64) for _ in mesh.uv], cols)))
    mesh.tris.append([ids[0], ids[1], ids[2], flags, material, kind])
    mesh.tris.append([ids[0], ids[2], ids[3], flags, material, kind])


# ------------------------------------------------------------------ writing a group back

def collidable(flags):
    return bool(flags & 0x08) or (bool(flags & 0x20) and not flags & 0x04)


def build_bsp(P, tris, ids, max_faces=30, depth=0):
    """Nodes and refs of an axis-aligned BSP over the faces ids (list of triangle indices).
    Each split tries the three axes at the faces' median and keeps the one duplicating least."""
    nodes, refs = [], []
    T = np.asarray(tris)
    tmin_all = P[T].min(1)
    tmax_all = P[T].max(1)
    cent_all = P[T].mean(1)

    def leaf(me, faces):
        nodes[me] = [4, -1, -1, len(faces), len(refs), 0.0]
        refs.extend(faces)
        return me

    def rec(faces, depth):
        me = len(nodes)
        nodes.append(None)
        if len(faces) <= max_faces or depth > 30:
            return leaf(me, faces)
        f = np.asarray(faces)
        best = None
        for axis in range(3):
            split = float(np.median(cent_all[f, axis]))
            lo, hi = tmin_all[f, axis], tmax_all[f, axis]
            neg = f[(lo < split) | ((lo == split) & (hi == split))]
            pos = f[hi > split]
            if not len(neg) or not len(pos):
                continue
            cost = max(len(neg), len(pos)) + 0.5 * (len(neg) + len(pos) - len(f))
            if best is None or cost < best[0]:
                best = (cost, axis, split, neg, pos)
        if best is None or best[0] >= len(f):
            return leaf(me, faces)
        _, axis, split, neg, pos = best
        a = rec(list(neg), depth + 1)
        b = rec(list(pos), depth + 1)
        nodes[me] = [axis, a, b, 0, 0, split]
        return me

    if ids:
        rec(list(ids), 0)
    else:
        nodes.append([4, -1, -1, 0, 0, 0.0])
    return nodes, refs


def write_mesh(mesh, grp):
    """Puts the mesh back into grp: vertices compacted, triangles sorted into batches, BSP."""
    # Vertices in the order of the batches that first use them: the client lights the vertices
    # below the last transition batch's maxIndex as transition vertices (exterior light blended
    # in), so those must come first, then the interior ones, then the exterior ones.
    tris = sorted(mesh.tris, key=lambda t: (t[5], t[4]))
    used = []
    seen = set()
    for t in tris:
        for i in t[:3]:
            if i not in seen:
                seen.add(i)
                used.append(i)
    remap = {o: k for k, o in enumerate(used)}
    assert len(used) < 65536, len(used)
    grp.verts = [list(map(float, mesh.p[i])) for i in used]
    grp.normals = [list(map(float, mesh.n[i])) for i in used]
    grp.uvs = [[list(map(float, s[i])) for i in used] for s in mesh.uv]
    grp.colors = [bytearray(int(min(max(round(x), 0), 255)) for x in s[i]) for s in mesh.col for i in []]
    grp.colors = [[bytearray(int(min(max(round(x), 0), 255)) for x in s[i]) for i in used] for s in mesh.col]
    grp.indices = []
    grp.tri_flags = []
    grp.tri_mats = []
    grp.batches = []
    counts = [0, 0, 0]
    P = np.array(grp.verts)
    k = 0
    while k < len(tris):
        kind, mat = tris[k][5], tris[k][4]
        j = k
        while j < len(tris) and tris[j][5] == kind and tris[j][4] == mat:
            j += 1
        # material 0xFF (collision only) gets no batch but keeps its triangles at the end
        start = len(grp.indices)
        vids = []
        for t in tris[k:j]:
            ids = [remap[i] for i in t[:3]]
            grp.indices += ids
            grp.tri_flags.append(t[3])
            grp.tri_mats.append(t[4])
            vids += ids
        if mat != 0xFF:
            bp = P[vids]
            lo = np.floor(bp.min(0)).astype(int)
            hi = np.ceil(bp.max(0)).astype(int)
            grp.batches.append([int(lo[0]), int(lo[1]), int(lo[2]), int(hi[0]), int(hi[1]), int(hi[2]),
                                start, len(grp.indices) - start, min(vids), max(vids), 0, mat])
            counts[kind] += 1
        k = j
    # collision-only triangles must come after every batch: move them
    if any(m == 0xFF for m in grp.tri_mats):
        order = sorted(range(len(grp.tri_mats)), key=lambda i: grp.tri_mats[i] == 0xFF)
        idx = [grp.indices[3 * i:3 * i + 3] for i in order]
        flags = [grp.tri_flags[i] for i in order]
        mats = [grp.tri_mats[i] for i in order]
        # recompute batch starts
        pos = {}
        for new, old in enumerate(order):
            pos[old] = new
        for b in grp.batches:
            b[6] = pos[b[6] // 3] * 3
        grp.indices = [i for t in idx for i in t]
        grp.tri_flags, grp.tri_mats = flags, mats
    grp.trans_batches, grp.int_batches, grp.ext_batches = counts
    T = np.array(grp.indices).reshape(-1, 3)
    coll = list(range(len(T)))      # as Blizzard does: every face, the client also finds its group with it
    nodes, refs = build_bsp(P, T, coll)
    grp.bsp_nodes = nodes
    grp.bsp_refs = refs
    bb = P.min(0).tolist() + P.max(0).tolist()
    grp.bbox = bb
    # chunks that must exist
    tags = [t for t, b in grp.sub]
    for tag in ('MOBN', 'MOBR'):
        if tag not in tags:
            grp.sub.append([tag, b''])
    grp.flags |= 0x1
    if mesh.col:
        grp.flags |= 0x4


# ------------------------------------------------------------------ transforms

def rot_z(deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    if abs(c) < 1e-9:
        c = 0.0
    if abs(s) < 1e-9:
        s = 0.0
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1.0]])


def quat_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


# ------------------------------------------------------------------ the building

class Building(object):
    """The destination WMO: a root and its groups (wmo.Group), groups as Mesh while editing."""

    def __init__(self, root_data, group_datas, keep_groups):
        self.root = wmo.Root(root_data)
        self.groups = []
        self.meshes = []
        old = {}
        for g in keep_groups:
            old[g] = len(self.groups)
            grp = wmo.Group(group_datas[g])
            self.groups.append(grp)
            self.meshes.append(Mesh(grp))
        infos = [self.root.group_infos[g] for g in keep_groups]
        self.root.group_infos = [list(i) for i in infos]
        # portals between kept groups stay; the others are reported for sealing
        self.open_portals = []      # (group, portal verts, plane, side)
        self._rebuild_portals(keep_groups, old)

    def _rebuild_portals(self, keep, old):
        root = self.root
        refs = []
        portals, pverts = [], []
        new_index = {}
        per_group = []
        for g in keep:
            grp = self.groups[old[g]]
            mine = []
            for k in range(grp.portal_start, grp.portal_start + grp.portal_count):
                p, other, side, filler = root.portal_refs[k]
                s, c = root.portals[p][:2]
                verts = root.portal_verts[s:s + c]
                plane = root.portals[p][2:6]
                if other in old:
                    if p not in new_index:
                        new_index[p] = len(portals)
                        portals.append([len(pverts), c] + list(plane))
                        pverts += [list(v) for v in verts]
                    mine.append([new_index[p], old[other], side, filler])
                else:
                    self.open_portals.append((old[g], [np.array(v) for v in verts], plane, side))
            per_group.append(mine)
        root.portals, root.portal_verts = portals, pverts
        self.group_refs = per_group      # per group: [portal, other, side, filler]

    def add_portal(self, verts, ga, gb):
        """A portal between groups ga and gb; verts: polygon (np arrays)."""
        P = np.array(verts)
        n = np.cross(P[1] - P[0], P[2] - P[0])
        n /= np.linalg.norm(n)
        d = -float(n @ P[0])
        idx = len(self.root.portals)
        self.root.portals.append([len(self.root.portal_verts), len(P)] + [float(x) for x in n] + [d])
        self.root.portal_verts += [list(map(float, v)) for v in P]
        for g, other in ((ga, gb), (gb, ga)):
            c = np.array(self.meshes[g].bbox()).reshape(2, 3).mean(0)
            side = 1 if float(n @ c + d) > 0 else -1
            self.group_refs[g].append([idx, other, side, 0])
        return idx

    def add_room(self, src_root, src_group_datas, src_groups, R, t, doodad_sets=('Set_$DefaultGlobal',),
                 name=None, unique_id=None):
        """Copies groups src_groups of another WMO, moved by R, t. Returns their new indices."""
        root = self.root
        # textures and materials
        base = len(root.motx.blob)
        while base % 4:
            root.motx.blob.append(0)
            base += 1
        root.motx.blob += src_root.motx.bytes()
        mat_base = len(root.materials)
        for m in src_root.materials:
            m = list(m)
            for f in (3, 6, 9):
                m[f] += base
            root.materials.append(m)
        assert len(root.materials) < 255, 'too many materials'
        # doodads of the chosen sets
        wanted = set()
        for s in src_root.doodad_sets:
            if s[0] in doodad_sets:
                wanted.update(range(s[1], s[1] + s[2]))
        dd_map = {}
        lt_map = {}
        q_rot = (0.0, 0.0, math.sin(math.atan2(R[1, 0], R[0, 0]) / 2), math.cos(math.atan2(R[1, 0], R[0, 0]) / 2))
        old = {g: len(self.groups) + k for k, g in enumerate(src_groups)}
        new_ids = []
        for g in src_groups:
            grp = wmo.Group(src_group_datas[g])
            # doodads
            refs = []
            for d in grp.doodad_refs:
                if d not in wanted:
                    continue
                if d not in dd_map:
                    dd = list(src_root.doodads[d])
                    nm = src_root.modn.get(dd[0] & 0xFFFFFF)
                    off = root.modn.add(nm)
                    dd[0] = off | (dd[0] & 0xFF000000)
                    p = R @ np.array(dd[1:4]) + t
                    dd[1:4] = [float(x) for x in p]
                    q = quat_mul(q_rot, dd[4:8])
                    dd[4:8] = [float(x) for x in q]
                    dd_map[d] = len(root.doodads)
                    root.doodads.append(dd)
                refs.append(dd_map[d])
            grp.doodad_refs = refs
            # lights
            lrefs = []
            for l in grp.light_refs:
                if l not in lt_map:
                    lt = bytearray(src_root.lights[l])
                    p = R @ np.array(struct.unpack_from('<3f', lt, 8)) + t
                    struct.pack_into('<3f', lt, 8, *p)
                    lt_map[l] = len(root.lights)
                    root.lights.append(lt)
                lrefs.append(lt_map[l])
            grp.light_refs = lrefs
            # flags: no liquid, keep the rest
            grp.liquid = None
            grp.sub = [s for s in grp.sub if s[0] != 'MLIQ']
            grp.flags &= ~0x1000
            grp.liquid_type = 15        # "no liquid": 0 fills the group with water
            grp.fog_ids = [0, 0, 0, 0]
            if unique_id is not None:
                grp.unique_id = unique_id
            mesh = Mesh(grp)
            mesh.tris = [[a, b, c, f, (m + mat_base) if m != 0xFF else 0xFF, k] for a, b, c, f, m, k in mesh.tris]
            mesh.transform(R, t)
            gname = name or src_root.group_name(g)
            info = list(src_root.group_infos[g])
            info[7] = root.mogn.add(gname)
            grp.name_off = info[7]
            grp.desc_off = 0
            root.group_infos.append(info)
            self.groups.append(grp)
            self.meshes.append(mesh)
            new_ids.append(len(self.groups) - 1)
            # portals
            mine = []
            for k in range(grp.portal_start, grp.portal_start + grp.portal_count):
                p, other, side, filler = src_root.portal_refs[k]
                s, c = src_root.portals[p][:2]
                verts = [R @ np.array(v) + t for v in src_root.portal_verts[s:s + c]]
                if other in old:
                    key = ('src', id(src_root), p)
                    if not hasattr(self, '_pidx'):
                        self._pidx = {}
                    if key not in self._pidx:
                        Pv = np.array(verts)
                        n = R @ np.array(src_root.portals[p][2:5])
                        d = -float(n @ Pv[0])
                        self._pidx[key] = len(root.portals)
                        root.portals.append([len(root.portal_verts), c] + [float(x) for x in n] + [d])
                        root.portal_verts += [list(map(float, v)) for v in verts]
                    mine.append([self._pidx[key], old[other], side, filler])
                else:
                    n = R @ np.array(src_root.portals[p][2:5])
                    self.open_portals.append((len(self.groups) - 1, verts, list(n) + [0.0], side))
            self.group_refs.append(mine)
        return new_ids

    def finish(self, portals=True):
        """Group headers, portal ref ranges, root counts and bbox. portals=False: none at all,
        the client then draws every group (no occlusion between rooms)."""
        root = self.root
        if not portals:
            root.portals, root.portal_verts = [], []
            self.group_refs = [[] for _ in self.groups]
        refs = []
        for g, grp in enumerate(self.groups):
            write_mesh(self.meshes[g], grp)
            grp.portal_start = len(refs)
            grp.portal_count = len(self.group_refs[g])
            refs += self.group_refs[g]
            info = root.group_infos[g]
            info[0] = grp.flags
            info[1:7] = grp.bbox
            # MODR/MOLR presence follows the lists
            tags = [t for t, b in grp.sub]
            for tag, flag, lst in (('MODR', 0x800, grp.doodad_refs), ('MOLR', 0x200, grp.light_refs)):
                if lst and tag not in tags:
                    # insert after MOBA
                    i = tags.index('MOBA') + 1
                    grp.sub.insert(i, [tag, b''])
                    tags.insert(i, tag)
                if lst:
                    grp.flags |= flag
                info[0] = grp.flags
        root.portal_refs = refs
        bbs = np.array([grp.bbox for grp in self.groups])
        root.bbox = bbs[:, :3].min(0).tolist() + bbs[:, 3:].max(0).tolist()
        root.doodad_sets = [['Set_$DefaultGlobal', 0, len(root.doodads), 0]]
        root.recount()

    def files(self, path):
        """{archive name: bytes} of the root and its groups."""
        out = {path: self.root.pack()}
        for g, grp in enumerate(self.groups):
            out['%s_%03d.wmo' % (path[:-4], g)] = grp.pack()
        return out
