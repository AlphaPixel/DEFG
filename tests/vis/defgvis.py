"""Numbers and pictures for judging a change to DEFG's gridding or smoothing.

smoothvis (smoothvis.cpp) grids a fixed set of cases with one copy of DEFG into a folder. Run it once per code version
or setting, each into its own folder, then compare the folders here. A variant is <folder>:<amount>, where <amount> is
the smoothing amount smoothvis used: 0, 50 or 100.

  python -I defgvis.py table <variant> [<variant> ...]
      Per case and variant: rms error against the true surface (synthetic cases only) | furthest outside the range
      of the input elevations | 99th percentile of the bend (second difference along rows and columns, a measure of
      creases) | cells with data.
  python -I defgvis.py sheet <out.png> <case> <variant>[,<variant>...] [col0 row0 width height [zoom]]
      The variants side by side as hill-shaded pictures, optionally one window of the grid, enlarged. All are shaded
      with the same vertical exaggeration, taken from the first variant.

Needs numpy and Pillow."""
import os
import sys

import numpy as np
from PIL import Image

NULL = -9999.0


def cases(folder):
    out = {}
    for line in open(os.path.join(folder, "manifest.txt")):
        name, amount, cols, rows = line.split()
        out[name] = (int(rows), int(cols))
    return out


def grid(variant, case):
    folder, amount = variant.rsplit(":", 1)
    return np.fromfile(os.path.join(folder, "%s_s%s.raw" % (case, amount)), dtype=np.float32).reshape(cases(folder)[case]).astype(np.float64)


def bend(z, ok):
    out = []
    for axis in (0, 1):
        a, m = np.moveaxis(z, axis, 0), np.moveaxis(ok, axis, 0)
        out.append(np.abs(a[2:] - 2 * a[1:-1] + a[:-2])[m[2:] & m[1:-1] & m[:-2]])
    return np.concatenate(out)


def shade(z, ok, zfactor):
    gy, gx = np.gradient(np.where(ok, z, np.nan) * zfactor)
    az, alt = np.radians(315.0), np.radians(45.0)
    lx, ly, lz = np.cos(alt) * np.sin(az), -np.cos(alt) * np.cos(az), np.sin(alt)
    lit = (-gx * lx - gy * ly + lz) / np.sqrt(gx * gx + gy * gy + 1.0)
    g = np.clip(36 + 219 * np.clip(lit, 0, 1), 0, 255)
    rgb = np.stack([g, g, g], axis=-1)
    rgb[~np.isfinite(lit)] = (104, 116, 134)
    return rgb.astype(np.uint8)


def table(variants):
    first = variants[0].rsplit(":", 1)[0]
    names = list(cases(first))
    print("rms error | furthest outside the data | bend p99 | cells with data")
    print("%-28s" % "variant" + "".join("%26s" % n for n in names))
    for v in variants:
        folder = v.rsplit(":", 1)[0]
        line = "%-28s" % (os.path.basename(os.path.normpath(folder)) + ":" + v.rsplit(":", 1)[1])[-28:]
        for case in names:
            z = grid(v, case)
            ok = z != NULL
            pts = np.loadtxt(os.path.join(folder, case + ".pts"))
            tpath = os.path.join(folder, case + "_truth.raw")
            rms = "-"
            if os.path.exists(tpath):
                truth = np.fromfile(tpath, dtype=np.float32).reshape(z.shape).astype(np.float64)
                rms = "%.3f" % np.sqrt(((z - truth)[ok] ** 2).mean())
            outside = max(0.0, z[ok].max() - pts[:, 2].max(), pts[:, 2].min() - z[ok].min())
            line += "%26s" % ("%s|%.2f|%.3f|%d" % (rms, outside, np.percentile(bend(z, ok), 99), ok.sum()))
        print(line)


def sheet(path, case, variants, box, zoom):
    ref = grid(variants[0], case)
    gy, gx = np.gradient(np.where(ref != NULL, ref, np.nan))
    slope = np.nanmedian(np.sqrt(gx * gx + gy * gy))
    zfactor = 0.6 / slope if slope > 0 else 1.0
    tiles = []
    for v in variants:
        z = grid(v, case)
        im = Image.fromarray(shade(z, z != NULL, zfactor))
        if box:
            im = im.crop((box[0], box[1], box[0] + box[2], box[1] + box[3]))
        tiles.append(im.resize((im.width * zoom, im.height * zoom), Image.NEAREST))
    w, h = tiles[0].size
    out = Image.new("RGB", (w * len(tiles) + 8 * (len(tiles) - 1), h), (255, 0, 255))
    for i, t in enumerate(tiles):
        out.paste(t, (i * (w + 8), 0))
    out.save(path)
    print(path, out.size)


if len(sys.argv) > 2 and sys.argv[1] == "table":
    table(sys.argv[2:])
elif len(sys.argv) in (5, 9, 10) and sys.argv[1] == "sheet":
    sheet(sys.argv[2], sys.argv[3], sys.argv[4].split(","), tuple(int(x) for x in sys.argv[5:9]) or None,
          int(sys.argv[9]) if len(sys.argv) > 9 else 1)
else:
    sys.exit(__doc__)
