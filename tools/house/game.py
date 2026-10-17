"""Reading WMOs (3.3.5, v17) out of the client's archives."""
import struct
import sys

sys.path.insert(0, r'E:\WoW-mods\installer')
import core  # noqa: E402

CLIENT = r'E:\world of warcraft 3.3.5a hd'
_client = None


def client():
    global _client
    if _client is None:
        _client = core.Client(CLIENT)
    return _client


def read(name):
    name = name.replace('/', chr(92))
    c = client()
    w = c.winner(name)
    if not w:
        return None
    return c.open(w).read(name)


def listfile():
    """Every name of every archive's (listfile)."""
    c = client()
    names = set()
    for _, path in c.archives():
        a = c.open(path)
        if a is None or not a.contains('(listfile)'):
            continue
        try:
            text = a.read('(listfile)').decode('latin1')
        except Exception:
            continue
        names.update(n.strip() for n in text.splitlines() if n.strip())
    return names


def chunks(data, start=0, end=None):
    i, end = start, len(data) if end is None else end
    while i + 8 <= end:
        tag = data[i:i + 4][::-1].decode('latin1')
        size = struct.unpack_from('<I', data, i + 4)[0]
        yield tag, i + 8, size
        i += 8 + size


class Group(object):
    def __init__(self, data):
        self.raw = data
        self.chunks = {}
        for tag, off, size in chunks(data):
            if tag == 'MOGP':
                self.header = data[off:off + 68]
                self.flags = struct.unpack_from('<I', data, off + 8)[0]
                self.bbox = struct.unpack_from('<6f', data, off + 12)
                for t, o, s in chunks(data, off + 68, off + size):
                    self.chunks.setdefault(t, []).append(data[o:o + s])
        movt = self.chunks.get('MOVT', [b''])[0]
        self.verts = [struct.unpack_from('<3f', movt, k * 12) for k in range(len(movt) // 12)]
        movi = self.chunks.get('MOVI', [b''])[0]
        self.indices = struct.unpack_from('<%dH' % (len(movi) // 2), movi)
        mopy = self.chunks.get('MOPY', [b''])[0]
        self.polys = [struct.unpack_from('<BB', mopy, k * 2) for k in range(len(mopy) // 2)]

    def triangles(self):
        for k in range(len(self.indices) // 3):
            a, b, c = self.indices[3 * k:3 * k + 3]
            yield self.polys[k] if k < len(self.polys) else (0, 0), self.verts[a], self.verts[b], self.verts[c]


class Root(object):
    def __init__(self, name):
        self.name = name
        self.data = read(name)
        self.chunks = {}
        for tag, off, size in chunks(self.data):
            self.chunks[tag] = self.data[off:off + size]
        h = struct.unpack_from('<9I', self.chunks['MOHD'])
        self.ngroups = h[1]
        mogi = self.chunks.get('MOGI', b'')
        self.group_info = [struct.unpack_from('<I6fi', mogi, g * 32) for g in range(len(mogi) // 32)]
        mogn = self.chunks.get('MOGN', b'')
        self.group_names = []
        for gi in self.group_info:
            n = gi[7]
            self.group_names.append(mogn[n:mogn.index(b'\0', n)].decode('latin1') if 0 <= n < len(mogn) else '')

    def group(self, g):
        return Group(read('%s_%03d.wmo' % (self.name[:-4], g)))
