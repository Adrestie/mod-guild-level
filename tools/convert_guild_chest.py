# -*- coding: utf-8 -*-
# This file is part of mod-guild-level, GPL-2.0-or-later.
"""The Guild Chest of Cataclysm's Mobile Banking, made readable by a 3.3.5a client.

    python tools/convert_guild_chest.py <wow.export dir> [<output dir>]

<wow.export dir> holds what wow.export exported RAW (spaces removed from paths)
from a modern client: the
two models, their skins (unknown/<FileDataID>), and their textures as BLP and
PNG (the PNG of the banner cloth is what gets tinted). <output dir> defaults to
the module's data/ folder; it gets:

    art/World/GuildLevel/GuildChest/*.m2, *.skin, *.blp   the client files
    dbc/guild_level_GameObjectDisplayInfo.dbc             displays 92010, 92011

WHAT CHANGES, AND WHY. The models are Cataclysm's (version 272), wrapped by
later clients in a chunked file (MD21, plus SFID and TXID for the files they
name by number). 272 and 3.3.5a's 264 share their whole layout here: two bones,
inline animations, no particle, no ribbon. So the model is unwrapped and its
version set to 264, and only what 3.3.5a cannot do is redone:

  * TEXTURE NAMES. Modern files name their textures by FileDataID (TXID); the
    3.3.5a client wants a path in the model. Each texture gets one, in the
    module's own folder.
  * THE GUILD BANNER. The cloth is drawn by a Cataclysm shader with six
    textures, four of them the guild's colours and emblem (types 15 to 18),
    which 3.3.5a does not know. The cloth is baked once into a coloured
    texture -- Alliance blue, Horde red -- and drawn with one plain texture.
  * SHADERS. Every batch draws one texture with the plain shader 0: the two
    reflection maps (orbreflect*) of the chest's metal are dropped.
  * Texture coordinates get their lookup (empty in the source, which 3.3.5a
    reads), the skin its maximum bones, the header flags lose 0x80.
"""
import os
import struct
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_FOLDER = "World\\GuildLevel\\GuildChest"

# FileDataIDs of the source files, and the name each gets in the module.
SKINS = {"alliance": 509731, "horde": 509736}
SOURCE_MODELS = {"alliance": "world/replaceabletextureprops/guild/guildchest_alliance01.m2",
                 "horde": "world/replaceabletextureprops/guild/guildchest_horde01.m2"}
TEXTURES = {
    446011: ("world/skillactivated/containers/guildchest_alliance_01.blp", "GuildChest_Alliance.blp"),
    446013: ("world/skillactivated/containers/guildchest_horde_01.blp", "GuildChest_Horde.blp"),
    147183: ("item/objectcomponents/weapon/orbreflectbright.blp", "GuildChest_ReflectBright.blp"),
    244191: ("world/generic/human/passivedoodads/throne/orbreflectdarker.blp", "GuildChest_ReflectDark.blp"),
    341745: ("world/generic/horde/chest/chestgold01.blp", "GuildChest_Gold.blp"),
}
# The cloth (and its border, the same image) is baked; types 15 to 18 too.
CLOTH = {"alliance": (446012, 458531, "world/skillactivated/containers/guildchestcloth_alliance_01.png"),
         "horde": (446014, 458532, "world/skillactivated/containers/guildchestcloth_horde_01.png")}
BANNER_COLOUR = {"alliance": (46, 78, 168), "horde": (168, 30, 26)}

DISPLAY_IDS = {"alliance": 92010, "horde": 92011}
MODEL_NAMES = {"alliance": "GuildChest_Alliance01", "horde": "GuildChest_Horde01"}

# MD20 header offsets (264 and 272 alike).
VERSION, FLAGS = 0x04, 0x10
TEXTURE_ARRAY, REPLACEABLE_LOOKUP, TEX_UNIT_LOOKUP = 0x50, 0x68, 0x88
BOUNDING_BOX = 0xA0


def chunks(raw):
    found, pos = {}, 0
    while pos + 8 <= len(raw):
        tag = raw[pos:pos + 4].decode("ascii")
        size = struct.unpack_from("<I", raw, pos + 4)[0]
        found[tag] = raw[pos + 8:pos + 8 + size]
        pos += 8 + size
    return found


def append(md, data):
    """Data at the end of the model, 16-byte aligned; returns its offset."""
    while len(md) % 16:
        md.append(0)
    offset = len(md)
    md.extend(data)
    return offset


def convert_model(src, faction):
    parts = chunks(open(src, "rb").read())
    md = bytearray(parts["MD21"])
    assert md[:4] == b"MD20" and struct.unpack_from("<I", md, VERSION)[0] == 272
    txid = struct.unpack_from("<%dI" % (len(parts["TXID"]) // 4), parts["TXID"])

    struct.pack_into("<I", md, VERSION, 264)
    struct.pack_into("<I", md, FLAGS, struct.unpack_from("<I", md, FLAGS)[0] & 0x7)

    banner = "%s\\GuildChest_%sBanner.blp" % (OUT_FOLDER, faction.capitalize())
    count, offset = struct.unpack_from("<II", md, TEXTURE_ARRAY)
    for i in range(count):
        base = offset + 16 * i
        kind = struct.unpack_from("<I", md, base)[0]
        fdid = txid[i]
        if 15 <= kind <= 18 or fdid in CLOTH[faction][:2]:
            name = banner
        else:
            name = "%s\\%s" % (OUT_FOLDER, TEXTURES[fdid][1])
        raw_name = name.encode("ascii") + b"\x00"
        name_offset = append(md, raw_name)
        struct.pack_into("<IIII", md, base, 0, struct.unpack_from("<I", md, base + 4)[0], len(raw_name), name_offset)

    # Types 15 to 18 are gone: the lookup stops before them.
    lookup_count, lookup_offset = struct.unpack_from("<II", md, REPLACEABLE_LOOKUP)
    struct.pack_into("<II", md, REPLACEABLE_LOOKUP, min(lookup_count, 15), lookup_offset)

    # One texture coordinate set, the first, for every texture unit.
    struct.pack_into("<II", md, TEX_UNIT_LOOKUP, 1, append(md, struct.pack("<H", 0)))

    box = struct.unpack_from("<6f", md, BOUNDING_BOX)
    return bytes(md), box


def convert_skin(src):
    skin = bytearray(open(src, "rb").read())
    assert skin[:4] == b"SKIN"
    count, offset = struct.unpack_from("<II", skin, 36)
    for i in range(count):
        base = offset + 24 * i
        struct.pack_into("<H", skin, base + 2, 0)        # shader: plain, one texture
        struct.pack_into("<H", skin, base + 16, 1)       # texture count
    struct.pack_into("<I", skin, 44, 21)                 # bone count max, as 3.3.5a models
    return bytes(skin)


def write_blp(image, path):
    """BLP2, uncompressed BGRA (encoding 3), with every mip level."""
    image = image.convert("RGBA")
    width, height = image.size
    levels = []
    while True:
        r, g, b, a = image.split()
        levels.append(Image.merge("RGBA", (b, g, r, a)).tobytes())
        if image.size == (1, 1) or len(levels) == 16:
            break
        image = image.resize((max(1, image.size[0] // 2), max(1, image.size[1] // 2)), Image.LANCZOS)
    header_size = 148 + 1024
    offsets, sizes, pos = [], [], header_size
    for level in levels:
        offsets.append(pos)
        sizes.append(len(level))
        pos += len(level)
    offsets += [0] * (16 - len(offsets))
    sizes += [0] * (16 - len(sizes))
    with open(path, "wb") as f:
        f.write(b"BLP2" + struct.pack("<I4BII", 1, 3, 8, 8, 1, width, height))
        f.write(struct.pack("<16I", *offsets) + struct.pack("<16I", *sizes))
        f.write(b"\x00" * 1024)
        for level in levels:
            f.write(level)


def bake_banner(png, colour, path):
    """The grey cloth, multiplied by the faction's colour, opaque."""
    cloth = Image.open(png).convert("L")
    channels = [cloth.point(lambda v, c=c: v * c // 255) for c in colour]
    write_blp(Image.merge("RGBA", channels + [Image.new("L", cloth.size, 255)]), path)


def write_dbc(rows, path):
    """GameObjectDisplayInfo.dbc rows: id, model, 10 sounds, geobox, effect package."""
    strings = bytearray(b"\x00")
    records = bytearray()
    for display_id, model, box in rows:
        name_offset = len(strings)
        strings.extend(model.encode("ascii") + b"\x00")
        records.extend(struct.pack("<II10I6fI", display_id, name_offset, *([0] * 10), *box, 0))
    with open(path, "wb") as f:
        f.write(struct.pack("<4sIIII", b"WDBC", len(rows), 19, 76, len(strings)))
        f.write(records + strings)


def main():
    source = sys.argv[1]
    data = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "data")
    art = os.path.join(data, "art", *OUT_FOLDER.split("\\"))
    os.makedirs(art, exist_ok=True)
    os.makedirs(os.path.join(data, "dbc"), exist_ok=True)

    rows = []
    for faction in ("alliance", "horde"):
        model, box = convert_model(os.path.join(source, SOURCE_MODELS[faction]), faction)
        name = MODEL_NAMES[faction]
        open(os.path.join(art, name + ".m2"), "wb").write(model)
        skin = convert_skin(os.path.join(source, "unknown", str(SKINS[faction])))
        open(os.path.join(art, name + "00.skin"), "wb").write(skin)
        bake_banner(os.path.join(source, "png", CLOTH[faction][2]), BANNER_COLOUR[faction],
                    os.path.join(art, "GuildChest_%sBanner.blp" % faction.capitalize()))
        rows.append((DISPLAY_IDS[faction], "%s\\%s.mdx" % (OUT_FOLDER, name), box))
        print(faction, "model", len(model), "bytes, box", ["%.2f" % v for v in box])

    for fdid, (src, out) in TEXTURES.items():
        with open(os.path.join(source, "raw", src), "rb") as f:
            open(os.path.join(art, out), "wb").write(f.read())

    write_dbc(rows, os.path.join(data, "dbc", "guild_level_GameObjectDisplayInfo.dbc"))
    print("written to", os.path.abspath(data))


if __name__ == "__main__":
    main()
