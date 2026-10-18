"""Inspect the two supplied Mars FBXs in an isolated Blender background process.

Usage: blender --background --python Tools/Art/inspect_mars_destruction.py
Original FBXs and the user's open Blender scene are never edited.
"""
import json
import math
from pathlib import Path

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "SourceArt/MarsDestruction"
OUTPUT.mkdir(parents=True, exist_ok=True)


def bounds(objects):
    points = [o.matrix_world @ Vector(c) for o in objects for c in o.bound_box]
    lo = Vector(tuple(min(p[i] for p in points) for i in range(3)))
    hi = Vector(tuple(max(p[i] for p in points) for i in range(3)))
    return lo, hi


def islands(mesh):
    parent = list(range(len(mesh.vertices)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for e in mesh.edges:
        a, b = (find(i) for i in e.vertices)
        parent[b] = a
    return len({find(i) for i in range(len(parent))})


def import_source(name, relative):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(ROOT / relative))
    objects = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    bpy.context.view_layer.update()
    lo, hi = bounds(objects)
    materials = {slot.material for o in objects for slot in o.material_slots if slot.material}
    images = {n.image for m in materials if m.use_nodes for n in m.node_tree.nodes
              if n.type == 'TEX_IMAGE' and n.image}
    report = {
        'source': relative, 'mesh_objects': len(objects),
        'vertices': sum(len(o.data.vertices) for o in objects),
        'triangles': sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in objects),
        'connected_islands': sum(islands(o.data) for o in objects),
        'materials': len(materials), 'images': len(images),
        'bounds_min': list(lo), 'bounds_max': list(hi), 'dimensions': list(hi - lo),
        'uv_layers': sorted({len(o.data.uv_layers) for o in objects}),
        'image_sizes': sorted({tuple(i.size) for i in images}),
        'missing_images': [i.filepath for i in images if not Path(bpy.path.abspath(i.filepath)).is_file()],
        'objects': [{'name': o.name, 'vertices': len(o.data.vertices),
                     'triangles': sum(len(p.vertices) - 2 for p in o.data.polygons),
                     'islands': islands(o.data)} for o in objects],
    }
    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    for o in objects:
        for c in list(o.users_collection):
            c.objects.unlink(o)
        collection.objects.link(o)
        o.parent = None
        o.matrix_world = o.matrix_world.copy()
    return objects, report


def aim(obj, point):
    obj.rotation_euler = (Vector(point) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def main():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    scene = bpy.context.scene
    scene.name = 'Mars_Asset_Inspection'
    whole, whole_report = import_source('Source_Intact', 'mars planet 3d model/mars+planet+3d+model.fbx')
    broken, broken_report = import_source('Source_Debris', 'asteroid 3d model/asteroid+3d+model.fbx')
    report = {'intact': whole_report, 'debris': broken_report,
              'note': 'Independent meshes. Normalized side-by-side envelope sizes are for inspection, not a geometric correspondence.'}
    (OUTPUT / 'asset_audit.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    for objects, x in [(whole, -1.65), (broken, 1.65)]:
        lo, hi = bounds(objects)
        center = (lo + hi) * .5
        scale = 2.5 / max(hi - lo)
        for o in objects:
            # Bake world transform, then use one shared normalization about the original group center.
            transform = o.matrix_world.copy()
            for v in o.data.vertices:
                v.co = (transform @ v.co - center) * scale + Vector((x, 0, 0))
            o.matrix_world.identity()
    bpy.ops.object.camera_add(location=(0, -9.7, 3.0))
    camera = bpy.context.object
    camera.name = 'Inspection_Camera'
    aim(camera, (0, 0, 0))
    camera.data.type = 'ORTHO'
    camera.data.ortho_scale = 6.4
    scene.camera = camera
    for name, location, energy, size, color in [
        ('Key', (-3, -4, 5), 1700, 5, (1, .77, .56)),
        ('Fill', (4, -2, 1), 1100, 4, (.6, .73, 1)),
        ('Rim', (0, 3, 4), 2000, 3, (1, .35, .12)),
    ]:
        bpy.ops.object.light_add(type='AREA', location=location)
        light = bpy.context.object
        light.name = name
        light.data.energy = energy
        light.data.shape = 'DISK'
        light.data.size = size
        light.data.color = color
        aim(light, (0, 0, 0))
    scene.world.color = (.035, .035, .035)
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1400
    scene.render.resolution_y = 800
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.view_settings.view_transform = 'AgX'
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT / 'Mars_Asset_Inspection.blend'))
    scene.render.filepath = str(OUTPUT / 'Asset_Comparison.png')
    bpy.ops.render.render(write_still=True)
    print('MARS_AUDIT', json.dumps({k: {a: b for a, b in v.items() if a != 'objects'}
                                         for k, v in report.items() if isinstance(v, dict)}))


if __name__ == '__main__':
    main()
