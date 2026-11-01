"""Read the authored ship geometry in the live Blender MCP session, then remove only the audit import."""
import bpy
import json
from pathlib import Path
from mathutils import Vector

root = Path('D:/projects/space')
before = set(bpy.data.objects)
try:
    bpy.ops.wm.obj_import(filepath=str(root / 'SourceArt/TwinforkR1/SM_Twinfork_R1_Shell.obj'),
                          forward_axis='X', up_axis='Z')
    imported = set(bpy.data.objects) - before
    # Blender's importer changes the authoring basis. Read source coordinates for
    # comparison with Unreal's +X-forward, centimetre socket frame.
    vertices = [Vector(tuple(float(n) for n in line.split()[1:4]))
                for line in (root / 'SourceArt/TwinforkR1/SM_Twinfork_R1_Shell.obj').read_text().splitlines()
                if line.startswith('v ')]
    noses = {}
    for side in [-1, 1]:
        points = [v for v in vertices if abs(v.y - side * 345) < 80]
        noses[str(side)] = max(v.x for v in points)
    result = dict(blender=bpy.app.version_string, bounds_min=[min(v[i] for v in vertices) for i in range(3)],
                  bounds_max=[max(v[i] for v in vertices) for i in range(3)], nose_faces_x=noses,
                  light_housing_centre=[660, 345, 19], light_housing_half_size=[8, 27.5, 14])
    (root / 'Saved/SpacecraftFlow/blender_mounts.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result))
finally:
    for obj in set(bpy.data.objects) - before:
        mesh = obj.data if obj.type == 'MESH' else None
        bpy.data.objects.remove(obj, do_unlink=True)
        if mesh and mesh.users == 0:
            bpy.data.meshes.remove(mesh)
