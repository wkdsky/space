"""Targeted Phobos gameplay promotion and level-owned Mars exclusion narrative.

Run through SpaceDebugAuthoringTools.run('mars_access_configure'), with PIE stopped.
Does not rebuild the scene or change celestial positions or NAV coordinates.
"""
import json
import math
import traceback
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_saved_dir()) / 'SpaceDebug'
PHOBOS_GAMEPLAY_RADIUS_CM = 9000.0


def vec(v): return [v.x, v.y, v.z]


def context():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if world else unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    manager = next(a for a in actors if isinstance(a, unreal.JTSSpaceWorldManager))
    c = manager.get_editor_property('compressed_space')
    if not world: c.capture_authored_bodies(False)
    return world, actors, manager, c


def run(operation):
    world, actors, manager, c = context()
    if operation == 'measure_phobos':
        planet = next(a for a in actors if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'MarI')
        visual = planet.get_gameplay_surface_actor()
        center, extent = visual.get_actor_bounds(False)
        ship = next((a for a in actors if isinstance(a, unreal.JTSSpacecraftActor)), None)
        if not ship:
            ship = unreal.get_default_object(unreal.load_class(None, '/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1.BP_Twinfork_R1_C'))
        feet = ship.get_landing_support_component().get_editor_property('feet')
        return json.dumps(dict(center=vec(center), extent_cm=vec(extent), scale=vec(visual.get_actor_scale3d()),
            feet=[vec(v.get_editor_property('deployed_contact_frame').translation) for v in feet]))
    if operation == 'navigation_probe':
        try:
            assert world
            ship = next(a for a in actors if isinstance(a, unreal.JTSSpacecraftActor))
            bodies = unreal.JTSCelestialNavigationLibrary.gather_bodies(manager, ship)
            target = next(v for v in bodies if v.planet and str(v.planet.get_planet_id()) == 'MarI')
            result = dict(show_on_chart=target.get_editor_property('bShowOnStarChart'),
                show_marker=target.get_editor_property('bShowScreenMarker'),
                marker_eligible=unreal.JTSCelestialNavigationLibrary.is_within_marker_distance(target),
                collision=target.visual_actor.get_actor_enable_collision(),
                chart_names=[str(v.display_name) for v in bodies if v.get_editor_property('bShowOnStarChart')])
            assert not result['show_on_chart'] and not result['show_marker'] and not result['marker_eligible']
            assert result['collision']
            (OUT/'phobos_hidden_navigation.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
            return json.dumps(result, ensure_ascii=False)
        except Exception:
            return traceback.format_exc()
    if operation == 'restore_performance':
        if hasattr(unreal, '_mars_access_throttle'):
            unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings')).set_editor_property('bThrottleCPUWhenNotForeground', unreal._mars_access_throttle)
            del unreal._mars_access_throttle
        return 'Editor background performance setting restored'
    if operation == 'inspect':
        bodies = []
        for e in c.get_editor_property('bodies'):
            a = e.actor
            bodies.append(dict(actor=a.get_actor_label(), cls=a.get_class().get_name(),
                center=vec(a.get_planet_center() if isinstance(a, unreal.JTSPlanetAnchor) else a.get_actor_bounds(False)[0]),
                radius=c.authored_radius(a), bubble=c.bubble_radius(a), collision=a.get_actor_enable_collision(),
                surface=str(a.get_gameplay_surface_actor()) if isinstance(a, unreal.JTSPlanetAnchor) else None))
        gm = unreal.get_default_object(unreal.load_class(None, '/Game/Space/Blueprints/Modes/BP_SpaceWorldGameMode.BP_SpaceWorldGameMode_C'))
        result = dict(bodies=bodies, controllers=[str(v) for v in gm.get_editor_property('surface_gameplay_controllers')])
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT/'mars_access_inspect.json').write_text(json.dumps(result, indent=2), encoding='utf8')
        return json.dumps(result)
    if operation == 'configure':
        assert not world, 'Stop PIE before authoring'
        assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().startswith('/Game/Space/Maps/L_SpaceWorld.')
        entries = c.get_editor_property('bodies')
        old = next((a for a in actors if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'MarI'), None)
        visual = old.get_gameplay_surface_actor() if old else next(e.actor for e in entries if e.actor.actor_has_tag('MarI'))
        center, extent = visual.get_actor_bounds(False)
        radius = extent.get_abs_max() if hasattr(extent, 'get_abs_max') else max(extent.x, extent.y, extent.z)
        # Set an absolute authored size, so repeated configuration cannot compound the scale.
        visual.modify()
        visual.set_actor_scale3d(visual.get_actor_scale3d() * (PHOBOS_GAMEPLAY_RADIUS_CM / radius))
        # Scale around the mesh's bounds centre, even if an imported mesh has an offset pivot.
        visual.set_actor_location(visual.get_actor_location() + center - visual.get_actor_bounds(False)[0], False, True)
        radius = PHOBOS_GAMEPLAY_RADIUS_CM
        es = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        planet = old or es.spawn_actor_from_class(unreal.JTSPlanetAnchor, center)
        planet.modify()
        planet.set_actor_label('MarI_PlanetAnchor')
        settings = dict(planet_id='MarI', navigation_display_name='火卫一 / Phobos', approximate_radius=radius,
            gameplay_surface_actor=visual, gameplay_surface_component=visual.static_mesh_component,
            gravity_strength=75.0, gravity_influence_range=20000.0, space_exit_range=22000.0,
            takeoff_transition_altitude=c.get_editor_property('near_altitude_centimeters'),
            space_flight_altitude=c.get_editor_property('near_altitude_centimeters'),
            approach_transition_altitude=c.get_editor_property('near_altitude_centimeters'),
            near_flight_reference_radius=21000.0, near_flight_size_exponent=.5,
            near_flight_max_speed_multiplier=3.0, cruise_approach_exponent_override=.1,
            navigation_marker_range_kilometers=0.0, bShowOnStarChart=False, bShowScreenMarker=False)
        for key, value in settings.items(): planet.set_editor_property(key, value)
        mesh = visual.static_mesh_component.static_mesh
        mesh.modify()
        setup = mesh.get_editor_property('body_setup')
        setup.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        setup.set_editor_property('double_sided_geometry', True)
        sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        sme.set_lod_build_settings(mesh, 0, sme.get_lod_build_settings(mesh, 0))
        visual.modify()
        visual.set_actor_enable_collision(True)
        visual.static_mesh_component.set_collision_profile_name('BlockAll')
        visual.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        visual.static_mesh_component.set_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA, unreal.CollisionResponseType.ECR_IGNORE)
        visual.static_mesh_component.set_editor_property('generate_overlap_events', False)
        # Remove the former background membership/metadata; one presentation owner per mesh.
        for a in actors:
            if not isinstance(a, unreal.JTSPlanetAnchor): continue
            a.modify()
            a.set_editor_property('celestial_background_actors', [v for v in a.get_editor_property('celestial_background_actors') if v != visual])
            a.set_editor_property('background_navigation', [v for v in a.get_editor_property('background_navigation') if v.actor != visual])
        c.modify()
        for i, e in enumerate(entries):
            if e.actor == visual: e.actor = planet
            if e.actor.actor_has_tag('Mars'):
                tuning = e.get_editor_property('interference')
                tuning.set_editor_property('bEnabled', True)
                tuning.trigger_offset = 18000.0
                tuning.minimum_clearance = 3000.0
                tuning.braking_seconds = .65
                tuning.turning_seconds = 2.8
                tuning.retreat_seconds = 3.0
                tuning.retreat_distance = 16000.0
                tuning.recovery_seconds = .55
                e.set_editor_property('interference', tuning)
            entries[i] = e
        c.set_editor_property('bodies', entries)
        c.capture_authored_bodies(False)
        # Reuse the existing empty surface controller through a Blueprint configuration.
        path = '/Game/Space/Blueprints/Planets/Phobos/BP_PhobosSurfaceController'
        bp = unreal.load_asset(path)
        if not bp:
            factory = unreal.BlueprintFactory()
            factory.set_editor_property('parent_class', unreal.JTSPassivePlanetSurfaceController)
            bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_PhobosSurfaceController', '/Game/Space/Blueprints/Planets/Phobos', unreal.Blueprint, factory)
        unreal.get_default_object(bp.generated_class()).set_editor_property('planet_id', 'MarI')
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        gm_bp = unreal.load_asset('/Game/Space/Blueprints/Modes/BP_SpaceWorldGameMode')
        gm = unreal.get_default_object(gm_bp.generated_class())
        gm.modify()
        defs = [v for v in gm.get_editor_property('surface_gameplay_controllers') if str(v.planet_id) != 'MarI']
        definition = unreal.JTSSurfaceGameplayControllerDefinition()
        assert definition.import_text('(PlanetId=MarI,ControllerClass="%s")' % bp.generated_class().get_path_name())
        defs.append(definition)
        gm.set_editor_property('surface_gameplay_controllers', defs)
        unreal.BlueprintEditorLibrary.compile_blueprint(gm_bp)
        for asset in [mesh, bp, gm_bp]: assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        return run('inspect')
    if operation == 'probe':
        assert world
        allow_background_verification()
        unreal._mars_access_probe = MarsAccessProbe(world, actors, manager, c)
        return 'Production server-RPC interference probe started at normal game speed'
    if operation == 'status':
        result = unreal._mars_access_probe.result
        return json.dumps({**result, 'samples': len(result['samples']), 'last': result['samples'][-1:]})
    if operation == 'phobos_probe':
        assert world
        allow_background_verification()
        unreal._phobos_landing_probe = PhobosLandingProbe(world, actors, manager, c)
        return 'Phobos terrain/support scan and normal-speed controlled landing started'
    if operation == 'phobos_status':
        result = unreal._phobos_landing_probe.result
        return json.dumps({**result, 'samples': len(result['samples']), 'last': result['samples'][-1:]})
    if operation == 'phobos_hover':
        assert world
        ship = next(a for a in actors if isinstance(a, unreal.JTSSpacecraftActor))
        planet = next(a for a in actors if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'MarI')
        if ship.is_landed(): assert ship.begin_surface_takeoff()
        ship.call_method('ServerSetFlightInput', (unreal.JTSSpacecraftInputState(),))
        ship.set_actor_location(ship.get_actor_location() + planet.get_radial_up_vector(ship.get_actor_location())*1500, False, True)
        return 'PIE-only hover setup for local altimeter and hidden-name visual verification'
    if operation == 'phobos_exit_takeoff':
        assert world
        ship = next(a for a in actors if isinstance(a, unreal.JTSSpacecraftActor))
        assert ship.is_landed() and str(ship.get_landed_planet().get_planet_id()) == 'MarI'
        assert ship.try_disembark_player(ship.get_boarded_player()), 'Safe real-terrain disembark failed'
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        character = pc.get_controlled_pawn()
        location = vec(character.get_actor_location())
        assert ship.try_board_player(character), 'Reboarding failed'
        assert ship.begin_surface_takeoff(), 'Normal Phobos surface takeoff failed'
        result = dict(disembarked=True, exit_location=location, reboarded=True, takeoff_started=True, flight_state=str(ship.get_flight_state()))
        (OUT/'phobos_exit_takeoff.json').write_text(json.dumps(result, indent=2), encoding='utf8')
        return json.dumps(result)
    raise ValueError(operation)


def allow_background_verification():
    settings = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
    if not hasattr(unreal, '_mars_access_throttle'): unreal._mars_access_throttle = settings.get_editor_property('bThrottleCPUWhenNotForeground')
    settings.set_editor_property('bThrottleCPUWhenNotForeground', False)


class MarsAccessProbe:
    def __init__(self, world, actors, manager, c):
        self.world, self.c = world, c
        self.ship = next(a for a in actors if isinstance(a, unreal.JTSSpacecraftActor))
        self.mars = next(e.actor for e in c.get_editor_property('bodies') if e.actor.actor_has_tag('Mars'))
        self.pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if self.pc.get_controlled_pawn() != self.ship:
            pawn = self.pc.get_controlled_pawn()
            pawn.set_actor_location(self.ship.get_boarding_interaction_center(), False, True)
            assert self.ship.try_board_player(pawn)
        if self.ship.is_landed(): assert self.ship.begin_surface_takeoff()
        self.m = self.ship.get_flight_movement_component()
        self.center = c.authored_center(self.mars)
        self.radial = unreal.Vector(0, 0, 1)
        start = self.center + self.radial * (c.bubble_radius(self.mars) + 20000)
        self.ship.set_actor_location(start, False, True)  # Test setup only; subsequent flight uses owned RPC.
        self.ship.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(-self.radial, unreal.Vector(1, 0, 0)), True)
        self.result = dict(status='running', phases=[], samples=[], errors=[])
        self.start_time = unreal.GameplayStatics.get_time_seconds(world)
        self.had_control = False
        self.last_phase = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        unreal.SystemLibrary.execute_console_command(world, 't.IdleWhenNotForeground 0')
        unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS 60')

    def tick(self, _):
        try:
            phase = str(self.ship.get_interference_component().get_phase())
            controlled = self.ship.get_interference_component().is_controlling_flight()
            if phase != self.last_phase:
                self.result['phases'].append(phase)
                self.last_phase = phase
            self.had_control |= controlled
            elapsed = unreal.GameplayStatics.get_time_seconds(self.world) - self.start_time
            distance = (self.ship.get_actor_location() - self.center).length() - self.c.bubble_radius(self.mars)
            self.result['samples'].append(dict(t=elapsed, phase=phase, margin=distance, speed=self.ship.get_current_speed(), location=vec(self.ship.get_actor_location()), forward=vec(self.ship.get_actor_forward_vector())))
            if distance < 0: raise AssertionError('Entered excluded Mars near-flight region')
            state = unreal.JTSSpacecraftInputState()
            assert state.import_text('(MoveForward=1,bBoosting=True)')
            # Continuous pilot input must not defeat the active takeover.
            self.ship.call_method('ServerSetFlightInput', (state,))
            if self.had_control and not controlled:
                outward = (self.ship.get_actor_location() - self.center).normal()
                self.result.update(status='complete', outward_dot=unreal.MathLibrary.dot_vector_vector(self.ship.get_actor_forward_vector(), outward))
                assert self.result['outward_dot'] > .99
                self.stop()
            elif elapsed > 25: raise AssertionError('Takeover timeout')
        except Exception as exc:
            self.result['errors'].append(str(exc))
            self.result['status'] = 'failed'
            self.stop()

    def stop(self):
        self.ship.call_method('ServerSetFlightInput', (unreal.JTSSpacecraftInputState(),))
        unreal.unregister_slate_post_tick_callback(self.handle)
        (OUT/'mars_interference_native.json').write_text(json.dumps(self.result, indent=2), encoding='utf8')


class PhobosLandingProbe:
    def __init__(self, world, actors, manager, c):
        self.world, self.manager = world, manager
        self.ship = next(a for a in actors if isinstance(a, unreal.JTSSpacecraftActor))
        self.planet = next(a for a in actors if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'MarI')
        self.pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if self.pc.get_controlled_pawn() != self.ship:
            pawn = self.pc.get_controlled_pawn()
            pawn.set_actor_location(self.ship.get_boarding_interaction_center(), False, True)
            assert self.ship.try_board_player(pawn)
        self.ship.call_method('ServerSetFlightInput', (unreal.JTSSpacecraftInputState(),))
        self.ship.clear_grounded_planet()
        self.center = self.planet.get_planet_center()
        # Make the tested body the observer's near-flight body before surface traces.
        self.ship.set_actor_location(self.center + unreal.Vector(0, 0, self.planet.get_approximate_radius() + 4000), False, True)
        self.result = dict(status='preparing', samples=[], errors=[])
        self.start_time = unreal.GameplayStatics.get_time_seconds(world)
        self.prepared = False
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def prepare(self):
        hits, chosen, supported_sites = [], None, 0
        for i in range(96):
            z = 1 - 2*(i+.5)/96
            angle = i * math.pi*(3-math.sqrt(5))
            radial = unreal.Vector(math.sqrt(1-z*z)*math.cos(angle), math.sqrt(1-z*z)*math.sin(angle), z)
            forward = unreal.MathLibrary.cross_vector_vector(radial, unreal.Vector(0, 1, 0)).normal()
            query = self.center + radial * (self.planet.get_approximate_radius() + 10000)
            frame = self.planet.get_surface_frame_at(query, forward)
            if not frame: continue
            hits.append(dict(direction=vec(radial), point=vec(frame.location), normal=vec(frame.up)))
            for heading in range(12):
                heading_forward = unreal.MathLibrary.rotate_angle_axis(frame.forward, heading*15, frame.up)
                pose = unreal.Transform(location=frame.location, rotation=unreal.MathLibrary.make_rot_from_xz(heading_forward, frame.up))
                if self.ship.snap_spacecraft_to_surface_transform(self.planet, pose):
                    supported_sites += 1
                    if chosen is None: chosen = self.planet.get_surface_frame_at(query, heading_forward)
                    break
        assert chosen, 'No real terrain position can support this ship on Phobos'
        # Restore the selected pose after scan, then start an ordinary flight descent.
        assert self.ship.snap_spacecraft_to_surface_transform(self.planet, chosen.transform)
        self.ship.clear_grounded_planet()
        self.ship.set_actor_location(chosen.location + self.planet.get_radial_up_vector(chosen.location)*1800, False, True)
        self.ship.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(chosen.forward, chosen.up), True)
        self.result = dict(status='running', surface_hits=len(hits), supported_sites=supported_sites,
            authored_radius_cm=self.planet.get_approximate_radius(), selected_surface=vec(chosen.location), samples=[], errors=[])
        self.start_time = unreal.GameplayStatics.get_time_seconds(self.world)
        self.prepared = True

    def tick(self, _):
        try:
            if not self.prepared:
                if unreal.GameplayStatics.get_time_seconds(self.world)-self.start_time > .5: self.prepare()
                return
            elapsed = unreal.GameplayStatics.get_time_seconds(self.world)-self.start_time
            self.result['samples'].append(dict(t=elapsed, state=str(self.ship.get_flight_state()), failure=str(self.ship.get_last_landing_failure()), location=vec(self.ship.get_actor_location())))
            state = unreal.JTSSpacecraftInputState()
            assert state.import_text('(Lift=-1,bDescentKeyHeld=True)')
            self.ship.call_method('ServerSetFlightInput', (state,))
            if self.ship.is_landed():
                self.result.update(status='complete', landed_planet=str(self.ship.get_landed_planet().get_planet_id()), contacts=len(self.ship.get_landing_support_component().get_foot_contacts()))
                self.stop()
            elif elapsed > 35: raise AssertionError('Phobos native landing timeout')
        except Exception as exc:
            self.result['errors'].append(str(exc))
            self.result['status'] = 'failed'
            self.stop()

    def stop(self):
        self.ship.call_method('ServerSetFlightInput', (unreal.JTSSpacecraftInputState(),))
        unreal.unregister_slate_post_tick_callback(self.handle)
        (OUT/'phobos_landing_native.json').write_text(json.dumps(self.result, indent=2), encoding='utf8')
