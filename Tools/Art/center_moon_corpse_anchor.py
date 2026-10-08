"""Move the authored corpse anchor to the fitted crater's center and real collision surface."""
import unreal
import json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()/"SourceArt/SkeletonAstronaut"
fit=json.loads((root/"moon_crater_fit.json").read_text())
actors={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
anchor=actors["Moon_CorpseAnchor_A"]
mesh=actors["MoonPlanet"].static_mesh_component
point=unreal.Vector(*[c*100 for c in fit["center_surface_local_m"]])
world_point=mesh.get_world_transform().transform_location(point)
anchor.modify()
anchor.set_actor_location(world_point,False,False)
assert anchor.snap_to_planet_surface()
print("CENTERED_ANCHOR",anchor.get_actor_location(),anchor.get_actor_rotation())
