"""Move the existing corpse to the detected inner-foot circle centre, retaining real-mesh grounding."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve() / "SourceArt/MoonAntNests"
foot = json.loads((root / "moon_crater_foot.json").read_text())
subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = {a.get_actor_label(): a for a in subsystem.get_all_level_actors()}
corpse = actors["Moon_Corpse_SkeletonAstronaut"]
moon = actors["MoonPlanet"].static_mesh_component
planet = actors["BP_MoonSurfaceController"].get_owning_planet()
if not planet:
    planet = next(a for a in actors.values() if isinstance(a, unreal.JTSPlanetAnchor)
                  and a.get_editor_property("gameplay_surface_actor") == actors["MoonPlanet"])
center = moon.get_world_transform().transform_location(unreal.Vector(*foot["foot_center_local_cm"]))
anchor = subsystem.spawn_actor_from_class(unreal.JTSPlanetSurfaceAnchor, center, corpse.get_actor_rotation())
try:
    anchor.set_editor_property("planet_anchor", planet)
    anchor.set_editor_property("surface_clearance", 0)
    corpse.modify()
    assert corpse.snap_to_planet_surface_anchor(anchor)
    surface = anchor.get_surface_transform()
    normal = moon.get_world_transform().rotation.rotate_vector(unreal.Vector(*foot["foot_normal"]))
    offset = surface.translation - center
    tangent_error = (offset - normal * unreal.MathLibrary.dot_vector_vector(offset, normal)).length()
    assert tangent_error < 10, tangent_error
    report = {
        "ring_center_world_cm": [center.x, center.y, center.z],
        "ground_center_world_cm": [surface.translation.x, surface.translation.y, surface.translation.z],
        "corpse_location_cm": [corpse.get_actor_location().x, corpse.get_actor_location().y, corpse.get_actor_location().z],
        "ring_center_tangent_error_cm": tangent_error,
        "clearance_cm": corpse.get_editor_property("planet_surface_clearance"),
    }
    (root / "moon_corpse_basin_center.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("CORPSE_BASIN_CENTER", report)
finally:
    assert subsystem.destroy_actor(anchor)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
