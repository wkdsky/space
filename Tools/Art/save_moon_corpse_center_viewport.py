"""Save a native Unreal viewport image of the corpse at the fitted crater centre."""
import base64
import json
from pathlib import Path
import unreal


def capture_moon_corpse_centre():
    root = Path(unreal.Paths.project_dir()).resolve() / "SourceArt/MoonAntNests"
    camera = json.loads((root / "moon_corpse_center_camera.json").read_text())
    args = {
        "captureTransform": camera,
        "annotations": {"gridSpacing": 0, "gridExtent": 0, "gridHeight": 0,
                        "maxLabelDistance": 40000, "maxLabels": 1,
                        "classFilter": {"refPath": "/Script/space.JTSMoonCorpseActor"}},
        "bShowUI": False,
    }
    result = unreal.ToolsetRegistry.execute_tool("EditorToolset.EditorAppToolset", "CaptureViewport", json.dumps(args))
    handle = None

    def save(delta):
        if not result.is_complete:
            return
        unreal.unregister_slate_post_tick_callback(handle)
        assert not result.error, result.error
        capture = json.loads(result.value)["returnValue"]
        (root / "Moon_Corpse_CraterCenter.png").write_bytes(base64.b64decode(capture.pop("image")["data"]))
        (root / "Moon_Corpse_CraterCenter.json").write_text(json.dumps(capture, indent=2), encoding="utf-8")
        unreal.log("MOON_CORPSE_CENTRE_VIEWPORT_SAVED")

    handle = unreal.register_slate_post_tick_callback(save)
    return handle


moon_corpse_centre_capture_handle = capture_moon_corpse_centre()
