"""Inspect and configure project assets through SpacecraftFlowTools in Unreal MCP."""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/SpacecraftFlow'
OUT.mkdir(parents=True, exist_ok=True)
EA = unreal.EditorAssetLibrary
EARTH = '/Game/Space/Maps/L_EarthLaunchPrototype'
SPACE = '/Game/Space/Maps/L_SpaceWorld'
ENTRY = '/Game/Space/FrontEnd/Maps/L_Entry'
LOBBY = '/Game/Space/FrontEnd/Maps/L_PreLaunchLobby'
SHIP = '/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1'


def vec(v):
    return [v.x, v.y, v.z]


def path(obj):
    return obj.get_path_name() if obj else None


def properties(obj, names):
    result = {}
    for name in names:
        try:
            result[name] = str(obj.get_editor_property(name))
        except Exception as exc:
            result[name] = str(exc)
    return result


def ship_info(ship):
    rows = []
    for c in ship.get_components_by_class(unreal.SceneComponent):
        row = dict(name=c.get_name(), cls=c.get_class().get_name(), parent=path(c.get_attach_parent()),
                   relative=str(c.get_relative_transform()), world=str(c.get_world_transform()),
                   absolute=properties(c, ['absolute_location', 'absolute_rotation', 'absolute_scale']))
        if isinstance(c, unreal.StaticMeshComponent):
            row.update(mesh=path(c.static_mesh), collision=str(c.get_collision_enabled()),
                       pawn_response=str(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)),
                       step_up=str(c.get_editor_property('can_character_step_up_on')))
            if c.static_mesh:
                row['mesh_bounds'] = str(c.static_mesh.get_bounding_box())
        rows.append(row)
    return dict(path=path(ship), cls=path(ship.get_class()), transform=str(ship.get_actor_transform()),
                feet=str(ship.get_landing_support_component().get_editor_property('feet')),
                presentation=properties(ship.get_component_by_class(unreal.JTSSpacecraftPresentationComponent), ['headlight_intensity', 'headlight_range']),
                components=rows)


def inspect():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE first'
    es = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    report = dict(maps={}, blueprints={})
    for level in [ENTRY, LOBBY, EARTH, SPACE]:
        assert levels.load_level(level)
        placed = es.get_all_level_actors()
        settings = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings()
        gm_class = settings.get_editor_property('default_game_mode')
        gm = unreal.get_default_object(gm_class) if gm_class else None
        report['maps'][level] = dict(game_mode=path(gm_class),
            game_mode_properties=properties(gm, ['spacecraft_class', 'default_pawn_class', 'player_controller_class']) if gm else {},
            ships=[ship_info(a) for a in placed if isinstance(a, unreal.JTSSpacecraftActor)])
        if level == EARTH:
            report['maps'][level]['ground'] = [dict(label=a.get_actor_label(), path=path(a), transform=str(a.get_actor_transform()),
                bounds=str(a.get_actor_bounds(False))) for a in placed if not isinstance(a, unreal.JTSSpacecraftActor)
                and isinstance(a, (unreal.StaticMeshActor, unreal.PlayerStart))]
    for bp_path in [SHIP, '/Game/Space/Blueprints/Ships/BP_Spacecraft', SHIP + '_Equipped']:
        bp = unreal.load_asset(bp_path)
        if not bp:
            continue
        actor = es.spawn_actor_from_class(bp.generated_class(), unreal.Vector(), transient=True)
        try:
            report['blueprints'][bp_path] = ship_info(actor)
        finally:
            es.destroy_actor(actor)
    assert levels.load_level(ENTRY)
    return report


def apply():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    es = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    bp = unreal.load_asset(SHIP)
    subs = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    changed = []
    # Character movement must not use the exterior hull or its low collision pieces as steps.
    for handle in subs.k2_gather_subobject_data_for_blueprint(bp):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if isinstance(obj, unreal.PrimitiveComponent):
            obj.set_editor_property('can_character_step_up_on', unreal.CanBeCharacterBase.ECB_NO)
            changed.append(obj.get_name())
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert EA.save_loaded_asset(bp, False)

    # Old host snapshots refer to this class by path. Keep it as a compatibility child of
    # the configured production craft, so resumed expeditions receive the same complete rig.
    legacy = unreal.load_asset('/Game/Space/Blueprints/Ships/BP_Spacecraft')
    unreal.BlueprintEditorLibrary.reparent_blueprint(legacy, bp.generated_class())
    cdo = unreal.get_default_object(legacy.generated_class())
    parent = unreal.get_default_object(bp.generated_class())
    source = {c.get_name(): c for c in parent.get_components_by_class(unreal.SceneComponent)}
    for c in cdo.get_components_by_class(unreal.SceneComponent):
        p = source.get(c.get_name())
        if not p:
            continue
        for key in ['relative_location', 'relative_rotation', 'relative_scale3d']:
            c.set_editor_property(key, p.get_editor_property(key))
        if isinstance(c, unreal.StaticMeshComponent):
            c.set_static_mesh(p.static_mesh)
            for i, material in enumerate(p.get_materials()):
                c.set_material(i, material)
        if isinstance(c, unreal.PrimitiveComponent):
            c.set_collision_profile_name(p.get_collision_profile_name())
            c.set_collision_enabled(p.get_collision_enabled())
            c.set_editor_property('can_character_step_up_on', unreal.CanBeCharacterBase.ECB_NO)
    cdo.get_landing_support_component().set_editor_property('feet', parent.get_landing_support_component().get_editor_property('feet'))
    unreal.BlueprintEditorLibrary.compile_blueprint(legacy)
    assert EA.save_loaded_asset(legacy, False)

    assert levels.load_level(EARTH)
    old = next(a for a in es.get_all_level_actors() if isinstance(a, unreal.JTSSpacecraftActor))
    location = old.get_actor_location()
    label = old.get_actor_label()
    rotation = old.get_actor_rotation()
    # The Earth prototype has a planar launch floor. Match its measured top to the authored
    # deployed soles; spherical planets continue to use the runtime surface solver.
    floor = next(a for a in es.get_all_level_actors() if a.get_actor_label() == 'Floor')
    origin, extent = floor.get_actor_bounds(True)
    feet = parent.get_landing_support_component().get_editor_property('feet')
    sole_z = min(f.deployed_contact_frame.translation.z for f in feet)
    location.z = origin.z + extent.z - sole_z
    assert es.destroy_actor(old)
    ship = es.spawn_actor_from_class(bp.generated_class(), location, rotation)
    ship.set_actor_label(label)
    ship.set_folder_path('Gameplay/Spacecraft')
    # Keep the entry capsule outside the new hull, alongside its boarding volume.
    start = next(a for a in es.get_all_level_actors() if isinstance(a, unreal.PlayerStart))
    start.set_actor_location(location + unreal.Vector(0, -950, 96 - location.z), False, False)
    assert levels.save_current_level()
    return dict(ship=ship_info(ship), no_step_components=changed, floor_top=origin.z + extent.z,
                legacy_parent=path(bp.generated_class()),
                legacy_feet=len(cdo.get_landing_support_component().get_editor_property('feet')))


def close_editor():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    unreal.SystemLibrary.quit_editor()
    return dict(closing=True)


def pie_probe():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world and 'L_Entry' in world.get_name()
    gi = unreal.GameplayStatics.get_game_instance(world)
    online = next(o for o in unreal.ObjectIterator(unreal.JTSOnlineSessionSubsystem) if o.get_outer() == gi)
    online.create_expedition('MCPRIG', '', 1, unreal.JTSLobbyVisibility.PRIVATE)

    class RouteProbe:
        def __init__(self):
            self.stage = 0
            self.result = dict(status='running')
            self.handle = unreal.register_slate_post_tick_callback(self.tick)

        def tick(self, delta):
            import traceback
            done = False
            try:
                w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
                if not w or unreal.GameplayStatics.get_time_seconds(w) < 1:
                    return
                def command(name):
                    (OUT / 'action.txt').write_text(name)
                    return route_status()
                if self.stage == 0 and 'L_PreLaunchLobby' in w.get_name():
                    command('start')
                    self.stage = 1
                elif self.stage == 1 and 'L_EarthLaunchPrototype' in w.get_name():
                    self.result['earth'] = command('earth_validate')
                    command('board')
                    self.result['launch'] = command('launch')['phase']
                    assert 'LAUNCHING' in self.result['launch']
                    self.stage = 2
                elif self.stage == 2 and 'L_SpaceWorld' in w.get_name():
                    self.result['space'] = command('board')
                    assert 'BP_Twinfork_R1_C' in self.result['space']['pawn']
                    assert not self.result['space']['ignore_move'] and not self.result['space']['ignore_look']
                    assert self.result['space']['ships'][0]['feet'] == 4
                    self.result.update(status='passed', success=True)
                    done = True
            except Exception:
                self.result.update(status='failed', success=False, traceback=traceback.format_exc())
                done = True
            if done:
                unreal.unregister_slate_post_tick_callback(self.handle)
            self.result['stage'] = self.stage
            (OUT / 'menu_route.json').write_text(json.dumps(self.result, indent=2))

    unreal._spacecraft_flow_route = RouteProbe()
    return dict(world=path(world), state=str(online.get_operation_state()), error=online.get_last_error())


def route_status():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    ships = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpacecraftActor)
    action_file = OUT / 'action.txt'
    action = action_file.read_text().strip() if action_file.exists() else ''
    if action:
        action_file.write_text('')
    if action == 'start':
        pc.call_method('ServerSetReady', (True,))
        pc.call_method('ServerRequestStartExpedition')
    elif action == 'board':
        ship = ships[0]
        pawn = pc.get_controlled_pawn()
        pawn.set_actor_location(ship.get_boarding_interaction_center() + unreal.Vector(0, -500, 0), False, True)
        pc.call_method('ServerRequestBoardSpacecraft', (ship,))
    elif action == 'earth_validate':
        s = ships[0]
        components = {c.get_name(): c for c in s.get_components_by_class(unreal.SceneComponent)}
        soles = [components[str(f.foot_component_name)].get_world_transform().transform_location(f.sole_centre_local).z
                 for f in s.get_landing_support_component().get_editor_property('feet')]
        pawn = pc.get_controlled_pawn()
        capsule_height = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
        pawn.set_actor_location(s.get_actor_location() + unreal.Vector(0, -950, capsule_height + 5 - s.get_actor_location().z), False, True)
        hit = pawn.add_actor_world_offset(unreal.Vector(0, 950, 0), True, False)
        blocked_at = vec(pawn.get_actor_location())
        report = dict(sole_z=soles, gear_alpha=s.get_component_by_class(unreal.JTSSpacecraftPresentationComponent).get_gear_deploy_alpha(),
                      capsule_end=blocked_at, capsule_half_height=capsule_height, ship_location=vec(s.get_actor_location()), hit=str(hit),
                      no_step=all(c.get_editor_property('can_character_step_up_on') == unreal.CanBeCharacterBase.ECB_NO
                                  for c in s.get_components_by_class(unreal.PrimitiveComponent)
                                  if c.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
                                  and c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN) == unreal.CollisionResponseType.ECR_BLOCK))
        (OUT / 'earth_validation.json').write_text(json.dumps(report, indent=2))
        assert all(abs(z) < 0.2 for z in soles) and report['gear_alpha'] == 1
        assert blocked_at[1] < s.get_actor_location().y - 450 and report['no_step']
        return report
    elif action == 'launch':
        ships[0].deposit_resources([unreal.JTSResourceType.FUEL] * 10)
        gm = unreal.GameplayStatics.get_game_mode(world)
        gm.finish_earth_collection()
    elif action == 'entry':
        unreal.GameplayStatics.open_level(world, unreal.Name(ENTRY))
    elif action == 'light':
        ships[0].call_method('ServerToggleHeadlights')
    elif action == 'descent':
        ship = ships[0]
        assert pc.get_controlled_pawn() == ship

        class DescentProbe:
            def __init__(self):
                self.started = unreal.GameplayStatics.get_time_seconds(world)
                self.seen = []
                self.handle = unreal.register_slate_post_tick_callback(self.tick)

            def tick(self, delta):
                import traceback
                elapsed = unreal.GameplayStatics.get_time_seconds(world) - self.started
                state = str(ship.get_flight_state())
                if not self.seen or self.seen[-1] != state:
                    self.seen.append(state)
                result = None
                try:
                    if ship.is_landed():
                        assert len(ship.get_landing_support_component().get_foot_contacts()) == 4
                        assert ship.are_headlights_on()
                        result = dict(success=True, elapsed=elapsed, states=self.seen,
                                      feet=4, lights=True, planet=str(ship.get_landed_planet().get_planet_id()))
                    elif elapsed > 45:
                        raise AssertionError('Driver descent did not land: ' + str(ship.get_last_landing_failure()))
                    else:
                        state = unreal.JTSSpacecraftInputState()
                        assert state.import_text('(Lift=-1,bDescentKeyHeld=True,bFlightAssistEnabled=True,SpeedLimit=0.5)')
                        ship.call_method('ServerSetFlightInput', (state,))
                except Exception:
                    result = dict(success=False, elapsed=elapsed, states=self.seen, traceback=traceback.format_exc())
                if result:
                    unreal.unregister_slate_post_tick_callback(self.handle)
                    (OUT / 'descent.json').write_text(json.dumps(result, indent=2))

        unreal._spacecraft_flow_descent = DescentProbe()
    return dict(world=path(world), pc=path(pc), pawn=path(pc.get_controlled_pawn()),
                ignore_move=pc.is_move_input_ignored(), ignore_look=pc.is_look_input_ignored(),
                mouse=pc.get_editor_property('show_mouse_cursor'),
                phase=str(unreal.GameplayStatics.get_game_state(world).get_gameplay_phase()),
                ships=[dict(path=path(s), cls=path(s.get_class()), location=vec(s.get_actor_location()),
                            feet=len(s.get_landing_support_component().get_editor_property('feet')),
                            lights=s.are_headlights_on(), state=str(s.get_flight_state()),
                            beams=[dict(name=c.get_name(), parent=path(c.get_attach_parent()), location=vec(c.get_world_location()),
                                        intensity=c.get_editor_property('intensity'), visible=c.is_visible())
                                   for c in s.get_components_by_class(unreal.SpotLightComponent)]) for s in ships])








