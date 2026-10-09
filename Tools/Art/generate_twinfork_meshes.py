# Twinfork R1 mesh generator. Units: centimetres. +X forward, +Y right, +Z up.
import math
import os
from pathlib import Path

OUT = str(Path(__file__).resolve().parents[2] / "SourceArt" / "TwinforkR1")

class Mesh:
    def __init__(self, name):
        self.name = name
        self.verts = []
        self.tris = []  # (a,b,c,mat)
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
            f.write("# %s\n" % self.name)
            for name in self.mats:
                f.write("usemtl %s\n" % name)  # declaration only; groups below
            current = None
            # rewrite properly
        with open(path, "w", encoding="utf-8") as f:
            f.write("# %s verts=%d tris=%d\n" % (self.name, len(self.verts), len(self.tris)))
            # UE's OBJ importer flips Y. Write the negated Y so the asset
            # matches the spec: +X forward, +Y right, +Z up.
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


def lerp(a, b, t):
    return a + (b - a) * t


def sample_stations(stations, x):
    if x <= stations[0][0]:
        return stations[0]
    if x >= stations[-1][0]:
        return stations[-1]
    for i in range(len(stations) - 1):
        x0, hw0, zb0, zt0 = stations[i]
        x1, hw1, zb1, zt1 = stations[i + 1]
        if x0 <= x <= x1:
            t = 0.0 if x1 == x0 else (x - x0) / (x1 - x0)
            return (x, lerp(hw0, hw1, t), lerp(zb0, zb1, t), lerp(zt0, zt1, t))
    return stations[-1]


def ring(mesh, x, hw, zb, zt, cy, cw, ch):
    """Chamfered rectangle ring. cw/ch are chamfer sizes."""
    cw = min(cw, hw * 0.9)
    ch = min(ch, (zt - zb) * 0.45)
    pts = [
        (x, cy - hw + cw, zb),
        (x, cy + hw - cw, zb),
        (x, cy + hw, zb + ch),
        (x, cy + hw, zt - ch),
        (x, cy + hw - cw, zt),
        (x, cy - hw + cw, zt),
        (x, cy - hw, zt - ch),
        (x, cy - hw, zb + ch),
    ]
    return [mesh.v(*p) for p in pts]


def connect_rings(mesh, a, b, m, cap_a=False, cap_b=False, flip_walls=False, flip_caps=False):
    n = len(a)
    for i in range(n):
        j = (i + 1) % n
        if flip_walls:
            # Default loft walls point into the tube. The closed shell needs the opposite.
            mesh.quad(a[i], a[j], b[j], b[i], m)
        else:
            mesh.quad(a[i], b[i], b[j], a[j], m)
    if cap_a:
        # fan, winding so normal points -X (away from increasing ring index)
        c = mesh.v(
            sum(mesh.verts[i][0] for i in a) / n,
            sum(mesh.verts[i][1] for i in a) / n,
            sum(mesh.verts[i][2] for i in a) / n,
        )
        for i in range(n):
            if flip_caps:
                mesh.tri(c, a[i], a[(i + 1) % n], m)
            else:
                mesh.tri(c, a[(i + 1) % n], a[i], m)
    if cap_b:
        c = mesh.v(
            sum(mesh.verts[i][0] for i in b) / n,
            sum(mesh.verts[i][1] for i in b) / n,
            sum(mesh.verts[i][2] for i in b) / n,
        )
        for i in range(n):
            if flip_caps:
                mesh.tri(c, b[(i + 1) % n], b[i], m)
            else:
                mesh.tri(c, b[i], b[(i + 1) % n], m)


def loft(mesh, stations, cy, cw, ch, m, xs, flip_walls=False, flip_caps=False):
    rings = []
    for x in xs:
        s = sample_stations(stations, x)
        rings.append(ring(mesh, s[0], s[1], s[2], s[3], cy, cw, ch))
    for i in range(len(rings) - 1):
        connect_rings(
            mesh, rings[i], rings[i + 1], m,
            cap_a=(i == 0), cap_b=(i == len(rings) - 2),
            flip_walls=flip_walls,
            flip_caps=flip_caps,
        )
    return rings


def box(mesh, x0, x1, y0, y1, z0, z1, m, chamfer=0.0):
    """Axis-aligned chamfered box. chamfer applies to all edges via an octagon if > 0 on YZ, and X ends stay flat-ish via short bevel rings."""
    if chamfer <= 0.01:
        v = [
            mesh.v(x0, y0, z0), mesh.v(x1, y0, z0), mesh.v(x1, y1, z0), mesh.v(x0, y1, z0),
            mesh.v(x0, y0, z1), mesh.v(x1, y0, z1), mesh.v(x1, y1, z1), mesh.v(x0, y1, z1),
        ]
        # bottom, top, -Y, +Y, -X, +X
        mesh.quad(v[0], v[3], v[2], v[1], m)
        mesh.quad(v[4], v[5], v[6], v[7], m)
        mesh.quad(v[0], v[1], v[5], v[4], m)
        mesh.quad(v[3], v[7], v[6], v[2], m)
        mesh.quad(v[0], v[4], v[7], v[3], m)
        mesh.quad(v[1], v[2], v[6], v[5], m)
        return
    c = chamfer
    # build as loft of two chamfered rings
    hw = (y1 - y0) * 0.5
    cy = (y0 + y1) * 0.5
    a = ring(mesh, x0, hw, z0, z1, cy, c, c)
    b = ring(mesh, x1, hw, z0, z1, cy, c, c)
    connect_rings(mesh, a, b, m, cap_a=True, cap_b=True)


def octagon_2d(hx, hy, cut):
    """Points in a local XY plane, CCW, for an octagon of half-size hx,hy."""
    cx = min(cut, hx * 0.45)
    cy = min(cut, hy * 0.45)
    return [
        (-hx + cx, -hy), (hx - cx, -hy), (hx, -hy + cy), (hx, hy - cy),
        (hx - cx, hy), (-hx + cx, hy), (-hx, hy - cy), (-hx, -hy + cy),
    ]


def nozzle(mesh, origin, axis, outer, inner, depth, wall, m_body, m_glow, sides=8):
    """
    origin is the EXIT face center. axis points OUT of the nozzle.
    outer/inner are (half_w, half_h) of the exit frame. depth is how far the housing goes INTO the hull (opposite axis).
    A recessed throat is built so the cavity is real geometry.
    """
    ax = axis
    # build an orthonormal frame: ax, right, up
    if abs(ax[2]) > 0.9:
        right = (1.0, 0.0, 0.0)
    else:
        right = (0.0, 0.0, 1.0)
    # right = normalize(cross(up_guess, ax))? We want a stable frame.
    # For +X/-X nozzles, up is +Z and right is +Y when looking along +X? Keep it simple.
    if abs(ax[0]) > 0.5:
        right = (0.0, 1.0, 0.0)
        up = (0.0, 0.0, 1.0)
    else:
        right = (1.0, 0.0, 0.0)
        up = (0.0, 1.0, 0.0)
    def P(t, y, z):
        return (
            origin[0] + ax[0] * t + right[0] * y + up[0] * z,
            origin[1] + ax[1] * t + right[1] * y + up[1] * z,
            origin[2] + ax[2] * t + right[2] * y + up[2] * z,
        )
    ow, oh = outer
    iw, ih = inner
    cut_o = min(ow, oh) * 0.38
    cut_i = min(iw, ih) * 0.38
    outer_pts = octagon_2d(ow, oh, cut_o)
    inner_pts = octagon_2d(iw, ih, cut_i)
    # t=0 exit face, t=-wall outer lip back, t=-depth throat back
    lip = wall
    throat = depth
    Ro = [mesh.v(*P(0.0, y, z)) for y, z in outer_pts]
    Ri = [mesh.v(*P(-0.6, y, z)) for y, z in inner_pts]
    Bo = [mesh.v(*P(-lip, y, z)) for y, z in outer_pts]
    # cavity walls from inner lip to throat
    Ti = [mesh.v(*P(-throat, y * 0.86, z * 0.86)) for y, z in inner_pts]
    Glow = [mesh.v(*P(-throat + 1.5, y * 0.55, z * 0.55)) for y, z in inner_pts]
    n = len(Ro)
    # front annulus
    for i in range(n):
        j = (i + 1) % n
        mesh.quad(Ro[i], Ro[j], Ri[j], Ri[i], m_body)
    # outer side from exit to back lip
    for i in range(n):
        j = (i + 1) % n
        mesh.quad(Ro[i], Bo[i], Bo[j], Ro[j], m_body)
    # cavity
    for i in range(n):
        j = (i + 1) % n
        mesh.quad(Ri[i], Ri[j], Ti[j], Ti[i], m_body)
    # glow cap (faces outward toward exit, so normal +axis)
    gc = mesh.v(*P(-throat + 1.5, 0, 0))
    for i in range(n):
        mesh.tri(gc, Glow[(i + 1) % n], Glow[i], m_glow)
    for i in range(n):
        j = (i + 1) % n
        mesh.quad(Ti[i], Glow[i], Glow[j], Ti[j], m_body)
    # back outer cap ring so the housing is closed
    bc = mesh.v(*P(-lip, 0, 0))
    for i in range(n):
        mesh.tri(bc, Bo[i], Bo[(i + 1) % n], m_body)


def panel_frame(mesh, x, y, z, w, h, depth, recess, m_frame, m_cap, normal="x"):
    """
    Recessed cap. The frame is a bezel; the cap sits in the recess and is a separate mesh when requested.
    Here we only build the frame into `mesh` if m_cap is None; caller builds cap separately.
    normal: 'x' face looks +X, '-x', 'z', '-y', 'y'
    w along the first tangent, h along the second.
    """
    return  # built by dedicated helpers


# ---------- hull ----------
SIDE = [
    (-650, 78, -42, 68),
    (-580, 85, -50, 95),
    (-460, 105, -65, 110),
    (380, 105, -65, 110),
    (520, 96, -55, 98),
    (650, 80, -40, 75),
]
SIDE_XS = [-650, -600, -580, -520, -460, -200, 0, 180, 380, 500, 580, 650]

CABIN = [
    (-390, 125, -55, 135),
    (-300, 132, -55, 190),
    (-200, 135, -55, 230),
    (-80, 135, -55, 230),
    (40, 128, -55, 175),
    (120, 118, -55, 120),
    (210, 110, -55, 85),
    (270, 80, -50, 60),
]
CABIN_XS = [-390, -300, -200, -80, 0, 40, 90, 150, 210, 270]

HULL = Mesh("SM_Twinfork_R1_Hull")
M_HULL = HULL.mat("Hull")
M_STRIPE = HULL.mat("Stripe")
M_DARK = HULL.mat("Dark")
M_GLASS = HULL.mat("Glass")
M_CYAN = HULL.mat("Cyan")
M_AMBER = HULL.mat("Amber")
M_ORANGE = HULL.mat("Orange")

# side hulls
loft(HULL, SIDE, -345, 16, 14, M_HULL, SIDE_XS)
loft(HULL, SIDE, 345, 16, 14, M_HULL, SIDE_XS)

# Tail bridge. Spec envelope is Y=-430..430, Z to 110, but the side-hull
# shoulders beside the cabin are taller (top about 119). A deck that stops
# at 110 leaves a dark step at each cabin-rear corner. Raise the deck to the
# shoulder, bury it 70cm into the cabin (boarding starts at the door jamb
# X=-338, ahead of this deck) and out through both hulls. Sharp corners.
box(HULL, -580, -320, -450, 450, -55, 122, M_HULL, chamfer=0)

# cabin body
loft(HULL, CABIN, 0, 18, 16, M_HULL, CABIN_XS)

# orange stripe bands on the outer upper shoulder of each side hull (separate thin shells, not a recolor of whole faces)
def stripe(cy_sign):
    cy = 345 * cy_sign
    xs = [-520, -200, 80, 420, 600]
    for i in range(len(xs) - 1):
        s0 = sample_stations(SIDE, xs[i])
        s1 = sample_stations(SIDE, xs[i + 1])
        # band on the outer top edge
        y_outer0 = cy + cy_sign * (s0[1] - 6)
        y_inner0 = cy + cy_sign * (s0[1] - 34)
        y_outer1 = cy + cy_sign * (s1[1] - 6)
        y_inner1 = cy + cy_sign * (s1[1] - 34)
        z0 = s0[3] - 8
        z1 = s1[3] - 8
        a = HULL.v(xs[i], y_inner0, z0)
        b = HULL.v(xs[i], y_outer0, z0 + 2)
        c = HULL.v(xs[i + 1], y_outer1, z1 + 2)
        d = HULL.v(xs[i + 1], y_inner1, z1)
        e = HULL.v(xs[i], y_inner0, z0 - 16)
        f = HULL.v(xs[i], y_outer0, z0 - 10)
        g = HULL.v(xs[i + 1], y_outer1, z1 - 10)
        h = HULL.v(xs[i + 1], y_inner1, z1 - 16)
        HULL.quad(a, b, c, d, M_STRIPE)
        HULL.quad(e, h, g, f, M_STRIPE)
        HULL.quad(b, f, g, c, M_STRIPE)
        HULL.quad(a, d, h, e, M_STRIPE)

stripe(1)
stripe(-1)
# Dorsal stripe across the tail bridge. Ends are buried in the side hulls
# and the forward end is buried under the taller cabin roof.
box(HULL, -560, -340, -400, 400, 116, 124, M_STRIPE, chamfer=0)

# windshield: sloped quad glass from x=-80 high to x=170 low, inset from the cabin sides
def windshield():
    # frame
    outer = [
        HULL.v(-70, -78, 168),
        HULL.v(-70, 78, 168),
        HULL.v(165, 58, 78),
        HULL.v(165, -58, 78),
    ]
    inner = [
        HULL.v(-62, -66, 158),
        HULL.v(-62, 66, 158),
        HULL.v(152, 46, 86),
        HULL.v(152, -46, 86),
    ]
    # frame band
    for i in range(4):
        j = (i + 1) % 4
        HULL.quad(outer[i], outer[j], inner[j], inner[i], M_DARK)
    # glass pane slightly inset
    g = [
        HULL.v(-60, -62, 154),
        HULL.v(-60, 62, 154),
        HULL.v(148, 42, 90),
        HULL.v(148, -42, 90),
    ]
    HULL.quad(g[0], g[1], g[2], g[3], M_GLASS)
    # side trapezoid windows
    for s in (-1, 1):
        sw = [
            HULL.v(-40, s * 128, 150),
            HULL.v(70, s * 118, 130),
            HULL.v(70, s * 118, 70),
            HULL.v(-40, s * 128, 78),
        ]
        # push outward a little so it reads as a pane
        pane = [HULL.v(HULL.verts[i][0], HULL.verts[i][1] + s * 2.0, HULL.verts[i][2]) for i in sw]
        if s > 0:
            HULL.quad(pane[0], pane[3], pane[2], pane[1], M_GLASS)
        else:
            HULL.quad(pane[0], pane[1], pane[2], pane[3], M_GLASS)
        # dark frame
        fr = [HULL.v(HULL.verts[i][0], HULL.verts[i][1] + s * 1.2, HULL.verts[i][2]) for i in [
            HULL.v(-48, s * 136, 162), HULL.v(78, s * 126, 140), HULL.v(78, s * 126, 58), HULL.v(-48, s * 136, 66)
        ]]
        if s > 0:
            HULL.quad(fr[0], fr[1], pane[1], pane[0], M_DARK)
            HULL.quad(fr[1], fr[2], pane[2], pane[1], M_DARK)
            HULL.quad(fr[2], fr[3], pane[3], pane[2], M_DARK)
            HULL.quad(fr[3], fr[0], pane[0], pane[3], M_DARK)
        else:
            HULL.quad(fr[0], pane[0], pane[1], fr[1], M_DARK)
            HULL.quad(fr[1], pane[1], pane[2], fr[2], M_DARK)
            HULL.quad(fr[2], pane[2], pane[3], fr[3], M_DARK)
            HULL.quad(fr[3], pane[3], pane[0], fr[0], M_DARK)

windshield()

# door frame on the inboard corridor, left side of cabin (Y negative). Sliding door is a separate mesh.
# Opening roughly X=-330..-215, Y=-135, Z= -10..110
box(HULL, -338, -330, -142, -128, -15, 118, M_DARK, chamfer=3)
box(HULL, -215, -207, -142, -128, -15, 118, M_DARK, chamfer=3)
box(HULL, -330, -215, -142, -128, 108, 122, M_DARK, chamfer=3)
box(HULL, -330, -215, -142, -128, -28, -12, M_DARK, chamfer=3)

# main gun housing under cabin nose: X=135..300, octagonal, muzzle opening
def main_gun():
    # housing as lofted octagon
    def gun_ring(x, hw, zb, zt):
        return ring(HULL, x, hw, zb, zt, 0, 14, 12)
    rings = [
        gun_ring(135, 40, -116, -32),
        gun_ring(210, 47, -120, -28),
        gun_ring(270, 42, -112, -36),
        gun_ring(300, 35, -109, -39),
    ]
    for i in range(len(rings) - 1):
        connect_rings(HULL, rings[i], rings[i + 1], M_DARK, cap_a=(i == 0), cap_b=False)
    # muzzle annulus at x=300, inner hole ~49
    outer = rings[-1]
    inner = ring(HULL, 299.2, 24.5, -98.5, -49.5, 0, 8, 7)
    n = 8
    for i in range(n):
        j = (i + 1) % n
        HULL.quad(outer[i], inner[i], inner[j], outer[j], M_DARK)
    # cavity
    throat = ring(HULL, 278, 21, -96, -52, 0, 7, 6)
    for i in range(n):
        j = (i + 1) % n
        HULL.quad(inner[i], inner[j], throat[j], throat[i], M_DARK)
    cap = HULL.v(279, 0, -74)
    for i in range(n):
        HULL.tri(cap, throat[i], throat[(i + 1) % n], M_DARK)

main_gun()

# rear main nozzle, exit center (-650,0,15), axis -X so housing extends toward +X (into the bridge)
nozzle(
    HULL, (-650, 0, 15), (-1, 0, 0),
    outer=(105, 92), inner=(72, 62), depth=70, wall=18,
    m_body=M_DARK, m_glow=M_CYAN,
)

# four landing jets, exit at given points, axis -Z (out), housing goes +Z into the hull
for pos in ((430, -345, -71), (-330, -345, -71), (430, 345, -71), (-330, 345, -71)):
    nozzle(
        HULL, pos, (0, 0, -1),
        outer=(43, 40), inner=(26, 24), depth=22, wall=8,
        m_body=M_DARK, m_glow=M_AMBER,
    )

# hardpoint bezels (frames). Caps are separate meshes.
def front_bezel(y):
    # 120 x 80 face at x just behind 650, on the nose cap. Recessed frame.
    # Build a shallow octagonal bezel whose outer face is at x=642 and recess goes to x=635.
    outer = ring(HULL, 646, 60, 20 - 40, 20 + 40, y, 16, 12)
    inner = ring(HULL, 645.2, 48, 20 - 30, 20 + 30, y, 12, 9)
    back = ring(HULL, 639, 48, 20 - 30, 20 + 30, y, 12, 9)
    n = 8
    for i in range(n):
        j = (i + 1) % n
        HULL.quad(outer[i], inner[i], inner[j], outer[j], M_DARK)
        HULL.quad(inner[i], inner[j], back[j], back[i], M_DARK)
    # close outer skirt back onto the nose a little
    skirt = ring(HULL, 638, 60, 20 - 40, 20 + 40, y, 16, 12)
    for i in range(n):
        j = (i + 1) % n
        HULL.quad(outer[i], skirt[i], skirt[j], outer[j], M_DARK)

front_bezel(-345)
front_bezel(345)

def top_base(y):
    # 200 x 100 pad centered at (180, y, 115), sitting on the hull roof
    box(HULL, 180 - 100, 180 + 100, y - 50, y + 50, 108, 116, M_DARK, chamfer=6)
    # two short rails
    box(HULL, 180 - 70, 180 + 70, y - 28, y - 18, 116, 124, M_DARK, chamfer=2)
    box(HULL, 180 - 70, 180 + 70, y + 18, y + 28, 116, 124, M_DARK, chamfer=2)
    # pins
    box(HULL, 175, 185, y - 8, y + 8, 116, 121, M_ORANGE, chamfer=1.5)

top_base(-345)
top_base(345)

def side_panel(y_sign):
    y = 450 * y_sign
    # 130 x 72 panel on the outer side, shallow recess 5
    # outer face near the hull side. Hull half-width at x=-220 is 105, center 345, so outer skin Y = 345+105=450 for +Y.
    y_face = y
    y_in = y - y_sign * 5
    x0, x1 = -220 - 65, -220 + 65
    z0, z1 = 20 - 36, 20 + 36
    # bezel
    box(HULL, x0 - 6, x0, y_in, y_face + y_sign * 2, z0 - 6, z1 + 6, M_DARK, chamfer=2)
    box(HULL, x1, x1 + 6, y_in, y_face + y_sign * 2, z0 - 6, z1 + 6, M_DARK, chamfer=2)
    box(HULL, x0, x1, y_in, y_face + y_sign * 2, z1, z1 + 6, M_DARK, chamfer=2)
    box(HULL, x0, x1, y_in, y_face + y_sign * 2, z0 - 6, z0, M_DARK, chamfer=2)
    # keyed slot
    box(HULL, -228, -212, y_in - y_sign * 1, y_in + y_sign * 3, 8, 32, M_ORANGE, chamfer=1)

side_panel(1)
side_panel(-1)

# small panel lines / vents on the side hulls for detail (not a box ship)
for cy in (-345, 345):
    box(HULL, -80, 40, cy - 8, cy + 8, -62, -56, M_DARK, chamfer=2)
    box(HULL, 300, 420, cy - 70, cy + 70, -20, -12, M_DARK, chamfer=3)

# Landing gear is a separate mesh (SM_Twinfork_Gear) driven at runtime.
# Keeping the struts in this hull made them impossible to retract.

HULL.save()

# Gameplay shell. Same silhouette as the authored hull, with loft walls flipped
# so the outside faces outward. The open interior hull asset stays separate.
SHELL = Mesh("SM_Twinfork_R1_Shell")
for name in HULL.mats:
    SHELL.mat(name)
M_SHELL = SHELL.mat("Hull")
M_SHELL_DARK = SHELL.mat("Dark")
M_SHELL_GLASS = SHELL.mat("Glass")
M_SHELL_AMBER = SHELL.mat("Amber")
M_SHELL_CYAN = SHELL.mat("Cyan")
M_SHELL_ORANGE = SHELL.mat("Orange")
M_SHELL_STRIPE = SHELL.mat("Stripe")

loft(SHELL, SIDE, -345, 16, 14, M_SHELL, SIDE_XS, flip_walls=True)
loft(SHELL, SIDE, 345, 16, 14, M_SHELL, SIDE_XS, flip_walls=True)
box(SHELL, -580, -320, -450, 450, -55, 122, M_SHELL, chamfer=0)
loft(SHELL, CABIN, 0, 18, 16, M_SHELL, CABIN_XS, flip_walls=True)


def shell_stripe(cy_sign):
    cy = 345 * cy_sign
    xs = [-520, -200, 80, 420, 600]
    for i in range(len(xs) - 1):
        s0 = sample_stations(SIDE, xs[i])
        s1 = sample_stations(SIDE, xs[i + 1])
        y_outer0 = cy + cy_sign * (s0[1] - 4)
        y_inner0 = cy + cy_sign * (s0[1] - 36)
        y_outer1 = cy + cy_sign * (s1[1] - 4)
        y_inner1 = cy + cy_sign * (s1[1] - 36)
        z0 = s0[3] - 4
        z1 = s1[3] - 4
        a = SHELL.v(xs[i], y_inner0, z0)
        b = SHELL.v(xs[i], y_outer0, z0 + 3)
        c = SHELL.v(xs[i + 1], y_outer1, z1 + 3)
        d = SHELL.v(xs[i + 1], y_inner1, z1)
        e = SHELL.v(xs[i], y_inner0, z0 - 18)
        f = SHELL.v(xs[i], y_outer0, z0 - 8)
        g = SHELL.v(xs[i + 1], y_outer1, z1 - 8)
        h = SHELL.v(xs[i + 1], y_inner1, z1 - 18)
        SHELL.quad(a, b, c, d, M_SHELL_STRIPE)
        SHELL.quad(e, h, g, f, M_SHELL_STRIPE)
        SHELL.quad(b, f, g, c, M_SHELL_STRIPE)
        SHELL.quad(a, d, h, e, M_SHELL_STRIPE)
        SHELL.quad(d, c, b, a, M_SHELL_STRIPE)
        SHELL.quad(f, g, h, e, M_SHELL_STRIPE)
        SHELL.quad(c, g, f, b, M_SHELL_STRIPE)
        SHELL.quad(e, h, d, a, M_SHELL_STRIPE)


shell_stripe(1)
shell_stripe(-1)
box(SHELL, -560, -340, -400, 400, 118, 126, M_SHELL_STRIPE, chamfer=0)
# Dark chevron on the tail bridge, matching the concept's rear joint.
box(SHELL, -500, -470, -240, 240, 124, 130, M_SHELL_DARK, chamfer=0)


def shell_windshield():
    outer = [
        SHELL.v(-70, -78, 172),
        SHELL.v(-70, 78, 172),
        SHELL.v(165, 58, 82),
        SHELL.v(165, -58, 82),
    ]
    glass = [
        SHELL.v(-58, -60, 160),
        SHELL.v(-58, 60, 160),
        SHELL.v(146, 40, 94),
        SHELL.v(146, -40, 94),
    ]
    for i in range(4):
        j = (i + 1) % 4
        SHELL.quad(outer[i], outer[j], glass[j], glass[i], M_SHELL_DARK)
        SHELL.quad(glass[i], glass[j], outer[j], outer[i], M_SHELL_DARK)
    SHELL.quad(glass[0], glass[1], glass[2], glass[3], M_SHELL_GLASS)
    SHELL.quad(glass[3], glass[2], glass[1], glass[0], M_SHELL_GLASS)
    for s in (-1, 1):
        pane = [
            SHELL.v(-40, s * 132, 154),
            SHELL.v(70, s * 122, 134),
            SHELL.v(70, s * 122, 74),
            SHELL.v(-40, s * 132, 82),
        ]
        SHELL.quad(pane[0], pane[1], pane[2], pane[3], M_SHELL_GLASS)
        SHELL.quad(pane[3], pane[2], pane[1], pane[0], M_SHELL_GLASS)


shell_windshield()

# Door plug on the inboard corridor so the cabin opening stays closed.
box(SHELL, -330, -215, -148, -122, -10, 112, M_SHELL, chamfer=0)
# Door frame and orange outline sit on the plug so the closed shell still reads as a hatch.
box(SHELL, -338, -322, -154, -116, -18, 120, M_SHELL_DARK, chamfer=2)
box(SHELL, -223, -207, -154, -116, -18, 120, M_SHELL_DARK, chamfer=2)
box(SHELL, -322, -223, -154, -116, 108, 122, M_SHELL_DARK, chamfer=2)
box(SHELL, -322, -223, -154, -116, -22, -8, M_SHELL_DARK, chamfer=2)
box(SHELL, -318, -228, -150, -146, -4, 104, M_SHELL_ORANGE, chamfer=1)
box(SHELL, -318, -228, -124, -120, -4, 104, M_SHELL_ORANGE, chamfer=1)
box(SHELL, -318, -228, -146, -124, 100, 108, M_SHELL_ORANGE, chamfer=1)
box(SHELL, -318, -228, -146, -124, -8, 0, M_SHELL_ORANGE, chamfer=1)

# Main gun housing under the cabin nose. Closed shroud, no open trough.
def shell_main_gun():
    def gun_ring(x, hw, zb, zt):
        return ring(SHELL, x, hw, zb, zt, 0, 14, 12)
    rings = [
        gun_ring(135, 40, -116, -32),
        gun_ring(210, 47, -120, -28),
        gun_ring(270, 42, -112, -36),
        gun_ring(300, 35, -109, -39),
    ]
    for i in range(len(rings) - 1):
        connect_rings(SHELL, rings[i], rings[i + 1], M_SHELL_DARK, cap_a=(i == 0), cap_b=False, flip_walls=True)
    outer = rings[-1]
    inner = ring(SHELL, 299.2, 24.5, -98.5, -49.5, 0, 8, 7)
    n = 8
    for i in range(n):
        j = (i + 1) % n
        SHELL.quad(outer[i], inner[i], inner[j], outer[j], M_SHELL_DARK)
        SHELL.quad(inner[j], inner[i], outer[i], outer[j], M_SHELL_DARK)
    throat = ring(SHELL, 286, 18, -94, -54, 0, 6, 5)
    for i in range(n):
        j = (i + 1) % n
        SHELL.quad(inner[i], inner[j], throat[j], throat[i], M_SHELL_DARK)
        SHELL.quad(throat[i], throat[j], inner[j], inner[i], M_SHELL_DARK)
    cap = SHELL.v(287, 0, -74)
    for i in range(n):
        SHELL.tri(cap, throat[i], throat[(i + 1) % n], M_SHELL_DARK)
        SHELL.tri(cap, throat[(i + 1) % n], throat[i], M_SHELL_DARK)


shell_main_gun()

# Rear nozzle and four landing bells. Double-sided so the closed shell still shows the bells.
nozzle(
    SHELL, (-650, 0, 15), (-1, 0, 0),
    outer=(105, 92), inner=(72, 62), depth=70, wall=18,
    m_body=M_SHELL_DARK, m_glow=M_SHELL_CYAN,
)
for pos in ((430, -345, -71), (-330, -345, -71), (430, 345, -71), (-330, 345, -71)):
    nozzle(
        SHELL, pos, (0, 0, -1),
        outer=(43, 40), inner=(26, 24), depth=22, wall=8,
        m_body=M_SHELL_DARK, m_glow=M_SHELL_AMBER,
    )

# Empty hardpoint frames. Caps stay separate and hidden; the bezels remain on the skin.
def shell_front_bezel(y):
    outer = ring(SHELL, 646, 60, 20 - 40, 20 + 40, y, 16, 12)
    inner = ring(SHELL, 645.2, 48, 20 - 30, 20 + 30, y, 12, 9)
    back = ring(SHELL, 639, 48, 20 - 30, 20 + 30, y, 12, 9)
    n = 8
    for i in range(n):
        j = (i + 1) % n
        SHELL.quad(outer[i], inner[i], inner[j], outer[j], M_SHELL_DARK)
        SHELL.quad(outer[j], inner[j], inner[i], outer[i], M_SHELL_DARK)
        SHELL.quad(inner[i], inner[j], back[j], back[i], M_SHELL_DARK)
        SHELL.quad(back[i], back[j], inner[j], inner[i], M_SHELL_DARK)
    skirt = ring(SHELL, 638, 60, 20 - 40, 20 + 40, y, 16, 12)
    for i in range(n):
        j = (i + 1) % n
        SHELL.quad(outer[i], skirt[i], skirt[j], outer[j], M_SHELL_DARK)
        SHELL.quad(outer[j], skirt[j], skirt[i], outer[i], M_SHELL_DARK)
    # Amber corner ticks on the nose frame.
    box(SHELL, 644, 648, y - 8, y + 8, 52, 60, M_SHELL_AMBER, chamfer=1)
    box(SHELL, 644, 648, y - 8, y + 8, -20, -12, M_SHELL_AMBER, chamfer=1)


shell_front_bezel(-345)
shell_front_bezel(345)


def shell_top_base(y):
    box(SHELL, 80, 280, y - 50, y + 50, 108, 116, M_SHELL_DARK, chamfer=6)
    box(SHELL, 110, 250, y - 28, y - 18, 116, 124, M_SHELL_DARK, chamfer=2)
    box(SHELL, 110, 250, y + 18, y + 28, 116, 124, M_SHELL_DARK, chamfer=2)
    box(SHELL, 175, 185, y - 8, y + 8, 116, 121, M_SHELL_ORANGE, chamfer=1.5)


shell_top_base(-345)
shell_top_base(345)


def shell_side_panel(y_sign):
    y = 450 * y_sign
    y_face = y
    y_in = y - y_sign * 5
    x0, x1 = -285, -155
    z0, z1 = -16, 56
    y0, y1 = min(y_in, y_face + y_sign * 2), max(y_in, y_face + y_sign * 2)
    box(SHELL, x0 - 6, x0, y0, y1, z0 - 6, z1 + 6, M_SHELL_DARK, chamfer=2)
    box(SHELL, x1, x1 + 6, y0, y1, z0 - 6, z1 + 6, M_SHELL_DARK, chamfer=2)
    box(SHELL, x0, x1, y0, y1, z1, z1 + 6, M_SHELL_DARK, chamfer=2)
    box(SHELL, x0, x1, y0, y1, z0 - 6, z0, M_SHELL_DARK, chamfer=2)
    # Orange outline just proud of the bezel.
    oy0 = y + y_sign * 3
    oy1 = y + y_sign * 7
    o0, o1 = min(oy0, oy1), max(oy0, oy1)
    box(SHELL, x0, x1, o0, o1, z1 - 3, z1 + 1, M_SHELL_ORANGE, chamfer=1)
    box(SHELL, x0, x1, o0, o1, z0 - 1, z0 + 3, M_SHELL_ORANGE, chamfer=1)
    box(SHELL, x0, x0 + 4, o0, o1, z0, z1, M_SHELL_ORANGE, chamfer=1)
    box(SHELL, x1 - 4, x1, o0, o1, z0, z1, M_SHELL_ORANGE, chamfer=1)


shell_side_panel(1)
shell_side_panel(-1)

# Recessed amber bars on the outer shoulder, proud of the skin by a few centimetres.
def shell_cove(x0, x1, cy):
    s0 = sample_stations(SIDE, (x0 + x1) * 0.5)
    z_top = s0[3] + 4
    box(SHELL, x0, x1, cy - 12, cy + 12, z_top, z_top + 4, M_SHELL_AMBER, chamfer=0)


for cy in (-345, 345):
    shell_cove(-300, -120, cy)
    shell_cove(60, 240, cy)

SHELL.save()

# ---------- caps (blanking plates) ----------
def make_front_cap(name, y):
    m = Mesh(name)
    d = m.mat("Dark")
    # plate sits in the recess, just proud of the back of the bezel, x=640..646
    box(m, 639.5, 645.5, y - 46, y + 46, 20 - 28, 20 + 28, d, chamfer=8)
    # inner inset
    box(m, 644.6, 646.2, y - 30, y + 30, 20 - 16, 20 + 16, m.mat("Hull"), chamfer=4)
    m.save()

make_front_cap("SM_Twinfork_Cap_Front", -345)

def make_top_cap(name):
    m = Mesh(name)
    d = m.mat("Dark")
    box(m, -90, 90, -40, 40, 0, 6, d, chamfer=4)
    box(m, -70, 70, -22, -16, 6, 10, d, chamfer=1)
    box(m, -70, 70, 16, 22, 6, 10, d, chamfer=1)
    m.save()

make_top_cap("SM_Twinfork_Cap_Top")

def make_side_cap(name):
    m = Mesh(name)
    d = m.mat("Dark")
    # local: plate thickness along +Y (outward). X width 118, Z height 64.
    box(m, -59, 59, 0, 4, -32, 32, d, chamfer=4)
    box(m, -40, 40, 3.5, 6, -18, 18, m.mat("Hull"), chamfer=2)
    m.save()

make_side_cap("SM_Twinfork_Cap_Side")

# ---------- door, step, gear ----------
DOOR = Mesh("SM_Twinfork_Door")
box(DOOR, -108, 0, -4, 4, -50, 58, DOOR.mat("Hull"), chamfer=4)
box(DOOR, -96, -8, 3.2, 7, -8, 28, DOOR.mat("Dark"), chamfer=2)
box(DOOR, -70, -40, 5, 8, 2, 16, DOOR.mat("Orange"), chamfer=1)
DOOR.save()

STEP = Mesh("SM_Twinfork_Step")
# Tread depth is 12cm so the deployed step leaves the 105cm corridor passable.
box(STEP, -50, 50, 0, 12, 0, 4, STEP.mat("Dark"), chamfer=2)
box(STEP, -36, 36, 2, 10, 4, 7, STEP.mat("Orange"), chamfer=1)
box(STEP, -46, -40, 1, 11, -6, 0, STEP.mat("Dark"), chamfer=1)
box(STEP, 40, 46, 1, 11, -6, 0, STEP.mat("Dark"), chamfer=1)
STEP.save()

GEAR = Mesh("SM_Twinfork_Gear")
# Hip origin. The authored hips sit at hull Z=-62, and the spec sole is Z=-190, so the foot is local Z=-128.
gd = GEAR.mat("Dark")
go = GEAR.mat("Orange")
# upper strut
box(GEAR, -10, 10, -10, 10, -48, 4, gd, chamfer=3)
# knee block
box(GEAR, -14, 14, -14, 14, -62, -42, go, chamfer=3)
# lower strut
box(GEAR, -8, 8, -8, 8, -118, -58, gd, chamfer=2)
# foot pad
box(GEAR, -26, 26, -18, 18, -128, -112, gd, chamfer=3)
box(GEAR, -16, 16, -8, 8, -128, -122, go, chamfer=2)
GEAR.save()

# ---------- weapons (module origin at the hardpoint, +X forward) ----------
PULSE = Mesh("SM_Twinfork_PulseCannon")
# short cartridge, protrudes only ~20 forward of the mount (mount is the nose face)
pd = PULSE.mat("Dark")
pc = PULSE.mat("Cyan")
ph = PULSE.mat("Hull")
box(PULSE, -8, 20, -34, 34, -22, 22, pd, chamfer=6)
box(PULSE, 16, 24, -16, 16, -8, 8, pc, chamfer=2)
# side cheeks
box(PULSE, -4, 12, -40, -30, -10, 10, ph, chamfer=2)
box(PULSE, -4, 12, 30, 40, -10, 10, ph, chamfer=2)
PULSE.save()

RAIL = Mesh("SM_Twinfork_Railgun")
# 470 x 68 x 82, barrel +X, occupies roughly local X=-90..380 so world X=90..560 when mounted at 180
rd = RAIL.mat("Dark")
rc = RAIL.mat("Cyan")
rh = RAIL.mat("Hull")
box(RAIL, -90, 250, -34, 34, 0, 56, rh, chamfer=8)
box(RAIL, 40, 380, -16, 16, 14, 46, rd, chamfer=5)
# muzzle
box(RAIL, 360, 392, -12, 12, 18, 42, rc, chamfer=3)
# breech block behind
box(RAIL, -110, -70, -28, 28, 6, 50, rd, chamfer=4)
# rail shoes that sit in the mount rails
box(RAIL, -60, 60, -24, -16, -4, 2, rd, chamfer=1)
box(RAIL, -60, 60, 16, 24, -4, 2, rd, chamfer=1)
RAIL.save()

POD = Mesh("SM_Twinfork_MissilePod")
# 260 x 95 x 100. For the RIGHT mount the pod extends +Y. Origin at hardpoint.
# Spec: outward 95, cells 2 columns x 3 rows, firing end at local X = +150 (world -70 when mount x=-220).
md = POD.mat("Dark")
mh = POD.mat("Hull")
mo = POD.mat("Orange")
mc = POD.mat("Cyan")
# local +Y is outward. Body X -110..150, Y 8..95, Z -50..50.
box(POD, -110, 150, 8, 95, -50, 50, mh, chamfer=8)
box(POD, 142, 156, 18, 86, -40, 40, md, chamfer=4)
for yy in (32, 70):
    for zz in (-26, 0, 26):
        box(POD, 150, 162, yy - 8, yy + 8, zz - 8, zz + 8, md, chamfer=2)
        box(POD, 158, 164, yy - 4, yy + 4, zz - 4, zz + 4, mc, chamfer=1)
box(POD, -20, 40, 86, 96, -6, 8, mo, chamfer=2)
POD.save()

# Collision is a chain of plain boxes following the side-hull stations.
# A single chamfered loft becomes one convex hull that fills the outer bevel
# and closes the door lane (left skin was reaching Y=-450). Boxes stay inside
# the structural section: half-width 105 at Y center +/-345, so the inboard
# face is Y=+/-240 and the outboard face is Y=+/-450.
def collision_side(name, cy):
    m = Mesh(name)
    k = m.mat("Hull")
    sign = -1.0 if cy < 0 else 1.0
    # (x0, x1, half_width, z0, z1) sampled inside the side-hull stations.
    spans = (
        (-640, -500, 90, -52, 100),
        (-500, -200, 105, -65, 110),
        (-200, 200, 105, -65, 110),
        (200, 450, 105, -65, 110),
        (450, 650, 88, -48, 90),
    )
    for x0, x1, hw, z0, z1 in spans:
        y_in = cy - sign * hw
        y_out = cy + sign * hw
        box(m, x0, x1, min(y_in, y_out), max(y_in, y_out), z0, z1, k, chamfer=0.0)
    m.save()

collision_side("SM_Twinfork_Col_Left", -345)
collision_side("SM_Twinfork_Col_Right", 345)

COL_C = Mesh("SM_Twinfork_Col_Cabin")
loft(COL_C, CABIN, 0, 12, 10, COL_C.mat("Hull"), [-390, -80, 80, 270])
COL_C.save()

COL_B = Mesh("SM_Twinfork_Col_Bridge")
# Stop at the aft door jamb (X=-348) so the corridor opening stays walkable.
box(COL_B, -620, -348, -450, 450, -55, 122, COL_B.mat("Hull"), chamfer=0.0)
COL_B.save()

print("done")
