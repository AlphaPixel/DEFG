"""PROTOTYPE, not in DEFG: densification as its designer describes it: after a first gridding, put a synthetic point at
the place furthest from every point (the middle of the biggest empty area), with the elevation the first surface has
there; repeat until no place is further than <radius> cells from a point; then grid again with the added points.
Usage: python -I densify.py <smoothvis output folder of the first pass> <etopo511B.xyz> <out data dir> <radius>
           [smoothing amount of the first pass]
Writes <out data dir>/etopo511B.xyz (the original points plus the synthetic ones) for a second smoothvis run.
Needs numpy and scipy."""
import os
import sys

import numpy as np
from scipy.spatial import cKDTree

first, src, outdata, radius = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
amount = int(sys.argv[5]) if len(sys.argv) > 5 else 100
NULL = -9999.0
raw = np.loadtxt(src)
# the gridding tool uses X = -A, Y = -B over the points' own extent
X, Y = -raw[:, 0], -raw[:, 1]
X0, X1, Y0, Y1 = X.min(), X.max(), Y.min(), Y.max()
rows, cols = None, None
for line in open(os.path.join(first, "manifest.txt")):
    name, a, c, r = line.split()
    if name == "etopo":
        cols, rows = int(c), int(r)
surf = np.fromfile(os.path.join(first, "etopo_s%d.raw" % amount), dtype=np.float32).reshape(rows, cols).astype(np.float64)
pts = np.loadtxt(os.path.join(first, "etopo.pts"))            # col row z
gy, gx = np.mgrid[0:rows, 0:cols]
ok = surf != NULL
d, _ = cKDTree(pts[:, :2]).query(np.c_[gx[ok], gy[ok]])
cx, cy, cz = gx[ok].astype(np.float64), gy[ok].astype(np.float64), surf[ok]
added = []
while True:
    i = int(np.argmax(d))
    if d[i] <= radius:
        break
    added.append((cx[i], cy[i], cz[i]))
    d = np.minimum(d, np.hypot(cx - cx[i], cy - cy[i]))
added = np.array(added)
os.makedirs(outdata, exist_ok=True)
with open(os.path.join(outdata, "etopo511B.xyz"), "w") as f:
    for a, b, z in raw:
        f.write("%.12e %.12e %.12e\n" % (a, b, z))
    for c, r, z in added:
        f.write("%.12e %.12e %.12e\n" % (-(X0 + c * (X1 - X0) / (cols - 1)), -(Y0 + r * (Y1 - Y0) / (rows - 1)), z))
print("%d synthetic points added to %d (no place now further than %.0f cells from a point; it was up to %.1f)" % (
    len(added), len(raw), radius, cKDTree(pts[:, :2]).query(np.c_[gx[ok], gy[ok]])[0].max()))
