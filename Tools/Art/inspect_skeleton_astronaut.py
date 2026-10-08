"""Inspect source meshes and reference transforms in background Blender."""
import bpy
import json
from pathlib import Path

root = Path("D:/projects/space/SourceArt/SkeletonAstronaut")
for o in bpy.data.objects:
    if o.type == "ARMATURE":
        o.animation_data_clear()
        o.data.pose_position = "REST"
bpy.context.view_layer.update()
info = []
for o in bpy.data.objects:
    row = {"name": o.name, "type": o.type, "dim": list(o.dimensions), "loc": list(o.location), "rotation": list(o.rotation_euler), "scale": list(o.scale), "parent": o.parent.name if o.parent else None}
    if o.type == "ARMATURE":
        row["bones"] = [{"name": b.name, "parent": b.parent.name if b.parent else None, "head": list(o.matrix_world @ b.head_local), "tail": list(o.matrix_world @ b.tail_local)} for b in o.data.bones]
    if o.type == "MESH":
        row["materials"] = [m.name for m in o.data.materials]
        row["weights"] = [g.name for g in o.vertex_groups]
        row["vertices"] = len(o.data.vertices)
    info.append(row)
name = Path(bpy.data.filepath).stem
(root / (name + "_inspection.json")).write_text(json.dumps(info, indent=2))
print(json.dumps(info)[:16000])
