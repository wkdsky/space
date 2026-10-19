"""Run through Unreal MCP in the open L_SpaceWorld editor (or its PIE world)."""
import json
import runpy
from pathlib import Path
import unreal

ROOT = Path('D:/projects/space')
OUT = ROOT / 'Saved/SpaceDebug'
OUT.mkdir(parents=True, exist_ok=True)


def configure():
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    all_actors = actors.get_all_level_actors()
    manager = next(a for a in all_actors if isinstance(a, unreal.JTSSpaceWorldManager))
    deimos = next(a for a in all_actors if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'MarII')
    backup = OUT / 'L_SpaceWorld_before_debug.umap'
    if not backup.exists():
        backup.write_bytes((ROOT / 'Content/Space/Maps/L_SpaceWorld.umap').read_bytes())
    manager.set_editor_property('initial_planet', deimos)
    manager.set_editor_property('initial_planet_id', 'MarII')
    debug = next((a for a in all_actors if isinstance(a, unreal.JTSSpaceDebugTravelManager)), None)
    if not debug:
        debug = actors.spawn_actor_from_class(unreal.JTSSpaceDebugTravelManager, unreal.Vector(0, 0, 0))
    debug.set_actor_label('Space_DebugTravel')
    debug.set_folder_path('Debug')
    entries = []
    for planet_id, label in [('Moon', '月球 / Moon'), ('MarII', '火卫二 / Deimos')]:
        entry = unreal.JTSDebugTravelDestination()
        entry.set_editor_property('planet_id', planet_id)
        entry.set_editor_property('display_name', unreal.Text(label))
        entries.append(entry)
    debug.set_editor_property('destinations', entries)
    level.save_current_level()
    report = {'initial_planet': str(manager.get_editor_property('initial_planet_id')),
              'debug_actor': debug.get_path_name(), 'destinations': [str(e.planet_id) for e in entries]}
    (OUT / 'configuration.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    return json.dumps(report, ensure_ascii=False)


def pie():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world, 'Start PIE first'
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    ships = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpacecraftActor)
    debug = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceDebugTravelManager)[0]
    return world, pc, ships, debug


def inspect():
    world, pc, ships, debug = pie()
    pawn = pc.get_controlled_pawn()
    manager = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceWorldManager)[0]
    def xyz(v): return [v.x, v.y, v.z]
    inventory = pawn.get_inventory_component() if isinstance(pawn, unreal.JTSCharacter) else None
    report = {'pawn': pawn.get_path_name() if pawn else None,
              'player_location': xyz(pawn.get_actor_location()) if pawn else None,
              'current_planet': str(manager.get_current_planet().get_planet_id()) if manager.get_current_planet() else None,
              'surface_ready': manager.is_surface_gameplay_ready(),
              'default_planet': str(debug.get_editor_property('DefaultPlanetId')),
              'status': str(debug.get_editor_property('Status')),
              'cursor': pc.get_editor_property('show_mouse_cursor'),
              'selected_slot': inventory.get_selected_quickbar_slot() if inventory else None,
              'inventory': [{'id': str(inventory.get_item_at_slot(i).item_id), 'count': inventory.get_item_at_slot(i).stack_count}
                            for i in range(inventory.get_inventory_capacity())] if inventory else [],
              'ships': [{'path': s.get_path_name(), 'location': xyz(s.get_actor_location()),
                         'landed': s.is_landed(), 'planet': str(s.get_landed_planet().get_planet_id()) if s.get_landed_planet() else None,
                         'occupants': len(s.get_occupants())} for s in ships]}
    pickups = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSWorldPickupActor)
    report['pickup_count'] = len(pickups)
    report['nearby_pickups'] = [{'location': xyz(a.get_actor_location())}
                              for a in pickups if (a.get_actor_location() - pawn.get_actor_location()).length() < 1500]
    report['crew'] = [{'controller': c.get_name(), 'pawn': c.get_controlled_pawn().get_name(),
                       'location': xyz(c.get_controlled_pawn().get_actor_location())}
                      for c in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlayerController) if c.get_controlled_pawn()]
    (OUT / 'last_pie.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    return json.dumps(report, ensure_ascii=False)


def inspect_sky():
    world, pc, ships, debug = pie()
    result = {'current_planet': str(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceWorldManager)[0].get_current_planet().get_planet_id()), 'bodies': []}
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetAnchor):
        surface = actor.get_editor_property('gameplay_surface_actor')
        result['bodies'].append({'id': str(actor.get_planet_id()), 'hidden': actor.get_editor_property('hidden'), 'surface_hidden': surface.get_editor_property('hidden') if surface else None, 'scale': str(surface.get_actor_scale3d()) if surface else None})
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
        if actor.actor_has_tag('CelestialBackground'):
            result['bodies'].append({'id': actor.get_name(), 'hidden': actor.get_editor_property('hidden'), 'scale': str(actor.get_actor_scale3d()), 'collision': actor.get_actor_enable_collision()})
    (OUT / 'sky_runtime.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
    return json.dumps(result, ensure_ascii=False)


def run(operation):
    if operation.startswith('mars_access_'): return runpy.run_path(str(ROOT / 'Tools/Editor/configure_mars_access.py'))['run'](operation[len('mars_access_'):])
    if operation.startswith('celestial_coherence_'): return runpy.run_path(str(ROOT / 'Tools/Editor/validate_celestial_coherence.py'))['run'](operation[len('celestial_coherence_'):])
    if operation.startswith('multi_body_'): return runpy.run_path(str(ROOT / 'Tools/Editor/validate_multi_body_flight.py'))['run'](operation[len('multi_body_'):])
    if operation.startswith('earth_moon_'): return runpy.run_path(str(ROOT / 'Tools/Editor/validate_earth_moon_sky.py'))['run'](operation[len('earth_moon_'):])
    if operation.startswith('antigravity_'): return runpy.run_path(str(ROOT / 'Tools/Editor/configure_antigravity_flight.py'))['run'](operation[len('antigravity_'):])
    if operation.startswith('independent_'): return runpy.run_path(str(ROOT / 'Tools/Editor/configure_independent_space.py'))['run'](operation[len('independent_'):])
    if operation == 'navigation_quit_editor':
        unreal.SystemLibrary.quit_editor()
        return 'editor shutdown requested'
    if operation == 'configure_navigation': return runpy.run_path(str(ROOT / 'Tools/Editor/configure_celestial_navigation.py'))['configure']()
    if operation == 'inspect_navigation': return runpy.run_path(str(ROOT / 'Tools/Editor/configure_celestial_navigation.py'))['inspect']()
    if operation.startswith('navigation_view_'): return runpy.run_path(str(ROOT / 'Tools/Editor/configure_celestial_navigation.py'))['view_setup'](operation[len('navigation_view_'):])
    if operation == 'inspect_phobos': return runpy.run_path(str(ROOT / 'Tools/Editor/inspect_phobos_sky.py'))['run']()
    if operation == 'probe_phobos_flight': return runpy.run_path(str(ROOT / 'Tools/Editor/probe_mars_local_flight.py'))['run']('phobos_flight.json', True)
    if operation == 'capture_phobos': return runpy.run_path(str(ROOT / 'Tools/Editor/capture_space_sky.py'))['run']('MarII', 'Phobos_Before.png', 'MarI')
    if operation == 'capture_phobos_detail': return runpy.run_path(str(ROOT / 'Tools/Editor/capture_space_sky.py'))['run']('MarII', 'Phobos_Detail.png', 'MarI', 2)
    if operation == 'capture_phobos_after': return runpy.run_path(str(ROOT / 'Tools/Editor/capture_space_sky.py'))['run']('MarII', 'Phobos_After.png', 'MarI')
    if operation == 'capture_phobos_flight': return runpy.run_path(str(ROOT / 'Tools/Editor/capture_space_sky.py'))['run']('MarII', 'Phobos_Flight.png', 'MarI')
    if operation == 'probe_mars_flight': return runpy.run_path(str(ROOT / 'Tools/Editor/probe_mars_local_flight.py'))['run']()
    if operation == 'stop_mars_probe':
        unreal._mars_local_flight_probe.finish('stopped')
        return 'stopped'
    if operation == 'capture_mars_flight': return runpy.run_path(str(ROOT / 'Tools/Editor/capture_space_sky.py'))['run']('MarII', 'Mars_Flight.png')
    if operation == 'capture_sky': return runpy.run_path(str(ROOT / 'Tools/Editor/capture_space_sky.py'))['run']()
    if operation == 'configure': return configure()
    if operation == 'inspect': return inspect()
    if operation == 'inspect_sky': return inspect_sky()
    if operation == 'inspect_sky_editor':
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        result = []
        for actor in actors:
            if isinstance(actor, unreal.JTSPlanetAnchor):
                row = {'name': actor.get_actor_label(), 'path': actor.get_path_name(), 'center': str(actor.get_planet_center())}
                for prop in ['planet_id', 'parent_planet_id', 'celestial_companion', 'gameplay_surface_actor', 'approximate_radius', 'heliocentric_distance_kilometers', 'companion_apparent_separation_centimeters']:
                    row[prop] = str(actor.get_editor_property(prop))
                result.append(row)
        return json.dumps(result, ensure_ascii=False)
    world, pc, ships, debug = pie()
    if operation == 'inspect_sky_geometry':
        return json.dumps([{'id': str(a.get_planet_id()), 'center': str(a.get_planet_center()),
            'center_actor': str(a.get_editor_property('center_actor')),
            'query_component': str(a.get_gameplay_surface_component()),
            'surface': str(a.get_gameplay_surface_actor()),
            'location': str(a.get_gameplay_surface_actor().get_actor_location()),
            'components': [{'name': c.get_name(), 'mobility': str(c.get_editor_property('mobility'))}
                for c in a.get_gameplay_surface_actor().get_components_by_class(unreal.SceneComponent)]}
            for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetAnchor)])
    if operation == 'sky_earth_view':
        # PIE-only observer setup; not a runtime travel destination or authored level change.
        manager = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceWorldManager)[0]
        earth = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetAnchor) if str(a.get_planet_id()) == 'EarthFragments')
        arrival = next((a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetArrivalAnchor) if a.get_planet_anchor() == earth), None)
        if arrival:
            ships[0].set_actor_transform(arrival.get_spacecraft_arrival_transform(), False, True)
        else:
            ships[0].set_actor_location(earth.get_planet_center() + unreal.Vector(0, 0, earth.get_approximate_radius() + 1000), False, True)
        ships[0].set_grounded_planet(earth)
        manager.set_current_planet(earth)
        return json.dumps({'observer_planet': str(earth.get_planet_id()), 'source': arrival.get_name() if arrival else 'near-surface observer'})
    if operation == 'add_crew':
        for _ in range(3): unreal.GameplayStatics.create_player(world, -1, True)
        return inspect()
    if operation == 'nonhost_travel':
        controllers = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlayerController)
        assert len(controllers) > 1
        controllers[1].call_method('ServerRequestDebugPlanet', ('Moon', False))
        return inspect()
    if operation == 'board':
        pc.call_method('ServerRequestBoardSpacecraft', (ships[0],))
        return inspect()
    if operation == 'disembark':
        pc.call_method('ServerRequestDisembarkSpacecraft', (ships[0],))
        return inspect()
    if operation == 'ui':
        result = []
        hud = pc.get_hud()
        for obj, props in [(debug, ['DefaultPlanetId', 'Status']), (hud, ['PrototypeWidget'])]:
            row = {'object': str(obj)}
            for prop in props:
                try: row[prop] = str(obj.get_editor_property(prop))
                except Exception as ex: row[prop] = str(ex)
            result.append(row)
        for cls in [unreal.JTSSpaceDebugWidget, unreal.JTSPrototypeHUDWidget]:
            for widget in unreal.WidgetLibrary.get_all_widgets_of_class(world, cls, False):
                result.append({'widget': widget.get_name(), 'visibility': str(widget.get_visibility()),
                               'opacity': widget.get_render_opacity(), 'geometry': str(widget.get_cached_geometry())})
        return json.dumps(result, ensure_ascii=False)
    actions = {'travel_moon': ('Moon', False), 'travel_deimos': ('MarII', False),
               'default_moon': ('Moon', True), 'default_deimos': ('MarII', True),
               'invalid_travel': ('Mars', False)}
    if operation in actions:
        pc.call_method('ServerRequestDebugPlanet', actions[operation])
        return inspect()
    raise ValueError(operation)
