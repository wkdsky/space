"""Check saved/runtime geometry, surface fit, deterministic rebuilding, and visibility traces."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve() / "SourceArt/MoonAntNests"
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world() or editor.get_editor_world()
is_pie = bool(editor.get_game_world())
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSMoonAntNestActor)
saved = json.loads((root / "moon_ant_entrance_shapes.json").read_text())
plan = json.loads((root / "moon_ant_nests_authored.json").read_text())
assert len(actors) == saved["count"] == 9
rows = []
for expected in saved["nests"]:
    actor_name = expected["actor"].rsplit(".", 1)[-1]
    nest = next(n for n in actors if n.get_name() == actor_name)
    component = nest.get_editor_property("entrance_shape")
    fallback = nest.get_editor_property("nest_mesh")
    assert component.get_editor_property("enabled")
    assert component.is_visible() and not fallback.is_visible()
    assert fallback.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    assert component.get_collision_enabled() == unreal.CollisionEnabled.QUERY_ONLY
    shape = component.get_editor_property("shape")
    for key, value in expected["shape"].items():
        actual = shape.get_editor_property(key)
        assert abs(actual - value) < 0.0001, (expected["name"], key, actual, value)
    original = next(p for p in plan["nests"] if p["name"] == expected["name"])
    assert (nest.get_actor_location() - unreal.Vector(*original["location_cm"])).length() < 0.1
    slope = component.get_editor_property("wall_slope")
    wall_normal = unreal.MathLibrary.normal(unreal.Vector(1, 0, slope))
    vertices, triangles, normals, uvs, tangents = unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(component, 0)
    assert vertices and triangles
    signature = [(v.x, v.y, v.z) for v in vertices]
    max_gap = 0
    for v in vertices:
        gap = min(abs(unreal.MathLibrary.dot_vector_vector(v, wall_normal)), abs(v.z))
        max_gap = max(max_gap, gap)
    assert max_gap < 0.251, (expected["name"], max_gap)
    for i in range(0, len(triangles), 3):
        a, b, c = [vertices[triangles[i + j]] for j in range(3)]
        assert unreal.MathLibrary.cross_vector_vector(b - a, c - a).length() > 0.001
    component.rebuild_entrance()
    rebuilt = unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(component, 0)[0]
    assert signature == [(v.x, v.y, v.z) for v in rebuilt]
    # Cast toward a point inside a real mouth triangle, not the hidden old arch.
    target = (vertices[0] + vertices[1] + vertices[2]) / 3
    transform = component.get_world_transform()
    target = transform.transform_location(target)
    normal = transform.rotation.rotate_vector(wall_normal)
    result = unreal.SystemLibrary.line_trace_single(world, target + normal * 30, target - normal * 3,
        unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, False)
    assert result and result.to_tuple()[9] == nest and result.to_tuple()[10] == component, expected["name"]
    rows.append({"name": expected["name"], "profile": expected["profile"],
                 "mouth_triangles": len(triangles) // 3, "maximum_surface_gap_cm": max_gap,
                 "stable_rebuild": True, "visibility_trace_hits_nest": True})
if is_pie:
    controller = next(c for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSMoonSurfaceController)
                      if str(c.get_planet_id()) == "Moon")
    assert controller.is_surface_gameplay_initialized()
    assert set(controller.get_editor_property("placed_moon_ant_nests")) == set(actors)
    assert any(n.get_active_moon_ant_count() for n in actors)
report = {"mode": "PIE" if is_pie else "Editor", "count": len(rows), "profiles": len(set(r["profile"] for r in rows)), "nests": rows}
(root / ("moon_ant_entrance_shapes_pie_validation.json" if is_pie else "moon_ant_entrance_shapes_validation.json")).write_text(
    json.dumps(report, indent=2), encoding="utf-8")
print("MOON_ANT_SHAPE_VALIDATION", json.dumps(report))
