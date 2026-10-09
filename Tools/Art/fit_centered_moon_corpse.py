"""Fit corpse support to the native crater-centre facets; run with Blender's numpy."""
import json
from pathlib import Path
import numpy as np

root = Path(__file__).resolve().parents[2] / "SourceArt/MoonAntNests"
source = json.loads((root / "centered_corpse_geometry.json").read_text())
points = np.array(source["vertices"])
faces = np.array(source["triangles"])
groups = [points[:, 1] < -55, (points[:, 1] >= -55) & (points[:, 1] < -15),
          (points[:, 1] >= -15) & (points[:, 1] < 30),
          (points[:, 1] > 75) & (points[:, 0] < 0), (points[:, 1] > 75) & (points[:, 0] >= 0)]


def height(x, y):
    result = np.full(len(x), -np.inf)
    for a, b, c in faces:
        det = (b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        u = ((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/det
        v = ((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/det
        mask = (u >= -1e-7) & (v >= -1e-7) & (u+v <= 1.0000001)
        result[mask] = np.maximum(result[mask], (u*a[2]+v*b[2]+(1-u-v)*c[2])[mask])
    return result


# The six basin facets have a shallow crease beneath the body's centre. Conform the
# static lying pose continuously to those facets; the rigged player mesh is untouched.
import bpy
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
terrain = height(points[:, 0], points[:, 1])
assert np.all(np.isfinite(terrain))
minimum = float(np.min(points[:, 2]))
shift = terrain + .3 - minimum
vertices = points.copy()
vertices[:, 2] += shift
vertices[:, 1] *= -1
vertices /= 100
name = 'SM_SkeletonAstronaut_Corpse_CraterCenter'
mesh = bpy.data.meshes.new(name)
mesh.from_pydata(vertices.tolist(), [], [tuple(reversed(face['v'])) for face in source['faces']])
mesh.update()
obj = bpy.data.objects.new(name, mesh)
bpy.context.collection.objects.link(obj)
for slot in source['material_slots']:
    mesh.materials.append(bpy.data.materials.new(slot))
for polygon, face in zip(mesh.polygons, source['faces']):
    polygon.material_index = face['slot']
    polygon.use_smooth = False
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.fbx(filepath=str(root/(name+'.fbx')), use_selection=True,
    object_types={'MESH'}, bake_anim=False, axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE')
bpy.ops.wm.save_as_mainfile(filepath=str(root/'CenteredMoonCorpse.blend'))
contacts = [float(np.min(points[mask, 2])-minimum+.3) for mask in groups]
report = {'mesh': name, 'minimum_support_clearance_cm': .3,
    'region_contacts_cm': dict(zip(['head','back','pelvis','left_boot','right_boot'], contacts)),
    'vertical_pose_adjustment_cm': [float(np.min(shift)), float(np.max(shift))],
    'actor_transform': source['actor_transform']}
(root/'centered_corpse_fit.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('CENTERED_CORPSE_FIT', report)
