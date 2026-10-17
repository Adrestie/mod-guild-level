"""BLP2 textures to RGBA numpy arrays (palette, DXT1/3/5, raw)."""
import struct

import numpy as np


def _565(c):
    c = c.astype(np.uint32)
    r = ((c >> 11) & 31) * 255 // 31
    g = ((c >> 5) & 63) * 255 // 63
    b = (c & 31) * 255 // 31
    return np.stack([r, g, b], -1).astype(np.int32)


def _dxt_colors(blocks, dxt1):
    c0 = blocks[:, 0].astype(np.uint16) | (blocks[:, 1].astype(np.uint16) << 8)
    c1 = blocks[:, 2].astype(np.uint16) | (blocks[:, 3].astype(np.uint16) << 8)
    p0, p1 = _565(c0), _565(c1)
    four = (c0 > c1) | (not dxt1)
    four = np.asarray(four)[:, None]
    p2 = np.where(four, (2 * p0 + p1) // 3, (p0 + p1) // 2)
    p3 = np.where(four, (p0 + 2 * p1) // 3, 0)
    pal = np.stack([p0, p1, p2, p3], 1)                 # n,4,3
    a = np.full((len(blocks), 4), 255, np.int32)
    if dxt1:
        a[:, 3] = np.where(four[:, 0], 255, 0)
    bits = blocks[:, 4:8].astype(np.uint32)
    word = bits[:, 0] | (bits[:, 1] << 8) | (bits[:, 2] << 16) | (bits[:, 3] << 24)
    idx = np.stack([(word >> (2 * k)) & 3 for k in range(16)], 1)   # n,16
    rgb = np.take_along_axis(pal, idx[:, :, None].repeat(3, 2), 1)
    alpha = np.take_along_axis(a, idx, 1)
    return rgb, alpha


def _dxt(data, w, h, kind):
    bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
    size = 8 if kind == 1 else 16
    raw = np.frombuffer(data[:bw * bh * size], np.uint8).reshape(-1, size)
    if kind == 1:
        rgb, alpha = _dxt_colors(raw, True)
    else:
        rgb, _ = _dxt_colors(raw[:, 8:], False)
        if kind == 3:
            ab = raw[:, :8]
            nib = np.stack([(ab[:, k // 2] >> (4 * (k % 2))) & 15 for k in range(16)], 1).astype(np.int32)
            alpha = nib * 17
        else:
            a0 = raw[:, 0].astype(np.int32)
            a1 = raw[:, 1].astype(np.int32)
            bits = np.zeros(len(raw), np.uint64)
            for k in range(6):
                bits |= raw[:, 2 + k].astype(np.uint64) << np.uint64(8 * k)
            idx = np.stack([((bits >> np.uint64(3 * k)) & np.uint64(7)).astype(np.int32) for k in range(16)], 1)
            pal = np.zeros((len(raw), 8), np.int32)
            pal[:, 0], pal[:, 1] = a0, a1
            gt = (a0 > a1)[:, None]
            for k in range(2, 8):
                pal[:, k] = np.where(gt[:, 0], ((8 - k) * a0 + (k - 1) * a1) // 7,
                                     np.where(k < 6, ((6 - k) * a0 + (k - 1) * a1) // 5, 0 if k == 6 else 255))
            alpha = np.take_along_axis(pal, idx, 1)
    px = np.concatenate([rgb, alpha[:, :, None]], 2).reshape(bh, bw, 4, 4, 4)
    img = px.transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 4)
    return img[:h, :w].astype(np.uint8)


def decode(data, mip=0):
    assert data[:4] == b'BLP2', data[:4]
    _, comp, alpha_depth, alpha_type, has_mips = struct.unpack_from('<IBBBB', data, 4)
    w, h = struct.unpack_from('<II', data, 12)
    offs = struct.unpack_from('<16I', data, 20)
    sizes = struct.unpack_from('<16I', data, 84)
    while mip > 0 and (not offs[mip] or not sizes[mip]):
        mip -= 1
    w, h = max(1, w >> mip), max(1, h >> mip)
    body = data[offs[mip]:offs[mip] + sizes[mip]]
    if comp == 2:
        kind = 1 if alpha_type == 0 else (3 if alpha_type == 1 else 5)
        return _dxt(body, w, h, kind)
    if comp == 1:
        pal = np.frombuffer(data[148:148 + 1024], np.uint8).reshape(256, 4)[:, [2, 1, 0, 3]].copy()
        idx = np.frombuffer(body[:w * h], np.uint8)
        img = pal[idx].reshape(h, w, 4)
        if alpha_depth == 0:
            img[:, :, 3] = 255
        elif alpha_depth == 8:
            img[:, :, 3] = np.frombuffer(body[w * h:w * h * 2], np.uint8).reshape(h, w)
        elif alpha_depth == 1:
            bits = np.unpackbits(np.frombuffer(body[w * h:], np.uint8), bitorder='little')[:w * h]
            img[:, :, 3] = (bits * 255).reshape(h, w)
        elif alpha_depth == 4:
            nb = np.frombuffer(body[w * h:], np.uint8)
            vals = np.stack([nb & 15, nb >> 4], 1).reshape(-1)[:w * h]
            img[:, :, 3] = (vals * 17).reshape(h, w)
        return img
    raw = np.frombuffer(body[:w * h * 4], np.uint8).reshape(h, w, 4)
    return raw[:, :, [2, 1, 0, 3]].copy()
