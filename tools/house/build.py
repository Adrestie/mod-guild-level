"""The guild hall WMO: the faction hall plus rooms attached through its walls.

Each attachment: a source WMO, its groups, the source portal used as the door, the hall wall
(axis, face coordinate, outward sign), the door's position along the wall, the floor height there,
the corridor length."""
import math
import struct

import numpy as np

import game
import merge
import wmo

FLAG_FACE = 0x20      # rendered and collidable


def wall_axes(axis, out):
    """(outward normal, along-wall unit vector) of a wall."""
    n = np.zeros(3)
    n[axis] = out
    along = np.array([0.0, 1.0, 0.0]) if axis == 0 else np.array([1.0, 0.0, 0.0])
    return n, along


def mean_color(mesh):
    if not mesh.col:
        return None
    return np.array(mesh.col[0]).mean(0)


def facing(points, normal):
    """points ordered counter-clockwise as seen from normal."""
    P = np.array(points)
    s = np.zeros(3)
    for i in range(1, len(P) - 1):
        s += np.cross(P[i] - P[0], P[i + 1] - P[0])
    return points if s @ normal >= 0 else points[::-1]


def plane_uv(p, normal, scale=4.0):
    n = normal / np.linalg.norm(normal)
    a = np.array([0.0, 0.0, 1.0]) if abs(n[2]) < 0.9 else np.array([1.0, 0.0, 0.0])
    u = np.cross(a, n); u /= np.linalg.norm(u)
    v = np.cross(n, u)
    return (float(p @ u) / scale, -float(p @ v) / scale)


def seal(mesh, verts, normal, material):
    """A wall over a polygon (portal), facing normal."""
    pts = facing([np.array(v) for v in verts], normal)
    col = mean_color(mesh)
    ids = []
    for p in pts:
        cols = [col.copy() for _ in mesh.col] if col is not None else []
        ids.append(mesh.add_vertex((p, normal, [np.array(plane_uv(p, normal)) for _ in mesh.uv], cols)))
    for j in range(1, len(ids) - 1):
        mesh.tris.append([ids[0], ids[j], ids[j + 1], FLAG_FACE, material, merge.BT_INT])


def main_material(mesh, vertical=True):
    area = {}
    for a, b, c, f, m, k in mesh.tris:
        if m == 0xFF:
            continue
        cr = np.cross(mesh.p[b] - mesh.p[a], mesh.p[c] - mesh.p[a])
        ln = np.linalg.norm(cr)
        if ln < 1e-9:
            continue
        if vertical and abs(cr[2] / ln) > 0.3:
            continue
        if not vertical and cr[2] / ln < 0.7:
            continue
        area[m] = area.get(m, 0) + ln / 2
    return max(area, key=area.get)


def attach(b, hall, att):
    """Adds the room of att to building b, through hall group `hall`."""
    src_name = att['wmo']
    src_root = wmo.Root(game.read(src_name))
    datas = {g: game.read('%s_%03d.wmo' % (src_name[:-4], g)) for g in att['groups']}
    door = att['door']
    s, c = src_root.portals[door][:2]
    dverts = [np.array(v) for v in src_root.portal_verts[s:s + c]]
    n = np.array(src_root.portals[door][2:5])
    # the room group behind the door and the side it lies on
    side = None
    for g in att['groups']:
        grp = wmo.Group(datas[g])
        for k in range(grp.portal_start, grp.portal_start + grp.portal_count):
            p, other, sd, _ = src_root.portal_refs[k]
            if p == door:
                side = sd
    src_out = -side * n                     # from the room through its door
    axis, face, out = att['wall']
    hall_out, along = wall_axes(axis, out)
    # rotate so that the room's door looks into the hall (-hall_out)
    ang = math.degrees(math.atan2(-hall_out[1], -hall_out[0]) - math.atan2(src_out[1], src_out[0]))
    R = merge.rot_z(round(ang / 90.0) * 90.0 if abs(ang / 90.0 - round(ang / 90.0)) < 1e-3 else ang)
    dc = np.mean(dverts, 0)
    dz = att.get('src_floor', min(v[2] for v in dverts))
    target = np.zeros(3)
    target[axis] = face + out * att['gap']
    target[1 - axis] = att['at']
    target[2] = att['floor']
    t = target - R @ np.array([dc[0], dc[1], dz])
    ids = b.add_room(src_root, datas, att['groups'], R, t, doodad_sets=att.get('sets', ('Set_$DefaultGlobal',)),
                     unique_id=b.groups[hall].unique_id)
    room = ids[att['groups'].index(att.get('door_group', att['groups'][0]))]
    # the door, moved
    door_verts = [R @ v + t for v in dverts]
    us = [float(v @ along) for v in door_verts]
    zs = [float(v[2]) for v in door_verts]
    u0, u1, z0, z1 = min(us), max(us), min(zs), max(zs)
    if 'src_floor' in att:
        z0 = att['floor']
    # the corridor may be smaller than the room's door: a frame closes the rest
    if att.get('max_w') and u1 - u0 > att['max_w']:
        c = (u0 + u1) / 2
        u0, u1 = c - att['max_w'] / 2, c + att['max_w'] / 2
    if att.get('max_h') and z1 - z0 > att['max_h']:
        z1 = z0 + att['max_h']
    rm = b.meshes[room]
    frame(rm, door_verts, along, u0, u1, z0, z1, hall_out, main_material(rm))
    # drop the door from the open portals (it is now the corridor's end)
    b.open_portals = [op for op in b.open_portals
                      if not (op[0] == room and np.allclose(np.mean(op[1], 0), np.mean(door_verts, 0), atol=1e-3))]
    hm = b.meshes[hall]
    if att.get('through'):
        # an opening the hall already has (its entrance): no cut, a frame around the corridor
        near = None
        for op in b.open_portals:
            if op[0] == hall:
                d = np.linalg.norm(np.mean(op[1], 0)[:2] - np.array([target[0], target[1]]) + hall_out[:2] * att['gap'])
                if near is None or d < near[0]:
                    near = (d, op)
        op = near[1]
        b.open_portals.remove(op)
        frame(hm, op[1], along, u0, u1, z0, z1, -hall_out, att.get('frame_material', main_material(hm)))
        # whatever blocked the way out (an invisible wall at an instance's entrance) goes too
        removed = cut_wall(hm, axis, face, out, along, hall_out, u0, u1, z0, z1, att.get('cut_in', 1.0))
    else:
        removed = cut_wall(hm, axis, face, out, along, hall_out, u0, u1, z0, z1, att.get('cut_in', 0.35))
    corridor(b, att, room, axis, face, out, along, u0, u1, z0, z1)
    trim = att.get('trim_material', 5)
    plank_frame(hm, axis, face, -out, along, u0, u1, z0, z1, trim)
    plank_frame(b.meshes[room], axis, face + out * att['gap'], out, along, u0, u1, z0, z1, trim)
    clear_doodads(b, hall, axis, face, out, along, u0 - 0.7, u1 + 0.7, z0 - 0.5, z1, att.get('clearance', 3.0))
    # the portal between hall and room, on the hall's wall face
    pv = [_P(axis, u0, z0, face), _P(axis, u1, z0, face), _P(axis, u1, z1, face), _P(axis, u0, z1, face)]
    b.add_portal(pv, hall, room)
    return ids, removed


_m2_boxes = {}


def m2_box(name):
    """Bounding box of an M2 model (its own axes), None if unreadable."""
    key = name.lower()
    if key not in _m2_boxes:
        n = name[:-4] + '.m2' if name.lower().endswith(('.mdx', '.mdl')) else name
        data = game.read(n)
        box = None
        if data:
            if data[:4] == b'MD21':
                data = data[8:]
            if data[:4] == b'MD20':
                box = struct.unpack_from('<6f', data, 0xA0)
        _m2_boxes[key] = box
    return _m2_boxes[key]


def quat_rotate(q, v):
    x, y, z, w = q
    u = np.array([x, y, z])
    return 2 * (u @ v) * u + (w * w - u @ u) * v + 2 * w * np.cross(u, v)


def clear_doodads(b, g, axis, face, out, along, u0, u1, z0, z1, depth):
    """Takes out of group g the decorations standing in front of a doorway."""
    root = b.root
    grp = b.groups[g]
    keep = []
    for d in grp.doodad_refs:
        dd = root.doodads[d]
        name = root.modn.get(dd[0] & 0xFFFFFF)
        box = m2_box(name)
        pos = np.array(dd[1:4])
        if box is None:
            corners = [pos]
        else:
            corners = [pos + dd[8] * quat_rotate(dd[4:8], np.array([box[0 + 3 * i], box[1 + 3 * j], box[2 + 3 * k]]))
                       for i in (0, 1) for j in (0, 1) for k in (0, 1)]
        C = np.array(corners)
        us = C @ along
        ws = (face - C[:, axis]) * out          # distance into the hall from the wall face
        zs = C[:, 2]
        hit = us.max() > u0 and us.min() < u1 and ws.max() > -0.5 and ws.min() < depth and \
            zs.max() > z0 and zs.min() < z1
        if hit:
            print('   decoration removed in front of the door:', name)
        else:
            keep.append(d)
    grp.doodad_refs = keep


def add_box(mesh, axis, along, lo, hi, material, color):
    """A box given in (u, z, w) wall coordinates: lo and hi corners. Six faces, outward."""
    (u0, z0, w0), (u1, z1, w1) = lo, hi
    P = lambda u, z, w: _P(axis, u, z, w)
    wn = np.zeros(3); wn[axis] = 1.0
    faces = [
        ([P(u0, z0, w0), P(u0, z1, w0), P(u0, z1, w1), P(u0, z0, w1)], -along),
        ([P(u1, z0, w0), P(u1, z1, w0), P(u1, z1, w1), P(u1, z0, w1)], along),
        ([P(u0, z0, w0), P(u1, z0, w0), P(u1, z0, w1), P(u0, z0, w1)], np.array([0, 0, -1.0])),
        ([P(u0, z1, w0), P(u1, z1, w0), P(u1, z1, w1), P(u0, z1, w1)], np.array([0, 0, 1.0])),
        ([P(u0, z0, w0), P(u1, z0, w0), P(u1, z1, w0), P(u0, z1, w0)], -wn),
        ([P(u0, z0, w1), P(u1, z0, w1), P(u1, z1, w1), P(u0, z1, w1)], wn),
    ]
    for pts, nrm in faces:
        pts = facing(pts, nrm)
        merge.add_quad(mesh, pts, nrm, [plane_uv(p, nrm, 1.5) for p in pts], FLAG_FACE, material, merge.BT_INT,
                       color=color)


def plank_frame(mesh, axis, face, toward, along, u0, u1, z0, z1, material, width=0.4, thick=0.15, back=0.6):
    """Planks around a doorway on a wall face: two jambs and a lintel, standing out by `thick`
    toward `toward` (+1/-1 on the axis) and reaching `back` into the opening to hide the joint."""
    col = mean_color(mesh)
    wa, wb = sorted((face - toward * back, face + toward * thick))
    e = 0.06     # a little into the opening, so no face lies on the corridor's own
    add_box(mesh, axis, along, (u0 - width, z0, wa), (u0 + e, z1 + width, wb), material, col)
    add_box(mesh, axis, along, (u1 - e, z0, wa), (u1 + width, z1 + width, wb), material, col)
    add_box(mesh, axis, along, (u0 + e, z1 - e, wa), (u1 - e, z1 + width, wb), material, col)


def _P(axis, u, z, w):
    p = np.zeros(3)
    p[axis] = w
    p[1 - axis] = u
    p[2] = z
    return p


def frame(mesh, poly, along, u0, u1, z0, z1, normal, material):
    """The part of polygon poly outside the rectangle [u0,u1]x[z0,z1], as a wall facing normal."""
    recs = [(np.array(p, np.float64), normal, [], []) for p in poly]
    planes = [(-along, float(u0)), (along, -float(u1)), (np.array([0, 0, -1.0]), float(z0)),
              (np.array([0, 0, 1.0]), -float(z1))]
    rest = recs
    pieces = []
    for n, d in planes:
        o = merge.clip_poly(rest, (n, d), True)
        if len(o) >= 3 and merge.poly_area(o) > 1e-4:
            pieces.append([r[0] for r in o])
        rest = merge.clip_poly(rest, (n, d), False)
        if len(rest) < 3:
            break
    for pc in pieces:
        seal(mesh, pc, normal, material)


def cut_wall(hm, axis, face, out, along, hall_out, u0, u1, z0, z1, cut_in):
    inner = face - out * cut_in
    nd = np.zeros(3); nd[axis] = -out
    planes = [
        (-along, float(u0)),            # outside when u < u0:  f = -u + u0 > 0
        (along, -float(u1)),            # outside when u > u1
        (np.array([0, 0, -1.0]), float(z0) + 0.02),   # outside when z < z0 + 0.02
        (np.array([0, 0, 1.0]), -float(z1)),          # outside when z > z1
        (nd, float(inner * out)),       # outside when further into the hall than inner
    ]
    return merge.cut_box(hm, planes)


def corridor(b, att, room, axis, face, out, along, u0, u1, z0, z1):
    rm = b.meshes[room]
    a_face = face
    b_face = face + out * att['gap']
    P = lambda u, z, w: _P(axis, u, z, w)
    wall_mat = att.get('corridor_wall', main_material(rm))
    floor_mat = att.get('corridor_floor', main_material(rm, vertical=False))
    col = mean_color(rm)
    for (pts, nrm, mat) in (
            ([P(u0, z0, a_face), P(u0, z0, b_face), P(u0, z1, b_face), P(u0, z1, a_face)], along, wall_mat),
            ([P(u1, z0, a_face), P(u1, z0, b_face), P(u1, z1, b_face), P(u1, z1, a_face)], -along, wall_mat),
            ([P(u0, z1, a_face), P(u0, z1, b_face), P(u1, z1, b_face), P(u1, z1, a_face)], np.array([0, 0, -1.0]), wall_mat),
            ([P(u0, z0, a_face), P(u0, z0, b_face), P(u1, z0, b_face), P(u1, z0, a_face)], np.array([0, 0, 1.0]), floor_mat)):
        pts = facing(pts, nrm)
        uvs = [plane_uv(p, nrm, 2.5) for p in pts]
        merge.add_quad(rm, pts, nrm, uvs, FLAG_FACE, mat, merge.BT_INT, color=col)


def build(hall_wmo, keep, attachments, seal_material=None, portals=True):
    root_data = game.read(hall_wmo)
    r0 = wmo.Root(root_data)
    gdatas = {g: game.read('%s_%03d.wmo' % (hall_wmo[:-4], g)) for g in range(len(r0.group_infos))}
    b = merge.Building(root_data, gdatas, keep)
    results = []
    for att in attachments:
        results.append(attach(b, 0, att))
    # seal what leads nowhere
    b.seals = []
    for g, verts, plane, side in b.open_portals:
        n = np.array(plane[:3])
        mesh = b.meshes[g]
        mat = seal_material if (g == 0 and seal_material is not None) else main_material(mesh)
        seal(mesh, verts, side * n / np.linalg.norm(n), mat)
        b.seals.append((g, [np.array(v) for v in verts], side * n / np.linalg.norm(n)))
    b.open_portals = []
    b.finish(portals=portals)
    return b, results
