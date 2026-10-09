"""Observe population feedback, density, real-surface slopes and motion in standard PIE."""
import json
import math
from pathlib import Path
import unreal


def start_moon_ant_ecology_validation():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world
    nests = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSMoonAntNestActor)
    assert len(nests) == 9
    planet = next(p for p in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetAnchor)
                  if str(p.get_planet_id()) == "Moon")
    root = Path(unreal.Paths.project_dir()).resolve() / "SourceArt/MoonAntNests"
    start = unreal.GameplayStatics.get_time_seconds(world)
    state = {"last_sample": -1, "snapshots": [], "samples": {}, "excess_population": 0,
             "invalid_slopes": 0, "moved_ants": set(), "loss_test": None, "density": None}
    handle = None

    def tick(delta):
        now = unreal.GameplayStatics.get_time_seconds(world) - start
        if now - state["last_sample"] < 1:
            return
        state["last_sample"] = now
        ants = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSMoonAntActor)
        counts = [nest.get_active_moon_ant_count() for nest in nests]
        state["excess_population"] += sum(n > 30 for n in counts)
        for ant in ants:
            assert ant.get_owner() in nests
            point = ant.get_actor_location()
            hit = planet.project_point_to_surface(point)
            slope = math.degrees(math.acos(max(-1, min(1, unreal.MathLibrary.dot_vector_vector(
                hit.impact_normal, planet.get_radial_up_vector(hit.impact_point))))))
            if slope > 35.05:
                state["invalid_slopes"] += 1
            previous = state["samples"].get(ant.get_name())
            if previous is not None and (point - previous).length() > 5:
                state["moved_ants"].add(ant.get_name())
            state["samples"][ant.get_name()] = point
        if not state["snapshots"] or now - state["snapshots"][-1]["seconds"] >= 10:
            state["snapshots"].append({"seconds": round(now, 2), "counts": counts})
            print("ANT_ECOLOGY_PROGRESS", state["snapshots"][-1])
        if now >= 40 and state["loss_test"] is None:
            assert min(counts) >= 28, counts
            distances = [(ant.get_actor_location() - ant.get_owner().get_actor_location()).length() for ant in ants]
            near = sum(d < 900 for d in distances)
            middle = sum(900 <= d < 2800 for d in distances)
            far = sum(d >= 2800 for d in distances)
            state["density"] = {"near": near, "middle": middle, "far": far, "maximum_distance_cm": max(distances)}
            assert near > middle > far, state["density"]
            nest = nests[0]
            before = nest.get_active_moon_ant_count()
            slow = nest.get_population_spawn_interval()
            victims = [a for a in ants if a.get_owner() == nest][:15]
            for ant in victims:
                ant.destroy_actor()
            after = nest.get_active_moon_ant_count()
            fast = nest.get_population_spawn_interval()
            assert before-after == len(victims) and fast < slow
            state["loss_test"] = {"nest": nest.get_name(), "before": before, "after_loss": after,
                                  "slow_interval_seconds": slow, "fast_interval_seconds": fast}
        # Include the first complete 90–150 second natural activity cycle as well as forced losses.
        if now < 160:
            return
        unreal.unregister_slate_post_tick_callback(handle)
        assert min(counts) >= 28 and max(counts) <= 30, counts
        assert state["excess_population"] == 0 and state["invalid_slopes"] == 0, state
        assert len(state["moved_ants"]) > 100
        state["loss_test"]["recovered_count"] = nests[0].get_active_moon_ant_count()
        result = {"passed": True, "nest_count": 9, "final_counts": counts,
                  "snapshots": state["snapshots"], "density": state["density"], "loss_test": state["loss_test"],
                  "excess_population_samples": 0, "invalid_slope_samples": 0,
                  "distinct_moving_ants": len(state["moved_ants"])}
        (root / "moon_ant_ecology_pie_validation.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        print("ANT_ECOLOGY_PASSED", result)

    def guarded_tick(delta):
        if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() != world:
            unreal.unregister_slate_post_tick_callback(handle)
            return
        try:
            tick(delta)
        except Exception as error:
            unreal.unregister_slate_post_tick_callback(handle)
            (root / "moon_ant_ecology_pie_validation.json").write_text(
                json.dumps({"passed": False, "error": str(error)}, indent=2), encoding="utf-8")
            raise

    handle = unreal.register_slate_post_tick_callback(guarded_tick)
    return handle


moon_ant_ecology_validation_handle = start_moon_ant_ecology_validation()
