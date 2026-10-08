"""Validate animation compatibility and export real player animations for art QA."""
import unreal
import json
from pathlib import Path

root=Path(unreal.Paths.project_dir()).resolve()/"SourceArt/SkeletonAstronaut"
mesh=unreal.load_asset("/Game/Space/Characters/SkeletonAstronaut/SK_SkeletonAstronaut")
reference=unreal.load_asset("/Game/ThirdParty/Player/UltimateModularMen/Casual_2/Mesh/Casual_2")
bp=unreal.load_asset("/Game/Space/Blueprints/Player/BP_JTSPlayer_SkeletonAstronaut")
cdo=unreal.get_default_object(bp.generated_class())
component=cdo.get_editor_property("mesh")
assert mesh.skeleton == reference.skeleton
assert component.get_editor_property("skeletal_mesh_asset") == mesh
assert "ABP_JTSPlayer_Casual_2" in component.anim_class.get_path_name()
assert not component.get_editor_property("override_materials")
assert mesh.physics_asset is not None
assert all(s.material_interface for s in mesh.materials)
report={"skeleton":mesh.skeleton.get_path_name(),"player_blueprint":bp.get_path_name(),
        "animation_blueprint":component.anim_class.get_path_name(),"physics_asset":mesh.physics_asset.get_path_name(),
        "mesh":mesh.get_path_name(),"checks":[],"animations":[]}
for label in ("Idle_Neutral","Walk","Run","Punch_Left","Punch_Right"):
    path="/Game/ThirdParty/Player/UltimateModularMen/Casual_2/Mesh/Casual_2CharacterArmature_"+label
    animation=unreal.load_asset(path)
    assert animation.get_editor_property("skeleton") == mesh.skeleton
    task=unreal.AssetExportTask();task.object=animation;task.filename=str(root/("QA_"+label+".fbx"))
    task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.AnimSequenceExporterFBX()
    task.options=unreal.FbxExportOption();task.options.ascii=False
    assert unreal.Exporter.run_asset_export_task(task)
    report["animations"].append({"name":label,"asset":path,"duration":animation.get_play_length()})
report["checks"]=["same skeleton asset","player mesh configured","existing animation blueprint reused","no old material overrides","physics assigned","all materials assigned","five player animation skeletons match"]
(root/"unreal_validation.json").write_text(json.dumps(report,indent=2))
print(json.dumps(report))
for path in ("/Game/Space/Blueprints/Modes/BP_EarthGameMode", "/Game/Space/Blueprints/Modes/BP_SpaceWorldGameMode"):
    mode=unreal.load_asset(path)
    defaults=unreal.get_default_object(mode.generated_class())
    print("GAMEMODE",path,"DEFAULT_PAWN",defaults.get_editor_property("default_pawn_class").get_path_name())
