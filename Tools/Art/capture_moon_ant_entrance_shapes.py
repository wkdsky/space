"""Capture four real, fitted entrance silhouettes from the Unreal viewport."""
import base64
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve() / "SourceArt/MoonAntNests"
actors = {a.get_actor_label(): a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
_shape_captures = [("Moon_AntNest_01", "Slit"), ("Moon_AntNest_02", "Leaning"),
                   ("Moon_AntNest_04", "Olive"), ("Moon_AntNest_05", "Irregular")]
_shape_capture_result = None
_shape_capture_stem = None

def _capture_next_shape():
    global _shape_capture_result, _shape_capture_stem
    if not _shape_captures:
        unreal.unregister_slate_post_tick_callback(_shape_capture_handle)
        unreal.log("MOON_ANT_SHAPE_CAPTURES_COMPLETE")
        return
    name, suffix = _shape_captures.pop(0)
    nest = actors[name]
    component = nest.get_editor_property("entrance_shape")
    shape = component.get_editor_property("shape")
    height = shape.get_editor_property("height")
    slope = component.get_editor_property("wall_slope")
    transform = component.get_world_transform()
    normal = transform.rotation.rotate_vector(unreal.MathLibrary.normal(unreal.Vector(1, 0, slope)))
    target = transform.transform_location(unreal.Vector(-slope * height * 0.45, 0, height * 0.45))
    camera = target + normal * max(shape.get_editor_property("width") * 1.3, height * 3.5)
    forward = -normal
    right = unreal.MathLibrary.normal(unreal.MathLibrary.cross_vector_vector(nest.get_actor_up_vector(), forward))
    up = unreal.MathLibrary.cross_vector_vector(forward, right)
    rotation = unreal.MathLibrary.make_rotation_from_axes(forward, right, up)
    args = {"captureTransform": {
        "location": {"x": camera.x, "y": camera.y, "z": camera.z},
        "rotation": {"pitch": rotation.pitch, "yaw": rotation.yaw, "roll": rotation.roll}},
        "annotations": {"gridSpacing": 0, "gridExtent": 0, "gridHeight": 0, "maxLabelDistance": 0,
                        "classFilter": {"refPath": "/Script/space.JTSMoonAntNestActor"}, "maxLabels": 0}, "bShowUI": False}
    _shape_capture_stem = "Moon_AntNest_Shape_" + suffix
    _shape_capture_result = unreal.ToolsetRegistry.execute_tool("EditorToolset.EditorAppToolset", "CaptureViewport", json.dumps(args))

def _save_shape_capture(delta):
    if _shape_capture_result is None:
        _capture_next_shape()
        return
    if not _shape_capture_result.is_complete:
        return
    if _shape_capture_result.error:
        unreal.unregister_slate_post_tick_callback(_shape_capture_handle)
        raise RuntimeError(_shape_capture_result.error)
    result = json.loads(_shape_capture_result.value)["returnValue"]
    (root / (_shape_capture_stem + ".png")).write_bytes(base64.b64decode(result["image"]["data"]))
    result.pop("image")
    (root / (_shape_capture_stem + ".json")).write_text(json.dumps(result, indent=2), encoding="utf-8")
    unreal.log("MOON_ANT_SHAPE_CAPTURED " + _shape_capture_stem)
    _capture_next_shape()

_shape_capture_handle = unreal.register_slate_post_tick_callback(_save_shape_capture)
