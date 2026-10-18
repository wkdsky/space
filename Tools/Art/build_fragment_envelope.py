"""Blender offline: --background --python this.py -- source.json output.json.

Input is the UE mesh export in planet-local, full-size centimetres. Output
contains one convex hull per material part and a fixed-centre enclosing ellipse.
"""
import sys
import json
from pathlib import Path
import bmesh
from mathutils import Vector
import numpy as np

source, target = sys.argv[sys.argv.index('--') + 1:]
parts = json.loads(Path(source).read_text(encoding='utf-8'))['parts']
hulls = []
points = []
for key, values in sorted(parts.items(), key=lambda item: int(item[0])):
    vertices = np.unique(np.round(np.array(values), 5), axis=0)
    points.extend(vertices.tolist())
    bm = bmesh.new()
    for p in vertices:
        bm.verts.new(Vector(p))
    bmesh.ops.convex_hull(bm, input=list(bm.verts), use_existing_faces=False)
    bmesh.ops.triangulate(bm, faces=list(bm.faces))
    # Drop vertices unused by the hull so distance queries and assets stay small.
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.verts.index_update()
    bm.normal_update()
    verts = [list(v.co) for v in bm.verts]
    centre = Vector(np.mean(verts, axis=0))
    indices = []
    for f in bm.faces:
        if f.normal.dot(f.calc_center_median() - centre) < 0:
            f.normal_flip()
        indices.extend(v.index for v in f.verts)
    hulls.append(dict(part=int(key), vertices=verts, triangles=indices,
                      bounds_min=np.min(verts, axis=0).tolist(), bounds_max=np.max(verts, axis=0).tolist()))
    bm.free()

# Minimum-volume axis-aligned ellipsoid with the explosion centre held fixed.
# The dual update is scale independent; the final expansion guarantees containment.
x = np.unique(np.array(points), axis=0) / 100000.0
w = np.full(len(x), 1.0 / len(x))
for _ in range(12000):
    diag = np.sum(w[:, None] * x * x, axis=0)
    leverage = np.sum(x * x / diag, axis=1)
    i = int(np.argmax(leverage))
    m = float(leverage[i])
    if m < 3.001:
        break
    step = (m - 3) / (3 * (m - 1))
    w *= 1 - step
    w[i] += step
radii = np.sqrt(3 * np.sum(w[:, None] * x * x, axis=0))
radii *= np.sqrt(np.max(np.sum((x / radii) ** 2, axis=1))) * 1.00001
report = dict(fragments=hulls, region_radii=(radii * 100000).tolist(),
              source_vertices=len(x), max_region_normalized_radius=float(np.max(np.linalg.norm(x/radii, axis=1))))
Path(target).write_text(json.dumps(report), encoding='utf-8')
print('FRAGMENT_ENVELOPE', len(hulls), sum(len(h['triangles'])//3 for h in hulls), report['region_radii'], report['max_region_normalized_radius'])
