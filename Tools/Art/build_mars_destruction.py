"""Build an authored Mars explosion proof of concept and a rigid-skinned FBX.

blender --background --factory-startup --python Tools/Art/build_mars_destruction.py
The optional --render argument also renders a 10 fps, 28-second drift preview.
The final 107 fragments retain their supplied geometry and relative arrangement.
"""
import json
import math
import random
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'SourceArt/MarsDestruction'
OUT.mkdir(parents=True, exist_ok=True)
FPS, END, START, HANDOFF_END, RENDER_STEP = 30, 840, 60, 360, 3
RNG = random.Random(481516)


def box(objects):
    points = [o.matrix_world @ Vector(v) for o in objects for v in o.bound_box]
    lo = Vector(tuple(min(p[i] for p in points) for i in range(3)))
    hi = Vector(tuple(max(p[i] for p in points) for i in range(3)))
    return (lo + hi) * .5, max(hi - lo)


def source(relative):
    old = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(ROOT / relative))
    bpy.context.view_layer.update()
    meshes = sorted((o for o in bpy.data.objects if o not in old and o.type == 'MESH'), key=lambda o: o.name)
    for obj in meshes:
        assert len(obj.data.uv_layers) == 1
        obj.data.uv_layers.active.name = 'UVMap'
    return meshes


def textured(name, image):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value = .82
    tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
    tex.image = image
    mat.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
    return mat


def debris_atlas(objects):
    """Pack padded source texture tiles; remap UVs without baking geometry."""
    entries = []
    for mat in {s.material for o in objects for s in o.material_slots if s.material}:
        image = next(n.image for n in mat.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image)
        entries.append((mat, image, min(512, image.size[0])))
    entries.sort(key=lambda e: (-e[2], e[0].name))
    size, gutter = 4096, 4
    pixels = np.zeros((size, size, 4), dtype=np.float32)
    pixels[:, :, 3] = 1
    x = y = height = 0
    tiles = {}
    for mat, image, width in entries:
        extent = width + gutter * 2
        if x + extent > size:
            x, y, height = 0, y + height, 0
        assert y + extent <= size, 'Atlas overflow'
        if image.size[0] != width:
            image.scale(width, width)
        tile = np.empty(width * width * 4, dtype=np.float32)
        image.pixels.foreach_get(tile)
        tile = tile.reshape(width, width, 4)
        pixels[y:y + extent, x:x + extent] = np.pad(tile, ((gutter, gutter), (gutter, gutter), (0, 0)), mode='edge')
        tiles[mat.name] = (x + gutter, y + gutter, width)
        x += extent
        height = max(height, extent)
    atlas = bpy.data.images.new('T_MarsDebris_Atlas', width=size, height=size, alpha=False)
    atlas.pixels.foreach_set(pixels.ravel())
    atlas.filepath_raw = str(OUT / 'T_MarsDebris_Atlas.png')
    atlas.file_format = 'PNG'
    atlas.save()
    material = textured('M_MarsDebris_Atlas', atlas)
    for o in objects:
        uv = o.data.uv_layers.active
        for face in o.data.polygons:
            tx, ty, tw = tiles[o.material_slots[face.material_index].material.name]
            for li in face.loop_indices:
                u, v = uv.data[li].uv
                uv.data[li].uv = ((tx + min(1, max(0, u)) * tw) / size,
                                 (ty + min(1, max(0, v)) * tw) / size)
            face.material_index = 0
        o.data.materials.clear()
        o.data.materials.append(material)
    return material


def surface_handoff(whole, material, fragment_centers):
    """Open seams before replacing the unrelated source surface; never eject another shell.

    UV1.x stores the rest radius, UV1.y stores distance from a fragment-direction seam.
    The source bone contracts only 3% while fading. Unreal derives the same progress
    from current / pre-skinned local radius, so seeking the animation also seeks the mask.
    """
    mesh = whole.data
    seeds = [p.normalized() for p in fragment_centers if p.length > .05]
    layer = mesh.uv_layers.new(name='TransitionData')
    assignments = []
    gaps = []
    for face in mesh.polygons:
        direction = sum((mesh.vertices[i].co for i in face.vertices), Vector()).normalized()
        winner = max(range(len(seeds)), key=lambda i: direction.dot(seeds[i]))
        assignments.append(winner)
        for vi in face.vertices:
            d = mesh.vertices[vi].co.normalized()
            gaps.append((winner, max(0, d.dot(seeds[winner]) - max(d.dot(s) for j, s in enumerate(seeds) if j != winner))))
    maxima = {}
    for group, gap in gaps:
        maxima[group] = max(maxima.get(group, .0001), gap)
    k = 0
    for face in mesh.polygons:
        for li in face.loop_indices:
            group, gap = gaps[k]
            layer.data[li].uv = (mesh.vertices[mesh.loops[li].vertex_index].co.length,
                                 gap / maxima[group])
            k += 1
    mesh.uv_layers.active_index = 0
    nodes, links = material.node_tree.nodes, material.node_tree.links
    bsdf = nodes.get('Principled BSDF')
    output = nodes.get('Material Output')
    uv = nodes.new('ShaderNodeUVMap')
    uv.uv_map = 'TransitionData'
    split = nodes.new('ShaderNodeSeparateXYZ')
    links.new(uv.outputs['UV'], split.inputs[0])
    field = nodes.new('ShaderNodeMath')
    field.operation = 'MULTIPLY_ADD'
    field.inputs[1].default_value = .82
    field.inputs[2].default_value = .08
    links.new(split.outputs['Y'], field.inputs[0])
    progress = nodes.new('ShaderNodeValue')
    progress.name = 'SurfaceHandoffProgress'
    # Match the source bone's explicitly sampled linear keys.
    for frame in range(0, END + 1, 3):
        progress.outputs[0].default_value = min(1, max(0, (frame - START) / (HANDOFF_END - START)))
        progress.outputs[0].keyframe_insert('default_value', frame=frame)
    mask = nodes.new('ShaderNodeMath')
    mask.operation = 'GREATER_THAN'
    links.new(field.outputs[0], mask.inputs[0])
    links.new(progress.outputs[0], mask.inputs[1])
    transparent = nodes.new('ShaderNodeBsdfTransparent')
    mix = nodes.new('ShaderNodeMixShader')
    links.new(mask.outputs[0], mix.inputs[0])
    links.new(transparent.outputs[0], mix.inputs[1])
    links.new(bsdf.outputs[0], mix.inputs[2])
    links.new(mix.outputs[0], output.inputs['Surface'])


def skinned(objects):
    arm_data = bpy.data.armatures.new('MarsExplosion_Skeleton')
    arm = bpy.data.objects.new('SK_MarsExplosion_POC', arm_data)
    bpy.context.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    root = arm_data.edit_bones.new('root')
    root.head, root.tail = (0, 0, 0), (0, 0, .1)
    for obj, center, role in objects:
        bone = arm_data.edit_bones.new(obj.name)
        bone.head, bone.tail = center, center + Vector((0, 0, .08))
        bone.parent = root
    bpy.ops.object.mode_set(mode='OBJECT')
    for obj, center, role in objects:
        group = obj.vertex_groups.new(name=obj.name)
        group.add(list(range(len(obj.data.vertices))), 1, 'REPLACE')
        obj.parent = arm
        obj.modifiers.new('RigidFragmentSkin', 'ARMATURE').object = arm
        # Identical UV channel order is necessary when joining the meshes for FBX.
        if len(obj.data.uv_layers) == 1:
            obj.data.uv_layers.new(name='TransitionData')
        obj.data.uv_layers.active_index = 0
    sizes = {o.name: max(max(v.co[i] for v in o.data.vertices) - min(v.co[i] for v in o.data.vertices)
                         for i in range(3)) for o, _, role in objects if role == 'debris'}
    logs = np.log(list(sizes.values()))
    small, large = np.percentile(logs, [10, 90])
    motion = []
    for obj, center, role in objects:
        pb = arm.pose.bones[obj.name]
        basis = pb.bone.matrix_local.to_quaternion()
        pb.rotation_mode = 'QUATERNION'
        if role == 'intact':
            distance, duration, angle = 0, 0, 0
        else:
            size = min(1, max(0, (math.log(sizes[obj.name]) - small) / (large - small)))
            distance, duration = .19 - .07 * size, 21 + 3 * size
            angle = math.radians(RNG.uniform(-1, 1) * (2.5 - 1.7 * size))
            axis = Vector((RNG.uniform(-1, 1), RNG.uniform(-1, 1), RNG.uniform(-1, 1))).normalized()
            radial = center.normalized()
            motion.append(dict(name=obj.name, size_proxy=sizes[obj.name], radial_travel=distance,
                               drift_seconds=duration, initial_speed=2.4 * distance / duration,
                               rotation_degrees=math.degrees(angle)))
        for frame in range(0, END + 1, 3):
            if role == 'intact':
                t = min(1, max(0, (frame - START) / (HANDOFF_END - START)))
                scale = 1 - .03 * t if frame <= HANDOFF_END else 0
                pb.location, pb.rotation_quaternion = (0, 0, 0), (1, 0, 0, 0)
            else:
                u = min(1, max(0, (frame - START) / (FPS * duration)))
                remaining = (1 - u) ** 2.4
                pb.location = basis.inverted() @ (-radial * distance * remaining)
                pb.rotation_quaternion = basis.inverted() @ Quaternion(axis, -angle * remaining) @ basis
                scale = 1
            pb.scale = (scale,) * 3
            for prop in ['location', 'rotation_quaternion', 'scale']:
                pb.keyframe_insert(prop, frame=frame, group=obj.name)
    action = arm.animation_data.action
    action.name = 'A_MarsPlateDrift_POC'
    for slot in action.slots:
        for action_layer in action.layers:
            for strip in action_layer.strips:
                bag = strip.channelbag(slot)
                if bag:
                    for curve in bag.fcurves:
                        for key in curve.keyframe_points:
                            key.interpolation = 'LINEAR'
    (OUT / 'fragment_motion.json').write_text(json.dumps(motion, indent=2), encoding='utf-8')
    return arm


def aim(obj, point=(0, 0, 0)):
    obj.rotation_euler = (Vector(point) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def main():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    scene = bpy.context.scene
    scene.name = 'Mars_Internal_Explosion_POC'
    scene.render.fps = FPS
    scene.frame_start, scene.frame_end = 0, END
    intact = source('mars planet 3d model/mars+planet+3d+model.fbx')
    debris = source('asteroid 3d model/asteroid+3d+model.fbx')
    # FBX import replaces the scene timebase with the supplied source's 24 fps.
    scene.render.fps, scene.render.fps_base = FPS, 1.0
    assert len(intact) == 1 and len(debris) == 107
    debris_atlas(debris)
    original = intact[0]
    image = next(n.image for n in original.material_slots[0].material.node_tree.nodes if n.type == 'TEX_IMAGE' and n.image)
    image.scale(4096, 4096)
    image.filepath_raw = str(OUT / 'T_MarsIntact_4K.png')
    image.file_format = 'PNG'
    image.save()
    outer_mat = textured('M_MarsIntact', image)
    original.data.materials.clear()
    original.data.materials.append(outer_mat)
    objects = []
    for group, diameter, role in [(intact, 2.0, 'intact'), (debris, 2.0, 'debris')]:
        center, old_diameter = box(group)
        for i, obj in enumerate(group):
            transform = obj.matrix_world.copy()
            obj.parent = None
            for v in obj.data.vertices:
                v.co = (transform @ v.co - center) * diameter / old_diameter
            obj.matrix_world = Matrix.Identity(4)
            obj.name = 'Intact' if role == 'intact' else f'Debris_{i:03d}'
            pivot = Vector() if role == 'intact' else sum((v.co for v in obj.data.vertices), Vector()) / len(obj.data.vertices)
            objects.append((obj, pivot, role))
    surface_handoff(original, outer_mat, [center for _, center, role in objects if role == 'debris'])
    arm = skinned(objects)
    # Keep a standalone atlas-optimized final mesh FBX with the exact final relative placement.
    scene.frame_set(END)
    bpy.ops.object.select_all(action='DESELECT')
    copies = []
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for obj, _, role in objects:
        if role != 'debris':
            continue
        mesh = bpy.data.meshes.new_from_object(obj.evaluated_get(depsgraph))
        copy = bpy.data.objects.new('Final_' + obj.name, mesh)
        bpy.context.collection.objects.link(copy)
        copy.select_set(True)
        copies.append(copy)
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'SM_MarsDebris_Final.fbx'), use_selection=True,
                             object_types={'MESH'},
                             bake_anim=False, axis_forward='-Y', axis_up='Z', path_mode='RELATIVE',
                             mesh_smooth_type='FACE')
    for copy in copies:
        bpy.data.objects.remove(copy, do_unlink=True)
    # One skinned mesh, two materials. Only the 107 supplied fragments move outward.
    scene.frame_set(0)
    bpy.ops.object.select_all(action='DESELECT')
    for obj, _, _ in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = original
    bpy.ops.object.join()
    original.name = 'MarsExplosion_Geometry'
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'SK_MarsExplosion_POC.fbx'), use_selection=True,
                             object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False,
                             bake_anim=True, bake_anim_use_all_bones=True,
                             bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                             bake_anim_simplify_factor=0, axis_forward='-Y', axis_up='Z',
                             path_mode='RELATIVE', mesh_smooth_type='FACE')
    bpy.ops.object.camera_add(location=(0, -8, 2.1))
    camera = bpy.context.object
    camera.name = 'POC_Camera'
    aim(camera)
    camera.data.type, camera.data.ortho_scale = 'ORTHO', 3.5
    scene.camera = camera
    for name, loc, power, color in [('Key', (-3, -4, 5), 1300, (1, .78, .59)),
                                    ('Fill', (4, -3, 1), 600, (.57, .68, 1)),
                                    ('Rim', (1, 3, 3), 1800, (1, .28, .055))]:
        bpy.ops.object.light_add(type='AREA', location=loc)
        light = bpy.context.object
        light.name, light.data.energy, light.data.color = name, power, color
        light.data.shape, light.data.size = 'DISK', 4
        aim(light)
    scene.world.use_nodes = True
    scene.world.node_tree.nodes.get('Background').inputs['Color'].default_value = (.002, .003, .008, 1)
    scene.world.node_tree.nodes.get('Background').inputs['Strength'].default_value = .2
    scene.render.engine = 'BLENDER_EEVEE'
    scene.cycles.samples, scene.cycles.use_denoising = 8, True
    scene.render.resolution_x, scene.render.resolution_y = 900, 600
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.view_settings.view_transform = 'AgX'
    for frame, label in [(0, 'INTACT'), (START, 'FRACTURE_AND_DECELERATE'),
                          (120, 'PLATES_SEPARATING'), (HANDOFF_END, 'SURFACE_HANDOFF_COMPLETE'),
                          (420, 'SLOW_DRIFT'), (780, 'EXACT_SUPPLIED_LAYOUT'), (END, 'END')]:
        scene.timeline_markers.new(label, frame=frame)
    scene.frame_set(0)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'Mars_Internal_Explosion_POC.blend'))
    summary = {'version': 2, 'style': 'restrained plate drift; deceleration from release',
               'fps': FPS, 'frames': [0, END], 'duration_seconds': END / FPS,
               'movement_start_seconds': START / FPS, 'surface_handoff_end_seconds': HANDOFF_END / FPS,
               'intact_triangles': 5004, 'original_debris_triangles': 5080,
               'shell_pieces': 0, 'final_debris_pieces': len(debris),
               'bones': len(arm.data.bones), 'export_materials': 2,
               'final_envelope_diameter_vs_intact': 1.0,
               'radial_travel_in_planet_radii': [.12, .19],
               'drift_duration_seconds': [21, 24], 'max_rotation_degrees': 2.5,
               'motion_curve': 'p(u) = 1 - (1-u)^2.4; monotonically decreasing outward speed',
               'final_shape': 'Original 107 supplied fragments. Shared centering and diameter normalization only.',
               'surface_transition': 'Seam-first masked handoff, encoded in UV1; source bone contraction drives UE material without a second clock.',
               'render_step': RENDER_STEP, 'render_frames_directory': 'Saved/MarsDestruction/frames_drift',
               'preview_only': [],
               'warning': 'Authored gameplay motion, not a self-gravity solver. Multiplayer trigger and exploration collision remain to be integrated.'}
    (OUT / 'prototype_manifest.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    frames_dir = ROOT / 'Saved/MarsDestruction/frames_drift'
    frames_dir.mkdir(parents=True, exist_ok=True)
    frames = range(0, END + 1, RENDER_STEP) if '--render' in sys.argv else [0, 60, 120, 210, 300, 360, 540, 840]
    for frame in frames:
        scene.frame_set(frame)
        scene.render.filepath = str(frames_dir / f'{frame:04d}.png')
        bpy.ops.render.render(write_still=True)
    print('MARS_PROTOTYPE', json.dumps(summary))


if __name__ == '__main__':
    main()
