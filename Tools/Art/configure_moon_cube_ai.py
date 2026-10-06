"""Run with Unreal Editor Python after compiling the native crowd AI."""
import unreal

PATH = "/Game/Space/Blueprints/Enemies/BP_MoonCubeEnemy"
bp = unreal.load_asset(PATH)
if bp is None:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.JTSMoonCubeEnemy)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "BP_MoonCubeEnemy", "/Game/Space/Blueprints/Enemies", unreal.Blueprint, factory)
    if bp is None:
        raise RuntimeError("Could not create the moon cube configuration Blueprint")
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cube_class = unreal.load_class(None, PATH + ".BP_MoonCubeEnemy_C")
cdo = unreal.get_default_object(cube_class)
behavior = cdo.get_editor_property("Behavior")
for name, value in dict(roam_radius=2800.0, aggro_radius=3500.0, leash_radius=4500.0,
                        retaliation_leash_radius=9000.0, sight_radius=6500.0,
                        home_return_radius=600.0, reacquire_cooldown=3.0,
                        chase_speed=380.0, acceleration=900.0, hover_height=50.0,
                        scan_interval=0.35, target_memory_seconds=6.0,
                        retaliation_memory_seconds=15.0, bAcquireOnSpawn=True,
                        bShareSettlementAlert=True, movement_interval=0.05,
                        idle_movement_interval=0.3, separation_radius=140.0,
						distant_movement_interval=0.1, distant_movement_distance=1000.0,
                        separation_weight=0.75).items():
    behavior.set_editor_property(name, value)
cdo.set_editor_property("Behavior", behavior)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp)

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for name in ("L_SpaceWorld", "L_SpaceWorld_Authoring"):
    if not level.load_level("/Game/Space/Maps/" + name):
        raise RuntimeError("Could not load " + name)
    clusters = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.JTSMoonCubeCluster)]
    if name == "L_SpaceWorld" and len(clusters) != 1:
        raise RuntimeError("Expected exactly one moon cube settlement")
    for cluster in clusters:
        cluster.set_editor_property("EnemyClass", cube_class)
        print("MOON_CROWD_AI_CONFIGURED", name, cluster.get_name(), cube_class.get_path_name())
    if clusters and not level.save_current_level():
        raise RuntimeError("Could not save " + name)
print("MOON_CROWD_AI_CONFIGURATION_COMPLETE")
