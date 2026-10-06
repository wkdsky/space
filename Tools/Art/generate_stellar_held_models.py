"""Author all stellar weapons as short upright scepters (centimetres, Z-up).

Grip origin is inside the handle. The spherical core is centred at (0, 0, 36).
Shell, Panel, Core material slots are stable across all twelve variants.
"""
from pathlib import Path
import math

ROOT = Path(__file__).resolve().parents[2] / "SourceArt" / "StellarWeapons"
NAMES = ("Jet", "Focus", "BlackHole", "Explosion", "Healing", "Freezing",
         "Shaping", "Effect", "Diffusion", "Shadow", "Instance", "Disassembly")


class Mesh:
    def __init__(self):
        self.faces = []

    def face(self, points, material=0, smooth_centre=None):
        for i in range(1, len(points) - 1):
            self.faces.append((material, (points[0], points[i], points[i + 1]), smooth_centre))

    def lathe(self, profile, material=0, segments=24, caps=False, smooth_centre=None):
        rings = [[(radius * math.cos(i * math.tau / segments),
                   radius * math.sin(i * math.tau / segments), z)
                  for i in range(segments)] for z, radius in profile]
        for a, b in zip(rings, rings[1:]):
            for i in range(segments):
                j = (i + 1) % segments
                self.face((a[i], a[j], b[j], b[i]), material, smooth_centre)
        if caps:
            self.face(list(reversed(rings[0])), material)
            self.face(rings[-1], material)

    def save(self, name):
        ROOT.mkdir(parents=True, exist_ok=True)
        out = ["mtllib StellarWeapons.mtl", f"o {name}", "s off"]
        index = 1
        for material, label in enumerate(("Shell", "Panel", "Core")):
            out.append(f"usemtl {label}")
            for mat, points, smooth in self.faces:
                if mat != material:
                    continue
                # Compensate the UE OBJ importer's Y reflection while keeping outward winding.
                pts = [(x, -y, z) for x, y, z in (points[0], points[2], points[1])]
                u = tuple(pts[1][k] - pts[0][k] for k in range(3))
                v = tuple(pts[2][k] - pts[0][k] for k in range(3))
                n = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
                length = math.sqrt(sum(t*t for t in n))
                if length < 1e-8:
                    continue
                for p, uv in zip(pts, ((0, 0), (1, 0), (0, 1))):
                    normal = tuple(t / length for t in n)
                    if smooth:
                        normal = (p[0] - smooth[0], p[1] + smooth[1], p[2] - smooth[2])
                        size = math.sqrt(sum(t*t for t in normal))
                        normal = tuple(t / size for t in normal)
                    out.append("v %.6f %.6f %.6f" % p)
                    out.append("vt %.4f %.4f" % uv)
                    out.append("vn %.6f %.6f %.6f" % normal)
                out.append("f " + " ".join(f"{i}/{i}/{i}" for i in range(index, index + 3)))
                index += 3
        (ROOT / f"{name}.obj").write_text("\n".join(out) + "\n", encoding="utf-8")
        print(f"{name}: {(index - 1) // 3} triangles")


def scepter(variant):
    m = Mesh()
    m.lathe(((-14, 2.8), (-13, 4.0), (-10, 4.0), (-9, 2.8),
             (9, 2.8), (12, 3.5), (17, 2.5), (24, 3.2)), caps=True)
    for z in (-8, -4, 0, 4, 8):
        m.lathe(((z, 2.85), (z + 0.7, 2.85)), material=1, caps=True)
    m.lathe(((-13, 4.05), (-11, 4.05), (-11, 3.8), (-13, 3.8), (-13, 4.05)), material=1)
    m.lathe(((11, 3.55), (12.2, 3.55)), material=2, caps=True)
    # Accessories vary the collar; all retain the same orb-and-staff silhouette.
    facets = (8, 12, 16, 6, 10, 8, 6, 12, 16, 10, 8, 12)[variant]
    collar_radius = 5.8 + (variant % 3) * 0.45
    m.lathe(((21, 3.3), (23, collar_radius), (26, collar_radius), (28, 4.4)),
            material=1, segments=facets, caps=True)
    m.lathe(((23.8, collar_radius + 0.1), (24.6, collar_radius + 0.1)),
            material=2, segments=facets, caps=True)
    profile = [(36 + 9 * math.sin(-math.pi/2 + i * math.pi/24),
                9 * math.cos(-math.pi/2 + i * math.pi/24)) for i in range(25)]
    m.lathe(profile, material=2, segments=32, smooth_centre=(0, 0, 36))
    return m


if __name__ == "__main__":
    ROOT.mkdir(parents=True, exist_ok=True)
    (ROOT / "StellarWeapons.mtl").write_text(
        "newmtl Shell\nKd 0.08 0.11 0.15\nnewmtl Panel\nKd 0.5 0.6 0.7\nnewmtl Core\nKd 0.2 0.8 1\n",
        encoding="utf-8")
    for variant, name in enumerate(NAMES):
        scepter(variant).save("SM_Stellar" + name)
