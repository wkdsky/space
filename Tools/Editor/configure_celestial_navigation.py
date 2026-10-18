"""Level-authored names, fixed ship marker distances, and background exclusion via Unreal MCP."""
import json
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_saved_dir()) / 'SpaceDebug'
OUT.mkdir(parents=True, exist_ok=True)
# Estimated once at 90-degree horizontal FOV / 16:9 using the existing flight scale profile.
# Runtime never compares projected size with a threshold.
MARKER_DISTANCES_CM = {'Moon': 265000.0, 'MarII': 145000.0, 'EarthFragments': 150000.0}


def configure():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE first'
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    anchors = {str(a.get_planet_id()): a for a in actors if isinstance(a, unreal.JTSPlanetAnchor)}
    for key, name, palette in [('Moon', '月球', 0), ('MarII', '火卫二', 1), ('EarthFragments', '地球', 2)]:
        anchors[key].set_editor_property('navigation_display_name', unreal.Text(name))
        anchors[key].set_editor_property('navigation_palette_index', palette)
        anchors[key].set_editor_property('navigation_marker_distance', MARKER_DISTANCES_CM[key])
    entries = []
    for tag, name, palette in [('Mars', '火星', 1), ('MarI', '火卫一', 2)]:
        actor = next(a for a in actors if a.actor_has_tag(tag))
        box = actor.static_mesh_component.static_mesh.get_bounding_box()
        extent = (box.max - box.min) * 0.5
        radius = max(extent.x, extent.y, extent.z) if tag == 'Mars' else (extent.x * extent.y * extent.z) ** (1.0 / 3.0)
        entry = unreal.JTSCelestialBackgroundNavigation()
        entry.set_editor_property('actor', actor)
        entry.set_editor_property('display_name', unreal.Text(name))
        entry.set_editor_property('palette_index', palette)
        entry.set_editor_property('local_visual_radius', radius)
        entry.set_editor_property('show_on_star_chart', False)
        entry.set_editor_property('show_screen_marker', False)
        entries.append(entry)
    anchors['MarII'].set_editor_property('background_navigation', entries)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    report = {'anchors': {key: str(a.get_editor_property('navigation_display_name')) for key, a in anchors.items()},
              'marker_distances_cm': {key: a.get_editor_property('navigation_marker_distance') for key, a in anchors.items()},
              'backgrounds': [{'name': str(e.display_name), 'actor': e.actor.get_path_name(), 'local_radius': e.local_visual_radius,
                              'star_chart': e.show_on_star_chart, 'screen_marker': e.show_screen_marker} for e in entries]}
    (OUT / 'celestial_navigation_config.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    return json.dumps(report, ensure_ascii=False)


def inspect():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world
    ship = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpacecraftActor)[0]
    manager = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceWorldManager)[0]
    bodies = unreal.JTSCelestialNavigationLibrary.gather_bodies(manager, ship)
    widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.JTSPrototypeHUDWidget, False)
    result = {'cruise': manager.is_cruise_presentation_active(ship), 'bodies': [], 'players': []}
    for b in bodies:
        result['bodies'].append({'name': str(b.display_name), 'radius_cm': b.visual_radius,
            'center': [b.center.x, b.center.y, b.center.z], 'range_cm': b.range_centimeters,
            'visible': b.visible, 'gameplay': bool(b.planet),
            'ship_center_distance_cm': (ship.get_actor_location() - b.center).length(),
            'marker_trigger_range_cm': b.ship_distance_centimeters,
            'marker_distance_cm': b.marker_distance_centimeters,
            'within_marker_distance': unreal.JTSCelestialNavigationLibrary.is_within_marker_distance(b),
            'star_chart': b.show_on_star_chart, 'screen_marker': b.show_screen_marker})
    for widget in widgets:
        marker = widget.get_editor_property('CelestialMarkerWidget')
        nav = widget.get_editor_property('CruiseNavigationWidget')
        owner = widget.get_owning_player()
        result['players'].append({'controller': owner.get_name(), 'pawn': str(owner.get_controlled_pawn()),
            'marker_visibility': str(marker.get_visibility()), 'navigation_visibility': str(nav.get_visibility()),
            'markers': [{'name': str(m.display_name), 'fraction': m.screen_diameter_fraction,
                         'min': [m.min.x, m.min.y], 'max': [m.max.x, m.max.y]} for m in marker.get_editor_property('markers')]})
    (OUT / 'celestial_navigation_pie.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
    return json.dumps(result, ensure_ascii=False)


def view_setup(mode):
    """PIE-only repeatable observer poses; never changes authored transforms or runtime gameplay code."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world
    ship = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpacecraftActor)[0]
    manager = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceWorldManager)[0]
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    if mode.startswith('travel_'):
        pc.call_method('ServerRequestDebugPlanet', (mode[len('travel_'):], False))
        return 'PIE-only debug travel'
    if mode.startswith('range_'):
        _, key, distance = mode.split('_')
        anchors = {str(a.get_planet_id()): a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetAnchor)}
        target = anchors[key]
        unreal._celestial_navigation_qa_target = key
        others = [a for k, a in anchors.items() if k != key]
        # Probe away from the plane of other systems so another landing shell cannot change the reference mid-calibration.
        direction = unreal.MathLibrary.cross_vector_vector(
            others[0].get_planet_center() - target.get_planet_center(),
            others[1].get_planet_center() - target.get_planet_center()).normal()
        physical_distance=float(distance)
        c=manager.get_editor_property('compressed_space')
        if c.get_editor_property('bodies') and target.get_editor_property('navigation_marker_range_kilometers')>0:
            # The configured trigger uses simulated distance. Find a repeatable physical
            # observer pose at that distance without changing runtime gameplay.
            center=target.get_planet_center()
            nav_center=c.navigation_position(center)
            lo=0.0;hi=10000000.0
            for _ in range(80):
                mid=(lo+hi)/2
                nav_range=(c.navigation_position(center+direction*mid)-nav_center).length()*100000
                if nav_range<float(distance):lo=mid
                else:hi=mid
            physical_distance=(lo+hi)/2
        ship.set_actor_location(target.get_planet_center() + direction * physical_distance, False, True)
        ship.get_flight_movement_component().stop_movement_immediately()
        return 'PIE-only range probe: ' + key + ' ' + distance + ' cm from center'
    if mode == 'hide_ship':
        ship.set_actor_hidden_in_game(True)
        return 'PIE-only unobstructed UI capture; ship collision and gameplay unchanged'
    if mode == 'add_passenger':
        unreal.GameplayStatics.create_player(world, -1, True)
        return 'created second local player for independent-view regression'
    if mode == 'board':
        unreal._celestial_navigation_qa_cameras = {}
        pawn = pc.get_controlled_pawn()
        if pawn != ship and not ship.is_player_boarded(pawn):
            pawn.set_actor_location(ship.get_boarding_interaction_center(), False, True)
            assert ship.try_board_player(pawn)
        controllers = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlayerController)
        for passenger in controllers:
            if passenger == pc: continue
            pawn = passenger.get_controlled_pawn()
            if ship.is_player_boarded(pawn): continue
            pawn.set_actor_location(ship.get_boarding_interaction_center(), False, True)
            assert ship.try_board_player(pawn)
        assert ship.begin_surface_takeoff()
        return 'boarded and takeoff started'
    if mode == 'near':
        # Debug travel restores all members and returns crew to the actual landing anchor.
        pc.call_method('ServerRequestDebugPlanet', ('MarII', False))
        return 'returned to Deimos'
    bodies = unreal.JTSCelestialNavigationLibrary.gather_bodies(manager, ship)
    mars = next(b for b in bodies if str(b.display_name) == '火星')
    deimos = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetAnchor) if str(a.get_planet_id()) == 'MarII')
    if mode in ['cameras_split', 'cameras_forward', 'cameras_back', 'cameras_deimos', 'cameras_phobos', 'cameras_distance', 'cameras_distance_back', 'cameras_close', 'cameras_zoomout', 'cameras_offaxis']:
        if not hasattr(unreal, '_celestial_navigation_qa_cameras'):
            unreal._celestial_navigation_qa_cameras = {}
        controllers = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlayerController)
        for index, controller in enumerate(controllers):
            camera = unreal._celestial_navigation_qa_cameras.get(controller.get_name())
            if not camera:
                pose = unreal.Transform(location=ship.get_actor_location())
                lib = unreal.get_default_object(unreal.GameplayStatics.static_class())
                camera = lib.call_method('BeginDeferredActorSpawnFromClass', (world, unreal.CameraActor.static_class(), pose, unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN, None, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
                camera = lib.call_method('FinishSpawningActor', (camera, pose, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
                camera.camera_component.set_editor_property('constrain_aspect_ratio', False)
                camera.camera_component.set_editor_property('field_of_view', 90)
                unreal._celestial_navigation_qa_cameras[controller.get_name()] = camera
            target = next(b for b in bodies if str(b.display_name) == (
                '火卫二' if mode == 'cameras_deimos' else '火卫一')) if mode in ['cameras_deimos', 'cameras_phobos'] else mars
            if mode in ['cameras_distance', 'cameras_distance_back', 'cameras_close', 'cameras_zoomout', 'cameras_offaxis']:
                target_key = getattr(unreal, '_celestial_navigation_qa_target', 'MarII')
                target = next(b for b in bodies if b.planet and str(b.planet.get_planet_id()) == target_key)
                camera.camera_component.set_editor_property('field_of_view', (55 if index == 0 else 150) if mode in ['cameras_distance', 'cameras_distance_back'] else 90)
            location = ship.get_actor_location() + ship.get_actor_up_vector() * 300
            if mode == 'cameras_zoomout' and index > 0:
                location = location + (location - target.center).normal() * target.marker_distance_centimeters
            if mode in ['cameras_deimos', 'cameras_phobos', 'cameras_close']:
                offset = (location - target.center).normal()
                location = target.center + offset * target.visual_radius * 4
            camera.set_actor_location(location, False, True)
            forward = (target.center - location).normal()
            if mode == 'cameras_back' or (mode in ['cameras_split', 'cameras_distance_back'] and index > 0): forward = forward * -1.0
            right = unreal.MathLibrary.cross_vector_vector(unreal.Vector(0, 0, 1), forward).normal()
            if mode == 'cameras_offaxis' and index > 0:
                forward = (forward * 0.8660254 + right * 0.5).normal()
                right = unreal.MathLibrary.cross_vector_vector(unreal.Vector(0, 0, 1), forward).normal()
            up = unreal.MathLibrary.cross_vector_vector(forward, right)
            camera.set_actor_rotation(unreal.MathLibrary.make_rotation_from_axes(forward, right, up), False)
            controller.set_view_target_with_blend(camera, 0)
        return 'PIE cameras: ' + mode
    if mode == 'near_airborne':
        target_key = getattr(unreal, '_celestial_navigation_qa_target', 'MarII')
        surface = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetAnchor) if str(a.get_planet_id()) == target_key)
        arrival = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetArrivalAnchor) if a.get_planet_anchor() == surface)
        transform = arrival.get_spacecraft_arrival_transform()
        radial = (transform.translation - surface.get_planet_center()).normal()
        ship.set_actor_location(transform.translation + radial * 3000, False, True)
        ship.get_flight_movement_component().stop_movement_immediately()
        return 'PIE airborne near-surface observer: ' + target_key
    direction = (deimos.get_planet_center() - mars.center).normal()
    position = mars.center + direction * (mars.visual_radius * 4.0)
    ship.set_actor_location(position, False, True)
    forward = direction * -1.0
    up = ship.get_actor_up_vector()
    right = unreal.MathLibrary.cross_vector_vector(up, forward).normal()
    up = unreal.MathLibrary.cross_vector_vector(forward, right)
    ship.set_actor_rotation(unreal.MathLibrary.make_rotation_from_axes(forward, right, up), False)
    ship.get_flight_movement_component().stop_movement_immediately()
    manager.update_spacecraft_flight_state(ship) if hasattr(manager, 'update_spacecraft_flight_state') else None
    return 'PIE observer in Mars local corridor'
