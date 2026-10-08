"""Check the existing Moon nest generator around the placed corpse in PIE."""
import json
from pathlib import Path
import unreal

world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, "Start PIE before checking runtime nests"
corpses=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSMoonCorpseActor)
assert len(corpses)==1
corpse=corpses[0]
controller=next(c for c in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSMoonSurfaceController)
                if str(c.get_planet_id())=="Moon")
assert controller.is_surface_gameplay_initialized()
planet=controller.get_owning_planet()
data=unreal.load_asset("/Game/Space/Data/Planets/Moon/PDA_MoonSurfaceGameplay")
assert data and data.get_editor_property("moon_ant_nest_outer_radius_around_corpse")==600.0
center=corpse.get_actor_location()
nests=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSMoonAntNestActor)
assert len(nests)==data.get_editor_property("moon_ant_nest_count")==5
rows=[]
for nest in nests:
    assert nest.get_owner()==corpse
    point=nest.get_actor_location()
    distance=(point-center).length()
    assert distance<=650.0, (nest.get_name(),distance)
    rows.append({"actor":nest.get_name(),"location_cm":[point.x,point.y,point.z],
                 "distance_from_corpse_cm":distance,"owner_is_corpse":True})
ants=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSMoonAntActor)
assert ants, "Wait for the existing nest spawn timers"
report={"corpse_count":1,"surface_gameplay_initialized":True,"nest_count":len(nests),
        "configured_outer_radius_cm":600.0,"observed_live_ant_count":len(ants),"nests":rows}
root=Path(unreal.Paths.project_dir()).resolve()/"SourceArt/SkeletonAstronaut"
(root/"moon_corpse_nest_validation.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
print("MOON_CORPSE_NEST_VALIDATION",json.dumps(report))
