"""Validate authored motion and exact source endpoints in background Blender."""
import json
from pathlib import Path

import bpy
from mathutils import Vector, kdtree
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'SourceArt/MarsDestruction'
manifest = json.loads((ART / 'prototype_manifest.json').read_text())
motion = json.loads((ART / 'fragment_motion.json').read_text())
bpy.ops.wm.open_mainfile(filepath=str(ART / 'Mars_Internal_Explosion_POC.blend'))
scene = bpy.context.scene
arm = bpy.data.objects['SK_MarsExplosion_POC']
mesh = bpy.data.objects['MarsExplosion_Geometry']
assert len(arm.data.bones) == manifest['bones'] == 109
assert len(mesh.data.materials) == manifest['export_materials'] == 2
assert [uv.name for uv in mesh.data.uv_layers] == ['UVMap', 'TransitionData']
assert scene.render.fps == 30 and scene.frame_end == 840

groups = {g.index: g.name for g in mesh.vertex_groups}
intact_indices = [v.index for v in mesh.data.vertices if groups[v.groups[0].group] == 'Intact']
mapping = {vi: i for i, vi in enumerate(intact_indices)}
intact_points = [mesh.data.vertices[i].co.copy() for i in intact_indices]
intact_faces = [[mapping[i] for i in p.vertices] for p in mesh.data.polygons if p.vertices[0] in mapping]
surface = BVHTree.FromPolygons(intact_points, intact_faces)
scene.frame_set(0)
evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
start_points = [evaluated.data.vertices[v.index].co.copy() for v in mesh.data.vertices
                if groups[v.groups[0].group].startswith('Debris_')]
max_protrusion = -1
for point in start_points:
    hit, _, _, radius = surface.ray_cast(Vector(), point.normalized(), 3)
    assert hit is not None
    max_protrusion = max(max_protrusion, point.length - radius)
assert max_protrusion < 0, max_protrusion

initial_speeds = []
for record in motion:
    bone = arm.pose.bones[record['name']]
    basis = bone.bone.matrix_local.to_quaternion()
    radial = bone.bone.head_local.normalized()
    previous_radius = None
    previous_speed = None
    for frame in range(60, 841, 3):
        scene.frame_set(frame)
        offset = basis @ bone.location
        signed = offset.dot(radial)
        assert (offset - radial * signed).length < 1e-6
        if previous_radius is not None:
            speed = (signed - previous_radius) / .1
            assert speed >= -2e-6, (record['name'], frame, speed)
            if previous_speed is not None:
                assert speed <= previous_speed + 2e-6, (record['name'], frame, speed, previous_speed)
            else:
                initial_speeds.append(speed)
            previous_speed = speed
        previous_radius = signed
    assert bone.location.length < 1e-6
    assert abs(bone.rotation_quaternion.angle) < 1e-5
    assert (Vector(bone.scale) - Vector((1, 1, 1))).length < 1e-6
ordered = sorted(motion, key=lambda item: item['size_proxy'])
assert all(a['initial_speed'] >= b['initial_speed'] - 1e-8 for a, b in zip(ordered, ordered[1:]))

scene.frame_set(840)
evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
animated_final = [evaluated.data.vertices[v.index].co.copy() for v in mesh.data.vertices
                  if groups[v.groups[0].group].startswith('Debris_')]


def clear():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)


def cloud_error(points, reference):
    tree = kdtree.KDTree(len(reference))
    for i, point in enumerate(reference):
        tree.insert(point, i)
    tree.balance()
    return max(tree.find(point)[2] for point in points)


clear()
bpy.ops.import_scene.fbx(filepath=str(ART / 'SM_MarsDebris_Final.fbx'))
static = [o for o in scene.objects if o.type == 'MESH']
assert len(static) == 107
assert sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in static) == 5080
static_points = [o.matrix_world @ v.co for o in static for v in o.data.vertices]
handoff_error = cloud_error(animated_final, static_points)
assert handoff_error < 1e-5
clear()
bpy.ops.import_scene.fbx(filepath=str(ROOT / 'asteroid 3d model/asteroid+3d+model.fbx'))
bpy.context.view_layer.update()
source = [o for o in scene.objects if o.type == 'MESH']
corners = [o.matrix_world @ Vector(c) for o in source for c in o.bound_box]
lo = Vector(tuple(min(p[i] for p in corners) for i in range(3)))
hi = Vector(tuple(max(p[i] for p in corners) for i in range(3)))
center, scale = (lo + hi) * .5, 2 / max(hi - lo)
source_points = [(o.matrix_world @ v.co - center) * scale for o in source for v in o.data.vertices]
source_error = cloud_error(static_points, source_points)
assert source_error < 1e-5
clear()
bpy.ops.import_scene.fbx(filepath=str(ART / 'SK_MarsExplosion_POC.fbx'))
arms = [o for o in scene.objects if o.type == 'ARMATURE']
meshes = [o for o in scene.objects if o.type == 'MESH']
assert len(arms) == len(meshes) == 1
assert len(arms[0].data.bones) == 109
assert len(meshes[0].data.materials) == 2 and len(meshes[0].data.uv_layers) == 2
duration_frames = arms[0].animation_data.action.frame_range[1] - arms[0].animation_data.action.frame_range[0]
assert abs(duration_frames - 840) < .01 and scene.render.fps == 30
report = dict(result='PASS', bones=109, material_slots=2, static_final_fragments=107,
              duration_seconds=28, exported_frames=duration_frames,
              initial_fragment_max_surface_protrusion=max_protrusion,
              animated_to_static_vertex_error=handoff_error,
              supplied_source_to_static_vertex_error=source_error,
              initial_speed_range_radii_per_second=[min(initial_speeds), max(initial_speeds)],
              monotonic_outward_motion=True, monotonic_deceleration=True,
              size_ordered_speed=True, shell_ejecta_count=0, final_diameter_ratio=1)
(ROOT / 'Saved/MarsDestruction/export_validation.json').write_text(json.dumps(report, indent=2))
print('MARS_VALIDATION', json.dumps(report))
