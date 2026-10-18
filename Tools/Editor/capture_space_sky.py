"""Transient PIE capture of the existing local-sky presentation, invoked through Unreal MCP."""
import json
from pathlib import Path
import unreal


def run(planet_id_override=None, output_name=None, target_tag_override=None, fov_angle=90):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world
    manager = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceWorldManager)[0]
    planet = manager.get_current_planet()
    ship = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpacecraftActor)[0]
    bodies = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetAnchor)
    if planet_id_override:
        planet = next(a for a in bodies if str(a.get_planet_id()) == planet_id_override)
    planet_id = str(planet.get_planet_id())
    if planet_id == 'MarII':
        target = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor) if a.actor_has_tag('Mars'))
        target_location = target.get_actor_bounds(False)[0]
    else:
        target = next(a for a in bodies if str(a.get_planet_id()) == ('Moon' if planet_id == 'EarthFragments' else 'EarthFragments'))
        target_location = target.get_planet_center()
    if target_tag_override:
        target = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor) if a.actor_has_tag(target_tag_override))
        target_location = target.get_actor_bounds(False)[0]
    up = (ship.get_actor_location() - planet.get_planet_center()).normal()
    location = ship.get_actor_location() + up * 500
    if planet_id == 'EarthFragments':
        up = (target_location - planet.get_planet_center()).normal()
        location = planet.get_planet_center() + up * (planet.get_approximate_radius() + 10000)
    forward = (target_location - location).normal()
    view_up = (up - forward * unreal.MathLibrary.dot_vector_vector(up, forward)).normal()
    if view_up.length() < 0.01:
        view_up = (unreal.Vector(0, 0, 1) - forward * forward.z).normal()
    right = unreal.MathLibrary.cross_vector_vector(view_up, forward)
    pose = unreal.Transform(location=location, rotation=unreal.MathLibrary.make_rotation_from_axes(forward, right, view_up))
    lib = unreal.get_default_object(unreal.GameplayStatics.static_class())
    actor = lib.call_method('BeginDeferredActorSpawnFromClass', (world, unreal.SceneCapture2D.static_class(), pose, unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN, None, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    actor = lib.call_method('FinishSpawningActor', (actor, pose, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))

    class Capture:
        def __init__(self):
            self.actor = actor
            self.component = actor.capture_component2d
            self.texture = unreal.RenderingLibrary.create_render_target2d(world, 1600, 900, unreal.TextureRenderTargetFormat.RTF_RGBA8)
            self.component.set_editor_property('texture_target', self.texture)
            self.component.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
            self.component.set_editor_property('fov_angle', fov_angle)
            self.component.set_editor_property('always_persist_rendering_state', True)
            self.component.set_editor_property('hidden_actors', [ship] + list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSCharacter)))
            self.started = unreal.GameplayStatics.get_time_seconds(world)
            self.handle = unreal.register_slate_post_tick_callback(self.tick)

        def tick(self, delta):
            if unreal.GameplayStatics.get_time_seconds(world) - self.started < 3:
                return
            try:
                output = Path(unreal.Paths.project_saved_dir()) / 'SpaceDebug'
                unreal.RenderingLibrary.export_render_target(world, self.texture, str(output), output_name or ('Sky_' + planet_id + '.png'))
            finally:
                self.actor.destroy_actor()
                unreal.unregister_slate_post_tick_callback(self.handle)

    unreal._space_sky_capture = Capture()
    return json.dumps({'planet': planet_id, 'camera': str(location), 'target': str(target_location), 'fov': fov_angle})
