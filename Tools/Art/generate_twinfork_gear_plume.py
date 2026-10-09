# Gear pieces and the rear plume cone. Units: centimetres. +X forward, +Y right, +Z up.
# The hull OBJ importer flips Y, so this writer does too.
import os
from pathlib import Path

OUT = str(Path(__file__).resolve().parents[2] / "SourceArt" / "TwinforkR1")


class Mesh:
    def __init__(self, name):
        self.name = name
        self.verts = []
        self.tris = []
        self.mats = []

    def mat(self, name):
        if name not in self.mats:
            self.mats.append(name)
        return self.mats.index(name)

    def v(self, x, y, z):
        self.verts.append((float(x), float(y), float(z)))
        return len(self.verts) - 1

    def tri(self, a, b, c, m):
        self.tris.append((a, b, c, m))

    def quad(self, a, b, c, d, m):
        self.tri(a, b, c, m)
        self.tri(a, c, d, m)

    def save(self):
        os.makedirs(OUT, exist_ok=True)
        path = os.path.join(OUT, self.name + ".obj")
        with open(path, "w", encoding="utf-8") as f:
            f.write("# %s verts=%d tris=%d\n" % (self.name, len(self.verts), len(self.tris)))
            for x, y, z in self.verts:
                f.write("v %.4f %.4f %.4f\n" % (x, -y, z))
            by = {}
            for a, b, c, m in self.tris:
                by.setdefault(m, []).append((a, c, b))
            for m, faces in by.items():
                f.write("usemtl %s\n" % self.mats[m])
                f.write("g %s\n" % self.mats[m])
                for a, b, c in faces:
                    f.write("f %d %d %d\n" % (a + 1, b + 1, c + 1))
        print(self.name, len(self.verts), len(self.tris), self.mats)
        return path


def box(mesh, x0, x1, y0, y1, z0, z1, m):
    v = [
        mesh.v(x0, y0, z0), mesh.v(x1, y0, z0), mesh.v(x1, y1, z0), mesh.v(x0, y1, z0),
        mesh.v(x0, y0, z1), mesh.v(x1, y0, z1), mesh.v(x1, y1, z1), mesh.v(x0, y1, z1),
    ]
    mesh.quad(v[0], v[3], v[2], v[1], m)
    mesh.quad(v[4], v[5], v[6], v[7], m)
    mesh.quad(v[0], v[1], v[5], v[4], m)
    mesh.quad(v[3], v[7], v[6], v[2], m)
    mesh.quad(v[0], v[4], v[7], v[3], m)
    mesh.quad(v[1], v[2], v[6], v[5], m)


def cone(mesh, length, radius, segments, m):
    """Unit-style cone along +X. Origin is the nozzle lip; the tip is at +X length."""
    base = []
    import math
    for i in range(segments):
        a = (i / segments) * math.pi * 2.0
        base.append(mesh.v(0.0, math.cos(a) * radius, math.sin(a) * radius))
    tip = mesh.v(length, 0.0, 0.0)
    origin = mesh.v(0.0, 0.0, 0.0)
    for i in range(segments):
        j = (i + 1) % segments
        mesh.tri(base[i], base[j], tip, m)
        mesh.tri(origin, base[j], base[i], m)


# Upper leg. Pivot is the hip. Deployed the knee hangs at local Z=-78.
# Stowed pitch -92 swings that knee aft, up against the belly.
UPPER = Mesh("SM_Twinfork_Gear_Upper")
ud = UPPER.mat("Dark")
uo = UPPER.mat("Orange")
box(UPPER, -16, 16, -16, 16, -10, 8, ud)          # hip collar
box(UPPER, -11, 11, -11, 11, -78, -6, ud)         # upper strut
box(UPPER, -15, 15, -15, 15, -86, -70, uo)        # knee block, pivot for the shin
UPPER.save()

# Lower leg. Pivot is the knee. Deployed the ankle is at local Z=-34.
# Hip -62 + knee -78 + shin -34 + sole -16 = world Z=-190, the spec ground line.
# Stowed the shin slides to local Z=-6 and nests inside the upper strut.
LOWER = Mesh("SM_Twinfork_Gear_Lower")
ld = LOWER.mat("Dark")
lo = LOWER.mat("Orange")
box(LOWER, -9, 9, -9, 9, -34, 4, ld)
box(LOWER, -13, 13, -13, 13, -12, 2, lo)          # guide collar that stays at the knee
LOWER.save()

# Foot. Pivot is the ankle. The sole sits 16cm below that pivot.
FOOT = Mesh("SM_Twinfork_Gear_Foot")
fd = FOOT.mat("Dark")
fo = FOOT.mat("Orange")
box(FOOT, -8, 8, -8, 8, -8, 4, fd)                # ankle post
box(FOOT, -28, 28, -20, 20, -18, -6, fd)          # pad
box(FOOT, -16, 16, -8, 8, -18, -12, fo)           # sole stripe
FOOT.save()

# Rear exhaust cone. Parent FX sockets already yaw 180, so +X on this mesh
# leaves the ship toward world -X. Wider and longer than the old stub.
PLUME = Mesh("SM_Twinfork_Plume_Wide")
cone(PLUME, 1.0, 1.0, 12, PLUME.mat("defaultMat"))
PLUME.save()
