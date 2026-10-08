"""Export the active player's reference skeleton without changing existing assets."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve() / "SourceArt/SkeletonAstronaut"
root.mkdir(parents=True, exist_ok=True)
mesh = unreal.load_asset("/Game/ThirdParty/Player/UltimateModularMen/Casual_2/Mesh/Casual_2")
task = unreal.AssetExportTask()
task.object = mesh
task.filename = str(root / "Casual_2_reference.fbx")
task.automated = True
task.prompt = False
task.replace_identical = True
task.exporter = unreal.SkeletalMeshExporterFBX()
options = unreal.FbxExportOption()
options.ascii = False
options.level_of_detail = False
options.export_morph_targets = False
task.options = options
assert unreal.Exporter.run_asset_export_task(task)
bp = unreal.load_asset("/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2")
cdo = unreal.get_default_object(bp.generated_class())
component = cdo.get_editor_property("mesh")
info = {"mesh": mesh.get_path_name(), "skeleton": mesh.skeleton.get_path_name(),
        "relative_location": str(component.relative_location),
        "relative_rotation": str(component.relative_rotation),
        "relative_scale": str(component.relative_scale3d),
        "anim_class": str(component.anim_class),
        "materials": [str(m.material_interface) for m in mesh.materials]}
print(json.dumps(info))
(root / "player_reference.json").write_text(json.dumps(info, indent=2), encoding="utf-8")
