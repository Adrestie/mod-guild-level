"""The textures of the guild level-up toast, after the modern client's renown toast
(MajorFactionCelebrationBannerTemplate), for a 3.3.5 interface that has no MaskTexture:

- Interface/GuildLevel/Toast.tga: the atlas evergreentoastcelebration (512 x 512) as it is; the pieces
  are cut by texture coordinates (GuildLevelUp_Client.lua).
- Interface/GuildLevel/ToastShine.tga: the shine sweeping the top filigree, MASKED by
  evergreen-toast-celebration-mask as the client masks it, baked frame by frame: 16 frames of
  512 x 128 (the mask's width, the shine's band), 2 columns x 8 rows. The source: the shine at
  (-200, 150) moving 400 to the right in 1 s with smoothing OUT, the mask at (0, 150), both from the
  toast's centre; the mask frame's alpha 0.4 is left to the Lua.

    python bake_toast.py <atlas png> <mask png>     (wow.export PNG exports, alpha included)
"""
import json
import os
import sys

from PIL import Image

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
OUT = os.path.join(ROOT, 'data', 'art', 'Interface', 'GuildLevel')
SHINE = (285, 1, 184, 98)                 # evergreen-toast-celebration-shine in the atlas
FRAMES, COLS = 16, 2
BAND_W, BAND_H = 512, 128


def main(atlas_png, mask_png):
    os.makedirs(OUT, exist_ok=True)
    atlas = Image.open(atlas_png).convert('RGBA')
    atlas.save(os.path.join(OUT, 'Toast.tga'))

    mask = Image.open(mask_png).convert('RGBA').split()[3]          # 512 x 512, centred on (0, 150)
    band_mask = mask.crop((0, 256 - BAND_H // 2, 512, 256 + BAND_H // 2))
    x, y, w, h = SHINE
    shine = atlas.crop((x, y, x + w, y + h))
    sheet = Image.new('RGBA', (BAND_W * COLS, BAND_H * (FRAMES // COLS)), (0, 0, 0, 0))
    for i in range(FRAMES):
        t = i / (FRAMES - 1)
        dx = -200 + 400 * (1 - (1 - t) ** 2)                         # smoothing OUT
        frame = Image.new('RGBA', (BAND_W, BAND_H), (0, 0, 0, 0))
        # the band's centre is the mask's centre: the shine's centre at dx from it
        frame.alpha_composite(shine, (round(BAND_W / 2 + dx - w / 2), round(BAND_H / 2 - h / 2)))
        r, g, b, a = frame.split()
        a = Image.fromarray(((__import__('numpy').asarray(a, dtype='float32') *
                              __import__('numpy').asarray(band_mask, dtype='float32') / 255.0)).astype('uint8'))
        frame = Image.merge('RGBA', (r, g, b, a))
        sheet.paste(frame, ((i % COLS) * BAND_W, (i // COLS) * BAND_H))
    sheet.save(os.path.join(OUT, 'ToastShine.tga'))
    print('Toast.tga', atlas.size, 'ToastShine.tga', sheet.size)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
