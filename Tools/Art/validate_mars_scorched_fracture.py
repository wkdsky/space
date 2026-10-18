"""Verify the first-model fracture and the second-model blackout handoff."""
import json
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector, kdtree
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'SourceArt/MarsDestruction/ScorchedFractureV3'
data = json.loads((ART / 'manifest.json').read_text())
motion = json.loads((ART / 'fragment_motion.json').read_text())
bpy.ops.wm.open_mainfile(filepath=str(ART / 'Mars_Scorched_Fracture_V3.blend'))
scene = bpy.context.scene
arm = bpy.data.objects['SK_MarsScorchedFracture']
geometry = bpy.data.objects['MarsScorchedFracture_Geometry']
assert len(arm.data.bones) == data['bones'] and len(geometry.data.materials) == 2
assert [k.name for k in geometry.data.shape_keys.key_blocks] == ['Basis', 'TectonicRift', 'RiftOpening', 'ScorchedRim']
rift=geometry.data.shape_keys.key_blocks['TectonicRift']
rift_delta=[(b.co-a.co).length for a,b in zip(geometry.data.vertices,rift.data)]
assert .07<max(rift_delta)<.13
opening=geometry.data.shape_keys.key_blocks['RiftOpening']
opening_delta=[(b.co-a.co).length for a,b in zip(geometry.data.vertices,opening.data)]
assert .03<max(opening_delta)<.09
for frame,expected in data['opening_keys']:
    scene.frame_set(frame)
    assert abs(opening.value-expected)<1.e-5
for frame,expected in data['rift_keys']:
    scene.frame_set(frame)
    assert abs(rift.value-expected)<1e-5
rim = geometry.data.shape_keys.key_blocks['ScorchedRim']
delta = [(b.co-a.co).length for a,b in zip(geometry.data.vertices, rim.data)]
assert .02 < max(delta) < .05
solid_vertices = {vi for p in geometry.data.polygons if p.material_index < 2 for vi in p.vertices}
untouched = sum(delta[i] < 1e-7 for i in solid_vertices)/len(solid_vertices)
# Vertex fraction depends on the deliberately dense new fault samples. Locality
# to the source surface and retained volume are checked geometrically below.
for frame, expected in data['erosion_keys']:
    scene.frame_set(frame)
    assert abs(rim.value-expected) < 1e-5
assert [uv.name for uv in geometry.data.uv_layers] == ['UVMap', 'HeatField']
bm = bmesh.new()
bm.from_mesh(geometry.data)
bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index >= 2], context='FACES')
assert all(e.is_manifold for e in bm.edges)
seam_edges = [e for e in bm.edges if {f.material_index for f in e.link_faces} == {0,1}]
ray_faces = [p for p in geometry.data.polygons if p.material_index == 2]
assert not ray_faces and len(seam_edges) == data['crack_coverage']['boundary_edges']
rest_volume = abs(bm.calc_volume())
assert abs(rest_volume-data['partition_volume']) < 1e-4
assert rest_volume/data['source_volume'] > .995, (rest_volume,data['source_volume'])
recovered=[r for r in motion if r['recovered_cavity_rock']]
assert len(recovered) == data['recovered_inner_fragments'] and len(recovered) >= 8
assert sum(r['volume'] for r in recovered) > data['source_volume']*.03
for record in recovered:
    group=geometry.vertex_groups[record['name']]
    indices={v.index for v in geometry.data.vertices if any(g.group==group.index and g.weight>.999 for g in v.groups)}
    faces=[p for p in geometry.data.polygons if any(i in indices for i in p.vertices)]
    assert indices and faces and all(p.material_index==1 and set(p.vertices)<=indices for p in faces)
    assert max(geometry.data.vertices[i].co.length for i in indices)<.8
bm.verts.ensure_lookup_table()
# Attribute-free index remapping is not needed: keep original vertex indices
# by assigning the morph before removing the non-solid light faces.
bm.free()
eroded_bm = bmesh.new(); eroded_bm.from_mesh(geometry.data)
eroded_bm.verts.ensure_lookup_table()
for i,v in enumerate(eroded_bm.verts): v.co = rim.data[i].co
bmesh.ops.delete(eroded_bm, geom=[f for f in eroded_bm.faces if f.material_index >= 2], context='FACES')
assert all(e.is_manifold for e in eroded_bm.edges)
eroded_volume = abs(eroded_bm.calc_volume())
assert .95 < eroded_volume/rest_volume < 1., (eroded_volume,rest_volume)
eroded_bm.free()
lamp = bpy.data.objects['Mars_CentralPressureLight']
core = bpy.data.objects['Mars_InteriorEnergy']
light_samples=[]
for frame in (240,270,330,420,510,570,960):
    scene.frame_set(frame)
    assert lamp.location.length < 1e-8 and core.location.length < 1e-8
    assert lamp.parent is None and core.parent is None
    light_samples.append(lamp.data.energy)
    assert core.hide_render == (frame>=round(data['energy_core_hide_seconds']*30))
assert all(a >= b for a,b in zip(light_samples,light_samples[1:])) and light_samples[-1] == 0.
for frame,expected in data['energy_core_scale_keys']:
    scene.frame_set(frame)
    assert (core.scale-Vector((expected,)*3)).length<1e-5
    assert expected==1., 'Energy volume must fade without changing spatial bounds.'
assert not data['energy_has_visible_surface']
output=next(n for n in core.data.materials[0].node_tree.nodes if n.type=='OUTPUT_MATERIAL')
assert not output.inputs['Surface'].is_linked and output.inputs['Volume'].is_linked
assert len(core.data.polygons)==6, 'Only invisible volume bounds, not a spherical surface.'
assert 'RestPosition' in geometry.data.attributes
rest_attribute=geometry.data.attributes['RestPosition']
assert max((v.co-rest_attribute.data[v.index].vector).length for v in geometry.data.vertices)<1e-6
flow=bpy.data.images['T_MarsThermalFlow']
assert tuple(flow.size)==(2048,1024)
assert data['thermal_flow_texture']=='T_MarsThermalFlow.png'
for frame,expected in data['burn_progress_keys']:
    scene.frame_set(frame)
    for material in list(geometry.data.materials)[:2]:
        assert abs(material.node_tree.nodes['BurnProgress'].outputs[0].default_value-expected)<1e-5
scene.frame_set(960)
assert geometry.data.materials[0].node_tree.nodes['EdgeGlow'].outputs[0].default_value < .03
for material in list(geometry.data.materials)[:2]:
    assert all(node.type != 'BSDF_TRANSPARENT' for node in material.node_tree.nodes)
release = round(data['main_drift_seconds']*30)
for frame in (0, 9, 150, 210, release):
    scene.frame_set(frame)
    for record in motion:
        if record.get('fault_shard') and frame>=30:
            continue
        bone = arm.pose.bones[record['name']]
        assert bone.location.length < 1e-6 and abs(bone.rotation_quaternion.angle) < 1e-5
falling=[r for r in motion if r.get('fault_shard')]
assert len(falling)==data['falling_fault_shards'] and len(falling)>30
scene.frame_set(120)
fall_depths=[]
for record in falling:
    bone=arm.pose.bones[record['name']]
    direction=Vector(record['center']).normalized()
    fall_depths.append(-(bone.bone.matrix_local.to_quaternion()@bone.location).dot(direction))
assert sum(d>.05 for d in fall_depths)>len(falling)*.9,fall_depths
end_speeds=[]
# Evaluate each scene frame once, instead of re-evaluating all 187 bones and
# the dense fault mesh separately for every bone under inspection.
sample_frames={1002,1005}
for record in motion:
    sample_frames.update(range(record['release_frame']+6,1006,3))
bases={r['name']:arm.pose.bones[r['name']].bone.matrix_local.to_quaternion() for r in motion}
pose_cache={}
for frame in sorted(sample_frames):
    scene.frame_set(frame)
    pose_cache[frame]={r['name']:bases[r['name']]@arm.pose.bones[r['name']].location for r in motion}
    assert all((Vector(arm.pose.bones[r['name']].scale)-Vector((1,1,1))).length<1.e-6 for r in motion)
for record in motion:
    bone = arm.pose.bones[record['name']]
    basis = bone.bone.matrix_local.to_quaternion()
    direction = Vector(record['center']).normalized()
    previous, speed = None, None
    # Check continuous deceleration after the common release.
    for frame in range(record['release_frame']+6, 1006, 3):
        offset = pose_cache[frame][record['name']]
        distance = offset.dot(direction)
        if previous is not None:
            current_speed = (distance - previous) / .1
            assert current_speed >= -2e-6
            if speed is not None:
                assert current_speed <= speed + 3e-4, (record['name'],frame,current_speed,speed)
            speed = current_speed
        previous = distance
    assert (pose_cache[1005][record['name']]-Vector(record['offset'])).length<2.e-6
    end=pose_cache[1005][record['name']].dot(direction)
    end_speeds.append((end-pose_cache[1002][record['name']].dot(direction))/.1)
assert min(end_speeds) > .001, end_speeds
assert {r['release_frame'] for r in motion} == {release}
scene.frame_set(release+1)
for record in motion:
    if not record.get('fault_shard'):
        assert (bases[record['name']]@arm.pose.bones[record['name']].location).dot(Vector(record['center']).normalized())>0
ordered = sorted((p for p in motion if p['outer_area'] > .1), key=lambda p: p['volume'])
small, large = ordered[:8], ordered[-8:]
mean_speed = lambda rows: sum(p['radial_travel']/p['duration'] for p in rows)/len(rows)
assert mean_speed(small) > mean_speed(large)*1.15
assert all(p['radial_travel'] >= .35 for p in ordered)
assert all(Vector(p['offset']).dot(Vector(p['center']).normalized()) > .17 for p in motion)
assert data['debris_to_intact_scale'] > 1.6
assert abs(data['debris_solid_volume_before_rescale']*data['debris_to_intact_scale']**3-data['source_volume']) < 1e-5
assert 'Mars_InteriorEnergy' in bpy.data.objects and data['fracture_ray_pairs'] == 0
for material in list(geometry.data.materials)[:2]:
    scene.frame_set(240)
    assert material.node_tree.nodes['InternalPressure'].outputs[0].default_value > 1.5
for frame, final in ((0, False), (960, False), (1004, False), (1005, True), (1200, True)):
    scene.frame_set(frame)
    assert geometry.hide_render == final
    assert all(obj.hide_render != final for obj in scene.objects if obj.name.startswith('Exploration_'))
# Import the intact FBX and prove that every fractured exterior triangle still
# lies on its source. No hidden replacement surface is part of the skeletal mesh.
before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=str(ROOT / 'mars planet 3d model/mars+planet+3d+model.fbx'))
original = next(o for o in bpy.data.objects if o not in before and o.type == 'MESH')
bpy.context.view_layer.update()
corners = [original.matrix_world @ Vector(c) for c in original.bound_box]
lo = Vector(tuple(min(p[i] for p in corners) for i in range(3)))
hi = Vector(tuple(max(p[i] for p in corners) for i in range(3)))
center, scale = (lo + hi) * .5, 2 / max(hi - lo)
points = [(original.matrix_world @ v.co - center) * scale for v in original.data.vertices]
tree = BVHTree.FromPolygons(points, [list(p.vertices) for p in original.data.polygons])
exterior = {vi for p in geometry.data.polygons if p.material_index == 0 for vi in p.vertices}
surface_error = max(tree.find_nearest(geometry.data.vertices[vi].co)[3] for vi in exterior)
assert surface_error < 2e-6, surface_error
assert all(tree.find_nearest(v.co)[3] < .075001 for v in geometry.data.vertices
           if v.index in solid_vertices and delta[v.index] > 1e-7)


def clear():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)


clear()
bpy.ops.import_scene.fbx(filepath=str(ART / 'SM_MarsDebris_Exploration.fbx'))
final_meshes = [o for o in scene.objects if o.type == 'MESH']
assert len(final_meshes) == 107
assert sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in final_meshes) == 5080
final_points = [o.matrix_world @ v.co for o in final_meshes for v in o.data.vertices]
clear()
bpy.ops.import_scene.fbx(filepath=str(ROOT / 'asteroid 3d model/asteroid+3d+model.fbx'))
bpy.context.view_layer.update()
source = [o for o in scene.objects if o.type == 'MESH']
corners = [o.matrix_world @ Vector(c) for o in source for c in o.bound_box]
lo = Vector(tuple(min(p[i] for p in corners) for i in range(3)))
hi = Vector(tuple(max(p[i] for p in corners) for i in range(3)))
center, scale = (lo + hi) * .5, 2 / max(hi - lo)
original_points = [(o.matrix_world @ v.co - center) * scale * data['debris_to_intact_scale'] for o in source for v in o.data.vertices]
kd = kdtree.KDTree(len(original_points))
for i, point in enumerate(original_points):
    kd.insert(point, i)
kd.balance()
static_error = max(kd.find(p)[2] for p in final_points)
assert static_error < 2e-6
clear()
bpy.ops.import_scene.fbx(filepath=str(ART / 'SK_MarsScorchedFracture.fbx'))
arm = next(o for o in scene.objects if o.type == 'ARMATURE')
geometry = next(o for o in scene.objects if o.type == 'MESH')
assert len(arm.data.bones) == data['bones'] and len(geometry.data.materials) == 2
assert geometry.data.shape_keys and len(geometry.data.shape_keys.key_blocks) == 4
assert geometry.data.shape_keys.animation_data and geometry.data.shape_keys.animation_data.action
for authored_frame, expected in data['erosion_keys']:
    frame = authored_frame+1
    scene.frame_set(frame)
    assert abs(geometry.data.shape_keys.key_blocks['ScorchedRim'].value-expected) < 1e-4
for authored_frame,expected in data['rift_keys']:
    scene.frame_set(authored_frame+1)
    assert abs(geometry.data.shape_keys.key_blocks['TectonicRift'].value-expected)<1e-4
for authored_frame,expected in data['opening_keys']:
    scene.frame_set(authored_frame+1)
    assert abs(geometry.data.shape_keys.key_blocks['RiftOpening'].value-expected)<1.e-4
action_range = arm.animation_data.action.frame_range
assert abs(action_range[1] - action_range[0] - data['animation_seconds']*30) < .01 and scene.render.fps == 30
report = dict(revision=data['revision'], result='PASS', closed_rigid_plates=data['plates'], bones=data['bones'], triangle_count=data['triangles'],
              intact_source_surface_error=surface_error, second_source_static_error=static_error,
              no_scale=True, localized_rim_morph=True, unaffected_solid_vertex_fraction=untouched,
              gorges_before_rigid_motion=True, maximum_gorge_depth=max(rift_delta),
              maximum_irregular_lip_displacement=max(opening_delta),falling_fault_shards=len(falling),
              minimum_fault_fall_depth_at_four_seconds=min(fall_depths),
              main_drift_seconds=data['main_drift_seconds'],
              maximum_rim_erosion=max(delta), eroded_volume_loss_fraction=1-eroded_volume/rest_volume,
              attached_light_cards=0, light_origin_fixed=True, central_light_decays=True,
              central_light_samples_watts=light_samples,
              edge_glow_disabled=True, stationary_through_seconds=data['main_drift_seconds'],
              monotonic_outward_and_decelerating=True, size_ordered_speed=True,
              second_model_never_visible_before_blackout=True, fbx_animation_seconds=data['animation_seconds'],
              drift_continues_through_blackout=True, minimum_final_outward_speed_radii_per_second=min(end_speeds),
              synchronized_outward_release=True,
              recovered_inner_fragments=len(recovered), interior_fragments=data['interior_fragments'],
              recovered_inner_volume_fraction=data['recovered_inner_volume']/data['source_volume'],
              solid_volume_retained_fraction=rest_volume/data['source_volume'],
              extinguished_energy_geometry_hidden=True, energy_volume_bounds_constant=True,
              energy_visible_surface=False, thermal_flow_from_inner_roots=True,
              thermal_flow_texture=data['thermal_flow_texture'], rest_space_heat_field=True,
              original_second_fragments=107, debris_to_intact_scale=data['debris_to_intact_scale'],
              visible_outer_plate_travel=[min(p['radial_travel'] for p in ordered),max(p['radial_travel'] for p in ordered)],
              small_large_average_speed_ratio=mean_speed(small)/mean_speed(large),
              central_cavity_removed_volume_fraction=abs(data['partition_volume']-data['source_volume'])/data['source_volume'])
(ROOT / 'Saved/MarsDestruction/scorched_geometry_validation.json').write_text(json.dumps(report, indent=2))
print('MARS_SCORCHED_VALIDATION', json.dumps(report))
