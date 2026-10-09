"""Save the native Unreal viewport capture for ant-nest visual QA."""
import base64
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve() / "SourceArt/MoonAntNests"
overview = globals().pop("MOON_ANT_NEST_CAPTURE_OVERVIEW", False)
camera = json.loads(MoonAntNestAuthoringTools.camera_transform(1, overview))
args = {
    "captureTransform": camera,
    "annotations": {
        "gridSpacing": 0, "gridExtent": 0, "gridHeight": 0,
        "maxLabelDistance": 40000 if overview else 0,
        "classFilter": {"refPath": "/Script/space.JTSMoonAntNestActor"},
        "maxLabels": 9 if overview else 0,
    },
    "bShowUI": False,
}
capture_result = unreal.ToolsetRegistry.execute_tool(
    "EditorToolset.EditorAppToolset", "CaptureViewport", json.dumps(args)
)


def save_capture(delta):
    if not capture_result.is_complete:
        return
    unreal.unregister_slate_post_tick_callback(capture_tick)
    if capture_result.error:
        raise RuntimeError(capture_result.error)
    result = json.loads(capture_result.value)["returnValue"]
    stem = "Moon_AntNests_CraterLayout" if overview else "Moon_AntNest_Close"
    (root / (stem + ".png")).write_bytes(base64.b64decode(result["image"]["data"]))
    result.pop("image")
    (root / (stem + ".json")).write_text(json.dumps(result, indent=2), encoding="utf-8")
    unreal.log("MOON_ANT_NEST_VIEWPORT_SAVED " + stem)


capture_tick = unreal.register_slate_post_tick_callback(save_capture)
