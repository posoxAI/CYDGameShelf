# Adds a surface texture to the printed parts as a mesh step (STL only; the STEP files stay smooth).
# Draft 4. usage: python3 texture.py peel|micro [step_mm] [out_dir]
#   peel  - flattened orange peel: soft lumps ~1.5-2.5 mm across with flat tops, up to 0.25 mm high
#   micro - micro-texture / subtle noise: fine grain ~0.5 mm across, about 0.1 mm deep
# The texture fades out 1.5 mm before every edge and hole, so the parts still fit and the screen, screws,
# nuts, speaker holes, chin grooves and the stylus dip stay smooth.
import sys
from pathlib import Path
import numpy as np
import trimesh
import manifold3d as mf
from scipy.ndimage import gaussian_filter
import case as C
import cadquery as cq

KIND = sys.argv[1] if len(sys.argv) > 1 else "peel"
STEP = float(sys.argv[2]) if len(sys.argv) > 2 else (0.3 if KIND == "peel" else 0.15)
OUT = Path(sys.argv[3]) if len(sys.argv) > 3 else Path(__file__).parent / f"out-{KIND}"
OUT.mkdir(exist_ok=True)
FADE, DEPTH = 1.5, 0.6


def height(X, seed):
    rng = np.random.default_rng(seed)
    noise = rng.standard_normal(X.shape)
    if KIND == "peel":
        f = gaussian_filter(noise, 0.75 / STEP, mode="wrap")
        lo, hi = np.percentile(f, 25), np.percentile(f, 88)
        u = np.clip((f - lo) / (hi - lo), 0, 1)
        return 0.25 * u * u * (3 - 2 * u)                         # flat valleys, flattened tops
    f = gaussian_filter(noise, 0.22 / STEP, mode="wrap") + 0.35 * gaussian_filter(rng.standard_normal(X.shape), 0.7 / STEP, mode="wrap")
    f = (f - f.min()) / (f.max() - f.min())
    return 0.11 * f                                               # gentle grain, no flat areas


def patch(x0, x1, y0, y1, z_surf, outward, holes, seed):
    xs = np.arange(x0, x1 + STEP / 2, STEP)
    ys = np.arange(y0, y1 + STEP / 2, STEP)
    X, Y = np.meshgrid(xs, ys, indexing="ij")
    h = height(X, seed)
    d = np.minimum.reduce([X - x0, x1 - X, Y - y0, y1 - Y])
    for hole in holes:
        if hole[0] == "rect":
            _, a0, a1, b0, b1 = hole
            d = np.minimum(d, np.hypot(np.maximum.reduce([a0 - X, 0 * X, X - a1]), np.maximum.reduce([b0 - Y, 0 * Y, Y - b1])))
        else:
            _, hx, hy, r = hole
            d = np.minimum(d, np.hypot(X - hx, Y - hy) - r)
    m = np.clip(d / FADE, 0, 1)
    h = h * m * m * (3 - 2 * m)
    nx, ny = X.shape
    top = np.c_[X.ravel(), Y.ravel(), (z_surf + outward * (h - 0.02)).ravel()]
    zb = z_surf - outward * DEPTH
    idx = np.arange(nx * ny).reshape(nx, ny)
    a, b, c, d4 = idx[:-1, :-1].ravel(), idx[1:, :-1].ravel(), idx[1:, 1:].ravel(), idx[:-1, 1:].ravel()
    tris = [np.c_[a, b, c], np.c_[a, c, d4]]
    # boundary loop of the top grid, counter-clockwise seen from +z
    loop = list(idx[:, 0]) + list(idx[-1, 1:]) + list(idx[-2::-1, -1]) + list(idx[0, -2:0:-1])
    nb = len(loop)
    base = top.shape[0]
    bot = np.c_[top[loop, 0], top[loop, 1], np.full(nb, zb)]
    ctr = np.array([[(x0 + x1) / 2, (y0 + y1) / 2, zb]])
    verts = np.vstack([top, bot, ctr])
    k = np.arange(nb)
    lp = np.array(loop)
    side = [np.c_[lp[(k + 1) % nb], lp[k], base + k], np.c_[lp[(k + 1) % nb], base + k, base + (k + 1) % nb]]
    bottom = np.c_[base + (k + 1) % nb, base + k, np.full(nb, base + nb)]
    faces = np.vstack(tris + side + [bottom])
    mesh = trimesh.Trimesh(verts, faces, process=False)
    if mesh.volume < 0:
        mesh.invert()
    keep = C.box(x0, x1, y0, y1, zb - outward * 0.1 if outward < 0 else zb - 0.1, z_surf + 1.0) if outward > 0 else \
        C.box(x0, x1, y0, y1, z_surf - 1.0, zb + 0.1)
    for hole in holes:
        if hole[0] == "rect":
            keep = keep.cut(C.box(hole[1], hole[2], hole[3], hole[4], -50, 50))
        else:
            keep = keep.cut(C.cyl(hole[1], hole[2], -50, 50, 2 * hole[3]))
    return to_manifold(mesh), to_manifold(cq_mesh(keep))


def cq_mesh(wp):
    v, t = wp.val().tessellate(0.01, 0.1)
    return trimesh.Trimesh(np.array([[p.x, p.y, p.z] for p in v]), np.array(t), process=True)


def to_manifold(mesh):
    mesh.merge_vertices()
    m = mf.Manifold(mf.Mesh(vert_properties=np.asarray(mesh.vertices, dtype=np.float32), tri_verts=np.asarray(mesh.faces, dtype=np.uint32)))
    assert m.status() == mf.Error.NoError, m.status()
    return m


def mirror_x(m):
    return m.mirror([1, 0, 0]).translate([C.W, 0, 0])            # model frame -> real geometry, like case.true_shape


def apply(part_file, regions):
    part = trimesh.load(C.Path(__file__).parent / "out" / part_file)
    pm = to_manifold(part)
    for seed, args in regions:
        tex, keep = patch(*args, seed=seed)
        pm = pm + mirror_x(tex ^ keep)
    out = pm.to_mesh()
    res = trimesh.Trimesh(np.asarray(out.vert_properties)[:, :3], np.asarray(out.tri_verts), process=False)
    res.export(OUT / part_file)
    return len(res.faces), res.is_watertight, res.bounds


if __name__ == "__main__":
    wx0, wx1, wy0, wy1 = C.WIN
    gy0, gy1 = C.WIN[3] + 4.0, C.WIN[3] + 4.0 + 2.5 * 5 + C.GROOVE_W
    face_holes = [("rect", wx0 - 1.8, wx1 + 1.8, wy0 - 1.8, wy1 + 1.8), ("rect", 11.2, 42.8, gy0 - 0.8, gy1 + 0.8),
                  ("rect", C.CH_X1 - 1.0, C.W + 1, C.L - C.DIP_L - 1.0, C.L + 1)] + [("circ", x, y, 4.0) for x, y in C.HOLES]
    m = C.CH_FRONT + 0.6
    print("face", apply("cyd-case-face.stl", [(11, (m, C.W - m, m, C.L - m, 0.0, -1, face_holes))]))
    m = C.CH_BACK + 0.6
    back_holes = [("circ", C.SPK_C[0], C.SPK_C[1], 4.8)] + [("circ", x, y, 4.1) for x, y in C.HOLES if y > C.L / 2]
    print("back", apply("cyd-case-back.stl", [(23, (m, C.W - m, C.STEP_Y + C.RAMP + 0.8, C.L - m, C.T, +1, back_holes))]))
    x0, x1 = C.WALL - C.LEDGE + C.GAP + 0.8, C.W - C.WALL + C.LEDGE - C.GAP - 0.8
    y0, y1 = C.WALL_END - C.LEDGE + C.GAP + 0.8, C.BAY_Y1 + C.LEDGE - C.GAP - 0.8
    lid_holes = [("circ", C.LID_SCREW[0], C.LID_SCREW[1], 4.2)]
    print("lid", apply("cyd-case-battery-lid.stl", [(37, (x0, x1, y0, y1, C.Z_TOP, +1, lid_holes))]))
