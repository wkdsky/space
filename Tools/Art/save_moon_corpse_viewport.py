"""Save the native Unreal MCP viewport image to source-art QA, without image processing."""
import base64
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()/"SourceArt/SkeletonAstronaut"
actor=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
           if a.get_actor_label()=="Moon_Corpse_SkeletonAstronaut")
wide=globals().pop("MOON_CORPSE_CAPTURE_WIDE",False)
offset=unreal.Vector(0,-5500,19500) if wide else unreal.Vector(250,-280,340)
point=actor.get_actor_location()+actor.get_actor_transform().rotation.rotate_vector(offset)
target=actor.get_actor_location()+actor.get_actor_up_vector()*25
forward=unreal.MathLibrary.normal(target-point)
right=unreal.MathLibrary.normal(unreal.MathLibrary.cross_vector_vector(actor.get_actor_up_vector(),forward))
up=unreal.MathLibrary.cross_vector_vector(forward,right)
rotation=unreal.MathLibrary.make_rotation_from_axes(forward,right,up)
camera={"location":{"x":point.x,"y":point.y,"z":point.z},
        "rotation":{"pitch":rotation.pitch,"yaw":rotation.yaw,"roll":rotation.roll},
        "scale":{"x":1,"y":1,"z":1}}
args={"captureTransform":camera,"annotations":{"gridSpacing":0,"gridExtent":0,"gridHeight":0,
      "maxLabelDistance":0,"classFilter":{"refPath":"/Script/Engine.Actor"},"maxLabels":0},"bShowUI":False}
capture_result=unreal.ToolsetRegistry.execute_tool("EditorToolset.EditorAppToolset","CaptureViewport",json.dumps(args))
def save_capture(delta):
    if not capture_result.is_complete:return
    unreal.unregister_slate_post_tick_callback(capture_tick)
    if capture_result.error:raise RuntimeError(capture_result.error)
    result=json.loads(capture_result.value)["returnValue"]
    filename="Moon_Corpse_CraterLayout.png" if wide else "Moon_Corpse_InLevel.png"
    (root/filename).write_bytes(base64.b64decode(result["image"]["data"]))
    result.pop("image")
    metadata="moon_corpse_crater_capture.json" if wide else "moon_corpse_capture.json"
    (root/metadata).write_text(json.dumps(result,indent=2))
    unreal.log("MOON_CORPSE_VIEWPORT_SAVED "+filename)
capture_tick=unreal.register_slate_post_tick_callback(save_capture)
