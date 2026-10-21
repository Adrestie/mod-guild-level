"""A presentation camera for each Customization object, for the interface's turntable preview.

A Model frame of the 3.3.5 client draws with one of the model's own cameras (SetCamera); without
one it falls back to a view from above. The objects have none (or a portrait one framed for
another use): this adds one at the end of the model's camera list, in front of the object and a
little above it, looking at the middle of its height, at the distance where the whole object --
turned any way about its vertical axis -- fills the frame's height. The client's projection takes
the camera's field of view across the frame's diagonal (GxuXformCreateProjection_SG): FOV is
divided by sqrt(aspect^2 + 1) for the vertical.

    add(m2 bytes, aspect) -> (new m2 bytes, the camera's index)
"""
import math
import struct

FOV = 1.0                           # radians, across the diagonal
ELEVATION = math.radians(15)        # the camera's height angle
MARGIN = 1.04                       # a little room around the object

CAMERAS = 0x110                     # MD20 header: M2Array of cameras
BOUNDS = 0xA0                       # bounding box: min x, y, z, max x, y, z
CAMERA_SIZE = 116                   # modern layout: the field of view is a track


def _chunks(data):
    i = 0
    while i + 8 <= len(data):
        tag = data[i:i + 4]
        size = struct.unpack_from('<I', data, i + 4)[0]
        yield tag, i + 8, size
        i += 8 + size


def add(data, aspect):
    data = bytearray(data)
    md = next((at, size) for tag, at, size in _chunks(data) if tag == b'MD21')
    base, size = md
    md20 = bytearray(data[base:base + size])
    x0, y0, z0, x1, y1, z1 = struct.unpack_from('<6f', md20, BOUNDS)

    # turned about the vertical axis through the model's origin (SetFacing): the farthest corner
    # sweeps a cylinder; its height's middle is looked at
    rho = max(math.hypot(x, y) for x in (x0, x1) for y in (y0, y1))
    zc = (z0 + z1) / 2
    radius = max(0.05, math.hypot(rho, (z1 - z0) / 2))
    vertical = FOV / math.sqrt(aspect * aspect + 1)
    half = min(vertical, vertical * aspect) / 2
    distance = radius / math.sin(half) * MARGIN
    eye = (distance * math.cos(ELEVATION), 0.0, zc + distance * math.sin(ELEVATION))
    target = (0.0, 0.0, zc)

    count, offset = struct.unpack_from('<II', md20, CAMERAS)
    old = bytes(md20[offset:offset + count * CAMERA_SIZE]) if count else b''

    def align(buf):
        while len(buf) % 16:
            buf.append(0)

    align(md20)
    # one key at time 0 for each track: its times and values, each behind an array per animation
    def key_track(value_bytes):
        align(md20)
        times_at = len(md20)
        md20.extend(struct.pack('<I', 0))
        values_at = len(md20)
        md20.extend(value_bytes)
        align(md20)
        times_arr = len(md20)
        md20.extend(struct.pack('<II', 1, times_at))
        values_arr = len(md20)
        md20.extend(struct.pack('<II', 1, values_at))
        return struct.pack('<hhIIII', 0, -1, 1, times_arr, 1, values_arr)

    zero_vec = struct.pack('<9f', *([0.0] * 9))      # spline key: value, in and out tangents
    pos_track = key_track(zero_vec)
    target_track = key_track(zero_vec)
    roll_track = key_track(struct.pack('<3f', 0.0, 0.0, 0.0))
    fov_track = key_track(struct.pack('<3f', FOV, 0.0, 0.0))
    camera = (struct.pack('<Iff', 0, distance + radius * 3, max(0.05, distance - radius * 2))
              + pos_track + struct.pack('<3f', *eye)
              + target_track + struct.pack('<3f', *target)
              + roll_track + fov_track)
    assert len(camera) == CAMERA_SIZE

    align(md20)
    cameras_at = len(md20)
    md20.extend(old + camera)
    struct.pack_into('<II', md20, CAMERAS, count + 1, cameras_at)

    out = data[:base - 8] + b'MD21' + struct.pack('<I', len(md20)) + md20 + data[base + size:]
    return bytes(out), count
