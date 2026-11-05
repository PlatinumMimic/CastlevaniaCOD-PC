"""The pause menu's button bars (Summons, Item, Equip...) cleaned for the HD texture pack.

The game draws each bar from two sheets: first `c122d3ab...` (128x32), a streaky green brush stroke whose alpha is
the bar's fill and extent (u 1.32..2.14, wrapping: texels 41..128 then 0..18, across the bar's 130 pixels), then
`55517e5c...` (256x256) at u 0.4375..0.875, v 0.125..0.25: the pointed bar shape with the honeycomb, whose right
end is painted ragged, like a torn flag. Upscaled 4x (upscale.py) the streaks and the teeth come out hard-edged, so
on a 4K screen the bars trail off in jagged lines. This rewrites both in the upscaled pictures: the bar shapes'
ragged ends (two of them, every palette) become a smooth fade, and the brush stroke a soft fill that tapers over
the bar's right half, its colour the stroke's own, smoothed.

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
    # the buttons') and the plain one under it; the cut where the teeth begin, and where the fade ends
    XC, XE = 448 + 356, 448 + 452
    for f in sorted(glob.glob(os.path.join(src, "55517e5cdb2fca7c-*.png"))):
        a = np.array(Image.open(f).convert("RGBA")).astype(np.float32)
        for y0, y1 in [(116, 246), (246, 366)]:
            for y in range(y0, y1):
                row = a[y]
                if row[XC - 1, 3] < 20:  # the row ends before the cut (the pointed tip's short rows): its teeth go
                    row[XC:XE + 8, 3] = 0
                    continue
                px = row[XC - 4:XC].mean(0)  # the colour and alpha just before the cut, carried on and faded out
                n = XE - XC
                fade = 1 - smooth(np.arange(n) / float(n - 1))
                row[XC:XE, :3] = px[:3]
                row[XC:XE, 3] = px[3] * fade
                row[XE:XE + 8, 3] = 0
        Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).save(os.path.join(out, os.path.basename(f)))
        print(os.path.basename(f), "bars cleaned")


def smooth_stroke(src, out):
    # the brush stroke, 512x128 upscaled: its colour smoothed wide (a 49 x 25 box, wrapping across), its alpha a
    # profile along the drawn span (t 0 at the bar's left, 1 at its right end): steady over the left half, fading
    # over the right, soft at the top and bottom
    for f in sorted(glob.glob(os.path.join(src, "c122d3ab*.png"))):
        a = np.array(Image.open(f).convert("RGBA")).astype(np.float32)
        h, w = a.shape[:2]
        col = a[..., :3]
        k = 24
        pad = np.concatenate([col[:, -k:], col, col[:, :k]], axis=1)
        cs = np.cumsum(np.pad(pad, ((0, 0), (1, 0), (0, 0))), axis=1)
        colx = ((cs[:, 2 * k + 1:] - cs[:, :-(2 * k + 1)]) / (2 * k + 1))[:, :w]
        cy = np.cumsum(np.pad(colx, ((1, 0), (0, 0), (0, 0))), axis=0)
        ky = 12
        coly = np.empty_like(colx)
        for y in range(h):
            y0, y1 = max(0, y - ky), min(h, y + ky + 1)
            coly[y] = (cy[y1] - cy[y0]) / (y1 - y0)
        x = np.arange(w) / w * 128.0  # texel coordinates of the 128-wide texture
        t = np.where(x >= 41, (x - 41) / 105.0, (x + 128 - 41) / 105.0)
        hprof = np.where(t < 0.5, 1.0, 1 - smooth((t - 0.5) / 0.5))
        hprof = np.where(t > 1, 0, hprof)
        yy = (np.arange(h) + 0.5) / h
        vprof = 1 - smooth(np.abs(yy - 0.5) / 0.5) ** 1.5
        a[..., :3] = coly
        a[..., 3] = 0.7 * 128 * hprof[None, :] * vprof[:, None]  # the PS2's alpha, 128 opaque
        Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).save(os.path.join(out, os.path.basename(f)))
        print(os.path.basename(f), "stroke smoothed")


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
