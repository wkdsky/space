"""Bake the astronaut skeleton into a relaxed, asymmetric supine landmark pose."""
import bpy
import json
import math
from pathlib import Path
from mathutils import Matrix, Vector

root=Path("D:/projects/space/SourceArt/SkeletonAstronaut")
bpy.ops.wm.open_mainfile(filepath=str(root/"SkeletonAstronaut.blend"))
rig=bpy.data.objects["CharacterArmature"]
source=bpy.data.objects["SK_SkeletonAstronaut"]
before=set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=str(root/"QA_Idle_Neutral.fbx"))
imported=[o for o in bpy.data.objects if o not in before]
animated=next(o for o in imported if o.type=="ARMATURE")
rig.animation_data_create();rig.animation_data.action=animated.animation_data.action
rig.animation_data.action_slot=animated.animation_data.action_slot
bpy.context.scene.frame_set(10)
bpy.context.view_layer.update()
pose={p.name:p.matrix_basis.copy() for p in rig.pose.bones}
rig.animation_data_clear()
for p in rig.pose.bones:p.matrix_basis=pose[p.name]
for o in imported:bpy.data.objects.remove(o,do_unlink=True)
bpy.context.view_layer.update()
# Settle the lower body below the bulky helmet/backpack. Preserve the upper
# torso while allowing the abdomen to bend, and carry the independent feet.
settle=Matrix.Translation(rig.matrix_world.inverted().to_3x3() @ Vector((0,0.22,0)))
bpy.ops.object.select_all(action="DESELECT");rig.select_set(True)
bpy.context.view_layer.objects.active=rig
bpy.ops.object.mode_set(mode="EDIT")
# This temporary, static-only pose needs a bending offset at the waist. The
# shared playable skeleton is never edited or exported by this bake.
rig.data.edit_bones["Torso"].use_connect=False
bpy.ops.object.mode_set(mode="OBJECT")
bpy.context.view_layer.update()
upper_torso=rig.pose.bones["Torso"].matrix.copy()
rig.pose.bones["Body"].matrix=settle @ rig.pose.bones["Body"].matrix
bpy.context.view_layer.update()
rig.pose.bones["Torso"].matrix=upper_torso
bpy.context.view_layer.update()
for side in ("L","R"):
    foot=rig.pose.bones["Foot_"+side]
    foot.matrix=settle @ foot.matrix
bpy.context.view_layer.update()
# Slightly separated limbs avoid a standing attention pose lying on its back.
for name,degrees in (("UpperArm_L",-16),("UpperArm_R",25),("UpperLeg_L",-7),("UpperLeg_R",5)):
    bone=rig.pose.bones[name]
    pivot=bone.matrix.translation.copy()
    rotation=Matrix.Rotation(math.radians(degrees),4,"Y")
    if "UpperLeg" in name:
        rotation=Matrix.Rotation(math.radians(6),4,"X") @ rotation
    delta=Matrix.Translation(pivot) @ rotation @ Matrix.Translation(-pivot)
    foot=rig.pose.bones.get(name.replace("UpperLeg", "Foot")) if "UpperLeg" in name else None
    foot_matrix=foot.matrix.copy() if foot else None
    bone.matrix=delta @ bone.matrix
    # Foot bones are children of Root on this skeleton, not of the calves.
    # Move the foot with its leg instead of leaving a detached ankle behind.
    if foot:foot.matrix=delta @ foot_matrix
    bpy.context.view_layer.update()
graph=bpy.context.evaluated_depsgraph_get()
evaluated=source.evaluated_get(graph)
data=bpy.data.meshes.new_from_object(evaluated,preserve_all_data_layers=True,depsgraph=graph)
corpse=bpy.data.objects.new("SM_SkeletonAstronaut_Corpse",data)
bpy.context.collection.objects.link(corpse)
lying=Matrix.Rotation(-math.pi/2,4,"X")
for vertex in data.vertices:vertex.co=lying @ source.matrix_world @ vertex.co
lo=Vector([min(v.co[i] for v in data.vertices) for i in range(3)])
hi=Vector([max(v.co[i] for v in data.vertices) for i in range(3)])
offset=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
for vertex in data.vertices:vertex.co-=offset
support_regions={}
for label,bones in {"left_boot":["Foot_L"],"right_boot":["Foot_R"],
                    "pelvis":["Hips"],"head":["Head"],
                    "back":["Torso"],"left_hand":["Wrist_L"],"right_hand":["Wrist_R"]}.items():
    indices={source.vertex_groups[b].index for b in bones if b in source.vertex_groups}
    points=[data.vertices[v.index].co for v in source.data.vertices
            if any(g.group in indices and g.weight>0.5 for g in v.groups)]
    if points:support_regions[label]={"minimum_z_m":min(p.z for p in points),"vertex_count":len(points)}
assert all(support_regions[label]["minimum_z_m"]<0.025 for label in
           ("left_boot","right_boot","pelvis","head","back")), support_regions
for p in data.polygons:p.use_smooth=False
bpy.context.view_layer.update()
bpy.ops.object.select_all(action="DESELECT");corpse.select_set(True)
bpy.context.view_layer.objects.active=corpse
bpy.ops.export_scene.fbx(filepath=str(root/"SM_SkeletonAstronaut_Corpse.fbx"),use_selection=True,
    object_types={"MESH"},bake_anim=False,axis_forward="-Y",axis_up="Z",mesh_smooth_type="FACE")
for o in list(bpy.data.objects):
    if o.type in {"ARMATURE","MESH"} and o!=corpse and o.name!="Plane":bpy.data.objects.remove(o,do_unlink=True)
ground=bpy.data.objects.get("Plane")
if ground:ground.location.z=0.0
cam=bpy.context.scene.camera;cam.location=(2.2,-3,3.2)
cam.rotation_euler=(Vector((0,0,0.22))-cam.location).to_track_quat("-Z","Y").to_euler()
cam.data.ortho_scale=2.7
scene=bpy.context.scene;scene.cycles.samples=24
scene.render.resolution_x=1100;scene.render.resolution_y=900
scene.render.filepath=str(root/"Preview_Corpse.png")
bpy.ops.wm.save_as_mainfile(filepath=str(root/"SkeletonAstronaut_Corpse.blend"))
bpy.ops.render.render(write_still=True)
data.calc_loop_triangles()
report={"vertices":len(data.vertices),"triangles":len(data.loop_triangles),"dimensions_m":list(corpse.dimensions),
        "materials":[m.name for m in data.materials],"minimum_z_m":min(v.co.z for v in data.vertices),
        "pivot":"XY visual center, Z lowest point", "pose":"Supine, relaxed asymmetric arms and legs",
        "support_regions":support_regions}
(root/"corpse_pose_report.json").write_text(json.dumps(report,indent=2))
print("CORPSE_POSE",report)
