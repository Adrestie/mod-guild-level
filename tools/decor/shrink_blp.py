"""Brings the BLP2 textures of a folder down to a largest side of 512 (or the size given): the
mip levels above it are dropped, the next one becomes the first. Nothing is resampled, so the
texture below that size is the same as before. Textures without mip levels are left as they are.

    python shrink_blp.py <folder> [size]
"""
import glob
import os
import struct
import sys

HEADER = 148                        # magic, type, encoding bytes, size, 16 offsets, 16 sizes


def shrink(data, limit):
    """The texture without its levels above limit, or None when it needs nothing."""
    if data[:4] != b'BLP2':
        return None
    compression, has_mips = data[8], data[11]
    width, height = struct.unpack_from('<II', data, 12)
    if max(width, height) <= limit or not has_mips:
        return None
    offsets = list(struct.unpack_from('<16I', data, 20))
    sizes = list(struct.unpack_from('<16I', data, 84))
    drop = 0
    while max(width >> drop, height >> drop) > limit and drop < 15 and offsets[drop + 1] and sizes[drop + 1]:
        drop += 1
    if not drop:
        return None
    # the palette of a palettized texture follows the header
    prefix = data[HEADER:HEADER + 1024] if compression == 1 else b''
    levels = [(offsets[i], sizes[i]) for i in range(drop, 16) if offsets[i] and sizes[i]]
    out = bytearray(data[:HEADER]) + prefix
    new_offsets, new_sizes = [], []
    for ofs, size in levels:
        new_offsets.append(len(out))
        new_sizes.append(size)
        out += data[ofs:ofs + size]
    new_offsets += [0] * (16 - len(new_offsets))
    new_sizes += [0] * (16 - len(new_sizes))
    struct.pack_into('<II', out, 12, max(1, width >> drop), max(1, height >> drop))
    struct.pack_into('<16I', out, 20, *new_offsets)
    struct.pack_into('<16I', out, 84, *new_sizes)
    return bytes(out)


def main(folder, limit=512):
    before = after = changed = 0
    for path in glob.glob(os.path.join(folder, '*.blp')):
        data = open(path, 'rb').read()
        before += len(data)
        new = shrink(data, limit)
        if new is not None:
            open(path, 'wb').write(new)
            changed += 1
            data = new
        after += len(data)
    print('%d texture(s) shrunk, %.1f MB -> %.1f MB' % (changed, before / 1e6, after / 1e6))


if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 512)
