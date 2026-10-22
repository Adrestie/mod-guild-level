"""A presentation camera for each Customization object, for the interface's turntable preview.

A Model frame of the 3.3.5 client draws with one of the model's own cameras (SetCamera); without
one it falls back to a view from above. The objects have none (or a portrait one framed for
another use): this adds one at the end of the model's camera list. The camera rides with the model
(turning the model in the frame turns the camera with it), so the turntable is the camera's: it
orbits the object once every ORBIT milliseconds, a little above it, looking at the middle of its
height, on a global loop of its own (independent of the model's animations).

Its distance is the least at which the object -- seen from any side -- fits the frame: the circle
its farthest corner sweeps about the vertical axis in the frame's width, its height in the frame's
height. The client's projection takes the camera's field of view across the frame's diagonal
(GxuXformCreateProjection_SG): FOV divided by sqrt(aspect^2 + 1) is the vertical.

    add(m2 bytes, aspect, box) -> (new m2 bytes, the camera's index); box: what the model draws
    (build_decor.bounds), the header's box holding its lights' and particles' reach too
"""
import math
import struct

FOV = 1.0                           # radians, across the diagonal
ELEVATION = math.radians(12)        # the camera's height angle
MARGIN = 1.06                       # a little room around the object
ORBIT = 12000                       # milliseconds a turn
KEYS = 48                           # keys of the orbit

GLOBAL_LOOPS = 0x14                 # MD20 header: M2Array of global loop durations
CAMERAS = 0x110                     # M2Array of cameras
BOUNDS = 0xA0                       # bounding box: min x, y, z, max x, y, z
CAMERA_SIZE = 116                   # modern layout: the field of view is a track


def _chunks(data):
    i = 0
    while i + 8 <= len(data):
        tag = data[i:i + 4]
        size = struct.unpack_from('<I', data, i + 4)[0]
        yield tag, i + 8, size
        i += 8 + size


def _distance(x0, y0, z0, x1, y1, z1, aspect):
    """The camera's distance from the vertical axis."""
    rho = max(math.hypot(x, y) for x in (x0, x1) for y in (y0, y1))
    rho = max(rho, 0.05)
    half_h = max((z1 - z0) / 2, 0.05)
    vertical = FOV / math.sqrt(aspect * aspect + 1)
    tv = math.tan(vertical / 2)
    th = aspect * tv
    # the object's nearest point is rho closer than its axis; its silhouette is at most rho wide
    # and its height at most half_h from the middle
    by_width = rho / th + rho
    by_height = half_h / tv + rho
    return max(by_width, by_height) * MARGIN, rho, half_h


def add(data, aspect, box=None):
    """box: what to frame (min x, y, z, max x, y, z), else the header's bounding box."""
    data = bytearray(data)
    base, size = next((at, size) for tag, at, size in _chunks(data) if tag == b'MD21')
    md20 = bytearray(data[base:base + size])
    x0, y0, z0, x1, y1, z1 = box or struct.unpack_from('<6f', md20, BOUNDS)
    distance, rho, half_h = _distance(x0, y0, z0, x1, y1, z1, aspect)
    zc = (z0 + z1) / 2
    height = zc + distance * math.tan(ELEVATION)

    def align():
        while len(md20) % 16:
            md20.append(0)

    # the orbit's own loop
    nloops, oloops = struct.unpack_from('<II', md20, GLOBAL_LOOPS)
    loops = bytes(md20[oloops:oloops + nloops * 4]) if nloops else b''
    align()
    loops_at = len(md20)
    md20.extend(loops + struct.pack('<I', ORBIT))
    struct.pack_into('<II', md20, GLOBAL_LOOPS, nloops + 1, loops_at)
    orbit_loop = nloops

    count, offset = struct.unpack_from('<II', md20, CAMERAS)
    old = bytes(md20[offset:offset + count * CAMERA_SIZE]) if count else b''

    def track(times, values, interpolation=0, loop=-1):
        """An M2Track of one animation: its times, its values (spline keys), behind arrays."""
        align()
        times_at = len(md20)
        md20.extend(struct.pack('<%dI' % len(times), *times))
        align()
        values_at = len(md20)
        md20.extend(values)
        align()
        times_arr = len(md20)
        md20.extend(struct.pack('<II', len(times), times_at))
        values_arr = len(md20)
        md20.extend(struct.pack('<II', len(times), values_at))
        return struct.pack('<hhIIII', interpolation, loop, 1, times_arr, 1, values_arr)

    # the orbit: the camera's position around the vertical axis, linear between its keys
    times, keys = [], b''
    for k in range(KEYS + 1):
        a = 2 * math.pi * k / KEYS
        times.append(int(ORBIT * k / KEYS))
        keys += struct.pack('<9f', distance * math.cos(a), distance * math.sin(a), height, 0, 0, 0, 0, 0, 0)
    pos_track = track(times, keys, interpolation=1, loop=orbit_loop)
    target_track = track([0], struct.pack('<9f', 0.0, 0.0, zc, 0, 0, 0, 0, 0, 0))
    roll_track = track([0], struct.pack('<3f', 0.0, 0.0, 0.0))
    fov_track = track([0], struct.pack('<3f', FOV, 0.0, 0.0))
    reach = distance + max(rho, half_h) * 2
    # the clip planes wide: the frame scales the model (and the camera with it) by the interface's
    # size, the planes stay as they are
    camera = (struct.pack('<Iff', 0, reach * 50, 0.01)
              + pos_track + struct.pack('<3f', 0.0, 0.0, 0.0)
              + target_track + struct.pack('<3f', 0.0, 0.0, 0.0)
              + roll_track + fov_track)
    assert len(camera) == CAMERA_SIZE

    align()
    cameras_at = len(md20)
    md20.extend(old + camera)
    struct.pack_into('<II', md20, CAMERAS, count + 1, cameras_at)

    out = data[:base - 8] + b'MD21' + struct.pack('<I', len(md20)) + md20 + data[base + size:]
    return bytes(out), count
