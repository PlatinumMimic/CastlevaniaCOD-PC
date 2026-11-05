"""The pause menu's button bars (Summons, Item, Equip...) cleaned for the HD texture pack.

The game draws each bar from two sheets: first `c122d3ab...` (128x32), a streaky green brush stroke whose alpha is
the bar's fill and extent (u 1.32..2.14, wrapping: texels 41..128 then 0..18, across the bar's 130 pixels), then
`55517e5c...` (256x256) at u 0.4375..0.875, v 0.125..0.25: the pointed bar shape with the honeycomb, whose right
end is painted ragged, like a torn flag. The game scrolls the stroke along the bar (u moves 0.002 a frame, the
texture's width in 8.5 s), a slow sheen in its streaks. Upscaled 4x (upscale.py) the streaks and the teeth come out
hard-edged, so on a 4K screen the bars trail off in jagged lines, and a stroke given a fade of its own moves with
the scroll (Julian: the slots' background moved; it is to be solid). This rewrites both in the upscaled pictures:
the bar shapes' ragged ends (two of them, every palette) become a fade to black in the shape's colour (its alpha
is nothing over the bar: the draw takes its colour, scaled by the stroke's alpha) that ends where the stroke's draw
does (texel 103 of 112, the bar's 400th pixel of 271..411), and the stroke a fill the same along its length (its
own colour and alpha averaged across), so the scroll changes nothing.

usage: python -I tools/texpack/clean_menu_bars.py <upscaled pictures> <out folder>

Checked on cvtest's pausemenu scenario at 1080p and 4K: solid green bars with the honeycomb at the left, the labels
sharp, a soft end, no streaks or teeth (5 Nov 2026)."""
import glob
import os
import sys

import numpy as np
from PIL import Image


def smooth(t):  # smoothstep
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


def clean_bars(src, out):
    # the bar shapes: x 448..896 of the 1024 sheet (u 0.4375..0.875), the pointed bar at rows 116..246 (v 0.125..0.25,
    # the buttons') and the plain one under it. The shape's alpha is 0 over the bar (its colour is what the draw
    # takes, scaled by the stroke's alpha left in the frame), with alpha only on its edge lines and the teeth at the
    # right; so the bar's end is made in its colour: darkened to black from texel 88 to 103 of the drawn region (the
    # bar's 381st to 400th pixel of 271..411, where the stroke's draw ends), and the teeth's alpha cleared
    XC, XE = 448 + 88 * 4, 448 + 103 * 4
    n = XE - XC
    fade = (1 - smooth(np.arange(n) / float(n - 1)))[:, None]
    for f in sorted(glob.glob(os.path.join(src, "55517e5cdb2fca7c-*.png"))):
        a = np.array(Image.open(f).convert("RGBA")).astype(np.float32)
        for y0, y1 in [(116, 246), (246, 366)]:
            a[y0:y1, XC:XE, :3] *= fade[None, :, :]
            a[y0:y1, XE:448 + 452, :3] = 0
            a[y0:y1, XC:448 + 452, 3] = 0
        Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).save(os.path.join(out, os.path.basename(f)))
        print(os.path.basename(f), "bars' ends darkened, their teeth cleared")


def smooth_stroke(src, out):
    # the brush stroke, 512x128 upscaled: the same along its length, as the game scrolls it: each row's colour and
    # alpha averaged across (the rows then smoothed down, 25 of them)
    for f in sorted(glob.glob(os.path.join(src, "c122d3ab*.png"))):
        a = np.array(Image.open(f).convert("RGBA")).astype(np.float32)
        h, w = a.shape[:2]
        rows = a.mean(axis=1)  # h x 4
        ky = 12
        sm = np.empty_like(rows)
        for y in range(h):
            y0, y1 = max(0, y - ky), min(h, y + ky + 1)
            sm[y] = rows[y0:y1].mean(0)
        a[..., :3] = sm[:, None, :3]
        a[..., 3] = sm[:, 3][:, None]  # the stroke's own alpha (the PS2's, 128 opaque), as even as the game drew it
        Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).save(os.path.join(out, os.path.basename(f)))
        print(os.path.basename(f), "stroke made even")


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    clean_bars(src, out)
    smooth_stroke(src, out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
