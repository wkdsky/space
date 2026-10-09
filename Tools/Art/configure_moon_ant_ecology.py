"""Configure Moon population balance and the grounded crater-centre corpse after native compilation."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve() / "SourceArt/MoonAntNests"
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world(), "Stop PIE before configuring Moon ecology"
data = unreal.load_asset("/Game/Space/Data/Planets/Moon/PDA_MoonSurfaceGameplay")
balance = {
    "max_active_moon_ants_per_nest": 30,
    "moon_ant_spawn_interval_min": .12, "moon_ant_spawn_interval_max": 1.0,
    "moon_ant_spawn_chance": 1.0,
    "moon_ant_spawn_near_weight": .65, "moon_ant_spawn_mid_weight": .25, "moon_ant_spawn_far_weight": .10,
    "moon_ant_spawn_near_distance_min": 60, "moon_ant_spawn_near_distance_max": 900,
    "moon_ant_spawn_mid_distance_min": 900, "moon_ant_spawn_mid_distance_max": 2800,
    "moon_ant_spawn_far_distance_min": 2800, "moon_ant_spawn_far_distance_max": 6000,
    "moon_ant_roam_radius": 6000, "moon_ant_max_home_radius": 7000,
    "moon_ant_roam_retarget_interval_min": 2.5, "moon_ant_roam_retarget_interval_max": 6.0,
    "moon_ant_surface_duration_min": 90, "moon_ant_surface_duration_max": 150,
    "moon_ant_turn_speed": 240,
}
data.modify()
for name, value in balance.items():
    data.set_editor_property(name, value)
assert unreal.EditorAssetLibrary.save_loaded_asset(data, only_if_is_dirty=False)
nest_bp = unreal.load_asset("/Game/Space/Blueprints/Planets/Moon/BP_MoonAntNestEntrance")
unreal.get_default_object(nest_bp.generated_class()).set_editor_property("maintain_population", True)
unreal.BlueprintEditorLibrary.compile_blueprint(nest_bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(nest_bp, only_if_is_dirty=False)
ant_bp = unreal.load_asset("/Game/Space/Blueprints/Planets/Moon/BP_MoonAnt")
steering = unreal.get_default_object(ant_bp.generated_class()).get_editor_property("surface_steering")
steering.set_editor_property("max_slope_degrees", 35)
steering.set_editor_property("probe_distance", 65)
steering.set_editor_property("body_radius", 12)
steering.set_editor_property("avoidance_commit_seconds", 1.25)
unreal.BlueprintEditorLibrary.compile_blueprint(ant_bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(ant_bp, only_if_is_dirty=False)

fit = json.loads((root / "centered_corpse_fit.json").read_text())
mesh_path = "/Game/Space/Characters/SkeletonAstronaut/" + fit["mesh"]
flag = "Interchange.FeatureFlags.Import.FBX"
previous = unreal.SystemLibrary.get_console_variable_int_value(flag)
unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), flag + " 0")
try:
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_as_skeletal = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_mesh = True
    options.import_materials = False
    options.import_textures = False
    options.static_mesh_import_data.auto_generate_collision = False
    task = unreal.AssetImportTask()
    task.filename = str(root / (fit["mesh"] + ".fbx"))
    task.destination_path, task.destination_name = mesh_path.rsplit("/", 1)
    task.automated = True
    task.replace_existing = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(mesh_path)
    assert mesh and task.imported_object_paths
    slots = list(mesh.static_materials)
    for slot in slots:
        key = str(slot.material_slot_name).removeprefix("M_SA_")
        slot.material_interface = unreal.load_asset("/Game/Space/Characters/SkeletonAstronaut/Materials/MI_SA_" + key)
        assert slot.material_interface, key
    mesh.set_editor_property("static_materials", slots)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
finally:
    unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), flag + " " + str(previous))
actors = {a.get_actor_label(): a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
corpse = actors["Moon_Corpse_SkeletonAstronaut"]
corpse.modify()
corpse.get_editor_property("corpse_mesh").set_static_mesh(mesh)
q = unreal.Quat(*fit["actor_transform"]["rotation"])
corpse.set_actor_rotation(q.rotator(), False)
corpse.set_actor_location(unreal.Vector(*fit["actor_transform"]["location"]), False, False)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(root / "moon_ant_ecology_balance.json").write_text(json.dumps(balance, indent=2), encoding="utf-8")
print("MOON_ANT_ECOLOGY_CONFIGURED", balance)
