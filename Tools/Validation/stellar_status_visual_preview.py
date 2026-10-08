"""Render four real cube enemies and downward hail in a disposable editor world.

Run via the editor Python console. No gameplay map or preview actor is saved.
"""
from pathlib import Path
import json
import time
import unreal

OUT = Path(unreal.Paths.project_saved_dir()).resolve() / "StellarStatusQA"
OUT.mkdir(parents=True, exist_ok=True)
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
FX = "/Game/Space/Blueprints/Weapons/Stellar/"
enemy_cls = unreal.load_class(None, "/Script/space.JTSMoonCubeEnemy")
status_cls = unreal.load_class(None, FX + "BP_StellarTargetStatusFX.BP_StellarTargetStatusFX_C")
cube = unreal.load_asset("/Engine/BasicShapes/Cube")
body_mat = unreal.load_asset("/Game/Space/Materials/Moon/M_MoonCubeBody")
preview_actors = []

def spawn(cls, location, rotation=unreal.Rotator()):
    actor = actors.spawn_actor_from_class(cls, location, rotation, transient=True)
    preview_actors.append(actor)
    return actor

for i, flags in enumerate(((False,True,False,False),(True,False,False,False),(False,False,False,True),(True,True,False,True))):
    enemy = spawn(enemy_cls, unreal.Vector(0, (i-1.5)*180, 50))
    for mesh in enemy.get_components_by_class(unreal.StaticMeshComponent):
        mesh.set_static_mesh(cube); mesh.set_material(0, body_mat)
    status = spawn(status_cls, enemy.get_actor_location())
    status.set_owner(enemy); status.update_status(*flags)

floor = spawn(unreal.StaticMeshActor, unreal.Vector(0,0,-12))
floor.static_mesh_component.set_static_mesh(cube)
floor.set_actor_scale3d(unreal.Vector(18,18,.2))
light = spawn(unreal.DirectionalLight, unreal.Vector(0,0,600), unreal.Rotator(-55,-25,0))
light.light_component.set_editor_property("intensity", 4.)
sky = spawn(unreal.SkyLight, unreal.Vector(0,0,300))
sky.light_component.set_editor_property("intensity", .8)
capture = spawn(unreal.SceneCapture2D, unreal.Vector(850,0,330))
capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(capture.get_actor_location(), unreal.Vector(0,0,80)), False)
component = capture.get_component_by_class(unreal.SceneCaptureComponent2D)
component.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
component.set_editor_property("capture_every_frame", False)
component.set_editor_property("fov_angle", 55.)
settings = component.get_editor_property("post_process_settings")
settings.set_editor_property("override_auto_exposure_method", True)
settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property("override_auto_exposure_bias", True)
settings.set_editor_property("auto_exposure_bias", 0.)
component.set_editor_property("post_process_settings", settings)
target = unreal.RenderingLibrary.create_render_target2d(world, 1600, 900, unreal.TextureRenderTargetFormat.RTF_RGBA8)
component.set_editor_property("texture_target", target)
started = time.monotonic()
stage = 0

def tick(delta):
    global stage, started
    if time.monotonic() - started < 8: return
    try:
        if stage == 0:
            component.capture_scene()
            stage = 1; started = time.monotonic()
        elif stage == 1:
            unreal.RenderingLibrary.export_render_target(world, target, str(OUT), "StatusLayers.png")
            for actor in preview_actors[:8]: actors.destroy_actor(actor)
            hail = spawn(unreal.load_class(None, FX+"BP_StellarFreezingFX.BP_StellarFreezingFX_C"), unreal.Vector())
            hail.update_area(unreal.JTSStellarWeaponMode.FREEZING, unreal.Vector(0,0,4), unreal.Vector(0,0,1), 500., unreal.LinearColor(.18,.65,1,1), True, False, False)
            capture.set_actor_location(unreal.Vector(2050,0,700), False, False)
            capture.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(capture.get_actor_location(), unreal.Vector(0,0,420)), False)
            component.capture_scene()
            stage = 2; started = time.monotonic()
        else:
            unreal.RenderingLibrary.export_render_target(world, target, str(OUT), "DownwardHail.png")
            unreal.unregister_slate_post_tick_callback(handle)
            (OUT / "result.json").write_text(json.dumps({"rendered": ["StatusLayers.png", "DownwardHail.png"], "labels_left_to_right": ["Poison+Ice+Fire", "Poison", "Ice", "Fire"]}, indent=2))
            print("STELLAR_STATUS_VISUAL_QA_COMPLETE", str(OUT))
    except Exception:
        unreal.unregister_slate_post_tick_callback(handle)
        raise

handle = unreal.register_slate_post_tick_callback(tick)
print("STELLAR_STATUS_VISUAL_QA_STARTED")
