"""Render samples of the project's real player animation clips on the new mesh."""
import bpy
import json
import sys
from pathlib import Path
ROOT=Path("D:/projects/space/SourceArt/SkeletonAstronaut")
rig=bpy.data.objects["CharacterArmature"]
mesh=bpy.data.objects["SK_SkeletonAstronaut"]
report=[]
original=set(bpy.data.objects)
for label in ("Idle_Neutral","Walk","Run","Punch_Left","Punch_Right"):
    bpy.ops.import_scene.fbx(filepath=str(ROOT/("QA_"+label+".fbx")))
    imported=[o for o in bpy.data.objects if o not in original]
    animated=next(o for o in imported if o.type=="ARMATURE")
    action=animated.animation_data.action
    rig.animation_data_create();rig.animation_data.action=action
    rig.animation_data.action_slot=animated.animation_data.action_slot
    # Verify animated rig and mesh rig rest matrices before reusing the action.
    errors=[]
    for b in rig.data.bones:
        other=animated.data.bones.get(b.name)
        if other:
            delta=rig.matrix_world @ b.matrix_local - animated.matrix_world @ other.matrix_local
            errors.append(max(abs(x) for row in delta for x in row))
    for o in imported:bpy.data.objects.remove(o,do_unlink=True)
    first,last=action.frame_range
    frame=first+(last-first)*(0.48 if "Punch" in label else 0.23)
    bpy.context.scene.frame_set(int(frame),subframe=frame-int(frame))
    bpy.context.view_layer.update()
    # Reject any non-finite pose values or wildly expanded mesh bounds.
    dim=list(mesh.dimensions)
    assert all(0 < x < 3.0 for x in dim), (label,dim)
    report.append({"clip":label,"frame":frame,"dimensions_m":dim,"rest_matrix_max_error":max(errors),"image":"Preview_"+label+".png"})
    bpy.context.scene.render.filepath=str(ROOT/("Preview_"+label+".png"))
    bpy.ops.render.render(write_still=True)
(ROOT/"animation_qa.json").write_text(json.dumps(report,indent=2))
print("ANIMATION_QA",report)
