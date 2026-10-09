"""Standard PIE smoke check: shared Mass ownership and server presentation-only ant Actors."""
import json
from pathlib import Path
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, 'Start normal L_SpaceWorld PIE first'
# This engine-internal library is intentionally omitted from ordinary Python wrappers.
library = unreal.get_default_object(unreal.load_class(None, '/Script/Engine.SubsystemBlueprintLibrary'))
enemies = library.call_method('GetWorldSubsystem', args=(
    world, unreal.load_class(None, '/Script/space.JTSPlanetEnemySubsystem')))
start = unreal.GameplayStatics.get_time_seconds(world)
report = {'samples': [], 'last_sample': -1, 'positions': {}, 'moving': set()}
handle = None

def tick(delta):
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() != world:
        unreal.unregister_slate_post_tick_callback(handle)
        return
    seconds = unreal.GameplayStatics.get_time_seconds(world) - start
    if seconds - report['last_sample'] < 1:
        return
    report['last_sample'] = seconds
    ants = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSMoonAntActor)
    nests = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSMoonAntNestActor)
    registered = enemies.get_registered_ant_count()
    assert len(nests) == 9
    assert registered == len(ants), (registered, len(ants))
    for ant in ants:
        assert ant.has_mass_entity() and not ant.is_actor_tick_enabled(), ant.get_name()
        assert ant.get_component_by_class(unreal.JTSStellarTargetComponent)
        point = ant.get_actor_location()
        previous = report['positions'].get(ant.get_name())
        if previous is not None and (previous - point).length() > 5:
            report['moving'].add(ant.get_name())
        report['positions'][ant.get_name()] = point
    report['samples'].append({'seconds': round(seconds, 2), 'actors': len(ants), 'entities': registered,
                              'counts': [n.get_active_moon_ant_count() for n in nests],
                              'host_tick_ms': enemies.get_last_tick_milliseconds()})
    if seconds < 45:
        return
    unreal.unregister_slate_post_tick_callback(handle)
    assert len(ants) >= 252 and len(report['moving']) > 200
    result = {'passed': True, 'server_actor_ticks': 0, 'distinct_moving_ants': len(report['moving']),
              'samples': report['samples'], 'shared_entity_host': 'JTSPlanetEnemySubsystem'}
    root = Path(unreal.Paths.project_dir()) / 'SourceArt/MoonAntNests'
    (root / 'moon_ant_ecs_pie_validation.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print('ANT_ECS_PIE_PASSED', result)

def guarded_tick(delta):
    try:
        tick(delta)
    except Exception as error:
        unreal.unregister_slate_post_tick_callback(handle)
        root = Path(unreal.Paths.project_dir()) / 'SourceArt/MoonAntNests'
        (root / 'moon_ant_ecs_pie_validation.json').write_text(
            json.dumps({'passed': False, 'error': str(error)}, indent=2), encoding='utf-8')
        raise

handle = unreal.register_slate_post_tick_callback(guarded_tick)
