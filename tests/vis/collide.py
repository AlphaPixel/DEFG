"""Which contour's points should a grid cell keep when two contour lines pass through it?
Tests rules on etopo511B (and on the same data upside down, where every lake is a mesa) with an exact Delaunay
triangulation of the resulting nodes, counting the long sloping triangles that show as spikes across level ground.
Rules:
  average  DEFG before the shared-cell rule: all the cell's points averaged into one node
  lowest   the cell keeps its lowest elevation's points
  facing   an occupancy pass first (which cells hold any point). From a cell holding more than one elevation, 16 rays
           are walked across the occupancy raster until each meets an occupied cell; they add up to a direction toward
           the open ground. The cell keeps the elevation whose points lie furthest that way inside the cell. If the
           rays find no open ground, the points are averaged. This is the rule DEFG::OpenGround implements.
Usage: python -I collide.py <a smoothvis output folder>     (uses its etopo.pts; needs numpy and scipy)"""
import os
import sys

import numpy as np
from scipy.spatial import Delaunay

base = sys.argv[1]
pts0 = np.loadtxt(os.path.join(base, "etopo.pts"))      # col row z
COLS, ROWS = [(int(c), int(r)) for n, a, c, r in (l.split() for l in open(os.path.join(base, "manifest.txt"))) if n == "etopo"][0]
RAYS = [(np.cos(a), np.sin(a)) for a in np.arange(16) * np.pi / 8]
REACH, OPEN = 64, 4.0


def open_ground(occ, cx, cy):
    """sum of the free run of each ray times its direction; (0, 0) if no ray runs OPEN cells"""
    dx = dy = best = 0.0
    for ux, uy in RAYS:
        run = 0
        for t in range(1, REACH + 1):
            x, y = int(round(cx + t * ux)), int(round(cy + t * uy))
            if x < 0 or y < 0 or x >= COLS or y >= ROWS or occ[y, x]:
                break
            run = t
        dx += run * ux
        dy += run * uy
        best = max(best, run)
    return (dx, dy) if best >= OPEN and np.hypot(dx, dy) >= OPEN else (0.0, 0.0)


def nodes(pts, rule):
    cell = np.floor(pts[:, :2] + 1e-9).astype(np.int64)
    key = cell[:, 1] * 100000 + cell[:, 0]
    order = np.argsort(key, kind="stable")
    p, k = pts[order], key[order]
    occ = np.zeros((ROWS, COLS), bool)
    occ[cell[:, 1], cell[:, 0]] = True
    starts = np.r_[0, np.nonzero(np.diff(k))[0] + 1, len(k)]
    out, mixed, decided = [], 0, 0
    for a, b in zip(starts[:-1], starts[1:]):
        g = p[a:b]
        lv = np.unique(np.float32(g[:, 2]))
        if len(lv) == 1 or rule == "average":
            mixed += len(lv) > 1
            out.append(g.mean(axis=0))
            continue
        mixed += 1
        if rule == "lowest":
            keep = lv.min()
            decided += 1
        else:
            cx, cy = int(k[a] % 100000), int(k[a] // 100000)
            dx, dy = open_ground(occ, cx, cy)
            if dx == 0.0 and dy == 0.0:
                out.append(g.mean(axis=0))
                continue
            proj = g[:, 0] * dx + g[:, 1] * dy
            keep = np.float32(g[np.argmax(proj), 2])
            decided += 1
        out.append(g[np.float32(g[:, 2]) == keep].mean(axis=0))
    return np.array(out), mixed, decided


def spikes(nd, box=None):
    xy, z = nd[:, :2], nd[:, 2]
    s = Delaunay(xy).simplices
    e = np.stack([np.hypot(*(xy[s[:, i]] - xy[s[:, (i + 1) % 3]]).T) for i in range(3)], axis=1)
    area = 0.5 * np.abs(np.cross(xy[s[:, 1]] - xy[s[:, 0]], xy[s[:, 2]] - xy[s[:, 0]]))
    sel = e.max(axis=1) > 25
    if box:
        cen = xy[s].mean(axis=1)
        sel &= (cen[:, 0] > box[0]) & (cen[:, 0] < box[0] + box[2]) & (cen[:, 1] > box[1]) & (cen[:, 1] < box[1] + box[3])
    slop = sel & (np.ptp(z[s], axis=1) > 0.01)
    return int(slop.sum()), int((sel & ~slop).sum()), float(area[slop].sum())


flip = pts0.copy()
flip[:, 2] = pts0[:, 2].max() + pts0[:, 2].min() - pts0[:, 2]
print("long triangles (an edge over 25 cells): sloping / level, and the sloping ones' area in cells")
print("%-22s %-9s %14s %24s %10s" % ("data", "rule", "the lake area", "whole grid", "decided"))
for name, pts in (("etopo511B", pts0), ("etopo511B upside down", flip)):
    for rule in ("average", "lowest", "facing"):
        nd, mixed, decided = nodes(pts, rule)
        a = spikes(nd, (200, 300, 200, 200))
        w = spikes(nd)
        print("%-22s %-9s %5d / %-5d   %6d / %-5d %8.0f   %4d of %d" % (name, rule, a[0], a[1], w[0], w[1], w[2], decided, mixed))
