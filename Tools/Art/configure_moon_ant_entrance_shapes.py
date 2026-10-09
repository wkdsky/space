"""Assign and save deterministic, varied shape parameters to the existing crater entrances."""
import json
import random
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve() / "SourceArt/MoonAntNests"
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert not editor.get_game_world(), "Stop PIE before changing authored shapes"
assert editor.get_editor_world().get_path_name() == "/Game/Space/Maps/L_SpaceWorld.L_SpaceWorld"
actors = {a.get_actor_label(): a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
plan = json.loads((root / "moon_ant_nest_placements.json").read_text())
foot = json.loads((root / "moon_crater_foot.json").read_text())
edges = {tuple(sorted(e["vertices"])): e for e in foot["edges"]}
moon_material = actors["MoonPlanet"].static_mesh_component.get_material(0)
void_material = unreal.load_asset("/Game/Space/Materials/Moon/M_MoonAntNestVoid")
assert moon_material and void_material
blueprint = unreal.load_asset("/Game/Space/Blueprints/Planets/Moon/BP_MoonAntNestEntrance")
defaults = unreal.get_default_object(blueprint.generated_class()).get_editor_property("entrance_shape")
defaults.set_editor_property("void_material", void_material)
defaults.set_editor_property("rim_material", moon_material)
unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)

seed = 20261009
randomizer = random.Random(seed)
profiles = [unreal.JTSNestEntranceProfile.IRREGULAR_ARCH] * 3 + [
    unreal.JTSNestEntranceProfile.OLIVE, unreal.JTSNestEntranceProfile.SLIT,
    unreal.JTSNestEntranceProfile.LEANING,
] * 2
assert len(profiles) == len(plan["placements"])
randomizer.shuffle(profiles)
rows = []
with unreal.ScopedEditorTransaction("Randomize Moon ant entrance shapes"):
    for item, profile in zip(plan["placements"], profiles):
        nest = actors[item["name"]]
        original_transform = nest.get_actor_transform()
        component = nest.get_editor_property("entrance_shape")
        wall = edges[tuple(sorted(item["edge_vertices"]))]["wall_normal"]
        dot = lambda a, b: sum(x * y for x, y in zip(a, b))
        forward_dot = dot(wall, item["forward_local"])
        assert forward_dot > 0.5
        component.modify()
        nest.modify()
        component.set_editor_property("wall_slope", dot(wall, item["up_local"]) / forward_dot)
        component.set_editor_property("rim_width", randomizer.uniform(1.2, 2.2) if profile == unreal.JTSNestEntranceProfile.SLIT else randomizer.uniform(4.0, 6.5))
        component.set_editor_property("void_material", void_material)
        component.set_editor_property("rim_material", moon_material)
        component.randomize_shape_with_seed(randomizer.randrange(1, 2147483647), profile)
        assert nest.get_actor_transform() == original_transform
        shape = component.get_editor_property("shape")
        values = {name: shape.get_editor_property(name) for name in (
            "width", "height", "profile_power", "floor_roundness", "lean", "irregularity", "seed")}
        rows.append({"name": item["name"], "actor": nest.get_path_name(), "profile": str(profile),
                     "wall_slope": component.get_editor_property("wall_slope"),
                     "rim_width": component.get_editor_property("rim_width"), "shape": values})
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
report = {"assignment_seed": seed, "count": len(rows), "randomized_once_and_saved": True, "nests": rows}
(root / "moon_ant_entrance_shapes.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print("MOON_ANT_ENTRANCE_SHAPES", json.dumps(report))
