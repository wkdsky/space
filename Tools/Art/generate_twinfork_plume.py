# Exhaust plume cone. Origin at the nozzle throat, +X is the exhaust direction.
# The spacecraft presentation component scales X for length and YZ for width.
import os
from pathlib import Path

OUT = str(Path(__file__).resolve().parents[2] / "SourceArt" / "TwinforkR1")


class Mesh:
    def __init__(self, name):
        self.name = name
        self.verts = []
        self.tris = []

    def v(self, x, y, z):
        self.verts.append((float(x), float(y), float(z)))
        return len(self.verts) - 1

    def tri(self, a, b, c):
        self.tris.append((a, b, c))

    def save(self):
        os.makedirs(OUT, exist_ok=True)
        path = os.path.join(OUT, self.name + ".obj")
        with open(path, "w", encoding="utf-8") as f:
            f.write("# %s verts=%d tris=%d\n" % (self.name, len(self.verts), len(self.tris)))
            # UE OBJ import flips Y. Negate it so +Y stays right.
            for x, y, z in self.verts:
                f.write("v %.4f %.4f %.4f\n" % (x, -y, z))
            f.write("usemtl Plume\n")
            for a, b, c in self.tris:
                f.write("f %d %d %d\n" % (a + 1, b + 1, c + 1))
        print(self.name, len(self.verts), len(self.tris))
        return path


def ring(mesh, x, radius, sides):
    import math
    ids = []
    for i in range(sides):
        a = (math.pi * 2.0) * i / sides
        ids.append(mesh.v(x, math.cos(a) * radius, math.sin(a) * radius))
    return ids


def connect(mesh, a, b):
    n = len(a)
    for i in range(n):
        j = (i + 1) % n
        mesh.tri(a[i], b[i], b[j])
        mesh.tri(a[i], b[j], a[j])


PLUME = Mesh("SM_Twinfork_Plume")
# Unit length along +X. Base radius 1 so component scale is the visible size.
SIDES = 8
rings = [
    ring(PLUME, 0.0, 0.55, SIDES),
    ring(PLUME, 0.18, 1.0, SIDES),
    ring(PLUME, 0.55, 0.72, SIDES),
    ring(PLUME, 1.0, 0.18, SIDES),
]
for i in range(len(rings) - 1):
    connect(PLUME, rings[i], rings[i + 1])
tip = PLUME.v(1.08, 0.0, 0.0)
n = len(rings[-1])
for i in range(n):
    PLUME.tri(rings[-1][i], tip, rings[-1][(i + 1) % n])
# Throat cap so the flame reads as a solid tongue from the side.
throat = PLUME.v(0.0, 0.0, 0.0)
for i in range(n):
    PLUME.tri(throat, rings[0][(i + 1) % n], rings[0][i])
PLUME.save()
