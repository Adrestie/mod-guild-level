"""The faction crests of the guild level-up toast (its top icon, where the modern client puts the
faction's emblem): the Alliance lion and the Horde emblem of the modern guild frame sheet
(interface/guildframe/guildframe.blp). The sheet keeps them as a faint watermark (alpha about 32):
from its colour export (on black), the shape is the brightness and the colour is divided back by it. Written as
Interface/GuildLevel/CrestAlliance.tga and CrestHorde.tga, 128 x 128, the crest centred.

    python bake_crests.py <colour png>     (wow.export PNG export, without alpha)
"""
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
OUT = os.path.join(ROOT, 'data', 'art', 'Interface', 'GuildLevel')
CRESTS = {'Alliance': (326, 306, 438, 452), 'Horde': (442, 306, 552, 452)}
SIDE = 128


def main(colour_png, alpha_png=None):
    colour = Image.open(colour_png).convert('RGB')
    for name, box in CRESTS.items():
        rgb = np.asarray(colour.crop(box)).astype(np.float32)
        # the crest lies on black: its coverage is its brightness (soft edges kept), and the
        # colour is divided back by it
        cover = np.clip(rgb.max(axis=2) / 90.0, 0, 1)
        cover[cover < 0.12] = 0
        safe = np.where(cover > 0, cover, 1)[..., None]
        rgb = np.clip(rgb / safe, 0, 255)
        crest = Image.fromarray(np.dstack([rgb, cover * 255]).astype(np.uint8), 'RGBA')
        bb = crest.getbbox()
        crest = crest.crop(bb)
        k = (SIDE - 8) / max(crest.size)
        crest = crest.resize((round(crest.width * k), round(crest.height * k)), Image.LANCZOS)
        out = Image.new('RGBA', (SIDE, SIDE), (0, 0, 0, 0))
        out.alpha_composite(crest, ((SIDE - crest.width) // 2, (SIDE - crest.height) // 2))
        out.save(os.path.join(OUT, 'Crest%s.tga' % name))
        print(name, crest.size)


if __name__ == '__main__':
    main(sys.argv[1])
