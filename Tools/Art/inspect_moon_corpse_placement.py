"""Read the authored Moon corpse anchor and export the actual planet mesh for placement QA."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve() / "SourceArt/SkeletonAstronaut"
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
by_label = {a.get_actor_label(): a for a in actors}
anchor = by_label["Moon_CorpseAnchor_A"]
planet = anchor.get_planet_anchor()
moon = by_label["MoonPlanet"]
controller = by_label["BP_MoonSurfaceController"]
component = moon.static_mesh_component
mesh = component.static_mesh
def v(value):
    return [value.x, value.y, value.z]
def t(value):
    q = value.rotation
    return {"location": v(value.translation), "rotation_xyzw": [q.x, q.y, q.z, q.w], "scale": v(value.scale3d)}
info = {
    "anchor": anchor.get_path_name(), "anchor_transform": t(anchor.get_actor_transform()),
    "anchor_mesh_local": v(component.get_world_transform().inverse_transform_location(anchor.get_actor_location())),
    "resolved_surface_transform": t(anchor.get_surface_transform()),
    "planet": planet.get_path_name(), "planet_location": v(planet.get_actor_location()),
    "planet_mesh": mesh.get_path_name(), "planet_mesh_transform": t(component.get_world_transform()),
    "controller": controller.get_path_name(),
    "controller_anchor": controller.get_editor_property("corpse_surface_anchor").get_path_name(),
    "controller_corpse_class": controller.get_editor_property("moon_corpse_class").get_path_name(),
    "corpse_actors": [a.get_path_name() for a in actors if isinstance(a, unreal.JTSMoonCorpseActor)],
}
task = unreal.AssetExportTask()
task.object = mesh
task.filename = str(root / "MoonPlanet_placement_reference.fbx")
task.automated = True
task.prompt = False
task.replace_identical = True
task.exporter = unreal.StaticMeshExporterFBX()
assert unreal.Exporter.run_asset_export_task(task)
(root / "moon_placement_before.json").write_text(json.dumps(info, indent=2), encoding="utf-8")
print(json.dumps(info))
