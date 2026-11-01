"""Level-only navigation configuration; concrete IDs and references never enter runtime C++."""
from pathlib import Path
import json
import time
import traceback
import unreal

OUT = Path(unreal.Paths.project_saved_dir()).resolve() / 'NavigationMap'
OUT.mkdir(parents=True, exist_ok=True)


def _editor():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE first'
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def inspect_editor():
    actors = _editor()
    manager = next(a for a in actors if isinstance(a, unreal.JTSSpaceWorldManager))
    anchors = [a for a in actors if isinstance(a, unreal.JTSPlanetAnchor)]
    return dict(level=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name(),
                dirty_maps=[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
                dirty_content=[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()],
                anchors=[dict(id=str(a.get_planet_id()), path=a.get_path_name(), name=str(a.get_editor_property('navigation_display_name')),
                              chart=a.get_editor_property('bShowOnStarChart'),
                              backgrounds=[dict(name=str(e.display_name), chart=e.show_on_star_chart,
                                                actor=e.actor.get_path_name()) for e in a.get_editor_property('background_navigation')]) for a in anchors],
                groups=[dict(actor=b.actor.get_path_name(), group=str(b.presentation_group), radius=b.radius_kilometers,
                             position=[b.navigation_position_kilometers.x, b.navigation_position_kilometers.y, b.navigation_position_kilometers.z])
                        for b in manager.compressed_space.bodies])


def configure():
    actors = _editor()
    for actor in actors:
        if not isinstance(actor, unreal.JTSPlanetAnchor):
            continue
        names = {'EarthFragments': '地球', 'Moon': '月球', 'Mars': '火星', 'MarI': '火卫一', 'MarII': '火卫二'}
        if str(actor.get_planet_id()) in names:
            actor.set_editor_property('navigation_display_name', unreal.Text(names[str(actor.get_planet_id())]))
            actor.set_editor_property('bShowOnStarChart', str(actor.get_planet_id()) not in {'MarI', 'MarII'})
        entries = list(actor.get_editor_property('background_navigation'))
        for entry in entries:
            if str(entry.display_name) in names.values():
                entry.set_editor_property('show_on_star_chart', str(entry.display_name) not in {'火卫一', '火卫二'})
        actor.set_editor_property('background_navigation', entries)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return inspect_editor()


def close_editor():
    _editor()
    saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    backup = None
    if not saved:
        # Preserve this validation instance without overwriting a map locked by another Editor.
        assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(), 'Unsaved content requires attention'
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        backup = '/Game/Developers/Codex/L_SpaceWorld_NavigationCheckpoint'
        assert unreal.EditorLoadingAndSavingUtils.save_map(world, backup), 'Could not preserve validation world'
    unreal.SystemLibrary.quit_editor()
    return dict(closing=True, backup=backup)


def inspect_dirty():
    return dict(packages=[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
                content=[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()])


def _pie():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world, 'Start PIE first'
    manager = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpaceWorldManager)[0]
    ship = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpacecraftActor)[0]
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    return world, manager, ship, pc


def _snapshot(label, screenshot=True):
    world, manager, ship, pc = _pie()
    result = dict(label=label, landed=ship.is_landed(), players=[])
    widgets = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.JTSPrototypeHUDWidget, False)
    for widget in widgets:
        nav = widget.get_editor_property('CruiseNavigationWidget')
        if not nav:
            continue
        map_center = nav.get_ground_map_center()
        player_position = nav.get_player_map_position()
        result['players'].append(dict(owner=widget.get_owning_player().get_name(), stage=nav.get_navigation_stage(),
                                      zoom=nav.get_navigation_zoom(), coordinates=nav.get_navigation_coordinates(),
                                      surface_texture=nav.get_surface_map_texture().get_path_name() if nav.get_surface_map_texture() else None,
                                      map_center=[map_center.x, map_center.y, map_center.z],
                                      player_position=[player_position.x, player_position.y],
                                      body_positions=[[p.x, p.y, p.z] for p in nav.get_navigation_body_projections()],
                                      ship_contact=nav.has_navigation_ship_contact(),
                                      contacts=list(nav.get_navigation_contact_names()), visible=str(nav.get_visibility())))
        texture = nav.get_surface_map_texture()
        if screenshot and texture and nav.get_navigation_stage() == 'Ground':
            unreal.RenderingLibrary.export_render_target(world, texture, str(OUT), label + '_surface.png')
    if screenshot:
        filename = str(OUT / (label + '.png')).replace('\\', '/')
        unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename=' + filename + ' -nosuffix', pc)
    return result


def capture():
    return _snapshot('manual_capture')


def qa_status():
    return getattr(unreal, '_jts_navigation_map_qa', dict(status='not_started'))


def start_qa():
    world, manager, ship, pc = _pie()
    assert not getattr(unreal, '_jts_navigation_map_qa', {}).get('status') == 'running'
    result = dict(status='running', snapshots=[], failures=[])
    unreal._jts_navigation_map_qa = result
    space = manager.compressed_space

    class Probe:
        def __init__(self):
            self.index = 0
            self.next_time = time.monotonic() + 2
            self.planet = manager.get_current_planet()
            self.radial = (ship.get_actor_location() - space.authored_center(self.planet)).normal()
            self.near = None
            self.local = None
            self.walk_remaining = 0
            self.follow_samples = []
            self.actions = [(self.mars, 3.0), (self.ground, 0.5), (self.walk, 0.8), (self.ground_moved, 0.1),
                            (self.board, 0.5), (lambda: self.record('landed_boarded', 'Ground'), 0.1),
                            (self.takeoff, 0.5), (self.surface, 1.8), (lambda: self.record('near', 'Surface'), 0.1),
                            (self.system, 0.65), (lambda: self.record('surface_to_system', None), 1.1),
                            (lambda: self.record('system', 'System'), 0.1), (self.rotate_system, 0.8),
                            (lambda: self.record('system_rotated', 'System'), 0.1), (self.translate_system, 0.8),
                            (lambda: self.record('system_translated', 'System'), 0.1), (self.interstellar, 0.7),
                            (lambda: self.record('system_to_interstellar', None), 1.1),
                            (lambda: self.record('interstellar', 'Interstellar'), 0.1), (self.system, 1.8),
                            (lambda: self.record('return_system', 'System'), 0.1), (self.surface, 1.8),
                            (lambda: self.record('return_near', 'Surface'), 0.1), (self.moon, 3.0),
                            (self.moon_ground, 0.1), (self.board, 0.5), (self.takeoff, 0.5), (self.surface, 1.8),
                            (self.system, 1.8), (lambda: self.record('earth_system', 'System'), 0.1)]

        def record(self, label, expected):
            snap = _snapshot(label)
            result['snapshots'].append(snap)
            assert snap['players'], 'No local navigation HUD was found'
            if expected:
                for player in snap['players']:
                    if player['stage'] != expected:
                        result['failures'].append(dict(label=label, expected=expected, actual=player['stage']))
                    target = {'Ground': 0, 'Surface': 0, 'System': 1, 'Interstellar': 2}[expected]
                    if abs(player['zoom'] - target) > 0.03:
                        result['failures'].append(dict(label=label, expected_zoom=target, actual_zoom=player['zoom']))
                    if expected == 'Ground' and not player['surface_texture']:
                        result['failures'].append(dict(label=label, missing_terrain=True))
                    if expected == 'Ground' and not player['ship_contact']:
                        result['failures'].append(dict(label=label, missing_ship=True))
                    if label.startswith('system') or label == 'return_system':
                        if '火卫一' in player['contacts'] or '火卫二' in player['contacts']:
                            result['failures'].append(dict(label=label, hidden_moons_visible=True))
            return snap

        def mars(self):
            # Use the same server-authoritative debug travel as the in-game tool;
            # validation must not depend on the user's last saved debug destination.
            pc.call_method('ServerRequestDebugPlanet', (unreal.Name('Mars'), False))

        def ground(self):
            self.planet = manager.get_current_planet()
            assert str(self.planet.get_planet_id()) == 'Mars'
            self.radial = (ship.get_actor_location() - space.authored_center(self.planet)).normal()
            self.record('ground', 'Ground')

        def walk(self):
            pawn = pc.get_controlled_pawn()
            assert isinstance(pawn, unreal.JTSCharacter)
            self.walk_remaining = 20

        def ground_moved(self):
            current = self.record('ground_moved', 'Ground')
            before = result['snapshots'][0]['players'][0]['coordinates']
            assert current['players'][0]['coordinates'] != before, 'Ground coordinates did not update'
            assert len(self.follow_samples) == 20
            assert max(s['center_error'] for s in self.follow_samples) < 5, 'Map center lagged behind the astronaut'
            assert max(s['player_offset'] for s in self.follow_samples) < 0.001, 'Player marker left map center'
            result['continuous_follow'] = self.follow_samples

        def board(self):
            pawn = pc.get_controlled_pawn()
            if isinstance(pawn, unreal.JTSCharacter):
                pawn.set_actor_location(ship.get_boarding_interaction_center(), False, True)
                assert ship.try_board_player(pawn), 'Boarding failed'

        def takeoff(self):
            assert ship.begin_surface_takeoff(), 'Takeoff failed'
            ship.get_flight_movement_component().set_component_tick_enabled(False)

        def position(self, location):
            ship.set_actor_location(location, False, True)
            ship.get_flight_movement_component().stop_movement_immediately()

        def surface(self):
            self.near = space.authored_center(self.planet) + self.radial * (space.authored_radius(self.planet) + space.near_altitude_centimeters * 0.4)
            self.position(self.near)

        def system(self):
            # Immediately outside the same near-flight shell used by the runtime map.
            self.local = space.authored_center(self.planet) + self.radial * (space.authored_radius(self.planet) + space.near_altitude_centimeters + 100)
            self.position(self.local)

        def rotate_system(self):
            self.system_rotation = ship.get_actor_rotation()
            ship.set_actor_rotation(unreal.Rotator(pitch=34, yaw=71, roll=29), False)

        def translate_system(self):
            snapshots = {s['label']: s for s in result['snapshots']}
            assert snapshots['system']['players'][0]['body_positions'] != snapshots['system_rotated']['players'][0]['body_positions'], 'Hull rotation did not move contacts'
            self.position(ship.get_actor_location() + ship.get_actor_right_vector() * 12000)

        def interstellar(self):
            snapshots = {s['label']: s for s in result['snapshots']}
            assert snapshots['system_rotated']['players'][0]['body_positions'] != snapshots['system_translated']['players'][0]['body_positions'], 'Ship translation did not move contacts'
            origin = space.authored_center(self.planet)
            distance = space.authored_radius(self.planet) + space.near_altitude_centimeters
            members = list(space.bodies)
            source = next(b for b in members if b.actor == self.planet)
            group = [b for b in members if b.presentation_group == source.presentation_group]
            primary = max(group, key=lambda b: b.radius_kilometers)
            radius = max(primary.radius_kilometers * manager.navigation_map_settings.minimum_system_radii,
                         max((b.navigation_position_kilometers - primary.navigation_position_kilometers).length() + b.radius_kilometers for b in group)
                         * manager.navigation_map_settings.system_extent_multiplier)
            for _ in range(40):
                distance *= 1.25
                location = origin + self.radial * distance
                if (space.navigation_position(location) - primary.navigation_position_kilometers).length() > radius * 1.5:
                    self.position(location)
                    return
            raise RuntimeError('Could not leave the local system')

        def moon(self):
            ship.get_flight_movement_component().set_component_tick_enabled(True)
            pc.call_method('ServerRequestDebugPlanet', (unreal.Name('Moon'), False))

        def moon_ground(self):
            self.planet = manager.get_current_planet()
            assert str(self.planet.get_planet_id()) == 'Moon'
            self.radial = (ship.get_actor_location() - space.authored_center(self.planet)).normal()
            self.record('moon_ground', 'Ground')

        def tick(self, dt):
            try:
                if self.walk_remaining:
                    pawn = pc.get_controlled_pawn()
                    # UWidget is not a UUserWidget: inspect the owning HUD's navigation child.
                    hud = unreal.WidgetLibrary.get_all_widgets_of_class(world, unreal.JTSPrototypeHUDWidget, False)[0]
                    nav = hud.get_editor_property('CruiseNavigationWidget')
                    self.follow_samples.append(dict(center_error=(nav.get_ground_map_center() - pawn.get_actor_location()).length(),
                                                    player_offset=(nav.get_player_map_position().x ** 2 + nav.get_player_map_position().y ** 2) ** 0.5))
                    pawn.set_actor_location(pawn.get_actor_location() + pawn.get_actor_forward_vector() * 35, False, True)
                    self.walk_remaining -= 1
                    self.next_time = time.monotonic() + 0.5
                    return
                if time.monotonic() < self.next_time:
                    return
                if self.index >= len(self.actions):
                    result['status'] = 'passed' if not result['failures'] else 'failed'
                    ship.get_flight_movement_component().set_component_tick_enabled(True)
                    unreal.unregister_slate_post_tick_callback(self.handle)
                else:
                    action, delay = self.actions[self.index]
                    action()
                    self.index += 1
                    self.next_time = time.monotonic() + delay
            except Exception:
                result['status'] = 'failed'
                result['traceback'] = traceback.format_exc()
                ship.get_flight_movement_component().set_component_tick_enabled(True)
                unreal.unregister_slate_post_tick_callback(self.handle)
            (OUT / 'pie_validation.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')

    probe = Probe()
    probe.handle = unreal.register_slate_post_tick_callback(probe.tick)
    unreal._jts_navigation_map_probe = probe
    return result
