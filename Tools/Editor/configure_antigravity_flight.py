"""Level/Blueprint flight and lighting tuning, called through the Unreal MCP bridge."""
import json
import math
import runpy
from pathlib import Path
import unreal

OUT=Path(unreal.Paths.project_saved_dir())/'SpaceDebug'

def xyz(v): return [v.x,v.y,v.z]

def run(operation):
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if world else unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    if operation.startswith('balance_'):
        return runpy.run_path(str(Path(__file__).with_name('validate_planet_flight_balance.py')))['run'](operation[len('balance_'):],world,actors)
    if operation.startswith('approach_'):
        return runpy.run_path(str(Path(__file__).with_name('validate_cruise_approach.py')))['run'](operation[len('approach_'):],world,actors)
    if operation=='camera_configure':
        assert not world, 'Stop PIE before saving camera defaults'
        bp=unreal.load_asset('/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1')
        cls=unreal.EditorAssetLibrary.load_blueprint_class(bp.get_path_name())
        unreal.get_default_object(cls).get_flight_camera_boom().set_editor_property('enable_camera_lag',False)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
        return 'Saved production Blueprint camera: hull-attached translation, existing orbit rotation/zoom'
    if operation.startswith('camera_'):
        return camera_validation(operation[len('camera_'):],world,actors)
    if operation=='configure':
        assert not world, 'Stop PIE before saving authoring data'
        return configure(actors)
    if operation=='native':
        assert world
        cls=runpy.run_path(str(Path(__file__).with_name('configure_independent_space.py')))['NativeFlightProbe']
        unreal._antigravity_probe=cls(world,actors,1.0,'antigravity_native.json',('Moon','MarII','EarthFragments','Moon'))
        return 'Owner-RPC native flight started at real time: Deimos > Moon > Deimos > Earth > Moon'
    if operation=='native_status':
        p=unreal._antigravity_probe
        (OUT/'antigravity_native.json').write_text(json.dumps(p.result,indent=2),encoding='utf8')
        return json.dumps({'status':p.result['status'],'stage':p.stage,'legs':p.result['legs'],'samples':len(p.result['samples']),'errors':p.result['errors'][-5:],'last':p.result['samples'][-1:]})
    if operation=='flight_inspect':
        assert world
        ship=next(a for a in actors if isinstance(a,unreal.JTSSpacecraftActor))
        m=ship.get_flight_movement_component()
        stats=m.get_effective_stats()
        return json.dumps({'cruise_multiplier':stats.cruise_speed_multiplier,'instant':stats.instant_velocity_response,
            'active_multiplier':m.get_flight_speed_multiplier(),'velocity_cm_s':xyz(m.get_editor_property('velocity'))})
    if operation in ['light_off','light_on']:
        assert world
        light=next(a for a in actors if a.actor_has_tag('MarsReflectedFill'))
        light.spot_light_component.set_intensity(3000000 if operation=='light_on' else 0)
        return 'PIE-only Mars-reflected lighting comparison: '+operation
    if operation=='lighting_view':
        assert world
        ship=next(a for a in actors if isinstance(a,unreal.JTSSpacecraftActor))
        planet=next(a for a in actors if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='MarII')
        mars=next(a for a in actors if a.actor_has_tag('Mars'))
        frame=planet.get_surface_frame_at(ship.get_actor_location(),ship.get_actor_forward_vector())
        position=frame.location+frame.up*300+frame.right*1500
        local=planet.get_surface_frame_at(position,frame.forward)
        position=local.location+local.up*300
        toward=(mars.get_actor_bounds(False)[0]-position).normal()
        forward=(toward-local.up*unreal.MathLibrary.dot_vector_vector(toward,local.up)).normal()
        right=unreal.MathLibrary.cross_vector_vector(local.up,forward).normal()
        up=unreal.MathLibrary.cross_vector_vector(forward,right).normal()
        pose=unreal.Transform(location=position,rotation=unreal.MathLibrary.make_rotation_from_axes(forward,right,up))
        lib=unreal.get_default_object(unreal.GameplayStatics.static_class())
        camera=lib.call_method('BeginDeferredActorSpawnFromClass',(world,unreal.CameraActor.static_class(),pose,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        camera=lib.call_method('FinishSpawningActor',(camera,pose,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        camera.camera_component.set_field_of_view(100)
        camera.camera_component.set_editor_property('constrain_aspect_ratio',False)
        unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(camera,0)
        ship.set_actor_hidden_in_game(True)
        for a in actors:
            if isinstance(a,unreal.JTSCharacter): a.set_actor_hidden_in_game(True)
        unreal._antigravity_light_camera=camera
        return 'PIE-only surface observer: no gameplay or saved camera changes'
    if operation=='inspect':
        result={'lights':[],'exposure':[]}
        for a in actors:
            if isinstance(a,unreal.Light) or isinstance(a,unreal.SkyLight):
                c=a.get_component_by_class(unreal.LightComponentBase)
                row={'name':a.get_name(),'label':a.get_actor_label(),'type':a.get_class().get_name(),'location':xyz(a.get_actor_location()),'forward':xyz(a.get_actor_forward_vector())}
                for prop in ['intensity','light_color','mobility','cast_shadows','indirect_lighting_intensity','attenuation_radius','intensity_units','source_type','sky_distance_threshold']:
                    try: row[prop]=str(c.get_editor_property(prop))
                    except Exception: pass
                result['lights'].append(row)
            if isinstance(a,unreal.PostProcessVolume):
                settings=a.get_editor_property('settings')
                row={'name':a.get_name(),'unbound':a.get_editor_property('unbound')}
                for prop in ['auto_exposure_method','auto_exposure_min_brightness','auto_exposure_max_brightness','auto_exposure_bias','override_auto_exposure_bias','dynamic_global_illumination_method']:
                    try: row[prop]=str(settings.get_editor_property(prop))
                    except Exception: pass
                result['exposure'].append(row)
        (OUT/'antigravity_environment_before.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf8')
        return json.dumps(result,ensure_ascii=False)
    raise ValueError(operation)


def configure(actors):
    layout=json.loads(runpy.run_path(str(Path(__file__).with_name('configure_independent_space.py')))['configure'](actors))
    bp=unreal.load_asset('/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1')
    cls=unreal.EditorAssetLibrary.load_blueprint_class(bp.get_path_name())
    movement=unreal.get_default_object(cls).get_flight_movement_component()
    stats=movement.get_base_stats()
    stats.cruise_speed_multiplier=100
    stats.instant_velocity_response=True
    movement.set_editor_property('base_stats',stats)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
    movement=unreal.get_default_object(cls).get_flight_movement_component()
    assert movement.get_base_stats().cruise_speed_multiplier==100 and movement.get_base_stats().instant_velocity_response
    planet=next(a for a in actors if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='MarII')
    mars=next(a for a in actors if a.actor_has_tag('Mars'))
    center=planet.get_planet_center()
    toward=(mars.get_actor_bounds(False)[0]-center).normal()
    lights=[a for a in actors if a.actor_has_tag('MarsReflectedFill')]
    sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    light=lights[0] if lights else sub.spawn_actor_from_class(unreal.SpotLight,center+toward*100000)
    light.tags=['MarsReflectedFill']
    light.set_actor_label('MarII_Mars_ReflectedLight')
    light.set_folder_path('Space/Lighting')
    light.set_actor_location(center+toward*100000,False,True)
    light.set_actor_rotation(unreal.MathLibrary.make_rot_from_x(toward*-1),False)
    c=light.spot_light_component
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity_units',unreal.LightUnits.CANDELAS)
    c.set_editor_property('use_inverse_squared_falloff',True)
    c.set_intensity(3000000)
    c.set_light_color(unreal.LinearColor(1.0,.66,.44,1),False)
    c.set_attenuation_radius(150000)
    c.set_inner_cone_angle(25)
    c.set_outer_cone_angle(35)
    c.set_source_radius(10000)
    c.set_cast_shadows(True)
    c.set_editor_property('indirect_lighting_intensity',0)
    light.set_actor_tick_enabled(False)
    light.set_actor_enable_collision(False)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    sun=next(a for a in actors if isinstance(a,unreal.DirectionalLight) and a.get_actor_label()=='Sun_Main')
    arrival=next(a for a in actors if isinstance(a,unreal.JTSPlanetSurfaceAnchor) and a.get_planet_anchor()==planet)
    normal=arrival.get_actor_up_vector()
    sun_cos=unreal.MathLibrary.dot_vector_vector(normal,sun.get_actor_forward_vector()*-1)
    report={'cruise_multiplier':100,'instant_velocity_response':True,
        'world_forward_m_s':stats.max_move_speed/100,'world_cruise_m_s':stats.max_move_speed,
        'world_cruise_boost_m_s':stats.max_move_speed*stats.boost_multiplier,
        'layout':layout,'mars_reflected_light':{'intensity_cd':c.get_editor_property('intensity'),'direction_to_mars':xyz(toward),'local_range_cm':150000,
        'sun_incidence_at_default_site':sun_cos,'purpose':'Local warm Mars-reflected light, artist-amplified for night-side terrain readability; no ambient/global exposure change'}}
    (OUT/'antigravity_configuration.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    return json.dumps(report,ensure_ascii=False)


def camera_validation(operation,world,actors):
    """PIE-only production camera/input checks; never replaces view targets or disables ship Tick."""
    assert world
    ship=next(a for a in actors if isinstance(a,unreal.JTSSpacecraftActor))
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    if operation.startswith('takeoff_'):
        # Begin takeoff and issue directional intent in one editor call, before any
        # game frame can mask an initial input lock. Boarding uses the existing QA path.
        runpy.run_path(str(Path(__file__).with_name('configure_celestial_navigation.py')))['view_setup']('board')
        camera_validation('input_'+operation[len('takeoff_'):],world,actors)
        return camera_validation('inspect',world,actors)
    if operation.startswith('input_'):
        mode=operation[len('input_'):]
        previous=getattr(unreal,'_camera_follow_input_callback',None)
        if previous:
            unreal.unregister_slate_post_tick_callback(previous)
            unreal._camera_follow_input_callback=None
        state=unreal.JTSSpacecraftInputState()
        assert state.import_text('(MoveForward=%f,MoveRight=%f,Lift=%f,bFlightAssistEnabled=True,bBoosting=%s,SpeedLimit=1.0)'%(
            1 if mode=='forward' else -1 if mode=='reverse' else 0,
            1 if mode=='right' else -1 if mode=='left' else 0,1 if mode=='lift' else -1 if mode=='descend' else 0,
            'True' if mode in ['forward','reverse','right','left'] else 'False'))
        assert pc.get_controlled_pawn()==ship
        ship.call_method('ServerSetFlightInput',(state,))
        if mode!='stop':
            def submit(delta):
                if unreal.SystemLibrary.is_valid(ship): ship.call_method('ServerSetFlightInput',(state,))
            unreal._camera_follow_input_callback=unreal.register_slate_post_tick_callback(submit)
        return 'PIE owner-RPC flight intent with native ship/camera Tick: '+mode
    assert operation=='inspect'
    boom=ship.get_flight_camera_boom()
    camera=ship.get_flight_camera()
    rotation=camera.get_world_rotation()
    pivot=camera.get_world_location()+camera.get_forward_vector()*boom.target_arm_length-unreal.MathLibrary.quat_rotate_vector(rotation.quaternion(),boom.socket_offset)
    expected=boom.get_world_location()+boom.target_offset
    manager=next(a for a in actors if isinstance(a,unreal.JTSSpaceWorldManager))
    planet=manager.get_current_planet()
    envelope=ship.get_surface_envelope_component()
    frame=envelope.get_frame()
    result={'seconds':unreal.GameplayStatics.get_time_seconds(world),'ship_location':xyz(ship.get_actor_location()),
            'forward':xyz(ship.get_actor_forward_vector()),'right':xyz(ship.get_actor_right_vector()),
            'up':xyz(ship.get_actor_up_vector()),'landed':ship.is_landed(),
            'planet':str(planet.get_planet_id()) if planet else None,
            'radial_up':xyz(planet.get_radial_up_vector(ship.get_actor_location())) if planet else None,
            'envelope_valid':frame.valid,'following':envelope.is_following(),
            'ship_tick_enabled':ship.is_actor_tick_enabled(),'cruise':manager.is_cruise_presentation_active(ship),
            'velocity_cm_s':xyz(ship.get_flight_velocity()),'position_lag':boom.get_editor_property('enable_camera_lag'),
            'pivot_error_cm':(pivot-expected).length(),'camera_distance_cm':(camera.get_world_location()-ship.get_actor_location()).length(),
            'arm_length_cm':boom.target_arm_length,'players':[],
            'camera_actor_count':len([a for a in actors if isinstance(a,unreal.CameraActor)])}
    for controller in actors:
        if not isinstance(controller,unreal.JTSPlayerController):continue
        pcm=controller.player_camera_manager
        target=controller.get_view_target()
        result['players'].append({'controller':controller.get_name(),'pawn':controller.get_controlled_pawn().get_name(),
            'view_target':target.get_name(),'view_target_type':target.get_class().get_name(),
            'move_ignored':controller.is_move_input_ignored(),
            'camera_distance_cm':(pcm.get_camera_location()-ship.get_actor_location()).length()})
    return json.dumps(result)
