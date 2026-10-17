"""WMO v17 (3.3.5) root and group files: parsed into editable lists, written back.

Root: textures (MOTX blob, materials point into it), materials (64-byte records), group
names, group infos, portals (vertices + plane), portal refs, lights, doodad sets, doodad names,
doodad defs, fogs; other chunks are kept as raw bytes, in order.
Group: header fields, triangles (MOPY flags/material, MOVI), vertices, normals, uvs (one or
two sets), batches, light refs, doodad refs, BSP (MOBN/MOBR), colours (one or two sets), liquid.
"""
import struct


def _chunks(data, start=0, end=None):
    i, end = start, len(data) if end is None else end
    while i + 8 <= end:
        tag = data[i:i + 4][::-1].decode('latin1')
        size = struct.unpack_from('<I', data, i + 4)[0]
        yield tag, data[i + 8:i + 8 + size]
        i += 8 + size


def _chunk(tag, body):
    return tag[::-1].encode('latin1') + struct.pack('<I', len(body)) + body


def _strings(blob):
    """{offset: string} of a zero-separated string block."""
    out, i = {}, 0
    while i < len(blob):
        j = blob.index(b'\0', i)
        if j > i:
            out[i] = blob[i:j].decode('latin1')
        i = j + 1
    return out


class StringBlock(object):
    """A zero-separated, 4-aligned string block (MOTX, MOGN, MODN)."""

    def __init__(self, blob=b''):
        self.blob = bytearray(blob)

    def get(self, off):
        if off < 0 or off >= len(self.blob):
            return ''
        return self.blob[off:self.blob.index(b'\0', off)].decode('latin1')

    def add(self, s, align=4):
        """The offset of s, appended if missing."""
        raw = s.encode('latin1')
        i = 0
        while True:
            i = self.blob.find(raw + b'\0', i)
            if i < 0:
                break
            if i == 0 or self.blob[i - 1] == 0:
                return i
            i += 1
        while len(self.blob) % align:
            self.blob.append(0)
        off = len(self.blob)
        self.blob += raw + b'\0'
        return off

    def bytes(self, align=4):
        b = bytes(self.blob)
        while len(b) % align:
            b += b'\0'
        return b


MOMT = struct.Struct('<16I')   # flags shader blend tex1 sidn framesidn tex2 diff ground tex3 color2 flags2 runtime[4]
assert MOMT.size == 64


class Root(object):
    def __init__(self, data):
        self.order = []
        self.raw = {}
        for tag, body in _chunks(data):
            self.order.append(tag)
            self.raw[tag] = body
        h = self.raw['MOHD']
        (self.n_textures, self.n_groups, self.n_portals, self.n_lights, self.n_doodad_names,
         self.n_doodad_defs, self.n_doodad_sets, self.ambient, self.wmo_id) = struct.unpack_from('<9I', h)
        self.bbox = list(struct.unpack_from('<6f', h, 36))
        self.flags, self.num_lod = struct.unpack_from('<HH', h, 60)
        self.motx = StringBlock(self.raw.get('MOTX', b''))
        momt = self.raw.get('MOMT', b'')
        self.materials = [list(MOMT.unpack_from(momt, k * 64)) for k in range(len(momt) // 64)]
        self.mogn = StringBlock(self.raw.get('MOGN', b''))
        mogi = self.raw.get('MOGI', b'')
        self.group_infos = [list(struct.unpack_from('<I6fi', mogi, k * 32)) for k in range(len(mogi) // 32)]
        mopv = self.raw.get('MOPV', b'')
        self.portal_verts = [list(struct.unpack_from('<3f', mopv, k * 12)) for k in range(len(mopv) // 12)]
        mopt = self.raw.get('MOPT', b'')
        self.portals = [list(struct.unpack_from('<HH4f', mopt, k * 20)) for k in range(len(mopt) // 20)]
        mopr = self.raw.get('MOPR', b'')
        self.portal_refs = [list(struct.unpack_from('<HHhH', mopr, k * 8)) for k in range(len(mopr) // 8)]
        molt = self.raw.get('MOLT', b'')
        self.lights = [bytearray(molt[k * 48:(k + 1) * 48]) for k in range(len(molt) // 48)]
        mods = self.raw.get('MODS', b'')
        self.doodad_sets = [[mods[k * 32:k * 32 + 20].rstrip(b'\0').decode('latin1')] +
                            list(struct.unpack_from('<III', mods, k * 32 + 20)) for k in range(len(mods) // 32)]
        self.modn = StringBlock(self.raw.get('MODN', b''))
        modd = self.raw.get('MODD', b'')
        self.doodads = [list(struct.unpack_from('<I3f4ffI', modd, k * 40)) for k in range(len(modd) // 40)]
        mfog = self.raw.get('MFOG', b'')
        self.fogs = [bytearray(mfog[k * 48:(k + 1) * 48]) for k in range(len(mfog) // 48)]

    def texture(self, off):
        return self.motx.get(off)

    def group_name(self, g):
        return self.mogn.get(self.group_infos[g][7])

    def pack(self):
        body = {}
        body['MOHD'] = struct.pack('<9I6fHH', self.n_textures, len(self.group_infos), len(self.portals),
                                   len(self.lights), self.n_doodad_names, self.n_doodad_defs,
                                   len(self.doodad_sets), self.ambient, self.wmo_id, *self.bbox,
                                   self.flags, self.num_lod)
        body['MOTX'] = self.motx.bytes()
        body['MOMT'] = b''.join(MOMT.pack(*m) for m in self.materials)
        body['MOGN'] = self.mogn.bytes()
        body['MOGI'] = b''.join(struct.pack('<I6fi', *g) for g in self.group_infos)
        body['MOPV'] = b''.join(struct.pack('<3f', *v) for v in self.portal_verts)
        body['MOPT'] = b''.join(struct.pack('<HH4f', *p) for p in self.portals)
        body['MOPR'] = b''.join(struct.pack('<HHhH', *r) for r in self.portal_refs)
        body['MOLT'] = b''.join(bytes(x) for x in self.lights)
        body['MODS'] = b''.join(s[0].encode('latin1').ljust(20, b'\0') + struct.pack('<III', *s[1:4])
                                for s in self.doodad_sets)
        body['MODN'] = self.modn.bytes()
        body['MODD'] = b''.join(struct.pack('<I3f4ffI', *d) for d in self.doodads)
        body['MFOG'] = b''.join(bytes(x) for x in self.fogs)
        out = b''
        for tag in self.order:
            out += _chunk(tag, body.get(tag, self.raw[tag]))
        return out

    def recount(self):
        """Header counts from the content (Blizzard's own files do not always agree)."""
        self.n_textures = len(self.materials)
        self.n_doodad_names = len(_strings(self.modn.bytes()))
        self.n_doodad_defs = len(self.doodads)


GROUP_SUB = ['MOPY', 'MOVI', 'MOVT', 'MONR', 'MOTV', 'MOBA', 'MOLR', 'MODR', 'MOBN', 'MOBR', 'MOCV', 'MLIQ']


class Group(object):
    def __init__(self, data):
        self.pre = []          # chunks before MOGP (MVER)
        for tag, body in _chunks(data):
            if tag != 'MOGP':
                self.pre.append((tag, body))
                continue
            h = body[:68]
            (self.name_off, self.desc_off, self.flags) = struct.unpack_from('<III', h)
            self.bbox = list(struct.unpack_from('<6f', h, 12))
            (self.portal_start, self.portal_count, self.trans_batches, self.int_batches,
             self.ext_batches, self.pad) = struct.unpack_from('<6H', h, 36)
            self.fog_ids = list(h[48:52])
            self.liquid_type, self.unique_id, self.flags2, self.unused = struct.unpack_from('<4I', h, 52)
            self.sub = []     # [(tag, body)] in order, raw
            for t, b in _chunks(body, 68):
                self.sub.append([t, b])
        self._parse()

    def _get(self, tag, n=0):
        k = 0
        for t, b in self.sub:
            if t == tag:
                if k == n:
                    return b
                k += 1
        return None

    def _parse(self):
        mopy = self._get('MOPY') or b''
        self.tri_flags = list(mopy[0::2])
        self.tri_mats = list(mopy[1::2])
        movi = self._get('MOVI') or b''
        self.indices = list(struct.unpack_from('<%dH' % (len(movi) // 2), movi))
        movt = self._get('MOVT') or b''
        self.verts = [list(struct.unpack_from('<3f', movt, k * 12)) for k in range(len(movt) // 12)]
        monr = self._get('MONR') or b''
        self.normals = [list(struct.unpack_from('<3f', monr, k * 12)) for k in range(len(monr) // 12)]
        self.uvs = []
        k = 0
        while True:
            motv = self._get('MOTV', k)
            if motv is None:
                break
            self.uvs.append([list(struct.unpack_from('<2f', motv, i * 8)) for i in range(len(motv) // 8)])
            k += 1
        moba = self._get('MOBA') or b''
        self.batches = [list(struct.unpack_from('<6hIHHHBB', moba, k * 24)) for k in range(len(moba) // 24)]
        molr = self._get('MOLR') or b''
        self.light_refs = list(struct.unpack_from('<%dH' % (len(molr) // 2), molr))
        modr = self._get('MODR') or b''
        self.doodad_refs = list(struct.unpack_from('<%dH' % (len(modr) // 2), modr))
        mobn = self._get('MOBN') or b''
        self.bsp_nodes = [list(struct.unpack_from('<HhhHIf', mobn, k * 16)) for k in range(len(mobn) // 16)]
        mobr = self._get('MOBR') or b''
        self.bsp_refs = list(struct.unpack_from('<%dH' % (len(mobr) // 2), mobr))
        self.colors = []
        k = 0
        while True:
            mocv = self._get('MOCV', k)
            if mocv is None:
                break
            self.colors.append([bytearray(mocv[i * 4:i * 4 + 4]) for i in range(len(mocv) // 4)])
            k += 1
        self.liquid = self._get('MLIQ')

    def pack(self):
        bodies = {
            'MOPY': bytes(b for pair in zip(self.tri_flags, self.tri_mats) for b in pair),
            'MOVI': struct.pack('<%dH' % len(self.indices), *self.indices),
            'MOVT': b''.join(struct.pack('<3f', *v) for v in self.verts),
            'MONR': b''.join(struct.pack('<3f', *v) for v in self.normals),
            'MOBA': b''.join(struct.pack('<6hIHHHBB', *b) for b in self.batches),
            'MOLR': struct.pack('<%dH' % len(self.light_refs), *self.light_refs),
            'MODR': struct.pack('<%dH' % len(self.doodad_refs), *self.doodad_refs),
            'MOBN': b''.join(struct.pack('<HhhHIf', *n) for n in self.bsp_nodes),
            'MOBR': struct.pack('<%dH' % len(self.bsp_refs), *self.bsp_refs),
        }
        seen = {}
        sub = b''
        for t, b in self.sub:
            k = seen.get(t, 0)
            seen[t] = k + 1
            if t == 'MOTV':
                body = b''.join(struct.pack('<2f', *v) for v in self.uvs[k])
            elif t == 'MOCV':
                body = b''.join(bytes(c) for c in self.colors[k])
            elif t == 'MLIQ':
                body = self.liquid
            else:
                body = bodies.get(t, b)
            sub += _chunk(t, body)
        h = struct.pack('<III6f6H', self.name_off, self.desc_off, self.flags, *self.bbox,
                        self.portal_start, self.portal_count, self.trans_batches, self.int_batches,
                        self.ext_batches, self.pad)
        h += bytes(self.fog_ids) + struct.pack('<4I', self.liquid_type, self.unique_id, self.flags2, self.unused)
        out = b''.join(_chunk(t, b) for t, b in self.pre)
        return out + _chunk('MOGP', h + sub)

    def triangles(self):
        for k in range(len(self.indices) // 3):
            yield k, self.indices[3 * k], self.indices[3 * k + 1], self.indices[3 * k + 2]
